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
#include <sys/uio.h>

#ifndef DEBUG_SOCKLIB
#define DEBUG_SOCKLIB 0
#endif

#ifndef USE_SENDFILE

#if defined(__linux__) /* linux sendfile works, but isn't implemented  */
#include <sys/sendfile.h>
#define USE_SENDFILE 1
#elif defined(__unix__) /* BSD sendfile should work, but isn't implemented */
#define USE_SENDFILE 0
#elif defined(__APPLE__) /* Apple sendfile was broken and probably still is */
#define USE_SENDFILE 0
#else /* sendfile might not be available - always set to 0 */
#define USE_SENDFILE 0
#endif

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
  return flim + 1;
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
Library Global Variables and Data Structure
*/

typedef struct fd_info_s fd_info_s;

struct fd_info_s {
  pthread_mutex_t lock;
  sock_packet_s* packet;
  struct Reactor* owner;
  sock_rw_hook_s* rw_hooks;
  uint32_t sent;
  unsigned open : 1;
  unsigned client : 1;
  unsigned close : 1;
  unsigned rsv : 5;
};

fd_info_s* fd_info = NULL;
size_t fd_capacity = 0;

struct {
  sock_packet_s* _Atomic pool;
  sock_packet_s* allocated;
} buffer_pool;

#define validate_mem(fd) ((fd) >= fd_capacity)
#define validate_connection(fd) (validate_mem(fd) || fd_info[(fd)].open == 0)

/* *****************************************************************************
User land Buffer
*/

// packet sizes
#ifndef BUFFER_PACKET_REAL_SIZE
#define BUFFER_PACKET_REAL_SIZE \
  ((sizeof(sock_packet_s) * 1) + BUFFER_PACKET_SIZE)
#endif

/**
Checks out a `sock_packet_s` from the packet pool, transfering the
ownership
of the memory to the calling function. returns NULL if the pool was empty and
memory allocation had failed.

Every checked out buffer packet comes with an attached buffer of
BUFFER_PACKET_SIZE bytes. This buffer is accessible using the `packet->buffer`
pointer (which can be safely overwritten to point to an external buffer).

This attached buffer is safely and automatically freed or returned to the memory
pool once `sock_send_packet` or `sock_free_packet` are called.
*/
sock_packet_s* sock_checkout_packet(void) {
  sock_packet_s* packet = atomic_load(&buffer_pool.pool);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "Packet being checked out (pool = %p).\n", packet);
#endif
  while (packet &&
         atomic_compare_exchange_weak(&buffer_pool.pool, &packet,
                                      packet->metadata.next) == 0)
    ;
#if defined(BUFFER_ALLOW_MALLOC) && BUFFER_ALLOW_MALLOC == 1
  if (packet == NULL)
    packet = malloc(BUFFER_PACKET_REAL_SIZE);
#endif
  if (packet) {
    *packet = (sock_packet_s){
        .buffer = packet + 1,
    };
  }
  return packet;
}

/**
Use `sock_free_packet` to free unused packets that were checked-out using
`sock_checkout_packet`.

NEVER use `free`, for any packet checked out using the pool management function
`sock_checkout_packet`.
*/
void sock_free_packet(sock_packet_s* packet) {
  sock_packet_s* next = packet;
  if (next) {
    for (;;) {
      if (next->metadata.is_fd) {
        if (next->metadata.keep_open == 0)
          close((int)((ssize_t)next->buffer));
      } else if (next->metadata.external)
        free(next->buffer);
      if (next->metadata.next == NULL)
        break;
      next = next->metadata.next;
    }
  }
#if defined(BUFFER_ALLOW_MALLOC) && BUFFER_ALLOW_MALLOC == 1
  while (packet) {
    next = packet->metadata.next;
    if (packet >= buffer_pool.allocated &&
        packet < (buffer_pool.allocated +
                  (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL))) {
      packet->metadata.next = atomic_load(&buffer_pool.pool);
      while (!atomic_compare_exchange_weak(&buffer_pool.pool,
                                           &packet->metadata.next, packet))
        ;
    } else {
      free(packet);
    }
    packet = next;
  }
#else
  next->metadata.next = atomic_load(&buffer_pool.pool);
  while (!atomic_compare_exchange_weak(&buffer_pool.pool, &next->metadata.next,
                                       packet))
    ;
#endif
}

/* *****************************************************************************
Socket State
*/

