/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/io-core
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define H_FACIL_IO_H

/* *****************************************************************************
 * Table of contents (find by subject):
 * =================
 * Helper and compile time settings (MACROs)
 *
 * Connection Callback (Protocol) Management
 * Listening to Incoming Connections
 * Connecting to remote servers as a client
 * Starting the IO reactor and reviewing it's state
 * Socket / Connection Functions
 * Connection Read / Write Hooks, for overriding the system calls
 * Concurrency overridable functions
 * Connection Task scheduling
 * Event / Task scheduling
 * Startup / State Callbacks (fork, start up, idle, etc')
 * TLS Support (weak functions, to bea overriden by library wrapper)
 * Lower Level API - for special circumstances, use with care under
 *
 * Pub/Sub / Cluster Messages API
 * Cluster Messages and Pub/Sub
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 *
 * SipHash
 * SHA-1
 * SHA-2
 *
 *
 *
 * Quick Overview
 * ==============
 *
 * The core IO library follows an evented design and uses callbacks for IO
 * events. Using the API described in the Connection Callback (Protocol)
 * Management section:
 *
 * - Each connection / socket, is identified by a process unique number
 *   (`uuid`).
 *
 * - Connections are assigned protocol objects (`fio_protocol_s`) using the
 *   `fio_attach` function.
 *
 * - The callbacks in the protocol object are called whenever an IO event
 *   occurs.
 *
 * - Callbacks are protected using one of two connection bound locks -
 *   `FIO_PR_LOCK_TASK` for most tasks and `FIO_PR_LOCK_WRITE` for `on_ready`
 *   and `on_timeout` tasks.
 *
 * - User data is assumed to be stored in the protocol object using C style
 *   inheritance.
 *
 * Reading and writing operations use an internal user-land buffer and they
 * never fail... unless, the client is so slow that they appear to be attacking
 * the network layer (slowloris), the connection was lost due to other reasons
 * or the system is out of memory.
 *
 * Because the framework is evented, there's API that offers easy event and task
 * scheduling, including timers etc'. Also, connection events can be
 * rescheduled, allowing connections to behave like state-machines.
 *
 * The core library includes Pub/Sub (publish / subscribe) services which offer
 * easy IPC (inter process communication) in a network friendly API. Pub/Sub
 * services can be extended to synchronize with external databases such as
 * Redis.
 *
 **************************************************************************** */
/* *****************************************************************************
0001 macros.h
***************************************************************************** */

/* *****************************************************************************
Compilation Macros
***************************************************************************** */

#ifndef FIO_MAX_SOCK_CAPACITY
/**
 * The maximum number of connections per worker process.
 */
#define FIO_MAX_SOCK_CAPACITY 262144
#endif

#ifndef FIO_CPU_CORES_LIMIT
/**
 * If facil.io detects more CPU cores than the number of cores stated in the
 * FIO_CPU_CORES_LIMIT, it will assume an error and cap the number of cores
 * detected to the assigned limit.
 *
 * This is only relevant to automated values, when running facil.io with zero
 * threads and processes, which invokes a large matrix of workers and threads
 * (see {facil_run})
 *
 * The default auto-detection cap is set at 8 cores. The number is arbitrary
 * (historically the number 7 was used after testing `malloc` race conditions on
 * a MacBook Pro).
 *
 * This does NOT effect manually set (non-zero) worker/thread values.
 */
#define FIO_CPU_CORES_LIMIT 64
#endif

#ifndef FIO_PUBSUB_SUPPORT
/**
 * If true (1), compiles the facil.io pub/sub API.
 */
#define FIO_PUBSUB_SUPPORT 1
#endif

#ifndef FIO_TLS_PRINT_SECRET
/* If true, the master key secret SHOULD be printed using FIO_LOG_DEBUG */
#define FIO_TLS_PRINT_SECRET 0
#endif

#ifndef FIO_WEAK_TLS
/* If true, the weak-function TLS implementation will always be compiled. */
#define FIO_WEAK_TLS 0
#endif

#ifndef FIO_TLS_IGNORE_MISSING_ERROR
/* If true, a no-op TLS implementation will be enabled (for debugging). */
#define FIO_TLS_IGNORE_MISSING_ERROR 0
#endif

#ifndef FIO_TLS_TIMEOUT
/* The default timeout for TLS connections (protocol assignment deferred) */
#define FIO_TLS_TIMEOUT 4
#endif

/* *****************************************************************************
Import STL
***************************************************************************** */
#ifndef FIO_LOG_LENGTH_LIMIT
/**
 * Since logging uses stack memory rather than dynamic allocation, it's memory
 * usage must be limited to avoid exploding the stack. The following sets the
 * memory used for a logging event.
 */
#define FIO_LOG_LENGTH_LIMIT 2048
#endif

/* Backwards support for version 0.7.x memory allocator behavior */
#ifdef FIO_OVERRIDE_MALLOC
#warning FIO_OVERRIDE_MALLOC is deprecated, use FIO_MALLOC_OVERRIDE_SYSTEM
#define FIO_MALLOC_OVERRIDE_SYSTEM
#elif defined(FIO_FORCE_MALLOC)
#warning FIO_FORCE_MALLOC is deprecated, use FIO_MEMORY_DISABLE
#define FIO_MEMORY_DISABLE
#endif

/* let it run once without side-effects, to prevent self-inclusion CLI errors */
#define FIO_LOG
#define FIO_EXTERN
#include "fio-stl.h"

#define FIO_RISKY_HASH 1
#include "fio-stl.h"

/* Enable CLI extension before enabling the custom memory allocator. */
#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_EXTERN
#define FIO_CLI
#include "fio-stl.h"

/* Enable custom memory allocator. */
#define FIO_EXTERN
#define FIO_MALLOC
#include "fio-stl.h"

/* Enable required extensions and FIOBJ types. */
#define FIO_EXTERN
#define FIO_ATOMIC
#define FIO_LOCK
#define FIO_BITWISE
#define FIO_ATOL
#define FIO_NTOL
#define FIO_RAND
#define FIO_TIME
#define FIO_GLOB_MATCH
#define FIO_URL
#include "fio-stl.h"
#define FIOBJ_EXTERN
#define FIOBJ_MALLOC
#define FIO_FIOBJ
#include "fio-stl.h"

#include <limits.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/* *****************************************************************************
C++ extern start
***************************************************************************** */
/* support C++ */
#ifdef __cplusplus
extern "C" {
/* C++ keyword was deprecated */
#ifndef register
#define register
#endif
#endif
/* ************************************************************************** */
#ifndef FIO_MAX_SOCK_CAPACITY /* Development inclusion - ignore line */
#include "0002 includes.h"    /* Development inclusion - ignore line */
#endif                        /* Development inclusion - ignore line */
/* ************************************************************************** */

/** The main protocol object type. See `struct fio_protocol_s`. */
typedef struct fio_protocol_s fio_protocol_s;

/** An opaque type used for the SSL/TLS functions. */
typedef struct fio_tls_s fio_tls_s;

/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */

struct fio_start_args {
  /**
   * The number of threads to run in the thread pool. Has "smart" defaults.
   *
   *
   * A positive value will indicate a set number of threads (or workers).
   *
   * Zeros and negative values are fun and include an interesting shorthand:
   *
   * * Negative values indicate a fraction of the number of CPU cores. i.e.
   *   -2 will normally indicate "half" (1/2) the number of cores.
   *
   * * If the other option (i.e. `.workers` when setting `.threads`) is zero,
   *   it will be automatically updated to reflect the option's absolute value.
   *   i.e.:
   *   if .threads == -2 and .workers == 0,
   *   than facil.io will run 2 worker processes with (cores/2) threads per
   *   process.
   */
  int16_t threads;
  /** The number of worker processes to run. See `threads`. */
  int16_t workers;
};

/**
 * Starts the facil.io event loop. This function will return after facil.io is
 * done (after shutdown).
 *
 * See the `struct fio_start_args` details for any possible named arguments.
 *
 * This method blocks the current thread until the server is stopped (when a
 * SIGINT/SIGTERM is received).
 */
void fio_start(struct fio_start_args args);
#define fio_start(...) fio_start((struct fio_start_args){__VA_ARGS__})

/**
 * Attempts to stop the facil.io application. This only works within the Root
 * process. A worker process will simply respawn itself.
 */
void fio_stop(void);

/**
 * Returns the last time the server reviewed any pending IO events.
 */
struct timespec fio_last_tick(void);

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void);

