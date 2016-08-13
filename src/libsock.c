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
#include <sys/mman.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/uio.h>

#ifndef DEBUG_SOCKLIB
#define DEBUG_SOCKLIB 0
#endif

#ifndef NOTIFY_ON_CLOSE
#define NOTIFY_ON_CLOSE 0
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

#ifndef BUFFER_ALLOW_MALLOC
/**
Setting the `BUFFER_ALLOW_MALLOC` to 1 will allod dynamic buffer packet
allocation.

In some cases (not all), this might improve performance or prevent buffer packet
starvation (depends how you use the library).

*/
#define BUFFER_ALLOW_MALLOC 0
#endif

/**
Socket `write` / `flush` / `read` contension uses a spinlock instead of a mutex.
*/
#ifndef FD_USE_SPIN_LOCK
#define FD_USE_SPIN_LOCK 1
#endif

/* *****************************************************************************
Support `libreact` on_close callback, if exist.
*/

#pragma weak reactor_on_close
void reactor_on_close(intptr_t uuid) {}
#pragma weak reactor_remove
int reactor_remove(intptr_t uuid) {
  return -1;
}

/* *****************************************************************************
Support timeout setting.
*/
#pragma weak sock_touch
void sock_touch(intptr_t uuid) {}

/* *****************************************************************************
A Simple busy lock implementation ... (spnlock.h) ... copied for portability
*/
#ifndef _SPN_LOCK_H
#define _SPN_LOCK_H

/* allow of the unused flag */
#ifndef __unused
#define __unused __attribute__((unused))
#endif

#include <stdlib.h>
#include <stdint.h>

/*********
 * manage the way threads "wait" for the lock to release
 */
#if (defined(__unix__) || defined(__APPLE__) || defined(__linux__)) && \
    defined(__has_include) && __has_include(<time.h>)
/* nanosleep seems to be the most effective and efficient reschedule */
#include <time.h>
#define reschedule_thread()                           \
  {                                                   \
    static const struct timespec tm = {.tv_nsec = 1}; \
    nanosleep(&tm, NULL);                             \
  }

#else /* no effective rescheduling, just spin... */
#define reschedule_thread()

#endif
/* end `reschedule_thread` block*/

/*********
 * The spin lock core functions (spn_trylock, spn_unlock, is_spn_locked)
 */

/* prefer C11 standard implementation where available (trust the system) */
#if defined(__has_include) && __has_include(<stdatomic.h>)
#include <stdatomic.h>
typedef atomic_bool spn_lock_i;
#define SPN_LOCK_INIT ATOMIC_VAR_INIT(0)
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i* lock) {
  __asm__ volatile("" ::: "memory");
  return atomic_exchange(lock, 1);
}
/** Releases a lock. */
__unused static inline void spn_unlock(spn_lock_i* lock) {
  atomic_store(lock, 0);
  __asm__ volatile("" ::: "memory");
}
/** returns a lock's state (non 0 == Busy). */
__unused static inline int is_spn_locked(spn_lock_i* lock) {
  return atomic_load(lock);
}
#else

/* Test for compiler builtins */

/* use clang builtins if available - trust the compiler */
#if defined(__clang__)
#if defined(__has_builtin) && __has_builtin(__sync_swap)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i* lock) {
  return __sync_swap(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif
/* use gcc builtins if available - trust the compiler */
#elif defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i* lock) {
  return __sync_fetch_and_or(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif

/* Check if compiler builtins were available, if not, try assembly*/
#if SPN_TMP_HAS_BUILTIN
#undef SPN_TMP_HAS_BUILTIN

/* use Intel's asm if on Intel - trust Intel's documentation */
#elif defined(__amd64__) || defined(__x86_64__) || defined(__x86__) || \
    defined(__i386__) || defined(__ia64__) || defined(_M_IA64) ||      \
    defined(__itanium__) || defined(__i386__)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i* lock) {
  spn_lock_i tmp;
  __asm__ volatile("xchgb %0,%1" : "=r"(tmp), "=m"(*lock) : "0"(1) : "memory");
  return tmp;
}

/* use SPARC's asm if on SPARC - trust the design */
#elif defined(__sparc__) || defined(__sparc)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns TRUE (non-zero) if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i* lock) {
  spn_lock_i tmp;
  __asm__ volatile("ldstub    [%1], %0" : "=r"(tmp) : "r"(lock) : "memory");
  return tmp; /* return 0xFF if the lock was busy, 0 if free */
}

