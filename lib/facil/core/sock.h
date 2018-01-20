#ifndef H_LIBSOCK_H
#define H_LIBSOCK_H
/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define LIB_SOCK_VERSION_MAJOR 0
#define LIB_SOCK_VERSION_MINOR 4
#define LIB_SOCK_VERSION_PATCH 0

#ifndef LIB_SOCK_MAX_CAPACITY
/** The maximum `fd` value `sock.h` should support. */
#define LIB_SOCK_MAX_CAPACITY 4194304
#endif
/** \file
 * The `sock.h` is a non-blocking socket helper library, using a user level
 * buffer, non-blocking sockets and some helper functions.
 *
 * This library is great when using it alongside `evio.h`.
 *
 * The library is designed to be thread safe, but not fork safe - mostly since
 * sockets, except listenning sockets, shouldn't be shared among processes.
 *
 * Socket connections accepted or created using this library will use the
 * TCP_NODELAY option by default.
 *
 * Non TCP/IP stream sockets and file descriptors (i.e., unix sockets) can be
 * safely used with this library. However, file descriptors that can't use the
 * `read` or `write` system calls MUST set correct Read / Write hooks or they
 * will fail.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// clang-format off
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#   if defined(__has_include)
#     if __has_include(<endian.h>)
#      include <endian.h>
#     elif __has_include(<sys/endian.h>)
#      include <sys/endian.h>
#     endif
#   endif
#   if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
                __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __BIG_ENDIAN__
#   endif
#endif
// clang-format on

#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

/* *****************************************************************************
A simple, predictable UUID for file-descriptors, for collision prevention
*/
#ifndef sock_uuid2fd
#define sock_uuid2fd(uuid) ((int)((uintptr_t)uuid >> 8))
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
 * Sets a socket to non blocking state.
 *
 * This function is called automatically for the new socket, when using
 * `sock_accept` or `sock_connect`.
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
 * Opens a listening non-blocking socket. Return's the socket's UUID.
 *
 * If `port` is provided (`address` can be NULL), a TCP/IP socket will be
 * opened. Otherwise, `address` is required and a Unix Socket will be used
 * (remember Unix Sockets have name length restrictions).
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 *
 * UUIDs with values less then -1 are valid values, depending on the system's
 * byte-ordering.
 *
 * Socket UUIDs are predictable and shouldn't be used outside the local system.
 * They protect against connection mixups on concurrent systems (i.e. when
 * saving client data for "broadcasting" or when an old client task is preparing
 * a response in the background while a disconnection and a new connection occur
 * on the same `fd`).
 */
intptr_t sock_listen(const char *address, const char *port);

/**
* `sock_accept` accepts a new socket connection from the listening socket
* `server_fd`, allowing the use of `sock_` functions with this new file
* descriptor.

* When using `evio`, remember to call `int evio_add(intptr_t uuid);` to
* listen for events.
*
* Returns -1 on error. Returns a valid socket (non-random) UUID.
*
* Socket UUIDs are predictable and shouldn't be used outside the local system.
* They protect against connection mixups on concurrent systems (i.e. when saving
* client data for "broadcasting" or when an old client task is preparing a
* response in the background while a disconnection and a new connection occur on
* the same `fd`).
*/
intptr_t sock_accept(intptr_t srv_uuid);

/**
 * `sock_connect` is similar to `sock_accept` but should be used to initiate a
 * client connection to the address requested.
 *
 * If `port` is provided (`address` can be NULL), a TCP/IP socket will be
 * opened. Otherwise, `address` is required and a Unix Socket will be used
 * (remember Unix Sockets have name length restrictions).
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 *
 * Socket UUIDs are predictable and shouldn't be used outside the local system.
 * They protect against connection mixups on concurrent systems (i.e. when
 * saving client data for "broadcasting" or when an old client task is preparing
 * a response in the background while a disconnection and a new connection occur
 * on the same `fd`).
 *
 * When using `evio`, remember to call `int evio_add(sock_fd2uuid(uuid),
 * (void*)uuid);` to listen for events.
 *
 * NOTICE:
 *
 * This function is non-blocking, meaning that the connection probably wasn't
 * established by the time the function returns (this prevents the function from
 * hanging while waiting for a network timeout).
 *
 * Use select, poll, `evio` or other solutions to review the connection state
 * before attempting to write to the socket.
 */
intptr_t sock_connect(char *address, char *port);

