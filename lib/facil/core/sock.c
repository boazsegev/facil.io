/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "sock.h"
#include "spnlock.inc"
/* *****************************************************************************
Includes and state
***************************************************************************** */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/un.h>

#include "fio_mem.h"

/* *****************************************************************************
OS Sendfile settings.
*/

#ifndef USE_SENDFILE

#if defined(__linux__) /* linux sendfile works  */
#include <sys/sendfile.h>
#define USE_SENDFILE 1
#elif defined(__unix__) /* BSD sendfile should work, but isn't tested */
#include <sys/uio.h>
#define USE_SENDFILE 1
#elif defined(__APPLE__) /* Is the apple sendfile still broken? */
#include <sys/uio.h>
#define USE_SENDFILE 1
#else /* sendfile might not be available - always set to 0 */
#define USE_SENDFILE 0
#endif

#endif

/* *****************************************************************************
Support an on_close callback.
*/

#pragma weak sock_on_close
void __attribute__((weak)) sock_on_close(intptr_t uuid) { (void)(uuid); }

/* *****************************************************************************
Support timeout setting.
*/
#pragma weak sock_touch
void __attribute__((weak)) sock_touch(intptr_t uuid) { (void)(uuid); }

/* *****************************************************************************
Support `defer``.
*/

#pragma weak defer
int defer(void (*func)(void *, void *), void *arg, void *arg2) {
  func(arg, arg2);
  return 0;
}

#pragma weak sock_flush_defer
void sock_flush_defer(void *arg, void *ignored) {
  sock_flush((intptr_t)arg);
  return;
  (void)ignored;
}

/* *****************************************************************************
User-Land Buffer and Packets
***************************************************************************** */

#ifndef BUFFER_PACKET_POOL
/* ~4 pages of memory */
#define BUFFER_PACKET_POOL (((4096 << 2) - 16) / sizeof(packet_s))
#endif

typedef struct packet_s {
  struct packet_s *next;
  int (*write_func)(int fd, struct packet_s *packet);
  union {
    void (*free_func)(void *);
    void (*close_func)(intptr_t);
  };
  union {
    void *buffer;
    intptr_t fd;
  };
  intptr_t offset;
  uintptr_t length;
} packet_s;

static struct {
  packet_s *next;
  spn_lock_i lock;
  uint8_t init;
  packet_s mem[BUFFER_PACKET_POOL];
} packet_pool;

void SOCK_DEALLOC_NOOP(void *arg) { (void)arg; }

typedef struct func_s { void (*task)(void *); } func_s;

static void sock_packet_free_cb(void *task, void *buffer) {
  func_s *t = (void *)&task;
  t->task(buffer);
}

static void sock_packet_free_attempt(void *packet_, void *ignr) {
  if (spn_trylock(&packet_pool.lock)) {
    defer(sock_packet_free_attempt, packet_, ignr);
    return;
  }
  packet_s *packet = packet_;
  packet->next = packet_pool.next;
  packet_pool.next = packet;
  spn_unlock(&packet_pool.lock);
}

static inline void sock_packet_free(packet_s *packet) {
  if (packet->free_func == fio_free) {
    fio_free(packet->buffer);
  } else if (packet->free_func == free) {
    free(packet->buffer);
  } else {
    defer(sock_packet_free_cb, (void *)((uintptr_t)packet->free_func),
          packet->buffer);
  }
  if (packet >= packet_pool.mem &&
      packet <= packet_pool.mem + (BUFFER_PACKET_POOL - 1)) {
    sock_packet_free_attempt(packet, NULL);
  } else
    fio_free(packet);
}

static inline packet_s *sock_packet_new(void) {
  packet_s *packet;
  if (spn_trylock(&packet_pool.lock))
    goto no_lock;
  packet = packet_pool.next;
  if (packet == NULL)
    goto none_in_pool;
  packet_pool.next = packet->next;
  spn_unlock(&packet_pool.lock);
  return packet;
none_in_pool:
  if (!packet_pool.init)
    goto init;
  spn_unlock(&packet_pool.lock);
no_lock:
  packet = fio_malloc(sizeof(*packet));
  if (!packet) {
    perror("FATAL ERROR: memory allocation failed");
    exit(errno);
  }
  return packet;
init:
  packet_pool.init = 1;
  for (size_t i = 2; i < BUFFER_PACKET_POOL; i++) {
    packet_pool.mem[i - 1].next = packet_pool.mem + i;
  }
  packet_pool.next = packet_pool.mem + 1;
  spn_unlock(&packet_pool.lock);
  packet = packet_pool.mem;
  return packet;
}

/* *****************************************************************************
Default Socket Read/Write Hook
***************************************************************************** */

static ssize_t sock_default_hooks_read(intptr_t uuid, void *udata, void *buf,
                                       size_t count) {
  return read(sock_uuid2fd(uuid), buf, count);
  (void)(udata);
}
static ssize_t sock_default_hooks_write(intptr_t uuid, void *udata,
                                        const void *buf, size_t count) {
  return write(sock_uuid2fd(uuid), buf, count);
  (void)(udata);
}

