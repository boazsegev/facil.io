#ifndef HTTP1_H
#define HTTP1_H

#include "http.h"

#ifndef HTTP1_MAX_HEADER_SIZE
/** Sets the maximum allowed size of a requests header section
(all cookies, headers and other data that isn't the request's "body").

Defaults to ~8Kb headers per request */
#define HTTP1_MAX_HEADER_SIZE (8 * 1024) /* ~ 8kb */
#endif

#ifndef HTTP1_MAX_HEADER_COUNT
/** Sets the maximum allowed headers per request (obligatory).

Defaults to 64 headers per request */
#define HTTP1_MAX_HEADER_COUNT (64)
#endif

#ifndef HTTP1_POOL_SIZE
/** Defines the pre-allocated memory for incoming concurrent connections.

Any concurrent HTTP1 connections over this amount will be dynamically allocated.
*/
#define HTTP1_POOL_SIZE (64) /* should be ~0.5Mb with default values*/
#endif

extern char* HTTP1;

/**
Allocates memory for an HTTP/1.1 protocol.

The protocol self destructs when the `on_close` callback is called.

To free the protocol manually, it's possible to call it's `on_close` callback.
*/
protocol_s* http1_alloc(intptr_t fd, http_settings_s* settings);

/**
Listens for incoming HTTP/1.1 connections on the specified posrt and address,
implementing the requested settings.
*/
int http1_listen(char* port, char* address, http_settings_s settings);

#define http1_listen(port, address, ...) \
  http1_listen((port), (address), (http_settings_s){__VA_ARGS__})

#endif

/** Should be defined by `http.h` ... if it was removed, use a lower value. */
#ifndef HTTP_BUSY_UNLESS_HAS_FDS
#define HTTP_BUSY_UNLESS_HAS_FDS 8
#endif
