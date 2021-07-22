/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
External STL features published
***************************************************************************** */
#define FIO_EXTERN_COMPLETE   1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>
/* *****************************************************************************
Quick Patches
***************************************************************************** */
#if FIO_OS_WIN
#ifndef fork
#define fork() ((pid_t)(-1))
#endif
#ifndef waitpid
#define waitpid(...) ((pid_t)(-1))
#endif
#ifndef WIFEXITED
#define WIFEXITED(...) (-1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(...) (-1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(...) (-1)
#endif
#ifndef pipe
#define pipe(pfd) _pipe(pfd, 0, _O_BINARY)
#endif
#ifndef close
#define _close
#endif
#endif
/* *****************************************************************************
Internal helpers
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

#define FIO_QUEUE
#define FIO_SOCK /* should be public? */
#define FIO_STREAM
#include "fio-stl.h"

/* *****************************************************************************
Polling Helpers
***************************************************************************** */

#if !defined(FIO_ENGINE_EPOLL) && !defined(FIO_ENGINE_KQUEUE) &&               \
    !defined(FIO_ENGINE_POLL)
#if defined(HAVE_EPOLL) || __has_include("sys/epoll.h")
#define FIO_ENGINE_EPOLL 1
#elif (defined(HAVE_KQUEUE) || __has_include("sys/event.h"))
#define FIO_ENGINE_KQUEUE 1
#else
#define FIO_ENGINE_POLL 1
#endif
#endif /* !defined(FIO_ENGINE_EPOLL) ... */

FIO_IFUNC void fio_uuid_monitor_close(void);
FIO_IFUNC void fio_uuid_monitor_init(void);
FIO_SFUNC size_t fio_uuid_monitor_review(void);
FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid);
FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid);
FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid);
FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid);

/* *****************************************************************************
Queue Helpers
***************************************************************************** */
static fio_queue_s tasks_io_core = FIO_QUEUE_STATIC_INIT(tasks_io_core);
static fio_queue_s tasks_user = FIO_QUEUE_STATIC_INIT(tasks_user);

FIO_IFUNC fio_queue_s *fio_queue_select(uintptr_t flag) {
  fio_queue_s *queues[] = {
      &tasks_user,
      &tasks_io_core,
  };
  return queues[(flag & 1)];
}

/* *****************************************************************************
ENV data maps
***************************************************************************** */

/** An object that can be linked to any facil.io connection (uuid). */
typedef struct {
  void (*on_close)(void *data);
  void *udata;
  uintptr_t flags;
} fio_uuid_env_obj_s;

/* cleanup event task */
static void fio_uuid_env_obj_call_callback_task(void *p, void *udata) {
  union {
    void (*fn)(void *);
    void *p;
  } u = {.p = p};
  u.fn(udata);
}

/* cleanup event scheduling */
FIO_IFUNC void fio_uuid_env_obj_call_callback(fio_uuid_env_obj_s o) {
  union {
    void (*fn)(void *);
    void *p;
  } u = {.fn = o.on_close};
  if (o.on_close) {
    fio_queue_push_urgent(fio_queue_select(o.flags),
                          fio_uuid_env_obj_call_callback_task,
                          u.p,
                          o.udata);
  }
}

/* for storing env string keys */
#define FIO_STR_SMALL fio_str
#include "fio-stl.h"

#define FIO_UMAP_NAME              fio___uuid_env
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
UUID validity, needed?
***************************************************************************** */

#define FIO_UMAP_NAME          fio_uuid_validity_map
#define FIO_MAP_TYPE           fio_uuid_s *
#define FIO_MAP_TYPE           fio_uuid_s *
#define FIO_MAP_HASH_FN(o)     fio_risky_ptr(o)
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))
#include <fio-stl.h>
static fio_uuid_validity_map_s fio_uuid_validity_map = FIO_MAP_INIT;

FIO_IFUNC void fio_uuid_set_valid(fio_uuid_s *uuid) {
  fio_uuid_validity_map_set(&fio_uuid_validity_map,
                            fio_risky_ptr(uuid),
                            uuid,
                            NULL);
}

FIO_IFUNC void fio_uuid_set_invalid(fio_uuid_s *uuid) {
  fio_uuid_validity_map_remove(&fio_uuid_validity_map,
                               fio_risky_ptr(uuid),
                               uuid,
                               NULL);
}

FIO_IFUNC fio_uuid_s *fio_uuid_is_valid(fio_uuid_s *uuid) {
  return fio_uuid_validity_map_get(&fio_uuid_validity_map,
                                   fio_risky_ptr(uuid),
                                   uuid);
}

FIO_IFUNC void fio_uuid_invalidate_all() {
  fio_uuid_validity_map_destroy(&fio_uuid_validity_map);
}

/* *****************************************************************************
Global State
***************************************************************************** */

static struct {
  FIO_LIST_HEAD protocols;
  fio_thread_mutex_t env_lock;
  fio___uuid_env_s env;
  struct timespec tick;
  fio_uuid_s *io_wake_uuid;
  struct {
    int in;
    int out;
  } thread_suspenders, io_wake;
  pid_t master;
  uint16_t threads;
  uint16_t workers;
  uint8_t is_master;
  uint8_t is_worker;
  volatile uint8_t running;
} fio_data = {
    .thread_suspenders = {-1, -1},
    .io_wake = {-1, -1},
    .env_lock = FIO_THREAD_MUTEX_INIT,
    .is_master = 1,
    .is_worker = 1,
};

FIO_IFUNC void fio_reset_wakeup_pipes(void *i_) {
  int *i = i_;
  if (i[0] > 0) {
    close(i[0]);
    close(i[1]);
  }
  FIO_ASSERT(!pipe(i),
             "%d - couldn't initiate thread wakeup pipes.",
             (int)getpid());
  fio_sock_set_non_block(i[0]);
  fio_sock_set_non_block(i[1]);
}

