#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

/* this library requires the request object and extends it. */
#include "http-request.h"
#include "http-objpool.h"
#include "http-status.h"
#include "http-mime-types.h"
#include "lib-server.h"

/* defined in the request header, and used here:
HTTP_HEAD_MAX_SIZE
*/

/**
The struct HttpResponse type will contain all the data required for handling the
response.

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
  */
  size_t content_length;
  /**
  The actual header buffer - do not edit directly.

  The extra 64 bytes are for the status line, allowing us to edit the status
  line data event after writing headers to the response buffer.
  */
  char header_buffer[HTTP_HEAD_MAX_SIZE + 64];
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
    The server through which the response will be sent.
    */
    server_pt server;
    /**
    The socket's fd, for sending the response.
    */
    int fd;
    /**
    The length of the headers written so far.
    */
    int headers_length;
    /**
    Set to true once the headers were sent.
    */
    unsigned headers_sent : 1;
    /**
    Reserved for future use.
    */
    unsigned date_written : 1;
    /**
    Set to true when the "Connection" header is written to the buffer.
    */
    unsigned connection_written : 1;
    /**
    Reserved for future use.
    */
    unsigned rsrv : 4;
    /**
    An opaque user data flag.
    */
    unsigned opaque : 1;

  } metadata;
};

/**
The response object and it's API contains a buffer for the headers and allows us
to easily manage a response.

The response object and it's API are NOT thread-safe (it is assumed that no two
threads handle the same response at the same time).

The response header's buffer size is limited and too many headers will fail the
response.

The response object allows us to easily update the response status (all
responses start with the default 200 "OK" status code), write headers and write
cookie data to the header buffer.

The response object also allows us to easily update the body size and send body
data or open files (which will be automatically closed once sending is done).

The response does NOT support chuncked encoding.

The following is the response API container, use:

     struct HttpRequest * response = HttpResponse.new(request);
*/
struct ___HttpResponse_class___ {
  /**
  Destroys the response object pool. This function ISN'T thread-safe.
  */
  void (*destroy_pool)(void);
  /**
  Creates the response object pool (unless it already exists). This function
  ISN'T thread-safe.
  */
  void (*create_pool)(void);
  /**
  Creates a new response object or recycles a response object from the response
  pool.

  returns NULL on failuer, or a pointer to a valid response object.
  */
  struct HttpResponse* (*new)(struct HttpRequest*);
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
  Writes a header to the response.

  This is equivelent to writing:

       HttpResponse.write_header(* response, header, value, strlen(value));

  If the headers were already sent, new headers cannot be sent and the function
  will return -1. On success, the function returns 0.
  */
  int (*write_header_cstr)(struct HttpResponse*,
                           const char* header,
                           const char* value);
  /**
  Writes a header to the response. This function writes only the requested
  number of bytes from the header value and should be used when the header value
  doesn't contain a NULL terminating byte.

  If the headers were already sent, new headers cannot be sent and the function
  will return -1. On success, the function returns 0.
  */
  int (*write_header)(struct HttpResponse*,
                      const char* header,
                      const char* value,
                      size_t value_len);

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

  The server's outgoing buffer will take ownership of the body and free it's
  memory using `free` once the data was sent.

  If the connection was already closed, the function will return -1. On success,
  the function returns 0.
  */
  int (*sendfile)(struct HttpResponse*, FILE* pf, size_t length);
  /**
  Closes the connection.
  */
  void (*close)(struct HttpResponse*);
} HttpResponse;

/* end include guard */
#endif /* HTTP_RESPONSE_H */
