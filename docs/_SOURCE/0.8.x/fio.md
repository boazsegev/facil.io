---
title: facil.io - 0.8.x Core IO Library Documentation
sidebar: 0.8.x/_sidebar.md
---
# facil.io - 0.8.x Core IO Library Documentation

## Lower Level API Notice

>> **The core IO library is probably not the API most developers need to focus on** (although it's always good to know).
>>
>> This API is used to power the higher level API offered by the [HTTP / WebSockts extension](./http) and the [dynamic FIOBJ types](./fiobj).

## Overview

The core IO library follows an evented design and uses callbacks for IO events. Using the API described in the [Connection Management section](#connection-protocol-management):

- Each connection / socket, is identified by a process unique number (`uuid`).

- Connections are assigned protocol objects (`fio_protocol_s`) using the `fio_attach` function.

- The callbacks in the protocol object are called whenever an IO event occurs.

- Callbacks are protected using one of two connection bound locks - `FIO_PR_LOCK_TASK` for most tasks and `FIO_PR_LOCK_WRITE` for `on_ready` and `on_timeout` tasks.

   This makes it easy to handle multi-threading. Connection data that is mutated only by the connection itself and within the same type of protected tasks, is safe from corruption.

- User data is assumed to be stored in the protocol object using C style inheritance.

   This approach improves performance by avoiding a `udata` pointer where possible. This saves a layer of redirection that often results in increased CPU cache misses.

   The most convenient approach would [place the `fio_protocol_s` object as the first element in a larger `struct`](https://github.com/boazsegev/facil.io/blob/d4ee13109736e6df7548744ab05efeb9aa645614/examples/raw-http.c#L55-L56) and simply [type-cast  the larger `struct`'s pointer](https://github.com/boazsegev/facil.io/blob/d4ee13109736e6df7548744ab05efeb9aa645614/examples/raw-http.c#L264-L266).

[Reading and writing operations](#reading-writing) use an internal user-land buffer and they never fail... unless, the client is so slow that they appear to be attacking the network layer (slowloris) the connection was lost due to other reasons or the system is out of memory.

Because the framework is evented, there's API that offers easy [event and task scheduling](#event-task-scheduling), including timers etc'. Also, connection events can be rescheduled, allowing connections to behave like state-machines.

The core library includes [Pub/Sub (publish / subscribe) services](pub-sub-services) which offer easy IPC (inter process communication) in a network friendly API. Pub/Sub services can be extended to synchronize with external databases such as Redis.

## Examples

The `tests` folder contains example code as well as code used for testing. This is true for the [C STL library](https://github.com/facil-io/cstl/tree/master/tests), the IO core library and the extensions a well.

## Required files

The core IO library functions are declared at `fio.h` header and defined in the `fio.c` implementation file. The core IO library requires the `fio-stl.h` header.

The headers should be well documented and the code is usually easy to read. I also do my best to keep this documentation up to date.

-------------------------------------------------------------------------------
## Starting the IO reactor and reviewing it's state

#### `fio_start`

```c
struct fio_start_args {
  int16_t threads;
  int16_t workers;
};
void fio_start(struct fio_start_args args);
#define fio_start(...) fio_start((struct fio_start_args){__VA_ARGS__})
```

The number of threads to run in the thread pool. Has "smart" defaults.


A positive value will indicate a set number of threads (or workers).

Zeros and negative values are fun and include an interesting shorthand:

* Negative values indicate a fraction of the number of CPU cores. i.e. -2 will normally indicate "half" (1/2) the number of cores.

* If the other option (i.e. `.workers` when setting `.threads`) is zero, it will be automatically updated to reflect the option's absolute value.

  i.e.: if `.threads == -2` and `.workers == 0`, than facil.io will run 2 worker processes with (cores/2) threads per process.


Starts the facil.io event loop. This function will return after facil.io is done (after shutdown).

See the `struct fio_start_args` details for any possible named arguments.

This method blocks the current thread until the server is stopped (when a SIGINT/SIGTERM is received).

#### `fio_stop`

```c
void fio_stop(void);
```

Attempts to stop the facil.io application. This only works within the Root process. A worker process will simply re-spawn itself.

#### `fio_last_tick`

```c
struct timespec fio_last_tick(void);
```

Returns the last time the server reviewed any pending IO events.

#### `fio_engine`

```c
char const *fio_engine(void);
```

Returns a C string detailing the IO engine selected during compilation.

Valid values are `"kqueue"`, `"epoll"` and `"poll"`.

#### `fio_expected_concurrency`

```c
void fio_expected_concurrency(int16_t *threads,
                              int16_t *workers);
```

Returns the number of expected threads / processes to be used by facil.io.

The pointers should start with valid values that match the expected threads / processes values passed to `fio_start`.

The data in the pointers will be overwritten with the result.

#### `fio_is_running`

```c
int16_t fio_is_running(void);
```
Returns the number of worker processes if facil.io is running.

1 is returned when in single process mode, otherwise the number of workers is returned.

#### `fio_is_worker`

```c
int fio_is_worker(void);
```

Returns 1 if the current process is a worker process or a single process.

Otherwise returns 0.

**Note**: When cluster mode is off, the root process is also the worker process. This means that single process instances don't automatically re-spawn after critical errors.

#### `fio_is_master`

```c
int fio_is_master(void);
```

Returns 1 if the current process is the master (root) process.

Otherwise returns 0.

#### `fio_master_pid`

```c
pid_t fio_master_pid(void);
```

Returns facil.io's parent (root) process pid. 

#### `fio_reap_children`

```c
void fio_reap_children(void);
```

Initializes zombie reaping for the process. Call before `fio_start` to enable global zombie reaping. 

#### `fio_signal_handler_reset`
```c
void fio_signal_handler_reset(void);
```

Resets any existing signal handlers, restoring their state to before they were set by facil.io.

This stops both child reaping (`fio_reap_children`) and the default facil.io signal handlers (i.e., CTRL-C).

This function will be called automatically by facil.io whenever facil.io stops.


## Startup / State Callbacks (fork, start up, idle, etc')


```c
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
```

#### `fio_state_callback_add`

```c
void fio_state_callback_add(callback_type_e, void (*func)(void *), void *arg);
```

Adds a callback to the list of callbacks to be called for the event.

 * `FIO_CALL_ON_INITIALIZE`: Called once during library initialization.
 
 * `FIO_CALL_PRE_START`: Called once before starting up the IO reactor.
 
 * `FIO_CALL_BEFORE_FORK`: Called before each time the IO reactor forks a new worker.
 
 * `FIO_CALL_AFTER_FORK`: Called after each fork (in both parent and child), before `FIO_CALL_IN_XXX`.
 
 * `FIO_CALL_IN_CHILD`: Called by a worker process right after forking (and after `FIO_CALL_AFTER_FORK`).
 
 * `FIO_CALL_IN_MASTER`: Called by the root / master process after forking (and after `FIO_CALL_AFTER_FORK`).

 * `FIO_CALL_ON_START`: Called every time a *Worker* proceess starts.
 
 * `FIO_CALL_ON_PUBSUB_CONNECT`: **SHOULD** be called by pub/sub engines after they (re)connect to backend.

 * `FIO_CALL_ON_PUBSUB_ERROR`: **SHOULD** be called by pub/sub engines for backend connection errors.

 * `FIO_CALL_ON_USR`: User state event queue (unused, available to the user).

 * `FIO_CALL_ON_IDLE`: Called when facil.io enters idling mode.

 * `FIO_CALL_ON_USR_REVERSE`: A reversed user state event queue (unused, available to the user).
 
 * `FIO_CALL_ON_SHUTDOWN`: Called before starting the shutdown sequence.
 
 * `FIO_CALL_ON_FINISH`: Called just before finishing up (both on chlid and parent processes).
 
 * `FIO_CALL_ON_PARENT_CRUSH`: Called by each worker the moment it detects the master process crashed.
 
 * `FIO_CALL_ON_CHILD_CRUSH`: Called by the parent (master) after a worker process crashed.
 
 * `FIO_CALL_AT_EXIT`: An alternative to the system's at_exit.
 
Callbacks will be called using logical order for build-up and tear-down.

During initialization related events, FIFO will be used (first in/scheduled, first out/executed).

During shut-down related tasks, LIFO will be used (last in/scheduled, first out/executed).

Idling callbacks are scheduled rather than performed during the event, so they might be performed out of order or concurrently when running in multi-threaded mode.

#### `fio_state_callback_remove`

```c
int fio_state_callback_remove(callback_type_e, void (*func)(void *), void *arg);
```

Removes a callback from the list of callbacks to be called for the event.

#### `fio_state_callback_force`

```c
void fio_state_callback_force(callback_type_e);
```

Forces all the existing callbacks to run, as if the event occurred.

Callbacks for all initialization / idling tasks are called in order of creation (where `callback_type_e <= FIO_CALL_ON_IDLE`).

Callbacks for all cleanup oriented tasks are called in reverse order of creation (where `callback_type_e >= FIO_CALL_ON_SHUTDOWN`).

During an event, changes to the callback list are ignored (callbacks can't add or remove other callbacks for the same event).

#### `fio_state_callback_clear`

```c
void fio_state_callback_clear(callback_type_e);
```

Clears all the existing callbacks for the event.

## Event and Task Scheduling

#### `fio_defer`

```c
int fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);
```

Defers a task's execution.

Tasks are functions of the type `void task(void *, void *)`, they return nothing (void) and accept two opaque `void *` pointers, user-data 1 (`udata1`) and user-data 2 (`udata2`).

Returns -1 or error, 0 on success.

#### `fio_defer_io_task`

```c
int fio_defer_io_task(intptr_t uuid,
                      void (*task)(intptr_t uuid, fio_protocol_s *, void *udata),
                      void *udata);
```

Schedules a protected connection task. The task will run within the connection's lock.

If an error occurs or the connection is closed before the task can run, the task will be called with a NULL protocol pointer, for resource cleanup.

#### `fio_run_every`

```c
void fio_run_every(size_t milliseconds,
                   int32_t repetitions,
                   int (*task)(void *, void *),
                   void *udata1,
                   void *udata2,
                   void (*on_finish)(void *, void *));
```

Creates a timer to run a task at the specified interval.

The task will repeat `repetitions` times. If `repetitions` is set to -1, task
will repeat forever.

If `task` returns a non-zero value, it will stop repeating.

The `on_finish` handler is always called (even on error).

#### `fio_defer_perform`

```c
void fio_defer_perform(void);
```

Performs all deferred tasks.

#### `fio_defer_has_queue`

```c
int fio_defer_has_queue(void);
```

Returns true if there are deferred functions waiting for execution.

## Connection Callback (Protocol) Management

The Protocol `struct` is part of facil.io's core design, it defines the callbacks used for the connection and sets it's behavior.

For concurrency reasons, a protocol instance **should** be unique to each connections. Different connections shouldn't share a single protocol object (callbacks and data can obviously be shared).

All the callbacks receive a unique connection ID (a localized "UUID") that can be converted to the original file descriptor when in need.

This allows facil.io to prevent old connection handles from sending data to new connections after a file descriptor is "recycled" by the OS.

#### `fio_protocol_s` (type)

```c
struct fio_protocol_s {
  void (*on_data)(intptr_t uuid, fio_protocol_s *protocol);
  void (*on_ready)(intptr_t uuid, fio_protocol_s *protocol);
  uint8_t (*on_shutdown)(intptr_t uuid, fio_protocol_s *protocol);
  void (*on_close)(intptr_t uuid, fio_protocol_s *protocol);
  void (*on_timeout)(intptr_t uuid, fio_protocol_s *protocol);
  uintptr_t rsv;
};
```


* `on_data` - called when a data is available, but will not run concurrently. 

    ```c
    void on_data(intptr_t uuid, fio_protocol_s *protocol);
    ```

* `on_ready` - called once all pending `fio_write` calls are finished.. 

    ```c
    void on_ready(intptr_t uuid, fio_protocol_s *protocol);
    ```

* `on_shutdown`- called when the server is shutting down, immediately before closing the connection.

    The callback runs within a `FIO_PR_LOCK_TASK` lock, so it will never run concurrently with {on_data} or other connection specific tasks.

    The `on_shutdown` callback should return 0 to close the socket or a number between 1..254 to delay the socket closure by that amount of seconds.

    Once the socket was marked for closure, facil.io will allow 8 seconds for all the data to be sent before forcefully closing the socket (regardless of state).

    If the `on_shutdown` returns `255`, the socket is ignored and it will be abruptly terminated when all other sockets have finished their graceful shutdown procedure.

		```c
		uint8_t on_shutdown(intptr_t uuid, fio_protocol_s *protocol);
		```

* `on_close` - Called when the connection was closed, but will not run concurrently. 

    ```c
    void on_close(intptr_t uuid, fio_protocol_s *protocol);
    ```

* `on_timeout` - called when a connection's timeout was reached. 

    ```c
    void on_timeout(intptr_t uuid, fio_protocol_s *protocol);
    ```

* `rsv` - reserved data - used by facil.io internally. 

    ```c
    uintptr_t rsv;
    ```

#### `fio_attach`

```c
void fio_attach(intptr_t uuid, fio_protocol_s *protocol);
```

Attaches (or updates) a protocol object to a socket UUID.

The new protocol object can be NULL, which will detach ("hijack"), the socket.

The old protocol's `on_close` (if any) will be scheduled.

On error, the new protocol's `on_close` callback will be called immediately.

#### `fio_attach_fd`

```c
void fio_attach_fd(int fd, fio_protocol_s *protocol);
```

Attaches (or updates) a protocol object to a file descriptor (`fd`).

The new protocol object can be NULL, which will detach ("hijack"), the socket and the `fd` can be one created outside of facil.io.

The old protocol's `on_close` (if any) will be scheduled.

On error, the new protocol's `on_close` callback will be called immediately.

**Note**: before attaching a file descriptor that was created outside of facil.io's library, make sure it is set to non-blocking mode (see `fio_set_non_block`). facil.io file descriptors are all non-blocking and it will assumes this is the case for the attached `fd`.

#### `fio_timeout_set`

```c
void fio_timeout_set(intptr_t uuid, uint8_t timeout);
```

Sets a timeout for a specific connection (only when running and valid).

#### `fio_timeout_get`

```c
uint8_t fio_timeout_get(intptr_t uuid);
```

Gets a timeout for a specific connection. Returns 0 if none.

#### `fio_touch`

```c
void fio_touch(intptr_t uuid);
```

"Touches" a socket connection, resetting it's timeout counter.

#### `fio_force_event`

```c
enum fio_io_event {
  FIO_EVENT_ON_DATA,
  FIO_EVENT_ON_READY,
  FIO_EVENT_ON_TIMEOUT
};
void fio_force_event(intptr_t uuid, enum fio_io_event);
```

Schedules an IO event, even if it did not occur.

#### `fio_suspend`

```c
void fio_suspend(intptr_t uuid);
```
Temporarily prevents `on_data` events from firing.

The `on_data` event will be automatically rescheduled when (if) the socket's outgoing buffer fills up or when `fio_force_event` is called with `FIO_EVENT_ON_DATA`.

**Note**: the function will work as expected when called within the protocol's `on_data` callback and the `uuid` refers to a valid socket. Otherwise the function might quietly fail.

#### `fio_capa`

```c
size_t fio_capa(void);
```

Returns the maximum number of open files facil.io can handle per worker process.

Total OS limits might apply as well but aren't shown.

The value of 0 indicates either that the facil.io library wasn't initialized yet or that it's resources were released.

### Lower Level Protocol API - for special circumstances, use with care.

#### `fio_protocol_try_lock`

```c
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid);
```

This function allows out-of-task access to a connection's `fio_protocol_s` object by attempting to acquire a locked pointer.

**Careful**: mostly, the protocol object will be locked and a pointer will be sent to the connection event's callback. However, if you need access to the protocol object from outside a running connection task, you might need to lock the protocol to prevent it from being closed / freed in the background.

On error, consider calling `fio_defer` or `fio_defer_io_task` instead of busy waiting. Busy waiting **should** be avoided whenever possible.

#### `fio_protocol_unlock`

```c
void fio_protocol_unlock(fio_protocol_s *pr);
```

Don't unlock what you don't own... see `fio_protocol_try_lock` for details.

## Listening to Incoming Connections

```c
struct fio_listen_args {
	/* Arguments for the fio_listen function */
  const char *url;
  void (*on_open)(intptr_t uuid, void *udata);
  void *udata;
  fio_tls_s *tls;
  void (*on_finish)(void *udata);
  uint8_t is_master_only;
};
intptr_t fio_listen(struct fio_listen_args args);
#define fio_listen(url_, ...)                                                  \
  fio_listen((struct fio_listen_args){.url = (url_), __VA_ARGS__})
```

Sets up a network service on a listening socket.

Returns the listening socket's uuid or -1 (on error).

Listening to incoming connections is pretty straight forward.

After a new connection is accepted, the `on_open` callback is called. `on_open` should allocate the new connection's protocol and call `fio_attach` to attach the protocol to the connection's `uuid`.

The protocol's `on_close` callback is expected to handle any cleanup required.

The `on_finish` callback is called when the listening socket is closed (application exit).

The following named arguments are recognized:

* `url` - The binding address in URL format. Defaults to: tcp://0.0.0.0:3000

		```c
		const char *url;
		```


* `on_open` - Called whenever a new connection is accepted.

	Should either call `fio_attach` or close the connection.

  ```c
  void on_open(intptr_t uuid, void *udata);
  ```
  
* `udata` - Opaque user data.

		```c
		void *udata;
		```

  
* `tls` - a pointer to a `fio_tls_s` object, for SSL/TLS support.

		```c
		fio_tls_s *tls;
		```


* `on_finish` - Called when the server is done, usable for cleanup.

	This will be called separately for every process before exiting.

  ```c
  void on_finish(void *udata);
  ```

* `is_master_only` - Listens and connects using only the master process (non-worker).

    On single-process implementations, this is meaningless.

    On multi-process implementations, this is an anti-pattern used to move some workload to the master.

    In practice it's only used for extending the pub/sub system using multi-machine clusters (`fio_cluster_listen2remote`).

    ```c
    uint8_t is_master_only;
    ```

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

#### `FIO_PING_ETERNAL`

```c
void FIO_PING_ETERNAL(intptr_t, fio_protocol_s *);
```

A ping function that does nothing except keeping the connection alive.


## Connecting to remote servers as a client

#### `FIO_RECONNECT_DELAY`

```c
#define FIO_RECONNECT_DELAY 150
```

The number of milliseconds to wait before attempting a reconnect.

A delay is important so reconnection attempts don't bombard and overrun both the local IO reactor and the network at large.

#### `fio_connect`

```c
/** Connection handle. */
typedef struct fio_connect_s fio_connect_s;
/** Named arguments for the `fio_connect` function, that allows non-blocking connections to be established. */
struct fio_connect_args {
  const char *url;
  void (*on_open)(intptr_t uuid, void *udata);
  void (*on_finish)(void *udata);
  fio_tls_s *tls;
  void *udata;
  uint8_t timeout;
  uint8_t auto_reconnect;
  uint8_t is_master_only;
};
/* the `fio_connect` function */
fio_connect_s *fio_connect(struct fio_connect_args);
/* the `fio_connect` MACRO allowing for named arguments. */
#define fio_connect(...) fio_connect((struct fio_connect_args){__VA_ARGS__})
```

Creates a client connection (in addition or instead of the server).

See the `struct fio_connect_args` details for any possible named arguments.

* `.url` - The address of the server we are connecting to in URL format.

* `.on_open` - Called whenever a connection is (re)established.

	```c
	void on_open(intptr_t uuid, void *udata);
	```

* `.on_finish` - Called whenever a connection is (re)established.

	```c
	void (*on_finish)(void *udata);
	```

* `.tls` - A pointer to a `fio_tls_s` object, for SSL/TLS support.

* `.udata` - Opaque user data.

* `.timeout` - A non-system timeout after which connection is assumed to have failed.

* `.auto_reconnect` - If true, the connection will auto-reconnect while running.

* `is_master_only` - Listens and connects using only the master process (non-worker).

    On single-process implementations, this is meaningless.

    On multi-process implementations, this is an anti-pattern used to move some workload to the master.

    In practice it can be used for extending the pub/sub system (i.e., `fio_cluster_connect2remote`) or for using the pub/sub system to share a single database / backend connection.

    ```c
    uint8_t is_master_only;
    ```


**Note**: experimental API - untested.

# Socket / Connection - Read / Write Functions

#### `fio_read`

```c
ssize_t fio_read(intptr_t uuid, void *buf, size_t len);
```


`fio_read` attempts to read up to count bytes from the socket into the buffer starting at `buffer`.

`fio_read`'s return values are wildly different then the native return values and they aim at making far simpler sense.

`fio_read` returns the number of bytes read (0 is a valid return value which simply means that no bytes were read from the buffer).

On a fatal connection error that leads to the connection being closed (or if the connection is already closed), `fio_read` returns -1.

The value 0 is the valid value indicating no data was read.

Data might be available in the kernel's buffer while it is not available to be read using `fio_read` (i.e., when using a transport layer, such as TLS).

#### `fio_write`

```c
ssize_t fio_write(intptr_t uuid, const void *buf, size_t len);
```

`fio_write` copies `legnth` data from the buffer and schedules the data to be sent over the socket.

The data isn't necessarily written to the socket. The actual writing to the socket is handled by the IO reactor.

On error, -1 will be returned. Otherwise returns 0.

Returns the same values as `fio_write2`.

This is equivalent to calling:

```c
fio_write2(uuid, .data.buf = buf, .len = len, .copy = 1);
```

#### `fio_write2`

```c
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
#define fio_write2(uuid, ...)                                                  \
  fio_write2_fn(uuid, (fio_write_args_s){__VA_ARGS__})
```

Schedules data to be written to the socket.

`fio_write2` is similar to `fio_write`, except that it allows far more flexibility.

On error, -1 will be returned. Otherwise returns 0.

See the `fio_write_args_s` structure for details.

NOTE: The data is "moved" to the ownership of the socket, not copied. The data will be deallocated according to the `.dealloc` function.

#### `FIO_DEALLOC_NOOP`

```c
void FIO_DEALLOC_NOOP(void *arg);
```
A no-op stub function for fio_write2 in cases not deallocation is required (set `.dealloc = FIO_DEALLOC_NOOP`).

####`fio_sendfile`

```c
ssize_t fio_sendfile(intptr_t uuid, int source_fd, off_t offset, size_t len);
```

Sends data from a file as if it were a single atomic packet (sends up to length bytes or until EOF is reached).

Once the file was sent, the `source_fd` will be closed using `close`.

The file will be buffered to the socket chunk by chunk, so that memory consumption is capped. The system's `sendfile` might be used if conditions permit.

`offset` dictates the starting point for the data to be sent and length sets the maximum amount of data to be sent.

Returns -1 and closes the file on error. Returns 0 on success.

This is equivalent to calling:

```c
fio_write2(uuid,
          .data = {.fd = source_fd},
          .offset = (uintptr_t)offset,
          .len = len,
          .is_fd = 1);
```


#### `fiobj_send_free`

```c
ssize_t fiobj_send_free(intptr_t uuid, FIOBJ o);
```

Writes a FIOBJ object to the `uuid` and frees it once it was sent.

This _almost_ is equivalent to calling, with some additional safeguards:

```c
fio_str_info_s s = fiobj2cstr(o);
fio_write2(uuid,
          .data.buf = (char *)o,
          .offset = ((uintptr_t)s.buf - (uintptr_t)o),
          .dealloc = fiobj___free_after_send,
          .len = s.len);

```

#### `fio_pending`

```c
size_t fio_pending(intptr_t uuid);
```

Returns the number of `fio_write` calls that are waiting in the socket's queue and haven't been processed.

#### `fio_flush`

```c
ssize_t fio_flush(intptr_t uuid);
```

`fio_flush` attempts to write any remaining data in the internal buffer to the underlying file descriptor and closes the underlying file descriptor once if it's marked for closure (and all the data was sent).

Return values: 1 will be returned if data remains in the buffer. 0 will be returned if the buffer was fully drained. -1 will be returned on an error or when the connection is closed.

`errno` will be set to `EWOULDBLOCK` if the socket's lock is busy.


#### `fio_flush_all`

```c
#define fio_flush_strong(uuid)                                                 \
  do {                                                                         \
    errno = 0;                                                                 \
  } while (fio_flush(uuid) > 0 || errno == EWOULDBLOCK)
```

Blocks until all the data was flushed from the buffer

#### `fio_flush_all`

```c
size_t fio_flush_all(void);
```

`fio_flush_all` attempts flush all the open connections.

Returns the number of sockets still in need to be flushed.

## Connection ENV - Object Links / Environment

#### `fio_uuid_env_set`

```c
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
/* the fio_uuid_env_set function */
void fio_uuid_env_set(intptr_t uuid, fio_uuid_env_args_s);
/* the MACRO for enabling named arguments. */
#define fio_uuid_env_set(uuid, ...)                                            \
  fio_uuid_env_set(uuid, (fio_uuid_env_args_s){__VA_ARGS__})
```

Links an object to a connection's lifetime / environment.

The `on_close` callback will be called once the connection has died.

If the `uuid` is invalid, the `on_close` callback will be called immediately.

**Note**: the `on_close` callback will be called within a high priority lock. Long tasks should be deferred so they are performed outside the lock.

#### `fio_uuid_env_unset`

```c
/** Named arguments for the `fio_uuid_env_unset` function. */
typedef struct {
  intptr_t type;
  fio_str_info_s name;
} fio_uuid_env_unset_args_s;
/* the fio_uuid_env_unset function */
int fio_uuid_env_unset(intptr_t uuid, fio_uuid_env_unset_args_s);
/* the MACRO for enabling named arguments. */
#define fio_uuid_env_unset(uuid, ...)                                          \
  fio_uuid_env_unset(uuid, (fio_uuid_env_unset_args_s){__VA_ARGS__})
```

Un-links an object from the connection's lifetime, so it's `on_close` callback will **NOT** be called.

Returns 0 on success and -1 if the object couldn't be found, setting `errno` to `EBADF` if the `uuid` was invalid and `ENOTCONN` if the object wasn't found (wasn't linked).

**Note**: a failure likely means that the object's `on_close` callback was already called!

#### `fio_uuid_env_remove`

```c
fio_uuid_env_remove(intptr_t uuid, fio_uuid_env_unset_args_s);
#define fio_uuid_env_remove(uuid, ...)                                         \
  fio_uuid_env_remove(uuid, (fio_uuid_env_unset_args_s){__VA_ARGS__})
```

Removes an object from the connection's lifetime / environment, calling it's `on_close` callback as if the connection was closed.

**Note**: the `on_close` callback will be called within a high priority lock. Long tasks should be deferred so they are performed outside the lock.

## Socket / Connection Functions

```c
typedef enum { /* DON'T CHANGE VALUES - they match fio-stl.h */
               FIO_SOCKET_SERVER = 0,
               FIO_SOCKET_CLIENT = 1,
               FIO_SOCKET_NONBLOCK = 2,
               FIO_SOCKET_TCP = 4,
               FIO_SOCKET_UDP = 8,
               FIO_SOCKET_UNIX = 16,
} fio_socket_flags_e;
```

#### `fio_uuid2fd`

```c
#define fio_uuid2fd(uuid) ((int)((uintptr_t)uuid >> 8))
```

Convert between a facil.io connection's identifier (uuid) and system's fd.


`fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active `uuid`.

#### `fio_fd2uuid`

```c
intptr_t fio_fd2uuid(int fd);
```

If the file descriptor was closed directly (not using `fio_close`) or the closure event hadn't been processed, a false positive is possible, in which case the `uuid` might not be valid for long.

Returns -1 on error. Returns a valid socket (non-random) UUID.

#### `fio_socket`

```c
intptr_t fio_socket(const char *address,
                    const char *port,
                    fio_socket_flags_e flags);
```

Creates a TCP/IP, UDP or Unix socket and returns it's unique identifier.

For TCP/IP or UDP server sockets (flag sets `FIO_SOCKET_SERVER`), a `NULL` `address` variable is recommended. Use "localhost" or "127.0.0.1" to limit access to the local machine.

For TCP/IP or UDP client sockets (flag sets `FIO_SOCKET_CLIENT`), a remote `address` and `port` combination will be required. `connect` will be called.

For TCP/IP and Unix server sockets (flag sets `FIO_SOCKET_SERVER`), `listen` will automatically be called by this function.

For Unix server or client sockets, the `port` variable is silently ignored.

If the socket is meant to be attached to the facil.io reactor, `FIO_SOCKET_NONBLOCK` MUST be set.

The following flags control the type and behavior of the socket:

* `FIO_SOCKET_SERVER` - (default) server mode (may call `listen`).
* `FIO_SOCKET_CLIENT` - client mode (calls `connect`).
* `FIO_SOCKET_NONBLOCK` - sets the socket to non-blocking mode.
* `FIO_SOCKET_TCP` - TCP/IP socket (default).
* `FIO_SOCKET_UDP` - UDP socket.
* `FIO_SOCKET_UNIX` - Unix Socket.

Returns -1 on error. Any other value is a valid unique identifier.

**Note**: facil.io uses unique identifiers to protect sockets from collisions. However these identifiers can be converted to the underlying file descriptor using the `fio_uuid2fd` macro.

**Note**: UDP server sockets can't use `fio_read` or `fio_write` since they are connectionless.

#### `fio_accept`

```c
intptr_t fio_accept(intptr_t srv_uuid);
```

`fio_accept` accepts a new socket connection from a server socket - see the server flag on `fio_socket`.

Accepted connection are automatically set to non-blocking mode and the O_CLOEXEC flag is set.

**Note**: this function does **NOT** attach the socket to the IO reactor - see `fio_attach`.

#### `fio_set_non_block`

```c
int fio_set_non_block(int fd);
```

Sets a socket to non blocking state.

This will also set the O_CLOEXEC flag for the file descriptor.

This function is called automatically for the new socket, when using `fio_accept`, `fio_connect` or `fio_socket`.

#### `fio_is_valid`

```c
int fio_is_valid(intptr_t uuid);
```

Returns 1 if the uuid refers to a valid and open, socket.

Returns 0 if not.

#### `fio_is_closed`

```c
int fio_is_closed(intptr_t uuid);
```

Returns 1 if the uuid is invalid or the socket is flagged to be closed.

Returns 0 if the socket is valid, open and isn't flagged to be closed.

#### `fio_close`

```c
void fio_close(intptr_t uuid);
```

`fio_close` marks the connection for disconnection once all the data was sent. The actual disconnection will be managed by the `fio_flush` function.

`fio_flash` will be automatically scheduled.

#### `fio_force_close`

```c
void fio_force_close(intptr_t uuid);
```

`fio_force_close` closes the connection immediately, without adhering to any protocol restrictions and without sending any remaining data in the connection buffer.

#### `fio_peer_addr`

```c
fio_str_info_s fio_peer_addr(intptr_t uuid);
```

Returns the information available about the socket's peer address.

If no information is available, the struct will be initialized with zero (`addr == NULL`).

The information is only available when the socket was accepted using `fio_accept` or opened using `fio_connect`.

#### `fio_local_addr`

```c
size_t fio_local_addr(char *dest, size_t limit);
```

Writes the local machine address (qualified host name) to the buffer.

Returns the amount of data written (excluding the NUL byte).

`limit` is the maximum number of bytes in the buffer, including the NUL byte.

If the returned value == limit - 1, the result might have been truncated.

If 0 is returned, an error might have occurred (see `errno`) and the contents of `dest` is undefined.

## Connection Read / Write Hooks - Overriding the system calls

#### `fio_rw_hook_s`

```c
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
```
The following `struct` is used for setting a the read/write hooks that will replace the default system calls to `recv` and `write`.

**Note**: facil.io library functions MUST NEVER be called by any r/w hook, or a deadlock might occur.

#### `fio_rw_hook_set`

```c
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata);
```

Sets a socket hook state (a pointer to the `struct`).

#### `fio_rw_hook_replace_unsafe`

```c
int fio_rw_hook_replace_unsafe(intptr_t uuid,
                               fio_rw_hook_s *rw_hooks,
                               void *udata);
```
Replaces an existing read/write hook with another from within a read/write hook callback.

Does NOT call any cleanup callbacks.

Replaces existing `udata`. Call with the existing `udata` to keep it.

Returns -1 on error, 0 on success.

Note: this function is marked as unsafe, since it should only be called from within an existing read/write hook callback. Otherwise, data corruption might occur.

#### `FIO_DEFAULT_RW_HOOKS`

```c
extern const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS;
```

The default Read/Write hooks used for system Read/Write (by default: `udata == NULL`).
## Pub/Sub Services

facil.io supports a [Publish–Subscribe Pattern](https://en.wikipedia.org/wiki/Publish–subscribe_pattern) API which can be used for Inter Process Communication (IPC), messaging, horizontal scaling and similar use-cases.

### Subscription Control


#### `fio_subscribe`

```c
subscription_s *fio_subscribe(subscribe_args_s args);
/** MACRO enabling named arguments. */
#define fio_subscribe(...) fio_subscribe((subscribe_args_s){__VA_ARGS__})
```

This function subscribes to either a numerical "filter" or a named channel (but not both).

The `fio_subscribe` function is shadowed by the `fio_subscribe` MACRO, which allows the function to accept "named arguments", as shown in the following example:

```c
static void my_message_handler(fio_msg_s *msg); 
//...
subscription_s * s = fio_subscribe(.channel = {.data="name", .len = 4},
                                   .on_message = my_message_handler);
```


The function accepts the following named arguments:

* `uuid`

    If `uuid` is set, the subscription will be attached (and owned) by a connection.

    The function will return `NULL` since the subscription ownership will be transferred to the connection's environment instead of returned to the caller.

    The `on_message` callback will be called within a connection's task lock (`FIO_PR_LOCK_TASK`).

    If `uuid` is `<= 0`, the subscription will be considered as a global subscription, owned by the calling function, as if `uuid` wasn't set. A pointer to the subscription will be returned to the calling function.

        // type:
        intptr_t uuid;

* `filter`:

    If `filter` is set, all messages that match the filter's numerical value will be forwarded to the subscription's callback.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by filter.

    **Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks (both subjects are covered later on).

        // type:
        int32_t filter;

* `channel`:

    If `filter == 0` (or unset), than the subscription will be made using `channel` binary name matching. Note that and empty string (`NULL` pointer and `0` length) is a valid channel name.

    Subscriptions can either require to be matched by filter or matched by channel. This will match the subscription by channel (only messages with no `filter` and the same `channel` value will be forwarded to the subscription).

        // type:
        fio_str_info_s channel;


* `is_pattern`:

    If the optional `is_pattern` is set (true), pattern matching will be used as an alternative to exact channel name matching.

    The pattern matching function used for all pattern matching can be set using the global `FIO_PUBSUB_PATTERN_MATCH` function pointer. By default that pointer points to the `fio_glob_match` function, that provides binary glob matching (not UTF-8 matching), which matches the **Redis** pattern matching logic.

    This is significantly slower, as no Hash Map can be used to locate a match - each message published to a channel will be tested for a match against each pattern.

* `on_message`:

    The callback will be called for each message forwarded to the subscription.

        // callback example:
        void on_message(fio_msg_s *msg);

* `on_unsubscribe`:

    The callback will be called once the subscription is canceled, allowing it's resources to be freed.

        // callback example:
        void (*on_unsubscribe)(void *udata1, void *udata2);

* `udata1` and `udata2`:

    These are the opaque user data pointers passed to `on_message` and `on_unsubscribe`.

        // type:
        void *udata1;
        void *udata2;


The function returns a pointer to the opaque subscription type `subscription_s`.

On error, `NULL` will be returned and the `on_unsubscribe` callback will be called.

The `on_message` should accept a pointer to the `fio_msg_s` type:

```c
typedef struct fio_msg_s {
 int32_t filter;
 fio_str_info_s channel;
 intptr_t uuid;
 fio_protocol_s * pr;
 fio_str_info_s msg;
 void *udata1;
 void *udata2;
 uint8_t is_json;
} fio_msg_s;
```

* `filter` is the numerical filter (if any) used in the `fio_subscribe` and `fio_publish` functions. Negative values are reserved and 0 == channel name matching.

* `channel` is an **immutable** binary string containing the channel name given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details. Editing the string will cause undefined behavior.

* `uuid` is the connection's identifier (if any) to which the subscription belongs.

  A connection `uuid` 0 marks an un-bound (non-connection related) subscription.

* `pr` is the connection's protocol (if any).

  If the subscription is bound to a connection, the protocol will be locked using a task lock and will be available using this pointer.

* `msg` is an immutable binary string containing the message data given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details. Editing the string will cause undefined behavior.

* `is_json` is a binary flag (1 or 0) that marks the message as JSON. This is the flag passed as passed to the `fio_publish` function.

* `udata1` and `udata2` are the opaque user data pointers passed to `fio_subscribe` during the subscription.

**Note (1)**: if a subscription object is no longer required, i.e., if `fio_unsubscribe` will only be called once a connection was closed or once facil.io is shutting down, consider using [`fio_uuid_link`](#fio_uuid_link) or [`fio_state_callback_add`](#fio_state_callback_add) to control the subscription's lifetime.


**Note (2)**: passing protocol object pointers to the `udata` is not safe, since protocol objects might be destroyed or invalidated due to either network events (socket closure) or internal changes (i.e., `fio_attach` being called). The preferred way is to add the `uuid` to the `udata` field and call [`fio_protocol_try_lock`](#fio_protocol_try_lock) to access the protocol object.

#### `fio_subscription_channel`

```c
fio_str_info_s fio_subscription_channel(subscription_s *subscription);
```

This helper returns a temporary String with the subscription's channel (or a binary
string representing the filter).

To keep the string beyond the lifetime of the subscription, copy the string.

#### `fio_message_defer`

```c
void fio_message_defer(fio_msg_s *msg);
```

Defers the subscription's callback handler, so the subscription will be called again for the same message.

A classic use case allows facil.io to handle other events while waiting on a lock / mutex to become available in a multi-threaded environment.

#### `fio_unsubscribe`

```c
void fio_unsubscribe(subscription_s *subscription);
```

Cancels an existing subscription.

This function will be automatically deferred if a subscription task is running on a different thread, which might delay the effects of the function.

The subscription task won't be called again once the function completes it's task.

#### `fio_unsubscribe_uuid`

```c
void fio_unsubscribe_uuid(subscribe_args_s args);
/** MACRO enabling named arguments. */
#define fio_unsubscribe_uuid(...)                                              \
  fio_unsubscribe_uuid((subscribe_args_s){__VA_ARGS__})
```

Cancels an existing subscriptions that was bound to a connection's `uuid`. See `fio_subscribe` and `fio_unsubscribe` for more details.

Accepts the same arguments as `fio_subscribe`, except the `udata` and callback details are ignored (no need to provide `udata` or callback details).

The `fio_unsubscribe_uuid` function is shadowed by the `fio_unsubscribe_uuid` macro, which allows the function to accept the following "named arguments" (see `fio_subscribe` example):

* `uuid`

    Use this function only if a `uuid` value was provided to the `fio_subscribe` function. The same value should be provided here, to make sure the correct subscription is found and removed.

* `filter`:

    If a `filter` value was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

* `channel`:

    If a `channel` name was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

* `match`:

    If an optional `match` callback was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

### Publishing messages

#### `fio_publish`

```c
void fio_publish(fio_publish_args_s args);
```

This function publishes a message to either a numerical "filter" or a named channel (but not both).

The message can be a `NULL` or an empty message.

The `fio_publish` function is shadowed by the `fio_publish` MACRO, which allows the function to accept "named arguments", as shown in the following example:

```c
fio_publish(.channel = {.data="name", .len = 4},
            .message = {.data = "foo", .len = 3});
```

The function accepts the following named arguments:


* `filter`:

    If `filter` is set, all messages that match the filter's numerical value will be forwarded to the subscription's callback.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by filter.

        // type:
        int32_t filter;

* `channel`:

    If `filter == 0` (or unset), than the subscription will be made using `channel` binary name matching. Note that and empty string (NULL pointer and 0 length) is a valid channel name.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by channel (only messages with no `filter` will be received).

        // type:
        fio_str_info_s channel;


* `message`:

    The message data to be sent.

        // type:
        fio_str_info_s message;

* `is_json`:

    A flag indicating if the message is JSON data or binary/text.

        // type:
        uint8_t is_json;


* `engine`:

    The pub/sub engine that should be used to forward this message (see later). Defaults to `FIO_PUBSUB_DEFAULT` (or `FIO_PUBSUB_CLUSTER`).

    Pub/Sub engines dictate the behavior of the pub/sub instructions. The possible internal pub/sub engines are:

    * `FIO_PUBSUB_CLUSTER` - used to publish the message to all clients in the network cluster. Unless `fio_pubsub_clusterfy` is called (currently unimplemented), acts the same as `FIO_PUBSUB_LOCAL`).

    * `FIO_PUBSUB_LOCAL` - used to publish the message to all the worker processes and the root / master process.

    * `FIO_PUBSUB_PROCESS` - used to publish the message only within the current process.

    * `FIO_PUBSUB_SIBLINGS` - used to publish the message except within the current process.

    * `FIO_PUBSUB_ROOT` - used to publish the message exclusively to the root / master process.
    
    The default pub/sub can be changed globally by assigning a new default engine to the `FIO_PUBSUB_DEFAULT` global variable.

        // type:
        fio_pubsub_engine_s const *engine;
        // default engine:
        extern fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT;

### Pub/Sub Message MiddleWare Meta-Data

It's possible to attach meta-data to facil.io pub/sub messages before they are published.

This is only available for named channels (filter == 0).

This allows, for example, messages to be encoded as network packets for
outgoing protocols (i.e., encoding for WebSocket transmissions), improving
performance in large network based broadcasting.

**Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

#### `FIO_PUBSUB_METADATA_LIMIT` macro

```c
#define FIO_PUBSUB_METADATA_LIMIT 4
```

This compilation macro sets the number of different metadata callbacks that can be attached.

Larger numbers will effect performance.

The default value should be enough for the following metadata objects:

- WebSocket server headers.

- WebSocket client (header + masked message copy).

- EventSource (SSE) encoded named channel and message.


#### `fio_message_metadata`

```c
void *fio_message_metadata(fio_msg_s *msg, int id);;
```

Finds the message's meta-data by the meta-data's type ID. Returns the data or NULL.

#### `fio_message_metadata_add`

```c
// The function:
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *))
// The matadata builder type:
typedef void *(*fio_msg_metadata_fn)(fio_str_info_s ch,
                                     fio_str_info_s msg,
                                     uint8_t is_json);
// Example matadata builder
void * foo_metadata(fio_str_info_s ch,
                    fio_str_info_s msg,
                    uint8_t is_json);

// Example cleanup - called to free the data
void foo_free(void * metadata);

```

It's possible to attach metadata to facil.io named messages (`filter == 0`) before they are published.

This allows, for example, messages to be encoded as network packets for outgoing protocols (i.e., encoding for WebSocket transmissions), improving performance in large network based broadcasting.

Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.

The callback should return a `void *` pointer.

To remove a callback, call `fio_message_metadata_remove` with the returned value (the metadata ID).

The cluster messaging system allows some messages to be flagged as JSON and this flag is available to the metadata callback.

Returns a positive number on success (the metadata ID) or zero (0) on failure.

#### `fio_message_metadata_remove`

```c
void fio_message_metadata_remove(int id);
```

Removed the metadata callback.

Removal might be delayed if live metatdata exists.

Removal only occurs if `fio_message_metadata_remove` was called the same number of times `fio_message_metadata_add` was called.

### External Pub/Sub Services

facil.io can be linked with external Pub/Sub services using "engines" (`fio_pubsub_engine_s`).

Pub/Sub engines dictate the behavior of the pub/sub instructions. 

This allows for an application to connect to a service such as Redis or NATs for horizontal pub/sub scaling.

A [Redis engine](redis) is bundled as part of the facio.io extensions but isn't part of the core library.

The default engine can be set using the `FIO_PUBSUB_DEFAULT` global variable. It's initial default is `FIO_PUBSUB_CLUSTER` (see [`fio_publish`](#fio_publish)).

**Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

Engines MUST provide the listed function pointers and should be attached using the `fio_pubsub_attach` function.

Engines should disconnect / detach, before being destroyed, by using the `fio_pubsub_detach` function.

When an engine received a message to publish, it should call the `fio_publish` function with the engine to which the message is forwarded. i.e.:

```c
fio_publish(
    .engine = FIO_PROCESS_ENGINE,
    .channel = {0, 4, "name"},
    .message = {0, 4, "data"} );
```


#### `fio_pubsub_attach`

```c
void fio_pubsub_attach(fio_pubsub_engine_s *engine);
```

Attaches an engine, so it's callbacks can be called by facil.io.

The `subscribe` callback will be called for every existing channel.

The engine type defines the following callback:

* `subscribe` - Should subscribe channel. Failures are ignored.

        void subscribe(const fio_pubsub_engine_s *eng,
                       fio_str_info_s channel,
                       fio_match_fn match);

* `unsubscribe` - Should unsubscribe channel. Failures are ignored.

        void unsubscribe(const fio_pubsub_engine_s *eng,
                         fio_str_info_s channel,
                         fio_match_fn match);

* `publish` - Should publish a message through the engine. Failures are ignored.

        int publish(const fio_pubsub_engine_s *eng,
                    fio_str_info_s channel,
                    fio_str_info_s msg, uint8_t is_json);

**Note**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


**IMPORTANT**: The `subscribe` and `unsubscribe` callbacks are called from within an internal lock. They MUST NEVER call pub/sub functions except by exiting the lock using `fio_defer`.

#### `fio_pubsub_detach`

```c
void fio_pubsub_detach(fio_pubsub_engine_s *engine);
```

Detaches an engine, so it could be safely destroyed.

#### `fio_pubsub_reattach`

```c
void fio_pubsub_reattach(fio_pubsub_engine_s *eng);
```

Engines can ask facil.io to call the `subscribe` callback for all active channels.

This allows engines that lost their connection to their Pub/Sub service to resubscribe all the currently active channels with the new connection.

CAUTION: This is an evented task... try not to free the engine's memory while re-subscriptions are under way...

**Note**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


**IMPORTANT**: The `subscribe` and `unsubscribe` callbacks are called from within an internal lock. They MUST NEVER call pub/sub functions except by exiting the lock using `fio_defer`.

#### `fio_pubsub_is_attached`

```c
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);
```

Returns true (1) if the engine is attached to the system.

-------------------------------------------------------------------------------
## Concurrency Management - Weak functions

Weak functions are functions that can be overridden during the compilation / linking stage.

This provides control over some operations such as thread creation and process forking, which could be important when integrating facil.io into a VM engine such as Ruby or JavaScript.

### Forking

#### `fio_fork`

```c
int fio_fork(void);
```

OVERRIDE THIS to replace the default `fork` implementation.

Should behaves like the system's `fork`.

Current implementation simply calls [`fork`](http://man7.org/linux/man-pages/man2/fork.2.html).


### Thread Creation

#### `fio_thread_new`

```c
void *fio_thread_new(void *(*thread_func)(void *), void *arg);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Accepts a pointer to a function and a single argument that should be executed
within a new thread.

The function should allocate memory for the thread object and return a
pointer to the allocated memory that identifies the thread.

On error NULL should be returned.

The default implementation returns a `pthread_t *`.

#### `fio_thread_free`

```c
void fio_thread_free(void *p_thr);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Frees the memory associated with a thread identifier (allows the thread to
run it's course, just the identifier is freed).

#### `fio_thread_join`

```c
int fio_thread_join(void *p_thr);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Accepts a pointer returned from `fio_thread_new` (should also free any
allocated memory) and joins the associated thread.

Return value is ignored.

-------------------------------------------------------------------------------
## Hashing and Friends

The facil.io IO core also includes a number or hashing and encoding primitives that are often used in network related applications.

### SHA-1

SHA-1 example:

```c
fio_sha1_s sha1;
fio_sha1_init(&sha1);
fio_sha1_write(&sha1,
             "The quick brown fox jumps over the lazy dog", 43);
char *hashed_result = fio_sha1_result(&sha1);
```

#### `fio_sha1_init`

```c
fio_sha1_s fio_sha1_init(void);
```

Initializes or resets the `fio_sha1_s` object. This must be performed before hashing data using SHA-1.

The SHA-1 container type (`fio_sha1_s`) is defines as follows:

The `fio_sha1_s` structure's content should be ignored.

#### `fio_sha1`

```c
inline char *fio_sha1(fio_sha1_s *s, const void *data, size_t len)
```

A SHA1 helper function that performs initialization, writing and finalizing.

SHA-1 hashing container - 

The `sha1_s` type will contain all the sha1 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

#### `fio_sha1_write`

```c
void fio_sha1_write(fio_sha1_s *s, const void *data, size_t len);
```

Writes data to the sha1 buffer.

#### `fio_sha1_result`

```c
char *fio_sha1_result(fio_sha1_s *s);
```

Finalizes the SHA1 hash, returning the Hashed data.

`fio_sha1_result` can be called for the same object multiple times, but the finalization will only be performed the first time this function is called.

### SHA-2

SHA-2 example:

```c
fio_sha2_s sha2;
fio_sha2_init(&sha2, SHA_512);
fio_sha2_write(&sha2,
             "The quick brown fox jumps over the lazy dog", 43);
char *hashed_result = fio_sha2_result(&sha2);
```

#### `fio_sha2_init`

```c
fio_sha2_s fio_sha2_init(fio_sha2_variant_e variant);
```

Initializes / resets the SHA-2 object.

SHA-2 is actually a family of functions with different variants. When initializing the SHA-2 container, a variant must be chosen. The following are valid variants:

* `SHA_512`

* `SHA_384`

* `SHA_512_224`

* `SHA_512_256`

* `SHA_256`

* `SHA_224`

The `fio_sha2_s` structure's content should be ignored.

#### `fio_sha2_write`

```c
void fio_sha2_write(fio_sha2_s *s, const void *data, size_t len);
```

Writes data to the SHA-2 buffer.

#### `fio_sha2_result`

```c
char *fio_sha2_result(fio_sha2_s *s);
```

Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the finalization will only be performed the first time this function is called.

#### `fio_sha2_512`

```c
inline char *fio_sha2_512(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 512 variant.

#### `fio_sha2_256`

```c
inline char *fio_sha2_256(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 256 variant.

#### `fio_sha2_256`

```c
inline char *fio_sha2_384(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 384 variant.

-------------------------------------------------------------------------------
## Version and Compilation Related Macros

The following macros effect facil.io's compilation and can be used to validate the API version exposed by the library.

### Version Macros

Currently the version macros are only available for facil.io's STL core library.

### Compilation Macros

The facil.io core library has some hard coded values that can be adjusted by defining the following macros during compile time.

#### `FIO_MAX_SOCK_CAPACITY`

This macro define the maximum hard coded number of connections per worker process.

To be more accurate, this number represents the highest `fd` value allowed by library functions.

If the soft coded OS limit is higher than this number, than this limit will be enforced instead.

#### `FIO_ENGINE_POLL`, `FIO_ENGINE_EPOLL`, `FIO_ENGINE_KQUEUE`

If set, facil.io will prefer the specified polling system call (`poll`, `epoll` or `kqueue`) rather then attempting to auto-detect the correct system call.

To set any of these flag while using the facil.io `makefile`, set the `FIO_FORCE_POLL` / `FIO_FORCE_EPOLL` / `FIO_FORCE_KQUEUE` environment variable to true. i.e.:

```bash
FIO_FORCE_POLL=1 make
```

It should be noted that for most use-cases, `epoll` and `kqueue` will perform better.

#### `FIO_CPU_CORES_LIMIT`

The facil.io startup procedure allows for auto-CPU core detection.

Sometimes it would make sense to limit this auto-detection to a lower number, such as on systems with more than 32 cores.

This is only relevant to automated values, when running facil.io with zero threads and processes, which invokes a large matrix of workers and threads (see [facil_start](#facil_start)).

This does NOT effect manually set (non-zero) worker/thread values.

#### `FIO_POLL_MAX_EVENTS`

This macro sets the maximum number of IO events facil.io will pre-schedule at the beginning of each cycle, when using `epoll` or `kqueue` (not when using `poll`).

Since this requires stack pre-allocated memory, this number shouldn't be set too high. Reasonable values range from 8 to 160.

The default value is currently 64.