/**
 * Returns the number of expected threads / processes to be used by facil.io.
 *
 * The pointers should start with valid values that match the expected threads /
 * processes values passed to `fio_start`.
 *
 * The data in the pointers will be overwritten with the result.
 */
void fio_expected_concurrency(int16_t *restrict threads,
                              int16_t *restrict workers);

/**
 * Returns the number of worker processes if facil.io is running.
 *
 * (1 is returned when in single process mode, otherwise the number of workers)
 */
int16_t fio_is_running(void);

/**
 * Returns 1 if the current process is a worker process or a single process.
 *
 * Otherwise returns 0.
 *
 * NOTE: When cluster mode is off, the root process is also the worker process.
 *       This means that single process instances don't automatically respawn
 *       after critical errors.
 */
int fio_is_worker(void);

/**
 * Returns 1 if the current process is the master (root) process.
 *
 * Otherwise returns 0.
 */
int fio_is_master(void);

/** Returns facil.io's parent (root) process pid. */
pid_t fio_master_pid(void);

/** Returns the current process pid. */
pid_t fio_getpid(void);

/**
 * Initializes zombie reaping for the process. Call before `fio_start` to enable
 * global zombie reaping.
 */
void fio_reap_children(void);

/**
 * Resets any existing signal handlers, restoring their state to before they
 * were set by facil.io.
 *
 * This stops both child reaping (`fio_reap_children`) and the default facil.io
 * signal handlers (i.e., CTRL-C).
 *
 * This function will be called automatically by facil.io whenever facil.io
 * stops.
 */
void fio_signal_handler_reset(void);

/* *****************************************************************************
Startup / State Callbacks (fork, start up, idle, etc')
***************************************************************************** */

/** a callback type signifier */
typedef enum {
  /** Called once during library initialization. */
  FIO_CALL_ON_INITIALIZE,
  /** Called once before starting up the IO reactor. */
  FIO_CALL_PRE_START,
  /** Called before each time the IO reactor forks a new worker. */
  FIO_CALL_BEFORE_FORK,
  /** Called after each fork (both parent and child), before FIO_CALL_IN_XXX */
  FIO_CALL_AFTER_FORK,
  /** Called by a worker process right after forking. */
  FIO_CALL_IN_CHILD,
  /** Called by the master process after spawning a worker (after forking). */
  FIO_CALL_IN_MASTER,
  /** Called every time a *Worker* proceess starts. */
  FIO_CALL_ON_START,
  /** SHOULD be called by pub/sub engines after they (re)connect to backend. */
  FIO_CALL_ON_PUBSUB_CONNECT,
  /** SHOULD be called by pub/sub engines for backend connection errors. */
  FIO_CALL_ON_PUBSUB_ERROR,
  /** User state event queue (unused, available to the user). */
  FIO_CALL_ON_USR,
  /** Called when facil.io enters idling mode. */
  FIO_CALL_ON_IDLE,
  /** A reversed user state event queue (unused, available to the user). */
  FIO_CALL_ON_USR_REVERSE,
  /** Called before starting the shutdown sequence. */
  FIO_CALL_ON_SHUTDOWN,
  /** Called just before finishing up (both on chlid and parent processes). */
  FIO_CALL_ON_FINISH,
  /** Called by each worker the moment it detects the master process crashed. */
  FIO_CALL_ON_PARENT_CRUSH,
  /** Called by the parent (master) after a worker process crashed. */
  FIO_CALL_ON_CHILD_CRUSH,
  /** An alternative to the system's at_exit. */
  FIO_CALL_AT_EXIT,
  /** used for testing and array allocation - must be last. */
  FIO_CALL_NEVER
} callback_type_e;

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e, void (*func)(void *), void *arg);

/** Removes a callback from the list of callbacks to be called for the event. */
int fio_state_callback_remove(callback_type_e, void (*func)(void *), void *arg);

/**
 * Forces all the existing callbacks to run, as if the event occurred.
 *
 * Callbacks for all initialization / idling tasks are called in order of
 * creation (where callback_type_e <= FIO_CALL_ON_IDLE).
 *
 * Callbacks for all cleanup oriented tasks are called in reverse order of
 * creation (where callback_type_e >= FIO_CALL_ON_SHUTDOWN).
 *
 * During an event, changes to the callback list are ignored (callbacks can't
 * add or remove other callbacks for the same event).
 */
void fio_state_callback_force(callback_type_e);

/** Clears all the existing callbacks for the event. */
void fio_state_callback_clear(callback_type_e);

/* *****************************************************************************
Event / Task scheduling
***************************************************************************** */

/**
 * Defers a task's execution.
 *
 * Tasks are functions of the type `void task(void *, void *)`, they return
 * nothing (void) and accept two opaque `void *` pointers, user-data 1
 * (`udata1`) and user-data 2 (`udata2`).
 *
 * Returns -1 or error, 0 on success.
 */
int fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error occurs or the connection is closed before the task can run, the
 * task will be called with a NULL protocol pointer, for resource cleanup.
 */
int fio_defer_io_task(intptr_t uuid,
                      void (*task)(intptr_t uuid,
                                   fio_protocol_s *,
                                   void *udata),
                      void *udata);

/**
 * Creates a timer to run a task at the specified interval.
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to -1, task
 * will repeat forever.
 *
 * If `task` returns a non-zero value, it will stop repeating.
 *
 * The `on_finish` handler is always called (even on error).
 */
void fio_run_every(size_t milliseconds,
                   int32_t repetitions,
                   int (*task)(void *, void *),
                   void *udata1,
                   void *udata2,
                   void (*on_finish)(void *, void *));

/**
 * Performs all deferred tasks.
 */
void fio_defer_perform(void);

/** Returns true if there are deferred functions waiting for execution. */
int fio_defer_has_queue(void);

/* *****************************************************************************












Connection Callback (Protocol) Management












***************************************************************************** */

/**************************************************************************/ /**
The Protocol
============

The Protocol struct defines the callbacks used for the connection and sets it's
behavior. The Protocol struct is part of facil.io's core design.

For concurrency reasons, a protocol instance SHOULD be unique to each
connections. Different connections shouldn't share a single protocol object
(callbacks and data can obviously be shared).

All the callbacks receive a unique connection ID (a localized UUID) that can be
converted to the original file descriptor when in need.

This allows facil.io to prevent old connection handles from sending data
to new connections after a file descriptor is "recycled" by the OS.
*/
struct fio_protocol_s {
  /** Called when a data is available, but will not run concurrently */
  void (*on_data)(intptr_t uuid, fio_protocol_s *protocol);
  /** called once all pending `fio_write` calls are finished. */
  void (*on_ready)(intptr_t uuid, fio_protocol_s *protocol);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * The callback runs within a {FIO_PR_LOCK_TASK} lock, so it will never run
   * concurrently with {on_data} or other connection specific tasks.
   *
   * The `on_shutdown` callback should return 0 to close the socket or a number
   * between 1..254 to delay the socket closure by that amount of seconds.
   *
   * Once the socket was marked for closure, facil.io will allow 8 seconds for
   * all the data to be sent before forcefully closing the socket (regardless of
   * state).
   *
   * If the `on_shutdown` returns 255, the socket is ignored and it will be
   * abruptly terminated when all other sockets have finished their graceful
   * shutdown procedure.
   */
  uint8_t (*on_shutdown)(intptr_t uuid, fio_protocol_s *protocol);
  /** Called when the connection was closed, but will not run concurrently */
  void (*on_close)(intptr_t uuid, fio_protocol_s *protocol);
  /** called when a connection's timeout was reached */
  void (*on_timeout)(intptr_t uuid, fio_protocol_s *protocol);
  /** reserved data - used by facil.io internally */
  uintptr_t rsv;
};

