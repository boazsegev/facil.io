/* *****************************************************************************
Copyright: Boaz Segev, 2018-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

#define FIO_EXTERN_COMPLETE 1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>

#define FIO_STRING_NAME fio_str
#define FIO_REF_NAME fio_str
#define FIO_REF_CONSTRUCTOR_ONLY
#include "fio-stl.h"

#define FIO_SOCK
#include <fio-stl.h>

#include <pthread.h>
#include <sys/mman.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <arpa/inet.h>

/* force poll for testing? */
#ifndef FIO_ENGINE_POLL
#define FIO_ENGINE_POLL 0
#endif

#if !FIO_ENGINE_POLL && !FIO_ENGINE_EPOLL && !FIO_ENGINE_KQUEUE
#if defined(__linux__)
#define FIO_ENGINE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) ||     \
    defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#define FIO_ENGINE_KQUEUE 1
#else
#define FIO_ENGINE_POLL 1
#endif
#endif

/* for kqueue and epoll only */
#ifndef FIO_POLL_MAX_EVENTS
#define FIO_POLL_MAX_EVENTS 64
#endif

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 1000
#endif

#ifndef FIO_USE_URGENT_QUEUE
#define FIO_USE_URGENT_QUEUE 1
#endif

#ifndef DEBUG_SPINLOCK
#define DEBUG_SPINLOCK 0
#endif

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

/* Slowloris mitigation  (must be less than 1<<16) */
#ifndef FIO_SLOWLORIS_LIMIT
#define FIO_SLOWLORIS_LIMIT (1 << 10)
#endif

#if !defined(__clang__) && !defined(__GNUC__)
#define __thread _Thread_value
#endif

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void deferred_on_close(void *uuid_, void *pr_);
static void deferred_on_shutdown(void *arg, void *arg2);
static void deferred_on_ready(void *arg, void *arg2);
static void deferred_on_data(void *uuid, void *arg2);
static void deferred_ping(void *arg, void *arg2);

/* *****************************************************************************
Section Start Marker











                       Main State Machine Data Structures












***************************************************************************** */

/** An object that can be linked to any facil.io connection (uuid). */
typedef struct {
  void *data;
  void (*on_close)(void *data);
} fio_uuid_env_obj_s;

#define FIO_SMALL_STR_NAME fio_uuid_env_name
#include <fio-stl.h>

#define FIO_MAP_NAME fio___uuid_env
#define FIO_MAP_TYPE fio_uuid_env_obj_s
#define FIO_MAP_TYPE_DESTROY(o)                                                \
  do {                                                                         \
    if (o.on_close)                                                            \
      o.on_close(o.data);                                                      \
  } while (0)
#define FIO_MAP_KEY fio_uuid_env_name_s
#define FIO_MAP_KEY_INVALID (fio_uuid_env_name_s) FIO_SMALL_STR_INIT
#define FIO_MAP_KEY_DESTROY(k) fio_uuid_env_name_destroy(&k)
/* destroy discarded keys when overwriting existing data (duplicate keys aren't
 * copied): */
#define FIO_MAP_KEY_DISCARD(k) fio_uuid_env_name_destroy(&k)
#include <fio-stl.h>

/** User-space socket buffer data */
typedef struct fio_packet_s fio_packet_s;
struct fio_packet_s {
  fio_packet_s *next;
  int (*write_func)(int fd, struct fio_packet_s *packet);
  void (*dealloc)(void *buf);
  union {
    void *buf;
    intptr_t fd;
  } data;
  uintptr_t offset;
  uintptr_t len;
};

/** Connection data (fd_data) */
typedef struct {
  /* current data to be send */
  fio_packet_s *packet;
  /** the last packet in the queue. */
  fio_packet_s **packet_last;
  /* Data sent so far */
  size_t sent;
  /* fd protocol */
  fio_protocol_s *protocol;
  /* timer handler */
  time_t active;
  /** The number of pending packets that are in the queue. */
  uint16_t packet_count;
  /* timeout settings */
  uint8_t timeout;
  /* indicates that the fd should be considered scheduled (added to poll) */
  fio_lock_i scheduled;
  /* protocol lock */
  fio_lock_i protocol_lock;
  /* used to convert `fd` to `uuid` and validate connections */
  uint8_t counter;
  /* socket lock */
  fio_lock_i sock_lock;
  /** Connection is open */
  uint8_t open;
  /** indicated that the connection should be closed. */
  uint8_t close;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
  /** RW hooks. */
  fio_rw_hook_s *rw_hooks;
  /** RW udata. */
  void *rw_udata;
  /* Objects linked to the UUID */
  fio___uuid_env_s env;
} fio_fd_data_s;

typedef struct {
  struct timespec last_cycle;
  /* connection capacity */
  uint32_t capa;
  /* connections counted towards shutdown (NOT while running) */
  uint32_t connection_count;
  /* thread list */
  FIO_LIST_HEAD thread_ids;
  /* active workers */
  uint16_t workers;
  /* timer handler */
  uint16_t threads;
  /* timeout review loop flag */
  uint8_t need_review;
  /* spinning down process */
  uint8_t volatile active;
  /* worker process flag - true also for single process */
  uint8_t is_worker;
  /* polling and global lock */
  fio_lock_i lock;
  /* The highest active fd with a protocol object */
  uint32_t max_protocol_fd;
  /* timer handler */
  pid_t parent;
#if FIO_ENGINE_POLL
  struct pollfd *poll;
#endif
  fio_fd_data_s info[];
} fio_data_s;

static fio_data_s *fio_data = NULL;

/* used for protocol locking by task type. */
typedef struct {
  fio_lock_i locks[3];
  unsigned rsv : 8;
} protocol_metadata_s;

/* used for accessing the protocol locking in a safe byte aligned way. */
union protocol_metadata_union_u {
  size_t opaque;
  protocol_metadata_s meta;
};

#define fd_data(fd) (fio_data->info[(uintptr_t)(fd)])
#define uuid_data(uuid) fd_data(fio_uuid2fd((uuid)))
#define fd2uuid(fd)                                                            \
  ((intptr_t)((((uintptr_t)(fd)) << 8) | fd_data((fd)).counter))

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
  if (fio_data)
    return fio_data->capa;
  return 0;
}

/* *****************************************************************************
Packet allocation (for socket's user-buffer)
***************************************************************************** */

static inline void fio_packet_free(fio_packet_s *packet) {
  packet->dealloc(packet->data.buf);
  fio_free(packet);
}
static inline fio_packet_s *fio_packet_alloc(void) {
  fio_packet_s *packet = fio_malloc(sizeof(*packet));
  FIO_ASSERT_ALLOC(packet);
  return packet;
}

/* *****************************************************************************
Core Connection Data Clearing
***************************************************************************** */

/* set the minimal max_protocol_fd */
static void fio_max_fd_min(uint32_t fd) {
  if (fio_data->max_protocol_fd > fd)
    return;
  fio_lock(&fio_data->lock);
  if (fio_data->max_protocol_fd < fd)
    fio_data->max_protocol_fd = fd;
  fio_unlock(&fio_data->lock);
}

/* set the minimal max_protocol_fd */
static void fio_max_fd_shrink(void) {
  fio_lock(&fio_data->lock);
  uint32_t fd = fio_data->max_protocol_fd;
  while (fd && fd_data(fd).protocol == NULL)
    --fd;
  fio_data->max_protocol_fd = fd;
  fio_unlock(&fio_data->lock);
}

/* resets connection data, marking it as either open or closed. */
static inline int fio_clear_fd(intptr_t fd, uint8_t is_open) {
  fio_packet_s *packet;
  fio_protocol_s *protocol;
  fio_rw_hook_s *rw_hooks;
  void *rw_udata;
  fio___uuid_env_s env;
  fio_lock(&(fd_data(fd).sock_lock));
  env = fd_data(fd).env;
  packet = fd_data(fd).packet;
  protocol = fd_data(fd).protocol;
  rw_hooks = fd_data(fd).rw_hooks;
  rw_udata = fd_data(fd).rw_udata;
  fd_data(fd) = (fio_fd_data_s){
      .open = is_open,
      .sock_lock = fd_data(fd).sock_lock,
      .protocol_lock = fd_data(fd).protocol_lock,
      .rw_hooks = (fio_rw_hook_s *)&FIO_DEFAULT_RW_HOOKS,
      .counter = fd_data(fd).counter + 1,
      .packet_last = &fd_data(fd).packet,
  };
  fio_unlock(&(fd_data(fd).sock_lock));
  if (rw_hooks && rw_hooks->cleanup)
    rw_hooks->cleanup(rw_udata);
  while (packet) {
    fio_packet_s *tmp = packet;
    packet = packet->next;
    fio_packet_free(tmp);
  }
  fio___uuid_env_destroy(&env);
  if (protocol && protocol->on_close) {
    fio_defer(deferred_on_close, (void *)fd2uuid(fd), protocol);
  }
  if (is_open)
    fio_max_fd_min(fd);
  return 0;
}

static inline void fio_force_close_in_poll(intptr_t uuid) {
  uuid_data(uuid).close = 2;
  fio_force_close(uuid);
}

/* *****************************************************************************
Protocol Locking and UUID validation
***************************************************************************** */

/* Macro for accessing the protocol locking / metadata. */
#define prt_meta(prt) (((union protocol_metadata_union_u *)(&(prt)->rsv))->meta)

/** locks a connection's protocol returns a pointer that need to be unlocked. */
inline static fio_protocol_s *protocol_try_lock(intptr_t fd,
                                                enum fio_protocol_lock_e type) {
  errno = 0;
  if (fio_trylock(&fd_data(fd).protocol_lock))
    goto would_block;
  fio_protocol_s *pr = fd_data(fd).protocol;
  if (!pr) {
    fio_unlock(&fd_data(fd).protocol_lock);
    goto invalid;
  }
  if (fio_trylock(&prt_meta(pr).locks[type])) {
    fio_unlock(&fd_data(fd).protocol_lock);
    goto would_block;
  }
  fio_unlock(&fd_data(fd).protocol_lock);
  return pr;
would_block:
  errno = EWOULDBLOCK;
  return NULL;
invalid:
  errno = EBADF;
  return NULL;
}
/** See `fio_protocol_try_lock` for details. */
inline static void protocol_unlock(fio_protocol_s *pr,
                                   enum fio_protocol_lock_e type) {
  fio_unlock(&prt_meta(pr).locks[type]);
}

/** returns 1 if the UUID is valid and 0 if it isn't. */
#define uuid_is_valid(uuid)                                                    \
  ((intptr_t)(uuid) >= 0 &&                                                    \
   ((uint32_t)fio_uuid2fd((uuid))) < fio_data->capa &&                         \
   ((uintptr_t)(uuid)&0xFF) == uuid_data((uuid)).counter)

/* public API. */
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid,
                                      enum fio_protocol_lock_e type) {
  if (!uuid_is_valid(uuid)) {
    errno = EBADF;
    return NULL;
  }
  return protocol_try_lock(fio_uuid2fd(uuid), type);
}

/* public API. */
void fio_protocol_unlock(fio_protocol_s *pr, enum fio_protocol_lock_e type) {
  protocol_unlock(pr, type);
}

/* *****************************************************************************
UUID validation and state
***************************************************************************** */

/* public API. */
intptr_t fio_fd2uuid(int fd) {
  if (fd < 0 || (size_t)fd >= fio_data->capa)
    return -1;
  if (!fd_data(fd).open) {
    fio_lock(&fd_data(fd).protocol_lock);
    fio_clear_fd(fd, 1);
    fio_unlock(&fd_data(fd).protocol_lock);
  }
  return fd2uuid(fd);
}

/* public API. */
int fio_is_valid(intptr_t uuid) { return uuid_is_valid(uuid); }

/* public API. */
int fio_is_closed(intptr_t uuid) {
  return !uuid_is_valid(uuid) || !uuid_data(uuid).open || uuid_data(uuid).close;
}

void fio_stop(void) {
  if (fio_data)
    fio_data->active = 0;
}

/* public API. */
int16_t fio_is_running(void) { return fio_data && fio_data->active; }

/* public API. */
struct timespec fio_last_tick(void) {
  return fio_data->last_cycle;
}

#define touchfd(fd) fd_data((fd)).active = fio_data->last_cycle.tv_sec

/* public API. */
void fio_touch(intptr_t uuid) {
  if (uuid_is_valid(uuid))
    touchfd(fio_uuid2fd(uuid));
}

/* public API. */
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
UUID attachments (linking objects to the UUID's lifetime)
***************************************************************************** */

void fio_uuid_env_set___(void); /* for sublime text function listing */
/* public API. */
void fio_uuid_env_set FIO_NOOP(intptr_t uuid, fio_uuid_env_args_s args) {
  if ((!args.data && !args.on_close) || !uuid_is_valid(uuid))
    goto invalid;
  fio_uuid_env_name_s n = FIO_SMALL_STR_INIT;
  fio_uuid_env_obj_s i = {.data = NULL};
  if (args.name.buf && args.name.len) {
    if (args.const_name) {
      fio_uuid_env_name_set_const(&n, args.name.buf, args.name.len);
    } else {
      fio_uuid_env_name_set_copy(&n, args.name.buf, args.name.len);
    }
  }
  fio_lock(&uuid_data(uuid).sock_lock);
  if (!uuid_is_valid(uuid))
    goto locked_invalid;
  fio___uuid_env_set(
      &uuid_data(uuid).env, fio_uuid_env_name_hash(&n, args.type), n,
      (fio_uuid_env_obj_s){.on_close = args.on_close, .data = args.data}, &i);
  fio_unlock(&uuid_data(uuid).sock_lock);
  if (i.on_close)
    i.on_close(i.data);
  return;
locked_invalid:
  fio_unlock(&uuid_data(uuid).sock_lock);
invalid:
  errno = EBADF;
  if (args.on_close)
    args.on_close(args.data);
}

void fio_uuid_env_remove____(void); /* for sublime text function listing */
/* public API. */
void fio_uuid_env_remove FIO_NOOP(intptr_t uuid,
                                  fio_uuid_env_unset_args_s args) {
  fio_uuid_env_name_s n = FIO_SMALL_STR_INIT;
  fio_uuid_env_obj_s i = {.data = NULL};
  fio_uuid_env_name_set_const(&n, args.name.buf, args.name.len);
  if (!uuid_is_valid(uuid))
    goto invalid;
  fio_lock(&uuid_data(uuid).sock_lock);
  if (!uuid_is_valid(uuid))
    goto locked_invalid;
  fio___uuid_env_remove(&uuid_data(uuid).env,
                        fio_uuid_env_name_hash(&n, args.type), n, &i);
  fio_unlock(&uuid_data(uuid).sock_lock);
  if (i.on_close)
    i.on_close(i.data);
  return;
locked_invalid:
  fio_unlock(&uuid_data(uuid).sock_lock);
  if (i.on_close)
    i.on_close(i.data);
invalid:
  errno = EBADF;
  return;
}

#if 0 /* UNSAFE don't enable unless single threaded mode is ensured */
/* public API. */
void *fio_uuid_env_get FIO_NOOP(intptr_t uuid, fio_uuid_env_unset_args_s args) {
  fio_uuid_env_name_s n = FIO_SMALL_STR_INIT;
  fio_uuid_env_obj_s i = {.data = NULL};
  fio_uuid_env_name_set_const(&n, args.name.buf, args.name.len);
  if (!uuid_is_valid(uuid))
    goto invalid;
  fio_lock(&uuid_data(uuid).sock_lock);
  if (!uuid_is_valid(uuid))
    goto locked_invalid;
  i = fio___uuid_env_get(&uuid_data(uuid).env,
                         fio_uuid_env_name_hash(&n, args.type), n);
  fio_unlock(&uuid_data(uuid).sock_lock);
  return i.data;
locked_invalid:
  fio_unlock(&uuid_data(uuid).sock_lock);
invalid:
  errno = EBADF;
  return NULL;
}
#endif

void fio_uuid_env_unset___(void); /* for sublime text function listing */
/* public API. */
fio_uuid_env_args_s fio_uuid_env_unset
FIO_NOOP(intptr_t uuid, fio_uuid_env_unset_args_s args) {

  fio_uuid_env_name_s n = FIO_SMALL_STR_INIT;
  fio_uuid_env_name_set_const(&n, args.name.buf, args.name.len);
  if (!uuid_is_valid(uuid))
    goto invalid;
  fio_lock(&uuid_data(uuid).sock_lock);
  if (!uuid_is_valid(uuid))
    goto locked_invalid;
  fio_uuid_env_obj_s i = {.data = NULL};
  /* default object comparison is always true */
  int test = fio___uuid_env_remove(
      &uuid_data(uuid).env, fio_uuid_env_name_hash(&n, args.type), n, &i);
  if (test) {
    goto remove_error;
  }
  fio_unlock(&uuid_data(uuid).sock_lock);
  return (fio_uuid_env_args_s){
      .on_close = i.on_close,
      .data = i.data,
      .type = args.type,
      .name = args.name,
  };
locked_invalid:
  fio_unlock(&uuid_data(uuid).sock_lock);
invalid:
  errno = EBADF;
  return (fio_uuid_env_args_s){.type = 0};
remove_error:
  fio_unlock(&uuid_data(uuid).sock_lock);
  errno = ENOTCONN;
  return (fio_uuid_env_args_s){.type = 0};
}

/* *****************************************************************************
Section Start Marker











                         Default Thread / Fork handler

                           And Concurrency Helpers












***************************************************************************** */

/**
OVERRIDE THIS to replace the default `fork` implementation.

Behaves like the system's `fork`.
*/
#pragma weak fio_fork
int __attribute__((weak)) fio_fork(void) { return fork(); }

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer to a function and a single argument that should be executed
 * within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
#pragma weak fio_thread_new
void *__attribute__((weak))
fio_thread_new(void *(*thread_func)(void *), void *arg) {
  pthread_t *thread = malloc(sizeof(*thread));
  FIO_ASSERT_ALLOC(thread);
  if (pthread_create(thread, NULL, thread_func, arg))
    goto error;
  return thread;
error:
  free(thread);
  return NULL;
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
#pragma weak fio_thread_free
void __attribute__((weak)) fio_thread_free(void *p_thr) {
  if (*((pthread_t *)p_thr)) {
    pthread_detach(*((pthread_t *)p_thr));
  }
  free(p_thr);
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
#pragma weak fio_thread_join
int __attribute__((weak)) fio_thread_join(void *p_thr) {
  if (!p_thr || !(*((pthread_t *)p_thr)))
    return -1;
  pthread_join(*((pthread_t *)p_thr), NULL);
  *((pthread_t *)p_thr) = (pthread_t)NULL;
  free(p_thr);
  return 0;
}

/* *****************************************************************************
Suspending and renewing thread execution (signaling events)
***************************************************************************** */

#ifndef DEFER_THROTTLE
#define DEFER_THROTTLE 2097148UL
#endif
#ifndef FIO_DEFER_THROTTLE_LIMIT
#define FIO_DEFER_THROTTLE_LIMIT 134217472UL
#endif

/**
 * The polling throttling model will use pipes to suspend and resume threads...
 *
 * However, it seems the approach is currently broken, at least on macOS.
 * I don't know why.
 *
 * If polling is disabled, the progressive throttling model will be used.
 *
 * The progressive throttling makes concurrency and parallelism likely, but uses
 * progressive nano-sleep throttling system that is less exact.
 */
#ifndef FIO_DEFER_THROTTLE_POLL
#define FIO_DEFER_THROTTLE_POLL 0
#endif

typedef struct fio_thread_queue_data_s {
  FIO_LIST_NODE node;
  int fd_wait;   /* used for weaiting (read signal) */
  int fd_signal; /* used for signalling (write) */
} fio_thread_queue_s;

#define FIO_LIST_NAME fio_thread_queue
#include "fio-stl.h"

FIO_LIST_HEAD fio_thread_queue = FIO_LIST_INIT(fio_thread_queue);
static __thread fio_thread_queue_s fio_thread_data = {
    .fd_wait = -1,
    .fd_signal = -1,
};

static fio_lock_i fio_thread_lock = FIO_LOCK_INIT;

FIO_IFUNC void fio_thread_make_suspendable(void) {
  if (fio_thread_data.fd_signal >= 0)
    return;
  int fd[2] = {0, 0};
  int ret = pipe(fd);
  FIO_ASSERT(ret == 0, "`pipe` failed.");
  FIO_ASSERT(fio_set_non_block(fd[0]) == 0,
             "(fio) couldn't set internal pipe to non-blocking mode.");
  FIO_ASSERT(fio_set_non_block(fd[1]) == 0,
             "(fio) couldn't set internal pipe to non-blocking mode.");
  fio_thread_data.fd_wait = fd[0];
  fio_thread_data.fd_signal = fd[1];
}

FIO_IFUNC void fio_thread_cleanup(void) {
  if (fio_thread_data.fd_signal < 0)
    return;
  close(fio_thread_data.fd_wait);
  close(fio_thread_data.fd_signal);
  fio_thread_data.fd_wait = -1;
  fio_thread_data.fd_signal = -1;
}

/* suspend thread execution (might be resumed unexpectedly) */
FIO_SFUNC void fio_thread_suspend(void) {
  fio_lock(&fio_thread_lock);
  fio_thread_queue_push(&fio_thread_queue, &fio_thread_data);
  fio_unlock(&fio_thread_lock);
  struct pollfd list = {
      .events = (POLLPRI | POLLIN),
      .fd = fio_thread_data.fd_wait,
  };
  if (poll(&list, 1, 5000) > 0) {
    /* thread was removed from the list through signal */
    uint64_t data;
    int r = read(fio_thread_data.fd_wait, &data, sizeof(data));
    (void)r;
  } else {
    /* remove self from list */
    fio_lock(&fio_thread_lock);
    fio_thread_queue_remove(&fio_thread_data);
    fio_unlock(&fio_thread_lock);
  }
}

/* wake up a single thread */
FIO_SFUNC void fio_thread_signal(void) {
  fio_thread_queue_s *t;
  int fd = -2;
  fio_lock(&fio_thread_lock);
  t = (fio_thread_queue_s *)fio_thread_queue_shift(&fio_thread_queue);
  if (t)
    fd = t->fd_signal;
  fio_unlock(&fio_thread_lock);
  if (fd >= 0) {
    uint64_t data = 1;
    int r = write(fd, (void *)&data, sizeof(data));
    (void)r;
  } else if (fd == -1) {
    /* hardly the best way, but there's a thread sleeping on air */
    kill(getpid(), SIGCONT);
  }
}

/* wake up all threads */
FIO_SFUNC void fio_thread_broadcast(void) {
  while (fio_thread_queue_any(&fio_thread_queue)) {
    fio_thread_signal();
  }
}

static size_t fio_poll(void);
/**
 * A thread entering this function should wait for new evennts.
 */
static void fio_defer_thread_wait(void) {
#if FIO_ENGINE_POLL
  fio_poll();
  return;
#endif
  if (FIO_DEFER_THROTTLE_POLL) {
    fio_thread_suspend();
  } else {
    /* keeps threads active (concurrent), but reduces performance */
    static __thread size_t static_throttle = 262143UL;
    FIO_THREAD_WAIT(static_throttle);
    if (fio_defer_has_queue())
      static_throttle = 1;
    else if (static_throttle < FIO_DEFER_THROTTLE_LIMIT)
      static_throttle = (static_throttle << 1);
  }
}

static inline void fio_defer_on_thread_start(void) {
  if (FIO_DEFER_THROTTLE_POLL)
    fio_thread_make_suspendable();
}
static inline void fio_defer_thread_signal(void) {
  if (FIO_DEFER_THROTTLE_POLL)
    fio_thread_signal();
}
static inline void fio_defer_on_thread_end(void) {
  if (FIO_DEFER_THROTTLE_POLL) {
    fio_thread_broadcast();
    fio_thread_cleanup();
  }
}

/* *****************************************************************************
Section Start Marker














                             Task Management

                  Task / Event schduling and execution















***************************************************************************** */

#ifndef DEFER_QUEUE_BLOCK_COUNT
#if UINTPTR_MAX <= 0xFFFFFFFF
/* Almost a page of memory on most 32 bit machines: ((4096/4)-8)/3 */
#define DEFER_QUEUE_BLOCK_COUNT 338
#else
/* Almost a page of memory on most 64 bit machines: ((4096/8)-8)/3 */
#define DEFER_QUEUE_BLOCK_COUNT 168
#endif
#endif

/* task node data */
typedef struct {
  void (*func)(void *, void *);
  void *arg1;
  void *arg2;
} fio_defer_task_s;

/* task queue block */
typedef struct fio_defer_queue_block_s fio_defer_queue_block_s;
struct fio_defer_queue_block_s {
  fio_defer_task_s tasks[DEFER_QUEUE_BLOCK_COUNT];
  fio_defer_queue_block_s *next;
  size_t write;
  size_t read;
  unsigned char state;
};

/* task queue object */
typedef struct { /* a lock for the state machine, used for multi-threading
                    support */
  fio_lock_i lock;
  /* current active block to pop tasks */
  fio_defer_queue_block_s *reader;
  /* current active block to push tasks */
  fio_defer_queue_block_s *writer;
  /* static, built-in, queue */
  fio_defer_queue_block_s static_queue;
} fio_task_queue_s;

/* the state machine - this holds all the data about the task queue and pool */
static fio_task_queue_s task_queue_normal = {
    .reader = &task_queue_normal.static_queue,
    .writer = &task_queue_normal.static_queue};

static fio_task_queue_s task_queue_urgent = {
    .reader = &task_queue_urgent.static_queue,
    .writer = &task_queue_urgent.static_queue};

/* *****************************************************************************
Internal Task API
***************************************************************************** */

#if TEST || DEBUG
static size_t fio_defer_count_alloc, fio_defer_count_dealloc;
#define COUNT_ALLOC fio_atomic_add(&fio_defer_count_alloc, 1)
#define COUNT_DEALLOC fio_atomic_add(&fio_defer_count_dealloc, 1)
#define COUNT_RESET                                                            \
  do {                                                                         \
    fio_defer_count_alloc = fio_defer_count_dealloc = 0;                       \
  } while (0)
#else
#define COUNT_ALLOC
#define COUNT_DEALLOC
#define COUNT_RESET
#endif

static inline void fio_defer_push_task_fn(fio_defer_task_s task,
                                          fio_task_queue_s *queue) {
  fio_lock(&queue->lock);

  /* test if full */
  if (queue->writer->state && queue->writer->write == queue->writer->read) {
    /* return to static buffer or allocate new buffer */
    if (queue->static_queue.state == 2) {
      queue->writer->next = &queue->static_queue;
    } else {
      queue->writer->next = fio_malloc(sizeof(*queue->writer->next));
      COUNT_ALLOC;
      if (!queue->writer->next)
        goto critical_error;
    }
    queue->writer = queue->writer->next;
    queue->writer->write = 0;
    queue->writer->read = 0;
    queue->writer->state = 0;
    queue->writer->next = NULL;
  }

  /* place task and finish */
  queue->writer->tasks[queue->writer->write++] = task;
  /* cycle buffer */
  if (queue->writer->write == DEFER_QUEUE_BLOCK_COUNT) {
    queue->writer->write = 0;
    queue->writer->state = 1;
  }
  fio_unlock(&queue->lock);
  return;

critical_error:
  fio_unlock(&queue->lock);
  FIO_ASSERT_ALLOC(NULL)
}

#define fio_defer_push_task(func_, arg1_, arg2_)                               \
  do {                                                                         \
    fio_defer_push_task_fn(                                                    \
        (fio_defer_task_s){.func = func_, .arg1 = arg1_, .arg2 = arg2_},       \
        &task_queue_normal);                                                   \
    fio_defer_thread_signal();                                                 \
  } while (0)

#if FIO_USE_URGENT_QUEUE
#define fio_defer_push_urgent(func_, arg1_, arg2_)                             \
  fio_defer_push_task_fn(                                                      \
      (fio_defer_task_s){.func = func_, .arg1 = arg1_, .arg2 = arg2_},         \
      &task_queue_urgent)
#else
#define fio_defer_push_urgent(func_, arg1_, arg2_)                             \
  fio_defer_push_task(func_, arg1_, arg2_)
#endif

static inline fio_defer_task_s fio_defer_pop_task(fio_task_queue_s *queue) {
  fio_defer_task_s ret = (fio_defer_task_s){.func = NULL};
  fio_defer_queue_block_s *to_free = NULL;
  /* lock the state machine, grab/create a task and place it at the tail */
  fio_lock(&queue->lock);

  /* empty? */
  if (queue->reader->write == queue->reader->read && !queue->reader->state)
    goto finish;
  /* collect task */
  ret = queue->reader->tasks[queue->reader->read++];
  /* cycle */
  if (queue->reader->read == DEFER_QUEUE_BLOCK_COUNT) {
    queue->reader->read = 0;
    queue->reader->state = 0;
  }
  /* did we finish the queue in the buffer? */
  if (queue->reader->write == queue->reader->read) {
    if (queue->reader->next) {
      to_free = queue->reader;
      queue->reader = queue->reader->next;
    } else {
      if (queue->reader != &queue->static_queue &&
          queue->static_queue.state == 2) {
        to_free = queue->reader;
        queue->writer = &queue->static_queue;
        queue->reader = &queue->static_queue;
      }
      queue->reader->write = queue->reader->read = queue->reader->state = 0;
    }
  }

finish:
  if (to_free == &queue->static_queue) {
    queue->static_queue.state = 2;
    queue->static_queue.next = NULL;
  }
  fio_unlock(&queue->lock);

  if (to_free && to_free != &queue->static_queue) {
    fio_free(to_free);
    COUNT_DEALLOC;
  }
  return ret;
}

/* same as fio_defer_clear_queue , just inlined */
static inline void fio_defer_clear_tasks_for_queue(fio_task_queue_s *queue) {
  fio_lock(&queue->lock);
  while (queue->reader) {
    fio_defer_queue_block_s *tmp = queue->reader;
    queue->reader = queue->reader->next;
    if (tmp != &queue->static_queue) {
      COUNT_DEALLOC;
      free(tmp);
    }
  }
  queue->static_queue = (fio_defer_queue_block_s){.next = NULL};
  queue->reader = queue->writer = &queue->static_queue;
  fio_unlock(&queue->lock);
}

/**
 * Performs a single task from the queue, returning -1 if the queue was empty.
 */
static inline int
fio_defer_perform_single_task_for_queue(fio_task_queue_s *queue) {
  fio_defer_task_s task = fio_defer_pop_task(queue);
  if (!task.func)
    return -1;
  task.func(task.arg1, task.arg2);
  return 0;
}

static inline void fio_defer_clear_tasks(void) {
  fio_defer_clear_tasks_for_queue(&task_queue_normal);
#if FIO_USE_URGENT_QUEUE
  fio_defer_clear_tasks_for_queue(&task_queue_urgent);
#endif
}

static void fio_defer_on_fork(void) {
  task_queue_normal.lock = FIO_LOCK_INIT;
#if FIO_USE_URGENT_QUEUE
  task_queue_urgent.lock = FIO_LOCK_INIT;
#endif
}

/* *****************************************************************************
External Task API
***************************************************************************** */

/** Defer an execution of a function for later. */
int fio_defer(void (*func)(void *, void *), void *arg1, void *arg2) {
  /* must have a task to defer */
  if (!func)
    goto call_error;
  fio_defer_push_task(func, arg1, arg2);
  return 0;

call_error:
  return -1;
}

/** Performs all deferred functions until the queue had been depleted. */
void fio_defer_perform(void) {
#if FIO_USE_URGENT_QUEUE
  while (fio_defer_perform_single_task_for_queue(&task_queue_urgent) == 0 ||
         fio_defer_perform_single_task_for_queue(&task_queue_normal) == 0)
    ;
#else
  while (fio_defer_perform_single_task_for_queue(&task_queue_normal) == 0)
    ;
#endif
  //   for (;;) {
  // #if FIO_USE_URGENT_QUEUE
  //     fio_defer_task_s task = fio_defer_pop_task(&task_queue_urgent);
  //     if (!task.func)
  //       task = fio_defer_pop_task(&task_queue_normal);
  // #else
  //     fio_defer_task_s task = fio_defer_pop_task(&task_queue_normal);
  // #endif
  //     if (!task.func)
  //       return;
  //     task.func(task.arg1, task.arg2);
  //   }
}

/** Returns true if there are deferred functions waiting for execution. */
int fio_defer_has_queue(void) {
#if FIO_USE_URGENT_QUEUE
  return task_queue_urgent.reader != task_queue_urgent.writer ||
         task_queue_urgent.reader->write != task_queue_urgent.reader->read ||
         task_queue_normal.reader != task_queue_normal.writer ||
         task_queue_normal.reader->write != task_queue_normal.reader->read;
#else
  return task_queue_normal.reader != task_queue_normal.writer ||
         task_queue_normal.reader->write != task_queue_normal.reader->read;
#endif
}

/** Clears the queue. */
void fio_defer_clear_queue(void) { fio_defer_clear_tasks(); }

/* Thread pool task */
static void *fio_defer_cycle(void *ignr) {
  fio_defer_on_thread_start();
  for (;;) {
    fio_defer_perform();
    if (!fio_is_running())
      break;
    fio_defer_thread_wait();
  }
  fio_defer_on_thread_end();
  return ignr;
}

/* thread pool type */
typedef struct {
  size_t thread_count;
  void *threads[];
} fio_defer_thread_pool_s;

/* joins a thread pool */
static void fio_defer_thread_pool_join(fio_defer_thread_pool_s *pool) {
  for (size_t i = 0; i < pool->thread_count; ++i) {
    fio_thread_join(pool->threads[i]);
  }
  free(pool);
}

/* creates a thread pool */
static fio_defer_thread_pool_s *fio_defer_thread_pool_new(size_t count) {
  if (!count)
    count = 1;
  fio_defer_thread_pool_s *pool =
      malloc(sizeof(*pool) + (count * sizeof(void *)));
  FIO_ASSERT_ALLOC(pool);
  pool->thread_count = count;
  for (size_t i = 0; i < count; ++i) {
    pool->threads[i] = fio_thread_new(fio_defer_cycle, NULL);
    if (!pool->threads[i]) {
      pool->thread_count = i;
      goto error;
    }
  }
  return pool;
error:
  FIO_LOG_FATAL("couldn't spawn threads for thread pool, attempting shutdown.");
  fio_stop();
  fio_defer_thread_pool_join(pool);
  return NULL;
}

/* *****************************************************************************
Section Start Marker









                                     Timers










***************************************************************************** */

typedef struct {
  FIO_LIST_NODE node;
  struct timespec due;
  size_t interval; /*in ms */
  size_t repetitions;
  void (*task)(void *);
  void *arg;
  void (*on_finish)(void *);
} fio_timer_s;

#define FIO_MALLOC_TMP_USE_SYSTEM 1
#define FIO_LIST_NAME fio_timer
#include "fio-stl.h"

static FIO_LIST_HEAD fio_timers = FIO_LIST_INIT(fio_timers);
static fio_lock_i fio_timer_lock = FIO_LOCK_INIT;
static inline fio_timer_s *fio_timer_new() {
  fio_timer_s *t = malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  return t;
}
static inline void fio_timer_free(fio_timer_s *t) { free(t); }

/** Marks the current time as facil.io's cycle time */
static inline void fio_mark_time(void) {
  clock_gettime(CLOCK_REALTIME, &fio_data->last_cycle);
}

/** Calculates the due time for a task, given it's interval */
static struct timespec fio_timer_calc_due(size_t interval) {
  struct timespec now = fio_last_tick();
  if (interval >= 1000) {
    unsigned long long secs = interval / 1000;
    now.tv_sec += secs;
    interval -= secs * 1000;
  }
  now.tv_nsec += (interval * 1000000UL);
  if (now.tv_nsec >= 1000000000L) {
    now.tv_nsec -= 1000000000L;
    now.tv_sec += 1;
  }
  return now;
}

/** Returns the number of miliseconds until the next event, up to FIO_POLL_TICK
 */
static size_t fio_timer_calc_first_interval(void) {
  if (fio_defer_has_queue())
    return 0;
  if (fio_timer_is_empty(&fio_timers)) {
    return FIO_POLL_TICK;
  }
  struct timespec now = fio_last_tick();
  struct timespec due = fio_timer_root(fio_timers.next)->due;
  if (due.tv_sec < now.tv_sec ||
      (due.tv_sec == now.tv_sec && due.tv_nsec <= now.tv_nsec))
    return 0;
  size_t interval = 1000L * (due.tv_sec - now.tv_sec);
  if (due.tv_nsec >= now.tv_nsec) {
    interval += (due.tv_nsec - now.tv_nsec) / 1000000L;
  } else {
    interval -= (now.tv_nsec - due.tv_nsec) / 1000000L;
  }
  if (interval > FIO_POLL_TICK)
    interval = FIO_POLL_TICK;
  return interval;
}

/* simple a<=>b if "a" is bigger a negative result is returned, eq == 0. */
static int fio_timer_compare(struct timespec a, struct timespec b) {
  if (a.tv_sec == b.tv_sec) {
    if (a.tv_nsec < b.tv_nsec)
      return 1;
    if (a.tv_nsec > b.tv_nsec)
      return -1;
    return 0;
  }
  if (a.tv_sec < b.tv_sec)
    return 1;
  return -1;
}

/** Places a timer in an ordered linked list. */
static void fio_timer_add_order(fio_timer_s *timer) {
  timer->due = fio_timer_calc_due(timer->interval);
  fio_lock(&fio_timer_lock);
  FIO_LIST_EACH(fio_timer_s, node, &fio_timers, node) {
    if (fio_timer_compare(timer->due, node->due) >= 0) {
      fio_timer_push(&node->node, timer);
      goto finish;
    }
  }
  fio_timer_push(&fio_timers, timer);
finish:
  fio_unlock(&fio_timer_lock);
}

/** Performs a timer task and re-adds it to the queue (or cleans it up) */
static void fio_timer_perform_single(void *timer_, void *ignr) {
  fio_timer_s *timer = timer_;
  timer->task(timer->arg);
  if (!timer->repetitions || fio_atomic_sub(&timer->repetitions, 1))
    goto reschedule;
  if (timer->on_finish)
    timer->on_finish(timer->arg);
  fio_timer_free(timer);
  return;
  (void)ignr;
reschedule:
  fio_timer_add_order(timer);
}

/** schedules all timers that are due to be performed. */
static void fio_timer_schedule(void) {
  struct timespec now = fio_last_tick();
  fio_lock(&fio_timer_lock);
  while (fio_timer_any(&fio_timers) &&
         fio_timer_compare(fio_timer_root(fio_timers.next)->due, now) >= 0) {
    fio_timer_s *tmp = fio_timer_remove(fio_timer_root(fio_timers.next));
    fio_defer(fio_timer_perform_single, tmp, NULL);
  }
  fio_unlock(&fio_timer_lock);
}

static void fio_timer_clear_all(void) {
  fio_lock(&fio_timer_lock);
  while (fio_timer_any(&fio_timers)) {
    fio_timer_s *timer = fio_timer_pop(&fio_timers);
    if (timer->on_finish)
      timer->on_finish(timer->arg);
    fio_timer_free(timer);
  }
  fio_unlock(&fio_timer_lock);
}

/**
 * Creates a timer to run a task at the specified interval.
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error.
 *
 * The `on_finish` handler is always called (even on error).
 */
int fio_run_every(size_t milliseconds, size_t repetitions, void (*task)(void *),
                  void *arg, void (*on_finish)(void *)) {
  if (!task || (milliseconds == 0 && !repetitions))
    return -1;
  fio_timer_s *timer = fio_timer_new();
  FIO_ASSERT_ALLOC(timer);
  fio_mark_time();
  *timer = (fio_timer_s){
      .due = fio_timer_calc_due(milliseconds),
      .interval = milliseconds,
      .repetitions = repetitions,
      .task = task,
      .arg = arg,
      .on_finish = on_finish,
  };
  fio_timer_add_order(timer);
  return 0;
}

/* *****************************************************************************
Section Start Marker











                               Concurrency Helpers












***************************************************************************** */

volatile uint8_t fio_signal_children_flag = 0;
volatile fio_lock_i fio_signal_set_flag = 0;
/* store old signal handlers to propegate signal handling */
static struct sigaction fio_old_sig_chld;
static struct sigaction fio_old_sig_pipe;
static struct sigaction fio_old_sig_term;
static struct sigaction fio_old_sig_int;
#if !FIO_DISABLE_HOT_RESTART
static struct sigaction fio_old_sig_usr1;
#endif

/*
 * Zombie Reaping
 * With thanks to Dr Graham D Shaw.
 * http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
 */
static void reap_child_handler(int sig) {
  (void)(sig);
  int old_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = old_errno;
  if (fio_old_sig_chld.sa_handler != SIG_IGN &&
      fio_old_sig_chld.sa_handler != SIG_DFL)
    fio_old_sig_chld.sa_handler(sig);
}

/* initializes zombie reaping for the process */
void fio_reap_children(void) {
  struct sigaction sa;
  if (fio_old_sig_chld.sa_handler)
    return;
  sa.sa_handler = reap_child_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, &fio_old_sig_chld) == -1) {
    perror("Child reaping initialization failed");
    kill(0, SIGINT);
    exit(errno);
  }
}

/* handles the SIGUSR1, SIGINT and SIGTERM signals. */
static void sig_int_handler(int sig) {
  struct sigaction *old = NULL;
  switch (sig) {
#if !FIO_DISABLE_HOT_RESTART
  case SIGUSR1:
    fio_signal_children_flag = 1;
    old = &fio_old_sig_usr1;
    break;
#endif
    /* fallthrough */
  case SIGINT:
    if (!old)
      old = &fio_old_sig_int;
    /* fallthrough */
  case SIGTERM:
    if (!old)
      old = &fio_old_sig_term;
    fio_stop();
    break;
  case SIGPIPE:
    if (!old)
      old = &fio_old_sig_pipe;
  /* fallthrough */
  default:
    break;
  }
  /* propagate signale handling to previous existing handler (if any) */
  if (old && old->sa_handler != SIG_IGN && old->sa_handler != SIG_DFL)
    old->sa_handler(sig);
}

/* setup handling for the SIGUSR1, SIGPIPE, SIGINT and SIGTERM signals. */
static void fio_signal_handler_setup(void) {
  /* setup signal handling */
  struct sigaction act;
  if (fio_trylock(&fio_signal_set_flag))
    return;

  memset(&act, 0, sizeof(act));

  act.sa_handler = sig_int_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if (sigaction(SIGINT, &act, &fio_old_sig_int)) {
    perror("couldn't set signal handler");
    return;
  };

  if (sigaction(SIGTERM, &act, &fio_old_sig_term)) {
    perror("couldn't set signal handler");
    return;
  };
#if !FIO_DISABLE_HOT_RESTART
  if (sigaction(SIGUSR1, &act, &fio_old_sig_usr1)) {
    perror("couldn't set signal handler");
    return;
  };
#endif

  act.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &act, &fio_old_sig_pipe)) {
    perror("couldn't set signal handler");
    return;
  };
}

