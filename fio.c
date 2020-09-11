/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/io-core
***************************************************************************** */

/* *****************************************************************************
External STL features published
***************************************************************************** */
#define FIO_EXTERN_COMPLETE   1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>
/* *****************************************************************************
Developent Sugar (ignore)
***************************************************************************** */
#ifndef H_FACIL_IO_H            /* Development inclusion - ignore line */
#define FIO_EXTERN_COMPLETE   1 /* Development inclusion - ignore line */
#define FIOBJ_EXTERN_COMPLETE 1 /* Development inclusion - ignore line */
#define FIO_VERSION_GUARD       /* Development inclusion - ignore line */
#include "0003 main api.h"      /* Development inclusion - ignore line */
#include "0004 pubsub.h"        /* Development inclusion - ignore line */
#include "0005 overridables.h"  /* Development inclusion - ignore line */
#include "0006 footer.h"        /* Development inclusion - ignore line */
#endif                          /* Development inclusion - ignore line */

/* *****************************************************************************
Implementation include statements and Macros
***************************************************************************** */
#define FIO_SOCK
#define FIO_STREAM
#define FIO_SIGNAL
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

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 1000
#endif

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

#if !defined(FIO_ENGINE_EPOLL) && !defined(FIO_ENGINE_KQUEUE) &&               \
    !defined(FIO_ENGINE_POLL)
#if defined(HAVE_EPOLL) || __has_include("sys/epoll.h")
#define FIO_ENGINE_EPOLL 1
#elif defined(HAVE_KQUEUE) || __has_include("sys/event.h")
#define FIO_ENGINE_KQUEUE 1
#else
#define FIO_ENGINE_POLL 1
#endif
#endif /* !defined(FIO_ENGINE_EPOLL) ... */

#ifndef FIO_SOCKET_BUFFER_PER_WRITE
#define FIO_SOCKET_BUFFER_PER_WRITE (1UL << 16)
#endif

/* *****************************************************************************
Lock designation
***************************************************************************** */

enum fio_protocol_lock_e {
  FIO_PR_LOCK_TASK = 0,
  FIO_PR_LOCK_PING = 1,
  FIO_PR_LOCK_DATA = 2,
};

// clang-format off
#define FIO_UUID_LOCK_PROTOCOL             FIO_LOCK_SUBLOCK(0)
#define FIO_UUID_LOCK_RW_HOOKS_READ        FIO_LOCK_SUBLOCK(1)
#define FIO_UUID_LOCK_RW_HOOKS_WRITE       FIO_LOCK_SUBLOCK(2)
#define FIO_UUID_LOCK_RW_HOOKS             (FIO_UUID_LOCK_RW_HOOKS_READ | FIO_UUID_LOCK_RW_HOOKS_WRITE)
#define FIO_UUID_LOCK_ENV                  FIO_LOCK_SUBLOCK(3)
#define FIO_UUID_LOCK_STREAM               FIO_LOCK_SUBLOCK(4)
#define FIO_UUID_LOCK_FULL                 (FIO_UUID_LOCK_PROTOCOL | FIO_UUID_LOCK_RW_HOOKS | FIO_UUID_LOCK_ENV | FIO_UUID_LOCK_STREAM)
#define FIO_UUID_LOCK_POLL_READ            FIO_LOCK_SUBLOCK(6)
#define FIO_UUID_LOCK_POLL_WRITE           FIO_LOCK_SUBLOCK(7)
// clang-format on

/* *****************************************************************************
Task Queue
***************************************************************************** */

#define FIO_QUEUE
#include <fio-stl.h>

FIO_IFUNC int fio_defer_urgent(void (*task)(void *, void *),
                               void *udata1,
                               void *udata2);

FIO_IFUNC size_t fio___timer_calc_first_interval(void);

/* *****************************************************************************
Internal STL features in use (unpublished)
***************************************************************************** */
#define FIO_STR_SMALL fio_str
#include "fio-stl.h"

/* *****************************************************************************
UUID env objects / store
***************************************************************************** */

/** An object that can be linked to any facil.io connection (uuid). */
typedef struct {
  void *udata;
  void (*on_close)(void *data);
} fio_uuid_env_obj_s;

/* cleanup event task */
static void fio_uuid_env_obj_call_callback_task(void *p, void *udata) {
  union {
    void *p;
    void (*fn)(void *);
  } u;
  u.p = p;
  u.fn(udata);
}

/* cleanup event scheduling */
FIO_IFUNC void fio_uuid_env_obj_call_callback(fio_uuid_env_obj_s o) {
  union {
    void *p;
    void (*fn)(void *);
  } u;
  u.fn = o.on_close;
  if (o.on_close) {
    fio_defer_urgent(fio_uuid_env_obj_call_callback_task, u.p, o.udata);
  }
}

#define FIO_OMAP_NAME              fio___uuid_env
#define FIO_MAP_TYPE               fio_uuid_env_obj_s
#define FIO_MAP_TYPE_DESTROY(o)    fio_uuid_env_obj_call_callback((o))
#define FIO_MAP_DESTROY_AFTER_COPY 0

/* destroy discarded keys when overwriting existing data (const_name support) */
#define FIO_MAP_KEY                 fio_str_s /* the small string type */
#define FIO_MAP_KEY_COPY(dest, src) (dest) = (src)
#define FIO_MAP_KEY_DESTROY(k)      fio_str_destroy(&k)
#define FIO_MAP_KEY_DISCARD         FIO_MAP_KEY_DESTROY
#define FIO_MAP_KEY_CMP(a, b)       fio_str_is_eq(&(a), &(b))
#include <fio-stl.h>

/* *****************************************************************************
Section Start Marker









                               UUID Data Structure










***************************************************************************** */

typedef struct {
  /* fd protocol */
  fio_protocol_s *protocol;
  /** RW hooks. */
  fio_rw_hook_s *rw_hooks;
  /** RW udata. */
  void *rw_udata;
  /* current data to be send */
  fio_stream_s stream;
  /* timer handler */
  time_t active;
  /* Objects linked to the UUID */
  fio___uuid_env_s env;
  /* timeout settings */
  uint8_t timeout;
  /* fd_data lock */
  fio_lock_i lock;
  /* used to convert `fd` to `uuid` and validate connections */
  uint8_t counter;
  /** Connection is open */
  uint8_t open;
  /** indicated that the connection should be closed. */
  uint8_t close;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
} fio_fd_data_s;

/* *****************************************************************************
State machine types and data
***************************************************************************** */

struct fio___data_s {
  /* last `poll` cycle */
  struct timespec last_cycle;
  /* connection capacity */
  uint32_t capa;
  /* The highest active fd with a protocol object */
  uint32_t max_open_fd;
  /* current process ID */
  pid_t pid;
  /* parent process ID */
  pid_t parent;
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
  /* master process flag */
  uint8_t is_master;
  /* polling and global lock */
  fio_lock_i lock;
  /* fd_data array */
  fio_fd_data_s info[];
} * fio_data;

/* *****************************************************************************
Section Start Marker









                     Internal / External Helpers / Macros










***************************************************************************** */

/* *****************************************************************************
Helpers (declarations)
***************************************************************************** */
FIO_IFUNC int fio_defer_urgent(void (*task)(void *, void *),
                               void *udata1,
                               void *udata2);

/* *****************************************************************************
Forking / Cleanup (declarations)
***************************************************************************** */
static fio_lock_i fio_fork_lock;
FIO_IFUNC void fio_defer_on_fork(void);
FIO_IFUNC void fio_state_callback_on_fork(void);
FIO_IFUNC void fio_state_callback_clear_all(void);
FIO_IFUNC void fio_state_callback_on_fork(void);
/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void mock_on_ev(intptr_t uuid, fio_protocol_s *protocol);
static void mock_on_data(intptr_t uuid, fio_protocol_s *protocol);
static uint8_t mock_on_shutdown(intptr_t uuid, fio_protocol_s *protocol);
static uint8_t mock_on_shutdown_eternal(intptr_t uuid,
                                        fio_protocol_s *protocol);
static void mock_ping(intptr_t uuid, fio_protocol_s *protocol);
static void mock_ping2(intptr_t uuid, fio_protocol_s *protocol);

static void deferred_on_close(void *uuid_, void *pr);
static void deferred_on_shutdown(void *uuid, void *arg2);
static void deferred_on_ready(void *uuid, void *arg2);
static void deferred_on_data(void *uuid, void *arg2);
static void deferred_on_timeout(void *uuid, void *arg2);

/* *****************************************************************************
Polling / Signals (declarations)
***************************************************************************** */
static inline void fio_force_close_in_poll(intptr_t uuid);
FIO_IFUNC void fio_poll_close(void);
FIO_IFUNC void fio_poll_init(void);
FIO_IFUNC void fio_pubsub_init(void);
FIO_IFUNC void fio_poll_add_read(intptr_t fd);
FIO_IFUNC void fio_poll_add_write(intptr_t fd);
FIO_IFUNC void fio_poll_add(intptr_t fd);
FIO_IFUNC void fio_poll_remove_fd(intptr_t fd);
FIO_SFUNC size_t fio_poll(void);
static void fio_signal_handler_setup(void);

/* *****************************************************************************
Protocol access (declerations)
***************************************************************************** */
FIO_IFUNC fio_protocol_s *protocol_try_lock(intptr_t uuid, uint8_t sub);
FIO_IFUNC uint8_t protocol_is_locked(fio_protocol_s *pr, uint8_t sub);
FIO_IFUNC void protocol_unlock(fio_protocol_s *pr, uint8_t sub);
inline static void protocol_validate(fio_protocol_s *protocol);

/* *****************************************************************************
URL to Address
***************************************************************************** */

typedef struct {
  char *address;
  char *port;
  uint16_t flags;
} fio___addr_info_s;

/** Converts a URL to an address format for getaddrinfo and fio_sock_open. */
FIO_SFUNC fio___addr_info_s *fio___addr_info_new(const char *url);
/** Frees an allocated address information object. */
FIO_SFUNC void fio___addr_info_free(fio___addr_info_s *addr);

/* *****************************************************************************
Helper access macors and functions
***************************************************************************** */

#define fd_data(fd)     (fio_data->info[(uintptr_t)(fd)])
#define uuid_data(uuid) fd_data(fio_uuid2fd((uuid)))
#define fd2uuid(fd)                                                            \
  ((intptr_t)((((uintptr_t)(fd)) << 8) | fd_data((fd)).counter))

/** returns 1 if the UUID is valid and 0 if it isn't. */
#define uuid_is_valid(uuid)                                                    \
  ((intptr_t)(uuid) >= 0 &&                                                    \
   ((uint32_t)fio_uuid2fd((uuid))) < fio_data->capa &&                         \
   ((uintptr_t)(uuid)&0xFF) == uuid_data((uuid)).counter)

/** tests UUID after a previous `uuid_is_valid` evaluated to 1 to the uuid . */
#define uuid_is_still_valid(uuid)                                              \
  (((uintptr_t)(uuid)&0xFF) == uuid_data((uuid)).counter)

FIO_IFUNC void fio_mark_time(void) { fio_data->last_cycle = fio_time_real(); }

#define touchfd(fd)                                                            \
  (fd_data((fd)).active = fio_time2milli(fio_data->last_cycle))

/* *****************************************************************************
Connection Locking Helpers
***************************************************************************** */

FIO_IFUNC int uuid_try_lock(intptr_t uuid, uint8_t lock_group) {
  if (!uuid_is_valid(uuid))
    goto error;
  if (fio_trylock_group(&uuid_data(uuid).lock, lock_group))
    goto would_block;
  if (!uuid_is_still_valid(uuid))
    goto error_locked;
  return 0;
would_block:
  errno = EWOULDBLOCK;
  return 1;
error_locked:
  fio_unlock(&uuid_data(uuid).lock);
error:
  errno = EBADF;
  return 2;
}

FIO_IFUNC int uuid_lock(intptr_t uuid, uint8_t lock_group) {
  int r;
  while ((r = uuid_try_lock(uuid, lock_group)) == 1) {
    FIO_THREAD_RESCHEDULE();
  }
  return r;
}
#define UUID_TRYLOCK(uuid, lock_group, reschedule_lable, invalid_uuid_goto)    \
  do {                                                                         \
    switch (uuid_try_lock(uuid, lock_group)) {                                 \
    case 1:                                                                    \
      goto reschedule_lable;                                                   \
    case 2:                                                                    \
      goto invalid_uuid_goto;                                                  \
    }                                                                          \
  } while (0)

#define UUID_LOCK(uuid, lock_group, invalid_uuid_goto)                         \
  do {                                                                         \
    switch (uuid_lock(uuid, lock_group)) {                                     \
    case 0:                                                                    \
      break;                                                                   \
    case 2:                                                                    \
      goto invalid_uuid_goto;                                                  \
    }                                                                          \
  } while (0)

#define UUID_UNLOCK(uuid, lock_group)                                          \
  do {                                                                         \
    fio_unlock_group(&uuid_data(uuid).lock, lock_group);                       \
  } while (0)

#define FD_UNLOCK(fd, lock_group)                                              \
  do {                                                                         \
    fio_unlock_group(&fd_data(uuid).lock, lock_group);                         \
  } while (0)

/* *****************************************************************************
Protocol Locking Helpers
***************************************************************************** */

/* used for protocol locking and atomic data. */
typedef struct {
  fio_lock_i lock;
} protocol_metadata_s;

/* used for accessing the protocol locking in a safe byte aligned way. */
union protocol_metadata_union_u {
  uintptr_t opaque;
  protocol_metadata_s meta;
};

/* Macro for accessing the protocol locking / metadata. */
#define prt_meta(prt) (((union protocol_metadata_union_u *)(&(prt)->rsv))->meta)

/** locks a connection's protocol returns a pointer that need to be unlocked. */
FIO_IFUNC fio_protocol_s *protocol_try_lock(intptr_t uuid, uint8_t sub) {
  uint8_t attempt;
  UUID_TRYLOCK(uuid, FIO_UUID_LOCK_PROTOCOL, would_block, invalid);
  fio_protocol_s *pr = uuid_data(uuid).protocol;
  if (!pr)
    goto invalid_locked;
  attempt = fio_trylock_sublock(&prt_meta(pr).lock, sub);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_PROTOCOL);
  if (attempt)
    goto would_block;
  errno = 0;
  return pr;
would_block:
  errno = EWOULDBLOCK;
  return NULL;
invalid_locked:
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_PROTOCOL);
  errno = EBADF;
invalid:
  return NULL;
}
/** See `fio_protocol_try_lock` for details. */
FIO_IFUNC void protocol_unlock(fio_protocol_s *pr, uint8_t sub) {
  fio_unlock_sublock(&prt_meta(pr).lock, sub);
}

FIO_IFUNC uint8_t protocol_is_locked(fio_protocol_s *pr, uint8_t sub) {
  return fio_is_sublocked(&prt_meta(pr).lock, sub);
}

/* *****************************************************************************
Reset connection data
***************************************************************************** */

/* resets connection data, marking it as either open or closed. */
FIO_IFUNC int fio_clear_fd(intptr_t fd, uint8_t is_open) {
  fio_stream_s stream;
  fio_protocol_s *protocol;
  fio_rw_hook_s *rw_hooks;
  void *rw_udata;
  fio___uuid_env_s env;
  uint8_t active = fio_data->active;
  fio_lock_group(&(fd_data(fd).lock), FIO_UUID_LOCK_FULL);
  intptr_t uuid = fd2uuid(fd);
  env = fd_data(fd).env;
  stream = fd_data(fd).stream;
  protocol = fd_data(fd).protocol;
  rw_hooks = fd_data(fd).rw_hooks;
  rw_udata = fd_data(fd).rw_udata;
  fd_data(fd) = (fio_fd_data_s){
      .rw_hooks = (fio_rw_hook_s *)&FIO_DEFAULT_RW_HOOKS,
      .open = is_open,
      .lock = fd_data(fd).lock,
      .counter = fd_data(fd).counter + (!is_open),
      .stream = FIO_STREAM_INIT(fd_data(fd).stream),
      .env = FIO_MAP_INIT,
  };
  fio_unlock_full(&(fd_data(fd).lock));
  if (rw_hooks && rw_hooks->cleanup)
    rw_hooks->cleanup(rw_udata);
  fio_stream_destroy(&stream);
  fio___uuid_env_destroy(&env);
  if (protocol && protocol->on_close) {
    fio_defer(deferred_on_close, (void *)uuid, protocol);
  }
  if (fio_data->max_open_fd < (uint32_t)fd && is_open) {
    fio_data->max_open_fd = fd;
  } else if (active) {
    while (fio_data->max_open_fd && !fd_data(fio_data->max_open_fd).open)
      --fio_data->max_open_fd;
  } else {
    while (fio_data->max_open_fd && (!fd_data(fio_data->max_open_fd).open ||
                                     !fd_data(fio_data->max_open_fd).timeout))
      --fio_data->max_open_fd;
  }

  FIO_LOG_DEBUG("FD %d re-initialized (state: %p-%s).",
                (int)fd,
                (void *)fd2uuid(fd),
                (is_open ? "open" : "closed"));
  return 0;
}

/* *****************************************************************************
Copy address to string
***************************************************************************** */

