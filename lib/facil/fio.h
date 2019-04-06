/*
Copyright: Boaz Segev, 2018-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#ifndef H_FACIL_IO_H
/**
"facil.h" is the main header for the facil.io server platform.
*/
#define H_FACIL_IO_H

/* *****************************************************************************
 * Table of contents (find by subject):
 * =================
 * Version and helper macros
 * Helper String Information Type
 * Memory pool / custom allocator for short lived objects
 * Logging and testing helpers
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
 * Lower Level API - for special circumstances, use with care under
 *
 * Pub/Sub / Cluster Messages API
 * Cluster Messages and Pub/Sub
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 *
 * Atomic Operations and Spin Locking Helper Functions
 *
 * SipHash
 * SHA-1
 * SHA-2
 * Base64 (URL) encoding
 *
 * Memory Allocator Details
 *
 * Spin locking Implementation
 *
 ******** facil.io Data Types (String, Set / Hash Map, Linked Lists, etc')
 *
 * These types can be included by defining the macros and (re)including fio.h.
 *
 *
 *
 *                      #ifdef FIO_INCLUDE_LINKED_LIST
 *
 * Linked List Helpers
 * Independent Linked List API
 * Embedded Linked List API* Independent Linked List Implementation
 * Embeded Linked List Implementation
 *
 *
 *            #ifdef FIO_SET_NAME - can be included more than once
 *
 * Set / Hash Map Data-Store
 * Set / Hash Map API
 * Set / Hash Map Internal Data Structures
 * Set / Hash Map Internal Helpers
 * Set / Hash Map Implementation
 *
 **************************************************************************** */

/* *****************************************************************************
C++ extern start
***************************************************************************** */
/* support C++ */
#ifdef __cplusplus
extern "C" {
/* C++ keyword was deprecated */
#define register
#endif

/* *****************************************************************************
Import STL
***************************************************************************** */
#define FIO_RISKY_HASH 1
#include "fio-stl.h"

#ifndef FIO_LOG_LENGTH_LIMIT
/**
 * Since logging uses stack memory rather than dynamic allocation, it's memory
 * usage must be limited to avoid exploding the stack. The following sets the
 * memory used for a logging event.
 */
#define FIO_LOG_LENGTH_LIMIT 2048
#endif
#if !defined(FIO_FORCE_MALLOC)
#define FIO_MALLOC 1
#define FIO_EXTERN 1
#include "fio-stl.h"
#endif

#define FIO_EXTERN 1
#define FIO_ATOMIC 1
#define FIO_BITWISE 1
#define FIO_ATOL 1
#define FIO_NTOL 1
#define FIO_RAND 1
#define FIO_CLI 1
#define FIO_STR_INFO 1
#include "fio-stl.h"

/* *****************************************************************************
Compilation Macros
***************************************************************************** */

#ifndef FIO_MAX_SOCK_CAPACITY
/**
 * The maximum number of connections per worker process.
 */
#define FIO_MAX_SOCK_CAPACITY 131072
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
#define FIO_CPU_CORES_LIMIT 8
#endif

#ifndef FIO_DEFER_THROTTLE_PROGRESSIVE
/**
 * The progressive throttling model makes concurrency and parallelism more
 * likely.
 *
 * Otherwise threads are assumed to be intended for "fallback" in case of slow
 * user code, where a single thread should be active most of the time and other
 * threads are activated only when that single thread is slow to perform.
 */
#define FIO_DEFER_THROTTLE_PROGRESSIVE 1
#endif

#ifndef FIO_PUBSUB_SUPPORT
/**
 * If true (1), compiles the facil.io pub/sub API.
 */
#define FIO_PUBSUB_SUPPORT 1
#endif

#ifndef FIO_IGNORE_MACRO
/**
 * This is used internally to ignore macros that shadow functions (avoiding
 * named arguments when required).
 */
#define FIO_IGNORE_MACRO
#endif

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <strings.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#if defined(__FreeBSD__)
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/* *****************************************************************************
Patch for OSX version < 10.12 from https://stackoverflow.com/a/9781275/4025095
***************************************************************************** */
#if defined(__MACH__) && !defined(CLOCK_REALTIME)
#include <sys/time.h>
#define CLOCK_REALTIME 0
#define clock_gettime patch_clock_gettime
// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_REALTIME will have already been defined.
FIO_FUNC inline int patch_clock_gettime(int clk_id, struct timespec *t) {
  struct timeval now;
  int rv = gettimeofday(&now, NULL);
  if (rv)
    return rv;
  t->tv_sec = now.tv_sec;
  t->tv_nsec = now.tv_usec * 1000;
  return 0;
  (void)clk_id;
}
#endif

/* *****************************************************************************












Connection Callback (Protocol) Management












***************************************************************************** */

typedef struct fio_protocol_s fio_protocol_s;
/**************************************************************************/ /**
* The Protocol

The Protocol struct defines the callbacks used for the connection and sets it's
behaviour. The Protocol struct is part of facil.io's core design.

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
  uint8_t (*on_shutdown)(intptr_t uuid, fio_protocol_s *protocol);
  /** Called when the connection was closed, but will not run concurrently */
  void (*on_close)(intptr_t uuid, fio_protocol_s *protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t uuid, fio_protocol_s *protocol);
  /** private metadata used by facil. */
  size_t rsv;
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

/**
 * Sets a socket to non blocking state.
 *
 * This will also set the O_CLOEXEC flag for the file descriptor.
 *
 * This function is called automatically for the new socket, when using
 * `fio_accept` or `fio_connect`.
 */
int fio_set_non_block(int fd);

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

/* *****************************************************************************
Listening to Incoming Connections
***************************************************************************** */

/* Arguments for the fio_listen function */
struct fio_listen_args {
  /**
   * Called whenever a new connection is accepted.
   *
   * Should either call `fio_attach` or close the connection.
   */
  void (*on_open)(intptr_t uuid, void *udata);
  /** The network service / port. Defaults to "3000". */
  const char *port;
  /** The socket binding address. Defaults to the recommended NULL. */
  const char *address;
  /** a pointer to a `fio_tls_s` object, for SSL/TLS support (fio_tls.h). */
  void *tls;
  /** Opaque user data. */
  void *udata;
  /**
   * Called when the server starts (or a worker process is respawned), allowing
   * for further initialization, such as timed event scheduling or VM
   * initialization.
   *
   * This will be called separately for every worker process whenever it is
   * spawned.
   */
  void (*on_start)(intptr_t uuid, void *udata);
  /**
   * Called when the server is done, usable for cleanup.
   *
   * This will be called separately for every process. */
  void (*on_finish)(intptr_t uuid, void *udata);
};

/**
 * Sets up a network service on a listening socket.
 *
 * Returns the listening socket's uuid or -1 (on error).
 *
 * See the `fio_listen` Macro for details.
 */
intptr_t fio_listen(struct fio_listen_args args);

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
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  // Read to the buffer, starting after the "Echo: "
  while ((len = fio_read(uuid, buffer + 6, 1018)) > 0) {
    fprintf(stderr, "Read: %.*s", (int)len, buffer + 6);
    // Write back the message
    fio_write(uuid, buffer, len + 6);
    // Handle goodbye
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      fio_write(uuid, "Goodbye.\n", 9);
      fio_close(uuid);
      return;
    }
  }
}

// A callback called whenever a timeout is reach
static void echo_ping(intptr_t uuid, fio_protocol_s *prt) {
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
                                 .ping = echo_ping};
  fprintf(stderr, "New Connection %p received from %s\n", (void *)echo_proto,
          fio_peer_addr(uuid).data);
  fio_attach(uuid, echo_proto);
  fio_write2(uuid, .data.buffer = "Echo Service: Welcome\n", .length = 22,
             .after.dealloc = FIO_DEALLOC_NOOP);
  fio_timeout_set(uuid, 5);
}

int main() {
  // Setup a listening socket
  if (fio_listen(.port = "3000", .on_open = echo_on_open) == -1) {
    perror("No listening socket available on port 3000");
    exit(-1);
  }
  // Run the server and hang until a stop signal is received.
  fio_start(.threads = 4, .workers = 1);
}
```
*/
#define fio_listen(...) fio_listen((struct fio_listen_args){__VA_ARGS__})

/* *****************************************************************************
Connecting to remote servers as a client
***************************************************************************** */

/**
Named arguments for the `fio_connect` function, that allows non-blocking
connections to be established.
*/
struct fio_connect_args {
  /** The address of the server we are connecting to. */
  const char *address;
  /** The port on the server we are connecting to. */
  const char *port;
  /**
   * The `on_connect` callback either call `fio_attach` or close the connection.
   */
  void (*on_connect)(intptr_t uuid, void *udata);
  /**
   * The `on_fail` is called when a socket fails to connect. The old sock UUID
   * is passed along.
   */
  void (*on_fail)(intptr_t uuid, void *udata);
  /** a pointer to a `fio_tls_s` object, for SSL/TLS support (fio_tls.h). */
  void *tls;
  /** Opaque user data. */
  void *udata;
  /** A non-system timeout after which connection is assumed to have failed. */
  uint8_t timeout;
};

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
intptr_t fio_connect(struct fio_connect_args);
#define fio_connect(...) fio_connect((struct fio_connect_args){__VA_ARGS__})

/* *****************************************************************************
URL address parsing
***************************************************************************** */

/** the result returned by `fio_url_parse` */
typedef struct {
  fio_str_info_s scheme;
  fio_str_info_s user;
  fio_str_info_s password;
  fio_str_info_s host;
  fio_str_info_s port;
  fio_str_info_s path;
  fio_str_info_s query;
  fio_str_info_s target;
} fio_url_s;

/**
 * Parses the URI returning it's components and their lengths (no decoding
 * performed, doesn't accept decoded URIs).
 *
 * The returned string are NOT NUL terminated, they are merely locations within
 * the original string.
 *
 * This function attempts to accept many different formats, including any of the
 * following:
 *
 * * `/complete_path?query#target`
 *
 *   i.e.: /index.html?page=1#list
 *
 * * `host:port/complete_path?query#target`
 *
 *   i.e.:
 *      example.com
 *      example.com:8080
 *      example.com/index.html
 *      example.com:8080/index.html
 *      example.com:8080/index.html?key=val#target
 *
 * * `user:password@host:port/path?query#target`
 *
 *   i.e.: user:1234@example.com:8080/index.html
 *
 * * `username[:password]@host[:port][...]`
 *
 *   i.e.: john:1234@example.com
 *
 * * `schema://user:password@host:port/path?query#target`
 *
 *   i.e.: http://example.com/index.html?page=1#list
 *
 * Invalid formats might produce unexpected results. No error testing performed.
 */
