/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http1_request.h"
#include "http1.h"

#include <string.h>
#include <strings.h>

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's request object. */
http_request_s *http1_request_create(void) {
  http1_request_s *req = malloc(sizeof(*req));
  req->request.body_file = 0;
  http1_request_clear((http_request_s *)req);
  return (http_request_s *)req;
}
/** Destroys the request object. */
void http1_request_destroy(http_request_s *request) {
  http1_request_clear(request);
  free(request);
}
/** Recycle's the request object, clearing it's data. */
void http1_request_clear(http_request_s *request) {
  if (request->body_file)
    close(request->body_file);
  *request = (http_request_s){.http_version = HTTP_V1, .fd = request->fd};
  ((http1_request_s *)request)->buffer_pos = 0;
  ((http1_request_s *)request)->header_pos = 0;
}
/** Duplicates a request object. */
http_request_s *http1_request_dup(http_request_s *request) {
  http1_request_s *req = (http1_request_s *)http1_request_create();
  *req = *((http1_request_s *)request);
  return (http_request_s *)req;
}

/* *****************************************************************************
Header Access
***************************************************************************** */

/** searches for a header in the request's data store, returning a `header_s`
 * structure with all it's data.*/
http_header_s http1_request_header_find(http_request_s *request,
                                        const char *header, size_t header_len) {
  for (size_t i = 0; i < request->headers_count; i++) {
    if (((http1_request_s *)request)->headers[i].name_len == header_len &&
        strncasecmp((char *)header,
                    (char *)((http1_request_s *)request)->headers[i].name,
                    header_len) == 0)
      return ((http1_request_s *)request)->headers[i];
  }
  return (http_header_s){.name = NULL};
}
/** Starts itterating the header list, returning the first header. Header
 * itteration is NOT thread-safe. */
http_header_s http1_request_header_first(http_request_s *request) {
  ((http1_request_s *)request)->header_pos = 0;
  if (!request->headers_count)
    return (http_header_s){.name = NULL};
  return ((http1_request_s *)request)->headers[0];
}
/**
 * Continues itterating the header list.
 *
 * Returns NULL header data if at end of list (header.name == NULL);
 *
 * Header itteration is NOT thread-safe. */
http_header_s http1_request_header_next(http_request_s *request) {
  ((http1_request_s *)request)->header_pos++;
  if (((http1_request_s *)request)->header_pos >= request->headers_count)
    return (http_header_s){.name = NULL};
  return ((http1_request_s *)request)
      ->headers[((http1_request_s *)request)->header_pos];
}