/**
 * Attaches (or updates) a protocol object to a socket UUID.
 *
 * The new protocol object can be NULL, which will detach ("hijack"), the
 * socket .
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * On error, the new protocol's `on_close` callback will be called immediately.
 */
void fio_attach(intptr_t uuid, fio_protocol_s *protocol);

/**
 * Attaches (or updates) a protocol object to a file descriptor (fd).
 *
 * The new protocol object can be NULL, which will detach ("hijack"), the
 * socket and the `fd` can be one created outside of facil.io.
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * On error, the new protocol's `on_close` callback will be called immediately.
 *
 * NOTE: before attaching a file descriptor that was created outside of
 * facil.io's library, make sure it is set to non-blocking mode (see
 * `fio_set_non_block`). facil.io file descriptors are all non-blocking and it
 * will assumes this is the case for the attached fd.
 */
void fio_attach_fd(int fd, fio_protocol_s *protocol);

/** Sets a timeout for a specific connection (only when running and valid). */
void fio_timeout_set(intptr_t uuid, uint8_t timeout);

/** Gets a timeout for a specific connection. Returns 0 if none. */
uint8_t fio_timeout_get(intptr_t uuid);

/**
 * "Touches" a socket connection, resetting it's timeout counter.
 */
void fio_touch(intptr_t uuid);

enum fio_io_event {
  FIO_EVENT_ON_DATA,
  FIO_EVENT_ON_READY,
  FIO_EVENT_ON_TIMEOUT
};
/** Schedules an IO event, even if it did not occur. */
void fio_force_event(intptr_t uuid, enum fio_io_event);

/**
 * Temporarily prevents `on_data` events from firing.
 *
 * The `on_data` event will be automatically rescheduled when (if) the socket's
 * outgoing buffer fills up or when `fio_force_event` is called with
 * `FIO_EVENT_ON_DATA`.
 *
 * Note: the function will work as expected when called within the protocol's
 * `on_data` callback and the `uuid` refers to a valid socket. Otherwise the
 * function might quietly fail.
 */
void fio_suspend(intptr_t uuid);

/**
 * Returns the maximum number of open files facil.io can handle per worker
 * process.
 *
 * Total OS limits might apply as well but aren't shown.
 *
 * The value of 0 indicates either that the facil.io library wasn't initialized
 * yet or that it's resources were released.
 */
size_t fio_capa(void);

/* *****************************************************************************
Lower Level Protocol API - for special circumstances, use with care.
***************************************************************************** */

/**
 * This function allows out-of-task access to a connection's `fio_protocol_s`
 * object by attempting to acquire a locked pointer.
 *
 * CAREFUL: mostly, the protocol object will be locked and a pointer will be
 * sent to the connection event's callback. However, if you need access to the
 * protocol object from outside a running connection task, you might need to
 * lock the protocol to prevent it from being closed / freed in the background.
 *
 * On error, consider calling `fio_defer` or `fio_defer_io_task` instead of busy
 * waiting. Busy waiting SHOULD be avoided whenever possible.
 */
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid);
/** Don't unlock what you don't own... see `fio_protocol_try_lock` for
 * details. */
void fio_protocol_unlock(fio_protocol_s *pr);

/* *****************************************************************************
Listening to Incoming Connections
***************************************************************************** */

/* Arguments for the fio_listen function */
struct fio_listen_args {
  /** The binding address in URL format. Defaults to: tcp://0.0.0.0:3000 */
  const char *url;
  /**
   * Called whenever a new connection is accepted.
   *
   * Should either call `fio_attach` or close the connection.
   */
  void (*on_open)(intptr_t uuid, void *udata);
  /** Opaque user data. */
  void *udata;
  /** a pointer to a `fio_tls_s` object, for SSL/TLS support. */
  fio_tls_s *tls;
  /**
   * Called when the server is done, usable for cleanup.
   *
   * This will be called separately for every process before exiting.
   */
  void (*on_finish)(void *udata);
  /** Listen and connect using only the master process (non-worker). */
  uint8_t is_master_only;
};

/**
 * Sets up a network service on a listening socket.
 *
 * Returns the listening socket's uuid or -1 (on error).
 *
 * See the `fio_listen` Macro for details.
 */
intptr_t fio_listen(struct fio_listen_args args);

/** A ping function that does nothing except keeping the connection alive. */
void FIO_PING_ETERNAL(intptr_t, fio_protocol_s *);

/************************************************************************ */ /**
Listening to Incoming Connections
===

Listening to incoming connections is pretty straight forward.

After a new connection is accepted, the `on_open` callback is called. `on_open`
should allocate the new connection's protocol and call `fio_attach` to attach
the protocol to the connection's uuid.

The protocol's `on_close` callback is expected to handle any cleanup required.

The following is an example echo server using facil.io:

```c
#include <fio.h>

// A callback to be called whenever data is available on the socket
static void echo_on_data(intptr_t uuid, fio_protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  // echo buffer
  char buf[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  // Read to the buffer, starting after the "Echo: "
  while ((len = fio_read(uuid, buf + 6, 1018)) > 0) {
    fprintf(stderr, "Read: %.*s", (int)len, buf + 6);
    // Write back the message
    fio_write(uuid, buf, len + 6);
    // Handle goodbye
    if ((buf[6] | 32) == 'b' && (buf[7] | 32) == 'y' &&
        (buf[8] | 32) == 'e') {
      fio_write(uuid, "Goodbye.\n", 9);
      fio_close(uuid);
      return;
    }
  }
}

// A callback called whenever a timeout is reach
static void echo_on_timeout(intptr_t uuid, fio_protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  fio_write(uuid, "Server: Are you there?\n", 23);
}

// A callback called if the server is shutting down...
// ... while the connection is still open
static uint8_t echo_on_shutdown(intptr_t uuid, fio_protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  fio_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
  return 0;
}

static void echo_on_close(intptr_t uuid, fio_protocol_s *proto) {
  fprintf(stderr, "Connection %p closed.\n", (void *)proto);
  free(proto);
  (void)uuid;
}

// A callback called for new connections
static void echo_on_open(intptr_t uuid, void *udata) {
  (void)udata; // ignore this
  // Protocol objects MUST be dynamically allocated when multi-threading.
  fio_protocol_s *echo_proto = malloc(sizeof(*echo_proto));
  *echo_proto = (fio_protocol_s){.service = "echo",
                                 .on_data = echo_on_data,
                                 .on_shutdown = echo_on_shutdown,
                                 .on_close = echo_on_close,
                                 .on_timeout = echo_on_timeout};
  fprintf(stderr, "New Connection %p received from %s\n", (void *)echo_proto,
          fio_peer_addr(uuid).data);
  fio_attach(uuid, echo_proto);
  fio_write2(uuid, .data.buf = "Echo Service: Welcome\n", .len = 22,
             .after.dealloc = FIO_DEALLOC_NOOP);
  fio_timeout_set(uuid, 5);
}

int main() {
  // Setup a listening socket
  if (fio_listen(NULL, .on_open = echo_on_open) == -1) {
    perror("No listening socket available on port 3000");
    exit(-1);
  }
  // Run the server and hang until a stop signal is received.
  fio_start(.threads = 4, .workers = 1);
}
```
*/
#define fio_listen(url_, ...)                                                  \
  fio_listen((struct fio_listen_args){.url = (url_), __VA_ARGS__})

/* *****************************************************************************
Connecting to remote servers as a client
***************************************************************************** */

#ifndef FIO_RECONNECT_DELAY
/**
 * The number of milliseconds to wait before attempting a reconnect.
 *
 * A delay is important so reconnection attempts don't bombard and overrun both
 * the local IO reactor and the network at large.
 */
