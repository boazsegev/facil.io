/* *****************************************************************************












              Socket / Connection - Read / Write Functions












***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* ************************************************************************** */

/**
 * `fio_read` attempts to read up to count bytes from the socket into the
 * buffer starting at `buffer`.
 *
 * `fio_read`'s return values are wildly different then the native return
 * values and they aim at making far simpler sense.
 *
 * `fio_read` returns the number of bytes read (0 is a valid return value which
 * simply means that no bytes were read from the buffer).
 *
 * On a fatal connection error that leads to the connection being closed (or if
 * the connection is already closed), `fio_read` returns -1.
 *
 * The value 0 is the valid value indicating no data was read.
 *
 * Data might be available in the kernel's buffer while it is not available to
 * be read using `fio_read` (i.e., when using a transport layer, such as TLS).
 */
ssize_t fio_read(intptr_t uuid, void *buf, size_t len) {
  if (!uuid_is_valid(uuid) || !uuid_data(uuid).open) {
    errno = EBADF;
    return -1;
  }
  if (len == 0)
    return 0;
  int old_errno = errno;
  ssize_t ret;
  void *udata;
  ssize_t (*rw_read)(intptr_t, void *, void *, size_t);

  UUID_LOCK(uuid, FIO_UUID_LOCK_RW_HOOKS_READ, invalid_uuid);
  rw_read = uuid_data(uuid).rw_hooks->read;
  udata = uuid_data(uuid).rw_udata;
  while ((ret = rw_read(uuid, udata, buf, len)) < 0 && errno == EINTR)
    ;
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS_READ);
  if (ret > 0) {
    fio_touch(uuid);
    return ret;
  }
  if (ret < 0 &&
      (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOTCONN)) {
    errno = old_errno;
    return 0;
  }
  fio_force_close(uuid);
invalid_uuid:
  return -1;
}

/**
 * `fio_write2_fn` is the actual function behind the macro `fio_write2`.
 */
ssize_t fio_write2_fn(intptr_t uuid, fio_write_args_s o) {
  fio_stream_packet_s *p = NULL;
  uint8_t had_any = fio_stream_any(&uuid_data(uuid).stream);
  if (o.is_fd) {
    p = fio_stream_pack_fd(o.data.fd, o.len, o.offset, o.keep_open);
  } else {
    if (!o.copy && !o.dealloc)
      o.dealloc = free;
    p = fio_stream_pack_data(
        (void *)o.data.buf, o.len, o.offset, o.copy, o.dealloc);
  }
  if (!p)
    goto some_error;
  UUID_LOCK(uuid, FIO_UUID_LOCK_STREAM, some_error);
  fio_stream_add(&uuid_data(uuid).stream, p);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);
  if (!had_any) {
    fio_defer_urgent(deferred_on_ready, (void *)uuid, NULL);
  }
  return (ssize_t)o.len;
some_error:
  fio_stream_pack_free(p);
  return -1;
}

/** A noop function for fio_write2 in cases not deallocation is required. */
void FIO_DEALLOC_NOOP(void *arg) { (void)arg; }

/**
 * Returns the number of `fio_write` calls that are waiting in the socket's
 * queue and haven't been processed.
 */
size_t fio_pending(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    return 0;
  return fio_stream_packets(&uuid_data(uuid).stream);
}

/**
 * `fio_flush` attempts to write any remaining data in the internal buffer to
 * the underlying file descriptor and closes the underlying file descriptor once
 * if it's marked for closure (and all the data was sent).
 *
 * Return values: 1 will be returned if data remains in the buffer. 0
 * will be returned if the buffer was fully drained. -1 will be returned on an
 * error or when the connection is closed.
 *
 * errno will be set to EWOULDBLOCK if the socket's lock is busy.
 */
