/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H
#include "lib-server.h"
#include "http-request.h"
#include "http-objpool.h"
#include <stdio.h>

#ifndef HTTP_HEAD_MAX_SIZE
#define HTTP_HEAD_MAX_SIZE 8192  // 8*1024
#endif
/**
 the following structures are defined herein:
*/

/**
A Procotol suited for Http/1.x servers. The struct must be obtained using a
contructor. i.e.:

       struct HttpProtocol http = HttpProtocol();

the `struct HttpProtocol` objects live on the stack and their memory is
automatically released at the end of the block (function/if block.etc').
*/
struct HttpProtocol;

extern struct ___HttpProtocol_CLASS___ {
  /** returns a new, initialized, Http Protocol object. */
  struct HttpProtocol* (*new)(void);
  /** destroys an existing HttpProtocol object, releasing it's memory and
  resources. */
  void (*destroy)(struct HttpProtocol*);
} HttpProtocol;

/************************************************/ /**
The HttpProtocol implements a very basic and raw protocol layer over Http,
leaving much of the work for the implementation.

Some helpers are provided for request management (see the HttpRequest struct)
and
some minor error handling is provided as well...

The Http response is left for independent implementation. The request object
contains a reference to the socket's file descriptor waiting for the response.

A single connection cannot run two `on_request` callbacks asynchronously.
 */

/** This holds the Http protocol, it's settings and callbacks, such as maximum
body size, the on request callback, etc'. */
struct HttpProtocol {
  /**
  this is the "parent" protocol class, used internally. do not edit data on this
  class.

  This must be the first declaration to allow pointer casting inheritance. */
  struct Protocol parent;
  /** sets the maximum size for a body, in Mb (Mega-Bytes). */
  int maximum_body_size;
  /** the callback to be performed when requests come in. */
  void (*on_request)(struct HttpRequest* request);
  /**
  A public folder for file transfers - allows to circumvent any application
  layer server and simply serve files.
  */
  char* public_folder;
  /** an internal request pool, to avoid malloc */
  object_pool request_pool;
};

#endif /* HTTP_PROTOCOL_H */
