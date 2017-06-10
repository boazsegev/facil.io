/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP1_H
#define H_HTTP1_H
#include "http.h"

#ifndef HTTP1_MAX_HEADER_SIZE
/**
Sets the maximum allowed size of a requests header section
(all cookies, headers and other data that isn't the request's "body").

This value includes the request line itself.

Defaults to ~16Kb headers per request.
*/
#define HTTP1_MAX_HEADER_SIZE (16 * 1024) /* ~16kb */
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

/** The HTTP/1.1 protocol identifier. */
extern char *HTTP1_Protocol_String;

/* *****************************************************************************
HTTP listening helpers
*/

/**
Allocates memory for an upgradable HTTP/1.1 protocol.

The protocol self destructs when the `on_close` callback is called.
*/
protocol_s *http1_on_open(intptr_t fd, http_settings_s *settings);

#endif
