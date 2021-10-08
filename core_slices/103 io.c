/* *****************************************************************************
IO data types
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO data types
***************************************************************************** */

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

fio_protocol_s FIO_PROTOCOL_HIJACK;

typedef enum {
  FIO_IO_OPEN = 1,      /* 0b00001 */
  FIO_IO_SUSPENDED = 2, /* 0b00010 */
  FIO_IO_THROTTLED = 4, /* 0b00100 */
  FIO_IO_CLOSING = 8,   /* 0b01000 */
  FIO_IO_CLOSED = 14,   /* 0b01110 */
} fio_state_e;

struct fio_s {
  /* user udata - MUST be first */
  void *udata;
  /* fd protocol */
  fio_protocol_s *protocol;
  /** RW hooks. */
  fio_tls_s *tls;
  /* timeout review linked list */
  FIO_LIST_NODE timeouts;
  /* current data to be send */
  fio_stream_s stream;
  /* env */
  env_safe_s env;
  /* timer handler */
  int64_t active;
  /* socket */
  int fd;
  /** Task lock */
  fio_lock_i lock;
  /** Connection state */
  volatile uint8_t state;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
};

FIO_IFUNC fio_s *fio_dup2(fio_s *io);
FIO_IFUNC void fio_free2(fio_s *io);

FIO_SFUNC void fio___deferred_on_close(void *fn, void *udata) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn};
  u.fn(udata);
  MARK_FUNC();
}

FIO_SFUNC void fio___init_task(void *io, void *ignr_) {
  fio_set_valid(io);
  fio_free2(io);
  (void)ignr_;
}
/* IO object constructor (MUST be thread safe). */
FIO_SFUNC void fio___init(fio_s *io) {
  *io = (fio_s){
      .protocol = &FIO_PROTOCOL_HIJACK,
      .state = FIO_IO_OPEN,
      .stream = FIO_STREAM_INIT(io->stream),
      .timeouts = FIO_LIST_INIT(io->timeouts),
      .env = ENV_SAFE_INIT,
      .fd = -1,
      .lock = FIO_LOCK_INIT,
  };
  fio_queue_push(FIO_QUEUE_SYSTEM, fio___init_task, fio_dup2(io));
}

/* IO object destructor (NOT thread safe). */
FIO_SFUNC void fio___destroy(fio_s *io) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.fn = io->protocol->on_close};

  fio_set_invalid(io);
  env_safe_destroy(&io->env);
  fio_stream_destroy(&io->stream);
  FIO_LIST_REMOVE(&io->timeouts);
  fio_monitor_forget(io->fd);
  fio_sock_close(io->fd);
  io->protocol->tls_functions.free(io->tls);
  if (io->protocol->reserved.ios.next == io->protocol->reserved.ios.prev) {
    FIO_LIST_REMOVE(&io->protocol->reserved.protocols);
  }
  fio_queue_push(FIO_QUEUE_IO(io), fio___deferred_on_close, u.p, io->udata);
  FIO_LOG_DEBUG2("(%d) IO %p (fd %d) freed.",
                 (int)fio_data.pid,
                 (void *)io,
                 (int)(io->fd));
}

#define FIO_REF_NAME         fio
#define FIO_REF_INIT(obj)    fio___init(&(obj))
#define FIO_REF_DESTROY(obj) fio___destroy(&(obj))
#include <fio-stl.h>

/* *****************************************************************************
Reference Counter Debugging
***************************************************************************** */
#if FIO_DEBUG_REF

FIO_IFUNC fio_s *fio_dup2_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 1);
  return fio_dup2(io);
}

FIO_IFUNC void fio_undup_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 2);
  fio_undup(io);
}

FIO_IFUNC void fio_free2_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 3);
  fio_free2(io);
}

#define fio_dup2(io)  fio_dup2_dbg(io, __func__)
#define fio_undup(io) fio_undup_dbg(io, __func__)
#define fio_free2(io) fio_free2_dbg(io, __func__)
#endif
/* *****************************************************************************
Managing reference counting (decrease only in the IO thread)
***************************************************************************** */

/* perform reference count decrease. */
FIO_SFUNC void fio_undup___task(void *io, void *ignr) {
  (void)ignr;
  if (!io)
    return;
  MARK_FUNC();
  /* don't trust users to manage reference counts? */
  if (fio_is_valid(io))
    fio_free2(io);
  else
    FIO_LOG_ERROR("user event attempted to double-free IO %p", io);
}