ssize_t fio_flush(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    return -1;
  char mem[FIO_SOCKET_BUFFER_PER_WRITE];
  char *buf = mem;
  size_t len = FIO_SOCKET_BUFFER_PER_WRITE;
  ssize_t w = 0;
  ssize_t fl = 0;
  UUID_LOCK(uuid, FIO_UUID_LOCK_RW_HOOKS_WRITE | FIO_UUID_LOCK_STREAM, some_error);

  fio_stream_read(&uuid_data(uuid).stream, &buf, &len);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);

  fl = uuid_data(uuid).rw_hooks->flush(uuid, uuid_data(uuid).rw_udata);
  if (len)
    w = uuid_data(uuid).rw_hooks->write(
        uuid, uuid_data(uuid).rw_udata, buf, len);

  if (w && w > 0) {
    UUID_LOCK(uuid, FIO_UUID_LOCK_STREAM, some_error);
    fio_stream_advance(&uuid_data(uuid).stream, w);
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);
  }

  UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS_WRITE);
  if(fl > 0 || w > 0) {
   touchfd(fio_uuid2fd(uuid));
  }

  if (fl > 0 || fio_stream_any(&uuid_data(uuid).stream))
    return 1;

  if (uuid_data(uuid).close) {
    const int fd = fio_uuid2fd(uuid);
    fio_clear_fd(fd, 0);
    close(fd);
  }

  return 0;

some_error:
  return -1;
}

/** Blocks until all the data was flushed from the buffer */
#define fio_flush_strong(uuid)                                                 \
  do {                                                                         \
    errno = 0;                                                                 \
  } while (fio_flush(uuid) > 0 || errno == EWOULDBLOCK)

/**
 * `fio_flush_all` attempts flush all the open connections.
 *
 * Returns the number of sockets still in need to be flushed.
 */
size_t fio_flush_all(void) {
  if (!fio_data)
    return 0;
  size_t c = 0;
  for (size_t i = 0; i <= fio_data->max_open_fd; ++i) {
    if (fio_flush(fd2uuid(i)) == 1)
      ++c;
  }
  return c;
}

/* *****************************************************************************



Connection Object Links / Environment



***************************************************************************** */

/**
 * Links an object to a connection's lifetime / environment.
 *
 * The `on_close` callback will be called once the connection has died.
 *
 * If the `uuid` is invalid, the `on_close` callback will be called immediately.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
void fio_uuid_env_set___(void); /* function marker */
void fio_uuid_env_set FIO_NOOP(intptr_t uuid, fio_uuid_env_args_s args) {
  uint64_t hash =
      fio_risky_hash(args.name.buf, args.name.len, (uint64_t)uuid + args.type);
  fio_str_s key = FIO_STR_INIT;
  fio_uuid_env_obj_s obj = {.data = args.data, .on_close = args.on_close};
  fio_uuid_env_obj_s old = {0};
  if (args.const_name) {
    fio_str_init_const(&key, args.name.buf, args.name.len);
  } else {
    fio_str_init_copy(&key, args.name.buf, args.name.len);
  }
  UUID_LOCK(uuid, FIO_UUID_LOCK_ENV, invalid_uuid);
  fio___uuid_env_set(&uuid_data(uuid).env, hash, key, obj, &old);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_ENV);
  if (old.on_close)
    old.on_close(old.data);
  return;
invalid_uuid:
  fio_str_destroy(&key);
  if (args.on_close)
    args.on_close(args.data);
}

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found, setting `errno`
 * to `EBADF` if the `uuid` was invalid and `ENOTCONN` if the object wasn't
 * found (wasn't linked).
 *
 * NOTICE: a failure likely means that the object's `on_close` callback was
 * already called!
 */
void fio_uuid_env_unset___(void); /* function marker */
int fio_uuid_env_unset FIO_NOOP(intptr_t uuid, fio_uuid_env_unset_args_s args) {
  int r = -1;
  uint64_t hash =
      fio_risky_hash(args.name.buf, args.name.len, (uint64_t)uuid + args.type);
  fio_uuid_env_obj_s old = {0};
  fio_str_s key;
  fio_str_init_const(&key, args.name.buf, args.name.len);
  UUID_LOCK(uuid, FIO_UUID_LOCK_ENV, invalid_uuid);
  r = fio___uuid_env_remove(&uuid_data(uuid).env, hash, key, &old);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_ENV);
  return r;
invalid_uuid:
  fio_str_destroy(&key);
  return r;
}

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 *
 * NOTE: the `on_close` callback will be called within a high priority lock.
 * Long tasks should be deferred so they are performed outside the lock.
 */
void fio_uuid_env_remove___(void); /* function marker */
void fio_uuid_env_remove FIO_NOOP(intptr_t uuid,
                                  fio_uuid_env_unset_args_s args) {
  {
    uint64_t hash = fio_risky_hash(
        args.name.buf, args.name.len, (uint64_t)uuid + args.type);
    fio_uuid_env_obj_s old = {0};
    fio_str_s key;
    fio_str_init_const(&key, args.name.buf, args.name.len);
    UUID_LOCK(uuid, FIO_UUID_LOCK_ENV, invalid_uuid);
    fio___uuid_env_remove(&uuid_data(uuid).env, hash, key, &old);
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_ENV);
    if (old.on_close)
      old.on_close(old.data);
    return;
  invalid_uuid:
    fio_str_destroy(&key);
    return;
  }
}