FIO_DESTRUCTOR(fio_cleanup_at_exit) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_thread_mutex_destroy(&fio_data.env_lock);
  fio___uuid_env_destroy(&fio_data.env);
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_uuid_monitor_close();
  if (fio_data.io_wake_uuid) {
    fio_uuid_free(fio_data.io_wake_uuid);
    fio_data.io_wake_uuid = NULL;
  }
  while (fio_queue_count(&tasks_io_core) + fio_queue_count(&tasks_user)) {
    fio_queue_perform_all(&tasks_io_core);
    fio_queue_perform_all(&tasks_user);
  }
  close(fio_data.thread_suspenders.in);
  close(fio_data.thread_suspenders.out);
  close(fio_data.io_wake.in);
  close(fio_data.io_wake.out);
  fio_data.thread_suspenders.in = -1;
  fio_data.thread_suspenders.out = -1;
  fio_data.io_wake.in = -1;
  fio_data.io_wake.out = -1;
  fio_uuid_monitor_close();
  fio_cli_end();
  fio_uuid_invalidate_all();
  for (int i = 0; i < FIO_CALL_NEVER; ++i)
    fio_state_callback_clear((callback_type_e)i);
}

static void fio_cleanup_after_fork(void *ignr_);

FIO_CONSTRUCTOR(fio_data_state_init) {
  FIO_LOG_DEBUG("initializeing facio.io IO state.");
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.master = getpid();
  fio_data.tick = fio_time_real();
  fio_uuid_monitor_init();
#if FIO_OS_POSIX
  fio_reset_wakeup_pipes(&fio_data.thread_suspenders.in);
  fio_reset_wakeup_pipes(&fio_data.io_wake.in);
#else
  fio_state_callback_add(FIO_CALL_PRE_START,
                         fio_reset_wakeup_pipes,
                         &fio_data.thread_suspenders.in);
  fio_state_callback_add(FIO_CALL_PRE_START,
                         fio_reset_wakeup_pipes,
                         &fio_data.io_wake.in);
#endif
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio_cleanup_after_fork, NULL);
}

/* *****************************************************************************
Thread suspension helpers
***************************************************************************** */

FIO_SFUNC void fio_user_thread_suspent(void) {
  short e = fio_sock_wait_io(fio_data.thread_suspenders.in, POLLIN, 2000);
  if (e != -1 && (e & POLLIN)) {
    char buf[512];
    int r = fio_sock_read(fio_data.thread_suspenders.in, buf, 512);
    (void)r;
  }
}

FIO_IFUNC void fio_user_thread_wake(void) {
  char buf[1] = {0};
  fio_sock_write(fio_data.thread_suspenders.out, buf, 1);
}

FIO_IFUNC void fio_user_thread_wake_all(void) {
  fio_reset_wakeup_pipes(&fio_data.thread_suspenders.in);
}

/* *****************************************************************************
Signal Helpers
***************************************************************************** */
#define FIO_SIGNAL
#include "fio-stl.h"
FIO_IFUNC void fio_io_thread_wake(void);
/* Handles signals */
static void fio___stop_signal_handler(int sig, void *ignr_) {
  static int64_t last_signal = 0;
  if (last_signal + 2000 >= fio_time2milli(fio_data.tick))
    return;
  last_signal = fio_time2milli(fio_data.tick);
  if (fio_data.running) {
    fio_data.running = 0;
    FIO_LOG_INFO("(%d) received shutdown signal.", getpid());
    if (fio_data.is_master && fio_data.workers) {
      kill(0, sig);
    }
    fio_io_thread_wake();
    fio_user_thread_wake_all();
  } else {
    FIO_LOG_FATAL("(%d) received another shutdown signal while shutting down.",
                  getpid());
    exit(-1);
  }
  (void)ignr_;
}

/* Handles signals */
static void fio___worker_reset_signal_handler(int sig, void *ignr_) {
  if (!fio_data.workers || !fio_data.running)
    return;
  static int64_t last_signal = 0;
  if (last_signal + 2000 >= fio_time2milli(fio_data.tick))
    return;
  last_signal = fio_time2milli(fio_data.tick);
  if (fio_data.is_worker) {
    fio_data.running = 0;
    FIO_LOG_INFO("(%d) received worker restart signal.", getpid());
  } else {
    FIO_LOG_INFO("(%d) forwarding worker restart signal.",
                 (int)fio_data.master);
    kill(0, sig);
  }
  (void)ignr_;
}

/* *****************************************************************************
UUID data types
***************************************************************************** */

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

typedef enum {
  FIO_UUID_OPEN = 1,          /* 0b0001 */
  FIO_UUID_SUSPENDED_BIT = 2, /* 0b0010 */
  FIO_UUID_SUSPENDED = 3,     /* 0b0011 */
  FIO_UUID_CLOSED_BIT = 4,    /* 0b0100 */
  FIO_UUID_CLOSED = 6,        /* 0b0110 */
  FIO_UUID_CLOSING = 7,       /* 0b0111 */
} fio_uuid_state_e;

struct fio_uuid_s {
  /* fd protocol */
  fio_protocol_s *protocol;
  /* timeout review linked list */
  FIO_LIST_NODE timeouts;
  /** RW hooks. */
  fio_tls_s *tls;
  /* user udata */
  void *udata;
  /* current data to be send */
  fio_stream_s stream;
  /* Objects linked to the uuid */
  fio___uuid_env_s env;
  /* timer handler */
  int64_t active;
  /* socket */
  int fd;
  /** Connection is open */
  volatile fio_uuid_state_e state;
  /** Task lock */
  fio_lock_i lock;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
};
FIO_IFUNC fio_uuid_s *fio_uuid_dup2(fio_uuid_s *uuid);
FIO_IFUNC int fio_uuid_free2(fio_uuid_s *uuid);

FIO_SFUNC void fio___touch(void *uuid_, void *should_free) {
  fio_uuid_s *uuid = uuid_;
  uuid->active = fio_time2milli(fio_data.tick);
  FIO_LIST_REMOVE(&uuid->timeouts);
  FIO_LIST_PUSH(uuid->protocol->reserved.uuids.prev, &uuid->timeouts);
  // FIO_LOG_DEBUG("touched %p (fd %d)", uuid_, uuid->fd);
  FIO_LIST_REMOVE(&uuid->protocol->reserved.protocols);
  FIO_LIST_PUSH(fio_data.protocols.prev, &uuid->protocol->reserved.protocols);
  if (should_free)
    fio_uuid_free2(uuid);
}

FIO_SFUNC void fio___deferred_on_close(void *fn, void *udata) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn};
  u.fn(udata);
}

FIO_SFUNC void fio_uuid___init_task(void *uuid_, void *ignr_) {
  fio_uuid_s *const uuid = uuid_;
  fio_uuid_set_valid(uuid);
  fio_uuid_free2(uuid);
  (void)ignr_;
}

