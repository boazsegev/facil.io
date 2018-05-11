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
#define FACIL_VERSION_MAJOR 0
#define FACIL_VERSION_MINOR 6
#define FACIL_VERSION_PATCH 4

#ifndef FACIL_PRINT_STATE
/**
 * When FACIL_PRINT_STATE is set to 1, facil.io will print out common messages
 * regarding the server state (start / finish / listen messages).
 */
#define FACIL_PRINT_STATE 1
#endif

#ifndef FACIL_CPU_CORES_LIMIT
/**
 * If facil.io detects more CPU cores than the number of cores stated in the
 * FACIL_CPU_CORES_LIMIT, it will assume an error and cap the number of cores
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
 * The does NOT effect manually set (non-zero) worker/thread values.
 */
#define FACIL_CPU_CORES_LIMIT 8
#endif

#ifndef FIO_DEDICATED_SYSTEM
/**
 * If FIO_DEDICATED_SYSTEM is false, threads will be used (mostly) for
 * non-prallel concurrency (protection against slow user code / high load) and
 * processes will be used for parallelism. Otherwise, both threads and processes
 * will be used for prallel concurrency (at the expense of increased polling).
 *
 * If FIO_DEDICATED_SYSTEM is true, facil.io assumes that the whole system is at
 * it's service and that no other process is using the CPU cores.
 *
 * Accordingly, facil.io will poll the IO more often in an attempt to activate
 * the threads and utilize all the cores whenever events occur.
 *
 * My tests show that the non-polling approach is faster, but it may be system
 * specific.
 */
#define FIO_DEDICATED_SYSTEM 0
#endif

#ifndef FACIL_DISABLE_HOT_RESTART
/**
 * Disables the hot restart reaction to the SIGUSR1 signal
 *
 * The hot restart will attempt to shut down all workers, and spawn new workers,
 * cleaning up any data cached by any of the workers.
 *
 * It's quite useless unless the workers are running their own VMs which are
 * initialized in the listening socket's `on_start` callback.
 */
#define FACIL_DISABLE_HOT_RESTART 0
#endif

/* *****************************************************************************
Required facil libraries
***************************************************************************** */
#include "defer.h"
#include "fiobj.h"
#include "sock.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
Core object types
***************************************************************************** */

typedef struct FacilIOProtocol protocol_s;
/**************************************************************************/ /**
* The Protocol

The Protocol struct defines the callbacks used for the connection and sets it's
behaviour. The Protocol struct is part of facil.io's core design.

For concurrency reasons, a protocol instance SHOULD be unique to each
connections. Different connections shouldn't share a single protocol object
(callbacks and data can obviously be shared).

All the callbacks recieve a unique connection ID (a localized UUID) that can be
converted to the original file descriptor when in need.

This allows facil.io to prevent old connection handles from sending data
to new connections after a file descriptor is "recycled" by the OS.
*/
struct FacilIOProtocol {
  /**
   * A string to identify the protocol's service (i.e. "http").
   *
   * The string should be a global constant, only a pointer comparison will be
   * used (not `strcmp`).
   */
  const char *service;
  /** Called when a data is available, but will not run concurrently */
  void (*on_data)(intptr_t uuid, protocol_s *protocol);
  /** called when the socket is ready to be written to. */
  void (*on_ready)(intptr_t uuid, protocol_s *protocol);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * The callback runs within a {FIO_PR_LOCK_TASK} lock, so it will never run
   * concurrently wil {on_data} or other connection specific tasks.
   */
  void (*on_shutdown)(intptr_t uuid, protocol_s *protocol);
  /** Called when the connection was closed, but will not run concurrently */
  void (*on_close)(intptr_t uuid, protocol_s *protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t uuid, protocol_s *protocol);
  /** private metadata used by facil. */
  size_t rsv;
};

