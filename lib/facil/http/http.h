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

/** the `http_listen settings, see detils in the struct definition. */
typedef struct http_settings_s http_settings_s;

/* *****************************************************************************
The Request / Response type and functions
***************************************************************************** */

/**
 * A generic HTTP handle used for HTTP request/response data.
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
  } private_data;
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
  char *name;
  /** The cookie's value (leave blank to delete cookie). */
  char *value;
  /** The cookie's domain (optional). */
  char *domain;
  /** The cookie's path (optional). */
  char *path;
  /** The cookie name's size in bytes or a terminating NULL will be assumed.*/
  size_t name_len;
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
int http_set_header(http_s *h, fiobj_s *name, fiobj_s *value);

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header2(http_s *h, fio_cstr_s name, fio_cstr_s value);

/**
 * Sets a response cookie, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 *
 * Note: Long cookie names and long cookie values will be considered a security
 * vaiolation and an error will be returned. It should be noted that most
 * proxies and servers will refuse long cookie names or values and many impose
 * total header lengths (including cookies) of ~8Kib.
 */
int http_set_cookie(http_s *h, http_cookie_args_s);
#define http_set_cookie(http___handle, ...)                                    \
  http_set_cookie((http___handle), (http_cookie_args_s){__VA_ARGS__})

/**
 * Sends the response headers and body.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_send_body(http_s *h, void *data, uintptr_t length);

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_s *h, int fd, uintptr_t length, uintptr_t offset);

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * The `filename` variable MUST be a dynamic String (allocated with
 * `fiobj_str_new`), since it might be extended to include the `.gz` extension
 * for automatic `gzip` detection.
 *
 * Remember to call `fiobj_free` unless the file name will be used again.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile2(http_s *h, fiobj_s *filename);

/**
 * Sends an HTTP error response.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 *
 * The `uuid` and `settings` arguments are only required if the `http_s` handle
 * is NULL.
 */
int http_send_error(http_s *h, size_t error);

/**
 * Sends the response headers and starts streaming. Use `http_defer` to continue
 * straming.
 *
 * Returns -1 on error and 0 on success.
 */
int http_stream(http_s *h, void *data, uintptr_t length);

/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_s *h);

/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_s *h, void *data, uintptr_t length, fiobj_s *mime_type);

/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using `filename`
 * will be made.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_s *h, fiobj_s *filename, fiobj_s *mime_type);

/**
 * Defers the request / response handling for later and INVALIDATES the current
 * `http_s` handle.
 *
 * The `task` MUST call one of the `http_send_*`, `http_stream`, `http_finish`
 * or `http_defer`functions.
 *
 * The (optional) `fallback` is for read only puposes (most importantly reading
 * the `udata` and/or freeing the `udata` and it's resources). It MUST NOT call
 * any of the `http_` functions.
 *
 * Returns -1 on error and 0 on success.
 *
 * Note: the currecnt `http_s` handle will become invalid once this function is
 *    called and it's data might be deallocated, invalid or used by a different
 *    thread.
 *
 * Note: HTTP/1.1 requests CAN'T be deferred due to protocol requirements.
 */
int http_defer(http_s *h, void (*task)(http_s *h), void (*fallback)(http_s *h));

/* *****************************************************************************
Websocket Upgrade (server side)
***************************************************************************** */

/**
 * The type for a Websocket handle, used to identify a Websocket connection.
 *
 * Similar to an `http_s` handle, it is only valid within the scope of the
 * specific connection (the callbacks / tasks) and shouldn't be stored or
 * accessed otherwise.
 */
typedef struct ws_s ws_s;

/**
 * This struct is used for the named agruments in the `http_upgrade2ws`
 * function and macro.
 */
typedef struct {
  /** REQUIRED: The `http_s` to be initiating the websocket connection.*/
  http_s *http;
  /**
   * The (optional) on_message callback will be called whenever a websocket
   * message is received for this connection.
   *
   * The data received points to the websocket's message buffer and it will be
   * overwritten once the function exits (it cannot be saved for later, but it
   * can be copied).
   */
  void (*on_message)(ws_s *ws, char *data, size_t size, uint8_t is_text);
  /**
   * The (optional) on_open callback will be called once the websocket
   * connection is established and before is is registered with `facil`, so no
   * `on_message` events are raised before `on_open` returns.
   */
  void (*on_open)(ws_s *ws);
  /**
   * The (optional) on_ready callback will be after a the underlying socket's
   * buffer changes it's state from full to available.
   *
   * If the socket's buffer is never full, the callback is never called.
   *
   * It should be noted that `libsock` manages the socket's buffer overflow and
   * implements and augmenting user-land buffer, allowing data to be safely
   * written to the websocket without worrying over the socket's buffer.
   */
  void (*on_ready)(ws_s *ws);
  /**
   * The (optional) on_shutdown callback will be called if a websocket
   * connection is still open while the server is shutting down (called before
   * `on_close`).
   */
  void (*on_shutdown)(ws_s *ws);
  /**
   * The (optional) on_close callback will be called once a websocket connection
   * is terminated or failed to be established.
   */
  void (*on_close)(ws_s *ws);
  /** Opaque user data. */
  void *udata;
} websocket_settings_s;

