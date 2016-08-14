/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "libsock.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

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
OS Sendfile settings.
*/

#ifndef USE_SENDFILE

#if defined(__linux__) /* linux sendfile works  */
#include <sys/sendfile.h>
#define USE_SENDFILE 1
#elif defined(__unix__) /* BSD sendfile should work, but isn't tested */
#include <sys/uio.h>
#define USE_SENDFILE 0
#elif defined(__APPLE__) /* AIs the pple sendfile still broken? */
#include <sys/uio.h>
#define USE_SENDFILE 1
#else /* sendfile might not be available - always set to 0 */
#define USE_SENDFILE 0
#endif

#endif

/* *****************************************************************************
Library related helper functions
*/

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

/**
Gets the maximum number of file descriptors this process can be allowed to
access (== maximum fd value + 1).
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

/* *****************************************************************************
A Simple busy lock implementation ... (spnlock.h) Copied for portability
Written by Boaz Segev at 2016. Donated to the public domain for all to enjoy.
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
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
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
#if defined(__has_include)
#if __has_include(<stdatomic.h>)
#define SPN_TMP_HAS_ATOMICS 1
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
__unused static inline int spn_is_locked(spn_lock_i* lock) {
  return atomic_load(lock);
}
#endif
#endif

/* Chack if stdatomic was available */
#ifdef SPN_TMP_HAS_ATOMICS
#undef SPN_TMP_HAS_ATOMICS

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
__unused static inline int spn_is_locked(spn_lock_i* lock) {
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
  /** write buffer - a linked list */
  sock_packet_s* packet;
  /** The fd UUID for the current connection */
  fduuid_u fduuid;
  /** the amount of data sent from the current buffer packet */
  uint32_t sent;
  /** state lock */
  spn_lock_i lock;
  /* -- state flags -- */
  /** Connection is open */
  unsigned open : 1;
  /** indicated that the connection should be closed. */
  unsigned close : 1;
  /** indicated that the connection experienced an error. */
  unsigned err : 1;
  /** future flags. */
  unsigned rsv : 5;
  /* -- placement enforces padding to guaranty memory alignment -- */
  /** Read/Write hooks. */
  sock_rw_hook_s* rw_hooks;
} fd_info_s;

#define LIB_SOCK_STATE_OPEN 1
#define LIB_SOCK_STATE_CLOSED 0

static fd_info_s* fd_info = NULL;
static size_t fd_capacity = 0;

#define uuid2info(uuid) fd_info[sock_uuid2fd(uuid)]
#define validate_connection(uuid) (sock_uuid2fd(uuid) >= fd_capacity)

static struct {
  sock_packet_s* pool;
  sock_packet_s* allocated;
  spn_lock_i lock;
} buffer_pool = {.lock = SPN_LOCK_INIT};

#define BUFFER_PACKET_REAL_SIZE (sizeof(sock_packet_s) + BUFFER_PACKET_SIZE)

#define is_valid(uuid)                                \
  (fd_info[sock_uuid2fd(uuid)].fduuid.data.counter == \
       ((fduuid_u)(uuid)).data.counter &&             \
   uuid2info(uuid).open)

