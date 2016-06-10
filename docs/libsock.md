# The Non-Blocking POSIX Sockets Library

`libsock` is a non-blocking socket helper library, providing a user level buffer, non-blocking
sockets and some helper functions.

This library is great when using it alongside `libreact`, since `libreact` will automatically call `sock_flush` for every socket that was added to the reactor and is ready to send data. Also, `libsock` can automatically add sockets to the reactor when `sock_accept` or `sock_connect` are provided with a reactor object's pointer.

`libsock` requires only the following two files from this repository: [`src/libsock.h`](../src/libsock.h) and [`src/libsock.c`](../src/libsock.c).

If you're looking to utilize `lib-server` the only helpful section in this file is the section regarding Transport Layer Callbacks (TLC) that can be used to implement TLS or other intermediary protocols.

## The Socket object

In order to allow `libsock` to be used with any existing libraries, the socket object is kept in an internal library buffer that is thread-safe.

Sockets data is accessed using their file descriptor (int), just like any POISIX socket. For further information, look over the `static struct FDData` declaration in the `libsock.c` file.

To initialize this data storage (an array of `FDData` objects), the library command `init_socklib` is used. If the storage isn't initialized up front, it will be initialized (and reinitialized) dynamically as needed, at the expense of performance.


### `init_socklib(size_t max_fd)`

Initializes the library up to a `max_fd` value for a file descriptor.

For maximum capacity, it is recommended that you call:

```c
init_socklib( sock_max_capacity() );
```

## The `libsock` API

The `libsock` API can be divided into a few different categories:

- General helper functions

- Accepting connections and opening new sockets.

- Sending and receiving data.

- Direct user level buffer API.

- Transport Layer Callbacks (TLC).

It should be noted that the library was built with convenience and safety in mind, so it incurs a performance cost related to these safety and convenience features (i.e. mutex locking protects the user-land buffer at the cost of performance).

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

#### `int sock_listen(char* address, char* port)`

Opens a listenning non-blocking socket. Return's the socket's file descriptor.

Returns -1 on error or the listenning socket's `fd` on sucess.

### Accepting connections and opening new sockets

Accepting connections, initiating connections and opening sockets - as well as attaching open sockets to the `libsock` state machine - are required actions so that we can use sockets with the `libsock` API.

#### `int sock_accept(struct Reactor* owner, int server_fd)`

`sock_accept` accepts a new socket connection from the listening socket
`server_fd`, allowing the use of `sock_` functions with this new file
descriptor.

#### `int sock_connect(struct Reactor* owner, char* address, char* port)`

`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns the new file descriptor fd. Retruns -1 on error.

#### `int sock_open(struct Reactor* owner, int fd)`

`sock_open` takes an existing file decriptor `fd` and initializes it's status as
open and available for `sock_API` calls.

This will reinitialize the data (user buffer etc') for the file descriptor
provided.

If a reactor pointer `owner` is provided, the `fd` will be attached to the
reactor.

Returns -1 on error and 0 on success.

#### `int sock_attach(struct Reactor* owner, int fd)`

`sock_attach` sets the reactor owner for a socket and attaches the socket to the
reactor.

Use this function when the socket was already opened with no reactor association
and it's data (buffer etc') is already initialized.

This is useful for a delayed attachment, where some more testing is required
before attaching a socket to a reactor.

Returns -1 on error and 0 on success.

#### `void sock_clear(int fd)`

Clears a socket state data and buffer.

Use this function after the socket was closed remotely or without using the
`sock_API`.

This function does **not** effect the state of the socket at the reactor. Call
`reactor_add` / `reactor_close` / `reactor_remove` for those purposes.

If the socket is owned by the reactor, it is unnecessary to call this function
for remote close events or after a `reactor_close` call.

#### `int sock_status(int fd)`

Returns the state of the socket, similar to calling `fcntl(fd, F_GETFL)`.

Returns -1 if the connection is closed.

The macro `sock_is_closed(fd)` translates to `(sock_status((fd)) < 0)`

### Sending and receiving data

Reading and writing data to a socket, using the `libsock` API, allows easy access to a user level buffer and file streaming capabilities, as well as Transport Layer Callbacks (TLC) chains that simplify the task of implementing layered protocols (i.e. TLS).

#### `ssize_t sock_read(int fd, void* buf, size_t count)`

`sock_read` attempts to read up to count bytes from file descriptor fd into
the
buffer starting at buf.

It's behavior should conform to the native `read` implementations, except some
data might be available in the `fd`'s buffer while it is not available to be
read using sock_read (i.e., when using a transport layer, such as TLS).

Also, some internal buffering will be used in cases where the transport layer
data available is larger then the data requested.

#### `sock_write(sockfd, buf, count)`

`sock_write` writes up to count bytes from the buffer pointed `buf` to the buffer associated with the file descriptor `fd`.

The data isn't necessarily written to the socket and multiple calls to `sock_flush` might be required for the data to be actually sent.

On error, -1 will be returned. Otherwise returns 0. All the bytes are transferred to the socket's user level buffer.

**Note** this is actually a specific case of `sock_write2` and this macro actually calls `sock_write2`.

```c
#define sock_write(sockfd, buf, count) \
        sock_write2(.fd = (sockfd), .buffer = (buf), .length = (count))
