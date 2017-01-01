/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_SOCK
#define LIB_SOCK "0.2.2"
#define LIB_SOCK_VERSION_MAJOR 0
#define LIB_SOCK_VERSION_MINOR 2
#define LIB_SOCK_VERSION_PATCH 2

/** \file
The libsock is a non-blocking socket helper library, using a user level buffer,
non-blocking sockets and some helper functions.

This library is great when using it alongside `libreact`.

The library is designed to be thread safe, but not fork safe.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/* *****************************************************************************
User land buffer settings for every packet's pre-alocated memory size (17Kb)

This information is also useful when implementing read / write hooks.
*/
#ifndef BUFFER_PACKET_SIZE
#define BUFFER_PACKET_SIZE                                                     \
  (1024 * 16) /* Use 32 Kb. With sendfile, 16 Kb might be better. */
#endif
#ifndef BUFFER_FILE_READ_SIZE
#define BUFFER_FILE_READ_SIZE BUFFER_PACKET_SIZE
#endif
#ifndef BUFFER_PACKET_POOL
#define BUFFER_PACKET_POOL 1024
#endif

/* *****************************************************************************
A simple, predictable UUID for file-descriptors, for collision prevention
*/

#ifndef FD_UUID_TYPE_DEFINED
#define FD_UUID_TYPE_DEFINED
/** fduuid_u is used to identify a specific connection, helping to manage file
 * descriptor collisions (when a new connection receives an old connection's
 * file descriptor), especially when the `on_close` event is fired after an
 * `accept` was called and the old file descriptor was already recycled.
 *
 * This requires that sizeof(int) < sizeof(uintptr_t) or sizeof(int)*8 >= 32
 */
typedef union {
  intptr_t uuid;
  struct {
    int fd : (sizeof(int) < sizeof(intptr_t) ? (sizeof(int) * 8) : 24);
    unsigned counter : (sizeof(int) < sizeof(intptr_t)
                            ? ((sizeof(intptr_t) - sizeof(int)) * 8)
                            : ((sizeof(intptr_t) * 8) - 24));
  } data;
} fduuid_u;

#define FDUUID_FAIL(uuid) (uuid == -1)
#define sock_uuid2fd(uuid) ((fduuid_u)(uuid)).data.fd
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

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

NOTICE:

This function is non-blocking, meaning that the connection probably wasn't
established by the time the function returns (this prevents the function from
hanging while waiting for a network timeout).

Use select, poll, `libreact` or other solutions to review the connection state
before attempting to write to the socket.
*/
intptr_t sock_connect(char *address, char *port);

/**
`sock_open` takes an existing file descriptor `fd` and initializes it's status
as open and available for `sock_API` calls, returning a valid UUID.

This will reinitialize the data (user buffer etc') for the file descriptor
provided, calling the `reactor_on_close` callback if the `fd` was previously
marked as used.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

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
"Touches" a socket connection. This is a place holder for an optional callback
for systems that apply timeout reviews. `libsock` supplies a default
implementation (that does nothing) is cases where a callback wasn't defined.
*/
void sock_touch(intptr_t uuid);

/**
`sock_read` attempts to read up to count bytes from the socket into the buffer
starting at buf.

`sock_read`'s return values are wildly different then the native return values
and they aim at making far simpler sense.

`sock_read` returns the number of bytes read (0 is a valid return value which
simply means that no bytes were read from the buffer).

On a connection error (NOT EAGAIN or EWOULDBLOCK) or signal interrupt,
`sock_read` returns -1.

Data might be available in the kernel's buffer while it is not available to be
read using `sock_read` (i.e., when using a transport layer, such as TLS).
*/
ssize_t sock_read(intptr_t uuid, void *buf, size_t count);

typedef struct {
  /** The fd for sending data. */
  intptr_t fduuid;
  /** The data to be sent. This can be either a byte stream or a file pointer
   * (`FILE *`). */
  const void *buffer;
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
} sock_write_info_s;
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
#define sock_write(uuid, buf, count)                                           \
  sock_write2(.fduuid = (uuid), .buffer = (buf), .length = (count))

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
__unused static inline ssize_t sock_sendfile(intptr_t uuid, int source_fd,
                                             off_t offset, size_t length) {
  return sock_write2(.fduuid = uuid, .buffer = (void *)((intptr_t)source_fd),
                     .length = length, .is_fd = 1, .offset = offset);
}

