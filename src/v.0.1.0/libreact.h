/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIBREACT_H
#define LIBREACT_H

#define LIBREACT_VERSION 0.1.0

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/time.h>
#include <sys/types.h>

#if !defined(__unix__) && !defined(__linux__) && !defined(__APPLE__) && \
    !defined(__CYGWIN__)
#error This library currently supports only Unix based systems (i.e. Linux and BSD)
#endif

#define reactor_start_with(...)                            \
  {                                                        \
    struct ReactorSettings __the_actual_protocol_object_ = \
        (struct ReactorSettings){__VA_ARGS__};             \
    reactor_start(&__the_actual_protocol_object_);         \
  }

/*****************************/ /**
 This small library implementing a reactor pattern using callbacks.

Here are the supported events and their callbacks:

- File Descriptor events:

      - Ready to Read (`on_data` callback).

      - Ready to Write (`on_ready` callback).

      - Closed (`on_close` callback).

      - Reactor Exit (`on_shutdown` callback - will be called before the file
descriptor is closed and an `on_close` callback is fired).

- Global events:

      - "tick" (`on_tick` callback), whenever a cycle occurs.

      - Idle (`on_idle` callback), whenever a cycle with no events has occured.


 Here's a full example:

 ```
 #include "libreact.h"
// we're writing a small server, many included files...
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <netdb.h>
 #include <string.h>
 #include <fcntl.h>
 #include <stdio.h>
 #include <unistd.h>
 #include <signal.h>
 #include <errno.h>

 // to make it simple, we'll have a global object.
 struct ReactorSettings *global_reactor;

 // this will initialize the global object.
 void on_init(struct ReactorSettings *reactor) { global_reactor = reactor; };

 // this will accept connections, send data and close the connection.
 void on_data(struct ReactorSettings *reactor, int fd);

 // this will handle the exit signal (^C).
 void on_sigint(int sig);

 int main(int argc, char const *argv[]) {
   printf("starting up a rude server on port 3000\n");
   // setup the exit signal
   signal(SIGINT, on_sigint);

   // create a server socket... this will take a moment...
   int srvfd;
   char *port = "3000";
   // setup the address
   struct addrinfo hints;
   struct addrinfo *servinfo;       // will point to the results
   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
   if (getaddrinfo(NULL, port, &hints, &servinfo)) {
     perror("addr err");
     return -1;
   }
   // get the file descriptor
   srvfd =
       socket(servinfo->ai_family, servinfo->ai_socktype,
servinfo->ai_protocol);
   if (srvfd <= 0) {
     perror("addr err");
     return -1;
   }
   // bind the address to the socket
   if (bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
     perror("bind err");
     return -1;
   }
   // make sure the socket is non-blocking
   static int flags;
   if (-1 == (flags = fcntl(srvfd, F_GETFL, 0)))
     flags = 0;
   fcntl(srvfd, F_SETFL, flags | O_NONBLOCK);
   // listen in
   listen(srvfd, SOMAXCONN);
   if (errno)
     perror("starting. last error was");

   // now that everything is ready, call the reactor library...
   enter_reactor_loop(.on_data = on_data, .first = srvfd, .on_init = on_init);
   if (errno)
     perror("\nFinished. last error was");
 }
 void on_data(struct ReactorSettings *reactor, int fd) {
   int client = 0;
   unsigned int len = 0;
   if (fd == reactor->first) {
     // yes, this is our listening socket...
     // we can add more using:
     //    reactor->add(reactor, new_socket);
     while ((client = accept(fd, NULL, &len)) > 0) {
       printf("fire in the hole, throw them out!\n");
       send(client, "Goodbye!\n", 9, 0);
       close(client);
     } // reactor is edge triggered, we need to clear the cache.
   }
 }
 void on_sigint(int sig) {
   // stop the reactor
   if (global_reactor)
     global_reactor->stop(global_reactor);
 }

 ```

As you can see, entering an event based reactor pattern is easy... but writing
the server code and all the protocol dynamics (no to mention, parsing) that's
difficult.

To make things even simpler, check out the lib-server library with it's dynamic
protocol and multi-threading capabilities. This will leave you with simply
writing the actual network protocol, and you can usually find an existing
library for that :-)

 */

