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
  fio_uuid_s *uuid = uuid_;
  if (fio_trylock(&uuid->lock))
    goto reschedule;

  return;
reschedule:
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_shutdown,
                 .udata1 = uuid,
                 .udata2 = udata);
}

static void fio_ev_on_ready_user(void *uuid_, void *udata) {
  fio_uuid_s *uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN) || !uuid->state) {
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
  fio_uuid_s *uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    char buf_mem[FIO_SOCKET_BUFFER_PER_WRITE];
    char *buf = buf_mem;
    size_t len = FIO_SOCKET_BUFFER_PER_WRITE;
    fio_stream_read(&uuid->stream, &buf, &len);
    if (len) {
      ssize_t r = fio_sock_read(uuid->fd, buf, len);
      if (r > 0) {
        fio_stream_advance(&uuid->stream, len);
      } else if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
        uuid->state = FIO_UUID_CLOSED;
        goto finish;
      }
    }
    if (!fio_stream_any(&uuid->stream)) {
      fio_queue_push_urgent(fio_queue_select(uuid->protocol->reserved.flags),
                            .fn = fio_ev_on_ready_user,
                            .udata1 = fio_uuid_dup(uuid),
                            .udata2 = udata);
      fio_user_thread_wake();
    }
  }
finish:
  fio_uuid_free(uuid);
}

static void fio_ev_on_data(void *uuid_, void *udata) {
  fio_uuid_s *uuid = uuid_;
  if (fio_trylock(&uuid->lock))
    goto reschedule;
  uuid->protocol->on_data(uuid, uuid->udata);
  fio_unlock(&uuid->lock);
  fio_uuid_free(uuid);
  return;
reschedule:
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_data,
                 .udata1 = fio_uuid_dup(uuid),
                 .udata2 = udata);
}

static void fio_ev_on_timeout(void *uuid_, void *udata) {
  fio_uuid_s *uuid = uuid_;
  uuid->protocol->on_timeout(uuid, uuid->udata);
  fio_uuid_free(uuid);
  return;
  (void)udata;
}