void fio_undup___(void); /* sublime text marker */
/** Route reference count decrease to the IO thread. */
void fio_undup FIO_NOOP(fio_s *io) {
  if (!io)
    return;
  MARK_FUNC();
  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = fio_undup___task, .udata1 = io);
}

/** Increase reference count. */
fio_s *fio_dup(fio_s *io) { return fio_dup2(io); }
#define fio_dup fio_dup2

/* *****************************************************************************
Closing the IO connection
***************************************************************************** */

/** common safe / unsafe implementation (unsafe = called from user thread). */
FIO_IFUNC void fio_close_now___task(void *io_, void *ignr_) {
  (void)ignr_;
  fio_s *io = io_;
  if (!fio_is_valid(io) || !(io->state & FIO_IO_OPEN)) {
    FIO_LOG_DDEBUG2("(%d) fio_close_now called for closed IO %p",
                    (int)fio_data.pid,
                    io);
    return;
  }
  fio_atomic_exchange(&io->state, FIO_IO_CLOSED);
  fio_monitor_forget(io->fd);
  fio_free2(io);
}

/* Public / safe IO immediate closure. */
void fio_close_now(fio_s *io) {
  fio_atomic_or(&io->state, FIO_IO_CLOSING);
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_close_now___task, io);
  MARK_FUNC();
}

/** Unsafe IO immediate closure (callable only from IO threads). */
FIO_IFUNC void fio_close_now_unsafe(fio_s *io) {
  MARK_FUNC();
  fio_close_now___task(io, NULL);
}

/* *****************************************************************************
Timeout Marking
***************************************************************************** */

/** "touches" a socket, should only be called from the IO thread. */
FIO_SFUNC void fio_touch___unsafe(void *io_, void *should_free) {
  fio_s *io = io_;
  int64_t now;
  /* brunching vs risking a cache miss... */
  if (!(io->state & FIO_IO_OPEN)) {
    goto finish;
  }
  now = fio_data.tick;
  if (io->active == now) /* skip a double-touch for the same IO cycle. */
    goto finish;
  io->active = now;
  /* (re)insert the IO to the end of the timeout linked list in the protocol. */
  FIO_LIST_REMOVE(&io->timeouts);
  FIO_LIST_PUSH(&io->protocol->reserved.ios, &io->timeouts);
  if (FIO_LIST_IS_EMPTY(&io->protocol->reserved.protocols)) {
    /* insert the protocol object to the monitored protocol list. */
    FIO_LIST_REMOVE(&io->protocol->reserved.protocols);
    FIO_LIST_PUSH(&fio_data.protocols, &io->protocol->reserved.protocols);
  }
finish:
  if (should_free)
    fio_free2(should_free);
}

/* Resets a socket's timeout counter. */
void fio_touch(fio_s *io) {
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_touch___unsafe,
                 .udata1 = io,
                 .udata2 = fio_dup(io));
}

/* *****************************************************************************
Timeout Review
***************************************************************************** */

/** Schedules the timeout event for any timed out IO object */
FIO_SFUNC int fio___review_timeouts(void) {
  int c = 0;
  static time_t last_to_review = 0;
  /* test timeouts at whole second intervals */
  if (last_to_review + 1000 > fio_data.tick)
    return c;
  last_to_review = fio_data.tick;
  const int64_t now_milli = fio_data.tick;

  // FIO_LOG_DDEBUG2("performing timeout review.");

  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_ASSERT_DEBUG(pr->reserved.flags, "protocol object flags unmarked?!");
    if (!pr->timeout || pr->timeout > FIO_IO_TIMEOUT_MAX)
      pr->timeout = FIO_IO_TIMEOUT_MAX;
    int64_t limit = now_milli - ((int64_t)pr->timeout * 1000);
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "io protocol ownership error");
      if (io->active >= limit)
        break;
      FIO_LOG_DDEBUG2("(%d) scheduling timeout for %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      fio_queue_push(FIO_QUEUE_IO(io),
                     .fn = fio_ev_on_timeout,
                     .udata1 = fio_dup(io),
                     .udata2 = NULL);
      ++c;
    }
  }
  return c;
}

/* *****************************************************************************
Testing for any open IO objects
***************************************************************************** */

FIO_IFUNC void fio___close_all_io_in_protocol(fio_protocol_s *pr) {
  FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
    fio_close_now_unsafe(io);
  }
}