#define FIO_RECONNECT_DELAY 150
#endif

/**
Named arguments for the `fio_connect` function, that allows non-blocking
connections to be established.
*/
struct fio_connect_args {
  /** The address of the server we are connecting to in URL format. */
  const char *url;
  /** Called whenever a connection is (re)established. */
  void (*on_open)(intptr_t uuid, void *udata);
  /** Called for cleanup, when the connection won't be reopened. */
  void (*on_finish)(void *udata);
  /** a pointer to a `fio_tls_s` object, for SSL/TLS support. */
  fio_tls_s *tls;
  /** Opaque user data. */
  void *udata;
  /** A non-system timeout after which connection is assumed to have failed. */
  uint8_t timeout;
  /** If true, the connection will auto-reconnect while running. */
  uint8_t auto_reconnect;
  /** Connect using only the master process (non-worker). */
  uint8_t is_master_only;
};

typedef struct fio_connect_s fio_connect_s;
/**
Creates a client connection (in addition or instead of the server).

See the `struct fio_connect_args` details for any possible named arguments.

* `.address` should be the address of the server.

* `.port` the server's port.

* `.udata`opaque user data.

* `.on_connect` called once a connection was established.

    Should return a pointer to a `fio_protocol_s` object, to handle connection
    callbacks.

* `.on_fail` called if a connection failed to establish.

(experimental: untested)
*/
fio_connect_s *fio_connect(struct fio_connect_args);
#define fio_connect(...) fio_connect((struct fio_connect_args){__VA_ARGS__})

/* *****************************************************************************
Socket / Connection - Read / Write Functions
***************************************************************************** */
/**
 * `fio_read` attempts to read up to count bytes from the socket into the
 * buffer starting at `buffer`.
 *
 * `fio_read`'s return values are wildly different then the native return
 * values and they aim at making far simpler sense.
 *
 * `fio_read` returns the number of bytes read (0 is a valid return value which
 * simply means that no bytes were read from the buffer).
 *
 * On a fatal connection error that leads to the connection being closed (or if
 * the connection is already closed), `fio_read` returns -1.
 *
 * The value 0 is the valid value indicating no data was read.
 *
 * Data might be available in the kernel's buffer while it is not available to
 * be read using `fio_read` (i.e., when using a transport layer, such as TLS).
 */
ssize_t fio_read(intptr_t uuid, void *buf, size_t len);

/** The following structure is used for `fio_write2_fn` function arguments. */
typedef struct {
  /** Select either a file or a buffer for this write operation. */
  union {
    /** The (optional) in-memory data to be sent. */
    const void *buf;
    /** The (optional) file that should be sent (if no buffer provided). */
    const int fd;
  } data;
  /** The length of the buffer / the amount of data to be sent from the file. */
  uintptr_t len;
  /** Starting point offset from the buffer or file descriptor's beginning. */
  uintptr_t offset;
  /**
   * This deallocation callback will be called when the packet is finished
   * with the buffer - unless the data is being copied, in which case it will be
   * called immediately after copying was performed.
   *
   * If no deallocation callback is set, `free` will be used unless copying the
   * data (in which case no callback will be called).
   *
   * Note: socket library functions MUST NEVER be called by a callback, or a
   * deadlock might occur.
   */
  void (*dealloc)(void *buf);
  /** a boolean value indicating if the buffer should be copied (buf only). */
  const uint8_t copy;
  /** a boolean value indicating if a file is used instead of a buffer. */
  const uint8_t is_fd;
  /** a boolean value indicating if the file shouldn't be closed. */
  const uint8_t keep_open;
} fio_write_args_s;

/**
 * `fio_write2_fn` is the actual function behind the macro `fio_write2`.
 */
ssize_t fio_write2_fn(intptr_t uuid, fio_write_args_s options);

/**
 * Schedules data to be written to the socket.
 *
 * `fio_write2` is similar to `fio_write`, except that it allows far more
 * flexibility.
 *
 * On error, -1 will be returned. Otherwise returns 0.
 *
 * See the `fio_write_args_s` structure for details.
 *
 * NOTE: The data is "moved" to the ownership of the socket, not copied. The
 * data will be deallocated according to the `.dealloc` function.
 */
#define fio_write2(uuid, ...)                                                  \
  fio_write2_fn(uuid, (fio_write_args_s){__VA_ARGS__})

/** A noop function for fio_write2 in cases not deallocation is required. */
void FIO_DEALLOC_NOOP(void *arg);

/**
 * `fio_write` copies `legnth` data from the buffer and schedules the data to
 * be sent over the socket.
 *
 * The data isn't necessarily written to the socket. The actual writing to the
 * socket is handled by the IO reactor.
 *
 * On error, -1 will be returned. Otherwise returns 0.
 *
 * Returns the same values as `fio_write2`.
 */
FIO_IFUNC ssize_t fio_write(const intptr_t uuid,
                            const void *buf,
                            const size_t len) {
  return fio_write2(uuid, .data = {.buf = buf}, .len = len, .copy = 1);
}

/**
 * Sends data from a file as if it were a single atomic packet (sends up to
 * length bytes or until EOF is reached).
 *
 * Once the file was sent, the `source_fd` will be closed using `close`.
 *
 * The file will be buffered to the socket chunk by chunk, so that memory
 * consumption is capped. The system's `sendfile` might be used if conditions
 * permit.
 *
 * `offset` dictates the starting point for the data to be sent and length sets
 * the maximum amount of data to be sent.
 *
 * Returns -1 and closes the file on error. Returns 0 on success.
 */
FIO_IFUNC ssize_t fio_sendfile(intptr_t uuid,
                               int source_fd,
                               off_t offset,
                               size_t len) {
  return fio_write2(uuid,
                    .data = {.fd = source_fd},
                    .offset = (uintptr_t)offset,
                    .len = len,
                    .is_fd = 1);
}

/* internal helper */
FIO_SFUNC void fiobj___free_after_send(void *o) { fiobj_str_free((FIOBJ)o); }

/** Writes a FIOBJ object to the `uuid` and frees it once it was sent. */
FIO_IFUNC ssize_t fiobj_send_free(intptr_t uuid, FIOBJ o) {
  if (o == FIOBJ_INVALID)
    return 0;
  if (FIOBJ_TYPE_CLASS(o) != FIOBJ_T_STRING) {
    FIOBJ tmp = fiobj2json(FIOBJ_INVALID, o, 0);
    fiobj_free(o);
    o = tmp;
    if (o == FIOBJ_INVALID)
      return 0;
  }
  fio_str_info_s s = fiobj_str_info(o);
  return fio_write2(uuid,
                    .data = {.buf = (char *)o},
                    .offset = ((uintptr_t)s.buf - (uintptr_t)o),
                    .dealloc = fiobj___free_after_send,
                    .len = s.len);
}
/**
 * Returns the number of `fio_write` calls that are waiting in the socket's
 * queue and haven't been processed.
 */
size_t fio_pending(intptr_t uuid);

/**
 * `fio_flush` attempts to write any remaining data in the internal buffer to
 * the underlying file descriptor and closes the underlying file descriptor once
 * if it's marked for closure (and all the data was sent).
 *
 * Return values: 1 will be returned if data remains in the buffer. 0
 * will be returned if the buffer was fully drained. -1 will be returned on an
 * error or when the connection is closed.
 *
 * errno will be set to EWOULDBLOCK if the socket's lock is busy.
 */
ssize_t fio_flush(intptr_t uuid);

/** Blocks until all the data was flushed from the buffer */
#define fio_flush_strong(uuid)                                                 \
  do {                                                                         \
    errno = 0;                                                                 \
  } while (fio_flush(uuid) > 0 || errno == EWOULDBLOCK)

/**
 * `fio_flush_all` attempts flush all the open connections.
 *
 * Returns the number of sockets still in need to be flushed.
 */
