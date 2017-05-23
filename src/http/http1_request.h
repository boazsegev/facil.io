/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP1_REQUEST_H
#define H_HTTP1_REQUEST_H
#include "http1.h"
#include "http_request.h"

/* *****************************************************************************
Data Structure
***************************************************************************** */
typedef struct {
  http_request_s request;
  size_t buffer_pos;
  size_t header_pos;
  http_header_s headers[HTTP1_MAX_HEADER_COUNT];
  char buffer[HTTP1_MAX_HEADER_SIZE];
} http1_request_s;

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's request object. */
http_request_s *http1_request_create(void);
/** Destroys the request object. */
void http1_request_destroy(http_request_s *);
/** Recycle's the request object, clearing it's data. */
void http1_request_clear(http_request_s *request);
/** Duplicates a request object. */
http_request_s *http1_request_dup(http_request_s *);

/* *****************************************************************************
Header Access
***************************************************************************** */

/** searches for a header in the request's data store, returning a `header_s`
 * structure with all it's data.
 *
 * This doesn't effect header iteration.
 */
http_header_s http1_request_header_find(http_request_s *request,
                                        const char *header, size_t header_len);
/** Starts iterating the header list, returning the first header. Header
 * iteration is NOT thread-safe. */
http_header_s http1_request_header_first(http_request_s *request);
/**
 * Continues iterating the header list.
 *
 * Returns NULL header data if at end of list (header.name == NULL);
 *
 * Header itteration is NOT thread-safe. */
http_header_s http1_request_header_next(http_request_s *request);

#endif /* H_HTTP_REQUEST_H */
