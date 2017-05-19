#ifndef H_HTTP_RESPONSE_H
#define H_HTTP_RESPONSE_H

#include "http_request.h"
#include <stdio.h>
#include <time.h>

typedef struct {
  uint8_t http_version;
  uint8_t rsv1;
  /**
The response status
*/
  uint16_t status;
  /**
  The socket UUID for the response.
  */
  intptr_t fd;
  /**
  The originating.
  */
  http_request_s *request;
  /**
  The body's response length.
  If this isn't set manually, the first call to
  `HttpResponse.write_body` (and friends) will set the length to the length
  being written (which might be less then the total data sent, if the sending is
  fragmented).
  Set the value to -1 to force the HttpResponse not to write the
  `Content-Length` header.
  */
  ssize_t content_length;
  /**
  The HTTP date for the response (in seconds since epoche).
  Defaults to now (approximately, not exactly, uses cached data).
  The date will be automatically formatted to match the HTTP protocol
  specifications. It is better to avoid setting the "Date" header manualy.
  */
  time_t date;
  /**
  The HTTP date for the response (in seconds since epoche).
  Defaults to now (approximately, not exactly, uses cached data).
  The date will be automatically formatted to match the HTTP protocol
  specifications. It is better to avoid setting the "Date" header manualy.
  */
  time_t last_modified;
} http_response_s;

#endif
