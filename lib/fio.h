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

#ifndef FIO_UUID_TIMEOUT_MAX
#define FIO_UUID_TIMEOUT_MAX 600
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
typedef struct fio_uuid_s fio_uuid_s;

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
UUID related operations (set protocol, read, write, etc')
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
    /* A linked list of currently attached UUIDs - do NOT alter. */
    FIO_LIST_HEAD uuids;
    /* A linked list of other protocols used by IO core - do NOT alter. */
    FIO_LIST_NODE protocols;
    /* internal flags - set to zero, do NOT alter. */
    uintptr_t flags;
  } reserved;
  /** Called when a data is available. Locks the UUID's task lock. */
  void (*on_data)(fio_uuid_s *uuid, void *udata);
  /** called once all pending `fio_write` calls are finished. */
  void (*on_ready)(fio_uuid_s *uuid, void *udata);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * Locks the UUID's task lock.
   *
   * The `on_shutdown` callback should return 0 to close the socket or a
   * non-zero value to delay the closure of the socket.
   *
   * Once the socket was marked for closure, facil.io will allow a limited
   * amount of time for data to be sent, after which the socket will be closed
   * even if the client did not consume all buffered data.
   *
   * If the socket closure is delayed, it will might be abruptly terminated when
   * all other sockets have finished their graceful shutdown procedure.
   */
  uint8_t (*on_shutdown)(fio_uuid_s *uuid, void *udata);
  /**
   * Called when the connection was closed, and all pending tasks are complete.
   */
  void (*on_close)(void *udata);
  /** Called when a connection's timeout was reached */
  void (*on_timeout)(fio_uuid_s *uuid, void *udata);
  /**
   * The timeout value in seconds for all connections using this protocol.
   *
   * Limited to 600 seconds. The value 0 will be the same as the timeout limit.
   */
  uint32_t timeout;
};

/** Points to a function that keeps the connection alive. */
extern void (*FIO_PING_ETERNAL)(fio_uuid_s *uuid, void *udata);

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
 * Returns NULL on error.
 */
fio_uuid_s *fio_attach_fd(int fd,
                          fio_protocol_s *protocol,
                          void *udata,
                          fio_tls_s *tls);

/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_uuid_s *uuid, fio_protocol_s *protocol);

/** Associates a new `udata` pointer with the UUID, returning the old `udata` */
void *fio_udata_set(fio_uuid_s *uuid, void *udata);

/** Returns the `udata` pointer associated with the UUID. */
void *fio_udata_get(fio_uuid_s *uuid);

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_uuid_s *uuid, void *buf, size_t len);

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
void fio_write2(fio_uuid_s *uuid, fio_write_args_s args);
#define fio_write2(uuid, ...) fio_write2(uuid, (fio_write_args_s){__VA_ARGS__})

/** Helper macro for a common fio_write2 (copies the buffer). */
#define fio_write(uuid, buf_, len_)                                            \
  fio_write2(uuid, .buf = (buf_), .len = (len_), .copy = 1)

/** Marks the UUID for closure as soon as scheduled data was sent. */
void fio_close(fio_uuid_s *uuid);

/** Marks the UUID for immediate closure. */
void fio_close_now(fio_uuid_s *uuid);

/**
 * Increases a UUID's reference count, so it won't be automatically destroyed
 * when all tasks have completed.
 *
 * Use this function in order to use the UUID outside of a scheduled task.
 */
fio_uuid_s *fio_uuid_dup(fio_uuid_s *uuid);
/**
 * Decreases a UUID's reference count, so it could be automatically destroyed
 * when all other tasks have completed.
 *
 * Use this function in order to use the UUID outside of a scheduled task.
 */
void fio_uuid_free(fio_uuid_s *uuid);

/**
 * Returns true if a UUID is busy with an ongoing task.
 *
 * This is only valid if the UUID was copied to a deferred task or if called
 * within an on_timeout or on_ready callback, allowing the user to speculate
 * about the IO task lock being engaged.
 *
 * Note that the information provided is speculative as the situation may change
 * between the moment the data is collected and the moment the function returns.
 */
int fio_uuid_is_busy(fio_uuid_s *uuid);

/* Resets a socket's timeout counter. */
void fio_touch(fio_uuid_s *uuid);
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

#ifdef TEST                                    /* development sugar, ignore */
FIO_SFUNC void FIO_NAME_TEST(io, state)(void); /* development sugar, ignore */
#endif                                         /* development sugar, ignore */
/* *****************************************************************************
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_uuid_s *uuid,
                  void (*task)(fio_uuid_s *uuid, void *udata),
                  void *udata);
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
#include "012 uuid.h"           /* Development inclusion - ignore line */
#include "020 state.h"          /* Development inclusion - ignore line */
#include "021 tasks.h"          /* Development inclusion - ignore line */
#include "999 footer.h"         /* Development inclusion - ignore line */

#include "002 helpers.c" /* Development inclusion - ignore line */
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
