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

#define FIO_STREAM
#include "fio-stl.h"

#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_QUEUE
#define FIO_SIGNAL
#define FIO_SOCK /* should be public? */
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

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 1000
#endif

#ifndef FIO_POLL_SHUTDOWN_TICK
#define FIO_POLL_SHUTDOWN_TICK 10
#endif

FIO_SFUNC void fio_monitor_init(void);
FIO_SFUNC void fio_monitor_destroy(void);
FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags);
FIO_SFUNC void fio_monitor_forget(int fd);
FIO_SFUNC int fio_monitor_review(int timeout);

FIO_IFUNC void fio_monitor_read(fio_s *io);
FIO_IFUNC void fio_monitor_write(fio_s *io);
FIO_IFUNC void fio_monitor_all(fio_s *io);

/* *****************************************************************************
Queue Helpers
***************************************************************************** */

FIO_IFUNC fio_queue_s *fio_queue_select(uintptr_t flag);

#define FIO_QUEUE_SYSTEM fio_queue_select(1)
#define FIO_QUEUE_USER   fio_queue_select(0)

/* *****************************************************************************
ENV data maps (must be defined before the IO object that owns them)
***************************************************************************** */

/** An object that can be linked to any facil.io connection (fio_s). */
typedef struct {
  void (*on_close)(void *data);
  void *udata;
} env_obj_s;

/* cleanup event task */
static void env_obj_call_callback_task(void *p, void *udata) {
  union {
    void (*fn)(void *);
    void *p;
  } u = {.p = p};
  u.fn(udata);
}

/* cleanup event scheduling */
FIO_IFUNC void env_obj_call_callback(env_obj_s o) {
  union {
    void (*fn)(void *);
    void *p;
  } u = {.fn = o.on_close};
  if (o.on_close) {
    fio_queue_push_urgent(FIO_QUEUE_USER,
                          env_obj_call_callback_task,
                          u.p,
                          o.udata);
  }
}

/* for storing ENV string keys */
#define FIO_STR_SMALL sstr
#include "fio-stl.h"

#define FIO_UMAP_NAME              env
#define FIO_MAP_TYPE               env_obj_s
#define FIO_MAP_TYPE_DESTROY(o)    env_obj_call_callback((o))
#define FIO_MAP_DESTROY_AFTER_COPY 0

/* destroy discarded keys when overwriting existing data (const_name support) */
#define FIO_MAP_KEY                 sstr_s /* the small string type */
#define FIO_MAP_KEY_COPY(dest, src) (dest) = (src)
#define FIO_MAP_KEY_DESTROY(k)      sstr_destroy(&k)
#define FIO_MAP_KEY_DISCARD         FIO_MAP_KEY_DESTROY
#define FIO_MAP_KEY_CMP(a, b)       sstr_is_eq(&(a), &(b))
#include <fio-stl.h>

/* *****************************************************************************
CPU Core Counting
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
Event deferring (declarations)
***************************************************************************** */

static void fio_ev_on_ready(void *io, void *udata);
static void fio_ev_on_ready_user(void *io, void *udata);
static void fio_ev_on_data(void *io, void *udata);
static void fio_ev_on_timeout(void *io, void *udata);
static void fio_ev_on_shutdown(void *io, void *udata);
static void fio_ev_on_close(void *io, void *udata);

static void mock_ping_eternal(fio_s *io);

/** Points to a function that keeps the connection alive. */
void (*FIO_PING_ETERNAL)(fio_s *) = mock_ping_eternal;

/* *****************************************************************************
Event deferring (mock functions)
***************************************************************************** */

static void mock_on_data(fio_s *io) { fio_suspend(io); }
static void mock_on_ready(fio_s *io) { (void)io; }
static void mock_on_shutdown(fio_s *io) { (void)io; }
static void mock_on_close(void *udata) { (void)udata; }
static void mock_timeout(fio_s *io) {
  fio_close_now(io);
  (void)io;
}
static void mock_ping_eternal(fio_s *io) { fio_touch(io); }

FIO_IFUNC void fio_protocol_validate(fio_protocol_s *p) {
  if (p && !(p->reserved.flags & 8)) {
    if (!p->on_data)
      p->on_data = mock_on_data;
    if (!p->on_timeout)
      p->on_timeout = mock_timeout;
    if (!p->on_ready)
      p->on_ready = mock_on_ready;
    if (!p->on_shutdown)
      p->on_shutdown = mock_on_shutdown;
    if (!p->on_close)
      p->on_close = mock_on_close;
    p->reserved.flags |= 8;
  }
}

/* *****************************************************************************
Child Reaping
***************************************************************************** */

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
/* *****************************************************************************
Global State
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO Registry - NOT thread safe (access from IO core thread only)
***************************************************************************** */

#define FIO_UMAP_NAME             fio_validity_map
#define FIO_MEMORY_NAME           fio_validity_map_mem
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#define FIO_MAP_TYPE              fio_s *
#define FIO_MAP_HASH_FN(o)        fio_risky_ptr(o)
#define FIO_MAP_TYPE_CMP(a, b)    ((a) == (b))
#include <fio-stl.h>

/* *****************************************************************************
Global State
***************************************************************************** */

static struct {
  FIO_LIST_HEAD protocols;
  fio_thread_mutex_t env_lock;
  env_s env;
  int64_t tick;
  fio_validity_map_s valid;
  fio_s *io_wake_io;
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
  fio_timer_queue_s timers;
  fio_queue_s tasks[2];
} fio_data = {
    .env_lock = FIO_THREAD_MUTEX_INIT,
    .env = FIO_MAP_INIT,
    .valid = FIO_MAP_INIT,
    .thread_suspenders = {-1, -1},
    .io_wake = {-1, -1},
    .is_master = 1,
    .is_worker = 1,
};

/* *****************************************************************************
Queue Helpers
***************************************************************************** */

/** Returns the task queue according to the flag specified. */
FIO_IFUNC fio_queue_s *fio_queue_select(uintptr_t flag) {
  return fio_data.tasks + (flag & 1);
}

/* *****************************************************************************
Wakeup Pipe Helpers
***************************************************************************** */

