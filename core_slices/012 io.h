/* *****************************************************************************
IO related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/**************************************************************************/ /**
The Protocol
============

The Protocol struct defines the callbacks used for a family of connections and
sets their behavior. The Protocol struct is part of facil.io's core design.

All the callbacks receive a unique connection ID that can be converted to the
original file descriptor when in need (which shouldn't really happen).

This allows facil.io to prevent old connection handles from sending data to new
connections after a file descriptor is "recycled" by the OS.
*/
struct fio_protocol_s {
  /**
   * Reserved / private data - used by facil.io internally.
   * MUST be initialized to zero
   */
  struct {
    /* A linked list of currently attached IOs - do NOT alter. */
    FIO_LIST_HEAD ios;
    /* A linked list of other protocols used by IO core - do NOT alter. */
    FIO_LIST_NODE protocols;
    /* internal flags - do NOT alter after initial initialization to zero. */
    uintptr_t flags;
  } reserved;
  /**
   * Called when an IO was attached. Locks the IO's task lock.
   *
   * Note: this is scheduled when setting the protocol, but depending on race
   * conditions, the previous protocol, and the socket's state, it may run after
   * the `on_ready` or `on_data` are called.
   * */
  void (*on_attach)(fio_s *io);
  /** Called when a data is available. Locks the IO's task lock. */
  void (*on_data)(fio_s *io);
  /** called once all pending `fio_write` calls are finished. */
  void (*on_ready)(fio_s *io);
  /**
   * Called when the connection was closed, and all pending tasks are complete.
   */
  void (*on_close)(void *udata);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * Locks the IO's task lock.
   *
   * After the `on_shutdown` callback returns, the socket is marked for closure.
   *
   * Once the socket was marked for closure, facil.io will allow a limited
   * amount of time for data to be sent, after which the socket might be closed
   * even if the client did not consume all buffered data.
   */
  void (*on_shutdown)(fio_s *io);
  /** Called when a connection's timeout was reached */
  void (*on_timeout)(fio_s *io);
  /**
   * Defines Transport Later callbacks that facil.io will treat as non-blocking
   * system calls
   * */
  struct fio_tls_functions_s {
    /** Called to perform a non-blocking `read`, same as the system call. */
    ssize_t (*read)(int fd, void *buf, size_t len, fio_tls_s *tls);
    /** Called to perform a non-blocking `write`, same as the system call. */
    ssize_t (*write)(int fd, const void *buf, size_t len, fio_tls_s *tls);
    /** Sends any unsent internal data. Returns 0 only if all data was sent. */
    int (*flush)(int fd, fio_tls_s *tls);
    /** Increases a fio_tls_s object's reference count, or creates a new one. */
    fio_tls_s *(*dup)(fio_tls_s *tls, fio_s *io);
    /** Decreases a fio_tls_s object's reference count, or frees the object. */
    void (*free)(fio_tls_s *tls);
  } tls_functions;
  /**
   * The timeout value in seconds for all connections using this protocol.
   *
   * Limited to FIO_IO_TIMEOUT_MAX seconds. The value 0 will be the same as the
   * timeout limit.
   */
  uint32_t timeout;
};

/** Points to a function that keeps the connection alive. */
extern void (*FIO_PING_ETERNAL)(fio_s *io);

/** Default TLS functions to be set by TLS library extensions. */
extern fio_tls_functions_s FIO_TLS_DEFAULT;

/* *****************************************************************************
Listening to Incoming Connections
***************************************************************************** */

/* Arguments for the fio_listen function */
struct fio_listen_args {
  /** The binding address in URL format. Defaults to: tcp://0.0.0.0:3000 */
  const char *url;
  /**
   * Called whenever a new connection is accepted.
   *
   * Should either call `fio_attach` or close the connection.
   */
  void (*on_open)(int fd, void *udata);
  /** Opaque user data. */
  void *udata;
  /**
   * Called when the server is done, usable for cleanup.
   *
   * This will be called separately for every process before exiting.
   */
  void (*on_finish)(void *udata);
  /* for internal use. */
  int reserved;
  /** Listen and connect using only the master process (non-worker). */
  uint8_t is_master_only;
};

/**
 * Sets up a network service on a listening socket.
 *
 * Returns 0 on success or -1 on error.
 *
 * See the `fio_listen` Macro for details.
 */
int fio_listen(struct fio_listen_args args);

/************************************************************************ */ /**
Listening to Incoming Connections
===

Listening to incoming connections is pretty straight forward:

* After a new connection is accepted, the `on_open` callback is called.

* `on_open` should "attach" the new connection using `fio_attach_fd`.

* Once listening is done, cleanup is performed in `on_finish`.

The following is an example echo server using facil.io:

```c

```
*/
#define fio_listen(url_, ...)                                                  \
  fio_listen((struct fio_listen_args){.url = (url_), __VA_ARGS__})

