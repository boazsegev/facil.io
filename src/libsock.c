/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/** \file
The libreact-socket is a socket wrapper, using a user level buffer, non-blocking
sockets and some helper functions.
*/

#include "libsock.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netdb.h>
#include <stdatomic.h>  // http://en.cppreference.com/w/c/atomic

#ifndef DEBUG_SOCKLIB
#define DEBUG_SOCKLIB 0
#endif

#ifndef USE_SENDFILE
#define USE_SENDFILE 0
#endif

/* *****************************************************************************
Avoid including the "libreact.h" file, the following is all we need.
*/
// struct Reactor; // in the libreact_socket.h file
#pragma weak reactor_add
int reactor_add(struct Reactor* reactor, int fd) {
  fprintf(stderr, "ERROR: Reactor library bypassed or missing!\n");
  return -1;
}
#pragma weak reactor_close
void reactor_close(struct Reactor* resactor, int fd) {}

/* *****************************************************************************
User land Buffer
*/

// packet sizes
#ifndef BUFFER_PACKET_REAL_SIZE
#define BUFFER_PACKET_REAL_SIZE \
  ((sizeof(sock_packet_s) * 1) + BUFFER_PACKET_SIZE)
#endif

// The global packet container pool
static struct {
  sock_packet_s* _Atomic pool;
  sock_packet_s* initialized;
} ContainerPool = {NULL};

// grab a packet from the pool
sock_packet_s* sock_checkout_packet(void) {
  sock_packet_s* packet = atomic_load(&ContainerPool.pool);
  while (packet) {
    if (atomic_compare_exchange_weak(&ContainerPool.pool, &packet,
                                     packet->metadata.next))
      break;
  }
  if (packet == NULL) {
    packet = malloc(BUFFER_PACKET_REAL_SIZE);
  }
  if (!packet)
    return NULL;
  *packet = (sock_packet_s){.buffer = packet + 1, .metadata.next = NULL};
  return packet;
}
// return a packet to the pool, or free it (when the pool is full).
void sock_free_packet(sock_packet_s* packet) {
  sock_packet_s* next = packet;
  while (next) {
    if (next->metadata.is_fd)
      close((int)((ssize_t)next->buffer));
    else if (next->metadata.is_file)
      fclose(next->buffer);
    else if (next->metadata.external)
      free(next->buffer);
    next = next->metadata.next;
  }
  if (ContainerPool.initialized) {
    while (packet) {
      next = packet->metadata.next;
      if (packet >= ContainerPool.initialized &&
          packet < (ContainerPool.initialized +
                    (sizeof(sock_packet_s) * BUFFER_MAX_PACKET_POOL))) {
        packet->metadata.next = atomic_load(&ContainerPool.pool);
        for (;;) {
          if (atomic_compare_exchange_weak(&ContainerPool.pool,
                                           &packet->metadata.next, packet))
            break;
        }
      } else {
        free(packet);
      }
      packet = next;
    }
  } else {
    while (packet) {
      next = packet->metadata.next;
      free(packet);
      packet = next;
    }
  }
}

/* *****************************************************************************
Socket related data and internal helpers
*/

/** a map for all the active file descriptors that were added to any reactor -
 * do NOT edit! */
static struct FDData {
  pthread_mutex_t lock;
  sock_packet_s* packet;
  struct Reactor* owner;
  sock_rw_hook_s* rw_hooks;
  uint32_t sent;
  unsigned open : 1;
  unsigned client : 1;
  unsigned close : 1;
  unsigned rsv : 5;
}* fd_map = NULL;

static size_t fdmax = 0;

void sock_clear(int fd) {
  if (!fd_map || fdmax <= fd)
    return;
  struct FDData* sfd = fd_map + fd;
  pthread_mutex_lock(&sfd->lock);
  if (sfd->packet)
    sock_free_packet(sfd->packet);
  if (sfd->rw_hooks && sfd->rw_hooks->on_clear)
    sfd->rw_hooks->on_clear(fd, sfd->rw_hooks);
  *sfd = (struct FDData){.lock = sfd->lock};
  pthread_mutex_unlock(&sfd->lock);
}