static void fio_tcp_addr_cpy(int fd, int family, struct sockaddr *addrinfo) {
  const char *result =
      inet_ntop(family,
                family == AF_INET
                    ? (void *)&(((struct sockaddr_in *)addrinfo)->sin_addr)
                    : (void *)&(((struct sockaddr_in6 *)addrinfo)->sin6_addr),
                (char *)fd_data(fd).addr,
                sizeof(fd_data(fd).addr));
  if (result) {
    fd_data(fd).addr_len = strlen((char *)fd_data(fd).addr);
  } else {
    fd_data(fd).addr_len = 0;
    fd_data(fd).addr[0] = 0;
  }
}
/* *****************************************************************************
Section Start Marker










                       Polling State Machine - epoll











***************************************************************************** */
#ifndef H_FACIL_IO_H   /* Development inclusion - ignore line */
#include "0011 base.c" /* Development inclusion - ignore line */
#endif                 /* Development inclusion - ignore line */

static inline void fio_force_close_in_poll(intptr_t uuid) {
  fio_force_close(uuid);
}

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

/* *****************************************************************************
Start Section
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

FIO_IFUNC void fio_poll_close(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_IFUNC void fio_poll_init(void) {
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

FIO_IFUNC int fio___poll_add2(int fd, uint32_t events, int ep_fd) {
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

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  fio___poll_add2(fd,
                  (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[1]);
  return;
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  fio___poll_add2(fd,
                  (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[2]);
  return;
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  if (fio___poll_add2(fd,
                      (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                      evio_fd[1]) == -1)
    return;
  fio___poll_add2(fd,
                  (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[2]);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN), .data.fd = fd};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

FIO_SFUNC size_t fio_poll(void) {
  int timeout_millisec = fio___timer_calc_first_interval();
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
            fio_defer_urgent(deferred_on_ready,
                             (void *)fd2uuid(events[i].data.fd),
                             NULL);
          }
          if (events[i].events & EPOLLIN)
            fio_defer(deferred_on_data,
                      (void *)fd2uuid(events[i].data.fd),
                      NULL);
        }
      } // end for loop
      total += active_count;
    }
  }
  return total;
}

/* *****************************************************************************
Section Start Marker










                       Polling State Machine - kqueue











***************************************************************************** */
#elif FIO_ENGINE_KQUEUE
#include <sys/event.h>

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "kqueue"; }

static int evio_fd = -1;

FIO_IFUNC void fio_poll_close(void) { close(evio_fd); }

FIO_IFUNC void fio_poll_init(void) {
  fio_poll_close();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent,
         fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent,
         fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  struct kevent chevent[2];
  EV_SET(chevent,
         fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  EV_SET(chevent + 1,
         fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, 2, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  FIO_LOG_WARNING("fio_poll_remove_fd called for %d", (int)fd);
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

FIO_SFUNC size_t fio_poll(void) {
  if (evio_fd < 0)
    return -1;
  int timeout_millisec = fio___timer_calc_first_interval();
  struct kevent events[FIO_POLL_MAX_EVENTS] = {{0}};

  const struct timespec timeout = {
      .tv_sec = (timeout_millisec / 1024),
      .tv_nsec = ((timeout_millisec & (1023UL)) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, FIO_POLL_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_WRITE) {
        fio_defer_urgent(deferred_on_ready,
                         ((void *)fd2uuid(events[i].udata)),
                         NULL);
      } else if (events[i].filter == EVFILT_READ) {
        fio_defer(deferred_on_data, (void *)fd2uuid(events[i].udata), NULL);
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

/* *****************************************************************************
Section Start Marker










                       Polling State Machine - poll











***************************************************************************** */
#elif FIO_ENGINE_POLL

#define FIO_POLL
#define FIO_POLL_HAS_UDATA_COLLECTION 0
#include "fio-stl.h"

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "poll"; }

FIO_SFUNC void fio___poll_ev_wrap_data(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_defer(deferred_on_data, (void *)uuid, udata);
}
FIO_SFUNC void fio___poll_ev_wrap_ready(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_defer_urgent(deferred_on_ready, (void *)uuid, udata);
}
FIO_SFUNC void fio___poll_ev_wrap_close(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_force_close_in_poll(uuid);
  (void)udata;
}

static fio_poll_s fio___poll_data = FIO_POLL_INIT(fio___poll_ev_wrap_data,
                                                  fio___poll_ev_wrap_ready,
                                                  fio___poll_ev_wrap_close);
FIO_IFUNC void fio_poll_close(void) { fio_poll_destroy(&fio___poll_data); }

FIO_IFUNC void fio_poll_init(void) {}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  fio_poll_forget(&fio___poll_data, fd);
}

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLIN);
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLOUT);
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLIN | POLLOUT);
}

/** returns non-zero if events were scheduled, 0 if idle */
static size_t fio_poll(void) {
  int timeout_millisec = fio___timer_calc_first_interval();
  return (size_t)(fio_poll_review(&fio___poll_data, timeout_millisec) > 0);
}

#endif /* FIO_ENGINE_POLL */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0800 threads.h
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
***************************************************************************** */

/* only call within child process! */
void fio_on_fork(void) {
  if (fio_data) {
    fio_data->lock = FIO_LOCK_INIT;
    fio_data->is_master = 0;
    fio_data->is_worker = 1;
    fio_data->pid = getpid();
  }
  fio_unlock(&fio_fork_lock);
  fio_defer_on_fork();
  fio_malloc_after_fork();
  fio_state_callback_on_fork();
  fio_poll_init();

  /* don't pass open connections belonging to the parent onto the child. */
  if (fio_data) {
    const size_t limit = fio_data->capa;
    for (size_t i = 0; i < limit; ++i) {
      fd_data(i).lock = FIO_LOCK_INIT;
      fd_data(i).lock = FIO_LOCK_INIT;
      if (fd_data(i).protocol) {
        fd_data(i).protocol->rsv = 0;
      }
      if (fd_data(i).protocol || fd_data(i).open) {
        fio_force_close(fd2uuid(i));
      }
    }
  }
}

/**
OVERRIDE THIS to replace the default `fork` implementation.

Behaves like the system's `fork`.... bt calls the correct state callbacks.
*/
int fio_fork(void) {
  int child;
  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  child = fork();
  switch (child + 1) {
  case 0:
    /* error code -1 */
    return child;
  case 1:
    /* child = 0, this is the child process */
    fio_on_fork();
    fio_state_callback_force(FIO_CALL_AFTER_FORK);
    fio_state_callback_force(FIO_CALL_IN_CHILD);
    fio_defer_perform();
    return child;
  }
  /* child == number, this is the master process */
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_state_callback_force(FIO_CALL_IN_MASTER);
  fio_defer_perform();
  return child;
}

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
void *fio_thread_new(void *(*thread_func)(void *), void *arg) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    pthread_t t;
    if (!pthread_create(&t, NULL, thread_func, arg))
      return (void *)t;
    return NULL;
  }
  pthread_t *t = malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  if (!pthread_create(t, NULL, thread_func, arg))
    return t;
  free(t);
  t = NULL;
  return t;
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
void fio_thread_free(void *p_thr) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    pthread_detach(((pthread_t)p_thr));
    return;
  }
  pthread_detach(((pthread_t *)p_thr)[0]);
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
int fio_thread_join(void *p_thr) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    return pthread_join(((pthread_t)p_thr), NULL);
  }
  int r = pthread_join(((pthread_t *)p_thr)[0], NULL);
  free(p_thr);
  return r;
}
/* ************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* ************************************************************************** */

/* *****************************************************************************
IO reactor's work
***************************************************************************** */
static __thread volatile uint8_t fio___was_idle = 0;

FIO_SFUNC void fio___review_timeouts(void *at, void *arg2) {
  uintptr_t fd = (uintptr_t)at;
  const size_t now = fio_time2milli(fio_data->last_cycle);
  size_t limit = 10;
  while (fd <= fio_data->max_open_fd && --limit) {
    const size_t lim =
        fd_data(fd).timeout ? ((size_t)fd_data(fd).timeout * 1000) : 255000U;
    if (fd_data(fd).open && lim + (size_t)fd_data(fd).active <= now) {
      const intptr_t uuid = fd2uuid(fd);
      fio_defer(deferred_on_timeout, (void *)uuid, NULL);
    }
    ++fd;
  }
  if (fd <= fio_data->max_open_fd) {
    fio_defer(fio___review_timeouts, (void *)(fd - 1), arg2);
    return;
  }
  fio_data->need_review = 1;
}

FIO_SFUNC void fio___perform_tasks_and_poll(void) {
  static long last_sec = 0;
  uint8_t flag = fio_signal_review() || fio_poll();
  fio_mark_time();
  if (flag) {
    fio___was_idle = 0;
  } else if (!fio___was_idle) {
    fio_state_callback_force(FIO_CALL_ON_IDLE);
    fio___was_idle = 1;
  }
  if (fio_data->need_review && last_sec != fio_data->last_cycle.tv_sec) {
    last_sec = fio_data->last_cycle.tv_sec;
    fio_data->need_review = 0;
    fio_defer(fio___review_timeouts, NULL, NULL);
  }
  fio_defer_perform();
}

FIO_SFUNC void *fio___worker_thread(void *arg) {
  (void)arg;
  /* on avarage, thread wakeup should be staggered / spread evenly */
  const size_t wait_len = 10000 * fio_data->threads;
  while (fio_data->active) {
    fio___perform_tasks_and_poll();
    if (fio_defer_has_queue())
      continue;
    FIO_THREAD_WAIT(wait_len);
  }
  return NULL;
}

FIO_SFUNC void fio___worker_cycle(void) {
  fio_state_callback_force(FIO_CALL_ON_START);
  void **threads = NULL;
  if (fio_data->threads > 1) {
    /* spawn threads */
    threads = malloc(sizeof(void *) * fio_data->threads);
    for (size_t i = 0; i + 1 < fio_data->threads; ++i) {
      threads[i + 1] = NULL;
      threads[i] = fio_thread_new(fio___worker_thread, NULL);
      if (!threads[i]) {
        FIO_LOG_ERROR("couldn't spawn thread (%d): %s", errno, strerror(errno));
        if (fio_data->workers)
          kill(0, SIGINT);
        goto done;
      }
    }
  }
  /* this is the polling thread - only one..? */
  while (fio_data->active) {
    fio___perform_tasks_and_poll();
  }
  FIO_LOG_INFO("(%d) starting shutdown sequence", fio_getpid());
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  for (size_t i = 0; i <= fio_data->max_open_fd; ++i) {
    /* test for and call shutdown callback + possible close callback */
    if (fd_data(i).open || fd_data(i).protocol) {
      fio_defer(deferred_on_shutdown, (void *)fd2uuid(i), NULL);
    }
  }
  if (fio_data->max_open_fd + 1 < fio_data->capa)
    fio_clear_fd(fio_data->max_open_fd + 1, 0);
  while (fio_data->max_open_fd) {
    fio___perform_tasks_and_poll();
  }
  /* test for possible lingering connections (on_shutdown == 255) */
  for (size_t i = 0; i < fio_data->capa; ++i) {
    if (fd_data(i).open || fd_data(i).protocol) {
      fio_close(fd2uuid(i));
    }
  }
  fio_defer_perform();

done:
  if (threads) {
    /* join threads */
    fio_data->active = 0;
    for (size_t i = 0; threads[i]; ++i) {
      fio_thread_join(threads[i]);
    }
    free(threads);
  }

  fio_state_callback_force(FIO_CALL_ON_FINISH);
  fio_defer_perform();
}

/* *****************************************************************************
Sentinal tasks for worker processes
***************************************************************************** */

static void fio_sentinel_task(void *arg1, void *arg2);
static void *fio_sentinel_worker_thread(void *arg) {
#if !DEBUG
restart:
#endif
  errno = 0;
  pid_t child = fio_fork();
  /* release fork lock. */
  fio_unlock(&fio_fork_lock);
  if (child == -1) {
    FIO_LOG_FATAL("couldn't spawn worker.");
    perror("\n           errno");
    fio_stop();
    if (!fio_is_master())
      kill(fio_master_pid(), SIGINT);
    return NULL;
  } else if (child) {
    int status;
    waitpid(child, &status, 0);
#if DEBUG
    if (fio_data->active) { /* !WIFEXITED(status) || WEXITSTATUS(status) */
      if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        FIO_LOG_FATAL("Child worker (%d) crashed. Stopping services.", child);
        fio_lock(&fio_fork_lock);
        fio_state_callback_force(FIO_CALL_ON_CHILD_CRUSH);
        fio_unlock(&fio_fork_lock);
      } else {
        FIO_LOG_FATAL("Child worker (%d) shutdown. Stopping services.", child);
      }
      kill(0, SIGINT);
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
      goto restart;
    }
#endif
  } else {
    FIO_LOG_INFO("worker process %d spawned.", fio_getpid());
    FIO_ASSERT_DEBUG(!fio_is_master(), "A worker process must NOT be a master");
    fio___worker_cycle();
    exit(0);
  }
  return NULL;
  (void)arg;
}

static void fio_sentinel_task(void *arg1, void *arg2) {
  if (!fio_data->active)
    return;
  fio_lock(&fio_fork_lock); /* will wait for worker thread to release lock. */
  void *thrd =
      fio_thread_new(fio_sentinel_worker_thread, (void *)&fio_fork_lock);
  fio_thread_free(thrd);
  fio_lock(&fio_fork_lock);   /* will wait for worker thread to release lock. */
  fio_unlock(&fio_fork_lock); /* release lock for next fork. */
  (void)arg1;
  (void)arg2;
}

/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */

/* sublime text marker */
void fio_start____(void);
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
  fio_data->threads = args.threads;
  fio_data->workers = args.workers;
  fio_data->need_review = 1;
  fio_data->active = 1;
  fio_state_callback_force(FIO_CALL_PRE_START);
  if (fio_data->active) {
    FIO_LOG_INFO("\n\t Starting facil.io in %s mode."
                 "\n\t Engine:     %s"
                 "\n\t Worker(s):  %d"
                 "\n\t Threads(s): %d"
                 "\n\t Capacity:   %zu (max fd)"
                 "\n\t Root PID:   %zu",
                 fio_data->workers ? "cluster" : "single processes",
                 fio_engine(),
                 fio_data->workers,
                 fio_data->threads,
                 (size_t)fio_data->capa,
                 (size_t)fio_data->parent);
#if HAVE_OPENSSL
    FIO_LOG_INFO("linked to OpenSSL %s", OpenSSL_version(0));
#endif
    if (args.workers) {
      for (int i = 0; i < args.workers; ++i) {
        fio_sentinel_task(NULL, NULL);
      }
      /* this starts with the worker flag off */
      fio___worker_cycle();
    } else {
      fio_data->is_worker = 1;
      fio_data->is_master = 1;
      fio___worker_cycle();
    }
  }
  fio_signal_handler_reset();
}

/**
 * Attempts to stop the facil.io application. This only works within the Root
 * process. A worker process will simply respawn itself.
 */
void fio_stop(void) {
  if (fio_data)
    fio_data->active = 0;
}

/**
 * Returns the last time the server reviewed any pending IO events.
 */
struct timespec fio_last_tick(void) {
  if (!fio_data)
    return fio_time_real();
  return fio_data->last_cycle;
}

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void); /* impplemented in the pollng logic */

/* *****************************************************************************
Calculate Concurrency
***************************************************************************** */

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
 * processes values passed to `fio_start`.
 *
 * The data in the pointers will be overwritten with the result.
 */
void fio_expected_concurrency(int16_t *restrict threads,
                              int16_t *restrict processes) {
  if (!threads || !processes)
    return;
  if (!*threads && !*processes) {
    /* both options set to 0 - default to master_worker with X cores threads */
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
            (size_t)cpu_count,
            (size_t)FIO_CPU_CORES_LIMIT,
            (size_t)cpu_count);
        print_cores_warning = 0;
      }
      cpu_count = FIO_CPU_CORES_LIMIT;
    }