FIO_SFUNC void fio_uuid___init(fio_uuid_s *uuid) {
  *uuid = (fio_uuid_s){
      .state = FIO_UUID_OPEN,
      .stream = FIO_STREAM_INIT(uuid->stream),
      .timeouts = FIO_LIST_INIT(uuid->timeouts),
      .active = fio_time2milli(fio_data.tick),
  };
  fio_queue_push_urgent(&tasks_io_core,
                        .fn = fio_uuid___init_task,
                        .udata1 = fio_uuid_dup2(uuid));
  FIO_LOG_DEBUG("UUID %p initialized.", (void *)uuid);
}

FIO_SFUNC void fio_uuid___destroy(fio_uuid_s *uuid) {
  fio_uuid_set_invalid(uuid);
  fio___uuid_env_destroy(&uuid->env);
  fio_stream_destroy(&uuid->stream);
  // o->rw_hooks->cleanup(uuid->rw_udata);
  FIO_LIST_REMOVE(&uuid->timeouts);
  union {
    void *p;
    void (*fn)(void *);
  } u = {.fn = uuid->protocol->on_close};
  fio_queue_push(&tasks_user, fio___deferred_on_close, u.p, uuid->udata);
#if FIO_ENGINE_POLL
  fio_uuid_monitor_remove(uuid);
#endif
  fio_sock_close(uuid->fd);
  if (uuid->protocol->reserved.uuids.next ==
      uuid->protocol->reserved.uuids.prev) {
    FIO_LIST_REMOVE(&uuid->protocol->reserved.protocols);
  }
  FIO_LOG_DEBUG("UUID %p (fd %d) being freed.", (void *)uuid, (int)(uuid->fd));
}

#define FIO_REF_NAME         fio_uuid
#define FIO_REF_INIT(obj)    fio_uuid___init(&(obj))
#define FIO_REF_DESTROY(obj) fio_uuid___destroy(&(obj))
#include <fio-stl.h>

FIO_SFUNC void fio_uuid_free_task(void *uuid, void *ignr) {
  (void)ignr;
  fio_uuid_free2(uuid);
}

fio_uuid_s *fio_uuid_dup(fio_uuid_s *uuid) { return fio_uuid_dup2(uuid); }

#define fio_uuid_dup fio_uuid_dup2

void fio_uuid_free(fio_uuid_s *uuid) {
  fio_queue_push(&tasks_io_core, .fn = fio_uuid_free_task, .udata1 = uuid);
}

FIO_IFUNC void fio_uuid_close(fio_uuid_s *uuid) {
  if (fio_atomic_and(&uuid->state,
                     ((fio_uuid_state_e)(~(unsigned)FIO_UUID_OPEN))) &
      FIO_UUID_OPEN) {
    uuid->state |= FIO_UUID_CLOSED;
    fio_uuid_monitor_remove(uuid);
    fio_uuid_free(uuid);
  }
}

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void fio_ev_on_shutdown(void *uuid_, void *udata);
static void fio_ev_on_ready(void *uuid_, void *udata);
static void fio_ev_on_data(void *uuid_, void *udata);
static void fio_ev_on_timeout(void *uuid_, void *udata);
static void mock_ping_eternal(fio_uuid_s *uuid, void *udata);

/* *****************************************************************************
Event deferring (mock functions)
***************************************************************************** */

static void mock_on_ev(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
  (void)udata;
}
static void mock_on_data(fio_uuid_s *uuid, void *udata) {
  if ((uuid->state & FIO_UUID_OPEN))
    uuid->state |= FIO_UUID_SUSPENDED;
  (void)udata;
}
static uint8_t mock_on_shutdown(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
  (void)udata;
  return 0;
}
static uint8_t mock_on_shutdown_eternal(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
  (void)udata;
  return (uint8_t)-1;
}
static void mock_timeout(fio_uuid_s *uuid, void *udata) {
  fio_close(uuid);
  (void)uuid;
  (void)udata;
}
static void mock_ping_eternal(fio_uuid_s *uuid, void *udata) {
  fio_touch(uuid);
  (void)udata;
}

FIO_IFUNC void fio_protocol_validate(fio_protocol_s *p) {
  if (p && !(p->reserved.flags & 8)) {
    if (!p->on_data)
      p->on_data = mock_on_data;
    if (!p->on_timeout)
      p->on_timeout = mock_timeout;
    if (!p->on_ready)
      p->on_ready = mock_on_ev;
    if (!p->on_shutdown)
      p->on_shutdown = mock_on_shutdown;
    p->reserved.flags = 8;
    p->reserved.protocols = FIO_LIST_INIT(p->reserved.protocols);
    p->reserved.uuids = FIO_LIST_INIT(p->reserved.uuids);
  }
}

/* *****************************************************************************
IO thread wakeup protocol
***************************************************************************** */

static void fio_io_wakeup_on_data(fio_uuid_s *uuid, void *udata) {
  char buf[1024];
  ssize_t l;
  while ((l = fio_sock_read(uuid->fd, buf, 1024)) > 0)
    ;
  (void)udata;
}

static void fio_io_wakeup_on_close(void *udata) {
  fio_data.io_wake_uuid = NULL;
  (void)udata;
}

static fio_protocol_s FIO_IO_WAKEUP_PROTOCOL = {
    .on_data = fio_io_wakeup_on_data,
    .on_timeout = mock_ping_eternal,
    .on_shutdown = mock_on_shutdown_eternal,
    .on_close = fio_io_wakeup_on_close,
};

FIO_SFUNC void fio_io_wakeup_prep(void) {
  if (fio_data.io_wake_uuid)
    return;
  fio_uuid_s *uuid = fio_data.io_wake_uuid = fio_uuid_new2();
  FIO_ASSERT_ALLOC(uuid);
  uuid->tls = NULL;
  uuid->fd = fio_data.io_wake.in;
  fio_protocol_validate(&FIO_IO_WAKEUP_PROTOCOL);
  FIO_IO_WAKEUP_PROTOCOL.reserved.flags |= 1; /* system protocol */
  uuid->protocol = &FIO_IO_WAKEUP_PROTOCOL;
  fio___touch(uuid, NULL);
  fio_uuid_monitor_add_read(uuid);
}

FIO_IFUNC void fio_io_thread_wake(void) {
  char buf[1] = {0};
  fio_sock_write(fio_data.io_wake.out, buf, 1);
}

FIO_IFUNC void fio_io_thread_wake_clear(void) {
  char buf[1024];
  ssize_t l;
  while ((l = fio_sock_read(fio_data.io_wake.in, buf, 1024)) > 0)
    ;
}

