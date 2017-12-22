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
Compile Time Settings
***************************************************************************** */

/** When a new connection is accepted, it will be immediately declined with a
 * 503 service unavailable (server busy) response unless the following number of
 * file descriptors is available.*/
#ifndef HTTP_BUSY_UNLESS_HAS_FDS
#define HTTP_BUSY_UNLESS_HAS_FDS 64
#endif

#ifndef HTTP_DEFAULT_BODY_LIMIT
#define HTTP_DEFAULT_BODY_LIMIT (1024 * 1024 * 50)
#endif

/* *****************************************************************************
The Request / Response type and functions
***************************************************************************** */

/**
 * A generic type used for HTTP request/response data.
 *
 * The `http_s` data can only be accessed safely from within the `on_request`
 * HTTP callback OR an `http_defer` callback.
 */
typedef struct {
  /** the HTTP request's "head" starts with a private data used by facil.io */
  struct {
    /** the connection's owner - used by facil.io, don't use directly! */
    protocol_s *owner;
    /** The response headers, if they weren't sent. Don't access directly. */
    fiobj_s *out_headers;
    /** a private request ID, used by the owner (facil.io), do not touch. */
    uintptr_t request_id;
  } private;
  /** a time merker indicating when the request was received. */
  struct timespec received_at;
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
  /** an opaque user data pointer, to be used BEFORE calling `http_defer`. */
  void *udata;
} http_s;

/**
* This is a helper for setting cookie data.

This struct is used together with the `http_response_set_cookie`. i.e.:

      http_response_set_cookie(response,
        .name = "my_cookie",
        .value = "data" );

*/
typedef struct {
  /** The cookie's name (Symbol). */
  fiobj_s *name;
  /** The cookie's value (leave blank to delete cookie). */
  char *value;
  /** The cookie's domain (optional). */
  char *domain;
  /** The cookie's path (optional). */
  char *path;
  /** The cookie value's size in bytes or a terminating NULL will be assumed.*/
  size_t value_len;
  /** The cookie domain's size in bytes or a terminating NULL will be assumed.*/
  size_t domain_len;
  /** The cookie path's size in bytes or a terminating NULL will be assumed.*/
  size_t path_len;
  /** Max Age (how long should the cookie persist), in seconds (0 == session).*/
  int max_age;
  /** Limit cookie to secure connections.*/
  unsigned secure : 1;
  /** Limit cookie to HTTP (intended to prevent javascript access/hijacking).*/
  unsigned http_only : 1;
} http_cookie_args_s;

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header(http_s *r, fiobj_s *name, fiobj_s *value);

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header2(http_s *r, fio_cstr_s name, fio_cstr_s value);

/**
 * Sets a response cookie, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_cookie(http_s *r, http_cookie_args_s);
#define http_set_cookie(http__req__, ...)                                      \
  http_set_cookie((http__req__), (http_cookie_args_s){__VA_ARGS__})

/**
 * Sends the response headers and body.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_send_body(http_s *r, void *data, uintptr_t length);

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_s *r, int fd, uintptr_t length, uintptr_t offset);

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile2(http_s *r, char *filename, size_t name_length);

/**
 * Sends an HTTP error response.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 *
 * The `uuid` argument is optional and will be used only if the `http_s`
 * argument is set to NULL.
 */
int http_send_error(http_s *r, intptr_t uuid, size_t error);

/**
 * Sends the response headers and starts streaming. Use `http_defer` to continue
 * straming.
 *
 * Returns -1 on error and 0 on success.
 */
int http_stream(http_s *r, void *data, uintptr_t length);

/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_s *r);

/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_s *r, void *data, uintptr_t length, char *mime_type,
                   uintptr_t type_length);

/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using `filename`
 * will be made.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_s *r, char *filename, size_t name_length,
                   char *mime_type, uintptr_t type_length);

/**
 * Defers the request / response handling for later.
 *
 * Returns -1 on error and 0 on success.
 *
 * Note: HTTP/1.1 requests CAN'T be deferred due to protocol requirements.
 */
int http_defer(http_s *r, void (*task)(http_s *r));

/* *****************************************************************************
Listening to HTTP connections
***************************************************************************** */

/** The HTTP settings. */
typedef struct http_settings_s {
  /** REQUIRED: the callback to be performed when requests come in. */
  void (*on_request)(http_s *request);
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
  void (*on_finish)(struct http_settings_s *settings);
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

/**
 * Listens to HTTP connections at the specified `port`.
 *
 * Leave as NULL to ignore IP binding.
 *
 * Returns -1 on error and 0 on success.
 */
int http_listen(const char *port, const char *binding, struct http_settings_s);
/** Listens to HTTP connections at the specified `port` and `binding`. */
#define http_listen(port, binding, ...)                                        \
  http_listen((port), (binding), (struct http_settings_s){__VA_ARGS__})

/**
 * Returns the settings used to setup the connection.
 *
 * Returns -1 on error and 0 on success.
 */
struct http_settings_s *http_settings(http_s *r);

/* *****************************************************************************
TODO: HTTP client mode
***************************************************************************** */

/* *****************************************************************************
HTTP Helper functions that might be used globally
***************************************************************************** */

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *http_gmtime(const time_t *timer, struct tm *tmbuf);

/**
Writes an HTTP date string to the `target` buffer.

This requires ~32 bytes of space to be available at the target buffer (unless
it's a super funky year, 32 bytes is about 3 more than you need).

Returns the number of bytes actually written.
*/
size_t http_date2str(char *target, struct tm *tmbuf);
/** An alternative, RFC 2109 date representation. Requires */
size_t http_date2rfc2109(char *target, struct tm *tmbuf);
/** An alternative, RFC 2822 date representation. */
size_t http_date2rfc2822(char *target, struct tm *tmbuf);

/**
 * Prints Unix time to a HTTP time formatted string.
 *
 * This variation implements chached results for faster processeing, at the
 * price of a less accurate string.
 */
size_t http_time2str(char *target, const time_t t);

/** Decodes a URL encoded string, no buffer overflow protection. */
ssize_t http_decode_url_unsafe(char *dest, const char *url_data);

/** Decodes a URL encoded string (i.e., the "query" part of a request). */
ssize_t http_decode_url(char *dest, const char *url_data, size_t length);

/** Decodes the "path" part of a request, no buffer overflow protection. */
ssize_t http_decode_path_unsafe(char *dest, const char *url_data);

/** Decodes the "path" part of an HTTP request, no buffer overflow protection.
 */
ssize_t http_decode_path(char *dest, const char *url_data, size_t length);

/* support C++ */
#ifdef __cplusplus
}
#endif

#endif /* H_HTTP_H */
