#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

/* this library requires the request object and extends it. */
#include "lib-server.h"
#include "http-request.h"
#include "http-status.h"
#include "http-mime-types.h"

/**
The struct HttpResponse type will contain all the data required for handling the
response.

Example use (excluding error checks):

    void on_request(struct HttpRequest request) {
      struct HttpResponse* response = HttpResponse.create(req); // (initialize)
      HttpResponse.write_header2(response, "X-Data", "my data");
      HttpResponse.set_cookie(response, (struct HttpCookie){
        .name = "my_cookie",
        .value = "data"
      });
      HttpResponse.write_body(response, "Hello World!\r\n", 14);
      HttpResponse.destroy(response); // release/pool resources
    }

    int main()
    {
      char * public_folder = NULL
      start_http_server(on_request, public_folder, .threads = 16);
    }


To set a response's content length, use `response->content_length` or, if
sending the body using a single write, it's possible to leave out the
content-length header (see the `HttpResponse.write_body` for more details).

The response object and it's API are NOT thread-safe (it is assumed that no two
threads handle the same response at the same time).
*/
struct HttpResponse {
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
  int status;
  /**
  Metadata about the response's state - don't edit this data (except the opaque
  data, if needed).
  */
  struct {
    /**
    an HttpResponse class object identifier, used to validate that the response
    object pointer is actually pointing to a response object (only validated
    before storing the object in the pool or freeing the object's memory).
    */
    void* classUUID;
    /**
    A `libsock` buffer packet used for header data (to avoid double copy).
    */
    sock_packet_s* packet;
    /**
    The server through which the response will be sent.
    */
    server_pt server;
    /**
    The socket's fd, for sending the response.
    */
    uint64_t fd_uuid;
    /**
    A pointer to the header's writing position.
    */
    char* headers_pos;
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
    unsigned connection_len_written : 1;
    /**
    Set to true in order to close the connection once the response was sent.
    */
    unsigned should_close : 1;
    /**
    Reserved for future use.
    */
    unsigned rsrv : 1;
    /**
    An opaque user data flag.
    */
    unsigned opaque : 1;

  } metadata;
};

/**
The struct HttpCookie is a helper for seting cookie data.

This struct is used together with the `HttpResponse.set_cookie`. i.e.:

      HttpResponse.set_cookie(response, (struct HttpCookie){
        .name = "my_cookie",
        .value = "data"
      });

*/
struct HttpCookie {
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
};

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

Before using any response object (usually performed before the server starts),
it is important to inialize the response object pool:

    HttpResponse.create_pool()

To destroy the pool (usually after the server is done), use:

    HttpResponse.destroy_pool()

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

Initializing and destroying the request object pool is NOT thread-safe.

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
extern struct HttpResponseClass {
  /**
  Destroys the response object pool. This function ISN'T thread-safe.
  */
  void (*destroy_pool)(void);
  /**
  Creates the response object pool (unless it already exists). This function
  ISN'T thread-safe.
  */
  void (*init_pool)(void);
  /**
  Creates a new response object or recycles a response object from the response
  pool.

  returns NULL on failuer, or a pointer to a valid response object.
  */
  struct HttpResponse* (*create)(struct HttpRequest*);
  /**
  Destroys the response object or places it in the response pool for recycling.
  */
  void (*destroy)(struct HttpResponse*);
  /**
  The pool limit property (defaults to 64) sets the limit of the pool storage,
  making sure that excess memory used is cleared rather then recycled.
  */
  int pool_limit;
  /**
  Clears the HttpResponse object, linking it with an HttpRequest object (which
  will be used to set the server's pointer and socket fd).
  */
  void (*reset)(struct HttpResponse*, struct HttpRequest*);
  /** Gets a response status, as a string */
  char* (*status_str)(struct HttpResponse*);
  /**
  Writes a header to the response. This function writes only the requested
  number of bytes from the header name and the requested number of bytes from
  the header value. It can be used even when the header name and value don't
  contain NULL terminating bytes.

  If the header buffer is full or the headers were already sent (new headers
  cannot be sent), the function will return -1.

  On success, the function returns 0.
  */
  int (*write_header)(struct HttpResponse*,
                      const char* header,
                      size_t header_len,
                      const char* value,
                      size_t value_len);
  /**
  Writes a header to the response. This function writes only the requested
  number of bytes from the header value and can be used even when the header
  value doesn't contain a NULL terminating byte.

  If the header buffer is full or the headers were already sent (new headers
  cannot be sent), the function will return -1.

  On success, the function returns 0.
  */
  int (*write_header1)(struct HttpResponse*,
                       const char* header,
                       const char* value,
                       size_t value_len);
  /**
  Writes a header to the response.

  This is equivelent to writing:

       HttpResponse.write_header(response, header, value, strlen(value));

       If the header buffer is full or the headers were already sent (new
  headers
       cannot be sent), the function will return -1.

       On success, the function returns 0.
  */
  int (*write_header2)(struct HttpResponse*,
                       const char* header,
                       const char* value);
  /**
  Set / Delete a cookie using this helper function.

  To set a cookie, use (in this example, a session cookie):

      HttpResponse.set_cookie(response, (struct HttpCookie){
              .name = "my_cookie",
              .value = "data" });

  To delete a cookie, use:

      HttpResponse.set_cookie(response, (struct HttpCookie){
              .name = "my_cookie",
              .value = NULL });

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
  int (*set_cookie)(struct HttpResponse*, struct HttpCookie);
  /**
  Prints a string directly to the header's buffer, appending the header
  seperator (the new line marker '\r\n' should NOT be printed to the headers
  buffer).

  If the header buffer is full or the headers were already sent (new headers
  cannot be sent), the function will return -1.

  On success, the function returns 0.
  */
  int (*printf)(struct HttpResponse*, const char* format, ...);
  /**
  Sends the headers (if they weren't previously sent).

  If the connection was already closed, the function will return -1. On success,
  the function returns 0.
  */
  int (*send)(struct HttpResponse*);
  /**
  Sends the headers (if they weren't previously sent) and writes the data to the
  underlying socket.

  The body will be copied to the server's outgoing buffer.

  If the connection was already closed, the function will return -1. On success,
  the function returns 0.
  */
  int (*write_body)(struct HttpResponse*, const char* body, size_t length);
  /**
  Sends the headers (if they weren't previously sent) and writes the data to the
  underlying socket.

  The server's outgoing buffer will take ownership of the body and free it's
  memory using `free` once the data was sent.

  If the connection was already closed, the function will return -1. On success,
  the function returns 0.
  */
  int (*write_body_move)(struct HttpResponse*, const char* body, size_t length);
  /**
  Sends the headers (if they weren't previously sent) and writes the data to the
  underlying socket.

  The server's outgoing buffer will take ownership of the file and close it
  using `fclose` once the data was sent.

  If the connection was already closed, the function will return -1. On success,
  the function returns 0.
  */
  int (*sendfile)(struct HttpResponse*, int source_fd, size_t length);
  /**
  Sends the complete file referenced by the `file_path` string.

  This function requires that the headers weren't previously sent and that the
  file exists.

  On failure, the function will return -1. On success, the function returns 0.
  */
  int (*sendfile2)(struct HttpResponse*, char* file_path);
  /**
  Closes the connection.
  */
  void (*close)(struct HttpResponse*);
} HttpResponse;

/* end include guard */
#endif /* HTTP_RESPONSE_H */