/**************************************************************************/ /**
* Listening to Incoming Connections

Listenning to incoming connections is pretty straight forward.

After a new connection is accepted, the `on_open` callback is called. `on_open`
should allocate the new connection's protocol and retuen it's address.

The protocol's `on_close` callback is expected to handle the cleanup.

These settings will be used to setup listening sockets.

i.e.

```c
// A callback to be called whenever data is available on the socket
static void echo_on_data(intptr_t uuid,
                         protocol_s *prt
                         ) {
  (void)prt; // we can ignore the unused argument
  // echo buffer
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  // Read to the buffer, starting after the "Echo: "
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    // Write back the message
    sock_write(uuid, buffer, len + 6);
    // Handle goodbye
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

// A callback called whenever a timeout is reach
static void echo_ping(intptr_t uuid, protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  sock_write(uuid, "Server: Are you there?\n", 23);
}

// A callback called if the server is shutting down...
// ... while the connection is still open
static void echo_on_shutdown(intptr_t uuid, protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}

// A callback called for new connections
static protocol_s *echo_on_open(intptr_t uuid, void *udata) {
  (void)udata; // ignore this
  // Protocol objects MUST always be dynamically allocated.
  protocol_s *echo_proto = malloc(sizeof(*echo_proto));
  *echo_proto = (protocol_s){
      .service = "echo",
      .on_data = echo_on_data,
      .on_shutdown = echo_on_shutdown,
      .on_close = (void (*)(protocol_s *))free, // simply free when done
      .ping = echo_ping};

  sock_write(uuid, "Echo Service: Welcome\n", 22);
  facil_set_timeout(uuid, 5);
  return echo_proto;
}

int main() {
  // Setup a listening socket
  if (facil_listen(.port = "8888", .on_open = echo_on_open)) {
      perror("No listening socket available on port 8888");
      exit(-1);
  }
  // Run the server and hang until a stop signal is received.
  facil_run(.threads = 4, .processes = 1);
}

```
*/
struct facil_listen_args {
  /**
   * Called whenever a new connection is accepted.
   *
   * Should either call `facil_attach` or close the connection.
   */
  void (*on_open)(intptr_t fduuid, void *udata);
  /** The network service / port. Defaults to "3000". */
  const char *port;
  /** The socket binding address. Defaults to the recommended NULL. */
  const char *address;
  /** Opaque user data. */
  void *udata;
  /**
   * Called when the server starts (or a worker process is respawned), allowing
   * for further initialization, such as timed event scheduling or VM
   * initialization.
   *
   * This will be called seperately for every worker process whenever it is
   * spawned.
   */
  void (*on_start)(intptr_t uuid, void *udata);
  /**
   * Called when the server is done, usable for cleanup.
   *
   * This will be called seperately for every process. */
  void (*on_finish)(intptr_t uuid, void *udata);
};

/** Schedule a network service on a listening socket. */
int facil_listen(struct facil_listen_args args);

/**
 * Schedule a network service on a listening socket.
 *
 * See the `struct facil_listen_args` details for any possible named arguments.
 */
#define facil_listen(...) facil_listen((struct facil_listen_args){__VA_ARGS__})

/* *****************************************************************************
Connecting to remote servers as a client
***************************************************************************** */

/**
Named arguments for the `server_connect` function, that allows non-blocking
connections to be established.
*/
struct facil_connect_args {
  /** The address of the server we are connecting to. */
  char *address;
  /** The port on the server we are connecting to. */
  char *port;
  /**
   * The `on_connect` callback should return a pointer to a protocol object
   * that will handle any connection related events.
   *
   * Should either call `facil_attach` or close the connection.
   */
  void (*on_connect)(intptr_t uuid, void *udata);
  /**
   * The `on_fail` is called when a socket fails to connect. The old sock UUID
   * is passed along.
   */
  void (*on_fail)(intptr_t uuid, void *udata);
  /** Opaque user data. */
  void *udata;
  /** A non-system timeout after which connection is assumed to have failed. */
  uint8_t timeout;
};

/**
Creates a client connection (in addition or instead of the server).

See the `struct facil_listen_args` details for any possible named arguments.

* `.address` should be the address of the server.

* `.port` the server's port.

* `.udata`opaque user data.

* `.on_connect` called once a connection was established.

    Should return a pointer to a `protocol_s` object, to handle connection
    callbacks.

* `.on_fail` called if a connection failed to establish.

(experimental: untested)
*/
intptr_t facil_connect(struct facil_connect_args);
#define facil_connect(...)                                                     \
  facil_connect((struct facil_connect_args){__VA_ARGS__})

/* *****************************************************************************
Core API
***************************************************************************** */

struct facil_run_args {
  /**
   * The number of threads to run in the thread pool. Has "smart" defaults.
   *
   *
   * A positive value will indicate a set number of threads (or processes).
   *
   * Zeros and negative values are fun and include an interesting shorthand:
   *
   * * Negative values indicate a fraction of the number of CPU cores. i.e.
   *   -2 will normally indicate "half" (1/2) the number of cores.
   *
   * * If the other option (i.e. `.processes` when setting `.threads`) is zero,
   *   it will be automatically updated to reflect the option's absolute value.
   *   i.e.:
   *   if .threads == -2 and .processes == 0,
   *   than facil.io will run 2 processes with (cores/2) threads per process.
   */
  int16_t threads;
  /** The number of processes to run (including this one). See `threads`. */
  int16_t processes;
  /** called if the event loop in cycled with no pending events. */
  void (*on_idle)(void);
  /** called when the server is done, to clean up any leftovers. */
  void (*on_finish)(void);
};

/**
 * Starts the facil.io event loop. This function will return after facil.io is
 * done (after shutdown).
 *
 * See the `struct facil_run_args` details for any possible named arguments.
 */
