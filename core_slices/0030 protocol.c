/* *****************************************************************************












Connection Callback (Protocol) Management












***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* ************************************************************************** */

/* *****************************************************************************
Mock Protocol Callbacks and Service Funcions
***************************************************************************** */
static void mock_on_ev(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  (void)protocol;
}

static void mock_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  fio_suspend(uuid);
  (void)protocol;
}

static uint8_t mock_on_shutdown(intptr_t uuid, fio_protocol_s *protocol) {
  return 0;
  (void)protocol;
  (void)uuid;
}

/* mostly for the IPC connections */
FIO_SFUNC uint8_t mock_on_shutdown_eternal(intptr_t uuid,
                                           fio_protocol_s *protocol) {
  return 255;
  (void)protocol;
  (void)uuid;
}

static void mock_ping(intptr_t uuid, fio_protocol_s *protocol) {
  (void)protocol;
  if (!protocol_is_locked(protocol, FIO_PR_LOCK_TASK))
    fio_close(uuid);
}
static void mock_ping2(intptr_t uuid, fio_protocol_s *protocol) {
  (void)protocol;
  touchfd(fio_uuid2fd(uuid));
  if (uuid_data(uuid).timeout == 255)
    return;
  protocol->ping = mock_ping;
  uuid_data(uuid).timeout = 8;
  fio_close(uuid);
}

/* *****************************************************************************
Deferred event handlers - these tasks safely forward the events to the Protocol
***************************************************************************** */
static void deferred_on_close(void *uuid_, void *pr_) {
  fio_protocol_s *pr = pr_;
  if (pr->rsv)
    goto postpone;
  pr->on_close((intptr_t)uuid_, pr);
  return;
postpone:
  fio_defer(deferred_on_close, uuid_, pr_);
}

static void deferred_on_shutdown(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol) {
    return;
  }
  fio_protocol_s *pr = protocol_try_lock((intptr_t)arg, FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF)
      return;
    goto postpone;
  }
  touchfd(fio_uuid2fd(arg));
  uint8_t r = pr->on_shutdown ? pr->on_shutdown((intptr_t)arg, pr) : 0;
  if (r) {
    if (r == 255) {
      uuid_data(arg).timeout = 0;
    } else {
      uuid_data(arg).timeout = r;
    }
    pr->ping = mock_ping2;
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
  } else {
    uuid_data(arg).timeout = 8;
    pr->ping = mock_ping;
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
    fio_close((intptr_t)arg);
  }
  return;
postpone:
  fio_defer(deferred_on_shutdown, arg, NULL);
  (void)arg2;
}

static void deferred_on_ready_usr(void *arg, void *arg2) {
  errno = 0;
  fio_protocol_s *pr = protocol_try_lock((intptr_t)arg, FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF)
      return;
    goto postpone;
  }
  pr->on_ready((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
  return;
postpone:
  fio_defer(deferred_on_ready, arg, NULL);
  (void)arg2;
}

static void deferred_on_ready(void *arg, void *arg2) {
  errno = 0;
  if (fio_flush((intptr_t)arg) > 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
    if (arg2)
      fio_defer_urgent(deferred_on_ready, arg, NULL);
    else
      fio_poll_add_write(fio_uuid2fd(arg));
    return;
  }
  if (!uuid_data(arg).protocol) {
    return;
  }

  fio_defer(deferred_on_ready_usr, arg, NULL);
}

static void deferred_on_data(void *uuid, void *arg2) {
  if (fio_is_closed((intptr_t)uuid)) {
    return;
  }
  if (!uuid_data(uuid).protocol)
    goto no_protocol;
  fio_protocol_s *pr = protocol_try_lock((intptr_t)uuid, FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF) {
      return;
    }
    goto postpone;
  }
  fio_unlock_group(&uuid_data(uuid).lock, FIO_UUID_LOCK_POLL_READ);
  pr->on_data((intptr_t)uuid, pr);
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
  if (!fio_trylock_group(&uuid_data(uuid).lock, FIO_UUID_LOCK_POLL_READ)) {
    fio_poll_add_read(fio_uuid2fd((intptr_t)uuid));
  }
  return;

postpone:
  if (arg2) {
    /* the event is being forced, so force rescheduling */
    fio_defer(deferred_on_data, (void *)uuid, (void *)1);
  } else {
    /* the protocol was locked, so there might not be any need for the event */
    fio_poll_add_read(fio_uuid2fd((intptr_t)uuid));
  }
  return;

no_protocol:
  /* a missing protocol might still want to invoke the RW hook flush */
  deferred_on_ready(uuid, arg2);
  return;
}

