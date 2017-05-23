# A Simple Socket Library for non-blocking Berkeley Sockets

This library aims to:

* Resolve the file descriptor collision security risk.

* Resolve issues with partial writes and concurrent write operations by implementing a user land buffer and a spinlock.

* Provide `sendfile` alternatives when sending big data stored on the disk - `sendfile` being broken on some system and lacking support for TLS.

* Provide a solution to the fact that closing a connection locally (using `close`) will prevent event loops and socket polling systems (`epoll`/`kqueue`) from raising the `on_hup` / `on_close` event. `libsock` provides support for local closure notification - this is done by defining an optional `reactor_on_close(intptr_t uuid)` overridable function.

* Provide support for timeout a management callback for server architectures that require the timeout to be reset ("touched") whenever a read/write occurs - this is done by defining an optional `sock_touch(intptr_t uuid)` callback.

`libsock` requires only the following three files from this repository: [`src/spnlock.h`](../src/spnlock.h),  [`src/libsock.h`](../src/libsock.h) and [`src/libsock.c`](../src/libsock.c).

#### The Story Of The Partial `write`

For some unknown reason, there is this little tidbit of information for the `write` function family that is always written in the documentation and is commonly ignored by first time network programmers:

> On success, **the number of bytes written** is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.

It is often missed that the number of bytes actually written might be smaller then the amount we wanted to write.

For this reason, `libsock` implements a user land buffer. All calls to `sock_write` promise to write the whole of the data (careful) unless a connection issue causes them to fail.