/* *****************************************************************************
Housekeeping cycle
***************************************************************************** */
FIO_SFUNC void fio___cycle_housekeeping(void) {
  static int old = 0;
  static time_t last_to_review = 0;
  int c = fio_uuid_monitor_review();
  fio_data.tick = fio_time_real();
  c += fio_signal_review();
  if (!c) {
    if (!old) {
      fio_state_callback_force(FIO_CALL_ON_IDLE);
    }
#if FIO_OS_POSIX
    if (!fio_data.is_master && fio_data.running &&
        getppid() != fio_data.master) {
      fio_data.running = 0;
      FIO_LOG_FATAL("(%d) parent process (%d != %d) seems to have crashed",
                    getpid(),
                    (int)fio_data.master,
                    (int)getppid());
    }
#endif
  }
  old = c;

  /* test timeouts? */
  if (last_to_review != fio_data.tick.tv_sec) {
    last_to_review = fio_data.tick.tv_sec;
    FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
      if (!pr->timeout || pr->timeout > FIO_UUID_TIMEOUT_MAX)
        pr->timeout = FIO_UUID_TIMEOUT_MAX;
      time_t limit =
          fio_time2milli(fio_data.tick) - ((time_t)pr->timeout * 1000);
      FIO_LIST_EACH(fio_uuid_s, timeouts, &pr->reserved.uuids, uuid) {
        if (uuid->active >= limit)
          break;
        fio_queue_push_urgent(fio_queue_select(pr->reserved.flags),
                              .fn = fio_ev_on_timeout,
                              .udata1 = fio_uuid_dup(uuid),
                              .udata2 = NULL);
        FIO_LOG_DEBUG("scheduling timeout for %p", (void *)uuid);
      }
    }
  }
  /* what if there were no other events and timeouts were scheduled?  */
  fio_user_thread_wake();
}

FIO_SFUNC void fio___cycle_housekeeping_running(void) {
  if (!fio_data.io_wake_uuid)
    fio_io_wakeup_prep();
  fio___cycle_housekeeping();
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio_cleanup_after_fork(void *ignr_) {
  (void)ignr_;
  if (!fio_data.is_master) {
    fio_uuid_monitor_close();
    fio_uuid_monitor_init();
  }

  fio_reset_wakeup_pipes(&fio_data.thread_suspenders.in);
  fio_reset_wakeup_pipes(&fio_data.io_wake.in);

  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_uuid_s, timeouts, &pr->reserved.uuids, uuid) {
      FIO_LOG_DEBUG("cleanup for fd %d", uuid->fd);
      // fio_sock_close(uuid->fd);
      // uuid->fd = -1;
      uuid->state = FIO_UUID_CLOSED;
      fio_uuid_free2(uuid);
    }
    FIO_LIST_REMOVE(&pr->reserved.protocols);
  }
}

static void fio_cleanup_start_shutdown() {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_uuid_s, timeouts, &pr->reserved.uuids, uuid) {
      fio_queue_push_urgent(fio_queue_select(pr->reserved.flags),
                            .fn = fio_ev_on_shutdown,
                            .udata1 = fio_uuid_dup(uuid),
                            .udata2 = NULL);
    }
  }
}
/* *****************************************************************************
Copy address to string
***************************************************************************** */

FIO_SFUNC void fio_tcp_addr_cpy(fio_uuid_s *uuid,
                                int family,
                                struct sockaddr *addrinfo) {
  const char *result =
      inet_ntop(family,
                family == AF_INET
                    ? (void *)&(((struct sockaddr_in *)addrinfo)->sin_addr)
                    : (void *)&(((struct sockaddr_in6 *)addrinfo)->sin6_addr),
                (char *)uuid->addr,
                sizeof(uuid->addr));
  if (result) {
    uuid->addr_len = strlen((char *)uuid->addr);
  } else {
    uuid->addr_len = 0;
    uuid->addr[0] = 0;
  }
}

/* *****************************************************************************
Misc Helpers
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
  cpu_count = FIO_CPU_CORES_FALLBACK;
#endif
  return cpu_count;
}
/* *****************************************************************************
Internal helpers
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void fio_ev_on_shutdown(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if (fio_trylock(&uuid->lock))
    goto reschedule;
  if (!uuid->protocol->on_shutdown(uuid, uuid->udata))
    uuid->state = FIO_UUID_CLOSING;
  fio_unlock(&uuid->lock);
  fio_uuid_free(uuid);
  return;
reschedule:
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_shutdown,
                 .udata1 = uuid,
                 .udata2 = udata);
}

static void fio_ev_on_ready_user(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    if (fio_trylock(&uuid->lock))
      goto reschedule;
    uuid->protocol->on_ready(uuid, uuid->udata);
    fio_unlock(&uuid->lock);
  }
  fio_uuid_free(uuid);
  return;
reschedule:
  fio_queue_push_urgent(fio_queue_select(uuid->protocol->reserved.flags),
                        .fn = fio_ev_on_ready_user,
                        .udata1 = uuid,
                        .udata2 = udata);
}

static void fio_ev_on_ready(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    char buf_mem[FIO_SOCKET_BUFFER_PER_WRITE];
    size_t total = 0;
    for (;;) {
      size_t len = FIO_SOCKET_BUFFER_PER_WRITE;
      char *buf = buf_mem;
      fio_stream_read(&uuid->stream, &buf, &len);
      if (!len)
        break;
      ssize_t r = fio_sock_write(uuid->fd, buf, len);
      if (r > 0) {
        total += r;
        fio_stream_advance(&uuid->stream, len);
      } else if (r == 0 ||
                 (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)) {
        fio_uuid_close(uuid);
        goto finish;
      } else {
        break;
      }
    }
    if (total)
      fio___touch(uuid, NULL);
    if (!fio_stream_any(&uuid->stream)) {
      if ((uuid->state & FIO_UUID_CLOSED_BIT)) {
        fio_uuid_free(uuid); /* free the UUID again to close it..? */
      } else {
        fio_queue_push_urgent(fio_queue_select(uuid->protocol->reserved.flags),
                              .fn = fio_ev_on_ready_user,
                              .udata1 = fio_uuid_dup(uuid),
                              .udata2 = udata);
        fio_user_thread_wake();
      }
    } else {
      fio_uuid_monitor_add_write(uuid);
    }
  }
finish:
  fio_uuid_free(uuid);
}