static void deferred_ping(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol || uuid_data(arg).close ||
      uuid_data(arg).active + 1000 > fio_time2milli(fio_data->last_cycle)) {
    return;
  }
  fio_protocol_s *pr = protocol_try_lock((intptr_t)arg, FIO_PR_LOCK_PING);
  if (pr) {
    pr->ping((intptr_t)arg, pr);
    protocol_unlock(pr, FIO_PR_LOCK_PING);
    if (uuid_data(arg).active + 255000 <= fio_time2milli(fio_data->last_cycle))
      fio_close((intptr_t)arg);
  }
  return;
  (void)arg2;
}

/* *****************************************************************************
Setting the protocol
***************************************************************************** */

/** sets up mock functions if missing from the protocol object. */
inline static void protocol_validate(fio_protocol_s *protocol) {
  if (protocol) {
    if (!protocol->on_close) {
      protocol->on_close = mock_on_ev;
    }
    if (!protocol->on_data) {
      protocol->on_data = mock_on_data;
    }
    if (!protocol->on_ready) {
      protocol->on_ready = mock_on_ev;
    }
    if (!protocol->ping) {
      protocol->ping = mock_ping;
    }
    if (!protocol->on_shutdown) {
      protocol->on_shutdown = mock_on_shutdown;
    }
    protocol->rsv = 0;
  }
}

/* managing the protocol pointer array and the `on_close` callback */
static int fio_attach__internal(void *uuid_, void *protocol) {
  intptr_t uuid = (intptr_t)uuid_;
  protocol_validate(protocol);
  UUID_LOCK(uuid, FIO_UUID_LOCK_PROTOCOL, error_invalid_uuid);
  fio_protocol_s *old_pr = uuid_data(uuid).protocol;
  uuid_data(uuid).open = 1;
  uuid_data(uuid).protocol = protocol;
  touchfd(fio_uuid2fd(uuid));
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_PROTOCOL);
  if (old_pr) {
    /* protocol replacement */
    fio_defer(deferred_on_close, (void *)uuid, old_pr);
    if (!protocol) {
      /* hijacking */
      fio_poll_remove_fd(fio_uuid2fd(uuid));
      fio_poll_add_write(fio_uuid2fd(uuid));
    }
  } else if (protocol) {
    /* adding a new uuid to the reactor */
    fio_poll_add(fio_uuid2fd(uuid));
  }
  return 0;

error_invalid_uuid:
  if (protocol)
    fio_defer(deferred_on_close, (void *)uuid, protocol);
  return -1;
}

/**
 * Attaches (or updates) a protocol object to a socket UUID.
 *
 * The new protocol object can be NULL, which will detach ("hijack"), the
 * socket .
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * On error, the new protocol's `on_close` callback will be called immediately.
 */
void fio_attach(intptr_t uuid, fio_protocol_s *protocol) {
  fio_attach__internal((void *)uuid, protocol);
}

/**
 * Attaches (or updates) a protocol object to a file descriptor (fd).
 *
 * The new protocol object can be NULL, which will detach ("hijack"), the
 * socket and the `fd` can be one created outside of facil.io.
 *
 * The old protocol's `on_close` (if any) will be scheduled.
 *
 * On error, the new protocol's `on_close` callback will be called immediately.
 *
 * NOTE: before attaching a file descriptor that was created outside of
 * facil.io's library, make sure it is set to non-blocking mode (see
 * `fio_set_non_block`). facil.io file descriptors are all non-blocking and it
 * will assumes this is the case for the attached fd.
 */
void fio_attach_fd(int fd, fio_protocol_s *protocol) {
  if (fd == -1 || !fio_data || (uintptr_t)fd >= fio_data->capa)
    goto error;
  if (!fd_data(fd).open)
    fio_clear_fd((intptr_t)fd, 1);
  fio_attach__internal((void *)fd2uuid(fd), protocol);
  return;
error:
  if (protocol->on_close)
    protocol->on_close(-1, protocol);
}

/** Sets a timeout for a specific connection (only when running and valid). */
void fio_timeout_set(intptr_t uuid, uint8_t timeout) {
  if (uuid_is_valid(uuid)) {
    uuid_data(uuid).timeout = timeout;
  }
}

/** Gets a timeout for a specific connection. Returns 0 if none. */
uint8_t fio_timeout_get(intptr_t uuid) {
  if (uuid_is_valid(uuid)) {
    return uuid_data(uuid).timeout;
  }
  return 0;
}

