/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_REACT
#define LIB_REACT "0.3.0"
#define LIB_REACT_VERSION_MAJOR 0
#define LIB_REACT_VERSION_MINOR 3
#define LIB_REACT_VERSION_PATCH 0

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef REACTOR_MAX_EVENTS
#define REACTOR_MAX_EVENTS 64
#endif
#ifndef REACTOR_TICK
#define REACTOR_TICK 256 /** in milliseconsd */
#endif

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(__unix__) && !defined(__linux__) && !defined(__APPLE__) &&        \
    !defined(__CYGWIN__)
#error This library currently supports only Unix based systems (i.e. Linux and BSD)
#endif

/* until linux supports KQueue, which might not happen... */
#if !defined(__linux__) && !defined(__CYGWIN__)
#define reactor_epoll 1
#define reactor_kqueue 0
#else
#define reactor_epoll 0
#define reactor_kqueue 1
#endif

/* *****************************************************************************
A simple, predictable UUID for file-descriptors, for collision prevention

It's protected against duplicate definition (i.e., when including `libsock.h`)
*/
#ifndef FD_UUID_TYPE_DEFINED
#define FD_UUID_TYPE_DEFINED
/** fduuid_u is used to identify a specific connection, helping to manage file
 * descriptor collisions (when a new connection receives an old connection's
 * file descriptor), especially when the `on_close` event is fired after an
 * `accept` was called and the old file descriptor was already recycled.
 *
 * This requires that sizeof(int) < sizeof(uintptr_t) or sizeof(int)*8 >= 32
 */
typedef union {
  intptr_t uuid;
  struct {
    int fd : (sizeof(int) < sizeof(intptr_t) ? (sizeof(int) * 8) : 24);
    unsigned counter : (sizeof(int) < sizeof(intptr_t)
                            ? ((sizeof(intptr_t) - sizeof(int)) * 8)
                            : ((sizeof(intptr_t) * 8) - 24));
  } data;
} fduuid_u;

#define FDUUID_FAIL(uuid) (uuid == -1)
#define sock_uuid2fd(uuid) ((fduuid_u)(uuid)).data.fd
#endif

/*****************************/ /** \file
This small library implements a reactor pattern using callbacks.

Here are the supported events and their callbacks:

- File Descriptor events:

    - Ready to Read (`reactor_on_data` callback).

    - Ready to Write (`reactor_on_ready` callback).

    - Closed (`reactor_on_close` callback).

Here's a quick example for an HTTP hello world (no HTTP parsing required)...:

    #include "libreact.h"  // the reactor library
    #include "libsock.h"   // easy socket functions, also allows integration.

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

*/

/* *****************************************************************************
Initialization and global workflow.
*/

/**
Initializes the processes reactor object.

Reactor objects are a per-process object. Avoid forking a process with an active
reactor, as some unexpected results might occur.

Returns -1 on error, otherwise returns 0.
*/
ssize_t reactor_init();
/**
Reviews any pending events (up to REACTOR_MAX_EVENTS) and calls any callbacks.

Returns -1 on error, otherwise returns the number of events handled by the
reactor.
*/
int reactor_review();
/**
Closes the reactor, releasing it's resources.
*/
void reactor_stop();

/* *****************************************************************************
Adding and removing normal file descriptors.
*/

/**
Adds a file descriptor to the reactor, so that callbacks will be called for it's
events.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add(intptr_t uuid);

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add_timer(intptr_t uuid, long milliseconds);
/**
Removes a file descriptor from the reactor - further callbacks for this file
descriptor won't be called.

Returns -1 on error, otherwise return value is system dependent. If the file
descriptor wasn't owned by the reactor, it isn't an error.
*/
int reactor_remove(intptr_t uuid);
/**
Removes a timer file descriptor from the reactor - further callbacks for this
file descriptor's timer events won't be called.

Returns -1 on error, otherwise return value is system dependent. If the file
descriptor wasn't owned by the reactor, it isn't an error.
*/
int reactor_remove_timer(intptr_t uuid);
/**
Closes a file descriptor, calling it's callback if it was registered with the
reactor.
*/
void reactor_close(intptr_t uuid);

/* *****************************************************************************
Timers.
*/

/**
Adds a file descriptor as a timer object.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add_timer(intptr_t uuid, long milliseconds);
/**
epoll requires the timer to be "reset" before repeating. Kqueue requires no such
thing.

This method promises that the timer will be repeated when running on epoll. This
method is redundent on kqueue.
*/
void reactor_reset_timer(intptr_t uuid);
/**
Creates a timer file descriptor, system dependent.

Returns -1 on error, or a valid fd on success (not an fd UUID, as these are
controlled by `libsock` and `libreact` can be used independently as well).
*/
intptr_t reactor_make_timer();

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* end of include guard: LIB_REACT */
