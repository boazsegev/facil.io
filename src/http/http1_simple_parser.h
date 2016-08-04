#ifndef HTTP1_SIMPLE_PARSER_H
#define HTTP1_SIMPLE_PARSER_H
#include "http_request.h"

#ifndef HTTP_HEADERS_LOWERCASE
/** when defined, HTTP headers will be converted to lowercase and header
 * searches will be case sensitive. This improves the parser performance in some
 * instances (which surprised me.) */
#define HTTP_HEADERS_LOWERCASE 1
#endif

#ifndef HTTP_PARSER_TEST
/* a debugging flag that adds the test function and decleration */
#define HTTP_PARSER_TEST 0
#endif

/**
Parses HTTP request headers. This allows review of the expected content length
before accepting any content (server resource management).

Returns the number of bytes consumed before the full request was accepted.

Returns -1 on fatal error (i.e. protocol error).
Returns -2 when the request parsing didn't complete.

Incomplete request parsing updates the content in the buffer. The same buffer
and the same `http_request_s` should be returned to the parsed on the next
attempt, only the `len` argument is expected to grow.

The buffer should be kept intact for the life of the request object, as the
HTTP/1.1 parser does NOT copy any data.
*/
ssize_t http1_parse_request_headers(void* buffer,
                                    size_t len,
                                    http_request_s* request);

/**
Parses HTTP request body content (if any).

Returns the number of bytes consumed before the body consumption was complete.

Returns -1 on fatal error (i.e. protocol error).
Returns -2 when the request parsing didn't complete (all the data was consumed).

Incomplete body parsing doesn't require the buffer to remain static (it can be
recycled).

It is expected that the next attempt will contain fresh data in the `buffer`
argument.
*/
ssize_t http1_parse_request_body(void* buffer,
                                 size_t len,
                                 http_request_s* request);

#if defined(DEBUG) && DEBUG == 1
void http_parser_test(void);
#endif

#endif
