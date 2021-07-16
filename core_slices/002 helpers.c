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
  } u;
  u.fn(udata);
  u.p = p;
}

/* cleanup event scheduling */
FIO_IFUNC void fio_uuid_env_obj_call_callback(fio_uuid_env_obj_s o) {
  union {
    void (*fn)(void *);
    void *p;
  } u;
  if (o.on_close) {
    u.fn = o.on_close;
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
Global State
***************************************************************************** */

static struct {
  FIO_LIST_HEAD protocols;
  fio_thread_mutex_t env_lock;
  fio___uuid_env_s env;
  struct timespec tick;
#if FIO_OS_WIN
  HANDLE master;
#else
  pid_t master;
#endif
  struct {
    int in;
    int out;
  } thread_suspenders;
  uint16_t threads;
  uint16_t workers;
  uint8_t is_master;
  uint8_t is_worker;
  volatile uint8_t running;
} fio_data = {
    .env_lock = FIO_THREAD_MUTEX_INIT,
    .is_master = 1,
    .is_worker = 1,
};

void fio_cleanup_at_exit(void) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  for (int i = 0; i < FIO_CALL_NEVER; ++i)
    fio_state_callback_clear((callback_type_e)i);
  fio_thread_mutex_destroy(&fio_data.env_lock);
  fio___uuid_env_destroy(&fio_data.env);
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_uuid_monitor_close();
  close(fio_data.thread_suspenders.in);
  close(fio_data.thread_suspenders.out);
}

static void fio_cleanup_after_fork(void *ignr_);

FIO_CONSTRUCTOR(fio_data_state_init) {
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.master = getpid();
  atexit(fio_cleanup_at_exit);
  fio_uuid_monitor_init();
  pipe(&fio_data.thread_suspenders.in);

  fio_state_callback_add(FIO_CALL_IN_CHILD, fio_cleanup_after_fork, NULL);
}

/* *****************************************************************************
Thread suspension helpers
***************************************************************************** */

FIO_IFUNC void fio_user_thread_wake(void) {
  char buf[1] = {0};
  fio_sock_write(fio_data.thread_suspenders.out, buf, 1);
}

FIO_IFUNC void fio_user_thread_suspent(void) {
  char buf[1];
  fio_sock_read(fio_data.thread_suspenders.in, buf, 1);
}

FIO_IFUNC void fio_user_thread_wake_all(void) {
  close(fio_data.thread_suspenders.in);
  close(fio_data.thread_suspenders.out);
  FIO_ASSERT(!pipe(&fio_data.thread_suspenders.in),
             "%d - couldn't initiate pipes in worker.",
             (int)getpid());
}

/* *****************************************************************************
Signal Helpers
***************************************************************************** */
#define FIO_SIGNAL
#include "fio-stl.h"
static volatile uint8_t fio_signal_forwarded = 0;
/* Handles signals */
static void fio___stop_signal_handler(int sig, void *ignr_) {
  fio_data.running = 0;
  FIO_LOG_INFO("(%d) received shutdown signal.", getpid());
  if (fio_data.is_master && fio_data.workers && !fio_signal_forwarded) {
    kill(0, sig);
    fio_signal_forwarded = 1;
  }
  (void)ignr_;
}

/* Handles signals */
static void fio___worker_reset_signal_handler(int sig, void *ignr_) {
  if (!fio_data.workers || !fio_data.running)
    return;
  if (fio_data.is_worker) {
    fio_data.running = 0;
    FIO_LOG_INFO("(%d) received worker restart signal.", getpid());
  } else if (!fio_signal_forwarded) {
    kill(0, sig);
    FIO_LOG_INFO("(%d) forwarding worker restart signal.",
                 (int)fio_data.master);
  }
  fio_signal_forwarded = 1;
  (void)ignr_;
}

/* *****************************************************************************
UUID data types
***************************************************************************** */

#define FIO_UMAP_NAME          fio_uuid_validity_map
#define FIO_MAP_TYPE           fio_uuid_s *
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))
#include <fio-stl.h>

static fio_uuid_validity_map_s fio_uuid_validity_map = FIO_MAP_INIT;

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

typedef enum {
  FIO_UUID_OPEN = 1,      /* 0b0001 */
  FIO_UUID_SUSPENDED = 3, /* 0b0011 */
  FIO_UUID_CLOSED = 6,    /* 0b0110 */
  FIO_UUID_CLOSING = 7,   /* 0b0111 */
} fio_uuid_state_e;