static void destroy_all_fd_data(void) {
  if (fd_map) {
    while (fdmax--) {  // include 0 in countdown
      sock_clear(fdmax);
      pthread_mutex_destroy(&fd_map[fdmax].lock);
    }
    free(fd_map);
    fd_map = NULL;
  }
  if (ContainerPool.initialized) {
    ContainerPool.pool = NULL;
    free(ContainerPool.initialized);
    ContainerPool.initialized = NULL;
  }
}

static void init_pool_data(void) {
  ContainerPool.initialized =
      calloc(BUFFER_PACKET_REAL_SIZE, BUFFER_MAX_PACKET_POOL);
  void* initaddr = ContainerPool.initialized;
  for (size_t i = 0; i < (BUFFER_MAX_PACKET_POOL - 1); i++) {
    // By filling up the packet pool, we avoid memory fragmentation (unless
    // packets are freed).
    initaddr += BUFFER_PACKET_REAL_SIZE;
    ((sock_packet_s*)initaddr)->metadata.next =
        (initaddr + BUFFER_PACKET_REAL_SIZE);
  }
  ContainerPool.pool = ContainerPool.initialized;
}

int8_t init_socklib(size_t max) {
  if (!fd_map)
    atexit(destroy_all_fd_data);
  if (ContainerPool.initialized == NULL)
    init_pool_data();
  struct FDData* n_map = calloc(sizeof(*n_map), max);
  if (!n_map)
    return -1;
  if (fd_map) {
    memcpy(n_map, fd_map, sizeof(*n_map) * fdmax);
  }
  fd_map = n_map;
  while (fdmax < max) {
    if (pthread_mutex_init(&fd_map[fdmax++].lock, NULL)) {
      perror("init_socklib Couldn't initialize a mutex!");
      fdmax--;
      exit(1);
    }
  }
  fdmax = max;
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  if (DEBUG_SOCKLIB)
    fprintf(stderr,
            "\nInitialized fd_map for %lu elements, each one %lu bytes.\n"
            "overall: %lu bytes.\n"
            "Initialized packet pool for %d elements, each one %lu bytes.\n"
            "overall: %lu bytes.\n"
            "=== Total: %lu bytes\n==========\n\n",
            max, sizeof(*fd_map), sizeof(*fd_map) * max, BUFFER_MAX_PACKET_POOL,
            BUFFER_PACKET_REAL_SIZE,
            BUFFER_PACKET_REAL_SIZE * BUFFER_MAX_PACKET_POOL,
            (BUFFER_PACKET_REAL_SIZE * BUFFER_MAX_PACKET_POOL) +
                (sizeof(*fd_map) * max));
#endif
  return 0;
}

#define require_mem(fd)                                         \
  do {                                                          \
    if ((fd) >= fdmax && init_socklib((fd))) {                  \
      perror("Couldn't allocate memory for fd_map (sock_API)"); \
      exit(1);                                                  \
    }                                                           \
  } while (0);

#define require_conn(fd)                         \
  do {                                           \
    if ((fd) >= fdmax || fd_map[(fd)].open == 0) \
      return -1;                                 \
  } while (0);

/* *****************************************************************************
Machine wide and Helper API
*/

/**
Gets the maximum number of file descriptors this process can be allowed to
access.

Returns -1 on error.
*/
ssize_t sock_max_capacity(void) {
  // get current limits
  static ssize_t flim = 0;
  if (flim)
    return flim;
#ifdef _SC_OPEN_MAX
  flim = sysconf(_SC_OPEN_MAX) - 1;
#elif defined(OPEN_MAX)
  flim = OPEN_MAX - 1;
#endif
  // try to maximize limits - collect max and set to max
  struct rlimit rlim;
  getrlimit(RLIMIT_NOFILE, &rlim);
  // printf("Meximum open files are %llu out of %llu\n", rlim.rlim_cur,
  //        rlim.rlim_max);
  rlim.rlim_cur = rlim.rlim_max;
  setrlimit(RLIMIT_NOFILE, &rlim);
  getrlimit(RLIMIT_NOFILE, &rlim);
  // printf("Meximum open files are %llu out of %llu\n", rlim.rlim_cur,
  //        rlim.rlim_max);
  // if the current limit is higher than it was, update
  if (flim < rlim.rlim_cur)
    flim = rlim.rlim_cur;
  // return what we have
  return flim;
}

