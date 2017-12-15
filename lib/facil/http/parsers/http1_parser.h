#ifndef H_HTTP1_PARSER_H
/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

/**
This is a callback based parser. It parses the skeleton of the HTTP/1.x protocol
and leaves most of the work (validation, error checks, etc') to the callbacks.

This is an attempt to replace the existing HTTP/1.x parser with something easier
to maintain and that could be used for an HTTP/1.x client as well.
*/
#define H_HTTP1_PARSER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef HTTP_HEADERS_LOWERCASE
/** when defined, HTTP headers will be converted to lowercase and header
 * searches will be case sensitive. */
#define HTTP_HEADERS_LOWERCASE 1
#endif

#if HTTP_HEADERS_LOWERCASE

#define HEADER_NAME_IS_EQ(var_name, const_name, len)                           \
  (!memcmp((var_name), (const_name), (len)))
#else
#define HEADER_NAME_IS_EQ(var_name, const_name, len)                           \
  (!strncasecmp((var_name), (const_name), (len)))
#endif

/** this struct contains the state of the parser. */
typedef struct http1_parser_s {
  void *udata;
  struct http1_parser_protected_read_only_state_s {
    ssize_t content_length; /* negative values indicate chuncked data state */
    ssize_t read;           /* total number of bytes read so far (body only) */
    uint8_t reserved;       /* for internal use */
  } state;
} http1_parser_s;

/**
 * Available options for the parsing function.
 *
 * Callbacks should return 0 unless an error occured.
 */
struct http1_fio_parser_args_s {
  /** REQUIRED: the parser object that manages the parser's state. */
  http1_parser_s *parser;
  /** REQUIRED: the data to be parsed. */
  void *buffer;
  /** REQUIRED: the length of the data to be parsed. */
  size_t length;
  /** called when a request was received. */
  int (*const on_request)(http1_parser_s *parser);
  /** called when a response was received. */
  int (*const on_response)(http1_parser_s *parser);
  /** called when a request method is parsed. */
  int (*const on_method)(http1_parser_s *parser, char *method,
                         size_t method_len);
  /** called when a response status is parsed. the status_str is the string
   * without the prefixed numerical status indicator.*/
  int (*const on_status)(http1_parser_s *parser, size_t status,
                         char *status_str, size_t len);
  /** called when a request path (excluding query) is parsed. */
  int (*const on_path)(http1_parser_s *parser, char *path, size_t path_len);
  /** called when a request path (excluding query) is parsed. */
  int (*const on_query)(http1_parser_s *parser, char *query, size_t query_len);
  /** called when a the HTTP/1.x version is parsed. */
  int (*const on_http_version)(http1_parser_s *parser, char *version,
                               size_t len);
  /** called when a header is parsed. */
  int (*const on_header)(http1_parser_s *parser, char *name, size_t name_len,
                         char *data, size_t data_len);
  /** called when a body chunk is parsed. */
  int (*const on_body_chunk)(http1_parser_s *parser, char *data,
                             size_t data_len);
  /** called when a protocol error occured. */
  int (*const on_error)(http1_parser_s *parser);
};

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
size_t http1_fio_parser_fn(struct http1_fio_parser_args_s *args);

static inline __attribute__((unused)) size_t
http1_fio_parser(struct http1_fio_parser_args_s args) {
  return http1_fio_parser_fn(&args);
}
#if __STDC_VERSION__ >= 199901L
#define http1_fio_parser(...)                                                  \
  http1_fio_parser((struct http1_fio_parser_args_s){__VA_ARGS__})
#endif

#endif