/**
`sock_flush` writes the data in the internal buffer to the underlying file
descriptor and closes the underlying fd once it's marked for closure (and all
the data was sent).

Return value: 0 will be returned on success and -1 will be returned on an error
or when the connection is closed.

**Please Note**: when using `libreact`, the `sock_flush` will be called
automatically when the socket is ready.
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
*/

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
  void *buffer;
  /** Metadata about the packet. */
  struct {
    /** allows the linking of a number of packets together. */
    struct sock_packet_s *next;
    /** Starting point offset, when the buffer is a file (see
     * `sock_packet_s.metadata.is_fd`). */
    off_t offset;
    /** sets whether a packet can be inserted before this packet without
     * interrupting the communication flow. */
    unsigned can_interrupt : 1;
    /** sets whether a packet's buffer contains a file descriptor - casting, not
     * pointing, i.e.: `packet->buffer = (void*)fd;` */
    unsigned is_fd : 1;
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
    unsigned rsrv : 2;
    /**/
  } metadata;
} sock_packet_s;

/**
Checks out a `sock_packet_s` from the packet pool, transfering the
ownership of the memory to the calling function. The function will hang until a
packet becomes available, so never check out more then a single packet at a
time and remember to free or send the packet.

Every checked out buffer packet comes with an attached buffer of
BUFFER_PACKET_SIZE bytes. This buffer is accessible using the `packet->buffer`
pointer (which can be safely overwritten to point to an external buffer).

This attached buffer is safely and automatically freed or returned to the memory
pool once `sock_send_packet` or `sock_free_packet` are called.
*/
sock_packet_s *sock_checkout_packet(void);
/**
Attaches a packet to a socket's output buffer and calls `sock_flush` for the
socket.

The packet's memory is **always** handled by the `sock_send_packet` function
(even on error).

Returns -1 on error. Returns 0 on success.
*/
ssize_t sock_send_packet(intptr_t uuid, sock_packet_s *packet);

/**
Returns TRUE (non 0) if there is data waiting to be written to the socket in the
user-land buffer.
*/
_Bool sock_packets_pending(intptr_t uuid);

/**
Use `sock_free_packet` to free unused packets that were checked-out using
`sock_checkout_packet`.

NEVER use `free`, for any packet checked out using the pool management function
`sock_checkout_packet`.
*/
void sock_free_packet(sock_packet_s *packet);

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
  ssize_t (*read)(intptr_t fduuid, void *buf, size_t count);
  /** Implement writing to a file descriptor. Should behave like the file system
   * `write` call.*/
  ssize_t (*write)(intptr_t fduuid, const void *buf, size_t count);
  /** When implemented, this function will be called to flush any data remaining
   * in the internal buffer.
   * The function should return the number of bytes remaining in the internal
   * buffer (0 is a valid response) on -1 (on error).
   * It is important thet the `flush` function write to the underlying fd until
   * the
   * writing operation returns -1 with EWOULDBLOCK or all the data was written.
   */
  ssize_t (*flush)(intptr_t fduuid);
  /** The `on_clear` callback is called when the socket data is cleared, ideally
   * when the connection is closed, allowing for dynamic sock_rw_hook_s memory
   * management.
   *
   * The `on_clear` callback should manage is own thread safety mechanism, if
   * required. */
  void (*on_clear)(intptr_t fduuid, struct sock_rw_hook_s *rw_hook);
} sock_rw_hook_s;

/* *****************************************************************************
RW hooks implementation
*/

/** Gets a socket hook state (a pointer to the struct). */
struct sock_rw_hook_s *sock_rw_hook_get(intptr_t fduuid);

/** Sets a socket hook state (a pointer to the struct). */
int sock_rw_hook_set(intptr_t fduuid, sock_rw_hook_s *rw_hooks);

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

#endif /* LIB_SOCK */