#endif
    *threads = (int16_t)cpu_count;
    if (cpu_count > 3) {
      /* leave a thread for kernel tasks */
      --(*threads);
    }
  } else if (*threads < 0 || *processes < 0) {
    /* Set any option that is less than 0 be equal to cores/value */
    /* Set any option equal to 0 be equal to the other option in value */
    ssize_t cpu_count = fio_detect_cpu_cores();
    size_t thread_cpu_adjust = (*threads <= 0 ? 1 : 0);
    size_t worker_cpu_adjust = (*processes < 0 ? 1 : 0);

    if (cpu_count > 0) {
      int16_t tmp = 0;
      if (*threads < 0) {
        tmp = (int16_t)(cpu_count / (*threads * -1));
        if (!tmp)
          tmp = 1;
      } else if (*threads == 0) {
        tmp = -1 * *processes;
        thread_cpu_adjust = 0;
      } else
        tmp = *threads;
      if (*processes < 0) {
        *processes = (int16_t)(cpu_count / (*processes * -1));
        if (*processes == 0) {
          *processes = 1;
          worker_cpu_adjust = 0;
        }
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

  /* make sure we have at least one thread */
  if (*threads <= 0)
    *threads = 1;
}

/* *****************************************************************************
Concurrency State
***************************************************************************** */

/**
 * Returns the number of worker processes if facil.io is running.
 *
 * (1 is returned when in single process mode, otherwise the number of workers)
 */
int16_t fio_is_running(void) {
  if (!fio_data || !fio_data->active)
    return 0;
  return fio_data->workers;
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
int fio_is_worker(void) {
  if (!fio_data)
    return 0;
  return fio_data->is_worker;
}

/**
 * Returns 1 if the current process is the master (root) process.
 *
 * Otherwise returns 0.
 */
int fio_is_master(void) {
  if (!fio_data)
    return 0;
  return fio_data->is_master;
}

/** Returns facil.io's parent (root) process pid. */
pid_t fio_master_pid(void) {
  if (!fio_data)
    return getpid();
  return fio_data->parent;
}

/** Returns the current process pid. */
pid_t fio_getpid(void) {
  if (!fio_data)
    return getpid();
  return fio_data->pid;
}

/* *****************************************************************************







                    Handle signals and child reaping







***************************************************************************** */

static volatile uint8_t fio___signal_children_flag = 0;

/*
 * Zombie Reaping
 * With thanks to Dr Graham D Shaw.
 * http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
 */
static void reap_child_handler(int sig, void *ignr_) {
  (void)(sig);
  (void)ignr_;
  int old_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = old_errno;
}

/**
 * Initializes zombie reaping for the process. Call before `fio_start` to enable
 * global zombie reaping.
 */
void fio_reap_children(void) {
  if (fio_signal_monitor(SIGCHLD, reap_child_handler, NULL)) {
    perror("Child reaping initialization failed");
    kill(0, SIGINT);
    exit(errno);
  }
}

#if !FIO_DISABLE_HOT_RESTART
static void fio___sig_handler_hot_restart(int sig, void *flag);
static int reset_monitoring_task(void *a_, void *b_) {
  (void)a_;
  (void)b_;
  fio_signal_monitor(SIGUSR1, fio___sig_handler_hot_restart, NULL);
  return 0;
}

static void fio___sig_handler_hot_restart(int sig, void *flag) {
  (void)(sig);
  fio___signal_children_flag = 1;
  if (!fio_is_master())
    fio_stop();
  else if (!flag) {
    fio_signal_monitor(SIGUSR1, fio___sig_handler_hot_restart, (void *)1);
    fio_run_every(100, 1, reset_monitoring_task, NULL, NULL, NULL);
    kill(0, SIGUSR1);
  }
}
#endif

static void fio___sig_handler_stop(int sig, void *ignr_) {
  (void)(sig);
  (void)ignr_;
  if (!fio_data->active) {
    if (fio_data->max_open_fd) {
      FIO_LOG_WARNING("Server exit was signaled (again?) while server is "
                      "shutting down.\n          Pushing things...");
      fio_data->max_open_fd = 0;
    } else {
      FIO_LOG_FATAL("Server exit was signaled while server not running.");
      exit(-1);
    }
  }
#if !FIO_DISABLE_HOT_RESTART
  kill(0, SIGUSR1);
#endif
  fio_stop();
}

/* setup handling for the SIGUSR1, SIGPIPE, SIGINT and SIGTERM signals. */
static void fio_signal_handler_setup(void) {
  /* setup signal handling */
  fio_signal_monitor(SIGINT, fio___sig_handler_stop, NULL);
  fio_signal_monitor(SIGTERM, fio___sig_handler_stop, NULL);
  fio_signal_monitor(SIGQUIT, fio___sig_handler_stop, NULL);
  fio_signal_monitor(SIGPIPE, NULL, NULL);
#if !FIO_DISABLE_HOT_RESTART
  fio_signal_monitor(SIGUSR1, fio___sig_handler_hot_restart, NULL);
#endif
}

/**
 * Resets any existing signal handlers, restoring their state to before they
 * were set by facil.io.
 *
 * This stops both child reaping (`fio_reap_children`) and the default facil.io
 * signal handlers (i.e., CTRL-C).
 *
 * This function will be called automatically by facil.io whenever facil.io
 * stops.
 */
void fio_signal_handler_reset(void) {
  fio_signal_forget(SIGINT);
  fio_signal_forget(SIGTERM);
  fio_signal_forget(SIGQUIT);
  fio_signal_forget(SIGPIPE);
  fio_signal_forget(SIGCHLD);
#if !FIO_DISABLE_HOT_RESTART
  fio_signal_forget(SIGUSR1);
#endif
}

/* *****************************************************************************







        Startup / State Callbacks (fork, start up, idle, etc')







***************************************************************************** */

typedef struct {
  void (*func)(void *);
  void *arg;
} fio___state_task_s;

#define FIO_ATOMIC
#define FIO_OMAP_NAME          fio_state_tasks
#define FIO_MAP_TYPE           fio___state_task_s
#define FIO_MAP_TYPE_CMP(a, b) (a.func == b.func && a.arg == b.arg)
#include <fio-stl.h>

static fio_state_tasks_s fio_state_tasks_array[FIO_CALL_NEVER];
static fio_lock_i fio_state_tasks_array_lock[FIO_CALL_NEVER + 1];

FIO_IFUNC uint64_t fio___state_callback_hash(void (*func)(void *), void *arg) {
  uint64_t hash = (uint64_t)(uintptr_t)func + (uint64_t)(uintptr_t)arg;
  hash = fio_risky_ptr((void *)(uintptr_t)hash);
  return hash;
}

FIO_IFUNC void fio_state_callback_clear_all(void) {
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_callback_clear((callback_type_e)i);
  }
}

FIO_IFUNC void fio_state_callback_on_fork(void) {
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_array_lock[i] = FIO_LOCK_INIT;
  }
}

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e e,
                            void (*func)(void *),
                            void *arg) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  uint64_t hash = fio___state_callback_hash(func, arg);
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  fio_state_tasks_set(fio_state_tasks_array + (uintptr_t)e,
                      hash,
                      (fio___state_task_s){func, arg},
                      NULL);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
  if (e == FIO_CALL_ON_INITIALIZE &&
      fio_state_tasks_array_lock[FIO_CALL_NEVER]) {
    /* initialization tasks already performed, perform this without delay */
    func(arg);
  }
}

/** Removes a callback from the list of callbacks to be called for the event. */
int fio_state_callback_remove(callback_type_e e,
                              void (*func)(void *),
                              void *arg) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return -1;
  int ret;
  uint64_t hash = fio___state_callback_hash(func, arg);
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  ret = fio_state_tasks_remove(fio_state_tasks_array + (uintptr_t)e,
                               hash,
                               (fio___state_task_s){func, arg},
                               NULL);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
  return ret;
}

/** Clears all the existing callbacks for the event. */
void fio_state_callback_clear(callback_type_e e) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  fio_state_tasks_destroy(fio_state_tasks_array + (uintptr_t)e);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
}

/**
 * Forces all the existing callbacks to run, as if the event occurred.
 *
 * Callbacks are called from last to first (last callback executes first).
 *
 * During an event, changes to the callback list are ignored (callbacks can't
 * remove other callbacks for the same event).
 */
void fio_state_callback_force(callback_type_e e) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  fio___state_task_s *ary = NULL;
  size_t len = 0;

  /* copy task queue */
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  if (fio_state_tasks_array[e].w) {
    ary = fio_malloc(sizeof(*ary) * fio_state_tasks_array[e].w);
    FIO_ASSERT_ALLOC(ary);
    FIO_MAP_EACH(fio_state_tasks, (fio_state_tasks_array + e), pos) {
      if (!pos->hash || !pos->obj.func)
        continue;
      ary[len++] = pos->obj;
    }
  }
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);

  /* perform copied tasks */
  if (e <= FIO_CALL_ON_IDLE) {
    /* perform tasks in order */
    for (size_t i = 0; i < len; ++i) {
      ary[i].func(ary[i].arg);
    }
  } else {
    /* perform tasks in reverse */
    while (len--) {
      ary[len].func(ary[len].arg);
    }
  }

  /* cleanup */
  fio_free(ary);
}

/* *****************************************************************************
Test state callbacks
***************************************************************************** */
#ifdef TEST

/* State callback test task */
FIO_SFUNC void fio___test__state_callbacks__task(void *udata) {
  struct {
    uint64_t num;
    uint8_t bit;
    uint8_t rev;
  } *p = udata;
  uint64_t mask = (((uint64_t)1ULL << p->bit) - 1);
  if (!p->rev)
    mask = ~mask;
  FIO_ASSERT(!(p->num & mask),
             "fio_state_tasks order error on bit %d (%p | %p) %s",
             (int)p->bit,
             (void *)(uintptr_t)p->num,
             (void *)(uintptr_t)mask,
             (p->rev ? "reversed" : ""));
  p->num |= (uint64_t)1ULL << p->bit;
  p->bit += 1;
}

/* State callback tests */
FIO_SFUNC void FIO_NAME_TEST(io, state)(void) {
  fprintf(stderr, "* testing state callback performance and ordering.\n");
  fio_state_tasks_s fio_state_tasks_array_old[FIO_CALL_NEVER];
  /* store old state */
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_array_old[i] = fio_state_tasks_array[i];
    fio_state_tasks_array[i] = (fio_state_tasks_s)FIO_MAP_INIT;
  }
  struct {
    uint64_t num;
    uint8_t bit;
    uint8_t rev;
  } data = {0, 0, 0};
  /* state tests for build up tasks */
  for (size_t i = 0; i < 24; ++i) {
    fio_state_callback_add(FIO_CALL_ON_IDLE,
                           fio___test__state_callbacks__task,
                           &data);
  }
  fio_state_callback_force(FIO_CALL_ON_IDLE);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* state tests for clean up tasks */
  data.num = 0;
  data.bit = 0;
  data.rev = 1;
  for (size_t i = 0; i < 24; ++i) {
    fio_state_callback_add(FIO_CALL_ON_SHUTDOWN,
                           fio___test__state_callbacks__task,
                           &data);
  }
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* restore old state and cleanup */
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_destroy(fio_state_tasks_array + i);
    fio_state_tasks_array[i] = fio_state_tasks_array_old[i];
  }
}
#endif /* TEST */

/* *****************************************************************************







                              Event / Task scheduling







***************************************************************************** */

/* *****************************************************************************
Task Queues
***************************************************************************** */

static fio_queue_s fio___task_queue = FIO_QUEUE_STATIC_INIT(fio___task_queue);
static fio_queue_s fio___io_task_queue =
    FIO_QUEUE_STATIC_INIT(fio___io_task_queue);
static fio_timer_queue_s fio___timer_tasks = FIO_TIMER_QUEUE_INIT;

FIO_IFUNC void fio___defer_clear_io_queue(void) {
  while (fio_queue_count(&fio___io_task_queue)) {
    fio_queue_task_s t = fio_queue_pop(&fio___io_task_queue);
    if (!t.fn)
      continue;
    union {
      void (*t)(void *, void *);
      void (*io)(intptr_t, fio_protocol_s *, void *);
    } u = {.t = t.fn};
    u.io((uintptr_t)t.udata1, NULL, t.udata2);
  }
}

/** Returns the number of miliseconds until the next event, or FIO_POLL_TICK */
FIO_IFUNC size_t fio___timer_calc_first_interval(void) {
  if (fio_queue_count(&fio___task_queue) ||
      fio_queue_count(&fio___io_task_queue))
    return 0;
  struct timespec now_tm = fio_last_tick();
  const int64_t now = fio_time2milli(now_tm);
  const int64_t next = fio_timer_next_at(&fio___timer_tasks);
  if (next >= now + FIO_POLL_TICK || next == -1)
    return FIO_POLL_TICK;
  if (next <= now)
    return 0;
  return next - now;
}

FIO_IFUNC void fio_defer_on_fork(void) {
  fio___task_queue.lock = FIO_LOCK_INIT;
  fio___io_task_queue.lock = FIO_LOCK_INIT;
  fio___timer_tasks.lock = FIO_LOCK_INIT;
  fio___defer_clear_io_queue();
}

FIO_IFUNC int fio_defer_urgent(void (*task)(void *, void *),
                               void *udata1,
                               void *udata2) {
  return fio_queue_push_urgent(&fio___task_queue,
                               .fn = task,
                               .udata1 = udata1,
                               .udata2 = udata2);
}

/** The IO task internal wrapper */
FIO_IFUNC int fio___perform_io_task(void) {
  fio_queue_task_s t = fio_queue_pop(&fio___io_task_queue);
  if (!t.fn)
    return -1;
  union {
    void (*t)(void *, void *);
    void (*io)(intptr_t, fio_protocol_s *, void *);
  } u = {.t = t.fn};
  fio_protocol_s *pr = protocol_try_lock((intptr_t)t.udata1, FIO_PR_LOCK_DATA);
  if (pr) {
    u.io((intptr_t)t.udata1, pr, t.udata2);
    protocol_unlock(pr, FIO_PR_LOCK_DATA);
  } else if (errno == EWOULDBLOCK) {
    fio_queue_push(&fio___io_task_queue, u.t, t.udata1, t.udata2);
  } else {
    u.io((intptr_t)(-1), NULL, t.udata2);
  }
  return 0;
}

FIO_IFUNC int fio___perform_task(void) {
  return fio_queue_perform(&fio___task_queue);
}

/* *****************************************************************************
Task Queues - Public API
***************************************************************************** */

/**
 * Defers a task's execution.
 *
 * Tasks are functions of the type `void task(void *, void *)`, they return
 * nothing (void) and accept two opaque `void *` pointers, user-data 1
 * (`udata1`) and user-data 2 (`udata2`).
 *
 * Returns -1 or error, 0 on success.
 */
int fio_defer(void (*task)(void *, void *), void *udata1, void *udata2) {
  return fio_queue_push(&fio___task_queue,
                        .fn = task,
                        .udata1 = udata1,
                        .udata2 = udata2);
}

/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error occurs or the connection is closed before the task can run, the
 * task will be called with a NULL protocol pointer, for resource cleanup.
 */
int fio_defer_io_task(intptr_t uuid,
                      void (*task)(intptr_t uuid,
                                   fio_protocol_s *,
                                   void *udata),
                      void *udata) {
  union {
    void (*t)(void *, void *);
    void (*io)(intptr_t, fio_protocol_s *, void *);
  } u = {.io = task};
  if (fio_queue_push(&fio___io_task_queue,
                     .fn = u.t,
                     .udata1 = (void *)uuid,
                     .udata2 = udata))
    goto error;
  return 0;
error:
  task(uuid, NULL, udata);
  return -1;
}

/**
 * Creates a timer to run a task at the specified interval.
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to -1, task
 * will repeat forever.
 *
 * If `task` returns a non-zero value, it will stop repeating.
 *
 * The `on_finish` handler is always called (even on error).
 */
void fio_run_every(size_t milliseconds,
                   int32_t repetitions,
                   int (*task)(void *, void *),
                   void *udata1,
                   void *udata2,
                   void (*on_finish)(void *, void *)) {
  fio_timer_schedule(&fio___timer_tasks,
                     .fn = task,
                     .udata1 = udata1,
                     .udata2 = udata2,
                     .on_finish = on_finish,
                     .every = milliseconds,
                     .repetitions = repetitions,
                     .start_at = fio_time2milli(fio_data->last_cycle));
}

/**
 * Performs all deferred tasks.
 */
void fio_defer_perform(void) {
  while ((fio___perform_task() + fio___perform_io_task() + 1 >= 0) ||
         fio_timer_push2queue(&fio___task_queue,
                              &fio___timer_tasks,
                              fio_time2milli(fio_data->last_cycle))) {
  }
}

/** Returns true if there are deferred functions waiting for execution. */
int fio_defer_has_queue(void) {
  return fio_queue_count(&fio___task_queue) > 0 ||
         fio_queue_count(&fio___io_task_queue) > 0;
}

/* *****************************************************************************
Test
***************************************************************************** */
#ifdef TEST

typedef struct {
  size_t nulls;
  size_t with_value;
  intptr_t last_uuid;
} fio___test__defer_io_s;

/** defer IO test task */
static void fio___test__defer_io_task(intptr_t uuid,
                                      fio_protocol_s *pr,
                                      void *udata) {
  fio___test__defer_io_s *i = udata;
  if (pr)
    ++(i->with_value);
  else
    ++(i->nulls);
  if (uuid != -1)
    i->last_uuid = uuid;
}

/** defer IO test */
static void FIO_NAME_TEST(io, tasks)(void) {
  fprintf(stderr, "* testing defer (IO tasks only - main test in STL).\n");
  fio___test__defer_io_s expect = {0}, result = {0};
  for (size_t i = 0; i < fio_capa(); ++i) {
    fio_protocol_s *pr = fio_protocol_try_lock(fio_fd2uuid(i));
    if (pr) {
      expect.last_uuid = fio_fd2uuid(i);
      ++(expect.with_value);
    } else {
      ++(expect.nulls);
    }
    fio_defer_io_task(fio_fd2uuid(i), fio___test__defer_io_task, &result);
    if (pr)
      fio_protocol_unlock(pr);
  }
  while (!fio___perform_io_task())
    fprintf(stderr, ".");
  fprintf(stderr, "\n");
  FIO_ASSERT(expect.nulls == result.nulls,
             "fallbacks (NULL) don't match (%zu != %zu)",
             expect.nulls,
             result.nulls);
  FIO_ASSERT(expect.with_value == result.with_value,
             "valid count (non-NULL) doesn't match (%zu != %zu)",
             expect.with_value,
             result.with_value);
  FIO_ASSERT(expect.last_uuid == result.last_uuid,
             "last_uuid doesn't match (%p != %p)",
             (void *)expect.last_uuid,
             (void *)result.last_uuid);
}

