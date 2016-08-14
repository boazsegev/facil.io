#ifndef HTTP_RESPONSE
/**
The HttpResponse library
========================

This library helps us to write HTTP valid responses, even when we do not know
the internals of the HTTP protocol.

The response object allows us to easily update the response status (all
responses start with the default 200 "OK" status code), write headers and cookie
data to the header buffer and send the response's body.

The response object also allows us to easily update the body size and send body
data or open files (which will be automatically closed once sending is done).

As example flow for the response could be:

     ; // get an initialized HttpRequest object
     struct HttpRequest * response = HttpResponse.create(request);
     ; // ... write headers and body, i.e.
     HttpResponse.write_header_cstr(response, "X-Data", "my data");
     HttpResponse.write_body(response, "Hello World!\r\n", 14);
     ; // release the object
     HttpResponse.destroy(response);


--
Thread-safety:

The response object and it's API are NOT thread-safe (it is assumed that no two
threads handle the same response at the same time).

Also, the response object will link itself to a libsock buffer packet, so it
should be created and dispatched during the same event - `sock_packet_s` objects
shouldn't be held across events or for a period of time... In other words:

**Create the response object only when you are ready to send a response**.

---
Misc notes:
The response header's buffer size is limited and too many headers will fail the
response.

The response object allows us to easily update the response status (all
responses start with the default 200 "OK" status code), write headers and write
cookie data to the header buffer.

The response object also allows us to easily update the body size and send body
data or open files (which will be automatically closed once sending is done).

The response does NOT support chuncked encoding.

The following is the response API container, use:

     struct HttpRequest * response = HttpResponse.create(request);


---
Performance:

A note about using this library with the HTTP/1 protocol family (if this library
supports HTTP/2, in the future, the use of the response object will be required,
as it might not be possible to handle the response manually):

Since this library safeguards against certain mistakes and manages an
internal header buffer, it comes at a performance cost (it adds a layer of data
copying to the headers).

This cost is mitigated by the optional use of a response object pool, so that it
actually saves us from using `malloc` for the headers - for some cases this is
faster.

In my performance tests, the greatest issue is this: spliting the headers from
the body means that the socket's buffer is under-utilized on the first call to
`send`, while sending the headers. While other operations incure minor costs,
this is the actual reason for degraded performance when using this library.

The order of performance should be considered as follows:

1. Destructive: Overwriting the request's header buffer with both the response
headers and the response data (small responses). Sending the data through the
socket using the `Server.write` function.

2. Using malloc to allocate enough memory for both the response's headers AND
it's body.  Sending the data through the socket using the `Server.write_move`
function.

3. Using the HttpResponse object to send the response.

Network issues and response properties might influence the order of performant
solutions.
*/
#define HTTP_RESPONSE
#include "http.h"
#include "http_request.h"

typedef struct {
  /**
The body's response length.

If this isn't set manually, the first call to
`HttpResponse.write_body` (and friends) will set the length to the length
being written (which might be less then the total data sent, if the sending is
fragmented).

Set the value to -1 to force the HttpResponse not to write nor automate the
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
  /**
  The response status
  */
  uint16_t status;
  /**
  Metadata about the response's state - don't edit this data (except the opaque
  data, if needed).
  */
  struct {
    /**
    The request object to which this response is "responding".
    */
    http_request_s* request;
    /**
    The libsock fd UUID.
    */
    intptr_t fd;
    /**
    A `libsock` buffer packet used for header data (to avoid double copy).
    */
    sock_packet_s* packet;
    /**
    A pointer to the header's writing position.
    */
    char* headers_pos;
    /**
    Internally used by the logging API.
    */
    clock_t clock_start;
    /**
    HTTP protocol version identifier.
    */
    uint8_t version;
    /**
    Set to true once the headers were sent.
    */
    unsigned headers_sent : 1;
    /**
    Set to true when the "Date" header is written to the buffer.
    */
    unsigned date_written : 1;
    /**
    Set to true when the "Connection" header is written to the buffer.
    */
    unsigned connection_written : 1;
    /**
    Set to true when the "Content-Length" header is written to the buffer.
    */
    unsigned content_length_written : 1;
    /**
    Set to true in order to close the connection once the response was sent.
    */
    unsigned should_close : 1;
    /**
    Internally used by the logging API.
    */
    unsigned logged : 1;
    /**
    Reserved for future use.
    */
    unsigned rsrv : 2;

  } metadata;

} http_response_s;

/**
The struct HttpCookie is a helper for seting cookie data.

This struct is used together with the `HttpResponse.set_cookie`. i.e.:

      HttpResponse.set_cookie(response, (struct HttpCookie){
        .name = "my_cookie",
        .value = "data"
      });

*/
typedef struct {
  /** The cookie's name (key). */
  char* name;
  /** The cookie's value (leave blank to delete cookie). */
  char* value;
  /** The cookie's domain (optional). */
  char* domain;
  /** The cookie's path (optional). */
  char* path;
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

/**
Initializes a response object with the request object. This function assumes the
response object memory is garbage and might have been stack-allocated.

Notice that the `http_request_s` pointer must point at a valid request object
and that the request object must remain valid until the response had been
completed.

Hangs on failuer (waits for available resources).
*/
http_response_s http_response_init(http_request_s* request);
/**
Releases any resources held by the response object (doesn't release the response
object itself, which might have been allocated on the stack).

This function assumes the response object might have been stack-allocated.
*/
void http_response_destroy(http_response_s* response);
/** Gets a response status, as a string. */
const char* http_response_status_str(uint16_t status);
/** Gets the mime-type string (C string) associated with the file extension. */
const char* http_response_ext2mime(const char* ext);
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
int http_response_write_header(http_response_s*, http_headers_s header);
#define http_response_write_header(response, ...) \
  http_response_write_header(response, (http_headers_s){__VA_ARGS__})

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
int http_response_set_cookie(http_response_s*, http_cookie_s);
#define http_response_set_cookie(response, ...) \
  http_response_set_cookie(response, (http_cookie_s){__VA_ARGS__})

/**
Indicates that any pending data (i.e. unsent headers) should be sent and that no
more use of the response object will be made. This will also release any
resources aquired when the response object was initialized, similar to the
`http_response_destroy` function.

If logging was initiated and hadn't been performed, it will be performed.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
void http_response_finish(http_response_s*);
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http_response_write_body(http_response_s*, const char* body, size_t length);

// /**
// REVIEW: IS THIS APPLICABLE FOR HTTP/2 AS WELL? this must be a unified API.
//
// Sends the headers (if they weren't previously sent) and writes the data to
// the
// underlying socket.
//
// The server's outgoing buffer will take ownership of the body and free it's
// memory using `free` once the data was sent.
//
// If the connection was already closed, the function will return -1. On
// success,
// the function returns 0.
// */
// int http_response_write_body_move(http_response_s*,
//                                   const char* body,
//                                   size_t length);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file and close it
using `fclose` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http_response_sendfile(http_response_s*,
                           int source_fd,
                           off_t offset,
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

This function will honor Ranged requests by setting the byte range
appropriately.

On failure, the function will return -1 (no response will be sent).

On success, the function returns 0.
*/
int http_response_sendfile2(http_response_s* response,
                            http_request_s* request,
                            const char* file_path_safe,
                            size_t path_safe_len,
                            const char* file_path_unsafe,
                            size_t path_unsafe_len,
                            uint8_t log);
/**
Starts counting miliseconds for log results.
*/
void http_response_log_start(http_response_s*);
/**
prints out the log to stderr.
*/
void http_response_log_finish(http_response_s*);

#endif
