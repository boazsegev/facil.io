/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_H
#define HTTP_H

#include "facil.h"

/** an HTTP/1.1 vs. HTTP/2 identifier. */
enum HTTP_VERSION { HTTP_V1 = 0, HTTP_V2 = 1 };

/** HTTP header information */
typedef struct {
  const char *name;
  union {
    const char *data;
    const char *value;
  };
  uint32_t name_len;
  union {
    uint32_t data_len;
    uint32_t value_len;
  };
} http_header_s;

/** Settings typedef */
typedef struct http_settings_s http_settings_s;

/* *****************************************************************************
Core include files
*/
// clang-format off
#include <time.h>
#include "http_request.h"
#include "http_response.h"
// clang-format on
/* *****************************************************************************
Hard Coded Settings
*/

/** When a new connection is accepted, it will be immediately declined with a
 * 503 service unavailable (server busy) response unless the following number of
 * file descriptors is available.*/
#ifndef HTTP_BUSY_UNLESS_HAS_FDS
#define HTTP_BUSY_UNLESS_HAS_FDS 64
#endif

#ifndef HTTP_DEFAULT_BODY_LIMIT
#define HTTP_DEFAULT_BODY_LIMIT (1024 * 1024 * 50)
#endif

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
HTTP Core API & data structure
*/

/** Manages protocol settings for the HTTP protocol */
struct http_settings_s {
  /**
  The maximum size of an HTTP request's body (when posting data).

  Defaults to ~ 50Mb.
  */
  size_t max_body_size;
  /** REQUIRED: the callback to be performed when requests come in. */
  void (*on_request)(http_request_s *request);
  /**
  A public folder for file transfers - allows to circumvent any application
  layer server and simply serve files.
  */
  const char *public_folder;
  /**
  The length of the public_folder string.
  */
  size_t public_folder_length;
  /** Opaque user data. */
  void *udata;
  /** Opaque user data for the optional `set_rw_hooks`. */
  void *rw_udata;
  /** (optional) the callback to be performed when the HTTP service closes. */
  void (*on_finish)(intptr_t uuid, void *udata);
  /**
   * Allows a an implementation for the transport layer (i.e. TLS) without
   * effecting the HTTP protocol.
   */
  sock_rw_hook_s *(*set_rw_hooks)(intptr_t fduuid, void *rw_udata);
  /**
   * A cleanup callback for the `rw_udata`.
   */
  void (*on_finish_rw)(intptr_t uuid, void *rw_udata);
  /**
  Logging flag - set to TRUE to log static file requests.

  Dynamic request logging is always the dynamic application's responsibility.
  */
  uint8_t log_static;
  /** An HTTP/1.x connection timeout. Defaults to ~5 seconds.*/
  uint8_t timeout;
  /**
  The default HTTP version which a new connection will use. At the moment, only
  version HTTP/1.1 is supported.
  */
  enum HTTP_VERSION version;
  /**
  internal flag for library use.
  */
  uint8_t private_metaflags;
};

typedef protocol_s *(*http_on_open_func)(intptr_t, void *);
typedef void (*http_on_finish_func)(void *);
/**
Return the callback used for creating the HTTP protocol in the `settings`.
*/
http_on_open_func http_get_on_open_func(http_settings_s *settings);
/**
Return the callback used for freeing the HTTP protocol in the `settings`.
*/
http_on_finish_func http_get_on_finish_func(http_settings_s *settings);

/**
Listens for incoming HTTP connections on the specified posrt and address,
implementing the requested settings.

Since facil.io doesn't support native TLS/SLL
*/
int http_listen(const char *port, const char *address,
                http_settings_s settings);

#define http_listen(port, address, ...)                                        \
  http_listen((port), (address), (http_settings_s){__VA_ARGS__})

/* *****************************************************************************
HTTP Helper functions that might be used globally
*/

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

/**
A fast, inline alternative to `sprintf(dest, "%lu", num)`.

Writes an **unsigned** number to a buffer, as a string. This is an unsafe
functions that assumes the buffer will have at least 21 bytes and isn't NULL.

A NULL terminating byte is written.

Returns the number of bytes actually written (excluding the NULL byte).
*/
static inline size_t http_ul2a(char *dest, size_t num) {
  uint8_t digits = 1;
  size_t tmp = num;
  while ((tmp /= 10))
    ++digits;

  dest += digits;
  *(dest--) = 0;
  for (size_t i = 0; i < digits; i++) {
    num = num - (10 * (tmp = (num / 10)));
    *(dest--) = '0' + num;
    num = tmp;
  }
  return digits;
}

/** Decodes a URL encoded string, no buffer overflow protection. */
ssize_t http_decode_url_unsafe(char *dest, const char *url_data);

/** Decodes a URL encoded string (i.e., the "query" part of a request). */
ssize_t http_decode_url(char *dest, const char *url_data, size_t length);

/** Decodes the "path" part of a request, no buffer overflow protection. */
ssize_t http_decode_path_unsafe(char *dest, const char *url_data);

/** Decodes the "path" part of an HTTP request, no buffer overflow protection.
 */
ssize_t http_decode_path(char *dest, const char *url_data, size_t length);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
