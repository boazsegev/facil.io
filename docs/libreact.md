# The Reactor Library - KQueue/EPoll abstraction

The Reactor library, `libreact` is a KQueue/EPoll abstraction and is part of`lib-server`'s core.

You don't need to read this file if you're looking to utilize `lib-server`.

This file is here as quick reference to anyone interested in maintaining `lib-server` or learning about how it's insides work.

It should be noted that `server_pt` and `struct Reactor *` are both pointers to [`struct Reactor`](#the-reactor-object-struct-reactor-) objects, so that a `server_pt` can be used for `libreact`'s API.

## The reactor object: `struct Reactor *`

The reactor API hinges on the `struct Reactor` object, which is used both for storing the event callback settings and information about the state of the reactor (open connections, etc').

The reactor object should be considered partially opaque and some of it's content should be ignored.

This is the reactor object:

```c
struct Reactor {
  void (*on_data)(struct Reactor* reactor, int fd);
  void (*on_ready)(struct Reactor* reactor, int fd);
  void (*on_shutdown)(struct Reactor* reactor, int fd);
  void (*on_close)(struct Reactor* reactor, int fd);

  time_t last_tick;
  int maxfd; // REQUIRED

  struct {
    int reactor_fd;
    char* map;
    void* events;
  } private;
};
```

### Event Callbacks

`on_data` - a pointer to a function that will be called when the file descriptor has incoming data. This is edge triggered and will not be called again unless all the previous data was consumed.

`on_ready` - a pointer to a function that will be called when the file descriptor is ready to send data (outgoing).

`on_shutdown` - a pointer to a function that will be called for any open file descriptors when the reactor is shutting down.

`on_close` - a pointer to a function that will be called when a file descriptor was closed REMOTELY. `on_close` will NOT get called when a connection is closed locally, unless using `reactor_close` function.

**Notice**: the `on_close` callback will be called **only** when the `fd` was closed **remotely** or when it was closed using the [`reactor_close`](#void-reactor_closestruct-reactor-int-fd) function. Both EPoll and KQueue will **not** raise an event for an `fd` that was closed using the native `close` function.

**Notice**: `on_open` is missing by design, as it is expected that any initialization required for the `on_open` event will be performed before attaching the `fd` (socket/timer/pipe) to the reactor using [reactor_add](#int-reactor_addstruct-reactor-int-fd).

### The Reactor's Data and Settings

`time_t last_tick` - the time (seconds since epoch) of the last "tick" (event cycle).

`int maxfd` - REQUIRED: the maximum value for a file descriptor that the reactor will be required to handle (the capacity -1).

`struct private` - for internal use (read code for more information).

## The Reactor API

The following are the functions that control the Reactor.

### `int reactor_init(struct Reactor*)`

Initializes the reactor, making the reactor "live".

Once initialized, the reactor CANNOT be forked, so do not fork the process after calling `reactor_init`, or data corruption will be experienced.

Returns -1 on error, otherwise returns 0.

### `int reactor_review(struct Reactor*)`

Reviews any pending events (up to REACTOR_MAX_EVENTS which limits the number of events that should reviewed each cycle).

Returns -1 on error, otherwise returns the number of events handled by the reactor (0 isn't an error).

### `void reactor_stop(struct Reactor*)`

Closes the reactor, releasing it's resources (except the actual struct Reactor, which might have been allocated on the stack and should be handled by the caller).

### `int reactor_add(struct Reactor*, int fd)`

Adds a file descriptor to the reactor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent and could be safely ignored.

### `int reactor_remove(struct Reactor*, int fd)`

Removes a file descriptor from the reactor - further callbacks won't be called.

Returns -1 on error, otherwise return value is system dependent and could be safely ignored. If the file descriptor wasn't owned by the reactor, it isn't an error.

### `void reactor_close(struct Reactor*, int fd)`

Closes a file descriptor, calling it's callback if it was registered with the reactor.

### `int reactor_add_timer(struct Reactor*, int fd, long milliseconds)`

Adds a file descriptor as a timer object.

Returns -1 on error, otherwise return value is system dependent.

### `void reactor_reset_timer(int fd)`

EPoll requires the timer to be "reset" before repeating. Kqueue requires no such thing.

This method promises that the timer will be repeated when running on epoll. This method is redundant on kqueue.

### `int reactor_make_timer(void)`

Creates a timer file descriptor, system dependent.

Returns -1 on error, otherwise returns the file descriptor.

## A Quick Example

The following example isn't very interesting, but it's good enough to demonstrate the API:

```c
#include "libreact.h"
; // we're writing a small server, many included files...
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

; // this will accept connections,
; // it will be a simple HTTP hello world. (with keep alive)
void on_data(struct Reactor* reactor, int fd);

struct Reactor r = {.on_data = on_data, .maxfd = 1024};
int srvfd; // to make it simple, we'll have a global object

; // this will handle the exit signal (^C).
void on_sigint(int sig);

int main(int argc, char const* argv[]) {
  printf("starting up an http hello world example on port 3000\n");
  ; // setup the exit signal
  signal(SIGINT, on_sigint);
  ; // create a server socket... this will take a moment...
  char* port = "3000";
  ; // setup the address
  struct addrinfo hints;
  struct addrinfo* servinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me

  if (getaddrinfo(NULL, port, &hints, &servinfo)) {
    perror("addr err");
    return -1;
  }

  srvfd =   // get the file descriptor
      socket(servinfo->ai_family, servinfo->ai_socktype,
                                  servinfo->ai_protocol);
  if (srvfd <= 0) {
    perror("addr err");
    freeaddrinfo(servinfo);
    return -1;
  }

  { // avoid the "address taken"
    int optval = 1;
    setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &optval,
              sizeof(optval));
  }

  ; // bind the address to the socket

  if (bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
    perror("bind err");
    freeaddrinfo(servinfo);
    return -1;
  }

  ; // make sure the socket is non-blocking

  static int flags;
  if (-1 == (flags = fcntl(srvfd, F_GETFL, 0)))
    flags = 0;
  fcntl(srvfd, F_SETFL, flags | O_NONBLOCK);

  ; // listen in
  listen(srvfd, SOMAXCONN);
  if (errno)
    perror("starting. last error was");

  freeaddrinfo(servinfo); // free the address data memory

  ; // now that everything is ready, call the reactor library...
  reactor_init(&r);
  reactor_add(&r, srvfd);

  while (reactor_review(&r) >= 0)
    ;

  if (errno)
    perror("\nFinished. last error was");
}

void on_data(struct Reactor* reactor, int fd) {
  if (fd == srvfd) { // yes, this is our listening socket...
    int client = 0;
    unsigned int len = 0;
    while ((client = accept(fd, NULL, &len)) > 0) {
      reactor_add(&r, client);
    }  // reactor is edge triggered, we need to clear the cache.
  } else {
    char buff[8094];
    ssize_t len;
    static char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=2\r\n"
        "\r\n"
        "Hello World!\r\n";

    if ((len = recv(fd, buff, 8094, 0)) > 0) {
      len = write(fd, response, strlen(response));
    }
  }
}

void on_sigint(int sig) { // stop the reactor
  reactor_stop(&r);
}
```