/**
 * "Touches" a socket connection, resetting it's timeout counter.
 */
void fio_touch(intptr_t uuid) {
  if (uuid_is_valid(uuid)) {
    touchfd(fio_uuid2fd(uuid));
  }
}

/** Schedules an IO event, even if it did not occur. */
void fio_force_event(intptr_t uuid, enum fio_io_event e) {
  switch (e) {
  case FIO_EVENT_ON_DATA:
    fio_suspend(uuid);
    fio_defer(deferred_on_data, (void *)uuid, NULL);
    break;
  case FIO_EVENT_ON_READY:
    fio_defer(deferred_on_ready, (void *)uuid, NULL);
    break;
  case FIO_EVENT_ON_TIMEOUT:
    fio_defer(deferred_ping, (void *)uuid, NULL);
    break;
  }
}

/**
 * Temporarily prevents `on_data` events from firing.
 *
 * The `on_data` event will be automatically rescheduled when (if) the socket's
 * outgoing buffer fills up or when `fio_force_event` is called with
 * `FIO_EVENT_ON_DATA`.
 *
 * Note: the function will work as expected when called within the protocol's
 * `on_data` callback and the `uuid` refers to a valid socket. Otherwise the
 * function might quietly fail.
 */
void fio_suspend(intptr_t uuid) {
  UUID_TRYLOCK(uuid, FIO_UUID_LOCK_POLL_READ, reschedule_lable, invalid_lable);
reschedule_lable:
invalid_lable:
  return;
}

/**
 * Returns the maximum number of open files facil.io can handle per worker
 * process.
 *
 * Total OS limits might apply as well but aren't shown.
 *
 * The value of 0 indicates either that the facil.io library wasn't initialized
 * yet or that it's resources were released.
 */
size_t fio_capa(void) {
  if (!fio_data)
    return 0;
  return fio_data->capa;
}

/* *****************************************************************************
Lower Level Protocol API - for special circumstances, use with care.
***************************************************************************** */

/**
 * This function allows out-of-task access to a connection's `fio_protocol_s`
 * object by attempting to acquire a locked pointer.
 *
 * CAREFUL: mostly, the protocol object will be locked and a pointer will be
 * sent to the connection event's callback. However, if you need access to the
 * protocol object from outside a running connection task, you might need to
 * lock the protocol to prevent it from being closed / freed in the background.
 *
 * On error, consider calling `fio_defer` or `fio_defer_io_task` instead of busy
 * waiting. Busy waiting SHOULD be avoided whenever possible.
 */
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid) {
  return protocol_try_lock(uuid, FIO_PR_LOCK_TASK);
}
/** Don't unlock what you don't own... see `fio_protocol_try_lock` for
 * details. */
void fio_protocol_unlock(fio_protocol_s *pr) {
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
}

/* *****************************************************************************







                            Listening protocol







***************************************************************************** */
typedef struct {
  fio_protocol_s pr;
  void (*on_open)(intptr_t uuid, void *udata);
  fio_tls_s *tls;
  void *udata;
  uint32_t addr_len;
  int fd;
  char addr[];
} fio_listen_s;

/* *****************************************************************************
Listen protocol state callbacks
***************************************************************************** */
FIO_SFUNC void fio___listen_start(void *pr_) {
  fio_listen_s *p = (fio_listen_s *)pr_;
  if (!fio_is_worker()) /* only workers listen */
    return;
  if (fio_is_master()) {
    /* copy everything, so the socket isn't closed when the reactor stops. */
    fio_listen_s *tmp = malloc(sizeof(*tmp) + p->addr_len + 1);
    memcpy(tmp, p, sizeof(*tmp) + p->addr_len + 1);
    tmp->fd = dup(p->fd);
    if (tmp->fd == -1) {
      FIO_LOG_ERROR("listening file descriptor duplication error!");
      tmp->fd = p->fd;
    }
    if (tmp->tls)
      fio_tls_dup(tmp->tls);
    p = tmp;
  }
  fio_attach_fd(p->fd, &p->pr);
  FIO_LOG_INFO("(%d) started listening at: %s", (int)getpid(), p->addr);
}

FIO_SFUNC void fio___listen_free(void *pr_) {
  fio_listen_s *p = (fio_listen_s *)pr_;
  close(p->fd);
  if (p->tls)
    fio_tls_free(p->tls);
  free(p);
}

/* *****************************************************************************
Listening protocol callbacks
***************************************************************************** */

