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
#define FACIL_VERSION_MINOR 7
#define FACIL_VERSION_PATCH 0

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
 * This does NOT effect manually set (non-zero) worker/thread values.
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

typedef struct protocol_s protocol_s;
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
struct protocol_s {
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
   *
   * The `on_shutdown` callback should return 0 to close the socket or a number
   * between 1..254 to delay the socket closure by that amount of time.
   *
   * Once the socket wass marked for closure, facil.io will allow 8 seconds for
   * all the data to be sent before forcfully closing the socket (regardless of
   * state).
   *
   * If the `on_shutdown` returns 255, the socket is ignored and it will be
   * abruptly terminated when all other sockets have finished their graceful
   * shutdown procedure.
   */
  uint8_t (*on_shutdown)(intptr_t uuid, protocol_s *protocol);
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

/**
 * Schedule a network service on a listening socket.
 *
 * Returns the listening socket or -1 (on error).
 */
intptr_t facil_listen(struct facil_listen_args args);

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
  union {
    /** The number of worker processes to run. See `threads`. */
    int16_t workers;
    /** alias to `workers`. See `threads`. */
    int16_t processes;
  };
};

/**
 * Starts the facil.io event loop. This function will return after facil.io is
 * done (after shutdown).
 *
 * See the `struct facil_run_args` details for any possible named arguments.
 *
 * This method blocks the current thread until the server is stopped (when a
 * SIGINT/SIGTERM is received).
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
 * Returns the number of worker processes if facil.io is running.
 *
 * (1 is returned when in single process mode, otherwise the number of workers)
 */
int16_t facil_is_running(void);

/**
OVERRIDE THIS to replace the default `fork` implementation or to inject hooks
into the forking function.

Behaves like the system's `fork`.
*/
int facil_fork(void);

/** returns facil.io's parent (root) process pid. */
pid_t facil_parent_pid(void);

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

/** Sets a timeout for a specific connection (only when running and valid). */
void facil_set_timeout(intptr_t uuid, uint8_t timeout);

/** Gets a timeout for a specific connection. Returns 0 if none. */
uint8_t facil_get_timeout(intptr_t uuid);