void facil_run(struct facil_run_args args);
#define facil_run(...) facil_run((struct facil_run_args){__VA_ARGS__})

/**
 * Returns the number of expected threads / processes to be used by facil.io.
 *
 * The pointers should start with valid values that match the expected threads /
 * processes values passed to `facil_run`.
 *
 * The data in the pointers will be overwritten with the result.
 */
void facil_expected_concurrency(int16_t *threads, int16_t *processes);

/**
 * returns true (1) if the facil.io engine is already running.
 */
int facil_is_running(void);

/**
OVERRIDE THIS to replace the default `fork` implementation or to inject hooks
into the forking function.

Behaves like the system's `fork`.
*/
int facil_fork(void);

/**
 * Attaches (or updates) a protocol object to a socket UUID.
 *
 * The new protocol object can be NULL, which will detach ("hijack"), the
 * socket.
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * Returns -1 on error and 0 on success.
 *
 * On error, the new protocol's `on_close` callback will be called.
 */
int facil_attach(intptr_t uuid, protocol_s *protocol);

/**
 * Attaches (or updates) a LOCKED protocol object to a socket UUID.
 *
 * The protocol will be attached in the FIO_PR_LOCK_TASK state, requiring a
 * furthur call to `facil_protocol_unlock`.
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * Returns -1 on error and 0 on success.
 *
 * On error, the new protocol's `on_close` callback will be called.
 */
int facil_attach_locked(intptr_t uuid, protocol_s *protocol);

/** Sets a timeout for a specific connection (if active). */
void facil_set_timeout(intptr_t uuid, uint8_t timeout);

/** Gets a timeout for a specific connection. Returns 0 if none. */
uint8_t facil_get_timeout(intptr_t uuid);

enum facil_io_event {
  FIO_EVENT_ON_DATA,
  FIO_EVENT_ON_READY,
  FIO_EVENT_ON_TIMEOUT,
};
/** Schedules an IO event, even id it did not occur. */
void facil_force_event(intptr_t uuid, enum facil_io_event);

/**
 * Temporarily prevents `on_data` events from firing.
 *
 * The `on_data` event will be automatically rescheduled when (if) the socket's
 * outgoing buffer fills up or when `facil_force_event` is called with
 * `FIO_EVENT_ON_DATA`.
 *
 * Note: the function will work as expected when called within the protocol's
 * `on_data` callback and the `uuid` refers to a valid socket. Otherwise the
 * function might quitely fail.
 */
void facil_quite(intptr_t uuid);

/* *****************************************************************************
Helper API
***************************************************************************** */

/**
 * Initializes zombie reaping for the process. Call before `facil_run` to enable
 * global zombie reaping.
 */
void facil_reap_children(void);

/**
 * Returns the last time the server reviewed any pending IO events.
 */
struct timespec facil_last_tick(void);

/** Counts all the connections of a specific type `service`. */
size_t facil_count(void *service);

/**
 * Creates a system timer (at the cost of 1 file descriptor).
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error or the new file descriptor on succeess.
 *
 * The `on_finish` handler is always called (even on error).
 */
int facil_run_every(size_t milliseconds, size_t repetitions,
                    void (*task)(void *), void *arg, void (*on_finish)(void *));

/**
 * This is used to lock the protocol againste concurrency collisions and
 * concurent memory deallocation.
 *
 * However, there are three levels of protection that allow non-coliding tasks
 * to protect the protocol object from being deallocated while in use:
 *
 * * `FIO_PR_LOCK_TASK` - a task lock locks might change data owned by the
 *    protocol object. This task is used for tasks such as `on_data` and
 *    (usually) `facil_defer`.
 *
 * * `FIO_PR_LOCK_WRITE` - a lock that promises only to use static data (data
 *    that tasks never changes) in order to write to the underlying socket.
 *    This lock is used for tasks such as `on_ready` and `ping`
 *
 * * `FIO_PR_LOCK_STATE` - a lock that promises only to retrive static data
 *    (data that tasks never changes), performing no actions. This usually
 *    isn't used for client side code (used internally by facil) and is only
 *     meant for very short locks.
 */
enum facil_protocol_lock_e {
  FIO_PR_LOCK_TASK = 0,
  FIO_PR_LOCK_WRITE = 1,
  FIO_PR_LOCK_STATE = 2,
};

