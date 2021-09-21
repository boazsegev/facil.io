/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/io-core
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define H_FACIL_IO_H

#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
General Compile Time Settings
***************************************************************************** */
#ifndef FIO_CPU_CORES_FALLBACK
/**
 * When failing to detect the available CPU cores, this is the used value.
 *
 * Note: this does not affect the FIO_MEMORY_ARENA_COUNT_FALLBACK value.
 */
#define FIO_CPU_CORES_FALLBACK 8
#endif

#ifndef FIO_CPU_CORES_LIMIT
/** Maximum number of cores to detect. */
#define FIO_CPU_CORES_LIMIT 32
#endif

#ifndef FIO_SOCKET_BUFFER_PER_WRITE
/** The buffer size on the stack, for when a call to `write` required a copy. */
#define FIO_SOCKET_BUFFER_PER_WRITE (1UL << 16)
#endif

#ifndef FIO_SOCKET_THROTTLE_LIMIT
/** Throttle the client (prevent `on_data`) at outgoing byte queue limit. */
#define FIO_SOCKET_THROTTLE_LIMIT (1UL << 20)
#endif

#ifndef FIO_IO_TIMEOUT_MAX
#define FIO_IO_TIMEOUT_MAX 600
#endif

#ifndef FIO_SHOTDOWN_TIMEOUT
/** The number of shutdown seconds after which unsent data is ignored. */
#define FIO_SHOTDOWN_TIMEOUT 5
#endif

/* *****************************************************************************
CSTL modules
***************************************************************************** */
#define FIO_LOG
#define FIO_EXTERN
#include "fio-stl.h"

#if !defined(FIO_USE_THREAD_MUTEX) && FIO_OS_POSIX
#define FIO_USE_THREAD_MUTEX 1
#endif

/* CLI extension should use the system allocator. */
#define FIO_EXTERN
#define FIO_CLI
#include "fio-stl.h"

#define FIO_EXTERN
#define FIO_MALLOC
#include "fio-stl.h"

#define FIO_EXTERN
#define FIO_ATOL
#define FIO_ATOMIC
#define FIO_BITWISE
#define FIO_GLOB_MATCH
#define FIO_LOCK
#define FIO_RAND
#define FIO_THREADS
#define FIO_TIME
#define FIO_URL
#include "fio-stl.h"

#define FIO_EXTERN
#define FIOBJ_EXTERN
#define FIOBJ_MALLOC
#define FIO_FIOBJ
#include "fio-stl.h"

#define FIO_STREAM
#define FIO_QUEUE
#define FIO_SOCK
#include "fio-stl.h"

/* Should be automatic, but why not... */
#undef FIO_EXTERN
#undef FIO_EXTERN_COMPLETE
/* *****************************************************************************
Additional Included files
***************************************************************************** */
#if FIO_OS_POSIX
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

/** The main protocol object type. See `struct fio_protocol_s`. */
typedef struct fio_protocol_s fio_protocol_s;

/** The main protocol object type. See `struct fio_protocol_s`. */
typedef struct fio_s fio_s;

/** TLS context object, if any. */
typedef struct fio_tls_s fio_tls_s;