static void sock_default_hooks_on_close(intptr_t fduuid,
                                        struct sock_rw_hook_s *rw_hook,
                                        void *udata) {
  (void)udata;
  (void)rw_hook;
  (void)fduuid;
}

static ssize_t sock_default_hooks_flush(intptr_t uuid, void *udata) {
  return 0;
  (void)(uuid);
  (void)(udata);
}

const sock_rw_hook_s SOCK_DEFAULT_HOOKS = {
    .read = sock_default_hooks_read,
    .write = sock_default_hooks_write,
    .flush = sock_default_hooks_flush,
    .on_close = sock_default_hooks_on_close,
};

/* *****************************************************************************
Socket Data Structures
***************************************************************************** */
struct fd_data_s {
  /** Connection counter - collision protection. */
  uint8_t counter;
  /** Connection lock */
  spn_lock_i lock;
  /** Connection is open */
  unsigned open : 1;
  /** indicated that the connection should be closed. */
  unsigned close : 1;
  /** future flags. */
  unsigned rsv : 5;
  /** the currently active packet to be sent. */
  packet_s *packet;
  /** the last packet in the queue. */
  packet_s **packet_last;
  /** The number of pending packets that are in the queue. */
  size_t packet_count;
  /** RW hooks. */
  sock_rw_hook_s *rw_hooks;
  /** RW udata. */
  void *rw_udata;
  /** Peer/listenning address. */
  struct sockaddr_in6 addrinfo;
  /** address length. */
  socklen_t addrlen;
};

static struct sock_data_store_s {
  size_t capacity;
  struct fd_data_s *fds;
} sock_data_store;

#define fd2uuid(fd)                                                            \
  (((uintptr_t)(fd) << 8) | (sock_data_store.fds[(fd)].counter & 0xFF))
#define fdinfo(fd) sock_data_store.fds[(fd)]
#define uuidinfo(fd) sock_data_store.fds[sock_uuid2fd((fd))]

#define lock_fd(fd) spn_lock(&sock_data_store.fds[(fd)].lock)
#define unlock_fd(fd) spn_unlock(&sock_data_store.fds[(fd)].lock)

static inline int validate_uuid(uintptr_t uuid) {
  uintptr_t fd = (uintptr_t)sock_uuid2fd(uuid);
  if ((intptr_t)uuid == -1 || sock_data_store.capacity <= fd ||
      fdinfo(fd).counter != (uuid & 0xFF))
    return -1;
  return 0;
}

static inline void sock_packet_rotate_unsafe(uintptr_t fd) {
  packet_s *packet = fdinfo(fd).packet;
  fdinfo(fd).packet = packet->next;
  if (&packet->next == fdinfo(fd).packet_last) {
    fdinfo(fd).packet_last = &fdinfo(fd).packet;
  }
  --fdinfo(fd).packet_count;
  sock_packet_free(packet);
}

static void clear_sock_lib(void) {
  free(sock_data_store.fds);
  sock_data_store.fds = NULL;
  sock_data_store.capacity = 0;
}

static inline int initialize_sock_lib(size_t capacity) {
  static uint8_t init_exit = 0;
  if (capacity > LIB_SOCK_MAX_CAPACITY)
    capacity = LIB_SOCK_MAX_CAPACITY;
  if (sock_data_store.capacity >= capacity)
    goto finish;
  struct fd_data_s *new_collection =
      realloc(sock_data_store.fds, sizeof(*new_collection) * capacity);
  if (!new_collection)
    return -1;
  sock_data_store.fds = new_collection;
  for (size_t i = sock_data_store.capacity; i < capacity; i++) {
    fdinfo(i) = (struct fd_data_s){
        .open = 0,
        .lock = SPN_LOCK_INIT,
        .rw_hooks = (sock_rw_hook_s *)&SOCK_DEFAULT_HOOKS,
        .packet_last = &fdinfo(i).packet,
        .counter = 0,
    };
  }
  sock_data_store.capacity = capacity;

#ifdef DEBUG
  fprintf(stderr,
          "\nInitialized libsock for %lu sockets, "
          "each one requires %lu bytes.\n"
          "overall ovearhead: %lu bytes.\n"
          "Initialized packet pool for %lu elements, "
          "each one %lu bytes.\n"
          "overall buffer ovearhead: %lu bytes.\n"
          "=== Socket Library Total: %lu bytes ===\n\n",
          capacity, sizeof(struct fd_data_s),
          sizeof(struct fd_data_s) * capacity, BUFFER_PACKET_POOL,
          sizeof(packet_s), sizeof(packet_s) * BUFFER_PACKET_POOL,
          (sizeof(packet_s) * BUFFER_PACKET_POOL) +
              (sizeof(struct fd_data_s) * capacity));
#endif

finish:
  packet_pool.lock = SPN_LOCK_INIT;
  for (size_t i = 0; i < sock_data_store.capacity; ++i) {
    sock_data_store.fds[i].lock = SPN_LOCK_INIT;
  }
  if (init_exit)
    return 0;
  init_exit = 1;
  atexit(clear_sock_lib);
  return 0;
}

