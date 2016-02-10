/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include <stdlib.h>
#include <stdio.h>

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
  /** retures an new heap allocated request object */
  struct HttpRequest* (*new)(struct Server* server, int sockfd);
  /** releases the resources used by a request object and frees it's memory. */
  void (*destroy)(struct HttpRequest* request);

  /* Header handling */

  /** Restarts header iteration. */
  void (*first)(struct HttpRequest* self);
  /**
  Moves to the next header. Returns 0 if the end of the list was
  reached.
  */
  int (*next)(struct HttpRequest* self);
  /**
  Finds a specific header matching the requested string.
  all headers are lower-case, so the string should be lower case.

  Returns 0 if the header couldn't be found.
  */
  int (*find)(struct HttpRequest* self, char* const name);
  /** returns the name of the current header in the itteration cycle. */
  char* (*name)(struct HttpRequest* self);
  /** returns the value of the current header in the itteration cycle. */
  char* (*value)(struct HttpRequest* self);

} HttpRequest;

/**
The Request object allows easy access to the request's raw data and body.
See the details of the structure for all the helper methods provided, such as
header itteration. The struct must be obtained using a
contructor. i.e.:

       struct Request* request = Request.new(&http, sockfd);

the `struct Request` objects live on the heap and thery should be freed using
a destructor. i.e.:

       struct Request* request = Request.destroy(&http, sockfd);
*/
struct HttpRequest {
  /** The sucket waiting on the response */
  int sockfd;
  /** The server initiating that forwarded the request. */
  struct Server* server;
  /** buffers the head of the request (not the body) */
  char buffer[HTTP_HEAD_MAX_SIZE];
  /**
  points to the HTTP method name's location within the buffer (actually,
  position 0). */
  char* method;
  /** The portion of the request URL that follows the ?, if any. */
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
  struct _Request_Private_Data {
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