FIO_SFUNC int fio___is_waiting_on_io(void) {
  int c = 0;
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      c = 1;
      FIO_LOG_DDEBUG2("(%d) waiting for IO %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      if ((io->state & FIO_IO_OPEN))
        fio_close(io);
      else
        fio_queue_push(FIO_QUEUE_SYSTEM, fio_ev_on_ready, fio_dup(io));
    }
  }
  return c;
}

/* *****************************************************************************
Testing for any open IO objects
***************************************************************************** */

FIO_SFUNC void fio___close_all_io(void) {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    fio___close_all_io_in_protocol(pr);
  }
}

/* *****************************************************************************
IO object protocol update
***************************************************************************** */

FIO_SFUNC void fio_protocol_set___attached_task(void *io_, void *ignr_) {
  fio_s *io = io_;
  if (fio_trylock(&io->lock))
    goto reschedule;
  io->protocol->on_attach(io);
  fio_unlock(&io->lock);
  // FIO_LOG_DDEBUG2("on_attach callback returned, monitoring IO %p", io_);
  fio_monitor_all(io);
  fio_free2(io);
  return;
reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
                 fio_protocol_set___attached_task,
                 io_,
                 ignr_);
}

/* completes an update for an IO object's protocol */
FIO_SFUNC void fio_protocol_set___task(void *io_, void *old_protocol_) {
  fio_s *io = io_;
  fio_protocol_s *p = io->protocol;
  fio_protocol_s *old = old_protocol_;
  FIO_ASSERT_DEBUG(p != old, "protocol switch with identical protocols!");
  fio_touch___unsafe(io, io);
  io->tls = p->tls_functions.dup(io->tls, io);
  if (old && FIO_LIST_IS_EMPTY(&old->reserved.ios)) {
    old->tls_functions.free(io->tls);
    FIO_LIST_REMOVE(&old->reserved.protocols);
  }
  (void)p; /* if not DEBUG */
}

/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_s *io, fio_protocol_s *protocol) {
  if (!io || io->protocol == protocol)
    return;
  if (!protocol)
    protocol = &FIO_PROTOCOL_HIJACK;
  fio_protocol_validate(protocol);
  fio_protocol_s *old = io->protocol;
  io->protocol = protocol;
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_protocol_set___task,
                 .udata1 = fio_dup(io),
                 .udata2 = old);
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_protocol_set___attached_task,
                 .udata1 = fio_dup(io));
}

/* *****************************************************************************
Copy address to string
***************************************************************************** */

/** Caches an attached IO object's peer address. */
FIO_SFUNC void fio___addr_cpy(fio_s *io) {
  /* TODO: Fix Me */
  struct sockaddr addrinfo;
  // size_t len = sizeof(addrinfo);
  const char *result =
      inet_ntop(addrinfo.sa_family,
                addrinfo.sa_family == AF_INET
                    ? (void *)&(((struct sockaddr_in *)&addrinfo)->sin_addr)
                    : (void *)&(((struct sockaddr_in6 *)&addrinfo)->sin6_addr),
                (char *)io->addr,
                sizeof(io->addr));
  if (result) {
    io->addr_len = strlen((char *)io->addr);
  } else {
    io->addr_len = 0;
    io->addr[0] = 0;
  }
}

/* *****************************************************************************
IO object attachment
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
fio_s *fio_attach_fd(int fd,
                     fio_protocol_s *protocol,
                     void *udata,
                     fio_tls_s *tls) {
  if (!protocol || fd == -1)
    goto error;
  fio_s *io = fio_new2();
  FIO_ASSERT_ALLOC(io);
  io->fd = fd;
  io->tls = tls;
  io->udata = udata;
  fio_sock_set_non_block(fd); /* never accept a blocking socket */
  /* fio___addr_cpy(io); // ??? */
  fio_protocol_set(io, protocol);
  FIO_LOG_DEBUG2("(%d) attaching fd %d to IO %p",
                 (int)fio_data.pid,
                 fd,
                 (void *)io);
  MARK_FUNC();
  return io;
error:
  if (protocol && protocol->on_close)
    protocol->on_close(udata);
  return NULL;
}

/* *****************************************************************************
Misc
***************************************************************************** */

/** Returns true if an IO is busy with an ongoing task. */
int fio_is_busy(fio_s *io) { return (int)io->lock; }