#else
/* I don't know how to provide green thread safety on PowerPC or ARM */
#error "Couldn't implement a spinlock for this system / compiler"
#endif /* types and atomic exchange */
/** Initialization value in `free` state. */
#define SPN_LOCK_INIT 0

/** Releases a lock. */
__unused static inline void spn_unlock(spn_lock_i* lock) {
  __asm__ volatile("" ::: "memory");
  *lock = 0;
}
/** returns a lock's state (non 0 == Busy). */
__unused static inline int is_spn_locked(spn_lock_i* lock) {
  __asm__ volatile("" ::: "memory");
  return *lock;
}

#endif /* has atomics */
#include <stdio.h>
/** Busy waits for the lock. */
__unused static inline void spn_lock(spn_lock_i* lock) {
  while (spn_trylock(lock)) {
    reschedule_thread();
  }
}

/* *****************************************************************************
spnlock.h finished
*/
#endif

/* *****************************************************************************
Library Core Data
*/

typedef struct {
#if !defined(FD_USE_SPIN_LOCK) || FD_USE_SPIN_LOCK != 1
  pthread_mutex_t lock;
#endif
  sock_packet_s* packet;
  sock_rw_hook_s* rw_hooks;
  fduuid_u fduuid;
  uint32_t sent;
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
  spn_lock_i lock;
#endif
  unsigned open : 1;
  unsigned close : 1;
  unsigned rsv : 6;
} fd_info_s;

static fd_info_s* fd_info = NULL;
static size_t fd_capacity = 0;

static struct {
#if !defined(FD_USE_SPIN_LOCK) || FD_USE_SPIN_LOCK != 1
  pthread_mutex_t lock;
#endif
  sock_packet_s* pool;
  sock_packet_s* allocated;
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
  spn_lock_i lock;
#endif
} buffer_pool;

#define validate_mem() (fd_info == NULL)
#define validate_connection(fd_uuid)                          \
  (validate_mem() || sock_uuid2fd(fd_uuid) < 0 ||             \
   fd_info[sock_uuid2fd(fd_uuid)].fduuid.uuid != (fd_uuid) || \
   fd_info[sock_uuid2fd(fd_uuid)].open == 0)

#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
#define lock_fd(ffd) spn_lock(&((ffd)->lock))
#define unlock_fd(ffd) spn_unlock(&((ffd)->lock))
#define lock_pool() spn_lock(&buffer_pool.lock)
#define unlock_pool() spn_unlock(&buffer_pool.lock)

#else
#define lock_fd(ffd) pthread_mutex_lock(&(ffd)->lock)
#define unlock_fd(ffd) pthread_mutex_unlock(&(ffd)->lock)

#define lock_pool() pthread_mutex_lock(&buffer_pool.lock)
#define unlock_pool() pthread_mutex_unlock(&buffer_pool.lock)
#endif