/**
Sets a socket to non blocking state.
*/
inline int sock_set_non_block(int fd)  // Thanks to Bjorn Reese
{
/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
  /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
  int flags;
  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  // printf("flags initial value was %d\n", flags);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
  /* Otherwise, use the old way of doing it */
  static int flags = 1;
  return ioctl(fd, FIOBIO, &flags);
#endif
}

int sock_listen(char* address, char* port) {
  int srvfd;
  // setup the address
  struct addrinfo hints;
  struct addrinfo* servinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me
  if (getaddrinfo(address, port, &hints, &servinfo)) {
    perror("addr err");
    return -1;
  }
  // get the file descriptor
  srvfd =
      socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (srvfd <= 0) {
    perror("socket err");
    freeaddrinfo(servinfo);
    return -1;
  }
  // make sure the socket is non-blocking
  if (sock_set_non_block(srvfd) < 0) {
    perror("couldn't set socket as non blocking! ");
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
    for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      if (!bind(srvfd, p->ai_addr, p->ai_addrlen))
        bound = 1;
    }

    if (!bound) {
      perror("bind err");
      freeaddrinfo(servinfo);
      close(srvfd);
      return -1;
    }
  }
  freeaddrinfo(servinfo);
  // listen in
  if (listen(srvfd, SOMAXCONN) < 0) {
    perror("couldn't start listening");
    close(srvfd);
    return -1;
  }
  return srvfd;
}

/* *****************************************************************************
Normal (raw) socket operation API
*/

/**
`sock_accept` accepts a new socket connection from the listening socket
`server_fd`, allowing the use of `sock_` functions with this new file
descriptor.

The reactor is registered as the owner of the client socket and the client
socket is added to the reactor, when possible.
*/
int sock_accept(struct Reactor* owner, int server_fd) {
  static socklen_t cl_addrlen = 0;
  int client;
#ifdef SOCK_NONBLOCK
  client = accept4(server_fd, NULL, &cl_addrlen, SOCK_NONBLOCK);
  if (client <= 0)
    return -1;
#else
  client = accept(server_fd, NULL, &cl_addrlen);
  if (client <= 0)
    return -1;
  sock_set_non_block(client);
#endif
  // attach the new client socket to the reactor.
  if (sock_open(owner, client)) {
    close(client);
    return -1;
  }
  return client;
}

/**
`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns the new file descriptor fd. Retruns -1 on error.
*/
int sock_connect(struct Reactor* owner, char* address, char* port) {
  return -1;
}

/**
`sock_open` takes an existing file decriptor `fd` and initializes it's status as
open and available for `sock_API` calls.

If a reactor pointer `owner` is provided, the `fd` will be attached to the
reactor.

Retruns -1 on error and 0 on success.
*/
int sock_open(struct Reactor* owner, int fd) {
  require_mem(fd);
  // attach the new client socket to the reactor.
  if (owner && reactor_add(owner, fd) < 0) {
    return -1;
  }
  struct FDData* sfd = fd_map + fd;
  if (sfd->open) {  // remove old connection buffer and data.
    sock_clear(fd);
  }
  sfd->open = 1;
  sfd->owner = owner;
  return 0;
}

/**
`sock_attach` sets the reactor owner for a socket and attaches the socket to the
reactor.

Use this function when the socket was already opened with no reactor association
and it's data (buffer etc') is already initialized.

This is useful for a delayed attachment, where some more testing is required
before attaching a socket to a reactor.

Retruns -1 on error and 0 on success.
*/
int sock_attach(struct Reactor* owner, int fd) {
  require_conn(fd);
  fd_map[fd].owner = owner;
  return (reactor_add(owner, fd) < 0);
}