/* *****************************************************************************
Quick Windows Patches
***************************************************************************** */
#if FIO_OS_WIN
#ifndef pid_t
#define pid_t DWORD
#endif
#ifndef getpid
#define getpid GetCurrentProcessId
#endif
#endif
/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

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
IO related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/**************************************************************************/ /**
The Protocol
============

The Protocol struct defines the callbacks used for a family of connections and
sets their behavior. The Protocol struct is part of facil.io's core design.

All the callbacks receive a unique connection ID that can be converted to the
original file descriptor when in need (which shouldn't really happen).

This allows facil.io to prevent old connection handles from sending data to new
connections after a file descriptor is "recycled" by the OS.
*/
struct fio_protocol_s {
  /**
   * Reserved / private data - used by facil.io internally.
   * MUST be initialized to zero
   */
  struct {
    /* A linked list of currently attached IOs - do NOT alter. */
    FIO_LIST_HEAD ios;
    /* A linked list of other protocols used by IO core - do NOT alter. */
    FIO_LIST_NODE protocols;
    /* internal flags - do NOT alter after initial initialization to zero. */
    uintptr_t flags;
  } reserved;
  /**
   * Called when an IO was attached. Locks the IO's task lock.
   *
   * Note: this is scheduled when setting the protocol, but depending on race
   * conditions, the previous protocol, and the socket's state, it may run after
   * the `on_ready` or `on_data` are called.
   * */
  void (*on_attach)(fio_s *io);
  /** Called when a data is available. Locks the IO's task lock. */
  void (*on_data)(fio_s *io);
  /** called once all pending `fio_write` calls are finished. */
  void (*on_ready)(fio_s *io);
  /**
   * Called when the connection was closed, and all pending tasks are complete.
   */
  void (*on_close)(void *udata);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * Locks the IO's task lock.
   *
   * After the `on_shutdown` callback returns, the socket is marked for closure.
   *
   * Once the socket was marked for closure, facil.io will allow a limited
   * amount of time for data to be sent, after which the socket might be closed
   * even if the client did not consume all buffered data.
   */
  void (*on_shutdown)(fio_s *io);
  /** Called when a connection's timeout was reached */
  void (*on_timeout)(fio_s *io);
  /**
   * The timeout value in seconds for all connections using this protocol.
   *
   * Limited to 600 seconds. The value 0 will be the same as the timeout limit.
   */
  uint32_t timeout;
};

/** Points to a function that keeps the connection alive. */
extern void (*FIO_PING_ETERNAL)(fio_s *io);

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
  void (*on_open)(int fd, void *udata);
  /** Opaque user data. */
  void *udata;
  /**
   * Called when the server is done, usable for cleanup.
   *
   * This will be called separately for every process before exiting.
   */
  void (*on_finish)(void *udata);
  /* for internal use. */
  int reserved;
  /** Listen and connect using only the master process (non-worker). */
  uint8_t is_master_only;
};

/**
 * Sets up a network service on a listening socket.
 *
 * Returns 0 on success or -1 on error.
 *
 * See the `fio_listen` Macro for details.
 */
int fio_listen(struct fio_listen_args args);

/************************************************************************ */ /**
Listening to Incoming Connections
===

Listening to incoming connections is pretty straight forward:

* After a new connection is accepted, the `on_open` callback is called.

* `on_open` should "attach" the new connection using `fio_attach_fd`.

* Once listening is done, cleanup is performed in `on_finish`.

The following is an example echo server using facil.io:

```c

```
*/
#define fio_listen(url_, ...)                                                  \
  fio_listen((struct fio_listen_args){.url = (url_), __VA_ARGS__})

/* *****************************************************************************
Controlling the connection
***************************************************************************** */

/**
 * Attaches the socket in `fd` to the facio.io engine (reactor).
 *
 * * `fd` should point to a valid socket.
 *
 * * A `protocol` is always required. The system cannot manage a socket without
 *   a protocol.
 *
 * * `udata` is opaque user data and may be any value, including NULL.
 *
 * * `tls` is a context for Transport Layer Security and can be used to redirect
 *   read/write operations. NULL will result in unencrypted transmissions.
 *
 * Returns NULL on error. the `fio_s` pointer must NOT be used except within
 * proper callbacks.
 */
fio_s *fio_attach_fd(int fd,
                     fio_protocol_s *protocol,
                     void *udata,
                     fio_tls_s *tls);

/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_s *io, fio_protocol_s *protocol);

/**
 * Returns a pointer to the current protocol object.
 *
 * If `protocol` wasn't properly set, the pointer might be invalid.
 */
fio_protocol_s *fio_protocol_get(fio_s *io);

/** Associates a new `udata` pointer with the IO, returning the old `udata` */
FIO_IFUNC void *fio_udata_set(fio_s *io, void *udata) {
  void *old = *(void **)io;
  *(void **)io = udata;
  return old;
}