void fio_signal_handler_reset(void) {
  struct sigaction old;
  if (fio_signal_set_flag)
    return;
  fio_unlock(&fio_signal_set_flag);
  memset(&old, 0, sizeof(old));
  sigaction(SIGINT, &fio_old_sig_int, &old);
  sigaction(SIGTERM, &fio_old_sig_term, &old);
  sigaction(SIGPIPE, &fio_old_sig_pipe, &old);
  if (fio_old_sig_chld.sa_handler)
    sigaction(SIGCHLD, &fio_old_sig_chld, &old);
#if !FIO_DISABLE_HOT_RESTART
  sigaction(SIGUSR1, &fio_old_sig_usr1, &old);
  memset(&fio_old_sig_usr1, 0, sizeof(fio_old_sig_usr1));
#endif
  memset(&fio_old_sig_int, 0, sizeof(fio_old_sig_int));
  memset(&fio_old_sig_term, 0, sizeof(fio_old_sig_term));
  memset(&fio_old_sig_pipe, 0, sizeof(fio_old_sig_pipe));
  memset(&fio_old_sig_chld, 0, sizeof(fio_old_sig_chld));
}

/**
 * Returns 1 if the current process is a worker process or a single process.
 *
 * Otherwise returns 0.
 *
 * NOTE: When cluster mode is off, the root process is also the worker process.
 *       This means that single process instances don't automatically respawn
 *       after critical errors.
 */
int fio_is_worker(void) { return fio_data->is_worker; }

/**
 * Returns 1 if the current process is the master (root) process.
 *
 * Otherwise returns 0.
 */
int fio_is_master(void) {
  return fio_data->is_worker == 0 || fio_data->workers == 1;
}

/** returns facil.io's parent (root) process pid. */
pid_t fio_master_pid(void) { return fio_data->parent; }

static inline size_t fio_detect_cpu_cores(void) {
  ssize_t cpu_count = 0;
#ifdef _SC_NPROCESSORS_ONLN
  cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
  if (cpu_count < 0) {
    FIO_LOG_WARNING("CPU core count auto-detection failed.");
    return 0;
  }
#else
  FIO_LOG_WARNING("CPU core count auto-detection failed.");
#endif
  return cpu_count;
}

/**
 * Returns the number of expected threads / processes to be used by facil.io.
 *
 * The pointers should start with valid values that match the expected threads /
 * processes values passed to `fio_run`.
 *
 * The data in the pointers will be overwritten with the result.
 */
void fio_expected_concurrency(int16_t *threads, int16_t *processes) {
  if (!threads || !processes)
    return;
  if (!*threads && !*processes) {
    /* both options set to 0 - default to cores*cores matrix */
    ssize_t cpu_count = fio_detect_cpu_cores();
#if FIO_CPU_CORES_LIMIT
    if (cpu_count > FIO_CPU_CORES_LIMIT) {
      static int print_cores_warning = 1;
      if (print_cores_warning) {
        FIO_LOG_WARNING(
            "Detected %zu cores. Capping auto-detection of cores to %zu.\n"
            "      Avoid this message by setting threads / workers manually.\n"
            "      To increase auto-detection limit, recompile with:\n"
            "             -DFIO_CPU_CORES_LIMIT=%zu",
            (size_t)cpu_count, (size_t)FIO_CPU_CORES_LIMIT, (size_t)cpu_count);
        print_cores_warning = 0;
      }
      cpu_count = FIO_CPU_CORES_LIMIT;
    }
#endif
    *threads = *processes = (int16_t)cpu_count;
    if (cpu_count > 3) {
      /* leave a core available for the kernel */
      --(*processes);
    }
  } else if (*threads < 0 || *processes < 0) {
    /* Set any option that is less than 0 be equal to cores/value */
    /* Set any option equal to 0 be equal to the other option in value */
    ssize_t cpu_count = fio_detect_cpu_cores();
    size_t thread_cpu_adjust = (*threads <= 0 ? 1 : 0);
    size_t worker_cpu_adjust = (*processes <= 0 ? 1 : 0);

    if (cpu_count > 0) {
      int16_t tmp = 0;
      if (*threads < 0)
        tmp = (int16_t)(cpu_count / (*threads * -1));
      else if (*threads == 0) {
        tmp = -1 * *processes;
        thread_cpu_adjust = 0;
      } else
        tmp = *threads;
      if (*processes < 0)
        *processes = (int16_t)(cpu_count / (*processes * -1));
      else if (*processes == 0) {
        *processes = -1 * *threads;
        worker_cpu_adjust = 0;
      }
      *threads = tmp;
      tmp = *processes;
      if (worker_cpu_adjust && (*processes * *threads) >= cpu_count &&
          cpu_count > 3) {
        /* leave a resources available for the kernel */
        --*processes;
      }
      if (thread_cpu_adjust && (*threads * tmp) >= cpu_count && cpu_count > 3) {
        /* leave a resources available for the kernel */
        --*threads;
      }
    }
  }

  /* make sure we have at least one process and at least one thread */
  if (*processes <= 0)
    *processes = 1;
  if (*threads <= 0)
    *threads = 1;
}

static fio_lock_i fio_fork_lock = FIO_LOCK_INIT;

/* *****************************************************************************
Section Start Marker













                       Polling State Machine - epoll














***************************************************************************** */
#if FIO_ENGINE_EPOLL
#include <sys/epoll.h>

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "epoll"; }

/* epoll tester, in and out */
static int evio_fd[3] = {-1, -1, -1};

static void fio_poll_close(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

static void fio_poll_init(void) {
  fio_poll_close();
  for (int i = 0; i < 3; ++i) {
    evio_fd[i] = epoll_create1(EPOLL_CLOEXEC);
    if (evio_fd[i] == -1)
      goto error;
  }
  for (int i = 1; i < 3; ++i) {
    struct epoll_event chevent = {
        .events = (EPOLLOUT | EPOLLIN),
        .data.fd = evio_fd[i],
    };
    if (epoll_ctl(evio_fd[0], EPOLL_CTL_ADD, evio_fd[i], &chevent) == -1)
      goto error;
  }
  return;
error:
  FIO_LOG_FATAL("couldn't initialize epoll.");
  fio_poll_close();
  exit(errno);
  return;
}

static inline int fio_poll_add2(int fd, uint32_t events, int ep_fd) {
  struct epoll_event chevent;
  int ret;
  do {
    errno = 0;
    chevent = (struct epoll_event){
        .events = events,
        .data.fd = fd,
    };
    ret = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &chevent);
    if (ret == -1 && errno == ENOENT) {
      errno = 0;
      chevent = (struct epoll_event){
          .events = events,
          .data.fd = fd,
      };
      ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &chevent);
    }
  } while (errno == EINTR);

  return ret;
}

static inline void fio_poll_add_read(intptr_t fd) {
  fio_poll_add2(fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                evio_fd[1]);
  return;
}

static inline void fio_poll_add_write(intptr_t fd) {
  fio_poll_add2(fd, (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                evio_fd[2]);
  return;
}

static inline void fio_poll_add(intptr_t fd) {
  if (fio_poll_add2(fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                    evio_fd[1]) == -1)
    return;
  fio_poll_add2(fd, (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                evio_fd[2]);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN), .data.fd = fd};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

static size_t fio_poll(void) {
  int timeout_millisec = fio_timer_calc_first_interval();
  struct epoll_event internal[2];
  struct epoll_event events[FIO_POLL_MAX_EVENTS];
  int total = 0;
  /* wait for events and handle them */
  int internal_count = epoll_wait(evio_fd[0], internal, 2, timeout_millisec);
  if (internal_count == 0)
    return internal_count;
  for (int j = 0; j < internal_count; ++j) {
    int active_count =
        epoll_wait(internal[j].data.fd, events, FIO_POLL_MAX_EVENTS, 0);
    if (active_count > 0) {
      for (int i = 0; i < active_count; i++) {
        if (events[i].events & (~(EPOLLIN | EPOLLOUT))) {
          // errors are hendled as disconnections (on_close)
          fio_force_close_in_poll(fd2uuid(events[i].data.fd));
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLOUT) {
            fio_defer_push_urgent(deferred_on_ready,
                                  (void *)fd2uuid(events[i].data.fd), NULL);
          }
          if (events[i].events & EPOLLIN)
            fio_defer_push_task(deferred_on_data,
                                (void *)fd2uuid(events[i].data.fd), NULL);
        }
      } // end for loop
      total += active_count;
    }
  }
  return total;
}

#endif
/* *****************************************************************************
Section Start Marker













                       Polling State Machine - kqueue














***************************************************************************** */
#if FIO_ENGINE_KQUEUE
#include <sys/event.h>

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "kqueue"; }

static int evio_fd = -1;

static void fio_poll_close(void) { close(evio_fd); }

