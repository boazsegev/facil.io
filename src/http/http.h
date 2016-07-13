
#ifndef HTTP_H
#define HTTP_H

/* *****************************************************************************
Core include files
*/
#include "libserver.h"
#include "http_request.h"
#include "http_response.h"
#include <time.h>
/* *****************************************************************************
Hard Coded Settings
*/

/** When a new connection is accepted, it will be immediately declined with a
 * 503 service unavailable (server busy) response unless the following number of
 * file descriptors is available.*/
#ifndef HTTP_BUSY_UNLESS_HAS_FDS
#define HTTP_BUSY_UNLESS_HAS_FDS 64
#endif

/* *****************************************************************************
HTTP settings / core data structure
*/

/** Manages protocol settings for the HTTP protocol */
typedef struct {
  /**
  The maximum size of an HTTP request's body (when posting data).

  Defaults to ~ 50Mb.
  */
  size_t max_body_size;
  /** the callback to be performed when requests come in. */
  void (*on_request)(http_request_s* request);
  /**
  A public folder for file transfers - allows to circumvent any application
  layer server and simply serve files.
  */
  const char* public_folder;
  /**
  The length of the public_folder string.
  */
  size_t public_folder_length;
  /**
  Logging flag - set to TRUE to log static file requests.

  Dynamic request logging is always the dynamic application's responsibility.
  */
  uint8_t log_static;
  /** An HTTP connection timeout. For HTTP/1.1 this defaults to ~5 seconds.*/
  uint8_t timeout;
  /**
  internal flag for library use.
  */
  uint8_t private_metaflags;
} http_settings_s;

/* *****************************************************************************
HTTP Helper functions that might be used globally
*/

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm* http_gmtime(const time_t* timer, struct tm* tmbuf);

/**
Writes an HTTP date string to the `target` buffer.

This requires _____ bytes of space to be available at the target buffer.

Returns the number of bytes actually written.
*/
size_t http_date2str(char* target, struct tm* tmbuf);

/**
A fast, inline alternative to `sprintf(dest, "%lu", num)`.

Writes an **unsigned** number to a buffer, as a string. This is an unsafe
functions that assumes the buffer will have at least 21 bytes and isn't NULL.

A NULL terminating byte is written.

Returns the number of bytes actually written (excluding the NULL byte).
*/
inline size_t http_ul2a(char* dest, size_t num) {
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
ssize_t http_decode_url_unsafe(char* dest, const char* url_data);

/** Decodes a URL encoded string. */
ssize_t http_decode_url(char* dest, const char* url_data, size_t length);

/* *****************************************************************************
HTTP versions (they depend on the settings / core data structure)
*/

#include "http1.h"

/* *****************************************************************************
HTTP listening helpers
*/

#endif