size_t fio_flush_all(void);

/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */

/** Named arguments for the `fio_uuid_env` function. */
typedef struct {
  /** A numerical type filter. Defaults to 0. Negative values are reserved. */
  intptr_t type;
  /** The name for the link. The name and type uniquely identify the object. */
  fio_str_info_s name;
  /** The object being linked to the connection. */
  void *udata;
  /** A callback that will be called once the connection is closed. */
  void (*on_close)(void *data);
  /** Set to true (1) if the name string's life lives as long as the `env` . */
  uint8_t const_name;
} fio_uuid_env_args_s;

/** Named arguments for the `fio_uuid_env_unset` function. */
typedef struct {
  intptr_t type;
  fio_str_info_s name;
} fio_uuid_env_unset_args_s;

/**
 * Links an object to a connection's lifetime / environment.
 *
 * The `on_close` callback will be called once the connection has died.
 *
 * If the `uuid` is invalid, the `on_close` callback will be called immediately.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
void fio_uuid_env_set(intptr_t uuid, fio_uuid_env_args_s);

/**
 * Links an object to a connection's lifetime, calling the `on_close` callback
 * once the connection has died.
 *
 * If the `uuid` is invalid, the `on_close` callback will be called immediately.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
#define fio_uuid_env_set(uuid, ...)                                            \
  fio_uuid_env_set(uuid, (fio_uuid_env_args_s){__VA_ARGS__})

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found, setting `errno`
 * to `EBADF` if the `uuid` was invalid and `ENOTCONN` if the object wasn't
 * found (wasn't linked).
 *
 * NOTICE: a failure likely means that the object's `on_close` callback was
 * already called!
 */
int fio_uuid_env_unset(intptr_t uuid, fio_uuid_env_unset_args_s);

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * On error, all fields in the returned `fio_uuid_env_args_s` struct will be
 * equal to zero (or `NULL`).
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 *
 * NOTICE: a failure likely means that the object's `on_close` callback was
 * already called!
 */
#define fio_uuid_env_unset(uuid, ...)                                          \
  fio_uuid_env_unset(uuid, (fio_uuid_env_unset_args_s){__VA_ARGS__})

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
void fio_uuid_env_remove(intptr_t uuid, fio_uuid_env_unset_args_s);

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
#define fio_uuid_env_remove(uuid, ...)                                         \
  fio_uuid_env_remove(uuid, (fio_uuid_env_unset_args_s){__VA_ARGS__})

/* *****************************************************************************
Socket / Connection Functions
***************************************************************************** */

typedef enum { /* DON'T CHANGE VALUES - they match fio-stl.h */
               FIO_SOCKET_SERVER = 0,
               FIO_SOCKET_CLIENT = 1,
               FIO_SOCKET_NONBLOCK = 2,
               FIO_SOCKET_TCP = 4,
               FIO_SOCKET_UDP = 8,
               FIO_SOCKET_UNIX = 16,
} fio_socket_flags_e;

/**
 * Convert between a facil.io connection's identifier (uuid) and system's fd.
 */
#define fio_uuid2fd(uuid) ((int)((uintptr_t)uuid >> 8))

/**
 * `fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active
 * `uuid`.
 *
 * If the file descriptor was closed directly (not using `fio_close`) or the
 * closure event hadn't been processed, a false positive is possible, in which
 * case the `uuid` might not be valid for long.
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 */
intptr_t fio_fd2uuid(int fd);

/**
 * Creates a TCP/IP, UDP or Unix socket and returns it's unique identifier.
 *
 * For TCP/IP or UDP server sockets (flag sets `FIO_SOCKET_SERVER`), a NULL
 * `address` variable is recommended. Use "localhost" or "127.0.0.1" to limit
 * access to the local machine.
 *
 * For TCP/IP or UDP client sockets (flag sets `FIO_SOCKET_CLIENT`), a remote
 * `address` and `port` combination will be required. `connect` will be called.
 *
 * For TCP/IP and Unix server sockets (flag sets `FIO_SOCKET_SERVER`), `listen`
 * will automatically be called by this function.
 *
 * For Unix server or client sockets, the `port` variable is silently ignored.
 *
 * If the socket is meant to be attached to the facil.io reactor,
 * `FIO_SOCKET_NONBLOCK` MUST be set.
 *
 * The following flags control the type and behavior of the socket:
 *
 * - FIO_SOCKET_SERVER - (default) server mode (may call `listen`).
 * - FIO_SOCKET_CLIENT - client mode (calls `connect`).
 * - FIO_SOCKET_NONBLOCK - sets the socket to non-blocking mode.
 * - FIO_SOCKET_TCP - TCP/IP socket (default).
 * - FIO_SOCKET_UDP - UDP socket.
 * - FIO_SOCKET_UNIX - Unix Socket.
 *
 * Returns -1 on error. Any other value is a valid unique identifier.
 *
 * Note: facil.io uses unique identifiers to protect sockets from collisions.
 *       However these identifiers can be converted to the underlying file
 *       descriptor using the `fio_uuid2fd` macro.
 *
 * Note: UDP server sockets can't use `fio_read` or `fio_write` since they are
 * connectionless.
 */
intptr_t fio_socket(const char *address,
                    const char *port,
                    fio_socket_flags_e flags);

/**
 * `fio_accept` accepts a new socket connection from a server socket - see the
 * server flag on `fio_socket`.
 *
 * Accepted connection are automatically set to non-blocking mode and the
 * O_CLOEXEC flag is set.
 *
 * NOTE: this function does NOT attach the socket to the IO reactor - see
 * `fio_attach`.
 */
intptr_t fio_accept(intptr_t srv_uuid);

/**
 * Sets a socket to non blocking state.
 *
 * This will also set the O_CLOEXEC flag for the file descriptor.
 *
 * This function is called automatically for the new socket, when using
 * `fio_accept`, `fio_connect` or `fio_socket`.
 */
int fio_set_non_block(int fd);

/**
 * Returns 1 if the uuid refers to a valid and open, socket.
 *
 * Returns 0 if not.
 */
int fio_is_valid(intptr_t uuid);

/**
 * Returns 1 if the uuid is invalid or the socket is flagged to be closed.
 *
 * Returns 0 if the socket is valid, open and isn't flagged to be closed.
 */
int fio_is_closed(intptr_t uuid);

/**
 * `fio_close` marks the connection for disconnection once all the data was
 * sent. The actual disconnection will be managed by the `fio_flush` function.
 *
 * `fio_flash` will be automatically scheduled.
 */
void fio_close(intptr_t uuid);

/**
 * `fio_force_close` closes the connection immediately, without adhering to any
 * protocol restrictions and without sending any remaining data in the
 * connection buffer.
 */
void fio_force_close(intptr_t uuid);

/**
 * Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`addr == NULL`).
 *
 * The information is only available when the socket was accepted using
 * `fio_accept` or opened using `fio_connect`.
 */
fio_str_info_s fio_peer_addr(intptr_t uuid);

/**
 * Writes the local machine address (qualified host name) to the buffer.
 *
 * Returns the amount of data written (excluding the NUL byte).
 *
 * `limit` is the maximum number of bytes in the buffer, including the NUL byte.
 *
 * If the returned value == limit - 1, the result might have been truncated.
 *
 * If 0 is returned, an error might have occurred (see `errno`) and the contents
 * of `dest` is undefined.
 */
size_t fio_local_addr(char *dest, size_t limit);

/* *****************************************************************************
Connection Read / Write Hooks, for overriding the system calls
***************************************************************************** */

