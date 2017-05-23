# The Reactor Library - KQueue/EPoll abstraction

The Reactor library, `libreact` is a KQueue/EPoll abstraction and is part of [`libserver`'s](./libserver.md) core.

`libreact` requires only the following two files from this repository: [`src/libreact.h`](../src/libreact.h) and [`src/libreact.c`](../src/libreact.c).

It should be noted that exactly like `epoll` and `kqueue`, `libreact` might produce unexpected results if forked after initialized, since this will cause the `epoll`/`kqueue` data to be shared across processes, even though these processes will not have access to new file descriptors (i.e. `fd` 90 on one process might reference file "A" while on a different process the same `fd` (90) might reference file "B").

`libreact` adopts `libsock`'s use of `intptr_t` type system for `fd` UUIDs. However, depending on the system's byte order of with very minor adjustments to the `libreact.h` file, the library could be used seamlessly with regular file descriptors.

This documentation isn't relevant for `libserver` users. `libserver` implements `libreact` callbacks and `libreact` cannot be used without removing `libserver` from the project.

This file is here as quick reference to anyone interested in maintaining `libserver` or learning about how it's insides work.

## Event Callbacks

Event callbacks are defined during the linking stage and are hardwired once compilation is complete.

`void reactor_on_data(intptr_t)` - called when the file descriptor has incoming data. This is edge triggered and will not be called again unless all the previous data was consumed.

`void reactor_on_ready(intptr_t)` - called when the file descriptor is ready to send data (outgoing). `sock_flush` is called automatically and there is no need to call `sock_flush` when implementing this callback.

`void reactor_on_shutdown(intptr_t)` - called for any open file descriptors when the reactor is shutting down.

`void reactor_on_close(intptr_t)` - called when a file descriptor was closed REMOTELY. `on_close` will NOT get called when a connection is closed locally, unless using `libsock` or the `reactor_close` function.

**Notice**: Both EPoll and KQueue will **not** raise an event for an `fd` that was closed using the native `close` function, so unless using `libsock` or calling `reactor_close`, the `reactor_on_close` callback will only be called for remote events.

**Notice**: The `on_open` event is missing by design, as it is expected that any initialization required for the `on_open` event will be performed before attaching the `intptr_t fd` (socket/timer/pipe) to the reactor using [reactor_add]().

## The Reactor API

The following are the functions that control the Reactor.

#### `ssize_t reactor_init(void)`

Initializes the processes reactor object.

Reactor objects are a per-process object. Avoid forking a process with an active reactor, as some unexpected results might occur.

Returns -1 on error, otherwise returns 0.

#### `int reactor_review(void)`

Reviews any pending events (up to REACTOR_MAX_EVENTS which limits the number of events that should reviewed each cycle).

Returns -1 on error, otherwise returns the number of events handled by the reactor (0 isn't an error).

#### `void reactor_stop(void)`

Closes the reactor, releasing it's resources (except the actual struct Reactor, which might have been allocated on the stack and should be handled by the caller).

#### `int reactor_add(intptr_t uuid)`

Adds a file descriptor to the reactor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent and could be safely ignored.

#### `int reactor_add_timer(intptr_t uuid, long milliseconds)`

Adds a file descriptor as a timer object.

Returns -1 on error, otherwise return value is system dependent.

#### `int reactor_remove(intptr_t uuid)`

Removes a file descriptor from the reactor - further callbacks for this file descriptor won't be called.

Returns -1 on error, otherwise return value is system dependent and could be safely ignored. If the file descriptor wasn't owned by the reactor, it isn't an error.

#### `int reactor_remove_timer(intptr_t uuid)`

Removes a timer file descriptor from the reactor - further callbacks for this file descriptor's timer events won't be called.

Returns -1 on error, otherwise return value is system dependent. If the file descriptor wasn't owned by the reactor, it isn't an error.

#### `void reactor_close(intptr_t uuid)`

Closes a file descriptor, calling the `reactor_on_close` callback.

#### `void reactor_reset_timer(intptr_t uuid)`

EPoll requires the timer to be "reset" before repeating. Kqueue requires no such thing.

This method promises that the timer will be repeated when running on epoll. This method is redundant on kqueue.

#### `int reactor_make_timer(void)`

Creates a timer file descriptor, system dependent.

Returns -1 on error, otherwise returns the file descriptor.

## A Quick Example

The following example, an Echo server, isn't very interesting - but it's good enough to demonstrate the API:

```c
#include "libsock.h"   // easy socket functions, also allows integration.
#include "libreact.h"  // the reactor library

// a global server socket
int srvfd = -1;
// a global running flag
int run = 1;

// create the callback. This callback will be global and hardcoded,
// so there are no runtime function pointer resolutions.
void reactor_on_data(int fd) {
  if (fd == srvfd) {
    int new_client;
    // accept connections.
    while ((new_client = sock_accept(fd)) > 0) {
      fprintf(stderr, "Accepted new connetion\n");
      reactor_add(new_client);
    }
    fprintf(stderr, "No more clients... (or error)?\n");
  } else {
    fprintf(stderr, "Handling incoming data.\n");
    // handle data
    char data[1024];
    ssize_t len;
    while ((len = sock_read(fd, data, 1024)) > 0)
      sock_write(fd,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Length: 12\r\n"
                 "Connection: keep-alive\r\n"
                 "Keep-Alive: 1;timeout=5\r\n"
                 "\r\n"
                 "Hello World!",
                 100);
  }
}

void reactor_on_close(int fd) {
  fprintf(stderr, "%d closed the connection.\n", fd);
}

void stop_signal(int sig) {
  run = 0;
  signal(sig, SIG_DFL);
}
int main() {
  srvfd = sock_listen(NULL, "3000");
  signal(SIGINT, stop_signal);
  signal(SIGTERM, stop_signal);
  sock_lib_init();
  reactor_init();
  reactor_add(srvfd);
  fprintf(stderr, "Starting reactor loop\n");
  while (run && reactor_review() >= 0)
    ;
  fprintf(stderr, "\nGoodbye.\n");
}
```