/**
* `sock_open` takes an existing file descriptor `fd` and initializes it's status
* as open and available for `sock_API` calls, returning a valid UUID.
*
* This will reinitialize the data (user buffer etc') for the file descriptor
* provided, calling the `sock_on_close` callback if the `fd` was previously
* marked as used.

* When using `evio`, remember to call `int evio_add(sock_fd2uuid(uuid),
* (void*)uuid);` to listen for events.
*
* Returns -1 on error. Returns a valid socket (non-random) UUID.
*
* Socket UUIDs are predictable and shouldn't be used outside the local system.
* They protect against connection mixups on concurrent systems (i.e. when saving
* client data for "broadcasting" or when an old client task is preparing a
* response in the background while a disconnection and a new connection occur on
* the same `fd`).
*/
intptr_t sock_open(int fd);

/**
 * Returns 1 if the uuid refers to a valid and open, socket.
 *
 * Returns 0 if not.
 */
int sock_isvalid(intptr_t uuid);

/** The return type for the `sock_peer_addr` function. */
typedef struct {
  uint32_t addrlen;
  struct sockaddr *addr;
} sock_peer_addr_s;
/**
 * Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`addr == NULL`).
 * The information is only available when the socket was accepted using
 * `sock_accept` or opened using `sock_connect`.
 */
sock_peer_addr_s sock_peer_addr(intptr_t uuid);

/**
* `sock_fd2uuid` takes an existing file decriptor `fd` and returns it's active
* `uuid`.

* If the file descriptor is marked as closed (wasn't opened / registered with
* `libsock`) the function returns -1;
*
* If the file descriptor was closed remotely (or not using `libsock`), a false
* positive will be possible. This is not an issue, since the use of an invalid
fd
* will result in the registry being updated and the fd being closed.
*
* Returns -1 on error. Returns a valid socket (non-random) UUID.
*/
intptr_t sock_fd2uuid(int fd);

/**
 * OVERRIDABLE:
 *
 * "Touches" a socket connection.
 *
 * This is a place holder for an optional callback for systems that apply
 * timeout reviews.
 *
 * `sock` supplies a default implementation (that does nothing) is cases where a
 * callback wasn't defined.
 */
void sock_touch(intptr_t uuid);

/**
 * OVERRIDABLE:.
 *
 * This is a place holder for an optional callback for when the socket is closed
 * locally.
 *
 * Notice that calling `sock_close()` won't close the socket before all the data
 * in the buffer is sent. This function will be called only one the connection
 * is actually closed.
 *
 * `sock` supplies a default implementation (that does nothing) is cases where a
 * callback wasn't defined.
 */
void sock_on_close(intptr_t uuid);

/**
 * `sock_read` attempts to read up to count bytes from the socket into the
 * buffer starting at buf.
 *
 * `sock_read`'s return values are wildly different then the native return
 * values and they aim at making far simpler sense.
 *
 * `sock_read` returns the number of bytes read (0 is a valid return value which
 * simply means that no bytes were read from the buffer).
 *
 * On a connection error (NOT EAGAIN or EWOULDBLOCK), signal interrupt, or when
 * the connection was closed, `sock_read` returns -1.
 *
 * The value 0 is the valid value indicating no data was read.
 *
 * Data might be available in the kernel's buffer while it is not available to
 * be read using `sock_read` (i.e., when using a transport layer, such as TLS).
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
    /**
     * This deallocation callback will be called when the packet is finished
     * with the buffer if the `move` flags is set.
     *
     * If no deallocation callback is `free` will be used.
     *
     * Note: `sock` library functions MUST NEVER be called by a callback, or a
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
  };
  /** The length (size) of the buffer, or the amount of data to be sent from the
   * file descriptor.
   */
  uintptr_t length;
  /** Starting point offset from the buffer or file descriptor's beginning. */
  intptr_t offset;
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
#define SOCK_CLOSE_NOOP ((void (*)(intptr_t))SOCK_DEALLOC_NOOP)

/**
 * `sock_write2_fn` is the actual function behind the macro `sock_write2`.
 */
ssize_t sock_write2_fn(sock_write_info_s options);

/**
 * `sock_write2` is similar to `sock_write`, except special properties can be
 * set.
 *
 * On error, -1 will be returned. Otherwise returns 0. All the bytes are
 * transferred to the socket's user level buffer.
 */
#define sock_write2(...) sock_write2_fn((sock_write_info_s){__VA_ARGS__})
/**
 * `sock_write` copies `legnth` data from the buffer and schedules the data to
 * be sent over the socket.
 *
 * The data isn't necessarily written to the socket and multiple calls to
 * `sock_flush` might be required before all the data is actually sent.
 *
 * On error, -1 will be returned. Otherwise returns 0. All the bytes are
 * transferred to the socket's user level buffer.
 *
 * Returns the same values as `sock_write2`.
 */