/* reset a socket state */
static inline void set_fd(int fd, unsigned int state) {
  fd_info_s old_data;
  // lock and update
  spn_lock(&fd_info[fd].lock);
  old_data = fd_info[fd];
  fd_info[fd] = (fd_info_s){
      .fduuid.data.counter = fd_info[fd].fduuid.data.counter + state,
      .fduuid.data.fd = fd,
      .lock = fd_info[fd].lock,
      .open = state,
  };
  // unlock
  spn_unlock(&fd_info[fd].lock);
  // should be called within the lock? - no function calling within a spinlock.
  if (old_data.rw_hooks && old_data.rw_hooks->on_clear)
    old_data.rw_hooks->on_clear(old_data.fduuid.uuid, old_data.rw_hooks);
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

/**
Destroys the library data.

Call this function before calling any `libsock` functions.
*/
static void destroy_lib_data(void) {
  if (fd_info) {
    while (fd_capacity--) {  // include 0 in countdown
      set_fd(fd_capacity, LIB_SOCK_STATE_CLOSED);
    }
    munmap(fd_info, (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL) +
                        (sizeof(fd_info_s) * fd_capacity));
  }
  fd_info = NULL;
  buffer_pool.pool = NULL;
  buffer_pool.allocated = NULL;
  buffer_pool.lock = SPN_LOCK_INIT;
}

/**
Initializes the library.

Call this function before calling any `libsock` functions.
*/
void sock_lib_init(void) {
  if (fd_info)
    return;

  fd_capacity = sock_max_capacity();
  size_t fd_map_mem_size = sizeof(fd_info_s) * fd_capacity;
  size_t buffer_mem_size = BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL;

  void* buff_mem;
  buff_mem = mmap(NULL, fd_map_mem_size + buffer_mem_size,
                  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (buff_mem == MAP_FAILED || buff_mem == NULL) {
    perror("Couldn't initialize libsock - not enough memory? ");
    exit(1);
  }

  fd_info = buff_mem;
  for (size_t i = 0; i < fd_capacity; i++) {
    fd_info[i] = (fd_info_s){.lock = SPN_LOCK_INIT};
    spn_unlock(&fd_info[i].lock);
  }
  /* initialize pool */
  buffer_pool.allocated = buff_mem + fd_map_mem_size;
  buffer_pool.pool = buffer_pool.allocated;
  sock_packet_s* pos = buffer_pool.pool;
  for (size_t i = 0; i < BUFFER_PACKET_POOL - 1; i++) {
    *pos = (sock_packet_s){
        .metadata.next = (void*)(((uintptr_t)pos) + BUFFER_PACKET_REAL_SIZE),
    };
    pos = pos->metadata.next;
  }
  pos->metadata.next = 0;
  /* deallocate and manage on exit */
  atexit(destroy_lib_data);
#ifdef DEBUG
  fprintf(stderr,
          "\nInitialized libsock for %lu sockets, "
          "each one requires %lu bytes.\n"
          "overall ovearhead: %lu bytes.\n"
          "Initialized packet pool for %d elements, "
          "each one %lu bytes.\n"
          "overall buffer ovearhead: %lu bytes.\n"
          "=== Total: %lu bytes ===\n\n",
          fd_capacity, sizeof(*fd_info), sizeof(*fd_info) * fd_capacity,
          BUFFER_PACKET_POOL, BUFFER_PACKET_REAL_SIZE,
          BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL,
          (BUFFER_PACKET_REAL_SIZE * BUFFER_PACKET_POOL) +
              (sizeof(*fd_info) * fd_capacity));
#endif
}

#define review_lib()     \
  do {                   \
    if (fd_info == NULL) \
      sock_lib_init();   \
  } while (0)

/* *****************************************************************************
Read / Write internals
*/

#define ERR_OK (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN)
#define ERR_TRY_AGAIN (errno == EINTR)

static inline int sock_flush_fd_failed(int fd) {
  sock_free_packet(fd_info[fd].packet);
  fd_info[fd].packet = NULL;
  fd_info[fd].close = 1;
  fd_info[fd].err = 1;
  return 0;
}

#if USE_SENDFILE == 1

#if defined(__linux__) /* linux sendfile API */
static inline int sock_flush_os_sendfile(int fd) {
  size_t sent;
  sock_packet_s* packet = fd_info[fd].packet;
  sent =
      sendfile64(fd, (int)((ssize_t)packet->buffer), &packet->metadata.offset,
                 packet->length - fd_info[fd].sent);

  if (sent < 0) {
    if (ERR_OK)
      return -1;
    else if (ERR_TRY_AGAIN)
      return 0;
    else
      return sock_flush_fd_failed(fd);
  }
  if (sent == 0)
    fd_info[fd].sent = packet->length;
  fd_info[fd].sent += sent;
  return 0;
}

#elif defined(__APPLE__) || defined(__unix__) /* BSD / Apple API */

static inline int sock_flush_os_sendfile(int fd) {
  off_t act_sent;
  sock_packet_s* packet = fd_info[fd].packet;
  act_sent = packet->length - fd_info[fd].sent;

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
    if (ERR_OK)
      return -1;
    else if (ERR_TRY_AGAIN)
      return 0;
    else
      return sock_flush_fd_failed(fd);
  }
  if (act_sent == 0) {
    fd_info[fd].sent = packet->length;
    return 0;
  }
  packet->metadata.offset += act_sent;
  fd_info[fd].sent += act_sent;
  return 0;
}
#endif

#else

static inline int sock_flush_os_sendfile(int fd) {
  return -1;
}

#endif

static inline int sock_flush_fd(int fd) {
  if (USE_SENDFILE && fd_info[fd].rw_hooks == NULL)
    return sock_flush_os_sendfile(fd);
  ssize_t sent;
  sock_packet_s* packet = fd_info[fd].packet;
  // how much data are we expecting to send...?
  ssize_t i_exp = (BUFFER_PACKET_SIZE > packet->length) ? packet->length
                                                        : BUFFER_PACKET_SIZE;

  // read data into the internal buffer
  if (packet->metadata.internal_flag == 0) {
    ssize_t i_read;
    i_read = pread((int)((ssize_t)packet->buffer), packet + 1, i_exp,
                   packet->metadata.offset);
    if (i_read <= 0) {
      fd_info[fd].sent = fd_info[fd].packet->length;
      return 0;
    } else {
      packet->metadata.offset += i_read;
      packet->metadata.internal_flag = 1;
    }
  }
  // send the data
  if (fd_info[fd].rw_hooks && fd_info[fd].rw_hooks->write)
    sent = fd_info[fd].rw_hooks->write(
        fd_info[fd].fduuid.uuid, (((void*)(packet + 1)) + fd_info[fd].sent),
        i_exp - fd_info[fd].sent);
  else
    sent = write(fd, (((void*)(packet + 1)) + fd_info[fd].sent),
                 i_exp - fd_info[fd].sent);
  // review result and update packet data
  if (sent < 0) {
    if (ERR_OK)
      return -1;
    else if (ERR_TRY_AGAIN)
      return 0;
    else
      return sock_flush_fd_failed(fd);
  }
  fd_info[fd].sent += sent;
  if (fd_info[fd].sent >= i_exp) {
    packet->metadata.internal_flag = 0;
    fd_info[fd].sent = 0;
    packet->length -= i_exp;
  }
  return 0;
}

static inline int sock_flush_data(int fd) {
  ssize_t sent;
  if (fd_info[fd].rw_hooks && fd_info[fd].rw_hooks->write)
    sent = fd_info[fd].rw_hooks->write(
        fd_info[fd].fduuid.uuid, fd_info[fd].packet->buffer + fd_info[fd].sent,
        fd_info[fd].packet->length - fd_info[fd].sent);
  else
    sent = write(fd, fd_info[fd].packet->buffer + fd_info[fd].sent,
                 fd_info[fd].packet->length - fd_info[fd].sent);
  if (sent < 0) {
    if (ERR_OK)
      return -1;
    else if (ERR_TRY_AGAIN)
      return 0;
    else
      return sock_flush_fd_failed(fd);
  }
  fd_info[fd].sent += sent;
  return 0;
}

static void sock_flush_unsafe(int fd) {
  while (fd_info[fd].packet) {
    if (fd_info[fd].packet->metadata.is_fd == 0) {
      if (sock_flush_data(fd))
        return;
    } else {
      if (sock_flush_fd(fd))
        return;
    }
    if (fd_info[fd].packet && fd_info[fd].packet->length <= fd_info[fd].sent) {
      sock_packet_s* packet = fd_info[fd].packet;
      fd_info[fd].packet = packet->metadata.next;
      packet->metadata.next = NULL;
      fd_info[fd].sent = 0;
      sock_free_packet(packet);
    }
  }
}

static inline void sock_send_packet_unsafe(int fd, sock_packet_s* packet) {
  fd_info_s* sfd = fd_info + fd;
  if (sfd->packet == NULL) {
    /* no queue, nothing to check */
    sfd->packet = packet;
    sock_flush_unsafe(fd);
    return;

  } else if (packet->metadata.urgent == 0) {
    /* not urgent, last in line */
    sock_packet_s* pos = sfd->packet;
    while (pos->metadata.next)
      pos = pos->metadata.next;
    pos->metadata.next = packet;
    sock_flush_unsafe(fd);
    return;

  } else {
    /* urgent, find a spot we can interrupt */
    sock_packet_s** pos = &sfd->packet;
    while (*pos && (*pos)->metadata.can_interrupt == 0)
      pos = &(*pos)->metadata.next;
    sock_packet_s* tail = *pos;
    *pos = packet;
    if (tail) {
      pos = &packet->metadata.next;
      while (*pos)
        pos = &(*pos)->metadata.next;
      *pos = tail;
    }
  }
  sock_flush_unsafe(fd);
}

/* *****************************************************************************
Listen
*/

/**
Opens a listening non-blocking socket. Return's the socket's UUID.
*/
intptr_t sock_listen(const char* address, const char* port) {
  review_lib();
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
Accept
*/

intptr_t sock_accept(intptr_t srv_uuid) {
  review_lib();
  static socklen_t cl_addrlen = 0;
  int client;
#ifdef SOCK_NONBLOCK
  client = accept4(sock_uuid2fd(srv_uuid), NULL, &cl_addrlen, SOCK_NONBLOCK);
  if (client <= 0)
    return -1;
#else
  client = accept(sock_uuid2fd(srv_uuid), NULL, &cl_addrlen);
  if (client <= 0)
    return -1;
  sock_set_non_block(client);
#endif
  set_fd(client, LIB_SOCK_STATE_OPEN);
  return fd_info[client].fduuid.uuid;
}

/* *****************************************************************************
Connect
*/
intptr_t sock_connect(char* address, char* port) {
  review_lib();
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

/* *****************************************************************************
Open existing
*/

intptr_t sock_open(int fd) {
  review_lib();
  set_fd(fd, LIB_SOCK_STATE_OPEN);
  return fd_info[fd].fduuid.uuid;
}

/* *****************************************************************************
Information about the socket
*/

/**
Returns 1 if the uuid refers to a valid and open, socket.

Returns 0 if not.
*/
int sock_isvalid(intptr_t uuid) {
  review_lib();
  return is_valid(uuid);
}

/**
`sock_fd2uuid` takes an existing file decriptor `fd` and returns it's active
`uuid`.
*/
intptr_t sock_fd2uuid(int fd) {
  review_lib();
  return fd_info[fd].open ? fd_info[fd].fduuid.uuid : -1;
}

/* *****************************************************************************
Buffer API.
*/

static inline sock_packet_s* sock_try_checkout_packet(void) {
  sock_packet_s* packet;
  spn_lock(&buffer_pool.lock);
  packet = buffer_pool.pool;
  if (packet) {
    buffer_pool.pool = packet->metadata.next;
    spn_unlock(&buffer_pool.lock);
    *packet = (sock_packet_s){.buffer = packet + 1, .metadata.next = NULL};
    return packet;
  }
  spn_unlock(&buffer_pool.lock);
  return packet;
}

/**
Checks out a `sock_packet_s` from the packet pool, transfering the
ownership of the memory to the calling function. The function will hang until a
packet becomes available, so never check out more then a single packet at a
time.
*/
sock_packet_s* sock_checkout_packet(void) {
  review_lib();
  sock_packet_s* packet = NULL;
  for (;;) {
    spn_lock(&buffer_pool.lock);
    packet = buffer_pool.pool;
    if (packet) {
      buffer_pool.pool = packet->metadata.next;
      spn_unlock(&buffer_pool.lock);
      *packet = (sock_packet_s){.buffer = packet + 1, .metadata.next = NULL};
      return packet;
    }
    spn_unlock(&buffer_pool.lock);
    reschedule_thread();
    sock_flush_all();
  }
}
/**
Attaches a packet to a socket's output buffer and calls `sock_flush` for the
socket.
*/
ssize_t sock_send_packet(intptr_t uuid, sock_packet_s* packet) {
  review_lib();
  if (!is_valid(uuid)) {
    sock_free_packet(packet);
    return -1;
  }
  spn_lock(&uuid2info(uuid).lock);
  sock_send_packet_unsafe(sock_uuid2fd(uuid), packet);
  spn_unlock(&uuid2info(uuid).lock);
  return 0;
}

/**
Use `sock_free_packet` to free unused packets that were checked-out using
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
      break; /* next will hold the last packet in the chain. */
    next = next->metadata.next;
  }
  spn_lock(&buffer_pool.lock);
  next->metadata.next = buffer_pool.pool;
  buffer_pool.pool = packet;
  spn_unlock(&buffer_pool.lock);
}

/* *****************************************************************************
Reading
*/
ssize_t sock_read(intptr_t uuid, void* buf, size_t count) {
  review_lib();
  if (!is_valid(uuid)) {
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
  // fprintf(stderr, "Read Error for %lu bytes from fd %d (closing))\n", count,
  //         sock_uuid2fd(uuid));
  sock_close(uuid);
  return -1;
}

/* *****************************************************************************
Flushing
*/

ssize_t sock_flush(intptr_t uuid) {
  review_lib();
  if (!is_valid(uuid))
    return -1;
  spn_lock(&uuid2info(uuid).lock);
  sock_flush_unsafe(sock_uuid2fd(uuid));
  spn_unlock(&uuid2info(uuid).lock);
  if (uuid2info(uuid).close) {
    sock_force_close(uuid);
    return -1;
  }
  return 0;
}
/**
`sock_flush_strong` performs the same action as `sock_flush` but returns only
after all the data was sent. This is an "active" wait, polling isn't
performed.
*/
void sock_flush_strong(intptr_t uuid) {
  review_lib();
  while (is_valid(uuid) && uuid2info(uuid).packet)
    sock_flush(uuid);
}
/**
Calls `sock_flush` for each file descriptor that's buffer isn't empty.
*/
void sock_flush_all(void) {
  for (size_t i = 0; i < fd_capacity; i++) {
    if (fd_info[i].packet == NULL || spn_is_locked(&fd_info[i].lock))
      continue;
    sock_flush(fd_info[i].fduuid.uuid);
  }
}

/* *****************************************************************************
Writing
*/

ssize_t sock_write2_fn(sock_write_info_s options) {
  review_lib();
  if (!is_valid(options.fduuid)) {
    errno = ENODEV;
    return -1;
  }
  if (options.buffer == NULL)
    return -1;
  if (!options.length && !options.is_fd)
    options.length = strlen(options.buffer);
  if (options.length == 0)
    return -1;
  sock_packet_s* packet = sock_checkout_packet();
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
        if (packet->metadata.urgent) {
          fprintf(stderr,
                  "Socket err:"
                  "Large data cannot be sent as an urgent packet.\n"
                  "Urgency silently ignored\n");
          packet->metadata.urgent = 0;
        }
        size_t to_cpy;
        spn_lock(&uuid2info(options.fduuid).lock);
        for (;;) {
          to_cpy = options.length > BUFFER_PACKET_SIZE ? BUFFER_PACKET_SIZE
                                                       : options.length;
          memcpy(packet->buffer, options.buffer, to_cpy);
          packet->length = to_cpy;
          options.length -= to_cpy;
          options.buffer += to_cpy;
          sock_send_packet_unsafe(sock_uuid2fd(options.fduuid), packet);
          if (!is_valid(options.fduuid) || uuid2info(options.fduuid).err == 1 ||
              options.length == 0)
            break;
          packet = sock_try_checkout_packet();
          while (packet == NULL) {
            sock_flush_all();
            sock_flush_unsafe(sock_uuid2fd(options.fduuid));
            packet = sock_try_checkout_packet();
          }
        }
        spn_unlock(&uuid2info(options.fduuid).lock);
        if (uuid2info(options.fduuid).packet == NULL &&
            uuid2info(options.fduuid).close) {
          sock_force_close(options.fduuid);
          return -1;
        }
        return is_valid(options.fduuid) ? 0 : -1;
      }
    }
  }
  // how did we get here?
  return -1;
}