static void fio_ev_on_data(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    if (fio_trylock(&uuid->lock))
      goto reschedule;
    uuid->protocol->on_data(uuid, uuid->udata);
    fio_unlock(&uuid->lock);
    if (!(uuid->state & FIO_UUID_CLOSED)) {
      /* this also tests for the suspended flag (0x02) */
      fio_uuid_monitor_add_read(uuid);
    }
  } else {
    FIO_LOG_DEBUG("skipping on_data callback for uuid %p (closed?)", uuid_);
  }
  fio_uuid_free(uuid);
  return;
reschedule:
  FIO_LOG_DEBUG("rescheduling on_data for uuid %p", uuid_);
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 .fn = fio_ev_on_data,
                 .udata1 = fio_uuid_dup(uuid),
                 .udata2 = udata);
}

static void fio_ev_on_timeout(void *uuid_, void *udata) {
  fio_uuid_s *const uuid = uuid_;
  if ((uuid->state & FIO_UUID_OPEN)) {
    uuid->protocol->on_timeout(uuid, uuid->udata);
  }
  fio_uuid_free(uuid);
  return;
  (void)udata;
}
/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO Polling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 993
#endif

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

#if FIO_ENGINE_POLL
/* fio_poll_remove might not work when polling from multiple threads */
#define FIO_POLL_EV_VALIDATE_UUID(uuid)                                        \
  do {                                                                         \
    fio_queue_perform_all(&tasks_io_core);                                     \
    if (!fio_uuid_is_valid(uuid)) {                                            \
      FIO_LOG_DEBUG("uuid validation failed for uuid %p", (void *)uuid);       \
      return;                                                                  \
    }                                                                          \
  } while (0);
#else
#define FIO_POLL_EV_VALIDATE_UUID(uuid)
#endif

FIO_IFUNC void fio___poll_ev_wrap_data(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  // FIO_LOG_DEBUG("event on_data detected for uuid %p", uuid_);
  FIO_POLL_EV_VALIDATE_UUID(uuid);
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 fio_ev_on_data,
                 fio_uuid_dup(uuid),
                 uuid->udata);
  fio_user_thread_wake();
  (void)fd;
}
FIO_IFUNC void fio___poll_ev_wrap_ready(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  // FIO_LOG_DEBUG("event on_ready detected for uuid %p", uuid_);
  FIO_POLL_EV_VALIDATE_UUID(uuid);
  fio_queue_push(&tasks_io_core,
                 fio_ev_on_ready,
                 fio_uuid_dup(uuid),
                 uuid->udata);
  (void)fd;
}

FIO_IFUNC void fio___poll_ev_wrap_close(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  // FIO_LOG_DEBUG("event on_close detected for uuid %p", uuid_);
  FIO_POLL_EV_VALIDATE_UUID(uuid);
  fio_uuid_close(uuid);
  (void)fd;
}

FIO_IFUNC int fio_uuid_monitor_tick_len(void) {
  return (FIO_POLL_TICK * fio_data.running) | 7;
}

/* *****************************************************************************
EPoll
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

FIO_IFUNC void fio_uuid_monitor_close(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_IFUNC void fio_uuid_monitor_init(void) {
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

FIO_IFUNC int fio___peoll_add2(fio_uuid_s *uuid, uint32_t events, int ep_fd) {
  int ret = -1;
  struct epoll_event chevent;
  int fd = uuid->fd;
  if (fd == -1)
    return ret;
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

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  fio___peoll_add2(uuid,
                   (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[1]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  fio___peoll_add2(uuid,
                   (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[2]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  if (fio___peoll_add2(uuid,
                       (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                       evio_fd[1]) == -1)
    return;
  fio___peoll_add2(uuid,
                   (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[2]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN),
                                .data.ptr = uuid};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, uuid, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, uuid, &chevent);
}

FIO_SFUNC size_t fio_uuid_monitor(void) {
  int timeout_millisec = fio_uuid_monitor_tick_len();
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
          fio___poll_ev_wrap_close(0, events[i].data.ptr);
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLOUT) {
            fio___poll_ev_wrap_ready(0, events[i].data.ptr);
          }
          if (events[i].events & EPOLLIN)
            fio___poll_ev_wrap_data(0, events[i].data.ptr);
        }
      } // end for loop
      total += active_count;
    }
  }
  return total;
}

/* *****************************************************************************
KQueue
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

FIO_IFUNC void fio_uuid_monitor_close(void) { close(evio_fd); }

FIO_IFUNC void fio_uuid_monitor_init(void) {
  fio_uuid_monitor_close();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  struct kevent chevent[1];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  struct kevent chevent[1];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  struct kevent chevent[2];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  EV_SET(chevent + 1,
         uuid->fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, 2, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid) {
  if (evio_fd < 0)
    return;
  struct kevent chevent[2];
  EV_SET(chevent, uuid->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, uuid->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  do {
    errno = 0;
    kevent(evio_fd, chevent, 2, NULL, 0, NULL);
  } while (errno == EINTR);
}

FIO_SFUNC size_t fio_uuid_monitor_review(void) {
  if (evio_fd < 0)
    return -1;
  int timeout_millisec = fio_uuid_monitor_tick_len();
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
        fio___poll_ev_wrap_ready(0, (void *)events[i].udata);
      } else if (events[i].filter == EVFILT_READ) {
        fio___poll_ev_wrap_data(0, (void *)events[i].udata);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio___poll_ev_wrap_close(0, (void *)events[i].udata);
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
Poll
***************************************************************************** */
#elif FIO_ENGINE_POLL

#define FIO_POLL
#include "fio-stl.h"

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "poll"; }

static fio_poll_s fio___poll_data = FIO_POLL_INIT(fio___poll_ev_wrap_data,
                                                  fio___poll_ev_wrap_ready,
                                                  fio___poll_ev_wrap_close);
FIO_IFUNC void fio_uuid_monitor_close(void) {
  fio_poll_destroy(&fio___poll_data);
}

FIO_IFUNC void fio_uuid_monitor_init(void) {
  fio___poll_data = (fio_poll_s)FIO_POLL_INIT(fio___poll_ev_wrap_data,
                                              fio___poll_ev_wrap_ready,
                                              fio___poll_ev_wrap_close);
}

FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid) {
  // FIO_LOG_DEBUG("IO monitor removing %p", uuid);
  fio_io_thread_wake();
  fio_poll_forget(&fio___poll_data, uuid->fd);
}

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  // FIO_LOG_DEBUG("IO monitor added read for %p (%d)", uuid, uuid->fd);
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLIN);
  fio_io_thread_wake();
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLOUT);
  fio_io_thread_wake();
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  // FIO_LOG_DEBUG("IO monitor adding %p (%d)", uuid, uuid->fd);
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLIN | POLLOUT);
  fio_io_thread_wake();
}

/** returns non-zero if events were scheduled, 0 if idle */
FIO_SFUNC size_t fio_uuid_monitor_review(void) {
  // FIO_LOG_DEBUG("IO monitor reviewing events with %zu sockets",
  //               fio___poll_fds_count(&fio___poll_data.fds));
  fio_io_thread_wake_clear();
  return fio_poll_review(&fio___poll_data, fio_uuid_monitor_tick_len());
}

#endif /* FIO_ENGINE_POLL */
/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

static void fio_spawn_worker(void *ignr_1, void *ignr_2);

/** Worker no-threads cycle */
static void fio___worker_nothread_cycle(void) {
  for (;;) {
    if (!fio_queue_perform(&tasks_io_core))
      continue;
    if (!fio_queue_perform(&tasks_user))
      continue;
    if (!fio_data.running)
      break;
    fio___cycle_housekeeping_running();
  }
  fio_cleanup_start_shutdown();
  for (;;) {
    if (!fio_queue_perform(&tasks_io_core))
      continue;
    if (!fio_queue_perform(&tasks_user))
      continue;
    break;
  }
}

/** Worker thread cycle */
static void *fio___user_thread_cycle(void *ignr) {
  (void)ignr;
  while (fio_data.running) {
    fio_queue_perform_all(&tasks_user);
    if (fio_data.running) {
      fio_user_thread_suspent();
    }
  }
  return NULL;
}
/** Worker cycle */
static void fio___worker_cycle(void) {
  while (fio_data.running) {
    fio___cycle_housekeeping_running();
    fio_queue_perform_all(&tasks_io_core);
  }
  fio_cleanup_start_shutdown();
  for (;;) {
    if (!fio_queue_perform(&tasks_io_core))
      continue;
    if (!fio_queue_perform(&tasks_user))
      continue;
    return;
  }
}

static void fio___worker_cleanup(void) {
  fio_cleanup_after_fork(NULL);
  do {
    fio___cycle_housekeeping();
    fio_queue_perform_all(&tasks_io_core);
    fio_queue_perform_all(&tasks_user);
  } while (fio_queue_count(&tasks_io_core) + fio_queue_count(&tasks_user));

  if (!fio_data.is_master)
    FIO_LOG_INFO("(%d) worker shutdown complete.", (int)getpid());
  else {
#if FIO_OS_POSIX
    /*
     * Wait for some of the children, assuming at least one of them will be a
     * worker.
     */
    for (size_t i = 0; i < fio_data.workers; ++i) {
      int jnk = 0;
      waitpid(-1, &jnk, 0);
    }
#endif
    FIO_LOG_INFO("(%d) shutdown complete.", (int)fio_data.master);
  }
}

/** Worker cycle */
static void fio___worker(void) {
  fio_data.is_worker = 1;
  fio_state_callback_force(FIO_CALL_ON_START);
  fio_thread_t *threads = NULL;
  if (fio_data.threads) {
    threads = calloc(sizeof(threads), fio_data.threads);
    FIO_ASSERT_ALLOC(threads);
    for (size_t i = 0; i < fio_data.threads; ++i) {
      FIO_ASSERT(!fio_thread_create(threads + i, fio___user_thread_cycle, NULL),
                 "thread creation failed in worker.");
    }
    fio___worker_cycle();
  } else {
    fio___worker_nothread_cycle();
  }
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  if (threads) {
    fio_user_thread_wake_all();
    for (size_t i = 0; i < fio_data.threads; ++i) {
      fio_queue_perform_all(&tasks_io_core);
      fio___cycle_housekeeping();
      fio_thread_join(threads[i]);
    }
    free(threads);
  }
  fio___worker_cleanup();
}
/** Worker sentinel */
static void *fio_worker_sentinal(void *GIL) {
  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  pid_t pid = fork();
  FIO_ASSERT(pid != (pid_t)-1, "system call `fork` failed.");
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_unlock(GIL);
  if (pid) {
    int status = 0;
    (void)status;
    if (waitpid(pid, &status, 0) != pid && fio_data.running)
      FIO_LOG_ERROR("waitpid failed, worker re-spawning might fail.");
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
      FIO_LOG_WARNING("abnormal worker exit detected");
      fio_state_callback_force(FIO_CALL_ON_CHILD_CRUSH);
    }
    if (fio_data.running) {
      FIO_ASSERT_DEBUG(
          0,
          "DEBUG mode prevents worker re-spawning, now crashing parent.");
      fio_queue_push(&tasks_io_core, .fn = fio_spawn_worker);
    }
    return NULL;
  }
  fio_data.is_master = 0;
  FIO_LOG_INFO("(%d) worker starting up.", (int)getpid());
  fio_state_callback_force(FIO_CALL_IN_CHILD);
  fio___worker();
  exit(0);
  return NULL;
}

static void fio_spawn_worker(void *ignr_1, void *ignr_2) {
  static fio_lock_i GIL;
  fio_thread_t t;
  if (!fio_data.is_master)
    return;
  fio_lock(&GIL);
  if (fio_thread_create(&t, fio_worker_sentinal, (void *)&GIL)) {
    fio_unlock(&GIL);
    FIO_LOG_ERROR(
        "sentinel thread creation failed, no worker will be spawned.");
  }
  fio_thread_detach(t);
  fio_lock(&GIL);
  fio_unlock(&GIL);
  (void)ignr_1;
  (void)ignr_2;
}

/**
 * Starts the facil.io event loop. This function will return after facil.io is
 * done (after shutdown).
 *
 * See the `struct fio_start_args` details for any possible named arguments.
 *
 * This method blocks the current thread until the server is stopped (when a
 * SIGINT/SIGTERM is received).
 */