static inline int clear_fd(uintptr_t fd, uint8_t is_open) {
  if (sock_data_store.capacity <= fd)
    goto reinitialize;
  packet_s *packet;
clear:
  spn_lock(&(fdinfo(fd).lock));
  struct fd_data_s old_data = fdinfo(fd);
  sock_data_store.fds[fd] = (struct fd_data_s){
      .open = is_open,
      .lock = fdinfo(fd).lock,
      .rw_hooks = (sock_rw_hook_s *)&SOCK_DEFAULT_HOOKS,
      .counter = fdinfo(fd).counter + 1,
      .packet_last = &sock_data_store.fds[fd].packet,
  };
  spn_unlock(&(fdinfo(fd).lock));
  while (old_data.packet) {
    packet = old_data.packet;
    old_data.packet = old_data.packet->next;
    sock_packet_free(packet);
  }
  old_data.rw_hooks->on_close(((fd << 8) | old_data.counter), old_data.rw_hooks,
                              old_data.rw_udata);
  if (old_data.open) {
    sock_on_close((fd << 8) | old_data.counter);
  }
  return 0;
reinitialize:
  if (fd >= LIB_SOCK_MAX_CAPACITY) {
    close(fd);
    return -1;
  }
  if (initialize_sock_lib(fd << 1))
    return -1;
  goto clear;
}

/* *****************************************************************************
Writing - from memory
***************************************************************************** */

static int sock_write_buffer(int fd, struct packet_s *packet) {
  int written = fdinfo(fd).rw_hooks->write(
      fd2uuid(fd), fdinfo(fd).rw_udata,
      ((uint8_t *)packet->buffer + packet->offset), packet->length);
  if (written > 0) {
    packet->length -= written;
    packet->offset += written;
    if (!packet->length)
      sock_packet_rotate_unsafe(fd);
  }
  return written;
}

/* *****************************************************************************
Writing - from files
***************************************************************************** */

#ifndef BUFFER_FILE_READ_SIZE
#define BUFFER_FILE_READ_SIZE 16384
#endif

static void sock_perform_close_fd(intptr_t fd) { close(fd); }
static void sock_perform_close_pfd(void *pfd) {
  close(*(int *)pfd);
  free(pfd);
}

static int sock_write_from_fd(int fd, struct packet_s *packet) {
  ssize_t asked = 0;
  ssize_t sent = 0;
  ssize_t total = 0;
  char buff[BUFFER_FILE_READ_SIZE];
  do {
    packet->offset += sent;
    packet->length -= sent;
  retry:
    asked =
        (packet->length < BUFFER_FILE_READ_SIZE)
            ? pread(packet->fd, buff, packet->length, packet->offset)
            : pread(packet->fd, buff, BUFFER_FILE_READ_SIZE, packet->offset);
    if (asked <= 0)
      goto read_error;
    sent = fdinfo(fd).rw_hooks->write(fd2uuid(fd), fdinfo(fd).rw_udata, buff,
                                      asked);
  } while (sent == asked && packet->length);
  if (sent >= 0) {
    packet->offset += sent;
    packet->length -= sent;
    total += sent;
    if (!packet->length) {
      sock_packet_rotate_unsafe(fd);
      return 1;
    }
  }
  return total;

read_error:
  if (sent == 0) {
    sock_packet_rotate_unsafe(fd);
    return 1;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    goto retry;
  return -1;
}

#if USE_SENDFILE && defined(__linux__) /* linux sendfile API */

static int sock_sendfile_from_fd(int fd, struct packet_s *packet) {
  ssize_t sent;
  sent = sendfile64(fd, packet->fd, &packet->offset, packet->length);
  if (sent < 0)
    return -1;
  packet->length -= sent;
  if (!packet->length)
    sock_packet_rotate_unsafe(fd);
  return sent;
}

#elif USE_SENDFILE &&                                                          \
    (defined(__APPLE__) || defined(__unix__)) /* BSD / Apple API */

static int sock_sendfile_from_fd(int fd, struct packet_s *packet) {
  off_t act_sent = 0;
  ssize_t ret = 0;
  while (packet->length) {
    act_sent = packet->length;
#if defined(__APPLE__)
    ret = sendfile(packet->fd, fd, packet->offset, &act_sent, NULL, 0);
#else
    ret = sendfile(packet->fd, fd, packet->offset, (size_t)act_sent, NULL,
                   &act_sent, 0);
#endif
    if (ret < 0)
      goto error;
    packet->length -= act_sent;
    packet->offset += act_sent;
  }
  sock_packet_rotate_unsafe(fd);
  return act_sent;
error:
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    packet->length -= act_sent;
    packet->offset += act_sent;
  }
  return -1;
}

// static int sock_sendfile_from_fd(int fd, struct packet_s *packet) {
//   struct sock_packet_file_data_s *ext = (void *)packet->buffer.buf;
//   off_t act_sent = 0;
//   ssize_t count = 0;
//   do {
//     fdinfo(fd).sent += act_sent;
//     packet->buffer.len -= act_sent;
//     act_sent = packet->buffer.len;
// #if defined(__APPLE__)
//     count = sendfile(ext->fd, fd, ext->offset + fdinfo(fd).sent, &act_sent,
//                      NULL, 0);
// #else
//     count = sendfile(ext->fd, fd, ext->offset + fdinfo(fd).sent,
//                      (size_t)act_sent, NULL, &act_sent, 0);
// #endif
//   } while (count >= 0 && packet->buffer.len > (size_t)act_sent);
//   if (!act_sent) {
//     fprintf(stderr, "Rotating after sent == %lu and length == %lu\n",
//             (size_t)act_sent, packet->buffer.len);
//     sock_packet_rotate_unsafe(fd);
//   }
//   if (count < 0)
//     return -1;
//   return act_sent;
// }