fio_url_s fio_url_parse(const char *url, size_t length);
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
 * Returns the number of expected threads / processes to be used by facil.io.
 *
 * The pointers should start with valid values that match the expected threads /
 * processes values passed to `fio_start`.
 *
 * The data in the pointers will be overwritten with the result.
 */
void fio_expected_concurrency(int16_t *threads, int16_t *workers);

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
pid_t fio_parent_pid(void);

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

/* *****************************************************************************
Socket / Connection Functions
***************************************************************************** */

/**
 * Creates a Unix or a TCP/IP socket and returns it's unique identifier.
 *
 * For TCP/IP server sockets (`is_server` is `1`), a NULL `address` variable is
 * recommended. Use "localhost" or "127.0.0.1" to limit access to the server
 * application.
 *
 * For TCP/IP client sockets (`is_server` is `0`), a remote `address` and `port`
 * combination will be required
 *
 * For Unix server or client sockets, set the `port` variable to NULL or `0`.
 *
 * Returns -1 on error. Any other value is a valid unique identifier.
 *
 * Note: facil.io uses unique identifiers to protect sockets from collisions.
 *       However these identifiers can be converted to the underlying file
 *       descriptor using the `fio_uuid2fd` macro.
 */
intptr_t fio_socket(const char *address, const char *port, uint8_t is_server);

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
 * If 0 is returned, an erro might have occured (see `errno`) and the contents
 * of `dest` is undefined.
 */
size_t fio_local_addr(char *dest, size_t limit);

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
ssize_t fio_read(intptr_t uuid, void *buffer, size_t count);

/** The following structure is used for `fio_write2_fn` function arguments. */
typedef struct {
  union {
    /** The in-memory data to be sent. */
    const void *buffer;
    /** The data to be sent, if this is a file. */
    const intptr_t fd;
  } data;
  union {
    /**
     * This deallocation callback will be called when the packet is finished
     * with the buffer.
     *
     * If no deallocation callback is set, `free` (or `close`) will be used.
     *
     * Note: socket library functions MUST NEVER be called by a callback, or a
     * deadlock might occur.
     */
    void (*dealloc)(void *buffer);
    /**
     * This is an alternative deallocation callback accessor (same memory space
     * as `dealloc`) for conveniently setting the file `close` callback.
     *
     * Note: `sock` library functions MUST NEVER be called by a callback, or a
     * deadlock might occur.
     */
    void (*close)(intptr_t fd);
  } after;
  /** The length (size) of the buffer, or the amount of data to be sent from the
   * file descriptor.
   */
  uintptr_t length;
  /** Starting point offset from the buffer or file descriptor's beginning. */
  uintptr_t offset;
  /** The packet will be sent as soon as possible. */
  unsigned urgent : 1;
  /**
   * The data union contains the value of a file descriptor (`int`). i.e.:
   *  `.data.fd = fd` or `.data.buffer = (void*)fd;`
   */
  unsigned is_fd : 1;
  /** for internal use */
  unsigned rsv : 1;
  /** for internal use */
  unsigned rsv2 : 1;
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
 * data will be deallocated according to the `.after.dealloc` function.
 */
#define fio_write2(uuid, ...)                                                  \
  fio_write2_fn(uuid, (fio_write_args_s){__VA_ARGS__})

/** A noop function for fio_write2 in cases not deallocation is required. */
void FIO_DEALLOC_NOOP(void *arg);
#define FIO_CLOSE_NOOP ((void (*)(intptr_t))FIO_DEALLOC_NOOP)

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
// ssize_t fio_write(uintptr_t uuid, void *buffer, size_t legnth);
inline FIO_FUNC ssize_t fio_write(const intptr_t uuid, const void *buffer,
                                  const size_t length) {
  if (!length || !buffer)
    return 0;
  void *cpy = fio_malloc(length);
  if (!cpy)
    return -1;
  memcpy(cpy, buffer, length);
  return fio_write2(uuid, .data.buffer = cpy, .length = length,
                    .after.dealloc = fio_free);
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
inline FIO_FUNC ssize_t fio_sendfile(intptr_t uuid, intptr_t source_fd,
                                     off_t offset, size_t length) {
  return fio_write2(uuid, .data.fd = source_fd, .length = length, .is_fd = 1,
                    .offset = offset);
}

/** a helper macro for sending reference counted core strings. */
#define FIO_STR_SEND_FREE2(str_name, uuid, s)                                  \
  fio_write2(r->sub_data.uuid, .data.buffer = (char *)s,                       \
             .offset = ((char *)s - str_name##_data(s)),                       \
             .length = str_name##_len(s),                                      \
             .after.dealloc = (void (*)(void *))str_name##_free2);

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

/**
 * Convert between a facil.io connection's identifier (uuid) and system's fd.
 */
#define fio_uuid2fd(uuid) ((int)((uintptr_t)uuid >> 8))

/**
 * `fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active
 * `uuid`.
 *
 * If the file descriptor was closed, __it will be registered as open__.
 *
 * If the file descriptor was closed directly (not using `fio_close`) or the
 * closure event hadn't been processed, a false positive will be possible. This
 * is not an issue, since the use of an invalid fd will result in the registry
 * being updated and the fd being closed.
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 */
intptr_t fio_fd2uuid(int fd);

/**
 * `fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active
 * `uuid`.
 *
 * If the file descriptor is marked as closed (wasn't opened / registered with
 * facil.io) the function returns -1;
 *
 * If the file descriptor was closed directly (not using `fio_close`) or the
 * closure event hadn't been processed, a false positive will be possible. This
 * is not an issue, since the use of an invalid fd will result in the registry
 * being updated and the fd being closed.
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 */
intptr_t fio_fd2uuid(int fd);

/* *****************************************************************************
Connection Object Links
***************************************************************************** */

/**
 * Links an object to a connection's lifetime, calling the `on_close` callback
 * once the connection has died.
 *
 * If the `uuid` is invalid, the `on_close` callback will be called immediately.
 *
 * NOTE: the `on_close` callback will be called with high priority. Long tasks
 * should be deferred.
 */
void fio_uuid_link(intptr_t uuid, void *obj, void (*on_close)(void *obj));

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
int fio_uuid_unlink(intptr_t uuid, void *obj);

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
int fio_rw_hook_replace_unsafe(intptr_t uuid, fio_rw_hook_s *rw_hooks,
                               void *udata);

/** The default Read/Write hooks used for system Read/Write (udata == NULL). */
extern const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS;

/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
***************************************************************************** */

/**
OVERRIDE THIS to replace the default `fork` implementation.

Behaves like the system's `fork`.
*/
int fio_fork(void);

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
void *fio_thread_new(void *(*thread_func)(void *), void *arg);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
void fio_thread_free(void *p_thr);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
int fio_thread_join(void *p_thr);

/* *****************************************************************************
Connection Task scheduling
***************************************************************************** */

/**
 * This is used to lock the protocol againste concurrency collisions and
 * concurrent memory deallocation.
 *
 * However, there are three levels of protection that allow non-coliding tasks
 * to protect the protocol object from being deallocated while in use:
 *
 * * `FIO_PR_LOCK_TASK` - a task lock locks might change data owned by the
 *    protocol object. This task is used for tasks such as `on_data`.
 *
 * * `FIO_PR_LOCK_WRITE` - a lock that promises only to use static data (data
 *    that tasks never changes) in order to write to the underlying socket.
 *    This lock is used for tasks such as `on_ready` and `ping`
 *
 * * `FIO_PR_LOCK_STATE` - a lock that promises only to retrieve static data
 *    (data that tasks never changes), performing no actions. This usually
 *    isn't used for client side code (used internally by facil) and is only
 *     meant for very short locks.
 */
enum fio_protocol_lock_e {
  FIO_PR_LOCK_TASK = 0,
  FIO_PR_LOCK_WRITE = 1,
  FIO_PR_LOCK_STATE = 2
};

/** Named arguments for the `fio_defer` function. */
typedef struct {
  /** The type of task to be performed. Defaults to `FIO_PR_LOCK_TASK` but could
   * also be seto to `FIO_PR_LOCK_WRITE`. */
  enum fio_protocol_lock_e type;
  /** The task (function) to be performed. This is required. */
  void (*task)(intptr_t uuid, fio_protocol_s *, void *udata);
  /** An opaque user data that will be passed along to the task. */
  void *udata;
  /** A fallback task, in case the connection was lost. Good for cleanup. */
  void (*fallback)(intptr_t uuid, void *udata);
} fio_defer_iotask_args_s;

/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error ocuurs or the connection is closed before the task can run, the
 * `fallback` task wil be called instead, allowing for resource cleanup.
 */
void fio_defer_io_task(intptr_t uuid, fio_defer_iotask_args_s args);
#define fio_defer_io_task(uuid, ...)                                           \
  fio_defer_io_task((uuid), (fio_defer_iotask_args_s){__VA_ARGS__})

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
 * Creates a timer to run a task at the specified interval.
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error.
 *
 * The `on_finish` handler is always called (even on error).
 */
int fio_run_every(size_t milliseconds, size_t repetitions, void (*task)(void *),
                  void *arg, void (*on_finish)(void *));

/**
 * Performs all deferred tasks.
 */
void fio_defer_perform(void);

/** Returns true if there are deferred functions waiting for execution. */
int fio_defer_has_queue(void);

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
  /** Called after each fork (both in parent and workers). */
  FIO_CALL_AFTER_FORK,
  /** Called by a worker process right after forking. */
  FIO_CALL_IN_CHILD,
  /** Called by the master process after spawning a worker (after forking). */
  FIO_CALL_IN_MASTER,
  /** Called every time a *Worker* proceess starts. */
  FIO_CALL_ON_START,
  /** Called when facil.io enters idling mode. */
  FIO_CALL_ON_IDLE,
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
  /** used for testing. */
  FIO_CALL_NEVER
} callback_type_e;

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e, void (*func)(void *), void *arg);

/** Removes a callback from the list of callbacks to be called for the event. */
int fio_state_callback_remove(callback_type_e, void (*func)(void *), void *arg);

/**
 * Forces all the existing callbacks to run, as if the event occurred.
 *
 * Callbacks are called from last to first (last callback executes first).
 *
 * During an event, changes to the callback list are ignored (callbacks can't
 * remove other callbacks for the same event).
 */
void fio_state_callback_force(callback_type_e);

/** Clears all the existing callbacks for the event. */
void fio_state_callback_clear(callback_type_e);

/* *****************************************************************************
Lower Level API - for special circumstances, use with care.
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
 * facil.io uses three different locks:
 *
 * * FIO_PR_LOCK_TASK locks the protocol for normal tasks (i.e. `on_data`,
 * `fio_defer`, `fio_every`).
 *
 * * FIO_PR_LOCK_WRITE locks the protocol for high priority `fio_write`
 * oriented tasks (i.e. `ping`, `on_ready`).
 *
 * * FIO_PR_LOCK_STATE locks the protocol for quick operations that need to copy
 * data from the protocol's data structure.
 *
 * IMPORTANT: Remember to call `fio_protocol_unlock` using the same lock type.
 *
 * Returns NULL on error (lock busy == EWOULDBLOCK, connection invalid == EBADF)
 * and a pointer to a protocol object on success.
 *
 * On error, consider calling `fio_defer` or `defer` instead of busy waiting.
 * Busy waiting SHOULD be avoided whenever possible.
 */
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid, enum fio_protocol_lock_e);
/** Don't unlock what you don't own... see `fio_protocol_try_lock` for
 * details. */
void fio_protocol_unlock(fio_protocol_s *pr, enum fio_protocol_lock_e);

/* *****************************************************************************
 * Pub/Sub / Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 *
 **************************************************************************** */
#if FIO_PUBSUB_SUPPORT

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
/** Used to publish the message only within the current process. */
#define FIO_PUBSUB_PROCESS ((fio_pubsub_engine_s *)2)
/** Used to publish the message except within the current process. */
#define FIO_PUBSUB_SIBLINGS ((fio_pubsub_engine_s *)3)
/** Used to publish the message exclusively to the root / master process. */
#define FIO_PUBSUB_ROOT ((fio_pubsub_engine_s *)4)

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
   * The actual message.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   *field might be used for internal data.
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
 * Pattern matching callback type - should return 0 unless channel matches
 * pattern.
 */
typedef int (*fio_match_fn)(fio_str_info_s pattern, fio_str_info_s channel);

extern fio_match_fn FIO_MATCH_GLOB;

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
  fio_match_fn match;
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
} subscribe_args_s;

