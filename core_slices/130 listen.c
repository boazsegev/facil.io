/* *****************************************************************************
Listening to Incoming Connections
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

FIO_SFUNC void fio_listen_on_data(fio_s *io) {
  struct fio_listen_args *l = fio_udata_get(io);
  int fd;
  while ((fd = accept(io->fd, NULL, NULL)) != -1) {
    FIO_LOG_DEBUG2("accepting a new connection (fd %d) at %s", fd, l->url);
    l->on_open(fd, l->udata);
  }
}
FIO_SFUNC void fio_listen_on_close(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_INFO("(%d) stopped listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)getpid()),
               l->url);
}

static fio_protocol_s FIO_PROTOCOL_LISTEN = {
    .on_data = fio_listen_on_data,
    .on_close = fio_listen_on_close,
    .on_timeout = mock_ping_eternal,
};

FIO_SFUNC void fio_listen___attach(void *udata) {
  struct fio_listen_args *l = udata;
#if FIO_OS_WIN
  int fd = -1;
  {
    SOCKET tmpfd = INVALID_SOCKET;
    WSAPROTOCOL_INFOA info;
    if (!WSADuplicateSocketA(l->reserved, GetCurrentProcessId(), &info) &&
        (tmpfd =
             WSASocketA(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, &info, 0, 0)) !=
            INVALID_SOCKET) {
      if (FIO_SOCK_FD_ISVALID(tmpfd))
        fd = (int)tmpfd;
      else
        fio_sock_close(tmpfd);
    }
  }
#else
  int fd = dup(l->reserved);
#endif
  FIO_ASSERT(fd != -1, "listening socket failed to `dup`");
  FIO_LOG_DEBUG2("Called dup(%d) to attach %d as a listening socket.",
                 l->reserved,
                 fd);
  fio_attach_fd(fd, &FIO_PROTOCOL_LISTEN, l, NULL);
  FIO_LOG_INFO("(%d) started listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)getpid()),
               l->url);
}

FIO_SFUNC void fio_listen___free(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_DEBUG2("(%d) closing listening socket at %s", getpid(), l->url);
  fio_sock_close(l->reserved); /* this socket was dupped and unused */
  fio_state_callback_remove(FIO_CALL_PRE_START, fio_listen___attach, l);
  fio_state_callback_remove(FIO_CALL_ON_START, fio_listen___attach, l);
  free(l);
}

int fio_listen___(void); /* Sublime Text marker */
int fio_listen FIO_NOOP(struct fio_listen_args args) {
  struct fio_listen_args *info = NULL;
  static uint16_t port_static = 0;
  char buf[1024];
  char port[64];
  fio_url_s u;
  size_t len = args.url ? strlen(args.url) : 0;
  if (!args.on_open ||
      (args.is_master_only && !fio_data.is_master && fio_data.running) ||
      len > 1024)
    goto error;

  /* No URL address give, use our own? */
  if (!args.url || !len) {
    char *src = "0.0.0.0";
    /* one time setup for auto-port numbering (3000, 3001, etc'...) */
    if (!port_static) {
      port_static = 3000;
      char *port_env = getenv("PORT");
      if (port_env)
        port_static = fio_atol(&port_env);
    }
    if (getenv("ADDRESS"))
      src = getenv("ADDRESS");
    len = strlen(src);
    FIO_MEMCPY(buf, src, len);
    buf[len++] = ':';
    len += fio_ltoa(buf + len, (int64_t)fio_atomic_add(&port_static, 1), 10);
    buf[len] = 0;
    args.url = buf;
  } else {
    FIO_MEMCPY(buf, args.url, len + 1);
  }

  info = malloc(sizeof(*info) + len + 1);
  FIO_ASSERT_ALLOC(info);
  *info = args;
  info->url = (const char *)(info + 1);
  memcpy(info + 1, buf, len + 1);

  /* parse URL */
  u = fio_url_parse(args.url, len);
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
  if (!u.host.buf && !u.port.buf && u.path.buf) {
/* unix socket */
#if FIO_OS_POSIX
    info->reserved =
        fio_sock_open(u.path.buf,
                      NULL,
                      FIO_SOCK_SERVER | FIO_SOCK_UNIX | FIO_SOCK_NONBLOCK);
    if (info->reserved == -1) {
      FIO_LOG_ERROR("failed to open a listening UNIX socket at: %s",
                    u.path.buf);
      goto error;
    }
#else
    FIO_LOG_ERROR("Unix suckets aren't supported on Windows.");
    goto error;
#endif
  } else {
    if (u.host.buf && u.host.len < 1024) {
      if (buf != u.host.buf)
        memmove(buf, u.host.buf, u.host.len);
      buf[u.host.len] = 0;
      u.host.buf = buf;
    }
    if (u.port.len < 64) {
      memmove(port, u.port.buf, u.port.len);
      port[u.port.len] = 0;
      u.port.buf = port;
    }
    info->reserved =
        fio_sock_open(u.host.buf,
                      u.port.buf,
                      FIO_SOCK_SERVER | FIO_SOCK_TCP | FIO_SOCK_NONBLOCK);
    if (info->reserved == -1) {
      FIO_LOG_ERROR("failed to open a listening TCP/IP socket at: %s:%s",
                    buf,
                    port);
      goto error;
    }
  }

  /* choose fd attachment timing (fd will be cleared during a fork) */
  if (args.is_master_only)
    fio_state_callback_add(FIO_CALL_PRE_START, fio_listen___attach, info);
  else
    fio_state_callback_add(FIO_CALL_ON_START, fio_listen___attach, info);
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio_listen___free, info);
  if (fio_data.running) {
    fio_listen___attach(info);
    if ((args.is_master_only && fio_data.is_master) ||
        (!args.is_master_only && fio_data.is_worker)) {
      FIO_LOG_WARNING("fio_listen called while running (fio_start), this is "
                      "unsafe in multi-process implementations");
    } else {
      FIO_LOG_ERROR("fio_listen called while running (fio_start) resulted in "
                    "attaching the socket to the wrong process.");
    }
  }
  return 0;
error:
  free(info);
  if (args.on_finish)
    args.on_finish(args.udata);
  return -1;
  (void)args;
  (void)args;
}