```

#### `ssize_t sock_write2(...)`

Translates as: `ssize_t sock_write2_fn(struct SockWriteOpt options)`

These are the basic options (named arguments) available:

```c
struct SockWriteOpt {
  /* The fd for sending data. */
  int fd;
  /* The data to be sent. This can be either a byte stream or a file pointer
   * (`FILE *`). */
  const void* buffer;
  /* The length (size) of the buffer. irrelevant for file pointers. */
  size_t length;
  /* The user land buffer will receive ownership of the buffer (forced as
   * TRUE
   * when `file` is set). */
  unsigned move : 1;
  /* The packet will be sent as soon as possible. */
  unsigned urgent : 1;
  /* The buffer points to a file pointer: `FILE *`  */
  unsigned file : 1;
  /* for internal use */
  unsigned rsv : 1;
  /**/
};
```
`sock_write2_fn` is the actual function behind the macro `sock_write2`.

`sock_write2` is similar to `sock_write`, except special properties can be set.

On error, -1 will be returned. Otherwise returns 0. All the bytes are transferred to the socket's user level buffer.

#### `ssize_t sock_flush(int fd)`

`sock_flush` writes the data in the internal buffer to the file descriptor fd and closes the fd once it's marked for closure (and all the data was sent).

The number of bytes actually written to the fd will be returned. 0 will be returned when no data is written and -1 will be returned on an error or when the connection is closed.

**Please Note**: when using `libreact`, the `sock_flush` will be called automatically when the socket is ready.

#### `void sock_close(struct Reactor* reactor, int fd)`

`sock_close` marks the connection for disconnection once all the data was sent.

The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.

If a reactor pointer is provided, the reactor API will be used and the `on_close` callback should be called once the socket is closed.

#### `void sock_force_close(struct Reactor* reactor, int fd)`

`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.

If a reactor pointer is provided, the reactor API will be used and the `on_close` callback should be called as expected.

### Direct user level buffer API.

The user land buffer is constructed from pre-allocated Packet objects, each containing BUFFER_PACKET_SIZE (~17Kb) memory size dedicated for message data.

```c
#define BUFFER_PACKET_SIZE (1024 * 17)
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
    /** sets whether a packet's buffer is of type `FILE *`. */
    unsigned is_file : 1;
    /** sets whether a packet's buffer is pre-allocated (references the
     * `internal_memory`) or whether the data is allocated using `malloc` and
     * should be freed. */
    unsigned external : 1;
    /** sets whether this packet (or packet chain) should be inserted in before
     * the first `can_interrupt` packet, or at the end of the queu. */
    unsigned urgent : 1;
    /** Reserved for future use. */
    unsigned rsrv : 4;
    /**/
  } metadata;
} sock_packet_s;
};
```

#### `sock_packet_s* sock_checkout_packet(void)`

Checks out a `sock_packet_s` from the packet pool, transferring the ownership of the memory to the calling function. returns NULL if both the pool was empty and memory allocation had failed.

#### `ssize_t sock_send_packet(int fd, sock_packet_s* packet)`

Attaches a packet to a socket's output buffer and calls `sock_flush` for the
socket.

The packet's memory is **always** handled by the `sock_send_packet` function (even on error).

Returns -1 on error. Returns 0 on success.

#### `void sock_free_packet(sock_packet_s* packet)`

Use `sock_free_packet` to free unused packets that were checked-out using `sock_checkout_packet`.

NEVER use `free`, for any packet checked out using the pool management function `sock_checkout_packet`.

### TLC - Transport Layer Callbacks.

incomplete documentation.

## A Quick Example

The following example isn't very interesting, but it's good enough to demonstrate the API:

```c
#include "libreact.h"
#include "libsock.h"
// we're writing a small server, many included files...
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

// this will accept connections,
// it will be a simple HTTP hello world. (with keep alive)
void on_data(struct Reactor* reactor, int fd);

struct Reactor r = {.on_data = on_data, .maxfd = 1024};
int srvfd; // to make it simple, we'll have a global object

// this will handle the exit signal (^C).
void on_sigint(int sig) {
  reactor_stop(&r);
}

int main(int argc, char const* argv[]) {
  printf("starting up an http hello world example on port 3000\n");
  // setup the exit signal
  signal(SIGINT, on_sigint);
  // create a server socket... this will take a moment...
  char* port = "3000";
  // setup the address
  struct addrinfo hints;
  struct addrinfo* servinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me

  if (getaddrinfo(NULL, port, &hints, &servinfo)) {
    perror("addr err");
    return -1;
  }

  srvfd =   // get the file descriptor
      socket(servinfo->ai_family, servinfo->ai_socktype,
                                  servinfo->ai_protocol);
  if (srvfd <= 0) {
    perror("addr err");
    freeaddrinfo(servinfo);
    return -1;
  }

  { // avoid the "address taken"
    int optval = 1;
    setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &optval,
              sizeof(optval));
  }

  // bind the address to the socket

  if (bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
    perror("bind err");
    freeaddrinfo(servinfo);
    return -1;
  }

  // make sure the socket is non-blocking

  static int flags;
  if (-1 == (flags = fcntl(srvfd, F_GETFL, 0)))
    flags = 0;
  fcntl(srvfd, F_SETFL, flags | O_NONBLOCK);

  // listen in
  listen(srvfd, SOMAXCONN);
  if (errno)
    perror("starting. last error was");

  freeaddrinfo(servinfo); // free the address data memory

  // now that everything is ready, call the reactor library...
  reactor_init(&r);
  reactor_add(&r, srvfd);

  while (reactor_review(&r) >= 0)
    ;

  if (errno)
    perror("\nFinished. last error was");
}

void on_data(struct Reactor* reactor, int fd) {
  if (fd == srvfd) { // yes, this is our listening socket...
    int client = 0;
    unsigned int len = 0;
    while ((client = accept(fd, NULL, &len)) > 0) {
      reactor_add(&r, client);
    }  // reactor is edge triggered, we need to clear the cache.
  } else {
    char buff[8094];
    ssize_t len;
    static char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=2\r\n"
        "\r\n"
        "Hello World!\r\n";

    if ((len = recv(fd, buff, 8094, 0)) > 0) {
      len = write(fd, response, strlen(response));
    }
  }
}
```