/** Returns the state of the socket, similar to calling `fcntl(fd, F_GETFL)`.
 * Returns -1 if the connection is closed. */
int sock_status(int fd) {
  if (fd < fdmax && fd_map[fd].open)
    return fcntl(fd, F_GETFL);
  return -1;
}

/**
`sock_read` attempts to read up to count bytes from file descriptor fd into the
buffer starting at buf.

It's behavior should conform to the native `read` implementations, except some
data might be available in the `fd`'s buffer while it is not available to be
read using sock_read (i.e., when using a transport layer, such as TLS).

Also, some internal buffering will be used in cases where the transport layer
data available is larger then the data requested.
*/
ssize_t sock_read(int fd, void* buf, size_t count) {
  require_conn(fd);
  ssize_t i_read;
  struct FDData* fd_d = fd_map + fd;
  if (fd_d->rw_hooks && fd_d->rw_hooks->read)
    i_read = fd_d->rw_hooks->read(fd, buf, count);
  else
    i_read = recv(fd, buf, count, 0);

  if (i_read > 0)
    return i_read;
  if (i_read && (errno & (EWOULDBLOCK | EAGAIN)))
    return 0;
  fd_d->close = 1;
  return -1;
}

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(sock_write_info_s options) {
  if (!options.fd || !options.buffer)
    return -1;
  require_conn(options.fd);
  if (!options.length && !options.file)
    options.length = strlen(options.buffer);
  sock_packet_s* packet = sock_checkout_packet();
  if (!packet) {
    if (options.file)
      fclose((void*)options.buffer);
    return -1;
  }
  packet->metadata.can_interrupt = 1;
  packet->metadata.urgent = options.urgent;
  if (options.file || options.is_fd) {
    packet->buffer = (void*)options.buffer;
    packet->length = options.length;
    packet->metadata.is_file = options.file;
    packet->metadata.is_fd = options.is_fd;
    return sock_send_packet(options.fd, packet);
  } else {
    if (options.move) {
      packet->buffer = (void*)options.buffer;
      packet->length = options.length;
      packet->metadata.external = 1;
      return sock_send_packet(options.fd, packet);
    } else {
      if (options.length <= BUFFER_PACKET_SIZE) {
        memcpy(packet->buffer, options.buffer, options.length);
        packet->length = options.length;
        return sock_send_packet(options.fd, packet);
      } else {
        sock_packet_s* last_packet = packet;
        size_t counter = 0;
        while (options.length > BUFFER_PACKET_SIZE) {
          memcpy(last_packet->buffer,
                 options.buffer + (counter * BUFFER_PACKET_SIZE),
                 BUFFER_PACKET_SIZE);
          last_packet->length = BUFFER_PACKET_SIZE;
          options.length -= BUFFER_PACKET_SIZE;
          ++counter;
          last_packet->metadata.next = sock_checkout_packet();
          if (last_packet->metadata.next) {
            last_packet = last_packet->metadata.next;
            continue;
          } else {
            sock_free_packet(packet);
            return -1;
          }
        }
        memcpy(last_packet->buffer,
               options.buffer + (counter * BUFFER_PACKET_SIZE), options.length);
        last_packet->length = options.length;
        return sock_send_packet(options.fd, packet);
      }
    }
  }
  // how did we get here?
  return -1;
}