/**
Clears a socket state data and buffer.
*/
void sock_clear(int fd) {
  if (validate_mem(fd))
    return;
  fd_info_s* sfd = fd_info + fd;
  pthread_mutex_lock(&sfd->lock);

  if (sfd->packet) {
    sock_free_packet(sfd->packet);
  }
  if (sfd->rw_hooks && sfd->rw_hooks->on_clear)
    sfd->rw_hooks->on_clear(fd, sfd->rw_hooks);
  *sfd = (fd_info_s){.lock = sfd->lock};

  pthread_mutex_unlock(&sfd->lock);
}

/* *****************************************************************************
Library Initialization
*/

#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
/** For development use, no deep testing */
void libsock_test(void);
#endif

static void destroy_lib_data(void) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "\nDestroying all libsock data\n");
#endif
  if (fd_info) {
    while (fd_capacity--) {  // include 0 in countdown
      sock_clear(fd_capacity);
      pthread_mutex_destroy(&fd_info[fd_capacity].lock);
    }
    free(fd_info);
    fd_info = NULL;
  }
  if (buffer_pool.allocated) {
    buffer_pool.pool = NULL;
    free(buffer_pool.allocated);
    buffer_pool.allocated = NULL;
  }
}

static void single_time_init(void) {
  if (buffer_pool.allocated)
    return;
  void* buff_mem = buffer_pool.allocated =
      calloc(BUFFER_PACKET_REAL_SIZE, BUFFER_PACKET_POOL);
  for (size_t i = 0; i < BUFFER_PACKET_POOL - 1; i++) {
    ((sock_packet_s*)buff_mem)->metadata.next =
        buff_mem + BUFFER_PACKET_REAL_SIZE;
    buff_mem += BUFFER_PACKET_REAL_SIZE;
  }
  atomic_store(&buffer_pool.pool, buffer_pool.allocated);
  atexit(destroy_lib_data);
}

int8_t init_socklib(size_t capacity) {
  if (capacity <= fd_capacity)
    return 0;
  single_time_init();
  fd_info_s* new_data = calloc(sizeof(fd_info_s), capacity);
  if (!new_data)
    return -1;
  if (fd_info) {
    memcpy(new_data, fd_info, sizeof(fd_info_s) * fd_capacity);
    free(fd_info);
  }
  fd_info = new_data;
  for (size_t i = fd_capacity; i < capacity; i++) {
    pthread_mutex_init(&fd_info[i].lock, NULL);
  }
  fd_capacity = capacity;
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  libsock_test();
  fprintf(stderr,
          "\nInitialized fd_info for %lu elements, each one %lu bytes.\n"
          "overall: %lu bytes.\n"
          "Initialized packet pool for %d elements, each one %lu bytes.\n"
          "overall: %lu bytes.\n"
          "=== Total: %lu bytes\n==========\n\n",
          fd_capacity, sizeof(*fd_info), sizeof(*fd_info) * fd_capacity,
          BUFFER_PACKET_POOL, BUFFER_PACKET_REAL_SIZE,
          BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL,
          (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL) +
              (sizeof(*fd_info) * fd_capacity));
#endif
  return 0;
}

/* *****************************************************************************
Core socket operation API
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
  if (validate_mem(fd))
    return -1;
  // attach the new client socket to the reactor.
  if (owner && reactor_add(owner, fd) < 0) {
    return -1;
  }
  fd_info_s* sfd = fd_info + fd;
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
  if (validate_connection(fd))
    return -1;
  fd_info[fd].owner = owner;
  return (reactor_add(owner, fd) < 0);
}

/** Returns the state of the socket, similar to calling `fcntl(fd, F_GETFL)`.
 * Returns -1 if the connection is closed. */
int sock_status(int fd) {
  if (validate_connection(fd))
    return -1;
  return fcntl(fd, F_GETFL);
}

/* *****************************************************************************
Socket Buffer Management - not MT safe
*/