#else
static int (*sock_sendfile_from_fd)(int fd, struct packet_s *packet) =
    sock_write_from_fd;

#endif

static int sock_sendfile_from_pfd(int fd, struct packet_s *packet) {
  int ret;
  struct packet_s tmp = *packet;
  tmp.fd = ((intptr_t *)tmp.buffer)[0];
  ret = sock_sendfile_from_fd(fd, &tmp);
  tmp.fd = packet->fd;
  *packet = tmp;
  return ret;
}

static int sock_write_from_pfd(int fd, struct packet_s *packet) {
  int ret;
  struct packet_s tmp = *packet;
  tmp.fd = ((intptr_t *)tmp.buffer)[0];
  ret = sock_write_from_fd(fd, &tmp);
  tmp.fd = packet->fd;
  *packet = tmp;
  return ret;
}

/* *****************************************************************************
The API
***************************************************************************** */

/* *****************************************************************************
Process wide and helper sock API.
*/

/** MUST be called after forking a process. */
void sock_on_fork(void) { initialize_sock_lib(0); }

/**
Sets a socket to non blocking state.

This function is called automatically for the new socket, when using
`sock_accept` or `sock_connect`.
*/
int sock_set_non_block(int fd) {
/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
  /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
  int flags;
  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  // printf("flags initial value was %d\n", flags);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#elif defined(FIONBIO)
  /* Otherwise, use the old way of doing it */
  static int flags = 1;
  return ioctl(fd, FIONBIO, &flags);
#else
#error No functions / argumnet macros for non-blocking sockets.
#endif
}

/**
Gets the maximum number of file descriptors this process can be allowed to
access (== maximum fd value + 1).

If the "soft" limit is lower then the "hard" limit, the process's limits will be
extended to the allowed "hard" limit.
*/
ssize_t sock_max_capacity(void) {
  // get current limits
  static ssize_t flim = 0;
  if (flim)
    return flim;
#ifdef _SC_OPEN_MAX
  flim = sysconf(_SC_OPEN_MAX);
#elif defined(FOPEN_MAX)
  flim = FOPEN_MAX;
#endif
  // try to maximize limits - collect max and set to max
  struct rlimit rlim = {.rlim_max = 0};
  if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
    fprintf(stderr, "WARNING: `getrlimit` failed in `sock_max_capacity`.\n");
  } else {
    // #if defined(__APPLE__) /* Apple's getrlimit is broken. */
    //     rlim.rlim_cur = rlim.rlim_max >= FOPEN_MAX ? FOPEN_MAX :
    //     rlim.rlim_max;
    // #else
    rlim.rlim_cur = rlim.rlim_max;
    // #endif

    if (rlim.rlim_cur > LIB_SOCK_MAX_CAPACITY)
      rlim.rlim_cur = LIB_SOCK_MAX_CAPACITY;

    if (!setrlimit(RLIMIT_NOFILE, &rlim))
      getrlimit(RLIMIT_NOFILE, &rlim);
    flim = rlim.rlim_cur;
  }
#if DEBUG
  fprintf(stderr,
          "libsock capacity initialization:\n"
          "*    Meximum open files %lu out of %lu\n",
          (unsigned long)flim, (unsigned long)rlim.rlim_max);
#endif
  // initialize library to maximum capacity
  initialize_sock_lib(flim);
  // return what we have
  return sock_data_store.capacity;
}

/* *****************************************************************************
The main sock API.
*/

/**
Opens a listening non-blocking socket. Return's the socket's UUID.

Returns -1 on error. Returns a valid socket (non-random) UUID.

UUIDs with values less then -1 are valid values, depending on the system's
byte-ordering.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_listen(const char *address, const char *port) {
  int srvfd;
  if (!port || *port == 0 || (port[0] == '0' && port[1] == 0)) {
    /* Unix socket */
    if (!address) {
      errno = EINVAL;
      fprintf(
          stderr,
          "ERROR: (sock) sock_listen - a Unix socket requires a valid address."
          "              or specify port for TCP/IP.\n");
      return -1;
    }
    struct sockaddr_un addr = {0};
    size_t addr_len = strlen(address);
    if (addr_len >= sizeof(addr.sun_path)) {
      errno = ENAMETOOLONG;
      return -1;
    }
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, address, addr_len + 1); /* copy the NUL byte. */
#if defined(__APPLE__)
    addr.sun_len = addr_len;
