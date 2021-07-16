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
  /** The timeout value for all connections using this protocol. */
  uint32_t timeout;
};

/** A ping function that does nothing except keeping the connection alive. */
void FIO_PING_ETERNAL(fio_uuid_s *uuid, void *udata);

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