/** Returns the `udata` pointer associated with the IO. */
FIO_IFUNC void *fio_udata_get(fio_s *io) { return *(void **)io; }

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_s *io, void *buf, size_t len);

typedef struct {
  /** The buffer with the data to send (if no file descriptor) */
  void *buf;
  /** The file descriptor to send (if no buffer) */
  intptr_t fd;
  /** The length of the data to be sent. On files, 0 = the whole file. */
  size_t len;
  /** The length of the data to be sent. On files, 0 = the whole file. */
  size_t offset;
  /**
   * If this is a buffer, the de-allocation function used to free it.
   *
   * If NULL, the buffer will NOT be de-allocated.
   */
  void (*dealloc)(void *);
  /** If non-zero, makes a copy of the buffer or keeps a file open. */
  uint8_t copy;
} fio_write_args_s;

/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2(fio_s *io, fio_write_args_s args);
#define fio_write2(io, ...) fio_write2(io, (fio_write_args_s){__VA_ARGS__})

/** Helper macro for a common fio_write2 (copies the buffer). */
#define fio_write(io, buf_, len_)                                              \
  fio_write2(io, .buf = (buf_), .len = (len_), .copy = 1)

/**
 * Sends data from a file as if it were a single atomic packet (sends up to
 * length bytes or until EOF is reached).
 *
 * Once the file was sent, the `source_fd` will be closed using `close`.
 *
 * The file will be buffered to the socket chunk by chunk, so that memory
 * consumption is capped.
 *
 * `offset` dictates the starting point for the data to be sent and length sets
 * the maximum amount of data to be sent.
 *
 * Returns -1 and closes the file on error. Returns 0 on success.
 */
#define fio_sendfile(io, source_fd, offset, bytes)                             \
  fio_write2((io),                                                             \
             .fd = (source_fd),                                                \
             .offset = (size_t)(offset),                                       \
             .len = (bytes))

/** Marks the IO for closure as soon as scheduled data was sent. */
void fio_close(fio_s *io);

/** Marks the IO for immediate closure. */
void fio_close_now(fio_s *io);

/**
 * Increases a IO's reference count, so it won't be automatically destroyed
 * when all tasks have completed.
 *
 * Use this function in order to use the IO outside of a scheduled task.
 */
fio_s *fio_dup(fio_s *io);

/**
 * Decreases a IO's reference count, so it could be automatically destroyed
 * when all other tasks have completed.
 *
 * Use this function once finished with a IO that was `dup`-ed.
 */
void fio_undup(fio_s *io);

/** Suspends future "on_data" events for the IO. */
void fio_suspend(fio_s *io);

/**
 * Returns true if a IO is busy with an ongoing task.
 *
 * This is only valid if the IO was copied to a deferred task or if called
 * within an on_timeout callback, allowing the user to speculate about the IO
 * task lock being engaged.
 *
 * Note that the information provided is speculative as the situation may change
 * between the moment the data is collected and the moment the function returns.
 */
int fio_is_busy(fio_s *io);

/* Resets a socket's timeout counter. */
void fio_touch(fio_s *io);
/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */

/** Named arguments for the `fio_env_set` function. */
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
} fio_env_set_args_s;

/** Named arguments for the `fio_env_unset` function. */
typedef struct {
  intptr_t type;
  fio_str_info_s name;
} fio_env_unset_args_s;

/**
 * Links an object to a connection's lifetime / environment.
 *
 * The `on_close` callback will be called once the connection has died.
 *
 * If the `io` is NULL, the value will be set for the global environment.
 */
void fio_env_set(fio_s *io, fio_env_set_args_s);

/**
 * Links an object to a connection's lifetime, calling the `on_close` callback
 * once the connection has died.
 *
 * If the `io` is NULL, the value will be set for the global environment, in
 * which case the `on_close` callback will only be called once the process
 * exits.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_set(io, ...) fio_env_set(io, (fio_env_set_args_s){__VA_ARGS__})

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found.
 */
