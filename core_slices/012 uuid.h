/* *****************************************************************************
UUID related operations (set protocol, read, write, etc')
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
    /* A linked list of currently attached UUIDs - do NOT alter. */
    FIO_LIST_HEAD uuids;
    /* A linked list of other protocols used by IO core - do NOT alter. */
    FIO_LIST_NODE protocols;
    /* internal flags - set to zero, do NOT alter. */
    uintptr_t flags;
  } reserved;
  /** Called when a data is available. Locks the UUID's task lock. */
  void (*on_data)(fio_uuid_s *uuid, void *udata);
  /** called once all pending `fio_write` calls are finished. */
  void (*on_ready)(fio_uuid_s *uuid, void *udata);
  /**
   * Called when the server is shutting down, immediately before closing the
   * connection.
   *
   * Locks the UUID's task lock.
   *
   * The `on_shutdown` callback should return 0 to close the socket or a
   * non-zero value to delay the closure of the socket.
   *
   * Once the socket was marked for closure, facil.io will allow a limited
   * amount of time for data to be sent, after which the socket will be closed
   * even if the client did not consume all buffered data.
   *
   * If the socket closure is delayed, it will might be abruptly terminated when
   * all other sockets have finished their graceful shutdown procedure.
   */
  uint8_t (*on_shutdown)(fio_uuid_s *uuid, void *udata);
  /**
   * Called when the connection was closed, and all pending tasks are complete.
   */
  void (*on_close)(void *udata);
  /** Called when a connection's timeout was reached */
  void (*on_timeout)(fio_uuid_s *uuid, void *udata);
  /**
   * The timeout value in seconds for all connections using this protocol.
   *
   * Limited to 600 seconds. The value 0 will be the same as the timeout limit.
   */
  uint32_t timeout;
};

/** Points to a function that keeps the connection alive. */
extern void (*FIO_PING_ETERNAL)(fio_uuid_s *uuid, void *udata);

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
 * Returns NULL on error.
 */
fio_uuid_s *fio_attach_fd(int fd,
                          fio_protocol_s *protocol,
                          void *udata,
                          fio_tls_s *tls);

/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_uuid_s *uuid, fio_protocol_s *protocol);

/** Associates a new `udata` pointer with the UUID, returning the old `udata` */
void *fio_udata_set(fio_uuid_s *uuid, void *udata);

/** Returns the `udata` pointer associated with the UUID. */
void *fio_udata_get(fio_uuid_s *uuid);

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_uuid_s *uuid, void *buf, size_t len);

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
void fio_write2(fio_uuid_s *uuid, fio_write_args_s args);
#define fio_write2(uuid, ...) fio_write2(uuid, (fio_write_args_s){__VA_ARGS__})

/** Helper macro for a common fio_write2 (copies the buffer). */
#define fio_write(uuid, buf_, len_)                                            \
  fio_write2(uuid, .buf = (buf_), .len = (len_), .copy = 1)

/** Marks the UUID for closure as soon as scheduled data was sent. */
void fio_close(fio_uuid_s *uuid);

/** Marks the UUID for immediate closure. */
void fio_close_now(fio_uuid_s *uuid);

/**
 * Increases a UUID's reference count, so it won't be automatically destroyed
 * when all tasks have completed.
 *
 * Use this function in order to use the UUID outside of a scheduled task.
 */
fio_uuid_s *fio_uuid_dup(fio_uuid_s *uuid);
/**
 * Decreases a UUID's reference count, so it could be automatically destroyed
 * when all other tasks have completed.
 *
 * Use this function in order to use the UUID outside of a scheduled task.
 */
void fio_uuid_free(fio_uuid_s *uuid);

/**
 * Returns true if a UUID is busy with an ongoing task.
 *
 * This is only valid if the UUID was copied to a deferred task or if called
 * within an on_timeout or on_ready callback, allowing the user to speculate
 * about the IO task lock being engaged.
 *
 * Note that the information provided is speculative as the situation may change
 * between the moment the data is collected and the moment the function returns.
 */
int fio_uuid_is_busy(fio_uuid_s *uuid);

/* Resets a socket's timeout counter. */
void fio_touch(fio_uuid_s *uuid);