#endif /* TEST */
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
  protocol->on_timeout = mock_ping;
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
    pr->on_timeout = mock_ping2;
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
  } else {
    uuid_data(arg).timeout = 8;
    pr->on_timeout = mock_ping;
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
  if (uuid_data(arg).protocol) {
    fio_defer(deferred_on_ready_usr, arg, NULL);
  }
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

static void deferred_on_timeout(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol || uuid_data(arg).close ||
      uuid_data(arg).active + 1000 > fio_time2milli(fio_data->last_cycle)) {
    return;
  }
  fio_protocol_s *pr = protocol_try_lock((intptr_t)arg, FIO_PR_LOCK_PING);
  if (pr) {
    pr->on_timeout((intptr_t)arg, pr);
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
    if (!protocol->on_timeout) {
      protocol->on_timeout = mock_ping;
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
  if (old_pr && old_pr != protocol) {
    /* protocol replacement */
    fio_defer(deferred_on_close, (void *)uuid, old_pr);
    if (!protocol) {
      /* hijacking */
      fio_poll_remove_fd(fio_uuid2fd(uuid));
      fio_poll_add_write(fio_uuid2fd(uuid));
    }
  } else if (!old_pr && protocol) {
    /* adding a new uuid to the reactor */
    fio_poll_add(fio_uuid2fd(uuid));
  }
  return 0;

error_invalid_uuid:
  FIO_LOG_DEBUG("fio_attach failed - invalid UUID? %p (%d)",
                (void *)uuid,
                uuid_is_valid(uuid));
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
    fio_defer(deferred_on_timeout, (void *)uuid, NULL);
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
  FIO_LOG_INFO("(%d) started listening at: %s", (int)fio_getpid(), p->addr);
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
  FIO_LOG_INFO("(%d) stopped listening at: %s", (int)fio_getpid(), p->addr);
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
  // if (!u.host.buf) // nothing to do, address MAY be NULL
  //   ;
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
              .on_timeout = FIO_PING_ETERNAL,
          },
      .on_open = args.on_open,
      .tls = args.tls,
      .addr_len = addr_len,
  };
  memcpy(p->addr, args.url, addr_len);
  p->addr[addr_len] = 0;
  if (!u.host.buf && !u.port.buf && u.path.buf) {
    /* unix socket */
    p->fd = fio_sock_open(u.path.buf,
                          NULL,
                          FIO_SOCK_SERVER | FIO_SOCK_UNIX | FIO_SOCK_NONBLOCK);
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
    if (args.is_master_only)
      fio_state_callback_add(FIO_CALL_PRE_START, fio___listen_start, p);
    else
      fio_state_callback_add(FIO_CALL_ON_START, fio___listen_start, p);
  } else {
    if ((args.is_master_only && fio_data->is_master) ||
        (!args.is_master_only && fio_data->is_worker)) {
      FIO_LOG_WARNING("fio_listen called while running (fio_start), this is "
                      "unsafe in multi-process implementations");
      fio___listen_start(p);
    } else {
      FIO_LOG_ERROR("fio_listen called while running (fio_start) resulted in "
                    "an attempt to open the socket in the wrrong process.");
      fio___listen_free(p);
      goto error;
    }
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

/* *****************************************************************************
Connection Object
***************************************************************************** */

struct fio_connect_s {
  fio_protocol_s pr;
  void (*on_open)(intptr_t uuid, void *udata);
  void (*on_finish)(void *udata);
  intptr_t uuid;
  pid_t owner;
  fio_tls_s *tls;
  void *udata;
  uint8_t timeout;
  uint8_t auto_reconnect;
  char url[];
};
FIO_SFUNC void fio___connect_on_close_after_open(void *pr);

FIO_SFUNC void fio___connect_free(void *c_) {
  fio_connect_s *c = c_;
  if (c->on_finish)
    c->on_finish(c->udata);
  if (c->tls)
    fio_tls_free(c->tls);
  (c->auto_reconnect ? free : fio_free)(c);
}

FIO_SFUNC void fio___connect_start(void *c_) {
  fio_connect_s *c = c_;
  if (!c || c->uuid != -1)
    return; /* don't double-connect */
  fio___addr_info_s *addr = fio___addr_info_new(c->url);
  if (!addr)
    goto error;
  c->uuid = fio_socket(addr->address,
                       addr->port,
                       addr->flags | FIO_SOCK_CLIENT | FIO_SOCK_NONBLOCK);
  fio___addr_info_free(addr);
  if (c->uuid == -1)
    goto error;
  fio_uuid_env_set(c->uuid,
                   .on_close = fio___connect_on_close_after_open,
                   .udata = c,
                   .name = {.buf = c->url, .len = strlen(c->url)},
                   .type = -1);
  fio_attach(c->uuid, &c->pr);
  if (c->tls)
    fio_tls_connect(c->uuid, c->tls, c);
  if (c->timeout)
    fio_timeout_set(c->uuid, c->timeout);
  FIO_LOG_DEBUG("(%d) connecting to %s", c->owner, c->url);
  return;
error:
  FIO_LOG_ERROR("(%d) connection to %s failed", c->owner, c->url);
  c->auto_reconnect = 0;
  fio___connect_on_close_after_open(&c->pr);
}

FIO_SFUNC int fio___connect_start_task(void *c_, void *ig_) {
  fio_connect_s *c = (fio_connect_s *)c_;
  if (!c)
    return -1;
  if (c->auto_reconnect && c->owner == fio_getpid())
    fio___connect_start(c_);
  return 0;
  (void)ig_;
}

FIO_SFUNC void fio___connect_on_ready(intptr_t uuid, fio_protocol_s *pr) {
  fio_connect_s *c = (fio_connect_s *)pr;
  fio_timeout_set(uuid, 0);
  c->on_open(uuid, c->udata);
}

FIO_SFUNC void fio___connect_on_close_after_open(void *pr) {
  fio_connect_s *c = (fio_connect_s *)pr;
  c->uuid = -1;
  if (c->auto_reconnect && c->owner == fio_getpid()) {
    FIO_LOG_DEBUG("(%d) lost connection to %s", c->owner, c->url);
    fio_run_every(FIO_RECONNECT_DELAY,
                  1,
                  fio___connect_start_task,
                  c,
                  NULL,
                  NULL);
  } else {
    fio_state_callback_remove(FIO_CALL_PRE_START, fio___connect_start, c);
    fio_state_callback_remove(FIO_CALL_AT_EXIT, fio___connect_free, c);
    fio___connect_free(c);
  }
}

FIO_SFUNC void fio___connect_on_timeout(intptr_t uuid, fio_protocol_s *pr_) {
  fio_close(uuid);
  (void)pr_;
}

/* *****************************************************************************
Connection API
***************************************************************************** */

/* Connecting as a client */
void fio_connect___(void); /* Sublime Test Marker */
/** Creates a client connection (in addition or instead of the server). */
fio_connect_s *fio_connect FIO_NOOP(struct fio_connect_args args) {
  fio_connect_s *c = NULL;
  size_t len;
  if (!args.url || !args.on_open || !(len = strlen(args.url)))
    goto arg_error;
  /* consider object lifetime before allocating */
  c = (args.auto_reconnect ? malloc : fio_malloc)(sizeof(*c) + len + 1);
  FIO_ASSERT_ALLOC(c);
  *c = (fio_connect_s){
      .pr =
          {
              .on_data = mock_on_ev,
              .on_ready = fio___connect_on_ready,
              .on_timeout = fio___connect_on_timeout,
          },
      .on_open = args.on_open,
      .on_finish = args.on_finish,
      .uuid = -1,
      .owner = fio_getpid(),
      .tls = args.tls,
      .udata = args.udata,
      .timeout = args.timeout,
      .auto_reconnect = args.auto_reconnect,
  };
  memcpy(c->url, args.url, len + 1);
  fio_state_callback_add(
      (args.is_master_only ? FIO_CALL_PRE_START : FIO_CALL_ON_START),
      fio___connect_start,
      c);
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio___connect_free, c);
  if (fio_data->active) {
    if ((args.is_master_only && fio_data->is_master) ||
        (!args.is_master_only && fio_data->is_worker))
      fio___connect_start(c);
    else {
      FIO_LOG_ERROR("fio_connect called while running, attempted to connect "
                    "using the wrong process (worker vs. master).");
    }
  }
  return c;
arg_error:
  if (args.on_finish)
    args.on_finish(args.udata);
  return c;
}

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
    p = fio_stream_pack_data((void *)o.data.buf,
                             o.len,
                             o.offset,
                             o.copy,
                             o.dealloc);
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
  UUID_LOCK(uuid,
            FIO_UUID_LOCK_RW_HOOKS_WRITE | FIO_UUID_LOCK_STREAM,
            some_error);

  fio_stream_read(&uuid_data(uuid).stream, &buf, &len);
  UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);

  fl = uuid_data(uuid).rw_hooks->flush(uuid, uuid_data(uuid).rw_udata);
  if (len)
    w = uuid_data(uuid).rw_hooks->write(uuid,
                                        uuid_data(uuid).rw_udata,
                                        buf,
                                        len);

  if (w && w > 0) {
    UUID_LOCK(uuid, FIO_UUID_LOCK_STREAM, some_error);
    fio_stream_advance(&uuid_data(uuid).stream, w);
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_STREAM);
  }

  UUID_UNLOCK(uuid, FIO_UUID_LOCK_RW_HOOKS_WRITE);
  if (fl > 0 || w > 0) {
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
  fio_uuid_env_obj_s obj = {.udata = args.udata, .on_close = args.on_close};
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
    old.on_close(old.udata);
  return;
invalid_uuid:
  fio_str_destroy(&key);
  if (args.on_close)
    args.on_close(args.udata);
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
    uint64_t hash = fio_risky_hash(args.name.buf,
                                   args.name.len,
                                   (uint64_t)uuid + args.type);
    fio_uuid_env_obj_s old = {0};
    fio_str_s key;
    fio_str_init_const(&key, args.name.buf, args.name.len);
    UUID_LOCK(uuid, FIO_UUID_LOCK_ENV, invalid_uuid);
    fio___uuid_env_remove(&uuid_data(uuid).env, hash, key, &old);
    UUID_UNLOCK(uuid, FIO_UUID_LOCK_ENV);
    if (old.on_close)
      old.on_close(old.udata);
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
  intptr_t uuid =
      fio_socket(NULL,
                 "8765",
                 FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(uuid != -1, "fio_uuid_env_test failed to create a socket!");
  fio_uuid_env_set(uuid,
                   .udata = &called,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 1);
  FIO_ASSERT(called == 0,
             "fio_uuid_env_set failed - on_close callback called too soon!");
  fio_uuid_env_set(uuid,
                   .udata = &removed,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 0);
  fio_uuid_env_set(uuid,
                   .udata = &overwritten,
                   .on_close = FIO_NAME_TEST(io, env_on_close),
                   .type = 0,
                   .name.buf = "abcdefghijklmnopqrstuvwxyz",
                   .name.len = 26);
  fio_uuid_env_set(uuid,
                   .udata = &overwritten,
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

/** Converts a URL to an address format for getaddrinfo and fio_sock_open. */
FIO_SFUNC fio___addr_info_s *fio___addr_info_new(const char *url) {
  fio___addr_info_s *i = NULL;
  size_t len = strlen(url);
  i = fio_malloc(sizeof(*i) + len + 1);
  FIO_ASSERT_ALLOC(i);
  char *s = (char *)(i + 1);
  memcpy(s, url, len + 1);
  fio_url_s u = fio_url_parse(s, len);
  if (u.host.buf || u.port.buf) {
    i->address = u.host.buf;
    i->port = u.port.buf;
    if (u.host.buf)
      u.host.buf[u.host.len] = 0;
    if (u.port.buf)
      u.port.buf[u.port.len] = 0;
    if (u.scheme.buf && (u.scheme.buf[0] | 32) == 'u' &&
        (u.scheme.buf[0] | 32) == 'd' && (u.scheme.buf[0] | 32) == 'p')
      i->flags = FIO_SOCK_UDP;
    else
      i->flags = FIO_SOCK_TCP;
  } else if (u.path.buf) {
    i->flags = FIO_SOCK_UNIX;
    i->address = u.path.buf + 1;
    u.path.buf[u.path.len] = 0;
    i->port = NULL;
  } else {
    fio_free(i);
    i = NULL;
  }
  return i;
}
/** Frees an allocated address information object. */
FIO_SFUNC void fio___addr_info_free(fio___addr_info_s *addr) { fio_free(addr); }

/**
 * `fio_fd2uuid` takes an existing file decriptor `fd` and returns it's active
 * `uuid`.
 *
 * If the file descriptor was closed directly (not using `fio_close`) or the
 * closure event hadn't been processed, a false positive is possible, in which
 * case the `uuid` might not be valid for long.
 *
 * Returns -1 on error. Returns a valid socket (non-random) UUID.
 */
intptr_t fio_fd2uuid(int fd) {
  if (fd < 0 || (uint32_t)fd >= fio_data->capa || !fd_data(fd).open)
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
 * - FIO_SOCKET_CLIENT - client mode (calls `connect`).
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
intptr_t fio_socket(const char *address,
                    const char *port,
                    fio_socket_flags_e flags) {
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
  intptr_t client = -1;
  struct sockaddr_in6 addrinfo[2]; /* grab a slice of stack (aligned) */
  socklen_t addrlen = sizeof(addrinfo);

  if (!uuid_is_valid(srv_uuid)) {
    FIO_LOG_ERROR("fio_accept called with an invalid UUID %p",
                  (void *)srv_uuid);
    return client;
  }

#ifdef SOCK_NONBLOCK
  client = accept4(fio_uuid2fd(srv_uuid),
                   (struct sockaddr *)addrinfo,
                   &addrlen,
                   SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (client <= 0)
    return client;
#else
  client = accept(fio_uuid2fd(srv_uuid), (struct sockaddr *)addrinfo, &addrlen);
  if (client <= 0)
    return -1;
  if (fio_sock_set_non_block(client) == -1) {
    close(client);
    FIO_LOG_ERROR("fio_accept couldn't set fd %d to non-bloccking state: %s",
                  client,
                  strerror(errno));
    client = -1;
    return client;
  }
#endif

  if ((uint32_t)client >= fio_data->capa) {
    close(client);
    FIO_LOG_ERROR("fio_accept capacity overflow, closing connection at %d",
                  client);
    client = -1;
    return client;
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
  client = fd2uuid(client);
  FIO_LOG_DEBUG("fio_accept accepted a new connection at %p", (void *)client);
  return client;
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
 * If 0 is returned, an error might have occurred (see `errno`) and the contents
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

static ssize_t fio_hooks_default_read(intptr_t uuid,
                                      void *udata,
                                      void *buf,
                                      size_t count) {
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
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0201 postoffice.c
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************
Section Start Marker












                                 Post Office












The postoffice publishes letters to "subscribers" ...

The design divides three classes of subscribers:

- Filter subscribers: accept any message that matches a numeral value.
- Names subscribers: accept any message who's channel name is an exact match.
- Pattern subscribers: accept any message who's channel name matches a pattern.

Messages are published to a numeral channel (filter) OR a named channel.

The postoffice forwards all messages to the appropriate subscribers.

IPC (inter process communication):
=====

Worker processes connect to the Master process and subscribe to messages. Those
messages are then sent to the child processes.

The Master process does NOT subscribe with the child processes. Instead, all
published messages (that aren't limited to the single  process) are sent to the
master process.
***************************************************************************** */

/* *****************************************************************************
Cluster IO message types (letter types)
***************************************************************************** */

/* Info byte bits:
 * [1-4] pub/sub/unsub and error message types.
 * [4] filter vs. named.
 * [5-6] fowarding type
 * [7] always zero.
 * [8] JSON marker.
 */

typedef enum fio_cluster_message_type_e {
  /* message type, 4th bit == filter */
  FIO_CLUSTER_MSG_PUB_NAME = 0x00,      /* 0b0000 */
  FIO_CLUSTER_MSG_PUB_FILTER = 0x04,    /* 0b0100 */
  FIO_CLUSTER_MSG_SUB_NAME = 0x01,      /* 0b0001 */
  FIO_CLUSTER_MSG_SUB_PATTERN = 0x02,   /* 0b0010 */
  FIO_CLUSTER_MSG_SUB_FILTER = 0x05,    /* 0b0101 */
  FIO_CLUSTER_MSG_UNSUB_NAME = 0x03,    /* 0b0011 */
  FIO_CLUSTER_MSG_UNSUB_PATTERN = 0x08, /* 0b1000 */
  FIO_CLUSTER_MSG_UNSUB_FILTER = 0x06,  /* 0b0110 */
  /* error message type, bits 2-4 of info */
  FIO_CLUSTER_MSG_ERROR = 0x09, /* 0b1001 */
  FIO_CLUSTER_MSG_PING = 0x0A,  /* 0b1010 */
  FIO_CLUSTER_MSG_PONG = 0x0B,  /* 0b1011 */
} fio_cluster_message_type_e;

typedef enum fio_cluster_message_forwarding_e {
  /* (u)subscibing message type, bits 5-6 of info */
  FIO_CLUSTER_MSG_FORARDING_NONE = 0x00,
  FIO_CLUSTER_MSG_FORARDING_GLOBAL = 0x20, /* 0b010000 */
  FIO_CLUSTER_MSG_FORARDING_LOCAL = 0x40   /* 0b100000 */
} fio_cluster_message_forwarding_e;

typedef struct channel_s channel_s;

typedef struct fio_collection_s fio_collection_s;

/** The default engine (settable). */
fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;

/* *****************************************************************************
Cluster IO Registry
***************************************************************************** */

#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_OMAP_NAME          fio_cluster_uuid
#define FIO_MAP_TYPE           intptr_t
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))
#include "fio-stl.h"

static fio_lock_i fio_cluster_registry_lock;
static fio_cluster_uuid_s fio_cluster_registry;

/* Unix Socket name */
static fio_str_s fio___cluster_name = FIO_STR_INIT;

/* *****************************************************************************
Slow memory allocator, for channel names and pub/sub registry.

Allows the use of the following MACROs:
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
***************************************************************************** */

#define FIO_MEMORY_NAME                      fio___slow
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG   21 /* 2Mb */
#define FIO_MEMORY_CACHE_SLOTS               2 /* small system allocation cache */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 3 /* 0.25Mb allocation blocks */
#define FIO_MEMORY_ENABLE_BIG_ALLOC          1 /* we might need this */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS    0 /* nothing to protect */
#define FIO_MEMORY_ARENA_COUNT               1 /* not musch contention, really */
#include "fio-stl.h"

/* *****************************************************************************
 * Glob Matching
 **************************************************************************** */
uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s,
                                    fio_str_info_s) = fio_glob_match;

/* *****************************************************************************
Postoffice message format (letter) - internal API
***************************************************************************** */

/** 8 bit type, 24 bit header length, 32 bit body length */
#define FIO___LETTER_INFO_BYTES 8

struct fio_letter_s {
  /* Note: reference counting and other data is stored before the object */
  /** protocol header - must be first */
  uint8_t info[FIO___LETTER_INFO_BYTES];
  /** message buffer */
  char buf[];
};

typedef struct {
  intptr_t from;
  void *meta[FIO_PUBSUB_METADATA_LIMIT];
} fio_letter_metadata_s;

typedef struct {
  fio_letter_s *l;
  fio_msg_s m;
  volatile intptr_t marker;
} fio_msg_wrapper_s;

/** Creates a new fio_letter_s object. */
FIO_IFUNC fio_letter_s *fio_letter_new_buf(uint8_t *info);
/** Creates a new fio_letter_s object and copies the data into the object. */
FIO_IFUNC fio_letter_s *fio_letter_new_copy(uint8_t type,
                                            int32_t filter,
                                            fio_str_info_s header,
                                            fio_str_info_s body);

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *letter);
/** Returns all information about the fio_letter_s object. */
fio_letter_info_s fio_letter_info(fio_letter_s *letter);
/** Returns a letter's metadata object */
FIO_IFUNC fio_letter_metadata_s *fio_letter_metadata(fio_letter_s *l);

/** helpers. */
/* Returns the fio_letter_s message type according to its info field */
FIO_IFUNC fio_cluster_message_type_e fio__letter2type(uint8_t *info);
/* Returns the fio_letter_s forwarding type according to its info field */
FIO_IFUNC fio_cluster_message_forwarding_e fio__letter2fwd(uint8_t *info);
/* Returns the fio_letter_s header according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2header(uint8_t *info);
/* Returns the fio_letter_s body according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2body(uint8_t *info);

/** small quick helpers. */
FIO_IFUNC uint32_t fio__letter_header_len(uint8_t *info);
FIO_IFUNC uint32_t fio__letter_body_len(uint8_t *info);
FIO_IFUNC fio_letter_s *fio___msg2letter(fio_msg_s *msg);
FIO_IFUNC uint8_t fio__letter_is_json(uint8_t *info);
FIO_IFUNC uint8_t fio__letter_is_filter(uint8_t *info);
FIO_IFUNC void fio__letter_set_json(uint8_t *info);

/** Writes the requested details to the  `info` object. Returns -1 on error. */
FIO_IFUNC int fio___letter_write_info(uint8_t *dest_info,
                                      uint8_t type,
                                      int32_t filter,
                                      uint32_t header_len,
                                      uint32_t body_len);

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
FIO_IFUNC void *fio_letter_msg_metadata(fio_letter_s *l, int id);
/* *****************************************************************************
Cluster - informing the master process about new chunnels
***************************************************************************** */

/** sends a letter to all connected peers and frees the letter */
FIO_SFUNC void fio_cluster_send_letter_to_peers(fio_letter_s *l);
/** informs the master process of a new subscription channel */
FIO_SFUNC void fio___cluster_inform_master_sub(channel_s *ch);
/** informs the master process when a subscription channel is obselete */
FIO_SFUNC void fio___cluster_inform_master_unsub(channel_s *ch);

/* *****************************************************************************
Cluster Letter Publishing Functions
***************************************************************************** */

/* publishes a letter in the current process */
FIO_SFUNC void fio_letter_publish(fio_letter_s *l);

/* *****************************************************************************
Cluster IO Letter Processing Function(s)
***************************************************************************** */

/* Forwards the "letter" to the postofficce system*/
FIO_SFUNC void fio_letter_process_worker(fio_letter_s *l, intptr_t uuid_from);
/* Forwards the "letter" to the postofficce system*/
FIO_SFUNC void fio_letter_process_master(fio_letter_s *l, intptr_t uuid_from);

/* *****************************************************************************
Cluster IO Protocol - Declarations
***************************************************************************** */

typedef struct {
  fio_protocol_s pr;
  fio_letter_s *msg;
  int32_t consumed;
  char info[FIO___LETTER_INFO_BYTES];
} fio__cluster_pr_s;

/* Open a new (client) cluster coonection */
FIO_SFUNC void fio__cluster_connect(void *ignr_);

/* de-register new cluster connections */
FIO_SFUNC void fio___cluster_on_close(intptr_t, fio_protocol_s *);

/* logs connection readiness only once - (debug) level logging. */
FIO_SFUNC void fio___cluster_on_ready(intptr_t, fio_protocol_s *);

/* handle streamed data */
FIO_IFUNC void fio___cluster_on_data_internal(intptr_t,
                                              fio_protocol_s *,
                                              void (*handler)(fio_letter_s *,
                                                              intptr_t));
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_master(intptr_t, fio_protocol_s *);
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_worker(intptr_t, fio_protocol_s *);

/* performs a `ping`. */
FIO_SFUNC void fio___cluster_on_timeout(intptr_t, fio_protocol_s *);

/**
 * Sends a set of messages (letters) with all the subscriptions in the process.
 *
 * Called by fio__cluster_connect.
 */
FIO_IFUNC void fio__cluster___send_all_subscriptions(intptr_t uuid);

/* *****************************************************************************
Cluster IO Listening Protocol - Declarations
***************************************************************************** */

/* starts listening (Pre-Start task) */
FIO_SFUNC void fio___cluster_listen(void *ignr_);

/* accept and register new cluster connections */
FIO_SFUNC void fio___cluster_listen_accept(intptr_t srv, fio_protocol_s *pr);

/* delete the uunix socket file until restarting */
FIO_SFUNC void fio___cluster_listen_on_close(intptr_t uuid, fio_protocol_s *pr);

FIO_SFUNC fio_protocol_s fio__cluster_listen_protocol = {
    .on_data = fio___cluster_listen_accept,
    .on_close = fio___cluster_listen_on_close,
    .on_timeout = FIO_PING_ETERNAL,
};

/* *****************************************************************************
 * Postoffice types - Channel / Subscriptions data
 **************************************************************************** */

#ifndef __clang__ /* clang might misbehave by assumming non-alignment */
#pragma pack(1)   /* https://gitter.im/halide/Halide/archives/2018/07/24 */
#endif
struct channel_s {
  size_t name_len;
  char *name;
  FIO_LIST_HEAD subscriptions;
  fio_collection_s *parent;
  fio_lock_i lock;
};
#ifndef __clang__
#pragma pack()
#endif

/* reference count wrappers for channels - slower lifetimes... */
#define FIO_REF_NAME      channel
#define FIO_REF_FLEX_TYPE char
#define FIO_REF_CONSTRUCTOR_ONLY
/* use the slower local allocator */
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
#include <fio-stl.h>

struct subscription_s {
  FIO_LIST_HEAD node;
  channel_s *parent;
  intptr_t uuid;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata1, void *udata2);
  void *udata1;
  void *udata2;
  /** prevents the callback from running concurrently for multiple messages. */
  fio_lock_i lock;
  fio_lock_i unsubscribed;
};

/* reference count wrappers for subscriptions - fio_malloc (connection life) */
#define FIO_REF_NAME subscription
#define FIO_REF_DESTROY(obj)                                                   \
  do {                                                                         \
    fio_defer((obj).on_unsubscribe, (obj).udata1, (obj).udata2);               \
    channel_free((obj).parent);                                                \
  } while (0)
#define FIO_REF_CONSTRUCTOR_ONLY
#include <fio-stl.h>

/* Use `malloc` / `free`, because channles might have a long life. */

/** Used internally by the Set object to create a new channel. */
static channel_s *channel_copy(channel_s *src) {
  channel_s *dest = channel_new(src->name_len + 1);
  FIO_ASSERT_ALLOC(dest);
  dest->name_len = src->name_len;
  dest->parent = src->parent;
  dest->name = (char *)(dest + 1);
  if (src->name_len)
    memcpy(dest->name, src->name, src->name_len);
  dest->name[src->name_len] = 0;
  FIO_LIST_INIT(dest->subscriptions);
  dest->lock = FIO_LOCK_INIT;
  fio___cluster_inform_master_sub(dest);
  return dest;
}

/** Tests if two channels are equal. */
static int channel_cmp(channel_s *ch1, channel_s *ch2) {
  return ch1->name_len == ch2->name_len &&
         !memcmp(ch1->name, ch2->name, ch1->name_len);
}
/* pub/sub channels and core data sets have a long life, so avoid fio_malloc */
#define FIO_UMAP_NAME            fio_ch_set
#define FIO_MAP_TYPE             channel_s *
#define FIO_MAP_TYPE_CMP(o1, o2) channel_cmp((o1), (o2))
#define FIO_MAP_TYPE_DESTROY(obj)                                              \
  do {                                                                         \
    fio___cluster_inform_master_unsub((obj));                                  \
    channel_free((obj));                                                       \
  } while (0)
#define FIO_MAP_TYPE_COPY(dest, src) ((dest) = channel_copy((src)))
/* use the slower local allocator */
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
#include <fio-stl.h>

/* engine sets are likely to remain static for the lifetime of the process */
#define FIO_OMAP_NAME            fio_engine_set
#define FIO_MAP_TYPE             fio_pubsub_engine_s *
#define FIO_MAP_TYPE_CMP(k1, k2) ((k1) == (k2))
/* use the system allocator for stuff that stays forever */
#define FIO_MALLOC_TMP_USE_SYSTEM
#include <fio-stl.h>

struct fio_collection_s {
  fio_ch_set_s channels;
  fio_lock_i lock;
};

#define COLLECTION_INIT                                                        \
  { .channels = FIO_MAP_INIT, .lock = FIO_LOCK_INIT }

static struct {
  fio_collection_s filters;
  fio_collection_s named;
  fio_collection_s patterns;
  struct {
    fio_engine_set_s set;
    fio_lock_i lock;
  } engines;
  struct {
    struct {
      intptr_t ref;
      fio_msg_metadata_fn builder;
      void (*cleanup)(void *);
    } callbacks[FIO_PUBSUB_METADATA_LIMIT];
  } meta;
} fio_postoffice = {
    .filters = COLLECTION_INIT,
    .named = COLLECTION_INIT,
    .patterns = COLLECTION_INIT,
    .engines.lock = FIO_LOCK_INIT,
};

/* *****************************************************************************
 * Postoffice types - message metadata
 **************************************************************************** */

static void *fio___msg_metadata_fn_noop(fio_str_info_s ch,
                                        fio_str_info_s msg,
                                        uint8_t is_json) {
  return NULL;
  (void)ch;
  (void)msg;
  (void)is_json;
}

static void fio___msg_metadata_fn_cleanup_noop(void *ig_) {
  return;
  (void)ig_;
}

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id) {
  if (!msg)
    return NULL;
  return fio_letter_msg_metadata(fio___msg2letter(msg), id);
}

/**
 * It's possible to attach metadata to facil.io named messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.
 *
 * The callback should return a `void *` pointer.
 *
 * To remove a callback, call `fio_message_metadata_remove` with the returned
 * value.
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 *
 * Returns a positive number on success (the metadata ID) or zero (0) on
 * failure.
 */
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *)) {
  int id = 0;
  int first = FIO_PUBSUB_METADATA_LIMIT;

  if (!builder && !cleanup)
    goto no_room;
  if (!builder)
    builder = fio___msg_metadata_fn_noop;
  if (!cleanup)
    cleanup = fio___msg_metadata_fn_cleanup_noop;

  for (id = 0; id < FIO_PUBSUB_METADATA_LIMIT; ++id) {
    if (!fio_atomic_add(&fio_postoffice.meta.callbacks[id].ref, 1) &&
        first >= FIO_PUBSUB_METADATA_LIMIT) {
      first = id;
    } else if (fio_postoffice.meta.callbacks[id].builder == builder &&
               fio_postoffice.meta.callbacks[id].cleanup == cleanup) {
      break;
    } else {
      fio_atomic_sub(&fio_postoffice.meta.callbacks[id].ref, 1);
    }
  }
  if (first < FIO_PUBSUB_METADATA_LIMIT) {
    if (id >= FIO_PUBSUB_METADATA_LIMIT)
      id = first;
    else
      fio_atomic_sub(&fio_postoffice.meta.callbacks[first].ref, 1);
  }
  if (id < FIO_PUBSUB_METADATA_LIMIT) {
    fio_postoffice.meta.callbacks[id].builder = builder;
    fio_postoffice.meta.callbacks[id].cleanup = cleanup;
  }
  if (id < FIO_PUBSUB_METADATA_LIMIT)
    return (++id);
no_room:
  id = 0;
  return id;
}

void fio_message_metadata_remove(int id) {
  if ((--id) >= FIO_PUBSUB_METADATA_LIMIT)
    goto error;
  if (fio_atomic_sub_fetch(&fio_postoffice.meta.callbacks[id].ref, 1) < 0)
    goto error;
  fio_postoffice.meta.callbacks[id].builder = fio___msg_metadata_fn_noop;
  return;
error:
  if (id < FIO_PUBSUB_METADATA_LIMIT && id >= 0)
    fio_atomic_exchange(&fio_postoffice.meta.callbacks[id].ref, 0);
  FIO_LOG_ERROR(
      "fio_message_metadata_remove called for an invalied (freed?) ID %d",
      id + 1);
}

/* *****************************************************************************
Postoffice message format (letter) - Implementation
***************************************************************************** */

/* define new2, free2 and dup */
#define FIO_REF_NAME      fio_letter
#define FIO_REF_FLEX_TYPE char
#define FIO_REF_METADATA  fio_letter_metadata_s
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {                      \
      if (fio_postoffice.meta.callbacks[i].ref) {                              \
        fio_postoffice.meta.callbacks[i].cleanup((m).meta);                    \
        fio_atomic_sub(&fio_postoffice.meta.callbacks[i].ref, 1);              \
      }                                                                        \
    }                                                                          \
  } while (0)
