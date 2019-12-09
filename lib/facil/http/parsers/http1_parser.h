/*
Copyright: Boaz Segev, 2017-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

/**
This is a callback based parser. It parses the skeleton of the HTTP/1.x protocol
and leaves most of the work (validation, error checks, etc') to the callbacks.

This is an attempt to replace the existing HTTP/1.x parser with something easier
to maintain and that could be used for an HTTP/1.x client as well.
*/
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Parser Settings
***************************************************************************** */

#ifndef HTTP_HEADERS_LOWERCASE
/**
 * When defined, HTTP headers will be converted to lowercase and header
 * searches will be case sensitive.
 *
 * This is highly recommended, required by facil.io and helps with HTTP/2
 * compatibility.
 */
#define HTTP_HEADERS_LOWERCASE 1
#endif

#ifndef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
#define HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING 1
#endif

#ifndef FIO_MEMCHAR
/** Prefer a custom memchr implementation. Usualy memchr is better. */
#define FIO_MEMCHAR 0
#endif

#if FIO_UNALIGNED_MEMORY_ACCESS_ENABLED
#define HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED 1
#endif

#ifndef HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED
/** Peforms some optimizations assuming unaligned memory access is okay. */
#define HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED 0
#endif

/* *****************************************************************************
Parser API
***************************************************************************** */

/** this struct contains the state of the parser. */
typedef struct http1_parser_s {
  struct http1_parser_protected_read_only_state_s {
    long long content_length; /* negative values indicate chuncked data state */
    ssize_t read;     /* total number of bytes read so far (body only) */
    uint8_t *next;    /* the known position for the end of request/response */
    uint8_t reserved; /* for internal use */
  } state;
} http1_parser_s;

#define HTTP1_PARSER_INIT                                                      \
  {                                                                            \
    { 0 }                                                                      \
  }
/**
 * Returns the amount of data actually consumed by the parser.
 *
 * The value 0 indicates there wasn't enough data to be parsed and the same
 * buffer (with more data) should be resubmitted.
 *
 * A value smaller than the buffer size indicates that EITHER a request /
 * response was detected OR that the leftover could not be consumed because more
 * data was required.
 *
 * Simply resubmit the reminder of the data to continue parsing.
 *
 * A request / response callback automatically stops the parsing process,
 * allowing the user to adjust or refresh the state of the data.
 */
static size_t http1_parse(http1_parser_s *parser, void *buffer, size_t length);

/* *****************************************************************************
Required Callbacks (MUST be implemented by including file)
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser);
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser);
/** called when a request method is parsed. */
static int
http1_on_method(http1_parser_s *parser, char *method, size_t method_len);
/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser,
                           size_t status,
                           char *status_str,
                           size_t len);
/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t path_len);
/** called when a request path (excluding query) is parsed. */
static int
http1_on_query(http1_parser_s *parser, char *query, size_t query_len);
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_version(http1_parser_s *parser, char *version, size_t len);
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser,
                           char *name,
                           size_t name_len,
                           char *data,
                           size_t data_len);
/** called when a body chunk is parsed. */
static int
http1_on_body_chunk(http1_parser_s *parser, char *data, size_t data_len);
/** called when a protocol error occurred. */
static int http1_on_error(http1_parser_s *parser);

/* *****************************************************************************

















                        Implementation Details

















***************************************************************************** */

#if HTTP_HEADERS_LOWERCASE
#define HEADER_NAME_IS_EQ(var_name, const_name, len)                           \
  (!memcmp((var_name), (const_name), (len)))
#else
#define HEADER_NAME_IS_EQ(var_name, const_name, len)                           \
  (!strncasecmp((var_name), (const_name), (len)))
#endif

/* *****************************************************************************
Seeking for characters in a string
***************************************************************************** */

#if FIO_MEMCHAR

/**
 * This seems to be faster on some systems, especially for smaller distances.
 *
 * On newer systems, `memchr` should be faster.
 */
