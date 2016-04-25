# The C Server Framework - Public API (v.0.3.1)

Writing servers in C, although fun, is repetitive and often involves copying a the code from [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and making a mess of some of the implementation details along the way.

`lib-server` is aimed at writing unix based (linux/BSD) servers application (network services), such as micro-services and web applications.

`lib-server` might not be optimized to your liking, but it's all working great for me. Besides, it's code is heavily commented code, easy to edit and tweak. To offer some comparison, `ev.c` from `libev` has ~5000 lines, while `libreact` is less then 400 lines (~260 lines of actual code)...

Using `lib-server` is super simple. It's based on Protocol structure and callbacks, so that we can dynamically change protocols and support stuff like HTTP upgrade requests (i.e. switching from HTTP to Websockets). Here's a simple echo server example:

Under the hood, `lib-server` uses `libreact` as the reactor, `libasync` to handle tasks and `libbuffer` for a user level network buffer and writing data asynchronously.

It should be noted that `server_pt` and `struct Reactor *` can both be used as pointers to a `struct Reactor` object, so that a `server_pt` can be used for [`libreact`'s API](./libreact.md).

## The Server Framework's Building blocks

The Server framework's API is comprised of a few building blocks, each should be understood in order to make use of the Server Framework:

* [The Server Object](#the-server-object-server_pt) is a pointer used by the API to reference the server data. Almost every API function requires this object as it's first argument.

* [Server Settings](#the-server-settings-struct-serversettings) are controlled using a `struct ServerSettings` which allows up to control certain global aspects of the server's implementation (i.e. set the Protocol for new incoming connections).

* [The Communication Protocol](#the-communication-protocol-struct-protocol) controls how an application responds to connection related events (i.e. the `on_close` event called when a connection is closed). Protocol objects can be global (the default protocol is always global) or connection specific. Protocols can be switched mid-stream - i.e. `on_open` can be used to create a new protocol object and set it up as a connection's protocol.

* [The Server's API itself](#the-server-api) is a collection of functions that use these building blocks to help you implement the actual application logic. This includes:

    * [Server start up and global state functions](#server-start-up-and-global-state).

    * [Global asynchronous task helpers](#asynchronous-tasks)

    * [Connection state management functions](#connection-state)

    * [Connection settings data and management functions](#connection-settings-data-and-management)

    * [IO Read/Write functions](#reading-and-writing-data)

    * [Connection related asynchronous task helpers](#connection-related-asynchronous-tasks)

At the end of this document you will find a [quick example](#a-quick-example) for a simple echo server.

## The Server object: `server_pt`

The Server API hinges on the `server_pt` data type (which is a pointer to a `struct Server`). This data type is used both for storing the Server's settings (i.e. callback pointers) and information about the state of the server.

The `server_pt` object should be opaque and it's content should be ignored.

## The Server Settings: `struct ServerSettings`

To implement a Server, some settings need to be set, such as the callbacks to be used (called a Protocol), the network port the server should listen to, etc'.

These settings are passed to the `Server.listen` function, either directly or using the `start_server` macro (which makes it easier to call `Server.listen`).

The ServerSettings is defined as follows:

```c
struct ServerSettings {
  struct Protocol* protocol;
  char* port;
  char* address;
  void (*on_init)(server_pt server);
  void (*on_finish)(server_pt server);
  void (*on_tick)(server_pt server);
  void (*on_idle)(server_pt server);
  char* busy_msg;
  void* udata;
  int threads;
  int processes;
  uint8_t timeout;
};
```
Once the Server's Settings have been passed on to the Server, they are accessible using the `Server.settings` function.

### The Protocol

The protocol pointer points to the default protocol to be assigned to new connections. This will be explained in more details further on.

### Server Callbacks

The server settings offer a number of possible callbacks to server-wide events that can be leveraged according to your needs.

`on_init` - a pointer to a function that will be called when the server had finished it's initialization.

`on_finish` - a pointer to a function that will be called when the server had finished it's shutdown process.

`on_tick` - a pointer to a function that will be called every time the [server's reactor](https://en.wikipedia.org/wiki/Reactor_pattern) cycles.

`on_idle` - a pointer to a function that will be called each time the server's reactor moves from an active state (network events were processed) to an idle state (no network events found).

If the `processes` property is more then 1, these callbacks will be called for each forked process.

### Server Properties

`char* port` - a string to the network port used for listening (i.e. `"80"` for HTTP or `"25"` for smtp). Defaults to `"3000"`

`char* address` - a string to the network address used for binding the listening socket - this is best left as NULL. Defaults to `NULL`

`char* busy_msg` - a string to a message that is sent when the server is busy (running low on available file descriptors). New connections will receive this message (if it's short enough) and will be disconnected immediately. i.e. the HTTP protocol implementation sets this message to: `"HTTP/1.1 503 Service Unavailable\r\n\r\nServer Busy."`. Defaults to an empty string (`NULL`).

`void* udata` - an opaque global user data pointer (`void *`). Once the server is running, this data is accessible using `Server.settings(srv)->udata`.

`int threads` - the number of threads to be assigned to the reactor. Defaults to `1` (implements a single threaded, evented design).

`int processes` - the number of processes to be used (worker processes). Defaults to `1` (no forking, the main process is used for running the server).

## The Communication Protocol: `struct Protocol`

The `struct Protocol` is an extendible connection property that defines the connection's callbacks.

Each connection is assigned a protocol when it is accepted. This protocol can be updated using the `Server.set_protocol` function.

A protocol can be (but doesn't have to be) shared among many connections.

The default protocol (see the `ServerSettings` section) is always shared among all new connection.

### The `struct Protocol` core

This is the Protocol structure's definition:

```c
struct Protocol {
  char* service;
  void (* on_open)(server_pt, uint64_t connection_id);
  void (* on_data)(server_pt, uint64_t connection_id);
  void (* on_ready)(server_pt, uint64_t connection_id);
  void (* on_shutdown)(server_pt, uint64_t connection_id);
  void (* on_close)(server_pt, uint64_t connection_id);
  void (* ping)(server_pt, uint64_t connection_id);
};
```

#### Core Protocol Properties

`char* service` - a string to identify the protocol's service, i.e. "http" (see `Server.each` for it's usefulness).

#### Protocol callbacks

`on_open` - called when a **new** connection is accepted and the protocol is assigned to this new connection.

`on_data` - called when new data is available after all previous data was read (this is [edge triggered](http://www.kegel.com/c10k.html#nb.edge)).

`on_ready` - called when the socket is ready to be written to (this is [edge triggered](http://www.kegel.com/c10k.html#nb.edge)).

`on_shutdown` - called when the server is shutting down, but before closing the connection and calling `on_close` (allowing a "going away" message to be sent).

`on_close` - called after the connection was closed (either by the remote client or locally by the server or a call to `close`).

`ping` - called when the connection's timeout was reached. If this callback is set to `NULL` (the default), the connection will be closed once the timeout was reached (see the information about timeouts).

Protocol callbacks all share the same function signature, accepting a pointer to the Server data (`server_pt`) and a unique connection identifier (similar to a UUID) using the 64 bit type `uint64_t`.

#### Extending the Protocol

Since, in C, a `struct` is just the way memory is organized, it is possible to "inherit" the protocol `struct` and extend it with more information (either global data, or per connection data when using a pre connection protocol).

i.e.

```c
// extend the protocol
struct MyProtocol {
  struct Protocol parent;
  uint64_t counter;
};
// use the `on_open` callback to count connection
void on_open(server_pt, uint64_t connection_id) {
  struct MyProtocol * p =
       (struct MyProtocol)Server.get_protocol(server_pt, connection_id);
  p->counter++;
}
//...
int main() {
  struct MyProtocol my_pr = { .parent.on_open = on_open,
                              // other settings
                            };
  //...
  start_server(.protocol = &my_pr);
}
```

A more complex example can be found in the code for the Websockets extension, where a protocol object is assigned to each connection and both the memory release and the connection's data management demonstrate a more complex and detailed implementation.

## The Server API

The Server framework API is designed to be intuitive... but much like all good intentions, reality poses requirements that introduce a learning curve.

The following is designed as a short API reference to ease this learning curve.

### Server start up and global state

The following functions and settings are related to a Server object's global data and settings.

#### The `start_server(...)` macro

The start_server(...) macro is a shortcut that allows to easily create a ServerSettings structure and start the server in a simple way.

See `Server.listen` and the information about `struct ServerSettings` for more details.

The macro looks like this:
```c
#define start_server(...) Server.listen((struct ServerSettings){__VA_ARGS__})
```

#### `pid_t Server.root_pid(server_pt server)`

Returns the originating process `pid`. This might be different the the current processes `pid` when forking is used (se the `processes` settings options for more details).

#### `struct Reactor* Server.reactor(server_pt server)`

This function allows direct access to the reactor object. Use this access with care.

It should be noted that this function us redundant, as the `server_pt` can be simply cast to a `struct Reactor *`.

#### `struct ServerSettings * Server.settings(server_pt server)`

Allows direct access to the server's original settings. use with care, as updating the settings may cause global changes (such as updating the callbacks) or render some information invalid (the number of threads cannot be updated once the server is running).

#### `long Server.capacity(void)`

Returns the adjusted capacity for any server instance on the system.

The capacity is calculating by attempting to increase the system's open file limit to the maximum allowed, and then adjusting the result with respect to possible memory limits and possible need for file descriptors for response processing.

#### `int Server.to_fd(server_pt server, uint64_t connection_id)`

Returns the file descriptor belonging to the connection's UUID, if available. Returns -1 if the connection is closed (we cannot use 0 since 0 is potentially a valid file descriptor).

This is similar to using the `server_uuid_to_fd(uint64_t connection_id)` macro, except it validates the connection's state before returning the file descriptor.

#### `int Server.listen(struct ServerSettings)`

Starts the server with the requested server settings (which MUST include a default protocol).

This method blocks the current thread until the server is stopped (either though a `Server.srv_stop` function or when a SIGINT/SIGTERM is received).

#### `void Server.stop(server_pt server)`

Stops a specific server, closing any open connections.

#### `void Server.stop_all(void)`

Stops any and all server instances, closing any open connections.

### Asynchronous tasks

The following functions allow access to the Server's thread-pool, for scheduling asynchronous tasks.

#### `int Server.run_async(server_pt server, void task(void*), void* arg)`

Runs an asynchronous task, IF threading is enabled (set the `threads` to 1 (the default) or greater).

If threading is disabled, the current thread will perform the task and return.

Returns -1 on error or 0 on success.

#### `int Server.run_after(server_pt server, long milliseconds, void task(void*), void* arg)`

Creates a system timer (at the cost of 1 file descriptor) and pushes the timer to the reactor. The task will NOT repeat. Returns -1 on error or the new file descriptor on success.

NOTICE: Do NOT create timers from within the on_close callback, as this might block resources from being properly freed (if the timer and the on_close object share the same fd number).

#### `int Server.run_every(server_pt server, long milliseconds, int repetitions, void task(void*), void* arg)`

Creates a system timer (at the cost of 1 file descriptor) and pushes the timer to the reactor. The task will repeat `repetitions` times. if `repetitions` is set to 0, task will repeat forever. Returns -1 on error or the new file descriptor on succeess.

NOTICE: Do NOT create timers from within the on_close callback, as this might block resources from being properly freed (if the timer and the on_close object share the same fd number).

### Connection state

The following functions and settings are related to managing a connection's state, such as closing a connection, hijacking a socket from the server, reseting the timeout count (`touch`), etc'.

#### `int Server.attach(server_pt server, int fd, struct Protocol* protocol)`

Attaches an existing connection (fd) to the server's reactor and protocol management system, so that the server can be used also to manage connection based resources asynchronously (i.e. database resources etc').

#### `void Server.close(server_pt server, uint64_t connection_id)`

Closes the connection.

If any data is waiting to be written, close will return immediately and the connection will only be closed once all the data was sent.

#### `int Server.hijack(server_pt server, uint64_t connection_id)`

Hijacks a socket (file descriptor) from the server, clearing up it's resources. The control of the socket is totally relinquished.

This method will block until all the data in the buffer is sent before releasing control of the socket.

The returned value is the fd for the socket, or -1 on error (since 0 is a valid fd).

#### `long Server.count(server_pt server, char* service)`

Counts the number of connections for the specified protocol (`NULL` = all protocols).

#### `void Server.touch(server_pt server, uint64_t connection_id)`

"Touches" a socket, reseting it's timeout counter.

### Connection settings, data and management

The following functions and settings are related to specific connection data and settings.

#### `uint8_t Server.is_busy(server_pt server, uint64_t connection_id)`

Returns true if a specific connection's protected callback is running.

Protected callbacks include only the `on_message` callback and tasks forwarded
to the connection using the `fd_task` or `each` functions.

#### `uint8_t Server.is_open(server_pt server, uint64_t connection_id)`

Returns true if the connection's UUID points to a valid connection (valid meaning `on_close` wasn't called and processed just yet).

#### `struct Protocol* Server.get_protocol(server_pt server, uint64_t connection_id)`

Retrieves the active protocol object for the requested file descriptor.

#### `int Server.set_protocol(server_pt server, uint64_t connection_id, struct Protocol* new_protocol)`

Sets the active protocol object for the requested file descriptor.

Returns -1 on error (i.e. connection closed), otherwise returns 0.

#### `void* Server.get_udata(server_pt server, uint64_t connection_id)`

Retrieves an opaque pointer set by `set_udata` and associated with the connection.

Since no new connections are expected on fd == 0..2, it's possible to store global data in these locations.

#### `void* Server.set_udata(server_pt server, uint64_t connection_id, void* udata)`

Sets the opaque pointer to be associated with the connection. Returns the old pointer, if any.

#### `void Server.set_timeout(server_pt server, uint64_t connection_id, uint8_t timeout)`

Sets the timeout limit for the specified connection, in seconds, up to 255 seconds (the maximum allowed timeout count).

### Reading and Writing Data

The following functions and settings are related to writing and reading data from an existing connection.

#### `ssize_t Server.read(server_pt srv, uint64_t c_id, void* buffer, size_t max_len)`

Reads up to `max_len` of data from a socket. the data is stored in the
`buffer` and the number of bytes received is returned.

Returns -1 if an error was raised and the connection was closed.

Returns the number of bytes written to the buffer. Returns 0 if no data was
available.

#### `ssize_t Server.write(server_pt srv, uint64_t c_id, void* data, size_t len)`

Copies & writes data to the socket, managing an asynchronous buffer.

returns 0 on success. success means that the data is in a buffer waiting to be written. If the socket is forced to close at this point, the buffer will be destroyed (never sent).

On error, returns -1. Returns 0 on success

#### `ssize_t Server.write_move(server_pt srv, uint64_t c_id, void* data, size_t len)`

Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

On error, returns -1. Returns 0 on success

#### `ssize_t Server.write_urgent(server_pt srv, uint64_t c_id, void* data, size_t len)`

Copies & writes data to the socket, managing an asynchronous buffer.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` variant will send the data as soon as possible, without disrupting any data packages (data written using `write` will not be interrupted in the middle).

On error, returns -1. Returns 0 on success

#### `ssize_t Server.write_move_urgent(server_pt srv, uint64_t c_id, void* data, size_t len)`

Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` variant will send the data as soon as possible, without disrupting any data packages (data written using `write` will not be interrupted in the middle).

On error, returns -1. Returns 0 on success

#### `ssize_t Server.sendfile(server_pt srv, uint64_t connection_id, FILE* file)`

Sends a whole file as if it were a single atomic packet.

Once the file was sent, the `FILE *` will be closed using `fclose`.

The file will be buffered to the socket chunk by chunk, so that memory
consumption is capped at ~ 64Kb.


#### `void Server.rw_hooks(server_pt, uint64_t, *, *)` (see detailed description)

Sets up the read/write hooks, allowing for transport layer extensions (i.e. SSL/TLS) or monitoring extensions.

These hooks are only relevant when reading or writing from the socket using the server functions (i.e. `Server.read` and `Server.write`).

These hooks are attached to the specified socket and they are cleared automatically once the connection is closed.

The following is the function's prototype:

```c
void Server.rw_hooks(
    server_pt srv,
    uint64_t connection_id,
    ssize_t (*reading_hook)(server_pt srv, int fd, void* buffer, size_t size),
    ssize_t (*writing_hook)(server_pt srv, int fd, void* data, size_t len));
```

A reading (or writing) hook will have the following prototype:

```c
ssize_t reading_hook(server_pt srv, int fd, void* buffer, size_t buffer_size);
ssize_t writing_hook(server_pt srv, int fd, void* data, size_t data_len);
```

**Writing hook**

A writing hook will be used instead of the `write` function to send data to the socket. This allows uses the buffer for special protocol extension or transport layers, such as SSL/TLS instead of buffering data to the network.

A writing hook is a function that takes in a pointer to the server (the buffer's owner), the socket to which writing should be performed (fd), a pointer to the data to be written and the length of the data to be written:

A writing hook should return the number of bytes actually sent from the data buffer (not the number of bytes sent through the socket, but the number of bytes that can be marked as sent).

A writing hook should return -1 if the data couldn't be sent and processing should be stop (the connection was lost or suffered a fatal error).

A writing hook should return 0 if no data was sent, but the connection should remain open or no fatal error occurred.

A writing hook MUST write data to the network, or it will not be called again until new data becomes available through `Server.write` (meaning, it might never get called again). Returning a positive value without writing data to the network will NOT cause the writing hook to be called again.

i.e.:

```c
ssize_t writing_hook(server_pt srv, int fd, void* data, size_t len) {
  int sent = write(fd, data, len);
  if (sent < 0 && (errno & (EWOULDBLOCK | EAGAIN | EINTR)))
    sent = 0;
  return sent;
}
```

**Reading hook**

The reading hook, similar to the writing hook, should behave the same as `read` and accepts the same arguments as the `writing_hook`, except the `length` argument should refer to the size of the buffer (or the amount of data to be read, if less then the size of the buffer).

The return values are the same as the writing hook's return values, except the number of bytes returned refers to the number of bytes written to the buffer.

i.e.

```c
ssize_t reading_hook(server_pt srv, int fd, void* buffer, size_t size) {
  ssize_t read = 0;
  if ((read = recv(fd, buffer, size, 0)) > 0) {
    return read;
  } else {
    if (read && (errno & (EWOULDBLOCK | EAGAIN)))
      return 0;
  }
  return -1;
}
```

### Connection Related Asynchronous Tasks

The following functions are helpers that allow asynchronous tasks to be performed in the context of specific connection(s).

#### `int Server.each(...)` (see details)

The function's prototype looks like this:

```c
int Server.each(server_pt server,
            uint64_t original_connection,
            char* service,
            void (*task)(server_pt server, uint64_t connection_id, void* arg),
            void* arg,
            void (*on_finish)(server_pt server,
                              uint64_t original_connection,
                              void* arg));
```


Schedules a specific task to run asynchronously for each connection (except the origin connection). a NULL service identifier == all connections (all protocols).

The task is performed within each target connection's busy "lock", meaning no two tasks (or `on_data` events) should be performed at the same time (concurrency will be avoided within the context of each connection, except for `on_shutdown`, `on_close` and `ping`).

The `task` variable is a pointer to a function (a task) to be called within each connection's context.

The `on_finish` callback will be called once the task is finished and it will receive the originating connection's UUID (could be 0). The originating connection might have been closed by that time.

The `service` string (pointer) identifier MUST be a constant string object OR a string that will persist until the `on_finish` callback is called. In other words, either hardcode the string or use `malloc` to allocate it before calling `each` and `free` to release the string from within the `on_finish` callback.

It is recommended the `on_finish` callback is only used to perform any resource cleanup necessary.

#### `int Server.each_block(...)` (see details)

The function's prototype looks like this:

```c
int Server.each_block(server_pt server,
                uint64_t fd_originator,
                char* service,
                void (*task)(server_pt srv, uint64_t c_id, void* arg),
                void* arg);
```



Schedules a specific task to run for each connection (except the origin connection). The tasks will be performed sequentially, in a blocking manner. The method will only return once all the tasks were completed. A NULL service identifier == all connections (all protocols).

The task, although performed **on** each connection, will be performed within the calling connection's lock, so take care for possible race conditions.

#### `int Server.fd_task(...)` (see details)

The function's prototype looks like this:

```c
int Server.fd_task(server_pt server,
                uint64_t sockfd,
                void (*task)(server_pt srv, uint64_t connection_id, void* arg),
                void* arg,
                void (*fallback)(server_pt srv, uint64_t c_id, void* arg));
```

Schedules a specific task to run asynchronously for a specific connection.

returns -1 on failure, 0 on success (success being scheduling the task).

If a connection was terminated before performing their scheduled tasks, the `fallback` task will be performed instead.

It is recommended to perform any resource cleanup within the fallback function and call the fallback function from within the main task.

## A Quick Example

The following example isn't very interesting, but a simple echo server is good enough to start with:

```c
#include "lib-server.h"
#include <stdio.h>
#include <string.h>
// Concurrency using thread? How many threads in the thread pool?
#define THREAD_COUNT 4
// Concurrency using processes? (more then a single process can be used)
#define PROCESS_COUNT 1
// a simple echo... this will be the only callback we need
void on_data(server_pt server, uint64_t fd_uuid) {
  char buff[1024];
  ssize_t incoming = 0;
  while ((incoming = Server.read(server, fd_uuid, buff, 1024)) > 0) {
    Server.write(server, fd_uuid, buff, incoming);  // echo the data.
    if (!strncasecmp(buff, "bye", 3)) {             // check for keyword "bye"
      Server.close(server, fd_uuid);  // closes the connection (on keyword)
    }
  }
}
// running the server
int main(void) {
  // We'll create the echo protocol object. We'll only use the on_data callback.
  struct Protocol protocol = {.on_data = on_data,
                              .service = "echo"};
  // This macro will call Server.listen(settings) with the settings we provide.
  start_server(.protocol = &protocol, .timeout = 10, .port = "3000",
               .threads = THREAD_COUNT, .processes = PROCESS_COUNT);
}
```