FIO_IFUNC void fio_reset_pipes___(int *i, const char *msg) {
  if (i[0] != -1)
    close(i[0]);
  if (i[1] != -1)
    close(i[1]);
  FIO_ASSERT(!pipe(i),
             "%d - couldn't initiate %s thread wakeup pipes.",
             (int)getpid(),
             msg);
  fio_sock_set_non_block(i[0]);
  fio_sock_set_non_block(i[1]);
}

FIO_IFUNC void fio_reset_wakeup_pipes(void) {
  fio_reset_pipes___(&fio_data.thread_suspenders.in, "user");
  fio_reset_pipes___(&fio_data.io_wake.in, "IO");
}

FIO_IFUNC void fio_close_wakeup_pipes(void) {
  if (fio_data.thread_suspenders.in == -1)
    return;
  close(fio_data.thread_suspenders.in);
  close(fio_data.thread_suspenders.out);
  close(fio_data.io_wake.in);
  close(fio_data.io_wake.out);
  fio_data.thread_suspenders.in = -1;
  fio_data.thread_suspenders.out = -1;
  fio_data.io_wake.in = -1;
  fio_data.io_wake.out = -1;
}

/* *****************************************************************************
IO Registry Helpers
***************************************************************************** */

FIO_IFUNC void fio_set_valid(fio_s *io) {
  fio_validity_map_set(&fio_data.valid, fio_risky_ptr(io), io, NULL);
  FIO_LOG_DEBUG("IO %p is now valid", (void *)io);
}

FIO_IFUNC void fio_set_invalid(fio_s *io) {
  fio_validity_map_remove(&fio_data.valid, fio_risky_ptr(io), io, NULL);
  FIO_LOG_DEBUG("IO %p is no longer valid", (void *)io);
}

FIO_IFUNC fio_s *fio_is_valid(fio_s *io) {
  fio_s *r = fio_validity_map_get(&fio_data.valid, fio_risky_ptr(io), io);
  return r;
}