enum facil_io_event {
  FIO_EVENT_ON_DATA,
  FIO_EVENT_ON_READY,
  FIO_EVENT_ON_TIMEOUT
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
Core Callbacks for fork, start up, idle and clean up events

To call a function after `fork` has complete, simply add it to the normal queue
using `defer`.
***************************************************************************** */

typedef enum {
  /* Called once right after facil_run is called. */
  FIO_CALL_PRE_START,
  /* Called before each time the facil.io master process forks to a worker. */
  FIO_CALL_BEFORE_FORK,
  /* Called after each time facil.io forks (both in parent and workers). */
  FIO_CALL_AFTER_FORK,
  /* Called by a worker process right after forking. */
  FIO_CALL_IN_CHILD,
  /* Called when starting up the server. */
  FIO_CALL_ON_START,
  /* Called when facil.io enters idling mode. */
  FIO_CALL_ON_IDLE,
  /* Called before starting the shutdown sequence. */
  FIO_CALL_ON_SHUTDOWN,
  /* Called just before finishing up (both on chlid and parent processes). */
  FIO_CALL_ON_FINISH,
  /* Called by each worker the moment it detects the master process crashed. */
  FIO_CALL_ON_PARENT_CRUSH,
  /* Called by the parent (master) after a worker process crashed. */
  FIO_CALL_ON_CHILD_CRUSH,
  /* An alternative to the system's at_exit. */
  FIO_CALL_AT_EXIT
} callback_type_e;

/** Adds a callback to the list of callbacks to be called for the event. */
void facil_core_callback_add(callback_type_e, void (*func)(void *), void *arg);

/** Removes a callback from the list of callbacks to be called for the event. */
int facil_core_callback_remove(callback_type_e, void (*func)(void *),
                               void *arg);

/** Forces all the existing callbacks to run, as if the event occured. */
void facil_core_callback_force(callback_type_e);

/** Clears all the existing callbacks for the event. */
void facil_core_callback_clear(callback_type_e);

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
  FIO_PR_LOCK_STATE = 2
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
Lower Level API - for special circumstances, use with care under .
***************************************************************************** */

/**
 * This function allows out-of-task access to a connection's `protocol_s` object
 * by attempting to aquire a locked pointer.
 *
 * CAREFUL: mostly, the protocol object will be locked and a pointer will be
 * sent to the connection event's callback. However, if you need access to the
 * protocol object from outside a running connection task, you might need to
 * lock the protocol to prevent it from being closed / freed in the background.
 *
 * facil.io uses three different locks:
 *
 * * FIO_PR_LOCK_TASK locks the protocol for normal tasks (i.e. `on_data`,
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

/* *****************************************************************************
 * Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 *
 * The API is implemented in the facil_cluster.c file.
 *
 **************************************************************************** */

/* *****************************************************************************
 * Cluster Messages and Pub/Sub
 **************************************************************************** */

/** An opaque subscription type. */
typedef struct subscription_s subscription_s;

/** A pub/sub engine data structure. See details later on. */
typedef struct pubsub_engine_s pubsub_engine_s;

/** The default engine (settable). Initial default is FACIL_PUBSUB_CLUSTER. */
extern pubsub_engine_s *FACIL_PUBSUB_DEFAULT;
/** Used to publish the message to all clients in the cluster. */
#define FACIL_PUBSUB_CLUSTER ((pubsub_engine_s *)1)
/** Used to publish the message only within the current process. */
#define FACIL_PUBSUB_PROCESS ((pubsub_engine_s *)2)
/** Used to publish the message except within the current process. */
#define FACIL_PUBSUB_SIBLINGS ((pubsub_engine_s *)3)

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct facil_msg_s {
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** A channel name, allowing for pub/sub patterns. */
  FIOBJ channel;
  /** The actual message. */
  FIOBJ msg;
  /** The `udata1` argument associated with the subscription. */
  void *udata1;
  /** The `udata1` argument associated with the subscription. */
  void *udata2;
} facil_msg_s;

/**
 * Pattern matching callback type - should return 0 unless channel matches
 * pattern.
 */
typedef int (*facil_match_fn)(FIOBJ pattern, FIOBJ channel);

extern facil_match_fn FACIL_MATCH_GLOB;

/** possible arguments for the facil_subscribe method. */
typedef struct {
  /**
   * If `filter` is set, all messages that match the filter's numerical value
   * will be forwarded to the subscription's callback.
   *
   * Subscriptions can either require a match by filter or match by channel.
   * This will match the subscription by filter.
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
  FIOBJ channel;
  /**
   * The the `match` function allows pattern matching for channel names.
   *
   * When using a match function, the channel name is considered to be a pattern
   * and each pub/sub message (a message where filter == 0) will be tested
   * against that pattern.
   *
   * Using pattern subscriptions extensively could become a performance concern,
   * since channel names are tested against each distinct pattern rather than
   * leveraging a hashmap for possible name matching.
   */
  facil_match_fn match;
  /**
   * The callback will be called for each message forwarded to the subscription.
   */
  void (*on_message)(facil_msg_s *msg);
  /** An optional callback for when a subscription is fully canceled. */
  void (*on_unsubscribe)(void *udata1, void *udata2);
  /** The udata values are ignored and made available to the callback. */
  void *udata1;
  /** The udata values are ignored and made available to the callback. */
  void *udata2;
} subscribe_args_s;

/** Publishing and on_message callback arguments. */
typedef struct facil_publish_args_s {
  /** The pub/sub engine that should be used to farward this message. */
  pubsub_engine_s const *engine;
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** The pub/sub target channnel. */
  FIOBJ channel;
  /** The pub/sub message. */
  FIOBJ message;
} facil_publish_args_s;

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure.
 *
 * See `subscribe_args_s` for details.
 */
subscription_s *facil_subscribe(subscribe_args_s args);
/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure.
 *
 * See `subscribe_args_s` for details.
 */
#define facil_subscribe(...) facil_subscribe((subscribe_args_s){__VA_ARGS__})

/**
 * Subscribes to a channel (enforces filter == 0).
 *
 * Returns a subscription pointer on success or NULL on failure.
 *
 * See `subscribe_args_s` for details.
 */
subscription_s *facil_subscribe_pubsub(subscribe_args_s args);
/**
 * Subscribes to a channel (enforces filter == 0).
 *
 * Returns a subscription pointer on success or NULL on failure.
 *
 * See `subscribe_args_s` for details.
 */
#define facil_subscribe_pubsub(...)                                            \
  facil_subscribe_pubsub((subscribe_args_s){__VA_ARGS__})

/**
 * Cancels an existing subscriptions - actual effects might be delayed, for
 * example, if the subscription's callback is running in another thread.
 */
void facil_unsubscribe(subscription_s *subscription);

/**
 * This helper returns a temporary handle to an existing subscription's channel
 * or filter.
 *
 * To keep the handle beyond the lifetime of the subscription, use `fiobj_dup`.
 */
FIOBJ facil_subscription_channel(subscription_s *subscription);

/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `facil_publish_args_s` for details.
 *
 * By default the message is sent using the FACIL_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FACIL_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FACIL_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
void facil_publish(facil_publish_args_s args);
/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `facil_publish_args_s` for details.
 *
 * By default the message is sent using the FACIL_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FACIL_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FACIL_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
#define facil_publish(...) facil_publish((facil_publish_args_s){__VA_ARGS__})

/** Finds the message's metadata by it's type ID. Returns the data or NULL. */
void *facil_message_metadata(facil_msg_s *msg, intptr_t type_id);

/**
 * Defers the current callback, so it will be called again for the message.
 */
void facil_message_defer(facil_msg_s *msg);

/**
 * Signals all workers to shutdown, which might invoke a respawning of the
 * workers unless the shutdown signal was received.
 *
 * NOT signal safe.
 */
void facil_cluster_signal_children(void);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/** Contains message metadata, set by message extensions. */
typedef struct facil_msg_metadata_s facil_msg_metadata_s;
struct facil_msg_metadata_s {
  /** The type ID should be used to identify the metadata's actual structure. */
  intptr_t type_id;
  /**
   * This method will be called by facil.io to cleanup the metadata resources.
   *
   * Don't alter / call this method, this data is reserved.
   */
  void (*on_finish)(facil_msg_s *msg, facil_msg_metadata_s *self);
  /** The pointer to be returned by the `facil_message_metadata` function. */
  void *metadata;
  /** RESERVED for internal use (Metadata linked list). */
  facil_msg_metadata_s *next;
};

/**
 * It's possible to attach metadata to facil.io pub/sub messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * The callback should return a pointer to a valid metadata object that remains
 * valid until the object's `on_finish` callback is called.
 *
 * Since the cluster messaging system automatically serializes some objects to
 * JSON (unless both the channel and the data are String objects or missing),
 * the pre-serialized data is available to the callback as the `raw_ch` and
 * `raw_msg` arguments.
 *
 * To remove a callback, set the `remove` flag to true (`1`).
 */
void facil_message_metadata_set(
    facil_msg_metadata_s *(*callback)(facil_msg_s *msg, FIOBJ raw_ch,
                                      FIOBJ raw_msg),
    int remove);

/**
 * facil.io can be linked with external Pub/Sub services using "engines".
 *
 * Only unfiltered messages and subscriptions (where filter == 0) will be
 * forwarded to external Pub/Sub services.
 *
 * Engines MUST provide the listed function pointers and should be registered
 * using the `pubsub_engine_register` function.
 *
 * Engines should deregister, before being destroyed, by using the
 * `pubsub_engine_deregister` function.
 *
 * When an engine received a message to publish, it should call the
 * `pubsub_publish` function with the engine to which the message is forwarded.
 * i.e.:
 *
 *       pubsub_publish(
 *           .engine = FACIL_PROCESS_ENGINE,
 *           .channel = channel_name,
 *           .message = msg_body );
 *
 * Engines MUST NOT free any of the FIOBJ objects they receive.
 *
 */
struct pubsub_engine_s {
  /** Should subscribe channel. Failures are ignored. */
  void (*subscribe)(const pubsub_engine_s *eng, FIOBJ channel,
                    facil_match_fn match);
  /** Should unsubscribe channel. Failures are ignored. */
  void (*unsubscribe)(const pubsub_engine_s *eng, FIOBJ channel,
                      facil_match_fn match);
  /** Should return 0 on success and -1 on failure. */
  int (*publish)(const pubsub_engine_s *eng, FIOBJ channel, FIOBJ msg);
  /**
   * facil.io will call this callback whenever starting, or restarting, the
   * reactor.
   *
   * This will be called when facil.io starts (the master process).
   *
   * This will also be called when forking, after facil.io closes all
   * connections and claim to shut down (running all deferred event).
   */
  void (*on_startup)(const pubsub_engine_s *eng);
};

/** Attaches an engine, so it's callback can be called by facil.io. */
void facil_pubsub_attach(pubsub_engine_s *engine);

/** Detaches an engine, so it could be safely destroyed. */
void facil_pubsub_detach(pubsub_engine_s *engine);

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 */
void facil_pubsub_reattach(pubsub_engine_s *eng);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* H_FACIL_H */
