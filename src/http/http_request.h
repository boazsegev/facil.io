/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_REQUEST_H
#define H_HTTP_REQUEST_H
#include <stdlib.h>

typedef struct {
  const char *name;
  const char *value;
  uint16_t name_length;
  uint16_t value_length;
} http_headers_s;

typedef struct {
  /** the HTTP version, also controlls the `http_request_s` flavor. */
  uint8_t http_version;
  /** the HTTP connection identifier. */
  intptr_t fd;
  /** points to the HTTP method name. */
  const char *method;
  /** The portion of the request URL that comes before the '?', if any. */
  const char *path;
  /** The string length of the path (editing the path requires update). */
  /** The portion of the request URL that follows the ?, if any. */
  const char *query;
  /** Points to a version string. */
  const char *version;
  /** Points to the body's host header value (a required header). */
  const char *host;
  /** points to the body's content type header, if any. */
  const char *content_type;
  /** points to the Upgrade header, if any. */
  const char *upgrade;
  /** points to the Connection header, if any. */
  const char *connection;

  /** the body's content's length, in bytes (can be 0). */
  size_t content_length;
  /* string lengths */
  uint16_t method_len;
  uint16_t path_len;
  uint16_t query_len;
  uint16_t host_len;
  uint16_t content_type_len;
  uint16_t upgrade_len;
  uint16_t version_len;

  /**
  Points the body of the request, if the body exists and is stored in memory.
  Otherwise, NULL. */
  const char *body_str;
  /** points a tmpfile file descriptor containing the body of the request. */
  int body_file;
} http_request_s;

#endif /* H_HTTP_REQUEST_H */