FIO_IFUNC void fio_invalidate_all() {
  fio_validity_map_destroy(&fio_data.valid);
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio___after_fork___core(void) {
  if (!fio_data.is_master) {
    fio_monitor_destroy();
    fio_monitor_init();
  }
  fio_reset_wakeup_pipes();
}

/* *****************************************************************************
Thread suspension helpers
***************************************************************************** */

FIO_SFUNC void fio_user_thread_suspent(void) {
  short e = fio_sock_wait_io(fio_data.thread_suspenders.in, POLLIN, 2000);
  if (e != -1 && (e & POLLIN)) {
    char buf[2];
    int r = fio_sock_read(fio_data.thread_suspenders.in, buf, 1);
    (void)r;
  }
}

FIO_IFUNC void fio_user_thread_wake(void) {
  char buf[2] = {0};
  int i = fio_sock_write(fio_data.thread_suspenders.out, buf, 2);
  (void)i;
}

FIO_IFUNC void fio_user_thread_wake_all(void) {
  fio_reset_pipes___(&fio_data.thread_suspenders.in, "user");
}

FIO_IFUNC void fio_io_thread_wake(void) {
  char buf[2] = {0};
  int i = fio_sock_write(fio_data.thread_suspenders.out, buf, 2);
  (void)i;
}

FIO_IFUNC void fio_io_thread_wake_clear(void) {
  char buf[1024];
  ssize_t l;
  while ((l = fio_sock_read(fio_data.io_wake.in, buf, 1024)) > 0)
    ;
}

/* *****************************************************************************
IO Wakeup Pipe Protocol
***************************************************************************** */

FIO_SFUNC void fio_io_wakeup_prep(void);

static void fio_io_wakeup_on_data(fio_s *io) {
  fio_io_thread_wake_clear();
  (void)io;
}

static void fio_io_wakeup_on_close(void *udata) {
  fio_data.io_wake_io = NULL;
  fio_data.io_wake.in = -1;
  if (fio_data.running) {
    fio_reset_wakeup_pipes();
    fio_io_wakeup_prep();
  }
  FIO_LOG_DEBUG("IO wakeup fio_s freed");
  (void)udata;
}

static fio_protocol_s FIO_IO_WAKEUP_PROTOCOL = {
    .on_data = fio_io_wakeup_on_data,
    .on_timeout = mock_ping_eternal,
    .on_close = fio_io_wakeup_on_close,
};

FIO_SFUNC void fio_io_wakeup_prep(void) {
  if (fio_data.io_wake_io || !fio_data.running)
    return;
  fio_data.io_wake_io =
      fio_attach_fd(fio_data.io_wake.in, &FIO_IO_WAKEUP_PROTOCOL, NULL, NULL);
  FIO_IO_WAKEUP_PROTOCOL.reserved.flags |= 1; /* system protocol */
}

/* *****************************************************************************
Signal Helpers
***************************************************************************** */
#define FIO_SIGNAL
#include "fio-stl.h"

/* Handles signals */
static void fio_signal_handler___stop(int sig, void *ignr_) {
  static int64_t last_signal = 0;
  if (last_signal + 2000 >= fio_data.tick)
    return;
  last_signal = fio_data.tick;
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
static void fio_signal_handler___worker_reset(int sig, void *ignr_) {
  if (!fio_data.workers || !fio_data.running)
    return;
  static int64_t last_signal = 0;
  if (last_signal + 2000 >= fio_data.tick)
    return;
  last_signal = fio_data.tick;
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

/** Initializes the signal handlers (except child reaping). */
FIO_IFUNC void fio_signal_handler_init(void) {
  fio_signal_monitor(SIGINT, fio_signal_handler___stop, NULL);
  fio_signal_monitor(SIGTERM, fio_signal_handler___stop, NULL);
  fio_signal_monitor(SIGPIPE, NULL, NULL);
#if FIO_OS_POSIX
  fio_signal_monitor(SIGUSR1, fio_signal_handler___worker_reset, NULL);
#else
  (void)fio___worker_reset_signal_handler;
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
  fio_signal_forget(SIGPIPE);
#if FIO_OS_POSIX
  fio_signal_forget(SIGUSR1);
  fio_signal_forget(SIGCHLD);
#endif
}

/* *****************************************************************************
Public API accessing the Core data structure
***************************************************************************** */

/**
 * Returns the last time the server reviewed any pending IO events.
 */
struct timespec fio_last_tick(void) {
  struct timespec r = {
      .tv_sec = fio_data.tick / 1000,
      .tv_nsec = ((fio_data.tick % 1000) * 1000000),
  };
  return r;
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
/* *****************************************************************************
IO data types
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO data types
***************************************************************************** */

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

fio_protocol_s FIO_PROTOCOL_HIJACK;

typedef enum {
  FIO_IO_OPEN = 1,          /* 0b0001 */
  FIO_IO_SUSPENDED_BIT = 2, /* 0b0010 */
  FIO_IO_SUSPENDED = 3,     /* 0b0011 */
  FIO_IO_CLOSED_BIT = 4,    /* 0b0100 */
  FIO_IO_CLOSED = 6,        /* 0b0110 */
  FIO_IO_CLOSING = 7,       /* 0b0111 */
} fio_state_e;

struct fio_s {
  /* user udata - MUST be first */
  void *udata;
  /* fd protocol */
  fio_protocol_s *protocol;
  /** RW hooks. */
  fio_tls_s *tls;
  /* timeout review linked list */
  FIO_LIST_NODE timeouts;
  /* current data to be send */
  fio_stream_s stream;
  /* env lock */
  fio_thread_mutex_t env_lock;
  /* env: objects linked to the io */
  env_s env;
  /* timer handler */
  int64_t active;
  /* socket */
  int fd;
#ifdef DEBUG
  /* debug into: close_now counter */
  unsigned int count;
#endif
  /** Task lock */
  fio_lock_i lock;
  /** Connection state */
  volatile uint8_t state;
  /** peer address length */
  uint8_t addr_len;
  /** peer address length */
  uint8_t addr[FIO_MAX_ADDR_LEN];
};

FIO_IFUNC fio_s *fio_dup2(fio_s *io);
FIO_IFUNC void fio_free2(fio_s *io);

FIO_SFUNC void fio___deferred_on_close(void *fn, void *udata) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn};
  u.fn(udata);
}

FIO_SFUNC void fio___init_task(void *io, void *ignr_) {
  fio_set_valid(io);
  (void)ignr_;
}
/* IO object constructor (MUST be thread safe). */
FIO_SFUNC void fio___init(fio_s *io) {
  *io = (fio_s){
      .protocol = &FIO_PROTOCOL_HIJACK,
      .state = FIO_IO_OPEN,
      .stream = FIO_STREAM_INIT(io->stream),
      .timeouts = FIO_LIST_INIT(io->timeouts),
      .env_lock = FIO_THREAD_MUTEX_INIT,
      .fd = -1,
  };
  fio_queue_push_urgent(FIO_QUEUE_SYSTEM, .fn = fio___init_task, .udata1 = io);
  FIO_LOG_DEBUG("IO %p allocated.", (void *)io);
}

/* IO object destructor (NOT thread safe). */
FIO_SFUNC void fio___destroy(fio_s *io) {
  fio_set_invalid(io);
  env_destroy(&io->env);
  fio_thread_mutex_destroy(&io->env_lock);
  fio_stream_destroy(&io->stream);
  // o->rw_hooks->cleanup(io->rw_udata);
  FIO_LIST_REMOVE(&io->timeouts);
  union {
    void *p;
    void (*fn)(void *);
  } u = {.fn = io->protocol->on_close};
  fio_monitor_forget(io->fd);
  fio_sock_close(io->fd);
  if (io->protocol->reserved.ios.next == io->protocol->reserved.ios.prev) {
    FIO_LIST_REMOVE(&io->protocol->reserved.protocols);
  }
  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 fio___deferred_on_close,
                 u.p,
                 io->udata);
  FIO_LOG_DEBUG("IO %p (fd %d) freed.", (void *)io, (int)(io->fd));
}

#define FIO_REF_NAME         fio
#define FIO_REF_INIT(obj)    fio___init(&(obj))
#define FIO_REF_DESTROY(obj) fio___destroy(&(obj))
#include <fio-stl.h>

#if 0
#define fio_undup(io)                                                          \
  do {                                                                         \
    FIO_LOG_DEBUG("calling fio_undup %p from %s", io, __func__);               \
    fio_undup(io);                                                             \
  } while (0)
#define fio_free2(io)                                                          \
  do {                                                                         \
    FIO_LOG_DEBUG("calling fio_free2 %p from %s", io, __func__);               \
    fio_free2(io);                                                             \
  } while (0)

#endif

/* *****************************************************************************
Managing reference counting (decrease only in the IO thread)
***************************************************************************** */

/* perform reference count decrease. */
FIO_SFUNC void fio_undup_task(void *io, void *ignr) {
  (void)ignr;
  /* don't trust users to manage reference counts? */
  if (fio_is_valid(io))
    fio_free2(io);
  else
    FIO_LOG_WARNING("user code attempted to double-free IO %p", io);
}

/** Route reference count decrease to the IO thread. */
void fio_undup FIO_NOOP(fio_s *io) {
  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = fio_undup_task, .udata1 = io);
}

/** Increase reference count. */
fio_s *fio_dup(fio_s *io) { return fio_dup2(io); }
#define fio_dup fio_dup2

/* *****************************************************************************
Closing the IO connection
***************************************************************************** */

/** common safe / unsafe implementation (unsafe = called from user thread). */
FIO_IFUNC void fio_close_now___common(fio_s *io, void (*free_fn)(fio_s *)) {
  if (!(fio_atomic_and(&io->state, (~FIO_IO_OPEN)) & FIO_IO_OPEN)) {
    FIO_LOG_DEBUG("fio_close_now called for closed IO %p", io);
    return;
  }
  io->state |= FIO_IO_CLOSED;
#ifdef DEBUG
  if (fio_atomic_add(&io->count, 1))
    FIO_LOG_ERROR("close_now called %u times", io->count);
#endif
  fio_monitor_forget(io->fd);
  free_fn(io);
}

/** Public / safe IO immediate closure. */
void fio_close_now(fio_s *io) { fio_close_now___common(io, fio_undup); }

/** Unsafe IO immediate closure (callable only from IO threads). */
FIO_IFUNC void fio_close_now_unsafe(fio_s *io) {
  fio_close_now___common(io, fio_free2);
}