static void fio_poll_init(void) {
  fio_poll_close();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

static inline void fio_poll_add_read(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

static inline void fio_poll_add_write(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

static inline void fio_poll_add(intptr_t fd) {
  struct kevent chevent[2];
  EV_SET(chevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, ((void *)fd));
  EV_SET(chevent + 1, fd, EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 2, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  if (evio_fd < 0)
    return;
  struct kevent chevent[2];
  EV_SET(chevent, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  do {
    errno = 0;
    kevent(evio_fd, chevent, 2, NULL, 0, NULL);
  } while (errno == EINTR);
}

static size_t fio_poll(void) {
  if (evio_fd < 0)
    return -1;
  int timeout_millisec = fio_timer_calc_first_interval();
  struct kevent events[FIO_POLL_MAX_EVENTS] = {{0}};

  const struct timespec timeout = {
      .tv_sec = (timeout_millisec / 1000),
      .tv_nsec = ((timeout_millisec & (~1023UL)) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, FIO_POLL_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_WRITE) {
        fio_defer_push_urgent(deferred_on_ready,
                              ((void *)fd2uuid(events[i].udata)), NULL);
      } else if (events[i].filter == EVFILT_READ) {
        fio_defer_push_task(deferred_on_data, (void *)fd2uuid(events[i].udata),
                            NULL);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio_force_close_in_poll(fd2uuid(events[i].udata));
      }
    }
  } else if (active_count < 0) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
  return active_count;
}

#endif
/* *****************************************************************************
Section Start Marker













                       Polling State Machine - poll














***************************************************************************** */

#if FIO_ENGINE_POLL

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "poll"; }

#define FIO_POLL_READ_EVENTS (POLLPRI | POLLIN)
#define FIO_POLL_WRITE_EVENTS (POLLOUT)

static void fio_poll_close(void) {}

static void fio_poll_init(void) {}

static inline void fio_poll_remove_fd(int fd) {
  fio_data->poll[fd].fd = -1;
  fio_data->poll[fd].events = 0;
}

static inline void fio_poll_add_read(int fd) {
  fio_data->poll[fd].fd = fd;
  fio_data->poll[fd].events |= FIO_POLL_READ_EVENTS;
}

static inline void fio_poll_add_write(int fd) {
  fio_data->poll[fd].fd = fd;
  fio_data->poll[fd].events |= FIO_POLL_WRITE_EVENTS;
}

static inline void fio_poll_add(int fd) {
  fio_data->poll[fd].fd = fd;
  fio_data->poll[fd].events = FIO_POLL_READ_EVENTS | FIO_POLL_WRITE_EVENTS;
}

static inline void fio_poll_remove_read(int fd) {
  fio_lock(&fio_data->lock);
  if (fio_data->poll[fd].events & FIO_POLL_WRITE_EVENTS)
    fio_data->poll[fd].events = FIO_POLL_WRITE_EVENTS;
  else {
    fio_poll_remove_fd(fd);
  }
  fio_unlock(&fio_data->lock);
}

static inline void fio_poll_remove_write(int fd) {
  fio_lock(&fio_data->lock);
  if (fio_data->poll[fd].events & FIO_POLL_READ_EVENTS)
    fio_data->poll[fd].events = FIO_POLL_READ_EVENTS;
  else {
    fio_poll_remove_fd(fd);
  }
  fio_unlock(&fio_data->lock);
}

/** returns non-zero if events were scheduled, 0 if idle */
static size_t fio_poll(void) {
  /* shrink fd poll range */
  size_t end = fio_data->capa; // max_protocol_fd might break TLS
  size_t start = 0;
  struct pollfd *list = NULL;
  fio_lock(&fio_data->lock);
  while (start < end && fio_data->poll[start].fd == -1)
    ++start;
  while (start < end && fio_data->poll[end - 1].fd == -1)
    --end;
  if (start != end) {
    /* copy poll list for multi-threaded poll */
    list = fio_malloc(sizeof(struct pollfd) * end);
    memcpy(list + start, fio_data->poll + start,
           (sizeof(struct pollfd)) * (end - start));
  }
  fio_unlock(&fio_data->lock);

  int timeout = fio_timer_calc_first_interval();
  size_t count = 0;

  if (start == end) {
    FIO_THREAD_WAIT((timeout * 1000000UL));
  } else if (poll(list + start, end - start, timeout) == -1) {
    goto finish;
  }
  for (size_t i = start; i < end; ++i) {
    if (list[i].revents) {
      touchfd(i);
      ++count;
      if (list[i].revents & FIO_POLL_WRITE_EVENTS) {
        // FIO_LOG_DEBUG("Poll Write %zu => %p", i, (void *)fd2uuid(i));
        fio_poll_remove_write(i);
        fio_defer_push_urgent(deferred_on_ready, (void *)fd2uuid(i), NULL);
      }
      if (list[i].revents & FIO_POLL_READ_EVENTS) {
        // FIO_LOG_DEBUG("Poll Read %zu => %p", i, (void *)fd2uuid(i));
        fio_poll_remove_read(i);
        fio_defer_push_task(deferred_on_data, (void *)fd2uuid(i), NULL);
      }
      if (list[i].revents & (POLLHUP | POLLERR)) {
        // FIO_LOG_DEBUG("Poll Hangup %zu => %p", i, (void *)fd2uuid(i));
        fio_poll_remove_fd(i);
        fio_force_close_in_poll(fd2uuid(i));
      }
      if (list[i].revents & POLLNVAL) {
        // FIO_LOG_DEBUG("Poll Invalid %zu => %p", i, (void *)fd2uuid(i));
        fio_poll_remove_fd(i);
        fio_lock(&fd_data(i).protocol_lock);
        fio_clear_fd(i, 0);
        fio_unlock(&fd_data(i).protocol_lock);
      }
    }
  }
finish:
  fio_free(list);
  return count;
}

#endif /* FIO_ENGINE_POLL */

/* *****************************************************************************
Section Start Marker












                         IO Callbacks / Event Handling













***************************************************************************** */

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

static uint8_t mock_on_shutdown_eternal(intptr_t uuid,
                                        fio_protocol_s *protocol) {
  return 255;
  (void)protocol;
  (void)uuid;
}

static void mock_ping(intptr_t uuid, fio_protocol_s *protocol) {
  (void)protocol;
  fio_force_close(uuid);
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

/** A ping function that does nothing except keeping the connection alive. */
void FIO_PING_ETERNAL(intptr_t uuid, fio_protocol_s *protocol) {
  (void)protocol;
  fio_touch(uuid);
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
  fio_defer_push_task(deferred_on_close, uuid_, pr_);
}

static void deferred_on_shutdown(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol) {
    return;
  }
  fio_protocol_s *pr = protocol_try_lock(fio_uuid2fd(arg), FIO_PR_LOCK_TASK);
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
      fio_atomic_add(&fio_data->connection_count, 1);
      uuid_data(arg).timeout = r;
    }
    pr->ping = mock_ping2;
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
  } else {
    fio_atomic_add(&fio_data->connection_count, 1);
    uuid_data(arg).timeout = 8;
    pr->ping = mock_ping;
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
    fio_close((intptr_t)arg);
  }
  return;
postpone:
  fio_defer_push_task(deferred_on_shutdown, arg, NULL);
  (void)arg2;
}

static void deferred_on_ready_usr(void *arg, void *arg2) {
  errno = 0;
  fio_protocol_s *pr = protocol_try_lock(fio_uuid2fd(arg), FIO_PR_LOCK_WRITE);
  if (!pr) {
    if (errno == EBADF)
      return;
    goto postpone;
  }
  pr->on_ready((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_WRITE);
  return;
postpone:
  fio_defer_push_task(deferred_on_ready, arg, NULL);
  (void)arg2;
}

static void deferred_on_ready(void *arg, void *arg2) {
  errno = 0;
  if (fio_flush((intptr_t)arg) > 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
    if (arg2)
      fio_defer_push_urgent(deferred_on_ready, arg, NULL);
    else
      fio_poll_add_write(fio_uuid2fd(arg));
    return;
  }
  if (!uuid_data(arg).protocol) {
    return;
  }

  fio_defer_push_task(deferred_on_ready_usr, arg, NULL);
}

static void deferred_on_data(void *uuid, void *arg2) {
  if (fio_is_closed((intptr_t)uuid)) {
    return;
  }
  if (!uuid_data(uuid).protocol)
    goto no_protocol;
  fio_protocol_s *pr = protocol_try_lock(fio_uuid2fd(uuid), FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF) {
      return;
    }
    goto postpone;
  }
  fio_unlock(&uuid_data(uuid).scheduled);
  pr->on_data((intptr_t)uuid, pr);
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
  if (!fio_trylock(&uuid_data(uuid).scheduled)) {
    fio_poll_add_read(fio_uuid2fd((intptr_t)uuid));
  }
  return;

postpone:
  if (arg2) {
    /* the event is being forced, so force rescheduling */
    fio_defer_push_task(deferred_on_data, (void *)uuid, (void *)1);
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
  if (!uuid_data(arg).protocol ||
      (uuid_data(arg).timeout &&
       (uuid_data(arg).timeout + uuid_data(arg).active >
        (fio_data->last_cycle.tv_sec)))) {
    return;
  }
  fio_protocol_s *pr = protocol_try_lock(fio_uuid2fd(arg), FIO_PR_LOCK_WRITE);
  if (!pr)
    goto postpone;
  pr->ping((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_WRITE);
  return;
postpone:
  fio_defer_push_task(deferred_ping, arg, NULL);
  (void)arg2;
}

/* *****************************************************************************
Forcing / Suspending IO events
***************************************************************************** */

void fio_force_event(intptr_t uuid, enum fio_io_event ev) {
  if (!uuid_is_valid(uuid))
    return;
  switch (ev) {
  case FIO_EVENT_ON_DATA:
    fio_trylock(&uuid_data(uuid).scheduled);
    fio_defer_push_task(deferred_on_data, (void *)uuid, (void *)1);
    break;
  case FIO_EVENT_ON_TIMEOUT:
    fio_defer_push_task(deferred_ping, (void *)uuid, NULL);
    break;
  case FIO_EVENT_ON_READY:
    fio_defer_push_urgent(deferred_on_ready, (void *)uuid, NULL);
    break;
  }
}

void fio_suspend(intptr_t uuid) {
  if (uuid_is_valid(uuid))
    fio_trylock(&uuid_data(uuid).scheduled);
}

/* *****************************************************************************
Section Start Marker












                               IO Socket Layer

                     Read / Write / Accept / Connect / etc'













***************************************************************************** */

/* *****************************************************************************
Internal socket initialization functions
***************************************************************************** */

/**
Sets a socket to non blocking state.

This function is called automatically for the new socket, when using
`fio_accept` or `fio_connect`.
*/
int fio_set_non_block(int fd) { return fio_sock_set_non_block(fd); }

static void fio_tcp_addr_cpy(int fd, int family, struct sockaddr *addrinfo) {
  const char *result =
      inet_ntop(family,
                family == AF_INET
                    ? (void *)&(((struct sockaddr_in *)addrinfo)->sin_addr)
                    : (void *)&(((struct sockaddr_in6 *)addrinfo)->sin6_addr),
                (char *)fd_data(fd).addr, sizeof(fd_data(fd).addr));
  if (result) {
    fd_data(fd).addr_len = strlen((char *)fd_data(fd).addr);
  } else {
    fd_data(fd).addr_len = 0;
    fd_data(fd).addr[0] = 0;
  }
}

/**
 * `fio_accept` accepts a new socket connection from a server socket - see the
 * server flag on `fio_socket`.
 *
 * NOTE: this function does NOT attach the socket to the IO reactor -see
 * `fio_attach`.
 */
intptr_t fio_accept(intptr_t srv_uuid) {
  struct sockaddr_in6 addrinfo[2]; /* grab a slice of stack (aligned) */
  socklen_t addrlen = sizeof(addrinfo);
  int client;
#ifdef SOCK_NONBLOCK
  client = accept4(fio_uuid2fd(srv_uuid), (struct sockaddr *)addrinfo, &addrlen,
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

  fio_lock(&fd_data(client).protocol_lock);
  fio_clear_fd(client, 1);
  fio_unlock(&fd_data(client).protocol_lock);
  /* copy peer address */
  if (((struct sockaddr *)addrinfo)->sa_family == AF_UNIX) {
    fd_data(client).addr_len = uuid_data(srv_uuid).addr_len;
    if (uuid_data(srv_uuid).addr_len) {
      memcpy(fd_data(client).addr, uuid_data(srv_uuid).addr,
             uuid_data(srv_uuid).addr_len + 1);
    }
  } else {
    fio_tcp_addr_cpy(client, ((struct sockaddr *)addrinfo)->sa_family,
                     (struct sockaddr *)addrinfo);
  }

  return fd2uuid(client);
}

/**
 * Creates a Unix or a TCP/IP socket and returns it's unique identifier.
 *
 * For TCP/IP or UDP server sockets (flag sets `FIO_SOCKET_SERVER`), a NULL
 * `address` variable is recommended. Use "localhost" or "127.0.0.1" to limit
 * access to the server application.
 *
 * For TCP/IP or UDP client sockets (flag sets `FIO_SOCKET_CLIENT`), a remote
 * `address` and `port` combination will be required.
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
 * - FIO_SOCKET_SERVER - Sets the socket to server mode (may call `listen`).
 * - FIO_SOCKET_CLIENT - Sets the socket to client mode (calls `connect).
 * - FIO_SOCKET_NONBLOCK - Sets the socket to non-blocking mode.
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
intptr_t fio_socket(const char *address, const char *port,
                    fio_socket_flags_e flags) {
  /* TCP/IP is the  default socket mode */
  if (!(flags & ((uint16_t)FIO_SOCKET_TCP | (uint16_t)FIO_SOCKET_UDP |
                 (uint16_t)FIO_SOCKET_UNIX)))
    flags |= FIO_SOCKET_TCP;
  int fd = fio_sock_open(address, port, flags);
  if (fd == -1) {
    FIO_LOG_DEBUG2("Couldn't open a socket for %s : %s (flags: %u)\r\n\t",
                   (address ? address : "---"), (port ? port : "0"),
                   (unsigned int)flags, strerror(errno));
    return -1;
  }

#if defined(TCP_FASTOPEN) && defined(IPPROTO_TCP)
  if ((flags & (FIO_SOCKET_SERVER | FIO_SOCKET_TCP)) ==
      (FIO_SOCKET_SERVER | FIO_SOCKET_TCP)) {
    // support TCP Fast Open when available
    int optval = 128;
    setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &optval, sizeof(optval));
  }
#endif

  fio_lock(&fd_data(fd).protocol_lock);
  fio_clear_fd(fd, 1);
  fio_unlock(&fd_data(fd).protocol_lock);
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
  /* TODO: setup default read/write hooks for UDP */
  return fd2uuid(fd);
}

/* *****************************************************************************
Internal socket flushing related functions
***************************************************************************** */

#ifndef BUFFER_FILE_READ_SIZE
#define BUFFER_FILE_READ_SIZE 49152
#endif

#if !defined(USE_SENDFILE) && !defined(USE_SENDFILE_LINUX) &&                  \
    !defined(USE_SENDFILE_BSD) && !defined(USE_SENDFILE_APPLE)
#if defined(__linux__) /* linux sendfile works  */
#define USE_SENDFILE_LINUX 1
#elif defined(__FreeBSD__) /* FreeBSD sendfile should work - not tested */
#define USE_SENDFILE_BSD 1
#elif defined(__APPLE__) /* Is the apple sendfile still broken? */
#define USE_SENDFILE_APPLE 2
#else /* sendfile might not be available - always set to 0 */
#define USE_SENDFILE 0
#endif

#endif

static void fio_sock_perform_close_fd(intptr_t fd) { close(fd); }

static inline void fio_sock_packet_rotate_unsafe(uintptr_t fd) {
  fio_packet_s *packet = fd_data(fd).packet;
  fd_data(fd).packet = packet->next;
  fio_atomic_sub(&fd_data(fd).packet_count, 1);
  if (!packet->next) {
    fd_data(fd).packet_last = &fd_data(fd).packet;
    fd_data(fd).packet_count = 0;
  } else if (&packet->next == fd_data(fd).packet_last) {
    fd_data(fd).packet_last = &fd_data(fd).packet;
  }
  fio_packet_free(packet);
}

static int fio_sock_write_buf(int fd, fio_packet_s *packet) {
  int written = fd_data(fd).rw_hooks->write(
      fd2uuid(fd), fd_data(fd).rw_udata,
      ((uint8_t *)packet->data.buf + packet->offset), packet->len);
  if (written > 0) {
    packet->len -= written;
    packet->offset += written;
    if (!packet->len) {
      fio_sock_packet_rotate_unsafe(fd);
    }
  }
  return written;
}

static int fio_sock_write_from_fd(int fd, fio_packet_s *packet) {
  ssize_t asked = 0;
  ssize_t sent = 0;
  ssize_t total = 0;
  char buff[BUFFER_FILE_READ_SIZE];
  do {
    packet->offset += sent;
    packet->len -= sent;
  retry:
    asked =
        pread(packet->data.fd, buff,
              ((packet->len < BUFFER_FILE_READ_SIZE) ? packet->len
                                                     : BUFFER_FILE_READ_SIZE),
              packet->offset);
    if (asked <= 0)
      goto read_error;
    sent = fd_data(fd).rw_hooks->write(fd2uuid(fd), fd_data(fd).rw_udata, buff,
                                       asked);
  } while (sent == asked && packet->len);
  if (sent >= 0) {
    packet->offset += sent;
    packet->len -= sent;
    total += sent;
    if (!packet->len) {
      fio_sock_packet_rotate_unsafe(fd);
      return 1;
    }
  }
  return total;

read_error:
  if (sent == 0) {
    fio_sock_packet_rotate_unsafe(fd);
    return 1;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    goto retry;
  return -1;
}

#if USE_SENDFILE_LINUX /* linux sendfile API */
#include <sys/sendfile.h>

static int fio_sock_sendfile_from_fd(int fd, fio_packet_s *packet) {
  ssize_t sent;
  sent = sendfile64(fd, packet->data.fd, (off_t *)&packet->offset, packet->len);
  if (sent < 0)
    return -1;
  packet->len -= sent;
  if (!packet->len)
    fio_sock_packet_rotate_unsafe(fd);
  return sent;
}

#elif USE_SENDFILE_BSD || USE_SENDFILE_APPLE /* FreeBSD / Apple API */
#include <sys/uio.h>

static int fio_sock_sendfile_from_fd(int fd, fio_packet_s *packet) {
  off_t act_sent = 0;
  ssize_t ret = 0;
  while (packet->len) {
    act_sent = packet->len;
#if USE_SENDFILE_APPLE
    ret = sendfile(packet->data.fd, fd, packet->offset, &act_sent, NULL, 0);
#else
    ret = sendfile(packet->data.fd, fd, packet->offset, (size_t)act_sent, NULL,
                   &act_sent, 0);
#endif
    if (ret < 0)
      goto error;
    packet->len -= act_sent;
    packet->offset += act_sent;
  }
  fio_sock_packet_rotate_unsafe(fd);
  return act_sent;
error:
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    packet->len -= act_sent;
    packet->offset += act_sent;
  }
  return -1;
}

#else
static int (*fio_sock_sendfile_from_fd)(int fd, fio_packet_s *packet) =
    fio_sock_write_from_fd;

#endif

/* *****************************************************************************
Socket / Connection Functions
***************************************************************************** */

/**
 * Returns the information available about the socket's peer address.
 *
 * If no information is available, the struct will be initialized with zero
 * (`addr == NULL`).
 * The information is only available when the socket was accepted using
 * `fio_accept` or opened using `fio_connect`.
 */

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
ssize_t fio_read(intptr_t uuid, void *buf, size_t count) {
  if (!uuid_is_valid(uuid) || !uuid_data(uuid).open) {
    errno = EBADF;
    return -1;
  }
  if (count == 0)
    return 0;
  fio_lock(&uuid_data(uuid).sock_lock);
  ssize_t (*rw_read)(intptr_t, void *, void *, size_t) =
      uuid_data(uuid).rw_hooks->read;
  void *udata = uuid_data(uuid).rw_udata;
  fio_unlock(&uuid_data(uuid).sock_lock);
  int old_errno = errno;
  ssize_t ret;
retry_int:
  ret = rw_read(uuid, udata, buf, count);
  if (ret > 0) {
    fio_touch(uuid);
    return ret;
  }
  if (ret < 0 && errno == EINTR)
    goto retry_int;
  if (ret < 0 &&
      (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOTCONN)) {
    errno = old_errno;
    return 0;
  }
  fio_force_close(uuid);
  return -1;
}

/**
 * `fio_write2_fn` is the actual function behind the macro `fio_write2`.
 */
ssize_t fio_write2_fn(intptr_t uuid, fio_write_args_s options) {
  if (!uuid_is_valid(uuid))
    goto error;

  /* create packet */
  fio_packet_s *packet = fio_packet_alloc();
  *packet = (fio_packet_s){
      .len = options.len,
      .offset = options.offset,
      .data.buf = (void *)options.data.buf,
  };
  if (options.is_fd) {
    packet->write_func = (uuid_data(uuid).rw_hooks == &FIO_DEFAULT_RW_HOOKS)
                             ? fio_sock_sendfile_from_fd
                             : fio_sock_write_from_fd;
    packet->dealloc =
        (options.after.dealloc ? options.after.dealloc
                               : (void (*)(void *))fio_sock_perform_close_fd);
  } else {
    packet->write_func = fio_sock_write_buf;
    packet->dealloc = (options.after.dealloc ? options.after.dealloc : free);
  }
  /* add packet to outgoing list */
  uint8_t was_empty = 1;
  fio_lock(&uuid_data(uuid).sock_lock);
  if (!uuid_is_valid(uuid)) {
    goto locked_error;
  }
  if (uuid_data(uuid).packet)
    was_empty = 0;
  if (options.urgent == 0) {
    *uuid_data(uuid).packet_last = packet;
    uuid_data(uuid).packet_last = &packet->next;
  } else {
    fio_packet_s **pos = &uuid_data(uuid).packet;
    if (*pos)
      pos = &(*pos)->next;
    packet->next = *pos;
    *pos = packet;
    if (!packet->next) {
      uuid_data(uuid).packet_last = &packet->next;
    }
  }
  fio_atomic_add(&uuid_data(uuid).packet_count, 1);
  fio_unlock(&uuid_data(uuid).sock_lock);

  if (was_empty) {
    touchfd(fio_uuid2fd(uuid));
    deferred_on_ready((void *)uuid, (void *)1);
  }
  return 0;
locked_error:
  fio_unlock(&uuid_data(uuid).sock_lock);
  fio_packet_free(packet);
  errno = EBADF;
  return -1;
error:
  if (options.after.dealloc) {
    options.after.dealloc((void *)options.data.buf);
  }
  errno = EBADF;
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
  return uuid_data(uuid).packet_count;
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
  if (uuid_data(uuid).packet || uuid_data(uuid).sock_lock) {
    uuid_data(uuid).close = 1;
    fio_poll_add_write(fio_uuid2fd(uuid));
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
  fio_packet_s *packet = NULL;
  fio_lock(&uuid_data(uuid).sock_lock);
  packet = uuid_data(uuid).packet;
  uuid_data(uuid).packet = NULL;
  uuid_data(uuid).packet_last = &uuid_data(uuid).packet;
  uuid_data(uuid).sent = 0;
  fio_unlock(&uuid_data(uuid).sock_lock);
  while (packet) {
    fio_packet_s *tmp = packet;
    packet = packet->next;
    fio_packet_free(tmp);
  }
  /* check for rw-hooks termination packet */
  if (uuid_data(uuid).open && (uuid_data(uuid).close & 1) &&
      uuid_data(uuid).rw_hooks->before_close(uuid, uuid_data(uuid).rw_udata)) {
    uuid_data(uuid).close = 2; /* don't repeat the before_close callback */
    fio_touch(uuid);
    fio_poll_add_write(fio_uuid2fd(uuid));
    return;
  }
  fio_lock(&uuid_data(uuid).protocol_lock);
  fio_clear_fd(fio_uuid2fd(uuid), 0);
  fio_unlock(&uuid_data(uuid).protocol_lock);
  close(fio_uuid2fd(uuid));
#if FIO_ENGINE_POLL
  fio_poll_remove_fd(fio_uuid2fd(uuid));
#endif
  if (fio_data->connection_count)
    fio_atomic_sub(&fio_data->connection_count, 1);
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
 * `fio_flush` attempts to write any remaining data in the internal buffer to
 * the underlying file descriptor and closes the underlying file descriptor once
 * if it's marked for closure (and all the data was sent).
 *
 * Return values: 1 will be returned if data remains in the buffer. 0
 * will be returned if the buffer was fully drained. -1 will be returned on an
 * error or when the connection is closed.
 */
ssize_t fio_flush(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    goto invalid;
  errno = 0;
  ssize_t flushed = 0;
  int tmp;
  /* start critical section */
  if (fio_trylock(&uuid_data(uuid).sock_lock))
    goto would_block;

  if (!uuid_data(uuid).packet)
    goto flush_rw_hook;

  const fio_packet_s *old_packet = uuid_data(uuid).packet;
  const size_t old_sent = uuid_data(uuid).sent;

  tmp = uuid_data(uuid).packet->write_func(fio_uuid2fd(uuid),
                                           uuid_data(uuid).packet);
  if (tmp <= 0) {
    goto test_errno;
  }

  if (uuid_data(uuid).packet_count >= 1024 &&
      uuid_data(uuid).packet == old_packet &&
      uuid_data(uuid).sent >= old_sent &&
      (uuid_data(uuid).sent - old_sent) < 32768) {
    /* Slowloris attack assumed */
    goto attacked;
  }

  /* end critical section */
  fio_unlock(&uuid_data(uuid).sock_lock);

  /* test for fio_close marker */
  if (!uuid_data(uuid).packet && uuid_data(uuid).close)
    goto closed;

  /* return state */
  return uuid_data(uuid).open && uuid_data(uuid).packet != NULL;

would_block:
  errno = EWOULDBLOCK;
  return -1;

closed:
  fio_force_close(uuid);
  return -1;

flush_rw_hook:
  flushed = uuid_data(uuid).rw_hooks->flush(uuid, uuid_data(uuid).rw_udata);
  fio_unlock(&uuid_data(uuid).sock_lock);
  if (!flushed)
    return 0;
  if (flushed < 0) {
    goto test_errno;
  }
  touchfd(fio_uuid2fd(uuid));
  return 1;

test_errno:
  fio_unlock(&uuid_data(uuid).sock_lock);
  switch (errno) {
  case EWOULDBLOCK: /* fallthrough */
#if EWOULDBLOCK != EAGAIN
  case EAGAIN: /* fallthrough */
#endif
  case ENOTCONN:      /* fallthrough */
  case EINPROGRESS:   /* fallthrough */
  case ENOSPC:        /* fallthrough */
  case EADDRNOTAVAIL: /* fallthrough */
  case EINTR:
    return 1;
  case EFAULT:
    FIO_LOG_ERROR("fio_flush EFAULT - possible memory address error sent to "
                  "Unix socket.");
    /* fallthrough */
  case EPIPE:  /* fallthrough */
  case EIO:    /* fallthrough */
  case EINVAL: /* fallthrough */
  case EBADF:
    uuid_data(uuid).close = 1;
    fio_force_close(uuid);
    return -1;
  }
  // fprintf(stderr, "UUID error: %p (%d)\n", (void *)uuid, errno);
  // perror("No errno handler");
  return 0;

invalid:
  /* bad UUID */
  errno = EBADF;
  return -1;

attacked:
  /* don't close, just detach from facil.io and mark uuid as invalid */
  FIO_LOG_WARNING("(facil.io) possible Slowloris attack from %.*s",
                  (int)fio_peer_addr(uuid).len, fio_peer_addr(uuid).buf);
  fio_unlock(&uuid_data(uuid).sock_lock);
  fio_clear_fd(fio_uuid2fd(uuid), 0);
  return -1;
}

/** `fio_flush_all` attempts flush all the open connections. */
size_t fio_flush_all(void) {
  if (!fio_data)
    return 0;
  size_t count = 0;
  for (uintptr_t i = 0; i <= fio_data->max_protocol_fd; ++i) {
    if ((fd_data(i).open || fd_data(i).packet) && fio_flush(fd2uuid(i)) > 0)
      ++count;
  }
  return count;
}

/* *****************************************************************************
Connection Read / Write Hooks, for overriding the system calls
***************************************************************************** */

static ssize_t fio_hooks_default_read(intptr_t uuid, void *udata, void *buf,
                                      size_t count) {
  return read(fio_uuid2fd(uuid), buf, count);
  (void)(udata);
}
static ssize_t fio_hooks_default_write(intptr_t uuid, void *udata,
                                       const void *buf, size_t count) {
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

/**
 * Replaces an existing read/write hook with another from within a read/write
 * hook callback.
 *
 * Does NOT call any cleanup callbacks.
 *
 * Returns -1 on error, 0 on success.
 */
int fio_rw_hook_replace_unsafe(intptr_t uuid, fio_rw_hook_s *rw_hooks,
                               void *udata) {
  int replaced = -1;
  uint8_t was_locked;
  intptr_t fd = fio_uuid2fd(uuid);
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
  /* protect against some fulishness... but not all of it. */
  was_locked = fio_trylock(&fd_data(fd).sock_lock);
  if (fd2uuid(fd) == uuid) {
    fd_data(fd).rw_hooks = rw_hooks;
    fd_data(fd).rw_udata = udata;
    replaced = 0;
  }
  if (!was_locked)
    fio_unlock(&fd_data(fd).sock_lock);
  return replaced;
}

/** Sets a socket hook state (a pointer to the struct). */
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata) {
  if (fio_is_closed(uuid))
    goto invalid_uuid;
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
  intptr_t fd = fio_uuid2fd(uuid);
  fio_rw_hook_s *old_rw_hooks;
  void *old_udata;
  fio_lock(&fd_data(fd).sock_lock);
  if (fd2uuid(fd) != uuid) {
    fio_unlock(&fd_data(fd).sock_lock);
    goto invalid_uuid;
  }
  old_rw_hooks = fd_data(fd).rw_hooks;
  old_udata = fd_data(fd).rw_udata;
  fd_data(fd).rw_hooks = rw_hooks;
  fd_data(fd).rw_udata = udata;
  fio_unlock(&fd_data(fd).sock_lock);
  if (old_rw_hooks && old_rw_hooks->cleanup)
    old_rw_hooks->cleanup(old_udata);
  return 0;
invalid_uuid:
  if (!rw_hooks->cleanup)
    rw_hooks->cleanup(udata);
  return -1;
}

/* *****************************************************************************
Section Start Marker












                           IO Protocols and Attachment













***************************************************************************** */

/* *****************************************************************************
Setting the protocol
***************************************************************************** */

/* managing the protocol pointer array and the `on_close` callback */
static int fio_attach__internal(void *uuid_, void *protocol_) {
  intptr_t uuid = (intptr_t)uuid_;
  fio_protocol_s *protocol = (fio_protocol_s *)protocol_;
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
    prt_meta(protocol) = (protocol_metadata_s){.rsv = 0};
  }
  if (!uuid_is_valid(uuid))
    goto invalid_uuid_unlocked;
  fio_lock(&uuid_data(uuid).protocol_lock);
  if (!uuid_is_valid(uuid)) {
    goto invalid_uuid;
  }
  fio_protocol_s *old_pr = uuid_data(uuid).protocol;
  uuid_data(uuid).open = 1;
  uuid_data(uuid).protocol = protocol;
  touchfd(fio_uuid2fd(uuid));
  fio_unlock(&uuid_data(uuid).protocol_lock);
  if (old_pr) {
    /* protocol replacement */
    fio_defer_push_task(deferred_on_close, (void *)uuid, old_pr);
    if (!protocol) {
      /* hijacking */
      fio_poll_remove_fd(fio_uuid2fd(uuid));
      fio_poll_add_write(fio_uuid2fd(uuid));
    }
  } else if (protocol) {
    /* adding a new uuid to the reactor */
    fio_poll_add(fio_uuid2fd(uuid));
  }
  fio_max_fd_min(fio_uuid2fd(uuid));
  return 0;

invalid_uuid:
  fio_unlock(&uuid_data(uuid).protocol_lock);
invalid_uuid_unlocked:
  // FIO_LOG_DEBUG("fio_attach failed for invalid uuid %p", (void *)uuid);
  if (protocol)
    fio_defer_push_task(deferred_on_close, (void *)uuid, protocol);
  if (uuid == -1)
    errno = EBADF;
  else
    errno = ENOTCONN;
  return -1;
}

/**
 * Attaches (or updates) a protocol object to a socket UUID.
 * Returns -1 on error and 0 on success.
 */
void fio_attach(intptr_t uuid, fio_protocol_s *protocol) {
  fio_attach__internal((void *)uuid, protocol);
}
/** Attaches (or updates) a protocol object to a socket UUID.
 * Returns -1 on error and 0 on success.
 */
void fio_attach_fd(int fd, fio_protocol_s *protocol) {
  fio_attach__internal((void *)fio_fd2uuid(fd), protocol);
}

/** Sets a timeout for a specific connection (only when running and valid). */
void fio_timeout_set(intptr_t uuid, uint8_t timeout) {
  if (uuid_is_valid(uuid)) {
    touchfd(fio_uuid2fd(uuid));
    uuid_data(uuid).timeout = timeout;
  } else {
    FIO_LOG_DEBUG("Called fio_timeout_set for invalid uuid %p", (void *)uuid);
  }
}
/** Gets a timeout for a specific connection. Returns 0 if there's no set
 * timeout or the connection is inactive. */
uint8_t fio_timeout_get(intptr_t uuid) { return uuid_data(uuid).timeout; }

/* *****************************************************************************
Core Callbacks for forking / starting up / cleaning up
***************************************************************************** */

typedef struct {
  FIO_LIST_NODE node;
  void (*func)(void *);
  void *arg;
} callback_data_s;

typedef struct {
  fio_lock_i lock;
  FIO_LIST_HEAD callbacks;
} callback_collection_s;

#define FIO_LIST_NAME callback_collection
#define FIO_LIST_TYPE callback_data_s
#include "fio-stl.h"

static callback_collection_s callback_collection[FIO_CALL_NEVER + 1];

static void fio_state_on_idle_perform(void *task, void *arg) {
  ((void (*)(void *))(uintptr_t)task)(arg);
}

static inline void fio_state_callback_ensure(callback_collection_s *c) {
  if (c->callbacks.next)
    return;
  c->callbacks = (FIO_LIST_HEAD)FIO_LIST_INIT(c->callbacks);
}

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e c_type, void (*func)(void *),
                            void *arg) {
  if (c_type == FIO_CALL_ON_INITIALIZE && fio_data) {
    func(arg);
    return;
  }
  if (!func || (int)c_type < 0 || c_type > FIO_CALL_NEVER)
    return;
  fio_lock(&callback_collection[c_type].lock);
  fio_state_callback_ensure(&callback_collection[c_type]);
  callback_data_s *tmp = malloc(sizeof(*tmp));
  FIO_ASSERT_ALLOC(tmp);
  *tmp = (callback_data_s){.func = func, .arg = arg};
  callback_collection_push(&callback_collection[c_type].callbacks, tmp);
  fio_unlock(&callback_collection[c_type].lock);
}

/** Removes a callback from the list of callbacks to be called for the event. */
int fio_state_callback_remove(callback_type_e c_type, void (*func)(void *),
                              void *arg) {
  if ((int)c_type < 0 || c_type > FIO_CALL_NEVER)
    return -1;
  fio_lock(&callback_collection[c_type].lock);
  FIO_LIST_EACH(callback_data_s, node, &callback_collection[c_type].callbacks,
                pos) {
    callback_data_s *tmp = pos;
    if (tmp->func == func && tmp->arg == arg) {
      callback_collection_remove(tmp);
      free(tmp);
      goto success;
    }
  }
  fio_unlock(&callback_collection[c_type].lock);
  return -1;
success:
  fio_unlock(&callback_collection[c_type].lock);
  return -0;
}

/** Forces all the existing callbacks to run, as if the event occurred. */
void fio_state_callback_force(callback_type_e c_type) {
  if ((int)c_type < 0 || c_type > FIO_CALL_NEVER)
    return;
  /* copy collection */
  FIO_LIST_HEAD copy = FIO_LIST_INIT(copy);
  fio_lock(&callback_collection[c_type].lock);
  fio_state_callback_ensure(&callback_collection[c_type]);
  switch (c_type) {            /* the difference between `unshift` and `push` */
  case FIO_CALL_ON_INITIALIZE: /* fallthrough */
  case FIO_CALL_PRE_START:     /* fallthrough */
  case FIO_CALL_BEFORE_FORK:   /* fallthrough */
  case FIO_CALL_AFTER_FORK:    /* fallthrough */
  case FIO_CALL_IN_CHILD:      /* fallthrough */
  case FIO_CALL_IN_MASTER:     /* fallthrough */
  case FIO_CALL_ON_START:      /* fallthrough */
    FIO_LIST_EACH(callback_data_s, node, &callback_collection[c_type].callbacks,
                  pos) {
      callback_data_s *tmp = fio_malloc(sizeof(*tmp));
      FIO_ASSERT_ALLOC(tmp);
      *tmp = *pos;
      callback_collection_unshift(&copy, tmp);
    }
    break;

  case FIO_CALL_ON_IDLE: /* idle callbacks are orderless and evented */
    FIO_LIST_EACH(callback_data_s, node, &callback_collection[c_type].callbacks,
                  pos) {
      fio_defer_push_task(fio_state_on_idle_perform,
                          (void *)(uintptr_t)pos->func, pos->arg);
    }
    break;

  case FIO_CALL_ON_SHUTDOWN:     /* fallthrough */
  case FIO_CALL_ON_FINISH:       /* fallthrough */
  case FIO_CALL_ON_PARENT_CRUSH: /* fallthrough */
  case FIO_CALL_ON_CHILD_CRUSH:  /* fallthrough */
  case FIO_CALL_AT_EXIT:         /* fallthrough */
  case FIO_CALL_NEVER:           /* fallthrough */
  default:
    FIO_LIST_EACH(callback_data_s, node, &callback_collection[c_type].callbacks,
                  pos) {
      callback_data_s *tmp = fio_malloc(sizeof(*tmp));
      FIO_ASSERT_ALLOC(tmp);
      *tmp = *pos;
      callback_collection_push(&copy, tmp);
    }
    break;
  }

  fio_unlock(&callback_collection[c_type].lock);
  /* run callbacks + free data */
  while (callback_collection_any(&copy)) {
    callback_data_s *tmp = callback_collection_pop(&copy);
    if (tmp->func) {
      tmp->func(tmp->arg);
    }
    fio_free(tmp);
  }
}

/** Clears all the existing callbacks for the event. */
void fio_state_callback_clear(callback_type_e c_type) {
  if ((int)c_type < 0 || c_type > FIO_CALL_NEVER)
    return;
  fio_lock(&callback_collection[c_type].lock);
  fio_state_callback_ensure(&callback_collection[c_type]);
  while (callback_collection_any(&callback_collection[c_type].callbacks)) {
    callback_data_s *tmp =
        callback_collection_shift(&callback_collection[c_type].callbacks);
    free(tmp);
  }
  fio_unlock(&callback_collection[c_type].lock);
}

void fio_state_callback_on_fork(void) {
  for (size_t i = 0; i < (FIO_CALL_NEVER + 1); ++i) {
    callback_collection[i].lock = FIO_LOCK_INIT;
  }
}
void fio_state_callback_clear_all(void) {
  for (size_t i = 0; i < (FIO_CALL_NEVER + 1); ++i) {
    fio_state_callback_clear((callback_type_e)i);
  }
}

/* *****************************************************************************
IO bound tasks
***************************************************************************** */

// typedef struct {
//   enum fio_protocol_lock_e type;
//   void (*task)(intptr_t uuid, fio_protocol_s *, void *udata);
//   void *udata;
//   void (*fallback)(intptr_t uuid, void *udata);
// } fio_defer_iotask_args_s;

static void fio_io_task_perform(void *uuid_, void *args_) {
  fio_defer_iotask_args_s *args = args_;
  intptr_t uuid = (intptr_t)uuid_;
  fio_protocol_s *pr = fio_protocol_try_lock(uuid, args->type);
  if (!pr)
    goto postpone;
  args->task(uuid, pr, args->udata);
  fio_protocol_unlock(pr, args->type);
  fio_free(args);
  return;
postpone:
  if (errno == EBADF) {
    if (args->fallback)
      args->fallback(uuid, args->udata);
    fio_free(args);
    return;
  }
  fio_defer_push_task(fio_io_task_perform, uuid_, args_);
}
/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error ocuurs or the connection is closed before the task can run, the
 * `fallback` task wil be called instead, allowing for resource cleanup.
 */
void fio_defer_io_task FIO_NOOP(intptr_t uuid, fio_defer_iotask_args_s args) {
  if (!args.task) {
    if (args.fallback)
      fio_defer_push_task((void (*)(void *, void *))args.fallback, (void *)uuid,
                          args.udata);
    return;
  }
  fio_defer_iotask_args_s *cpy = fio_malloc(sizeof(*cpy));
  FIO_ASSERT_ALLOC(cpy);
  *cpy = args;
  fio_defer_push_task(fio_io_task_perform, (void *)uuid, cpy);
}

/* *****************************************************************************
Initialize the library
***************************************************************************** */

static void fio_pubsub_on_fork(void);

/* Called within a child process after it starts. */
static void fio_on_fork(void) {
  fio_timer_lock = FIO_LOCK_INIT;
  fio_data->lock = FIO_LOCK_INIT;
  fio_defer_on_fork();
  fio_malloc_after_fork();
  fio_poll_init();
  fio_rand_reseed();
  fio_state_callback_on_fork();

  const size_t limit = fio_data->capa;
  for (size_t i = 0; i < limit; ++i) {
    fd_data(i).sock_lock = FIO_LOCK_INIT;
    fd_data(i).protocol_lock = FIO_LOCK_INIT;
    if (fd_data(i).protocol) {
      fd_data(i).protocol->rsv = 0;
      fio_force_close(fd2uuid(i));
    }
  }

  fio_pubsub_on_fork();
  fio_max_fd_shrink();
  uint16_t old_active = fio_data->active;
  fio_data->active = 0;
  fio_defer_perform();
  fio_data->active = old_active;
  fio_data->is_worker = 1;
}

static void __attribute__((destructor)) fio_lib_destroy(void) {
  uint8_t add_eol = fio_is_master();
  fio_data->active = 0;
  fio_on_fork();
  fio_defer_perform();
  fio_timer_clear_all();
  fio_defer_perform();
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_defer_perform();
  fio_state_callback_clear_all();
  fio_poll_close();
  fio_free(fio_data);
  /* memory library destruction must be last */
  FIO_LOG_DEBUG("(%d) facil.io resources released, exit complete.",
                (int)getpid());
  if (add_eol)
    fprintf(stderr, "\n"); /* add EOL to logs (logging adds EOL before text */
}

static void fio_cluster_init(void);
static void fio_pubsub_initialize(void);
static void __attribute__((constructor)) fio_lib_init(void) {
  /* detect socket capacity - MUST be first...*/
  ssize_t capa = 0;
  {
#ifdef _SC_OPEN_MAX
    capa = sysconf(_SC_OPEN_MAX);
#elif defined(FOPEN_MAX)
    capa = FOPEN_MAX;
#endif
    // try to maximize limits - collect max and set to max
    struct rlimit rlim = {.rlim_max = 0};
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
      FIO_LOG_WARNING("`getrlimit` failed in `fio_lib_init`.");
      perror("\terrno:");
    } else {
      rlim_t original = rlim.rlim_cur;
      rlim.rlim_cur = rlim.rlim_max;
      if (rlim.rlim_cur > FIO_MAX_SOCK_CAPACITY) {
        rlim.rlim_cur = rlim.rlim_max = FIO_MAX_SOCK_CAPACITY;
      }
      while (setrlimit(RLIMIT_NOFILE, &rlim) == -1 && rlim.rlim_cur > original)
        --rlim.rlim_cur;
      getrlimit(RLIMIT_NOFILE, &rlim);
      capa = rlim.rlim_cur;
      if (capa > 1024) /* leave a slice of room */
        capa -= 16;
    }
    /* initialize polling engine */
    fio_poll_init();
    /* initialize the cluster engine */
    fio_pubsub_initialize();
#if DEBUG
#if FIO_ENGINE_POLL
    FIO_LOG_INFO("facil.io " FIO_VERSION_STRING " capacity initialization:\n"
                 "*    Meximum open files %zu out of %zu\n"
                 "*    Allocating %zu bytes for state handling.\n"
                 "*    %zu bytes per connection + %zu for state handling.",
                 capa, (size_t)rlim.rlim_max,
                 (sizeof(*fio_data) + (capa * (sizeof(*fio_data->poll))) +
                  (capa * (sizeof(*fio_data->info)))),
                 (sizeof(*fio_data->poll) + sizeof(*fio_data->info)),
                 sizeof(*fio_data));
#else
    FIO_LOG_INFO("facil.io " FIO_VERSION_STRING " capacity initialization:\n"
                 "*    Meximum open files %zu out of %zu\n"
                 "*    Allocating %zu bytes for state handling.\n"
                 "*    %zu bytes per connection + %zu for state handling.",
                 capa, (size_t)rlim.rlim_max,
                 (sizeof(*fio_data) + (capa * (sizeof(*fio_data->info)))),
                 (sizeof(*fio_data->info)), sizeof(*fio_data));
#endif
#endif
  }

#if FIO_ENGINE_POLL
  /* allocate and initialize main data structures by detected capacity */
  fio_data = fio_mmap(sizeof(*fio_data) + (capa * (sizeof(*fio_data->poll))) +
                      (capa * (sizeof(*fio_data->info))));
  FIO_ASSERT_ALLOC(fio_data);
  fio_data->capa = capa;
  fio_data->poll =
      (void *)((uintptr_t)(fio_data + 1) + (sizeof(fio_data->info[0]) * capa));
#else
  /* allocate and initialize main data structures by detected capacity */
  fio_data = fio_mmap(sizeof(*fio_data) + (capa * (sizeof(*fio_data->info))));
  FIO_ASSERT_ALLOC(fio_data);
  fio_data->capa = capa;
#endif
  fio_data->parent = getpid();
  fio_data->connection_count = 0;
  fio_mark_time();

  for (ssize_t i = 0; i < capa; ++i) {
    fio_clear_fd(i, 0);
#if FIO_ENGINE_POLL
    fio_data->poll[i].fd = -1;
#endif
  }

  /* call initialization callbacks */
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
  fio_state_callback_clear(FIO_CALL_ON_INITIALIZE);
}

/* *****************************************************************************
Section Start Marker












                             Running the IO Reactor













***************************************************************************** */

static void fio_cluster_signal_children(void);

static void fio_review_timeout(void *arg, void *ignr) {
  // TODO: Fix review for connections with no protocol?
  (void)ignr;
  fio_protocol_s *tmp;
  time_t review = fio_data->last_cycle.tv_sec;
  intptr_t fd = (intptr_t)arg;

  uint16_t timeout = fd_data(fd).timeout;
  if (!timeout)
    timeout = 300; /* enforced timout settings */
  if (!fd_data(fd).protocol || (fd_data(fd).active + timeout >= review))
    goto finish;
  tmp = protocol_try_lock(fd, FIO_PR_LOCK_STATE);
  if (!tmp) {
    if (errno == EBADF)
      goto finish;
    goto reschedule;
  }
  if (prt_meta(tmp).locks[FIO_PR_LOCK_TASK] ||
      prt_meta(tmp).locks[FIO_PR_LOCK_WRITE])
    goto unlock;
  fio_defer_push_task(deferred_ping, (void *)fio_fd2uuid((int)fd), NULL);
unlock:
  protocol_unlock(tmp, FIO_PR_LOCK_STATE);
finish:
  do {
    fd++;
  } while (!fd_data(fd).protocol && (fd <= fio_data->max_protocol_fd));

  if (fio_data->max_protocol_fd < fd) {
    fio_data->need_review = 1;
    return;
  }
reschedule:
  fio_defer_push_task(fio_review_timeout, (void *)fd, NULL);
}

/* reactor pattern cycling - common actions */
static void fio_cycle_schedule_events(void) {
  static int idle = 0;
  static time_t last_to_review = 0;
  fio_mark_time();
  fio_timer_schedule();
  fio_max_fd_shrink();
  if (fio_signal_children_flag) {
    /* hot restart support */
    fio_signal_children_flag = 0;
    fio_cluster_signal_children();
  }
  int events = fio_poll();
  if (events < 0) {
    return;
  }
  if (events > 0) {
    idle = 1;
  } else {
    /* events == 0 */
    if (idle) {
      fio_state_callback_force(FIO_CALL_ON_IDLE);
      idle = 0;
    }
  }
  if (fio_data->need_review && fio_data->last_cycle.tv_sec != last_to_review) {
    last_to_review = fio_data->last_cycle.tv_sec;
    fio_data->need_review = 0;
    fio_defer_push_task(fio_review_timeout, (void *)0, NULL);
  }
}

/* reactor pattern cycling during cleanup */
static void fio_cycle_unwind(void *ignr, void *ignr2) {
  if (fio_data->connection_count) {
    fio_cycle_schedule_events();
    fio_defer_push_task(fio_cycle_unwind, ignr, ignr2);
    return;
  }
  fio_stop();
  return;
}

/* reactor pattern cycling */
static void fio_cycle(void *ignr, void *ignr2) {
  fio_cycle_schedule_events();
  fio_rand_feed2seed(&fio_data->last_cycle.tv_nsec,
                     sizeof(fio_data->last_cycle.tv_nsec));
  if (fio_data->active) {
    fio_defer_push_task(fio_cycle, ignr, ignr2);
    return;
  }
  return;
}

/* TODO: fixme */
static void fio_worker_startup(void) {
  /* Call the on_start callbacks for worker processes. */
  if (fio_data->workers == 1 || fio_data->is_worker) {
    fio_state_callback_force(FIO_CALL_ON_START);
    fio_state_callback_clear(FIO_CALL_ON_START);
  }

  if (fio_data->workers == 1) {
    /* Single Process - the root is also a worker */
    fio_data->is_worker = 1;
  } else if (fio_data->is_worker) {
    /* Worker Process */
    FIO_LOG_INFO("%d is running.", (int)getpid());
  } else {
    /* Root Process should run in single thread mode */
    fio_data->threads = 1;
  }

  /* require timeout review */
  fio_data->need_review = 1;

  /* the cycle task will loop by re-scheduling until it's time to finish */
  fio_defer_push_task(fio_cycle, NULL, NULL);

  /* A single thread doesn't need a pool. */
  if (fio_data->threads > 1) {
    fio_defer_thread_pool_join(fio_defer_thread_pool_new(fio_data->threads));
  } else {
    fio_defer_perform();
  }
}

/* performs all clean-up / shutdown requirements except for the exit sequence */
static void fio_worker_cleanup(void) {
  /* switch to winding down */
  if (fio_data->is_worker)
    FIO_LOG_INFO("(%d) detected exit signal.", (int)getpid());
  else
    FIO_LOG_INFO("Server Detected exit signal.");
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  for (size_t i = 0; i <= fio_data->max_protocol_fd; ++i) {
    if (fd_data(i).protocol) {
      fio_defer_push_task(deferred_on_shutdown, (void *)fd2uuid(i), NULL);
    }
  }
  fio_defer_push_task(fio_cycle_unwind, NULL, NULL);
  fio_defer_perform();
  for (size_t i = 0; i <= fio_data->max_protocol_fd; ++i) {
    if (fd_data(i).protocol || fd_data(i).open) {
      fio_force_close(fd2uuid(i));
    }
  }
  fio_defer_perform();
  fio_timer_clear_all();
  fio_defer_perform();
  fio_state_callback_force(FIO_CALL_ON_FINISH);
  fio_defer_perform();
  if (!fio_data->is_worker) {
    fio_cluster_signal_children();
    while (wait(NULL) != -1)
      ;
  }
  fio_defer_perform();
  fio_signal_handler_reset();
  if (fio_data->parent == getpid()) {
    FIO_LOG_INFO("   ---  Shutdown Complete  ---\n");
  } else {
    FIO_LOG_INFO("(%d) cleanup complete.", (int)getpid());
  }
}

static void fio_sentinel_task(void *arg1, void *arg2);
static void *fio_sentinel_worker_thread(void *arg) {
  errno = 0;
  pid_t child = fio_fork();
  /* release fork lock. */
  fio_unlock(&fio_fork_lock);
  if (child == -1) {
    FIO_LOG_FATAL("couldn't spawn worker.");
    perror("\n           errno");
    kill(fio_master_pid(), SIGINT);
    fio_stop();
    return NULL;
  } else if (child) {
    int status;
    waitpid(child, &status, 0);
#if DEBUG
    if (fio_data->active) { /* !WIFEXITED(status) || WEXITSTATUS(status) */
      if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        FIO_LOG_FATAL("Child worker (%d) crashed. Stopping services.", child);
        fio_state_callback_force(FIO_CALL_ON_CHILD_CRUSH);
      } else {
        FIO_LOG_FATAL("Child worker (%d) shutdown. Stopping services.", child);
      }
      fio_stop();
    }
#else
    if (fio_data->active) {
      /* don't call any functions while forking. */
      fio_lock(&fio_fork_lock);
      if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        FIO_LOG_ERROR("Child worker (%d) crashed. Respawning worker.",
                      (int)child);
        fio_state_callback_force(FIO_CALL_ON_CHILD_CRUSH);
      } else {
        FIO_LOG_WARNING("Child worker (%d) shutdown. Respawning worker.",
                        (int)child);
      }
      fio_defer_push_task(fio_sentinel_task, NULL, NULL);
      fio_unlock(&fio_fork_lock);
    }
#endif
  } else {
    fio_on_fork();
    fio_state_callback_force(FIO_CALL_AFTER_FORK);
    fio_state_callback_force(FIO_CALL_IN_CHILD);
    fio_worker_startup();
    fio_worker_cleanup();
    exit(0);
  }
  return NULL;
  (void)arg;
}

