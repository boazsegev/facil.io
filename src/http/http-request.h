/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#ifndef HTTP_HEAD_MAX_SIZE
#define HTTP_HEAD_MAX_SIZE 8192 /* 8*1024 */
#endif

/** this is defined elswere, but used here. See`lib-server.h` for details. */
struct Server;

/**
This is the interface for the HttpRequest object. Use the `HttpRequest` object
to make API calls, such as iterating through headers.
*/
extern const struct HttpRequestClass {
  /** returns an new heap allocated request object */
  struct HttpRequest* (*create)(void);
  /** releases the resources used by a request object but keep's it's core
   * memory. */
  void (*clear)(struct HttpRequest* request);
  /** releases the resources used by a request object and frees it's memory. */
  void (*destroy)(struct HttpRequest* request);
  /** validates that the object is indeed a request object */
  int (*is_request)(struct HttpRequest* self);

  /* Header handling */

  /** Restarts header iteration. */
  void (*first)(struct HttpRequest* self);
  /**
  Moves to the next header. Returns 0 if the end of the list was
  reached.
  */
  int (*next)(struct HttpRequest* self);
  /**
  Finds a specific header matching the requested string. The search is case
  insensitive.

  Returns 0 if the header couldn't be found.
  */
  int (*find)(struct HttpRequest* self, char* const name);
  /** returns the name of the current header in the iteration cycle. */
  char* (*name)(struct HttpRequest* self);
  /** returns the value of the current header in the iteration cycle. */
  char* (*value)(struct HttpRequest* self);

} HttpRequest;

/**
The Request object allows easy access to the request's raw data and body.
See the details of the structure for all the helper methods provided, such as
header itteration.

Memory management automated and provided by the HttpProtocol, but it is good to
be aware that the struct must be obtained using a contructor. i.e.:

       struct HttpRequest* request = HttpRequest.create(&http, sockfd);

Also, the `struct HttpRequest` objects live on the heap and thery should be
freed using a destructor. i.e.:

       struct HttpRequest* request = HttpRequest.destroy(&http, sockfd);

Again, both allocation and destruction are managed by the HttpProtocol and
shouldn't be performed by the developer (unless fixing a bug in the library).
*/
struct HttpRequest {
  /** The server initiating that forwarded the request. */
  struct Server* server;
  /** The socket waiting on the response */
  uint64_t sockfd;
  /** buffers the head of the request (not the body) */
  char buffer[HTTP_HEAD_MAX_SIZE];
  /**
  points to the HTTP method name's location within the buffer (actually,
  position 0). */
  char* method;
  /** The portion of the request URL that comes before the '?', if any. */
  char* path;
  /** The portion of the request URL that follows the ?, if any. */
  char* query;
  /**  points to the version string's location within the buffer. */
  char* version;
  /** points to the body's host header value (required). */
  char* host;
  /** the body's content's length, in bytes (can be 0). */
  size_t content_length;
  /** points to the body's content type header, if any. */
  char* content_type;
  /** points to the Upgrade header, if any. */
  char* upgrade;
  /**
  points the body of the request, if the body fitted within the buffer.
  otherwise, NULL. */
  char* body_str;
  /** points a tmpfile with the body of the request (the body was larger). */
  FILE* body_file;
  struct {
    /** points to the Upgrade header, if any. */
    void* is_request;
    /** points to the header's hash */
    char* header_hash;
    /** iteration position */
    unsigned int pos;
    /** maximum iteration value */
    unsigned int max;
    /** body size count (for parser validation) */
    unsigned int bd_rcved;
  } private;
};

#endif /* HTTP_PROTOCOL_H */
