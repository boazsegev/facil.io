#ifndef H_HTTP_RESPONSE_H
#define H_HTTP_RESPONSE_H
/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http_request.h"
#include <stdio.h>
#include <time.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /** The protocol version family (HTTP/1.1 / HTTP/2 etc'). */
  enum HTTP_VERSION http_version;
  /** Will be set to TRUE (1) once the headers were sent. */
  unsigned headers_sent : 1;
  /** Set to true when the "Date" header is written to the buffer. */
  unsigned date_written : 1;
  /** Set to true when the "Connection" header is written to the buffer. */
  unsigned connection_written : 1;
  /** Set to true when the "Content-Length" header is written to the buffer. */
  unsigned content_length_written : 1;
  /** Set to true in order to close the connection once the response was sent.
   */
  unsigned should_close : 1;
  /** Internally used by the logging API. */
  unsigned logged : 1;
  /** Set this value to TRUE to indicate the request pointer should be freed. */
  unsigned request_dupped : 1;
  /** The response status */
  uint16_t status;
  /** The socket UUID for the response. */
  intptr_t fd;
  /** The originating request. */
  http_request_s *request;
  /** The body's response length.
   *
   * If this isn't set manually, the first call to `http_response_write_body`
   * (and friends) will set the length to the length being written (which might
   * be less then the total data sent, if the sending is fragmented).
   *
   * The value to -1 to prevents `http_response_s` from sending the
   * `Content-Length` header.
   */
  ssize_t content_length;
  /** The HTTP Date for the response (in seconds since epoche).
   *
   * Defaults to now (approximately, not exactly, uses cached time data).
   *
   * The date will be automatically formatted to match the HTTP protocol
   * specifications.
   *
   * It is better to avoid setting the "Date" header manualy.
   */
  time_t date;
  /** The HTTP Last-Modified date for the response (in seconds since epoche).
   *
   * Defaults to now (approximately, not exactly, uses cached time data).
   *
   * The date will be automatically formatted to match the HTTP protocol
   * specifications.
   *
   * It is better to avoid setting the "Last-Modified" header manualy.
   */
  time_t last_modified;
  /**
  Internally used by the logging API.
  */
  clock_t clock_start;
} http_response_s;

/**
The struct HttpCookie is a helper for seting cookie data.

This struct is used together with the `http_response_set_cookie`. i.e.:

      http_response_set_cookie(response,
        .name = "my_cookie",
        .value = "data" );

*/
typedef struct {
  /** The cookie's name (key). */
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
} http_cookie_s;

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's response object. */
http_response_s *http_response_create(http_request_s *request);
/** Destroys the response object. No data is sent.*/
void http_response_destroy(http_response_s *);
/** Sends the data and destroys the response object.*/
void http_response_finish(http_response_s *);

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
int http_response_write_header_fn(http_response_s *, http_header_s header);
#define http_response_write_header(response, ...)                              \
  http_response_write_header_fn(response, (http_header_s){__VA_ARGS__})

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
int http_response_set_cookie(http_response_s *, http_cookie_s);
#define http_response_set_cookie(response, ...)                                \
  http_response_set_cookie(response, (http_cookie_s){__VA_ARGS__})

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http_response_write_body(http_response_s *, const char *body,
                             size_t length);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file and close it
using `close` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0. The file is alsways closed by the function.

If should be possible to destroy the response object and send an error response
if an error is detected. The function will avoid sending any data before it
knows the likelyhood of error is small enough.
*/
int http_response_sendfile(http_response_s *, int source_fd, off_t offset,
                           size_t length);
/**
Attempts to send the file requested using an **optional** response object (if no
response object is pointed to, a temporary response object will be created).

If a `file_path_unsafe` is provided, it will be appended to the `file_path_safe`
(if any) and URL decoded before attempting to locate and open the file. Any
insecure path manipulations in the `file_path_unsafe` (i.e. `..` or `//`) will
cause the function to fail.

`file_path_unsafe` MUST begine with a `/`, or it will be appended to
`file_path_safe` as part of the last folder's name. if `file_path_safe` ends
with a `/`, it will be trancated.

If the `log` flag is set, response logging will be performed.

If the path ends with a backslash ('/'), the string `"index.html"` will be
appended (a default file name for when serving a folder). No automatic folder
indexing is supported.

This function will honor Ranged requests by setting the byte range
appropriately.

On failure, the function will return -1 (no response will be sent).

On success, the function returns 0.
*/
int http_response_sendfile2(http_response_s *response, http_request_s *request,
                            const char *file_path_safe, size_t path_safe_len,
                            const char *file_path_unsafe,
                            size_t path_unsafe_len, uint8_t log);
/* *****************************************************************************
Helpers and common tasks
***************************************************************************** */

/** Gets a response status, as a string. */
const char *http_response_status_str(uint16_t status);
/** Gets the mime-type string (C string) associated with the file extension. */
const char *http_response_ext2mime(const char *ext);

/** Starts counting miliseconds for log results. */
void http_response_log_start(http_response_s *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
