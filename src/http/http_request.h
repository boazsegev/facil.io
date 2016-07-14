#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

typedef struct {
  const char* name;
  const char* value;
  uint16_t name_length;
  uint16_t value_length;
} http_headers_s;

typedef struct {
  /** points to the HTTP method name. */
  const char* method;
  /** The portion of the request URL that comes before the '?', if any. */
  const char* path;
  /** The string length of the path (editing the path requires update). */
  /** The portion of the request URL that follows the ?, if any. */
  const char* query;
  /** Points to a version string. */
  const char* version;
  /** Points to the body's host header value (a required header). */
  const char* host;
  /** points to the body's content type header, if any. */
  const char* content_type;
  /** points to the Upgrade header, if any. */
  const char* upgrade;
  /** points to the Connection header, if any. */
  const char* connection;

  /** the body's content's length, in bytes (can be 0). */
  size_t content_length;

  /* string lengths */

  uint16_t method_len;
  uint16_t path_len;
  uint16_t query_len;
  uint16_t host_len;
  uint16_t content_type_len;
  uint16_t upgrade_len;

  /** `version_len` is signed, to allow negative values (SPDY/HTTP2 etc). */
  int16_t version_len;

  /**
  Points the body of the request, if the body exists and is stored in memory.
  Otherwise, NULL. */
  const char* body_str;
  /** points a tmpfile file descriptor containing the body of the request. */
  int body_file;
  /** semi-private information. */
  struct {
    /**
    When pooling request objects, this points to the next object.
    In other times it may contain arbitrary data that can be used by the parser
    or implementation.
    */
    void* next;
    /**
    Implementation specific. This, conceptually, holds information about the
    "owner" of this request. */
    void* owner;
    /**
    Implementation specific. This, conceptually, holds the connection that
    "owns" this request, or an implementation identifier. */
    intptr_t fd;
    /** the current header position, for API or parser states. */
    uint16_t headers_pos;
    /** the maximum number of header space availble. */
    uint16_t max_headers;
  } metadata;

  uint16_t connection_len;
  uint16_t headers_count;

  http_headers_s headers[];
} http_request_s;

__unused static inline void http_request_clear(http_request_s* request) {
  if (request->body_file > 0) /* assumes no tempfile with fd 0 */
    close(request->body_file);
  *request = (http_request_s){
      .metadata.owner = request->metadata.owner,
      .metadata.fd = request->metadata.fd,
      .metadata.max_headers = request->metadata.max_headers,
  };
}

/** searches for a header in the header array, both reaturnning it's value and
 * setting it's position in the `request->metadata.header_pos` variable.*/
__unused static inline const char* http_request_find_header(
    http_request_s* request,
    const char* header,
    size_t header_len) {
  if (header == NULL || request == NULL)
    return NULL;
  if (header_len == 0)
    header_len = strlen(header);
  request->metadata.headers_pos = 0;
  while (request->metadata.headers_pos < request->headers_count) {
    if (header_len ==
            request->headers[request->metadata.headers_pos].name_length &&
        strncasecmp(request->headers[request->metadata.headers_pos].name,
                    header, header_len) == 0)
      return request->headers[request->metadata.headers_pos].value;
    request->metadata.headers_pos += 1;
  }
  return NULL;
}

#define HTTP_REQUEST_SIZE(header_count) \
  (sizeof(http_request_s) + ((header_count) * sizeof(http_headers_s)))

#endif