#endif
    // get the file descriptor
    srvfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srvfd == -1) {
      return -1;
    }
    if (sock_set_non_block(srvfd) == -1) {
      close(srvfd);
      return -1;
    }
    unlink(addr.sun_path);
    if (bind(srvfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      close(srvfd);
      return -1;
    }
    /* chmod for foriegn connections */
    fchmod(srvfd, 0777);

  } else {
    /* TCP/IP socket */
    // setup the address
    struct addrinfo hints = {0};
    struct addrinfo *servinfo;       // will point to the results
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    if (getaddrinfo(address, port, &hints, &servinfo)) {
      // perror("addr err");
      return -1;
    }
    // get the file descriptor
    srvfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                   servinfo->ai_protocol);
    if (srvfd <= 0) {
      // perror("socket err");
      freeaddrinfo(servinfo);
      return -1;
    }
    // make sure the socket is non-blocking
    if (sock_set_non_block(srvfd) < 0) {
      // perror("couldn't set socket as non blocking! ");
      freeaddrinfo(servinfo);
      close(srvfd);
      return -1;
    }
    // avoid the "address taken"
    {
      int optval = 1;
      setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    }
    // bind the address to the socket
    {
      int bound = 0;
      for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        if (!bind(srvfd, p->ai_addr, p->ai_addrlen))
          bound = 1;
      }

      if (!bound) {
        // perror("bind err");
        freeaddrinfo(servinfo);
        close(srvfd);
        return -1;
      }
    }
#ifdef TCP_FASTOPEN
    // support TCP Fast Open when available
    {
      int optval = 128;
      setsockopt(srvfd, servinfo->ai_protocol, TCP_FASTOPEN, &optval,
                 sizeof(optval));
    }
#endif
    freeaddrinfo(servinfo);
  }
  // listen in
  if (listen(srvfd, SOMAXCONN) < 0) {
    // perror("couldn't start listening");
    close(srvfd);
    return -1;
  }
  if (clear_fd(srvfd, 1))
    return -1;
  return fd2uuid(srvfd);
}

/**
`sock_accept` accepts a new socket connection from the listening socket
`server_fd`, allowing the use of `sock_` functions with this new file
descriptor.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_accept(intptr_t srv_uuid) {
  struct sockaddr_in6 addrinfo;
  socklen_t addrlen = sizeof(addrinfo);
  int client;
#ifdef SOCK_NONBLOCK
  client = accept4(sock_uuid2fd(srv_uuid), (struct sockaddr *)&addrinfo,
                   &addrlen, SOCK_NONBLOCK);
  if (client <= 0)
    return -1;
#else
  client =
      accept(sock_uuid2fd(srv_uuid), (struct sockaddr *)&addrinfo, &addrlen);
  if (client <= 0)
    return -1;
  sock_set_non_block(client);
#endif
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
  if (clear_fd(client, 1))
    return -1;
  fdinfo(client).addrinfo = addrinfo;
  fdinfo(client).addrlen = addrlen;
  return fd2uuid(client);
}

/**
`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

NOTICE:

This function is non-blocking, meaning that the connection probably wasn't
established by the time the function returns (this prevents the function from
hanging while waiting for a network timeout).

Use select, poll, `libreact` or other solutions to review the connection state
before attempting to write to the socket.
*/
intptr_t sock_connect(char *address, char *port) {
  int fd;
  int one = 1;
  if (!port || *port == 0 || (port[0] == '0' && port[1] == 0)) {
    /* Unix socket */
    if (!address) {
      errno = EINVAL;
      fprintf(
          stderr,
          "ERROR: (sock) sock_listen - a Unix socket requires a valid address."
          "              or specify port for TCP/IP.\n");
      return -1;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    size_t addr_len = strlen(address);
    if (addr_len >= sizeof(addr.sun_path)) {
      errno = ENAMETOOLONG;
      return -1;
    }
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, address, addr_len + 1); /* copy the NUL byte. */
#if defined(__APPLE__)
    addr.sun_len = addr_len;
#endif
    // get the file descriptor
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
      return -1;
    }
    if (sock_set_non_block(fd) == -1) {
      close(fd);
      return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 &&
        errno != EINPROGRESS) {
      close(fd);
      return -1;
    }
    if (clear_fd(fd, 1))
      return -1;
  } else {
    // setup the address
    struct addrinfo hints;
    struct addrinfo *addrinfo;       // will point to the results
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    if (getaddrinfo(address, port, &hints, &addrinfo)) {
      return -1;
    }
    // get the file descriptor
    fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
                addrinfo->ai_protocol);
    if (fd <= 0) {
      freeaddrinfo(addrinfo);
      return -1;
    }
    // make sure the socket is non-blocking
    if (sock_set_non_block(fd) < 0) {
      freeaddrinfo(addrinfo);
      close(fd);
      return -1;
    }

    for (struct addrinfo *i = addrinfo; i; i = i->ai_next) {
      if (connect(fd, i->ai_addr, i->ai_addrlen) == 0 || errno == EINPROGRESS)
        goto connection_requested;
    }
    freeaddrinfo(addrinfo);
    close(fd);
    return -1;

  connection_requested:

    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (clear_fd(fd, 1))
      return -1;
    memcpy(&fdinfo(fd).addrinfo, addrinfo->ai_addr, addrinfo->ai_addrlen);
    fdinfo(fd).addrlen = addrinfo->ai_addrlen;
    freeaddrinfo(addrinfo);
  }
  return fd2uuid(fd);
}