/* *****************************************************************************
Closing.
*/

void sock_close(intptr_t uuid) {
  // fprintf(stderr, "called sock_close for %lu (%d)\n", uuid,
  // sock_uuid2fd(uuid));
  review_lib();
  if (!is_valid(uuid))
    return;
  fd_info[sock_uuid2fd(uuid)].close = 1;
  sock_flush(uuid);
}

void sock_force_close(intptr_t uuid) {
  // fprintf(stderr, "called sock_force_close for %lu (%d)\n", uuid,
  //         sock_uuid2fd(uuid));
  review_lib();
  if (!is_valid(uuid))
    return;
  shutdown(sock_uuid2fd(uuid), SHUT_RDWR);
  close(sock_uuid2fd(uuid));
  set_fd(sock_uuid2fd(uuid), LIB_SOCK_STATE_CLOSED);
}

/* *****************************************************************************
RW hooks implementation
*/

/** Gets a socket hook state (a pointer to the struct). */
struct sock_rw_hook_s* sock_rw_hook_get(intptr_t uuid) {
  review_lib();
  if (validate_connection(uuid))
    return NULL;
  return uuid2info(uuid).rw_hooks;
}

/** Sets a socket hook state (a pointer to the struct). */
int sock_rw_hook_set(intptr_t uuid, sock_rw_hook_s* rw_hooks) {
  review_lib();
  if (validate_connection(uuid))
    return -1;
  spn_lock(&(uuid2info(uuid).lock));
  uuid2info(uuid).rw_hooks = rw_hooks;
  spn_unlock(&uuid2info(uuid).lock);
  return 0;
}

/* *****************************************************************************
test
*/
#ifdef DEBUG
void sock_libtest(void) {
  sock_packet_s *p, *pl;
  size_t count = 0;
  fprintf(stderr, "Testing packet pool\n");
  for (size_t i = 0; i < BUFFER_PACKET_POOL * 2; i++) {
    count = 1;
    pl = p = sock_checkout_packet();
    while (buffer_pool.pool) {
      count++;
      pl->metadata.next = sock_checkout_packet();
      pl = pl->metadata.next;
    }
    sock_free_packet(p);
    // fprintf(stderr, "Collected and freed %lu packets.\n", count);
  }
  fprintf(stderr,
          "liniar (no-contention) packet checkout + free shows %lu packets. "
          "test %s\n",
          count, count == BUFFER_PACKET_POOL ? "passed." : "FAILED!");
}
#endif
