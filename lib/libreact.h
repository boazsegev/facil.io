/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIBREACT_H
#define LIBREACT_H

#define LIBREACT_VERSION 0.2.0

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

/*****************************/ /**
 This small library implements a reactor pattern using callbacks.

Here are the supported events and their callbacks:

- File Descriptor events:

      - Ready to Read (`on_data` callback).

      - Ready to Write (`on_ready` callback).

      - Closed (`on_close` callback).

      - Reactor Exit (`on_shutdown` callback - will be called before the file
descriptor is closed and an `on_close` callback is fired).

 */
struct Reactor {
  // File Descriptor Callbacks

  /// called when the file descriptor has incoming data. This is edge triggered
  /// and will not be called again unless all the previous data was consumed.
  void (*on_data)(struct Reactor* reactor, int fd);
  /// called when the file descriptor is ready to send data (outgoing).
  void (*on_ready)(struct Reactor* reactor, int fd);
  /// called for any open file descriptors when the reactor is shutting down.
  void (*on_shutdown)(struct Reactor* reactor, int fd);
  /// called when a file descriptor was closed REMOTELY. `on_close` will NOT get
  /// called when a connection is closed locally, unless using `reactor_close`
  /// function.
  void (*on_close)(struct Reactor* reactor, int fd);

  // global data and settings

  // the time (seconds since epoch) of the last "tick" (event cycle)
  time_t last_tick;
  /// the maximum value for a file descriptor that the reactor will be required
  /// to handle (the capacity -1).
  int maxfd;

  // this data is set by the system, dont alter this data
  struct {
    /// The file descriptor designated by kqueue / epoll - do NOT touch!.
    int reactor_fd;
    /// a map for all the active file descriptors that were added to the
    /// reactor - do NOT edit!
    char* map;
    /// the reactor's events array - do NOT touch!
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
