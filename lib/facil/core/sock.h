#ifndef H_LIBSOCK_H
#define H_LIBSOCK_H
/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define LIB_SOCK_VERSION_MAJOR 0
#define LIB_SOCK_VERSION_MINOR 3
#define LIB_SOCK_VERSION_PATCH 2

/** \file
The `sock.h` is a non-blocking socket helper library, using a user level buffer,
non-blocking sockets and some helper functions.

This library is great when using it alongside `evio.h`.

The library is designed to be thread safe, but not fork safe - mostly since
sockets, except listenning sockets, shouldn't be shared among processes.

Socket connections accepted or created using this library will use the
TCP_NODELAY option by default.

Non TCP/IP stream sockets and file descriptors (i.e., unix sockets) can be
safely used with this library. However, file descriptors that can't use the
`read` or `write` system calls MUST set correct Read / Write hooks or they will
fail.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

/* *****************************************************************************
User land buffer settings for every packet's pre-alocated memory size (17Kb)

This information is also useful when implementing read / write hooks.
*/
#ifndef BUFFER_PACKET_SIZE
#define BUFFER_PACKET_SIZE                                                     \
  (1024 * 16) /* Use 32 Kb. With sendfile, 16 Kb appears to work better. */
#endif
#ifndef BUFFER_FILE_READ_SIZE
#define BUFFER_FILE_READ_SIZE 4096UL
#endif
#ifndef BUFFER_PACKET_POOL
#define BUFFER_PACKET_POOL 1024
#endif

/* *****************************************************************************
A simple, predictable UUID for file-descriptors, for collision prevention
*/
#ifndef sock_uuid2fd
#define sock_uuid2fd(uuid) ((intptr_t)((uintptr_t)uuid >> 8))
#endif

/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

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
access (== maximum fd value + 1).

If the "soft" limit is lower then the "hard" limit, the process's limits will be
extended to the allowed "hard" limit.
*/
ssize_t sock_max_capacity(void);

/* *****************************************************************************
The main sock_API.
*/

/**
Opens a listening non-blocking socket. Return's the socket's UUID.

Returns -1 on error. Returns a valid socket (non-random) UUID.

UUIDs with values less then -1 are valid values, depending on the system's
byte-ordering.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_listen(const char *address, const char *port);

/**
`sock_accept` accepts a new socket connection from the listening socket
`server_fd`, allowing the use of `sock_` functions with this new file
descriptor.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_accept(intptr_t srv_uuid);

/**
`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).

When using `evio`, remember to call `int evio_add(sock_fd2uuid(uuid),
(void*)uuid);` to listen for events.

NOTICE:

This function is non-blocking, meaning that the connection probably wasn't
established by the time the function returns (this prevents the function from
hanging while waiting for a network timeout).

Use select, poll, `evio` or other solutions to review the connection state
before attempting to write to the socket.
*/
intptr_t sock_connect(char *address, char *port);

/**
`sock_open` takes an existing file descriptor `fd` and initializes it's status
as open and available for `sock_API` calls, returning a valid UUID.

This will reinitialize the data (user buffer etc') for the file descriptor
provided, calling the `sock_on_close` callback if the `fd` was previously
marked as used.

When using `evio`, remember to call `int evio_add(sock_fd2uuid(uuid),
(void*)uuid);` to listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_open(int fd);

/**
Returns 1 if the uuid refers to a valid and open, socket.

Returns 0 if not.
*/
int sock_isvalid(intptr_t uuid);

/** The return type for the `sock_peer_addr` function. */
typedef struct {
  uint32_t addrlen;
  struct sockaddr *addr;
} sock_peer_addr_s;
/** Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`addr == NULL`).
 * The information is only available when the socket was accepted using
 * `sock_accept` or opened using `sock_connect`.
 */
sock_peer_addr_s sock_peer_addr(intptr_t uuid);

/**
`sock_fd2uuid` takes an existing file decriptor `fd` and returns it's active
`uuid`.

If the file descriptor is marked as closed (wasn't opened / registered with
`libsock`) the function returns -1;

If the file descriptor was closed remotely (or not using `libsock`), a false
positive will be possible. This is not an issue, since the use of an invalid fd
will result in the registry being updated and the fd being closed.

Returns -1 on error. Returns a valid socket (non-random) UUID.
*/
intptr_t sock_fd2uuid(int fd);

/**
OVERRIDABLE:

"Touches" a socket connection.

This is a place holder for an optional callback for systems that apply timeout
reviews.

`sock` supplies a default implementation (that does nothing) is cases where a
callback wasn't defined.
*/
void sock_touch(intptr_t uuid);