#include <fio-stl.h>

/** Creates a new fio_letter_s object. */
FIO_IFUNC fio_letter_s *fio_letter_new_buf(uint8_t *info) {
  fio_letter_s *letter =
      fio_letter_new2(fio__letter_body_len(info) + fio__letter_is_filter(info) +
                      fio__letter_header_len(info) + 2);
  if (letter) {
    memcpy(letter->info, info, FIO___LETTER_INFO_BYTES * sizeof(uint8_t));
  }
  return letter;
}

/** Creates a new fio_letter_s object and copies the data into the object. */
FIO_IFUNC fio_letter_s *fio_letter_new_copy(uint8_t type,
                                            int32_t filter,
                                            fio_str_info_s header,
                                            fio_str_info_s body) {
  fio_letter_s *l = NULL;
  uint8_t info[FIO___LETTER_INFO_BYTES];
  if (fio___letter_write_info(info, type, filter, header.len, body.len))
    return l;
  l = fio_letter_new_buf(info);
  if (!l)
    return l;
  const size_t filter_offset = (!!filter) << 2;
  if (filter)
    fio_u2buf32_little(l->buf, (int32_t)filter);
  if (header.len)
    memcpy(l->buf + filter_offset, header.buf, header.len);
  l->buf[header.len + filter_offset] = 0;
  if (body.len)
    memcpy(l->buf + header.len + 1 + filter_offset, body.buf, body.len);
  l->buf[header.len + 1 + body.len + filter_offset] = 0;
  return l;
}

/** Increases the object reference count. */
fio_letter_s *fio_letter_dup(fio_letter_s *letter) {
  return fio_letter_dup2(letter);
}

/** Frees object when the reference count reaches zero. */
void fio_letter_free(fio_letter_s *letter) { fio_letter_free2(letter); }

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *l) {
  return (sizeof(*l) + fio__letter_header_len(l->info) +
          fio__letter_body_len(l->info) + 2) +
         fio__letter_is_filter(l->info);
}

/** Returns all information about the fio_letter_s object. */
fio_letter_info_s fio_letter_info(fio_letter_s *letter) {
  fio_letter_info_s r = {
      .header = fio__letter2header(letter->info),
      .body = fio__letter2body(letter->info),
  };
  return r;
}