/* *****************************************************************************
Timeout Marking
***************************************************************************** */

/** "touches" a socket, should only be called from the IO thread. */
FIO_SFUNC void fio_touch___unsafe(void *io_, void *should_free) {
  fio_s *io = io_;
  int64_t now;
  /* brunching vs risking a cache miss... */
  if (!(io->state & FIO_IO_OPEN)) {
    goto finish;
  }
  now = fio_data.tick;
  if (io->active == now) /* skip a double-touch for the same IO cycle. */
    goto finish;
  io->active = now;
  /* (re)insert the IO to the end of the timeout linked list in the protocol. */
  FIO_LIST_REMOVE(&io->timeouts);
  FIO_LIST_PUSH(&io->protocol->reserved.ios, &io->timeouts);
  if (FIO_LIST_IS_EMPTY(&io->protocol->reserved.protocols)) {
    /* insert the protocol object to the monitored protocol list. */
    FIO_LIST_REMOVE(&io->protocol->reserved.protocols);
    FIO_LIST_PUSH(&fio_data.protocols, &io->protocol->reserved.protocols);
  }
finish:
  fio_free2(should_free);
}

/* Resets a socket's timeout counter. */
void fio_touch(fio_s *io) {
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_touch___unsafe,
                 .udata1 = io,
                 .udata2 = fio_dup(io));
}

/* *****************************************************************************
Timeout Review
***************************************************************************** */

/** Schedules the timeout event for any timed out IO object */
FIO_SFUNC int fio___review_timeouts(void) {
  int c = 0;
  static time_t last_to_review = 0;
  /* test timeouts at whole second intervals */
  if (last_to_review + 1000 > fio_data.tick)
    return c;
  last_to_review = fio_data.tick;
  const int64_t now_milli = fio_data.tick;

  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_ASSERT_DEBUG(pr->reserved.flags, "protocol object flags unmarked?!");
    if (!pr->timeout || pr->timeout > FIO_IO_TIMEOUT_MAX)
      pr->timeout = FIO_IO_TIMEOUT_MAX;
    int64_t limit = now_milli - ((int64_t)pr->timeout * 1000);
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "io protocol ownership error");
      if (io->active >= limit)
        break;
      fio_queue_push_urgent(fio_queue_select(pr->reserved.flags),
                            .fn = fio_ev_on_timeout,
                            .udata1 = fio_dup(io),
                            .udata2 = NULL);
      ++c;
      FIO_LOG_DEBUG("scheduling timeout for %p (fd %d)", (void *)io, io->fd);
    }
  }
  return c;
}

/* *****************************************************************************
Testing for any open IO objects
***************************************************************************** */

FIO_SFUNC int fio___is_waiting_on_io(void) {
  int c = 0;
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LOG_DEBUG("active IO objects for protocol at %p", (void *)pr);
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      c = 1;
      FIO_LOG_DEBUG("waiting for IO %p (fd %d)", (void *)io, io->fd);
      fio_close(io);
    }
  }
  return c;
}

/* *****************************************************************************
IO object protocol update
***************************************************************************** */

/* completes an update for an IO object's protocol */
FIO_SFUNC void fio_protocol_set___task(void *io_, void *old_protocol_) {
  fio_s *io = io_;
  fio_protocol_s *p = io->protocol;
  fio_protocol_s *old = old_protocol_;
  FIO_ASSERT_DEBUG(p != old, "protocol switch with identical protocols!");
  if (!(p->reserved.flags & 4)) {
    p->reserved.protocols = FIO_LIST_INIT(p->reserved.protocols);
    p->reserved.ios = FIO_LIST_INIT(p->reserved.ios);
    p->reserved.flags |= 4;
    FIO_LOG_DEBUG("attaching protocol %p (first IO %p)", (void *)p, io_);
  }
  fio_touch___unsafe(io, NULL);
  if (old && FIO_LIST_IS_EMPTY(&old->reserved.ios) &&
      (old->reserved.flags & 4)) {
    FIO_LIST_REMOVE(&old->reserved.protocols);
    old->reserved.flags &= ~(uintptr_t)4ULL;
    FIO_LOG_DEBUG("detaching protocol %p (last IO was %p)", (void *)old, io_);
  }
  fio_free2(io);
}
/**
 * Sets a new protocol object.
 *
 * If `protocol` is NULL, the function silently fails and the old protocol will
 * remain active.
 */
void fio_protocol_set(fio_s *io, fio_protocol_s *protocol) {
  if (!io || io->protocol == protocol)
    return;
  if (!protocol)
    protocol = &FIO_PROTOCOL_HIJACK;
  fio_protocol_validate(protocol);
  fio_protocol_s *old = io->protocol;
  io->protocol = protocol;
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_protocol_set___task,
                 .udata1 = fio_dup(io),
                 .udata2 = old);
  fio_monitor_all(io);
}

/* *****************************************************************************
Copy address to string
***************************************************************************** */

/** Caches an attached IO object's peer address. */
FIO_SFUNC void fio___addr_cpy(fio_s *io) {
  /* TODO: Fix Me */
  struct sockaddr addrinfo;
  size_t len = sizeof(addrinfo);
  const char *result =
      inet_ntop(addrinfo.sa_family,
                addrinfo.sa_family == AF_INET
                    ? (void *)&(((struct sockaddr_in *)&addrinfo)->sin_addr)
                    : (void *)&(((struct sockaddr_in6 *)&addrinfo)->sin6_addr),
                (char *)io->addr,
                sizeof(io->addr));
  if (result) {
    io->addr_len = strlen((char *)io->addr);
  } else {
    io->addr_len = 0;
    io->addr[0] = 0;
  }
}

/* *****************************************************************************
IO object attachment
***************************************************************************** */

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
fio_s *fio_attach_fd(int fd,
                     fio_protocol_s *protocol,
                     void *udata,
                     fio_tls_s *tls) {
  if (!protocol || fd == -1)
    goto error;
  fio_s *io = fio_new2();
  FIO_ASSERT_ALLOC(io);
  io->udata = udata;
  io->tls = tls;
  io->fd = fd;
  fio_sock_set_non_block(fd); /* never accept a blocking socket */
  /* fio___addr_cpy(io); // ??? */
  fio_protocol_set(io, protocol);
  FIO_LOG_DEBUG("attaching fd %d to IO %p", fd, (void *)io);
  return io;
error:
  if (protocol && protocol->on_close)
    protocol->on_close(udata);
  if (tls) {
    /* TODO: TLS cleanup */
  }
  return NULL;
}

