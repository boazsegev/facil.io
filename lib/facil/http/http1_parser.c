/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef __GNU_SOURCE
#define __GNU_SOURCE
#endif

#include "http1_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PREFER_MEMCHAR 0

/* *****************************************************************************
Seeking for characters in a string
***************************************************************************** */

#if PREFER_MEMCHAR

/* a helper that seeks any char, converts it to NUL and returns 1 if found. */
inline static uint8_t seek2ch(uint8_t **pos, uint8_t *const limit, uint8_t ch) {
  /* This is library based alternative that is sometimes slower  */
  if (*pos >= limit || **pos == ch) {
    return 0;
  }
  uint8_t *tmp = memchr(*pos, ch, limit - (*pos));
  if (tmp) {
    *pos = tmp;
    *tmp = 0;
    return 1;
  }
  *pos = limit;
  return 0;
}

#else

/* a helper that seeks any char, converts it to NUL and returns 1 if found. */
static inline uint8_t seek2ch(uint8_t **buffer, const uint8_t *const limit,
                              const uint8_t c) {
  /* this single char lookup is better when target is closer... */
  if (**buffer == c) {
    **buffer = 0;
    return 1;
  }

  uint64_t wanted = 0x0101010101010101ULL * c;
  uint64_t *lpos = (uint64_t *)*buffer;
  uint64_t *llimit = ((uint64_t *)limit) - 1;

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

#endif

/* a helper that seeks the EOL, converts it to NUL and returns it's length */
inline static uint8_t seek2eol(uint8_t **pos, uint8_t *const limit) {
  /* single char lookup using memchr might be better when target is far... */
  if (!seek2ch(pos, limit, '\n'))
    return 0;
  if ((*pos)[-1] == '\r') {
    (*pos)[-1] = 0;
    return 2;
  }
  return 1;
}

/* *****************************************************************************
HTTP/1.1 parsre stages
***************************************************************************** */

inline static int consume_response_line(struct http1_fio_parser_args_s *args,
                                        uint8_t *start, uint8_t *end) {
  args->parser->state.reserved |= 128;
  uint8_t *tmp = start;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (args->on_http_version(args->parser, (char *)start, tmp - start))
    return -1;
  tmp = start = tmp + 1;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (args->on_status(args->parser, atol((char *)start), (char *)(tmp + 1),
                      end - tmp))
    return -1;
  return 0;
}

inline static int consume_request_line(struct http1_fio_parser_args_s *args,
                                       uint8_t *start, uint8_t *end) {
  uint8_t *tmp = start;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (args->on_method(args->parser, (char *)start, tmp - start))
    return -1;
  tmp = start = tmp + 1;
  if (seek2ch(&tmp, end, '?')) {
    if (args->on_path(args->parser, (char *)start, tmp - start))
      return -1;
    tmp = start = tmp + 1;
    if (!seek2ch(&tmp, end, ' '))
      return -1;
    if (tmp - start > 0 &&
        args->on_query(args->parser, (char *)start, tmp - start))
      return -1;
  } else {
    tmp = start;
    if (!seek2ch(&tmp, end, ' '))
      return -1;
    if (args->on_path(args->parser, (char *)start, tmp - start))
      return -1;
  }
  start = tmp + 1;
  if (start + 7 >= end)
    return -1;
  if (args->on_http_version(args->parser, (char *)start, end - start))
    return -1;
  return 0;
}

inline static int consume_header(struct http1_fio_parser_args_s *args,
                                 uint8_t *start, uint8_t *end) {
  uint8_t t2 = 1;
  uint8_t *tmp = start;
  /* divide header name from data */
  if (!seek2ch(&tmp, end, ':'))
    return -1;
#if HTTP_HEADERS_LOWERCASE
  for (uint8_t *t3 = start; t3 < tmp; t3++) {
    *t3 = tolower(*t3);
  }
#endif

  tmp++;
  if (tmp[0] == ' ') {
    tmp++;
    t2++;
  };
#if HTTP_HEADERS_LOWERCASE
  if ((tmp - start) - t2 == 14 &&
      *((uint64_t *)start) == *((uint64_t *)"content-") &&
      *((uint64_t *)(start + 6)) == *((uint64_t *)"t-length")) {
    /* handle the special `content-length` header */
    args->parser->state.content_length = atol((char *)tmp);
  }
#else
  if ((tmp - start) - t2 == 14 &&
      HEADER_NAME_IS_EQ((char *)start, "content-length", 14)) {
    /* handle the special `content-length` header */
    args->parser->state.content_length = atol((char *)tmp);
  }
#endif
  /* perform callback */
  if (args->on_header(args->parser, (char *)start, (tmp - start) - t2,
                      (char *)tmp, end - tmp))
    return -1;
  return 0;
}

/* *****************************************************************************
HTTP/1.1 parsre function
***************************************************************************** */

size_t http1_fio_parser_fn(struct http1_fio_parser_args_s *args) {
  uint8_t *start = args->buffer;
  uint8_t *end = start;
  uint8_t *const stop = start + args->length;
  uint8_t eol_len = 0;
#define CONSUMED ((size_t)((uintptr_t)start - (uintptr_t)args->buffer))
  // fprintf(stderr, "** resuming with at %p with %.*s...(%lu)\n", args->buffer,
  // 4,
  //         start, args->length);
  switch ((args->parser->state.reserved & 31)) {

  /* request / response line */
  case 0:
    /* clear out any leadinng white space */
    while (*start == '\r' || *start == '\r' || *start == ' ' || *start == 0) {
      start++;
    }
    end = start;
    /* make sure the whole line is available*/
    if (!(eol_len = seek2eol(&end, stop)))
      return CONSUMED;

    if (start[0] >= '1' && start[0] <= '9') {
      /* HTTP response */
      if (consume_response_line(args, start, end - eol_len + 1))
        goto error;
    } else {
      /* HTTP request */
      if (consume_request_line(args, start, end - eol_len + 1))
        goto error;
    }
    end = start = end + 1;
    args->parser->state.reserved |= 1;

  /* fallthrough */
  /* headers */
  case 1:
    do {
      if (!(eol_len = seek2eol(&end, stop)))
        return CONSUMED;
      /* test for header ending */
      if (*start == 0)
        goto finished_headers; /* break the do..while loop, not the switch
                                  statement */
      if (consume_header(args, start, end - eol_len + 1))
        goto error;
      end = start = end + 1;
    } while ((args->parser->state.reserved & 2) == 0);
  finished_headers:
    end = start = end + 1;
    args->parser->state.reserved |= 2;
    if (args->parser->state.content_length == 0)
      goto finish;
    if (end >= stop)
      return args->length;

  /* fallthrough */
  /* request body */
  case 3: /*  2 | 1 == 3 */
    end = start + args->parser->state.content_length - args->parser->state.read;
    if (end > stop)
      end = stop;
    if (end == start)
      return CONSUMED;
    // fprintf(stderr, "Consuming body at (%lu/%lu):%.*s\n", end - start,
    //         args->parser->state.content_length, (int)(end - start), start);
    if (args->on_body_chunk(args->parser, (char *)start, end - start))
      goto error;
    args->parser->state.read += (end - start);
    start = end;
    if (args->parser->state.content_length <= args->parser->state.read)
      goto finish;
    return CONSUMED;
    break;
  }

error:
  args->on_error(args->parser);
  args->parser->state =
      (struct http1_parser_protected_read_only_state_s){0, 0, 0};
  return args->length;

finish:
  if (((args->parser->state.reserved & 128) ? args->on_response
                                            : args->on_request)(args->parser))
    goto error;
  args->parser->state =
      (struct http1_parser_protected_read_only_state_s){0, 0, 0};
  return CONSUMED;
}

#undef CONSUMED