/**
`sock_open` takes an existing file descriptor `fd` and initializes it's status
as open and available for `sock_*` API calls, returning a valid UUID.

This will reinitialize the data (user buffer etc') for the file descriptor
provided, calling the `reactor_on_close` callback if the `fd` was previously
marked as used.

When using `libreact`, remember to call `int reactor_add(intptr_t uuid);` to
listen for events.

Returns -1 on error. Returns a valid socket (non-random) UUID.

Socket UUIDs are predictable and shouldn't be used outside the local system.
They protect against connection mixups on concurrent systems (i.e. when saving
client data for "broadcasting" or when an old client task is preparing a
response in the background while a disconnection and a new connection occur on
the same `fd`).
*/
intptr_t sock_open(int fd) {
  if (clear_fd(fd, 1))
    return -1;
  return fd2uuid(fd);
}

/**
 * `sock_hijack` is the reverse of the `sock_open` function, removing the
 * connection from the `sock` library and clearing it's data without closing it
 * (`sock_on_close` will NOT be called).
 *
 * Returns the original `fd` for the socket. On error returns -1.
 */
int sock_hijack(intptr_t uuid) {
  const int fd = sock_uuid2fd(uuid);
  if (validate_uuid(uuid) && fdinfo(fd).open) {
    fprintf(stderr, "WARNING: SOCK HIJACK FAILING!\n");
    return -1;
  }
  fdinfo(fd).open = 0;
  clear_fd(fd, 0);
  return fd;
}

/** Returns the information available about the socket's peer address. */
sock_peer_addr_s sock_peer_addr(intptr_t uuid) {
  if (validate_uuid(uuid) || !fdinfo(sock_uuid2fd(uuid)).addrlen)
    return (sock_peer_addr_s){.addr = NULL};
  return (sock_peer_addr_s){
      .addrlen = fdinfo(sock_uuid2fd(uuid)).addrlen,
      .addr = (struct sockaddr *)&fdinfo(sock_uuid2fd(uuid)).addrinfo,
  };
}

/**
Returns 1 if the uuid refers to a valid and open, socket.

Returns 0 if not.
*/
int sock_isvalid(intptr_t uuid) {
  return validate_uuid(uuid) == 0 && uuidinfo(uuid).open;
}

/**
Returns 1 if the uuid is invalid or the socket is flagged to be closed.

Returns 0 if the socket is valid, open and isn't flagged to be closed.
*/
int sock_isclosed(intptr_t uuid) {
  return validate_uuid(uuid) || !uuidinfo(uuid).open || uuidinfo(uuid).close;
}

/**
`sock_fd2uuid` takes an existing file decriptor `fd` and returns it's active
`uuid`.

If the file descriptor is marked as closed (wasn't opened / registered with
`libsock`) the function returns -1;

If the file descriptor was closed remotely (or not using `libsock`), a false
positive will be possible. This is not an issue, since the use of an invalid fd
will result in the registry being updated and the fd being closed.

Returns -1 on error. Returns a valid socket (non-random) UUID.
*/
intptr_t sock_fd2uuid(int fd) {
  return (fd > 0 && sock_data_store.capacity > (size_t)fd &&
          sock_data_store.fds[fd].open)
             ? (intptr_t)(fd2uuid(fd))
             : -1;
}

