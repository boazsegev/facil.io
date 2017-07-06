/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_REQUEST_H
#define H_HTTP_REQUEST_H
typedef struct http_request_s http_request_s;

#include "http.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

struct http_request_s {
  /** the HTTP version, also controlls the `http_request_s` flavor. */
  enum HTTP_VERSION http_version;
  /** A pointer to the protocol's settings */
  const http_settings_s *settings;
  /** the HTTP connection identifier. */
  intptr_t fd;
  /** this is an opaque pointer, doubles for request pooling / chaining */
  void *udata;
  /** points to the HTTP method name. */
  const char *method;
  /** The portion of the request URL that comes before the '?', if any. */
  const char *path;
  /** The portion of the request URL that follows the ?, if any. */
  const char *query;
  /** Points to a version string, if any. */
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
  /** Points the body of the request, if the body exists and is stored in
   * memory. Otherwise, NULL. */
  const char *body_str;
  /** points a tmpfile file descriptor containing the body of the request (if
   * not in memory). */
  int body_file;
  /* string lengths */
  uint16_t method_len;
  uint16_t path_len;
  uint16_t query_len;
  uint16_t host_len;
  uint16_t content_type_len;
  uint16_t upgrade_len;
  uint16_t version_len;
  uint16_t connection_len;
  uint16_t headers_count;
};

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's request object. */
http_request_s *http_request_create(enum HTTP_VERSION);
/** Destroys the request object. */
void http_request_destroy(http_request_s *);
/** Recycle's the request object, clearing it's data. */
void http_request_clear(http_request_s *request);
/** Duplicates a request object. */
http_request_s *http_request_dup(http_request_s *);

/* *****************************************************************************
Header Access
***************************************************************************** */

/** searches for a header in the request's data store, returning a `header_s`
 * structure with all it's data.
 *
 * This doesn't effect header iteration.
 */
http_header_s http_request_header_find(http_request_s *request,
                                       const char *header, size_t header_len);
/** Starts iterating the header list, returning the first header. Header
 * iteration is NOT thread-safe. */
http_header_s http_request_header_first(http_request_s *request);
/**
 * Continues iterating the header list.
 *
 * Returns NULL header data if at end of list (header.name == NULL);
 *
 * Header itteration is NOT thread-safe. */
http_header_s http_request_header_next(http_request_s *request);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* H_HTTP_REQUEST_H */