/** Publishing and on_message callback arguments. */
typedef struct fio_publish_args_s {
  /** The pub/sub engine that should be used to forward this message. */
  fio_pubsub_engine_s const *engine;
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** The pub/sub target channnel. */
  fio_str_info_s channel;
  /** The pub/sub message. */
  fio_str_info_s message;
  /** flag indicating if the message is JSON data or binary/text. */
  uint8_t is_json;
} fio_publish_args_s;

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure.
 *
 * See `subscribe_args_s` for details.
 */
subscription_s *fio_subscribe(subscribe_args_s args);
/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure.
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

/** Finds the message's metadata by it's type ID. Returns the data or NULL. */
void *fio_message_metadata(fio_msg_s *msg, intptr_t type_id);

/**
 * Defers the current callback, so it will be called again for the message.
 */
void fio_message_defer(fio_msg_s *msg);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/** Contains message metadata, set by message extensions. */
typedef struct fio_msg_metadata_s fio_msg_metadata_s;
struct fio_msg_metadata_s {
  /**
   * The type ID should be used to identify the metadata's actual structure.
   *
   * Negative ID values are reserved for internal use.
   */
  intptr_t type_id;
  /**
   * This method will be called by facil.io to cleanup the metadata resources.
   *
   * Don't alter / call this method, this data is reserved.
   */
  void (*on_finish)(fio_msg_s *msg, void *metadata);
  /** The pointer to be disclosed to the `fio_message_metadata` function. */
  void *metadata;
};

/**
 * Pub/Sub Metadata callback type.
 */
typedef fio_msg_metadata_s (*fio_msg_metadata_fn)(fio_str_info_s ch,
                                                  fio_str_info_s msg,
                                                  uint8_t is_json);

/**
 * It's possible to attach metadata to facil.io pub/sub messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * The callback should return a valid metadata object. If the `.metadata` field
 * returned is NULL than the result will be ignored.
 *
 * To remove a callback, set the `enable` flag to false (`0`).
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 */
void fio_message_metadata_callback_set(fio_msg_metadata_fn callback,
                                       int enable);

/**
 * facil.io can be linked with external Pub/Sub services using "engines".
 *
 * Only unfiltered messages and subscriptions (where filter == 0) will be
 * forwarded to external Pub/Sub services.
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
 *           .engine = FIO_PROCESS_ENGINE,
 *           .channel = channel_name,
 *           .message = msg_body );
 *
 * IMPORTANT: The `subscribe` and `unsubscribe` callbacks are called from within
 *            an internal lock. They MUST NEVER call pub/sub functions except by
 *            exiting the lock using `fio_defer`.
 */
