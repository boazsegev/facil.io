/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_SERVER
#define LIB_SERVER "0.4.0"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

/* lib server is based off and requires the following libraries: */
#include "libreact.h"
#include "libasync.h"
#include "libsock.h"

#ifndef SERVER_PRINT_STATE
/**
When SERVER_PRINT_STATE is set to 1, the server API will print out common
messages regarding the server state (start / finish / listen messages).
*/
#define SERVER_PRINT_STATE 1
#endif

#if LIB_ASYNC_VERSION_MINOR != 4 || LIB_REACT_VERSION_MINOR != 3 || \
    LIB_SOCK_VERSION_MINOR != 1
#warning Lib-Server dependency versions are not in sync. Please review API versions.
#endif

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/** \file
## LibServer - a dynamic protocol network services library

* Library (not a framework): meanning, closer to the metal, abstracting only
what is required for API simplicity, error protection and performance.

* Dynamic Protocol: meanning a service can change protocols mid-stream. Example
usecase: Websockets (HTTP Upgrade).

* Network services: meanning multiple listenning network ports, each with it's
own logic.

`libserver` utilizes `libreact`, `libasync` and `libsock` to create a simple
API wrapper around these minimalistic libraries and managing the "glue" that
makes them work together.

It's simple and it's awesome :-)

Here's a simple example that emulates an HTTP hello world server. This example
will count the number of client and messages using the server and demonstrates
some recommended implementation techniques, such as protocol inheritance.

      #include "libserver.h"  // the reactor library
      #include <stdatomic.h>

      #define THREADS 4
      #define PROCESSES 4

      void on_close(protocol_s* protocol);
      void on_shutdown(intptr_t sock, protocol_s* protocol);
      void on_data(intptr_t sock, protocol_s* protocol);
      protocol_s* demo_on_open(intptr_t fd, void* udata);

      // Our demo protocol object uses "C" style inheritance,
      // where pointers location are the same so that a simple cast
      // from one object to the other, allows us to access more data.
      struct DemoProtocol {
        protocol_s protocol; // must be first for C style inheritance
        size_t _Atomic opened;
        size_t _Atomic closed;
        size_t _Atomic shutdown;
        size_t _Atomic messages;
      } demo_protocol = {
          .protocol.service = "Demo", // This allows us to ID the protocol type.
          .protocol.on_data = on_data,
          .protocol.on_shutdown = on_shutdown,
          .protocol.on_close = on_close,
      };

      // type-casting helper
      #define pr2demo(protocol) ((struct DemoProtocol*)(protocol))

      // A simple Hello World HTTP response emulation
      char hello_message[] =
          "HTTP/1.1 200 OK\r\n"
          "Content-Length: 12\r\n"
          "Connection: keep-alive\r\n"
          "Keep-Alive: 1;timeout=5\r\n"
          "\r\n"
          "Hello World!";

      protocol_s* on_open(intptr_t fd, void* udata) {
        // Count events
        atomic_fetch_add(&demo_protocol.opened, 1);
        // Set timeout
        server_set_timeout(fd, 5);
        // * return pointer to the protocol.
        // * This is the same as `(protocol_s *)&demo_protocol`
        return &demo_protocol.protocol;
      }

      void on_data(intptr_t sock, protocol_s* protocol) {
        // read data
        char data[1024];
        if (sock_read(sock, data, 1024)) {
          // Count event
          atomic_fetch_add(&pr2demo(protocol)->messages, 1);
          // send reply
          sock_write(sock, hello_message, sizeof(hello_message) - 1);
        }
      }

      void on_close(protocol_s* protocol) {
        // Count event
        atomic_fetch_add(&pr2demo(protocol)->closed, 1);
      }

      void on_shutdown(intptr_t sock, protocol_s* protocol) {
        // Count event
        atomic_fetch_add(&pr2demo(protocol)->shutdown, 1);
      }

      void on_idle(void) {
        // idle event example
        fprintf(stderr, "Server was idle, with %lu connections.\n",
                server_count(NULL));
      }

      int main() {
        // this isn't required normally,
        // but some systems require atomics to be initialized.
        atomic_store(&demo_protocol.opened, 0);
        atomic_store(&demo_protocol.closed, 0);
        atomic_store(&demo_protocol.shutdown, 0);
        atomic_store(&demo_protocol.messages, 0);
        // run the server
        server_listen(.port = "3000", .on_open = on_open, .udata = NULL);
        server_run(.threads = THREADS, .processes = PROCESSES,
                   .on_idle = on_idle);
        // print results
        fprintf(stderr,
                "** Server returned\n"
                "** %lu clients connected.\n"
                "** %lu clients disconnected.\n"
                "** %lu clients were connected when shutdown was called.\n"
                "** %lu messages were sent\n",
                atomic_load(&demo_protocol.opened),
                atomic_load(&demo_protocol.closed),
                atomic_load(&demo_protocol.shutdown),
                atomic_load(&demo_protocol.messages));
      }


*/