/* *****************************************************************************
Misc
***************************************************************************** */

/** Returns true if an IO is busy with an ongoing task. */
int fio_is_busy(fio_s *io) { return (int)io->lock; }

/** Suspends future "on_data" events for the IO. */
void fio_suspend(fio_s *io) { io->state |= FIO_IO_SUSPENDED_BIT; }

/* *****************************************************************************
Monitoring IO
***************************************************************************** */

FIO_IFUNC void fio_monitor_read(fio_s *io) {
#if 0
  short e = fio_sock_wait_io(io->fd, POLLIN, 0);
  if (e != -1 && (POLLIN & e)) {
    fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                   fio_ev_on_data,
                   io,
                   NULL);
  }
  if (e)
    return;
#endif
  fio_monitor_monitor(io->fd, io, POLLIN);
}
FIO_IFUNC void fio_monitor_write(fio_s *io) {
#if 0
  short e = fio_sock_wait_io(io->fd, POLLOUT, 0);
  if (e != -1 && (POLLOUT & e)) {
    fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                   fio_ev_on_data,
                   io,
                   NULL);
  }
  if (e)
    return;
#endif
  fio_monitor_monitor(io->fd, io, POLLIN);
}
FIO_IFUNC void fio_monitor_all(fio_s *io) {
  fio_monitor_monitor(io->fd, io, POLLIN | POLLOUT);
}

/* *****************************************************************************
Read
***************************************************************************** */

/**
 * Reads data to the buffer, if any data exists. Returns the number of bytes
 * read.
 *
 * NOTE: zero (`0`) is a valid return value meaning no data was available.
 */
size_t fio_read(fio_s *io, void *buf, size_t len) {
  ssize_t r = fio_sock_read(io->fd, buf, len);
  if (r > 0) {
    fio_touch(io);
    return r;
  }
  if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 0;
  fio_close(io);
  return 0;
}

/* *****************************************************************************
Write
***************************************************************************** */

static void fio_write2___task(void *io_, void *packet) {
  fio_s *io = io_;
  if (!io || (io->state & FIO_IO_CLOSED_BIT))
    goto error;
  fio_stream_add(&io->stream, packet);
  fio_ev_on_ready(io, io->udata); /* will call fio_free2 */
  return;
error:
  fio_stream_pack_free(packet);
  fio_free2(io);
}

void fio_write2___(void); /* Sublime Text marker*/
/**
 * Writes data to the outgoing buffer and schedules the buffer to be sent.
 */
void fio_write2 FIO_NOOP(fio_s *io, fio_write_args_s args) {
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
  if (!io)
    goto io_error;
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_write2___task,
                 .udata1 = fio_dup(io),
                 .udata2 = packet);
  fio_io_thread_wake();
  return;
error:
  FIO_LOG_ERROR("couldn't create user-packet for IO %p", (void *)io);
  if (args.dealloc)
    args.dealloc(args.buf);
  return;
io_error:
  fio_stream_pack_free(packet);
  FIO_LOG_ERROR("Invalid IO (NULL) for user-packet");
  return;
}

/** Marks the IO for closure as soon as the pending data was sent. */
void fio_close(fio_s *io) {
  if (io && (io->state & FIO_IO_OPEN) &&
      !(fio_atomic_or(&io->state, FIO_IO_CLOSED_BIT) & FIO_IO_CLOSED_BIT)) {
    FIO_LOG_DEBUG("scheduling IO %p (fd %d) for closure", (void *)io, io->fd);
    fio_monitor_forget(io->fd);
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   .fn = fio_ev_on_ready,
                   .udata1 = fio_dup(io));
  }
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio___after_fork___io(void) {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "IO protocol ownership error");
      FIO_LOG_DEBUG("cleanup for IO %p (fd %d)", (void *)io, io->fd);
      fio_close_now_unsafe(io);
    }
    FIO_LIST_REMOVE(&pr->reserved.protocols);
  }
}

static void fio___start_shutdown(void) {
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      fio_queue_push_urgent(fio_queue_select(pr->reserved.flags),
                            .fn = fio_ev_on_shutdown,
                            .udata1 = fio_dup(io),
                            .udata2 = NULL);
    }
  }
}
/* *****************************************************************************
Internal helpers
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
On Ready (User Callback Handling)
***************************************************************************** */

static void fio_ev_on_ready_user(void *io_, void *udata) {
  fio_s *const io = io_;
  if ((io->state & FIO_IO_OPEN) && !fio_stream_any(&io->stream)) {
    if (fio_trylock(&io->lock))
      goto reschedule;
    io->protocol->on_ready(io);
    fio_unlock(&io->lock);
  }
  fio_undup(io);
  return;

reschedule:
  fio_queue_push_urgent(fio_queue_select(io->protocol->reserved.flags),
                        .fn = fio_ev_on_ready_user,
                        .udata1 = io,
                        .udata2 = udata);
}

/* *****************************************************************************
On Ready (System Callback Handling)
***************************************************************************** */

static void fio_ev_on_ready(void *io_, void *udata) {
  fio_s *const io = io_;
  if ((io->state & FIO_IO_OPEN)) {
    char buf_mem[FIO_SOCKET_BUFFER_PER_WRITE];
    size_t total = 0;
    for (;;) {
      size_t len = FIO_SOCKET_BUFFER_PER_WRITE;
      char *buf = buf_mem;
      fio_stream_read(&io->stream, &buf, &len);
      if (!len)
        break;
      ssize_t r = fio_sock_write(io->fd, buf, len);
      if (r > 0) {
        total += r;
        fio_stream_advance(&io->stream, len);
        continue;
      } else if (r == -1 || errno == EWOULDBLOCK || errno == EAGAIN ||
                 errno == EINTR) {
        break;
      } else {
        fio_close_now_unsafe(io);
        goto finish;
      }
    }
    if (total)
      fio_touch___unsafe(io, NULL);
    if (!fio_stream_any(&io->stream)) {
      if ((io->state & FIO_IO_CLOSED_BIT)) {
        fio_close_now_unsafe(io);
      } else {
        fio_queue_push_urgent(fio_queue_select(io->protocol->reserved.flags),
                              .fn = fio_ev_on_ready_user,
                              .udata1 = io,
                              .udata2 = udata);
        return; /* fio_undup will be called after the user's on_ready event */
      }
    } else {
      fio_monitor_write(io);
    }
  }
finish:
  fio_free2(io);
}