// ssize_t sock_write(uintptr_t uuid, void *buffer, size_t legnth);
UNUSED_FUNC static inline ssize_t
sock_write(const intptr_t uuid, const void *buffer, const size_t length) {
  if (!length || !buffer)
    return 0;
  void *cpy = malloc(length);
  if (!cpy)
    return -1;
  memcpy(cpy, buffer, length);
  return sock_write2(.uuid = uuid, .buffer = cpy, .length = length);
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
 * `offset` dictates the starting point for te data to be sent and length sets
 * the maximum amount of data to be sent.
 *
 * Returns -1 and closes the file on error. Returns 0 on success.
 */
UNUSED_FUNC static inline ssize_t
sock_sendfile(intptr_t uuid, intptr_t source_fd, off_t offset, size_t length) {
  return sock_write2(.uuid = uuid, .buffer = (void *)(source_fd),
                     .length = length, .is_fd = 1, .offset = offset);
}

/**
 * `sock_close` marks the connection for disconnection once all the data was
 * sent. The actual disconnection will be managed by the `sock_flush` function.
 *
 * `sock_flash` will automatically be called.
 */
void sock_close(intptr_t uuid);
/**
 * `sock_force_close` closes the connection immediately, without adhering to any
 * protocol restrictions and without sending any remaining data in the
 * connection buffer.
 */
void sock_force_close(intptr_t uuid);

/* *****************************************************************************
Direct user level buffer API.
*/

/**
 * `sock_flush` writes the data in the internal buffer to the underlying file
 * descriptor and closes the underlying fd once it's marked for closure (and all
 * the data was sent).
 *
 * Return value: 0 will be returned on success and -1 will be returned on an
 * error or when the connection is closed.
 */
ssize_t sock_flush(intptr_t uuid);
/**
 * `sock_flush_strong` performs the same action as `sock_flush` but returns only
 * after all the data was sent. This is a "busy" wait, polling isn't performed.
 */
void sock_flush_strong(intptr_t uuid);
/**
 * Calls `sock_flush` for each file descriptor that's buffer isn't empty.
 */
void sock_flush_all(void);

/**
 * Returns TRUE (non 0) if there is data waiting to be written to the socket in
 * the user-land buffer.
 */
int sock_has_pending(intptr_t uuid);

/* *****************************************************************************
TLC - Transport Layer Callback.
*/

/**
 * The following struct is used for setting a the read/write hooks that will
 * replace the default system calls to `recv` and `write`.
 *
 * Note: `sock` library functions MUST NEVER be called by any callback, or a
 * deadlock might occur.
 */
typedef struct sock_rw_hook_s {
  /**
   * Implement reading from a file descriptor. Should behave like the file
   * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.
   *
   * Note: `sock` library functions MUST NEVER be called by any callback, or a
   * deadlock might occur.
   */
  ssize_t (*read)(intptr_t uuid, void *udata, void *buf, size_t count);
  /**
   * Implement writing to a file descriptor. Should behave like the file system
   * `write` call.
   *
   * Note: `sock` library functions MUST NEVER be called by any callback, or a
   * deadlock might occur.
   */
  ssize_t (*write)(intptr_t uuid, void *udata, const void *buf, size_t count);
  /**
   * When implemented, this function will be called to flush any data remaining
   * in the internal buffer.
   *
   * The function should return the number of bytes remaining in the internal
   * buffer (0 is a valid response) on -1 (on error).
   *
   * It is important thet the `flush` function write to the underlying fd until
   * the writing operation returns -1 with EWOULDBLOCK or all the data was
   * written.
   *
   * Note: `sock` library functions MUST NEVER be called by any callback, or a
   * deadlock might occur.
   */
  ssize_t (*flush)(intptr_t uuid, void *udata);
  /**
   * The `on_close` callback is called when the socket is closed, allowing for
   * dynamic sock_rw_hook_s memory management.
   *
   * The `on_close` callback should manage is own thread safety mechanism, if
   * required.
   *
   * Note: `sock` library functions MUST NEVER be called by any callback, or a
   * deadlock might occur.
   * */
  void (*on_close)(intptr_t uuid, struct sock_rw_hook_s *rw_hook, void *udata);
} sock_rw_hook_s;

/* *****************************************************************************
RW hooks implementation
*/

/** Sets a socket hook state (a pointer to the struct). */
int sock_rw_hook_set(intptr_t uuid, sock_rw_hook_s *rw_hooks, void *udata);

/** Gets a socket hook state (a pointer to the struct). */
struct sock_rw_hook_s *sock_rw_hook_get(intptr_t uuid);

/** Returns the socket's udata associated with the read/write hook. */
void *sock_rw_udata(intptr_t uuid);

/** The default Read/Write hooks used for system Read/Write (udata == NULL). */
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
