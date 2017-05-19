/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http1.h"
#include ""

char *HTTP1_Protocol_String = "facil_http/1.1_protocol";
/* *****************************************************************************
HTTP/1.1 data structures
*/

typedef struct {
  protocol_s protocol;
  http_settings_s *settings;
  char buffer[HTTP1_MAX_HEADER_SIZE];
  size_t buffer_pos;
  void (*on_request)(http_request_s *request);
  http_request_s
      request; /* MUST be last, as it has memory extensions for the headers*/
} http1_protocol_s; /* ~ 8416 bytes for (8 * 1024) buffer size and 64 headers */

#define HTTP1_PROTOCOL_SIZE                                                    \
  (sizeof(http1_protocol_s) + HTTP_REQUEST_SIZE(HTTP1_MAX_HEADER_COUNT))

/* *****************************************************************************
HTTP listening helpers
*/

/**
Allocates memory for an upgradable HTTP/1.1 protocol.

The protocol self destructs when the `on_close` callback is called.
*/
protocol_s *http1_on_open(intptr_t fd, http_settings_s *settings);