static void fio_sentinel_task(void *arg1, void *arg2) {
  if (!fio_data->active)
    return;
  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  fio_lock(&fio_fork_lock); /* will wait for worker thread to release lock. */
  void *thrd =
      fio_thread_new(fio_sentinel_worker_thread, (void *)&fio_fork_lock);
  fio_thread_free(thrd);
  fio_lock(&fio_fork_lock);   /* will wait for worker thread to release lock. */
  fio_unlock(&fio_fork_lock); /* release lock for next fork. */
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_state_callback_force(FIO_CALL_IN_MASTER);
  (void)arg1;
  (void)arg2;
}

FIO_SFUNC void fio_start_(void) {} /* marker for SublimeText3 jump feature */

/**
 * Starts the facil.io event loop. This function will return after facil.io is
 * done (after shutdown).
 *
 * See the `struct fio_start_args` details for any possible named arguments.
 *
 * This method blocks the current thread until the server is stopped (when a
 * SIGINT/SIGTERM is received).
 */
void fio_start FIO_NOOP(struct fio_start_args args) {
  fio_expected_concurrency(&args.threads, &args.workers);
  fio_signal_handler_setup();

  fio_data->workers = (uint16_t)args.workers;
  fio_data->threads = (uint16_t)args.threads;
  fio_data->active = 1;
  fio_data->is_worker = 0;

  fio_state_callback_force(FIO_CALL_PRE_START);

  FIO_LOG_INFO(
      "Server is running %u %s X %u %s with facil.io " FIO_VERSION_STRING
      " (%s)\n"
      "* Detected capacity: %d open file limit\n"
      "* Root pid: %d\n"
      "* Press ^C to stop\n",
      fio_data->workers, fio_data->workers > 1 ? "workers" : "worker",
      fio_data->threads, fio_data->threads > 1 ? "threads" : "thread",
      fio_engine(), fio_data->capa, (int)fio_data->parent);

  if (args.workers > 1) {
    for (int i = 0; i < args.workers && fio_data->active; ++i) {
      fio_sentinel_task(NULL, NULL);
    }
  }
  fio_worker_startup();
  fio_worker_cleanup();
}

/* *****************************************************************************
Section Start Marker















                       Listening to Incoming Connections
















***************************************************************************** */

/* *****************************************************************************
The listening protocol (use the facil.io API to make a socket and attach it)
***************************************************************************** */

typedef struct {
  fio_protocol_s pr;
  intptr_t uuid;
  void *udata;
  void (*on_open)(intptr_t uuid, void *udata);
  void (*on_start)(intptr_t uuid, void *udata);
  void (*on_finish)(intptr_t uuid, void *udata);
  char *port;
  char *addr;
  size_t port_len;
  size_t addr_len;
  fio_tls_s *tls;
} fio_listen_protocol_s;

static void fio_listen_cleanup_task(void *pr_) {
  fio_listen_protocol_s *pr = pr_;
  if (pr->tls)
    fio_tls_destroy(pr->tls);
  if (pr->on_finish) {
    pr->on_finish(pr->uuid, pr->udata);
  }
  fio_force_close(pr->uuid);
  if (pr->addr &&
      (!pr->port || *pr->port == 0 ||
       (pr->port[0] == '0' && pr->port[1] == 0)) &&
      fio_is_master()) {
    /* delete Unix sockets */
    unlink(pr->addr);
  }
  free(pr_);
}

static void fio_listen_on_startup(void *pr_) {
  fio_state_callback_remove(FIO_CALL_ON_SHUTDOWN, fio_listen_cleanup_task, pr_);
  fio_listen_protocol_s *pr = pr_;
  fio_attach(pr->uuid, &pr->pr);
  if (pr->port_len)
    FIO_LOG_DEBUG("(%d) started listening on port %s", (int)getpid(), pr->port);
  else
    FIO_LOG_DEBUG("(%d) started listening on Unix Socket at %s", (int)getpid(),
                  pr->addr);
}

static void fio_listen_on_close(intptr_t uuid, fio_protocol_s *pr_) {
  fio_listen_cleanup_task(pr_);
  (void)uuid;
}

static void fio_listen_on_data(intptr_t uuid, fio_protocol_s *pr_) {
  fio_listen_protocol_s *pr = (fio_listen_protocol_s *)pr_;
  for (int i = 0; i < 4; ++i) {
    intptr_t client = fio_accept(uuid);
    if (client == -1)
      return;
    pr->on_open(client, pr->udata);
  }
}

static void fio_listen_on_data_tls(intptr_t uuid, fio_protocol_s *pr_) {
  fio_listen_protocol_s *pr = (fio_listen_protocol_s *)pr_;
  for (int i = 0; i < 4; ++i) {
    intptr_t client = fio_accept(uuid);
    if (client == -1)
      return;
    fio_tls_accept(client, pr->tls, pr->udata);
    pr->on_open(client, pr->udata);
  }
}

static void fio_listen_on_data_tls_alpn(intptr_t uuid, fio_protocol_s *pr_) {
  fio_listen_protocol_s *pr = (fio_listen_protocol_s *)pr_;
  for (int i = 0; i < 4; ++i) {
    intptr_t client = fio_accept(uuid);
    if (client == -1)
      return;
    fio_tls_accept(client, pr->tls, pr->udata);
  }
}

/* stub for editor - unused */
void fio_listen____(void);
/**
 * Schedule a network service on a listening socket.
 *
 * Returns the listening socket or -1 (on error).
 */
intptr_t fio_listen FIO_NOOP(struct fio_listen_args args) {
  // ...
  if ((!args.on_open && (!args.tls || !fio_tls_alpn_count(args.tls))) ||
      (!args.address && !args.port)) {
    errno = EINVAL;
    goto error;
  }

  size_t addr_len = 0;
  size_t port_len = 0;
  if (args.address)
    addr_len = strlen(args.address);
  if (args.port) {
    port_len = strlen(args.port);
    char *tmp = (char *)args.port;
    if (!fio_atol(&tmp)) {
      port_len = 0;
      args.port = NULL;
    }
    if (*tmp) {
      /* port format was invalid, should be only numerals */
      errno = EINVAL;
      goto error;
    }
  }
  const intptr_t uuid =
      fio_socket(args.address, args.port,
                 FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  if (uuid == -1)
    goto error;

  fio_listen_protocol_s *pr = malloc(sizeof(*pr) + addr_len + port_len +
                                     ((addr_len + port_len) ? 2 : 0));
  FIO_ASSERT_ALLOC(pr);

  if (args.tls)
    fio_tls_dup(args.tls);

  *pr = (fio_listen_protocol_s){
      .pr =
          {
              .on_close = fio_listen_on_close,
              .ping = FIO_PING_ETERNAL,
              .on_data = (args.tls ? (fio_tls_alpn_count(args.tls)
                                          ? fio_listen_on_data_tls_alpn
                                          : fio_listen_on_data_tls)
                                   : fio_listen_on_data),
          },
      .uuid = uuid,
      .udata = args.udata,
      .on_open = args.on_open,
      .on_start = args.on_start,
      .on_finish = args.on_finish,
      .tls = args.tls,
      .addr_len = addr_len,
      .port_len = port_len,
      .addr = (char *)(pr + 1),
      .port = ((char *)(pr + 1) + addr_len + 1),
  };

  if (addr_len)
    memcpy(pr->addr, args.address, addr_len + 1);
  if (port_len)
    memcpy(pr->port, args.port, port_len + 1);

  if (fio_is_running()) {
    fio_attach(pr->uuid, &pr->pr);
  } else {
    fio_state_callback_add(FIO_CALL_ON_START, fio_listen_on_startup, pr);
    fio_state_callback_add(FIO_CALL_ON_SHUTDOWN, fio_listen_cleanup_task, pr);
  }

  if (args.port)
    FIO_LOG_INFO("Listening on port %s", args.port);
  else
    FIO_LOG_INFO("Listening on Unix Socket at %s", args.address);

  return uuid;
error:
  if (args.on_finish) {
    args.on_finish(-1, args.udata);
  }
  return -1;
}

/* *****************************************************************************
Section Start Marker















                   Connecting to remote servers as a client
















***************************************************************************** */

/* *****************************************************************************
The connection protocol (use the facil.io API to make a socket and attach it)
***************************************************************************** */

typedef struct {
  fio_protocol_s pr;
  intptr_t uuid;
  void *udata;
  fio_tls_s *tls;
  void (*on_connect)(intptr_t uuid, void *udata);
  void (*on_fail)(intptr_t uuid, void *udata);
} fio_connect_protocol_s;

static void fio_connect_on_close(intptr_t uuid, fio_protocol_s *pr_) {
  fio_connect_protocol_s *pr = (fio_connect_protocol_s *)pr_;
  if (pr->on_fail)
    pr->on_fail(uuid, pr->udata);
  if (pr->tls)
    fio_tls_destroy(pr->tls);
  fio_free(pr);
  (void)uuid;
}

static void fio_connect_on_ready(intptr_t uuid, fio_protocol_s *pr_) {
  fio_connect_protocol_s *pr = (fio_connect_protocol_s *)pr_;
  if (pr->pr.on_ready == mock_on_ev)
    return; /* Don't call on_connect more than once */
  pr->pr.on_ready = mock_on_ev;
  pr->on_fail = NULL;
  pr->on_connect(uuid, pr->udata);
  fio_poll_add(fio_uuid2fd(uuid));
  (void)uuid;
}

static void fio_connect_on_ready_tls(intptr_t uuid, fio_protocol_s *pr_) {
  fio_connect_protocol_s *pr = (fio_connect_protocol_s *)pr_;
  if (pr->pr.on_ready == mock_on_ev)
    return; /* Don't call on_connect more than once */
  pr->pr.on_ready = mock_on_ev;
  pr->on_fail = NULL;
  fio_tls_connect(uuid, pr->tls, pr->udata);
  pr->on_connect(uuid, pr->udata);
  fio_poll_add(fio_uuid2fd(uuid));
  (void)uuid;
}

static void fio_connect_on_ready_tls_alpn(intptr_t uuid, fio_protocol_s *pr_) {
  fio_connect_protocol_s *pr = (fio_connect_protocol_s *)pr_;
  if (pr->pr.on_ready == mock_on_ev)
    return; /* Don't call on_connect more than once */
  pr->pr.on_ready = mock_on_ev;
  pr->on_fail = NULL;
  fio_tls_connect(uuid, pr->tls, pr->udata);
  fio_poll_add(fio_uuid2fd(uuid));
  (void)uuid;
}

/* stub for sublime text function navigation */
intptr_t fio_connect___(struct fio_connect_args args);
intptr_t fio_connect FIO_NOOP(struct fio_connect_args args) {
  if ((!args.on_connect && (!args.tls || !fio_tls_alpn_count(args.tls))) ||
      (!args.address && !args.port)) {
    errno = EINVAL;
    goto error;
  }
  const intptr_t uuid =
      fio_socket(args.address, args.port,
                 (args.port ? FIO_SOCKET_TCP : FIO_SOCKET_UNIX) |
                     FIO_SOCKET_CLIENT | FIO_SOCKET_NONBLOCK);
  if (uuid == -1)
    goto error;
  fio_timeout_set(uuid, args.timeout);

  fio_connect_protocol_s *pr = fio_malloc(sizeof(*pr));
  FIO_ASSERT_ALLOC(pr);

  if (args.tls)
    fio_tls_dup(args.tls);

  *pr = (fio_connect_protocol_s){
      .pr =
          {
              .on_ready = (args.tls ? (fio_tls_alpn_count(args.tls)
                                           ? fio_connect_on_ready_tls_alpn
                                           : fio_connect_on_ready_tls)
                                    : fio_connect_on_ready),
              .on_close = fio_connect_on_close,
          },
      .uuid = uuid,
      .tls = args.tls,
      .udata = args.udata,
      .on_connect = args.on_connect,
      .on_fail = args.on_fail,
  };
  fio_attach(uuid, &pr->pr);
  return uuid;
error:
  if (args.on_fail)
    args.on_fail(-1, args.udata);
  return -1;
}

/* *****************************************************************************
Section Start Marker


























                       Cluster Messaging Implementation



























***************************************************************************** */

#if FIO_PUBSUB_SUPPORT

/* *****************************************************************************
 * Data Structures - Channel / Subscriptions data
 **************************************************************************** */

typedef enum fio_cluster_message_type_e {
  FIO_CLUSTER_MSG_FORWARD,
  FIO_CLUSTER_MSG_JSON,
  FIO_CLUSTER_MSG_ROOT,
  FIO_CLUSTER_MSG_ROOT_JSON,
  FIO_CLUSTER_MSG_PUBSUB_SUB,
  FIO_CLUSTER_MSG_PUBSUB_UNSUB,
  FIO_CLUSTER_MSG_PATTERN_SUB,
  FIO_CLUSTER_MSG_PATTERN_UNSUB,
  FIO_CLUSTER_MSG_SHUTDOWN,
  FIO_CLUSTER_MSG_ERROR,
  FIO_CLUSTER_MSG_PING,
} fio_cluster_message_type_e;

typedef struct fio_collection_s fio_collection_s;

#ifndef __clang__ /* clang might misbehave by assumming non-alignment */
#pragma pack(1)   /* https://gitter.im/halide/Halide/archives/2018/07/24 */
#endif
typedef struct {
  size_t name_len;
  char *name;
  volatile size_t ref;
  FIO_LIST_HEAD subscriptions;
  fio_collection_s *parent;
  fio_match_fn match;
  fio_lock_i lock;
} channel_s;
#ifndef __clang__
#pragma pack()
#endif

struct subscription_s {
  FIO_LIST_NODE node;
  channel_s *parent;
  intptr_t uuid;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata1, void *udata2);
  void *udata1;
  void *udata2;
  /** reference counter. */
  volatile uintptr_t ref;
  /** prevents the callback from running concurrently for multiple messages. */
  fio_lock_i lock;
  fio_lock_i unsubscribed;
};

#define FIO_LIST_NAME subscriptions
#define FIO_LIST_TYPE subscription_s
#include "fio-stl.h"

/* Use `malloc` / `free`, because channles might have a long life. */

/** Used internally by the Set object to create a new channel. */
static channel_s *fio_channel_copy(channel_s *src) {
  channel_s *dest = malloc(sizeof(*dest) + src->name_len + 1);
  FIO_ASSERT_ALLOC(dest);
  dest->name_len = src->name_len;
  dest->match = src->match;
  dest->parent = src->parent;
  dest->name = (char *)(dest + 1);
  if (src->name_len)
    memcpy(dest->name, src->name, src->name_len);
  dest->name[src->name_len] = 0;
  dest->subscriptions = (FIO_LIST_HEAD)FIO_LIST_INIT(dest->subscriptions);
  dest->ref = 1;
  dest->lock = FIO_LOCK_INIT;
  return dest;
}
/** Frees a channel (reference counting). */
static void fio_channel_free(channel_s *ch) {
  if (!ch)
    return;
  if (fio_atomic_sub(&ch->ref, 1))
    return;
  free(ch);
}
/** Increases a channel's reference count. */
static void fio_channel_dup(channel_s *ch) {
  if (!ch)
    return;
  fio_atomic_add(&ch->ref, 1);
}
/** Tests if two channels are equal. */
static int fio_channel_cmp(channel_s *ch1, channel_s *ch2) {
  return ch1->name_len == ch2->name_len && ch1->match == ch2->match &&
         !memcmp(ch1->name, ch2->name, ch1->name_len);
}
/* pub/sub channels and core data sets have a long life, so avoid fio_malloc */
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#define FIO_MAP_NAME fio_ch_set
#define FIO_MAP_TYPE channel_s *
#define FIO_MAP_TYPE_CMP(o1, o2) fio_channel_cmp((o1), (o2))
#define FIO_MAP_TYPE_DESTROY(obj) fio_channel_free((obj))
#define FIO_MAP_TYPE_COPY(dest, src) ((dest) = fio_channel_copy((src)))
#include <fio-stl.h>

#define FIO_MALLOC_TMP_USE_SYSTEM 1
#define FIO_ARRAY_NAME fio_meta_ary
#define FIO_ARRAY_TYPE fio_msg_metadata_fn
#include <fio-stl.h>

#define FIO_MALLOC_TMP_USE_SYSTEM 1
#define FIO_MAP_NAME fio_engine_set
#define FIO_MAP_TYPE fio_pubsub_engine_s *
#define FIO_MAP_TYPE_CMP(k1, k2) ((k1) == (k2))
#include <fio-stl.h>

struct fio_collection_s {
  fio_ch_set_s channels;
  fio_lock_i lock;
};

#define COLLECTION_INIT                                                        \
  { .channels = FIO_MAP_INIT, .lock = FIO_LOCK_INIT }

static struct {
  fio_collection_s filters;
  fio_collection_s pubsub;
  fio_collection_s patterns;
  struct {
    fio_engine_set_s set;
    fio_lock_i lock;
  } engines;
  struct {
    fio_meta_ary_s ary;
    fio_lock_i lock;
  } meta;
} fio_postoffice = {
    .filters = COLLECTION_INIT,
    .pubsub = COLLECTION_INIT,
    .patterns = COLLECTION_INIT,
    .engines.lock = FIO_LOCK_INIT,
    .meta.lock = FIO_LOCK_INIT,
};

/** used to contain the message before it's passed to the handler */
typedef struct {
  fio_msg_s msg;
  size_t marker;
  size_t meta_len;
  fio_msg_metadata_s *meta;
} fio_msg_client_s;

/** used to contain the message internally while publishing */
typedef struct {
  fio_str_info_s channel;
  fio_str_info_s data;
  uintptr_t ref; /* internal reference counter */
  int32_t filter;
  int8_t is_json;
  size_t meta_len;
  fio_msg_metadata_s meta[];
} fio_msg_internal_s;

/** The default engine (settable). */
fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;

/* *****************************************************************************
Internal message object creation
***************************************************************************** */

/** returns a temporary fio_meta_ary_s with a copy of the metadata array */
static fio_meta_ary_s fio_postoffice_meta_copy_new(void) {
  fio_meta_ary_s t = FIO_ARRAY_INIT;
  if (!fio_meta_ary_count(&fio_postoffice.meta.ary)) {
    return t;
  }
  fio_lock(&fio_postoffice.meta.lock);
  fio_meta_ary_concat(&t, &fio_postoffice.meta.ary);
  fio_unlock(&fio_postoffice.meta.lock);
  return t;
}

/** frees a temporary copy created by postoffice_meta_copy_new */
static inline void fio_postoffice_meta_copy_free(fio_meta_ary_s *cpy) {
  fio_meta_ary_destroy(cpy);
}

static void fio_postoffice_meta_update(fio_msg_internal_s *m) {
  if (m->filter || !m->meta_len)
    return;
  fio_meta_ary_s t = fio_postoffice_meta_copy_new();
  if (t.end > m->meta_len)
    t.end = m->meta_len;
  m->meta_len = t.end;
  while (t.end) {
    --t.end;
    m->meta[t.end] = t.ary[t.end](m->channel, m->data, m->is_json);
  }
  fio_postoffice_meta_copy_free(&t);
}

static fio_msg_internal_s *
fio_msg_internal_create(int32_t filter, uint32_t type, fio_str_info_s ch,
                        fio_str_info_s data, int8_t is_json, int8_t cpy) {
  fio_meta_ary_s t = FIO_ARRAY_INIT;
  if (!filter)
    t = fio_postoffice_meta_copy_new();
  fio_msg_internal_s *m = fio_malloc(sizeof(*m) + (sizeof(*m->meta) * t.end) +
                                     (ch.len) + (data.len) + 16 + 2);
  FIO_ASSERT_ALLOC(m);
  *m = (fio_msg_internal_s){
      .filter = filter,
      .channel = (fio_str_info_s){.buf = (char *)(m->meta + t.end) + 16,
                                  .len = ch.len},
      .data =
          (fio_str_info_s){.buf = ((char *)(m->meta + t.end) + ch.len + 16 + 1),
                           .len = data.len},
      .is_json = is_json,
      .ref = 1,
      .meta_len = t.end,
  };
  fio_u2buf32((uint8_t *)(m + 1) + (sizeof(*m->meta) * t.end), ch.len);
  fio_u2buf32((uint8_t *)(m + 1) + (sizeof(*m->meta) * t.end) + 4, data.len);
  fio_u2buf32((uint8_t *)(m + 1) + (sizeof(*m->meta) * t.end) + 8, type);
  fio_u2buf32((uint8_t *)(m + 1) + (sizeof(*m->meta) * t.end) + 12,
              (uint32_t)filter);
  // m->channel.buf[ch.len] = 0; /* redundant, fio_malloc is all zero */
  // m->data.buf[data.len] = 0; /* redundant, fio_malloc is all zero */
  if (cpy) {
    memcpy(m->channel.buf, ch.buf, ch.len);
    memcpy(m->data.buf, data.buf, data.len);
    while (t.end) {
      --t.end;
      m->meta[t.end] = t.ary[t.end](m->channel, m->data, is_json);
    }
  }
  fio_postoffice_meta_copy_free(&t);
  return m;
}

/** frees the internal message data */
static inline void fio_msg_internal_finalize(fio_msg_internal_s *m) {
  if (!m->channel.len)
    m->channel.buf = NULL;
  if (!m->data.len)
    m->data.buf = NULL;
}

/** frees the internal message data */
static inline void fio_msg_internal_free(fio_msg_internal_s *m) {
  if (fio_atomic_sub(&m->ref, 1))
    return;
  while (m->meta_len) {
    --m->meta_len;
    if (m->meta[m->meta_len].on_finish) {
      fio_msg_s tmp_msg = {
          .channel = m->channel,
          .msg = m->data,
      };
      m->meta[m->meta_len].on_finish(&tmp_msg, m->meta[m->meta_len].metadata);
    }
  }
  fio_free(m);
}

static void fio_msg_internal_free2(void *m) { fio_msg_internal_free(m); }

/* add reference count to fio_msg_internal_s */
static inline fio_msg_internal_s *fio_msg_internal_dup(fio_msg_internal_s *m) {
  fio_atomic_add(&m->ref, 1);
  return m;
}

/** internal helper */

static inline ssize_t fio_msg_internal_send_dup(intptr_t uuid,
                                                fio_msg_internal_s *m) {
  return fio_write2(uuid, .data.buf = fio_msg_internal_dup(m),
                    .offset = (sizeof(*m) + (m->meta_len * sizeof(*m->meta))),
                    .len = 16 + m->data.len + m->channel.len + 2,
                    .after.dealloc = fio_msg_internal_free2);
}

/**
 * A mock pub/sub callback for external subscriptions.
 */
static void fio_mock_on_message(fio_msg_s *msg) { (void)msg; }

/* *****************************************************************************
Channel Subscription Management
***************************************************************************** */

static void fio_pubsub_on_channel_create(channel_s *ch);
static void fio_pubsub_on_channel_destroy(channel_s *ch);

/* some comon tasks extracted */
static inline channel_s *fio_filter_dup_lock_internal(channel_s *ch,
                                                      uint64_t hashed,
                                                      fio_collection_s *c) {
  fio_lock(&c->lock);
  ch = fio_ch_set_set_if_missing(&c->channels, hashed, ch);
  fio_channel_dup(ch);
  fio_lock(&ch->lock);
  fio_unlock(&c->lock);
  return ch;
}

/** Creates / finds a filter channel, adds a reference count and locks it. */
static channel_s *fio_filter_dup_lock(uint32_t filter) {
  channel_s ch = (channel_s){
      .name = (char *)&filter,
      .name_len = (sizeof(filter)),
      .parent = &fio_postoffice.filters,
      .ref = 8, /* avoid freeing stack memory */
  };
  return fio_filter_dup_lock_internal(&ch, filter, &fio_postoffice.filters);
}

/** Creates / finds a pubsub channel, adds a reference count and locks it. */
static channel_s *fio_channel_dup_lock(fio_str_info_s name) {
  channel_s ch = (channel_s){
      .name = name.buf,
      .name_len = name.len,
      .parent = &fio_postoffice.pubsub,
      .ref = 8, /* avoid freeing stack memory */
  };
  uint64_t hashed_name = FIO_HASH_FN(name.buf, name.len, &fio_postoffice.pubsub,
                                     &fio_postoffice.pubsub);
  channel_s *ch_p =
      fio_filter_dup_lock_internal(&ch, hashed_name, &fio_postoffice.pubsub);
  if (subscriptions_is_empty(&ch_p->subscriptions)) {
    fio_pubsub_on_channel_create(ch_p);
  }
  return ch_p;
}

/** Creates / finds a pattern channel, adds a reference count and locks it. */
static channel_s *fio_channel_match_dup_lock(fio_str_info_s name,
                                             fio_match_fn match) {
  channel_s ch = (channel_s){
      .name = name.buf,
      .name_len = name.len,
      .parent = &fio_postoffice.patterns,
      .match = match,
      .ref = 8, /* avoid freeing stack memory */
  };
  uint64_t hashed_name = FIO_HASH_FN(name.buf, name.len, &fio_postoffice.pubsub,
                                     &fio_postoffice.pubsub);
  channel_s *ch_p =
      fio_filter_dup_lock_internal(&ch, hashed_name, &fio_postoffice.patterns);
  if (subscriptions_is_empty(&ch_p->subscriptions)) {
    fio_pubsub_on_channel_create(ch_p);
  }
  return ch_p;
}

/* to be used for reference counting (subtructing) */
static inline void fio_subscription_free(subscription_s *s) {
  if (fio_atomic_sub(&s->ref, 1)) {
    return;
  }
  if (s->on_unsubscribe) {
    s->on_unsubscribe(s->udata1, s->udata2);
  }
  fio_channel_free(s->parent);
  fio_free(s);
}

/** SublimeText 3 marker */
subscription_s *fio_subscribe___(subscribe_args_s args);

/** Subscribes to a filter, pub/sub channle or patten */
subscription_s *fio_subscribe FIO_NOOP(subscribe_args_s args) {
  if (!args.on_message)
    goto error;
  if (args.uuid < 0)
    args.uuid = 0;
  if (args.uuid && !uuid_is_valid(args.uuid))
    goto error;
  channel_s *ch;
  subscription_s *s = fio_malloc(sizeof(*s));
  FIO_ASSERT_ALLOC(s);
  *s = (subscription_s){
      .on_message = args.on_message,
      .on_unsubscribe = args.on_unsubscribe,
      .uuid = args.uuid,
      .udata1 = args.udata1,
      .udata2 = args.udata2,
      .ref = 1,
      .lock = FIO_LOCK_INIT,
  };
  if (args.filter) {
    ch = fio_filter_dup_lock(args.filter);
  } else if (args.match) {
    ch = fio_channel_match_dup_lock(args.channel, args.match);
  } else {
    ch = fio_channel_dup_lock(args.channel);
  }
  s->parent = ch;
  subscriptions_push(&ch->subscriptions, s);
  fio_unlock((&ch->lock));
  if (!args.uuid)
    return s;
  fio_uuid_env_set(
      args.uuid, .data = s, .on_close = (void (*)(void *))fio_unsubscribe,
      .name = {.buf = ch->name, .len = ch->name_len}, .const_name = 1,
      .type = (-1 - ((uintptr_t)args.match >> 8) -
               ((uintptr_t)((uint32_t)args.filter) << 1)));
  errno = 0;
  return NULL;
error:
  if (args.on_unsubscribe)
    args.on_unsubscribe(args.udata1, args.udata2);
  return NULL;
}