/* *****************************************************************************
Controlling the connection
***************************************************************************** */

/**
 * Attaches the socket in `fd` to the facio.io engine (reactor).
 *
 * * `fd` should point to a valid socket.
 *
 * * A `protocol` is always required. The system cannot manage a socket without
 *   a protocol.
 *
 * * `udata` is opaque user data and may be any value, including NULL.
 *
 * * `tls` is a context for Transport Layer Security and can be used to redirect
 *   read/write operations. NULL will result in unencrypted transmissions.
 *
 * Returns NULL on error. the `fio_s` pointer must NOT be used except within
 * proper callbacks.
 */
fio_s *fio_attach_fd(int fd,
                     fio_protocol_s *protocol,
                     void *udata,
                     fio_tls_s *tls);

/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_s *io, fio_protocol_s *protocol);

/**
 * Returns a pointer to the current protocol object.
 *
 * If `protocol` wasn't properly set, the pointer might be invalid.
 */
fio_protocol_s *fio_protocol_get(fio_s *io);

/** Associates a new `udata` pointer with the IO, returning the old `udata` */
FIO_IFUNC void *fio_udata_set(fio_s *io, void *udata) {
  void *old = ((void **)io)[0];
  ((void **)io)[0] = udata;
  return old;
}

/** Returns the `udata` pointer associated with the IO. */
FIO_IFUNC void *fio_udata_get(fio_s *io) { return ((void **)io)[0]; }

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_s *io, void *buf, size_t len);

typedef struct {
  /** The buffer with the data to send (if no file descriptor) */
  void *buf;
  /** The file descriptor to send (if no buffer) */
  intptr_t fd;
  /** The length of the data to be sent. On files, 0 = the whole file. */
  size_t len;
  /** The length of the data to be sent. On files, 0 = the whole file. */
  size_t offset;
  /**
   * If this is a buffer, the de-allocation function used to free it.
   *
   * If NULL, the buffer will NOT be de-allocated.
   */
  void (*dealloc)(void *);
  /** If non-zero, makes a copy of the buffer or keeps a file open. */
  uint8_t copy;
} fio_write_args_s;

/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2(fio_s *io, fio_write_args_s args);
#define fio_write2(io, ...) fio_write2(io, (fio_write_args_s){__VA_ARGS__})

/** Helper macro for a common fio_write2 (copies the buffer). */
#define fio_write(io, buf_, len_)                                              \
  fio_write2(io, .buf = (buf_), .len = (len_), .copy = 1)

/**
 * Sends data from a file as if it were a single atomic packet (sends up to
 * length bytes or until EOF is reached).
 *
 * Once the file was sent, the `source_fd` will be closed using `close`.
 *
 * The file will be buffered to the socket chunk by chunk, so that memory
 * consumption is capped.
 *
 * `offset` dictates the starting point for the data to be sent and length sets
 * the maximum amount of data to be sent.
 *
 * Returns -1 and closes the file on error. Returns 0 on success.
 */
#define fio_sendfile(io, source_fd, offset, bytes)                             \
  fio_write2((io),                                                             \
             .fd = (source_fd),                                                \
             .offset = (size_t)(offset),                                       \
             .len = (bytes))

/** Marks the IO for closure as soon as scheduled data was sent. */
void fio_close(fio_s *io);

/** Marks the IO for immediate closure. */
void fio_close_now(fio_s *io);

/**
 * Increases a IO's reference count, so it won't be automatically destroyed
 * when all tasks have completed.
 *
 * Use this function in order to use the IO outside of a scheduled task.
 */
fio_s *fio_dup(fio_s *io);

/**
 * Decreases a IO's reference count, so it could be automatically destroyed
 * when all other tasks have completed.
 *
 * Use this function once finished with a IO that was `dup`-ed.
 */
void fio_undup(fio_s *io);

/** Suspends future "on_data" events for the IO. */
void fio_suspend(fio_s *io);

/**
 * Returns true if a IO is busy with an ongoing task.
 *
 * This is only valid if the IO was copied to a deferred task or if called
 * within an on_timeout callback, allowing the user to speculate about the IO
 * task lock being engaged.
 *
 * Note that the information provided is speculative as the situation may change
 * between the moment the data is collected and the moment the function returns.
 */
int fio_is_busy(fio_s *io);

/* Resets a socket's timeout counter. */
void fio_touch(fio_s *io);

/**
 * Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`.len == 0`).
 */
fio_str_info_s fio_peer_addr(fio_s *io);

/**
 * Writes the local machine address (qualified host name) to the buffer.
 *
 * Returns the amount of data written (excluding the NUL byte).
 *
 * `limit` is the maximum number of bytes in the buffer, including the NUL byte.
 *
 * If the returned value == limit - 1, the result might have been truncated.
 *
 * If 0 is returned, an error might have occurred (see `errno`) and the contents
 * of `dest` is undefined.
 */
size_t fio_local_addr(char *dest, size_t limit);
