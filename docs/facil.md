# LibServer - a dynamic protocol network services library

LibServer is:

* A Library (not a framework): meaning, closer to the metal, abstracting only what is required for API simplicity, security, error protection and performance.

* For Dynamic Protocol: meaning services can change protocols mid-stream. Example use-case would be the HTTP/1.1 upgrade request to Websockets or HTTP/2.

* Network services: meaning implementing client-server or peer2peer network applications, such as web applications, micro-services, etc'.

`libserver` utilizes `libreact`, `libasync` and `libsock` to create a simple API wrapper around these minimalistic libraries and managing the "glue" that makes them work together.

It's simple, it's awesome, and you'd be crazy to use this in production without testing and reading through the code :-)

But if you'd rather write the whole thing yourself, I recommend starting with [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) as a good source for information.

## Core Concepts And A Quick Example

LibServer aims to provide a simple API, so that developers can focus on developing their applications rather then learning new APIs.

However, server applications are not the simplest of beasts, so if you encounter some minor complexity, I hope this documentation can help elevate the situation.

### The Protocol: How Network Services Communicate

By nature, network services implement higher level protocols to communicate (higher then TCP/IP). The HTTP protocol is a common example.

Typically, server applications "react", they read incoming data (known as a request), perform a task and send output (a response).

The base type to handle the demands of protocols is `protocol_s`. This is no more then a struct with information about the callbacks that should be invoked on network events (incoming data, disconnection etc').

Protocol objects must be unique per connection, and so they are usually dynamically allocated.

```c
struct Protocol {
  /**
  * A string to identify the protocol's service (i.e. "http").
  *
  * The string should be a global constant, only a pointer comparison will be
  * made (not `strcmp`).
  */
  const char* service;
  /** called when a data is available */
  void (*on_data)(intptr_t fduuid, protocol_s* protocol);
  /** called when the socket is ready to be written to. */
  void (*on_ready)(intptr_t fduuid, protocol_s* protocol);
  /** called when the server is shutting down,
   * but before closing the connection. */
  void (*on_shutdown)(intptr_t fduuid, protocol_s* protocol);
  /** called when the connection was closed */
  void (*on_close)(protocol_s* protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t fduuid, protocol_s* protocol);
  /** private metadata stored by `libserver`, usualy for object protection */
  uintptr_t _state_;
  /**/
};
```

Protocol objects can be global or initiated per connection.

HTTP is nice, but an echo example is classic. Here is a simple "echo" protocol implementation:

```c
#include "libserver.h"
#include <errno.h>

/* A callback to be called whenever data is available on the socket*/
static void echo_on_data(intptr_t uuid, /* The socket */
                          protocol_s* prt /* pointer to the protocol */
                           ) {
  (void)prt; /* ignore unused argument */
  /* echo buffer */
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  /* Read to the buffer, starting after the "Echo: " */
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    /* Write back the message */
    sock_write(uuid, buffer, len + 6);
    /* Handle goodbye */
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

/* A callback called whenever a timeout is reach (more later) */
static void echo_ping(intptr_t uuid, protocol_s* prt) {
  /* Read/Write is handled by `libsock` directly. */
  sock_write(uuid, "Server: Are you there?\n", 23);
}

/* A callback called if the server is shutting down...
... while the connection is open */
static void echo_on_shutdown(intptr_t uuid, protocol_s* prt) {
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}

/* A callback called for new connections */
static protocol_s *echo_on_open(intptr_t uuid, void *udata) {
  (void)udata; /*ignore this */
  /* Protocol objects MUST always be dynamically allocated. */
  protocol_s * echo_proto = malloc(sizeof( * echo_proto ));
  * echo_proto = (protocol_s) {.service = "echo",
                        .on_data = echo_on_data,
                        .on_shutdown = echo_on_shutdown,
                        .on_close = free, /* simply free when done */
                        .ping = echo_ping};

  sock_write(uuid, "Echo Service: Welcome\n", 22);
  server_set_timeout(uuid, 10);
  return echo_proto;
}

int main() {
  /* Setup a listening socket */
  if (server_listen(.port = "8888", .on_open = echo_on_open))
    perror("No listening socket available on port 8888"), exit(-1);
  /* Run the server and hang until a stop signal is received. */
  server_run(.threads = 4, .processes = 1);
}
/**/
```

In later examples, we might extend the `protocol_s` "class" to add more data / features we might require. "C" objects can use a typecasting stye of inheritance which comes very handy when implementing network protocols.