/** Suspends future "on_data" events for the IO. */
void fio_suspend(fio_s *io) { fio_atomic_or(&io->state, FIO_IO_SUSPENDED); }

/* *****************************************************************************
Monitoring IO
***************************************************************************** */

FIO_IFUNC void fio_monitor_read(fio_s *io) {
#if 0
  short e = fio_sock_wait_io(io->fd, POLLIN, 0);
  if (e != -1 && (POLLIN & e)) {
    fio_queue_push(FIO_QUEUE_IO(io),
                   fio_ev_on_data,
                   io,
                   NULL);
  }
  if (e)
    return;
#endif
  fio_monitor_monitor(io->fd, io, POLLIN);
}
FIO_IFUNC void fio_monitor_write(fio_s *io) {
#if 0
  short e = fio_sock_wait_io(io->fd, POLLOUT, 0);
  if (e != -1 && (POLLOUT & e)) {
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   fio_ev_on_ready,
                   io,
                   NULL);
  }
  if (e)
    return;
#endif
  fio_monitor_monitor(io->fd, io, POLLOUT);
}
FIO_IFUNC void fio_monitor_all(fio_s *io) {
  fio_monitor_monitor(io->fd, io, POLLIN | POLLOUT);
}

/* *****************************************************************************
Read
***************************************************************************** */

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_s *io, void *buf, size_t len) {
  ssize_t r = io->protocol->tls_functions.read(io->fd, buf, len, io->tls);
  if (r > 0) {
    fio_touch(io);
    return r;
  }
  if (!len | ((r == -1) &
              ((errno == EAGAIN) | (errno == EWOULDBLOCK) | (errno == EINTR))))
    return 0;
  FIO_LOG_DDEBUG2("(%d) fio_read error, closing IO %p (fd %d)",
                  fio_data.pid,
                  (void *)io,
                  io->fd);
  fio_close(io);
  return 0;
}

/* *****************************************************************************
Write
***************************************************************************** */

static void fio_write2___task(void *io_, void *packet) {
  fio_s *io = io_;
  if (!(io->state & FIO_IO_OPEN))
    goto error;
  MARK_FUNC();
  fio_stream_add(&io->stream, packet);
  fio_ev_on_ready(io, io); /* will call fio_free2 */
  return;
error:
  fio_stream_pack_free(packet);
  fio_free2(io);
}

void fio_write2___(void); /* Sublime Text marker*/
/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2 FIO_NOOP(fio_s *io, fio_write_args_s args) {
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
  if (!io || ((io->state & (FIO_IO_OPEN | FIO_IO_CLOSING)) ^ FIO_IO_OPEN))
    goto io_error;
  MARK_FUNC();
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_write2___task,
                 .udata1 = fio_dup(io),
                 .udata2 = packet);
#if FIO_ENGINE_POLL
  fio_io_thread_wake();
#else
  fio_monitor_write(io);
#endif
  return;
error:
  FIO_LOG_ERROR("couldn't create user-packet for IO %p", (void *)io);
  if (args.dealloc)
    args.dealloc(args.buf);
  return;
io_error:
  fio_stream_pack_free(packet);
  if (!io)
    FIO_LOG_ERROR("Invalid IO (NULL) for user-packet");
  return;
}

/** Marks the IO for closure as soon as the pending data was sent. */
void fio_close(fio_s *io) {
  if (io && (io->state & FIO_IO_OPEN) &&
      !(fio_atomic_or(&io->state, FIO_IO_CLOSING) & FIO_IO_CLOSING)) {
    FIO_LOG_DDEBUG2("(%d) scheduling IO %p (fd %d) for closure",
                    fio_data.pid,
                    (void *)io,
                    io->fd);
    fio_monitor_forget(io->fd);
    fio_queue_push(FIO_QUEUE_SYSTEM, fio_ev_on_ready, fio_dup(io));
    MARK_FUNC();
  }
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio___after_fork___io(void) {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "IO protocol ownership error");
      FIO_LOG_DDEBUG2("(%d) cleanup for IO %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      fio_close_now_unsafe(io);
    }
    FIO_LIST_REMOVE(&pr->reserved.protocols);
  }
}

static void fio___start_shutdown(void) {
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      fio_queue_push(FIO_QUEUE_IO(io),
                     .fn = fio_ev_on_shutdown,
                     .udata1 = fio_dup(io),
                     .udata2 = NULL);
    }
  }
}