/**
OVERRIDABLE:.

This is a place holder for an optional callback for when the socket is closed
locally.

Notice that calling `sock_close()` won't close the socket before all the data in
the buffer is sent. This function will be called only one the connection is
actually closed.

`sock` supplies a default implementation (that does nothing) is cases where a
callback wasn't defined.
*/
void sock_on_close(intptr_t uuid);

/**
`sock_read` attempts to read up to count bytes from the socket into the buffer
starting at buf.

`sock_read`'s return values are wildly different then the native return values
and they aim at making far simpler sense.

`sock_read` returns the number of bytes read (0 is a valid return value which
simply means that no bytes were read from the buffer).

On a connection error (NOT EAGAIN or EWOULDBLOCK), signal interrupt, or when the
connection was closed, `sock_read` returns -1.

The value 0 is the valid value indicating no data was read.

Data might be available in the kernel's buffer while it is not available to be
read using `sock_read` (i.e., when using a transport layer, such as TLS).
*/
ssize_t sock_read(intptr_t uuid, void *buf, size_t count);

typedef struct {
  /** The fsocket uuid for sending data. */
  intptr_t uuid;
  union {
    /** The in-memory data to be sent. */
    const void *buffer;
    /** The data to be sent, if this is a file. */
    const intptr_t data_fd;
  };
  union {
    /** This deallocation callback will be called when the packet is finished
     * with the buffer if the `move` flags is set.
     * If no deallocation callback is `free` will be used.
     */
    void (*dealloc)(void *buffer);
    /** This is an alternative deallocation callback accessor (same memory space
     * as `dealloc`) for conveniently setting the file `close` callback.
     */
    void (*close)(intptr_t fd);
  };
  /** The length (size) of the buffer, or the amount of data to be sent from the
   * file descriptor.
   */
  size_t length;
  /** Starting point offset from the buffer or file descriptor's beginning. */
  off_t offset;
  /** When sending data from the memory, using `move` will prevent copy and
   * memory allocations, moving the memory ownership to the `sock_write2`
   * function.
   */
  unsigned move : 1;
  /** The packet will be sent as soon as possible. */
  unsigned urgent : 1;
  /**
   * The buffer contains the value of a file descriptor (`int`). i.e.:
   *  `.data_fd = fd` or `.buffer = (void*)fd;`
   */
  unsigned is_fd : 1;
  /**
   * The buffer **points** to a file descriptor (`int`): `.buffer = (void*)&fd;`
   *
   * In the case the `dealloc` function will be called, allowing both closure
   * and deallocation of the `int` pointer.
   *
   * This feature can be used for file reference counting, such as implemented
   * by the `fio_file_cache` service.
   */
  unsigned is_pfd : 1;
  /** for internal use */
  unsigned rsv : 1;
} sock_write_info_s;

void SOCK_DEALLOC_NOOP(void *arg);
#define SOCK_DEALLOC_NOOP SOCK_DEALLOC_NOOP

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(sock_write_info_s options);

/**
`sock_write2` is similar to `sock_write`, except special properties can be set.

On error, -1 will be returned. Otherwise returns 0. All the bytes are
transferred to the socket's user level buffer.
*/
#define sock_write2(...) sock_write2_fn((sock_write_info_s){__VA_ARGS__})
/**
`sock_write` writes up to count bytes from the buffer pointed `buf` to the
buffer associated with the socket `sockfd`.

The data isn't necessarily written to the socket and multiple calls to
`sock_flush` might be required before all the data is actually sent.

On error, -1 will be returned. Otherwise returns 0. All the bytes are
transferred to the socket's user level buffer.

**Note** this is actually a specific case of `sock_write2` and this macro
actually calls `sock_write2`.
*/
#define sock_write(sock_uuid, buf, count)                                      \
  sock_write2(.uuid = (sock_uuid), .buffer = (buf), .length = (count))

/**
Sends data from a file as if it were a single atomic packet (sends up to
length bytes or until EOF is reached).

Once the file was sent, the `source_fd` will be closed using `close`.

The file will be buffered to the socket chunk by chunk, so that memory
consumption is capped. The system's `sendfile` might be used if conditions
permit.

`offset` dictates the starting point for te data to be sent and length sets
the maximum amount of data to be sent.

Returns -1 and closes the file on error. Returns 0 on success.
*/
UNUSED_FUNC static inline ssize_t
sock_sendfile(intptr_t uuid, intptr_t source_fd, off_t offset, size_t length) {
  return sock_write2(.uuid = uuid, .buffer = (void *)(source_fd),
                     .length = length, .is_fd = 1, .offset = offset, .move = 1);
}

