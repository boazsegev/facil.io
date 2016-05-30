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
TLC helpers
*/
inline static int send_packet_to_tlc(int fd, struct Packet* packet);

inline static int read_through_tlc(int fd,
                                   void* buffer,
                                   ssize_t* length,
                                   const size_t max_len);

/* *****************************************************************************
User land Buffer
*/

// packet sizes
#ifndef BUFFER_MAX_PACKET_POOL
#define BUFFER_MAX_PACKET_POOL 127
#endif

#ifndef DEBUG_SOCKLIB
#define DEBUG_SOCKLIB 1
#endif

// The global packet container pool
static struct {
  int pool_count;
  struct Packet* pool;
} ContainerPool = {0};

// the packet pool mutex
static pthread_mutex_t container_pool_locker = PTHREAD_MUTEX_INITIALIZER;

// grab a packet from the pool
struct Packet* sock_checkout_packet(void) {
  struct Packet* packet;
  pthread_mutex_lock(&container_pool_locker);
  packet = ContainerPool.pool;
  if (packet) {
    ContainerPool.pool = packet->metadata.next;
    ContainerPool.pool_count--;
    if (ContainerPool.pool_count < 0)  // just in case...?
      ContainerPool.pool_count = 0;
  } else {
    packet = malloc(sizeof(struct Packet));
  }
  pthread_mutex_unlock(&container_pool_locker);
  if (!packet)
    return NULL;
  packet->buffer = packet->internal_memory;
  packet->length = 0;
  packet->metadata = (struct PacketMetadata){};
  return packet;
}
// return a packet to the pool, or free it (when the pool is full).
void sock_free_packet(struct Packet* packet) {
  struct Packet* next = packet;
  while (next) {
    if (next->metadata.is_file)
      fclose(next->buffer);
    else if (next->metadata.external)
      free(next->buffer);
    next = next->metadata.next;
  }
  pthread_mutex_lock(&container_pool_locker);
  while (packet) {
    next = packet->metadata.next;
    if (ContainerPool.pool_count <= BUFFER_MAX_PACKET_POOL) {
      packet->metadata.next = ContainerPool.pool;
      ContainerPool.pool = packet;
      ContainerPool.pool_count++;
    } else
      free(packet);
    packet = next;
  }
  pthread_mutex_unlock(&container_pool_locker);
}

// unregister a buffer in the pool
static void destroy_packet_pool(void) {
  pthread_mutex_lock(&container_pool_locker);
  struct Packet* to_free;
  while ((to_free = ContainerPool.pool)) {
    ContainerPool.pool = to_free->metadata.next;
    free(to_free);
  }
  pthread_mutex_unlock(&container_pool_locker);
}

/* *****************************************************************************
Socket related data and internal helpers
*/

/** a map for all the active file descriptors that were added to any reactor -
 * do NOT edit! */
static struct FDData {
  pthread_mutex_t lock;
  struct Packet* packet;
  struct Reactor* owner;
  struct sockTLC* tlc;
  uint16_t sent;
  unsigned open : 1;
  unsigned client : 1;
  unsigned close : 1;
  unsigned rsv : 5;
}* fd_map = NULL;

static size_t fdmax = 0;

void sock_clear(int fd) {
  if (!fd_map || fdmax <= fd)
    return;
  pthread_mutex_lock(&fd_map[(fd)].lock);
  if (fd_map[fd].packet)
    sock_free_packet(fd_map[fd].packet);
  while (fd_map[fd].tlc) {
    if (fd_map[fd].tlc->on_clear)
      fd_map[fd].tlc->on_clear(fd, fd_map[fd].tlc);
    fd_map[fd].tlc = fd_map[fd].tlc->next;
  }
  fd_map[fd] = (struct FDData){.lock = fd_map[fd].lock};
  pthread_mutex_unlock(&fd_map[(fd)].lock);
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
  destroy_packet_pool();
}