/* Returns the fio_letter_s type according to its info field */
FIO_IFUNC fio_cluster_message_type_e fio__letter2type(uint8_t *info) {
  return (fio_cluster_message_type_e)(info[0] & 0x0F); /* bits 1-4 */
}

FIO_IFUNC fio_cluster_message_forwarding_e fio__letter2fwd(uint8_t *info) {
  return (fio_cluster_message_forwarding_e)(info[0] & 0x70); /* bits 5-7 */
}

FIO_IFUNC uint8_t fio__letter_is_json(uint8_t *info) { return info[0] >> 7; }

FIO_IFUNC uint8_t fio__letter_is_filter(uint8_t *info) { return (info[0] & 4); }

/* Returns the fio_letter_s type according to its info field */
FIO_IFUNC void fio__letter_set_json(uint8_t *info) { info[0] |= 128; }

/* Returns the fio_letter_s header according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2header(uint8_t *info) {
  fio_str_info_s r = {
      .len = fio__letter_header_len(info),
      .buf = (char *)(info + FIO___LETTER_INFO_BYTES) +
             fio__letter_is_filter(info),
  };
  return r;
}

/* Returns the fio_letter_s body according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2body(uint8_t *info) {
  fio_str_info_s r = {
      .len = fio__letter_body_len(info),
      .buf = (char *)(info + FIO___LETTER_INFO_BYTES) + 1 +
             fio__letter_is_filter(info) + fio__letter_header_len(info),
  };
  return r;
}

FIO_IFUNC uint32_t fio__letter_header_len(uint8_t *info) {
  return (((uint32_t)info[1] << 16) | ((uint32_t)info[2] << 8) |
          ((uint32_t)info[3]));
}

FIO_IFUNC uint32_t fio__letter_body_len(uint8_t *info) {
  return fio_buf2u32_little((const void *)(info + 4));
}

FIO_IFUNC fio_letter_s *fio___msg2letter(fio_msg_s *msg) {
  return FIO_PTR_FROM_FIELD(fio_msg_wrapper_s, m, msg)->l;
}

/** Writes the requested details to the  `info` object. Returns -1 on error. */
FIO_IFUNC int fio___letter_write_info(uint8_t *info,
                                      uint8_t type,
                                      int32_t filter,
                                      uint32_t header_len,
                                      uint32_t body_len) {
  if ((header_len & ((~(uint32_t)0) << 24)) ||
      (body_len & ((~(uint32_t)0) << 30)))
    return -1;
  info[0] = type | (!!filter);
  info[1] = (header_len >> 16) & 0xFF;
  info[2] = (header_len >> 8) & 0xFF;
  info[3] = (header_len)&0xFF;
  fio_u2buf32_little(info + 4, body_len);
  return 0;
}

/**
 * Increases a message's reference count, returning the published "letter".
 */
fio_letter_s *fio_message_dup(fio_msg_s *msg) {
  if (!msg)
    return NULL;
  return fio_letter_dup2(fio___msg2letter(msg));
}

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
FIO_IFUNC void *fio_letter_msg_metadata(fio_letter_s *l, int id) {
  if (!l || !id || (unsigned int)id > FIO_PUBSUB_METADATA_LIMIT)
    return NULL;
  --id;
  return fio_letter_metadata(l)->meta[id];
}

/* *****************************************************************************
 * Postoffice types - Channel / Subscriptions accessors
 **************************************************************************** */

/**
 * This helper returns a temporary String with the subscription's channel (or a
 * string representing the filter).
 *
 * To keep the string beyond the lifetime of the subscription, copy the string.
 */
fio_str_info_s fio_subscription_channel(subscription_s *s) {
  fio_str_info_s i = {0};
  if (!s || !s->parent)
    return i;
  i.buf = s->parent->name;
  i.len = s->parent->name_len;
  return i;
}

/* *****************************************************************************
Creating Subscriptions
***************************************************************************** */

/* Sublime Text marker */
void fio_subscribe___(void);
subscription_s *fio_subscribe FIO_NOOP(subscribe_args_s args) {
  subscription_s *s = NULL;
  if (!args.on_message)
    goto error;
  s = subscription_new();
  *s = (subscription_s){
      .uuid = args.uuid,
      .on_message = args.on_message,
      .on_unsubscribe = args.on_unsubscribe,
      .udata1 = args.udata1,
      .udata2 = args.udata2,
      .lock = FIO_LOCK_INIT,
      .unsubscribed = FIO_LOCK_INIT,
  };
  fio_collection_s *t;
  char buf[4 + 1];

  if (args.filter) {
    t = &fio_postoffice.filters;
    args.channel.buf = buf;
    args.channel.len = sizeof(args.filter);
    fio_u2buf32_little(buf, args.filter);
  }
  if (args.is_pattern) {
    t = &fio_postoffice.patterns;
  } else {
    t = &fio_postoffice.named;
  }

  uint64_t hash = fio_risky_hash(args.channel.buf,
                                 args.channel.len,
                                 (uint64_t)(uintptr_t)t);
  channel_s *c, channel = {.name = args.channel.buf,
                           .name_len = args.channel.len,
                           .parent = t};
  fio_lock(&t->lock);
  c = fio_ch_set_set_if_missing(&t->channels, hash, &channel);
  s->parent = c;
  FIO_LIST_PUSH(&c->subscriptions, &s->node);
  channel_dup(c);
  fio_unlock(&t->lock);
  if (!args.uuid)
    return s;
  fio_uuid_env_set(args.uuid,
                   .type = -3 + !args.is_pattern,
                   .name = {.buf = c->name, .len = c->name_len},
                   .udata = s,
                   .on_close = (void (*)(void *))fio_unsubscribe,
                   .const_name = 1);

  s = NULL;
  return s;

error:
  fio_defer(args.on_unsubscribe, args.udata1, args.udata2);
  return s;
}

void fio_unsubscribe(subscription_s *s) {
  channel_s *c = s->parent;
  fio_trylock(&s->unsubscribed);
  fio_collection_s *t = c->parent;
  fio_lock(&c->lock);
  FIO_LIST_REMOVE(&s->node);
  if (c->subscriptions.next == &c->subscriptions) {
    uint64_t hash =
        fio_risky_hash(c->name, c->name_len, (uint64_t)(uintptr_t)t);
    fio_lock(&t->lock);
    fio_ch_set_remove(&t->channels, hash, c, NULL); /* calls channel_free */
    fio_unlock(&t->lock);
  }
  fio_unlock(&c->lock);
  subscription_free(s); /* may call channel_free for subscription's reference */
}

void fio_unsubscribe_uuid FIO_NOOP(subscribe_args_s args) {
  fio_uuid_env_remove(args.uuid,
                      .type = (-3 + !args.is_pattern),
                      .name = args.channel);
}

/* *****************************************************************************
Cluster Letter Publishing Functions
***************************************************************************** */

/**Defers the current callback, so it will be called again for the message. */
void fio_message_defer(fio_msg_s *msg) {
  fio_msg_wrapper_s *m = FIO_PTR_FROM_FIELD(fio_msg_wrapper_s, m, msg);
  m->marker = -1;
}

FIO_SFUNC int fio_letter_publish_task_perform_inner(subscription_s *s,
                                                    fio_letter_s *l,
                                                    uint32_t filter) {
  fio_protocol_s *pr = NULL;
  fio_msg_wrapper_s data = {.l = l};
  if (fio_trylock(&s->lock))
    goto reschedule;
  if (s->uuid && !(pr = protocol_try_lock(s->uuid, FIO_PR_LOCK_TASK))) {
    if (errno == EWOULDBLOCK)
      goto reschedule_locked;
    goto done;
  }
  data.m = (fio_msg_s){
      .filter = filter,
      .channel = fio__letter2header(l->info),
      .uuid = s->uuid,
      .pr = pr,
      .msg = fio__letter2body(l->info),
      .udata1 = s->udata1,
      .udata2 = s->udata2,
      .is_json = fio__letter_is_json(l->info),
  };
  s->on_message(&data.m);
  if (data.marker)
    goto reschedule_pr_locked;

done:
  if (pr)
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
  fio_unlock(&s->lock);
  fio_letter_free2(l);
  subscription_free(s);
  return 0;
reschedule_pr_locked:
  if (pr)
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
reschedule_locked:
  fio_unlock(&s->lock);
reschedule:
  return -1;
}

FIO_SFUNC void fio_letter_publish_task_perform(void *s_, void *l_) {
  subscription_s *s = s_;
  fio_letter_s *l = l_;
  if (fio_letter_publish_task_perform_inner(s, l, 0))
    fio_defer(fio_letter_publish_task_perform, s_, l_);
}

FIO_SFUNC void fio_letter_publish_task_perform_filter(void *s_, void *l_) {
  subscription_s *s = s_;
  fio_letter_s *l = l_;
  uint32_t f = fio_buf2u32_little(l->buf);
  if (fio_letter_publish_task_perform_inner(s, l, f))
    fio_defer(fio_letter_publish_task_perform_filter, s_, l_);
}

FIO_SFUNC void fio_letter_publish_task_filter(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.filters.lock))
    goto reschedule;
  channel_s channel = {.name = l->buf, .name_len = 4};
  channel_s *ch = fio_ch_set_get(
      &fio_postoffice.filters.channels,
      fio_risky_hash(channel.name,
                     channel.name_len,
                     (uint64_t)(uintptr_t)&fio_postoffice.filters),
      &channel);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, i) {
      fio_defer(fio_letter_publish_task_perform_filter,
                subscription_dup(i),
                fio_letter_dup2(l));
    }
  }
  fio_unlock(&fio_postoffice.filters.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_filter, l_, ignr_);
}

FIO_SFUNC void fio_letter_publish_task_named(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.named.lock))
    goto reschedule;
  channel_s channel = {.name = fio__letter2header(l->info).buf,
                       .name_len = fio__letter2header(l->info).len};
  channel_s *ch =
      fio_ch_set_get(&fio_postoffice.named.channels,
                     fio_risky_hash(channel.name,
                                    channel.name_len,
                                    (uint64_t)(uintptr_t)&fio_postoffice.named),
                     &channel);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, i) {
      fio_defer(fio_letter_publish_task_perform,
                subscription_dup(i),
                fio_letter_dup2(l));
    }
  }
  fio_unlock(&fio_postoffice.named.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_named, l_, ignr_);
}

FIO_SFUNC void fio_letter_publish_task_pattern(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.patterns.lock))
    goto reschedule;
  fio_str_info_s ch_name = fio__letter2header(l->info);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, pos) {
    // pos->obj->name
    fio_str_info_s pat = {.buf = pos->obj->name, .len = pos->obj->name_len};
    if (FIO_PUBSUB_PATTERN_MATCH(pat, ch_name)) {
      FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, i) {
        fio_defer(fio_letter_publish_task_perform,
                  subscription_dup(i),
                  fio_letter_dup2(l));
      }
    }
  }
  fio_unlock(&fio_postoffice.patterns.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_pattern, l_, ignr_);
}

/* publishes a letter in the current process */
FIO_SFUNC void fio_letter_publish(fio_letter_s *l) {
  if (fio__letter_is_filter(l->info)) {
    fio_letter_publish_task_filter(fio_letter_dup2(l), NULL);
  } else {
    fio_letter_metadata_s *meta = fio_letter_metadata(l);
    register fio_str_info_s ch = fio__letter2header(l->info);
    register fio_str_info_s msg = fio__letter2body(l->info);
    register uint8_t is_json = fio__letter_is_json(l->info);
    for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {
      if (!fio_postoffice.meta.callbacks[i].ref)
        continue;
      if (!fio_atomic_add(&fio_postoffice.meta.callbacks[i].ref, 1)) {
        /* removed before we could request a reference to the object */
        fio_atomic_sub(&fio_postoffice.meta.callbacks[i].ref, 1);
        continue;
      }
      meta->meta[i] =
          fio_postoffice.meta.callbacks[i].builder(ch, msg, is_json);
    }
    fio_letter_publish_task_named(fio_letter_dup2(l), NULL);
    fio_letter_publish_task_pattern(fio_letter_dup2(l), NULL);
  }
}

/* *****************************************************************************
Cluster Letter Publishing API
***************************************************************************** */

// typedef struct fio_publish_args_s {
// fio_pubsub_engine_s const *engine;
// int32_t filter;
// fio_str_info_s channel;
// fio_str_info_s message;
// uint8_t is_json;
// } fio_publish_args_s;

/* Sublime Text Marker*/
void fio_publish___(void);
/**
 * Publishes a message to the relevant subscribers (if any).
 */
void fio_publish FIO_NOOP(fio_publish_args_s args) {
  if (!args.engine)
    args.engine = FIO_PUBSUB_DEFAULT;
  if (args.filter && (uintptr_t)args.engine >= 7)
    args.engine = FIO_PUBSUB_LOCAL;
  uint8_t type =
      args.filter ? FIO_CLUSTER_MSG_PUB_FILTER : FIO_CLUSTER_MSG_PUB_NAME;
  fio_letter_s *l = NULL;
  if ((uintptr_t)args.engine <= 7) {
    l = fio_letter_new_copy(type, args.filter, args.channel, args.message);
    FIO_ASSERT_ALLOC(l);
    if (args.is_json)
      fio__letter_set_json(l->info);
  }

  switch ((uintptr_t)args.engine) {
  case 1: /* FIO_PUBSUB_CLUSTER */
    /** Used to publish the message to all clients in the cluster. */
    fio_letter_metadata(l)->from = -1; /* subscribed connections will forward */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_GLOBAL;
    if (!fio_data->is_master)
      fio_cluster_send_letter_to_peers(fio_letter_dup2(l));
    break;

  case 2: /* TODO: FIO_PUBSUB_LOCAL */
    /** Used to publish the message to all clients in the local cluster. */
    fio_letter_metadata(l)->from = -1; /* subscribed connections will forward */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_LOCAL;
    if (!fio_data->is_master)
      fio_cluster_send_letter_to_peers(fio_letter_dup2(l));
    break;

  case 3: /* FIO_PUBSUB_SIBLINGS */
    /** Used to publish the message except within the current process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_LOCAL;
    fio_cluster_send_letter_to_peers(l); /* don't process the letter */
    return;

  case 4: /* FIO_PUBSUB_PROCESS */
    /** Used to publish the message only within the current process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_NONE;
    break;

  case 5: /* FIO_PUBSUB_ROOT */
    /** Used to publish the message exclusively to the root / master process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_NONE;
    if (!fio_data->is_master) {
      fio_cluster_send_letter_to_peers(l); /* don't process the letter */
      return;
    }
    break;
  default:
    if (!args.filter)
      args.engine->publish(args.engine,
                           args.channel,
                           args.message,
                           args.is_json);
    return;
  }
  fio_letter_publish(l);
  fio_letter_free2(l);
}

/* *****************************************************************************
Cluster - informing the master process about new chunnels
***************************************************************************** */

FIO_SFUNC void fio_cluster_send_letter_to_peers(fio_letter_s *l) {
  fio_lock(&fio_cluster_registry_lock);
  FIO_MAP_EACH(fio_cluster_uuid, &fio_cluster_registry, i) {
    fio_write2(i->obj,
               .data.buf = fio_letter_dup2(l),
               .len = fio_letter_len(l),
               .dealloc = (void (*)(void *))fio_letter_free);
  }
  fio_unlock(&fio_cluster_registry_lock);
  fio_letter_free(l);
}