/**
 * The following struct is used for setting a the read/write hooks that will
 * replace the default system calls to `recv` and `write`.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
typedef struct fio_rw_hook_s {
  /**
   * Implement reading from a file descriptor. Should behave like the file
   * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.
   *
   * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
   * deadlock might occur.
   */
  ssize_t (*read)(intptr_t uuid, void *udata, void *buf, size_t count);
  /**
   * Implement writing to a file descriptor. Should behave like the file system
   * `write` call.
   *
   * If an internal buffer is implemented and it is full, errno should be set to
   * EWOULDBLOCK and the function should return -1.
   *
   * The function is expected to call the `flush` callback (or it's logic)
   * internally. Either `write` OR `flush` are called.
   *
   * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
   * deadlock might occur.
   */
  ssize_t (*write)(intptr_t uuid, void *udata, const void *buf, size_t count);
  /**
   * When implemented, this function will be called to flush any data remaining
   * in the internal buffer.
   *
   * The function should return the number of bytes remaining in the internal
   * buffer (0 is a valid response) or -1 (on error).
   *
   * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
   * deadlock might occur.
   */
  ssize_t (*flush)(intptr_t uuid, void *udata);
  /**
   * The `before_close` callback is called only once before closing the `uuid`
   * and it might not get called at all if an abnormal closure is detected.
   *
   * If the function returns a non-zero value, than closure will be delayed
   * until the `flush` returns 0 (or less). This allows a closure signal to be
   * sent by the read/write hook when such a signal is required.
   *
   * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
   * deadlock might occur.
   * */
  ssize_t (*before_close)(intptr_t uuid, void *udata);
  /**
   * Called to perform cleanup after the socket was closed or a new read/write
   * hook was set using `fio_rw_hook_set`.
   *
   * This callback is always called, even if `fio_rw_hook_set` fails.
   * */
  void (*cleanup)(void *udata);
} fio_rw_hook_s;

/** Sets a socket hook state (a pointer to the struct). */
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata);

/**
 * Replaces an existing read/write hook with another from within a read/write
 * hook callback.
 *
 * Does NOT call any cleanup callbacks.
 *
 * Replaces existing udata. Call with the existing udata to keep it.
 *
 * Returns -1 on error, 0 on success.
 *
 * Note: this function is marked as unsafe, since it should only be called from
 *       within an existing read/write hook callback. Otherwise, data corruption
 *       might occur.
 */
int fio_rw_hook_replace_unsafe(intptr_t uuid,
                               fio_rw_hook_s *rw_hooks,
                               void *udata);

/** The default Read/Write hooks used for system Read/Write (udata == NULL). */
extern const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS;
/* ************************************************************************** */
#ifndef FIO_MAX_SOCK_CAPACITY /* Development inclusion - ignore line */
#include "0003 main api.h"    /* Development inclusion - ignore line */
#endif                        /* Development inclusion - ignore line */
/* ************************************************************************** */

/* *****************************************************************************
 * Pub/Sub / Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 *
 **************************************************************************** */

/* *****************************************************************************
 * Cluster Messages and Pub/Sub
 **************************************************************************** */

/** An opaque subscription type. */
typedef struct subscription_s subscription_s;

/** A pub/sub engine data structure. See details later on. */
typedef struct fio_pubsub_engine_s fio_pubsub_engine_s;

/** The default engine (settable). Initial default is FIO_PUBSUB_CLUSTER. */
extern fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT;
/** Used to publish the message to all clients in the cluster. */
#define FIO_PUBSUB_CLUSTER ((fio_pubsub_engine_s *)1)
/** TODO: Used to publish the message only within the current machine. */
#define FIO_PUBSUB_LOCAL ((fio_pubsub_engine_s *)2)
/** Used to publish the message except within the current process. */
#define FIO_PUBSUB_SIBLINGS ((fio_pubsub_engine_s *)3)
/** Used to publish the message only within the current process. */
#define FIO_PUBSUB_PROCESS ((fio_pubsub_engine_s *)4)
/** Used to publish the message exclusively to the root / master process. */
#define FIO_PUBSUB_ROOT ((fio_pubsub_engine_s *)5)

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct fio_msg_s {
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /**
   * A channel name, allowing for pub/sub patterns.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field might be used for internal data.
   */
  fio_str_info_s channel;
  /**
   * A connection (if any) to which the subscription belongs.
   *
   * The connection uuid 0 marks an un-bound (non-connection related)
   * subscription.
   */
  intptr_t uuid;
  /**
   * The connection's protocol (if any).
   *
   * If the subscription is bound to a connection, the protocol will be locked
   * using a task lock and will be available using this pointer.
   */
  fio_protocol_s *pr;
  /**
   * The actual message.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field is reserved for internal data and must NOT be used.
   **/
  fio_str_info_s msg;
  /** The `udata1` argument associated with the subscription. */
  void *udata1;
  /** The `udata1` argument associated with the subscription. */
  void *udata2;
  /** flag indicating if the message is JSON data or binary/text. */
  uint8_t is_json;
} fio_msg_s;

/**
 * The pattern matching callback used for pattern matching.
 *
 * Returns 1 on a match or 0 if the string does not match the pattern.
 *
 * By default, the value is set to `fio_glob_match` (see facil.io's C STL).
 */
extern uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s pattern,
                                           fio_str_info_s channel);

/**
 * Possible arguments for the fio_subscribe method.
 *
 * NOTICE: passing protocol objects to the `udata` is not safe. This is because
 * protocol objects might be destroyed or invalidated according to both network
 * events (socket closure) and internal changes (i.e., `fio_attach` being
 * called). The preferred way is to add the `uuid` to the `udata` field and call
 * `fio_protocol_try_lock`.
 */
typedef struct {
  /**
   * If `filter` is set, all messages that match the filter's numerical value
   * will be forwarded to the subscription's callback.
   *
   * Subscriptions can either require a match by filter or match by channel.
   * This will match the subscription by filter.
   *
   * Filer channels are cluster bound, are NOT forwarded to engines and can be
   * used for inter process communication (IPC).
   */
  int32_t filter;
  /**
   * If `channel` is set, all messages where `filter == 0` and the channel is an
   * exact match will be forwarded to the subscription's callback.
   *
   * Subscriptions can either require a match by filter or match by channel.
   * This will match the subscription by channel (only messages with no `filter`
   * will be received.
   */
  fio_str_info_s channel;
  /**
   * A connection (if any) to which the subscription should be bound.
   *
   * The connection uuid 0 isn't a valid uuid for subscriptions.
   */
  intptr_t uuid;
  /**
   * The callback will be called for each message forwarded to the subscription.
   */
  void (*on_message)(fio_msg_s *msg);
  /** An optional callback for when a subscription is fully canceled. */
  void (*on_unsubscribe)(void *udata1, void *udata2);
  /** The udata values are ignored and made available to the callback. */
  void *udata1;
  /** The udata values are ignored and made available to the callback. */
  void *udata2;
  /** If set, the string is treated as a pattern*/
  uint8_t is_pattern;
} subscribe_args_s;

/** Publishing and on_message callback arguments. */
typedef struct fio_publish_args_s {
  /** The pub/sub engine that should be used to forward this message. */
  fio_pubsub_engine_s const *engine;
  /** A numeral / internal chunnel. Negative values are reserved. */
  int32_t filter;
  /** The target named channnel. Only published when filter == 0. */
  fio_str_info_s channel;
  /** The message body / content. */
  fio_str_info_s message;
  /** A flag indicating if the message is JSON data or not. */
  uint8_t is_json;
} fio_publish_args_s;

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure. Returns NULL on
 * success when the subscription is bound to a connnection's uuid.
 *
 * Note: since ownership of the subscription is transferred to a connection's
 * UUID when the subscription is linked to a connection, the caller will not
 * receive a link to the subscription object. NULL is returned instead.
 *
 * See `subscribe_args_s` for details.
 */
subscription_s *fio_subscribe(subscribe_args_s args);

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure. Returns NULL on
 * success when the subscription is bound to a connnection's uuid.
 *
 * Note: since ownership of the subscription is transferred to a connection's
 * UUID when the subscription is linked to a connection, the caller will not
 * receive a link to the subscription object.
 *
 * See `subscribe_args_s` for details.
 */
#define fio_subscribe(...) fio_subscribe((subscribe_args_s){__VA_ARGS__})