struct fio_pubsub_engine_s {
  /** Should subscribe channel. Failures are ignored. */
  void (*subscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel,
                    fio_match_fn match);
  /** Should unsubscribe channel. Failures are ignored. */
  void (*unsubscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel,
                      fio_match_fn match);
  /** Should publish a message through the engine. Failures are ignored. */
  void (*publish)(const fio_pubsub_engine_s *eng, fio_str_info_s channel,
                  fio_str_info_s msg, uint8_t is_json);
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

#endif /* FIO_PUBSUB_SUPPORT */

/* *****************************************************************************











              Atomic Operations and Spin Locking Helper Functions











***************************************************************************** */

/**
 * Nanosleep seems to be the most effective and efficient thread rescheduler.
 */
FIO_FUNC inline void fio_reschedule_thread(void);

/** Nanosleep the thread - a blocking throttle. */
FIO_FUNC inline void fio_throttle_thread(size_t nano_sec);

/* *****************************************************************************







                              Hash Functions and Friends







***************************************************************************** */

/* defines the secret seed to be used by keyd hashing functions*/
#ifndef FIO_HASH_SECRET_SEED64_1
uint8_t __attribute__((weak)) fio_hash_secret_marker1;
uint8_t __attribute__((weak)) fio_hash_secret_marker2;
#define FIO_HASH_SECRET_SEED64_1 ((uintptr_t)&fio_hash_secret_marker1)
#define FIO_HASH_SECRET_SEED64_2 ((uintptr_t)&fio_hash_secret_marker2)
#endif

#if FIO_USE_RISKY_HASH
#define FIO_HASH_FN(data, length, key1, key2)                                  \
  fio_risky_hash((data), (length),                                             \
                 ((uint64_t)(key1) >> 19) | ((uint64_t)(key2) << 27))
#else
#define FIO_HASH_FN(data, length, key1, key2)                                  \
  fio_siphash13((data), (length), (uint64_t)(key1), (uint64_t)(key2))
#endif

/* *****************************************************************************
SipHash
***************************************************************************** */

/**
 * A SipHash variation (2-4).
 */
uint64_t fio_siphash24(const void *data, size_t len, uint64_t key1,
                       uint64_t key2);

/**
 * A SipHash 1-3 variation.
 */
uint64_t fio_siphash13(const void *data, size_t len, uint64_t key1,
                       uint64_t key2);

/**
 * The Hashing function used by dynamic facil.io objects.
 *
 * Currently implemented using SipHash 1-3.
 */
#define fio_siphash(data, length, k1, k2)                                      \
  fio_siphash13((data), (length), (k1), (k2))

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
  uint64_t length;
  uint8_t buffer[64];
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
inline FIO_FUNC char *fio_sha1(fio_sha1_s *s, const void *data, size_t len) {
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
  } length;
  uint8_t buffer[128];
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
inline FIO_FUNC char *fio_sha2_512(fio_sha2_s *s, const void *data,
                                   size_t len) {
  *s = fio_sha2_init(SHA_512);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 256 variant.
*/
inline FIO_FUNC char *fio_sha2_256(fio_sha2_s *s, const void *data,
                                   size_t len) {
  *s = fio_sha2_init(SHA_256);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 384 variant.
*/
inline FIO_FUNC char *fio_sha2_384(fio_sha2_s *s, const void *data,
                                   size_t len) {
  *s = fio_sha2_init(SHA_384);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/* *****************************************************************************
Base64 (URL) encoding
***************************************************************************** */

/**
This will encode a byte array (data) of a specified length (len) and
place the encoded data into the target byte buffer (target). The target buffer
MUST have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4)
bytes.

Returns the number of bytes actually written to the target buffer
(including the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.
*/
int fio_base64_encode(char *target, const char *data, int len);

/**
Same as fio_base64_encode, but using Base64URL encoding.
*/
int fio_base64url_encode(char *target, const char *data, int len);

/**
This will decode a Base64 encoded string of a specified length (len) and
place the decoded data into the target byte buffer (target).

The target buffer MUST have enough room for 2 bytes in addition to the expected
data (NUL byte + padding test).

A NUL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer (excluding the NUL byte).

If the target buffer is NUL, the encoded string will be destructively edited
and the decoded data will be placed in the original string's buffer.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3. Hence, the target buffer should
be, at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding
the NUL terminator byte).

Note:
====

The decoder is variation agnostic (will decode Base64, Base64 URL and Base64 XML
variations) and will attempt it's best to ignore invalid data, (in order to
support the MIME Base64 variation in RFC 2045).

This comes at the cost of error
checking, so the encoding isn't validated and invalid input might produce
surprising results.
*/
int fio_base64_decode(char *target, char *encoded, int base64_len);

/* *****************************************************************************
Testing
***************************************************************************** */

#if DEBUG
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

/* *****************************************************************************








                             Memory Allocator Details








***************************************************************************** */

/**
 * This is a custom memory allocator the utilizes memory pools to allow for
 * concurrent memory allocations across threads.
 *
 * Allocated memory is always zeroed out and aligned on a 16 byte boundary.
 *
 * Reallocated memory is always aligned on a 16 byte boundary but it might be
 * filled with junk data after the valid data (this is true also for
 * `fio_realloc2`).
 *
 * The memory allocator assumes multiple concurrent allocation/deallocation,
 * short life spans (memory is freed shortly, but not immediately, after it was
 * allocated) as well as small allocations (realloc almost always copies data).
 *
 * These assumptions allow the allocator to avoid lock contention by ignoring
 * fragmentation within a memory "block" and waiting for the whole "block" to be
 * freed before it's memory is recycled (no per-allocation "free list").
 *
 * An "arena" is allocated per-CPU core during initialization - there's no
 * dynamic allocation of arenas. This allows threads to minimize lock contention
 * by cycling through the arenas until a free arena is detected.
 *
 * There should be a free arena at any given time (statistically speaking) and
 * the thread will only be deferred in the unlikely event in which there's no
 * available arena.
 *
 * By avoiding the "free-list", the need for allocation "headers" is also
 * avoided and allocations are performed with practically zero overhead (about
 * 32 bytes overhead per 32KB memory, that's 1 bit per 1Kb).
 *
 * However, the lack of a "free list" means that memory "leaks" are more
 * expensive and small long-life allocations could cause fragmentation if
 * performed periodically (rather than performed during startup).
 *
 * This allocator should NOT be used for objects with a long life-span, because
 * even a single persistent object will prevent the re-use of the whole memory
 * block from which it was allocated (see FIO_MEMORY_BLOCK_SIZE for size).
 *
 * Some more details:
 *
 * Allocation and deallocations and (usually) managed by "blocks".
 *
 * A memory "block" can include any number of memory pages that are a multiple
 * of 2 (up to 1Mb of memory). However, the default value, set by the value of
 * FIO_MEMORY_BLOCK_SIZE_LOG, is 32Kb (see value at the end of this header).
 *
 * Each block includes a 32 byte header that uses reference counters and
 * position markers (24 bytes are required padding).
 *
 * The block's position marker (`pos`) marks the next available byte (counted in
 * multiples of 16 bytes).
 *
 * The block's reference counter (`ref`) counts how many allocations reference
 * memory in the block (including the "arena" that "owns" the block).
 *
 * Except for the position marker (`pos`) that acts the same as `sbrk`, there's
 * no way to know which "slices" are allocated and which "slices" are available.
 *
 * The allocator uses `mmap` when requesting memory from the system and for
 * allocations bigger than MEMORY_BLOCK_ALLOC_LIMIT (37.5% of the block).
 *
 * Small allocations are differentiated from big allocations by their memory
 * alignment.
 *
 * If a memory allocation is placed 16 bytes after whole block alignment (within
 * a block's padding zone), the memory was allocated directly using `mmap` as a
 * "big allocation". The 16 bytes include an 8 byte header and an 8 byte
 * padding.
 *
 * To replace the system's `malloc` function family compile with the
 * `FIO_OVERRIDE_MALLOC` defined (`-DFIO_OVERRIDE_MALLOC`).
 *
 * When using tcmalloc or jemalloc, it's possible to define `FIO_FORCE_MALLOC`
 * to prevent the facil.io allocator from compiling (`-DFIO_FORCE_MALLOC`).
 */

#ifndef FIO_MEMORY_BLOCK_SIZE_LOG
/**
 * The logarithmic value for a memory block, 15 == 32Kb, 16 == 64Kb, etc'
 *
 * By default, a block of memory is 32Kb silce from an 8Mb allocation.
 *
 * A value of 16 will make this a 64Kb silce from a 16Mb allocation.
 */
#define FIO_MEMORY_BLOCK_SIZE_LOG (15)
#endif

#undef FIO_MEMORY_BLOCK_SIZE
/** The resulting memoru block size, depends on `FIO_MEMORY_BLOCK_SIZE_LOG` */
#define FIO_MEMORY_BLOCK_SIZE ((uintptr_t)1 << FIO_MEMORY_BLOCK_SIZE_LOG)

/**
 * The maximum allocation size, after which `mmap` will be called instead of the
 * facil.io allocator.
 *
 * Defaults to 50% of the block (16Kb), after which `mmap` is used instead
 */
#ifndef FIO_MEMORY_BLOCK_ALLOC_LIMIT
#define FIO_MEMORY_BLOCK_ALLOC_LIMIT (FIO_MEMORY_BLOCK_SIZE >> 1)
#endif

/* *****************************************************************************









                           Spin locking Implementation









***************************************************************************** */

/**
 * Nanosleep seems to be the most effective and efficient thread rescheduler.
 */
FIO_FUNC inline void fio_reschedule_thread(void) {
  const struct timespec tm = {.tv_nsec = 1};
  nanosleep(&tm, NULL);
}

/** Nanosleep the thread - a blocking throttle. */
FIO_FUNC inline void fio_throttle_thread(size_t nano_sec) {
  const struct timespec tm = {.tv_nsec = (nano_sec % 1000000000),
                              .tv_sec = (nano_sec / 1000000000)};
  nanosleep(&tm, NULL);
}

#if DEBUG_SPINLOCK
/** Busy waits for a lock, reports contention. */
FIO_FUNC inline void fio_lock_dbg(fio_lock_i *lock, const char *file,
                                  int line) {
  size_t lock_cycle_count = 0;
  while (fio_trylock(lock)) {
    if (lock_cycle_count >= 8 &&
        (lock_cycle_count == 8 || !(lock_cycle_count & 511)))
      fprintf(stderr, "[DEBUG] fio-spinlock spin %s:%d round %zu\n", file, line,
              lock_cycle_count);
    ++lock_cycle_count;
    fio_reschedule_thread();
  }
  if (lock_cycle_count >= 8)
    fprintf(stderr, "[DEBUG] fio-spinlock spin %s:%d total = %zu\n", file, line,
            lock_cycle_count);
}
#define fio_lock(lock) fio_lock_dbg((lock), __FILE__, __LINE__)

FIO_FUNC inline int fio_trylock_dbg(fio_lock_i *lock, const char *file,
                                    int line) {
  static int last_line = 0;
  static size_t count = 0;
  int result = fio_trylock(lock);
  if (!result) {
    count = 0;
    last_line = 0;
  } else if (line == last_line) {
    ++count;
    if (count >= 2)
      fprintf(stderr, "[DEBUG] trying fio-spinlock %s:%d attempt %zu\n", file,
              line, count);
  } else {
    count = 0;
    last_line = line;
  }
  return result;
}
#define fio_trylock(lock) fio_trylock_dbg((lock), __FILE__, __LINE__)
#endif /* DEBUG_SPINLOCK */

#endif /* H_FACIL_IO_H */

/* *****************************************************************************






                    Memory allocation macros for helper types






***************************************************************************** */

#undef FIO__MALLOC
#undef FIO__CALLOC
#undef FIO__REALLOC
#undef FIO__FREE

#if FIO_FORCE_MALLOC || FIO_FORCE_MALLOC_TMP
#define FIO__MALLOC(size) calloc((size), 1)
#define FIO__CALLOC(size, units) calloc((size), (units))
#define FIO__REALLOC(ptr, new_length, existing_data_length)                    \
  realloc((ptr), (new_length))
#define FIO__FREE free

#else
#define FIO__MALLOC(size) fio_malloc((size))
#define FIO__CALLOC(size, units) fio_calloc((size), (units))
#define FIO__REALLOC(ptr, new_length, existing_data_length)                    \
  fio_realloc2((ptr), (new_length), (existing_data_length))
#define FIO__FREE fio_free
#endif /* FIO_FORCE_MALLOC || FIO_FORCE_MALLOC_TMP */

/* *****************************************************************************






                           Linked List Helpers

        exposes internally used inline helpers for linked lists






***************************************************************************** */

#if !defined(H_FIO_LINKED_LIST_H) && defined(FIO_INCLUDE_LINKED_LIST)

#define H_FIO_LINKED_LIST_H
#undef FIO_INCLUDE_LINKED_LIST
/* *****************************************************************************
Data Structure and Initialization.
***************************************************************************** */

/** an embeded linked list. */
typedef struct fio_ls_embd_s {
  struct fio_ls_embd_s *prev;
  struct fio_ls_embd_s *next;
} fio_ls_embd_s;

/** an independent linked list. */
typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const void *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/* *****************************************************************************
Embedded Linked List API
***************************************************************************** */

/** Adds a node to the list's head. */
FIO_FUNC inline void fio_ls_embd_push(fio_ls_embd_s *dest, fio_ls_embd_s *node);

/** Adds a node to the list's tail. */
FIO_FUNC inline void fio_ls_embd_unshift(fio_ls_embd_s *dest,
                                         fio_ls_embd_s *node);

/** Removes a node from the list's head. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list);

/** Removes a node from the list's tail. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list);

/** Removes a node from the containing node. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node);

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_embd_is_empty(fio_ls_embd_s *list);

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_embd_any(fio_ls_embd_s *list);

/**
 * Iterates through the list using a `for` loop.
 *
 * Access the data with `pos->obj` (`pos` can be named however you please).
 */
#define FIO_LS_EMBD_FOR(list, node)

/**
 * Takes a list pointer `plist` and returns a pointer to it's container.
 *
 * This uses pointer offset calculations and can be used to calculate any
 * struct's pointer (not just list containers) as an offset from a pointer of
 * one of it's members.
 *
 * Very useful.
 */
#define FIO_LS_EMBD_OBJ(type, member, plist)                                   \
  ((type *)((uintptr_t)(plist) - (uintptr_t)(&(((type *)0)->member))))

/* *****************************************************************************
Independent Linked List API
***************************************************************************** */

/** Adds an object to the list's head, returnin's the object's location. */
FIO_FUNC inline fio_ls_s *fio_ls_push(fio_ls_s *pos, const void *obj);

/** Adds an object to the list's tail, returnin's the object's location. */
FIO_FUNC inline fio_ls_s *fio_ls_unshift(fio_ls_s *pos, const void *obj);

/** Removes an object from the list's head. */
FIO_FUNC inline void *fio_ls_pop(fio_ls_s *list);

/** Removes an object from the list's tail. */
FIO_FUNC inline void *fio_ls_shift(fio_ls_s *list);

/** Removes a node from the list, returning the contained object. */
FIO_FUNC inline void *fio_ls_remove(fio_ls_s *node);

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_is_empty(fio_ls_s *list);

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_any(fio_ls_s *list);

/**
 * Iterates through the list using a `for` loop.
 *
 * Access the data with `pos->obj` (`pos` can be named however you please).
 */
#define FIO_LS_FOR(list, pos)

/* *****************************************************************************


                             Linked List Helpers

                               IMPLEMENTATION


***************************************************************************** */

/* *****************************************************************************
Embeded Linked List Implementation
***************************************************************************** */

/** Removes a node from the containing node. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node) {
  if (!node->next || node->next == node) {
    /* never remove the list's head */
    return NULL;
  }
  node->next->prev = node->prev;
  node->prev->next = node->next;
  node->prev = node->next = node;
  return node;
}

/** Adds a node to the list's head. */
FIO_FUNC inline void fio_ls_embd_push(fio_ls_embd_s *dest,
                                      fio_ls_embd_s *node) {
  if (!dest || !node)
    return;
  node->prev = dest->prev;
  node->next = dest;
  dest->prev->next = node;
  dest->prev = node;
}

/** Adds a node to the list's tail. */
FIO_FUNC inline void fio_ls_embd_unshift(fio_ls_embd_s *dest,
                                         fio_ls_embd_s *node) {
  fio_ls_embd_push(dest->next, node);
}

/** Removes a node from the list's head. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list) {
  return fio_ls_embd_remove(list->prev);
}

/** Removes a node from the list's tail. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list) {
  return fio_ls_embd_remove(list->next);
}

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_embd_is_empty(fio_ls_embd_s *list) {
  return list->next == list;
}

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_embd_any(fio_ls_embd_s *list) {
  return list->next != list;
}

#undef FIO_LS_EMBD_FOR
#define FIO_LS_EMBD_FOR(list, node)                                            \
  for (fio_ls_embd_s *node = (list)->next; node != (list); node = node->next)

/* *****************************************************************************
Independent Linked List Implementation
***************************************************************************** */

/** Removes an object from the containing node. */
FIO_FUNC inline void *fio_ls_remove(fio_ls_s *node) {
  if (!node || node->next == node) {
    /* never remove the list's head */
    return NULL;
  }
  const void *ret = node->obj;
  node->next->prev = node->prev;
  node->prev->next = node->next;
  FIO__FREE(node);
  return (void *)ret;
}

/** Adds an object to the list's head. */
FIO_FUNC inline fio_ls_s *fio_ls_push(fio_ls_s *pos, const void *obj) {
  if (!pos)
    return NULL;
  /* prepare item */
  fio_ls_s *item = (fio_ls_s *)FIO__MALLOC(sizeof(*item));
  FIO_ASSERT_ALLOC(item);
  *item = (fio_ls_s){.prev = pos->prev, .next = pos, .obj = obj};
  /* inject item */
  pos->prev->next = item;
  pos->prev = item;
  return item;
}

/** Adds an object to the list's tail. */
FIO_FUNC inline fio_ls_s *fio_ls_unshift(fio_ls_s *pos, const void *obj) {
  return fio_ls_push(pos->next, obj);
}

/** Removes an object from the list's head. */
FIO_FUNC inline void *fio_ls_pop(fio_ls_s *list) {
  return fio_ls_remove(list->prev);
}

/** Removes an object from the list's tail. */
FIO_FUNC inline void *fio_ls_shift(fio_ls_s *list) {
  return fio_ls_remove(list->next);
}

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_is_empty(fio_ls_s *list) {
  return list->next == list;
}

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_any(fio_ls_s *list) { return list->next != list; }

#undef FIO_LS_FOR
#define FIO_LS_FOR(list, pos)                                                  \
  for (fio_ls_s *pos = (list)->next; pos != (list); pos = pos->next)

#endif /* FIO_INCLUDE_LINKED_LIST */

/* *****************************************************************************











                               Set / Hash Map Data-Store











***************************************************************************** */

#ifdef FIO_SET_NAME

/**
 * A simple ordered Set / Hash Map implementation, with a minimal API.
 *
 * A Set is basically a Hash Map where the keys are also the values, it's often
 * used for caching objects.
 *
 * The Set's object type and behavior is controlled by the FIO_SET_OBJ_* marcos.
 *
 * A Hash Map is basically a set where the objects in the Set are key-value
 * couplets and only the keys are tested when searching the Set.
 *
 * To create a Set or a Hash Map, the macro FIO_SET_NAME must be defined. i.e.:
 *
 *         #define FIO_SET_NAME cstr_set
 *         #define FIO_SET_OBJ_TYPE char *
 *         #define FIO_SET_OBJ_COMPARE(k1, k2) (!strcmp((k1), (k2)))
 *         #include <fio.h>
 *
 * To create a Hash Map, rather than a pure Set, the macro FIO_SET_KEY_TYPE must
 * be defined. i.e.:
 *
 *         #define FIO_SET_KEY_TYPE char *
 *
 * This allows the FIO_SET_KEY_* macros to be defined as well. For example:
 *
 *         #define FIO_SET_NAME cstr_hashmap
 *         #define FIO_SET_KEY_TYPE char *
 *         #define FIO_SET_KEY_COMPARE(k1, k2) (!strcmp((k1), (k2)))
 *         #define FIO_SET_OBJ_TYPE char *
 *         #include <fio.h>
 *
 * It's possible to create a number of Set or HasMap types by reincluding the
 * fio.h header. i.e.:
 *
 *
 *         #define FIO_INCLUDE_STR
 *         #include <fio.h> // adds the fio_str_s types and functions
 *
 *         #define FIO_SET_NAME fio_str_set
 *         #define FIO_SET_OBJ_TYPE fio_str_s *
 *         #define FIO_SET_OBJ_COMPARE(k1, k2) (fio_str_iseq((k1), (k2)))
 *         #define FIO_SET_OBJ_COPY(key) fio_str_dup((key))
 *         #define FIO_SET_OBJ_DESTROY(key) fio_str_free2((key))
 *         #include <fio.h> // creates the fio_str_set_s Set and functions
 *
 *         #define FIO_SET_NAME fio_str_hash
 *         #define FIO_SET_KEY_TYPE fio_str_s *
 *         #define FIO_SET_KEY_COMPARE(k1, k2) (fio_str_iseq((k1), (k2)))
 *         #define FIO_SET_KEY_COPY(key) fio_str_dup((key))
 *         #define FIO_SET_KEY_DESTROY(key) fio_str_free2((key))
 *         #define FIO_SET_OBJ_TYPE fio_str_s *
 *         #define FIO_SET_OBJ_COMPARE(k1, k2) (fio_str_iseq((k1), (k2)))
 *         #define FIO_SET_OBJ_COPY(key) fio_str_dup((key))
 *         #define FIO_SET_OBJ_DESTROY(key) fio_str_free2((key))
 *         #include <fio.h> // creates the fio_str_hash_s Hash Map and functions
 *
 * The default integer Hash used is a pointer length type (uintptr_t). This can
 * be changed by defining ALL of the following macros:
 * * FIO_SET_HASH_TYPE              - the type of the hash value.
 * * FIO_SET_HASH2UINTPTR(hash, i)  - converts the hash value to a uintptr_t.
 * * FIO_SET_HASH_COMPARE(h1, h2)   - compares two hash values (1 == equal).
 * * FIO_SET_HASH_INVALID           - an invalid Hash value, all bytes are 0.
 * * FIO_SET_HASH_FORCE             - an always valid Hash value, all bytes 0xFF
 *
 *
 * Note: FIO_SET_HASH_TYPE should, normaly be left alone (uintptr_t is
 *       enough). Also, the hash value 0 is reserved to indicate an empty slot.
 *
 * Note: the FIO_SET_OBJ_COMPARE or the FIO_SET_KEY_COMPARE will be used to
 *       compare against invalid as well as valid objects. Invalid objects have
 *       their bytes all zero. FIO_SET_*_DESTROY should somehow mark them as
 *       invalid.
 *
 * Note: Before freeing the Set, FIO_SET_OBJ_DESTROY will be automatically
 *       called for every existing object.
 */

/* Used for naming functions and types, prefixing FIO_SET_NAME to the name */
#define FIO_NAME_FROM_MACRO_STEP2(name, postfix) name##_##postfix
#define FIO_NAME_FROM_MACRO_STEP1(name, postfix)                               \
  FIO_NAME_FROM_MACRO_STEP2(name, postfix)
#define FIO_NAME(postfix) FIO_NAME_FROM_MACRO_STEP1(FIO_SET_NAME, postfix)

/* Used for naming the `free` function */
#define FIO_NAME_FROM_MACRO_STEP4(name) name##_free
#define FIO_NAME_FROM_MACRO_STEP3(name) FIO_NAME_FROM_MACRO_STEP4(name)
#define FIO_NAME_FREE() FIO_NAME_FROM_MACRO_STEP3(FIO_SET_NAME)

/* The default Set object / value type is `void *` */
#if !defined(FIO_SET_OBJ_TYPE)
#define FIO_SET_OBJ_TYPE void *
#elif !defined(FIO_SET_NO_TEST)
#define FIO_SET_NO_TEST 1
#endif

/* The default Set has opaque objects that can't be compared */
#if !defined(FIO_SET_OBJ_COMPARE)
#define FIO_SET_OBJ_COMPARE(o1, o2) (1)
#endif

/** object copy required? */
#ifndef FIO_SET_OBJ_COPY
#define FIO_SET_OBJ_COPY(dest, obj) ((dest) = (obj))
#endif

/** object destruction required? */
#ifndef FIO_SET_OBJ_DESTROY
#define FIO_SET_OBJ_DESTROY(obj) ((void)0)
#endif

/** test for a pre-defined hash type, must be numerical (i.e. __int128_t)*/
#ifndef FIO_SET_HASH_TYPE
#define FIO_SET_HASH_TYPE uintptr_t
#endif

/** test for a pre-defined hash to integer conversion */
#ifndef FIO_SET_HASH2UINTPTR
#define FIO_SET_HASH2UINTPTR(hash, bits_used)                                  \
  (fio_rrot(hash, bits_used) ^ fio_ct_if2(bits_used, hash, 0))
#endif

/** test for a pre-defined hash to integer conversion */
#ifndef FIO_SET_HASH_FORCE
#define FIO_SET_HASH_FORCE (~(uintptr_t)0)
#endif

/** test for a pre-defined invalid hash value (all bytes are 0) */
#ifndef FIO_SET_HASH_INVALID
#define FIO_SET_HASH_INVALID ((FIO_SET_HASH_TYPE)0)
#endif

/** test for a pre-defined hash comparison */
#ifndef FIO_SET_HASH_COMPARE
#define FIO_SET_HASH_COMPARE(h1, h2) ((h1) == (h2))
#endif

/* Customizable memory management */
#ifndef FIO_SET_REALLOC /* NULL ptr indicates new allocation */
#define FIO_SET_REALLOC(ptr, original_size, new_size, valid_data_length)       \
  FIO__REALLOC((ptr), (new_size), (valid_data_length))
#endif

#ifndef FIO_SET_CALLOC
#define FIO_SET_CALLOC(size, count) FIO__CALLOC((size), (count))
#endif

#ifndef FIO_SET_FREE
#define FIO_SET_FREE(ptr, size) FIO__FREE((ptr))
#endif

/* The maximum number of bins to rotate when (partial/full) collisions occure */
#ifndef FIO_SET_MAX_MAP_SEEK
#define FIO_SET_MAX_MAP_SEEK (96)
#endif

/* The maximum number of full hash collisions that can be consumed */
#ifndef FIO_SET_MAX_MAP_FULL_COLLISIONS
#define FIO_SET_MAX_MAP_FULL_COLLISIONS (96)
#endif

/* Prime numbers are better */
#ifndef FIO_SET_CUCKOO_STEPS
#define FIO_SET_CUCKOO_STEPS 11
#endif

#ifdef FIO_SET_KEY_TYPE
typedef struct {
  FIO_SET_KEY_TYPE key;
  FIO_SET_OBJ_TYPE obj;
} FIO_NAME(couplet_s);

#define FIO_SET_TYPE FIO_NAME(couplet_s)

/** key copy required? */
#ifndef FIO_SET_KEY_COPY
#define FIO_SET_KEY_COPY(dest, obj) ((dest) = (obj))
#endif

/** key destruction required? */
#ifndef FIO_SET_KEY_DESTROY
#define FIO_SET_KEY_DESTROY(obj) ((void)0)
#endif

/* The default Hash Map-Set has will use straight euqality operators */
#ifndef FIO_SET_KEY_COMPARE
#define FIO_SET_KEY_COMPARE(o1, o2) ((o1) == (o2))
#endif

/** Internal macros for object actions in Hash mode */
#define FIO_SET_COMPARE(o1, o2) FIO_SET_KEY_COMPARE((o1).key, (o2).key)
#define FIO_SET_COPY(dest, src)                                                \
  do {                                                                         \
    FIO_SET_OBJ_COPY((dest).obj, (src).obj);                                   \
    FIO_SET_KEY_COPY((dest).key, (src).key);                                   \
  } while (0);
#define FIO_SET_DESTROY(couplet)                                               \
  do {                                                                         \
    FIO_SET_KEY_DESTROY((couplet).key);                                        \
    FIO_SET_OBJ_DESTROY((couplet).obj);                                        \
  } while (0);

#else /* a pure Set, not a Hash Map*/
/** Internal macros for object actions in Set mode */
#define FIO_SET_COMPARE(o1, o2) FIO_SET_OBJ_COMPARE((o1), (o2))
#define FIO_SET_COPY(dest, obj) FIO_SET_OBJ_COPY((dest), (obj))
#define FIO_SET_DESTROY(obj) FIO_SET_OBJ_DESTROY((obj))
#define FIO_SET_TYPE FIO_SET_OBJ_TYPE
#endif

/* *****************************************************************************
Set / Hash Map API
***************************************************************************** */

/** The Set container type. By default: fio_ptr_set_s */
typedef struct FIO_NAME(s) FIO_NAME(s);

#ifndef FIO_SET_INIT
/** Initializes the set */
#define FIO_SET_INIT                                                           \
  { .capa = 0 }
#endif

/** Frees all the objects in the set and deallocates any internal resources. */
FIO_FUNC void FIO_NAME_FREE()(FIO_NAME(s) * set);

#ifdef FIO_SET_KEY_TYPE

/**
 * Locates an object in the Hash Map, if it exists.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC inline FIO_SET_OBJ_TYPE
    FIO_NAME(find)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                   FIO_SET_KEY_TYPE key);

/**
 * Inserts an object to the Hash Map, rehashing if required, returning the new
 * object's location using a pointer.
 *
 * If an object already exists in the Hash Map, it will be destroyed.
 *
 * If `old` is set, the existing object (if any) will be copied to the location
 * pointed to by `old` before it is destroyed.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC inline void FIO_NAME(insert)(FIO_NAME(s) * set,
                                      const FIO_SET_HASH_TYPE hash_value,
                                      FIO_SET_KEY_TYPE key,
                                      FIO_SET_OBJ_TYPE obj,
                                      FIO_SET_OBJ_TYPE *old);

/**
 * Removes an object from the Hash Map, rehashing if required.
 *
 * Returns 0 on success and -1 if the object wasn't found.
 *
 * If `old` is set, the existing object (if any) will be copied to the location
 * pointed to by `old`.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC inline int FIO_NAME(remove)(FIO_NAME(s) * set,
                                     const FIO_SET_HASH_TYPE hash_value,
                                     FIO_SET_KEY_TYPE key,
                                     FIO_SET_OBJ_TYPE *old);

#else

/**
 * Locates an object in the Set, if it exists.
 *
 * NOTE: This is the function's pure Set variant (no FIO_SET_KEY_TYPE).
 */
FIO_FUNC inline FIO_SET_OBJ_TYPE
    FIO_NAME(find)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                   FIO_SET_OBJ_TYPE obj);

/**
 * Inserts an object to the Set only if it's missing, rehashing if required,
 * returning the new (or old) object.
 *
 * If the object already exists in the set, than the new object will be
 * destroyed and the old object will be returned.
 *
 * NOTE: This is the function's pure Set variant (no FIO_SET_KEY_TYPE).
 */
FIO_FUNC inline FIO_SET_OBJ_TYPE
    FIO_NAME(insert)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                     FIO_SET_OBJ_TYPE obj);

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 *
 * When setting `old` to NULL, the function behaves the same as `overwrite`.
 */