/** informs the master process of a new subscription channel */
FIO_SFUNC void fio___cluster_inform_master_sub(channel_s *ch) {
  register fio_str_info_s ch_info = {.buf = ch->name, .len = ch->name_len};
  int32_t filter = 0;
  if (fio_data->is_master)
    goto inform_engines;
  uint8_t type;
  if (ch->parent == &fio_postoffice.named)
    type = FIO_CLUSTER_MSG_SUB_NAME;
  else if (ch->parent == &fio_postoffice.filters) {
    filter = fio_buf2u32_little(ch->name);
    ch_info.len = 0;
    type = FIO_CLUSTER_MSG_SUB_FILTER;
  } else
    type = FIO_CLUSTER_MSG_SUB_PATTERN;
  fio_letter_s *l =
      fio_letter_new_copy(type, filter, ch_info, (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_cluster_send_letter_to_peers(l);
inform_engines:
  if (ch->parent == &fio_postoffice.filters)
    return;
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(fio_engine_set, &fio_postoffice.engines.set, i) {
    i->obj->subscribe(i->obj, ch_info, ch->parent == &fio_postoffice.patterns);
  }
  fio_unlock(&fio_postoffice.engines.lock);
}
/** informs the master process when a subscription channel is obselete */
FIO_SFUNC void fio___cluster_inform_master_unsub(channel_s *ch) {
  register fio_str_info_s ch_info = {.buf = ch->name, .len = ch->name_len};
  int32_t filter = 0;
  if (fio_data->is_master)
    goto inform_engines;
  uint8_t type;
  if (ch->parent == &fio_postoffice.named)
    type = FIO_CLUSTER_MSG_UNSUB_NAME;
  else if (ch->parent == &fio_postoffice.filters) {
    filter = fio_buf2u32_little(ch->name);
    ch_info.len = 0;
    type = FIO_CLUSTER_MSG_UNSUB_FILTER;
  } else
    type = FIO_CLUSTER_MSG_UNSUB_PATTERN;
  fio_letter_s *l =
      fio_letter_new_copy(type, filter, ch_info, (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_cluster_send_letter_to_peers(l);
inform_engines:
  if (ch->parent == &fio_postoffice.filters)
    return;
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(fio_engine_set, &fio_postoffice.engines.set, i) {
    i->obj->unsubscribe(i->obj,
                        ch_info,
                        ch->parent == &fio_postoffice.patterns);
  }
  fio_unlock(&fio_postoffice.engines.lock);
}

/* *****************************************************************************
Cluster Letter Processing
***************************************************************************** */

FIO_SFUNC void fio___cluster_uuid_on_message_internal(fio_msg_s *msg) {
  fio_letter_s *l = fio___msg2letter(msg);
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  if (!meta->from || meta->from == msg->uuid)
    return;
  fio_write2(msg->uuid,
             .data.buf = fio_letter_dup2(l),
             .len = fio_letter_len(l),
             .dealloc = (void (*)(void *))fio_letter_free);
}

FIO_SFUNC void fio_letter_process_master(fio_letter_s *l, intptr_t from) {
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  switch (fio__letter2type(l->info)) {
  case FIO_CLUSTER_MSG_PUB_NAME: /* fallthrough */
  case FIO_CLUSTER_MSG_PUB_FILTER:
    if (fio__letter2fwd(l->info))
      meta->from = from;
    fio_letter_publish(l);
    break;
  case FIO_CLUSTER_MSG_SUB_FILTER:
    fio_subscribe(.uuid = from,
                  .filter = fio_buf2u32_little(l->buf),
                  .on_message = fio___cluster_uuid_on_message_internal);
    break;
  case FIO_CLUSTER_MSG_UNSUB_FILTER:
    fio_unsubscribe_uuid(.uuid = from, .filter = fio_buf2u32_little(l->buf));
    break;
  case FIO_CLUSTER_MSG_SUB_NAME:
    fio_subscribe(.uuid = from,
                  .channel = fio__letter2header(l->info),
                  .on_message = fio___cluster_uuid_on_message_internal);
    break;
  case FIO_CLUSTER_MSG_UNSUB_NAME:
    fio_unsubscribe_uuid(.uuid = from, .channel = fio__letter2header(l->info));
    break;
  case FIO_CLUSTER_MSG_SUB_PATTERN:
    fio_subscribe(.uuid = from,
                  .channel = fio__letter2header(l->info),
                  .on_message = fio___cluster_uuid_on_message_internal,
                  .is_pattern = 1);
    break;
  case FIO_CLUSTER_MSG_UNSUB_PATTERN:
    fio_unsubscribe_uuid(.uuid = from,
                         .channel = fio__letter2header(l->info),
                         .is_pattern = 1);
    break;
  case FIO_CLUSTER_MSG_ERROR:
    break;
  case FIO_CLUSTER_MSG_PING:
    break;
  case FIO_CLUSTER_MSG_PONG:
    break;
  }
  fio_letter_free2(l);
}
FIO_SFUNC void fio_letter_process_worker(fio_letter_s *l, intptr_t from) {
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  switch (fio__letter2type(l->info)) {
  case FIO_CLUSTER_MSG_PUB_FILTER: /* fallthrough */
  case FIO_CLUSTER_MSG_PUB_NAME:
    meta->from = from;
    fio_letter_publish(l);
    break;
  case FIO_CLUSTER_MSG_SUB_FILTER:    /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_FILTER:  /* fallthrough */
  case FIO_CLUSTER_MSG_SUB_NAME:      /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_NAME:    /* fallthrough */
  case FIO_CLUSTER_MSG_SUB_PATTERN:   /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_PATTERN: /* fallthrough */
  case FIO_CLUSTER_MSG_ERROR:
    break;
  case FIO_CLUSTER_MSG_PING:
    break;
  case FIO_CLUSTER_MSG_PONG:
    break;
  }
  fio_letter_free2(l);
}

/* *****************************************************************************
Cluster IO Protocol
***************************************************************************** */

/* Open a new (client) cluster coonection */
FIO_SFUNC void fio__cluster_connect(void *ignr_) {
  (void)ignr_;
  FIO_LOG_DEBUG2("(%d) connecting to cluster.", fio_getpid());
  int fd = fio_sock_open(fio_str2ptr(&fio___cluster_name),
                         NULL,
                         FIO_SOCK_NONBLOCK | FIO_SOCK_UNIX | FIO_SOCK_CLIENT);
  if (fd == -1) {
    FIO_LOG_FATAL("(%d) worker process couldn't connect to master process",
                  fio_getpid());
    kill(0, SIGINT);
  }
  fio__cluster_pr_s *pr = malloc(sizeof(*pr));
  if (!pr) {
    FIO_LOG_FATAL("(%d) worker process couldn't connect to master process",
                  fio_getpid());
    kill(0, SIGINT);
  }
  *pr = (fio__cluster_pr_s){
      .pr =
          {
              .on_data = fio___cluster_on_data_worker,
              .on_ready = fio___cluster_on_ready,
#if !FIO_DISABLE_HOT_RESTART
              .on_shutdown = mock_on_shutdown_eternal,
#endif
              .on_close = fio___cluster_on_close,
              .on_timeout = fio___cluster_on_timeout,
          },
  };
  /* attach pub/sub protocol for incoming messages */
  fio_attach_fd(fd, &pr->pr);
  /* add to registry, making publishing available */
  intptr_t uuid = fd2uuid(fd);
  if (uuid != -1) {
    fio_lock(&fio_cluster_registry_lock);
    fio_cluster_uuid_set(&fio_cluster_registry,
                         fio_risky_hash(&uuid, sizeof(uuid), 0),
                         uuid,
                         NULL);
    fio_unlock(&fio_cluster_registry_lock);
  }
}

/* de-register new cluster connections */
FIO_SFUNC void fio___cluster_on_close(intptr_t uuid, fio_protocol_s *pr) {
  fio_lock(&fio_cluster_registry_lock);
  fio_cluster_uuid_remove(&fio_cluster_registry,
                          fio_risky_hash(&uuid, sizeof(uuid), 0),
                          uuid,
                          NULL);
  fio_unlock(&fio_cluster_registry_lock);
  if (((fio__cluster_pr_s *)pr)->msg) {
    fio_letter_free2(((fio__cluster_pr_s *)pr)->msg);
  }
  free(pr);
  if (pr->on_data == fio___cluster_on_data_worker && fio_data->active) {
    FIO_LOG_ERROR("(%d) lost cluster connection - master crushed?",
                  fio_getpid());
    fio_stop();
    kill(0, SIGINT);
  }
}

/* logs connection subsribe exiting only once + (debug) level logging. */
FIO_SFUNC void fio___cluster_on_ready(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  protocol->on_ready = mock_on_ev;
  fio__cluster___send_all_subscriptions(uuid);
  FIO_LOG_DEBUG2("(%d) worker process connected to cluster socket",
                 fio_getpid());
}

/* performs a `ping`. */
FIO_SFUNC void fio___cluster_on_timeout(intptr_t uuid,
                                        fio_protocol_s *protocol) {
  // FIO_CLUSTER_MSG_PING
  fio_letter_s *l = fio_letter_new_copy(FIO_CLUSTER_MSG_PING,
                                        0,
                                        (fio_str_info_s){0},
                                        (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_write2(uuid,
             .data.buf = l,
             .len = fio_letter_len(l),
             .dealloc = (void (*)(void *))fio_letter_free);
  (void)uuid;
  (void)protocol;
}

/* handle streamed data */
FIO_IFUNC void fio___cluster_on_data_internal(intptr_t uuid,
                                              fio_protocol_s *protocol,
                                              void (*handler)(fio_letter_s *,
                                                              intptr_t)) {
  fio__cluster_pr_s *p = (fio__cluster_pr_s *)protocol;
  ssize_t r = 0;
  if (p->consumed <= 0) {
    uint8_t buf[8192];
    if (p->consumed) {
      r = 0 - p->consumed;
      memcpy(buf, p->info, r);
      r = fio_read(uuid, buf + r, 8192 - r);
    } else {
      r = fio_read(uuid, buf, 8192);
    }
    uint8_t *pos = buf;
    /* we might have caught a number of messages... or only fragments */
    while (r > 0) {
      if (r < FIO___LETTER_INFO_BYTES) {
        /* header was broken */
        p->consumed = 0 - r;
        memcpy(p->info, pos, r);
        fio_force_event(uuid, FIO_EVENT_ON_DATA);
        return;
      }
      fio_letter_s *l = fio_letter_new_buf((uint8_t *)pos);
      uint32_t expect = fio_letter_len(l);
      /* copy data to the protocol and wait for the rest to arrive */
      if (expect > (uint32_t)r) {
        p->consumed = expect;
        /* (re)copy the info bytes?, is math really faster than 8 bytes? */
        if (r > FIO___LETTER_INFO_BYTES)
          memcpy(l->buf,
                 (pos + FIO___LETTER_INFO_BYTES),
                 (r - FIO___LETTER_INFO_BYTES));
        p->msg = l;
        break;
      }
      if (expect > FIO___LETTER_INFO_BYTES)
        memcpy(l->buf,
               (pos + FIO___LETTER_INFO_BYTES),
               (expect - FIO___LETTER_INFO_BYTES));
      handler(l, uuid);
      r -= expect;
      pos += expect;
    }
  }
  if (p->msg) {
    int64_t expect = fio_letter_len(p->msg);
    while (p->consumed < expect && (r = fio_read(uuid,
                                                 p->msg->buf + p->consumed,
                                                 expect - p->consumed)) > 0) {
      p->consumed += r;
      if (p->consumed < expect)
        continue;
      handler(p->msg, uuid);
      p->consumed = 0;
      p->msg = NULL;
    }
  }
}
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_master(intptr_t uuid, fio_protocol_s *pr) {
  fio___cluster_on_data_internal(uuid, pr, fio_letter_process_master);
}
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_worker(intptr_t uuid, fio_protocol_s *pr) {
  fio___cluster_on_data_internal(uuid, pr, fio_letter_process_worker);
}

/**
 * Sends a set of messages (letters) with all the subscriptions in the process.
 *
 * Called by fio__cluster_connect.
 */
FIO_IFUNC void fio__cluster___send_all_subscriptions(intptr_t uuid) {
  fio_lock(&fio_postoffice.filters.lock);
  fio_lock(&fio_postoffice.named.lock);
  fio_lock(&fio_postoffice.patterns.lock);

  /* calculate required buffer length */
  uint8_t *buf = NULL;
  uint8_t *pos = NULL;
  size_t len = (FIO___LETTER_INFO_BYTES + 2 + 4) *
               fio_ch_set_count(&fio_postoffice.filters.channels);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    len += FIO___LETTER_INFO_BYTES + 2 + i->obj->name_len;
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, i) {
    len += FIO___LETTER_INFO_BYTES + 2 + i->obj->name_len;
  }

  /* allocate memory */
  pos = buf = fio_malloc(len);
  FIO_ASSERT_ALLOC(buf);

  /* write all messages to the buffer as one long stream of data */
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.filters.channels, i) {
    fio___letter_write_info(pos, FIO_CLUSTER_MSG_SUB_FILTER, 1, 0, 0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, 4);
    pos[FIO___LETTER_INFO_BYTES + 4] = 0;
    pos += FIO___LETTER_INFO_BYTES + 4 + 2;
  }

  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    fio___letter_write_info(pos,
                            FIO_CLUSTER_MSG_SUB_NAME,
                            0,
                            i->obj->name_len,
                            0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, i->obj->name_len);
    pos[FIO___LETTER_INFO_BYTES + i->obj->name_len] = 0;
    pos += FIO___LETTER_INFO_BYTES + i->obj->name_len + 2;
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, i) {
    fio___letter_write_info(pos,
                            FIO_CLUSTER_MSG_SUB_PATTERN,
                            0,
                            i->obj->name_len,
                            0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, i->obj->name_len);
    pos[FIO___LETTER_INFO_BYTES + i->obj->name_len] = 0;
    pos += FIO___LETTER_INFO_BYTES + i->obj->name_len + 2;
  }

  fio_unlock(&fio_postoffice.filters.lock);
  fio_unlock(&fio_postoffice.named.lock);
  fio_unlock(&fio_postoffice.patterns.lock);

  /* send data */
  fio_write2(uuid, .data.buf = buf, .len = len, .dealloc = fio_free);
}
/* *****************************************************************************
Cluster IO Listening Protocol
***************************************************************************** */

/* starts listening (Pre-Start task) */
FIO_SFUNC void fio___cluster_listen(void *ignr_) {
  int fd = fio_sock_open(fio_str2ptr(&fio___cluster_name),
                         NULL,
                         FIO_SOCK_NONBLOCK | FIO_SOCK_UNIX | FIO_SOCK_SERVER);
  FIO_ASSERT(fd != -1, "facil.io IPC failed to create Unix Socket");
  fio_attach_fd(fd, &fio__cluster_listen_protocol);
  FIO_LOG_DEBUG("(%d) cluster listening on %p@%s",
                fio_data->parent,
                (void *)fio_fd2uuid(fd),
                fio_str2ptr(&fio___cluster_name));
  (void)ignr_;
}

/* accept and register new cluster connections */
FIO_SFUNC intptr_t fio___cluster_listen_accept_uuid(intptr_t srv) {
  FIO_LOG_DEBUG2("(%d) cluster attempting to accept connection",
                 fio_data->parent,
                 fio_str2ptr(&fio___cluster_name));
  intptr_t uuid = fio_accept(srv);
  if (uuid == -1) {
    FIO_LOG_ERROR("(%d) cluster couldn't accept client connection! (%s)",
                  fio_data->parent,
                  strerror(errno));
    return uuid;
  }
  fio_lock(&fio_cluster_registry_lock);
  fio_cluster_uuid_set(&fio_cluster_registry,
                       fio_risky_hash(&uuid, sizeof(uuid), 0),
                       uuid,
                       NULL);
  fio_unlock(&fio_cluster_registry_lock);
  fio_timeout_set(uuid, 55); /* setup ping from master */
  return uuid;
}

/* accept and register new cluster connections */
FIO_SFUNC void fio___cluster_listen_accept(intptr_t srv, fio_protocol_s *p_) {
  intptr_t uuid = fio___cluster_listen_accept_uuid(srv);
  if (uuid == -1)
    return;

  fio__cluster_pr_s *pr = malloc(sizeof(*pr));
  *pr = (fio__cluster_pr_s){
      .pr =
          {
              .on_data = fio___cluster_on_data_master,
              .on_shutdown = mock_on_shutdown_eternal,
              .on_close = fio___cluster_on_close,
              .on_timeout = fio___cluster_on_timeout,
          },
  };
  fio_attach(uuid, &pr->pr);
  FIO_LOG_DEBUG2("(%d) cluster acccepted a new client connection",
                 fio_data->parent);
  (void)p_;
}

/* delete the unix socket file until restarting */
FIO_SFUNC void fio___cluster_listen_on_close(intptr_t uuid,
                                             fio_protocol_s *pr) {
  if (fio_is_master() && fio_str_len(&fio___cluster_name)) {
    FIO_LOG_DEBUG2("(%d) cluster deleteing listening socket.",
                   fio_data->parent);
    unlink(fio_str2ptr(&fio___cluster_name));
    fio_cluster_registry_lock = FIO_LOCK_INIT;
    fio_cluster_uuid_destroy(&fio_cluster_registry);
  }
  (void)uuid;
  (void)pr;
}

/* *****************************************************************************
Pub/Sub Engine Management
***************************************************************************** */

static void fio___engine_subscribe_noop(const fio_pubsub_engine_s *eng,
                                        fio_str_info_s channel,
                                        uint8_t is_pattern) {
  return;
  (void)eng;
  (void)channel;
  (void)is_pattern;
}
/** Should unsubscribe channel. Failures are ignored. */
static void fio___engine_unsubscribe_noop(const fio_pubsub_engine_s *eng,
                                          fio_str_info_s channel,
                                          uint8_t is_pattern) {
  return;
  (void)eng;
  (void)channel;
  (void)is_pattern;
}
/** Should publish a message through the engine. Failures are ignored. */
static void fio___engine_publish_noop(const fio_pubsub_engine_s *eng,
                                      fio_str_info_s channel,
                                      fio_str_info_s msg,
                                      uint8_t is_json) {
  return;
  (void)eng;
  (void)channel;
  (void)msg;
  (void)is_json;
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
  if (!engine->subscribe && !engine->unsubscribe && !!engine->publish)
    return;
  if (!engine->subscribe)
    engine->subscribe = fio___engine_subscribe_noop;
  if (!engine->unsubscribe)
    engine->unsubscribe = fio___engine_unsubscribe_noop;
  if (!engine->publish)
    engine->publish = fio___engine_publish_noop;
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_set_if_missing(&fio_postoffice.engines.set,
                                (uint64_t)(uintptr_t)engine,
                                engine);
  fio_unlock(&fio_postoffice.engines.lock);
  fio_pubsub_reattach(engine);
}

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine) {
  if (FIO_PUBSUB_DEFAULT == engine)
    FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_remove(&fio_postoffice.engines.set,
                        (uint64_t)(uintptr_t)engine,
                        engine,
                        NULL);
  fio_unlock(&fio_postoffice.engines.lock);
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
  fio_lock(&fio_postoffice.named.lock);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    register fio_str_info_s c = {.buf = i->obj->name, .len = i->obj->name_len};
    eng->subscribe(eng, c, 0);
  }
  fio_unlock(&fio_postoffice.named.lock);
  fio_lock(&fio_postoffice.patterns.lock);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    register fio_str_info_s c = {.buf = i->obj->name, .len = i->obj->name_len};
    eng->subscribe(eng, c, 1);
  }
  fio_unlock(&fio_postoffice.patterns.lock);
}

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine) {
  fio_lock(&fio_postoffice.engines.lock);
  engine = fio_engine_set_get(&fio_postoffice.engines.set,
                              (uint64_t)(uintptr_t)engine,
                              engine);
  fio_unlock(&fio_postoffice.engines.lock);
  return engine != NULL;
}

