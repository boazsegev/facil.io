/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIBSOCK
#define LIBSOCK "0.0.3"

/** \file
The libsock is a non-blocking socket helper library, using a user level buffer,
non-blocking
sockets and some helper functions.

This library is great when using it alongside `libreact`.
*/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* *****************************************************************************
Avoide including the "libreact.h" file, the following is all we need.
*/
struct Reactor;

/* *****************************************************************************
Process wide and helper sock_API.
*/

/**
Sets a socket to non blocking state.

This function is called automatically for the new socket, when using
`sock_accept` or `sock_connect`.
*/
int sock_set_non_block(int fd);

/**
Gets the maximum number of file descriptors this process can be allowed to
access.

If the "soft" limit is lower then the "hard" limit, the process's limits will be
extended to the allowed "hard" limit.
*/
ssize_t sock_max_capacity(void);

/**
Opens a listenning non-blocking socket. Return's the socket's file descriptor.

Returns -1 on error.
*/
int sock_listen(char* address, char* port);

/* *****************************************************************************
The main sock_API.
*/

/** Optional: initializes the library up to a `max_fd` value for a file
 * descriptor. */
int8_t init_socklib(size_t max_fd);

/**
`sock_accept` accepts a new socket connection from the listening socket
`server_fd`, allowing the use of `sock_` functions with this new file
descriptor.
*/
int sock_accept(struct Reactor* owner, int server_fd);

/**
`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns the new file descriptor fd. Retruns -1 on error.

NOT IMPLEMENTED (yet)
*/
int sock_connect(struct Reactor* owner, char* address, char* port);

/**
`sock_open` takes an existing file decriptor `fd` and initializes it's status as
open and available for `sock_API` calls.

This will reinitialize the data (user buffer etc') for the file descriptor
provided.

If a reactor pointer `owner` is provided, the `fd` will be attached to the
reactor.

Returns -1 on error and 0 on success.
*/
int sock_open(struct Reactor* owner, int fd);

/**
`sock_attach` sets the reactor owner for a socket and attaches the socket to the
reactor.

Use this function when the socket was already opened with no reactor association
and it's data (buffer etc') is already initialized.

This is useful for a delayed attachment, where some more testing is required
before attaching a socket to a reactor.

Returns -1 on error and 0 on success.
*/
int sock_attach(struct Reactor* owner, int fd);

/**
Clears a socket state data and buffer.

Use this function after the socket was closed remotely or without using the
`sock_API`.

This function does **not** effect the state of the socket at the reactor. Call
`reactor_add` / `reactor_close` / `reactor_remove` for those purposes.

If the socket is owned by the reactor, it is unnecessary to call this function
for remote close events or after a `reactor_close` call.
*/
void sock_clear(int fd);

/** Returns the state of the socket, similar to calling `fcntl(fd, F_GETFL)`.
 * Returns -1 if the connection is closed. */
int sock_status(int fd);
#define sock_is_closed(fd) (sock_status((fd)) < 0)

/**
`sock_read` attempts to read up to count bytes from file descriptor fd into
the
buffer starting at buf.

It's behavior should conform to the native `read` implementations, except some
data might be available in the `fd`'s buffer while it is not available to be
read using sock_read (i.e., when using a transport layer, such as TLS).

Also, some internal buffering will be used in cases where the transport layer
data available is larger then the data requested.
*/
ssize_t sock_read(int fd, void* buf, size_t count);

/**
`sock_write` writes up to count bytes from the buffer pointed buf to the
buffer associated with the file descriptor fd.

The data isn't neccessarily written to the socket and multiple calls to
`sock_flush` might be required for the data to be actually sent.

On error, -1 will be returned. Otherwise returns 0. All the bytes are
transferred to the socket's user level buffer.

**Note** this is actually a specific case of `sock_write2` and this macro
actually calls `sock_write2`.
*/
#define sock_write(sockfd, buf, count) \
  sock_write2(.fd = (sockfd), .buffer = (buf), .length = (count))

