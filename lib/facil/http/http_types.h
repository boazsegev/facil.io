/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_TYPES_H
#define H_HTTP_TYPES_H

#include "fiobj.h"
#include <time.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/** A generic type used for HTTP request data. */
typedef struct http_req_s {
  /** the HTTP request's "head" starts with a private data used by facil.io */
  struct {
    /** a link to the connection's "owner" protocol. */
    void *owner;
    /** a link to the response object, if any.*/
    void *response;
    /** a private request ID, used by the owner (facil.io), do not touch. */
    uintptr_t request_id;
  } private;
  /** a time merker indicating when the request was received. */
  time_t received_at;
  /** a hash of general header data. When a header is set multiple times (such
   * as cookie headers), an Array will be used instead of a String. */
  fiobj_s *headers;
  /**
   * a placeholder for a hash of cookie data.
   * the hash will be initialized when parsing the request.
   */
  fiobj_s *cookies;
  /**
   * a placeholder for a hash of request data.
   * the hash will be initialized when parsing the request.
   */
  fiobj_s *params;
  /** a reader for body data (might be a temporary file or a string or NULL). */
  fiobj_s *body;

} http_req_s;

/* support C++ */
#ifdef __cplusplus
}
#endif

#endif /* H_HTTP_TYPES_H */