/**
 * Upgrades an HTTP/1.1 connection to a Websocket connection.
 */
void http_upgrade2ws(websocket_settings_s);

/** This macro allows easy access to the `websocket_upgrade` function. The macro
 * allows the use of named arguments, using the `websocket_settings_s` struct
 * members. i.e.:
 *
 *     on_message(ws_s * ws, char * data, size_t size, int is_text) {
 *        ; // ... this is the websocket on_message callback
 *        websocket_write(ws, data, size, is_text); // a simple echo example
 *     }
 *
 *     on_upgrade(http_s* h) {
 *        http_upgrade2ws( .http = h, .on_message = on_message);
 *     }
 */
#define http_upgrade2ws(...)                                                   \
  http_upgrade2ws((websocket_settings_s){__VA_ARGS__})

#include "websockets.h"

/* *****************************************************************************
Listening to HTTP connections (client mode functions are later on...)
***************************************************************************** */

/** The HTTP settings. */
typedef struct http_settings_s {
  /** REQUIRED: the callback to be performed when HTTP requests come in. */
  void (*on_request)(http_s *request);
  /** (optional) the callback to be performed when Upgrade requests come in. */
  void (*on_upgrade)(http_s *request, char *requested_protocol, size_t len);
  /** (optional) the callback to be performed when the HTTP service closes. */
  void (*on_finish)(struct http_settings_s *settings);
  /** Opaque user data. Facil.io will ignore this field, but you can use it. */
  void *udata;
  /**
   * A public folder for file transfers - allows to circumvent any application
   * layer server and simply serve files.
   */
  const char *public_folder;
  /**
   * The length of the public_folder string.
   */
  size_t public_folder_length;
  /**
   * The maximum size of an HTTP request's body (when posting data).
   *
   * Defaults to ~ 50Mb.
   */
  size_t max_body_size;
  /** An HTTP/1.x connection timeout. Defaults to ~5 seconds.*/
  uint8_t timeout;
  /**
   * The maximum websocket message size/buffer (in bytes) for Websocket
   * connections. Defaults to ~250KB.
   */
  size_t ws_max_msg_size;
  /**
   * Timeout for the websocket connections, a ping will be sent  whenever the
   * timeout is reached. Defaults to 40 seconds.
   *
   * Connections are only closed when a ping cannot be sent (the network layer
   * fails). Pongs aren't reviewed.
   */
  uint8_t ws_timeout;
  /** Logging flag - set to TRUE to log HTTP requests. */
  uint8_t log;
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
struct http_settings_s *http_settings(http_s *h);
/* *****************************************************************************
TODO: HTTP client mode
***************************************************************************** */

/* *****************************************************************************
HTTP Helper functions that might be used globally
***************************************************************************** */

/* *****************************************************************************
HTTP Status Strings and Mime-Type helpers
***************************************************************************** */

/** Returns a human readable string related to the HTTP status number. */
fio_cstr_s http_status2str(uintptr_t status);

/** Registers a Mime-Type to be associated with the file extension. */
void http_mimetype_register(char *file_ext, size_t file_ext_len,
                            fiobj_s *mime_type_str);

/**
 * Finds the mime-type associated with the file extension.
 *  Remember to call `fiobj_free`.
 */
fiobj_s *http_mimetype_find(char *file_ext, size_t file_ext_len);

/** Clears the Mime-Type registry (it will be emoty afterthis call). */
void http_mimetype_clear(void);

/* *****************************************************************************
Commonly used headers (fiobj Symbol objects)
***************************************************************************** */

extern fiobj_s *HTTP_HEADER_CACHE_CONTROL;
extern fiobj_s *HTTP_HEADER_CONNECTION;
extern fiobj_s *HTTP_HEADER_CONTENT_ENCODING;
extern fiobj_s *HTTP_HEADER_CONTENT_LENGTH;
extern fiobj_s *HTTP_HEADER_CONTENT_RANGE;
extern fiobj_s *HTTP_HEADER_CONTENT_TYPE;
extern fiobj_s *HTTP_HEADER_COOKIE;
extern fiobj_s *HTTP_HEADER_DATE;
extern fiobj_s *HTTP_HEADER_ETAG;
extern fiobj_s *HTTP_HEADER_LAST_MODIFIED;
extern fiobj_s *HTTP_HEADER_SET_COOKIE;
extern fiobj_s *HTTP_HEADER_UPGRADE;

/* *****************************************************************************
HTTP General Helper functions that might be used globally
***************************************************************************** */

/**
 * Returns a String object representing the unparsed HTTP request (HTTP version
 * is capped at HTTP/1.1). Mostly usable for proxy usage and debugging.
 */
fiobj_s *http_req2str(http_s *h);

/**
 * Writes a log line to `stderr` about the request / response object.
 *
 * This function is called automatically if the `.log` setting is enabled.
 */
void http_write_log(http_s *h);
/* *****************************************************************************
HTTP Time related helper functions that might be used globally
***************************************************************************** */

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *http_gmtime(time_t timer, struct tm *tmbuf);

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

/* *****************************************************************************
HTTP URL decoding helper functions that might be used globally
***************************************************************************** */

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