#define LIB_SOCK_STATE_OPEN 1
#define LIB_SOCK_STATE_CLOSED 0
/**
Sets / Updates the socket connection data.
*/
static inline void set_fd(int fd, _Bool state) {
  //
  fd_info_s old_data = fd_info[fd];
  // lock and update
  lock_fd(fd_info + fd);
  fd_info[fd] = (fd_info_s){
      .fduuid.data.counter = fd_info[fd].fduuid.data.counter + state,
      .fduuid.data.fd = fd,
      .lock = fd_info[fd].lock,
      .open = state,
  };
  // should be called within the lock.
  if (old_data.rw_hooks && old_data.rw_hooks->on_clear)
    old_data.rw_hooks->on_clear(old_data.fduuid.uuid, old_data.rw_hooks);
  // unlock
  unlock_fd(fd_info + fd);
  // clear old data
  if (old_data.packet)
    sock_free_packet(old_data.packet);
  // call callback if exists
  if (old_data.open) {
    // if (state == LIB_SOCK_STATE_OPEN)
    //   printf(
    //       "STRONG FD COLLISION PROTECTION: A new connection was accepted "
    //       "while the old one was marked as open.\n");
    reactor_remove(old_data.fduuid.uuid);
    reactor_on_close(old_data.fduuid.uuid);
  }
}

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
  flim = sysconf(_SC_OPEN_MAX);
#elif defined(OPEN_MAX)
  flim = OPEN_MAX;
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

intptr_t sock_listen(const char* address, const char* port) {
  int srvfd;
  // setup the address
  struct addrinfo hints;
  struct addrinfo* servinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me
  if (getaddrinfo(address, port, &hints, &servinfo)) {
    // perror("addr err");
    return -1;
  }
  // get the file descriptor
  srvfd =
      socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
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
    for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
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
  freeaddrinfo(servinfo);
  // listen in
  if (listen(srvfd, SOMAXCONN) < 0) {
    // perror("couldn't start listening");
    close(srvfd);
    return -1;
  }
  set_fd(srvfd, LIB_SOCK_STATE_OPEN);
  return fd_info[srvfd].fduuid.uuid;
}

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
  sock_packet_s* packet;
  lock_pool();
  packet = buffer_pool.pool;
  if (packet)
    buffer_pool.pool = packet->metadata.next;
  unlock_pool();
#if defined(BUFFER_ALLOW_MALLOC) && BUFFER_ALLOW_MALLOC == 1
  if (packet == NULL)
    packet = malloc(BUFFER_PACKET_REAL_SIZE);
#endif
  // zero out memort and set buffer location.
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
  if (packet == NULL)
    return;
  for (;;) {
    if (next->metadata.is_fd) {
      if (next->metadata.keep_open == 0)
        close((int)((ssize_t)next->buffer));
    } else if (next->metadata.external)
      free(next->buffer);
    if (next->metadata.next == NULL)
      break; /* next now holds the last packet in the chain. */
    next = next->metadata.next;
  }
#if defined(BUFFER_ALLOW_MALLOC) && BUFFER_ALLOW_MALLOC == 1
  if (packet >= buffer_pool.allocated &&
      packet < (buffer_pool.allocated +
                (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL))) {
    lock_pool();
    next->metadata.next = buffer_pool.pool;
    buffer_pool.pool = packet;
    unlock_pool();
    // #if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
    //       fprintf(stderr, "Packet checked in (pool = %p).\n", packet);
    // #endif
  } else {
    free(packet);
  }
#else
  lock_pool();
  next->metadata.next = buffer_pool.pool;
  buffer_pool.pool = packet;
  unlock_pool();
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "Packet checked in (pool = %p).\n", packet);
#endif

#endif
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
  buffer_pool.pool = NULL;
  buffer_pool.allocated = NULL;
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
// no destruction required
#else
  pthread_mutex_destroy(&buffer_pool.lock);
#endif
  if (fd_info) {
    while (fd_capacity--) {  // include 0 in countdown
      set_fd(fd_capacity, LIB_SOCK_STATE_CLOSED);
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
// no destruction required
#else
      pthread_mutex_destroy(&fd_info[fd_capacity].lock);
#endif
    }
    munmap(fd_info, (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL) +
                        (sizeof(fd_info_s) * fd_capacity));
    fd_info = NULL;
  }
}