struct ReactorSettings {
  // File Descriptor Callbacks

  /// called when the file descriptor has incoming data. This is edge triggered
  /// and will not be called again unless all the previous data was consumed.
  void (*on_data)(struct ReactorSettings* reactor, int fd);
  /// called when the file descriptor is ready to send data (outgoing).
  void (*on_ready)(struct ReactorSettings* reactor, int fd);
  /// called for any open file descriptors when the reactor is shutting down.
  void (*on_shutdown)(struct ReactorSettings* reactor, int fd);
  /// called when a file descriptor was closed (either locally or by the other
  /// party, when it's a socket or a pipe).
  void (*on_close)(struct ReactorSettings* reactor, int fd);

  // global callbacks

  /// called after the event loop was created but before any cycling takes
  /// place, allowing for further initialization (such as adding a server
  /// socket, setting up timers, etc').
  void (*on_init)(struct ReactorSettings* reactor);
  /// called whenever an event loop had cycled (a "tick").
  void (*on_tick)(struct ReactorSettings* reactor);
  /// called if an event loop cycled with no pending events.
  void (*on_idle)(struct ReactorSettings* reactor);

  // global data and settings

  /// the maximum amount of milliseconds to wait for events before `on_idle` is
  /// called. If you're using ticks or idle events to handle timers or timout
  /// events, it should be set to a shorted amount of time (i.e. 1000
  /// milliseconds). defaults to 1 second (1000 milli). set to -1 for an infinit
  /// wait.
  int tick;
  // the time (seconds since epoch) of the last "tick" (event cycle)
  time_t last_tick;
  /// what's the point of entering an event loop reactor if we're not listening
  /// to ANY events?! ... well, this will be the first event we'll listen to,
  /// allowing us to easily listen a server socket without resorting to an
  /// `on_init` callback. Use `on_init` to setup more than one.
  int first;
  /// the maximum value for a file descriptor that the reactor will be required
  /// to handle. defaults to the maximum capacity for the process.
  int last;
  /// The maximum events to be handled during a single event-loop cycle.
  /// Defaults to 64 events.
  int event_per_cycle;

  /// opaque user data.
  void* udata;

  // helper functions

  /// an instance function that will gracefully shut down the reactor.
  void (*stop)(struct ReactorSettings* reactor);
  /// adds a file descriptor to the reactor. returns -1 on error and 0 on
  /// sucess. review the error using errno.
  int (*add)(struct ReactorSettings* reactor, int fd);
  /// returns the file descriptor if it's mapped as active, otherwise returns 0.
  int (*exists)(struct ReactorSettings* reactor, int fd);
  /// adds a special file descriptor to the reactor, setting it up as a timer on
  /// kqueue/epoll. might fail if future versions support `select`.
  ///
  /// this might require (on linux) a `timerfd_create` call.
  int (*add_as_timer)(struct ReactorSettings* reactor,
                      int fd,
                      long milliseconds);
  /// epoll requires the timer to be "reset" before repeating. Kqueue requires
  /// no such thing. This method promises that the timer will be repeated when
  /// running on epoll. This method is redundent on kqueue.
  void (*reset_timer)(struct ReactorSettings* reactor, int fd);
  /// removes a file descriptor from the reactor without calling and callbacks.
  /// returns -1 on error and 0 on sucess. review the error using errno.
  /// Remember to close any file descriptors you hijack.
  int (*hijack)(struct ReactorSettings* reactor, int fd);
  /// returns a new file descriptor that can be used for setting up a timer.
  /// remember to close the new file descriptor when you're done with it.
  int (*open_timer_fd)(void);

  // set by the system.

  /// the inner data used by the reactor
  struct _Reactor_Data_ {
    /// The file descriptor designated by kqueue / epoll.
    int const reactor_fd;
    /// the event loop flag. setting it to false will stop the loop after next
    /// "tick".
    int run;
    /// a map for all the active file descriptors that were added to the
    /// reactor.
    char* map;
  } private;
};

/// The main library function. Takes a ReactorSettings structure and initiates a
/// reactor pattern loop using kqueue/epoll. This method will block your code.
int reactor_start(struct ReactorSettings*);

#endif /* end of include guard: LIBREACT_H */
