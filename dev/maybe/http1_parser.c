#ifndef __GNU_SOURCE
#define __GNU_SOURCE
#endif

#include "http1_parser.h"
#include <ctype.h>
#include <string.h>

// inline static uint8_t seek2ch(uint8_t **pos, uint8_t *limit, uint8_t ch) {
//   /* This is library based alternative that is sometimes slower  */
//   if (*pos >= limit || **pos == ch) {
//     return 0;
//   }
//   uint8_t *tmp = memchr(*pos, ch, limit - (*pos));
//   if (tmp) {
//     *pos = tmp;
//     *tmp = 0;
//     return 1;
//   }
//   *pos = limit;
//   return 0;
// }

/* a helper that seeks any char, converts it to NUL and returns 1 if found.
 * requires at lease 1 character.
 */
static inline uint8_t seek2ch(uint8_t **buffer, uint8_t *limit,
                              const uint8_t c) {
  /* this single char lookup is better when target is closer... */
  if (**buffer == c)
    return 1;

  uint64_t wanted = 0x0101010101010101ULL * c;
  uint64_t *lpos = (uint64_t *)*buffer;
  uint64_t *llimit = ((uint64_t *)((uintptr_t)limit & 7));

  for (; lpos < llimit; lpos++) {
    const uint64_t eq = ~((*lpos) ^ wanted);
    const uint64_t t0 = (eq & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
    const uint64_t t1 = (eq & 0x8080808080808080llu);
    if ((t0 & t1)) {
      break;
    }
  }

  *buffer = (uint8_t *)lpos;
  while (*buffer < limit) {
    if (**buffer == c) {
      **buffer = 0;
      return 1;
    }
    (*buffer)++;
  }
  return 0;
}

/* a helper that seeks the EOL, converts it to NUL and returns it's length */
inline static uint8_t seek2eol(uint8_t **pos, uint8_t *limit) {
  /* single char lookup using library is best when target is far... */
  if (*pos >= limit)
    return 0;
  uint8_t *tmp = memchr(*pos, '\n', limit - (*pos));
  if (tmp) {
    *pos = tmp;
    *tmp = 0;
    if (tmp[-1] == '\r') {
      (tmp)[-1] = 0;
      return 2;
    }
    return 1;
  }
  *pos = limit;
  return 0;
}

size_t http1_fio_parser_fn(struct http1_fio_parser_args_s *args) {
  uint8_t *start = args->buffer;
  uint8_t *end = start;
  uint8_t *stop = start + args->length;
  uint8_t eol_len = 0;
#define CONSUMED ((size_t)((uintptr_t)start - (uintptr_t)args->buffer))

  switch (args->parser->state.reserved & 0x0F) {

  case 0:
    /* request / response line */
    /* clear out any leadinng white space */
    while (*start == '\r' || *start == '\r' || *start == ' ' || *start == 0) {
      start++;
    }
    end = start;
    if (!(eol_len = seek2eol(&end, stop)))
      return CONSUMED;
    if (start[0] >= '1' && start[0] <= '9') {
      /* HTTP response */
      args->parser->state.reserved |= 128;
      uint8_t *tmp = start;
      if (!seek2ch(&tmp, end, ' '))
        goto error;
      if (args->on_http_version(args->parser, (char *)start, tmp - start))
        goto error;
      tmp = start = tmp + 1;
      if (!seek2ch(&tmp, end, ' '))
        goto error;
      if (args->on_status(args->parser, atol((char *)start), (char *)(tmp + 1),
                          end - tmp - eol_len))
        goto error;
    } else {
      /* HTTP request */
      uint8_t *tmp = start;
      if (!seek2ch(&tmp, end, ' '))
        goto error;
      if (args->on_method(args->parser, (char *)start, tmp - start))
        goto error;
      tmp = start = tmp + 1;
      if (seek2ch(&tmp, end, '?')) {
        if (args->on_path(args->parser, (char *)start, tmp - start))
          goto error;
        tmp = start = tmp + 1;
        if (!seek2ch(&tmp, end, ' '))
          goto error;
        if (tmp - start > 0 &&
            args->on_query(args->parser, (char *)start, tmp - start))
          goto error;
      } else {
        tmp = start;
        if (!seek2ch(&tmp, end, ' '))
          goto error;
        if (args->on_path(args->parser, (char *)start, tmp - start))
          goto error;
      }
      start = tmp + 1;
      if (start + 7 >= end)
        goto error;
      if (args->on_http_version(args->parser, (char *)start,
                                end - start - eol_len + 1))
        goto error;
    }
    end = start = end + 1;
    args->parser->state.reserved |= 1;

  /* fallthrough */
  case 1:
    do {
      uint8_t t2 = 0;
      if (!(eol_len = seek2eol(&end, stop)))
        return CONSUMED;
      uint8_t *tmp = start;
      /* test for header ending */
      if (*start == 0) {
        end = start = end + 1;
        args->parser->state.reserved |= 2;
        goto finished_headers; /* break the do..while loop, not the switch
                                  statement */
      }
      if (!seek2ch(&tmp, end, ':'))
        goto error;
#if HTTP_HEADERS_LOWERCASE
      for (uint8_t *t3 = start; t3 < tmp; t3++) {
        *t3 = tolower(*t3);
      }
#endif
      if (*tmp == ' ') {
        tmp++;
        t2 = 1;
      };
      if (tmp - start - t2 == 14 &&
          HEADER_NAME_IS_EQ((char *)start, "content-length", 14)) {
        /* handle the special `content-length` header */
        args->parser->state.content_length = atol((char *)tmp);
      }
      /* perform callback */
      if (args->on_header(args->parser, (char *)start, tmp - start - t2,
                          (char *)tmp + 1, end - (tmp + 1) - eol_len))
        goto error;
      end = start = end + 1;
    } while ((args->parser->state.reserved & 2) == 0);
  finished_headers:
    if (args->parser->state.content_length == 0)
      goto finish;

  /* fallthrough */
  /*  2 | 1 == 3 */
  case 3:
    end = start + args->parser->state.content_length - args->parser->state.read;
    if (end > stop)
      end = stop;
    if (args->on_body_chunk(args->parser, (char *)start, end - start))
      goto error;
    args->parser->state.read += end - start;
    if (args->parser->state.content_length == args->parser->state.read)
      goto finish;
    return CONSUMED;
    break;
  }
error:
  args->on_error(args->parser);
  return CONSUMED;
finish:
  *args->parser = (http1_parser_s){args->parser->udata, {0, 0, 0}};
  if (((args->parser->state.reserved & 128) ? args->on_response
                                            : args->on_request)(args->parser))
    goto error;
  return CONSUMED;
}

#undef CONSUMED