struct fio_uuid_s {
  /* fd protocol */
  fio_protocol_s *protocol;
  /* timeout review linked list */
  FIO_LIST_NODE timeouts;
  /** RW hooks. */
  // fio_rw_hook_s *rw_hooks;
  /* user udata */
  void *udata;
  /** RW udata. */
  void *rw_udata;
  /* current data to be send */
  fio_stream_s stream;
  /* Objects linked to the UUID */
  fio___uuid_env_s env;
  /* socket */
  int fd;
  /* timer handler */
  time_t active;
  /** Connection is open */
  fio_uuid_state_e state;
  /** Task lock */
  fio_lock_i lock;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
};

FIO_IFUNC void fio___touch(fio_uuid_s *uuid) {
  uuid->active = fio_time2milli(fio_data.tick);
}

FIO_SFUNC void fio___deferred_on_close(void *fn, void *udata) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn};
  u.fn(udata);
}

FIO_SFUNC void fio_uuid___init(fio_uuid_s *uuid) {
  *uuid = (fio_uuid_s){
      .timeouts = FIO_LIST_INIT(uuid->timeouts),
      .active = fio_time2milli(fio_data.tick),
  };
  fio_uuid_validity_map_set(&fio_uuid_validity_map,
                            fio_risky_ptr(uuid),
                            uuid,
                            NULL);
  FIO_LOG_DEBUG("UUID %p (fd %d) initialized.", (void *)uuid, (int)(uuid->fd));
}

FIO_SFUNC void fio_uuid___destroy(fio_uuid_s *uuid) {
  fio_uuid_validity_map_remove(&fio_uuid_validity_map,
                               fio_risky_ptr(uuid),
                               uuid,
                               NULL);
  fio___uuid_env_destroy(&uuid->env);
  fio_stream_destroy(&uuid->stream);
  // o->rw_hooks->cleanup(uuid->rw_udata);
  FIO_LIST_REMOVE(&uuid->timeouts);
  union {
    void *p;
    void (*fn)(void *);
  } u = {.fn = uuid->protocol->on_close};
  fio_queue_push(&tasks_user, fio___deferred_on_close, u.p, uuid->udata);
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
void fio_uuid_free(fio_uuid_s *uuid) {
  fio_queue_push(&tasks_io_core, .fn = fio_uuid_free_task, .udata1 = uuid);
}

#define fio_uuid_dup fio_uuid_dup2

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void fio_ev_on_shutdown(void *uuid, void *udata);
static void fio_ev_on_data(void *uuid, void *udata);
static void fio_ev_on_ready(void *uuid, void *udata);
static void fio_ev_on_timeout(void *uuid, void *udata);

/* *****************************************************************************
Event deferring (mock functions)
***************************************************************************** */

static void mock_on_ev(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
  (void)udata;
}
static void mock_on_data(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
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
static void mock_ping(fio_uuid_s *uuid, void *udata) {
  (void)uuid;
  (void)udata;
}
static void mock_ping_eternal(fio_uuid_s *uuid, void *udata) {
  fio___touch(uuid);
  (void)udata;
}

/* *****************************************************************************
Housekeeping cycle
***************************************************************************** */
FIO_SFUNC void fio___housekeeping(void) {
  static int old = 0;
  int c = fio_uuid_monitor_review();
  fio_data.tick = fio_time_real();
  c += fio_signal_review();
  if (!c) {
    if (!old) {
      fio_state_callback_force(FIO_CALL_ON_IDLE);
      fio_signal_forwarded = 0;
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
  fio_user_thread_wake(); /* superfluous, but safe  */
  old = c;

  /* TODO: test timeouts */
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio_cleanup_after_fork(void *ignr_) {
  (void)ignr_;

  fio_uuid_monitor_close();
  fio_uuid_monitor_init();
  close(fio_data.thread_suspenders.in);
  close(fio_data.thread_suspenders.out);
  FIO_ASSERT(!pipe(&fio_data.thread_suspenders.in),
             "%d - couldn't initiate pipes in worker.",
             (int)getpid());
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_uuid_s, timeouts, &pr->reserved.uuids, uuid) {
      fio_sock_close(uuid->fd);
      uuid->fd = -1;
      uuid->state = FIO_UUID_CLOSED;
      fio_uuid_free2(uuid);
    }
    FIO_LIST_REMOVE(&pr->reserved.protocols);
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