/* flushes but doesn't close the socket. flags fd for close on error. */
inline static void sock_flush_unsafe(int fd) {
  fd_info_s* sfd = fd_info + fd;
  sock_packet_s* packet;
  ssize_t sent;
  for (;;) {
    if ((packet = sfd->packet) == NULL)
      return;
    packet->metadata.can_interrupt = 0;

    if (packet->metadata.is_fd) {
/* File (fd) Packets */
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      fprintf(stderr,
              "(%d) flushing a file packet with %u/%lu (%lu). start at: %lld\n",
              fd, sfd->sent, packet->length, packet->length - sfd->sent,
              packet->metadata.offset);
#endif

      /* USE_SENDFILE ? */
      if (USE_SENDFILE && sfd->rw_hooks == NULL) {
#if defined(__linux__) /* linux sendfile API */
        sent = sendfile64(fd, (int)((ssize_t)packet->buffer),
                          &packet->metadata.offset, packet->length - sfd->sent);

        if (sent == 0) {
          sfd->packet = packet->metadata.next;
          packet->metadata.next = NULL;
          sock_free_packet(packet);
          sfd->sent = 0;
        } else if (sent < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
          } else if (errno == EINTR) {
            continue;
          } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
            perror("Linux sendfile failed");
#endif
            sfd->close = 1;
            sock_free_packet(sfd->packet);
            sfd->packet = NULL;
            return;
          }
        }
        sfd->sent += sent;
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
        fprintf(stderr, "(%d) Linux sendfile %ld / %u bytes from %lu total.\n",
                fd, sent, sfd->sent, packet->length);
#endif
        if (sfd->sent >= packet->length) {
          sfd->packet = packet->metadata.next;
          packet->metadata.next = NULL;
          sock_free_packet(packet);
          sfd->sent = 0;
          continue;
        }
#elif defined(__APPLE__) || defined(__unix__) /* BSD / Apple API */
        off_t act_sent = packet->length - sfd->sent;
#if defined(__APPLE__)
        if (sendfile((int)((ssize_t)packet->buffer), fd,
                     packet->metadata.offset, &act_sent, NULL, 0) < 0 &&
            act_sent == 0) {
#else
        if (sendfile((int)((ssize_t)packet->buffer), fd,
                     packet->metadata.offset, (size_t)act_sent, NULL, &act_sent,
                     0) < 0 &&
            act_sent == 0) {
#endif
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
          perror("Apple/BSD sendfile errno");
#endif
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
          } else if (errno == EINTR) {
            continue;
          } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
            perror("Apple/BSD sendfile failed");
#endif
            sfd->close = 1;
            sock_free_packet(sfd->packet);
            sfd->packet = NULL;
            return;
          }
        }
        if (act_sent == 0) {
          sfd->packet = packet->metadata.next;
          packet->metadata.next = NULL;
          sock_free_packet(packet);
          sfd->sent = 0;
          continue;
        }
        packet->metadata.offset += act_sent;
        sfd->sent += act_sent;
        if (sfd->sent >= packet->length) {
          sfd->packet = packet->metadata.next;
          packet->metadata.next = NULL;
          sock_free_packet(packet);
          sfd->sent = 0;
          continue;
        }

#else /* sendfile might not be available - always set to 0 */
      };
      if (0) {
#endif
      } else {
        // how much data are we expecting to send...?
        ssize_t i_exp = (BUFFER_FILE_READ_SIZE > packet->length)
                            ? packet->length
                            : BUFFER_FILE_READ_SIZE;

        // flag telling us if the file was read into the internal buffer
        if (packet->metadata.internal_flag == 0) {
          ssize_t i_read;
          i_read = pread((int)((ssize_t)packet->buffer), packet + 1, i_exp,
                         packet->metadata.offset);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
          fprintf(stderr,
                  "(%d) Read %ld from a file packet, preparing to send.\n", fd,
                  i_read);
#endif
          if (i_read <= 0) {
            sfd->packet = packet->metadata.next;
            packet->metadata.next = NULL;
            sock_free_packet(packet);
            sfd->sent = 0;
            continue;
          } else {
            packet->metadata.offset += i_read;
            packet->metadata.internal_flag = 1;
          }
        }

        // send the data
        if (sfd->rw_hooks && sfd->rw_hooks->write)
          sent = sfd->rw_hooks->write(fd, (((void*)(packet + 1)) + sfd->sent),
                                      i_exp - sfd->sent);
        else
          sent =
              write(fd, (((void*)(packet + 1)) + sfd->sent), i_exp - sfd->sent);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
        fprintf(stderr, "(%d) Sent file %ld / %ld bytes from %lu total.\n", fd,
                sent, i_exp, packet->length);
#endif

        // review result and update packet data
        if (sent == 0) {
          return;  // nothing to do?
        } else if (sent > 0) {
          // did we finished sending the amount of data we wanted to send?
          sfd->sent += sent;
          if (sfd->sent >= i_exp) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
            fprintf(stderr, "(%d) rotating file buffer (marking for read).\n",
                    fd);
#endif
            packet->metadata.internal_flag = 0;
            sfd->sent = 0;
            packet->length -= i_exp;
            if (packet->length == 0) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
              fprintf(stderr, "(%d) Finished sending file.\n", fd);
#endif
              sfd->packet = packet->metadata.next;
              packet->metadata.next = NULL;
              sock_free_packet(packet);
              continue;
            }
          }
          // return 0;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return;
        } else if (errno == EINTR) {
          continue;
        } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
          perror("send failed");