/**
 * Cancels an existing subscriptions - actual effects might be delayed, for
 * example, if the subscription's callback is running in another thread.
 */
void fio_unsubscribe(subscription_s *subscription);

/**
 * Cancels an existing subscriptions that was bound to a connection's UUID. See
 * `fio_subscribe` and `fio_unsubscribe` for more details.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 */
void fio_unsubscribe_uuid(subscribe_args_s args);

/**
 * Cancels an existing subscriptions that was bound to a connection's UUID. See
 * `fio_subscribe` and `fio_unsubscribe` for more details.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 */
#define fio_unsubscribe_uuid(...)                                              \
  fio_unsubscribe_uuid((subscribe_args_s){__VA_ARGS__})

/**
 * This helper returns a temporary String with the subscription's channel (or a
 * string representing the filter).
 *
 * To keep the string beyond the lifetime of the subscription, copy the string.
 */
fio_str_info_s fio_subscription_channel(subscription_s *subscription);

/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
void fio_publish(fio_publish_args_s args);
/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
#define fio_publish(...) fio_publish((fio_publish_args_s){__VA_ARGS__})
/** for backwards compatibility */
#define pubsub_publish fio_publish

/**
 * Defers the current callback, so it will be called again for the message.
 *
 * After calling this function, the `msg` object must NOT be accessted.
 */
void fio_message_defer(fio_msg_s *msg);

/* *****************************************************************************
 * Message metadata (advance usage API)
 **************************************************************************** */

/**
 * The number of different metadata callbacks that can be attached.
 *
 * Effects performance.
 *
 * The default value should be enough for the following metadata objects:
 * - WebSocket server headers.
 * - WebSocket client (header + masked message copy).
 * - EventSource (SSE) encoded named channel and message.
 */
#ifndef FIO_PUBSUB_METADATA_LIMIT
#define FIO_PUBSUB_METADATA_LIMIT 4
#endif

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id);

/**
 * Pub/Sub Metadata callback type.
 */
typedef void *(*fio_msg_metadata_fn)(fio_str_info_s ch,
                                     fio_str_info_s msg,
                                     uint8_t is_json);

/**
 * It's possible to attach metadata to facil.io named messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.
 *
 * The callback should return a `void *` pointer.
 *
 * To remove a callback, call `fio_message_metadata_remove` with the returned
 * value.
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 *
 * Returns a positive number on success (the metadata ID) or zero (0) on
 * failure.
 */
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *));

/**
 * Removed the metadata callback.
 *
 * Removal might be delayed if live metatdata exists.
 */
void fio_message_metadata_remove(int id);
/* *****************************************************************************
 * Message data duplication and defferal (advance usage API)
 **************************************************************************** */

/**
 * Message content data type (opaque, read only).
 *
 * Subscription messages are all based on a single letter that all subscribers
 * read (if it's "subject" matches their interests).
 *
 * This is type containing the raw letter data, which is network encoded.
 */
typedef struct fio_letter_s fio_letter_s;

/** Letter informatioin, as returned from the `fio_letter_info` function. */
typedef struct {
  fio_str_info_s header;
  fio_str_info_s body;
} fio_letter_info_s;

/**
 * Increases a message's reference count, returning the published "letter".
 *
 * This is performed by reference, the value MUST be considered read-only.
 *
 * Note: subscription messages are all based on a single letter that all
 *       subscribers read (if its "subject" matches their interests).
 *       Letter objects are achitecture agnostice, and may be sent on the net.
 */
fio_letter_s *fio_message_dup(fio_msg_s *msg);

/** Increases a letter's reference count by reference (for read-only access). */
fio_letter_s *fio_letter_dup(fio_letter_s *msg);

/** Decreseas a letter's referencce count and possibly frees its data. */
void fio_letter_free(fio_letter_s *letter);

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *letter);

/** Returns all information about the fio_letter_s object. Read Only! */
fio_letter_info_s fio_letter_info(fio_letter_s *letter);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/**
 * facil.io can be linked with external Pub/Sub services using "engines".
 *
 * Only named messages and subscriptions (where filter == 0) will be forwarded
 * to these "engines".
 *
 * Engines MUST provide the listed function pointers and should be attached
 * using the `fio_pubsub_attach` function.
 *
 * Engines should disconnect / detach, before being destroyed, by using the
 * `fio_pubsub_detach` function.
 *
 * When an engine received a message to publish, it should call the
 * `pubsub_publish` function with the engine to which the message is forwarded.
 * i.e.:
 *
 *       pubsub_publish(
 *           .engine = FIO_PUBSUB_CLUSTER,
 *           .channel = channel_name,
 *           .message = msg_body );
 *
 * Since only the master process guarantes to be subscribed to all the channels
 * in the cluster, it is recommended that engines only use the master process to
 * communicate with a pub/sub backend.
 *
 * IMPORTANT: The `subscribe` and `unsubscribe` callbacks are called from within
 *            an internal lock. They MUST NEVER call pub/sub functions except by
 *            exiting the lock using `fio_defer`.
 */
struct fio_pubsub_engine_s {
  /** Should subscribe channel. Failures are ignored. */
  void (*subscribe)(const fio_pubsub_engine_s *eng,
                    fio_str_info_s channel,
                    uint8_t is_pattern);
  /** Should unsubscribe channel. Failures are ignored. */
  void (*unsubscribe)(const fio_pubsub_engine_s *eng,
                      fio_str_info_s channel,
                      uint8_t is_pattern);
  /** Should publish a message through the engine. Failures are ignored. */
  void (*publish)(const fio_pubsub_engine_s *eng,
                  fio_str_info_s channel,
                  fio_str_info_s msg,
                  uint8_t is_json);
};

/**
 * Attaches an engine, so it's callback can be called by facil.io.
 *
 * The `subscribe` callback will be called for every existing channel.
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_attach(fio_pubsub_engine_s *engine);

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine);

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_reattach(fio_pubsub_engine_s *eng);

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);

/* *****************************************************************************
 * TODO: clusterfy the local network using UDP broadcasting for node discovery.
 **************************************************************************** */

/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *tls);
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0800 threads.h
***************************************************************************** */
#ifndef FIO_MAX_SOCK_CAPACITY /* Development inclusion - ignore line */
#include "0003 main api.h"    /* Development inclusion - ignore line */
#endif                        /* Development inclusion - ignore line */
/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
***************************************************************************** */
/**
 * OVERRIDE THIS to replace the default `fork` implementation.
 *
Behaves like the system's `fork`.... but calls the correct state callbacks.
 *
 * NOTE: When overriding, remember to call the proper state callbacks and call
 * fio_on_fork in the child process before calling any state callback or other
 * functions.
 */
FIO_WEAK int fio_fork(void);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer to a function and a single argument that should be executed
 * within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
FIO_WEAK void *fio_thread_new(void *(*thread_func)(void *), void *arg);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
FIO_WEAK void fio_thread_free(void *p_thr);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
FIO_WEAK int fio_thread_join(void *p_thr);

/**
 * When overrriding fio_fork, call this callback in the child process before
 * calling any other function. The `pid` must be the value of the child's
 * process ID.
 */
void fio_on_fork(void);

/* *****************************************************************************
TLS Support (weak functions, to be overriden by library wrapper)
***************************************************************************** */

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 *
 * If no server name is provided and no private key and public certificate are
 * provided, an empty TLS object will be created, (maybe okay for clients).
 *
 * If a server name is provided, but no certificate is attached, an anonymous
 * (self-signed) certificate will be initialized.
 *
 *      fio_tls_s * tls = fio_tls_new("www.example.com",
 *                                    "public_key.pem",
 *                                    "private_key.pem", NULL );
 */
FIO_WEAK fio_tls_s *fio_tls_new(const char *server_name,
                                const char *public_cert_file,
                                const char *private_key_file,
                                const char *pk_password);

/**
 * Increase the reference count for the TLS object.
 *
 * Decrease with `fio_tls_destroy`.
 */