FIO_FUNC FIO_SET_OBJ_TYPE
    FIO_NAME(overwrite)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                        FIO_SET_OBJ_TYPE obj, FIO_SET_OBJ_TYPE *old);

/**
 * Removes an object from the Set, rehashing if required.
 *
 * Returns 0 on success and -1 if the object wasn't found.
 *
 * NOTE: This is the function's pure Set variant (no FIO_SET_KEY_TYPE).
 */
FIO_FUNC inline int FIO_NAME(remove)(FIO_NAME(s) * set,
                                     const FIO_SET_HASH_TYPE hash_value,
                                     FIO_SET_OBJ_TYPE obj,
                                     FIO_SET_OBJ_TYPE *old);

#endif
/**
 * Allows a peak at the Set's last element.
 *
 * Remember that objects might be destroyed if the Set is altered
 * (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).
 */
FIO_FUNC inline FIO_SET_TYPE FIO_NAME(last)(FIO_NAME(s) * set);

/**
 * Allows the Hash to be momentarily used as a stack, destroying the last
 * object added (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).
 */
FIO_FUNC inline void FIO_NAME(pop)(FIO_NAME(s) * set);

/** Returns the number of object currently in the Set. */
FIO_FUNC inline size_t FIO_NAME(count)(const FIO_NAME(s) * set);