void fio___unsubscribe_deferred(void *s, void *udata_) {
  fio_unsubscribe(s);
  (void)udata_;
}
/** Unsubscribes from a filter, pub/sub channle or patten */
void fio_unsubscribe(subscription_s *s) {
  if (!s)
    return;
  if (fio_trylock(&s->unsubscribed))
    goto finish;
  if (fio_trylock(&s->lock))
    goto try_again_later;
  channel_s *ch = s->parent;
  uint8_t removed = 0;
  fio_lock(&ch->lock);
  subscriptions_remove(s);
  /* check if channel is done for */
  if (subscriptions_is_empty(&ch->subscriptions)) {
    fio_collection_s *c = ch->parent;
    uint64_t hashed = FIO_HASH_FN(
        ch->name, ch->name_len, &fio_postoffice.pubsub, &fio_postoffice.pubsub);
    /* lock collection */
    fio_lock(&c->lock);
    /* test again within lock */
    if (subscriptions_is_empty(&ch->subscriptions)) {
      fio_ch_set_remove(&c->channels, hashed, ch, NULL);
      removed = (c != &fio_postoffice.filters);
    }
    fio_unlock(&c->lock);
  }
  fio_unlock(&ch->lock);
  if (removed) {
    fio_pubsub_on_channel_destroy(ch);
  }

  /* promise the subscription will be inactive */
  s->on_message = NULL;
  fio_unlock(&s->lock);
finish:
  fio_subscription_free(s);
  return;
try_again_later:
  fio_unlock(&s->unsubscribed);
  fio_defer(fio___unsubscribe_deferred, s, NULL);
}

/** SublimeText 3 marker */
void fio_unsubscribe_uuid___(void);
/**
 * Cancels an existing subscriptions that was bound to a connection's UUID. See
 * `fio_subscribe` and `fio_unsubscribe` for more details.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored.
 */
void fio_unsubscribe_uuid FIO_NOOP(subscribe_args_s args) {
  if (args.filter) {
    args.channel = (fio_str_info_s){
        .buf = (char *)&args.filter,
        .len = sizeof(args.filter),
    };
  }
  fio_uuid_env_remove(args.uuid, .name = args.channel,
                      .type = (-1 - ((uintptr_t)args.match >> 8) -
                               ((uintptr_t)((uint32_t)args.filter) << 1)));
}

/**
 * This helper returns a temporary String with the subscription's channel (or a
 * string representing the filter).
 *
 * To keep the string beyond the lifetime of the subscription, copy the string.
 */
fio_str_info_s fio_subscription_channel(subscription_s *subscription) {
  return (fio_str_info_s){.buf = subscription->parent->name,
                          .len = subscription->parent->name_len};
}

/* *****************************************************************************
Engine handling and Management
***************************************************************************** */

/* implemented later, informs root process about pub/sub subscriptions */
static inline void fio_cluster_inform_root_about_channel(channel_s *ch,
                                                         int add);

/* runs in lock(!) let'm all know */
static void fio_pubsub_on_channel_create(channel_s *ch) {
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(&fio_postoffice.engines.set, pos) {
    if (!pos->hash)
      continue;
    pos->obj->subscribe(pos->obj,
                        (fio_str_info_s){.buf = ch->name, .len = ch->name_len},
                        ch->match);
  }
  fio_unlock(&fio_postoffice.engines.lock);
  fio_cluster_inform_root_about_channel(ch, 1);
}

/* runs in lock(!) let'm all know */
static void fio_pubsub_on_channel_destroy(channel_s *ch) {
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(&fio_postoffice.engines.set, pos) {
    if (!pos->hash)
      continue;
    pos->obj->unsubscribe(
        pos->obj, (fio_str_info_s){.buf = ch->name, .len = ch->name_len},
        ch->match);
  }
  fio_unlock(&fio_postoffice.engines.lock);
  fio_cluster_inform_root_about_channel(ch, 0);
}

/**
 * Attaches an engine, so it's callback can be called by facil.io.
 *
 * The `subscribe` callback will be called for every existing channel.
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_attach(fio_pubsub_engine_s *engine) {
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_set_if_missing(&fio_postoffice.engines.set, (uintptr_t)engine,
                                engine);
  fio_unlock(&fio_postoffice.engines.lock);
  fio_pubsub_reattach(engine);
}

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine) {
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_remove(&fio_postoffice.engines.set, (uintptr_t)engine, engine,
                        NULL);
  fio_unlock(&fio_postoffice.engines.lock);
}

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine) {
  fio_pubsub_engine_s *addr;
  fio_lock(&fio_postoffice.engines.lock);
  addr = fio_engine_set_get(&fio_postoffice.engines.set, (uintptr_t)engine,
                            engine);
  fio_unlock(&fio_postoffice.engines.lock);
  return addr != NULL;
}

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_reattach(fio_pubsub_engine_s *eng) {
  fio_lock(&fio_postoffice.pubsub.lock);
  FIO_MAP_EACH(&fio_postoffice.pubsub.channels, pos) {
    if (!pos->hash)
      continue;
    eng->subscribe(
        eng, (fio_str_info_s){.buf = pos->obj->name, .len = pos->obj->name_len},
        NULL);
  }
  fio_unlock(&fio_postoffice.pubsub.lock);
  fio_lock(&fio_postoffice.patterns.lock);
  FIO_MAP_EACH(&fio_postoffice.patterns.channels, pos) {
    if (!pos->hash)
      continue;
    eng->subscribe(
        eng, (fio_str_info_s){.buf = pos->obj->name, .len = pos->obj->name_len},
        pos->obj->match);
  }
  fio_unlock(&fio_postoffice.patterns.lock);
}

/* *****************************************************************************
 * Message Metadata handling
 **************************************************************************** */

void fio_message_metadata_callback_set(fio_msg_metadata_fn callback,
                                       int enable) {
  if (!callback)
    return;
  fio_lock(&fio_postoffice.meta.lock);
  fio_meta_ary_remove2(&fio_postoffice.meta.ary, callback);
  if (enable)
    fio_meta_ary_push(&fio_postoffice.meta.ary, callback);
  fio_unlock(&fio_postoffice.meta.lock);
}

/** Finds the message's metadata by it's type ID. */
void *fio_message_metadata(fio_msg_s *msg, intptr_t type_id) {
  fio_msg_metadata_s *meta = ((fio_msg_client_s *)msg)->meta;
  size_t len = ((fio_msg_client_s *)msg)->meta_len;
  while (len) {
    --len;
    if (meta[len].type_id == type_id)
      return meta[len].metadata;
  }
  return NULL;
}

/* *****************************************************************************
 * Publishing to the subsriptions
 **************************************************************************** */

/* common internal tasks */
static channel_s *fio_channel_find_dup_internal(channel_s *ch_tmp,
                                                uint64_t hashed,
                                                fio_collection_s *c) {
  fio_lock(&c->lock);
  channel_s *ch = fio_ch_set_get(&c->channels, hashed, ch_tmp);
  if (!ch) {
    fio_unlock(&c->lock);
    return NULL;
  }
  fio_channel_dup(ch);
  fio_unlock(&c->lock);
  return ch;
}

/** Finds a filter channel, increasing it's reference count if it exists. */
static channel_s *fio_filter_find_dup(uint32_t filter) {
  channel_s tmp = {.name = (char *)(&filter), .name_len = sizeof(filter)};
  channel_s *ch =
      fio_channel_find_dup_internal(&tmp, filter, &fio_postoffice.filters);
  return ch;
}

/** Finds a pubsub channel, increasing it's reference count if it exists. */
static channel_s *fio_channel_find_dup(fio_str_info_s name) {
  channel_s tmp = {.name = name.buf, .name_len = name.len};
  uint64_t hashed_name = FIO_HASH_FN(name.buf, name.len, &fio_postoffice.pubsub,
                                     &fio_postoffice.pubsub);
  channel_s *ch =
      fio_channel_find_dup_internal(&tmp, hashed_name, &fio_postoffice.pubsub);
  return ch;
}

/* defers the callback (mark only) */
void fio_message_defer(fio_msg_s *msg_) {
  fio_msg_client_s *cl = (fio_msg_client_s *)msg_;
  cl->marker = 1;
}

/* performs the actual callback */
static void fio_perform_subscription_callback(void *s_, void *msg_) {
  subscription_s *s = s_;
  if (fio_trylock(&s->lock)) {
    fio_defer_push_task(fio_perform_subscription_callback, s_, msg_);
    return;
  }
  fio_msg_internal_s *msg = (fio_msg_internal_s *)msg_;
  fio_msg_client_s m = {
      .msg =
          {
              .channel = msg->channel,
              .uuid = s->uuid,
              .msg = msg->data,
              .filter = msg->filter,
              .udata1 = s->udata1,
              .udata2 = s->udata2,
          },
      .meta_len = msg->meta_len,
      .meta = msg->meta,
      .marker = 0,
  };
  if (s->on_message && (!s->uuid || uuid_is_valid(s->uuid))) {
    /* the on_message callback is removed when a subscription is canceled. */
    if (!s->uuid) {
      s->on_message(&m.msg);
    } else if (uuid_is_valid(s->uuid)) {
      /* UUID bound subscriptions perform within a protocol's task lock */
      m.msg.pr = protocol_try_lock(fio_uuid2fd(s->uuid), FIO_PR_LOCK_TASK);
      if (m.msg.pr) {
        s->on_message(&m.msg);
        protocol_unlock(m.msg.pr, FIO_PR_LOCK_TASK);
      } else if (errno == EWOULDBLOCK) {
        m.marker = 1;
      }
    }
  }
  fio_unlock(&s->lock);
  if (m.marker) {
    fio_defer_push_task(fio_perform_subscription_callback, s_, msg_);
    return;
  }
  fio_msg_internal_free(msg);
  fio_subscription_free(s);
}

/** UNSAFE! publishes a message to a channel, managing the reference counts */
static void fio_publish2channel(channel_s *ch, fio_msg_internal_s *msg) {
  FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, s) {
    if (!s || s->on_message == fio_mock_on_message) {
      continue;
    }
    fio_atomic_add(&s->ref, 1);
    fio_atomic_add(&msg->ref, 1);
    fio_defer_push_task(fio_perform_subscription_callback, s, msg);
  }
  fio_msg_internal_free(msg);
}
static void fio_publish2channel_task(void *ch_, void *msg) {
  channel_s *ch = ch_;
  if (!ch_)
    return;
  if (!msg)
    goto finish;
  if (fio_trylock(&ch->lock)) {
    fio_defer_push_urgent(fio_publish2channel_task, ch, msg);
    return;
  }
  fio_publish2channel(ch, msg);
  fio_unlock(&ch->lock);
finish:
  fio_channel_free(ch);
}

/** Publishes the message to the current process and frees the strings. */
static void fio_publish2process(fio_msg_internal_s *m) {
  fio_msg_internal_finalize(m);
  channel_s *ch;
  if (m->filter) {
    ch = fio_filter_find_dup(m->filter);
    if (!ch) {
      goto finish;
    }
  } else {
    ch = fio_channel_find_dup(m->channel);
  }
  /* exact match */
  if (ch) {
    fio_defer_push_urgent(fio_publish2channel_task, ch,
                          fio_msg_internal_dup(m));
  }
  if (m->filter == 0) {
    /* pattern matching match */
    fio_lock(&fio_postoffice.patterns.lock);
    FIO_MAP_EACH(&fio_postoffice.patterns.channels, p) {
      if (!p->hash) {
        continue;
      }

      if (p->obj->match(
              (fio_str_info_s){.buf = p->obj->name, .len = p->obj->name_len},
              m->channel)) {
        fio_channel_dup(p->obj);
        fio_defer_push_urgent(fio_publish2channel_task, p->obj,
                              fio_msg_internal_dup(m));
      }
    }
    fio_unlock(&fio_postoffice.patterns.lock);
  }
finish:
  fio_msg_internal_free(m);
}

/* *****************************************************************************
 * Data Structures - Core Structures
 **************************************************************************** */

#define CLUSTER_READ_BUFFER 16384

#define FIO_MAP_NAME fio_sub_hash
#define FIO_MAP_TYPE subscription_s *
#define FIO_MAP_KEY fio_str_s
#define FIO_MAP_KEY_COPY(k1, k2)                                               \
  (k1) = (fio_str_s)FIO_STRING_INIT;                                           \
  fio_str_concat(&(k1), &(k2))
#define FIO_MAP_KEY_CMP(k1, k2) fio_str_is_eq(&(k1), &(k2))
#define FIO_MAP_KEY_DESTROY(key) fio_str_destroy(&(key))
#define FIO_MAP_TYPE_DESTROY(obj) fio_unsubscribe(obj)
#include <fio-stl.h>

#define FIO_ARRAY_NAME cluster_clients
#define FIO_ARRAY_TYPE intptr_t
#define FIO_ARRAY_TYPE_DESTROY(a) fio_close((a))
#include "fio-stl.h"

#define FIO_CLUSTER_NAME_LIMIT 255

typedef struct cluster_pr_s {
  fio_protocol_s protocol;
  fio_msg_internal_s *msg;
  void (*handler)(struct cluster_pr_s *pr);
  void (*sender)(void *data, intptr_t avoid_uuid);
  fio_sub_hash_s pubsub;
  fio_sub_hash_s patterns;
  intptr_t uuid;
  uint32_t exp_channel;
  uint32_t exp_msg;
  uint32_t type;
  int32_t filter;
  uint32_t len;
  fio_lock_i lock;
  uint8_t buf[CLUSTER_READ_BUFFER];
} cluster_pr_s;

static struct cluster_data_s {
  intptr_t uuid;
  cluster_clients_s clients;
  fio_lock_i lock;
  char name[FIO_CLUSTER_NAME_LIMIT + 1];
} cluster_data = {.clients = FIO_ARRAY_INIT, .lock = FIO_LOCK_INIT};

static void fio_cluster_data_cleanup(int delete_file) {
  if (delete_file && cluster_data.name[0]) {
#if DEBUG
    FIO_LOG_DEBUG("(%d) unlinking cluster's Unix socket.", (int)getpid());
#endif
    unlink(cluster_data.name);
  }
  cluster_clients_destroy(&cluster_data.clients);
  cluster_data.uuid = 0;
  cluster_data.lock = FIO_LOCK_INIT;
}

static void fio_cluster_cleanup(void *ignore) {
  /* cleanup the cluster data */
  fio_cluster_data_cleanup(fio_master_pid() == getpid());
  (void)ignore;
}

static void fio_cluster_init(void) {
  fio_cluster_data_cleanup(0);
  /* create a unique socket name */
  char *tmp_folder = getenv("TMPDIR");
  uint32_t tmp_folder_len = 0;
  if (!tmp_folder || ((tmp_folder_len = (uint32_t)strlen(tmp_folder)) >
                      (FIO_CLUSTER_NAME_LIMIT - 28))) {
#ifdef P_tmpdir
    tmp_folder = (char *)P_tmpdir;
    if (tmp_folder)
      tmp_folder_len = (uint32_t)strlen(tmp_folder);
#else
    tmp_folder = "/tmp/";
    tmp_folder_len = 5;
#endif
  }
  if (tmp_folder_len >= (FIO_CLUSTER_NAME_LIMIT - 28)) {
    tmp_folder_len = 0;
  }
  if (tmp_folder_len) {
    memcpy(cluster_data.name, tmp_folder, tmp_folder_len);
    if (cluster_data.name[tmp_folder_len - 1] != '/')
      cluster_data.name[tmp_folder_len++] = '/';
  }
  memcpy(cluster_data.name + tmp_folder_len, "facil-io-sock-", 14);
  tmp_folder_len += 14;
  tmp_folder_len +=
      snprintf(cluster_data.name + tmp_folder_len,
               FIO_CLUSTER_NAME_LIMIT - tmp_folder_len, "%d", (int)getpid());
  cluster_data.name[tmp_folder_len] = 0;

  /* remove if existing */
  unlink(cluster_data.name);
  /* add cleanup callback */
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio_cluster_cleanup, NULL);
}

/* *****************************************************************************
 * Cluster Protocol callbacks
 **************************************************************************** */

static inline void fio_cluster_protocol_free(void *pr) { fio_free(pr); }

static uint8_t fio_cluster_on_shutdown(intptr_t uuid, fio_protocol_s *pr_) {
  cluster_pr_s *p = (cluster_pr_s *)pr_;
  p->sender(fio_msg_internal_create(0, FIO_CLUSTER_MSG_SHUTDOWN,
                                    (fio_str_info_s){.len = 0},
                                    (fio_str_info_s){.len = 0}, 0, 1),
            -1);
  return 255;
  (void)pr_;
  (void)uuid;
}

static void fio_cluster_on_data(intptr_t uuid, fio_protocol_s *pr_) {
  cluster_pr_s *c = (cluster_pr_s *)pr_;
  ssize_t i = fio_read(uuid, c->buf + c->len, CLUSTER_READ_BUFFER - c->len);
  if (i <= 0)
    return;
  c->len += i;
  i = 0;
  do {
    if (!c->exp_channel && !c->exp_msg) {
      if (c->len - i < 16)
        break;
      c->exp_channel = fio_buf2u32(c->buf + i) + 1;
      c->exp_msg = fio_buf2u32(c->buf + i + 4) + 1;
      c->type = fio_buf2u32(c->buf + i + 8);
      c->filter = (int32_t)fio_buf2u32(c->buf + i + 12);
      if (c->exp_channel) {
        if (c->exp_channel >= (1024 * 1024 * 16) + 1) {
          FIO_LOG_FATAL("(%d) cluster message name too long (16Mb limit): %u\n",
                        (int)getpid(), (unsigned int)c->exp_channel);
          exit(1);
          return;
        }
      }
      if (c->exp_msg) {
        if (c->exp_msg >= (1024 * 1024 * 64) + 1) {
          FIO_LOG_FATAL("(%d) cluster message data too long (64Mb limit): %u\n",
                        (int)getpid(), (unsigned int)c->exp_msg);
          exit(1);
          return;
        }
      }
      c->msg = fio_msg_internal_create(
          c->filter, c->type,
          (fio_str_info_s){.buf = (char *)(c->msg + 1),
                           .len = c->exp_channel - 1},
          (fio_str_info_s){.buf = ((char *)(c->msg + 1) + c->exp_channel + 1),
                           .len = c->exp_msg - 1},
          (int8_t)(c->type == FIO_CLUSTER_MSG_JSON ||
                   c->type == FIO_CLUSTER_MSG_ROOT_JSON),
          0);
      i += 16;
    }
    if (c->exp_channel) {
      if (c->exp_channel + i > c->len) {
        memcpy(c->msg->channel.buf +
                   ((c->msg->channel.len + 1) - c->exp_channel),
               (char *)c->buf + i, (size_t)(c->len - i));
        c->exp_channel -= (c->len - i);
        i = c->len;
        break;
      } else {
        memcpy(c->msg->channel.buf +
                   ((c->msg->channel.len + 1) - c->exp_channel),
               (char *)c->buf + i, (size_t)(c->exp_channel));
        i += c->exp_channel;
        c->exp_channel = 0;
      }
    }
    if (c->exp_msg) {
      if (c->exp_msg + i > c->len) {
        memcpy(c->msg->data.buf + ((c->msg->data.len + 1) - c->exp_msg),
               (char *)c->buf + i, (size_t)(c->len - i));
        c->exp_msg -= (c->len - i);
        i = c->len;
        break;
      } else {
        memcpy(c->msg->data.buf + ((c->msg->data.len + 1) - c->exp_msg),
               (char *)c->buf + i, (size_t)(c->exp_msg));
        i += c->exp_msg;
        c->exp_msg = 0;
      }
    }
    fio_postoffice_meta_update(c->msg);
    c->handler(c);
    fio_msg_internal_free(c->msg);
    c->msg = NULL;
  } while (c->len > i);
  c->len -= i;
  if (c->len && i) {
    memmove(c->buf, c->buf + i, c->len);
  }
  (void)pr_;
}

static void fio_cluster_ping(intptr_t uuid, fio_protocol_s *pr_) {
  fio_msg_internal_s *m = fio_msg_internal_create(
      0, FIO_CLUSTER_MSG_PING, (fio_str_info_s){.len = 0},
      (fio_str_info_s){.len = 0}, 0, 1);
  fio_msg_internal_send_dup(uuid, m);
  fio_msg_internal_free(m);
  (void)pr_;
}

static void fio_cluster_on_close(intptr_t uuid, fio_protocol_s *pr_) {
  cluster_pr_s *c = (cluster_pr_s *)pr_;
  if (!fio_data->is_worker) {
    /* a child was lost, respawning is handled elsewhere. */
    fio_lock(&cluster_data.lock);
    cluster_clients_remove2(&cluster_data.clients, uuid);
    fio_unlock(&cluster_data.lock);
  } else if (fio_data->active) {
    /* no shutdown message received - parent crashed. */
    if (c->type != FIO_CLUSTER_MSG_SHUTDOWN && fio_is_running()) {
      FIO_LOG_FATAL("(%d) Parent Process crash detected!", (int)getpid());
      fio_state_callback_force(FIO_CALL_ON_PARENT_CRUSH);
      fio_state_callback_clear(FIO_CALL_ON_PARENT_CRUSH);
      fio_cluster_data_cleanup(1);
      fio_stop();
    }
  }
  if (c->msg)
    fio_msg_internal_free(c->msg);
  c->msg = NULL;
  fio_sub_hash_destroy(&c->pubsub);
  fio_cluster_protocol_free(c);
  (void)uuid;
}

static inline fio_protocol_s *
fio_cluster_protocol_alloc(intptr_t uuid,
                           void (*handler)(struct cluster_pr_s *pr),
                           void (*sender)(void *data, intptr_t auuid)) {
  cluster_pr_s *p = fio_mmap(sizeof(*p));
  if (!p) {
    FIO_LOG_FATAL("Cluster protocol allocation failed.");
    exit(errno);
  }
  p->protocol = (fio_protocol_s){
      .ping = fio_cluster_ping,
      .on_close = fio_cluster_on_close,
      .on_shutdown = fio_cluster_on_shutdown,
      .on_data = fio_cluster_on_data,
  };
  p->uuid = uuid;
  p->handler = handler;
  p->sender = sender;
  p->pubsub = (fio_sub_hash_s)FIO_MAP_INIT;
  p->patterns = (fio_sub_hash_s)FIO_MAP_INIT;
  p->lock = FIO_LOCK_INIT;
  return &p->protocol;
}

/* *****************************************************************************
 * Master (server) IPC Connections
 **************************************************************************** */

static void fio_cluster_server_sender(void *m_, intptr_t avoid_uuid) {
  fio_msg_internal_s *m = m_;
  fio_lock(&cluster_data.lock);
  FIO_ARRAY_EACH(&cluster_data.clients, pos) {
    if (*pos != -1) {
      if (*pos != avoid_uuid) {
        fio_msg_internal_send_dup(*pos, m);
      }
    }
  }
  fio_unlock(&cluster_data.lock);
  fio_msg_internal_free(m);
}

static void fio_cluster_server_handler(struct cluster_pr_s *pr) {
  /* what to do? */
  // fprintf(stderr, "-");
  switch ((fio_cluster_message_type_e)pr->type) {

  case FIO_CLUSTER_MSG_FORWARD: /* fallthrough */
  case FIO_CLUSTER_MSG_JSON: {
    fio_cluster_server_sender(fio_msg_internal_dup(pr->msg), pr->uuid);
    fio_publish2process(fio_msg_internal_dup(pr->msg));
    break;
  }

  case FIO_CLUSTER_MSG_PUBSUB_SUB: {
    subscription_s *s =
        fio_subscribe(.on_message = fio_mock_on_message, .match = NULL,
                      .channel = pr->msg->channel);
    fio_str_s tmp = (fio_str_s)FIO_STRING_INIT_EXISTING(
        pr->msg->channel.buf, pr->msg->channel.len, 0, NULL); // don't free
    fio_lock(&pr->lock);
    fio_sub_hash_set(&pr->pubsub,
                     FIO_HASH_FN(pr->msg->channel.buf, pr->msg->channel.len,
                                 &fio_postoffice.pubsub,
                                 &fio_postoffice.pubsub),
                     tmp, s, NULL);
    fio_unlock(&pr->lock);
    break;
  }
  case FIO_CLUSTER_MSG_PUBSUB_UNSUB: {
    fio_str_s tmp = FIO_STRING_INIT_EXISTING(
        pr->msg->channel.buf, pr->msg->channel.len, 0, NULL); // don't free
    fio_lock(&pr->lock);
    fio_sub_hash_remove(&pr->pubsub,
                        FIO_HASH_FN(pr->msg->channel.buf, pr->msg->channel.len,
                                    &fio_postoffice.pubsub,
                                    &fio_postoffice.pubsub),
                        tmp, NULL);
    fio_unlock(&pr->lock);
    break;
  }

  case FIO_CLUSTER_MSG_PATTERN_SUB: {
    uintptr_t match = fio_buf2u64(pr->msg->data.buf);
    subscription_s *s = fio_subscribe(.on_message = fio_mock_on_message,
                                      .match = (fio_match_fn)match,
                                      .channel = pr->msg->channel);
    fio_str_s tmp = FIO_STRING_INIT_EXISTING(
        pr->msg->channel.buf, pr->msg->channel.len, 0, NULL); // don't free
    fio_lock(&pr->lock);
    fio_sub_hash_set(&pr->patterns,
                     FIO_HASH_FN(pr->msg->channel.buf, pr->msg->channel.len,
                                 &fio_postoffice.pubsub,
                                 &fio_postoffice.pubsub),
                     tmp, s, NULL);
    fio_unlock(&pr->lock);
    break;
  }

  case FIO_CLUSTER_MSG_PATTERN_UNSUB: {
    fio_str_s tmp = FIO_STRING_INIT_EXISTING(
        pr->msg->channel.buf, pr->msg->channel.len, 0, NULL); // don't free
    fio_lock(&pr->lock);
    fio_sub_hash_remove(&pr->patterns,
                        FIO_HASH_FN(pr->msg->channel.buf, pr->msg->channel.len,
                                    &fio_postoffice.pubsub,
                                    &fio_postoffice.pubsub),
                        tmp, NULL);
    fio_unlock(&pr->lock);
    break;
  }

  case FIO_CLUSTER_MSG_ROOT_JSON:
    pr->type = FIO_CLUSTER_MSG_JSON; /* fallthrough */
  case FIO_CLUSTER_MSG_ROOT:
    fio_publish2process(fio_msg_internal_dup(pr->msg));
    break;

  case FIO_CLUSTER_MSG_SHUTDOWN: /* fallthrough */
  case FIO_CLUSTER_MSG_ERROR:    /* fallthrough */
  case FIO_CLUSTER_MSG_PING:     /* fallthrough */
  default:
    break;
  }
}

/** Called when a ne client is available */
static void fio_cluster_listen_accept(intptr_t uuid, fio_protocol_s *protocol) {
  (void)protocol;
  /* prevent `accept` backlog in parent */
  intptr_t client;
  while ((client = fio_accept(uuid)) != -1) {
    fio_attach(client,
               fio_cluster_protocol_alloc(client, fio_cluster_server_handler,
                                          fio_cluster_server_sender));
    fio_lock(&cluster_data.lock);
    cluster_clients_push(&cluster_data.clients, client);
    fio_unlock(&cluster_data.lock);
  }
}

/** Called when the connection was closed, but will not run concurrently */
static void fio_cluster_listen_on_close(intptr_t uuid,
                                        fio_protocol_s *protocol) {
  free(protocol);
  cluster_data.uuid = -1;
  if (fio_master_pid() == getpid()) {
#if DEBUG
    FIO_LOG_DEBUG("(%d) stopped listening for cluster connections",
                  (int)getpid());
#endif
    if (fio_data->active)
      fio_stop();
  }
  (void)uuid;
}