/** Named arguments for the `facil_defer` function. */
struct facil_defer_args_s {
  /** The socket (UUID) that will perform the task. This is required.*/
  intptr_t uuid;
  /** The type of task to be performed. Defaults to `FIO_PR_LOCK_TASK` but could
   * also be seto to `FIO_PR_LOCK_WRITE`. */
  enum facil_protocol_lock_e type;
  /** The task (function) to be performed. This is required. */
  void (*task)(intptr_t uuid, protocol_s *, void *arg);
  /** An opaque user data that will be passed along to the task. */
  void *arg;
  /** A fallback task, in case the connection was lost. Good for cleanup. */
  void (*fallback)(intptr_t uuid, void *arg);
};
/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error ocuurs or the connection is closed before the task can run, the
 * `fallback` task wil be called instead, allowing for resource cleanup.
 */
void facil_defer(struct facil_defer_args_s args);
#define facil_defer(...) facil_defer((struct facil_defer_args_s){__VA_ARGS__})

/** Named arguments for the `facil_defer` function. */
struct facil_each_args_s {
  /** The socket (UUID) that originates the task or -1 if none (0 is a valid
   * UUID). This socket will be EXCLUDED from performing the task.*/
  intptr_t origin;
  /** The target type of protocol that should perform the task. This is
   * required. */
  const void *service;
  /** The type of task to be performed. Defaults to `FIO_PR_LOCK_TASK` but could
   * also be seto to `FIO_PR_LOCK_WRITE`. */
  enum facil_protocol_lock_e task_type;
  /** The task (function) to be performed. This is required. */
  void (*task)(intptr_t uuid, protocol_s *, void *arg);
  /** An opaque user data that will be passed along to the task. */
  void *arg;
  /** An on_complete callback. Good for cleanup. */
  void (*on_complete)(intptr_t uuid, void *arg);
};

/**
 * Schedules a protected connection task for each `service` connection.
 * The tasks will run within each of the connection's locks.
 *
 * Once all the tasks were performed, the `on_complete` callback will be called.
 *
 * Returns -1 on error. `on_complete` is always called (even on error).
 */
int facil_each(struct facil_each_args_s args);
#define facil_each(...) facil_each((struct facil_each_args_s){__VA_ARGS__})

/* *****************************************************************************
 * Cluster specific API - local cluster messaging.
 *
 * Facil supports message process clustering, so that a multi-process
 * application can easily send and receive messages across process boundries.
 **************************************************************************** */

/** returns facil.io's parent (root) process pid. */
pid_t facil_parent_pid(void);

/**
Sets a callback / handler for a message of type `msg_type`.

Callbacks are invoked using an O(n) matching, where `n` is the number of
registered callbacks.

The `msg_type` value can be any positive number up to 2^31-1 (2,147,483,647).
All values less than 0 are reserved for internal use.
*/
void facil_cluster_set_handler(int32_t filter,
                               void (*on_message)(int32_t filter, FIOBJ ch,
                                                  FIOBJ msg));
/** Sends a message of type `msg_type` to the **other** cluster processes.

`msg_type` should match a message type used when calling
`facil_cluster_set_handler` to set the appropriate callback.

Unknown `msg_type` values are silently ignored.

The `msg_type` value can be any positive number less than 1,073,741,824. All
negative values and values above 1,073,741,824 are reserved for internal use.

Callbacks are invoked using an O(n) matching, where `n` is the number of
registered callbacks.
*/
int facil_cluster_send(int32_t filter, FIOBJ ch, FIOBJ msg);

/* *****************************************************************************
Lower Level API - for special circumstances, use with care under .
***************************************************************************** */

/**
 * This function allows out-of-task access to a connection's `protocol_s` object
 * by attempting to lock it.
 *
 * CAREFUL: mostly, the protocol object will be locked and a pointer will be
 * sent to the connection event's callback. However, if you need access to the
 * protocol object from outside a running connection task, you might need to
 * lock the protocol to prevent it from being closed in the background.
 *
 * facil.io uses three different locks:
 *
 * * FIO_PR_LOCK_TASK locks the protocol from normal tasks (i.e. `on_data`,
 * `facil_defer`, `facil_every`).
 *
 * * FIO_PR_LOCK_WRITE locks the protocol for high priority `sock_write`
 * oriented tasks (i.e. `ping`, `on_ready`).
 *
 * * FIO_PR_LOCK_STATE locks the protocol for quick operations that need to copy
 * data from the protoccol's data stracture.
 *
 * IMPORTANT: Remember to call `facil_protocol_unlock` using the same lock type.
 *
 * Returns NULL on error (lock busy == EWOULDBLOCK, connection invalid == EBADF)
 * and a pointer to a protocol object on success.
 *
 * On error, consider calling `facil_defer` or `defer` instead of busy waiting.
 * Busy waiting SHOULD be avoided whenever possible.
 */
protocol_s *facil_protocol_try_lock(intptr_t uuid, enum facil_protocol_lock_e);
/** Don't unlock what you don't own... see `facil_protocol_try_lock` for
 * details. */
void facil_protocol_unlock(protocol_s *pr, enum facil_protocol_lock_e);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* H_FACIL_H */