struct SockWriteOpt {
  /** The fd for sending data. */
  int fd;
  /** The data to be sent. This can be either a byte strteam or a file pointer
   * (`FILE *`). */
  const void* buffer;
  /** The length (size) of the buffer. irrelevant for file pointers. */
  size_t length;
  /** The user land buffer will receive ownership of the buffer (forced as
   * TRUE
   * when `file` is set). */
  unsigned move : 1;
  /** The packet will be sent as soon as possible. */
  unsigned urgent : 1;
  /** The buffer points to a file pointer: `FILE *`  */
  unsigned file : 1;
  /** for internal use */
  unsigned rsv : 1;
};
/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(struct SockWriteOpt options);
/**
`sock_write2` is similar to `sock_write`, except special properties can be set.

On error, -1 will be returned. Otherwise returns 0. All the bytes are
transferred to the socket's user level buffer.
*/
#define sock_write2(...) sock_write2_fn((struct SockWriteOpt){__VA_ARGS__})
/**
`sock_flush` writes the data in the internal buffer to the file descriptor fd
and closes the fd once it's marked for closure (and all the data was sent).

The number of bytes actually written to the fd will be returned. 0 will be
returned when no data is written and -1 will be returned on an error  or when
the connection is closed.

**Please Note**: when using `libreact`, the `sock_flush` will be called
automatically when the socket is ready.
*/
ssize_t sock_flush(int fd);
/**
`sock_close` marks the connection for disconnection once all the data was
sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.

If a reactor pointer is provied, the reactor API will be used and the
`on_close` callback should be called once the socket is closed.
*/
void sock_close(struct Reactor* reactor, int fd);
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.

If a reactor pointer is provied, the reactor API will be used and the
`on_close` callback should be called as expected.
*/
void sock_force_close(struct Reactor* reactor, int fd);

/* *****************************************************************************
Direct user level buffer API.

This API allows
*/

/** Each user land buffer packet's pre-alocated memory size (17Kb)*/
#ifndef BUFFER_PACKET_SIZE
#define BUFFER_PACKET_SIZE (1024 * 17)
#endif
#ifndef BUFFER_FILE_READ_SIZE
#define BUFFER_FILE_READ_SIZE (BUFFER_PACKET_SIZE - 1024)
#endif

/**
Buffer packets - can be used for directly writing individual or multiple packets
to the buffer instead of using the `sock_write(2)` helper functions / macros.

See `sock_checkout_packet` and `sock_send_packet` for more information.

Unused Packets that were checked out using the `sock_checkout_packet` function,
should never be freed using `free` and should always use the `sock_free_packet`
function.
*/
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
    /** Reserved for internal use - (memory shifting flag)*/
    unsigned nested : 1;
    /** Reserved for future use. */
    unsigned rsrv : 3;
  } metadata;
} sock_packet_s;

/**
Checks out a `sock_packet_s` from the packet pool, transfering the
ownership
of the memory to the calling function. returns NULL if the pool was empty and
memory allocation had failed.

Every checked out buffer packet comes with an attached buffer of
BUFFER_PACKET_SIZE bytes. This buffer is accessible using the `packet->buffer`
pointer (which can be safely overwritten to point to an external buffer).

This attached buffer is safely and automatically freed or returned to the memory
pool once `sock_send_packet` or `sock_free_packet` are called.
*/
sock_packet_s* sock_checkout_packet(void);
/**
Attaches a packet to a socket's output buffer and calls `sock_flush` for the
socket.

The packet's memory is **always** handled by the `sock_send_packet` function
(even on error).

Returns -1 on error. Returns 0 on success.
*/
ssize_t sock_send_packet(int fd, sock_packet_s* packet);

/**
Use `sock_free_packet` to free unused packets that were checked-out using
`sock_checkout_packet`.

NEVER use `free`, for any packet checked out using the pool management function
`sock_checkout_packet`.
*/
void sock_free_packet(sock_packet_s* packet);

/* *****************************************************************************
TLC - Transport Layer Callbacks.

Experimental
*/

/** This struct is used for setting Transport Layer data and hirarchy */
struct sockTLC {
  /** the next (lower level) Transport Layer encoder/decoder object. */
  struct sockTLC* next;
  /** opaque user data. */
  void* udata;
  /** TLC encoding function. Should return 0 on success and -1 on error. Should
   * edit the Packet directly.*/
  int (*wrap)(int fd, sock_packet_s* packet, void* udata);
  /** TLC decoding function. Should return 0 on success and -1 on error. Should
   * edit the buffer and the buffer length directly.*/
  int (*unwrap)(int fd,
                void* buffer,
                ssize_t* len,
                const size_t max_len,
                void* udata);
  /** The `on_clear` callback is called when the socket data is cleared, ideally
   * when the connection is closed, allowing for dynamic sockTLC memory
   * management.
   *
   * The `on_clear` callback is called within the socket's lock (mutex),
   * providing a small measure of thread safety. This means that `sock_API`
   * shouldn't be called from within this function (at least not in regards to
   * the specific `fd`). */
  void (*on_clear)(int fd, struct sockTLC* tlc);
};

/* *****************************************************************************
TLC implementation
*/

/** Gets a socket TLC chain. */
struct sockTLC* sock_tlc_get(int fd);

/** Sets a socket TLC chain. */
int sock_tlc_set(int fd, struct sockTLC* tlc);

/** Adds a TLC to the socket's TLC chain. */
int sock_tlc_add(int fd, struct sockTLC* tlc);

#endif /* LIBSOCK */
