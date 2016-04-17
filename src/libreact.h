/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIBREACT_H
#define LIBREACT_H

#define LIBREACT_VERSION "0.2.0"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef REACTOR_MAX_EVENTS
#define REACTOR_MAX_EVENTS 64
#endif
#ifndef REACTOR_TICK
#define REACTOR_TICK 1000
#endif

#include <sys/time.h>
#include <sys/types.h>

#if !defined(__unix__) && !defined(__linux__) && !defined(__APPLE__) && \
    !defined(__CYGWIN__)
#error This library currently supports only Unix based systems (i.e. Linux and BSD)
#endif

/*****************************/ /** \file
 This small library implements a reactor pattern using callbacks.

Here are the supported events and their callbacks:

- File Descriptor events:

    - Ready to Read (`on_data` callback).

    - Ready to Write (`on_ready` callback).

    - Closed (`on_close` callback).

    - Reactor Exit (`on_shutdown` callback - will be called before the file
descriptor is closed and an `on_close` callback is fired).

Here's a quick example for an HTTP hellow world (no HTTP parsing required)...
most of the code is the damn socket binding...:

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
 */

/**
The Reactor struct holds the reactor's core data and settings.
*/
struct Reactor {
  /* File Descriptor Callbacks */

  /**
  Called when the file descriptor has incoming data. This is edge triggered
  and will not be called again unless all the previous data was consumed.
  */
  void (*on_data)(struct Reactor* reactor, int fd);
  /**
  Called when the file descriptor is ready to send data (outgoing).
  */
  void (*on_ready)(struct Reactor* reactor, int fd);
  /**
  Called for any open file descriptors when the reactor is shutting down.
  */
  void (*on_shutdown)(struct Reactor* reactor, int fd);
  /**
  Called when a file descriptor was closed REMOTELY. `on_close` will NOT get
  called when a connection is closed locally, unless using `reactor_close`
  function.
  */
  void (*on_close)(struct Reactor* reactor, int fd);

  /* global data and settings */

  /** the time (seconds since epoch) of the last "tick" (event cycle) */
  time_t last_tick;
  /** REQUIRED: the maximum value for a file descriptor that the reactor will be
  required
  to handle (the capacity -1). */
  int maxfd;

  /** this data is set by the system, dont alter this data */
  struct {
    /** The file descriptor designated by kqueue / epoll - do NOT touch!. */
    int reactor_fd;
    /** a map for all the active file descriptors that were added to the
    reactor - do NOT edit! */
    char* map;
    /** the reactor's events array - do NOT touch! */
    void* events;
  } private;
};

/**
Initializes the reactor, making the reactor "live".

once initialized, the reactor CANNOT be forked, so do not fork the process after
calling `reactor_init`, or data corruption will be experienced.

Returns -1 on error, otherwise returns 0.
*/
int reactor_init(struct Reactor*);
/**
Reviews any pending events (up to REACTOR_MAX_EVENTS)

Returns -1 on error, otherwise returns the number of events handled by the
reactor.
*/
int reactor_review(struct Reactor*);
/**
Closes the reactor, releasing it's resources (except the actual struct Reactor,
which might have been allocated on the stack and should be handled by the
caller).
*/
void reactor_stop(struct Reactor*);
/**
Adds a file descriptor to the reactor, so that callbacks will be called for it's
events.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add(struct Reactor*, int fd);
/**
Removes a file descriptor from the reactor - further callbacks won't be called.

Returns -1 on error, otherwise return value is system dependent. If the file
descriptor wasn't owned by
the reactor, it isn't an error.
*/
int reactor_remove(struct Reactor*, int fd);
/**
Closes a file descriptor, calling it's callback if it was registered with the
reactor.
*/
void reactor_close(struct Reactor*, int fd);
/**
Adds a file descriptor as a timer object.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add_timer(struct Reactor*, int fd, long milliseconds);
/**
epoll requires the timer to be "reset" before repeating. Kqueue requires no such
thing.

This method promises that the timer will be repeated when running on epoll. This
method is redundent on kqueue.
*/
void reactor_reset_timer(int fd);
/**
Creates a timer file descriptor, system dependent.

Returns -1 on error, otherwise returns the file descriptor.
*/
int reactor_make_timer(void);

#endif /* end of include guard: LIBREACT_H */
