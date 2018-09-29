---
title: facil.io - 0.7.x Core Library Documentation
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

The core library types and functions can be found in the header `fio.h`.

The header is well documented and very long, and as a result, so is this documentation.

The header can be included more than once to produce multiple types of Hash Maps or data Sets. As well as to include some of it's optional features such as the binary String helpers and the linked list types.

## Connection (Protocol) Management

This section includes information about listening to incoming connections, connecting to remote machines and managing the protocol callback system.

The facil.io library is an evented library and the `fio_protocol_s` structure is at the core of the network evented design, so we'll start with the Protocol object.

### The `fio_protocol_s` structure

The Protocol structure defines the callbacks used for the connection and sets it's
behavior.

For concurrency reasons, a protocol instance SHOULD be unique to each connection and dynamically allocated. In single threaded applications, this is less relevant.

All the callbacks receive a unique connection ID (un-aptly named `uuid`) that can be
converted to the original file descriptor when in need.

This allows facil.io to prevent old connection handles from sending data
to new connections after a file descriptor is "recycled" by the OS.

The structure looks like this:

```c
struct fio_protocol_s {
    void (*on_data)(intptr_t uuid, fio_protocol_s *protocol);
    void (*on_ready)(intptr_t uuid, fio_protocol_s *protocol);
    uint8_t (*on_shutdown)(intptr_t uuid, fio_protocol_s *protocol);
    void (*on_close)(intptr_t uuid, fio_protocol_s *protocol);
    void (*ping)(intptr_t uuid, fio_protocol_s *protocol);
    size_t rsv;
};
```

#### `fio_protocol_s->on_data`

```c
void on_data(intptr_t uuid, fio_protocol_s *protocol);
```

Called when a data is available.

The function is called under the protocol's main lock (`FIO_PR_LOCK_TASK`), safeguarding the connection against collisions (the function will not run concurrently with itself for the same connection.



#### `fio_protocol_s->on_ready`

```c
void on_ready(intptr_t uuid, fio_protocol_s *protocol);
```