int8_t sock_lib_init(void) {
  if (fd_capacity)
    return (fd_info == NULL) ? -1 : 0;
  fd_capacity = sock_max_capacity();
  size_t fd_map_mem_size = sizeof(fd_info_s) * fd_capacity;
  size_t buffer_mem_size = BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL;
  void* buff_mem;
  buff_mem = mmap(NULL, fd_map_mem_size + buffer_mem_size,
                  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (buff_mem == MAP_FAILED || buff_mem == NULL) {
    return -1;
  }
  // pthread_mutexattr_init(&mtx_attr);
  // pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);

  // assign memory addresses.
  fd_info = buff_mem;
  buff_mem += fd_map_mem_size;
  buffer_pool.allocated = buff_mem;
  // initialize packet buffer
  for (size_t i = 0; i < BUFFER_PACKET_POOL - 1; i++) {
    *((sock_packet_s*)buff_mem) =
        (sock_packet_s){.metadata.next = buff_mem + BUFFER_PACKET_REAL_SIZE};
    buff_mem += BUFFER_PACKET_REAL_SIZE;
  }
  buffer_pool.pool = buffer_pool.allocated;

  for (size_t i = 0; i < fd_capacity; i++) {
    fd_info[i] = (fd_info_s){0};
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
    fd_info[i].lock = SPN_LOCK_INIT;
#else
    pthread_mutex_init(&fd_info[i].lock, NULL);
#endif
    // pthread_mutex_init(&fd_info[i].lock, &mtx_attr);
  }
#if defined(FD_USE_SPIN_LOCK) && FD_USE_SPIN_LOCK == 1
// no destruction required
#else
  pthread_mutex_init(&buffer_pool.lock);
#endif
  // pthread_mutexattr_destroy(&mtx_attr);
  atexit(destroy_lib_data);
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
intptr_t sock_accept(intptr_t server_fd) {
  if (validate_mem())
    return -1;
  static socklen_t cl_addrlen = 0;
  int client;
#ifdef SOCK_NONBLOCK
  client = accept4(sock_uuid2fd(server_fd), NULL, &cl_addrlen, SOCK_NONBLOCK);
  if (client <= 0)
    return -1;
#else
  client = accept(sock_uuid2fd(server_fd), NULL, &cl_addrlen);
  if (client <= 0)
    return -1;
  sock_set_non_block(client);
#endif
  set_fd(client, LIB_SOCK_STATE_OPEN);
  return fd_info[client].fduuid.uuid;
}

/**
`sock_connect` is similar to `sock_accept` but should be used to initiate a
client connection to the address requested.

Returns the new file descriptor fd. Retruns -1 on error.
*/
intptr_t sock_connect(char* address, char* port) {
  if (validate_mem()) {
    return -1;
  }
  int fd;
  // setup the address
  struct addrinfo hints;
  struct addrinfo* addrinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me
  if (getaddrinfo(address, port, &hints, &addrinfo)) {
    return -1;
  }
  // get the file descriptor
  fd =
      socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
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

  if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0 &&
      errno != EINPROGRESS) {
    close(fd);
    freeaddrinfo(addrinfo);
    return -1;
  }
  freeaddrinfo(addrinfo);
  set_fd(fd, LIB_SOCK_STATE_OPEN);
  return fd_info[fd].fduuid.uuid;
}

/**
`sock_open` takes an existing file decriptor `fd` and initializes it's status as
open and available for `sock_API` calls.

If a reactor was initialized, the `fd` will be attached to the reactor.

Retruns -1 on error and 0 on success.
*/
intptr_t sock_open(int fd) {
  if (validate_mem())
    return -1;
  // update local data
  set_fd(fd, LIB_SOCK_STATE_OPEN);
  return fd_info[fd].fduuid.uuid;
}

/**
`sock_fd2uuid` takes an existing file decriptor `fd` and returns it's active
`uuid`.

If the file descriptor is marked as closed (wasn't opened / registered with
`libsock`) the function returns -1;

If the file descriptor was closed remotely (or not using `libsock`), a false
positive would be returned. However, use of this uuid will result in the
fd being closed.

Returns -1 on error. Returns a valid socket (non-random) UUID.
*/
intptr_t sock_fd2uuid(int fd) {
  if (fd_info[fd].open) {
    return fd_info[fd].fduuid.uuid;
  }
  return -1;
}

/**
Returns 1 if the fduuid_u refers to a valid and open, socket.

Returns 0 if not.
*/
int sock_isvalid(intptr_t uuid) {
  if (validate_connection(uuid))
    return 0;
  return 1;
}

/* *****************************************************************************
Socket Buffer Management - not MT safe
*/

#define ERR_OK (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN)
#define ERR_TRY_AGAIN (errno == EINTR)

#define ERR_ON_FAILED(sfd)           \
  {                                  \
    (sfd)->close = 1;                \
    sock_free_packet((sfd)->packet); \
    (sfd)->packet = NULL;            \
  }

#define FORCE_ROTATE_PACKETS(sfd, pckt)    \
  {                                        \
    (sfd)->packet = (pckt)->metadata.next; \
    (pckt)->metadata.next = NULL;          \
    sock_free_packet(pckt);                \
    sfd->sent = 0;                         \
  }

#define ROTATE_PACKETS(sfd, pckt, _sent, ret_val) \
  {                                               \
    (sfd)->sent += (_sent);                       \
    if ((sfd)->sent >= (pckt)->length) {          \
      FORCE_ROTATE_PACKETS((sfd), (pckt))         \
      return ret_val;                             \
    }                                             \
  }

#if defined(__linux__) /* linux sendfile API */
inline static int send_file_os(int fd, fd_info_s* sfd, sock_packet_s* packet) {
  size_t sent;
restart:
  sent = sendfile64(fd, (int)((ssize_t)packet->buffer),
                    &packet->metadata.offset, packet->length - sfd->sent);

  if (sent == 0) {
    FORCE_ROTATE_PACKETS(sfd, packet);
    return 0;
  } else if (sent < 0) {
    if (ERR_OK) {
      return 1;
    } else if (ERR_TRY_AGAIN) {
      goto restart;
    } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      perror("Linux sendfile failed");
#endif
      ERR_ON_FAILED(sfd);
      return -1;
    }
  }
  ROTATE_PACKETS(sfd, packet, sent, 0);
  return 0;
}