/* Helper function for speed optimizations */
inline static ssize_t sock_flush_in_lock(int fd) {
  struct FDData* sfd = fd_map + fd;
  sock_packet_s* packet;

  ssize_t sent;
re_flush:
  sent = 0;
  packet = sfd->packet;
  if (!packet) {
    if (sfd->close)
      return -1;
    return 0;
  }
  packet->metadata.can_interrupt = 0;

  if (USE_SENDFILE) {
    if (packet->metadata.is_fd)
      ;
    else if (packet->metadata.is_file)
      ;
  } else if (packet->metadata.is_fd || packet->metadata.is_file) {
    // how much data are we expecting to send...?
    ssize_t i_exp = (BUFFER_FILE_READ_SIZE > packet->length)
                        ? packet->length
                        : BUFFER_FILE_READ_SIZE;

    // flag telling us if the file was read into the internal buffer
    if (packet->metadata.internal_flag == 0) {
      ssize_t i_read;
      if (packet->metadata.is_fd)
        i_read = read((int)((ssize_t)packet->buffer), packet + 1, i_exp);
      else
        i_read = fread(packet + 1, 1, i_exp, packet->buffer);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      fprintf(stderr, "(%d) Read %ld from a file packet, preparing to send.\n",
              fd, i_read);
#endif

      if (i_read <= 0) {
        sfd->packet = packet->metadata.next;
        packet->metadata.next = NULL;
        sock_free_packet(packet);
        goto re_flush;
      } else {
        packet->metadata.internal_flag = 1;
      }
    }

    // send the data
    if (sfd->rw_hooks && sfd->rw_hooks->write)
      sent = sfd->rw_hooks->write(fd, (((void*)(packet + 1)) + sfd->sent),
                                  i_exp - sfd->sent);
    else
      sent = write(fd, (((void*)(packet + 1)) + sfd->sent), i_exp - sfd->sent);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
    fprintf(stderr, "(%d) Sent file %ld / %ld bytes from %lu total.\n", fd,
            sent, i_exp, packet->length);
#endif
    // review result and update packet data
    if (sent == 0) {
      return 0;  // nothing to do.
    } else if (sent > 0) {
      // we finished sending the amount of data we wanted to send.
      if (sfd->sent + sent >= i_exp) {
        packet->metadata.internal_flag = 0;
        sfd->sent = 0;
        packet->length -= i_exp;
        if (packet->length == 0) {
          sfd->packet = packet->metadata.next;
          packet->metadata.next = NULL;
          sock_free_packet(packet);
        }
      }
      return sent;
    } else if (errno & (EAGAIN | EWOULDBLOCK))
      return 0;
    else
      return -1;
  }
  // send data packets
  if (sfd->rw_hooks && sfd->rw_hooks->write)
    sent = sfd->rw_hooks->write(fd, packet->buffer + sfd->sent,
                                packet->length - sfd->sent);
  else
    sent = write(fd, packet->buffer + sfd->sent, packet->length - sfd->sent);

#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "(%d) Sent data %ld / %ld bytes from %lu total.\n", fd, sent,
          packet->length - sfd->sent, packet->length);
#endif
  // handle results
  if (sent > 0) {
    sfd->sent += sent;
    if (sfd->sent >= packet->length) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      fprintf(stderr, "(%d) rotating data buffer packet.\n", fd);
#endif
      sfd->packet = packet->metadata.next;
      packet->metadata.next = NULL;
      sock_free_packet(packet);
      sfd->sent = 0;
      if (sfd->packet == NULL && sfd->close)
        return -1;
    }
    return sent;
  } else if (sent == 0) {
    return 0;  // nothing to do.
  } else if (errno & (EAGAIN | EWOULDBLOCK)) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
    fprintf(stderr, "(%d) Sending data error -1 is okay (avoid blocking).\n",
            fd);
#endif
    return 0;
  } else
    return -1;
  return sent;
}