FIO_SFUNC void fio_listen___on_data(intptr_t uuid, fio_protocol_s *pr) {
  fio_listen_s *p = (fio_listen_s *)pr;
  intptr_t c;
  while ((c = fio_accept(uuid)) != -1) {
    p->on_open(c, p->udata);
  }
}

FIO_SFUNC void fio_listen___on_data_tls(intptr_t uuid, fio_protocol_s *pr) {
  fio_listen_s *p = (fio_listen_s *)pr;
  intptr_t c;
  while ((c = fio_accept(uuid)) != -1) {
    fio_tls_accept(c, p->tls, p->udata);
  }
}

FIO_SFUNC void fio_listen___on_data_tls_no_alpn(intptr_t uuid,
                                                fio_protocol_s *pr) {
  fio_listen_s *p = (fio_listen_s *)pr;
  intptr_t c;
  while ((c = fio_accept(uuid)) != -1) {
    fio_tls_accept(c, p->tls, p->udata);
    p->on_open(c, p->udata);
  }
}

FIO_SFUNC void fio_listen___on_close(intptr_t uuid, fio_protocol_s *pr) {
  fio_listen_s *p = (fio_listen_s *)pr;
  FIO_LOG_INFO("(%d) stopped listening at: %s", (int)getpid(), p->addr);
  /* the master process will have a duplicated data-set */
  if (fio_is_master())
    fio___listen_free(pr);
  (void)uuid;
}

/* *****************************************************************************
Listening to Incoming Connections - Public API
***************************************************************************** */
/* sublime text marker */
intptr_t fio_listen___(void);
/**
 * Sets up a network service on a listening socket.
 *
 * Returns the listening socket's uuid or -1 (on error).
 *
 * See the `fio_listen` Macro for details.
 */
intptr_t fio_listen FIO_NOOP(struct fio_listen_args args) {
  static uint16_t port_static = 0;
  char buf[1024];
  char port[64];
  fio_listen_s *p = NULL;
  size_t addr_len = args.url ? strlen(args.url) : 0;

  /* one time setup for auto-port numbering (3000, 3001, etc'...) */
  if (!port_static) {
    port_static = 3000;
    char *port_env = getenv("PORT");
    if (port_env)
      port_static = fio_atol(&port_env);
  }

  /* No URL address give, use our own? */
  if (!args.url || !addr_len) {
    char *src = "0.0.0.0";
    if (getenv("ADDRESS"))
      src = getenv("ADDRESS");
    addr_len = strlen(src);
    memcpy(buf, src, addr_len);
    buf[addr_len++] = ':';
    addr_len +=
        fio_ltoa(buf + addr_len, (int64_t)fio_atomic_add(&port_static, 1), 10);
    buf[addr_len] = 0;
    args.url = buf;
  }
  /* parse URL */
  fio_url_s u = fio_url_parse(args.url, addr_len);
  if (!u.host.buf)
    ;
  if (!u.port.buf) {
    if (u.scheme.buf &&
        (((u.scheme.buf[0] | 32) == 'h' && (u.scheme.buf[1] | 32) == 't' &&
          (u.scheme.buf[2] | 32) == 't' && (u.scheme.buf[3] | 32) == 'p') ||
         ((u.scheme.buf[0] | 32) == 'w' && (u.scheme.buf[1] | 32) == 's'))) {
      u.port.buf = "http";
      u.port.len = 4;
      if ((u.scheme.buf[4] | 32) == 's' || (u.scheme.buf[2] | 32) == 's') {
        u.port.buf = "https";
        u.port.len = 5;
      }
    } else if (u.host.buf) {
      u.port.buf = port;
      u.port.len = fio_ltoa(port, (int64_t)fio_atomic_add(&port_static, 1), 10);
      u.port.buf[u.port.len] = 0;
    }
  }
  p = (fio_listen_s *)malloc(sizeof(*p) + addr_len + 1);
  FIO_ASSERT_ALLOC(p);
  *p = (fio_listen_s){
      .pr =
          {
              .on_data = (args.tls ? (fio_tls_alpn_count(args.tls)
                                          ? fio_listen___on_data_tls
                                          : fio_listen___on_data_tls_no_alpn)
                                   : fio_listen___on_data),
              .on_close = fio_listen___on_close,
              .ping = FIO_PING_ETERNAL,
          },
      .on_open = args.on_open,
      .tls = args.tls,
      .addr_len = addr_len,
  };
  memcpy(p->addr, args.url, addr_len);
  p->addr[addr_len] = 0;
  if (!u.host.buf && !u.port.buf && u.path.buf) {
    /* unix socket */
    p->fd = fio_sock_open(
        u.path.buf, NULL, FIO_SOCK_SERVER | FIO_SOCK_UNIX | FIO_SOCK_NONBLOCK);
    if (p->fd == -1) {
      FIO_LOG_ERROR("failed to open a listening UNIX socket at: %s", p->addr);
      goto error;
    }
  } else {
    if (u.host.buf && u.host.len < 1024) {
      memmove(buf, u.host.buf, u.host.len);
      buf[u.host.len] = 0;
      u.host.buf = buf;
    }
    if (u.port.len < 64) {
      memmove(port, u.port.buf, u.port.len);
      port[u.port.len] = 0;
      u.port.buf = port;
    }
    p->fd = fio_sock_open(u.host.buf,
                          u.port.buf,
                          FIO_SOCK_SERVER | FIO_SOCK_TCP | FIO_SOCK_NONBLOCK);
    if (p->fd == -1) {
      FIO_LOG_ERROR("failed to open a listening TCP/IP socket at: %s", p->addr);
      goto error;
    }
  }
  fio_tls_dup(p->tls);
  if (!fio_data || !fio_data->active) {
    fio_state_callback_add(FIO_CALL_ON_START, fio___listen_start, p);
  } else {
    FIO_LOG_WARNING("fio_listen called while running (fio_start), this is "
                    "unsafe in multi-process implementations");
    fio___listen_start(p);
  }

  fio_state_callback_add(FIO_CALL_AT_EXIT, fio___listen_free, p);
  if (args.on_finish)
    fio_state_callback_add(FIO_CALL_AT_EXIT, args.on_finish, args.udata);
  return (intptr_t)p;

error:
  if (args.on_finish)
    args.on_finish(args.udata);
  free(p);
  return -1;
}