#endif
          sfd->close = 1;
          sock_free_packet(sfd->packet);
          sfd->packet = NULL;
          return;
        }
      }
    } else {
      /* Data Packets */

      /* send data */
      if (sfd->rw_hooks && sfd->rw_hooks->write)
        sent = sfd->rw_hooks->write(fd, packet->buffer + sfd->sent,
                                    packet->length - sfd->sent);
      else
        sent =
            write(fd, packet->buffer + sfd->sent, packet->length - sfd->sent);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      fprintf(stderr, "(%d) Sent data %ld / %ld bytes from %lu total.\n", fd,
              sent, packet->length - sfd->sent, packet->length);
#endif

      /* review and update sent state */
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
        }
        continue;
      } else if (sent == 0) {
        return;  // nothing to do?
      } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      } else if (errno == EINTR) {
        continue;
      } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
        perror("send failed");
#endif
        sfd->close = 1;
        sock_free_packet(sfd->packet);
        sfd->packet = NULL;
        return;
      }
    }
  }
}

/* places a packet in the socket's buffer */
inline static ssize_t sock_send_packet_unsafe(int fd, sock_packet_s* packet) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  if (packet->metadata.is_fd)
    fprintf(stderr, "(%d) sending a file packet with length %u/%lu\n", fd,
            fd_info[fd].sent, packet->length);
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

  fd_info_s* sfd = fd_info + fd;
  if (sfd->packet == NULL) {
    sfd->packet = packet;
    sock_flush_unsafe(fd);
    return 0;

  } else if (packet->metadata.urgent == 0) {
    sock_packet_s* pos = sfd->packet;
    while (pos->metadata.next)
      pos = pos->metadata.next;
    pos->metadata.next = packet;
    sock_flush_unsafe(fd);
    return 0;

  } else {
    sock_packet_s* pos = sfd->packet;
    if (pos->metadata.can_interrupt) {
      sfd->packet = packet;
      while (packet->metadata.next)
        packet = packet->metadata.next;
      packet->metadata.next = pos;
    } else {
      while (pos->metadata.next &&
             pos->metadata.next->metadata.can_interrupt == 0)
        pos = pos->metadata.next;
      sock_packet_s* tail = pos->metadata.next;
      pos->metadata.next = packet;
      if (tail) {
        while (packet->metadata.next)
          packet = packet->metadata.next;
        packet->metadata.next = tail;
      }
    }
    sock_flush_unsafe(fd);
    return 0;
  }
  return -1;
}

