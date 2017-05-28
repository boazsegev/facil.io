#ifndef H_HTTP1_RESPONSE_H
#define H_HTTP1_RESPONSE_H
/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http1.h"

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's response object. */
http_response_s *http1_response_create(http_request_s *request);
/** Destroys the response object. No data is sent.*/
void http1_response_destroy(http_response_s *);
/** Sends the data and destroys the response object.*/
void http1_response_finish(http_response_s *);

/* *****************************************************************************
Writing data to the response object
***************************************************************************** */

/**
Writes a header to the response. This function writes only the requested
number of bytes from the header name and the requested number of bytes from
the header value. It can be used even when the header name and value don't
contain NULL terminating bytes by passing the `.name_len` or `.value_len` data
in the `http_headers_s` structure.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int http1_response_write_header_fn(http_response_s *, http_header_s header);

/**
Set / Delete a cookie using this helper function.

To set a cookie, use (in this example, a session cookie):

    http_response_set_cookie(response,
            .name = "my_cookie",
            .value = "data");

To delete a cookie, use:

    http_response_set_cookie(response,
            .name = "my_cookie",
            .value = NULL);

This function writes a cookie header to the response. Only the requested
number of bytes from the cookie value and name are written (if none are
provided, a terminating NULL byte is assumed).

Both the name and the value of the cookie are checked for validity (legal
characters), but other properties aren't reviewed (domain/path) - please make
sure to use only valid data, as HTTP imposes restrictions on these things.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int http1_response_set_cookie(http_response_s *, http_cookie_s);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http1_response_write_body(http_response_s *, const char *body,
                              size_t length);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file and close it
using `close` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http1_response_sendfile(http_response_s *, int source_fd, off_t offset,
                            size_t length);

#endif