/** A ping function that does nothing except keeping the connection alive. */
void FIO_PING_ETERNAL(intptr_t uuid, fio_protocol_s *pr) {
  (void)pr;
  touchfd(fio_uuid2fd(uuid));
}

/* *****************************************************************************







                Connecting to remote servers as a client







***************************************************************************** */

/* TODO: all of it... persistent connections (auto-reconnect)? */

/**
Creates a client connection (in addition or instead of the server).

See the `struct fio_connect_args` details for any possible named arguments.

* `.address` should be the address of the server.

* `.port` the server's port.

* `.udata`opaque user data.

* `.on_connect` called once a connection was established.

    Should return a pointer to a `fio_protocol_s` object, to handle connection
    callbacks.

* `.on_fail` called if a connection failed to establish.

(experimental: untested)
*/
intptr_t fio_connect FIO_NOOP(struct fio_connect_args args);

/* *****************************************************************************
Test Protocol API
***************************************************************************** */
#ifdef TEST

/* protocol locking tests */
FIO_SFUNC void FIO_NAME_TEST(io, protocol_lock)(void) {
  fprintf(stderr, "* testing protocol lock aquasition.\n");
  /* switch to test data set */
  fio_protocol_s *pr, *pr2;
  /* start tests */
  pr = fio_protocol_try_lock(fd2uuid(2));
  FIO_ASSERT(!pr, "locking a UUID without a protocol should return NULL");
  FIO_ASSERT(errno = EBADF, "no protocol should return EBADF");
  pr = fio_protocol_try_lock(fd2uuid(1));
  FIO_ASSERT(pr == fd_data(1).protocol,
             "locking UUID should return protocol (%p): %s",
             (void *)pr,
             strerror(errno));
  FIO_ASSERT(!fio_protocol_try_lock(fd2uuid(1)),
             "locking a locked protocol should fail.");
  FIO_ASSERT(errno = EWOULDBLOCK, "protocol busy should return EWOULDBLOCK");
  pr2 = protocol_try_lock(fd2uuid(1), 1);
  FIO_ASSERT(pr2 == pr, "locking an available sublock should be possible");
  fio_protocol_unlock(pr);
  pr = fio_protocol_try_lock(fd2uuid(1));
  FIO_ASSERT(pr2 == pr, "re-locking should be possible");
  fio_protocol_unlock(pr);
  protocol_unlock(pr2, 1);
}

FIO_SFUNC void FIO_NAME_TEST(io, protocol)(void) {
  /* TODO: test attachment, listening and connecting  */
  FIO_NAME_TEST(io, protocol_lock)();
  /*
   * TODO
   */
}

#endif /* TEST */