static void fio_listen2cluster(void *ignore) {
  /* this is called for each `fork`, but we only need this to run once. */
  fio_lock(&fio_fork_lock); /* prevent forking. */
  fio_lock(&cluster_data.lock);
  cluster_data.uuid =
      fio_socket(cluster_data.name, NULL,
                 FIO_SOCKET_UNIX | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  fio_unlock(&cluster_data.lock);
  if (cluster_data.uuid < 0) {
    FIO_LOG_FATAL("(facil.io cluster) failed to open cluster socket.");
    perror("             check file permissions. errno:");
    exit(errno);
  }
  fio_protocol_s *p = malloc(sizeof(*p));
  FIO_ASSERT_ALLOC(p);
  *p = (fio_protocol_s){
      .on_data = fio_cluster_listen_accept,
      .on_shutdown = mock_on_shutdown_eternal,
      .ping = FIO_PING_ETERNAL,
      .on_close = fio_cluster_listen_on_close,
  };
  FIO_LOG_DEBUG("(%d) Listening to cluster: %s", (int)getpid(),
                cluster_data.name);
  fio_attach(cluster_data.uuid, p);
  fio_unlock(&fio_fork_lock); /* allow forking. */
  (void)ignore;
}

/* *****************************************************************************
 * Worker (client) IPC connections
 **************************************************************************** */

static void fio_cluster_client_handler(struct cluster_pr_s *pr) {
  /* what to do? */
  switch ((fio_cluster_message_type_e)pr->type) {
  case FIO_CLUSTER_MSG_FORWARD: /* fallthrough */
  case FIO_CLUSTER_MSG_JSON:
    fio_publish2process(fio_msg_internal_dup(pr->msg));
    break;
  case FIO_CLUSTER_MSG_SHUTDOWN:
    fio_stop();
  case FIO_CLUSTER_MSG_ERROR:         /* fallthrough */
  case FIO_CLUSTER_MSG_PING:          /* fallthrough */
  case FIO_CLUSTER_MSG_ROOT:          /* fallthrough */
  case FIO_CLUSTER_MSG_ROOT_JSON:     /* fallthrough */
  case FIO_CLUSTER_MSG_PUBSUB_SUB:    /* fallthrough */
  case FIO_CLUSTER_MSG_PUBSUB_UNSUB:  /* fallthrough */
  case FIO_CLUSTER_MSG_PATTERN_SUB:   /* fallthrough */
  case FIO_CLUSTER_MSG_PATTERN_UNSUB: /* fallthrough */

  default:
    break;
  }
}
static void fio_cluster_client_sender(void *m_, intptr_t ignr_) {
  fio_msg_internal_s *m = m_;
  if (!uuid_is_valid(cluster_data.uuid) && fio_data->active) {
    /* delay message delivery until we have a vaild uuid */
    fio_defer_push_task((void (*)(void *, void *))fio_cluster_client_sender, m_,
                        (void *)ignr_);
    return;
  }
  fio_msg_internal_send_dup(cluster_data.uuid, m);
  fio_msg_internal_free(m);
}

/** The address of the server we are connecting to. */
// char *address;
/** The port on the server we are connecting to. */
// char *port;
/**
 * The `on_connect` callback should return a pointer to a protocol object
 * that will handle any connection related events.
 *
 * Should either call `facil_attach` or close the connection.
 */
static void fio_cluster_on_connect(intptr_t uuid, void *udata) {
  cluster_data.uuid = uuid;

  /* inform root about all existing channels */
  fio_lock(&fio_postoffice.pubsub.lock);
  FIO_MAP_EACH(&fio_postoffice.pubsub.channels, pos) {
    if (!pos->hash) {
      continue;
    }
    fio_cluster_inform_root_about_channel(pos->obj, 1);
  }
  fio_unlock(&fio_postoffice.pubsub.lock);
  fio_lock(&fio_postoffice.patterns.lock);
  FIO_MAP_EACH(&fio_postoffice.patterns.channels, pos) {
    if (!pos->hash) {
      continue;
    }
    fio_cluster_inform_root_about_channel(pos->obj, 1);
  }
  fio_unlock(&fio_postoffice.patterns.lock);

  fio_attach(uuid, fio_cluster_protocol_alloc(uuid, fio_cluster_client_handler,
                                              fio_cluster_client_sender));
  (void)udata;
}
/**
 * The `on_fail` is called when a socket fails to connect. The old sock UUID
 * is passed along.
 */
static void fio_cluster_on_fail(intptr_t uuid, void *udata) {
  FIO_LOG_FATAL("[%d] (facil.io) unknown cluster connection error", getpid());
  perror("       errno");
  kill(fio_master_pid(), SIGINT);
  fio_stop();
  // exit(errno ? errno : 1);
  (void)udata;
  (void)uuid;
}

static void fio_connect2cluster(void *ignore) {
  if (cluster_data.uuid)
    fio_force_close(cluster_data.uuid);
  cluster_data.uuid = 0;
  /* this is called for each child, but not for single a process worker. */
  fio_connect(.address = cluster_data.name, .port = NULL,
              .on_connect = fio_cluster_on_connect,
              .on_fail = fio_cluster_on_fail);
  (void)ignore;
}

static void fio_send2cluster(fio_msg_internal_s *m) {
  if (!fio_is_running()) {
    FIO_LOG_ERROR("facio.io cluster inactive, can't send message.");
    return;
  }
  if (fio_data->workers == 1) {
    /* nowhere to send to */
    return;
  }
  if (fio_is_master()) {
    fio_cluster_server_sender(fio_msg_internal_dup(m), -1);
  } else {
    fio_cluster_client_sender(fio_msg_internal_dup(m), -1);
  }
}

/* *****************************************************************************
 * Propegation
 **************************************************************************** */

static inline void fio_cluster_inform_root_about_channel(channel_s *ch,
                                                         int add) {
  if (!fio_data->is_worker || fio_data->workers == 1 || !cluster_data.uuid ||
      !ch)
    return;
  fio_str_info_s ch_name = {.buf = ch->name, .len = ch->name_len};
  fio_str_info_s msg = {.buf = NULL, .len = 0};
#if DEBUG
  FIO_LOG_DEBUG("(%d) informing root about: %s (%zu) msg type %d",
                (int)getpid(), ch_name.buf, ch_name.len,
                (ch->match ? (add ? FIO_CLUSTER_MSG_PATTERN_SUB
                                  : FIO_CLUSTER_MSG_PATTERN_UNSUB)
                           : (add ? FIO_CLUSTER_MSG_PUBSUB_SUB
                                  : FIO_CLUSTER_MSG_PUBSUB_UNSUB)));
#endif
  char buf[8] = {0};
  if (ch->match) {
    fio_u2buf64(buf, (uint64_t)ch->match);
    msg.buf = buf;
    msg.len = sizeof(ch->match);
  }

  fio_cluster_client_sender(
      fio_msg_internal_create(0,
                              (ch->match
                                   ? (add ? FIO_CLUSTER_MSG_PATTERN_SUB
                                          : FIO_CLUSTER_MSG_PATTERN_UNSUB)
                                   : (add ? FIO_CLUSTER_MSG_PUBSUB_SUB
                                          : FIO_CLUSTER_MSG_PUBSUB_UNSUB)),
                              ch_name, msg, 0, 1),
      -1);
}

/* *****************************************************************************
 * Initialization
 **************************************************************************** */

static void fio_accept_after_fork(void *ignore) {
  /* prevent `accept` backlog in parent */
  fio_cluster_listen_accept(cluster_data.uuid, NULL);
  (void)ignore;
}

static void fio_cluster_at_exit(void *ignore) {
  /* unlock all */
  fio_pubsub_on_fork();
  /* clear subscriptions of all types */
  while (fio_ch_set_count(&fio_postoffice.patterns.channels)) {
    channel_s *ch = fio_ch_set_last(&fio_postoffice.patterns.channels);
    while (subscriptions_any(&ch->subscriptions)) {
      fio_unsubscribe(subscriptions_root(ch->subscriptions.next));
    }
    fio_ch_set_pop(&fio_postoffice.patterns.channels, NULL);
  }

  while (fio_ch_set_count(&fio_postoffice.pubsub.channels)) {
    channel_s *ch = fio_ch_set_last(&fio_postoffice.pubsub.channels);
    while (subscriptions_any(&ch->subscriptions)) {
      fio_unsubscribe(subscriptions_root(ch->subscriptions.next));
    }
    fio_ch_set_pop(&fio_postoffice.pubsub.channels, NULL);
  }

  while (fio_ch_set_count(&fio_postoffice.filters.channels)) {
    channel_s *ch = fio_ch_set_last(&fio_postoffice.filters.channels);
    while (subscriptions_any(&ch->subscriptions)) {
      fio_unsubscribe(subscriptions_root(ch->subscriptions.next));
    }
    fio_ch_set_pop(&fio_postoffice.filters.channels, NULL);
  }
  fio_ch_set_destroy(&fio_postoffice.filters.channels);
  fio_ch_set_destroy(&fio_postoffice.patterns.channels);
  fio_ch_set_destroy(&fio_postoffice.pubsub.channels);

  /* clear engines */
  FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;
  while (fio_engine_set_count(&fio_postoffice.engines.set)) {
    fio_pubsub_detach(fio_engine_set_last(&fio_postoffice.engines.set));
    fio_engine_set_last(&fio_postoffice.engines.set);
  }
  fio_engine_set_destroy(&fio_postoffice.engines.set);

  /* clear meta hooks */
  fio_meta_ary_destroy(&fio_postoffice.meta.ary);
  /* perform newly created tasks */
  fio_defer_perform();
  (void)ignore;
}

static void fio_pubsub_initialize(void) {
  fio_cluster_init();
  fio_state_callback_add(FIO_CALL_PRE_START, fio_listen2cluster, NULL);
  fio_state_callback_add(FIO_CALL_IN_MASTER, fio_accept_after_fork, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio_connect2cluster, NULL);
  fio_state_callback_add(FIO_CALL_ON_FINISH, fio_cluster_cleanup, NULL);
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio_cluster_at_exit, NULL);
}

/* *****************************************************************************
Cluster forking handler
***************************************************************************** */