FIO_WEAK void fio_tls_dup(fio_tls_s *tls);

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
FIO_WEAK void fio_tls_free(fio_tls_s *tls);

/**
 * Adds a certificate to the SSL/TLS context / settings object.
 *
 * SNI support is implementation / library specific, but SHOULD be provided.
 *
 * If a server name is provided, but no certificate is attached, an anonymous
 * (self-signed) certificate will be initialized.
 *
 *      fio_tls_cert_add(tls, "www.example.com",
 *                            "public_key.pem",
 *                            "private_key.pem", NULL );
 */
FIO_WEAK void fio_tls_cert_add(fio_tls_s *,
                               const char *server_name,
                               const char *public_cert_file,
                               const char *private_key_file,
                               const char *pk_password);

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 *
 * The `on_selected` callback should accept the `uuid`, the user data pointer
 * passed to either `fio_tls_accept` or `fio_tls_connect` (here:
 * `udata_connetcion`) and the user data pointer passed to the
 * `fio_tls_alpn_add` function (`udata_tls`).
 *
 * The `on_cleanup` callback will be called when the TLS object is destroyed (or
 * `fio_tls_alpn_add` is called again with the same protocol name). The
 * `udata_tls` argument will be passed along, as is, to the callback (if set).
 *
 * Except for the `tls` and `protocol_name` arguments, all arguments can be
 * NULL.
 */
FIO_WEAK void fio_tls_alpn_add(fio_tls_s *tls,
                               const char *protocol_name,
                               void (*on_selected)(intptr_t uuid,
                                                   void *udata_connection,
                                                   void *udata_tls),
                               void *udata_tls,
                               void (*on_cleanup)(void *udata_tls));

/**
 * Adds a certificate to the "trust" list, which automatically adds a peer
 * verification requirement.
 *
 * Note, when the fio_tls_s object is used for server connections, this will
 * limit connections to clients that connect using a trusted certificate.
 *
 *      fio_tls_trust(tls, "google-ca.pem" );
 */
FIO_WEAK void fio_tls_trust(fio_tls_s *, const char *public_cert_file);

/* *****************************************************************************
TLS Support - non-user functions (called by the facil.io library)
***************************************************************************** */

/**
 * Returns the number of registered ALPN protocol names.
 *
 * This could be used when deciding if protocol selection should be delegated to
 * the ALPN mechanism, or whether a protocol should be immediately assigned.
 *
 * If no ALPN protocols are registered, zero (0) is returned.
 */
FIO_WEAK uintptr_t fio_tls_alpn_count(fio_tls_s *tls);

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * the result of `fio_accept`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
FIO_WEAK void fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata);

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * one received by a `fio_connect` specified callback `on_connect`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
FIO_WEAK void fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata);

/* *****************************************************************************
Mark weakness
***************************************************************************** */

#pragma weak fio_fork
#pragma weak fio_thread_new
#pragma weak fio_thread_free
#pragma weak fio_thread_join

#pragma weak fio_tls_new
#pragma weak fio_tls_dup
#pragma weak fio_tls_free
#pragma weak fio_tls_cert_add
#pragma weak fio_tls_alpn_add
#pragma weak fio_tls_trust
#pragma weak fio_tls_alpn_count
#pragma weak fio_tls_accept
#pragma weak fio_tls_connect
/* *****************************************************************************







                              Hash Functions and Friends







***************************************************************************** */

/* *****************************************************************************
SipHash
***************************************************************************** */

/**
 * A SipHash variation (2-4).
 */
uint64_t
fio_siphash24(const void *buf, size_t len, uint64_t key1, uint64_t key2);

/**
 * A SipHash 1-3 variation.
 */
uint64_t
fio_siphash13(const void *buf, size_t len, uint64_t key1, uint64_t key2);

/**
 * The Hashing function used by dynamic facil.io objects.
 *
 * Currently implemented using SipHash 1-3.
 */
#define fio_siphash(buf, len, k1, k2) fio_siphash13((buf), (len), (k1), (k2))

/* *****************************************************************************
SHA-1
***************************************************************************** */

/**
SHA-1 hashing container - you should ignore the contents of this struct.

The `sha1_s` type will contain all the sha1 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    fio_sha1_s sha1;
    fio_sha1_init(&sha1);
    fio_sha1_write(&sha1,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = fio_sha1_result(&sha1);
*/
typedef struct {
  uint64_t len;
  uint8_t buf[64];
  union {
    uint32_t i[5];
    unsigned char str[21];
  } digest;
} fio_sha1_s;

/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
fio_sha1_s fio_sha1_init(void);
/**
Writes data to the sha1 buffer.
*/
void fio_sha1_write(fio_sha1_s *s, const void *data, size_t len);
/**
Finalizes the SHA1 hash, returning the Hashed data.

`fio_sha1_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *fio_sha1_result(fio_sha1_s *s);

/**
An SHA1 helper function that performs initialiation, writing and finalizing.
*/
FIO_IFUNC char *fio_sha1(fio_sha1_s *s, const void *data, size_t len) {
  *s = fio_sha1_init();
  fio_sha1_write(s, data, len);
  return fio_sha1_result(s);
}

/* *****************************************************************************
SHA-2
***************************************************************************** */

/**
SHA-2 function variants.

This enum states the different SHA-2 function variants. placing SHA_512 at the
beginning is meant to set this variant as the default (in case a 0 is passed).
*/
typedef enum {
  SHA_512 = 1,
  SHA_512_256 = 3,
  SHA_512_224 = 5,
  SHA_384 = 7,
  SHA_256 = 2,
  SHA_224 = 4,
} fio_sha2_variant_e;

/**
SHA-2 hashing container - you should ignore the contents of this struct.

The `sha2_s` type will contain all the SHA-2 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    fio_sha2_s sha2;
    fio_sha2_init(&sha2, SHA_512);
    fio_sha2_write(&sha2,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = fio_sha2_result(&sha2);

*/
typedef struct {
  /* notice: we're counting bits, not bytes. max length: 2^128 bits */
  union {
    uint8_t bytes[16];
    uint8_t matrix[4][4];
    uint32_t words_small[4];
    uint64_t words[2];
#if defined(__SIZEOF_INT128__)
    __uint128_t i;
#endif
  } len;
  uint8_t buf[128];
  union {
    uint32_t i32[16];
    uint64_t i64[8];
    uint8_t str[65]; /* added 64+1 for the NULL byte.*/
  } digest;
  fio_sha2_variant_e type;
} fio_sha2_s;

/**
Initialize/reset the SHA-2 object.

SHA-2 is actually a family of functions with different variants. When
initializing the SHA-2 container, you must select the variant you intend to
apply. The following are valid options (see the sha2_variant enum):

- SHA_512 (== 0)
- SHA_384
- SHA_512_224
- SHA_512_256
- SHA_256
- SHA_224

*/
fio_sha2_s fio_sha2_init(fio_sha2_variant_e variant);
/**
Writes data to the SHA-2 buffer.
*/
void fio_sha2_write(fio_sha2_s *s, const void *data, size_t len);
/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *fio_sha2_result(fio_sha2_s *s);

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 512 variant.
*/
FIO_IFUNC char *fio_sha2_512(fio_sha2_s *s, const void *data, size_t len) {
  *s = fio_sha2_init(SHA_512);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 256 variant.
*/
FIO_IFUNC char *fio_sha2_256(fio_sha2_s *s, const void *data, size_t len) {
  *s = fio_sha2_init(SHA_256);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 384 variant.
*/
FIO_IFUNC char *fio_sha2_384(fio_sha2_s *s, const void *data, size_t len) {
  *s = fio_sha2_init(SHA_384);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/* *****************************************************************************
Testing
***************************************************************************** */
#ifdef TEST
void fio_test(void);
#else
#define fio_test()
#endif

/* *****************************************************************************
C++ extern end
***************************************************************************** */
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* H_FACIL_IO_H */