int fio_env_unset(fio_s *io, fio_env_unset_args_s);

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_unset(io, ...)                                                 \
  fio_env_unset(io, (fio_env_unset_args_s){__VA_ARGS__})

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 */
int fio_env_remove(fio_s *io, fio_env_unset_args_s);

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_remove(io, ...)                                                \
  fio_env_remove(io, (fio_env_unset_args_s){__VA_ARGS__})
/* *****************************************************************************
Task Scheduling
***************************************************************************** */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_s *io, void (*task)(fio_s *io, void *udata), void *udata);

/** Schedules a timer bound task, see `fio_timer_schedule` in the CSTL. */
void fio_run_every(fio_timer_schedule_args_s args);
/**
 * Schedules a timer bound task, see `fio_timer_schedule` in the CSTL.
 *
 * Possible "named arguments" (fio_timer_schedule_args_s members) include:
 *
 * * The timer function. If it returns a non-zero value, the timer stops:
 *        int (*fn)(void *, void *)
 * * Opaque user data:
 *        void *udata1
 * * Opaque user data:
 *        void *udata2
 * * Called when the timer is done (finished):
 *        void (*on_finish)(void *, void *)
 * * Timer interval, in milliseconds:
 *        uint32_t every
 * * The number of times the timer should be performed. -1 == infinity:
 *        int32_t repetitions
 */
#define fio_run_every(...)                                                     \
  fio_run_every((fio_timer_schedule_args_s){__VA_ARGS__})
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
  /** Called every time a *Worker* process starts. */
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
  /** Called just before finishing up (both on child and parent processes). */
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

#ifdef TEST                                    /* development sugar, ignore */
FIO_SFUNC void FIO_NAME_TEST(io, state)(void); /* development sugar, ignore */
#endif                                         /* development sugar, ignore */
/* *****************************************************************************
Pub/Sub
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Pub/Sub - defaults and builtin pub/sub engines
***************************************************************************** */

/** A pub/sub engine data structure. See details later on. */
typedef struct fio_pubsub_engine_s fio_pubsub_engine_s;

/** Used to publish the message exclusively to the root / master process. */
extern const fio_pubsub_engine_s *const FIO_PUBSUB_ROOT;
/** Used to publish the message only within the current process. */
extern const fio_pubsub_engine_s *const FIO_PUBSUB_PROCESS;
/** Used to publish the message except within the current process. */
extern const fio_pubsub_engine_s *const FIO_PUBSUB_SIBLINGS;
/** Used to publish the message for this process, its siblings and root. */
extern const fio_pubsub_engine_s *const FIO_PUBSUB_LOCAL;

/** The default engine (settable). Initial default is FIO_PUBSUB_LOCAL. */
extern const fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT;

/**
 * The pattern matching callback used for pattern matching.
 *
 * Returns 1 on a match or 0 if the string does not match the pattern.
 *
 * By default, the value is set to `fio_glob_match` (see facil.io's C STL).
 */
extern uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s pattern,
                                           fio_str_info_s channel);

/* *****************************************************************************
Pub/Sub - message format
***************************************************************************** */

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct fio_msg_s {
  /**
   * A connection (if any) to which the subscription belongs.
   *
   * The connection uuid 0 marks an un-bound (non-connection related)
   * subscription.
   */
  fio_s *io;
  /**
   * A channel name, allowing for pub/sub patterns.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field might be used for internal data.
   */
  fio_str_info_s channel;
  /**
   * The actual message.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field is reserved for internal data and must NOT be used.
   **/
  fio_str_info_s message;
  /** The `udata` argument associated with the subscription. */
  void *udata;
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** flag indicating if the message is JSON data or binary/text. */
  uint8_t is_json;
} fio_msg_s;

/* *****************************************************************************
Pub/Sub - Subscribe / Unsubscribe
***************************************************************************** */

/**
 * Pattern matching callback type. Use `fio_glob_match` (see facil.io's C STL)
 * for Redis style glob matching.
 */