static void fio_pubsub_on_fork(void) {
  fio_postoffice.filters.lock = FIO_LOCK_INIT;
  fio_postoffice.pubsub.lock = FIO_LOCK_INIT;
  fio_postoffice.patterns.lock = FIO_LOCK_INIT;
  fio_postoffice.engines.lock = FIO_LOCK_INIT;
  fio_postoffice.meta.lock = FIO_LOCK_INIT;
  cluster_data.lock = FIO_LOCK_INIT;
  cluster_data.uuid = 0;
  FIO_MAP_EACH(&fio_postoffice.filters.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(&fio_postoffice.pubsub.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(&fio_postoffice.patterns.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
}

/* *****************************************************************************
 * External API
 **************************************************************************** */

/** Signals children (or self) to shutdown) - NOT signal safe. */
static void fio_cluster_signal_children(void) {
  if (fio_master_pid() != getpid()) {
    fio_stop();
    return;
  }
  fio_cluster_server_sender(fio_msg_internal_create(0, FIO_CLUSTER_MSG_SHUTDOWN,
                                                    (fio_str_info_s){.len = 0},
                                                    (fio_str_info_s){.len = 0},
                                                    0, 1),
                            -1);
}

/* Sublime Text marker */
void fio_publish___(fio_publish_args_s args);
/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `facil_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
void fio_publish FIO_NOOP(fio_publish_args_s args) {
  if (args.filter && !args.engine) {
    args.engine = FIO_PUBSUB_CLUSTER;
  } else if (!args.engine) {
    args.engine = FIO_PUBSUB_DEFAULT;
  }
  fio_msg_internal_s *m = NULL;
  switch ((uintptr_t)args.engine) {
  case 0UL: /* fallthrough (missing default) */
  case 1UL: // ((uintptr_t)FIO_PUBSUB_CLUSTER):
    m = fio_msg_internal_create(
        args.filter,
        (args.is_json ? FIO_CLUSTER_MSG_JSON : FIO_CLUSTER_MSG_FORWARD),
        args.channel, args.message, args.is_json, 1);
    fio_send2cluster(m);
    fio_publish2process(m);
    break;
  case 2UL: // ((uintptr_t)FIO_PUBSUB_PROCESS):
    m = fio_msg_internal_create(args.filter, 0, args.channel, args.message,
                                args.is_json, 1);
    fio_publish2process(m);
    break;
  case 3UL: // ((uintptr_t)FIO_PUBSUB_SIBLINGS):
    m = fio_msg_internal_create(
        args.filter,
        (args.is_json ? FIO_CLUSTER_MSG_JSON : FIO_CLUSTER_MSG_FORWARD),
        args.channel, args.message, args.is_json, 1);
    fio_send2cluster(m);
    fio_msg_internal_free(m);
    m = NULL;
    break;
  case 4UL: // ((uintptr_t)FIO_PUBSUB_ROOT):
    m = fio_msg_internal_create(
        args.filter,
        (args.is_json ? FIO_CLUSTER_MSG_ROOT_JSON : FIO_CLUSTER_MSG_ROOT),
        args.channel, args.message, args.is_json, 1);
    if (fio_data->is_worker == 0 || fio_data->workers == 1) {
      fio_publish2process(m);
    } else {
      fio_cluster_client_sender(m, -1);
    }
    break;
  default:
    if (args.filter != 0) {
      FIO_LOG_ERROR("(pub/sub) pub/sub engines can only be used for "
                    "pub/sub messages (no filter).");
      return;
    }
    args.engine->publish(args.engine, args.channel, args.message, args.is_json);
  }
  return;
}

/* *****************************************************************************
 * Glob Matching
 **************************************************************************** */

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
static int fio_glob_match(fio_str_info_s pat, fio_str_info_s ch) {
  /* adapted and rewritten, with thankfulness, from the code at:
   * https://github.com/opnfv/kvmfornfv/blob/master/kernel/lib/glob.c
   *
   * Original version's copyright:
   * Copyright 2015 Open Platform for NFV Project, Inc. and its contributors
   * Under the MIT license.
   */

  /*
   * Backtrack to previous * on mismatch and retry starting one
   * character later in the string.  Because * matches all characters,
   * there's never a need to backtrack multiple levels.
   */
  uint8_t *back_pat = NULL, *back_str = (uint8_t *)ch.buf;
  size_t back_pat_len = 0, back_str_len = ch.len;

  /*
   * Loop over each token (character or class) in pat, matching
   * it against the remaining unmatched tail of str.  Return false
   * on mismatch, or true after matching the trailing nul bytes.
   */
  while (ch.len) {
    uint8_t c = *(uint8_t *)ch.buf++;
    uint8_t d = *(uint8_t *)pat.buf++;
    ch.len--;
    pat.len--;

    switch (d) {
    case '?': /* Wildcard: anything goes */
      break;

    case '*':       /* Any-length wildcard */
      if (!pat.len) /* Optimize trailing * case */
        return 1;
      back_pat = (uint8_t *)pat.buf;
      back_pat_len = pat.len;
      back_str = (uint8_t *)--ch.buf; /* Allow zero-length match */
      back_str_len = ++ch.len;
      break;

    case '[': { /* Character class */
      uint8_t match = 0, inverted = (*(uint8_t *)pat.buf == '^');
      uint8_t *cls = (uint8_t *)pat.buf + inverted;
      uint8_t a = *cls++;

      /*
       * Iterate over each span in the character class.
       * A span is either a single character a, or a
       * range a-b.  The first span may begin with ']'.
       */
      do {
        uint8_t b = a;

        if (cls[0] == '-' && cls[1] != ']') {
          b = cls[1];

          cls += 2;
          if (a > b) {
            uint8_t tmp = a;
            a = b;
            b = tmp;
          }
        }
        match |= (a <= c && c <= b);
      } while ((a = *cls++) != ']');

      if (match == inverted)
        goto backtrack;
      pat.len -= cls - (uint8_t *)pat.buf;
      pat.buf = (char *)cls;

    } break;
    case '\\':
      d = *(uint8_t *)pat.buf++;
      pat.len--;
    /* fallthrough */
    default: /* Literal character */
      if (c == d)
        break;
    backtrack:
      if (!back_pat)
        return 0; /* No point continuing */
      /* Try again from last *, one character later in str. */
      pat.buf = (char *)back_pat;
      ch.buf = (char *)++back_str;
      ch.len = --back_str_len;
      pat.len = back_pat_len;
    }
  }
  return !ch.len && !pat.len;
}

fio_match_fn FIO_MATCH_GLOB = fio_glob_match;

#else /* FIO_PUBSUB_SUPPORT */

static void fio_pubsub_on_fork(void) {}
static void fio_cluster_init(void) {}
static void fio_cluster_signal_children(void) {}

#endif /* FIO_PUBSUB_SUPPORT */

/* *****************************************************************************
Section Start Marker







                       SSL/TLS Weak Symbols for TLS Support
        TLS Support (weak functions, to bea overriden by library wrapper)







***************************************************************************** */

#if !FIO_TLS_FOUND && !HAVE_OPENSSL /* TODO: list flags here */

#define REQUIRE_TLS_LIBRARY()

/* TODO: delete me! (in wrapper implementation) */
#undef FIO_TLS_WEAK
#define FIO_TLS_WEAK __attribute__((weak))
#if !FIO_TLS_IGNORE_MISSING_ERROR
#undef REQUIRE_TLS_LIBRARY
#define REQUIRE_TLS_LIBRARY()                                                  \
  FIO_LOG_FATAL("No supported SSL/TLS library available.");                    \
  exit(-1);
#endif
/* STOP deleting after this line */

/* *****************************************************************************
The SSL/TLS helper data types (can be left as is)
***************************************************************************** */

typedef struct {
  fio_str_s private_key;
  fio_str_s public_key;
  fio_str_s password;
} cert_s;

static inline int fio___tls_cert_cmp(const cert_s *dest, const cert_s *src) {
  return fio_str_is_eq(&dest->private_key, &src->private_key);
}
static inline void fio___tls_cert_copy(cert_s *dest, cert_s *src) {
  *dest = (cert_s){
      .private_key = FIO_STRING_INIT,
      .public_key = FIO_STRING_INIT,
      .password = FIO_STRING_INIT,
  };
  fio_str_concat(&dest->private_key, &src->private_key);
  fio_str_concat(&dest->public_key, &src->public_key);
  fio_str_concat(&dest->password, &src->password);
}
static inline void fio___tls_cert_destroy(cert_s *obj) {
  fio_str_destroy(&obj->private_key);
  fio_str_destroy(&obj->public_key);
  fio_str_destroy(&obj->password);
}

#define FIO_ARRAY_NAME fio___tls_cert_ary
#define FIO_ARRAY_TYPE cert_s
#define FIO_ARRAY_TYPE_CMP(k1, k2) (fio___tls_cert_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_cert_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key) fio___tls_cert_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID ((cert_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#include <fio-stl.h>

typedef struct {
  fio_str_s pem;
} trust_s;

static inline int fio___tls_trust_cmp(const trust_s *dest, const trust_s *src) {
  return fio_str_is_eq(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_copy(trust_s *dest, trust_s *src) {
  *dest = (trust_s){
      .pem = FIO_STRING_INIT,
  };
  fio_str_concat(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_destroy(trust_s *obj) {
  fio_str_destroy(&obj->pem);
}

#define FIO_ARRAY_NAME fio___tls_trust_ary
#define FIO_ARRAY_TYPE trust_s
#define FIO_ARRAY_TYPE_CMP(k1, k2) (fio___tls_trust_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_trust_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key) fio___tls_trust_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID ((trust_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#include <fio-stl.h>

typedef struct {
  fio_str_s name; /* fio_str_s provides cache locality for small strings */
  void (*on_selected)(intptr_t uuid, void *udata_connection, void *udata_tls);
  void *udata_tls;
  void (*on_cleanup)(void *udata_tls);
} alpn_s;

static inline int fio_alpn_cmp(const alpn_s *dest, const alpn_s *src) {
  return fio_str_is_eq(&dest->name, &src->name);
}
static inline void fio_alpn_copy(alpn_s *dest, alpn_s *src) {
  *dest = (alpn_s){
      .name = FIO_STRING_INIT,
      .on_selected = src->on_selected,
      .udata_tls = src->udata_tls,
      .on_cleanup = src->on_cleanup,
  };
  fio_str_concat(&dest->name, &src->name);
}
static inline void fio_alpn_destroy(alpn_s *obj) {
  if (obj->on_cleanup)
    obj->on_cleanup(obj->udata_tls);
  fio_str_destroy(&obj->name);
}

#define FIO_MAP_NAME fio___tls_alpn_list
#define FIO_MAP_TYPE alpn_s
#define FIO_MAP_TYPE_INVALID ((alpn_s){.udata_tls = NULL})
#define FIO_MAP_TYPE_INVALID_SIMPLE 1
#define FIO_MAP_TYPE_CMP(k1, k2) fio_alpn_cmp(&(k1), &(k2))
#define FIO_MAP_TYPE_COPY(dest, obj) fio_alpn_copy(&(dest), &(obj))
#define FIO_MAP_TYPE_DESTROY(key) fio_alpn_destroy(&(key))
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#include <fio-stl.h>

/* *****************************************************************************
The SSL/TLS Context type
***************************************************************************** */

/** An opaque type used for the SSL/TLS functions. */
struct fio_tls_s {
  size_t ref; /* Reference counter, to guards the ALPN registry */
  fio___tls_alpn_list_s
      alpn; /* ALPN is the name for the protocol selection extension */

  /*** the next two components could be optimized away with tweaking stuff ***/

  fio___tls_cert_ary_s
      sni; /* SNI (server name extension) stores ID certificates */
  fio___tls_trust_ary_s
      trust; /* Trusted certificate registry (peer verification) */

  /************ TODO: implementation data fields go here ******************/
};

/* *****************************************************************************
ALPN Helpers
***************************************************************************** */

/** Returns a pointer to the ALPN data (callback, etc') IF exists in the TLS. */
FIO_IFUNC alpn_s *fio___tls_alpn_find(fio_tls_s *tls, char *name, size_t len) {
  alpn_s tmp = {.name = FIO_STRING_INIT_STATIC2(name, len)};
  alpn_s *pos =
      fio___tls_alpn_list_get_ptr(&tls->alpn, fio_str_hash(&tmp.name, 0), tmp);
  return pos;
}

/** Adds an ALPN data object to the ALPN "list" (set) */
FIO_IFUNC void fio___tls_alpn_add(
    fio_tls_s *tls, const char *protocol_name,
    void (*on_selected)(intptr_t uuid, void *udata_connection, void *udata_tls),
    void *udata_tls, void (*on_cleanup)(void *udata_tls)) {
  alpn_s tmp = {
      .name = FIO_STRING_INIT_STATIC(protocol_name),
      .on_selected = on_selected,
      .udata_tls = udata_tls,
      .on_cleanup = on_cleanup,
  };
  if (fio_str_len(&tmp.name) > 255) {
    FIO_LOG_ERROR("ALPN protocol names are limited to 255 bytes.");
    return;
  }
  fio___tls_alpn_list_set(&tls->alpn, fio_str_hash(&tmp.name, 0), tmp, NULL);
  tmp.on_cleanup = NULL;
  fio_alpn_destroy(&tmp);
}

/** Returns a pointer to the default (first) ALPN object in the TLS (if any). */
FIO_IFUNC alpn_s *fio___tls_alpn_default(fio_tls_s *tls) {
  if (!tls || !fio___tls_alpn_list_count(&tls->alpn))
    return NULL;
  FIO_MAP_EACH(&tls->alpn, pos) { return &pos->obj; }
  return NULL;
}

typedef struct {
  alpn_s alpn;
  intptr_t uuid;
  void *udata_connection;
} alpn_task_s;

FIO_IFUNC void fio___tls_alpn_select___task(void *t_, void *ignr_) {
  alpn_task_s *t = t_;
  if (fio_is_valid(t->uuid))
    t->alpn.on_selected(t->uuid, t->udata_connection, t->alpn.udata_tls);
  fio_free(t);
  (void)ignr_;
}

/** Schedules the ALPN protocol callback. */
FIO_IFUNC void fio___tls_alpn_select(alpn_s *alpn, intptr_t uuid,
                                     void *udata_connection) {
  if (!alpn || !alpn->on_selected)
    return;
  alpn_task_s *t = fio_malloc(sizeof(*t));
  *t = (alpn_task_s){
      .alpn = *alpn,
      .uuid = uuid,
      .udata_connection = udata_connection,
  };
  /* move task out of the socket's lock */
  fio_defer(fio___tls_alpn_select___task, t, NULL);
}

/* *****************************************************************************
SSL/TLS Context (re)-building - TODO: add implementation details
***************************************************************************** */

/** Called when the library specific data for the context should be destroyed */
static void fio___tls_destroy_context(fio_tls_s *tls) {
  /* TODO: Library specific implementation */
  FIO_LOG_DEBUG("destroyed TLS context %p", (void *)tls);
}

/** Called when the library specific data for the context should be built */
static void fio___tls_build_context(fio_tls_s *tls) {
  fio___tls_destroy_context(tls);
  /* TODO: Library specific implementation */

  /* Certificates */
  FIO_ARRAY_EACH(&tls->sni, pos) {
    fio_str_info_s k = fio_str_info(&pos->private_key);
    fio_str_info_s p = fio_str_info(&pos->public_key);
    fio_str_info_s pw = fio_str_info(&pos->password);
    if (p.len && k.len) {
      /* TODO: attache certificate */
      (void)pw;
    } else {
      /* TODO: self signed certificate */
    }
  }

  /* ALPN Protocols */
  FIO_MAP_EACH(&tls->alpn, pos) {
    fio_str_info_s name = fio_str_info(&pos->obj.name);
    (void)name;
    // map to pos->callback;
  }

  /* Peer Verification / Trust */
  if (fio___tls_trust_ary_count(&tls->trust)) {
    /* TODO: enable peer verification */

    /* TODO: Add each ceriticate in the PEM to the trust "store" */
    FIO_ARRAY_EACH(&tls->trust, pos) {
      fio_str_info_s pem = fio_str_info(&pos->pem);
      (void)pem;
    }
  }

  FIO_LOG_DEBUG("(re)built TLS context %p", (void *)tls);
}

/* *****************************************************************************
SSL/TLS RW Hooks - TODO: add implementation details
***************************************************************************** */

/* TODO: this is an example implementation - fix for specific library. */

#define TLS_BUFFER_LENGTH (1 << 15)
typedef struct {
  fio_tls_s *tls;
  size_t len;
  uint8_t alpn_ok;
  char buf[TLS_BUFFER_LENGTH];
} tls_buffer_s;

/**
 * Implement reading from a file descriptor. Should behave like the file
 * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_read(intptr_t uuid, void *udata, void *buf,
                            size_t count) {
  ssize_t ret = read(fio_uuid2fd(uuid), buf, count);
  if (ret > 0) {
    FIO_LOG_DEBUG("Read %zd bytes from %p", ret, (void *)uuid);
  }
  return ret;
  (void)udata;
}

/**
 * When implemented, this function will be called to flush any data remaining
 * in the internal buffer.
 *
 * The function should return the number of bytes remaining in the internal
 * buffer (0 is a valid response) or -1 (on error).
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_flush(intptr_t uuid, void *udata) {
  tls_buffer_s *tlsbuf = udata;
  if (!tlsbuf->len) {
    FIO_LOG_DEBUG("Flush empty for %p", (void *)uuid);
    return 0;
  }
  ssize_t r = write(fio_uuid2fd(uuid), tlsbuf->buf, tlsbuf->len);
  if (r < 0)
    return -1;
  if (r == 0) {
    errno = ECONNRESET;
    return -1;
  }
  size_t len = tlsbuf->len - r;
  if (len)
    memmove(tlsbuf->buf, tlsbuf->buf + r, len);
  tlsbuf->len = len;
  FIO_LOG_DEBUG("Sent %zd bytes to %p", r, (void *)uuid);
  return r;
}

/**
 * Implement writing to a file descriptor. Should behave like the file system
 * `write` call.
 *
 * If an internal buffer is implemented and it is full, errno should be set to
 * EWOULDBLOCK and the function should return -1.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_write(intptr_t uuid, void *udata, const void *buf,
                             size_t count) {
  tls_buffer_s *tlsbuf = udata;
  size_t can_copy = TLS_BUFFER_LENGTH - tlsbuf->len;
  if (can_copy > count)
    can_copy = count;
  if (!can_copy)
    goto would_block;
  memcpy(tlsbuf->buf + tlsbuf->len, buf, can_copy);
  tlsbuf->len += can_copy;
  FIO_LOG_DEBUG("Copied %zu bytes to %p", can_copy, (void *)uuid);
  fio_tls_flush(uuid, udata);
  return can_copy;
would_block:
  errno = EWOULDBLOCK;
  return -1;
}

/**
 * The `close` callback should close the underlying socket / file descriptor.
 *
 * If the function returns a non-zero value, it will be called again after an
 * attempt to flush the socket and any pending outgoing buffer.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 * */
static ssize_t fio_tls_before_close(intptr_t uuid, void *udata) {
  FIO_LOG_DEBUG("The `before_close` callback was called for %p", (void *)uuid);
  return 1;
  (void)udata;
}
/**
 * Called to perform cleanup after the socket was closed.
 * */
static void fio_tls_cleanup(void *udata) {
  tls_buffer_s *tlsbuf = udata;
  /* make sure the ALPN callback was called, just in case cleanup is required */
  if (!tlsbuf->alpn_ok) {
    fio___tls_alpn_select(fio___tls_alpn_default(tlsbuf->tls), -1,
                          NULL /* ALPN udata */);
  }
  fio_tls_destroy(tlsbuf->tls); /* manage reference count */
  fio_free(udata);
}

static fio_rw_hook_s FIO_TLS_HOOKS = {
    .read = fio_tls_read,
    .write = fio_tls_write,
    .before_close = fio_tls_before_close,
    .flush = fio_tls_flush,
    .cleanup = fio_tls_cleanup,
};

static size_t fio_tls_handshake(intptr_t uuid, void *udata) {
  /*TODO: test for handshake completion */
  if (0 /*handshake didn't complete */)
    return 0;
  if (fio_rw_hook_replace_unsafe(uuid, &FIO_TLS_HOOKS, udata) == 0) {
    FIO_LOG_DEBUG("Completed TLS handshake for %p", (void *)uuid);
    /*
     * make sure the connection is re-added to the reactor...
     * in case, while waiting for ALPN, it was suspended for missing a protocol.
     */
    fio_force_event(uuid, FIO_EVENT_ON_DATA);
  } else {
    FIO_LOG_DEBUG("Something went wrong during TLS handshake for %p",
                  (void *)uuid);
  }
  return 1;
}

static ssize_t fio_tls_read4handshake(intptr_t uuid, void *udata, void *buf,
                                      size_t count) {
  FIO_LOG_DEBUG("TLS handshake from read %p", (void *)uuid);
  if (fio_tls_handshake(uuid, udata))
    return fio_tls_read(uuid, udata, buf, count);
  errno = EWOULDBLOCK;
  return -1;
}

static ssize_t fio_tls_write4handshake(intptr_t uuid, void *udata,
                                       const void *buf, size_t count) {
  FIO_LOG_DEBUG("TLS handshake from write %p", (void *)uuid);
  if (fio_tls_handshake(uuid, udata))
    return fio_tls_write(uuid, udata, buf, count);
  errno = EWOULDBLOCK;
  return -1;
}

static ssize_t fio_tls_flush4handshake(intptr_t uuid, void *udata) {
  FIO_LOG_DEBUG("TLS handshake from flush %p", (void *)uuid);
  if (fio_tls_handshake(uuid, udata))
    return fio_tls_flush(uuid, udata);
  /* TODO: return a positive value only if handshake requires a write */
  return 1;
}
static fio_rw_hook_s FIO_TLS_HANDSHAKE_HOOKS = {
    .read = fio_tls_read4handshake,
    .write = fio_tls_write4handshake,
    .before_close = fio_tls_before_close,
    .flush = fio_tls_flush4handshake,
    .cleanup = fio_tls_cleanup,
};

static inline void fio_tls_attach2uuid(intptr_t uuid, fio_tls_s *tls,
                                       void *udata, uint8_t is_server) {
  fio_atomic_add(&tls->ref, 1); /* manage reference count */
  /* TODO: this is only an example implementation - fix for specific library */
  if (is_server) {
    /* Server mode (accept) */
    FIO_LOG_DEBUG("Attaching TLS read/write hook for %p (server mode).",
                  (void *)uuid);
  } else {
    /* Client mode (connect) */
    FIO_LOG_DEBUG("Attaching TLS read/write hook for %p (client mode).",
                  (void *)uuid);
  }
  /* common implementation (TODO) */
  tls_buffer_s *connection_data = fio_malloc(sizeof(*connection_data));
  FIO_ASSERT_ALLOC(connection_data);
  fio_rw_hook_set(uuid, &FIO_TLS_HANDSHAKE_HOOKS,
                  connection_data); /* 32Kb buffer */
  fio___tls_alpn_select(fio___tls_alpn_default(tls), uuid, udata);
  connection_data->alpn_ok = 1;
}

/* *****************************************************************************
SSL/TLS API implementation - this can be pretty much used as is...
***************************************************************************** */

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 */
fio_tls_s *FIO_TLS_WEAK fio_tls_new(const char *server_name, const char *cert,
                                    const char *key, const char *pk_password) {
  REQUIRE_TLS_LIBRARY();
  fio_tls_s *tls = calloc(sizeof(*tls), 1);
  tls->ref = 1;
  fio_tls_cert_add(tls, server_name, key, cert, pk_password);
  return tls;
}

/**
 * Adds a certificate  a new SSL/TLS context / settings object.
 */
void FIO_TLS_WEAK fio_tls_cert_add(fio_tls_s *tls, const char *server_name,
                                   const char *cert, const char *key,
                                   const char *pk_password) {
  REQUIRE_TLS_LIBRARY();
  cert_s c = {
      .private_key = FIO_STRING_INIT,
      .public_key = FIO_STRING_INIT,
      .password = FIO_STRING_INIT_STATIC2(
          pk_password, (pk_password ? strlen(pk_password) : 0)),
  };
  if (key && cert) {
    if (fio_str_readfile(&c.private_key, key, 0, 0).buf == NULL)
      goto file_missing;
    if (fio_str_readfile(&c.public_key, cert, 0, 0).buf == NULL)
      goto file_missing;
    fio___tls_cert_ary_push(&tls->sni, c);
  } else if (server_name) {
    /* Self-Signed TLS Certificates */
    c.private_key = (fio_str_s)FIO_STRING_INIT_STATIC(server_name);
    fio___tls_cert_ary_push(&tls->sni, c);
  }
  fio___tls_cert_destroy(&c);
  fio___tls_build_context(tls);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for either %s or %s or both.",
                key, cert);
  exit(-1);
}

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 *
 * The callback should accept the `uuid`, the user data pointer passed to either
 * `fio_tls_accept` or `fio_tls_connect` (here: `udata_connetcion`) and the user
 * data pointer passed to the `fio_tls_alpn_add` function (`udata_tls`).
 *
 * The `on_cleanup` callback will be called when the TLS object is destroyed (or
 * `fio_tls_alpn_add` is called again with the same protocol name). The
 * `udata_tls` argumrnt will be passed along, as is, to the callback (if set).
 *
 * Except for the `tls` and `protocol_name` arguments, all arguments can be
 * NULL.
 */
void FIO_TLS_WEAK fio_tls_alpn_add(
    fio_tls_s *tls, const char *protocol_name,
    void (*on_selected)(intptr_t uuid, void *udata_connection, void *udata_tls),
    void *udata_tls, void (*on_cleanup)(void *udata_tls)) {
  REQUIRE_TLS_LIBRARY();
  fio___tls_alpn_add(tls, protocol_name, on_selected, udata_tls, on_cleanup);
  fio___tls_build_context(tls);
}

/**
 * Returns the number of registered ALPN protocol names.
 *
 * This could be used when deciding if protocol selection should be delegated to
 * the ALPN mechanism, or whether a protocol should be immediately assigned.
 *
 * If no ALPN protocols are registered, zero (0) is returned.
 */
uintptr_t FIO_TLS_WEAK fio_tls_alpn_count(fio_tls_s *tls) {
  return tls ? fio___tls_alpn_list_count(&tls->alpn) : 0;
}

/**
 * Adds a certificate to the "trust" list, which automatically adds a peer
 * verification requirement.
 *
 *      fio_tls_trust(tls, "google-ca.pem" );
 */
void FIO_TLS_WEAK fio_tls_trust(fio_tls_s *tls, const char *public_cert_file) {
  REQUIRE_TLS_LIBRARY();
  trust_s c = {
      .pem = FIO_STRING_INIT,
  };
  if (!public_cert_file)
    return;
  if (fio_str_readfile(&c.pem, public_cert_file, 0, 0).buf == NULL)
    goto file_missing;
  fio___tls_trust_ary_push(&tls->trust, c);
  fio___tls_trust_destroy(&c);
  fio___tls_build_context(tls);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for %s ", public_cert_file);
  exit(-1);
}

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * the result of `fio_accept`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
void FIO_TLS_WEAK fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata) {
  REQUIRE_TLS_LIBRARY();
  fio_tls_attach2uuid(uuid, tls, udata, 1);
}

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * one received by a `fio_connect` specified callback `on_connect`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
void FIO_TLS_WEAK fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata) {
  REQUIRE_TLS_LIBRARY();
  fio_tls_attach2uuid(uuid, tls, udata, 0);
}

/**
 * Increase the reference count for the TLS object.
 *
 * Decrease with `fio_tls_destroy`.
 */
void FIO_TLS_WEAK fio_tls_dup(fio_tls_s *tls) { fio_atomic_add(&tls->ref, 1); }

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
void FIO_TLS_WEAK fio_tls_destroy(fio_tls_s *tls) {
  if (!tls)
    return;
  REQUIRE_TLS_LIBRARY();
  if (fio_atomic_sub(&tls->ref, 1))
    return;
  fio___tls_destroy_context(tls);
  fio___tls_alpn_list_destroy(&tls->alpn);
  fio___tls_cert_ary_destroy(&tls->sni);
  fio___tls_trust_ary_destroy(&tls->trust);
  free(tls);
}

#endif /* Library compiler flags */

/* *****************************************************************************
Section Start Marker












                             Hash Functions and Base64

                  SipHash / SHA-1 / SHA-2 / Base64 / Hex encoding













***************************************************************************** */

/* *****************************************************************************
SipHash
***************************************************************************** */

#if __BIG_ENDIAN__ /* SipHash is Little Endian */
#define sip_local64(i) fio_bswap64((i))
#else
#define sip_local64(i) (i)
#endif

static inline uint64_t fio_siphash_xy(const void *data, size_t len, size_t x,
                                      size_t y, uint64_t key1, uint64_t key2) {
  /* initialize the 4 words */
  uint64_t v0 = (0x0706050403020100ULL ^ 0x736f6d6570736575ULL) ^ key1;
  uint64_t v1 = (0x0f0e0d0c0b0a0908ULL ^ 0x646f72616e646f6dULL) ^ key2;
  uint64_t v2 = (0x0706050403020100ULL ^ 0x6c7967656e657261ULL) ^ key1;
  uint64_t v3 = (0x0f0e0d0c0b0a0908ULL ^ 0x7465646279746573ULL) ^ key2;
  const uint8_t *w8 = data;
  uint8_t len_mod = len & 255;
  union {
    uint64_t i;
    uint8_t str[8];
  } word;

#define hash_map_SipRound                                                      \
  do {                                                                         \
    v2 += v3;                                                                  \
    v3 = fio_lrot64(v3, 16) ^ v2;                                              \
    v0 += v1;                                                                  \
    v1 = fio_lrot64(v1, 13) ^ v0;                                              \
    v0 = fio_lrot64(v0, 32);                                                   \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = fio_lrot64(v1, 17) ^ v2;                                              \
    v3 = fio_lrot64(v3, 21) ^ v0;                                              \
    v2 = fio_lrot64(v2, 32);                                                   \
  } while (0);

  while (len >= 8) {
    word.i = sip_local64(fio_buf2u64(w8));
    v3 ^= word.i;
    /* Sip Rounds */
    for (size_t i = 0; i < x; ++i) {
      hash_map_SipRound;
    }
    v0 ^= word.i;
    w8 += 8;
    len -= 8;
  }
  word.i = 0;
  uint8_t *pos = word.str;
  switch (len) { /* fallthrough is intentional */
  case 7:
    pos[6] = w8[6];
    /* fallthrough */
  case 6:
    pos[5] = w8[5];
    /* fallthrough */
  case 5:
    pos[4] = w8[4];
    /* fallthrough */
  case 4:
    pos[3] = w8[3];
    /* fallthrough */
  case 3:
    pos[2] = w8[2];
    /* fallthrough */
  case 2:
    pos[1] = w8[1];
    /* fallthrough */
  case 1:
    pos[0] = w8[0];
  }
  word.str[7] = len_mod;

  /* last round */
  v3 ^= word.i;
  hash_map_SipRound;
  hash_map_SipRound;
  v0 ^= word.i;
  /* Finalization */
  v2 ^= 0xff;
  /* d iterations of SipRound */
  for (size_t i = 0; i < y; ++i) {
    hash_map_SipRound;
  }
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  /* XOR it all together */
  v0 ^= v1 ^ v2 ^ v3;
#undef hash_map_SipRound
  return v0;
}

uint64_t fio_siphash24(const void *data, size_t len, uint64_t key1,
                       uint64_t key2) {
  return fio_siphash_xy(data, len, 2, 4, key1, key2);
}

uint64_t fio_siphash13(const void *data, size_t len, uint64_t key1,
                       uint64_t key2) {
  return fio_siphash_xy(data, len, 1, 3, key1, key2);
}

/* *****************************************************************************
SHA-1
***************************************************************************** */

static const uint8_t sha1_padding[64] = {0x80, 0};

/**
Process the buffer once full.
*/
static inline void fio_sha1_perform_all_rounds(fio_sha1_s *s,
                                               const uint8_t *buf) {
  /* collect data */
  uint32_t a = s->digest.i[0];
  uint32_t b = s->digest.i[1];
  uint32_t c = s->digest.i[2];
  uint32_t d = s->digest.i[3];
  uint32_t e = s->digest.i[4];
  uint32_t t, w[16];
  /* copy data to words, performing byte swapping as needed */
  w[0] = fio_buf2u32(buf);
  w[1] = fio_buf2u32(buf + 4);
  w[2] = fio_buf2u32(buf + 8);
  w[3] = fio_buf2u32(buf + 12);
  w[4] = fio_buf2u32(buf + 16);
  w[5] = fio_buf2u32(buf + 20);
  w[6] = fio_buf2u32(buf + 24);
  w[7] = fio_buf2u32(buf + 28);
  w[8] = fio_buf2u32(buf + 32);
  w[9] = fio_buf2u32(buf + 36);
  w[10] = fio_buf2u32(buf + 40);
  w[11] = fio_buf2u32(buf + 44);
  w[12] = fio_buf2u32(buf + 48);
  w[13] = fio_buf2u32(buf + 52);
  w[14] = fio_buf2u32(buf + 56);
  w[15] = fio_buf2u32(buf + 60);
  /* perform rounds */
#undef perform_single_round
#define perform_single_round(num)                                              \
  t = fio_lrot32(a, 5) + e + w[num] + ((b & c) | ((~b) & d)) + 0x5A827999;     \
  e = d;                                                                       \
  d = c;                                                                       \
  c = fio_lrot32(b, 30);                                                       \
  b = a;                                                                       \
  a = t;

#define perform_four_rounds(i)                                                 \
  perform_single_round(i);                                                     \
  perform_single_round(i + 1);                                                 \
  perform_single_round(i + 2);                                                 \
  perform_single_round(i + 3);

  perform_four_rounds(0);
  perform_four_rounds(4);
  perform_four_rounds(8);
  perform_four_rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = fio_lrot32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^                  \
                          w[(i - 14) & 15] ^ w[(i - 16) & 15]),                \
                         1);                                                   \
  t = fio_lrot32(a, 5) + e + w[(i)&15] + ((b & c) | ((~b) & d)) + 0x5A827999;  \
  e = d;                                                                       \
  d = c;                                                                       \
  c = fio_lrot32(b, 30);                                                       \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(16);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = fio_lrot32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^                  \
                          w[(i - 14) & 15] ^ w[(i - 16) & 15]),                \
                         1);                                                   \
  t = fio_lrot32(a, 5) + e + w[(i)&15] + (b ^ c ^ d) + 0x6ED9EBA1;             \
  e = d;                                                                       \
  d = c;                                                                       \
  c = fio_lrot32(b, 30);                                                       \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(20);
  perform_four_rounds(24);
  perform_four_rounds(28);
  perform_four_rounds(32);
  perform_four_rounds(36);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = fio_lrot32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^                  \
                          w[(i - 14) & 15] ^ w[(i - 16) & 15]),                \
                         1);                                                   \
  t = fio_lrot32(a, 5) + e + w[(i)&15] + ((b & (c | d)) | (c & d)) +           \
      0x8F1BBCDC;                                                              \
  e = d;                                                                       \
  d = c;                                                                       \
  c = fio_lrot32(b, 30);                                                       \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(40);
  perform_four_rounds(44);
  perform_four_rounds(48);
  perform_four_rounds(52);
  perform_four_rounds(56);
#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = fio_lrot32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^                  \
                          w[(i - 14) & 15] ^ w[(i - 16) & 15]),                \
                         1);                                                   \
  t = fio_lrot32(a, 5) + e + w[(i)&15] + (b ^ c ^ d) + 0xCA62C1D6;             \
  e = d;                                                                       \
  d = c;                                                                       \
  c = fio_lrot32(b, 30);                                                       \
  b = a;                                                                       \
  a = t;
  perform_four_rounds(60);
  perform_four_rounds(64);
  perform_four_rounds(68);
  perform_four_rounds(72);
  perform_four_rounds(76);

  /* store data */
  s->digest.i[4] += e;
  s->digest.i[3] += d;
  s->digest.i[2] += c;
  s->digest.i[1] += b;
  s->digest.i[0] += a;
}

/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
fio_sha1_s fio_sha1_init(void) {
  return (fio_sha1_s){.digest.i[0] = 0x67452301,
                      .digest.i[1] = 0xefcdab89,
                      .digest.i[2] = 0x98badcfe,
                      .digest.i[3] = 0x10325476,
                      .digest.i[4] = 0xc3d2e1f0};
}

/**
Writes data to the sha1 buffer.
*/
void fio_sha1_write(fio_sha1_s *s, const void *data, size_t len) {
  size_t in_buf = s->len & 63;
  size_t partial = 64 - in_buf;
  s->len += len;
  if (partial > len) {
    memcpy(s->buf + in_buf, data, len);
    return;
  }
  if (in_buf) {
    memcpy(s->buf + in_buf, data, partial);
    len -= partial;
    data = (void *)((uintptr_t)data + partial);
    fio_sha1_perform_all_rounds(s, s->buf);
  }
  while (len >= 64) {
    fio_sha1_perform_all_rounds(s, data);
    data = (void *)((uintptr_t)data + 64);
    len -= 64;
  }
  if (len) {
    memcpy(s->buf + in_buf, data, len);
  }
  return;
}

char *fio_sha1_result(fio_sha1_s *s) {
  size_t in_buf = s->len & 63;
  if (in_buf > 55) {
    memcpy(s->buf + in_buf, sha1_padding, 64 - in_buf);
    fio_sha1_perform_all_rounds(s, s->buf);
    memcpy(s->buf, sha1_padding + 1, 56);
  } else if (in_buf != 55) {
    memcpy(s->buf + in_buf, sha1_padding, 56 - in_buf);
  } else {
    s->buf[55] = sha1_padding[0];
  }
  /* store the length in BITS - alignment should be promised by struct */
  /* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */
  uint64_t *len = (uint64_t *)(s->buf + 56);
  *len = s->len << 3;
  *len = fio_lton64(*len);
  fio_sha1_perform_all_rounds(s, s->buf);

  /* change back to little endian */
  s->digest.i[0] = fio_ntol32(s->digest.i[0]);
  s->digest.i[1] = fio_ntol32(s->digest.i[1]);
  s->digest.i[2] = fio_ntol32(s->digest.i[2]);
  s->digest.i[3] = fio_ntol32(s->digest.i[3]);
  s->digest.i[4] = fio_ntol32(s->digest.i[4]);

  return (char *)s->digest.str;
}

#undef perform_single_round

/* *****************************************************************************
SHA-2
***************************************************************************** */

static const uint8_t sha2_padding[128] = {0x80, 0};

/* SHA-224 and SHA-256 constants */
static uint32_t sha2_256_words[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/* SHA-512 and friends constants */
static uint64_t sha2_512_words[] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f,
    0xe9b5dba58189dbbc, 0x3956c25bf348b538, 0x59f111f1b605d019,
    0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242,
    0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
    0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3,
    0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 0x2de92c6f592b0275,
    0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f,
    0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
    0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc,
    0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6,
    0x92722c851482353b, 0xa2bfe8a14cf10364, 0xa81a664bbc423001,
    0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
    0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99,
    0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb,
    0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc,
    0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915,
    0xc67178f2e372532b, 0xca273eceea26619c, 0xd186b8c721c0c207,
    0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba,
    0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
    0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a,
    0x5fcb6fab3ad6faec, 0x6c44198c4a475817};

/* Specific Macros for the SHA-2 processing */

#define Ch(x, y, z) (((x) & (y)) ^ ((~(x)) & z))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define Eps0_32(x)                                                             \
  (fio_rrot32((x), 2) ^ fio_rrot32((x), 13) ^ fio_rrot32((x), 22))
#define Eps1_32(x)                                                             \
  (fio_rrot32((x), 6) ^ fio_rrot32((x), 11) ^ fio_rrot32((x), 25))
#define Omg0_32(x) (fio_rrot32((x), 7) ^ fio_rrot32((x), 18) ^ (((x) >> 3)))
#define Omg1_32(x) (fio_rrot32((x), 17) ^ fio_rrot32((x), 19) ^ (((x) >> 10)))

#define Eps0_64(x)                                                             \
  (fio_rrot64((x), 28) ^ fio_rrot64((x), 34) ^ fio_rrot64((x), 39))
#define Eps1_64(x)                                                             \
  (fio_rrot64((x), 14) ^ fio_rrot64((x), 18) ^ fio_rrot64((x), 41))
#define Omg0_64(x) (fio_rrot64((x), 1) ^ fio_rrot64((x), 8) ^ (((x) >> 7)))
#define Omg1_64(x) (fio_rrot64((x), 19) ^ fio_rrot64((x), 61) ^ (((x) >> 6)))

/**
Process the buffer once full.
*/
static inline void fio_sha2_perform_all_rounds(fio_sha2_s *s,
                                               const uint8_t *data) {
  if (s->type & 1) { /* 512 derived type */
    // process values for the 64bit words
    uint64_t a = s->digest.i64[0];
    uint64_t b = s->digest.i64[1];
    uint64_t c = s->digest.i64[2];
    uint64_t d = s->digest.i64[3];
    uint64_t e = s->digest.i64[4];
    uint64_t f = s->digest.i64[5];
    uint64_t g = s->digest.i64[6];
    uint64_t h = s->digest.i64[7];
    uint64_t t1, t2, w[80];
    w[0] = fio_buf2u64(data);
    w[1] = fio_buf2u64(data + 8);
    w[2] = fio_buf2u64(data + 16);
    w[3] = fio_buf2u64(data + 24);
    w[4] = fio_buf2u64(data + 32);
    w[5] = fio_buf2u64(data + 40);
    w[6] = fio_buf2u64(data + 48);
    w[7] = fio_buf2u64(data + 56);
    w[8] = fio_buf2u64(data + 64);
    w[9] = fio_buf2u64(data + 72);
    w[10] = fio_buf2u64(data + 80);
    w[11] = fio_buf2u64(data + 88);
    w[12] = fio_buf2u64(data + 96);
    w[13] = fio_buf2u64(data + 104);
    w[14] = fio_buf2u64(data + 112);
    w[15] = fio_buf2u64(data + 120);

#undef perform_single_round
#define perform_single_round(i)                                                \
  t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];                \
  t2 = Eps0_64(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

#define perform_4rounds(i)                                                     \
  perform_single_round(i);                                                     \
  perform_single_round(i + 1);                                                 \
  perform_single_round(i + 2);                                                 \
  perform_single_round(i + 3);

    perform_4rounds(0);
    perform_4rounds(4);
    perform_4rounds(8);
    perform_4rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[i] = Omg1_64(w[i - 2]) + w[i - 7] + Omg0_64(w[i - 15]) + w[i - 16];        \
  t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];                \
  t2 = Eps0_64(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(16);
    perform_4rounds(20);
    perform_4rounds(24);
    perform_4rounds(28);
    perform_4rounds(32);
    perform_4rounds(36);
    perform_4rounds(40);
    perform_4rounds(44);
    perform_4rounds(48);
    perform_4rounds(52);
    perform_4rounds(56);
    perform_4rounds(60);
    perform_4rounds(64);
    perform_4rounds(68);
    perform_4rounds(72);
    perform_4rounds(76);

    s->digest.i64[0] += a;
    s->digest.i64[1] += b;
    s->digest.i64[2] += c;
    s->digest.i64[3] += d;
    s->digest.i64[4] += e;
    s->digest.i64[5] += f;
    s->digest.i64[6] += g;
    s->digest.i64[7] += h;
    return;
  } else {
    // process values for the 32bit words
    uint32_t a = s->digest.i32[0];
    uint32_t b = s->digest.i32[1];
    uint32_t c = s->digest.i32[2];
    uint32_t d = s->digest.i32[3];
    uint32_t e = s->digest.i32[4];
    uint32_t f = s->digest.i32[5];
    uint32_t g = s->digest.i32[6];
    uint32_t h = s->digest.i32[7];
    uint32_t t1, t2, w[64];

    w[0] = fio_buf2u32(data);
    w[1] = fio_buf2u32(data + 4);
    w[2] = fio_buf2u32(data + 8);
    w[3] = fio_buf2u32(data + 12);
    w[4] = fio_buf2u32(data + 16);
    w[5] = fio_buf2u32(data + 20);
    w[6] = fio_buf2u32(data + 24);
    w[7] = fio_buf2u32(data + 28);
    w[8] = fio_buf2u32(data + 32);
    w[9] = fio_buf2u32(data + 36);
    w[10] = fio_buf2u32(data + 40);
    w[11] = fio_buf2u32(data + 44);
    w[12] = fio_buf2u32(data + 48);
    w[13] = fio_buf2u32(data + 52);
    w[14] = fio_buf2u32(data + 56);
    w[15] = fio_buf2u32(data + 60);

#undef perform_single_round
#define perform_single_round(i)                                                \
  t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];                \
  t2 = Eps0_32(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(0);
    perform_4rounds(4);
    perform_4rounds(8);
    perform_4rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[i] = Omg1_32(w[i - 2]) + w[i - 7] + Omg0_32(w[i - 15]) + w[i - 16];        \
  t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];                \
  t2 = Eps0_32(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(16);
    perform_4rounds(20);
    perform_4rounds(24);
    perform_4rounds(28);
    perform_4rounds(32);
    perform_4rounds(36);
    perform_4rounds(40);
    perform_4rounds(44);
    perform_4rounds(48);
    perform_4rounds(52);
    perform_4rounds(56);
    perform_4rounds(60);

    s->digest.i32[0] += a;
    s->digest.i32[1] += b;
    s->digest.i32[2] += c;
    s->digest.i32[3] += d;
    s->digest.i32[4] += e;
    s->digest.i32[5] += f;
    s->digest.i32[6] += g;
    s->digest.i32[7] += h;
  }
}

/**
Initialize/reset the SHA-2 object.

SHA-2 is actually a family of functions with different variants. When
initializing the SHA-2 container, you must select the variant you intend to
apply. The following are valid options (see the fio_sha2_variant_e enum):

- SHA_512 (== 0)
- SHA_384
- SHA_512_224
- SHA_512_256
- SHA_256
- SHA_224

*/
fio_sha2_s fio_sha2_init(fio_sha2_variant_e variant) {
  if (variant == SHA_256) {
    return (fio_sha2_s){
        .type = SHA_256,
        .digest.i32[0] = 0x6a09e667,
        .digest.i32[1] = 0xbb67ae85,
        .digest.i32[2] = 0x3c6ef372,
        .digest.i32[3] = 0xa54ff53a,
        .digest.i32[4] = 0x510e527f,
        .digest.i32[5] = 0x9b05688c,
        .digest.i32[6] = 0x1f83d9ab,
        .digest.i32[7] = 0x5be0cd19,
    };
  } else if (variant == SHA_384) {
    return (fio_sha2_s){
        .type = SHA_384,
        .digest.i64[0] = 0xcbbb9d5dc1059ed8,
        .digest.i64[1] = 0x629a292a367cd507,
        .digest.i64[2] = 0x9159015a3070dd17,
        .digest.i64[3] = 0x152fecd8f70e5939,
        .digest.i64[4] = 0x67332667ffc00b31,
        .digest.i64[5] = 0x8eb44a8768581511,
        .digest.i64[6] = 0xdb0c2e0d64f98fa7,
        .digest.i64[7] = 0x47b5481dbefa4fa4,
    };
  } else if (variant == SHA_512) {
    return (fio_sha2_s){
        .type = SHA_512,
        .digest.i64[0] = 0x6a09e667f3bcc908,
        .digest.i64[1] = 0xbb67ae8584caa73b,
        .digest.i64[2] = 0x3c6ef372fe94f82b,
        .digest.i64[3] = 0xa54ff53a5f1d36f1,
        .digest.i64[4] = 0x510e527fade682d1,
        .digest.i64[5] = 0x9b05688c2b3e6c1f,
        .digest.i64[6] = 0x1f83d9abfb41bd6b,
        .digest.i64[7] = 0x5be0cd19137e2179,
    };
  } else if (variant == SHA_224) {
    return (fio_sha2_s){
        .type = SHA_224,
        .digest.i32[0] = 0xc1059ed8,
        .digest.i32[1] = 0x367cd507,
        .digest.i32[2] = 0x3070dd17,
        .digest.i32[3] = 0xf70e5939,
        .digest.i32[4] = 0xffc00b31,
        .digest.i32[5] = 0x68581511,
        .digest.i32[6] = 0x64f98fa7,
        .digest.i32[7] = 0xbefa4fa4,
    };
  } else if (variant == SHA_512_224) {
    return (fio_sha2_s){
        .type = SHA_512_224,
        .digest.i64[0] = 0x8c3d37c819544da2,
        .digest.i64[1] = 0x73e1996689dcd4d6,
        .digest.i64[2] = 0x1dfab7ae32ff9c82,
        .digest.i64[3] = 0x679dd514582f9fcf,
        .digest.i64[4] = 0x0f6d2b697bd44da8,
        .digest.i64[5] = 0x77e36f7304c48942,
        .digest.i64[6] = 0x3f9d85a86a1d36c8,
        .digest.i64[7] = 0x1112e6ad91d692a1,
    };
  } else if (variant == SHA_512_256) {
    return (fio_sha2_s){
        .type = SHA_512_256,
        .digest.i64[0] = 0x22312194fc2bf72c,
        .digest.i64[1] = 0x9f555fa3c84c64c2,
        .digest.i64[2] = 0x2393b86b6f53b151,
        .digest.i64[3] = 0x963877195940eabd,
        .digest.i64[4] = 0x96283ee2a88effe3,
        .digest.i64[5] = 0xbe5e1e2553863992,
        .digest.i64[6] = 0x2b0199fc2c85b8aa,
        .digest.i64[7] = 0x0eb72ddc81c52ca2,
    };
  }
  FIO_LOG_FATAL("SHA-2 ERROR - variant unknown");
  exit(2);
}

/**
Writes data to the SHA-2 buffer.
*/
void fio_sha2_write(fio_sha2_s *s, const void *data, size_t len) {
  size_t in_buf;
  size_t partial;
  if (s->type & 1) { /* 512 type derived */
    in_buf = s->len.words[0] & 127;
    if (s->len.words[0] + len < s->len.words[0]) {
      /* we are at wraping around the 64bit limit */
      s->len.words[1] = (s->len.words[1] << 1) | 1;
    }
    s->len.words[0] += len;
    partial = 128 - in_buf;

    if (partial > len) {
      memcpy(s->buf + in_buf, data, len);
      return;
    }
    if (in_buf) {
      memcpy(s->buf + in_buf, data, partial);
      len -= partial;
      data = (void *)((uintptr_t)data + partial);
      fio_sha2_perform_all_rounds(s, s->buf);
    }
    while (len >= 128) {
      fio_sha2_perform_all_rounds(s, data);
      data = (void *)((uintptr_t)data + 128);
      len -= 128;
    }
    if (len) {
      memcpy(s->buf + in_buf, data, len);
    }
    return;
  }
  /* else... NOT 512 bits derived (64bit base) */

  in_buf = s->len.words[0] & 63;
  partial = 64 - in_buf;

  s->len.words[0] += len;

  if (partial > len) {
    memcpy(s->buf + in_buf, data, len);
    return;
  }
  if (in_buf) {
    memcpy(s->buf + in_buf, data, partial);
    len -= partial;
    data = (void *)((uintptr_t)data + partial);
    fio_sha2_perform_all_rounds(s, s->buf);
  }
  while (len >= 64) {
    fio_sha2_perform_all_rounds(s, data);
    data = (void *)((uintptr_t)data + 64);
    len -= 64;
  }
  if (len) {
    memcpy(s->buf + in_buf, data, len);
  }
  return;
}

/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *fio_sha2_result(fio_sha2_s *s) {
  if (s->type & 1) {
    /* 512 bits derived hashing */

    size_t in_buf = s->len.words[0] & 127;

    if (in_buf > 111) {
      memcpy(s->buf + in_buf, sha2_padding, 128 - in_buf);
      fio_sha2_perform_all_rounds(s, s->buf);
      memcpy(s->buf, sha2_padding + 1, 112);
    } else if (in_buf != 111) {
      memcpy(s->buf + in_buf, sha2_padding, 112 - in_buf);
    } else {
      s->buf[111] = sha2_padding[0];
    }
    /* store the length in BITS - alignment should be promised by struct */
    /* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */

    s->len.words[1] = (s->len.words[1] << 3) | (s->len.words[0] >> 61);
    s->len.words[0] = s->len.words[0] << 3;
    s->len.words[0] = fio_lton64(s->len.words[0]);
    s->len.words[1] = fio_lton64(s->len.words[1]);

#if !__BIG_ENDIAN__
    {
      uint_fast64_t tmp = s->len.words[0];
      s->len.words[0] = s->len.words[1];
      s->len.words[1] = tmp;
    }
#endif

    uint64_t *len = (uint64_t *)(s->buf + 112);
    len[0] = s->len.words[0];
    len[1] = s->len.words[1];
    fio_sha2_perform_all_rounds(s, s->buf);

    /* change back to little endian */
    s->digest.i64[0] = fio_ntol64(s->digest.i64[0]);
    s->digest.i64[1] = fio_ntol64(s->digest.i64[1]);
    s->digest.i64[2] = fio_ntol64(s->digest.i64[2]);
    s->digest.i64[3] = fio_ntol64(s->digest.i64[3]);
    s->digest.i64[4] = fio_ntol64(s->digest.i64[4]);
    s->digest.i64[5] = fio_ntol64(s->digest.i64[5]);
    s->digest.i64[6] = fio_ntol64(s->digest.i64[6]);
    s->digest.i64[7] = fio_ntol64(s->digest.i64[7]);
    // set NULL bytes for SHA-2 Type
    switch (s->type) {
    case SHA_512_224:
      s->digest.str[28] = 0;
      break;
    case SHA_512_256:
      s->digest.str[32] = 0;
      break;
    case SHA_384:
      s->digest.str[48] = 0;
      break;
    default:
      s->digest.str[64] =
          0; /* sometimes the optimizer messes the NUL sequence */
      break;
    }
    // fprintf(stderr, "result requested, in hex, is:");
    // for (size_t i = 0; i < ((s->type & 1) ? 64 : 32); i++)
    //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
    // fprintf(stderr, "\r\n");
    return (char *)s->digest.str;
  }

  size_t in_buf = s->len.words[0] & 63;
  if (in_buf > 55) {
    memcpy(s->buf + in_buf, sha2_padding, 64 - in_buf);
    fio_sha2_perform_all_rounds(s, s->buf);
    memcpy(s->buf, sha2_padding + 1, 56);
  } else if (in_buf != 55) {
    memcpy(s->buf + in_buf, sha2_padding, 56 - in_buf);
  } else {
    s->buf[55] = sha2_padding[0];
  }
  /* store the length in BITS - alignment should be promised by struct */
  /* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */
  uint64_t *len = (uint64_t *)(s->buf + 56);
  *len = s->len.words[0] << 3;
  *len = fio_lton64(*len);
  fio_sha2_perform_all_rounds(s, s->buf);

  /* change back to little endian, if required */

  s->digest.i32[0] = fio_ntol32(s->digest.i32[0]);
  s->digest.i32[1] = fio_ntol32(s->digest.i32[1]);
  s->digest.i32[2] = fio_ntol32(s->digest.i32[2]);
  s->digest.i32[3] = fio_ntol32(s->digest.i32[3]);
  s->digest.i32[4] = fio_ntol32(s->digest.i32[4]);
  s->digest.i32[5] = fio_ntol32(s->digest.i32[5]);
  s->digest.i32[6] = fio_ntol32(s->digest.i32[6]);
  s->digest.i32[7] = fio_ntol32(s->digest.i32[7]);

  // set NULL bytes for SHA_224
  if (s->type == SHA_224)
    s->digest.str[28] = 0;
  // fprintf(stderr, "SHA-2 result requested, in hex, is:");
  // for (size_t i = 0; i < ((s->type & 1) ? 64 : 32); i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return (char *)s->digest.str;
}

#undef perform_single_round

/* *****************************************************************************
Section Start Marker





























                                     Testing






























***************************************************************************** */

#if TEST || DEBUG

// clang-format off
#if defined(HAVE_OPENSSL)
#  include <openssl/sha.h>
#endif
// clang-format on

/* *****************************************************************************
Testing Core Callback add / remove / ensure
***************************************************************************** */

FIO_SFUNC void fio_state_callback_test_task(void *pi) {
  ((uintptr_t *)pi)[0] += 1;
}

#define FIO_STATE_TEST_COUNT 10
FIO_SFUNC void fio_state_callback_order_test_task(void *pi) {
  static uintptr_t start = FIO_STATE_TEST_COUNT;
  --start;
  FIO_ASSERT((uintptr_t)pi == start,
             "Callback order error, expecting %zu, got %zu", (size_t)start,
             (size_t)pi);
}

FIO_SFUNC void fio_state_callback_test(void) {
  fprintf(stderr, "=== Testing facil.io workflow state callback system\n");
  uintptr_t result = 0;
  uintptr_t other = 0;
  fio_state_callback_add(FIO_CALL_NEVER, fio_state_callback_test_task, &result);
  FIO_ASSERT(callback_collection[FIO_CALL_NEVER].callbacks.next,
             "Callback list failed to initialize.");
  fio_state_callback_force(FIO_CALL_NEVER);
  FIO_ASSERT(result == 1, "Callback wasn't called!");
  fio_state_callback_force(FIO_CALL_NEVER);
  FIO_ASSERT(result == 2, "Callback wasn't called (second time)!");
  fio_state_callback_remove(FIO_CALL_NEVER, fio_state_callback_test_task,
                            &result);
  fio_state_callback_force(FIO_CALL_NEVER);
  FIO_ASSERT(result == 2, "Callback wasn't removed!");
  fio_state_callback_add(FIO_CALL_NEVER, fio_state_callback_test_task, &result);
  fio_state_callback_add(FIO_CALL_NEVER, fio_state_callback_test_task, &other);
  fio_state_callback_clear(FIO_CALL_NEVER);
  fio_state_callback_force(FIO_CALL_NEVER);
  FIO_ASSERT(result == 2 && other == 0, "Callbacks werent cleared!");
  for (uintptr_t i = 0; i < FIO_STATE_TEST_COUNT; ++i) {
    fio_state_callback_add(FIO_CALL_NEVER, fio_state_callback_order_test_task,
                           (void *)i);
  }
  fio_state_callback_force(FIO_CALL_NEVER);
  fio_state_callback_clear(FIO_CALL_NEVER);
  fprintf(stderr, "* passed.\n");
}
#undef FIO_STATE_TEST_COUNT
/* *****************************************************************************
Testing fio_timers
***************************************************************************** */

FIO_SFUNC void fio_timer_test_task(void *arg) { ++(((size_t *)arg)[0]); }

FIO_SFUNC void fio_timer_test(void) {
  fprintf(stderr, "=== Testing facil.io timer system\n");
  size_t result = 0;
  const size_t total = 5;
  fio_data->active = 1;
  FIO_ASSERT(fio_timers.next, "Timers not initialized!");
  FIO_ASSERT(fio_run_every(0, 0, fio_timer_test_task, NULL, NULL) == -1,
             "Timers without an interval should be an error.");
  FIO_ASSERT(fio_run_every(1000, 0, NULL, NULL, NULL) == -1,
             "Timers without a task should be an error.");
  FIO_ASSERT(fio_run_every(900, total, fio_timer_test_task, &result,
                           fio_timer_test_task) == 0,
             "Timer creation failure.");
  FIO_ASSERT(fio_timer_any(&fio_timers),
             "Timer scheduling failure - no timer in list.");
  FIO_ASSERT(fio_timer_calc_first_interval() >= 898 &&
                 fio_timer_calc_first_interval() <= 902,
             "next timer calculation error %zu",
             fio_timer_calc_first_interval());

  fio_timer_s *first = fio_timer_root(fio_timers.next);
  FIO_ASSERT(fio_run_every(10000, total, fio_timer_test_task, &result,
                           fio_timer_test_task) == 0,
             "Timer creation failure (second timer).");
  FIO_ASSERT(fio_timer_root(fio_timers.next) == first, "Timer Ordering error!");

  FIO_ASSERT(fio_timer_calc_first_interval() >= 898 &&
                 fio_timer_calc_first_interval() <= 902,
             "next timer calculation error (after added timer) %zu",
             fio_timer_calc_first_interval());

  fio_data->last_cycle.tv_nsec += 800;
  fio_timer_schedule();
  fio_defer_perform();
  FIO_ASSERT(result == 0, "Timer filtering error (%zu != 0)\n", result);

  for (size_t i = 0; i < total; ++i) {
    fio_data->last_cycle.tv_sec += 1;
    // fio_data->last_cycle.tv_nsec += 1;
    fio_timer_schedule();
    fio_defer_perform();
    FIO_ASSERT(((i != total - 1 && result == i + 1) ||
                (i == total - 1 && result == total + 1)),
               "Timer running and rescheduling error (%zu != %zu)\n", result,
               i + 1);
    FIO_ASSERT(fio_timer_root(fio_timers.next) == first || i == total - 1,
               "Timer Ordering error on cycle %zu!", i);
  }

  fio_data->last_cycle.tv_sec += 10;
  fio_timer_schedule();
  fio_defer_perform();
  FIO_ASSERT(result == total + 2, "Timer # 2 error (%zu != %zu)\n", result,
             total + 2);
  fio_data->active = 0;
  fio_timer_clear_all();
  fio_defer_clear_tasks();
  fprintf(stderr, "* passed.\n");
}

/* *****************************************************************************
Testing listening socket
***************************************************************************** */

FIO_SFUNC void fio_socket_test(void) {
  /* initialize unix socket name */
  fio_str_s sock_name = FIO_STRING_INIT;
#ifdef P_tmpdir
  fio_str_write(&sock_name, P_tmpdir, strlen(P_tmpdir));
  if (fio_str_len(&sock_name) &&
      fio_str2ptr(&sock_name)[fio_str_len(&sock_name) - 1] == '/')
    fio_str_resize(&sock_name, fio_str_len(&sock_name) - 1);
#else
  fio_str_write(&sock_name, "/tmp", 4);
#endif
  fio_str_printf(&sock_name, "/fio_test_sock-%d.sock", (int)getpid());

  fprintf(stderr, "=== Testing facil.io listening socket creation (partial "
                  "testing only).\n");
  fprintf(stderr, "* testing on TCP/IP port 8765 and Unix socket: %s\n",
          fio_str2ptr(&sock_name));
  intptr_t uuid = fio_socket(fio_str2ptr(&sock_name), NULL,
                             FIO_SOCKET_UNIX | FIO_SOCKET_SERVER);
  FIO_ASSERT(uuid != -1, "Failed to open unix socket\n");
  FIO_ASSERT(uuid_data(uuid).open, "Unix socket not initialized");
  intptr_t client1 = fio_socket(fio_str2ptr(&sock_name), NULL,
                                FIO_SOCKET_UNIX | FIO_SOCKET_CLIENT);
  FIO_ASSERT(client1 != -1, "Failed to connect to unix socket.");
  intptr_t client2 = fio_accept(uuid);
  FIO_ASSERT(client2 != -1, "Failed to accept unix socket connection.");
  fprintf(stderr, "* Unix server addr %s\n", fio_peer_addr(uuid).buf);
  fprintf(stderr, "* Unix client1 addr %s\n", fio_peer_addr(client1).buf);
  fprintf(stderr, "* Unix client2 addr %s\n", fio_peer_addr(client2).buf);
  {
    char tmp_buf[28];
    ssize_t r = -1;
    ssize_t timer_junk;
    fio_write(client1, "Hello World", 11);
    if (0) {
      /* packet may have been sent synchronously, don't test */
      if (!uuid_data(client1).packet)
        unlink(__FILE__ ".sock");
      FIO_ASSERT(uuid_data(client1).packet, "fio_write error, no packet!")
    }
    /* prevent poll from hanging */
    fio_run_every(5, 1, fio_timer_test_task, &timer_junk, fio_timer_test_task);
    errno = EAGAIN;
    for (size_t i = 0; i < 100 && r <= 0 &&
                       (r == 0 || errno == EAGAIN || errno == EWOULDBLOCK);
         ++i) {
      fio_poll();
      fio_defer_perform();
      FIO_THREAD_RESCHEDULE();
      errno = 0;
      r = fio_read(client2, tmp_buf, 28);
    }
    if (!(r > 0 && r <= 28) || memcmp("Hello World", tmp_buf, r)) {
      perror("* ernno");
      unlink(__FILE__ ".sock");
    }
    FIO_ASSERT(r > 0 && r <= 28,
               "Failed to read from unix socket " __FILE__ ".sock %zd", r);
    FIO_ASSERT(!memcmp("Hello World", tmp_buf, r),
               "Unix socket Read/Write cycle error (%zd: %.*s)", r, (int)r,
               tmp_buf);
    fprintf(stderr, "* Unix socket Read/Write cycle passed: %.*s\n", (int)r,
            tmp_buf);
    fio_data->last_cycle.tv_sec += 10;
    fio_timer_clear_all();
  }

  fio_force_close(client1);
  fio_force_close(client2);
  fio_force_close(uuid);
  unlink(fio_str2ptr(&sock_name));
  /* free unix socket name */
  fio_str_destroy(&sock_name);

  uuid = fio_socket(NULL, "8765",
                    FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(uuid != -1, "Failed to open TCP/IP socket on port 8765");
  FIO_ASSERT(uuid_data(uuid).open, "TCP/IP socket not initialized");
  fprintf(stderr, "* TCP/IP server addr %s\n", fio_peer_addr(uuid).buf);
  client1 =
      fio_socket("localhost", "8765",
                 FIO_SOCKET_TCP | FIO_SOCKET_CLIENT | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(client1 != -1, "Failed to connect to TCP/IP socket on port 8765");
  fprintf(stderr, "* TCP/IP client1 addr %s\n", fio_peer_addr(client1).buf);
  errno = EAGAIN;
  for (size_t i = 0; i < 100 && (errno == EAGAIN || errno == EWOULDBLOCK);
       ++i) {
    errno = 0;
    FIO_THREAD_RESCHEDULE();
    client2 = fio_accept(uuid);
  }
  if (client2 == -1)
    perror("accept error");
  FIO_ASSERT(client2 != -1,
             "Failed to accept TCP/IP socket connection on port 8765");
  fprintf(stderr, "* TCP/IP client2 addr %s\n", fio_peer_addr(client2).buf);
  fio_force_close(client1);
  fio_force_close(client2);
  fio_force_close(uuid);
  fio_timer_clear_all();
  fio_defer_clear_tasks();
  fprintf(stderr, "* passed.\n");
}

/* *****************************************************************************
Testing listening socket
***************************************************************************** */

FIO_SFUNC void fio_cycle_test_task(void *arg) {
  fio_stop();
  (void)arg;
}
FIO_SFUNC void fio_cycle_test_task2(void *arg) {
  fprintf(stderr, "* facil.io cycling test fatal error!\n");
  exit(-1);
  (void)arg;
}

FIO_SFUNC void fio_cycle_test(void) {
  fprintf(stderr,
          "=== Testing facil.io cycling logic (partial - only tests timers)\n");
  fio_mark_time();
  fio_timer_clear_all();
  struct timespec start = fio_last_tick();
  fio_run_every(1000, 1, fio_cycle_test_task, NULL, NULL);
  fio_run_every(10000, 1, fio_cycle_test_task2, NULL, NULL);
  fio_start(.threads = 1, .workers = 1);
  struct timespec end = fio_last_tick();
  fio_timer_clear_all();
  FIO_ASSERT(end.tv_sec == start.tv_sec + 1 || end.tv_sec == start.tv_sec + 2,
             "facil.io cycling error?");
  fprintf(stderr, "* passed.\n");
}
/* *****************************************************************************
Testing fio_defer task system
***************************************************************************** */

#define FIO_DEFER_TOTAL_COUNT (512 * 1024)

#ifndef FIO_DEFER_TEST_PRINT
#define FIO_DEFER_TEST_PRINT 0
#endif

FIO_SFUNC void sample_task(void *i_count, void *unused2) {
  (void)(unused2);
  fio_atomic_add((uintptr_t *)i_count, 1);
}

FIO_SFUNC void sched_sample_task(void *count, void *i_count) {
  for (size_t i = 0; i < (uintptr_t)count; i++) {
    fio_defer(sample_task, i_count, NULL);
  }
}

FIO_SFUNC void fio_defer_test(void) {
  fprintf(stderr, "===============\n");
  fprintf(stderr, "* Testing facil.io task scheduling (fio_defer)\n");

  const size_t cpu_cores = fio_detect_cpu_cores();
  FIO_ASSERT(cpu_cores, "couldn't detect CPU cores!");
  uintptr_t i_count;
  clock_t start, end;
  FIO_ASSERT(!fio_defer_has_queue(), "facil.io queue always active.")
  i_count = 0;
  start = clock();
  for (size_t i = 0; i < FIO_DEFER_TOTAL_COUNT; i++) {
    sample_task(&i_count, NULL);
  }
  end = clock();
  if (FIO_DEFER_TEST_PRINT) {
    fprintf(stderr,
            "Deferless (direct call) counter: %lu cycles with i_count = %lu, "
            "%lu/%lu free/malloc\n",
            (unsigned long)(end - start), (unsigned long)i_count,
            (unsigned long)fio_defer_count_dealloc,
            (unsigned long)fio_defer_count_alloc);
  }
  size_t i_count_should_be = i_count;

  if (FIO_DEFER_TEST_PRINT) {
    fprintf(stderr, "\n");
  }

  for (size_t i = 1; FIO_DEFER_TOTAL_COUNT >> i; ++i) {
    i_count = 0;
    const size_t per_task = FIO_DEFER_TOTAL_COUNT >> i;
    const size_t tasks = 1 << i;
    start = clock();
    for (size_t j = 0; j < tasks; ++j) {
      fio_defer(sched_sample_task, (void *)per_task, &i_count);
    }
    FIO_ASSERT(fio_defer_has_queue(), "facil.io queue not marked.")
    fio_defer_thread_pool_join(fio_defer_thread_pool_new((i % cpu_cores) + 1));
    end = clock();
    if (FIO_DEFER_TEST_PRINT) {
      fprintf(stderr,
              "- Defer %zu threads, %zu scheduling loops (%zu each):\n"
              "    %lu cycles with i_count = %lu, %lu/%lu "
              "free/malloc\n",
              ((i % cpu_cores) + 1), tasks, per_task,
              (unsigned long)(end - start), (unsigned long)i_count,
              (unsigned long)fio_defer_count_dealloc,
              (unsigned long)fio_defer_count_alloc);
    } else {
      fprintf(stderr, ".");
    }
    FIO_ASSERT(i_count == i_count_should_be, "ERROR: defer count invalid\n");
    FIO_ASSERT(fio_defer_count_dealloc == fio_defer_count_alloc,
               "defer deallocation vs. allocation error, %zu != %zu",
               fio_defer_count_dealloc, fio_defer_count_alloc);
  }
  FIO_ASSERT(task_queue_normal.writer == &task_queue_normal.static_queue,
             "defer library didn't release dynamic queue (should be static)");
  fprintf(stderr, "\n* passed.\n");
}

/* *****************************************************************************
SipHash tests
***************************************************************************** */

FIO_SFUNC void fio_siphash_speed_test(void) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uint8_t buf[8192];
  memset(buf, 'T', sizeof(buf));
  /* warmup */
  uint64_t hash = 0;
  for (size_t i = 0; i < 4; i++) {
    hash += fio_siphash24(buf, sizeof(buf), 0, 0);
    memcpy(buf, &hash, sizeof(hash));
  }
  /* loop until test runs for more than 2 seconds */
  for (uint64_t cycles = 8192;;) {
    clock_t start, end;
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      hash += fio_siphash24(buf, sizeof(buf), 0, 0);
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    memcpy(buf, &hash, sizeof(hash));
    if ((end - start) >= (2 * CLOCKS_PER_SEC) ||
        cycles >= ((uint64_t)1 << 62)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", "fio SipHash24",
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 2;
  }
  /* loop until test runs for more than 2 seconds */
  for (uint64_t cycles = 8192;;) {
    clock_t start, end;
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      hash += fio_siphash13(buf, sizeof(buf), 0, 0);
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    memcpy(buf, &hash, sizeof(hash));
    if ((end - start) >= (2 * CLOCKS_PER_SEC) ||
        cycles >= ((uint64_t)1 << 62)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", "fio SipHash13",
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 2;
  }
}

FIO_SFUNC void fio_siphash_test(void) {
  fprintf(stderr, "===================================\n");
#if NDEBUG
  fio_siphash_speed_test();
#else
  fprintf(stderr, "fio SipHash speed test skipped (debug mode is slow)\n");
  (void)fio_siphash_speed_test;
#endif
}
/* *****************************************************************************
SHA-1 tests
***************************************************************************** */

FIO_SFUNC void fio_sha1_speed_test(void) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uint8_t buf[8192];
  uint8_t result[21];
  fio_sha1_s sha1;
  memset(buf, 'T', sizeof(buf));
  /* warmup */
  for (size_t i = 0; i < 4; i++) {
    sha1 = fio_sha1_init();
    fio_sha1_write(&sha1, buf, sizeof(buf));
    memcpy(result, fio_sha1_result(&sha1), 21);
  }
  /* loop until test runs for more than 2 seconds */
  for (size_t cycles = 8192;;) {
    clock_t start, end;
    sha1 = fio_sha1_init();
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      fio_sha1_write(&sha1, buf, sizeof(buf));
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    fio_sha1_result(&sha1);
    if ((end - start) >= (2 * CLOCKS_PER_SEC)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", "fio SHA-1",
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 1;
  }
}

#ifdef HAVE_OPENSSL
FIO_SFUNC void fio_sha1_open_ssl_speed_test(void) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uint8_t buf[8192];
  uint8_t result[21];
  SHA_CTX o_sh1;
  memset(buf, 'T', sizeof(buf));
  /* warmup */
  for (size_t i = 0; i < 4; i++) {
    SHA1_Init(&o_sh1);
    SHA1_Update(&o_sh1, buf, sizeof(buf));
    SHA1_Final(result, &o_sh1);
  }
  /* loop until test runs for more than 2 seconds */
  for (size_t cycles = 8192;;) {
    clock_t start, end;
    SHA1_Init(&o_sh1);
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      SHA1_Update(&o_sh1, buf, sizeof(buf));
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    SHA1_Final(result, &o_sh1);
    if ((end - start) >= (2 * CLOCKS_PER_SEC)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", "OpenSSL SHA-1",
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 1;
  }
}
#endif

FIO_SFUNC void fio_sha1_test(void) {
  // clang-format off
  struct {
    char *str;
    uint8_t hash[21];
  } sets[] = {
      {"The quick brown fox jumps over the lazy dog",
       {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e,
        0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12, 0}}, // a set with
                                                                   // a string
      {"",
       {
           0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
           0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09,
       }},        // an empty set
      {NULL, {0}} // Stop
  };
  // clang-format on
  int i = 0;
  fio_sha1_s sha1;
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "fio SHA-1 struct size: %zu\n", sizeof(fio_sha1_s));
  fprintf(stderr, "+ fio");
  while (sets[i].str) {
    sha1 = fio_sha1_init();
    fio_sha1_write(&sha1, sets[i].str, strlen(sets[i].str));
    if (strcmp(fio_sha1_result(&sha1), (char *)sets[i].hash)) {
      fprintf(stderr, ":\n--- fio SHA-1 Test FAILED!\nstring: %s\nexpected: ",
              sets[i].str);
      char *p = (char *)sets[i].hash;
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\ngot: ");
      p = fio_sha1_result(&sha1);
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\n");
      FIO_ASSERT(0, "SHA-1 failure.");
      return;
    }
    i++;
  }
  fprintf(stderr, " SHA-1 passed.\n");
#if NDEBUG
  fio_sha1_speed_test();
#else
  fprintf(stderr, "fio SHA1 speed test skipped (debug mode is slow)\n");
  (void)fio_sha1_speed_test;
#endif

#ifdef HAVE_OPENSSL

#if NDEBUG
  fio_sha1_open_ssl_speed_test();
#else
  fprintf(stderr, "OpenSSL SHA1 speed test skipped (debug mode is slow)\n");
  (void)fio_sha1_open_ssl_speed_test;
#endif
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "fio SHA-1 struct size: %lu\n",
          (unsigned long)sizeof(fio_sha1_s));
  fprintf(stderr, "OpenSSL SHA-1 struct size: %lu\n",
          (unsigned long)sizeof(SHA_CTX));
  fprintf(stderr, "===================================\n");
#endif /* HAVE_OPENSSL */
}

/* *****************************************************************************
SHA-2 tests
***************************************************************************** */

FIO_SFUNC char *sha2_variant_names[] = {
    "unknown", "SHA_512",     "SHA_256", "SHA_512_256",
    "SHA_224", "SHA_512_224", "none",    "SHA_384",
};

FIO_SFUNC void fio_sha2_speed_test(fio_sha2_variant_e var,
                                   const char *var_name) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uint8_t buf[8192];
  uint8_t result[65];
  fio_sha2_s sha2;
  memset(buf, 'T', sizeof(buf));
  /* warmup */
  for (size_t i = 0; i < 4; i++) {
    sha2 = fio_sha2_init(var);
    fio_sha2_write(&sha2, buf, sizeof(buf));
    memcpy(result, fio_sha2_result(&sha2), 65);
  }
  /* loop until test runs for more than 2 seconds */
  for (size_t cycles = 8192;;) {
    clock_t start, end;
    sha2 = fio_sha2_init(var);
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      fio_sha2_write(&sha2, buf, sizeof(buf));
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    fio_sha2_result(&sha2);
    if ((end - start) >= (2 * CLOCKS_PER_SEC)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", var_name,
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 1;
  }
}

FIO_SFUNC void fio_sha2_openssl_speed_test(const char *var_name, int (*init)(),
                                           int (*update)(), int (*final)(),
                                           void *sha) {
  /* test adapted from BearSSL code with credit to Thomas Pornin */
  uint8_t buf[8192];
  uint8_t result[1024];
  memset(buf, 'T', sizeof(buf));
  /* warmup */
  for (size_t i = 0; i < 4; i++) {
    init(sha);
    update(sha, buf, sizeof(buf));
    final(result, sha);
  }
  /* loop until test runs for more than 2 seconds */
  for (size_t cycles = 2048;;) {
    clock_t start, end;
    init(sha);
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      update(sha, buf, sizeof(buf));
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    final(result, sha);
    if ((end - start) >= (2 * CLOCKS_PER_SEC)) {
      fprintf(stderr, "%-20s %8.2f MB/s\n", var_name,
              (double)(sizeof(buf) * cycles) /
                  (((end - start) * 1000000.0 / CLOCKS_PER_SEC)));
      break;
    }
    cycles <<= 1;
  }
}
FIO_SFUNC void fio_sha2_test(void) {
  fio_sha2_s s;
  char *expect;
  char *got;
  char *str = "";
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "fio SHA-2 struct size: %zu\n", sizeof(fio_sha2_s));
  fprintf(stderr, "+ fio");
  // start tests
  s = fio_sha2_init(SHA_224);
  fio_sha2_write(&s, str, 0);
  expect = "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9\x47\x61\x02\xbb\x28\x82\x34\xc4"
           "\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a\xc5\xb3\xe4\x2f";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_256);
  fio_sha2_write(&s, str, 0);
  expect =
      "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27"
      "\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_512);
  fio_sha2_write(&s, str, 0);
  expect = "\xcf\x83\xe1\x35\x7e\xef\xb8\xbd\xf1\x54\x28\x50\xd6\x6d"
           "\x80\x07\xd6\x20\xe4\x05\x0b\x57\x15\xdc\x83\xf4\xa9\x21"
           "\xd3\x6c\xe9\xce\x47\xd0\xd1\x3c\x5d\x85\xf2\xb0\xff\x83"
           "\x18\xd2\x87\x7e\xec\x2f\x63\xb9\x31\xbd\x47\x41\x7a\x81"
           "\xa5\x38\x32\x7a\xf9\x27\xda\x3e";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_384);
  fio_sha2_write(&s, str, 0);
  expect = "\x38\xb0\x60\xa7\x51\xac\x96\x38\x4c\xd9\x32\x7e"
           "\xb1\xb1\xe3\x6a\x21\xfd\xb7\x11\x14\xbe\x07\x43\x4c\x0c"
           "\xc7\xbf\x63\xf6\xe1\xda\x27\x4e\xde\xbf\xe7\x6f\x65\xfb"
           "\xd5\x1a\xd2\xf1\x48\x98\xb9\x5b";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_512_224);
  fio_sha2_write(&s, str, 0);
  expect = "\x6e\xd0\xdd\x02\x80\x6f\xa8\x9e\x25\xde\x06\x0c\x19\xd3"
           "\xac\x86\xca\xbb\x87\xd6\xa0\xdd\xd0\x5c\x33\x3b\x84\xf4";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_512_256);
  fio_sha2_write(&s, str, 0);
  expect = "\xc6\x72\xb8\xd1\xef\x56\xed\x28\xab\x87\xc3\x62\x2c\x51\x14\x06"
           "\x9b\xdd\x3a\xd7\xb8\xf9\x73\x74\x98\xd0\xc0\x1e\xce\xf0\x96\x7a";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = fio_sha2_init(SHA_512);
  str = "god is a rotten tomato";
  fio_sha2_write(&s, str, strlen(str));
  expect = "\x61\x97\x4d\x41\x9f\x77\x45\x21\x09\x4e\x95\xa3\xcb\x4d\xe4\x79"
           "\x26\x32\x2f\x2b\xe2\x62\x64\x5a\xb4\x5d\x3f\x73\x69\xef\x46\x20"
           "\xb2\xd3\xce\xda\xa9\xc2\x2c\xac\xe3\xf9\x02\xb2\x20\x5d\x2e\xfd"
           "\x40\xca\xa0\xc1\x67\xe0\xdc\xdf\x60\x04\x3e\x4e\x76\x87\x82\x74";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  // s = fio_sha2_init(SHA_256);
  // str = "The quick brown fox jumps over the lazy dog";
  // fio_sha2_write(&s, str, strlen(str));
  // expect =
  //     "\xd7\xa8\xfb\xb3\x07\xd7\x80\x94\x69\xca\x9a\xbc\xb0\x08\x2e\x4f"
  //     "\x8d\x56\x51\xe4\x6d\x3c\xdb\x76\x2d\x02\xd0\xbf\x37\xc9\xe5\x92";
  // got = fio_sha2_result(&s);
  // if (strcmp(expect, got))
  //   goto error;

  s = fio_sha2_init(SHA_224);
  str = "The quick brown fox jumps over the lazy dog";
  fio_sha2_write(&s, str, strlen(str));
  expect = "\x73\x0e\x10\x9b\xd7\xa8\xa3\x2b\x1c\xb9\xd9\xa0\x9a\xa2"
           "\x32\x5d\x24\x30\x58\x7d\xdb\xc0\xc3\x8b\xad\x91\x15\x25";
  got = fio_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;
  fprintf(stderr, " SHA-2 passed.\n");

#if NDEBUG
  fio_sha2_speed_test(SHA_224, "fio SHA-224");
  fio_sha2_speed_test(SHA_256, "fio SHA-256");
  fio_sha2_speed_test(SHA_384, "fio SHA-384");
  fio_sha2_speed_test(SHA_512, "fio SHA-512");
#else
  fprintf(stderr, "fio SHA-2 speed test skipped (debug mode is slow)\n");
#endif

#ifdef HAVE_OPENSSL

#if NDEBUG
  {
    SHA512_CTX s2;
    SHA256_CTX s3;
    fio_sha2_openssl_speed_test("OpenSSL SHA512", SHA512_Init, SHA512_Update,
                                SHA512_Final, &s2);
    fio_sha2_openssl_speed_test("OpenSSL SHA256", SHA256_Init, SHA256_Update,
                                SHA256_Final, &s3);
  }
#endif
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "fio SHA-2 struct size: %zu\n", sizeof(fio_sha2_s));
  fprintf(stderr, "OpenSSL SHA-2/256 struct size: %zu\n", sizeof(SHA256_CTX));
  fprintf(stderr, "OpenSSL SHA-2/512 struct size: %zu\n", sizeof(SHA512_CTX));
  fprintf(stderr, "===================================\n");
#endif /* HAVE_OPENSSL */

  return;

error:
  fprintf(stderr,
          ":\n--- fio SHA-2 Test FAILED!\ntype: "
          "%s (%d)\nstring %s\nexpected:\n",
          sha2_variant_names[s.type], s.type, str);
  while (*expect)
    fprintf(stderr, "%02x", *(expect++) & 0xFF);
  fprintf(stderr, "\ngot:\n");
  while (*got)
    fprintf(stderr, "%02x", *(got++) & 0xFF);
  fprintf(stderr, "\n");
  (void)fio_sha2_speed_test;
  (void)fio_sha2_openssl_speed_test;
  FIO_ASSERT(0, "SHA-2 failure.");
}

/* *****************************************************************************
Poll (not kqueue or epoll) tests
***************************************************************************** */
#if FIO_ENGINE_POLL
FIO_SFUNC void fio_poll_test(void) {
  fprintf(stderr, "=== Testing poll add / remove fd\n");
  fio_poll_add(5);
  FIO_ASSERT(fio_data->poll[5].fd == 5, "fio_poll_add didn't set used fd data");
  FIO_ASSERT(fio_data->poll[5].events ==
                 (FIO_POLL_READ_EVENTS | FIO_POLL_WRITE_EVENTS),
             "fio_poll_add didn't set used fd flags");
  fio_poll_add(7);
  FIO_ASSERT(fio_data->poll[6].fd == -1,
             "fio_poll_add didn't reset unused fd data %d",
             fio_data->poll[6].fd);
  fio_poll_add(6);
  fio_poll_remove_fd(6);
  FIO_ASSERT(fio_data->poll[6].fd == -1,
             "fio_poll_remove_fd didn't reset unused fd data");
  FIO_ASSERT(fio_data->poll[6].events == 0,
             "fio_poll_remove_fd didn't reset unused fd flags");
  fio_poll_remove_read(7);
  FIO_ASSERT(fio_data->poll[7].events == (FIO_POLL_WRITE_EVENTS),
             "fio_poll_remove_read didn't remove read flags");
  fio_poll_add_read(7);
  fio_poll_remove_write(7);
  FIO_ASSERT(fio_data->poll[7].events == (FIO_POLL_READ_EVENTS),
             "fio_poll_remove_write didn't remove read flags");
  fio_poll_add_write(7);
  fio_poll_remove_read(7);
  FIO_ASSERT(fio_data->poll[7].events == (FIO_POLL_WRITE_EVENTS),
             "fio_poll_add_write didn't add the write flag?");
  fio_poll_remove_write(7);
  FIO_ASSERT(fio_data->poll[7].fd == -1,
             "fio_poll_remove (both) didn't reset unused fd data");
  FIO_ASSERT(fio_data->poll[7].events == 0,
             "fio_poll_remove (both) didn't reset unused fd flags");
  fio_poll_remove_fd(5);
  fprintf(stderr, "\n* passed.\n");
}
#else
#define fio_poll_test()
#endif

/* *****************************************************************************
Test UUID Linking
***************************************************************************** */

FIO_SFUNC void fio_uuid_env_test_on_close(void *obj) {
  fio_atomic_add((uintptr_t *)obj, 1);
}

FIO_SFUNC void fio_uuid_env_test(void) {
  fprintf(stderr, "=== Testing fio_uuid_env\n");
  uintptr_t called = 0;
  uintptr_t removed = 0;
  uintptr_t overwritten = 0;
  intptr_t uuid = fio_socket(
      NULL, "8765", FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(uuid != -1, "fio_uuid_env_test failed to create a socket!");
  fio_uuid_env_set(uuid, .data = &called,
                   .on_close = fio_uuid_env_test_on_close, .type = 1);
  FIO_ASSERT(called == 0,
             "fio_uuid_env_set failed - on_close callback called too soon!");
  fio_uuid_env_set(uuid, .data = &removed,
                   .on_close = fio_uuid_env_test_on_close, .type = 0);
  fio_uuid_env_set(uuid, .data = &overwritten,
                   .on_close = fio_uuid_env_test_on_close, .type = 0,
                   .name.buf = "a", .name.len = 1);
  fio_uuid_env_set(uuid, .data = &overwritten,
                   .on_close = fio_uuid_env_test_on_close, .type = 0,
                   .name.buf = "a", .name.len = 1);
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

/* *****************************************************************************
Byte Order Testing
***************************************************************************** */

FIO_SFUNC void fio_buf2u_test(void) {
  fprintf(stderr, "=== Testing fio_u2bufX and fio_u2bufX functions.\n");
  char buf[32];
  for (int64_t i = -1024; i < 1024; ++i) {
    fio_u2buf64(buf, i);
    __asm__ volatile("" ::: "memory");
    FIO_ASSERT((int64_t)fio_buf2u64(buf) == i,
               "fio_u2buf64 / fio_buf2u64  mismatch %zd != %zd",
               (ssize_t)fio_buf2u64(buf), (ssize_t)i);
  }
  for (int32_t i = -1024; i < 1024; ++i) {
    fio_u2buf32(buf, i);
    __asm__ volatile("" ::: "memory");
    FIO_ASSERT((int32_t)fio_buf2u32(buf) == i,
               "fio_u2buf32 / fio_buf2u32  mismatch %zd != %zd",
               (ssize_t)(fio_buf2u32(buf)), (ssize_t)i);
  }
  for (int16_t i = -1024; i < 1024; ++i) {
    fio_u2buf16(buf, i);
    __asm__ volatile("" ::: "memory");
    FIO_ASSERT((int16_t)fio_buf2u16(buf) == i,
               "fio_u2buf16 / fio_buf2u16  mismatch %zd != %zd",
               (ssize_t)(fio_buf2u16(buf)), (ssize_t)i);
  }
  fprintf(stderr, "* passed.\n");
}

/* *****************************************************************************
Pub/Sub partial tests
***************************************************************************** */

#if FIO_PUBSUB_SUPPORT

FIO_SFUNC void fio_pubsub_test_on_message(fio_msg_s *msg) {
  fio_atomic_add((uintptr_t *)msg->udata1, 1);
}
FIO_SFUNC void fio_pubsub_test_on_unsubscribe(void *udata1, void *udata2) {
  fio_atomic_add((uintptr_t *)udata1, 1);
  (void)udata2;
}

FIO_SFUNC void fio_pubsub_test(void) {
  fprintf(stderr, "=== Testing pub/sub (partial)\n");
  fio_data->active = 1;
  fio_data->is_worker = 1;
  fio_data->workers = 1;
  subscription_s *s = fio_subscribe(.filter = 1, .on_message = NULL);
  uintptr_t counter = 0;
  uintptr_t expect = 0;
  FIO_ASSERT(!s, "fio_subscribe should fail without a callback!");
  char buf[8];
  fio_u2buf32((uint8_t *)buf + 1, 42);
  FIO_ASSERT(fio_buf2u32((uint8_t *)buf + 1) == 42,
             "fio_u2buf32 / fio_buf2u32 not reversible (42)!");
  fio_u2buf32((uint8_t *)buf, 4);
  FIO_ASSERT(fio_buf2u32((uint8_t *)buf) == 4,
             "fio_u2buf32 / fio_buf2u32 not reversible (4)!");
  subscription_s *s2 =
      fio_subscribe(.filter = 1, .udata1 = &counter,
                    .on_message = fio_pubsub_test_on_message,
                    .on_unsubscribe = fio_pubsub_test_on_unsubscribe);
  FIO_ASSERT(s2, "fio_subscribe FAILED on filtered subscription.");
  fio_publish(.filter = 1);
  ++expect;
  fio_defer_perform();
  FIO_ASSERT(counter == expect, "publishing failed to filter 1!");
  fio_publish(.filter = 2);
  fio_defer_perform();
  FIO_ASSERT(counter == expect, "publishing to filter 2 arrived at filter 1!");
  fio_unsubscribe(s);
  fio_unsubscribe(s2);
  ++expect;
  fio_defer_perform();
  FIO_ASSERT(counter == expect, "unsubscribe wasn't called for filter 1!");
  s = fio_subscribe(.channel = {0, 4, "name"}, .udata1 = &counter,
                    .on_message = fio_pubsub_test_on_message,
                    .on_unsubscribe = fio_pubsub_test_on_unsubscribe);
  FIO_ASSERT(s, "fio_subscribe FAILED on named subscription.");
  fio_publish(.channel = {0, 4, "name"});
  ++expect;
  fio_defer_perform();
  FIO_ASSERT(counter == expect, "publishing failed to named channel!");
  fio_publish(.channel = {0, 4, "none"});
  fio_defer_perform();
  FIO_ASSERT(counter == expect,
             "publishing arrived to named channel with wrong name!");
  fio_unsubscribe(s);
  ++expect;
  fio_defer_perform();
  FIO_ASSERT(counter == expect, "unsubscribe wasn't called for named channel!");
  fio_data->is_worker = 0;
  fio_data->active = 0;
  fio_data->workers = 0;
  fio_defer_perform();
  (void)fio_pubsub_test_on_message;
  (void)fio_pubsub_test_on_unsubscribe;
  fprintf(stderr, "* passed.\n");
}
#else
#define fio_pubsub_test()
#endif

/* *****************************************************************************
Run all tests
***************************************************************************** */

void fio_test(void) {
  fio_siphash_test();
  fio_sha1_test();
  fio_sha2_test();
  FIO_ASSERT(fio_capa(), "facil.io initialization error!");
  fio_state_callback_test();
  fio_defer_test();
  fio_timer_test();
  fio_poll_test();
  fio_socket_test();
  fio_uuid_env_test();
  fio_cycle_test();
  fio_pubsub_test();
  (void)fio_sentinel_task;
  (void)deferred_on_shutdown;
  (void)fio_poll;
}

#endif /* TEST || DEBUG */