/**************************************************************************/ /**
* General info
*/

/* The following types are defined for the userspace of this library: */

// struct Server; /** used internally. no public data exposed */
struct ServerSettings;        /** sets up the server's behavior */
struct ServerServiceSettings; /** sets up a listenning socket's behavior */
typedef struct Protocol protocol_s; /** controls connection events */

/**************************************************************************/ /**
* The Protocol

The Protocol struct defines the callbacks used for the connection and sets the
behaviour for the connection's protocol.

All the callbacks recieve a unique connection ID (a semi UUID) that can be
converted to the original file descriptor if in need.

This allows the Server API to prevent old connection handles from sending data
to new connections after a file descriptor is "recycled" by the OS.
*/
struct Protocol {
  /**
  * A string to identify the protocol's service (i.e. "http").
  *
  * The string should be a global constant, only a pointer comparison will be
  * made (not `strcmp`).
  */
  const char* service;
  /** called when a data is available, but will not run concurrently */
  void (*on_data)(intptr_t fduuid, protocol_s* protocol);
  /** called when the socket is ready to be written to. */
  void (*on_ready)(intptr_t fduuid, protocol_s* protocol);
  /** called when the server is shutting down,
   * but before closing the connection. */
  void (*on_shutdown)(intptr_t fduuid, protocol_s* protocol);
  /** called when the connection was closed, but will not run concurrently */
  void (*on_close)(protocol_s* protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t fduuid, protocol_s* protocol);
  /** private metadata used for object protection */
  atomic_bool callback_lock;
};

/**************************************************************************/ /**
* The Service Settings

These settings will be used to setup listenning sockets.
*/
struct ServerServiceSettings {
  /** Called whenever a new connection is accepted. Should return a pointer to
   * the connection's protocol. */
  protocol_s* (*on_open)(intptr_t fduuid, void* udata);
  /** The network service / port. Defaults to "3000". */
  const char* port;
  /** The socket binding address. Defaults to the recommended NULL. */
  const char* address;
  /** Opaque user data. */
  void* udata;
  /**
  * Called when the server starts, allowing for further initialization, such as
  * timed event scheduling.
  *
  * This will be called seperately for every process. */
  void (*on_start)(void* udata);
  /** called when the server is done, to clean up any leftovers. */
  void (*on_finish)(void* udata);
};

/**************************************************************************/ /**
* The Server Settings

These settings will be used to setup server behaviour. missing settings will be
filled in with default values.
*/
struct ServerSettings {
  /** called if the event loop in cycled with no pending events. */
  void (*on_idle)(void);
  /**
  * Called when the server starts, allowing for further initialization, such as
  * timed event scheduling.
  *
  * This will be called seperately for every process. */
  void (*on_init)(void);
  /** called when the server is done, to clean up any leftovers. */
  void (*on_finish)(void);
  /**
  Sets the amount of threads to be created for the server's thread-pool.
  Defaults to 1 - the reactor and all callbacks will work using a single working
  thread, allowing for an evented single threaded design.
  */
  size_t threads;
  /** Sets the amount of processes to be used (processes will be forked).
  Defaults to 1 working processes (no forking). */
  size_t processes;
};

/* *****************************************************************************
* The Server API
* (and helper functions)
*/

/* *****************************************************************************
* Server actions
*/

/**
Listens to a server with any of the following server settings:

* `.threads` the number of threads to initiate in the server's thread pool.

* `.processes` the number of processes to use (a value of more then 1 will
initiate a `fork`).

* `.on_init` an on initialization callback (for every process forked).
`void (*callback)(void);``

* `.on_finish` a post run callback (for every process forked).
`void (*callback)(void);``

* `.on_idle` an idle server callback (for every process forked).
`void (*callback)(void);``

This method blocks the current thread until the server is stopped when a
SIGINT/SIGTERM is received.

To kill the server use the `kill` function with a SIGINT.
*/
int server_listen(struct ServerServiceSettings);
#define server_listen(...) \
  server_listen((struct ServerServiceSettings){__VA_ARGS__})
/** runs the server, hanging the current process and thread. */
ssize_t server_run(struct ServerSettings);
#define server_run(...) server_run((struct ServerSettings){__VA_ARGS__})
/** Stops the server, shouldn't be called unless int's impossible to send an
 * INTR signal. */