Called once all pending [`fio_write`](#fio_write) calls are finished.

For new connections this callback is also called once a connection was established and the connection can be written to.

#### `fio_protocol_s->on_shutdown`

```c
uint8_t on_shutdown(intptr_t uuid, fio_protocol_s *protocol);
```

Called when the server is shutting down, immediately before closing the connection.

The callback runs within a `FIO_PR_LOCK_TASK` lock, so it will never run concurrently with `on_data` or other connection specific tasks [`fio_defer_io_task`](#fio_defer_io_task).

The `on_shutdown` callback should return 0 under normal circumstances. This will mark the connection for immediate closure and allow 8 seconds for all pending data to be sent.

The `on_shutdown` callback may also return any number between 1..254 to delay the socket closure by that amount of time. 

If the `on_shutdown` returns 255, the socket is ignored and it will be abruptly terminated when all other sockets have finished their graceful shutdown procedure.

#### `fio_protocol_s->on_close`

```c
uint8_t on_close(intptr_t uuid, fio_protocol_s *protocol);
```

Called when the connection was closed, but will not run concurrently with other callbacks.

#### `fio_protocol_s->ping`

```c
uint8_t ping(intptr_t uuid, fio_protocol_s *protocol);
```

Called when a connection's timeout was reached.

This callback is called outside of the protocol's normal locks to support pinging in cases where the `on_data` callback is running in the background (which shouldn't happen, but we know it sometimes does).

#### `fio_protocol_s->rsv`

This is private metadata used by facil. In essence it holds the locking data and overwriting this data is extremely volatile.

The data MUST be set to 0 before [attaching a protocol](#fio_attach) to facil.io.

### Attaching / Detaching Protocol Objects

Once a protocol object was created, it should be attached to the fail.io library.

Detaching is also possible by attaching a NULL protocol (used for "hijacking" the socket from `facil.io`).

#### `fio_attach`

```c
void fio_attach(intptr_t uuid, fio_protocol_s *protocol);
```

Attaches (or updates) a protocol object to a connection's uuid.

The new protocol object can be NULL, which will detach ("hijack") the socket.

The old protocol's `on_close` (if any) will be scheduled.

On error, the new protocol's `on_close` callback will be called immediately.


#### `fio_attach_fd`

```c
void fio_attach_fd(int fd, fio_protocol_s *protocol);
```

Attaches (or updates) a protocol object to a file descriptor (fd).

The new protocol object can be NULL, which will detach ("hijack") the socket.

The `fd` can be one created outside of facil.io if it was set in to non-blocking mode (see [`fio_set_non_block`](#fio_set_non_block)).

The old protocol's `on_close` (if any) will be scheduled.

On error, the new protocol's `on_close` callback will be called immediately.


#### `fio_capa`

```c
size_t fio_capa(void);
```

Returns the maximum number of open files facil.io can handle per worker process.

In practice, this number represents the maximum `fd` value + 1.

Total OS limits might apply as well but aren't tested or known by facil.io.

The value of 0 indicates either that the facil.io library wasn't initialized
yet or that it's resources were already released.


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
void fio_force_event(intptr_t uuid, enum fio_io_event);
```

Schedules an IO event, even if it did not occur.

Possible events are:

* `FIO_EVENT_ON_DATA` - as if data is available to be read.
* `FIO_EVENT_ON_READY` - as if the connection can be written to (if there's data in the buffer, `fio_flush` will be called).
* `FIO_EVENT_ON_TIMEOUT` - as if the connection timed out (`ping`).

#### `fio_suspend`

```c
void fio_suspend(intptr_t uuid);
```

Temporarily prevents `on_data` events from firing.

Note: the function will work as expected when called within the protocol's `on_data` callback and the `uuid` refers to a valid socket. Otherwise the function might quietly fail.

### Listening to incoming connections

Listening to incoming connections is pretty straight forward and performed using the [`facil_listen`](#facil_listen) function.

After a new connection is accepted, the `on_open` callback passed to [`facil_listen`](#facil_listen) is called.

The `on_open` callback should allocate the new connection's protocol and call [`fio_attach`](#fio_attach) to attach a protocol to the connection's uuid.

The protocol's `on_close` callback is expected to handle any cleanup required.

The following is an example for a TCP/IP echo server using facil.io:

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
echo_proto = (fio_protocol_s){.service = "echo",
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

#### `fio_listen`

```c
intptr_t fio_listen(struct fio_listen_args args);
#define fio_listen(...) fio_listen((struct fio_listen_args){__VA_ARGS__})
```

The `fio_listen` function is shadowed by the `fio_listen` MACRO, which allows the function to accept "named arguments", as shown above in the example code:

```c
 if (fio_listen(.port = "3000", .on_open = echo_on_open) == -1) {
   perror("No listening socket available on port 3000");
   exit(-1);
 }
```

The following arguments are supported:

* `on_open`:

    This callback will be called whenever a new connection is accepted. It **must** either call [`fio_attach`](#fio_attach) or close the connection.

    The callback should accept the new connection's uuid and a void pointer (the optional `udata` pointer one passed to `fio_listen`)

        // callback example:
        void on_open(intptr_t uuid, void *udata);

* `port`:

    The network service / port. A NULL or "0" port indicates a Unix socket should be used.

        // type:
        const char *port;

* `address`:

    The socket binding address. Defaults to the recommended NULL (recommended for TCP/IP).

        // type:
        const char *address;

* `udata`:

    Opaque user data. This will be passed along to the `on_open` callback.

        // type:
        void *udata;


* `on_start`:

    A callback to be called when the server starts (or a worker process is re-spawned), allowing for further initialization, such as timed event scheduling or VM initialization.

        // callback example:
        void on_start(intptr_t uuid, void *udata);


* `on_finish`:

    A callback to be called for every process once the listening socket is closed.

        // callback example:
        void on_finish(intptr_t uuid, void *udata);



### Connecting to remote servers as a client

Establishing a client connection is about as easy as setting up a listening socket, and follows, give or take, the same procedure.

#### `fio_connect`

```c
intptr_t fio_connect(struct fio_connect_args args);
#define fio_connect(...) fio_connect((struct fio_connect_args){__VA_ARGS__})
```

The `fio_connect` function is shadowed by the `fio_connect` MACRO, which allows the function to accept "named arguments", similar to [`fio_listen](#fio_listen).

The following arguments are supported:

* `port`:

    The remote network service / port. A NULL or "0" port indicates a Unix socket connection should be attempted.

        // type:
        const char *port;

* `address`:

    The remote (or Unix socket) address. Defaults to the recommended NULL (recommended for TCP/IP).

        // type:
        const char *address;

* `on_connect`:

    This callback will be called once the new connection is established. It **must** either call [`fio_attach`](#fio_attach) or close the connection.

    The callback should accept the new connection's uuid and a void pointer (the optional `udata` pointer one passed to `fio_connect`)

        // callback example:
        void on_connect(intptr_t uuid, void *udata);

* `on_connect`:

    This callback will be called when a socket fails to connect. It's often a good place for cleanup.

    The callback should accept the attempted connection's uuid (might be -1) and a void pointer (the optional `udata` pointer one passed to `fio_connect`)

        // callback example:
        void on_connect(intptr_t uuid, void *udata);


* `udata`:

    Opaque user data. This will be passed along to the `on_connect` or `on_fail` callback.

        // type:
        void *udata;

* `timeout`:

    A non-system timeout after which connection is assumed to have failed.

        // type:
        uint8_t timeout;


### Manual Protocol Locking

#### `fio_protocol_try_lock`

```c
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid, enum fio_protocol_lock_e);
```

This function allows out-of-task access to a connection's `fio_protocol_s` object by attempting to acquire a locked pointer.

CAREFUL: mostly, the protocol object will be locked and a pointer will be sent to the connection event's callback. However, if you need access to the protocol object from outside a running connection task, you might need to lock the protocol to prevent it from being closed / freed in the background.

facil.io uses three different locks (see [`fio_defer_io_task`](#fio_defer_io_task) for more information):

* `FIO_PR_LOCK_TASK` locks the protocol for normal tasks (i.e. `on_data`).

* `FIO_PR_LOCK_WRITE` locks the protocol for high priority `fio_write`
oriented tasks (i.e. `ping`, `on_ready`).

* `FIO_PR_LOCK_STATE` locks the protocol for quick operations that need to copy
data from the protocol's data structure.

IMPORTANT: Remember to call [`fio_protocol_unlock`](fio_protocol_unlock) using the same lock type.

Returns a pointer to a protocol object on success and NULL on error and setting `errno` (lock busy == `EWOULDBLOCK`, connection invalid == `EBADF`).

On error, consider calling `fio_defer` or `fio_defer_io_task` instead of busy waiting. Busy waiting SHOULD be avoided whenever possible.

#### `fio_protocol_unlock`

```c
void fio_protocol_unlock(fio_protocol_s *pr, enum fio_protocol_lock_e);
```

Don't unlock what you didn't lock with `fio_protocol_try_lock`... see [`fio_protocol_try_lock`](#fio_protocol_try_lock) for details.

## Running facil.io

The facil.io IO reactor can be started in single-threaded, multi-threaded, forked (multi-process) and hybrid (forked + multi-threads) modes.

In cluster mode (when running more than a single process), a crashed worker process will be automatically re-spawned and "hot restart" is enabled (using the USR1 signal).

#### `fio_start`

```c
void fio_start(struct fio_start_args args);
#define fio_start(...) fio_start((struct fio_start_args){__VA_ARGS__})
```

Starts the facil.io event loop. This function will return after facil.io is done (after shutdown).

This method blocks the current thread until the server is stopped (when a SIGINT/SIGTERM is received).

The `fio_start` function is shadowed by the `fio_start` MACRO, which allows the function to accept "named arguments", i.e.:

```c
fio_start(.threads = 4, .workers = 2);
```

The following arguments are supported:

* `threads`:

    The number of threads to run in the thread pool.

        // type:
        int16_t threads;

* `workers`:

    The number of worker processes to run (in addition to a root process)

    This invokes facil.io's cluster mode, where a crashed worker will be automatically re-spawned and "hot restart" is enabled (using the USR1 signal).

        // type:
        int16_t workers;

Negative thread / worker values indicate a fraction of the number of CPU cores. i.e., -2 will normally indicate "half" (1/2) the number of cores.

If the other option (i.e. `.workers` when setting `.threads`) is zero, it will be automatically updated to reflect the option's absolute value. i.e.: if .threads == -2 and .workers == 0, than facil.io will run 2 worker processes with (cores/2) threads per process.

#### `fio_stop`

```c
void fio_stop(void);
```

Attempts to stop the facil.io application. This only works within the Root
process. A worker process will simply re-spawn itself (hot-restart).

#### `fio_expected_concurrency`

```c
void fio_expected_concurrency(int16_t *threads, int16_t *workers);
```

Returns the number of expected threads / processes to be used by facil.io.

The pointers should start with valid values that match the expected threads /
processes values passed to `fio_start`.

The data in the pointers will be overwritten with the result.

#### `fio_is_running`

```c
int16_t fio_is_running(void);
```

Returns the number of worker processes if facil.io is running (1 is returned when in single process mode, otherwise the number of workers).

Returns 0 if facil.io isn't running or is winding down (during shutdown).

#### `fio_is_worker`

```c
int fio_is_worker(void);
```

Returns 1 if the current process is a worker process or a single process.

Otherwise returns 0.

NOTE: When cluster mode is off, the root process is also the worker process. This means that single process instances don't automatically re-spawn after critical errors.


#### `fio_parent_pid`

```c
pid_t fio_parent_pid(void);
```

Returns facil.io's parent (root) process pid.

#### `fio_reap_children`

```c
void fio_reap_children(void);
```

Initializes zombie reaping for the process. Call before `fio_start` to enable
global zombie reaping.

#### `fio_last_tick`

```c
struct timespec fio_last_tick(void);
```

Returns the last time facil.io reviewed any pending IO events.

#### `fio_engine`

```c
char const *fio_engine(void);
```

Returns a C string detailing the IO engine selected during compilation.

Valid values are "kqueue", "epoll" and "poll".

## Socket / Connection Functions

### Creating, closing and testing sockets

#### `fio_socket`

```c
intptr_t fio_socket(const char *address, const char *port, uint8_t is_server);
```

Creates a TCP/IP or Unix socket and returns it's unique identifier.

For TCP/IP server sockets (`is_server` is `1`), a NULL `address` variable is recommended. Use "localhost" or "127.0.0.1" to limit access to the server application.

For TCP/IP client sockets (`is_server` is `0`), a remote `address` and `port` combination will be required

For Unix server or client sockets, set the `port` variable to NULL or `0` (and the `is_server` to `1`).

Returns -1 on error. Any other value is a valid unique identifier.

Note: facil.io uses unique identifiers to protect sockets from collisions. However these identifiers can be converted to the underlying file descriptor using the [`fio_uuid2fd`](#fio_uuid2fd) macro.


#### `fio_accept`

```c
intptr_t fio_accept(intptr_t srv_uuid);
```

`fio_accept` accepts a new socket connection from a server socket - see the server flag on [`fio_socket`](#fio_socket).

NOTE: this function does NOT attach the socket to the IO reactor - see [`fio_attach`](#fio_attach).

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

`fio_close` marks the connection for disconnection once all the data was sent. The actual disconnection will be managed by the [`fio_flush`](#fio_flush) function.

[`fio_flash`](#fio_flash) will be automatically scheduled.

#### `fio_force_close`

```c
void fio_force_close(intptr_t uuid);
```

`fio_force_close` closes the connection immediately, without adhering to any protocol restrictions and without sending any remaining data in the connection buffer.

#### `fio_set_non_block`

```c
int fio_set_non_block(int fd);
```

Sets a socket to non blocking state.

This function is called automatically for the new socket, when using
`fio_socket`, `fio_accept`, `fio_listen` or `fio_connect`.

Call this function before attaching an `fd` that was created outside of facil.io.

#### `fio_peer_addr`

```c
fio_str_info_s fio_peer_addr(intptr_t uuid);
```

Returns the information available about the socket's peer address.

If no information is available, the structure will be initialized with zero (`.data == NULL`).

The information is only available when the socket was accepted using `fio_accept` or opened using `fio_connect`.

#### The `fio_str_info_s` return value

```c
typedef struct fio_str_info_s {
  size_t capa; /* Buffer capacity, if the string is writable. */
  size_t len;  /* String length. */
  char *data;  /* Pointer to the string's first byte. */
} fio_str_info_s;
```

A string information type, reports information about a C string.

### Reading / Writing

#### `fio_read`

```c
ssize_t fio_read(intptr_t uuid, void *buffer, size_t count);
```

`fio_read` attempts to read up to count bytes from the socket into the buffer starting at `buffer`.

`fio_read`'s return values are wildly different then the native return values and they aim at making far simpler sense.

`fio_read` returns the number of bytes read (0 is a valid return value which simply means that no bytes were read from the buffer).

On a fatal connection error that leads to the connection being closed (or if the connection is already closed), `fio_read` returns -1.

The value 0 is the valid value indicating no data was read.

Data might be available in the kernel's buffer while it is not available to be read using `fio_read` (i.e., when using a transport layer, such as TLS, with Read/Write hooks).


#### `fio_write2`

```c
ssize_t fio_write2_fn(intptr_t uuid, fio_write_args_s options);
#define fio_write2(uuid, ...)                                                  \
 fio_write2_fn(uuid, (fio_write_args_s){__VA_ARGS__})
```

Schedules data to be written to the socket.

`fio_write2` is similar to `fio_write`, except that it allows far more flexibility.

NOTE: The data is "moved" to the ownership of the socket, not copied. By default, `free` (not `fio_free` will be called to deallocate the data. This can be controlled by the `.after.dealloc` function pointer argument.


The following arguments are supported (in addition to the `uuid` argument):

* `data.buffer` OR `data.fd`:

    The data to be sent. This can be either a block of memory or a file descriptor.

        // type:
        union {
           /** The in-memory data to be sent. */
           const void *buffer;
           /** The data to be sent, if this is a file. */
           const intptr_t fd;
        } data;

* `after.dealloc` OR `after.close`:

    The deallocation function. This function will be called to either free the memory of close the file once the data was sent.

    The default for memory is the system's `free` and for files the system `close` is called (using a wrapper function).

    A no-operation (do nothing) function is provided for sending static data: `FIO_DEALLOC_NOOP`. Use this: `.after.dealloc = FIO_DEALLOC_NOOP`

    Note: connection library functions MUST NEVER be called by a callback, or a deadlock might occur.

        // type:
        union {
          void (*dealloc)(void *buffer);
          void (*close)(intptr_t fd);
        } after;

* `length`:

    The length (size) of the buffer, or the amount of data to be sent from the file descriptor.

        // type:
        uintptr_t length;

* `offset`:

    Starting point offset from the buffer or file descriptor's beginning.

        // type:
        uintptr_t offset;

* `urgent`:

    The data will be sent as soon as possible, moving forward in the connection's queue as much as possible without fragmenting other `fio_write2` calls.

        // type:
        unsigned urgent : 1;

* `is_fd`:

    The `data` union contains the value of a file descriptor (`intptr_t`). i.e.: `.data.fd = fd` or `.data.buffer = (void*)fd;`

        // type:
        unsigned is_fd : 1;




On error, -1 will be returned. Otherwise returns 0.


#### `fio_write`

```c
inline ssize_t fio_write(const intptr_t uuid, const void *buffer,
                         const size_t length);
```

`fio_write` copies `legnth` data from the buffer and schedules the data to
be sent over the socket.

On error, -1 will be returned. Otherwise returns 0.

Returns the same values as `fio_write2` and is equivalent to:

```c
inline ssize_t fio_write(const intptr_t uuid, const void *buffer,
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
```


#### `fio_sendfile`

```c
inline static ssize_t fio_sendfile(intptr_t uuid, intptr_t source_fd,
                                   off_t offset, size_t length);
```

Sends data from a file as if it were a single atomic packet (sends up to `length` bytes or until EOF is reached).

Once the file was sent, the `source_fd` will be closed using `close`.

The file will be buffered to the socket chunk by chunk, so that memory consumption is capped. The system's `sendfile` might be used if conditions permit.

`offset` dictates the starting point for the data to be sent and length sets the maximum amount of data to be sent.

Returns -1 and closes the file on error. Returns 0 on success.

Returns the same values as `fio_write2` and is equivalent to:

```c
inline ssize_t fio_sendfile(intptr_t uuid, intptr_t source_fd,
                                    off_t offset, size_t length) {
 return fio_write2(uuid, .data.fd = source_fd, .length = length, .is_fd = 1,
                   .offset = offset);
}
```

#### `fio_pending`

```c
size_t fio_pending(intptr_t uuid);
```

Returns the number of `fio_write` calls that are waiting in the connection's queue and haven't been processed.

#### `fio_flush`

```c
ssize_t fio_flush(intptr_t uuid);
```

`fio_flush` attempts to write any remaining data in the internal buffer to the underlying file descriptor and closes the underlying file descriptor once if it's marked for closure (and all the data was sent).

Return values: 1 will be returned if data remains in the buffer. 0 will be returned if the buffer was fully drained. -1 will be returned on an error or when the connection is closed.

`errno` will be set to EWOULDBLOCK if the socket's lock is busy.


#### `fio_flush_strong`

```c
#define fio_flush_strong(uuid)                                                \
 do {                                                                         \
   errno = 0;                                                                 \
 } while (fio_flush(uuid) > 0 || errno == EWOULDBLOCK)
```

Blocks until all the data was flushed from the buffer.

#### `fio_flush_all`

```c
void fio_flush_all(void);
```
`fio_flush_all` attempts flush all the open connections.

#### `fio_uuid2fd`

```c
#define fio_uuid2fd(uuid) ((int)((uintptr_t)uuid >> 8))
```

Convert between a facil.io connection's identifier (uuid) and system's fd.

#### `fio_fd2uuid`

```c
intptr_t fio_fd2uuid(int fd);
```

`fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active `uuid`.

If the file descriptor was closed, **it will be registered as open**.

If the file descriptor was closed directly (not using `fio_close`) or the closure event hadn't been processed, a false positive will be possible.

This is not an issue, since the use of an invalid fd will result in the registry being updated and the fd being closed.

Returns -1 on error. Returns a valid socket (non-random) UUID.

### Connection Object Links

Connection object links can links an object to a connection's lifetime rather than it's Protocol's lifetime.

This is can be useful and is used internally by the `fio_subscribe` function to attach subscriptions to connections (when requested).


#### `fio_uuid_link`

```c
void fio_uuid_link(intptr_t uuid, void *obj, void (*on_close)(void *obj));
```

Links an object to a connection's lifetime, calling the `on_close` callback once the connection has died.

If the `uuid` is invalid, the `on_close` callback will be called immediately.

*NOTE*: the `on_close` callback will be called with high priority. Long tasks should be deferred using [`fio_defer`](#fio_defer).

#### `fio_uuid_unlink`

```c
void fio_uuid_unlink(intptr_t uuid, void *obj);
```

Un-links an object from the connection's lifetime, so it's `on_close` callback will NOT be called.

### Lower-Level: Read / Write / Close Hooks

facil.io's behavior can be altered to support complex networking needs, such as SSL/TLS integration.

This can be achieved using connection hooks for the common read/write/close operations.

To do so, a `fio_rw_hook_s` object must be created (a static object can be used as well).

```c
typedef struct fio_rw_hook_s {
 ssize_t (*read)(intptr_t uuid, void *udata, void *buf, size_t count);
 ssize_t (*write)(intptr_t uuid, void *udata, const void *buf, size_t count);
 ssize_t (*close)(intptr_t uuid, void *udata);
 ssize_t (*flush)(intptr_t uuid, void *udata);
} fio_rw_hook_s;
```

* The `read` hook callback:

    This callback should implement the reading function from the file descriptor. It must behave the same as the system's `read` call, including the setting `errno` to `EAGAIN` / `EWOULDBLOCK`.

    Note: facil.io library functions MUST NEVER be called by any r/w hook, or a deadlock might occur.

* The `write` hook callback:

    This callback should implement the writing function to the file descriptor. It must behave like the file system's `write` call, including the setting `errno` to `EAGAIN` / `EWOULDBLOCK`.

    Note: facil.io library functions MUST NEVER be called by any r/w hook, or a deadlock might occur.

* The `close` hook callback:

    The `close` callback should close the underlying socket / file descriptor. It should also be used to release any resources associated with the connection's read/write hooks.

    If the function returns a non-zero value, it will be called again after an attempt to flush the socket and any pending outgoing buffer.

    Note: facil.io library functions MUST NEVER be called by any r/w hook, or a deadlock might occur.

* The `flush` hook callback:

    When implemented, this function will be called to flush any data remaining in the read/write hook's internal buffer.

    The function should return the number of bytes remaining in the internal buffer (0 is a valid response) or -1 (on error).

    Note: facil.io library functions MUST NEVER be called by any r/w hook, or a deadlock might occur.


#### `fio_rw_hook_set`

```c
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata);
```

Sets a connection's hook callback object (`fio_rw_hook_s`).

Returns 0 on success or -1 on error (closed / invalid `uuid`).

#### `FIO_DEFAULT_RW_HOOKS`

```c
extern const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS;
```

The default Read/Write hooks used for system Read/Write (`udata` == NULL).

## Event / Task scheduling

facil.io allows a number of ways to schedule events / tasks:

* Queue - schedules an event / task to be performed as soon as possible.

* Timer - the event / task will be scheduled in the Queue after the designated period.

* State - the event / task will be called during a specific change in facil.io's state (starting up, cleaning up, etc').

### The Task Queue Functions

#### `fio_defer`

```c
int fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);
```

Defers a task's execution.

The task will be executed after all currently scheduled tasks (placed at the end of the scheduling queue).

Tasks are functions of the type `void task(void *, void *)`, they return nothing (void) and accept two opaque `void *` pointers, user-data 1 (`udata1`) and user-data 2 (`udata2`).

Returns -1 or error, 0 on success.

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


### Timer Functions

#### `fio_run_every`

```c
int fio_run_every(size_t milliseconds, size_t repetitions, void (*task)(void *),
                 void *arg, void (*on_finish)(void *));
```

Creates a timer to run a task at the specified interval.

Timer tasks accept only a single user data pointer (`udata` ).

The task will repeat `repetitions` times. If `repetitions` is set to 0, task
will repeat forever.

The `on_finish` handler is always called (even on error).

Returns -1 on error.

### Connection task scheduling

Connection tasks are performed within one of the connection's locks (`FIO_PR_LOCK_TASK`, `FIO_PR_LOCK_WRITE`, `FIO_PR_LOCK_STATE`), assuring a measure of safety.

#### `fio_defer_io_task`

```c
void fio_defer_io_task(intptr_t uuid, fio_defer_iotask_args_s args);
#define fio_defer_io_task(uuid, ...)                                           \
 fio_defer_connection_task((uuid), (fio_defer_iotask_args_s){__VA_ARGS__})
```

This function schedules an IO task using the specified lock type.

This function is shadowed by a macro, allowing it to accept named arguments, much like [fio_start](#fio_start). The following arguments are recognized:

* `type`:

    The type of lock that should be used to protect the IO task. Defaults to `FIO_PR_LOCK_TASK` (see later).

        // type:
        enum fio_protocol_lock_e type;

* `task`:

    The task (function) to be performed. The tasks accepts the connection's `uuid`, a pointer to the protocol object and the opaque `udata` pointer passed to `fio_defer_io_task`.

        // Callback example:
        void task(intptr_t uuid, fio_protocol_s *, void *udata);

* `fallback`:

    A fallback task (function) to be performed in cases where the `uuid` isn't valid by the time the task should be executed. The fallback task accepts the connection's `uuid` and the opaque `udata` pointer passed to `fio_defer_io_task`.

        // Callback example:
        void fallback(intptr_t uuid, void *udata);


* `udata`:

    An opaque pointer that's passed along to the task.

        // type:
        void *udata;

Lock types are one of the following:

* `FIO_PR_LOCK_TASK` - a task lock locks might change data owned by the protocol object. This task is used for tasks such as `on_data`.

* `FIO_PR_LOCK_WRITE` - a lock that promises only to use static data (data that tasks never changes) in order to write to the underlying socket. This lock is used for tasks such as `on_ready` and `ping`

* `FIO_PR_LOCK_STATE` - a lock that promises only to retrieve static data (data that tasks never changes), performing no actions. This usually isn't used for client side code (used internally by facil) and is only meant for very short locks.


### Startup / State Tasks (fork, start up, idle, etc')

facil.io allows callbacks to be called when certain events occur (such as before and after forking etc').

Callbacks will be called from last to first (last callback added executes first), allowing for logical layering of dependencies.

During an event, changes to the callback list are ignored (callbacks can't remove other callbacks for the same event).

#### `callback_type_e`- State callback timing type

The `fio_state_callback_*` functions manage callbacks for a specific timing. Valid timings values are:
 
 * `FIO_CALL_ON_INITIALIZE`: Called once during library initialization.
 
 * `FIO_CALL_PRE_START`: Called once before starting up the IO reactor.
 
 * `FIO_CALL_BEFORE_FORK`: Called before each time the IO reactor forks a new worker.
 
 * `FIO_CALL_AFTER_FORK`: Called after each fork (both in parent and workers).
 
 * `FIO_CALL_IN_CHILD`: Called by a worker process right after forking.
 
 * `FIO_CALL_ON_START`: Called every time a *Worker* proceess starts.
 
 * `FIO_CALL_ON_IDLE`: Called when facil.io enters idling mode.
 
 * `FIO_CALL_ON_SHUTDOWN`: Called before starting the shutdown sequence.
 
 * `FIO_CALL_ON_FINISH`: Called just before finishing up (both on chlid and parent processes).
 
 * `FIO_CALL_ON_PARENT_CRUSH`: Called by each worker the moment it detects the master process crashed.
 
 * `FIO_CALL_ON_CHILD_CRUSH`: Called by the parent (master) after a worker process crashed.
 
 * `FIO_CALL_AT_EXIT`: An alternative to the system's at_exit.
 
#### `fio_state_callback_add`

```c
void fio_state_callback_add(callback_type_e, void (*func)(void *), void *arg);
```

Adds a callback to the list of callbacks to be called for the event.

Callbacks will be called from last to first (last callback added executes first), allowing for logical layering of dependencies.

#### `fio_state_callback_remove`

```c
int fio_state_callback_remove(callback_type_e, void (*func)(void *), void *arg);
```

Removes a callback from the list of callbacks to be called for the event.

Callbacks will be called from last to first (last callback added executes first), allowing for logical layering of dependencies.

#### `fio_state_callback_force`

```c
void fio_state_callback_force(callback_type_e);
```

Forces all the existing callbacks to run, as if the event occurred.

Callbacks will be called from last to first (last callback added executes first), allowing for logical layering of dependencies.

During an event, changes to the callback list are ignored (callbacks can't remove other callbacks for the same event).

#### `fio_state_callback_clear`

```c
void fio_state_callback_clear(callback_type_e);
```

Clears all the existing callbacks for the event (doesn't effect a currently firing event).

## Pub/Sub Services

facil.io supports a [Publish–Subscribe Pattern](https://en.wikipedia.org/wiki/Publish–subscribe_pattern) API which can be used for Inter Process Communication (IPC), messaging, horizontal scaling and similar use-cases.

### Subscription Control


#### `fio_subscribe`

```c
subscription_s *fio_subscribe(subscribe_args_s args);
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


* `filter`:

    If `filter` is set, all messages that match the filter's numerical value will be forwarded to the subscription's callback.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by filter.

    **NOTE**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks (both subjects are covered later on).

        // type:
        int32_t filter;

* `channel`:

    If `filter == 0` (or unset), than the subscription will be made using `channel` binary name matching. Note that and empty string (NULL pointer and 0 length) is a valid channel name.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by channel (only messages with no `filter` will be received).

        // type:
        fio_str_info_s channel;


* `match`:

    If an optional `match` callback is provided, pattern matching will be used as an alternative to exact channel name matching.

    A single matching function is bundled with facil.io (`FIO_MATCH_GLOB`), which follows the Redis matching logic.

    This is significantly slower, as no Hash Map can be used to locate a match - each message published to a channel will be tested for a match against each pattern.

        // callback example:
        int foo_bar_match_fn(fio_str_info_s pattern, fio_str_info_s channel);
        // type:
        typedef int (*fio_match_fn)(fio_str_info_s pattern, fio_str_info_s channel);
        fio_match_fn match;
        extern fio_match_fn FIO_MATCH_GLOB;

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
 fio_str_info_s msg;
 void *udata1;
 void *udata2;
 uint8_t is_json;
} fio_msg_s;
```

* `filter` is the numerical filter (if any) used in the `fio_subscribe` and `fio_publish` functions. Negative values are reserved and 0 == channel name matching.

* `channel` is an immutable binary string containing the channel name given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details.

* `msg` is an immutable binary string containing the message data given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details.

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

This function will block if a subscription task is running on a different thread.

The subscription task won't be called after the function returns.

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

    * `FIO_PUBSUB_CLUSTER` - used to publish the message to all clients in the cluster.

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

**NOTE**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

#### `fio_message_metadata`

```c
void *fio_message_metadata(fio_msg_s *msg, intptr_t type_id);
```

Finds the message's meta-data by the meta-data's type ID. Returns the data or NULL.

#### `fio_message_metadata_callback_set`

```c
// The function:
void fio_message_metadata_callback_set(fio_msg_metadata_fn callback,
                                      int enable);
// The callback type:
typedef fio_msg_metadata_s (*fio_msg_metadata_fn)(fio_str_info_s ch,
                                                 fio_str_info_s msg,
                                                 uint8_t is_json);
// Example callback
fio_msg_metadata_s foo_metadata(fio_str_info_s ch,
                                fio_str_info_s msg,
                                uint8_t is_json);
```

The callback should return a `fio_msg_metadata_s` object. The object looks like this:

```c
typedef struct fio_msg_metadata_s fio_msg_metadata_s;
struct fio_msg_metadata_s {
 /** A type ID used to identify the meta-data. Negative values are reserved. */
 intptr_t type_id;
 /** Called by facil.io to cleanup the meta-data resources. */
 void (*on_finish)(fio_msg_s *msg, void *metadata);
 /** The pointer to be disclosed to the subscription client. */
 void *metadata;
 /** RESERVED for internal use (Metadata linked list): */
 fio_msg_metadata_s *next;
};
```

If the the returned `.metadata` field is NULL than the result will be ignored.

To remove a callback, set the `enable` flag to false (`0`).

The cluster messaging system allows some messages to be flagged as JSON and this flag is available to the meta-data callback.

### External Pub/Sub Services

facil.io can be linked with external Pub/Sub services using "engines" (`fio_pubsub_engine_s`).

Pub/Sub engines dictate the behavior of the pub/sub instructions. 

This allows for an application to connect to a service such as Redis or NATs for horizontal pub/sub scaling.

A [Redis engine](redis) is bundled as part of the facio.io extensions but isn't part of the core library.

The default engine can be set using the `FIO_PUBSUB_DEFAULT` global variable. It's initial default is `FIO_PUBSUB_CLUSTER` (see [`fio_publish`](#fio_publish)).

**NOTE**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

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

**NOTE**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


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

**NOTE**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


**IMPORTANT**: The `subscribe` and `unsubscribe` callbacks are called from within an internal lock. They MUST NEVER call pub/sub functions except by exiting the lock using `fio_defer`.

#### `fio_pubsub_is_attached`

```c
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);
```

Returns true (1) if the engine is attached to the system.


## The Custom Memory Allocator

facil.io includes a custom memory allocator designed for network application use.

Allocated memory is always zeroed out and aligned on a 16 byte boundary.

Reallocated memory is always aligned on a 16 byte boundary but it might be filled with junk data after the valid data (this can be minimized by using [`fio_realloc2`](#fio_realloc2)).

The memory allocator assumes multiple concurrent allocation/deallocation, short to medium life spans (memory is freed shortly, but not immediately, after it was allocated) and relatively small allocations (anything over 12Kb is forwarded to `mmap`).

This allocator should prevent memory fragmentation when allocating memory for shot / medium object life-spans (classic network use).

**Note**: this custom allocator could increase memory fragmentation if long-life allocations are performed periodically (rather than performed during startup). Use [`fio_mmap`](#fio_mmap) or the system's `malloc` for long-term allocations.

### Memory Allocator Overview

The easiest and fastest allocator that can be written will simply claim memory at top of the stack (using the historic `sbrk` instruction) and never free the memory.

This memory allocator works using a similar design using rotating blocks of 32Kb which are only freed once all the references to the block are freed.

The allocator utilizes memory pools and per-CPU bins to allow for concurrent memory allocations across threads and to minimize lock contention.

To replace the system's `malloc` function family compile with the `FIO_OVERRIDE_MALLOC` defined (`-DFIO_OVERRIDE_MALLOC`).

When using tcmalloc or jemalloc, consider defining `FIO_FORCE_MALLOC` to prevent facil.io's custom allocator from compiling (`-DFIO_FORCE_MALLOC`).

More details in the `fio.h` header.

### The Memory Allocator's API

The functions were designed to be a drop in replacement to the system's memory allocation functions (`malloc`, `free` and friends).

Where some improvement could be made, it was made using an added function name to add improved functionality (such as `fio_realloc2`).

#### `fio_malloc`

```c
void *fio_malloc(size_t size)
```

Allocates memory using a per-CPU core block memory pool.

Memory is always zeroed out.

Allocations above `FIO_MEMORY_BLOCK_ALLOC_LIMIT` (12,288 bytes when using the default 32Kb
blocks) will be redirected to `mmap`, as if `fio_mmap` was called.

#### `fio_calloc`

```c
void *fio_calloc(size_t size_per_unit, size_t unit_count)
```

Same as calling `fio_malloc(size_per_unit * unit_count)`;

Allocations above `FIO_MEMORY_BLOCK_ALLOC_LIMIT` (12,288 bytes when using 32Kb
blocks) will be redirected to `mmap`, as if `fio_mmap` was called.

#### `fio_free`

```c
void fio_free(void *ptr);
```

Frees memory that was allocated using this library.

#### `fio_realloc`

```c
void *fio_realloc(void *ptr, size_t new_size);
```

Re-allocates memory. An attempt to avoid copying the data is made only for big
memory allocations (larger than `FIO_MEMORY_BLOCK_ALLOC_LIMIT`).

#### `fio_realloc2`

```c
void *fio_realloc2(void *ptr, size_t new_size, size_t copy_length);
```

Re-allocates memory. An attempt to avoid copying the data is made only for big
memory allocations (larger than `FIO_MEMORY_BLOCK_ALLOC_LIMIT`).

This variation is slightly faster as it might copy less data.

####

```c
void *fio_mmap(size_t size);
```

Allocates memory directly using `mmap`, this is prefered for objects that
both require almost a page of memory (or more) and expect a long lifetime.

However, since this allocation will invoke the system call (`mmap`), it will
be inherently slower.

`fio_free` can be used for deallocating the memory.

## Linked Lists

Linked list helpers are inline functions that become available when (and if) the `fio_h` file is included with the `FIO_INCLUDE_LINKED_LIST` macro.

This can be performed even after if the `fio.h` file was previously included. i.e.:

```c
#include <fio.h> // No linked list helpers
#define FIO_INCLUDE_LINKED_LIST
#include <fio.h> // Linked list helpers become available
```

Linked lists come in two types:

1. Independent Object Lists:

    Used when a single object might belong to more than a single list, or when the object can't be edited.

1. Embedded Linked Lists

    Used when a single object always belongs to a single list and can be edited to add a `node` filed. This improves memory locality and performance, as well as minimizes memory allocations.

### Independent Linked List API

The independent object lists uses the following type:

```c
/** an independent linked list. */
typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const void *obj;
} fio_ls_s;
```

It can be initialized using the `FIO_LS_INIT` macro.


#### `FIO_LS_INIT`

```c
#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }
```

Initializes the list container. i.e.:

```c
fio_ls_s my_list = FIO_LS_INIT(my_list);
```

#### `fio_ls_push`

```c
inline void fio_ls_push(fio_ls_s *pos, const void *obj);
```

Adds an object to the list's head.

#### `fio_ls_unshift`

```c
inline void fio_ls_unshift(fio_ls_s *pos, const void *obj);
```

Adds an object to the list's tail.

#### `fio_ls_pop`

```c
inline void *fio_ls_pop(fio_ls_s *list);
```

Removes an object from the list's head.

#### `fio_ls_shift`

```c
inline void *fio_ls_shift(fio_ls_s *list);
```

Removes an object from the list's tail.

#### `fio_ls_remove`

```c
inline void *fio_ls_remove(fio_ls_s *node);
```

Removes a node from the list, returning the contained object.

#### `fio_ls_is_empty`

```c
inline int fio_ls_is_empty(fio_ls_s *list);
```

Tests if the list is empty.

#### `fio_ls_any`

```c
inline int fio_ls_any(fio_ls_s *list);
```

Tests if the list is NOT empty (contains any nodes).

#### `FIO_LS_FOR`

```c
#define FIO_LS_FOR(list, pos)
```

Iterates through the list using a `for` loop.

Access the data with `pos->obj` (`pos` can be named however you please).
 

### Embedded Linked List API

The embedded object lists uses the following node type:

```c
/** an embeded linked list. */
typedef struct fio_ls_embd_s {
  struct fio_ls_embd_s *prev;
  struct fio_ls_embd_s *next;
} fio_ls_embd_s;
```

It can be initialized using the [`FIO_LS_INIT`](#FIO_LS_INIT) macro (see above).

#### `fio_ls_embd_push`

```c
inline void fio_ls_embd_push(fio_ls_embd_s *dest, fio_ls_embd_s *node);
```

Adds a node to the list's head.

#### `fio_ls_embd_unshift`

```c
inline void fio_ls_embd_unshift(fio_ls_embd_s *dest, fio_ls_embd_s *node);
```

Adds a node to the list's tail.

#### `fio_ls_embd_pop`

```c
inline fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list);
```

Removes a node from the list's head.

#### `fio_ls_embd_shift`

```c
inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list);
```

Removes a node from the list's tail.

#### `fio_ls_embd_remove`

```c
inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node);
```

Removes a node from the containing node.

#### `fio_ls_embd_is_empty`

```c
inline int fio_ls_embd_is_empty(fio_ls_embd_s *list);
```

Tests if the list is empty.

#### `fio_ls_embd_any`

```c
inline int fio_ls_embd_any(fio_ls_embd_s *list);
```

Tests if the list is NOT empty (contains any nodes).

#### `FIO_LS_EMBD_FOR`

```c
#define FIO_LS_EMBD_FOR(list, node)
```

Iterates through the list using a `for` loop.

Access the data with `pos->obj` (`pos` can be named however you please).

#### `FIO_LS_EMBD_OBJ`

```c
#define FIO_LS_EMBD_OBJ(type, member, plist)                                   \
  ((type *)((uintptr_t)(plist) - (uintptr_t)(&(((type *)0)->member))))
```

Takes a list pointer `plist` (node) and returns a pointer to it's container.
 
This uses pointer offset calculations and can be used to calculate any structure's pointer (not just list containers) as an offset from a pointer of one of it's members.
 
Very useful.

## String Helpers

String helpers are inline functions that become available when (and if) the `fio_h` file is included with the `FIO_INCLUDE_STR` macro.

This can be performed even after if the `fio.h` file was previously included. i.e.:

```c
#include <fio.h> // No string helpers
#define FIO_INCLUDE_STR
#include <fio.h> // String helpers become available
```

### String API - Initialization and Destruction

The String API is used to manage binary strings, allowing the NUL byte to be a valid byte in the middle of the string (unlike C strings)

The String API uses the type `fio_str_s`, which shouldn't be accessed directly.

The `fio_str_s` objects can be allocated either on the stack or on the heap.

However, reference counting provided by the `fio_free2` and the `fio_str_send_free2` functions, requires that the `fio_str_s` object be allocated on the heap using `fio_malloc` or `fio_str_new2`.

#### `FIO_STR_INIT`

```c
#define FIO_STR_INIT ((fio_str_s){.data = NULL, .small = 1})
```

This value should be used for initialization. For example:

```c
// on the stack
fio_str_s str = FIO_STR_INIT;

// or on the heap
fio_str_s *str = fio_malloc(sizeof(*str);
*str = FIO_STR_INIT;
```

Remember to cleanup:

```c
// on the stack
fio_str_free(&str);

// or on the heap
fio_str_free(str);
fio_free(str);
```


#### `FIO_STR_INIT_EXISTING`

```c
#define FIO_STR_INIT_EXISTING(buffer, length, capacity)                        \
 ((fio_str_s){.data = (buffer),                                               \
              .len = (length),                                                \
              .capa = (capacity),                                             \
              .dealloc = fio_free})
```

This macro allows the container to be initialized with existing data, as long as it's memory was allocated using `fio_malloc`.

The `capacity` value should exclude the NUL character (if exists).

#### `FIO_STR_INIT_STATIC`

```c
#define FIO_STR_INIT_STATIC(buffer)                                            \
 ((fio_str_s){.data = (buffer), .len = strlen((buffer)), .dealloc = NULL})
```

This macro allows the container to be initialized with existing data, as long as it's memory was allocated using `fio_malloc`.

The `capacity` value should exclude the NUL character (if exists).

#### `fio_str_new2`

```c
inline fio_str_s *fio_str_new2(void);
```

Allocates a new fio_str_s object on the heap and initializes it.

Use `fio_str_free2` to free both the String data and the container.

NOTE: This makes the allocation and reference counting logic more intuitive.


#### `fio_str_new_copy2`

```c
inline fio_str_s *fio_str_new_copy2(fio_str_s *src);
```

Allocates a new fio_str_s object on the heap, initializes it and copies the
original (`src`) string into the new string.

Use `fio_str_free2` to free the new string's data and it's container.

#### `fio_str_dup`

```c
inline fio_str_s *fio_str_dup(fio_str_s *s);
```

Adds a references to the current String object and returns itself.

NOTE: Nothing is copied, reference Strings are referencing the same String. Editing one reference will effect the other.

The original's String's container should remain in scope (if on the stack) or remain allocated (if on the heap) until all the references were freed using `fio_str_free` / `fio_str_free2` or discarded.

#### `fio_str_free`

```c
inline int fio_str_free(fio_str_s *s);
```

Frees the String's resources and reinitializes the container.

Note: if the container isn't allocated on the stack, it should be freed
separately using `free` oR `fio_free`.

Returns 0 if the data was freed and -1 if the String is NULL or has un-freed
references (see fio_str_dup).

#### `fio_str_free2`

```c
void fio_str_free2(fio_str_s *s);
```

Frees the String's resources AS WELL AS the container.

Note: the container is freed using `fio_free`, make sure `fio_malloc` was used to allocate it.

#### `fio_str_send_free2`

```c
inline ssize_t fio_str_send_free2(const intptr_t uuid,
                                  const fio_str_s *str);
```

`fio_str_send_free2` sends the fio_str_s using `fio_write2`, freeing both the String and the container once the data was sent.

As the naming indicates, the String is assumed to have been allocated using `fio_str_new2` or `fio_malloc`.

### String API - String state (data pointers, length, capacity, etc')

Many of the String state functions return a `fio_str_info_s` structure with information about the String:

```c
typedef struct {
 size_t capa; /* String capacity */
 size_t len;  /* String length   */
 char *data;  /* String data     */
} fio_str_info_s;
```

Using this approach is safer than accessing the String data directly, since the short Strings behave differently than long strings and the `fio_str_s` structure fields are only valid for long strings.

#### `fio_str_info`

```c
inline fio_str_info_s fio_str_info(const fio_str_s *s);
```

Returns the String's complete state (capacity, length and pointer). 

#### `fio_str_len`

```c
inline size_t fio_str_len(fio_str_s *s);
```

Returns the String's length in bytes.

#### `fio_str_data`

```c
inline char *fio_str_data(fio_str_s *s);
```

Returns a pointer (`char *`) to the String's content.

#### `fio_str_bytes`

```c
#define fio_str_bytes(s) ((uint8_t *)fio_str_data((s)))
```

Returns a byte pointer (`uint8_t *`) to the String's unsigned content.

#### `fio_str_capa`

```c
inline size_t fio_str_capa(fio_str_s *s);
```

Returns the String's existing capacity (total used & available memory).

#### `fio_str_resize`

```c
inline fio_str_info_s fio_str_resize(fio_str_s *s, size_t size);
```

Sets the new String size without reallocating any memory (limited by existing capacity).

Returns the updated state of the String.

Note: When shrinking, any existing data beyond the new size may be corrupted.

#### `fio_str_clear`

```c
#define fio_str_clear(s) fio_str_resize((s), 0)
```

Clears the string (retaining the existing capacity).

#### `fio_str_hash`

```c
inline uint64_t fio_str_hash(const fio_str_s *s);
```

Returns the string's SipHash value (Uses SipHash 1-3).

### String API - Memory management

#### `fio_str_compact`

```c
void fio_str_compact(fio_str_s *s);
```

Performs a best attempt at minimizing memory consumption.

Actual effects depend on the underlying memory allocator and it's implementation. Not all allocators will free any memory.

#### `fio_str_capa_assert`

```c
fio_str_info_s fio_str_capa_assert(fio_str_s *s, size_t needed);
```

Requires the String to have at least `needed` capacity (including existing data).

Returns the current state of the String.

### String API - UTF-8 State

#### `fio_str_utf8_valid`

```c
size_t fio_str_utf8_valid(fio_str_s *s);
```

Returns 1 if the String is UTF-8 valid and 0 if not.

#### `fio_str_utf8_len`

```c
size_t fio_str_utf8_len(fio_str_s *s);
```

Returns the String's length in UTF-8 characters.

#### `fio_str_utf8_select`

```c
int fio_str_utf8_select(fio_str_s *s, intptr_t *pos, size_t *len);
```

Takes a UTF-8 character selection information (UTF-8 position and length) and updates the same variables so they reference the raw byte slice information.

If the String isn't UTF-8 valid up to the requested selection, than `pos` will be updated to `-1` otherwise values are always positive.

The returned `len` value may be shorter than the original if there wasn't enough data left to accomodate the requested length. When a `len` value of `0` is returned, this means that `pos` marks the end of the String.

Returns -1 on error and 0 on success.

#### `FIO_STR_UTF8_CODE_POINT`

```c
#define FIO_STR_UTF8_CODE_POINT(ptr, end, i32) // ...
```

Advances the `ptr` by one utf-8 character, placing the value of the UTF-8 character into the i32 variable (which must be a signed integer with 32bits or more). On error, `i32` will be equal to `-1` and `ptr` will not step forwards.

The `end` value is only used for overflow protection.

This helper macro is used internally but left exposed for external use.

### String API - Content Manipulation and Review

#### `fio_str_write`

```c
inline fio_str_info_s fio_str_write(fio_str_s *s, const void *src,
                                    size_t src_len);
```

Writes data at the end of the String (similar to `fio_str_insert` with the argument `pos == -1`).


#### `fio_str_write_i`

```c
inline fio_str_info_s fio_str_write_i(fio_str_s *s, int64_t num);

```

Writes a number at the end of the String using normal base 10 notation.

#### `fio_str_concat`

```c
inline fio_str_info_s fio_str_concat(fio_str_s *dest,
                                     fio_str_s const *src);
```

Appens the `src` String to the end of the `dest` String.

If `dest` is empty, the resulting Strings will be equal.


#### `fio_str_join`

```c
#define fio_str_join(dest, src) fio_str_concat((dest), (src))
```

Alias for [`fio_str_concat`](#fio_str_concat).

#### `fio_str_replace`

```c
fio_str_info_s fio_str_replace(fio_str_s *s, intptr_t start_pos,
                               size_t old_len, const void *src,
                               size_t src_len);
```

Replaces the data in the String - replacing `old_len` bytes starting at `start_pos`, with the data at `src` (`src_len` bytes long).

Negative `start_pos` values are calculated backwards, `-1` == end of String.

When `old_len` is zero, the function will insert the data at `start_pos`.

If `src_len == 0` than `src` will be ignored and the data marked for replacement will be erased.

#### `fio_str_vprintf`

```c
fio_str_info_s fio_str_vprintf(fio_str_s *s, const char *format,
                               va_list argv);
```

Writes to the String using a vprintf like interface.

Data is written to the end of the String.

#### `fio_str_printf`

```c
fio_str_info_s fio_str_printf(fio_str_s *s, const char *format, ...);
```

Writes to the String using a printf like interface.

Data is written to the end of the String.

#### `fio_str_readfile`

```c
fio_str_info_s fio_str_readfile(fio_str_s *s, const char *filename,
                             intptr_t start_at, intptr_t limit);
```

Opens the file `filename` and pastes it's contents (or a slice ot it) at the
end of the String. If `limit == 0`, than the data will be read until EOF.

If the file can't be located, opened or read, or if `start_at` is beyond
the EOF position, NULL is returned in the state's `data` field.

Works on POSIX only.

#### `fio_str_freeze`

```c
inline void fio_str_freeze(fio_str_s *s);
```

Prevents further manipulations to the String's content.

#### `fio_str_iseq`

```c
inline int fio_str_iseq(const fio_str_s *str1, const fio_str_s *str2);
```

Binary comparison returns `1` if both strings are equal and `0` if not.

## Hash Maps / Sets

facil.io includes a simple ordered Hash Map / Set implementation, with a minimal API.

A Set is basically a Hash Map where the keys are also the values, it's often used for caching objects while a Hash Map is used to find one object using another.

A Hash Map is basically a set where the objects in the Set are key-value couplets and only the keys are tested when searching the Set.

The Set's object type and behavior is controlled by the FIO_SET_OBJ_* marcos.

To create a Set or a Hash Map, the macro FIO_SET_NAME must be defined. i.e.:

```c
#define FIO_SET_NAME fio_str_info_set
#define FIO_SET_OBJ_TYPE char *
#define FIO_SET_OBJ_COMPARE(k1, k2) (!strcmp((k1), (k2)))
#include <fio.h>
// ...
fio_str_info_set_s my_set = FIO_SET_INIT; // note type name matches FIO_SET_NAME
uint64_t hash = fio_siphash("foo", 3);
fio_str_info_set_insert(&my_set, hash, "foo"); // note function name
```

This can be performed a number of times, defining a different Set / Hash Map each time.

### Defining the Set / Hash Map

The Set's object type and behavior is controlled by the FIO_SET_OBJ_* marcos: `FIO_SET_OBJ_TYPE`, `FIO_SET_OBJ_COMPARE`, `FIO_SET_OBJ_COPY`, `FIO_SET_OBJ_DESTROY`. i.e.:

```c
#define FIO_INCLUDE_STR
#include <fio.h> // adds the fio_str_s types and functions

#define FIO_SET_NAME fio_str_set
#define FIO_SET_OBJ_TYPE fio_str_s *
#define FIO_SET_OBJ_COMPARE(s1, s2) fio_str_iseq((s1), (s2))
#define FIO_SET_OBJ_COPY(dest, src) (dest) = fio_str_new_copy2((src))
#define FIO_SET_OBJ_DESTROY(str) fio_str_free2((str))
#include <fio.h> // creates the fio_str_set_s Set and functions

int main(void) {
  fio_str_s tmp = FIO_STR_INIT_STATIC("foo");
  // Initialize a Set for String caching
  fio_str_set_s my_set = FIO_SET_INIT;
  // Insert object to Set (will make copy)
  fio_str_s **pcpy = fio_str_set_insert(&my_set, fio_str_hash(&tmp), &tmp);
  printf("Original data %p\n", (void *)&tmp);
  printf("Stored data %p\n", (void *)*pcpy);
  printf("Stored data content: %s\n", fio_str_data(*pcpy));
  // Free set (will free copy).
  fio_str_set_free(&my_set);
}

```

To create a Hash Map, rather than a pure Set, the macro FIO_SET_KET_TYPE must
be defined. i.e.:

```c
#define FIO_SET_KEY_TYPE char *
```

This allows the FIO_SET_KEY_* macros to be defined as well. For example:

```c
#define FIO_SET_NAME fio_str_hash
#define FIO_SET_KEY_TYPE char *
#define FIO_SET_KEY_COMPARE(k1, k2) (!strcmp((k1), (k2)))
#define FIO_SET_OBJ_TYPE char *
#include <fio.h>
```

#### `FIO_SET_OBJ_TYPE`

```c
// default:
#define FIO_SET_OBJ_TYPE void *
```

Set's a Set or a Hash Map's object type. Defaults to `void *`.

#### `FIO_SET_OBJ_COMPARE`

```c
// default:
#define FIO_SET_OBJ_COMPARE(o1, o2) (1)
```

Compares two Set objects. This is only relevant to pure Sets (not Hash Maps). Defaults to doing nothing (always true).

#### `FIO_SET_OBJ_COPY`

```c
// default:
#define FIO_SET_OBJ_COPY(dest, obj) ((dest) = (obj))
```

Copies an object's data from an external object to the internal storage. This allows String and other dynamic data to be copied and retained for the lifetime of the object.

#### `FIO_SET_OBJ_DESTROY`

```c
// default:
#define FIO_SET_OBJ_DESTROY(obj) ((void)0)
```

Frees any allocated memory / resources used by the object data. By default this does nothing.

#### `FIO_SET_KEY_TYPE`

```c
#undef FIO_SET_KEY_TYPE
```

By defining a key type, the Set will be converted to a Hash Map and it's API will be slightly altered to reflect this change.

By default, no key type is defined.

#### `FIO_SET_KEY_COMPARE`

```c
#define FIO_SET_KEY_COMPARE(key1, key2) ((key1) == (key2))
```

Compares two Hash Map keys.

This is only relevant if the `FIO_SET_KEY_TYPE` was defined.


#### `FIO_SET_KEY_COPY`

```c
#define FIO_SET_KEY_COPY(dest, key) ((dest) = (key))
```

Copies the key's data from an external key to the internal storage. This allows String and other dynamic data to be copied and retained for the lifetime of the key-value pair.

This is only relevant if the `FIO_SET_KEY_TYPE` was defined.

#### `FIO_SET_KEY_DESTROY`

```c
#define FIO_SET_KEY_DESTROY(key) ((void)0)
```

Frees any allocated memory / resources used by the key data. By default this does nothing.

This is only relevant if the `FIO_SET_KEY_TYPE` was defined.

#### `FIO_SET_REALLOC`

```c
#define FIO_SET_REALLOC(ptr, original_size, new_size, valid_data_length)       \
 realloc((ptr), (new_size))
```

Allows for custom memory allocation / deallocation routines.

#### `FIO_SET_CALLOC`

```c
#define FIO_SET_CALLOC(size, count) calloc((size), (count))
```

Allows for custom memory allocation / deallocation routines.

#### `FIO_SET_FREE`

```c
#define FIO_SET_FREE(ptr, size) free((ptr))
```

### Naming the Set / Hash Map

Because the type and function names are dictated by the `FIO_SET_NAME`, it's impossible to name the functions and types that will be created.

For the purpose of this documentation, the `FIO_NAME(name)` will mark a type / function name so that it's equal to `FIO_SET_NAME + "_" + name`.

i.e., if `FIO_SET_NAME == foo` than `FIO_SET_NAME(s) == foo_s`.

### Set / Hash Map Initialization

#### `FIO_NAME(s)`

The Set's / Hash Map's type name. It's content should be considered opaque.

#### `FIO_SET_INIT`

```c
#define FIO_SET_INIT { .capa = 0 }
```

Initializes the Set or the Hash Map.

#### `FIO_NAME(free)`

```c
void FIO_NAME(free)(FIO_NAME(s) * set);
```

Deallocates any internal resources.

### Hash Map Find / Insert

These functions are defined if the Set defined is a Hash Map (`FIO_SET_KEY_TYPE` was defined).

#### `FIO_NAME(find)` (Hash Map)

```c
inline FIO_SET_OBJ_TYPE *
   FIO_NAME(find)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                  FIO_SET_KEY_TYPE key);
```
Locates an object in the Set, if it exists.

NOTE: This is the function's Hash Map variant. See `FIO_SET_KEY_TYPE`.

#### `FIO_NAME(insert)` (Hash Map)

```c
inline void FIO_NAME(insert)(FIO_NAME(s) * set,
                             const FIO_SET_HASH_TYPE hash_value,
                             FIO_SET_KEY_TYPE key,
                             FIO_SET_OBJ_TYPE obj);
```

Inserts an object to the Set only if it's missing, rehashing if required, returning the new (or old) object's pointer.

If the object already exists in the set, no action is performed (the old object is returned).

NOTE: This is the function's Hash Map variant. See `FIO_SET_KEY_TYPE`.

#### `FIO_NAME(remove)` (Hash Map)

```c
inline int FIO_NAME(remove)(FIO_NAME(s) * set,
                     const FIO_SET_HASH_TYPE hash_value,
                     FIO_SET_KEY_TYPE key);
```

Removes an object from the Set, rehashing if required.

Returns 0 on success and -1 if the object wasn't found.

NOTE: This is the function's Hash Map variant. See `FIO_SET_KEY_TYPE`.

### Set Find / Insert

These functions are defined if the Set is a pure Set (not a Hash Map).

#### `FIO_NAME(find)` (Set)

```c
inline FIO_SET_OBJ_TYPE *
   FIO_NAME(find)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                  FIO_SET_OBJ_TYPE obj);
```
Locates an object in the Set, if it exists.

NOTE: This is the function's pure Set variant (no `FIO_SET_KEY_TYPE`).

#### `FIO_NAME(insert)` (Set)

```c
inline FIO_SET_OBJ_TYPE *
   FIO_NAME(insert)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                    FIO_SET_OBJ_TYPE obj);
```

Inserts an object to the Set only if it's missing, rehashing if required, returning the new (or old) object's pointer.


If the object already exists in the set, than the new object will be destroyed and the old object's address will be returned.

NOTE: This is the function's pure Set variant (no `FIO_SET_KEY_TYPE`).

#### `FIO_NAME(overwrite)`

```c
inline FIO_SET_OBJ_TYPE *
   FIO_NAME(overwrite)(FIO_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                       FIO_SET_OBJ_TYPE obj);
```

Inserts an object to the Set, rehashing if required, returning the new object's pointer.

If the object already exists in the set, it will be destroyed and overwritten.

NOTE: This function doesn't exist when `FIO_SET_KEY_TYPE` is defined.

#### `FIO_NAME(remove)` (Set)

```c
inline int FIO_NAME(remove)(FIO_NAME(s) * set,
                             const FIO_SET_HASH_TYPE hash_value,
                             FIO_SET_OBJ_TYPE obj);
```

Removes an object from the Set, rehashing if required.

Returns 0 on success and -1 if the object wasn't found.

NOTE: This is the function's pure Set variant (no `FIO_SET_KEY_TYPE`).

### Set / Hash Map Data

#### `FIO_NAME(last)`

```c
inline FIO_SET_TYPE *FIO_NAME(last)(FIO_NAME(s) * set);
```

Allows a peak at the Set's last element.

Remember that objects might be destroyed if the Set is altered (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).

#### `FIO_NAME(pop)`

```c
inline void FIO_NAME(pop)(FIO_NAME(s) * set);
```

Allows the Hash to be momentarily used as a stack, destroying the last object added (`FIO_SET_OBJ_DESTROY` / `FIO_SET_KEY_DESTROY`).

#### `FIO_NAME(count)`

```c
inline size_t FIO_NAME(count)(const FIO_NAME(s) * set);
```

Returns the number of object currently in the Set.

#### `FIO_NAME(capa)`

```c
inline size_t FIO_NAME(capa)(const FIO_NAME(s) * set);
```

Returns a temporary theoretical Set capacity.

This could be used for testing performance and memory consumption.

#### `FIO_NAME(capa_require)`

```c
inline size_t FIO_NAME(capa_require)(FIO_NAME(s) * set,
                                     size_t min_capa);
```

Requires that a Set contains the minimal requested theoretical capacity.

Returns the actual (temporary) theoretical capacity.


#### `FIO_NAME(is_fragmented)`

```c
inline size_t FIO_NAME(is_fragmented)(const FIO_NAME(s) * set);
```

Returns non-zero if the Set is fragmented (more than 50% holes).

#### `FIO_NAME(compact)`

```c
inline size_t FIO_NAME(compact)(FIO_NAME(s) * set);
```

Attempts to minimize memory usage by removing empty spaces caused by deleted items and rehashing the Set.

Returns the updated Set capacity.

#### `FIO_NAME(rehash)`

```c
void FIO_NAME(rehash)(FIO_NAME(s) * set);
```

Forces a rehashing of the Set.


#### `FIO_SET_FOR_LOOP`

```c
#define FIO_SET_FOR_LOOP(set, pos) // ...
```

A macro for a `for` loop that iterates over all the Set's objects (in order).

`set` is a pointer to the Set variable and `pos` is a temporary variable name to be created for iteration.

`pos->hash` is the hashing value.

For Hash Maps, `pos->obj` is a key / value couplet, requiring a selection of `pos->obj.key` / `pos->obj.obj`.

For Pure Sets. the `pos->obj` is the actual object data.

**Important**: Since the Set might have "holes" (objects that were removed), it is important to skip any `pos->hash == 0` or the equivalent of `FIO_SET_HASH_COMPARE(pos->hash, FIO_SET_HASH_INVALID)`.

## General Helpers

Network applications require many common tasks, such as Atomic operations, locks, string to number conversions etc'

Many of the helper functions used by the facil.io core library are exposed for client use.

### Atomic operations

#### `fio_atomic_xchange`

```c
#define fio_atomic_xchange(p_obj, value) /* compiler specific */
```

An atomic exchange operation, sets a new value and returns the previous one.

`p_obj` must be a pointer to the object (not the object itself).

`value` is the new value to be set.

#### `fio_atomic_add`

```c
#define fio_atomic_add(p_obj, value) /* compiler specific */
```

An atomic addition operation.

Returns the new value.

`p_obj` must be a pointer to the object (not the object itself).

`value` is the new value to be set.

#### `fio_atomic_sub`

```c
#define fio_atomic_sub(p_obj, value) /* compiler specific */
```

An atomic subtraction operation.

Returns the new value.

`p_obj` must be a pointer to the object (not the object itself).

`value` is the new value to be set.

### Atomic locks

Atomic locks the `fio_lock_i` type.

```c
typedef uint8_t volatile fio_lock_i;
#define FIO_LOCK_INIT 0
```

#### `fio_trylock`

```c
inline int fio_trylock(fio_lock_i *lock);
```

Returns 0 if the lock was acquired and non-zero on failure.

#### `fio_lock`

```c
inline void fio_lock(fio_lock_i *lock);
```

Busy waits for the lock - CAREFUL, `fio_trylock` should be preferred.

#### `fio_is_locked`

```c
inline int fio_is_locked(fio_lock_i *lock);
```

Returns the current lock's state (non 0 == Busy).

#### `fio_unlock`

```c
inline void fio_unlock(fio_lock_i *lock);
```

Releases a lock.


#### `fio_reschedule_thread`

```c
inline void fio_reschedule_thread(void);
```

Reschedules a thread (used as part of the `fio_lock` busy wait logic).

Implemented using `nanosleep`, which seems to be the most effective and efficient thread rescheduler.

#### `fio_throttle_thread`

```c
inline void fio_throttle_thread(size_t nano_sec);
```

A blocking throttle using `nanosleep`.


### Byte Ordering Helpers (Network vs. Local)

Different byte ordering schemes often effect network applications, especially when sending binary data.

The 16 bit number might be represented as `0xaf00` on one machine and as `0x00af` on another.

These helpers help concert between network byte ordering (Big Endian) to a local byte ordering scheme.

These helpers also help extract numerical content from a binary string.

#### `fio_lton16`

```c
#define fio_lton16(i) /* system specific */
```

Local byte order to Network byte order, 16 bit integer.
#### `fio_lton32`

```c
#define fio_lton32(i) /* system specific */
```

Local byte order to Network byte order, 32 bit integer.
#### `fio_lton64`

```c
#define fio_lton64(i) /* system specific */
```

Local byte order to Network byte order, 62 bit integer.

#### `fio_str2u16`

```c
#define fio_str2u16(c) /* system specific */
```

Reads a 16 bit number from an unaligned network ordered byte stream.

Using this function makes it easy to avoid unaligned memory access issues.

#### `fio_str2u32`

```c
#define fio_str2u32(c) /* system specific */
```

Reads a 32 bit number from an unaligned network ordered byte stream.

Using this function makes it easy to avoid unaligned memory access issues.

#### `fio_str2u64`

```c
#define fio_str2u64(c) /* system specific */
```

Reads a 64 bit number from an unaligned network ordered byte stream.

Using this function makes it easy to avoid unaligned memory access issues.


#### `fio_u2str16`

```c
#define fio_u2str16(buffer, i)  /* simple byte value assignment using network order */
```

Writes a local 16 bit number to an unaligned buffer in network order.

No error checks or buffer tests are performed - make sure the buffer has at least 2 bytes available.

#### `fio_u2str32`

```c
#define fio_u2str32(buffer, i) /* simple byte value assignment using network order */
```

Writes a local 32 bit number to an unaligned buffer in network order.

No error checks or buffer tests are performed - make sure the buffer has at least 4 bytes available.

#### `fio_u2str64`

```c
#define fio_u2str64(buffer, i) /* simple byte value assignment using network order */
```

Writes a local 64 bit number to an unaligned buffer in network order.

No error checks or buffer tests are performed - make sure the buffer has at least 8 bytes available.

#### `fio_ntol16`

```c
#define fio_ntol16(i) /* system specific */
```

Network byte order to Local byte order, 16 bit integer.
#### `fio_ntol32`

```c
#define fio_ntol32(i) /* system specific */
```

Network byte order to Local byte order, 32 bit integer.
#### `fio_ntol64`

```c
#define fio_ntol64(i) /* system specific */
```

Network byte order to Local byte order, 62 bit integer.

#### `fio_bswap16`

```c
#define fio_bswap16(i) /* system specific */
```

Swaps the byte order in a 16 bit integer.

#### `fio_bswap32`

```c
#define fio_bswap32(i) /* system specific */
```

Swaps the byte order in a 32 bit integer.

#### `fio_bswap64`

```c
#define fio_bswap64(i) /* system specific */
```

Swaps the byte order in a 64 bit integer.

### Strings to Numbers

#### `fio_atol`

```c
int64_t fio_atol(char **pstr);
```

Converts between String data to a signed `int64_t`.

Numbers are assumed to be in base 10.

Octal (`0###`), Hex (`0x##`/`x##`) and binary (`0b##`/ `b##`) are recognized as well. For binary Most Significant Bit must come first.

The most significant difference between this function and `strtol` (aside of API design and speed), is the added support for binary representations.

#### `fio_atof`

```c
double fio_atof(char **pstr);
```

A helper function that converts between String data to a signed double.

Implemented using the standard `strtold` library function.

### Numbers to Strings

#### `fio_ltoa`

```c
size_t fio_ltoa(char *dest, int64_t num, uint8_t base);
```

A helper function that writes a signed int64_t to a string.

No overflow guard is provided, make sure there's at least 68 bytes available (for base 2).

Offers special support for base 2 (binary), base 8 (octal), base 10 and base 16 (hex). An unsupported base will silently default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the beginning of the string).

Returns the number of bytes actually written (excluding the NUL terminator).

#### `fio_ftoa`

```c
size_t fio_ftoa(char *dest, double num, uint8_t base);
```

A helper function that converts between a double to a string.

No overflow guard is provided, make sure there's at least 130 bytes
available (for base 2).

Supports base 2, base 10 and base 16. An unsupported base will silently
default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
beginning of the string).

Returns the number of bytes actually written (excluding the NUL
terminator).

### Random data Generation

#### `fio_rand64`

```c
uint64_t fio_rand64(void);
```

Returns 64 psedo-random bits. Probably not cryptographically safe.


#### `fio_rand_bytes`

```c
void fio_rand_bytes(void *target, size_t length);
```

Writes `length` bytes of psedo-random bits to the target buffer.

### Base64

#### `fio_base64_encode`

```c
int fio_base64_encode(char *target, const char *data, int len);
```

This will encode a byte array (data) of a specified length and place the encoded data into the target byte buffer (target). The target buffer MUST have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4) bytes.

Returns the number of bytes actually written to the target buffer (including the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.

#### `fio_base64url_encode`

```c
int fio_base64url_encode(char *target, const char *data, int len);
```

Same as [`fio_base64_encode`](#fio_base64_encode), but using Base64URL encoding.

#### `fio_base64_decode`

```c
int fio_base64_decode(char *target, char *encoded, int base64_len);
```

This will decode a Base64 encoded string of a specified length (len) and place the decoded data into the target byte buffer (target).

The target buffer MUST have enough room for 2 bytes in addition to the expected data (NUL byte + padding test).

A NUL byte will be appended to the target buffer. The function will return the number of bytes written to the target buffer (excluding the NUL byte).

If the target buffer is NUL, the encoded string will be destructively edited and the decoded data will be placed in the original string's buffer.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if the raw data's length isn't devisable by 3. Hence, the target buffer should be, at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding the NUL terminator byte).

**NOTE**:

The decoder is variation agnostic (will decode Base64, Base64 URL and Base64 XML variations) and will attempt it's best to ignore invalid data, (in order to support the MIME Base64 variation in RFC 2045).

This comes at the cost of error checking, so the encoding isn't validated and invalid input might produce surprising results.


### SipHash

#### `fio_siphash24`

```c
uint64_t fio_siphash24(const void *data, size_t len);
```

A SipHash variation (2-4).

#### `fio_siphash13`

```c
uint64_t fio_siphash13(const void *data, size_t len);
```

A SipHash 1-3 variation.

#### `fio_siphash`

```c
#define fio_siphash(data, length) fio_siphash13((data), (length))
```

The Hashing function used by dynamic facil.io objects.

Currently implemented using SipHash 1-3.


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

## Version and Compilation Related Macros

The following macros effect facil.io's compilation and can be used to validate the API version exposed by the library.

### Version Macros

The version macros relate the version for both facil.io's core library and it's bundled extensions.

#### `FIO_VERSION_MAJOR`

The major version macro is currently zero (0), since the facil.io library's API should still be considered unstable.

In the future, API breaking changes will cause this number to change.

#### `FIO_VERSION_MINOR`

The minor version normally represents new feature or substantial changes that don't effect existing API.

However, as long as facil.io's major version is zero (0), API breaking changes will cause the minor version (rather than the major version) to change.

#### `FIO_VERSION_PATCH`

The patch version is usually indicative to bug fixes.

However, as long as facil.io's major version is zero (0), new feature or substantial changes will cause the patch version to change.

#### `FIO_VERSION_STRING`

This macro translates to facil.io's literal string. It can be used, for example, like this:

```c
printf("Running facil.io version" FIO_VERSION_STRING "\r\n");
```

### Other Macros that effect facil.io's behavior / compilation

The facil.io core library has some hard coded values that can be adjusted by defining the following macros during compile time.

#### `FIO_MAX_SOCK_CAPACITY`

This macro define the maximum hard coded number of connections per worker process.

To be more accurate, this number represents the highest `fd` value allowed by library functions.

If the soft coded OS limit is higher than this number, than this limit will be enforced instead.


#### `FIO_CPU_CORES_LIMIT`

The facil.io startup procedure allows for auto-CPU core detection.

Sometimes it would make sense to limit this auto-detection to a lower number, such as on systems with more than 32 cores.

This is only relevant to automated values, when running facil.io with zero threads and processes, which invokes a large matrix of workers and threads (see [facil_start](#facil_start)).

This does NOT effect manually set (non-zero) worker/thread values.

#### `FIO_DEFER_THROTTLE_PROGRESSIVE`

The progressive throttling model makes concurrency and parallelism more likely.

Otherwise threads are assumed to be intended for "fallback" in case of slow user code, where a single thread should be active most of the time and other threads are activated only when that single thread is slow to perform. 

By default, `FIO_DEFER_THROTTLE_PROGRESSIVE` is true (1).

#### `FIO_PRINT_STATE`

When this macro is true (1), facil.io will print some state massages to stderr (startup / shutdown messages, etc').

By default this macro is set to true.

#### `FIO_PUBSUB_SUPPORT`

If true (1), compiles the facil.io pub/sub API .

## Weak functions

Weak functions are functions that can be over-ridden during the compilation / linking stage.

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