/**
`sock_read` attempts to read up to count bytes from the socket into the buffer
starting at buf.

On a connection error (NOT EAGAIN or EWOULDBLOCK), signal interrupt, or when the
connection was closed, `sock_read` returns -1.

The value 0 is the valid value indicating no data was read.

Data might be available in the kernel's buffer while it is not available to be
read using `sock_read` (i.e., when using a transport layer, such as TLS).
*/
ssize_t sock_read(intptr_t uuid, void *buf, size_t count) {
  if (validate_uuid(uuid) || !fdinfo(sock_uuid2fd(uuid)).open) {
    errno = EBADF;
    return -1;
  }
  lock_fd(sock_uuid2fd(uuid));
  if (!fdinfo(sock_uuid2fd(uuid)).open) {
    unlock_fd(sock_uuid2fd(uuid));
    errno = EBADF;
    return -1;
  }
  sock_rw_hook_s *rw = fdinfo(sock_uuid2fd(uuid)).rw_hooks;
  void *udata = fdinfo(sock_uuid2fd(uuid)).rw_udata;
  unlock_fd(sock_uuid2fd(uuid));
  if (count == 0)
    return rw->read(uuid, udata, buf, count);
  int old_errno = errno;
  ssize_t ret;
retry_int:
  ret = rw->read(uuid, udata, buf, count);
  if (ret > 0) {
    sock_touch(uuid);
    return ret;
  }
  if (ret < 0 && errno == EINTR)
    goto retry_int;
  if (ret < 0 &&
      (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOTCONN)) {
    errno = old_errno;
    return 0;
  }
  sock_force_close(uuid);
  return -1;
}

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(sock_write_info_s options) {
  int fd = sock_uuid2fd(options.uuid);

  /* this extra work can be avoided if an error is already known to occur...
   * but the extra complexity and branching isn't worth it, considering the
   * common case should be that there's no expected error.
   *
   * It also important to point out that errors should handle deallocation,
   * simplifying client-side error handling logic (this is a framework wide
   * design choice where callbacks are passed).
   */
  packet_s *packet = sock_packet_new();
  packet->length = options.length;
  packet->offset = options.offset;
  packet->buffer = (void *)options.buffer;
  if (options.is_fd) {
    packet->write_func = (fdinfo(fd).rw_hooks == &SOCK_DEFAULT_HOOKS)
                             ? sock_sendfile_from_fd
                             : sock_write_from_fd;
    packet->free_func =
        (options.dealloc ? options.dealloc
                         : (void (*)(void *))sock_perform_close_fd);
  } else if (options.is_pfd) {
    packet->write_func = (fdinfo(fd).rw_hooks == &SOCK_DEFAULT_HOOKS)
                             ? sock_sendfile_from_pfd
                             : sock_write_from_pfd;
    packet->free_func =
        (options.dealloc ? options.dealloc : sock_perform_close_pfd);
  } else {
    packet->write_func = sock_write_buffer;
    packet->free_func = (options.dealloc ? options.dealloc : free);
  }

  /* place packet in queue */

  if (validate_uuid(options.uuid) || !options.buffer)
    goto error;
  lock_fd(fd);
  if (!fdinfo(fd).open) {
    unlock_fd(fd);
    goto error;
  }
  packet->next = NULL;
  if (fdinfo(fd).packet == NULL) {
    fdinfo(fd).packet_last = &packet->next;
    fdinfo(fd).packet = packet;
  } else if (options.urgent == 0) {
    *fdinfo(fd).packet_last = packet;
    fdinfo(fd).packet_last = &packet->next;
  } else {
    packet_s **pos = &fdinfo(fd).packet;
    if (*pos)
      pos = &(*pos)->next;
    packet->next = *pos;
    *pos = packet;
    if (!packet->next) {
      fdinfo(fd).packet_last = &packet->next;
    }
  }
  ++fdinfo(fd).packet_count;
  unlock_fd(fd);
  sock_touch(options.uuid);
  defer(sock_flush_defer, (void *)options.uuid, NULL);
  return 0;

error:
  sock_packet_free(packet);
  errno = EBADF;
  return -1;
}
#define sock_write2(...) sock_write2_fn((sock_write_info_s){__VA_ARGS__})

/**
`sock_close` marks the connection for disconnection once all the data was sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.
*/
void sock_close(intptr_t uuid) {
  if (validate_uuid(uuid) || !fdinfo(sock_uuid2fd(uuid)).open)
    return;
  fdinfo(sock_uuid2fd(uuid)).close = 1;
  sock_flush_defer((void *)uuid, (void *)uuid);
}
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.
*/
void sock_force_close(intptr_t uuid) {
  if (validate_uuid(uuid))
    return;
  // fprintf(stderr,
  //         "INFO: (%d) `sock_force_close` called"
  //         " for %p (fd: %u) with errno %d\n",
  //         getpid(), (void *)uuid, (unsigned int)sock_uuid2fd(uuid), errno);
  // perror("errno");
  // // We might avoid shutdown, it has side-effects that aren't always clear
  // shutdown(sock_uuid2fd(uuid), SHUT_RDWR);
  close(sock_uuid2fd(uuid));
  clear_fd(sock_uuid2fd(uuid), 0);
}

/* *****************************************************************************
Direct user level buffer API.

The following API allows data to be written directly to the packet, minimizing
memory copy operations.
*/

/**
 * `sock_flush` writes the data in the internal buffer to the underlying file
 * descriptor and closes the underlying fd once it's marked for closure (and all
 * the data was sent).
 *
 * Return values: 1 will be returned if `sock_flush` should be called again. 0
 * will be returned if the socket was fully flushed. -1 will be returned on an
 * error or when the connection is closed.
 */
ssize_t sock_flush(intptr_t uuid) {
  int fd = sock_uuid2fd(uuid);
  if (validate_uuid(uuid) || !fdinfo(fd).open)
    return -1;
  ssize_t ret;
  uint8_t touch = 0;
  lock_fd(fd);
  sock_rw_hook_s *rw;
  void *rw_udata;
retry:
  rw = fdinfo(fd).rw_hooks;
  rw_udata = fdinfo(fd).rw_udata;
  unlock_fd(fd);
  while ((ret = rw->flush(uuid, rw_udata)) > 0) {
    touch = 1;
  }
  if (ret == -1) {
    if (errno == EINTR)
      goto retry;
    if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOTCONN ||
        errno == ENOSPC)
      goto finish;
    goto error;
  }
  lock_fd(fd);
  while (fdinfo(fd).packet &&
         (ret = fdinfo(fd).packet->write_func(fd, fdinfo(fd).packet)) > 0) {
    touch = 1;
  }
  if (ret == -1) {
    if (errno == EINTR)
      goto retry;
    if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOTCONN ||
        errno == ENOSPC)
      goto finish;
    goto error;
  }
  if (!touch && fdinfo(fd).close && !fdinfo(fd).packet)
    goto error;
finish:
  unlock_fd(fd);
  if (touch) {
    sock_touch(uuid);
    return 1;
  }
  return fdinfo(fd).packet != NULL || fdinfo(fd).close;