/**
 * Returns a temporary theoretical Set capacity.
 * This could be used for testing performance and memory consumption.
 */
FIO_FUNC inline size_t FIO_NAME(capa)(const FIO_NAME(s) * set);

/**
 * Requires that a Set contains the minimal requested theoretical capacity.
 *
 * Returns the actual (temporary) theoretical capacity.
 */
FIO_FUNC inline size_t FIO_NAME(capa_require)(FIO_NAME(s) * set,
                                              size_t min_capa);

/**
 * Returns non-zero if the Set is fragmented (more than 50% holes).
 */
FIO_FUNC inline size_t FIO_NAME(is_fragmented)(const FIO_NAME(s) * set);

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Set.
 *
 * Returns the updated Set capacity.
 */
FIO_FUNC inline size_t FIO_NAME(compact)(FIO_NAME(s) * set);

/** Forces a rehashing of the Set. */
FIO_FUNC void FIO_NAME(rehash)(FIO_NAME(s) * set);

#ifndef FIO_SET_FOR_LOOP
/**
 * A macro for a `for` loop that iterates over all the Set's objects (in
 * order).
 *
 * `set` is a pointer to the Set variable and `pos` is a temporary variable
 * name to be created for iteration.
 *
 * `pos->hash` is the hashing value and `pos->obj` is the object's data.
 *
 * NOTICE: Since the Set might have "holes" (objects that were removed), it is
 * important to skip any `pos->hash == 0` or the equivalent of
 * `FIO_SET_HASH_COMPARE(pos->hash, FIO_SET_HASH_INVALID)`.
 */
#define FIO_SET_FOR_LOOP(set, pos)
#endif

/* *****************************************************************************
Set / Hash Map Internal Data Structures
***************************************************************************** */