/**
Attches a packet to a socket's output buffer and calls `sock_flush` for the
socket.

The ownership of the packet's memory and it's resources is transferred to the
`sock_API` for automatic management. i.e., if an error occurs, the packet's
memory will be automatically released (or returned to the pool).

Returns -1 on error. Returns 0 on success. **Always** retains ownership of the
packet's memory and it's resources.
*/
ssize_t sock_send_packet(int fd, sock_packet_s* packet) {
  if (fd >= fdmax || !fd_map[(fd)].open || packet->length == 0) {
    sock_free_packet(packet);
    return -1;
  }
  struct FDData* sfd = fd_map + fd;
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  if (packet->metadata.is_fd || packet->metadata.is_file)
    fprintf(stderr, "(%d) sending a file packet with length %lu\n", fd,
            packet->length);
  else
    fprintf(stderr,
            "(%d) sending packet with length %lu\n"
            "(first char == %c(%d), last char == %c(%d))\n"
            "%.*s\n============\n",
            fd, packet->length, ((char*)packet->buffer)[0],
            ((char*)packet->buffer)[0],
            ((char*)packet->buffer)[packet->length - 1],
            ((char*)packet->buffer)[packet->length - 1], (int)packet->length,
            (char*)packet->buffer);
#endif

  // applys the move logic for either urgent or non urgent packets
  pthread_mutex_lock(&fd_map[(fd)].lock);
  sock_packet_s *tail, **pos = &(fd_map[fd].packet);
  if (packet->metadata.urgent) {
    while (*pos && (!(*pos)->metadata.next ||
                    !(*pos)->metadata.next->metadata.can_interrupt)) {
      pos = &((*pos)->metadata.next);
    }
  } else {
    while (*pos) {
      pos = &((*pos)->metadata.next);
    }
  }
  tail = (*pos);
  *pos = packet;
  if (tail) {
    pos = &(packet->metadata.next);
    while (*pos)
      pos = &((*pos)->metadata.next);
    *pos = tail;
  }
  ssize_t sent = sock_flush_in_lock(fd);
  pthread_mutex_unlock(&sfd->lock);
  if (sent < 0 && sfd->open)
    goto close_socket;
  return 0;
close_socket:
  if (sfd->owner)
    reactor_close(sfd->owner, fd);
  else
    close(fd);
  sock_clear(fd);
  return 0;
}

/**
`sock_flush` writes the data in the internal buffer to the file descriptor fd
and closes the fd once it's marked for closure (and all the data was sent).

The number of bytes actually written to the fd will be returned. 0 will be
returned if no data was written and -1 will be returned if an error occured or
if the connection was closed.

The connection is closed automatically if an error occured (and if open).
*/
ssize_t sock_flush(int fd) {
  if (fd >= fdmax || fd_map[fd].open == 0)
    return -1;
  struct FDData* sfd = fd_map + fd;
  if (sfd->packet == NULL) {
    if (sfd->close)
      goto close_socket;
    return 0;
  }
  ssize_t sent;
  pthread_mutex_lock(&sfd->lock);
  sent = sock_flush_in_lock(fd);
  pthread_mutex_unlock(&sfd->lock);
  if (sent < 0 && sfd->open)
    goto close_socket;
  return sent;
close_socket:
  if (sfd->owner)
    reactor_close(sfd->owner, fd);
  else
    close(fd);
  sock_clear(fd);
  return -1;
}
/**
`sock_close` marks the connection for disconnection once all the data was
sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.

If a reactor pointer is provied, the reactor API will be used and the
`on_close`
callback should be called.
*/
void sock_close(struct Reactor* reactor, int fd) {
  require_mem(fd);
  struct FDData* sfd = fd_map + fd;
  if (!sfd->open)
    return;
  sfd->owner = reactor;
  sfd->close = 1;
  sock_flush(fd);
}
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.

If a reactor pointer is provied, the reactor API will be used and the
`on_close`
callback should be called.
*/
void sock_force_close(struct Reactor* reactor, int fd) {
  require_mem(fd);
  struct FDData* sfd = fd_map + fd;
  if (!sfd->open)
    return;
  if (!reactor)
    reactor = sfd->owner;
  if (reactor)
    reactor_close(reactor, fd);
  else {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  sock_clear(fd);
}

/* *****************************************************************************
RW Hooks implementation
*/

/** Gets a socket RW hooks. */
struct sock_rw_hook_s* sock_rw_hook_get(int fd) {
  if (fd >= fdmax || fd_map[fd].open == 0)
    return NULL;
  return fd_map[fd].rw_hooks;
}

/** Sets a socket RW hook. */
int sock_rw_hook_set(int fd, struct sock_rw_hook_s* rw_hook) {
  if (fd >= fdmax || fd_map[fd].open == 0)
    return -1;
  fd_map[fd].rw_hooks = rw_hook;
  return 0;
}