static int
seek2ch(uint8_t **buffer, register uint8_t *const limit, const uint8_t c) {
  if (*buffer >= limit)
    return 0;
  if (**buffer == c) {
    return 1;
  }

#if !HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED
  /* too short for this mess */
  if ((uintptr_t)limit <= 16 + ((uintptr_t)*buffer & (~(uintptr_t)7)))
    goto finish;

  /* align memory */
  {
    const uint8_t *alignment =
        (uint8_t *)(((uintptr_t)(*buffer) & (~(uintptr_t)7)) + 8);
    if (*buffer < alignment)
      *buffer += 1; /* we already tested this char */
    if (limit >= alignment) {
      while (*buffer < alignment) {
        if (**buffer == c) {
          return 1;
        }
        *buffer += 1;
      }
    }
  }
  const uint8_t *limit64 = (uint8_t *)((uintptr_t)limit & (~(uintptr_t)7));
#else
  const uint8_t *limit64 = (uint8_t *)limit - 7;
#endif
  uint64_t wanted1 = 0x0101010101010101ULL * c;
  for (; *buffer < limit64; *buffer += 8) {
    const uint64_t eq1 = ~((*((uint64_t *)*buffer)) ^ wanted1);
    const uint64_t t0 = (eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
    const uint64_t t1 = (eq1 & 0x8080808080808080llu);
    if ((t0 & t1)) {
      break;
    }
  }
#if !HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED
finish:
#endif
  while (*buffer < limit) {
    if (**buffer == c) {
      return 1;
    }
    (*buffer)++;
  }
  return 0;
}

#else

/* a helper that seeks any char, converts it to NUL and returns 1 if found. */
inline static uint8_t seek2ch(uint8_t **pos, uint8_t *const limit, uint8_t ch) {
  /* This is library based alternative that is sometimes slower  */
  if (*pos >= limit)
    return 0;
  if (**pos == ch) {
    return 1;
  }
  uint8_t *tmp = memchr(*pos, ch, limit - (*pos));
  if (tmp) {
    *pos = tmp;
    return 1;
  }
  *pos = limit;
  return 0;
}

#endif

/* a helper that seeks the EOL, converts it to NUL and returns it's length */
inline static uint8_t seek2eol(uint8_t **pos, uint8_t *const limit) {
  /* single char lookup using memchr might be better when target is far... */
  if (!seek2ch(pos, limit, '\n'))
    return 0;
  if ((*pos)[-1] == '\r') {
    return 2;
  }
  return 1;
}

/* *****************************************************************************
Change a letter to lower case (latin only)
***************************************************************************** */

static uint8_t http_tolower(uint8_t c) {
  if (c >= 'A' && c <= 'Z')
    c |= 32;
  return c;
}

/* *****************************************************************************
String to Number
***************************************************************************** */

/** Converts a String to a number using base 10 */
static long long http1_atol(const uint8_t *buf, const uint8_t **end) {
  register unsigned long long i = 0;
  uint8_t inv = 0;
  while (*buf == ' ' || *buf == '\t' || *buf == '\f')
    ++buf;
  while (*buf == '-' || *buf == '+')
    inv ^= (*(buf++) == '-');
  while (i <= ((((~0ULL) >> 1) / 10)) && *buf >= '0' && *buf <= '9') {
    i = i * 10;
    i += *buf - '0';
    ++buf;
  }
  /* test for overflow */
  if (i >= (~((~0ULL) >> 1)) || (*buf >= '0' && *buf <= '9'))
    i = (~0ULL >> 1);
  if (inv)
    i = 0ULL - i;
  if (end)
    *end = buf;
  return i;
}

/** Converts a String to a number using base 16, overflow limited to 113bytes */
static long long http1_atol16(const uint8_t *buf, const uint8_t **end) {
  register unsigned long long i = 0;
  uint8_t inv = 0;
  for (int limit_ = 0;
       (*buf == ' ' || *buf == '\t' || *buf == '\f') && limit_ < 32;
       ++limit_)
    ++buf;
  for (int limit_ = 0; (*buf == '-' || *buf == '+') && limit_ < 32; ++limit_)
    inv ^= (*(buf++) == '-');
  if (*buf == '0')
    ++buf;
  if ((*buf | 32) == 'x')
    ++buf;
  for (int limit_ = 0; (*buf == '0') && limit_ < 32; ++limit_)
    ++buf;
  while (!(i & (~((~(0ULL)) >> 4)))) {
    if (*buf >= '0' && *buf <= '9') {
      i <<= 4;
      i |= *buf - '0';
    } else if ((*buf | 32) >= 'a' && (*buf | 32) <= 'f') {
      i <<= 4;
      i |= (*buf | 32) - ('a' - 10);
    } else
      break;
    ++buf;
  }
  if (inv)
    i = 0ULL - i;
  if (end)
    *end = buf;
  return i;
}

/* *****************************************************************************
HTTP/1.1 parsre stages
***************************************************************************** */

inline static int http1_consume_response_line(http1_parser_s *parser,
                                              uint8_t *start,
                                              uint8_t *end) {
  parser->state.reserved |= 128;
  uint8_t *tmp = start;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (http1_on_version(parser, (char *)start, tmp - start))
    return -1;
  tmp = start = tmp + 1;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (http1_on_status(
          parser, http1_atol(start, NULL), (char *)(tmp + 1), end - tmp))
    return -1;
  return 0;
}

inline static int http1_consume_request_line(http1_parser_s *parser,
                                             uint8_t *start,
                                             uint8_t *end) {
  uint8_t *tmp = start;
  uint8_t *host_start = NULL;
  uint8_t *host_end = NULL;
  if (!seek2ch(&tmp, end, ' '))
    return -1;
  if (http1_on_method(parser, (char *)start, tmp - start))
    return -1;
  tmp = start = tmp + 1;
  if (start[0] == 'h' && start[1] == 't' && start[2] == 't' &&
      start[3] == 'p') {
    if (start[4] == ':' && start[5] == '/' && start[6] == '/') {
      /* Request URI is in long form... emulate Host header instead. */
      tmp = host_end = host_start = (start += 7);
    } else if (start[4] == 's' && start[5] == ':' && start[6] == '/' &&
               start[7] == '/') {
      /* Secure request is in long form... emulate Host header instead. */
      tmp = host_end = host_start = (start += 8);
    } else
      goto review_path;
    if (!seek2ch(&tmp, end, ' '))
      return -1;
    *tmp = ' ';
    if (!seek2ch(&host_end, tmp, '/')) {
      if (http1_on_path(parser, (char *)"/", 1))
        return -1;
      goto start_version;
    }
    host_end[0] = '/';
    start = host_end;
  }
review_path:
  tmp = start;
  if (seek2ch(&tmp, end, '?')) {
    if (http1_on_path(parser, (char *)start, tmp - start))
      return -1;
    tmp = start = tmp + 1;
    if (!seek2ch(&tmp, end, ' '))
      return -1;
    if (tmp - start > 0 && http1_on_query(parser, (char *)start, tmp - start))
      return -1;
  } else {
    tmp = start;
    if (!seek2ch(&tmp, end, ' '))
      return -1;
    if (http1_on_path(parser, (char *)start, tmp - start))
      return -1;
  }
start_version:
  start = tmp + 1;
  if (start + 5 >= end) /* require "HTTP/" */
    return -1;
  if (http1_on_version(parser, (char *)start, end - start))
    return -1;
  /* */
  if (host_start &&
      http1_on_header(
          parser, (char *)"host", 4, (char *)host_start, host_end - host_start))
    return -1;
  return 0;
}

inline static int
http1_consume_header(http1_parser_s *parser, uint8_t *start, uint8_t *end) {
  uint8_t *end_name = start;
  /* divide header name from data */
  if (!seek2ch(&end_name, end, ':'))
    return -1;
#if HTTP_HEADERS_LOWERCASE
  for (uint8_t *t = start; t < end_name; t++) {
    *t = http_tolower(*t);
  }
#endif
  uint8_t *start_value = end_name + 1;
  if (start_value[0] == ' ') {
    start_value++;
  };
  if ((end_name - start) == 14 &&
#if HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED && HTTP_HEADERS_LOWERCASE
      *((uint64_t *)start) == *((uint64_t *)"content-") &&
      *((uint64_t *)(start + 6)) == *((uint64_t *)"t-length")
#else
      HEADER_NAME_IS_EQ((char *)start, "content-length", 14)
#endif
  ) {
    /* handle the special `content-length` header */
    parser->state.content_length = http1_atol(start_value, NULL);
  } else if ((end_name - start) == 17 && (end - start_value) >= 7 &&
             !parser->state.content_length &&
#if HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED && HTTP_HEADERS_LOWERCASE
             *((uint64_t *)start) == *((uint64_t *)"transfer") &&
             *((uint64_t *)(start + 8)) == *((uint64_t *)"-encodin")
#else
             HEADER_NAME_IS_EQ((char *)start, "transfer-encoding", 17)
#endif
  ) {
    /* handle the special `transfer-encoding: chunked` header? */
    /* this removes the `chunked` marker and "unchunks" the data */
    if (
#if HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED
        (((uint32_t *)(start_value))[0] | 0x20202020) ==
            ((uint32_t *)"chun")[0] &&
        (((uint32_t *)(start_value + 3))[0] | 0x20202020) ==
            ((uint32_t *)"nked")[0]
#else
        ((start_value[0] | 32) == 'c' && (start_value[1] | 32) == 'h' &&
         (start_value[2] | 32) == 'u' && (start_value[3] | 32) == 'n' &&
         (start_value[4] | 32) == 'k' && (start_value[5] | 32) == 'e' &&
         (start_value[6] | 32) == 'd')
#endif
    ) {
      /* simple case,`chunked` at the beginning */
      parser->state.reserved |= 64;
      start_value += 7;
      while (start_value < end && (*start_value == ',' || *start_value == ' '))
        ++start_value;
      if (!(end - start_value))
        return 0;
    } else if ((end - start_value) > 7 &&
               ((end[(-7 + 0)] | 32) == 'c' && (end[(-7 + 1)] | 32) == 'h' &&
                (end[(-7 + 2)] | 32) == 'u' && (end[(-7 + 3)] | 32) == 'n' &&
                (end[(-7 + 4)] | 32) == 'k' && (end[(-7 + 5)] | 32) == 'e' &&
                (end[(-7 + 6)] | 32) == 'd')) {
      /* simple case,`chunked` at the end of list */
      parser->state.reserved |= 64;
      end -= 7;
      while (start_value < end && (end[-1] == ',' || end[-1] == ' '))
        --end;
      if (!(end - start_value))
        return 0;
    } else if ((end - start_value) > 7 && (end - start_value) < 256) {
      /* complex case, `the, chunked, marker, is in the middle of list */
      uint8_t val[256];
      size_t val_len = 0;
      while (start_value < end) {
        if (
#if HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED
            (((uint32_t *)(start_value))[0] | 0x20202020) ==
                ((uint32_t *)"chun")[0] &&
            (((uint32_t *)(start_value + 3))[0] | 0x20202020) ==
                ((uint32_t *)"nked")[0]
#else
            ((start_value[0] | 32) == 'c' && (start_value[1] | 32) == 'h' &&
             (start_value[2] | 32) == 'u' && (start_value[3] | 32) == 'n' &&
             (start_value[4] | 32) == 'k' && (start_value[5] | 32) == 'e' &&
             (start_value[6] | 32) == 'd')
#endif

        ) {
          parser->state.reserved |= 64;
          start_value += 7;
          /* skip comma / white space */
          while (start_value < end &&
                 (*start_value == ',' || *start_value == ' '))
            ++start_value;
          break;
        }
        val[val_len++] = *start_value;
        ++start_value;
      }
      while (start_value < end) {
        val[val_len++] = *start_value;
        ++start_value;
      }
      /* perform callback with `val` */
      val[val_len] = 0;
      if (val_len &&
          http1_on_header(
              parser, (char *)start, (end_name - start), (char *)val, val_len))
        return -1;
      return 0;
    }
  } else if ((end_name - start) == 7 && !parser->state.content_length &&
#if HTTP1_UNALIGNED_MEMORY_ACCESS_ENABLED && HTTP_HEADERS_LOWERCASE
             *((uint64_t *)start) == *((uint64_t *)"trailer")
#else
             HEADER_NAME_IS_EQ((char *)start, "trailer", 7)
#endif
  ) {
    /* chunked data with trailer... */
    parser->state.reserved |= 64;
    parser->state.reserved |= 32;
    // return 0; /* hide Trailer header, since we process the headers? */
  }
  /* perform callback */
  if (http1_on_header(parser,
                      (char *)start,
                      (end_name - start),
                      (char *)start_value,
                      end - start_value))
    return -1;
  return 0;
}

/* *****************************************************************************
HTTP/1.1 Body handling
***************************************************************************** */

inline static int http1_consume_body_streamed(http1_parser_s *parser,
                                              void *buffer,
                                              size_t length,
                                              uint8_t **start) {
  uint8_t *end = *start + parser->state.content_length - parser->state.read;
  uint8_t *const stop = ((uint8_t *)buffer) + length;
  if (end > stop)
    end = stop;
  if (end > *start &&
      http1_on_body_chunk(parser, (char *)(*start), end - *start))
    return -1;
  parser->state.read += (end - *start);
  *start = end;
  if (parser->state.content_length <= parser->state.read)
    parser->state.reserved |= 4;
  return 0;
}

inline static int http1_consume_body_chunked(http1_parser_s *parser,
                                             void *buffer,
                                             size_t length,
                                             uint8_t **start) {
  uint8_t *const stop = ((uint8_t *)buffer) + length;
  uint8_t *end = *start;
  while (*start < stop) {
    if (parser->state.content_length == 0) {
      if (end + 2 >= stop)
        return 0;
      if ((end[0] == '\r' && end[1] == '\n')) {
        /* remove tailing EOL that wasn't processed and retest */
        end += 2;
        *start = end;
        if (end + 2 >= stop)
          return 0;
      }
      long long chunk_len = http1_atol16(end, (const uint8_t **)&end);
      if (end + 2 > stop) /* overflowed? */
        return 0;
      if ((end[0] != '\r' || end[1] != '\n'))
        return -1; /* required EOL after content length */
      end += 2;

      parser->state.content_length = 0 - chunk_len;
      *start = end;
      if (parser->state.content_length == 0) {
        /* all chunked data was parsed */
        /* update content-length */
        parser->state.content_length = parser->state.read;
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
        { /* add virtual header ... ? */
          char buf[512];
          size_t buf_len = 512;
          size_t tmp_len = parser->state.read;
          buf[--buf_len] = 0;
          while (tmp_len) {
            size_t mod = tmp_len / 10;
            buf[--buf_len] = '0' + (tmp_len - (mod * 10));
            tmp_len = mod;
          }
          if (http1_on_header(parser,
                              "content-length",
                              14,
                              (char *)buf + buf_len,
                              511 - buf_len))
            return -1;
        }
#endif
        /* consume trailing EOL */
        if (*start + 2 <= stop)
          *start += 2;
        if (parser->state.reserved & 32) {
          /* remove the "headers complete" and "trailer" flags */
          parser->state.reserved &= 0xDD; /* 0xDD == ~2 & ~32 & 0xFF */
          return -2;
        }
        /* the parsing complete flag */
        parser->state.reserved |= 4;
        return 0;
      }
    }
    end = *start + (0 - parser->state.content_length);
    if (end > stop)
      end = stop;
    if (end > *start &&
        http1_on_body_chunk(parser, (char *)(*start), end - *start)) {
      return -1;
    }
    parser->state.read += (end - *start);
    parser->state.content_length += (end - *start);
    *start = end;
  }
  return 0;
}

inline static int http1_consume_body(http1_parser_s *parser,
                                     void *buffer,
                                     size_t length,
                                     uint8_t **start) {
  if (parser->state.content_length > 0 &&
      parser->state.content_length > parser->state.read) {
    /* normal, streamed data */
    return http1_consume_body_streamed(parser, buffer, length, start);
  } else if (parser->state.content_length <= 0 &&
             (parser->state.reserved & 64)) {
    /* chuncked encoding */
    return http1_consume_body_chunked(parser, buffer, length, start);
  } else {
    /* nothing to do - parsing complete */
    parser->state.reserved |= 4;
  }
  return 0;
}

/* *****************************************************************************
HTTP/1.1 parsre function
***************************************************************************** */
#if DEBUG
#include <assert.h>
#define HTTP1_ASSERT assert
#else
#define HTTP1_ASSERT(...)
#endif

/**
 * Returns the amount of data actually consumed by the parser.
 *
 * The value 0 indicates there wasn't enough data to be parsed and the same
 * buffer (with more data) should be resubmitted.
 *
 * A value smaller than the buffer size indicates that EITHER a request /
 * response was detected OR that the leftover could not be consumed because more
 * data was required.
 *
 * Simply resubmit the reminder of the data to continue parsing.
 *
 * A request / response callback automatically stops the parsing process,
 * allowing the user to adjust or refresh the state of the data.
 */
static size_t http1_parse(http1_parser_s *parser, void *buffer, size_t length) {
  if (!length)
    return 0;
  HTTP1_ASSERT(parser && buffer);
  parser->state.next = NULL;
  uint8_t *start = (uint8_t *)buffer;
  uint8_t *end = start;
  uint8_t *const stop = start + length;
  uint8_t eol_len = 0;
#define HTTP1_CONSUMED ((size_t)((uintptr_t)start - (uintptr_t)buffer))

re_eval:
  switch ((parser->state.reserved & 15)) {

  case 0: /* request / response line */
    /* clear out any leadinng white space */
    while ((start < stop) &&
           (*start == '\r' || *start == '\n' || *start == ' ' || *start == 0)) {
      ++start;
    }
    end = start;
    /* make sure the whole line is available*/
    if (!(eol_len = seek2eol(&end, stop)))
      return HTTP1_CONSUMED;

    if (start[0] == 'H' && start[1] == 'T' && start[2] == 'T' &&
        start[3] == 'P') {
      /* HTTP response */
      if (http1_consume_response_line(parser, start, end - eol_len + 1))
        goto error;
    } else if (http_tolower(start[0]) >= 'a' && http_tolower(start[0]) <= 'z') {
      /* HTTP request */
      if (http1_consume_request_line(parser, start, end - eol_len + 1))
        goto error;
    }
    end = start = end + 1;
    parser->state.reserved |= 1;

  /* fallthrough */
  case 1: /* headers */
    do {
      if (start >= stop)
        return HTTP1_CONSUMED; /* buffer ended on header line */
      if (*start == '\r' || *start == '\n') {
        goto finished_headers; /* empty line, end of headers */
      }
      if (!(eol_len = seek2eol(&end, stop)))
        return HTTP1_CONSUMED;
      if (http1_consume_header(parser, start, end - eol_len + 1))
        goto error;
      end = start = end + 1;
    } while ((parser->state.reserved & 2) == 0);
  finished_headers:
    ++start;
    if (*start == '\n')
      ++start;
    end = start;
    parser->state.reserved |= 2;
  /* fallthrough */
  case 3: /* request body */
  {       /*  2 | 1 == 3 */
    int t3 = http1_consume_body(parser, buffer, length, &start);
    switch (t3) {
    case -1:
      goto error;
    case -2:
      goto re_eval;
    }
    break;
  }
  }
  /* are we done ? */
  if (parser->state.reserved & 4) {
    parser->state.next = start;
    if (((parser->state.reserved & 128) ? http1_on_response
                                        : http1_on_request)(parser))
      goto error;
    parser->state = (struct http1_parser_protected_read_only_state_s){0};
  }
  return HTTP1_CONSUMED;
error:
  http1_on_error(parser);
  parser->state = (struct http1_parser_protected_read_only_state_s){0};
  return length;
#undef HTTP1_CONSUMED
}

/* *****************************************************************************




HTTP/1.1 TESTING




***************************************************************************** */
#ifdef HTTP1_TEST_PARSER
#include "signal.h"

#define HTTP1_TEST_ASSERT(cond, ...)                                           \
  if (!(cond)) {                                                               \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    kill(0, SIGINT);                                                           \
    exit(-1);                                                                  \
  }

static size_t http1_test_pos;
static char http1_test_temp_buf[8092];
static size_t http1_test_temp_buf_pos;
static struct {
  char *test_name;
  char *request[16];
  struct {
    char body[1024];
    size_t body_len;
    const char *method;
    ssize_t status;
    const char *path;
    const char *query;
    const char *version;
    struct http1_test_header_s {
      const char *name;
      size_t name_len;
      const char *val;
      size_t val_len;
    } headers[12];
  } result, expect;
} http1_test_data[] = {
    {
        .test_name = "simple empty request",
        .request = {"GET / HTTP/1.1\r\nHost:localhost\r\n\r\n"},
        .expect =
            {
                .body = "",
                .body_len = 0,
                .method = "GET",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {.name = "host",
                         .name_len = 4,
                         .val = "localhost",
                         .val_len = 9},
                    },
            },
    },
    {
        .test_name = "space before header data",
        .request = {"POST /my/path HTTP/1.2\r\nHost: localhost\r\n\r\n"},
        .expect =
            {
                .body = "",
                .body_len = 0,
                .method = "POST",
                .path = "/my/path",
                .query = NULL,
                .version = "HTTP/1.2",
                .headers =
                    {
                        {.name = "host",
                         .name_len = 4,
                         .val = "localhost",
                         .val_len = 9},
                    },
            },
    },
    {
        .test_name = "simple request, fragmented header (in new line)",
        .request = {"GET / HTTP/1.1\r\n", "Host:localhost\r\n\r\n"},
        .expect =
            {
                .body = "",
                .body_len = 0,
                .method = "GET",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {.name = "host",
                         .name_len = 4,
                         .val = "localhost",
                         .val_len = 9},
                    },
            },
    },
    {
        .test_name = "request with query",
        .request = {"METHOD /path?q=query HTTP/1.3\r\nHost:localhost\r\n\r\n"},
        .expect =
            {
                .body = "",
                .body_len = 0,
                .method = "METHOD",
                .path = "/path",
                .query = "q=query",
                .version = "HTTP/1.3",
                .headers =
                    {
                        {.name = "host",
                         .name_len = 4,
                         .val = "localhost",
                         .val_len = 9},
                    },
            },
    },
    {
        .test_name = "mid-fragmented header",
        .request = {"GET / HTTP/1.1\r\nHost: loca", "lhost\r\n\r\n"},
        .expect =
            {
                .body = "",
                .body_len = 0,
                .method = "GET",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {.name = "host",
                         .name_len = 4,
                         .val = "localhost",
                         .val_len = 9},
                    },
            },
    },
    {
        .test_name = "simple with body",
        .request = {"GET / HTTP/1.1\r\nHost:with body\r\n"
                    "Content-lEnGth: 5\r\n\r\nHello"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "GET",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
                    },
            },
    },
    {
        .test_name = "fragmented body",
        .request = {"GET / HTTP/1.1\r\nHost:with body\r\n",
                    "Content-lEnGth: 5\r\n\r\nHe",
                    "llo"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "GET",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
                    },
            },
    },
    {
        .test_name = "fragmented body 2 (cuts EOL)",
        .request = {"POST / HTTP/1.1\r\nHost:with body\r\n",
                    "Content-lEnGth: 5\r\n",
                    "\r\n",
                    "He",
                    "llo"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
                    },
            },
    },
    {
        .test_name = "chunked body (simple)",
        .request = {"POST / HTTP/1.1\r\nHost:with body\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "5\r\n"
                    "Hello"
                    "\r\n0\r\n\r\n"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (end of list)",
        .request = {"POST / HTTP/1.1\r\nHost:with body\r\n"
                    "Transfer-Encoding: gzip, chunked\r\n"
                    "\r\n"
                    "5\r\n"
                    "Hello"
                    "\r\n0\r\n\r\n"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
                        {
                            .name = "transfer-encoding",
                            .name_len = 17,
                            .val = "gzip",
                            .val_len = 4,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (middle of list)",
        .request = {"POST / HTTP/1.1\r\nHost:with body\r\n"
                    "Transfer-Encoding: gzip, chunked, foo\r\n"
                    "\r\n",
                    "5\r\n"
                    "Hello"
                    "\r\n0\r\n\r\n"},
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
                        {
                            .name = "transfer-encoding",
                            .name_len = 17,
                            .val = "gzip, foo",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (fragmented)",
        .request =
            {
                "POST / HTTP/1.1\r\nHost:with body\r\n",
                "Transfer-Encoding: chunked\r\n",
                "\r\n"
                "5\r\n",
                "He",
                "llo",
                "\r\n0\r\n\r\n",
            },
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (fragmented + multi-message)",
        .request =
            {
                "POST / HTTP/1.1\r\nHost:with body\r\n",
                "Transfer-Encoding: chunked\r\n",
                "\r\n"
                "2\r\n",
                "He",
                "3\r\nl",
                "lo",
                "\r\n0\r\n\r\n",
            },
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (fragmented + broken-multi-message)",
        .request =
            {
                "POST / HTTP/1.1\r\nHost:with body\r\n",
                "Transfer-Encoding: chunked\r\n",
                "\r\n",
                "2\r\n",
                "H",
                "e",
                "3\r\nl",
                "l"
                "o",
                "\r\n0\r\n\r\n",
            },
        .expect =
            {
                .body = "Hello",
                .body_len = 5,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "5",
                            .val_len = 1,
                        },
#endif
                    },
            },
    },
    {
        .test_name = "chunked body (...longer...)",
        .request =
            {
                "POST / HTTP/1.1\r\nHost:with body\r\n",
                "Transfer-Encoding: chunked\r\n",
                "\r\n",
                "4\r\n",
                "Wiki\r\n",
                "5\r\n",
                "pedia\r\n",
                "E\r\n",
                " in\r\n",
                "\r\n",
                "chunks.\r\n",
                "0\r\n",
                "\r\n",
            },
        .expect =
            {
                .body = "Wikipedia in\r\n\r\nchunks.",
                .body_len = 23,
                .method = "POST",
                .path = "/",
                .query = NULL,
                .version = "HTTP/1.1",
                .headers =
                    {
                        {
                            .name = "host",
                            .name_len = 4,
                            .val = "with body",
                            .val_len = 9,
                        },
#ifdef HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING
                        {
                            .name = "content-length",
                            .name_len = 14,
                            .val = "23",
                            .val_len = 2,
                        },
#endif
                    },
            },
    },
    /* stop marker */
    {
        .request = {NULL},
    },
};

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  (void)parser;
  return 0;
}
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  (void)parser;
  return 0;
}
/** called when a request method is parsed. */
static int
http1_on_method(http1_parser_s *parser, char *method, size_t method_len) {
  (void)parser;
  http1_test_data[http1_test_pos].result.method = method;
  HTTP1_TEST_ASSERT(method_len ==
                        strlen(http1_test_data[http1_test_pos].expect.method),
                    "method_len test error for: %s",
                    http1_test_data[http1_test_pos].test_name);
  return 0;
}
/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser,
                           size_t status,
                           char *status_str,
                           size_t len) {
  (void)parser;
  http1_test_data[http1_test_pos].result.status = status;
  http1_test_data[http1_test_pos].result.method = status_str;
  HTTP1_TEST_ASSERT(len ==
                        strlen(http1_test_data[http1_test_pos].expect.method),
                    "status length test error for: %s",
                    http1_test_data[http1_test_pos].test_name);
  return 0;
}
/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t len) {
  (void)parser;
  http1_test_data[http1_test_pos].result.path = path;
  HTTP1_TEST_ASSERT(len == strlen(http1_test_data[http1_test_pos].expect.path),
                    "path length test error for: %s",
                    http1_test_data[http1_test_pos].test_name);
  return 0;
}
/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query, size_t len) {
  (void)parser;
  http1_test_data[http1_test_pos].result.query = query;
  HTTP1_TEST_ASSERT(len == strlen(http1_test_data[http1_test_pos].expect.query),
                    "query length test error for: %s",
                    http1_test_data[http1_test_pos].test_name);
  return 0;
}
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_version(http1_parser_s *parser, char *version, size_t len) {
  (void)parser;
  http1_test_data[http1_test_pos].result.version = version;
  HTTP1_TEST_ASSERT(len ==
                        strlen(http1_test_data[http1_test_pos].expect.version),
                    "version length test error for: %s",
                    http1_test_data[http1_test_pos].test_name);
  return 0;
}
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser,
                           char *name,
                           size_t name_len,
                           char *val,
                           size_t val_len) {
  (void)parser;
  size_t pos = 0;
  while (http1_test_data[http1_test_pos].result.headers[pos].name && pos < 8)
    ++pos;
  HTTP1_TEST_ASSERT(pos < 8,
                    "header result overflow for: %s",
                    http1_test_data[http1_test_pos].test_name);
  memcpy(http1_test_temp_buf + http1_test_temp_buf_pos, name, name_len);
  name = http1_test_temp_buf + http1_test_temp_buf_pos;
  http1_test_temp_buf_pos += name_len;
  http1_test_temp_buf[http1_test_temp_buf_pos++] = 0;
  memcpy(http1_test_temp_buf + http1_test_temp_buf_pos, val, val_len);
  val = http1_test_temp_buf + http1_test_temp_buf_pos;
  http1_test_temp_buf_pos += val_len;
  http1_test_temp_buf[http1_test_temp_buf_pos++] = 0;
  http1_test_data[http1_test_pos].result.headers[pos].name = name;
  http1_test_data[http1_test_pos].result.headers[pos].name_len = name_len;
  http1_test_data[http1_test_pos].result.headers[pos].val = val;
  http1_test_data[http1_test_pos].result.headers[pos].val_len = val_len;
  return 0;
}
/** called when a body chunk is parsed. */
static int
http1_on_body_chunk(http1_parser_s *parser, char *data, size_t data_len) {
  (void)parser;
  http1_test_data[http1_test_pos]
      .result.body[http1_test_data[http1_test_pos].result.body_len] = 0;
  HTTP1_TEST_ASSERT(data_len +
                            http1_test_data[http1_test_pos].result.body_len <=
                        http1_test_data[http1_test_pos].expect.body_len,
                    "body overflow for: %s"
                    "\r\n Expect:\n%s\nGot:\n%s%s\n",
                    http1_test_data[http1_test_pos].test_name,
                    http1_test_data[http1_test_pos].expect.body,
                    http1_test_data[http1_test_pos].result.body,
                    data);
  memcpy(http1_test_data[http1_test_pos].result.body +
             http1_test_data[http1_test_pos].result.body_len,
         data,
         data_len);
  http1_test_data[http1_test_pos].result.body_len += data_len;
  http1_test_data[http1_test_pos]
      .result.body[http1_test_data[http1_test_pos].result.body_len] = 0;
  return 0;
}