/* *****************************************************************************
On Data
***************************************************************************** */

static void fio_ev_on_data(void *io_, void *udata) {
  fio_s *const io = io_;
  if (!(io->state & FIO_IO_CLOSED)) {
    if (fio_trylock(&io->lock))
      goto reschedule;
    io->protocol->on_data(io);
    fio_unlock(&io->lock);
    if (!(io->state & FIO_IO_CLOSED)) {
      /* this also tests for the suspended flag (0x02) */
      fio_monitor_read(io);
    }
  } else if ((io->state & FIO_IO_OPEN)) {
    fio_monitor_write(io);
    FIO_LOG_DEBUG("skipping on_data callback for IO %p (fd %d)", io_, io->fd);
  }
  fio_undup(io);
  return;

reschedule:
  FIO_LOG_DEBUG("rescheduling on_data for IO %p (fd %d)", io_, io->fd);
  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 .fn = fio_ev_on_data,
                 .udata1 = io,
                 .udata2 = udata);
}

/* *****************************************************************************
On Timeout
***************************************************************************** */

static void fio_ev_on_timeout(void *io_, void *udata) {
  fio_s *const io = io_;
  if ((io->state & FIO_IO_OPEN)) {
    io->protocol->on_timeout(io);
  } else {
    FIO_LOG_WARNING("timeout event on a non-open IO %p (fd %d)", io_, io->fd);
  }
  fio_undup(io);
  return;
  (void)udata;
}

/* *****************************************************************************
On Shutdown
***************************************************************************** */

static void fio_ev_on_shutdown(void *io_, void *udata) {
  fio_s *const io = io_;
  if (fio_trylock(&io->lock))
    goto reschedule;
  io->protocol->on_shutdown(io);
  fio_close(io);
  fio_unlock(&io->lock);
  fio_undup(io);
  return;

reschedule:
  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 .fn = fio_ev_on_shutdown,
                 .udata1 = io,
                 .udata2 = udata);
}

/* *****************************************************************************
On Close
***************************************************************************** */

static void fio_ev_on_close(void *io, void *udata) {
  (void)udata;
  fio_close_now_unsafe(io);
  fio_free2(io);
}
/* *****************************************************************************
IO Polling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO Polling settings
***************************************************************************** */

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

/* *****************************************************************************
Polling Timeout Calculation
***************************************************************************** */

static int fio_poll_timeout_calc(void) {
  int t = fio_timer_next_at(&fio_data.timers) - fio_data.tick;
  if (t <= 0 || t > FIO_POLL_TICK)
    t = FIO_POLL_TICK;
  return t;
}

/* *****************************************************************************
Polling Wrapper Tasks
***************************************************************************** */

FIO_IFUNC void fio___poll_ev_wrap__user_task(void *fn_, void *io_) {
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.p = fn_};
  fio_s *io = io_;
  if (!fio_is_valid(io))
    goto io_invalid;

  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 .fn = u.fn,
                 .udata1 = fio_dup(io));
  return;
io_invalid:
  FIO_LOG_DEBUG("IO validation failed for IO %p", (void *)io);
}

FIO_IFUNC void fio___poll_ev_wrap__io_task(void *fn_, void *io_) {
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.p = fn_};
  fio_s *io = io_;
  if (!fio_is_valid(io))
    goto io_invalid;

  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = u.fn, .udata1 = fio_dup(io));
  return;
io_invalid:
  FIO_LOG_DEBUG("IO validation failed for IO %p", (void *)io);
}

FIO_IFUNC void fio___poll_ev_wrap__schedule(void (*wrapper)(void *, void *),
                                            void (*fn)(void *, void *),
                                            void *io) {
  /* delay task processing to after all IO object references increased */
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.fn = fn};
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio___poll_ev_wrap__user_task,
                 .udata1 = u.p,
                 .udata2 = io);
}

/* *****************************************************************************
Polling Callbacks
***************************************************************************** */

FIO_IFUNC void fio___poll_on_data(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_data detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__user_task,
                               fio_ev_on_data,
                               io);
}
FIO_IFUNC void fio___poll_on_ready(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_ready detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_ready,
                               io);
}

FIO_IFUNC void fio___poll_on_close(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_close detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_close,
                               io);
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

FIO_SFUNC void fio_monitor_destroy(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_SFUNC void fio_monitor_init(void) {
  fio_monitor_destroy();
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
  fio_monitor_destroy();
  exit(errno);
  return;
}

FIO_IFUNC int fio___epoll_add2(int fd,
                               void *udata,
                               uint32_t events,
                               int ep_fd) {
  int ret = -1;
  struct epoll_event chevent;
  if (fd == -1)
    return ret;
  do {
    errno = 0;
    chevent = (struct epoll_event){
        .events = events,
        .data.ptr = udata,
    };
    ret = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &chevent);
    if (ret == -1 && errno == ENOENT) {
      errno = 0;
      chevent = (struct epoll_event){
          .events = events,
          .data.ptr = udata,
      };
      ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &chevent);
    }
  } while (errno == EINTR);

  return ret;
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  if ((flags & POLLIN) &&
      fio___epoll_add2(fd,
                       udata,
                       (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                       evio_fd[1]) == -1)
    return;
  if ((flags & POLLOUT))
    fio___epoll_add2(fd,
                     udata,
                     (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                     evio_fd[2]);
  return;
}

FIO_IFUNC void fio_monitor_forget(int fd) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN)};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

FIO_SFUNC int fio_monitor_review(int timeout_millisec) {
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
          // errors are handled as disconnections (on_close)
          fio___poll_on_close(0, events[i].data.ptr);
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLIN)
            fio___poll_on_data(0, events[i].data.ptr);
          if (events[i].events & EPOLLOUT)
            fio___poll_on_ready(0, events[i].data.ptr);
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

