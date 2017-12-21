/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_H
#define H_HTTP_H

#include "facil.h"
#include "fiobj.h"
#include <time.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
The Request / Response type and functions
***************************************************************************** */

/** A generic type used for HTTP request/response data. */
typedef struct http_req_s {
  /** the HTTP request's "head" starts with a private data used by facil.io */
  struct {
    /** the connection's identifier - used by facil.io, don't use directly! */
    uintptr_t uuid;
    /** The response headers, if they weren't sent. Don't access directly. */
    fiobj_s *response_headers;
    /** a private request ID, used by the owner (facil.io), do not touch. */
    uintptr_t request_id;
    /** a private request reference counter, do not touch. */
    uintptr_t ref_count;
  } private;
  /** a time merker indicating when the request was received. */
  time_t received_at;
  union {
    /** a String containing the method data (supports non-standard methods. */
    fiobj_s *method;
    /** The status string., if the object is a response. */
    fiobj_s *status_str;
  };
  /** The HTTP version string, if any. */
  fiobj_s *version;
  /** The status used for the response (or if the object is a response). */
  uintptr_t status;
  /** The request path, if any. */
  fiobj_s *path;
  /** The request query, if any. */
  fiobj_s *query;
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
  /**
   * a reader for body data (might be a temporary file or a string or NULL).
   * see fiobj_io.h for details.
   */
  fiobj_s *body;
} http_req_s;

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header(http_req_s *r, fiobj_s *name, fiobj_s *value);
/**
 * Sets a response cookie, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_cookie(http_req_s *r, fiobj_s *name, fiobj_s *value);
/**
 * Sends the response headers and body.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_req_s` OBJECT IS NO LONGER VALID.
 */
int http_send_body(http_req_s *r, void *data, uintptr_t length);
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_req_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_req_s *r, int fd, uintptr_t length, uintptr_t offset);
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_req_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile2(http_req_s *r, char *filename, size_t name_length);
/**
 * Sends the response headers and starts streaming, creating a new and valid
 * `http_req_s` object that allows further streaming.
 *
 * Returns NULL on error and a new valid `http_req_s` object on success.
 *
 * THE OLD `http_req_s` OBJECT BECOMES INVALID.
 */
http_req_s *http_stream(http_req_s *r, void *data, uintptr_t length);
/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_req_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_req_s *r);
/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_req_s *r, void *data, uintptr_t length, char *mime_type,
                   uintptr_t type_length);
/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using `filename`
 * will be made.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_req_s *r, char *filename, size_t name_length,
                   char *mime_type, uintptr_t type_length);

/* *****************************************************************************
Listening to HTTP connections
***************************************************************************** */

/** The HTTP settings. */
typedef struct http_settings_s {
  /** REQUIRED: the callback to be performed when requests come in. */
  void (*on_request)(http_req_s *request);
  /**
   * A public folder for file transfers - allows to circumvent any application
   * layer server and simply serve files.
   */
  const char *public_folder;
  /**
   * The length of the public_folder string.
   */
  size_t public_folder_length;
  /** Opaque user data. Facil.io will ignore this field, but you can use it. */
  void *udata;
  /** (optional) the callback to be performed when the HTTP service closes. */
  void (*on_finish)(intptr_t uuid, void *udata);
  /**
   * Allows an implementation for the transport layer (i.e. TLS) without
   * effecting the HTTP protocol.
   */
  void (*set_rw_hooks)(intptr_t fduuid, void *rw_udata);
  /** Opaque user data for the optional `set_rw_hooks`. */
  void *rw_udata;
  /**
   * The maximum size of an HTTP request's body (when posting data).
   *
   * Defaults to ~ 50Mb.
   */
  size_t max_body_size;
  /** Logging flag - set to TRUE to log requests. */
  uint8_t log;
  /** An HTTP/1.x connection timeout. Defaults to ~5 seconds.*/
  uint8_t timeout;
} http_settings_s;

int http_listen(char *port, char *binding, struct http_settings_s);
#define http_listen(port, binding, ...)                                        \
  http_listen((port), (binding), (struct http_settings_s)(__VA_ARGS__))

/**
 * Returns the settings used to setup the connection.
 *
 * Returns -1 on error and 0 on success.
 */
struct http_settings_s *http_settings(http_req_s *r);

/* *****************************************************************************
TODO: HTTP client mode
***************************************************************************** */

/* support C++ */
#ifdef __cplusplus
}
#endif

#endif /* H_HTTP_H */