The only caveat is for very large amounts of data that exceed the available user land buffer (which you can change from the default ~16Mib), since `libsock` will keep flushing all the sockets (it won't return from the `sock_write` function call) until the data can be fully written to the user-land buffer.

However, this isn't normally an issue, since files can be sent at practically no cost when using `libsock` and large amounts of data are normally handled by files (temporary or otherwise).

#### The `sendfile` Experience

The `sendfile` directive is cool, it allows us to send data directly from a file to a socket, no copying required (although the kernel might or might not copy data).

However, the `sendfile` function call is useless when working with TLS connections, as the TLS implementation is performed in the application layer, meaning that the data needs to be encrypted in the application layer... So, no `sendfile` for TLS connections :-(

Another issue is that [`sendfile` is broken / unavailable on some systems](https://blog.phusion.nl/2015/06/04/the-brokenness-of-the-sendfile-system-call/)...

`libsock` Provides a dual solution for this:

1. On systems where `sendfile` is available, it will be used if no TLS or other read/write hook had been defined.

2. Where `sendfile` can't be used, `libsock` will write the file to the socket (or TLS), loading up to ~16Kb of data at a time.

    The same file (fd) can be sent to multiple clients at once (keeping it open in the application's cache) or automatically closed once it was sent.

This abstraction of the `sendfile` system call elevates the headache related with managing resources when sending big files.

#### Postponing The Timeout

Often, server applications need to implement a timeout review procedure that checks for stale connections.

However, this requires that the server architecture to implement a "write" and "read" logic (if only to set or reset the timeout for the connection) and somehow enforce this API, so that timeout events don't fire prematurely.

`libsock` saves us this extra work by providing an optional callback that allows server architectures to update their timeout state without implementing the whole "read/write" API stack.

By providing a `sock_touch` function that can be overwritten (a weak symbol function), the server architecture need only implement the `sock_touch` function to update any internal data structure or linked list, and `libsock` takes care of the rest.

This means that on any successful `sock_read`, `sock_write` or `sock_flush`, where data was written to the socket layer, `sock_touch` is called.

#### The Lost `on_close` Event

When polling using `kqueue` / `epoll` / the evented library of your choice, it is normal for locally closed sockets (when we call `close`) to close quietly, with no event being raised.

This can be annoying at times and often means that a wrapper is supplied for the `close` function and hopes are that no one calls `close` directly.

However, whenever a connection is closed using `libsock`, a callback `reactor_on_close(intptr_t)` is called.

This is supported by two facts: 1. because `sock_close` acts as a wrapper, calling the callback after closing the connection; 2. because connections are identified using a UUID (not an `fd`), making calls to `close` harder (though possible).

`sock_close` will always call the `reactor_on_close` callback, where you can call the function of your choice and presto: a local `close` will evoke the `on_close` event callback for the evented library of your choice.

The `reactor_on_close` name was chosen to have `libsock` work with `libreact`. This is a convenience, not a requirement.

It should be noted that a default `reactor_on_close` is provided, so there's no need to write one if you don't need one.

#### UUIDs & The File Descriptor Collision Security Risk

These things happen, whether we're using threads or evented programming techniques, the more optimized our code the more likely that we are exposed to file description collision risks.

Here is a quick example:

* Bob connects to his bank to get a bank statement on line.

* Bob receives the file descriptor 12 for the new connection and submits a request to the server.

* A request to the bank's database is performed in a non-blocking manner (evented, threaded, whatever you fancy). But, due to system stress or design or complexity, it will take a longer time to execute.

* Bob's connection is dropped, and file descriptor 12 is released by the system.

* Alice connects to the server and receives the (now available) file descriptor 12 for the new connection (Alice can even negotiate a valid TLS connection at this point).

* The database response arrives and the information is sent to the file descriptor.

* Alice gets Bob's bank statement.

... Hmmm... bad.

The risk might seem unlikely, but it exists.

Hence: `libsock` uses connection UUIDs to map to the underlying file descriptors.

These UUIDs are the C standard `intptr_t` type, which means that they are easy to handle, store and move around. The UUIDs are a simple system local scheme and shouldn't be shared among systems or processes (collision risks). The only certainly invalid UUID is `-1` (depending on the system's byte-ordering scheme).

In our example:

* Bob connects to his bank to get a bank statement on line.

* Bob receives the file descriptor 12, mapped to the connection UUID 0x114.

* [...] everything the same until Bob's connection drops.

* Alice connects to the server and receives the (now available) file descriptor 12 for the new connection. The connection is mapped to the UUID 0x884.

* The database response arrives but isn't sent because the write operation fails (invalid UUID 0x114).

So, using `libsock`, Alice will **not** receive Bob's bank statement.

#### TCP/IP as default

`libsock` defaults to TCP/IP sockets, so calls to `sock_listen` and `sock_connect` will assume TCP/IP.

However, importing other socket types should be possible by using the `sock_open(fd)` function, that allows us to import an existing file descriptor into `libsock`, assigning this file descriptor a valid UUID.

## Storing Data

In order to manage some of these features, such as the user land buffer and socket UUID match-protection, `libsock` needs some memory to be allocated for it's data structures.

To initialize this data storage, before using any of `libsock`'s features', the library command `sock_lib_init` should be called.

## A Simple API

The `libsock` API can be divided into a few different categories:

- General helper functions

- Accepting connections and opening new sockets.

- Sending and receiving data.

- Direct user level buffer API.

- Read/Write Hooks.

### General helper functions

The following helper functions are for common tasks when socket programming. These functions are independent from the `libsock` state machine.

#### `int sock_set_non_block(int fd)`

Sets a socket to non blocking state.

This function is called automatically for the new socket, when using
`sock_accept` or `sock_connect`.

#### `ssize_t sock_max_capacity(void)`

Gets the maximum number of file descriptors this process can be allowed to
access.

If the "soft" limit is lower then the "hard" limit, the process's limits will be
extended to the allowed "hard" limit.

### Accepting connections and opening new sockets

Accepting connections, initiating connections and opening sockets - as well as attaching open sockets to the `libsock` state machine - are required actions so that we can use sockets with the `libsock` API.


#### `intptr_t sock_listen(char* address, char* port)`

Opens a listening non-blocking socket. Return's the socket's UUID.

Returns -1 on error. Returns a valid socket (non-random) UUID.

UUIDs with values less then -1 are valid values, depending on the system's byte-ordering.

Socket UUIDs are predictable and shouldn't be used outside the local system. They protect against connection mixups on concurrent systems (i.e. when saving client data for "broadcasting" or when an old client task is preparing a response in the background while a disconnection and a new connection occur on the same `fd`).

#### `intptr_t sock_accept(intptr_t srv_uuid)`

`sock_accept` accepts a new socket connection from the listening socket `server_fd`, allowing the use of `sock_` functions with this new file descriptor.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system. They protect against connection mixups on concurrent systems (i.e. when saving client data for "broadcasting" or when an old client task is preparing a response in the background while a disconnection and a new connection occur on the same `fd`).

#### `intptr_t sock_connect(char* address, char* port)`

`sock_connect` is similar to `sock_accept` but should be used to initiate a client connection to the address requested.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system. They protect against connection mixups on concurrent systems (i.e. when saving client data for "broadcasting" or when an old client task is preparing a response in the background while a disconnection and a new connection occur on the same `fd`).

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to listen for events.

NOTICE:

This function is non-blocking, meaning that the connection probably wasn't established by the time the function returns (this prevents the function from hanging while waiting for a network timeout).

Use select, poll, `libreact` or other solutions to review the connection state before attempting to write to the socket.

#### `intptr_t sock_open(int fd)`

`sock_open` takes an existing file descriptor `fd` and initializes it's status as open and available for `sock_API` calls, returning a valid UUID.

This will reinitialize the data (user buffer etc') for the file descriptor provided, calling the `reactor_on_close` callback if the `fd` was previously marked as used.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system. They protect against connection mixups on concurrent systems (i.e. when saving client data for "broadcasting" or when an old client task is preparing a response in the background while a disconnection and a new connection occur on the same `fd`).

#### `int sock_isvalid(intptr_t uuid)`

Returns 1 if the uuid refers to a valid and open, socket.

Returns 0 if not.

### Sending and receiving data

Reading and writing data to a socket, using the `libsock` API, allows easy access to a user level buffer and file streaming capabilities, as well as read write hooks that simplify the task of implementing layered protocols (i.e. TLS).

#### `ssize_t sock_read(intptr_t uuid, void* buf, size_t count)`

`sock_read` attempts to read up to count bytes from the socket into the buffer starting at buf.

`sock_read`'s return values are wildly different then the native return values and they aim at making far simpler sense.

`sock_read` returns the number of bytes read (0 is a valid return value which simply means that no bytes were read from the buffer).

On a connection error (NOT EAGAIN or EWOULDBLOCK) or signal interrupt, `sock_read` returns -1.

Data might be available in the kernel's buffer while it is not available to be read using `sock_read` (i.e., when using a transport layer, such as TLS).

#### `sock_write(sockfd, buf, count)`

`sock_write` writes up to count bytes from the buffer pointed `buf` to the buffer associated with the socket `sockfd`.

The data isn't necessarily written to the socket and multiple calls to `sock_flush` might be required before all the data is actually sent.

On error, -1 will be returned. Otherwise returns 0. All the bytes are transferred to the socket's user level buffer.

**Note** this is actually a specific case of `sock_write2` and this macro actually calls `sock_write2`.

```c
#define sock_write(sockfd, buf, count) \
        sock_write2(.fduuid = (sockfd), .buffer = (buf), .length = (count))
```

#### `ssize_t sock_write2(...)`

Translates as: `ssize_t sock_write2_fn(sock_write_info_s options)`

These are the basic options (named arguments) available:

```c
typedef struct {
  /** The UUID for sending data. */
  intptr_t fduuid;
  /** The data to be sent. This can be either a byte stream or a file pointer
   * (`FILE *`). */
  const void* buffer;
  /** The length (size) of the buffer. irrelevant for file pointers. */
  size_t length;
  /** Starting point offset, when the buffer is a file
  * (see `sock_write_info_s.is_fd`). */
  off_t offset;
  /** The user land buffer will receive ownership of the buffer (forced as
   * TRUE
   * when `file` is set). */
  unsigned move : 1;
  /** The packet will be sent as soon as possible. */
  unsigned urgent : 1;
  /** The buffer contains the value of a file descriptor int - casting, not
   * pointing, i.e.: `.buffer = (void*)fd;` */
  unsigned is_fd : 1;
  /** for internal use */
  unsigned rsv : 1;
  /**/
} sock_write_info_s;
```

`sock_write2_fn` is the actual function behind the macro `sock_write2`.

`sock_write2` is similar to `sock_write`, except more special properties can be set.

On error, -1 will be returned. Otherwise returns 0. All the bytes are transferred to the socket's user level buffer.

#### `ssize_t sock_flush(intptr_t uuid)`

`sock_flush` writes the data in the internal buffer to the underlying file descriptor and closes the underlying fd once it's marked for closure (and all the data was sent).

Return value: 0 will be returned on success and -1 will be returned on an error or when the connection is closed.

**Please Note**: when using `libreact`, the `sock_flush` will be called automatically when the socket is ready. Also, when the connection is closed (during `sock_flush` or otherwise, the optional `reactor_on_close` callback will be called.


#### `ssize_t sock_flush_strong(intptr_t uuid)`

`sock_flush_strong` performs the same action as `sock_flush` but returns only after all the data was sent. This is an "active" wait, polling isn't performed.

#### `void sock_flush_all(void)`

Calls `sock_flush` for each UUID that has data waiting to be sent in the user land buffer.

#### `void sock_close(intptr_t uuid)`

`sock_close` marks the connection for disconnection once all the data was sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.

#### `void sock_force_close(intptr_t uuid)`

`sock_force_close` closes the connection immediately, without adhering to any protocol restrictions and without sending any remaining data in the connection buffer.

Also, once the connection is closed, the optional `reactor_on_close` callback will be called.

### Direct user level buffer API.

The user land buffer is constructed from pre-allocated Packet objects, each containing BUFFER_PACKET_SIZE (~17Kb) memory size dedicated for message data.

```c
// example configuration in libsock.h
#define BUFFER_PACKET_SIZE (1024 * 16)
```

Buffer packets - can be used for directly writing individual or multiple packets to the buffer instead of using the `sock_write(2)` helper functions / macros.

Unused Packets that were checked out using the `sock_checkout_packet` function, should never be freed using `free` and should always use the `sock_free_packet` function.

The data structure for a packet object provides detailed data about the packet's state and properties.

```c
typedef struct sock_packet_s {
  ssize_t length;
  void* buffer;
  /** Metadata about the packet. */
  struct {
    /** allows the linking of a number of packets together. */
    struct sock_packet_s* next;
    /** sets whether a packet can be inserted before this packet without
     * interrupting the communication flow. */
    unsigned can_interrupt : 1;
    /** sets whether a packet's buffer contains a file descriptor - casting, not
     * pointing, i.e.: `packet->buffer = (void*)fd;` */
    unsigned is_fd : 1;
    /** sets whether a packet's buffer is of type `FILE *`. */
    unsigned is_file : 1;
    /** Keeps the `FILE *` or fd open - avoids automatically closing the file.
     */
    unsigned keep_open : 1;
    /** sets whether a packet's buffer is pre-allocated (references the
     * `internal_memory`) or whether the data is allocated using `malloc` and
     * should be freed. */
    unsigned external : 1;
    /** sets whether this packet (or packet chain) should be inserted in before
     * the first `can_interrupt` packet, or at the end of the queu. */
    unsigned urgent : 1;
    /** Reserved for internal use - (memory shifting flag)*/
    unsigned internal_flag : 1;
    /** Reserved for future use. */
    unsigned rsrv : 1;
    /**/
  } metadata;
} sock_packet_s;
```

#### `sock_packet_s* sock_checkout_packet(void)`

Checks out a `sock_packet_s` from the packet pool, transferring the ownership of the memory to the calling function. returns NULL if both the pool was empty and memory allocation had failed.

#### `ssize_t sock_send_packet(intptr_t uuid, sock_packet_s* packet)`

Attaches a packet to a socket's output buffer and calls `sock_flush` for the socket.

The packet's memory is **always** handled by the `sock_send_packet` function (even on error). If the memory isn't part of the packet pool, it will be released using `free` after closing any files and freeing any memory associated with the packet.

Returns -1 on error. Returns 0 on success.

#### `void sock_free_packet(sock_packet_s* packet)`

Use `sock_free_packet` to free unused packets (including packet chains) that were checked-out using `sock_checkout_packet`.

NEVER use `free`, for any packet checked out using the pool management function `sock_checkout_packet`.

Passing a single packet will free also any packet it references (the `next` packet is also freed).

#### `_Bool sock_packets_pending(intptr_t uuid)`

Checks if there's data waiting to be written to the socket.

Returns TRUE (non 0) if there is data waiting to be written to the socket in the user-land buffer. Otherwise 0 is returned.

### RW Hooks.

The following struct is used for setting a the read/write hooks that will replace the default system calls to `recv` and `write`.

```c
typedef struct sock_rw_hook_s {
  /** Implement reading from a file descriptor. Should behave like the file
   * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.*/
  ssize_t (*read)(intptr_t fduuid, void* buf, size_t count);
  /** Implement writing to a file descriptor. Should behave like the file system
   * `write` call.*/
  ssize_t (*write)(intptr_t fduuid, const void* buf, size_t count);
  /** When implemented, this function will be called to flush any data remaining
   * in the internal buffer.
   * The function should return the number of bytes remaining in the internal
   * buffer (0 is a valid response) on -1 (on error).
   * It is important thet the `flush` function write to the underlying fd until the
   * writing operation returns -1 with EWOULDBLOCK or all the data was written.
   */
  ssize_t (*flush)(intptr_t fduuid);
  /** The `on_clear` callback is called when the socket data is cleared, ideally
   * when the connection is closed, allowing for dynamic sock_rw_hook_s memory
   * management.
   *
   * The `on_clear` callback is called within the socket's lock (mutex),
   * providing a small measure of thread safety. This means that `sock_API`
   * shouldn't be called from within this function (at least not in regards to
   * the specific `fd`). */
  void (*on_clear)(intptr_t fduuid, struct sock_rw_hook_s* rw_hook);
} sock_rw_hook_s;
```

#### `struct sock_rw_hook_s* sock_rw_hook_get(intptr_t fduuid)`

Gets a socket hook state (a pointer to the struct).

#### `int sock_rw_hook_set(intptr_t fduuid, struct sock_rw_hook_s* rw_hooks)`

Sets a socket hook state (a pointer to the struct).

****************************
Old version documentation waiting for update
****************************

## A Quick Example

The following example isn't very interesting, but it's good enough to demonstrate the API:

```c
#include "libsock.h"
// we're writing a small HTTP client
int main(int argc, char const* argv[]) {
  char request[] =
      "GET / HTTP/1.1\r\n"
      "Host: google.com\r\n"
      "\r\n";
  char buff[1024];
  ssize_t i_read;
  sock_lib_init();
  intptr_t uuid = sock_connect("www.google.com", "80");
  if (uuid == -1)
    perror("sock_connect failed"), exit(1);
  sock_write(uuid, request, sizeof(request) - 1);
  int run = 1;

  while ((i_read = sock_read(uuid, buff, 1024)) >= 0) {
    sock_flush(uuid);
    if (i_read == 0) {  // could be we hadn't finished connecting yet.
      if (run == 0)
        break;
      sched_yield();
    } else {
      run = 0;
      fprintf(stderr, "\n%.*s\n\n", (int)i_read, buff);
    }
  }
  fprintf(stderr, "done.\n");
  sock_close(uuid);
  return 0;
}
/**/
```