FIO_SFUNC void fio_monitor_destroy(void) {
  if (evio_fd != -1)
    close(evio_fd);
}

FIO_SFUNC void fio_monitor_init(void) {
  fio_monitor_destroy();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  struct kevent chevent[2];
  int i = 0;
  if ((flags & POLLIN)) {
    EV_SET(chevent,
           fd,
           EVFILT_READ,
           EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
           0,
           0,
           udata);
    ++i;
  }
  if ((flags & POLLOUT)) {
    EV_SET(chevent + i,
           fd,
           EVFILT_WRITE,
           EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
           0,
           0,
           udata);
  }
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, i, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_SFUNC void fio_monitor_forget(int fd) {
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

FIO_SFUNC int fio_monitor_review(int timeout_) {
  if (evio_fd < 0)
    return -1;
  struct kevent events[FIO_POLL_MAX_EVENTS] = {{0}};

  const struct timespec timeout = {.tv_sec = (timeout_ / 1024),
                                   .tv_nsec =
                                       ((timeout_ & (1023UL)) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, FIO_POLL_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_WRITE) {
        fio___poll_on_ready(0, events[i].udata);
      } else if (events[i].filter == EVFILT_READ) {
        fio___poll_on_data(0, events[i].udata);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio___poll_on_close(0, events[i].udata);
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

static fio_poll_s fio___poll_data =
    FIO_POLL_INIT(fio___poll_on_data, fio___poll_on_ready, fio___poll_on_close);
FIO_SFUNC void fio_monitor_destroy(void) { fio_poll_destroy(&fio___poll_data); }

FIO_SFUNC void fio_monitor_init(void) {
  fio___poll_data = (fio_poll_s)FIO_POLL_INIT(fio___poll_on_data,
                                              fio___poll_on_ready,
                                              fio___poll_on_close);
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  fio_poll_monitor(&fio___poll_data, fd, udata, flags);
}
FIO_SFUNC void fio_monitor_forget(int fd) {
  fio_poll_forget(&fio___poll_data, fd);
}

/** returns non-zero if events were scheduled, 0 if idle */
FIO_SFUNC int fio_monitor_review(int timeout) {
  return fio_poll_review(&fio___poll_data, timeout);
}

#endif /* FIO_ENGINE_POLL */
/* *****************************************************************************
Event / IO Reactor Pattern
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
The Reactor event scheduler
***************************************************************************** */
FIO_SFUNC void fio___schedule_events(void) {
  static int old = 0;
  /* make sure the user thread is active */
  if (fio_queue_count(FIO_QUEUE_USER))
    fio_user_thread_wake();
  /* schedule IO events */
  fio_io_wakeup_prep();
  /* make sure all system events were processed */
  fio_queue_perform_all(FIO_QUEUE_SYSTEM);
  int c = fio_monitor_review(fio_poll_timeout_calc());
  fio_data.tick = fio_time2milli(fio_time_real());
  /* schedule Signal events */
  c += fio_signal_review();
  /* review IO timeouts */
  fio___review_timeouts();
  /* schedule timer events */
  fio_timer_push2queue(FIO_QUEUE_USER, &fio_data.timers, fio_data.tick);
  /* schedule on_idle events */
  if (!c) {
    if (old) {
      fio_state_callback_force(FIO_CALL_ON_IDLE);
    }
    /* test that parent process is active (during idle cycle) */
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
  /* make sure the user thread is active after all events were scheduled */
  if (fio_queue_count(FIO_QUEUE_USER))
    fio_user_thread_wake();
}

/* *****************************************************************************
Shutdown cycle
***************************************************************************** */

/* cycles until all existing IO objects were closed. */
static void fio___shutdown_cycle(void) {
  for (;;) {
    if (!fio_queue_perform(FIO_QUEUE_SYSTEM))
      continue;
    fio_signal_review();
    if (!fio_queue_perform(FIO_QUEUE_USER))
      continue;
    if (fio___review_timeouts())
      continue;
    fio_data.tick = fio_time2milli(fio_time_real());
    if (fio_monitor_review(FIO_POLL_SHUTDOWN_TICK))
      continue;
    if (fio___is_waiting_on_io())
      continue;
    break;
  }
}

/* *****************************************************************************
Event consumption and cycling according to state (move some `if`s out of loop).
***************************************************************************** */

/** Worker no-threads cycle */
static void fio___worker_nothread_cycle(void) {
  for (;;) {
    if (!fio_queue_perform(FIO_QUEUE_SYSTEM))
      continue;
    if (!fio_queue_perform(FIO_QUEUE_USER))
      continue;
    if (!fio_data.running)
      break;
    fio___schedule_events();
  }
}

/** Worker thread cycle */
static void *fio___user_thread_cycle(void *ignr) {
  (void)ignr;
  for (;;) {
    fio_queue_perform_all(FIO_QUEUE_USER);
    if (fio_data.running) {
      fio_user_thread_suspent();
      continue;
    }
    return NULL;
  }
}
/** Worker cycle */
static void fio___worker_cycle(void) {
  fio_queue_perform_all(FIO_QUEUE_SYSTEM);
  while (fio_data.running) {
    fio___schedule_events();
    fio_queue_perform_all(FIO_QUEUE_SYSTEM);
  }
}

/* *****************************************************************************
Worker Processes work cycle
***************************************************************************** */

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
  if (threads) {
    fio_user_thread_wake_all();
    for (size_t i = 0; i < fio_data.threads; ++i) {
      fio_monitor_review(FIO_POLL_SHUTDOWN_TICK);
      fio_queue_perform_all(FIO_QUEUE_SYSTEM);
      if (fio_thread_join(threads[i]))
        FIO_LOG_ERROR("Couldn't join worker thread.");
    }
    free(threads);
  }
  fio___start_shutdown();
  fio___shutdown_cycle();
  if (fio_data.io_wake_io)
    fio_close_now_unsafe(fio_data.io_wake_io);
  fio_queue_perform_all(FIO_QUEUE_SYSTEM);
  fio_close_wakeup_pipes();
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
    FIO_LOG_INFO("(%d) IO shutdown complete.", (int)fio_data.master);
  }
}

/* *****************************************************************************
Spawning Worker Processes
***************************************************************************** */

static void fio_spawn_worker(void *ignr_1, void *ignr_2);

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
      fio_queue_push(FIO_QUEUE_SYSTEM, .fn = fio_spawn_worker);
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
  static fio_lock_i GIL = FIO_LOCK_INIT;
  fio_thread_t t;
  if (!fio_data.is_master)
    return;
  fio_lock(&GIL);
  if (fio_thread_create(&t, fio_worker_sentinal, (void *)&GIL)) {
    fio_unlock(&GIL);
    FIO_LOG_FATAL(
        "sentinel thread creation failed, no worker will be spawned.");
    fio_stop();
  }
  fio_thread_detach(t);
  fio_lock(&GIL);
  fio_unlock(&GIL);
  (void)ignr_1;
  (void)ignr_2;
}
/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

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
  fio_state_callback_force(FIO_CALL_PRE_START);
  fio_signal_handler_init();
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
  fio_reset_wakeup_pipes();
  if (fio_data.workers) {
    fio_data.is_worker = 0;
    for (size_t i = 0; i < fio_data.workers; ++i) {
      fio_spawn_worker(NULL, NULL);
    }
    fio___worker_nothread_cycle();
    fio___start_shutdown();
    fio___shutdown_cycle();
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
/* *****************************************************************************
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2) {
  fio_queue_push(FIO_QUEUE_USER,
                 .fn = task,
                 .udata1 = udata1,
                 .udata2 = udata2);
}

FIO_SFUNC void fio_io_task_wrapper(void *task_, void *ignr_) {
  fio_queue_task_s *t = task_;
  fio_s *io = t->udata1;
  union {
    void (*fn)(fio_s *io, void *udata);
    void (*fn2)(void *io, void *udata);
    void *p;
  } u = {.fn2 = t->fn};
  if (fio_trylock(&io->lock))
    goto reschedule;
  u.fn(io, t->udata2);
  fio_unlock(&io->lock);
  fio_free(t);
  fio_free(io);
  return;
reschedule:
  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 .fn = fio_io_task_wrapper,
                 .udata1 = task_,
                 .udata2 = ignr_);
}

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_s *io,
                  void (*task)(fio_s *io, void *udata),
                  void *udata) {
  union {
    void (*fn)(fio_s *io, void *udata);
    void (*fn2)(void *io, void *udata);
    void *p;
  } u;
  u.fn = task;
  fio_queue_task_s *t = fio_malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  *t = (fio_queue_task_s){.fn = u.fn2, .udata1 = fio_dup(io), .udata2 = udata};
  fio_queue_push(fio_queue_select(io->protocol->reserved.flags),
                 .fn = fio_io_task_wrapper,
                 .udata1 = io,
                 .udata2 = udata);
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
Listening to Incoming Connections
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

FIO_SFUNC void fio_listen_on_data(fio_s *io) {
  struct fio_listen_args *l = fio_udata_get(io);
  int fd;
  while ((fd = accept(io->fd, NULL, NULL)) != -1) {
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

FIO_SFUNC void fio_listen_on_ready(fio_s *io) {
  struct fio_listen_args *l = fio_udata_get(io);
  FIO_LOG_INFO("(%d) started listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)getpid()),
               l->url);
}

static fio_protocol_s FIO_PROTOCOL_LISTEN = {
    .on_data = fio_listen_on_data,
    .on_ready = fio_listen_on_ready,
    .on_close = fio_listen_on_close,
    .on_timeout = mock_ping_eternal,
};

FIO_SFUNC void fio_listen___attach(void *udata) {
  struct fio_listen_args *l = udata;
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
  FIO_LOG_DEBUG("Called dup(%d) to attach %d as a listening socket.",
                l->reserved,
                fd);
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
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Post fork cleanup
***************************************************************************** */
static void fio___after_fork(void *ignr_) {
  (void)ignr_;
  fio_malloc_after_fork();
  fio___after_fork___core();
  fio___after_fork___io();
}
/* *****************************************************************************
State data initialization
***************************************************************************** */

FIO_DESTRUCTOR(fio_cleanup_at_exit) {
  FIO_LOG_DEBUG("Calling AT_EXIT callbacks.");
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_thread_mutex_destroy(&fio_data.env_lock);
  env_destroy(&fio_data.env);
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  while (fio_queue_count(FIO_QUEUE_SYSTEM) + fio_queue_count(FIO_QUEUE_USER)) {
    fio_queue_perform_all(FIO_QUEUE_SYSTEM);
    fio_queue_perform_all(FIO_QUEUE_USER);
  }
  fio_close_wakeup_pipes();
  if (fio_data.io_wake_io) {
    fio_free2(fio_data.io_wake_io);
    fio_data.io_wake_io = NULL;
  }
  while (fio_queue_count(FIO_QUEUE_SYSTEM) + fio_queue_count(FIO_QUEUE_USER)) {
    fio_queue_perform_all(FIO_QUEUE_SYSTEM);
    fio_queue_perform_all(FIO_QUEUE_USER);
  }
  fio_monitor_destroy();
  fio_cli_end();
  fio_invalidate_all();
  for (int i = 0; i < FIO_CALL_NEVER; ++i)
    fio_state_callback_clear((callback_type_e)i);
}

FIO_CONSTRUCTOR(fio_data_state_init) {
  FIO_LOG_DEBUG("initializing facio.io IO state.");
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.master = getpid();
  fio_data.tick = fio_time2milli(fio_time_real());
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.timers = (fio_timer_queue_s)FIO_TIMER_QUEUE_INIT;
  fio_data.tasks[0] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_data.tasks[0]);
  fio_data.tasks[1] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_data.tasks[1]);
  fio_monitor_init();
  fio_protocol_validate(&FIO_PROTOCOL_HIJACK);
  FIO_PROTOCOL_HIJACK.on_timeout = FIO_PING_ETERNAL;
  FIO_PROTOCOL_HIJACK.reserved.protocols =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.protocols);
  FIO_PROTOCOL_HIJACK.reserved.ios =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.ios);
  FIO_PROTOCOL_HIJACK.reserved.flags |= 1;

  fio_state_callback_add(FIO_CALL_IN_CHILD, fio___after_fork, NULL);
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