/* *****************************************************************************
Test UUID Linking
***************************************************************************** */
#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, env_on_close)(void *obj) {
  fio_atomic_add((uintptr_t *)obj, 1);
}

FIO_SFUNC void FIO_NAME_TEST(io, env)(void) {
  fprintf(stderr, "=== Testing fio_uuid_env\n");
  uintptr_t called = 0;
  uintptr_t removed = 0;
  uintptr_t overwritten = 0;
  intptr_t uuid = fio_socket(
      NULL, "8765", FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(uuid != -1, "fio_uuid_env_test failed to create a socket!");
  fio_uuid_env_set(uuid,
                   .data = &called,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 1);
  FIO_ASSERT(called == 0,
             "fio_uuid_env_set failed - on_close callback called too soon!");
  fio_uuid_env_set(uuid,
                   .data = &removed,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 0);
  fio_uuid_env_set(uuid,
                   .data = &overwritten,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 0,
                   .name.buf = "abcdefghijklmnopqrstuvwxyz",
                   .name.len = 26);
  fio_uuid_env_set(uuid,
                   .data = &overwritten,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 0,
                   .name.buf = "abcdefghijklmnopqrstuvwxyz",
                   .name.len = 26,
                   .const_name = 1);
  fio_uuid_env_unset(uuid, .type = 0);
  fio_close(uuid);
  fio_defer_perform();
  FIO_ASSERT(called,
             "fio_uuid_env_set failed - on_close callback wasn't called!");
  FIO_ASSERT(!removed,
             "fio_uuid_env_unset failed - on_close callback was called "
             "(wasn't removed)!");
  FIO_ASSERT(
      overwritten == 2,
      "fio_uuid_env_set overwrite failed - on_close callback wasn't called!");
  fprintf(stderr, "* passed.\n");
}
#endif /* TEST */

/* *****************************************************************************




Socket / Connection Functions




***************************************************************************** */

/**
 * `fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active
 * `uuid`.
 *
 * If the file descriptor was closed, __it will be registered as open__.
 *
 * If the file descriptor was closed directly (not using `fio_close`) or the
 * closure event hadn't been processed, a false positive will be possible. This
 * is not an issue, since the use of an invalid fd will result in the registry
 * being updated and the fd being closed.
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 */
intptr_t fio_fd2uuid(int fd) {
  if (fd < 0 || (uint32_t)fd >= fio_data->capa)
    return -1;
  return fd2uuid(fd);
}

/**
 * Creates a TCP/IP, UDP or Unix socket and returns it's unique identifier.
 *
 * For TCP/IP or UDP server sockets (flag sets `FIO_SOCKET_SERVER`), a NULL
 * `address` variable is recommended. Use "localhost" or "127.0.0.1" to limit
 * access to the local machine.
 *
 * For TCP/IP or UDP client sockets (flag sets `FIO_SOCKET_CLIENT`), a remote
 * `address` and `port` combination will be required. `connect` will be called.
 *
 * For TCP/IP and Unix server sockets (flag sets `FIO_SOCKET_SERVER`), `listen`
 * will automatically be called by this function.
 *
 * For Unix server or client sockets, the `port` variable is silently ignored.
 *
 * If the socket is meant to be attached to the facil.io reactor,
 * `FIO_SOCKET_NONBLOCK` MUST be set.
 *
 * The following flags control the type and behavior of the socket:
 *
 * - FIO_SOCKET_SERVER - (default) server mode (may call `listen`).
 * - FIO_SOCKET_CLIENT - client mode (calls `connect).
 * - FIO_SOCKET_NONBLOCK - sets the socket to non-blocking mode.
 * - FIO_SOCKET_TCP - TCP/IP socket (default).
 * - FIO_SOCKET_UDP - UDP socket.
 * - FIO_SOCKET_UNIX - Unix Socket.
 *
 * Returns -1 on error. Any other value is a valid unique identifier.
 *
 * Note: facil.io uses unique identifiers to protect sockets from collisions.
 *       However these identifiers can be converted to the underlying file
 *       descriptor using the `fio_uuid2fd` macro.
 *
 * Note: UDP server sockets can't use `fio_read` or `fio_write` since they are
 * connectionless.
 */
intptr_t
fio_socket(const char *address, const char *port, fio_socket_flags_e flags) {
  int fd = fio_sock_open(address, port, flags);
  if (fd == -1)
    return -1;
  fio_clear_fd(fd, 1);
  /* copy address to the uuid */
  if (address) {
    size_t addr_len = strlen(address);
    if (addr_len > (FIO_MAX_ADDR_LEN - 1))
      addr_len = (FIO_MAX_ADDR_LEN - 1);
    memcpy(fd_data(fd).addr, address, addr_len);
    fd_data(fd).addr_len = addr_len;
  } else {
    memcpy(fd_data(fd).addr, "0.0.0.0", 7);
    fd_data(fd).addr_len = 7;
  }
  if (port && !(flags & FIO_SOCKET_UNIX)) {
    size_t port_len = strlen(port);
    if (fd_data(fd).addr_len + port_len <= (FIO_MAX_ADDR_LEN - 2)) {
      fd_data(fd).addr[fd_data(fd).addr_len++] = ':';
      memcpy(fd_data(fd).addr + fd_data(fd).addr_len, port, port_len);
      fd_data(fd).addr_len += port_len;
    }
  }
  fd_data(fd).addr[fd_data(fd).addr_len] = 0;
  return fd2uuid(fd);
}

/**
 * `fio_accept` accepts a new socket connection from a server socket - see the
 * server flag on `fio_socket`.
 *
 * Accepted connection are automatically set to non-blocking mode and the
 * O_CLOEXEC flag is set.
 *
 * NOTE: this function does NOT attach the socket to the IO reactor - see
 * `fio_attach`.
 */
intptr_t fio_accept(intptr_t srv_uuid) {
  struct sockaddr_in6 addrinfo[2]; /* grab a slice of stack (aligned) */
  socklen_t addrlen = sizeof(addrinfo);
  int client;

  if (!uuid_is_valid(srv_uuid))
    return -1;

#ifdef SOCK_NONBLOCK
  client = accept4(fio_uuid2fd(srv_uuid),
                   (struct sockaddr *)addrinfo,
                   &addrlen,
                   SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (client <= 0)
    return -1;
#else
  client = accept(fio_uuid2fd(srv_uuid), (struct sockaddr *)addrinfo, &addrlen);
  if (client <= 0)
    return -1;
  if (fio_set_non_block(client) == -1) {
    close(client);
    return -1;
  }
#endif

  if ((uint32_t)client >= fio_data->capa) {
    close(client);
    return -1;
  }

  // avoid the TCP delay algorithm.
  {
    int optval = 1;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
  }
  // handle socket buffers.
  {
    int optval = 0;
    socklen_t size = (socklen_t)sizeof(optval);
    if (!getsockopt(client, SOL_SOCKET, SO_SNDBUF, &optval, &size) &&
        optval <= 131072) {
      optval = 131072;
      setsockopt(client, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
      optval = 131072;
      setsockopt(client, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
    }
  }

  fio_clear_fd(client, 1);
  /* copy peer address */
  if (((struct sockaddr *)addrinfo)->sa_family == AF_UNIX) {
    fd_data(client).addr_len = uuid_data(srv_uuid).addr_len;
    if (uuid_data(srv_uuid).addr_len) {
      memcpy(fd_data(client).addr,
             uuid_data(srv_uuid).addr,
             uuid_data(srv_uuid).addr_len + 1);
    }
  } else {
    fio_tcp_addr_cpy(client,
                     ((struct sockaddr *)addrinfo)->sa_family,
                     (struct sockaddr *)addrinfo);
  }

  return fd2uuid(client);
}

/**
 * Sets a socket to non blocking state.
 *
 * This will also set the O_CLOEXEC flag for the file descriptor.
 *
 * This function is called automatically for the new socket, when using
 * `fio_accept`, `fio_connect` or `fio_socket`.
 */
int fio_set_non_block(int fd) { return fio_sock_set_non_block(fd); }

/**
 * Returns 1 if the uuid refers to a valid and open, socket.
 *
 * Returns 0 if not.
 */
int fio_is_valid(intptr_t uuid) { return uuid_is_valid(uuid); }

/**
 * Returns 1 if the uuid is invalid or the socket is flagged to be closed.
 *
 * Returns 0 if the socket is valid, open and isn't flagged to be closed.
 */
int fio_is_closed(intptr_t uuid) {
  return !uuid_is_valid(uuid) || !uuid_data(uuid).open || uuid_data(uuid).close;
}

/**
 * `fio_close` marks the connection for disconnection once all the data was
 * sent. The actual disconnection will be managed by the `fio_flush` function.
 *
 * `fio_flash` will be automatically scheduled.
 */
void fio_close(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    goto bad_fd;
  if (fio_stream_any(&uuid_data(uuid).stream) || uuid_data(uuid).lock) {
    uuid_data(uuid).close = 1;
    fio_defer_urgent(deferred_on_ready, (void *)uuid, NULL);
    return;
  }
  fio_force_close(uuid);
  return;
bad_fd:
  if (uuid != -1 && (uint32_t)fio_uuid2fd(uuid) >= fio_data->capa)
    goto too_high;
  errno = EBADF;
  return;
too_high:
  close(fio_uuid2fd(uuid));
}

/**
 * `fio_force_close` closes the connection immediately, without adhering to any
 * protocol restrictions and without sending any remaining data in the
 * connection buffer.
 */
void fio_force_close(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    goto bad_fd;
  // FIO_LOG_DEBUG("fio_force_close called for uuid %p", (void *)uuid);
  /* make sure the close marker is set */
  if (!uuid_data(uuid).close)
    uuid_data(uuid).close = 1;
  /* clear away any packets in case we want to cut the connection short. */
  fio_stream_s stream;
  UUID_LOCK(uuid, FIO_UUID_LOCK_STREAM, bad_fd);
  stream = uuid_data(uuid).stream;
  uuid_data(uuid).stream =
      (fio_stream_s)FIO_STREAM_INIT(uuid_data(uuid).stream);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);
  fio_stream_destroy(&stream);
  /* check for rw-hooks termination packet */
  UUID_LOCK(uuid, FIO_UUID_LOCK_RW_HOOKS, bad_fd);
  if (uuid_data(uuid).open && (uuid_data(uuid).close & 1) &&
      uuid_data(uuid).rw_hooks->before_close(uuid, uuid_data(uuid).rw_udata)) {
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS);
    uuid_data(uuid).close = 2; /* don't repeat the before_close callback */
    fio_touch(uuid);
    fio_defer_urgent(deferred_on_ready, (void *)uuid, NULL);
    return;
  }
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS);
  fio_clear_fd(fio_uuid2fd(uuid), 0);
  close(fio_uuid2fd(uuid));
#if FIO_ENGINE_POLL
  fio_poll_remove_fd(fio_uuid2fd(uuid));
#endif
  return;
bad_fd:
  if (uuid != -1 && (uint32_t)fio_uuid2fd(uuid) >= fio_data->capa)
    goto too_high;
  errno = EBADF;
  return;
too_high:
  close(fio_uuid2fd(uuid));
}

/**
 * Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`addr == NULL`).
 * The information is only available when the socket was accepted using
 * `fio_accept` or opened using `fio_connect`.
 */
fio_str_info_s fio_peer_addr(intptr_t uuid) {
  if (fio_is_closed(uuid) || !uuid_data(uuid).addr_len)
    return (fio_str_info_s){.buf = NULL, .len = 0, .capa = 0};
  return (fio_str_info_s){.buf = (char *)uuid_data(uuid).addr,
                          .len = uuid_data(uuid).addr_len,
                          .capa = 0};
}

/**
 * Writes the local machine address (qualified host name) to the buffer.
 *
 * Returns the amount of data written (excluding the NUL byte).
 *
 * `limit` is the maximum number of bytes in the buffer, including the NUL byte.
 *
 * If the returned value == limit - 1, the result might have been truncated.
 *
 * If 0 is returned, an erro might have occured (see `errno`) and the contents
 * of `dest` is undefined.
 */
size_t fio_local_addr(char *dest, size_t limit) {
  if (gethostname(dest, limit))
    return 0;

  struct addrinfo hints, *info;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_CANONNAME;   // get cannonical name

  if (getaddrinfo(dest, "http", &hints, &info) != 0)
    return 0;

  for (struct addrinfo *pos = info; pos; pos = pos->ai_next) {
    if (pos->ai_canonname) {
      size_t len = strlen(pos->ai_canonname);
      if (len >= limit)
        len = limit - 1;
      memcpy(dest, pos->ai_canonname, len);
      dest[len] = 0;
      freeaddrinfo(info);
      return len;
    }
  }

  freeaddrinfo(info);
  return 0;
}
/* *****************************************************************************
Connection Read / Write Hooks, for overriding the system calls
***************************************************************************** */

static ssize_t
fio_hooks_default_read(intptr_t uuid, void *udata, void *buf, size_t count) {
  return read(fio_uuid2fd(uuid), buf, count);
  (void)(udata);
}
static ssize_t fio_hooks_default_write(intptr_t uuid,
                                       void *udata,
                                       const void *buf,
                                       size_t count) {
  return write(fio_uuid2fd(uuid), buf, count);
  (void)(udata);
}

static ssize_t fio_hooks_default_before_close(intptr_t uuid, void *udata) {
  return 0;
  (void)udata;
  (void)uuid;
}

static ssize_t fio_hooks_default_flush(intptr_t uuid, void *udata) {
  return 0;
  (void)(uuid);
  (void)(udata);
}

static void fio_hooks_default_cleanup(void *udata) { (void)(udata); }

const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS = {
    .read = fio_hooks_default_read,
    .write = fio_hooks_default_write,
    .flush = fio_hooks_default_flush,
    .before_close = fio_hooks_default_before_close,
    .cleanup = fio_hooks_default_cleanup,
};

FIO_IFUNC void fio_rw_hook_validate(fio_rw_hook_s *rw_hooks) {
  if (!rw_hooks->read)
    rw_hooks->read = fio_hooks_default_read;
  if (!rw_hooks->write)
    rw_hooks->write = fio_hooks_default_write;
  if (!rw_hooks->flush)
    rw_hooks->flush = fio_hooks_default_flush;
  if (!rw_hooks->before_close)
    rw_hooks->before_close = fio_hooks_default_before_close;
  if (!rw_hooks->cleanup)
    rw_hooks->cleanup = fio_hooks_default_cleanup;
}

/**
 * Replaces an existing read/write hook with another from within a read/write
 * hook callback.
 *
 * Does NOT call any cleanup callbacks.
 *
 * Returns -1 on error, 0 on success.
 */
int fio_rw_hook_replace_unsafe(intptr_t uuid,
                               fio_rw_hook_s *rw_hooks,
                               void *udata) {
  uint8_t was_locked = 0;
  intptr_t fd = fio_uuid2fd(uuid);
  fio_rw_hook_validate(rw_hooks);
  /* protect against some fulishness... but not all of it. */
  UUID_TRYLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS, reschedule_lable, invalid_uuid);
  was_locked = 1;
reschedule_lable:
  fd_data(fd).rw_hooks = rw_hooks;
  fd_data(fd).rw_udata = udata;
  if (was_locked) {
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS);
  }
  return 0;

