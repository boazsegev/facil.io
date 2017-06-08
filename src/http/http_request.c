/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "http.h"
#include "http1_request.h"

#include <signal.h>
#include <sys/types.h>

/* *****************************************************************************
Unsupported function placeholders
***************************************************************************** */

static http_request_s *fail_create(void) { return NULL; }

static void fail_destroy(http_request_s *req) {
  (void)req;
  fprintf(stderr, "ERROR: possible memory leak - request to be freed has "
                  "unsupported version.\n");
  kill(0, SIGINT), exit(9);
}

static http_request_s *fail_dup(http_request_s *req) {
  (void)req;
  return NULL;
}

static http_header_s http_request_header_find_fail(http_request_s *request,
                                                   const char *header,
                                                   size_t header_len) {
  (void)request;
  (void)header;
  (void)header_len;
  return (http_header_s){.name = NULL};
}
static http_header_s http_request_header_seek_fail(http_request_s *request) {
  (void)request;
  return (http_header_s){.name = NULL};
}

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's request object. */
http_request_s *http_request_create(enum HTTP_VERSION ver) {
  static http_request_s *(*const vtable[2])(void) = {
      http1_request_create /* HTTP_V1 */, fail_create /* HTTP_V2 */};
  return vtable[ver]();
}
/** Destroys the request object. */
void http_request_destroy(http_request_s *request) {
  static void (*const vtable[2])(http_request_s *) = {
      http1_request_destroy /* HTTP_V1 */, fail_destroy /* HTTP_V2 */};
  vtable[request->http_version](request);
}
/** Recycle's the request object, clearing it's data. */
void http_request_clear(http_request_s *request) {
  static void (*const vtable[2])(http_request_s *) = {
      http1_request_clear /* HTTP_V1 */, fail_destroy /* HTTP_V2 */};
  vtable[request->http_version](request);
}

/** Duplicates a request object. */
http_request_s *http_request_dup(http_request_s *request) {
  static http_request_s *(*const vtable[2])(http_request_s *) = {
      http1_request_dup /* HTTP_V1 */, fail_dup /* HTTP_V2 */};
  return vtable[request->http_version](request);
}

/* *****************************************************************************
Header Access
***************************************************************************** */

/** searches for a header in the request's data store, returning a `header_s`
 * structure with all it's data.*/
http_header_s http_request_header_find(http_request_s *request,
                                       const char *header, size_t header_len) {
  static http_header_s (*const vtable[2])(http_request_s *, const char *,
                                          size_t) = {
      http1_request_header_find /* HTTP_V1 */,
      http_request_header_find_fail /* HTTP_V2 */};
  return vtable[request->http_version](request, header, header_len);
}
/** Starts itterating the header list, returning the first header. Header
 * itteration is NOT thread-safe. */
http_header_s http_request_header_first(http_request_s *request) {
  static http_header_s (*const vtable[2])(http_request_s *) = {
      http1_request_header_first /* HTTP_V1 */,
      http_request_header_seek_fail /* HTTP_V2 */};
  return vtable[request->http_version](request);
}
/**
 * Continues itterating the header list.
 *
 * Returns NULL header data if at end of list (header.name == NULL);
 *
 * Header itteration is NOT thread-safe. */
http_header_s http_request_header_next(http_request_s *request) {
  static http_header_s (*const vtable[2])(http_request_s *) = {
      http1_request_header_next /* HTTP_V1 */,
      http_request_header_seek_fail /* HTTP_V2 */};
  return vtable[request->http_version](request);
}