void server_stop(void);
/**
Returns the last time the server reviewed any pending IO events.
*/
time_t server_last_tick(void);
/****************************************************************************
* Socket actions
*/

/**
Gets the active protocol object for the requested file descriptor.

Returns NULL on error (i.e. connection closed), otherwise returns a `protocol_s`
pointer.
*/
protocol_s* server_get_protocol(intptr_t uuid);
/**
Sets a new active protocol object for the requested file descriptor.

This also schedules the old protocol's `on_close` callback to run, making sure
all resources are released.

Returns -1 on error (i.e. connection closed), otherwise returns 0.
*/
ssize_t server_switch_protocol(intptr_t fd, protocol_s* new_protocol);
/**
Sets a connection's timeout.

Returns -1 on error (i.e. connection closed), otherwise returns 0.
*/
void server_set_timeout(intptr_t uuid, uint8_t timeout);

/** Attaches an existing connection (fd) to the server's reactor and protocol
management system, so that the server can be used also to manage connection
based resources asynchronously (i.e. database resources etc').

On failure the fduuid_u.data.fd value will be -1.
*/
intptr_t server_attach(int fd, protocol_s* protocol);
/** Hijack a socket (file descriptor) from the server, clearing up it's
resources. The control of the socket is totally relinquished.

This method will block until all the data in the buffer is sent before
releasing control of the socket.

The returned value is the fd for the socket, or -1 on error.
*/
int server_hijack(intptr_t uuid);
/** Counts the number of connections for the specified protocol (NULL = all
protocols). */
long server_count(char* service);

/****************************************************************************
* Read and Write
*
* Simpley use `libsock` API for read/write.
*/

/**
Sends data from a file as if it were a single atomic packet (sends up to
length bytes or until EOF is reached).

Once the file was sent, the `source_fd` will be closed using `close`.

The file will be buffered to the socket chunk by chunk, so that memory
consumption is capped. The system's `sendfile` might be used if conditions
permit.

`offset` dictates the starting point for te data to be sent and length sets
the maximum amount of data to be sent.

Returns -1 and closes the file on error. Returns 0 on success.
*/
__unused static inline ssize_t sock_sendfile(intptr_t uuid,
                                             int source_fd,
                                             off_t offset,
                                             size_t length) {
  return sock_write2(.fduuid = uuid, .buffer = (void*)((intptr_t)source_fd),
                     .length = length, .is_fd = 1, .offset = offset);
}

/****************************************************************************
* Tasks + Async
*/

/**
Schedules a specific task to run asyncronously for each connection (except the
origin connection).
a NULL service identifier == all connections (all protocols).

The task is performed within each target connection's busy "lock", meanning no
two tasks (or `on_data` events) should be performed at the same time
(concurrency will be avoided within the context of each connection, except for
`on_shutdown`, `on_close` and `ping`).

The `on_finish` callback will be called once the task is finished and it will
receive the originating connection's UUID (could be 0). The originating
connection might have been closed by that time.

The `service` string (pointer) identifier MUST be a constant string object OR
a string that will persist until the `on_finish` callback is called. In other
words, either hardcode the string or use `malloc` to allocate it before
calling `each` and `free` to release the string from within the `on_finish`
callback.

It is recommended the `on_finish` callback is only used to perform any
resource cleanup necessary.
*/
void server_each(intptr_t origin_uuid,
                 const char* service,
                 void (*task)(intptr_t uuid, protocol_s* protocol, void* arg),
                 void* arg,
                 void (*on_finish)(intptr_t origin_uuid,
                                   protocol_s* protocol,
                                   void* arg));
/** Schedules a specific task to run asyncronously for a specific connection.

returns -1 on failure, 0 on success (success being scheduling the task).

If a connection was terminated before performing their sceduled tasks, the
`fallback` task will be performed instead.

It is recommended to perform any resource cleanup within the fallback function
and call the fallback function from within the main task, but other designes
are valid as well.
*/
void server_task(intptr_t uuid,
                 void (*task)(intptr_t uuid, protocol_s* protocol, void* arg),
                 void* arg,
                 void (*fallback)(intptr_t uuid, void* arg));
/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will repeat `repetitions` times. if
`repetitions` is set to 0, task will repeat forever. Returns -1 on error
or the new file descriptor on succeess.
*/
int server_run_every(size_t milliseconds,
                     size_t repetitions,
                     void (*task)(void*),
                     void* arg,
                     void (*on_finish)(void*));

/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will NOT repeat. Returns -1 on error or the
new file descriptor on succeess. */
__unused static inline int server_run_after(size_t milliseconds,
                                            void task(void*),
                                            void* arg) {
  return server_run_every(milliseconds, 1, task, arg, NULL);
}

#endif