void fio_start___(void); /* sublime text marker */
void fio_start FIO_NOOP(struct fio_start_args args) {
  fio_expected_concurrency(&args.threads, &args.workers);
  fio_signal_monitor(SIGINT, fio___stop_signal_handler, NULL);
  fio_signal_monitor(SIGTERM, fio___stop_signal_handler, NULL);
  fio_state_callback_force(FIO_CALL_PRE_START);
  fio_data.running = 1;
  fio_data.workers = args.workers;
  fio_data.threads = args.threads;

  FIO_LOG_INFO("\n\t Starting facil.io in %s mode."
               "\n\t Engine:     %s"
               "\n\t Worker(s):  %d"
               "\n\t Threads(s): %d"
               "\n\t Root PID:   %zu",
               fio_data.workers ? "cluster" : "single processes",
               fio_engine(),
               fio_data.workers,
               fio_data.threads,
               (size_t)fio_data.master);
#if HAVE_OPENSSL
  FIO_LOG_INFO("linked to OpenSSL %s", OpenSSL_version(0));
#endif

  if (fio_data.workers) {
    fio_data.is_worker = 0;
#if FIO_OS_POSIX
    fio_signal_monitor(SIGUSR1, fio___worker_reset_signal_handler, NULL);
#else
    (void)fio___worker_reset_signal_handler;
#endif
    for (size_t i = 0; i < fio_data.workers; ++i) {
      fio_spawn_worker(NULL, NULL);
    }
    fio___worker_nothread_cycle();
    fio___worker_cleanup();
  } else {
    fio___worker();
  }
  fio_state_callback_force(FIO_CALL_ON_FINISH);
}

/**
 * Attempts to stop the facil.io application. This only works within the Root
 * process. A worker process will simply respawn itself.
 */
void fio_stop(void) {
  fio_data.running = 0;
  if (fio_data.is_master && fio_data.workers)
    kill(0, SIGINT);
}

/**
 * Returns the last time the server reviewed any pending IO events.
 */
struct timespec fio_last_tick(void) {
  return fio_data.tick;
}

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void);

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
  if (0 && !*threads && !*processes) {
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
}
/**
 * Returns the number of worker processes if facil.io is running.
 *
 * (1 is returned when in single process mode, otherwise the number of workers)
 */
int16_t fio_is_running(void) { return (int)fio_data.running; }

/**
 * Returns 1 if the current process is a worker process or a single process.
 *
 * Otherwise returns 0.
 *
 * NOTE: When cluster mode is off, the root process is also the worker process.
 *       This means that single process instances don't automatically respawn
 *       after critical errors.
 */
int fio_is_worker(void) { return (int)fio_data.is_worker; }

/**
 * Returns 1 if the current process is the master (root) process.
 *
 * Otherwise returns 0.
 */
int fio_is_master(void) { return (int)fio_data.is_master; }

/** Returns facil.io's parent (root) process pid. */
pid_t fio_master_pid(void) { return fio_data.master; }

/** Returns the current process pid. */
pid_t fio_getpid(void);

/* reap children handler. */
static void fio___reap_children(int sig, void *ignr_) {
#if FIO_OS_POSIX
  FIO_ASSERT_DEBUG(sig == SIGCHLD, "wrong signal handler called");
  while (waitpid(-1, &sig, WNOHANG) > 0)
    ;
#else
  (void)sig;
  FIO_ASSERT(0, "Children reaping is only supported on POSIX systems.");
#endif
  (void)ignr_;
}
/**
 * Initializes zombie reaping for the process. Call before `fio_start` to enable
 * global zombie reaping.
 */
void fio_reap_children(void) {
#if FIO_OS_POSIX
  fio_signal_monitor(SIGCHLD, fio___reap_children, NULL);
#else
  (void)fio___reap_children;
  FIO_LOG_ERROR("fio_reap_children unsupported on this system.");
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
#if FIO_OS_POSIX
  fio_signal_forget(SIGCHLD);
  fio_signal_forget(SIGUSR1);
#endif
  fio_signal_forget(SIGINT);
  fio_signal_forget(SIGTERM);
}
/* *****************************************************************************
UUID related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Points to a function that keeps the connection alive. */
void (*FIO_PING_ETERNAL)(fio_uuid_s *, void *) = mock_ping_eternal;

/**
 * Attaches the socket in `fd` to the facio.io engine (reactor).
 *
 * * `fd` should point to a valid socket.
 *
 * * A `protocol` is always required. The system cannot manage a socket without
 *   a protocol.
 *
 * * `udata` is opaque user data and may be any value, including NULL.
 *
 * * `tls` is a context for Transport Layer Security and can be used to redirect
 *   read/write operations. NULL will result in unencrypted transmissions.
 *
 * Returns NULL on error.
 */
fio_uuid_s *fio_attach_fd(int fd,
                          fio_protocol_s *protocol,
                          void *udata,
                          fio_tls_s *tls) {
  if (!protocol || fd == -1)
    return NULL;
  fio_uuid_s *uuid = fio_uuid_new2();
  uuid->udata = udata;
  uuid->tls = tls;
  uuid->fd = fd;
  fio_sock_set_non_block(fd); /* never accept a blocking socket */
  fio_protocol_set(uuid, protocol);
  FIO_LOG_DEBUG("uuid %p attached with fd %d", (void *)uuid, fd);
  return uuid;
}

FIO_SFUNC void fio_protocol_set___task(void *uuid_, void *old_protocol_) {
  fio_uuid_s *uuid = uuid_;
  fio_protocol_s *old = old_protocol_;
  fio___touch(uuid, NULL);
  if (old && FIO_LIST_IS_EMPTY(&old->reserved.protocols))
    FIO_LIST_REMOVE(&old->reserved.protocols);
  fio_uuid_free2(uuid);
}
/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_uuid_s *uuid, fio_protocol_s *protocol) {
  if (!protocol || !uuid)
    return;
  fio_protocol_validate(protocol);
  fio_protocol_s *old = uuid->protocol;
  uuid->protocol = protocol;
  fio_queue_push(&tasks_io_core,
                 fio_protocol_set___task,
                 fio_uuid_dup(uuid),
                 old);
  fio_uuid_monitor_add(uuid);
}

/** Associates a new `udata` pointer with the UUID, returning the old `udata` */
void *fio_udata_set(fio_uuid_s *uuid, void *udata) {
  void *old = uuid->udata;
  uuid->udata = udata;
  return old;
}

/** Returns the `udata` pointer associated with the UUID. */
void *fio_udata_get(fio_uuid_s *uuid) { return uuid->udata; }

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_uuid_s *uuid, void *buf, size_t len) {
  ssize_t r = fio_sock_read(uuid->fd, buf, len);
  if (r > 0)
    return r;
  if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 0;
  fio_uuid_close(uuid);
  return 0;
}