/* *****************************************************************************
Read/Write socket operation API
*/

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
  if (validate_connection(fd))
    return -1;
  ssize_t i_read;
  fd_info_s* sfd = fd_info + fd;
  if (sfd->rw_hooks && sfd->rw_hooks->read)
    i_read = sfd->rw_hooks->read(fd, buf, count);
  else
    i_read = recv(fd, buf, count, 0);

  if (i_read > 0)
    return i_read;
  if (i_read && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR))
    return 0;
  sfd->close = 1;
  return -1;
}

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(sock_write_info_s options) {
  if (!options.fd || !options.buffer)
    return -1;
  if (validate_connection(options.fd))
    return -1;
  if (!options.length && !options.is_fd)
    options.length = strlen(options.buffer);
  if (options.length == 0)
    return -1;

  sock_packet_s* packet = sock_checkout_packet();
  while (packet == NULL) {
    sock_flush_all();
    packet = sock_checkout_packet();
  }
  packet->metadata.can_interrupt = 1;
  packet->metadata.urgent = options.urgent;
  if (options.is_fd) {
    packet->buffer = (void*)options.buffer;
    packet->length = options.length;
    packet->metadata.is_fd = options.is_fd;
    packet->metadata.offset = options.offset;
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
        pthread_mutex_lock(&fd_info[options.fd].lock);
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
            packet->metadata.can_interrupt = ~options.urgent;
            if (sock_send_packet_unsafe(options.fd, packet))
              goto multi_send_error;
            while (packet == NULL) {
              sock_flush_all();
              packet = sock_checkout_packet();
            }
            packet->metadata.urgent = options.urgent;
            last_packet = packet;
          }
        }
        memcpy(last_packet->buffer,
               options.buffer + (counter * BUFFER_PACKET_SIZE), options.length);
        last_packet->length = options.length;
        if (sock_send_packet_unsafe(options.fd, packet))
          goto multi_send_error;
        pthread_mutex_unlock(&fd_info[options.fd].lock);
        return 0;
      multi_send_error:
        pthread_mutex_unlock(&fd_info[options.fd].lock);
        return -1;
      }
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
ssize_t sock_send_packet(int fd, sock_packet_s* packet) {
  if (validate_connection(fd) || packet->length == 0) {
    sock_free_packet(packet);
    return -1;
  }
  fd_info_s* sfd = fd_info + fd;
  pthread_mutex_lock(&sfd->lock);
  sock_send_packet_unsafe(fd, packet);
  pthread_mutex_unlock(&sfd->lock);
  if (sfd->packet == NULL && sfd->close)
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
  if (validate_connection(fd)) {
    sock_clear(fd);
    return -1;
  }
  fd_info_s* sfd = fd_info + fd;
  if (sfd->packet == NULL) {
    if (sfd->close) {
      goto close_socket;
    }
    return 0;
  }
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "\n=========\n(%d) sock_flush called.\n", fd);
#endif
  pthread_mutex_lock(&sfd->lock);
  sock_flush_unsafe(fd);
  pthread_mutex_unlock(&sfd->lock);
  if (sfd->packet == NULL && sfd->close)
    goto close_socket;
  return 0;
close_socket:
  if (sfd->owner)
    reactor_close(sfd->owner, fd);
  else
    close(fd);
  sock_clear(fd);
  return -1;
}
/**
`sock_flush_strong` performs the same action as `sock_flush` but returns only
after all the data was sent. This is an "active" wait, polling isn't performed.
*/
void sock_flush_strong(int fd) {
  fd_info_s* sfd = fd_info + fd;
  while (validate_connection(fd) == 0 && sock_flush(fd) == 0 && sfd->packet)
    sched_yield();
  return;
}

/**
Calls `sock_flush` for each file descriptor that's buffer isn't empty.
*/
void sock_flush_all(void) {
  register fd_info_s* fds = fd_info;
  for (size_t i = 0; i < fd_capacity; i++) {
    if (fds[i].packet == NULL)
      continue;
    sock_flush(i);
  }
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
  if (validate_connection(fd))
    return;
  fd_info[fd].owner = reactor;
  fd_info[fd].close = 1;
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
  if (validate_connection(fd))
    return;
  if (!reactor)
    reactor = fd_info[fd].owner;
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
  if (validate_connection(fd))
    return NULL;
  return fd_info[fd].rw_hooks;
}

/** Sets a socket RW hook. */
int sock_rw_hook_set(int fd, struct sock_rw_hook_s* rw_hook) {
  if (validate_connection(fd))
    return -1;
  fd_info[fd].rw_hooks = rw_hook;
  return 0;
}

/* *****************************************************************************
Minor tests
*/

#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
void libsock_test(void) {
  sock_packet_s *p, *pl;
  fprintf(stderr, "Testing packet pool ");
  for (size_t i = 0; i < BUFFER_PACKET_POOL * 4; i++) {
    pl = p = sock_checkout_packet();
    while (buffer_pool.pool) {
      pl->metadata.next = sock_checkout_packet();
      pl = pl->metadata.next;
    }
    sock_free_packet(p);
  }
  size_t count = 1;
  pl = p = sock_checkout_packet();
  while (buffer_pool.pool) {
    ++count;
    pl->metadata.next = sock_checkout_packet();
    pl = pl->metadata.next;
  }
  sock_free_packet(p);
  fprintf(stderr, " - %s (%lu/%d)\n",
          (count == BUFFER_PACKET_POOL) ? "Pass" : "Fail", count,
          BUFFER_PACKET_POOL);
}
#endif