#elif defined(__APPLE__) || defined(__unix__) /* BSD / Apple API */

inline static int send_file_os(int fd, fd_info_s* sfd, sock_packet_s* packet) {
  off_t act_sent;
restart:
  act_sent = packet->length - sfd->sent;

#if defined(__APPLE__)
  if (sendfile((int)((ssize_t)packet->buffer), fd, packet->metadata.offset,
               &act_sent, NULL, 0) < 0 &&
      act_sent == 0)
#else
  if (sendfile((int)((ssize_t)packet->buffer), fd, packet->metadata.offset,
               (size_t)act_sent, NULL, &act_sent, 0) < 0 &&
      act_sent == 0)
#endif
  {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
    perror("Apple/BSD sendfile errno");
#endif

    if (ERR_OK) {
      packet->metadata.offset += act_sent;
      return 1;
    } else if (ERR_TRY_AGAIN) {
      goto restart;
    } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      perror("Apple/BSD sendfile failed");
#endif
      ERR_ON_FAILED(sfd);
      return -1;
    }
  }
  if (act_sent == 0) {
    FORCE_ROTATE_PACKETS(sfd, packet);
    return 0;
  }
  packet->metadata.offset += act_sent;
  ROTATE_PACKETS(sfd, packet, act_sent, 0);
  return 0;
}
#endif

/* returns 0 if the underlying buffer isn't full, and returns a value if the
 * underlying buffer is full or unavailable. */
inline static int send_file(int fd, fd_info_s* sfd, sock_packet_s* packet) {
  ssize_t sent;
restart:
/* File (fd) Packets */
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr,
          "(%d) flushing a file packet with %u/%lu (%lu). start at: %lld\n", fd,
          sfd->sent, packet->length, packet->length - sfd->sent,
          packet->metadata.offset);
