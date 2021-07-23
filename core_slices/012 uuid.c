/* *****************************************************************************
UUID related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Points to a function that keeps the connection alive. */
void (*FIO_PING_ETERNAL)(fio_uuid_s *, void *) = mock_ping_eternal;

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
                          fio_tls_s *tls) {
  if (!protocol || fd == -1)
    return NULL;
  fio_uuid_s *uuid = fio_uuid_new2();
  uuid->udata = udata;
  uuid->tls = tls;
  uuid->fd = fd;
  fio_sock_set_non_block(fd); /* never accept a blocking socket */
  fio_protocol_set(uuid, protocol);
  FIO_LOG_DEBUG("uuid %p attached with fd %d", (void *)uuid, fd);
  return uuid;
}

FIO_SFUNC void fio_protocol_set___task(void *uuid_, void *old_protocol_) {
  fio_uuid_s *uuid = uuid_;
  fio_protocol_s *old = old_protocol_;
  fio___touch(uuid, NULL);
  if (old && FIO_LIST_IS_EMPTY(&old->reserved.protocols))
    FIO_LIST_REMOVE(&old->reserved.protocols);
  fio_uuid_free2(uuid);
}
/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_uuid_s *uuid, fio_protocol_s *protocol) {
  if (!protocol || !uuid)
    return;
  fio_protocol_validate(protocol);
  fio_protocol_s *old = uuid->protocol;
  uuid->protocol = protocol;
  fio_queue_push(&tasks_io_core,
                 fio_protocol_set___task,
                 fio_uuid_dup(uuid),
                 old);
  fio_uuid_monitor_add(uuid);
}

/** Associates a new `udata` pointer with the UUID, returning the old `udata` */
void *fio_udata_set(fio_uuid_s *uuid, void *udata) {
  void *old = uuid->udata;
  uuid->udata = udata;
  return old;
}

/** Returns the `udata` pointer associated with the UUID. */
void *fio_udata_get(fio_uuid_s *uuid) { return uuid->udata; }

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_uuid_s *uuid, void *buf, size_t len) {
  ssize_t r = fio_sock_read(uuid->fd, buf, len);
  if (r > 0) {
    fio___touch(uuid, NULL);
    return r;
  }
  if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 0;
  fio_uuid_close(uuid);
  return 0;
}

static void fio_write2___task(void *uuid_, void *packet) {
  fio_uuid_s *uuid = uuid_;
  if (!(uuid->state & FIO_UUID_OPEN))
    goto error;
  fio_stream_add(&uuid->stream, packet);
  fio_ev_on_ready(uuid, uuid->udata);
  return;
error:
  fio_stream_pack_free(packet);
  fio_uuid_free2(uuid);
}
void fio_write2___(void); /* Sublime Text marker*/
/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2 FIO_NOOP(fio_uuid_s *uuid, fio_write_args_s args) {
  fio_stream_packet_s *packet = NULL;
  if (args.buf) {
    packet = fio_stream_pack_data(args.buf,
                                  args.len,
                                  args.offset,
                                  args.copy,
                                  args.dealloc);
  } else if (args.fd != -1) {
    packet = fio_stream_pack_fd(args.fd, args.len, args.offset, args.copy);
  }
  if (!packet)
    goto error;
  fio_queue_push(&tasks_io_core,
                 .fn = fio_write2___task,
                 .udata1 = fio_uuid_dup(uuid),
                 .udata2 = packet);
  fio_io_thread_wake();
  return;
error:
  FIO_LOG_ERROR("couldn't create user-packet for uuid %p", (void *)uuid);
}

/** Marks the UUID for closure as soon as the pending data was sent. */
void fio_close(fio_uuid_s *uuid) {
  if ((uuid->state & FIO_UUID_OPEN) && !(uuid->state & FIO_UUID_CLOSED_BIT)) {
    uuid->state |= FIO_UUID_CLOSED_BIT;
    fio_uuid_monitor_add_write(uuid);
  }
}

/** Marks the UUID for immediate closure. */
void fio_close_now(fio_uuid_s *uuid) { fio_uuid_close(uuid); }

/** Returns true if a UUID is busy with an ongoing task. */
int fio_uuid_is_busy(fio_uuid_s *uuid) { return (int)uuid->lock; }

/* Resets a socket's timeout counter. */
void fio_touch(fio_uuid_s *uuid) {
  static fio_uuid_s *last_uuid;
  static uint64_t last_call;
  const uint64_t this_call =
      ((uint64_t)fio_data.tick.tv_sec << 21) + fio_data.tick.tv_nsec;
  if (last_uuid == uuid && last_call == this_call)
    return;
  last_uuid = uuid;
  last_call = this_call;
  fio_queue_push(&tasks_io_core,
                 .fn = fio___touch,
                 .udata1 = fio_uuid_dup2(uuid),
                 .udata2 = uuid);
}