/* *****************************************************************************
Remote Cluster Connections
***************************************************************************** */

/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *tls);

/* *****************************************************************************
Cluster forking handler
***************************************************************************** */

FIO_SFUNC void fio_pubsub_on_fork(void *ignr_) {
  fio_cluster_registry_lock = FIO_LOCK_INIT;
  fio_postoffice.filters.lock = FIO_LOCK_INIT;
  fio_postoffice.named.lock = FIO_LOCK_INIT;
  fio_postoffice.patterns.lock = FIO_LOCK_INIT;
  fio_postoffice.engines.lock = FIO_LOCK_INIT;
  fio___slow_malloc_after_fork();
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.filters.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  (void)ignr_;
}

/* *****************************************************************************
Cluster State and Initialization
***************************************************************************** */

FIO_IFUNC void fio_pubsub_cleanup(void *ignr_) {
  (void)ignr_;
  if (fio_is_master()) {
    unlink(fio_str2ptr(&fio___cluster_name));
  }
  free(fio_str2ptr(&fio___cluster_name));
  fio_str_destroy(&fio___cluster_name);
  fio_cluster_uuid_destroy(&fio_cluster_registry);
  fio_ch_set_destroy(&fio_postoffice.filters.channels);
  fio_ch_set_destroy(&fio_postoffice.named.channels);
  fio_ch_set_destroy(&fio_postoffice.patterns.channels);
  fio_engine_set_destroy(&fio_postoffice.engines.set);
}

FIO_IFUNC void fio_pubsub_init(void) {
/* Set up the Unix Socket name */
#ifdef P_tmpdir
  if (sizeof(P_tmpdir) > 2 && P_tmpdir[sizeof(P_tmpdir) - 2] == '/') {
    fio_str_write(&fio___cluster_name, P_tmpdir, sizeof(P_tmpdir) - 1);
  } else {
    fio_str_write(&fio___cluster_name, P_tmpdir, sizeof(P_tmpdir) - 1);
    fio_str_write(&fio___cluster_name, "/", 1);
  }
#else
  fio_str_write(&fio___cluster_name, "/tmp/", 5);
#endif
  fio_str_printf(&fio___cluster_name, "facil.io.ipc.%d.sock", fio_getpid());
  /* move memory to eternal storage (syatem malloc) */
  {
    size_t len = fio_str_len(&fio___cluster_name);
    void *tmp = malloc(len + 1);
    FIO_ASSERT_ALLOC(tmp);
    memcpy(tmp, fio_str2ptr(&fio___cluster_name), len + 1);
    fio_str_destroy(&fio___cluster_name);
    fio_str_init_const(&fio___cluster_name, tmp, len);
  }
  for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {
    fio_postoffice.meta.callbacks[i].builder = fio___msg_metadata_fn_noop;
    fio_postoffice.meta.callbacks[i].cleanup =
        fio___msg_metadata_fn_cleanup_noop;
  }
  /* set up callbacks */
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio_pubsub_cleanup, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio_pubsub_on_fork, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio__cluster_connect, NULL);
  fio_state_callback_add(FIO_CALL_PRE_START, fio___cluster_listen, NULL);
}

/* *****************************************************************************
Test state callbacks
***************************************************************************** */
#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, postoffice_letter)(void) {
  int32_t filter = -42;
  fio_str_info_s ch = {.buf = "ch", .len = 2};
  fio_str_info_s body = {.buf = "body", .len = 4};
  fio_letter_s *l =
      fio_letter_new_copy(FIO_CLUSTER_MSG_PUB_FILTER, filter, ch, body);
  FIO_ASSERT_ALLOC(l);
  for (int rep = 0; rep < 4; ++rep) {
    FIO_ASSERT((fio__letter_is_filter(l->info) >> 2) == (rep < 2),
               "letter[%d] isn't properly marked as a filter letter",
               rep);
    FIO_ASSERT(!fio__letter_is_filter(l->info) ||
                   (int32_t)fio_buf2u32_little(l->buf) == filter,
               "letter[%d] filter value error (%d != %d)",
               rep,
               (int)fio_buf2u32_little(l->buf),
               (int)filter);
    FIO_ASSERT(fio_letter_info(l).header.len == ch.len,
               "letter[%d] header length error",
               rep);
    FIO_ASSERT(fio_letter_info(l).body.len == body.len,
               "letter[%d] body length error",
               rep);
    FIO_ASSERT(!memcmp(fio_letter_info(l).header.buf, ch.buf, ch.len),
               "letter[%d] header content error",
               rep);
    FIO_ASSERT(!memcmp(fio_letter_info(l).body.buf, body.buf, body.len),
               "letter[%d] body content error: %s != %s",
               rep,
               fio_letter_info(l).body.buf,
               body.buf);
    if (!(rep & 1)) {
      fio_letter_s *tmp = fio_letter_new_buf(l->info);
      FIO_ASSERT_ALLOC(tmp);
      FIO_ASSERT(fio_letter_len(tmp) == fio_letter_len(l),
                 "different letter lengths during copy");
      memcpy(tmp, l, fio_letter_len(tmp));
      fio_letter_free2(l);
      l = tmp;
    } else {
      fio_letter_free2(l);
      if (rep == 1) {
        filter = 0;
        l = fio_letter_new_copy(FIO_CLUSTER_MSG_PUB_NAME, filter, ch, body);
      }
    }
  }
}
/* State callback tests */
FIO_SFUNC void FIO_NAME_TEST(io, postoffice)(void) {
  fprintf(stderr, "* testing pub/sub postoffice.\n");
  FIO_NAME_TEST(io, postoffice_letter)();
  fprintf(stderr, "TODO.\n");
  /* TODO */
}
#endif /* TEST */
/* *****************************************************************************
Section Start Marker







                       SSL/TLS Weak Symbols for TLS Support
        TLS Support (weak functions, to bea overriden by library wrapper)







***************************************************************************** */
#ifndef H_FACIL_IO_H   /* Development inclusion - ignore line */
#include "0011 base.c" /* Development inclusion - ignore line */
#endif                 /* Development inclusion - ignore line */
/* *****************************************************************************
TLS Support
***************************************************************************** */
#if !FIO_TLS_FOUND || FIO_WEAK_TLS /* TODO: list flags here */

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
      .private_key = FIO_STR_INIT,
      .public_key = FIO_STR_INIT,
      .password = FIO_STR_INIT,
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

#define FIO_ARRAY_NAME                 fio___tls_cert_ary
#define FIO_ARRAY_TYPE                 cert_s
#define FIO_ARRAY_TYPE_CMP(k1, k2)     (fio___tls_cert_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_cert_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key)    fio___tls_cert_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID         ((cert_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE  1
#define FIO_MALLOC_TMP_USE_SYSTEM      1
#include <fio-stl.h>

typedef struct {
  fio_str_s pem;
} trust_s;

static inline int fio___tls_trust_cmp(const trust_s *dest, const trust_s *src) {
  return fio_str_is_eq(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_copy(trust_s *dest, trust_s *src) {
  *dest = (trust_s){
      .pem = FIO_STR_INIT,
  };
  fio_str_concat(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_destroy(trust_s *obj) {
  fio_str_destroy(&obj->pem);
}

#define FIO_ARRAY_NAME                 fio___tls_trust_ary
#define FIO_ARRAY_TYPE                 trust_s
#define FIO_ARRAY_TYPE_CMP(k1, k2)     (fio___tls_trust_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_trust_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key)    fio___tls_trust_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID         ((trust_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE  1
#define FIO_MALLOC_TMP_USE_SYSTEM      1
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
      .name = FIO_STR_INIT,
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

#define FIO_OMAP_NAME                fio___tls_alpn_list
#define FIO_MAP_TYPE                 alpn_s
#define FIO_MAP_TYPE_INVALID         ((alpn_s){.udata_tls = NULL})
#define FIO_MAP_TYPE_INVALID_SIMPLE  1
#define FIO_MAP_TYPE_CMP(k1, k2)     fio_alpn_cmp(&(k1), &(k2))
#define FIO_MAP_TYPE_COPY(dest, obj) fio_alpn_copy(&(dest), &(obj))
#define FIO_MAP_TYPE_DESTROY(key)    fio_alpn_destroy(&(key))
#define FIO_MALLOC_TMP_USE_SYSTEM    1
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
  alpn_s tmp = {.udata_tls = NULL};
  fio_str_init_const(&tmp.name, name, len);
  alpn_s *pos =
      fio___tls_alpn_list_get_ptr(&tls->alpn, fio_str_hash(&tmp.name, 0), tmp);
  return pos;
}

/** Adds an ALPN data object to the ALPN "list" (set) */
FIO_IFUNC void fio___tls_alpn_add(fio_tls_s *tls,
                                  const char *protocol_name,
                                  void (*on_selected)(intptr_t uuid,
                                                      void *udata_connection,
                                                      void *udata_tls),
                                  void *udata_tls,
                                  void (*on_cleanup)(void *udata_tls)) {
  alpn_s tmp = {
      .name = FIO_STR_INIT,
      .on_selected = on_selected,
      .udata_tls = udata_tls,
      .on_cleanup = on_cleanup,
  };
  if (protocol_name)
    fio_str_init_const(&tmp.name, protocol_name, strlen(protocol_name));
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
  FIO_MAP_EACH(fio___tls_alpn_list, &tls->alpn, pos) { return &pos->obj; }
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
FIO_IFUNC void fio___tls_alpn_select(alpn_s *alpn,
                                     intptr_t uuid,
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
  FIO_ARRAY_EACH(fio___tls_cert_ary, &tls->sni, pos) {
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
  FIO_MAP_EACH(fio___tls_alpn_list, &tls->alpn, pos) {
    fio_str_info_s name = fio_str_info(&pos->obj.name);
    (void)name;
    // map to pos->callback;
  }

  /* Peer Verification / Trust */
  if (fio___tls_trust_ary_count(&tls->trust)) {
    /* TODO: enable peer verification */

    /* TODO: Add each ceriticate in the PEM to the trust "store" */
    FIO_ARRAY_EACH(fio___tls_trust_ary, &tls->trust, pos) {
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
static ssize_t fio_tls_read(intptr_t uuid,
                            void *udata,
                            void *buf,
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
static ssize_t fio_tls_write(intptr_t uuid,
                             void *udata,
                             const void *buf,
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
    fio___tls_alpn_select(fio___tls_alpn_default(tlsbuf->tls),
                          -1,
                          NULL /* ALPN udata */);
  }
  fio_tls_free(tlsbuf->tls); /* manage reference count */
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
    FIO_LOG_DEBUG("Something went wrong when updating the TLS hooks for %p",
                  (void *)uuid);
  }
  return 1;
}

static ssize_t fio_tls_read4handshake(intptr_t uuid,
                                      void *udata,
                                      void *buf,
                                      size_t count) {
  FIO_LOG_DEBUG("TLS handshake from read %p", (void *)uuid);
  if (fio_tls_handshake(uuid, udata))
    return fio_tls_read(uuid, udata, buf, count);
  errno = EWOULDBLOCK;
  return -1;
}

static ssize_t fio_tls_write4handshake(intptr_t uuid,
                                       void *udata,
                                       const void *buf,
                                       size_t count) {
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

static inline void fio_tls_attach2uuid(intptr_t uuid,
                                       fio_tls_s *tls,
                                       void *udata,
                                       uint8_t is_server) {
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
  fio_rw_hook_set(uuid,
                  &FIO_TLS_HANDSHAKE_HOOKS,
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
fio_tls_s *FIO_TLS_WEAK fio_tls_new(const char *server_name,
                                    const char *cert,
                                    const char *key,
                                    const char *pk_password) {
  REQUIRE_TLS_LIBRARY();
  fio_tls_s *tls = calloc(sizeof(*tls), 1);
  tls->ref = 1;
  fio_tls_cert_add(tls, server_name, key, cert, pk_password);
  return tls;
}

/**
 * Increase the reference count for the TLS object.
 *
 * Decrease with `fio_tls_free`.
 */
void FIO_TLS_WEAK fio_tls_dup(fio_tls_s *tls) {
  if (tls)
    fio_atomic_add(&tls->ref, 1);
}

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
void FIO_TLS_WEAK fio_tls_free(fio_tls_s *tls) {
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

/**
 * Adds a certificate  a new SSL/TLS context / settings object.
 */
void FIO_TLS_WEAK fio_tls_cert_add(fio_tls_s *tls,
                                   const char *server_name,
                                   const char *cert,
                                   const char *key,
                                   const char *pk_password) {
  REQUIRE_TLS_LIBRARY();
  cert_s c = {
      .private_key = FIO_STR_INIT,
      .public_key = FIO_STR_INIT,
      .password = FIO_STR_INIT,
  };
  if (pk_password)
    fio_str_init_const(&c.password, pk_password, strlen(pk_password));
  if (key && cert) {
    if (fio_str_readfile(&c.private_key, key, 0, 0).buf == NULL)
      goto file_missing;
    if (fio_str_readfile(&c.public_key, cert, 0, 0).buf == NULL)
      goto file_missing;
    fio___tls_cert_ary_push(&tls->sni, c);
  } else if (server_name) {
    /* Self-Signed TLS Certificates */
    fio_str_init_const(&c.private_key, server_name, strlen(server_name));
    fio___tls_cert_ary_push(&tls->sni, c);
  }
  fio___tls_cert_destroy(&c);
  fio___tls_build_context(tls);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for either %s or %s or both.",
                key,
                cert);
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
void FIO_TLS_WEAK fio_tls_alpn_add(fio_tls_s *tls,
                                   const char *protocol_name,
                                   void (*on_selected)(intptr_t uuid,
                                                       void *udata_connection,
                                                       void *udata_tls),
                                   void *udata_tls,
                                   void (*on_cleanup)(void *udata_tls)) {
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
      .pem = FIO_STR_INIT,
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
  fio_timeout_set(uuid, FIO_TLS_TIMEOUT);
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
  /* TODO?: handle SNI */
}

#endif /* Library compiler flags */
/* *****************************************************************************
Test
***************************************************************************** */
#ifdef TEST
void fio_test(void) {
  /* switch to test data set */
  struct fio___data_s *old = fio_data;
  fio_protocol_s proto = {0};
  protocol_validate(&proto);
  fio_data = fio_malloc(sizeof(*old) + (sizeof(old->info[0]) * 128));
  fio_data->capa = 128;
  fd_data(1).protocol = &proto;
  fprintf(stderr, "Starting facil.io IO core tests.\n");

  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, state)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, tasks)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, env)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, protocol)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, rw_hooks)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, sock)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(io, postoffice)();

  /* free test data set and return normal data set */
  fio_free(fio_data);
  fio_data = old;
  fprintf(stderr, "===============\n");
  fprintf(stderr, "Done.\n");
}
#endif /* TEST */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0999 init.c
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#include "0020 reactor.c" /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************







Library initialization / cleanup







***************************************************************************** */

/* *****************************************************************************
Destroy the State Machine
***************************************************************************** */

static void __attribute__((destructor)) fio___data_free(void) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_state_callback_clear_all();
  fio_poll_close();
  fio_timer_destroy(&fio___timer_tasks);
  fio_queue_destroy(&fio___task_queue);
  free(fio_data);
}

/* *****************************************************************************
Initialize the State Machine
***************************************************************************** */

static void __attribute__((constructor)) fio___data_new(void) {
  ssize_t capa = 0;
  struct rlimit rlim = {.rlim_max = 0};
  {
#ifdef _SC_OPEN_MAX
    capa = sysconf(_SC_OPEN_MAX);
#elif defined(FOPEN_MAX)
    capa = FOPEN_MAX;
#endif
    // try to maximize limits - collect max and set to max
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
      FIO_LOG_WARNING("`getrlimit` failed in facil.io core initialization.");
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
  }
  fio_data = calloc(1, sizeof(*fio_data) + (sizeof(fio_data->info[0]) * capa));
  FIO_ASSERT_ALLOC(fio_data);
  fio_data->capa = capa;
  fio_data->pid = fio_data->parent = getpid();
  fio_data->is_master = 1;
  fio_data->lock = FIO_LOCK_INIT;
  fio_mark_time();
  fio_poll_init();
  FIO_LOG_PRINT__(
      FIO_LOG_LEVEL_DEBUG,
      "facil.io " FIO_VERSION_STRING " (%s) initialization:\n"
      "\t* Maximum open files %zu out of %zu\n"
      "\t* Allocating %zu bytes for state handling.\n"
      "\t* %zu bytes per connection + %zu bytes for state handling.",
      fio_engine(),
      capa,
      (size_t)rlim.rlim_max,
      sizeof(*fio_data) + (sizeof(fio_data->info[0]) * capa),
      sizeof(fio_data->info[0]),
      sizeof(*fio_data));
  fio_pubsub_init();
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
  fio_state_callback_clear(FIO_CALL_ON_INITIALIZE);
}