#endif
  /* USE_SENDFILE ? */
  if (USE_SENDFILE && sfd->rw_hooks == NULL) {
    return send_file_os(fd, sfd, packet);
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
      fprintf(stderr, "(%d) Read %ld from a file packet, preparing to send.\n",
              fd, i_read);
#endif
      if (i_read <= 0) {
        FORCE_ROTATE_PACKETS(sfd, packet);
        return 0;
      } else {
        packet->metadata.offset += i_read;
        packet->metadata.internal_flag = 1;
      }
    }
    // send the data
    if (sfd->rw_hooks && sfd->rw_hooks->write)
      sent = sfd->rw_hooks->write(sfd->fduuid.uuid,
                                  (((void*)(packet + 1)) + sfd->sent),
                                  i_exp - sfd->sent);
    else
      sent = write(fd, (((void*)(packet + 1)) + sfd->sent), i_exp - sfd->sent);
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
    fprintf(stderr, "(%d) Sent file %ld / %ld bytes from %lu total.\n", fd,
            sent, i_exp, packet->length);
#endif

    // review result and update packet data
    if (sent == 0) {
      return 1;  // nothing to do?
    } else if (sent > 0) {
      // did we finished sending the amount of data we wanted to send?
      sfd->sent += sent;
      if (sfd->sent >= i_exp) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
        fprintf(stderr, "(%d) rotating file buffer (marking for read).\n", fd);
#endif
        packet->metadata.internal_flag = 0;
        sfd->sent = 0;
        packet->length -= i_exp;
        if (packet->length == 0) {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
          fprintf(stderr, "(%d) Finished sending file.\n", fd);
#endif
          FORCE_ROTATE_PACKETS(sfd, packet);
          return 0;
        }
      }
      // return 0;
    } else if (ERR_OK) {
      return 1;
    } else if (ERR_TRY_AGAIN) {
      goto restart;
    } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
      perror("send failed");
#endif
      ERR_ON_FAILED(sfd);
      return -1;
    }
  }
  return 1;
}

/* flushes but doesn't close the socket. flags fd for close on error. */
inline static void sock_flush_unsafe(int fd) {
  fd_info_s* sfd = fd_info + fd;
  sock_packet_s* packet;
  ssize_t sent;
  if ((packet = sfd->packet) != NULL)
    sock_touch(sfd->fduuid.uuid);
  for (;;) {
    if ((packet = sfd->packet) == NULL)
      return;
    packet->metadata.can_interrupt = 0;

    if (packet->metadata.is_fd == 0) {
      /* Data Packets are more likely */

      /* send data */
      if (sfd->rw_hooks && sfd->rw_hooks->write)
        sent =
            sfd->rw_hooks->write(sfd->fduuid.uuid, packet->buffer + sfd->sent,
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
        ROTATE_PACKETS(sfd, packet, sent, ;);
        continue;
      } else if (sent == 0) {
        return;  // nothing to do?
      } else if (ERR_OK) {
        return;
      } else if (ERR_TRY_AGAIN) {
        continue;
      } else {
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
        perror("send failed");
#endif
        ERR_ON_FAILED(sfd);
        return;
      }
    } else {
      if (send_file(fd, sfd, packet))
        return;
    }
  }
}

#undef ERR_ON_FAILED
#undef FORCE_ROTATE_PACKETS
#undef ROTATE_PACKETS

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
`sock_read` attempts to read up to count bytes from file descriptor fd into
the
buffer starting at buf.

It's behavior should conform to the native `read` implementations, except some
data might be available in the `fd`'s buffer while it is not available to be
read using sock_read (i.e., when using a transport layer, such as TLS).