/**
`sock_flush` writes the data in the internal buffer to the underlying file
descriptor and closes the underlying fd once it's marked for closure (and all
the data was sent).

Return value: 0 will be returned on success and -1 will be returned on an error
or when the connection is closed.
*/
ssize_t sock_flush(intptr_t uuid);
/**
`sock_flush_strong` performs the same action as `sock_flush` but returns only
after all the data was sent. This is a "busy" wait, polling isn't performed.
*/
void sock_flush_strong(intptr_t uuid);
/**
Calls `sock_flush` for each file descriptor that's buffer isn't empty.
*/
void sock_flush_all(void);
/**
`sock_close` marks the connection for disconnection once all the data was sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.
*/
void sock_close(intptr_t uuid);
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.
*/
void sock_force_close(intptr_t uuid);

/* *****************************************************************************
Direct user level buffer API.

The following API allows data to be written directly to the packet, minimizing
memory copy operations.
*/

/**
The buffer of a user-land sock-packet.

Remember to set the correct `len` value, so the full amount of the data is sent.

See `sock_buffer_checkout`, `sock_buffer_send` and `sock_buffer_free` for more
information.
*/
typedef struct sock_buffer_s {
  size_t len;
  uint8_t buf[BUFFER_PACKET_SIZE];
} sock_buffer_s;

/**
Checks out a `sock_buffer_s` from the buffer pool.

This function will hang until a buffer becomes available, so never check out
more then a single buffer at a time and remember to free or send the buffer
using the `sock_buffer_*` functions.

Every checked out buffer packet comes with an attached buffer of
BUFFER_PACKET_SIZE bytes. This buffer is accessible using the `->buf`
pointer.

This attached buffer's memory is automatically manages as long as the
`sock_buffer_send` or `sock_buffer_free` functions are called.
*/
sock_buffer_s *sock_buffer_checkout(void);
/**
Attaches a packet to a socket's output buffer and calls `sock_flush` for the
socket.

Returns -1 on error. Returns 0 on success. The `buffer` memory is always
automatically managed.
*/
ssize_t sock_buffer_send(intptr_t uuid, sock_buffer_s *buffer);

/**
Returns TRUE (non 0) if there is data waiting to be written to the socket in the
user-land buffer.
*/
int sock_has_pending(intptr_t uuid);

/**
Use `sock_buffer_free` to free unused buffers that were checked-out using
`sock_buffer_checkout`.
*/
void sock_buffer_free(sock_buffer_s *buffer);

/* *****************************************************************************
TLC - Transport Layer Callback.

Experimental
*/

/**
The following struct is used for setting a the read/write hooks that will
replace the default system calls to `recv` and `write`. */
typedef struct sock_rw_hook_s {
  /** Implement reading from a file descriptor. Should behave like the file
   * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.*/
  ssize_t (*read)(intptr_t uuid, void *buf, size_t count);
  /** Implement writing to a file descriptor. Should behave like the file system
   * `write` call.*/
  ssize_t (*write)(intptr_t uuid, const void *buf, size_t count);
  /** When implemented, this function will be called to flush any data remaining
   * in the internal buffer.
   * The function should return the number of bytes remaining in the internal
   * buffer (0 is a valid response) on -1 (on error).
   * It is important thet the `flush` function write to the underlying fd until
   * the
   * writing operation returns -1 with EWOULDBLOCK or all the data was written.
   */
  ssize_t (*flush)(intptr_t fduuid);
  /** The `on_close` callback is called when the socket is closed, allowing for
   * dynamic sock_rw_hook_s memory management.
   *
   * The `on_close` callback should manage is own thread safety mechanism, if
   * required. */
  void (*on_close)(intptr_t fduuid, struct sock_rw_hook_s *rw_hook);
} sock_rw_hook_s;

/* *****************************************************************************
RW hooks implementation
*/

/** Gets a socket hook state (a pointer to the struct). */
struct sock_rw_hook_s *sock_rw_hook_get(intptr_t fduuid);

/** Sets a socket hook state (a pointer to the struct). */
int sock_rw_hook_set(intptr_t fduuid, sock_rw_hook_s *rw_hooks);

/** The default Read/Write hooks used for system Read/Write */
extern const sock_rw_hook_s SOCK_DEFAULT_HOOKS;

/* *****************************************************************************
test
*/
#ifdef DEBUG
void sock_libtest(void);
#endif

/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
}
#endif

#endif /* H_LIBSOCK_H */
