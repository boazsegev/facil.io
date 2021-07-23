/* *****************************************************************************
Internal helpers
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void fio_ev_on_shutdown(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if (fio_trylock(&uuid->lock))
    goto reschedule;
  if (!uuid->protocol->on_shutdown(uuid, uuid->udata))
    uuid->state = FIO_UUID_CLOSING;
  fio_unlock(&uuid->lock);
  fio_uuid_free(uuid);
  return;

reschedule:
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_shutdown,
                 .udata1 = uuid,
                 .udata2 = udata);
}

static void fio_ev_on_ready_user(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    if (fio_trylock(&uuid->lock))
      goto reschedule;
    uuid->protocol->on_ready(uuid, uuid->udata);
    fio_unlock(&uuid->lock);
  }

  fio_uuid_free(uuid);
  return;

reschedule:
  fio_queue_push_urgent(fio_queue_select(uuid->protocol->reserved.flags),
                        .fn = fio_ev_on_ready_user,
                        .udata1 = uuid,
                        .udata2 = udata);
}

static void fio_ev_on_ready(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    char buf_mem[FIO_SOCKET_BUFFER_PER_WRITE];
    size_t total = 0;
    for (;;) {
      size_t len = FIO_SOCKET_BUFFER_PER_WRITE;
      char *buf = buf_mem;
      fio_stream_read(&uuid->stream, &buf, &len);
      if (!len)
        break;
      ssize_t r = fio_sock_write(uuid->fd, buf, len);
      if (r > 0) {
        total += r;
        fio_stream_advance(&uuid->stream, len);
      } else if (r == 0 ||
                 (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)) {
        fio_uuid_close(uuid);
        goto finish;
      } else {
        break;
      }
    }
    if (total)
      fio___touch(uuid, NULL);
    if (!fio_stream_any(&uuid->stream)) {
      if ((uuid->state & FIO_UUID_CLOSED_BIT)) {
        fio_uuid_close(uuid); /* free the UUID again to close it..? */
      } else {
        fio_queue_push_urgent(fio_queue_select(uuid->protocol->reserved.flags),
                              .fn = fio_ev_on_ready_user,
                              .udata1 = fio_uuid_dup(uuid),
                              .udata2 = udata);
      }
    } else {
      fio_uuid_monitor_add_write(uuid);
    }
  }
finish:
  fio_uuid_free(uuid);
}

static void fio_ev_on_data(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    if (fio_trylock(&uuid->lock))
      goto reschedule;
    uuid->protocol->on_data(uuid, uuid->udata);
    fio_unlock(&uuid->lock);
    if (!(uuid->state & FIO_UUID_CLOSED)) {
      /* this also tests for the suspended flag (0x02) */
      fio_uuid_monitor_add_read(uuid);
    }
  } else {
    fio_uuid_monitor_add_write(uuid);
    FIO_LOG_DEBUG("skipping on_data callback for uuid %p (closed?)", uuid_);
  }
  fio_uuid_free(uuid);
  return;

reschedule:
  FIO_LOG_DEBUG("rescheduling on_data for uuid %p", uuid_);
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_data,
                 .udata1 = uuid,
                 .udata2 = udata);
}

static void fio_ev_on_timeout(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    uuid->protocol->on_timeout(uuid, uuid->udata);
  }
  fio_uuid_free(uuid);
  return;
  (void)udata;
}