Also, some internal buffering will be used in cases where the transport layer
data available is larger then the data requested.
*/
ssize_t sock_read(intptr_t uuid, void* buf, size_t count) {
  if (validate_connection(uuid)) {
    errno = ENODEV;
    return -1;
  }
  ssize_t i_read;
  fd_info_s* sfd = fd_info + sock_uuid2fd(uuid);
  if (sfd->rw_hooks && sfd->rw_hooks->read)
    i_read = sfd->rw_hooks->read(uuid, buf, count);
  else
    i_read = read(sock_uuid2fd(uuid), buf, count);

  if (i_read > 0) {
    sock_touch(uuid);
    return i_read;
  }
  if (i_read == -1 && (ERR_OK || ERR_TRY_AGAIN))
    return 0;
// return value of 0 or -1 with other errors will prompt a closure.
// sfd->close = 1;
#if defined(NOTIFY_ON_CLOSE) && NOTIFY_ON_CLOSE == 1
  fprintf(stderr, "Read Error for %lu bytes from fd %d (closing))\n", count,
          sock_uuid2fd(uuid));
#endif
  sock_close(uuid);
  return -1;
}

/**
`sock_write2_fn` is the actual function behind the macro `sock_write2`.
*/
ssize_t sock_write2_fn(sock_write_info_s options) {
  if (validate_connection(options.fduuid) || !options.buffer)
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
    return sock_send_packet(options.fduuid, packet);
  } else {
    if (options.move) {
      packet->buffer = (void*)options.buffer;
      packet->length = options.length;
      packet->metadata.external = 1;
      return sock_send_packet(options.fduuid, packet);
    } else {
      if (options.length <= BUFFER_PACKET_SIZE) {
        memcpy(packet->buffer, options.buffer, options.length);
        packet->length = options.length;
        return sock_send_packet(options.fduuid, packet);
      } else {
        lock_fd(fd_info + sock_uuid2fd(options.fduuid));
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
            if (sock_send_packet_unsafe(sock_uuid2fd(options.fduuid), packet))
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
        if (sock_send_packet_unsafe(sock_uuid2fd(options.fduuid), packet))
          goto multi_send_error;
        unlock_fd(fd_info + sock_uuid2fd(options.fduuid));
        return 0;
      multi_send_error:
        unlock_fd(fd_info + sock_uuid2fd(options.fduuid));
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
ssize_t sock_send_packet(intptr_t uuid, sock_packet_s* packet) {
  if (validate_connection(uuid) || packet->length == 0) {
    sock_free_packet(packet);
    return -1;
  }
  fd_info_s* sfd = fd_info + sock_uuid2fd(uuid);
  lock_fd(sfd);
  sock_send_packet_unsafe(sock_uuid2fd(uuid), packet);
  unlock_fd(sfd);
  if (sfd->packet == NULL && sfd->close)
    goto close_socket;
  return 0;
close_socket:
  sock_force_close(uuid);
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
ssize_t sock_flush(intptr_t uuid) {
  if (validate_connection(uuid)) {
    return -1;
  }
  fd_info_s* sfd = fd_info + sock_uuid2fd(uuid);
  if (sfd->packet == NULL) {
    // make sure the rw_hook finished sending all it's data.
    if (sfd->rw_hooks && sfd->rw_hooks->flush) {
      lock_fd(sfd);
      ssize_t val;
      if ((val = sfd->rw_hooks->flush(uuid)) > 0) {
        unlock_fd(sfd);
        return 0;
      }
      if ((val = sfd->rw_hooks->flush(uuid)) < 0)
        sfd->close = 1;
      unlock_fd(sfd);
    }
    if (sfd->close) {
      goto close_socket;
    }
    return 0;
  }
#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1
  fprintf(stderr, "\n=========\n(%d) sock_flush called.\n", sock_uuid2fd(uuid));
#endif
  lock_fd(sfd);
  sock_flush_unsafe(sock_uuid2fd(uuid));
  unlock_fd(sfd);
  if (sfd->packet == NULL && sfd->close)
    goto close_socket;
  return 0;
close_socket:
  sock_force_close(uuid);
  return -1;
}
/**
`sock_flush_strong` performs the same action as `sock_flush` but returns only
after all the data was sent. This is an "active" wait, polling isn't
performed.
*/
void sock_flush_strong(intptr_t uuid) {
  fd_info_s* sfd = fd_info + sock_uuid2fd(uuid);
  while (validate_connection(uuid) == 0 && sfd->packet) {
    sock_flush(uuid);
    sched_yield();
  }
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
    sock_flush(fd_info[i].fduuid.uuid);
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
void sock_close(intptr_t uuid) {
#if defined(NOTIFY_ON_CLOSE) && NOTIFY_ON_CLOSE == 1
  fprintf(stderr, "called sock_close for %lu (%d)\n", uuid, sock_uuid2fd(uuid));
#endif
  if (validate_connection(uuid))
    return;
  fd_info[sock_uuid2fd(uuid)].close = 1;
  sock_flush(uuid);
}
/**
`sock_force_close` closes the connection immediately, without adhering to any
protocol restrictions and without sending any remaining data in the connection
buffer.

If a reactor pointer is provied, the reactor API will be used and the
`on_close`
callback should be called.
*/
void sock_force_close(intptr_t uuid) {
#if defined(NOTIFY_ON_CLOSE) && NOTIFY_ON_CLOSE == 1
  fprintf(stderr, "called sock_force_close for %lu (%d)\n", uuid,
          sock_uuid2fd(uuid));
#endif
  if (validate_connection(uuid))
    return;
  shutdown(sock_uuid2fd(uuid), SHUT_RDWR);
  close(sock_uuid2fd(uuid));
  set_fd(sock_uuid2fd(uuid), LIB_SOCK_STATE_CLOSED);
}

/* *****************************************************************************
RW Hooks implementation
*/

/** Gets a socket RW hooks. */
struct sock_rw_hook_s* sock_rw_hook_get(intptr_t uuid) {
  if (validate_connection(uuid))
    return NULL;
  return fd_info[sock_uuid2fd(uuid)].rw_hooks;
}

/** Sets a socket RW hook. */
int sock_rw_hook_set(intptr_t uuid, sock_rw_hook_s* rw_hook) {
  if (validate_connection(uuid))
    return -1;
  lock_fd(fd_info + sock_uuid2fd(uuid));
  fd_info[sock_uuid2fd(uuid)].rw_hooks = rw_hook;
  unlock_fd(fd_info + sock_uuid2fd(uuid));
  return 0;
}

/* *****************************************************************************
Minor tests
*/

#if defined(DEBUG_SOCKLIB) && DEBUG_SOCKLIB == 1

#define THREADS_FOR_TEST 128
#define THREADS_EACH_COLLECTS 32

static void* take_in_out(void* _) {
  sock_packet_s* p[THREADS_EACH_COLLECTS];
  for (size_t i = 0; i < 1024 / THREADS_EACH_COLLECTS; i++) {
    for (size_t i = 0; i < THREADS_EACH_COLLECTS; i++) {
      p[i] = sock_checkout_packet();
      fprintf(stderr, "Thread %ld owns %p\n", (intptr_t)_, p[i]);
    }
    for (size_t i = 0; i < THREADS_EACH_COLLECTS; i++) {
      if (p[i])
        sock_free_packet(p[i]);
      fprintf(stderr, "Thread %ld frees %p\n", (intptr_t)_, p[i]);
    }
  }
  return _;
}

void libsock_test(void) {
  sock_packet_s *p, *pl;
  fprintf(stderr, "Testing packet pool ");
  for (size_t i = 0; i < BUFFER_PACKET_POOL * 2; i++) {
    pl = p = sock_checkout_packet();
    while (buffer_pool.pool) {
      pl->metadata.next = sock_checkout_packet();
      pl = pl->metadata.next;
    }
    sock_free_packet(p);
  }

  pthread_t threads[THREADS_FOR_TEST];
  for (size_t i = 0; i < THREADS_FOR_TEST; i++) {
    pthread_create(threads + i, NULL, take_in_out, (void*)((intptr_t)i));
  }
  void* res;
  for (size_t i = 0; i < THREADS_FOR_TEST; i++) {
    pthread_join(threads[i], &res);
  }
  fprintf(stderr, "***threads finished\n\n\n\n");
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