typedef uint8_t (*fio_pubsub_pattern_fn)(fio_str_info_s pattern,
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
   * The subscription owner - if none, the subscription is owned by the system.
   */
  fio_s *io;
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
   * The callback to be called for each message forwarded to the subscription.
   */
  void (*on_message)(fio_msg_s *msg);
  /** An optional callback for when a subscription is canceled. */
  void (*on_unsubscribe)(void *udata);
  /** The opaque udata value is ignored and made available to the callbacks. */
  void *udata;
  /**
   * If `filter` is set, `channel` is ignored and all messages that match the
   * filter's numerical value will be forwarded to the subscription's callback.
   *
   * Negative values are reserved for facil.io framework extensions.
   *
   * Filer channels are bound to the processes and workers, they are NOT
   * forwarded to engines and can be used for inter process communication (IPC).
   */
  int32_t filter;
  /** If set, pattern matching will be used (name is a pattern). */
  uint8_t is_pattern;
} subscribe_args_s;

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * The on_unsubscribe callback will be called on failure.
 */
void fio_subscribe(subscribe_args_s args);

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * See `subscribe_args_s` for details.
 */
#define fio_subscribe(...) fio_subscribe((subscribe_args_s){__VA_ARGS__})

/**
 * Cancels an existing subscriptions.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 *
 * Returns -1 if the subscription could not be found. Otherwise returns 0.
 */
int fio_unsubscribe(subscribe_args_s args);

/**
 * Cancels an existing subscriptions.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 *
 * Returns -1 if the subscription could not be found. Otherwise returns 0.
 */
#define fio_unsubscribe(...) fio_unsubscribe((subscribe_args_s){__VA_ARGS__})

/* *****************************************************************************
Pub/Sub - Publish
***************************************************************************** */

/** Publishing and on_message callback arguments. */
typedef struct fio_publish_args_s {
  /** The pub/sub engine that should be used to forward this message. */
  fio_pubsub_engine_s const *engine;
  /** The target named channel. Only published when filter == 0. */
  fio_str_info_s channel;
  /** The message body / content. */
  fio_str_info_s message;
  /** A numeral / internal channel. Negative values are reserved. */
  int32_t filter;
  /** A flag indicating if the message is JSON data or not. */
  uint8_t is_json;
} fio_publish_args_s;

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
 * After calling this function, the `msg` object must NOT be accessed again.
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
#if 0
/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *server_tls, fio_tls_s *client_tls);

/** Used to publish the message to any possible publishers. */
extern const fio_pubsub_engine_s *const FIO_PUBSUB_CLUSTER;
#endif
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
/* *****************************************************************************
Development Sugar (ignore)
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define FIO_EXTERN_COMPLETE   1 /* Development inclusion - ignore line */
#define FIOBJ_EXTERN_COMPLETE 1 /* Development inclusion - ignore line */
#define FIO_VERSION_GUARD       /* Development inclusion - ignore line */
#define TEST                    /* Development inclusion - ignore line */
#include "001 head.h"           /* Development inclusion - ignore line */
#include "011 startup.h"        /* Development inclusion - ignore line */
#include "012 io.h"             /* Development inclusion - ignore line */
#include "013 env.h"            /* Development inclusion - ignore line */
#include "020 tasks.h"          /* Development inclusion - ignore line */
#include "021 state.h"          /* Development inclusion - ignore line */
#include "030 pubsub.h"         /* Development inclusion - ignore line */
#include "099 footer.h"         /* Development inclusion - ignore line */
#include "101 helpers.c"        /* Development inclusion - ignore line */
#include "102 core.c"           /* Development inclusion - ignore line */
#include "103 io.c"             /* Development inclusion - ignore line */
#include "105 events.c"         /* Development inclusion - ignore line */
#include "109 polling.c"        /* Development inclusion - ignore line */
#include "110 reactor.c"        /* Development inclusion - ignore line */
#include "200 pubsub.c"         /* Development inclusion - ignore line */
#endif