/** called when a protocol error occurred. */
static int http1_on_error(http1_parser_s *parser) {
  (void)parser;
  http1_test_data[http1_test_pos].result.status = -1;
  return 0;
}

#define HTTP1_TEST_STRING_FIELD(field, i)                                      \
  HTTP1_TEST_ASSERT((!http1_test_data[i].expect.field &&                       \
                     !http1_test_data[i].result.field) ||                      \
                        http1_test_data[i].expect.field &&                     \
                            http1_test_data[i].result.field &&                 \
                            !memcmp(http1_test_data[i].expect.field,           \
                                    http1_test_data[i].result.field,           \
                                    strlen(http1_test_data[i].expect.field)),  \
                    "string field error for %s\n%s\n%s",                       \
                    http1_test_data[i].test_name,                              \
                    http1_test_data[i].expect.field,                           \
                    http1_test_data[i].result.field);
static void http1_parser_test(void) {
  http1_test_pos = 0;
  struct {
    const char *str;
    long long num;
    long long (*fn)(const uint8_t *, const uint8_t **);
  } atol_test[] = {
      {
          .str = "0",
          .num = 0,
          .fn = http1_atol,
      },
      {
          .str = "-0",
          .num = 0,
          .fn = http1_atol,
      },
      {
          .str = "1",
          .num = 1,
          .fn = http1_atol,
      },
      {
          .str = "-1",
          .num = -1,
          .fn = http1_atol,
      },
      {
          .str = "123456789",
          .num = 123456789,
          .fn = http1_atol,
      },
      {
          .str = "-123456789",
          .num = -123456789,
          .fn = http1_atol,
      },
      {
          .str = "0x0",
          .num = 0,
          .fn = http1_atol16,
      },
      {
          .str = "-0x0",
          .num = 0,
          .fn = http1_atol16,
      },
      {
          .str = "-0x1",
          .num = -1,
          .fn = http1_atol16,
      },
      {
          .str = "-f",
          .num = -15,
          .fn = http1_atol16,
      },
      {
          .str = "-20",
          .num = -32,
          .fn = http1_atol16,
      },
      {
          .str = "0xf0EAf9ff",
          .num = 0xf0eaf9ff,
          .fn = http1_atol16,
      },
      /* stop marker */
      {
          .str = NULL,
      },
  };
  fprintf(stderr, "* testing string=>number conversion\n");
  for (size_t i = 0; atol_test[i].str; ++i) {
    const uint8_t *end;
    fprintf(stderr, "  %s", atol_test[i].str);
    HTTP1_TEST_ASSERT(atol_test[i].fn((const uint8_t *)atol_test[i].str,
                                      &end) == atol_test[i].num,
                      "\nhttp1_atol error: %s != %lld",
                      atol_test[i].str,
                      atol_test[i].num);
    HTTP1_TEST_ASSERT((char *)end ==
                          (atol_test[i].str + strlen(atol_test[i].str)),
                      "\nhttp1_atol error: didn't end after (%s): %s",
                      atol_test[i].str,
                      (char *)end)
  }
  fprintf(stderr, "\n");
  for (unsigned long long i = 1; i; i <<= 1) {
    char tmp[128];
    size_t tmp_len = sprintf(tmp, "%llx", i);
    uint8_t *pos = (uint8_t *)tmp;
    HTTP1_TEST_ASSERT(http1_atol16(pos, (const uint8_t **)&pos) ==
                              (long long)i &&
                          pos == (uint8_t *)(tmp + tmp_len),
                      "http1_atol16 roundtrip error.");
  }

  for (size_t i = 0; http1_test_data[i].request[0]; ++i) {
    fprintf(stderr, "* http1 parser test: %s\n", http1_test_data[i].test_name);
    /* parse each request / response */
    http1_parser_s parser = HTTP1_PARSER_INIT;
    char buf[4096];
    size_t r = 0;
    size_t w = 0;
    http1_test_temp_buf_pos = 0;
    for (int j = 0; http1_test_data[i].request[j]; ++j) {
      memcpy(buf + w,
             http1_test_data[i].request[j],
             strlen(http1_test_data[i].request[j]));
      w += strlen(http1_test_data[i].request[j]);
      size_t p = http1_parse(&parser, buf + r, w - r);
      r += p;
      HTTP1_TEST_ASSERT(r <= w, "parser consumed more than the buffer holds!");
    }
    /* test each request / response before overwriting the buffer */
    HTTP1_TEST_STRING_FIELD(body, i);
    HTTP1_TEST_STRING_FIELD(method, i);
    HTTP1_TEST_STRING_FIELD(path, i);
    HTTP1_TEST_STRING_FIELD(version, i);
    r = 0;
    while (http1_test_data[i].result.headers[r].name) {
      HTTP1_TEST_STRING_FIELD(headers[r].name, i);
      HTTP1_TEST_STRING_FIELD(headers[r].val, i);
      HTTP1_TEST_ASSERT(http1_test_data[i].expect.headers[r].val_len ==
                                http1_test_data[i].result.headers[r].val_len &&
                            http1_test_data[i].expect.headers[r].name_len ==
                                http1_test_data[i].result.headers[r].name_len,
                        "--- name / value length error");
      ++r;
    }
    HTTP1_TEST_ASSERT(!http1_test_data[i].expect.headers[r].name,
                      "Expected header missing:\n\t%s: %s",
                      http1_test_data[i].expect.headers[r].name,
                      http1_test_data[i].expect.headers[r].val);
    /* advance counter */
    ++http1_test_pos;
  }
}

#endif