invalid_uuid:
  // rw_hooks->cleanup(udata);
  return -1;
}

/** Sets a socket hook state (a pointer to the struct). */
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata) {
  intptr_t fd = fio_uuid2fd(uuid);
  fio_rw_hook_validate(rw_hooks);
  fio_rw_hook_s *old_rw_hooks;
  void *old_udata;
  UUID_LOCK(uuid, FIO_UUID_LOCK_RW_HOOKS, invalid_uuid);
  old_rw_hooks = fd_data(fd).rw_hooks;
  old_udata = fd_data(fd).rw_udata;
  fd_data(fd).rw_hooks = rw_hooks;
  fd_data(fd).rw_udata = udata;
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS);
  if (old_rw_hooks && old_rw_hooks->cleanup)
    old_rw_hooks->cleanup(old_udata);
  return 0;
invalid_uuid:
  fio_unlock(&fd_data(fd).lock);
  rw_hooks->cleanup(udata);
  return -1;
}

/* *****************************************************************************
Test RW Hooks
***************************************************************************** */
#ifdef TEST

/* protocol locking tests */
static void FIO_NAME_TEST(io, rw_hooks)(void) {
  fprintf(stderr, "* testing read-write hooks.\n");
  /* TODO */
  FIO_LOG_WARNING("test missing!");
}
#endif /* TEST */

/* *****************************************************************************
Test Socket API
***************************************************************************** */
#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, sock)(void) {
  /*
   * TODO
   */
}

#endif /* TEST */