int8_t init_socklib(size_t max) {
  if (!fd_map)
    atexit(destroy_all_fd_data);
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
  fprintf(stderr,
          "Initialized fd_map for %lu elements, each one %lu bytes, overall: "
          "%lu bytes.\n",
          max, sizeof(*n_map), sizeof(*n_map) * max);
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
  if (fd_map[fd].open) {  // remove old connection buffer and data.
    sock_clear(fd);
  }
  fd_map[fd].open = 1;
  fd_map[fd].owner = owner;
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
  if (((i_read = recv(fd, buf, count, 0)) > 0) &&
      read_through_tlc(fd, buf, &i_read, count) == 0)
    // return read count
    return i_read;
  if (i_read && (errno & (EWOULDBLOCK | EAGAIN)))
    return 0;
  fd_map[fd].close = 1;
  return -1;
}

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(struct SockWriteOpt options) {
  if (!options.fd || !options.buffer)
    return -1;
  require_conn(options.fd);
  if (!options.length && !options.file)
    options.length = strlen(options.buffer);
  struct Packet* packet = sock_checkout_packet();
  if (!packet)
    return -1;
  packet->metadata.can_interrupt = 1;
  packet->metadata.urgent = options.urgent;
  if (options.file) {
    packet->buffer = (void*)options.buffer;
    packet->metadata.is_file = 1;
    return sock_send_packet(options.fd, packet);
  } else {
    if (options.move) {
      packet->buffer = (void*)options.buffer;
      packet->length = options.length;
      packet->metadata.external = 1;
      return sock_send_packet(options.fd, packet);
    } else {
      struct Packet* last_packet = packet;
      while (options.length > BUFFER_PACKET_SIZE) {
        memcpy(last_packet->buffer, options.buffer, BUFFER_PACKET_SIZE);
        last_packet->length = BUFFER_PACKET_SIZE;
        options.length -= BUFFER_PACKET_SIZE;
        last_packet->metadata.next = sock_checkout_packet();
        if (last_packet->metadata.next) {
          last_packet = last_packet->metadata.next;
          continue;
        } else {
          sock_free_packet(packet);
          return -1;
        }
      }
      memcpy(last_packet->buffer, options.buffer, options.length);
      last_packet->length = options.length;
      return sock_send_packet(options.fd, packet);
    }
  }
  // how did we get here?
  return -1;
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
ssize_t sock_send_packet(int fd, struct Packet* packet) {
  if (fd >= fdmax || !fd_map[(fd)].open || send_packet_to_tlc(fd, packet)) {
    sock_free_packet(packet);
    return -1;
  }
  // applys the move logic for either urgent or non urgent packets
  pthread_mutex_lock(&fd_map[(fd)].lock);
  struct Packet *tail, **pos = &(fd_map[fd].packet);
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
  pthread_mutex_unlock(&fd_map[(fd)].lock);
  sock_flush(fd);  // TODO: avoid unlock-lock sequence.
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
  if (fd_map[fd].packet == NULL) {
    if (fd_map[fd].close)
      goto close_socket;
    return 0;
  }
  ssize_t sent = 0;
  pthread_mutex_lock(
      &fd_map[(fd)].lock);  // TODO: move into different function.
re_flush:
  if (!fd_map[fd].packet)
    goto finish;  // what if the socket was flushed before we got here?
  fd_map[fd].packet->metadata.can_interrupt = 0;
  if (fd_map[fd].packet->metadata.is_file) {
    struct Packet* packet = sock_checkout_packet();
    if (!packet)
      goto error;
    packet->length = fread(packet->buffer, 1, BUFFER_FILE_READ_SIZE,
                           fd_map[fd].packet->buffer);
    if (packet->length == BUFFER_FILE_READ_SIZE) {
      if (send_packet_to_tlc(fd, packet))
        goto close_unlock;
      packet->metadata.next = fd_map[fd].packet;
      fd_map[fd].packet = packet;
    } else {
      if (packet->length) {
        if (send_packet_to_tlc(fd, packet))
          goto close_unlock;
        packet->metadata.next = fd_map[fd].packet->metadata.next;
        fd_map[fd].packet->metadata.next = NULL;
        sock_free_packet(fd_map[fd].packet);
        fd_map[fd].packet = packet;
      } else {
        packet->metadata.next = fd_map[fd].packet;
        fd_map[fd].packet = fd_map[fd].packet->metadata.next;
        sock_free_packet(packet);
      }
    }
    if (fd_map[fd].packet)
      goto re_flush;
    else
      goto finish;
  }
  sent = write(fd, fd_map[fd].packet->buffer + fd_map[fd].sent,
               fd_map[fd].packet->length - fd_map[fd].sent);
  if (sent == 0) {
    ;
  } else if (sent > 0) {
    fd_map[fd].sent += sent;
    if (fd_map[fd].sent >= fd_map[fd].packet->length) {
      struct Packet* packet = fd_map[fd].packet;
      fd_map[fd].packet = fd_map[fd].packet->metadata.next;
      packet->metadata.next = NULL;
      sock_free_packet(packet);
      fd_map[fd].sent = 0;
      if (fd_map[fd].packet == NULL && fd_map[fd].close)
        goto close_unlock;
    }
  } else if (errno & (EAGAIN | EWOULDBLOCK))
    sent = 0;
  else if (fd_map[fd].open == 0)
    goto error;
  else
    goto close_unlock;

finish:
  pthread_mutex_unlock(&fd_map[(fd)].lock);
  return sent;
error:
  pthread_mutex_unlock(&fd_map[(fd)].lock);
  return -1;
close_unlock:
  pthread_mutex_unlock(&fd_map[(fd)].lock);
close_socket:
  if (fd_map[fd].owner)
    reactor_close(fd_map[fd].owner, fd);
  else
    close(fd);
  sock_clear(fd);
  return -1;
}
/**
`sock_close` marks the connection for disconnection once all the data was sent.
The actual disconnection will be managed by the `sock_flush` function.

`sock_flash` will automatically be called.

If a reactor pointer is provied, the reactor API will be used and the `on_close`
callback should be called.
*/
void sock_close(struct Reactor* reactor, int fd) {
  require_mem(fd);
  if (!fd_map[fd].open)
    return;
  fd_map[fd].owner = reactor;
  fd_map[fd].close = 1;
  sock_flush(fd);
}
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.

If a reactor pointer is provied, the reactor API will be used and the `on_close`
callback should be called.
*/
void sock_force_close(struct Reactor* reactor, int fd) {
  require_mem(fd);
  if (!fd_map[fd].open)
    return;
  if (!reactor)
    reactor = fd_map[fd].owner;
  if (reactor)
    reactor_close(reactor, fd);
  else
    close(fd);
  sock_clear(fd);
}

/* *****************************************************************************
TLC implementation (TODO)
*/

/** Gets a socket TLC chain. */
struct sockTLC* sock_tlc_get(int fd) {
  if (fd >= fdmax || fd_map[fd].open == 0)
    return NULL;
  return fd_map[fd].tlc;
}

int sock_tlc_set(int fd, struct sockTLC* tlc) {
  require_conn(fd);
  pthread_mutex_lock(&fd_map[(fd)].lock);
  fd_map[fd].tlc = tlc;
  pthread_mutex_unlock(&fd_map[(fd)].lock);
  return 0;
}

int sock_tlc_add(int fd, struct sockTLC* tlc) {
  require_conn(fd);
  pthread_mutex_lock(&fd_map[(fd)].lock);
  tlc->next = fd_map[fd].tlc;
  fd_map[fd].tlc = tlc;
  pthread_mutex_unlock(&fd_map[(fd)].lock);
  return 0;
}

inline static int send_packet_to_tlc(int fd, struct Packet* packet) {
  struct sockTLC* tlc = fd_map[fd].tlc;
  while (tlc) {
    if (tlc->wrap && tlc->wrap(fd, packet, tlc->udata))
      return -1;
    tlc = tlc->next;
  }
  return 0;
}

inline static int read_through_tlc(int fd,
                                   void* buffer,
                                   ssize_t* length,
                                   const size_t max_len) {
  if (fd_map[fd].tlc == NULL)
    return 0;
  ssize_t tlc_count = 0;
  struct sockTLC* tlc = fd_map[fd].tlc;
  while (tlc) {
    ++tlc_count;
    tlc = tlc->next;
  }
  struct sockTLC* tlc_rev[tlc_count];
  tlc = fd_map[fd].tlc;
  for (size_t i = tlc_count; i;) {
    tlc_rev[--i] = tlc;
    tlc = tlc->next;
  }
  while (tlc_count--) {
    if (tlc_rev[tlc_count]->unwrap &&
        tlc_rev[tlc_count]->unwrap(fd, buffer, length, max_len,
                                   tlc_rev[tlc_count]->udata))
      return -1;
  }

  return 0;
}