static void fio_write2___task(void *uuid_, void *packet) {
  fio_uuid_s *uuid = uuid_;
  if (!(uuid->state & FIO_UUID_OPEN))
    goto error;
  fio_stream_add(&uuid->stream, packet);
  fio_ev_on_ready(uuid, uuid->udata);
  return;
error:
  fio_stream_pack_free(packet);
}
void fio_write2___(void); /* Sublime Text marker*/
/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2 FIO_NOOP(fio_uuid_s *uuid, fio_write_args_s args) {
  fio_stream_packet_s *packet = NULL;
  if (args.buf) {
    packet = fio_stream_pack_data(args.buf,
                                  args.len,
                                  args.offset,
                                  args.copy,
                                  args.dealloc);
  } else if (args.fd != -1) {
    packet = fio_stream_pack_fd(args.fd, args.len, args.offset, args.copy);
  }
  if (!packet)
    goto error;
  fio_queue_push(&tasks_io_core,
                 .fn = fio_write2___task,
                 .udata1 = fio_uuid_dup(uuid),
                 .udata2 = packet);
  fio_io_thread_wake();
  return;
error:
  FIO_LOG_ERROR("couldn't create user-packet for uuid %p", (void *)uuid);
}

/** Marks the UUID for closure as soon as the pending data was sent. */
void fio_close(fio_uuid_s *uuid) {
  if ((uuid->state & FIO_UUID_OPEN) && !(uuid->state & FIO_UUID_CLOSED_BIT)) {
    uuid->state |= FIO_UUID_CLOSED_BIT;
    fio_uuid_monitor_add_write(uuid);
  }
}

/** Marks the UUID for immediate closure. */
void fio_close_now(fio_uuid_s *uuid) { fio_uuid_close(uuid); }

/** Returns true if a UUID is busy with an ongoing task. */
int fio_uuid_is_busy(fio_uuid_s *uuid) { return (int)uuid->lock; }

/* Resets a socket's timeout counter. */
void fio_touch(fio_uuid_s *uuid) {
  static fio_uuid_s *last_uuid;
  static uint64_t last_call;
  const uint64_t this_call =
      ((uint64_t)fio_data.tick.tv_sec << 21) + fio_data.tick.tv_nsec;
  if (last_uuid == uuid && last_call == this_call)
    return;
  last_uuid = uuid;
  last_call = this_call;
  fio_queue_push(&tasks_io_core,
                 .fn = fio___touch,
                 .udata1 = fio_uuid_dup2(uuid),
                 .udata2 = uuid);
}
/* *****************************************************************************
Listening to Incoming Connections
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

FIO_SFUNC void fio_listen_on_data(fio_uuid_s *uuid, void *udata) {
  struct fio_listen_args *l = udata;
  int fd;
  while ((fd = accept(uuid->fd, NULL, NULL)) != -1) {
    FIO_LOG_DEBUG("accepting a new connection (fd %d) at %s", fd, l->url);
    l->on_open(fd, l->udata);
  }
}
FIO_SFUNC void fio_listen_on_close(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_INFO("(%d) stopped listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)getpid()),
               l->url);
}

FIO_SFUNC void fio_listen_on_ready(fio_uuid_s *uuid, void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_INFO("(%d) started listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)getpid()),
               l->url);
  (void)uuid;
}

static fio_protocol_s FIO_PROTOCOL_LISTEN = {
    .on_data = fio_listen_on_data,
    .on_ready = fio_listen_on_ready,
    .on_close = fio_listen_on_close,
    .on_timeout = mock_ping_eternal,
};

FIO_SFUNC void fio_listen___attach(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_DEBUG("Calling dup(%d) to attach as a listening socket.",
                l->reserved);
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
  fio_attach_fd(fd, &FIO_PROTOCOL_LISTEN, l, NULL);
}

FIO_SFUNC void fio_listen___free(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_DEBUG("(%d) closing listening socket at %s", getpid(), l->url);
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
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************







        Startup / State Callbacks (fork, start up, idle, etc')







***************************************************************************** */

typedef struct {
  void (*func)(void *);
  void *arg;
} fio___state_task_s;

#ifdef FIO_MEM_REALLOC_
#error FIO_MEM_REALLOC_ should have been undefined
#endif
#define FIO_MALLOC_TMP_USE_SYSTEM
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
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2) {
  fio_queue_push(&tasks_user, .fn = task, .udata1 = udata1, .udata2 = udata2);
}

FIO_SFUNC void fio_io_task_wrapper(void *task_, void *ignr_) {
  fio_queue_task_s *t = task_;
  fio_uuid_s *uuid = t->udata1;
  union {
    void (*fn)(fio_uuid_s *uuid, void *udata);
    void (*fn2)(void *uuid, void *udata);
    void *p;
  } u = {.fn2 = t->fn};
  if (fio_trylock(&uuid->lock))
    goto reschedule;
  u.fn(uuid, t->udata2);
  fio_unlock(&uuid->lock);
  fio_free(t);
  fio_uuid_free(uuid);
  return;
reschedule:
  fio_queue_push(&tasks_user,
                 .fn = fio_io_task_wrapper,
                 .udata1 = task_,
                 .udata2 = ignr_);
}

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_uuid_s *uuid,
                  void (*task)(fio_uuid_s *uuid, void *udata),
                  void *udata) {
  union {
    void (*fn)(fio_uuid_s *uuid, void *udata);
    void (*fn2)(void *uuid, void *udata);
    void *p;
  } u;
  u.fn = task;
  fio_queue_task_s *t = fio_malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  *t = (fio_queue_task_s){.fn = u.fn2,
                          .udata1 = fio_uuid_dup(uuid),
                          .udata2 = udata};
  fio_queue_push(&tasks_user,
                 .fn = fio_io_task_wrapper,
                 .udata1 = uuid,
                 .udata2 = udata);
}
/* *****************************************************************************
Testing
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */
#ifdef TEST
FIO_SFUNC void fio_test___task(void *u1, void *u2) {

  FIO_THREAD_WAIT(100000);
  fio_stop();
  (void)u1;
  (void)u2;
}
void fio_test(void) {
  FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  fprintf(stderr, "Testing facil.io IO-Core framework modules.\n");
  FIO_NAME_TEST(io, state)();
  fio_defer(fio_test___task, NULL, NULL);
  fio_start(.threads = -2, .workers = 0);
}
#endif
