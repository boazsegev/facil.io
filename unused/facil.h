/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FACIL_H
/**
"facil.h" is the main header for the facil.io server platform.
*/
#define H_FACIL_H

#ifndef FACIL_PRINT_STATE
/**
When FACIL_PRINT_STATE is set to 1, facil.io will print out common messages
regarding the server state (start / finish / listen messages).
*/
#define FACIL_PRINT_STATE 1
#endif
/* *****************************************************************************
Required facil libraries
***************************************************************************** */
#include "defer.h"
#include "sock.h"

/* *****************************************************************************
Core object types
***************************************************************************** */

typedef struct Protocol protocol_s;
/**************************************************************************/ /**
* The Protocol

The Protocol struct defines the callbacks used for the connection and sets the
behaviour for the connection's protocol.

For concurrency reasons, a protocol instance SHOULD be unique to each
connections. Different connections shouldn't share a single protocol object
(callbacks and data can obviously be shared).

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
   * used (not `strcmp`).
   */
  const char *service;
  /** called when a data is available, but will not run concurrently */
  void (*on_data)(intptr_t uuid, protocol_s *protocol);
  /** called when the socket is ready to be written to. */
  void (*on_ready)(intptr_t uuid, protocol_s *protocol);
  /** called when the server is shutting down,
   * but before closing the connection. */
  void (*on_shutdown)(intptr_t uuid, protocol_s *protocol);
  /** called when the connection was closed, but will not run concurrently */
  void (*on_close)(protocol_s *protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t uuid, protocol_s *protocol);
  /** private metadata used by facil. */
  size_t rsv;
};

/**************************************************************************/ /**
* Creating a network Service Settings

These settings will be used to setup listening sockets.
*/
struct facil_listen_args {
  /** Called whenever a new connection is accepted. Should return a pointer to
   * the connection's protocol. */
  protocol_s *(*on_open)(intptr_t fduuid, void *udata);
  /** The network service / port. Defaults to "3000". */
  const char *port;
  /** The socket binding address. Defaults to the recommended NULL. */
  const char *address;
  /** Opaque user data. */
  void *udata;
  /**
   * Called when the server starts, allowing for further initialization, such as
   * timed event scheduling.
   *
   * This will be called seperately for every process. */
  void (*on_start)(void *udata);
  /** called when the server is done, to clean up any leftovers. */
  void (*on_finish)(void *udata);
};
/** Schedule a network service on a listening socket. */
int facil_listen(struct facil_listen_args args);
/** Schedule a network service on a listening socket. */
#define facil_listen(...) facil_listen((struct facil_listen_args){__VA_ARGS__})

/* *****************************************************************************
Core API
***************************************************************************** */

struct facil_run_args {
  /** The number of threads to run in the thread pool. */
  uint16_t threads;
  /** The number of processes to utilize. Both 0 and 1 are ignored. */
  uint16_t processes;
  /** called if the event loop in cycled with no pending events. */
  void (*on_idle)(void);
  /** called when the server is done, to clean up any leftovers. */
  void (*on_finish)(void);
};
void facil_run(struct facil_run_args args);
#define facil_run(...) facil_run((struct facil_run_args){__VA_ARGS__})
/**
 * Attaches (or updates) a protocol object to a socket UUID.
 *
 * The new protocol object can be NULL, which will practically "hijack" the
 * socket away from facil.
 *
 * The old protocol's `on_close` will be scheduled, if they both exist.
 *
 * Returns -1 on error and 0 on success.
 */
int facil_attach(intptr_t uuid, protocol_s *protocol);

/** Sets a timeout for a specific connection (if active). */
void facil_set_timeout(intptr_t uuid, uint8_t timeout);

/* *****************************************************************************
Helper API
***************************************************************************** */

/**
Returns the last time the server reviewed any pending IO events.
*/
time_t facil_last_tick(void);

/**
 * Creates a system timer (at the cost of 1 file descriptor).
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error or the new file descriptor on succeess.
 */
int facil_run_every(size_t milliseconds, size_t repetitions,
                    void (*task)(void *), void *arg, void (*on_finish)(void *));

#endif