typedef struct FIO_NAME(_ordered_s_) {
  FIO_SET_HASH_TYPE hash;
  FIO_SET_TYPE obj;
} FIO_NAME(_ordered_s_);

typedef struct FIO_NAME(_map_s_) {
  FIO_SET_HASH_TYPE hash; /* another copy for memory cache locality */
  FIO_NAME(_ordered_s_) * pos;
} FIO_NAME(_map_s_);

/* the information in the Hash Map structure should be considered READ ONLY. */
struct FIO_NAME(s) {
  uintptr_t count;
  uintptr_t capa;
  uintptr_t pos;
  FIO_NAME(_ordered_s_) * ordered;
  FIO_NAME(_map_s_) * map;
  uint8_t has_collisions;
  uint8_t used_bits;
  uint8_t under_attack;
};

#undef FIO_SET_FOR_LOOP
#define FIO_SET_FOR_LOOP(set, container)                                       \
  for (__typeof__((set)->ordered) container = (set)->ordered;                  \
       container && (container < ((set)->ordered + (set)->pos)); ++container)

/* *****************************************************************************
Set / Hash Map Internal Helpers
***************************************************************************** */

/** Locates an object's map position in the Set, if it exists. */
FIO_FUNC inline FIO_NAME(_map_s_) *
    FIO_NAME(_find_map_pos_)(FIO_NAME(s) * set, FIO_SET_HASH_TYPE hash_value,
                             FIO_SET_TYPE obj) {
  if (FIO_SET_HASH_COMPARE(hash_value, FIO_SET_HASH_INVALID))
    hash_value = FIO_SET_HASH_FORCE;
  if (set->map) {
    /* make sure collisions don't effect seeking */
    if (set->has_collisions && set->pos != set->count) {
      FIO_NAME(rehash)(set);
    }
    size_t full_collisions_counter = 0;
    FIO_NAME(_map_s_) * pos;
    /*
     * Commonly, the hash is rotated, depending on it's state.
     * Different bits are used for each mapping, instead of a single new bit.
     */
    const uintptr_t mask = (1ULL << set->used_bits) - 1;

    uintptr_t i;
    const uintptr_t hash_value_i = FIO_SET_HASH2UINTPTR(hash_value, 0);
    uintptr_t hash_alt = FIO_SET_HASH2UINTPTR(hash_value, set->used_bits);

    /* O(1) access to object */
    pos = set->map + (hash_alt & mask);
    if (FIO_SET_HASH_COMPARE(FIO_SET_HASH_INVALID, pos->hash))
      return pos;
    if (FIO_SET_HASH_COMPARE(pos->hash, hash_value_i)) {
      if (!pos->pos || (FIO_SET_COMPARE(pos->pos->obj, obj)))
        return pos;
      /* full hash value collision detected */
      set->has_collisions = 1;
      ++full_collisions_counter;
    }

    /* Handle partial / full collisions with cuckoo steps O(x) access time */
    i = 0;
    const uintptr_t limit =
        FIO_SET_CUCKOO_STEPS * (set->capa > (FIO_SET_MAX_MAP_SEEK << 2)
                                    ? FIO_SET_MAX_MAP_SEEK
                                    : (set->capa >> 2));
    while (i < limit) {
      i += FIO_SET_CUCKOO_STEPS;
      pos = set->map + ((hash_alt + i) & mask);
      if (FIO_SET_HASH_COMPARE(FIO_SET_HASH_INVALID, pos->hash))
        return pos;
      if (FIO_SET_HASH_COMPARE(pos->hash, hash_value_i)) {
        if (!pos->pos || (FIO_SET_COMPARE(pos->pos->obj, obj)))
          return pos;
        /* full hash value collision detected */
        set->has_collisions = 1;
        if (++full_collisions_counter >= FIO_SET_MAX_MAP_FULL_COLLISIONS) {
          /* is the hash under attack? */
          FIO_LOG_WARNING(
              "(fio hash map) too many full collisions - under attack?");
          set->under_attack = 1;
        }
        if (set->under_attack) {
          return pos;
        }
      }
    }
  }
  return NULL;
  (void)obj; /* in cases where FIO_SET_OBJ_COMPARE does nothing */
}
#undef FIO_SET_CUCKOO_STEPS

/** Removes "holes" from the Set's internal Array - MUST re-hash afterwards.
 */
FIO_FUNC inline void FIO_NAME(_compact_ordered_array_)(FIO_NAME(s) * set) {
  if (set->count == set->pos)
    return;
  FIO_NAME(_ordered_s_) *reader = set->ordered;
  FIO_NAME(_ordered_s_) *writer = set->ordered;
  const FIO_NAME(_ordered_s_) *end = set->ordered + set->pos;
  for (; reader && (reader < end); ++reader) {
    if (FIO_SET_HASH_COMPARE(reader->hash, FIO_SET_HASH_INVALID)) {
      continue;
    }
    *writer = *reader;
    ++writer;
  }
  /* fix any possible counting errors as well as resetting position */
  set->pos = set->count = (writer - set->ordered);
}