error:
  unlock_fd(fd);
  // fprintf(stderr,
  //         "ERROR: sock `flush` failed"
  //         " for %p with %d\n",
  //         (void *)uuid, errno);
  sock_force_close(uuid);
  return -1;
}

/**
`sock_flush_strong` performs the same action as `sock_flush` but returns only
after all the data was sent. This is a "busy" wait, polling isn't performed.
*/
void sock_flush_strong(intptr_t uuid) {
  errno = 0;
  while (sock_flush(uuid) == 0 && errno == 0)
    ;
}

/**
Calls `sock_flush` for each file descriptor that's buffer isn't empty.
*/
void sock_flush_all(void) {
  for (size_t fd = 0; fd < sock_data_store.capacity; fd++) {
    if (!fdinfo(fd).open || !fdinfo(fd).packet)
      continue;
    sock_flush(fd2uuid(fd));
  }
}

/**
Returns the number of `sock_write` calls that are waiting in the socket's queue
and haven't been processed.
*/
int sock_has_pending(intptr_t uuid) {
  if (validate_uuid(uuid) || !uuidinfo(uuid).open)
    return 0;
  return (int)(uuidinfo(uuid).packet_count + uuidinfo(uuid).close);
}

/**
 * Returns the number of `sock_write` calls that are waiting in the socket's
 * queue and haven't been processed.
 */
size_t sock_pending(intptr_t uuid) {
  if (validate_uuid(uuid) || !uuidinfo(uuid).open)
    return 0;
  return (uuidinfo(uuid).packet_count + uuidinfo(uuid).close);
}

/* *****************************************************************************
TLC - Transport Layer Callback.

Experimental
*/

/** Gets a socket hook state (a pointer to the struct). */
struct sock_rw_hook_s *sock_rw_hook_get(intptr_t uuid) {
  if (validate_uuid(uuid) || !uuidinfo(uuid).open ||
      ((void)(uuid = sock_uuid2fd(uuid)),
       fdinfo(uuid).rw_hooks == &SOCK_DEFAULT_HOOKS))
    return NULL;
  return fdinfo(uuid).rw_hooks;
}

/** Returns the socket's udata associated with the read/write hook. */
void *sock_rw_udata(intptr_t uuid) {
  if (validate_uuid(uuid) || !fdinfo(sock_uuid2fd(uuid)).open)
    return NULL;
  uuid = sock_uuid2fd(uuid);
  return fdinfo(uuid).rw_udata;
}

/** Sets a socket hook state (a pointer to the struct). */
int sock_rw_hook_set(intptr_t uuid, sock_rw_hook_s *rw_hooks, void *udata) {
  if (validate_uuid(uuid) || !uuidinfo(uuid).open)
    return -1;
  if (!rw_hooks->read)
    rw_hooks->read = sock_default_hooks_read;
  if (!rw_hooks->write)
    rw_hooks->write = sock_default_hooks_write;
  if (!rw_hooks->flush)
    rw_hooks->flush = sock_default_hooks_flush;
  if (!rw_hooks->on_close)
    rw_hooks->on_close = sock_default_hooks_on_close;
  uuid = sock_uuid2fd(uuid);
  lock_fd(uuid);
  fdinfo(uuid).rw_hooks = rw_hooks;
  fdinfo(uuid).rw_udata = udata;
  unlock_fd(uuid);
  return 0;
}

/* *****************************************************************************
test
*/
#ifdef DEBUG
void sock_libtest(void) {
  if (0) { /* this test can't be performed witout initializeing `facil`. */
    char request[] = "GET / HTTP/1.1\r\n"
                     "Host: www.google.com\r\n"
                     "\r\n";
    char buff[1024];
    ssize_t i_read;
    intptr_t uuid = sock_connect("www.google.com", "80");
    if (uuid == -1) {
      perror("sock_connect failed");
      exit(1);
    }
    if (sock_write(uuid, request, sizeof(request) - 1) < 0)
      perror("sock_write error ");

    while ((i_read = sock_read(uuid, buff, 1024)) >= 0) {
      if (i_read == 0) { // could be we hadn't finished connecting yet.
        sock_flush(uuid);
        reschedule_thread();
      } else {
        fprintf(stderr, "\n%.*s\n\n", (int)i_read, buff);
        break;
      }
    }
    if (i_read < 0)
      perror("Error with sock_read ");
    fprintf(stderr, "done.\n");
    sock_close(uuid);
  }
  sock_max_capacity();
  for (int i = 0; i < 4; ++i) {
    packet_s *packet = sock_packet_new();
    sock_packet_free(packet);
  }
  packet_s *head, *pos;
  pos = head = packet_pool.next;
  size_t count = 0;
  while (pos) {
    count++;
    pos = pos->next;
  }
  fprintf(stderr, "Packet pool test %s (%lu =? %lu)\n",
          count == BUFFER_PACKET_POOL ? "PASS" : "FAIL",
          (unsigned long)BUFFER_PACKET_POOL, (unsigned long)count);
  printf("Allocated sock capacity %lu X %lu\n",
         (unsigned long)sock_data_store.capacity,
         (unsigned long)sizeof(struct fd_data_s));
}
#endif