/** (Re)allocates the set's internal, invalidatint the mapping (must rehash) */
FIO_FUNC inline void FIO_NAME(_reallocate_set_mem_)(FIO_NAME(s) * set) {
  const uintptr_t new_capa = 1ULL << set->used_bits;
  FIO_SET_FREE(set->map, set->capa * sizeof(*set->map));
  set->map = (FIO_NAME(_map_s_) *)FIO_SET_CALLOC(sizeof(*set->map), new_capa);
  set->ordered = (FIO_NAME(_ordered_s_) *)FIO_SET_REALLOC(
      set->ordered, (set->capa * sizeof(*set->ordered)),
      (new_capa * sizeof(*set->ordered)), (set->pos * sizeof(*set->ordered)));
  if (!set->map || !set->ordered) {
    perror("FATAL ERROR: couldn't allocate memory for Set data");
    exit(errno);
  }
  set->capa = new_capa;
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 */
FIO_FUNC inline FIO_SET_TYPE
FIO_NAME(_insert_or_overwrite_)(FIO_NAME(s) * set, FIO_SET_HASH_TYPE hash_value,
                                FIO_SET_TYPE obj, int overwrite,
                                FIO_SET_OBJ_TYPE *old) {
  if (FIO_SET_HASH_COMPARE(hash_value, FIO_SET_HASH_INVALID))
    hash_value = FIO_SET_HASH_FORCE;

  /* automatic fragmentation protection */
  if (FIO_NAME(is_fragmented)(set))
    FIO_NAME(rehash)(set);
  /* automatic capacity validation (we can never be at 100% capacity) */
  else if (set->pos >= set->capa) {
    ++set->used_bits;
    FIO_NAME(rehash)(set);
  }

  /* locate future position */
  FIO_NAME(_map_s_) *pos = FIO_NAME(_find_map_pos_)(set, hash_value, obj);

  if (!pos) {
    /* inserting a new object, with too many holes in the map */
    FIO_SET_COPY(set->ordered[set->pos].obj, obj);
    set->ordered[set->pos].hash = hash_value;
    ++set->pos;
    ++set->count;
    FIO_NAME(rehash)(set);
    return set->ordered[set->pos - 1].obj;
  }

  /* overwriting / new */
  if (pos->pos) {
    /* overwrite existing object */
    if (!overwrite) {
      FIO_SET_DESTROY(obj);
      return pos->pos->obj;
    }
#ifdef FIO_SET_KEY_TYPE
    if (old) {
      FIO_SET_OBJ_COPY((*old), pos->pos->obj.obj);
    }
    /* no need to recreate the key object, just the value object */
    FIO_SET_OBJ_DESTROY(pos->pos->obj.obj);
    FIO_SET_OBJ_COPY(pos->pos->obj.obj, obj.obj);
    return pos->pos->obj;
#else
    if (old) {
      FIO_SET_COPY((*old), pos->pos->obj);
    }
    FIO_SET_DESTROY(pos->pos->obj);
#endif
  } else {
    /* insert into new slot */
    pos->pos = set->ordered + set->pos;
    ++set->pos;
    ++set->count;
  }
  /* store object at position */
  pos->hash = hash_value;
  pos->pos->hash = hash_value;
  FIO_SET_COPY(pos->pos->obj, obj);

  return pos->pos->obj;
}

/* *****************************************************************************
Set / Hash Map Implementation
***************************************************************************** */

/** Frees all the objects in the set and deallocates any internal resources. */
FIO_FUNC void FIO_NAME_FREE()(FIO_NAME(s) * s) {
  /* destroy existing valid objects */
  const FIO_NAME(_ordered_s_) *const end = s->ordered + s->pos;
  if (s->ordered && s->ordered != end) {
    for (FIO_NAME(_ordered_s_) *pos = s->ordered; pos < end; ++pos) {
      if (!FIO_SET_HASH_COMPARE(FIO_SET_HASH_INVALID, pos->hash)) {
        FIO_SET_DESTROY(pos->obj);
      }
    }
  }
  /* free ordered array and hash mapping */
  FIO_SET_FREE(s->map, s->capa * sizeof(*s->map));
  FIO_SET_FREE(s->ordered, s->capa * sizeof(*s->ordered));
  *s = (FIO_NAME(s)){.map = NULL};
}

#ifdef FIO_SET_KEY_TYPE

/* Hash Map unique implementation */

/**
 * Locates an object in the Set, if it exists.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC FIO_SET_OBJ_TYPE FIO_NAME(find)(FIO_NAME(s) * set,
                                         const FIO_SET_HASH_TYPE hash_value,
                                         FIO_SET_KEY_TYPE key) {
  FIO_NAME(_map_s_) *pos =
      FIO_NAME(_find_map_pos_)(set, hash_value, (FIO_SET_TYPE){.key = key});
  if (!pos || !pos->pos) {
    FIO_SET_OBJ_TYPE empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
  }
  return pos->pos->obj.obj;
}

/**
 * Inserts an object to the Hash Map, rehashing if required, returning the new
 * object's location using a pointer.
 *
 * If an object already exists in the Hash Map, it will be destroyed.
 *
 * If `old` is set, the existing object (if any) will be copied to the location
 * pointed to by `old` before it is destroyed.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC void FIO_NAME(insert)(FIO_NAME(s) * set,
                               const FIO_SET_HASH_TYPE hash_value,
                               FIO_SET_KEY_TYPE key, FIO_SET_OBJ_TYPE obj,
                               FIO_SET_OBJ_TYPE *old) {
  FIO_NAME(_insert_or_overwrite_)
  (set, hash_value, (FIO_SET_TYPE){.key = key, .obj = obj}, 1, old);
}

/**
 * Removes an object from the Hash Map, rehashing if required.
 *
 * Returns 0 on success and -1 if the object wasn't found.
 *
 * If `old` is set, the existing object (if any) will be copied to the location
 * pointed to by `old`.
 *
 * NOTE: This is the function's Hash Map variant. See FIO_SET_KEY_TYPE.
 */
FIO_FUNC inline int FIO_NAME(remove)(FIO_NAME(s) * set,
                                     const FIO_SET_HASH_TYPE hash_value,
                                     FIO_SET_KEY_TYPE key,
                                     FIO_SET_OBJ_TYPE *old) {
  FIO_NAME(_map_s_) *pos =
      FIO_NAME(_find_map_pos_)(set, hash_value, (FIO_SET_TYPE){.key = key});
  if (!pos || !pos->pos)
    return -1;
  if (old)
    FIO_SET_OBJ_COPY((*old), pos->pos->obj.obj);
  FIO_SET_DESTROY(pos->pos->obj);
  --set->count;
  pos->pos->hash = FIO_SET_HASH_INVALID;
  if (pos->pos == set->pos + set->ordered - 1) {
    /* removing last item inserted */
    pos->hash = FIO_SET_HASH_INVALID; /* no need for a "hole" */
    do {
      --set->pos;
    } while (set->pos && FIO_SET_HASH_COMPARE(set->ordered[set->pos - 1].hash,
                                              FIO_SET_HASH_INVALID));
  }
  pos->pos = NULL; /* leave pos->hash set to mark "hole" */
  return 0;
}

#else /* FIO_SET_KEY_TYPE */

/* Set unique implementation */

/** Locates an object in the Set, if it exists. */
FIO_FUNC FIO_SET_OBJ_TYPE FIO_NAME(find)(FIO_NAME(s) * set,
                                         const FIO_SET_HASH_TYPE hash_value,
                                         FIO_SET_OBJ_TYPE obj) {
  FIO_NAME(_map_s_) *pos = FIO_NAME(_find_map_pos_)(set, hash_value, obj);
  if (!pos || !pos->pos) {
    FIO_SET_OBJ_TYPE empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
  }
  return pos->pos->obj;
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, than the new object will be
 * destroyed and the old object's address will be returned.
 */
FIO_FUNC FIO_SET_OBJ_TYPE FIO_NAME(insert)(FIO_NAME(s) * set,
                                           const FIO_SET_HASH_TYPE hash_value,
                                           FIO_SET_OBJ_TYPE obj) {
  return FIO_NAME(_insert_or_overwrite_)(set, hash_value, obj, 0, NULL);
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 *
 * When setting `old` to NULL, the function behaves the same as `overwrite`.
 */
FIO_FUNC FIO_SET_OBJ_TYPE
FIO_NAME(overwrite)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                    FIO_SET_OBJ_TYPE obj, FIO_SET_OBJ_TYPE *old) {
  return FIO_NAME(_insert_or_overwrite_)(set, hash_value, obj, 1, old);
}

/**
 * Removes an object from the Set, rehashing if required.
 */
FIO_FUNC int FIO_NAME(remove)(FIO_NAME(s) * set,
                              const FIO_SET_HASH_TYPE hash_value,
                              FIO_SET_OBJ_TYPE obj, FIO_SET_OBJ_TYPE *old) {
  if (FIO_SET_HASH_COMPARE(hash_value, FIO_SET_HASH_INVALID))
    return -1;
  FIO_NAME(_map_s_) *pos = FIO_NAME(_find_map_pos_)(set, hash_value, obj);
  if (!pos || !pos->pos)
    return -1;
  if (old)
    FIO_SET_COPY((*old), pos->pos->obj);
  FIO_SET_DESTROY(pos->pos->obj);
  --set->count;
  pos->pos->hash = FIO_SET_HASH_INVALID;
  if (pos->pos == set->pos + set->ordered - 1) {
    /* removing last item inserted */
    pos->hash = FIO_SET_HASH_INVALID; /* no need for a "hole" */
    do {
      --set->pos;
    } while (set->pos && FIO_SET_HASH_COMPARE(set->ordered[set->pos - 1].hash,
                                              FIO_SET_HASH_INVALID));
  }
  pos->pos = NULL; /* leave pos->hash set to mark "hole" */
  return 0;
}

#endif

/**
 * Allows a peak at the Set's last element.
 *
 * Remember that objects might be destroyed if the Set is altered
 * (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).
 */
FIO_FUNC inline FIO_SET_TYPE FIO_NAME(last)(FIO_NAME(s) * set) {
  if (!set->ordered || !set->pos) {
    FIO_SET_TYPE empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
  }
  return set->ordered[set->pos - 1].obj;
}

/**
 * Allows the Hash to be momentarily used as a stack, destroying the last
 * object added (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).
 */
FIO_FUNC void FIO_NAME(pop)(FIO_NAME(s) * set) {
  if (!set->ordered || !set->pos)
    return;
  FIO_SET_DESTROY(set->ordered[set->pos - 1].obj);
  set->ordered[set->pos - 1].hash = FIO_SET_HASH_INVALID;
  --(set->count);
  do {
    --(set->pos);
  } while (set->pos && FIO_SET_HASH_COMPARE(set->ordered[set->pos - 1].hash,
                                            FIO_SET_HASH_INVALID));
}

/** Returns the number of objects currently in the Set. */
FIO_FUNC inline size_t FIO_NAME(count)(const FIO_NAME(s) * set) {
  return (size_t)set->count;
}

/**
 * Returns a temporary theoretical Set capacity.
 * This could be used for testing performance and memory consumption.
 */
FIO_FUNC inline size_t FIO_NAME(capa)(const FIO_NAME(s) * set) {
  return (size_t)set->capa;
}

/**
 * Requires that a Set contains the minimal requested theoretical capacity.
 *
 * Returns the actual (temporary) theoretical capacity.
 */
FIO_FUNC inline size_t FIO_NAME(capa_require)(FIO_NAME(s) * set,
                                              size_t min_capa) {
  if (min_capa <= FIO_NAME(capa)(set))
    return FIO_NAME(capa)(set);
  set->used_bits = 2;
  while (min_capa > (1ULL << set->used_bits)) {
    ++set->used_bits;
  }
  FIO_NAME(rehash)(set);
  return FIO_NAME(capa)(set);
}

/**
 * Returns non-zero if the Set is fragmented (more than 50% holes).
 */
FIO_FUNC inline size_t FIO_NAME(is_fragmented)(const FIO_NAME(s) * set) {
  return ((set->pos - set->count) > (set->count >> 1));
}

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Set.
 *
 * Returns the updated Set capacity.
 */
FIO_FUNC inline size_t FIO_NAME(compact)(FIO_NAME(s) * set) {
  FIO_NAME(_compact_ordered_array_)(set);
  set->used_bits = 2;
  while (set->count >= (1ULL << set->used_bits)) {
    ++set->used_bits;
  }
  FIO_NAME(rehash)(set);
  return FIO_NAME(capa)(set);
}

/** Forces a rehashing of the Set. */
FIO_FUNC void FIO_NAME(rehash)(FIO_NAME(s) * set) {
  FIO_NAME(_compact_ordered_array_)(set);
  set->has_collisions = 0;
  uint8_t attempts = 0;
restart:
  if (set->used_bits >= 16 && ++attempts >= 3 && set->has_collisions) {
    FIO_LOG_FATAL(
        "facil.io Set / Hash Map has too many collisions (%zu/%zu)."
        "\n\t\tthis is a fatal implementation error,"
        "please report this issue at facio.io's open source project"
        "\n\t\tNote: hash maps and sets should never reach this point."
        "\n\t\tThey should be guarded against collision attacks.",
        set->pos, set->capa);
    exit(-1);
  }
  FIO_NAME(_reallocate_set_mem_)(set);
  {
    FIO_NAME(_ordered_s_) const *const end = set->ordered + set->pos;
    for (FIO_NAME(_ordered_s_) *pos = set->ordered; pos < end; ++pos) {
      FIO_NAME(_map_s_) *mp =
          FIO_NAME(_find_map_pos_)(set, pos->hash, pos->obj);
      if (!mp) {
        ++set->used_bits;
        goto restart;
      }
      mp->pos = pos;
      mp->hash = pos->hash;
    }
  }
}

#undef FIO_SET_OBJ_TYPE
#undef FIO_SET_OBJ_COMPARE
#undef FIO_SET_OBJ_COPY
#undef FIO_SET_OBJ_DESTROY
#undef FIO_SET_HASH_TYPE
#undef FIO_SET_HASH2UINTPTR
#undef FIO_SET_HASH_COMPARE
#undef FIO_SET_HASH_INVALID
#undef FIO_SET_KEY_TYPE
#undef FIO_SET_KEY_COPY
#undef FIO_SET_KEY_DESTROY
#undef FIO_SET_KEY_COMPARE
#undef FIO_SET_TYPE
#undef FIO_SET_COMPARE
#undef FIO_SET_COPY
#undef FIO_SET_DESTROY
#undef FIO_SET_MAX_MAP_SEEK
#undef FIO_SET_MAX_MAP_FULL_COLLISIONS
#undef FIO_SET_REALLOC
#undef FIO_SET_CALLOC
#undef FIO_SET_FREE
#undef FIO_NAME
#undef FIO_NAME_FROM_MACRO_STEP2
#undef FIO_NAME_FROM_MACRO_STEP1
#undef FIO_NAME_FROM_MACRO_STEP4
#undef FIO_NAME_FROM_MACRO_STEP3
#undef FIO_NAME_FREE
#undef FIO_SET_NAME
#undef FIO_FORCE_MALLOC_TMP

#endif
