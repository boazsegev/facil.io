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

#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_SIGNAL
#include "fio-stl.h"

/* for storing ENV string keys and all sorts of stuff */
#define FIO_STR_SMALL sstr
#include "fio-stl.h"

/* *****************************************************************************
Forking
***************************************************************************** */
static void fio___after_fork(void);

FIO_WEAK pid_t fio_fork(void) { return fork(); }

/* *****************************************************************************
IO Registry - NOT thread safe (access from IO core thread only)
***************************************************************************** */

#define FIO_UMAP_NAME          fio_validity_map
#define FIO_MAP_TYPE           fio_s *
#define FIO_MAP_HASH_FN(o)     fio_risky_ptr(o)
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))

#if 1
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#else
#define FIO_MEMORY_NAME        fio___val_mem
#define FIO_MEMORY_ARENA_COUNT 1
#endif

#ifndef FIO_VALIDATE_IO_MUTEX
/* required only if exposing fio_is_valid to users. */
#define FIO_VALIDATE_IO_MUTEX 0
#endif

#include <fio-stl.h>

/* *****************************************************************************
Reference Counter Debugging
***************************************************************************** */
#ifndef FIO_DEBUG_REF
#if DEBUG
#define FIO_DEBUG_REF 0
#else
#define FIO_DEBUG_REF 0
#endif
#endif

#if FIO_DEBUG_REF

#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_UMAP_NAME               ref_dbg
#define FIO_MAP_TYPE                uintptr_t
#define FIO_MAP_KEY                 sstr_s
#define FIO_MAP_KEY_COPY(dest, src) sstr_init_copy2(&(dest), &(src))
#define FIO_MAP_KEY_DESTROY(k)      sstr_destroy(&k)
#define FIO_MAP_KEY_DISCARD         FIO_MAP_KEY_DESTROY
#define FIO_MAP_KEY_CMP(a, b)       sstr_is_eq(&(a), &(b))
#include <fio-stl.h>

ref_dbg_s ref_dbg[4] = {FIO_MAP_INIT};
fio_thread_mutex_t ref_dbg_lock = FIO_THREAD_MUTEX_INIT;

FIO_IFUNC void fio_func_called(const char *func, uint_fast8_t i) {
  sstr_s s;
  sstr_init_const(&s, func, strlen(func));
  fio_thread_mutex_lock(&ref_dbg_lock);
  uintptr_t *a = ref_dbg_get_ptr(ref_dbg + i, sstr_hash(&s, 0), s);
  if (a)
    ++a[0];
  else
    ref_dbg_set(ref_dbg + i, sstr_hash(&s, 0), s, 1, NULL);
  fio_thread_mutex_unlock(&ref_dbg_lock);
}

#define MARK_FUNC() fio_func_called(__func__, 0)

FIO_DESTRUCTOR(fio_io_ref_dbg) {
  const char *name[] = {
      "call counter",
      "fio_dup",
      "fio_undup",
      "fio_free2 (internal)",
  };
  for (int i = 0; i < 4; ++i) {
    size_t total = 0;
    fprintf(stderr, "\n\t%s called by:\n", name[i]);
    FIO_MAP_EACH(ref_dbg, (ref_dbg + i), c) {
      total += c->obj.value;
      fprintf(stderr,
              "\t- %*.*s: %zu\n",
              (int)35,
              (int)35,
              sstr2ptr(&c->obj.key),
              (size_t)c->obj.value);
    }
    fprintf(stderr, "\t- total: %zu\n", total);
    ref_dbg_destroy(ref_dbg + i);
  }
}
#else
#define MARK_FUNC()
#endif
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
#define FIO_POLL_SHUTDOWN_TICK 100
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
#define FIO_QUEUE_IO(io) fio_queue_select((io)->protocol->reserved.flags)

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

typedef struct {
  fio_thread_mutex_t lock;
  env_s env;
} env_safe_s;

#define ENV_SAFE_INIT                                                          \
  (env_safe_s) { .lock = FIO_THREAD_MUTEX_INIT, .env = FIO_MAP_INIT }

FIO_IFUNC void env_safe_set(env_safe_s *e,
                            sstr_s key,
                            intptr_t type,
                            env_obj_s val) {
  const uint64_t hash = sstr_hash(&key, (uint64_t)type);
  fio_thread_mutex_lock(&e->lock);
  env_set(&e->env, hash, key, val, NULL);
  fio_thread_mutex_unlock(&e->lock);
}

FIO_IFUNC int env_safe_unset(env_safe_s *e, sstr_s key, intptr_t type) {
  int r;
  const uint64_t hash = sstr_hash(&key, (uint64_t)type);
  env_obj_s old;
  fio_thread_mutex_lock(&e->lock);
  r = env_remove(&e->env, hash, key, &old);
  fio_thread_mutex_unlock(&e->lock);
  return r;
}

FIO_IFUNC int env_safe_remove(env_safe_s *e, sstr_s key, intptr_t type) {
  int r;
  const uint64_t hash = sstr_hash(&key, (uint64_t)type);
  fio_thread_mutex_lock(&e->lock);
  r = env_remove(&e->env, hash, key, NULL);
  fio_thread_mutex_unlock(&e->lock);
  return r;
}

FIO_IFUNC void env_safe_destroy(env_safe_s *e) {
  env_destroy(&e->env);
  fio_thread_mutex_destroy(&e->lock);
}

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
    MARK_FUNC();
    p->reserved.ios = FIO_LIST_INIT(p->reserved.ios);
    p->reserved.protocols = FIO_LIST_INIT(p->reserved.protocols);
    p->reserved.flags |= 8;
    if (!p->on_attach)
      p->on_attach = mock_on_ready;
    if (!p->on_data)
      p->on_data = mock_on_data;
    if (!p->on_ready)
      p->on_ready = mock_on_ready;
    if (!p->on_close)
      p->on_close = mock_on_close;
    if (!p->on_shutdown)
      p->on_shutdown = mock_on_shutdown;
    if (!p->on_timeout)
      p->on_timeout = mock_timeout;
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
Global State
***************************************************************************** */

static struct {
  FIO_LIST_HEAD protocols;
  env_safe_s env;
  int64_t tick;
  fio_validity_map_s valid;
#if FIO_VALIDATE_IO_MUTEX
  fio_thread_mutex_t valid_lock;
#endif

  fio_s *io_wake_io;
  struct {
    int in;
    int out;
  } thread_suspenders, io_wake;
  pid_t master;
  pid_t pid;
  uint16_t threads;
  uint16_t workers;
  uint8_t is_master;
  uint8_t is_worker;
  uint8_t pubsub_filter;
  volatile uint8_t running;
  fio_timer_queue_s timers;
} fio_data = {
    .env = ENV_SAFE_INIT,
    .valid = FIO_MAP_INIT,
    .thread_suspenders = {-1, -1},
    .io_wake = {-1, -1},
    .is_master = 1,
    .is_worker = 1,
};
static fio_queue_s fio_tasks[2];

/* *****************************************************************************
Queue Helpers
***************************************************************************** */

/** Returns the task queue according to the flag specified. */
FIO_IFUNC fio_queue_s *fio_queue_select(uintptr_t flag) {
  return fio_tasks + (flag & 1U);
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
             (int)fio_data.pid,
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

#if FIO_VALIDATE_IO_MUTEX
#define FIO_VALIDATE_LOCK()    fio_thread_mutex_lock(&fio_data.valid_lock)
#define FIO_VALIDATE_UNLOCK()  fio_thread_mutex_unlock(&fio_data.valid_lock)
#define FIO_VALIDATE_DESTROY() fio_thread_mutex_destroy(&fio_data.valid_lock)
#else
#define FIO_VALIDATE_LOCK()
#define FIO_VALIDATE_UNLOCK()
#define FIO_VALIDATE_DESTROY()
#endif

FIO_IFUNC int fio_is_valid(fio_s *io) {
  FIO_VALIDATE_LOCK();
  fio_s *r = fio_validity_map_get(&fio_data.valid, fio_risky_ptr(io), io);
  FIO_VALIDATE_UNLOCK();
  return r == io;
}

FIO_IFUNC void fio_set_valid(fio_s *io) {
  FIO_VALIDATE_LOCK();
  fio_validity_map_set(&fio_data.valid, fio_risky_ptr(io), io, NULL);
  FIO_VALIDATE_UNLOCK();
  FIO_ASSERT_DEBUG(fio_is_valid(io),
                   "(%d) IO validity set, but map reported as invalid!",
                   (int)fio_data.pid);
  FIO_LOG_DEBUG2("(%d) IO %p is now valid", (int)fio_data.pid, (void *)io);
}

FIO_IFUNC void fio_set_invalid(fio_s *io) {
  fio_s *old = NULL;
  FIO_LOG_DEBUG2("(%d) IO %p is no longer valid",
                 (int)fio_data.pid,
                 (void *)io);
  FIO_VALIDATE_LOCK();
  fio_validity_map_remove(&fio_data.valid, fio_risky_ptr(io), io, &old);
  FIO_VALIDATE_UNLOCK();
  FIO_ASSERT_DEBUG(!old || old == io,
                   "(%d) invalidity map corruption (%p != %p)!",
                   (int)fio_data.pid,
                   io,
                   old);
  FIO_ASSERT_DEBUG(!fio_is_valid(io),
                   "(%d) IO validity removed, but map reported as valid!",
                   (int)fio_data.pid);
}

FIO_IFUNC void fio_invalidate_all() {
  FIO_VALIDATE_LOCK();
  fio_validity_map_destroy(&fio_data.valid);
  FIO_VALIDATE_UNLOCK();
  FIO_VALIDATE_DESTROY();
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

FIO_SFUNC void fio_user_thread_suspend(void) {
  char buf[sizeof(size_t)];
  int r = fio_sock_read(fio_data.thread_suspenders.in, buf, sizeof(size_t));
  if (!fio_queue_perform(FIO_QUEUE_USER))
    return;
  fio_sock_wait_io(fio_data.thread_suspenders.in, POLLIN, 2000);
  (void)r;
}

FIO_IFUNC void fio_user_thread_wake(void) {
  char buf[sizeof(size_t)] = {0};
  int i = fio_sock_write(fio_data.thread_suspenders.out, buf, sizeof(size_t));
  (void)i;
}

FIO_IFUNC void fio_user_thread_wake_all(void) {
  fio_reset_pipes___(&fio_data.thread_suspenders.in, "user");
}

FIO_IFUNC void fio_io_thread_wake(void) {
  char buf[sizeof(size_t)] = {0};
  int i = fio_sock_write(fio_data.thread_suspenders.out, buf, sizeof(size_t));
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
  FIO_LOG_DDEBUG2("(%d) IO wakeup fio_s freed", (int)fio_data.pid);
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
    FIO_LOG_INFO("(%d) received shutdown signal.", (int)fio_data.pid);
    if (fio_data.is_master && fio_data.workers) {
      kill(0, sig);
    }
    fio_io_thread_wake();
    fio_user_thread_wake_all();
  } else {
    FIO_LOG_FATAL("(%d) received another shutdown signal while shutting down.",
                  (int)fio_data.pid);
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
    FIO_LOG_INFO("(%d) received worker restart signal.", (int)fio_data.pid);
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
  FIO_IO_OPEN = 1,      /* 0b00001 */
  FIO_IO_SUSPENDED = 2, /* 0b00010 */
  FIO_IO_THROTTLED = 4, /* 0b00100 */
  FIO_IO_CLOSING = 8,   /* 0b01000 */
  FIO_IO_CLOSED = 14,   /* 0b01110 */
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
  /* env */
  env_safe_s env;
  /* timer handler */
  int64_t active;
  /* socket */
  int fd;
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
  MARK_FUNC();
}

FIO_SFUNC void fio___init_task(void *io, void *ignr_) {
  fio_set_valid(io);
  fio_free2(io);
  (void)ignr_;
}
/* IO object constructor (MUST be thread safe). */
FIO_SFUNC void fio___init(fio_s *io) {
  *io = (fio_s){
      .protocol = &FIO_PROTOCOL_HIJACK,
      .state = FIO_IO_OPEN,
      .stream = FIO_STREAM_INIT(io->stream),
      .timeouts = FIO_LIST_INIT(io->timeouts),
      .env = ENV_SAFE_INIT,
      .fd = -1,
      .lock = FIO_LOCK_INIT,
  };
  fio_queue_push(FIO_QUEUE_SYSTEM, fio___init_task, fio_dup2(io));
}

/* IO object destructor (NOT thread safe). */
FIO_SFUNC void fio___destroy(fio_s *io) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.fn = io->protocol->on_close};

  fio_set_invalid(io);
  env_safe_destroy(&io->env);
  fio_stream_destroy(&io->stream);
  // o->rw_hooks->cleanup(io->rw_udata);
  FIO_LIST_REMOVE(&io->timeouts);
  fio_monitor_forget(io->fd);
  fio_sock_close(io->fd);
  if (io->protocol->reserved.ios.next == io->protocol->reserved.ios.prev) {
    FIO_LIST_REMOVE(&io->protocol->reserved.protocols);
  }
  fio_queue_push(FIO_QUEUE_IO(io), fio___deferred_on_close, u.p, io->udata);
  FIO_LOG_DEBUG2("(%d) IO %p (fd %d) freed.",
                 (int)fio_data.pid,
                 (void *)io,
                 (int)(io->fd));
}

#define FIO_REF_NAME         fio
#define FIO_REF_INIT(obj)    fio___init(&(obj))
#define FIO_REF_DESTROY(obj) fio___destroy(&(obj))
#include <fio-stl.h>

/* *****************************************************************************
Reference Counter Debugging
***************************************************************************** */
#if FIO_DEBUG_REF

FIO_IFUNC fio_s *fio_dup2_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 1);
  return fio_dup2(io);
}

FIO_IFUNC void fio_undup_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 2);
  fio_undup(io);
}

FIO_IFUNC void fio_free2_dbg(fio_s *io, const char *func) {
  fio_func_called(func, 3);
  fio_free2(io);
}

#define fio_dup2(io)  fio_dup2_dbg(io, __func__)
#define fio_undup(io) fio_undup_dbg(io, __func__)
#define fio_free2(io) fio_free2_dbg(io, __func__)
#endif
/* *****************************************************************************
Managing reference counting (decrease only in the IO thread)
***************************************************************************** */

/* perform reference count decrease. */
FIO_SFUNC void fio_undup___task(void *io, void *ignr) {
  (void)ignr;
  if (!io)
    return;
  MARK_FUNC();
  /* don't trust users to manage reference counts? */
  if (fio_is_valid(io))
    fio_free2(io);
  else
    FIO_LOG_ERROR("user event attempted to double-free IO %p", io);
}

void fio_undup___(void); /* sublime text marker */
/** Route reference count decrease to the IO thread. */
void fio_undup FIO_NOOP(fio_s *io) {
  if (!io)
    return;
  MARK_FUNC();
  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = fio_undup___task, .udata1 = io);
}

/** Increase reference count. */
fio_s *fio_dup(fio_s *io) { return fio_dup2(io); }
#define fio_dup fio_dup2

/* *****************************************************************************
Closing the IO connection
***************************************************************************** */

/** common safe / unsafe implementation (unsafe = called from user thread). */
FIO_IFUNC void fio_close_now___task(void *io_, void *ignr_) {
  (void)ignr_;
  fio_s *io = io_;
  if (!fio_is_valid(io) || !(io->state & FIO_IO_OPEN)) {
    FIO_LOG_DDEBUG2("(%d) fio_close_now called for closed IO %p",
                    (int)fio_data.pid,
                    io);
    return;
  }
  fio_atomic_exchange(&io->state, FIO_IO_CLOSED);
  fio_monitor_forget(io->fd);
  fio_free2(io);
}

/* Public / safe IO immediate closure. */
void fio_close_now(fio_s *io) {
  fio_atomic_or(&io->state, FIO_IO_CLOSING);
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_close_now___task, io);
  MARK_FUNC();
}

/** Unsafe IO immediate closure (callable only from IO threads). */
FIO_IFUNC void fio_close_now_unsafe(fio_s *io) {
  MARK_FUNC();
  fio_close_now___task(io, NULL);
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
  if (should_free)
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

  // FIO_LOG_DDEBUG2("performing timeout review.");

  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_ASSERT_DEBUG(pr->reserved.flags, "protocol object flags unmarked?!");
    if (!pr->timeout || pr->timeout > FIO_IO_TIMEOUT_MAX)
      pr->timeout = FIO_IO_TIMEOUT_MAX;
    int64_t limit = now_milli - ((int64_t)pr->timeout * 1000);
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "io protocol ownership error");
      if (io->active >= limit)
        break;
      FIO_LOG_DDEBUG2("(%d) scheduling timeout for %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      fio_queue_push(FIO_QUEUE_IO(io),
                     .fn = fio_ev_on_timeout,
                     .udata1 = fio_dup(io),
                     .udata2 = NULL);
      ++c;
    }
  }
  return c;
}

/* *****************************************************************************
Testing for any open IO objects
***************************************************************************** */

FIO_IFUNC void fio___close_all_io_in_protocol(fio_protocol_s *pr) {
  FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
    fio_close_now_unsafe(io);
  }
}

FIO_SFUNC int fio___is_waiting_on_io(void) {
  int c = 0;
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      c = 1;
      FIO_LOG_DDEBUG2("(%d) waiting for IO %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      if ((io->state & FIO_IO_OPEN))
        fio_close(io);
      else
        fio_queue_push(FIO_QUEUE_SYSTEM, fio_ev_on_ready, fio_dup(io));
    }
  }
  return c;
}

/* *****************************************************************************
Testing for any open IO objects
***************************************************************************** */

FIO_SFUNC void fio___close_all_io(void) {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    fio___close_all_io_in_protocol(pr);
  }
}

/* *****************************************************************************
IO object protocol update
***************************************************************************** */

FIO_SFUNC void fio_protocol_set___attached_task(void *io_, void *ignr_) {
  fio_s *io = io_;
  if (fio_trylock(&io->lock))
    goto reschedule;
  io->protocol->on_attach(io);
  fio_unlock(&io->lock);
  // FIO_LOG_DDEBUG2("on_attach callback returned, monitoring IO %p", io_);
  fio_monitor_all(io);
  fio_free2(io);
  return;
reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
                 fio_protocol_set___attached_task,
                 io_,
                 ignr_);
}

/* completes an update for an IO object's protocol */
FIO_SFUNC void fio_protocol_set___task(void *io_, void *old_protocol_) {
  fio_s *io = io_;
  fio_protocol_s *p = io->protocol;
  fio_protocol_s *old = old_protocol_;
  FIO_ASSERT_DEBUG(p != old, "protocol switch with identical protocols!");
  fio_touch___unsafe(io, io);
  if (old && FIO_LIST_IS_EMPTY(&old->reserved.ios)) {
    FIO_LIST_REMOVE(&old->reserved.protocols);
  }
  (void)p; /* if not DEBUG */
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
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_protocol_set___attached_task,
                 .udata1 = fio_dup(io));
}

/* *****************************************************************************
Copy address to string
***************************************************************************** */

/** Caches an attached IO object's peer address. */
FIO_SFUNC void fio___addr_cpy(fio_s *io) {
  /* TODO: Fix Me */
  struct sockaddr addrinfo;
  // size_t len = sizeof(addrinfo);
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
  io->fd = fd;
  io->tls = tls;
  io->udata = udata;
  fio_sock_set_non_block(fd); /* never accept a blocking socket */
  /* fio___addr_cpy(io); // ??? */
  fio_protocol_set(io, protocol);
  FIO_LOG_DEBUG2("(%d) attaching fd %d to IO %p",
                 (int)fio_data.pid,
                 fd,
                 (void *)io);
  MARK_FUNC();
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
void fio_suspend(fio_s *io) { fio_atomic_or(&io->state, FIO_IO_SUSPENDED); }

/* *****************************************************************************
Monitoring IO
***************************************************************************** */

FIO_IFUNC void fio_monitor_read(fio_s *io) {
#if 0
  short e = fio_sock_wait_io(io->fd, POLLIN, 0);
  if (e != -1 && (POLLIN & e)) {
    fio_queue_push(FIO_QUEUE_IO(io),
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
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   fio_ev_on_ready,
                   io,
                   NULL);
  }
  if (e)
    return;
#endif
  fio_monitor_monitor(io->fd, io, POLLOUT);
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
  if (!len | ((r == -1) &
              ((errno == EAGAIN) | (errno == EWOULDBLOCK) | (errno == EINTR))))
    return 0;
  FIO_LOG_DDEBUG2("(%d) fio_read error, closing IO %p (fd %d)",
                  fio_data.pid,
                  (void *)io,
                  io->fd);
  fio_close(io);
  return 0;
}

/* *****************************************************************************
Write
***************************************************************************** */

static void fio_write2___task(void *io_, void *packet) {
  fio_s *io = io_;
  if (!(io->state & FIO_IO_OPEN))
    goto error;
  MARK_FUNC();
  fio_stream_add(&io->stream, packet);
  fio_ev_on_ready(io, io); /* will call fio_free2 */
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
  if (!io || ((io->state & (FIO_IO_OPEN | FIO_IO_CLOSING)) ^ FIO_IO_OPEN))
    goto io_error;
  MARK_FUNC();
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 .fn = fio_write2___task,
                 .udata1 = fio_dup(io),
                 .udata2 = packet);
#if FIO_ENGINE_POLL
  fio_io_thread_wake();
#else
  fio_monitor_write(io);
#endif
  return;
error:
  FIO_LOG_ERROR("couldn't create user-packet for IO %p", (void *)io);
  if (args.dealloc)
    args.dealloc(args.buf);
  return;
io_error:
  fio_stream_pack_free(packet);
  if (!io)
    FIO_LOG_ERROR("Invalid IO (NULL) for user-packet");
  return;
}

/** Marks the IO for closure as soon as the pending data was sent. */
void fio_close(fio_s *io) {
  if (io && (io->state & FIO_IO_OPEN) &&
      !(fio_atomic_or(&io->state, FIO_IO_CLOSING) & FIO_IO_CLOSING)) {
    FIO_LOG_DDEBUG2("(%d) scheduling IO %p (fd %d) for closure",
                    fio_data.pid,
                    (void *)io,
                    io->fd);
    fio_monitor_forget(io->fd);
    fio_queue_push(FIO_QUEUE_SYSTEM, fio_ev_on_ready, fio_dup(io));
    MARK_FUNC();
  }
}

/* *****************************************************************************
Cleanup helpers
***************************************************************************** */

static void fio___after_fork___io(void) {
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      FIO_ASSERT_DEBUG(io->protocol == pr, "IO protocol ownership error");
      FIO_LOG_DDEBUG2("(%d) cleanup for IO %p (fd %d)",
                      (int)fio_data.pid,
                      (void *)io,
                      io->fd);
      fio_close_now_unsafe(io);
    }
    FIO_LIST_REMOVE(&pr->reserved.protocols);
  }
}

static void fio___start_shutdown(void) {
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_LIST_EACH(fio_protocol_s, reserved.protocols, &fio_data.protocols, pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      fio_queue_push(FIO_QUEUE_IO(io),
                     .fn = fio_ev_on_shutdown,
                     .udata1 = fio_dup(io),
                     .udata2 = NULL);
    }
  }
}
/* *****************************************************************************
IO related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */

FIO_IFUNC sstr_s fio_env_name2str(fio_str_info_s name, uint8_t is_const) {
  sstr_s s;
  if (is_const) {
    sstr_init_const(&s, name.buf, name.len);
  } else {
    sstr_init_copy(&s, name.buf, name.len);
  }
  return s;
}

/**
 * Links an object to a connection's lifetime / environment.
 */
void fio_env_set FIO_NOOP(fio_s *io, fio_env_set_args_s args) {
  sstr_s key = fio_env_name2str(args.name, args.const_name);
  env_obj_s val = {
      .udata = args.udata,
      .on_close = args.on_close,
  };
  env_safe_s *e = io ? &io->env : &fio_data.env;
  env_safe_set(e, key, args.type, val);
}

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 */
int fio_env_unset FIO_NOOP(fio_s *io, fio_env_unset_args_s args) {
  sstr_s key = fio_env_name2str(args.name, 1);
  env_safe_s *e = io ? &io->env : &fio_data.env;
  return env_safe_unset(e, key, args.type);
}

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 */
int fio_env_remove FIO_NOOP(fio_s *io, fio_env_unset_args_s args) {
  sstr_s key = fio_env_name2str(args.name, 1);
  env_safe_s *e = io ? &io->env : &fio_data.env;
  return env_safe_remove(e, key, args.type);
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
#if FIO_VALIDATE_IO_MUTEX
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_ready_user - invalid IO: %p", io);
#endif
  if ((io->state & FIO_IO_OPEN) &&
      !(fio_stream_any(&io->stream) | (io->state & FIO_IO_CLOSING))) {
    if (fio_trylock(&io->lock))
      goto reschedule;
    io->protocol->on_ready(io);
    fio_unlock(&io->lock);
  }
  MARK_FUNC();
  fio_undup(io);
  return;

reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_ev_on_ready_user,
                 .udata1 = io,
                 .udata2 = udata);
}

/* *****************************************************************************
On Ready (System Callback Handling)
***************************************************************************** */

static void fio_ev_on_ready(void *io_, void *udata) {
  fio_s *const io = io_;
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_ready - invalid IO: %p", io);
  MARK_FUNC();
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
      } else if (r == -1 &&
                 (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)) {
        break;
      } else {
        fio_close_now_unsafe(io);
        goto finish;
      }
    }
    if (total)
      fio_touch___unsafe(io, NULL);
    if (!fio_stream_any(&io->stream)) {
      if ((io->state & FIO_IO_CLOSING)) {
        fio_close_now_unsafe(io);
      } else {
        if ((io->state & FIO_IO_THROTTLED)) {
          fio_atomic_and(&io->state, ~FIO_IO_THROTTLED);
          fio_monitor_read(io);
        }
        fio_queue_push(FIO_QUEUE_IO(io),
                       .fn = fio_ev_on_ready_user,
                       .udata1 = io,
                       .udata2 = udata);
        return; /* fio_undup will be called after the user's on_ready event */
      }
    } else if ((io->state & FIO_IO_OPEN)) {
      if (fio_stream_length(&io->stream) >= FIO_SOCKET_THROTTLE_LIMIT) {
#ifdef DEBUG
        if (!(io->state & FIO_IO_THROTTLED))
          FIO_LOG_DEBUG2("(%d) throttled IO %p (fd %d)",
                         (int)fio_data.pid,
                         (void *)io,
                         io->fd);
#endif
        fio_atomic_or(&io->state, FIO_IO_THROTTLED);
      }
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
#if FIO_VALIDATE_IO_MUTEX
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_data - invalid IO: %p", io);
#endif
  if (io->state == FIO_IO_OPEN) {
    /* test for closed/suspended/throttled */
    if (fio_trylock(&io->lock))
      goto reschedule;
    io->protocol->on_data(io);
    fio_unlock(&io->lock);
    if (io->state == FIO_IO_OPEN) {
      /* this also tests for the suspended flag (0x02) */
      fio_monitor_read(io);
    }
  } else if ((io->state & FIO_IO_OPEN)) {
    fio_monitor_write(io);
    // FIO_LOG_DEBUG2("skipping on_data callback for IO %p (fd %d)", io_,
    // io->fd);
  }
  MARK_FUNC();
  fio_undup(io);
  return;

reschedule:
  FIO_LOG_DEBUG2("(%d) rescheduling on_data for IO %p (fd %d)",
                 (int)fio_data.pid,
                 io_,
                 io->fd);
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_ev_on_data,
                 .udata1 = io,
                 .udata2 = udata);
}

/* *****************************************************************************
On Timeout
***************************************************************************** */

static void fio_ev_on_timeout(void *io_, void *udata) {
  fio_s *const io = io_;
#if FIO_VALIDATE_IO_MUTEX
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_timeout - invalid IO: %p", io);
#endif
  if ((io->state & FIO_IO_OPEN)) {
    io->protocol->on_timeout(io);
  } else { /* TODO: FIXME: why does this occur? ... What to do? */
    FIO_LOG_DEBUG2("(%d) timeout event on a non-open IO %p (fd %d)",
                   (int)fio_data.pid,
                   io_,
                   io->fd);
  }
  fio_undup(io);
  MARK_FUNC();
  return;
  (void)udata;
}

/* *****************************************************************************
On Shutdown
***************************************************************************** */

static void fio_ev_on_shutdown(void *io_, void *udata) {
  fio_s *const io = io_;
#if FIO_VALIDATE_IO_MUTEX
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_shutdown - invalid IO: %p", io);
#endif
  if (fio_trylock(&io->lock))
    goto reschedule;
  io->protocol->on_shutdown(io);
  fio_unlock(&io->lock);
  fio_close(io);
  fio_undup(io);
  MARK_FUNC();
  return;

reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_ev_on_shutdown,
                 .udata1 = io,
                 .udata2 = udata);
}

/* *****************************************************************************
On Close
***************************************************************************** */

static void fio_ev_on_close(void *io, void *udata) {
  (void)udata;
  MARK_FUNC();
  FIO_ASSERT_DEBUG(fio_is_valid(io), "on_close - invalid IO: %p", io);
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
  int t = fio_timer_next_at(&fio_data.timers);
  t = fio_ct_if_bool(t == -1, FIO_POLL_TICK, t - fio_data.tick);
  t = fio_ct_if_bool(t > FIO_POLL_TICK, FIO_POLL_TICK, t);
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
  MARK_FUNC();

  fio_queue_push(FIO_QUEUE_IO(io), .fn = u.fn, .udata1 = fio_dup(io));
  fio_user_thread_wake();
  return;
io_invalid:
  FIO_LOG_DEBUG2("(%d) IO validation failed for IO %p (User task)",
                 (int)fio_data.pid,
                 (void *)io);
}

FIO_IFUNC void fio___poll_ev_wrap__io_task(void *fn_, void *io_) {
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.p = fn_};
  fio_s *io = io_;
  if (!fio_is_valid(io))
    goto io_invalid;
  MARK_FUNC();

  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = u.fn, .udata1 = fio_dup(io));
  return;
io_invalid:
  FIO_LOG_DEBUG2("(%d) IO validation failed for IO %p (IO task)",
                 (int)fio_data.pid,
                 (void *)io);
}

/* delays callback scheduling so it occurs after pending free / init events. */
FIO_IFUNC void fio___poll_ev_wrap__schedule(void (*wrapper)(void *, void *),
                                            void (*fn)(void *, void *),
                                            void *io) {
  /* delay task processing to after all IO object references increased */
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.fn = fn};
  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = wrapper, .udata1 = u.p, .udata2 = io);
}

/* *****************************************************************************
Polling Callbacks
***************************************************************************** */

FIO_IFUNC void fio___poll_on_data(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG2("(%d) event on_data detected for IO %p", (int)fio_data.pid,
  // io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__user_task,
                               fio_ev_on_data,
                               io);
  MARK_FUNC();
}
FIO_IFUNC void fio___poll_on_ready(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG2("(%d) event on_ready detected for IO %p", (int)fio_data.pid,
  // io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_ready,
                               io);
  MARK_FUNC();
}

FIO_IFUNC void fio___poll_on_close(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG2("(%d) event on_close detected for IO %p", (int)fio_data.pid,
  // io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_close,
                               io);
  MARK_FUNC();
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
  if ((flags & POLLIN))
    fio___epoll_add2(fd,
                     udata,
                     (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                     evio_fd[1]);
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
    ++i;
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
  static int old = 1;
  /* schedule IO events */
  fio_io_wakeup_prep();
  /* make sure all system events were processed */
  fio_queue_perform_all(FIO_QUEUE_SYSTEM);
  int c = fio_monitor_review(fio_poll_timeout_calc());
  fio_data.tick = fio_time2milli(fio_time_real());
  /* schedule Signal events */
  c += fio_signal_review();
  /* review IO timeouts */
  if (fio___review_timeouts() || c)
    fio_user_thread_wake();
  /* schedule timer events */
  if (fio_timer_push2queue(FIO_QUEUE_USER, &fio_data.timers, fio_data.tick))
    fio_user_thread_wake();
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
                    (int)fio_data.pid,
                    (int)fio_data.master,
                    (int)getppid());
    }
#endif
  }
  old = c;
}

/* *****************************************************************************
Shutdown cycle
***************************************************************************** */

/* cycles until all existing IO objects were closed. */
static void fio___shutdown_cycle(void) {
  const int64_t limit = fio_data.tick + (FIO_SHOTDOWN_TIMEOUT * 1000);
  for (;;) {
    if (!fio_queue_perform(FIO_QUEUE_SYSTEM))
      continue;
    fio_signal_review();
    if (!fio_queue_perform(FIO_QUEUE_USER))
      continue;
    if (fio___review_timeouts())
      continue;
    fio_data.tick = fio_time2milli(fio_time_real());
    if (fio_data.tick >= limit)
      break;
    if (fio_monitor_review(FIO_POLL_SHUTDOWN_TICK))
      continue;
    if (fio___is_waiting_on_io())
      continue;
    break;
  }
  fio___close_all_io();
  for (;;) {
    if (!fio_queue_perform(FIO_QUEUE_SYSTEM))
      continue;
    fio_signal_review();
    if (!fio_queue_perform(FIO_QUEUE_USER))
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
      fio_user_thread_suspend();
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
    threads = calloc(sizeof(*threads), fio_data.threads);
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
      fio_user_thread_wake_all();
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
    FIO_LOG_INFO("(%d) worker shutdown complete.", (int)fio_data.pid);
}

/* *****************************************************************************
Spawning Worker Processes
***************************************************************************** */

static void fio_spawn_worker(void *ignr_1, void *ignr_2);

static fio_lock_i fio_spawn_GIL = FIO_LOCK_INIT;

/** Worker sentinel */
static void *fio_worker_sentinel(void *thr_ptr) {
  pid_t pid = fio_fork();
  FIO_ASSERT(pid != (pid_t)-1, "system call `fork` failed.");
  if (pid) {
    int status = 0;
    (void)status;
    fio_state_callback_force(FIO_CALL_AFTER_FORK);
    fio_state_callback_force(FIO_CALL_IN_MASTER);
    fio_unlock(&fio_spawn_GIL);
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
      if (thr_ptr) {
        fio_thread_detach(thr_ptr);
        memset(thr_ptr, 0, sizeof(fio_thread_t));
      }
      fio_queue_push(FIO_QUEUE_SYSTEM, fio_spawn_worker, thr_ptr);
    }
    return NULL;
  }
  fio_data.pid = getpid();
  fio_data.is_master = 0;
  fio_data.is_worker = 1;
  fio_unlock(&fio_spawn_GIL);
  fio___after_fork();
  FIO_LOG_INFO("(%d) worker starting up.", (int)fio_data.pid);
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_state_callback_force(FIO_CALL_IN_CHILD);
  fio___worker();
  exit(0);
  return NULL;
}

static void fio_spawn_worker(void *thr_ptr, void *ignr_2) {
  fio_thread_t t;
  fio_thread_t *pt = thr_ptr;
  if (!pt)
    pt = &t;
  if (!fio_data.is_master)
    return;

  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  /* do not allow master tasks to run in worker */
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) &&
         !fio_queue_perform(FIO_QUEUE_USER))
    ;

  fio_lock(&fio_spawn_GIL);
  if (fio_thread_create(pt, fio_worker_sentinel, thr_ptr)) {
    fio_unlock(&fio_spawn_GIL);
    FIO_LOG_FATAL(
        "sentinel thread creation failed, no worker will be spawned.");
    fio_stop();
  }
  if (!thr_ptr)
    fio_thread_detach(t);
  fio_lock(&fio_spawn_GIL);
  fio_unlock(&fio_spawn_GIL);
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
  fio_state_callback_force(FIO_CALL_PRE_START);
  if (!fio_data.running)
    goto finish_up;
  if (fio_data.workers) {
    fio_data.is_worker = 0;
    fio_thread_t *sentinels = calloc(sizeof(*sentinels), fio_data.workers);
    for (size_t i = 0; i < fio_data.workers; ++i) {
      fio_spawn_worker((void *)(sentinels + i), NULL);
    }
    fio___worker_nothread_cycle();
    fio___start_shutdown();
    fio___shutdown_cycle();
    for (size_t i = 0; i < fio_data.workers; ++i) {
      fio_thread_join(sentinels[i]);
    }
  } else {
    fio___worker();
  }
finish_up:
  fio_state_callback_force(FIO_CALL_ON_FINISH);
  FIO_LOG_INFO("shutdown complete.");
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
  fio_undup(io);
  return;
reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
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
  fio_queue_push(FIO_QUEUE_IO(io), .fn = fio_io_task_wrapper, .udata1 = t);
}

/** Schedules a timer bound task, see `fio_timer_schedule` in the CSTL. */
void fio_run_every FIO_NOOP(fio_timer_schedule_args_s args) {
  args.start_at += fio_data.tick * (!args.start_at);
  fio_timer_schedule FIO_NOOP(&fio_data.timers, args);
}
/**
 * Schedules a timer bound task, see `fio_timer_schedule` in the CSTL.
 *
 * Possible "named arguments" (fio_timer_schedule_args_s members) include:
 *
 * * The timer function. If it returns a non-zero value, the timer stops:
 *        int (*fn)(void *, void *)
 * * Opaque user data:
 *        void *udata1
 * * Opaque user data:
 *        void *udata2
 * * Called when the timer is done (finished):
 *        void (*on_finish)(void *, void *)
 * * Timer interval, in milliseconds:
 *        uint32_t every
 * * The number of times the timer should be performed. -1 == infinity:
 *        int32_t repetitions
 */
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

/** a callback type */
static const char *fio_state_tasks_names[FIO_CALL_NEVER + 1] = {
    [FIO_CALL_ON_INITIALIZE] = "ON_INITIALIZE",
    [FIO_CALL_PRE_START] = "PRE_START",
    [FIO_CALL_BEFORE_FORK] = "BEFORE_FORK",
    [FIO_CALL_AFTER_FORK] = "AFTER_FORK",
    [FIO_CALL_IN_CHILD] = "IN_CHILD",
    [FIO_CALL_IN_MASTER] = "IN_MASTER",
    [FIO_CALL_ON_START] = "ON_START",
    [FIO_CALL_ON_PUBSUB_CONNECT] = "ON_PUBSUB_CONNECT",
    [FIO_CALL_ON_PUBSUB_ERROR] = "ON_PUBSUB_ERROR",
    [FIO_CALL_ON_USR] = "ON_USR",
    [FIO_CALL_ON_IDLE] = "ON_IDLE",
    [FIO_CALL_ON_USR_REVERSE] = "ON_USR_REVERSE",
    [FIO_CALL_ON_SHUTDOWN] = "ON_SHUTDOWN",
    [FIO_CALL_ON_FINISH] = "ON_FINISH",
    [FIO_CALL_ON_PARENT_CRUSH] = "ON_PARENT_CRUSH",
    [FIO_CALL_ON_CHILD_CRUSH] = "ON_CHILD_CRUSH",
    [FIO_CALL_AT_EXIT] = "AT_EXIT",
    [FIO_CALL_NEVER] = "NEVER",
};

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

static void fio_state_callback_force___task(void *fn_p, void *arg) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn_p};
  u.fn(arg);
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
  union {
    void *p;
    void (*fn)(void *);
  } u;

  FIO_LOG_DDEBUG2("(%d) Scheduling %s callbacks.",
                  (int)fio_data.pid,
                  fio_state_tasks_names[e]);

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
      u.fn = ary[i].func;
      fio_queue_push(FIO_QUEUE_USER,
                     fio_state_callback_force___task,
                     u.p,
                     ary[i].arg);
    }
  } else {
    /* perform tasks in reverse */
    while (len--) {
      u.fn = ary[len].func;
      fio_queue_push(FIO_QUEUE_USER,
                     fio_state_callback_force___task,
                     u.p,
                     ary[len].arg);
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

/* listening socket on_data callback */
FIO_SFUNC void fio_listen_on_data(fio_s *io) {
  struct fio_listen_args *l = fio_udata_get(io);
  int fd;
  while ((fd = accept(io->fd, NULL, NULL)) != -1) {
    FIO_LOG_DEBUG2("(%d) accepting a new connection (fd %d) at %s",
                   (int)fio_data.pid,
                   fd,
                   l->url);
    l->on_open(fd, l->udata);
  }
}

/* listening socket on_close callback */
FIO_SFUNC void fio_listen_on_close(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_INFO("(%d) stopped listening on %s", (int)fio_data.pid, l->url);
}

/* listening socket protocol */
static fio_protocol_s FIO_PROTOCOL_LISTEN = {
    .on_data = fio_listen_on_data,
    .on_close = fio_listen_on_close,
    .on_timeout = mock_ping_eternal,
};

/* attaching the listening socket to the worker (@ON_START) */
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
  FIO_LOG_DEBUG2("(%d) Called dup(%d) to attach %d as a listening socket.",
                 (int)fio_data.pid,
                 l->reserved,
                 fd);
  fio_attach_fd(fd, &FIO_PROTOCOL_LISTEN, l, NULL);
  FIO_LOG_INFO("(%d) started listening on %s",
               (fio_data.is_master ? (int)fio_data.master : (int)fio_data.pid),
               l->url);
}

/* freeing the listening socket and its resources */
FIO_SFUNC void fio_listen___free(void *udata) {
  struct fio_listen_args *l = udata;
  FIO_LOG_DEBUG2("(%d) closing listening socket at %s",
                 (int)fio_data.pid,
                 l->url);
  fio_sock_close(l->reserved); /* this socket was dupped and unused */
  fio_state_callback_remove(FIO_CALL_PRE_START, fio_listen___attach, l);
  fio_state_callback_remove(FIO_CALL_ON_START, fio_listen___attach, l);
  free(l);
}

/* listening socket setup */
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
Pub/Sub
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Letter / Message Object
***************************************************************************** */

/*
Letter network format in bytes:
| 1 special info byte | 3 little endian channel length (24 bit value) |
| 4 little endian Filter | 4 little endian message length (32 bit values) |
| 8 message UUID |
| X bytes channel name + 1 NUL terminator|
| X bytes message length + 1 NUL terminator|
*/

#define LETTER_HEADER_LENGTH 20 /* without NUL terminators */

enum {
  FIO_PUBSUB_PROCESS_BIT = 1,
  FIO_PUBSUB_ROOT_BIT = 2,
  FIO_PUBSUB_SIBLINGS_BIT = 4,
  FIO_PUBSUB_CLUSTER_BIT = 8,
  FIO_PUBSUB_PING_BIT = 64,
  FIO_PUBSUB_JSON_BIT = 128,
} letter_info_bits_e;

typedef struct {
  fio_s *from;
  int32_t filter;
  uint32_t channel_len;
  uint32_t message_len;
  char buf[];
} letter_s;

#define FIO_REF_NAME      letter
#define FIO_REF_FLEX_TYPE char
#include "fio-stl.h"

/* allocates a new letter. */
FIO_IFUNC letter_s *letter_new(fio_s *from,
                               int32_t filter,
                               uint32_t channel_len,
                               uint32_t message_len) {
  size_t len = (LETTER_HEADER_LENGTH + 2) + message_len + channel_len;
  letter_s *l = letter_new2(len);
  FIO_ASSERT_ALLOC(l);
  l->from = from;
  l->filter = filter;
  l->channel_len = channel_len;
  l->message_len = message_len;
  return l;
}

/* allocates a new letter, and writes data to header */
FIO_IFUNC letter_s *letter_author(fio_s *from,
                                  uint64_t message_id,
                                  int32_t filter,
                                  char *channel,
                                  uint32_t channel_len,
                                  char *message,
                                  uint32_t message_len,
                                  uint8_t flags) {
  if ((channel_len >> 24) || (message_len >> 27))
    goto len_error;
  letter_s *l = letter_new(from, filter, channel_len, message_len);
  FIO_ASSERT_ALLOC(l);
  l->buf[0] = flags;
  fio_u2buf32_little(l->buf + 1, channel_len);
  fio_u2buf32_little(l->buf + 4, message_len); /* overwrite extra byte */
  fio_u2buf32_little(l->buf + 8, filter);
  fio_u2buf64_little(l->buf + 12, message_id);
  if (channel_len && channel) {
    memcpy(l->buf + LETTER_HEADER_LENGTH, channel, channel_len);
  }
  l->buf[LETTER_HEADER_LENGTH + channel_len] = 0;
  if (message_len && message) {
    memcpy(l->buf + LETTER_HEADER_LENGTH + 1 + channel_len,
           message,
           message_len);
  }
  l->buf[(LETTER_HEADER_LENGTH + 1) + channel_len + message_len] = 0;
  return l;

len_error:
  FIO_LOG_ERROR("(pubsub) payload too big (channel length of %u bytes, message "
                "length of %u bytes)",
                (unsigned int)channel_len,
                (unsigned int)message_len);
  return NULL;
}

/* frees a letter's reference. */
#define letter_free letter_free2

/* returns 1 if a letter is bound to a filter, otherwise 0. */
FIO_IFUNC int32_t letter_is_filter(letter_s *l) { return !!l->filter; }

/* returns a letter's ID (may be 0 for internal letters) */
FIO_IFUNC uint64_t letter_id(letter_s *l) {
  return fio_buf2u64_little(l->buf + 12);
}

/* returns a letter's channel (if none, returns the filter's address) */
FIO_IFUNC char *letter_channel(letter_s *l) {
  return l->buf + LETTER_HEADER_LENGTH;
}

/* returns a letter's message length (if any) */
FIO_IFUNC size_t letter_message_len(letter_s *l) { return l->message_len; }

/* returns a letter's channel length (if any) */
FIO_IFUNC size_t letter_channel_len(letter_s *l) { return l->channel_len; }

/* returns a letter's filter (if any) */
FIO_IFUNC int32_t letter_filter(letter_s *l) { return l->filter; }

/* returns a letter's message */
FIO_IFUNC char *letter_message(letter_s *l) {
  return l->buf + LETTER_HEADER_LENGTH + 1 + l->channel_len;
}

/* returns a letter's length */
FIO_IFUNC size_t letter_len(letter_s *l) {
  return LETTER_HEADER_LENGTH + 2 + l->channel_len + l->message_len;
}

/* write a letter to an IO object */
FIO_IFUNC void letter_write(fio_s *io, letter_s *l) {
  if (io == l->from)
    return;
  fio_write2(io,
             .buf = (char *)letter_dup2(l),
             .offset = (uintptr_t)(((letter_s *)0)->buf),
             .len = letter_len(l),
             .dealloc = (void (*)(void *))letter_free2);
}

/* *****************************************************************************
Letter Reading, Parsing and Sending
***************************************************************************** */

/* a letter parser object */
typedef struct {
  letter_s *letter;
  size_t pos;
  char buf[LETTER_HEADER_LENGTH + 2]; /* minimal message length */
} letter_parser_s;

/* a new letter parser */
FIO_IFUNC letter_parser_s *letter_parser_new(void) {
  letter_parser_s *p = fio_malloc(sizeof(*p));
  FIO_ASSERT_ALLOC(p);
  p->letter = NULL;
  p->pos = 0;
  return p;
}

/* free a letter parser */
FIO_IFUNC void letter_parser_free(letter_parser_s *parser) {
  letter_free(parser->letter);
  fio_free(parser);
}

/* pings are ignored (no pong required) */
FIO_SFUNC void letter_read_ping_callback(letter_s *letter) { (void)letter; }

/* forwards letters to callback, returns 0. Returns -1 on error. */
FIO_IFUNC int letter_read(fio_s *io,
                          letter_parser_s *parser,
                          void (*callback)(letter_s *)) {
  void (*callbacks[2])(letter_s *) = {callback, letter_read_ping_callback};
  for (;;) {
    ssize_t r;
    if (parser->letter) {
      letter_s *const letter = parser->letter;
      const size_t to_read = letter_len(parser->letter);
      while (parser->pos < to_read) {
        r = fio_read(io, letter->buf + parser->pos, to_read - parser->pos);
        if (r <= 0)
          return 0;
        parser->pos += r;
      }
      callbacks[((letter->buf[0] & FIO_PUBSUB_PING_BIT) / FIO_PUBSUB_PING_BIT)](
          letter);
      letter_free(letter);
      parser->letter = NULL;
      parser->pos = 0;
    }
    r = fio_read(io,
                 parser->buf + parser->pos,
                 (LETTER_HEADER_LENGTH + 2) - parser->pos);
    if (r <= 0)
      return 0;
    parser->pos += r;
    if (parser->pos < LETTER_HEADER_LENGTH)
      return 0;
    uint32_t channel_len = fio_buf2u32_little(parser->buf + 1);
    channel_len &= 0xFFFFFF;
    uint32_t message_len = fio_buf2u32_little(parser->buf + 4);
    int32_t filter = fio_buf2u32_little(parser->buf + 8);
    parser->letter = letter_new(io, filter, channel_len, message_len);
    if (!parser->letter) {
      return -1;
    }
    memcpy(parser->letter->buf, parser->buf, parser->pos);
  }
}

/* *****************************************************************************
Pub/Sub - defaults and builtin pub/sub engines
***************************************************************************** */

/** Used to publish the message exclusively to the root / master process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_ROOT =
    (fio_pubsub_engine_s *)FIO_PUBSUB_ROOT_BIT;
/** Used to publish the message only within the current process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_PROCESS =
    (fio_pubsub_engine_s *)FIO_PUBSUB_PROCESS_BIT;
/** Used to publish the message except within the current process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_SIBLINGS =
    (fio_pubsub_engine_s *)FIO_PUBSUB_SIBLINGS_BIT;
/** Used to publish the message for this process, its siblings and root. */
const fio_pubsub_engine_s *const FIO_PUBSUB_LOCAL =
    (fio_pubsub_engine_s *)(FIO_PUBSUB_SIBLINGS_BIT | FIO_PUBSUB_PROCESS_BIT |
                            FIO_PUBSUB_ROOT_BIT);
/** Used to publish the message to any possible publishers. */
const fio_pubsub_engine_s *const FIO_PUBSUB_CLUSTER =
    (fio_pubsub_engine_s *)(FIO_PUBSUB_CLUSTER_BIT | FIO_PUBSUB_SIBLINGS_BIT |
                            FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT);

/** The default engine (settable). Initial default is FIO_PUBSUB_LOCAL. */
const fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;

/**
 * The pattern matching callback used for pattern matching.
 *
 * Returns 1 on a match or 0 if the string does not match the pattern.
 *
 * By default, the value is set to `fio_glob_match` (see facil.io's C STL).
 */
uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s,
                                    fio_str_info_s) = fio_glob_match;

/* *****************************************************************************
Channel / Subscription Objects
***************************************************************************** */

/* channels can be either named (filters + exact strings) or pattern based. */
typedef enum {
  CHANNEL_TYPE_NAMED,
  CHANNEL_TYPE_PATTERN,
  CHANNEL_TYPE_NONE,
} channel_type_e;

/* The channel object */
typedef struct {
  FIO_LIST_HEAD subscriptions;
  channel_type_e type;
  int32_t filter;
  size_t name_len;
  char *name;
} channel_s;

/* The subscription object */
typedef struct {
  FIO_LIST_NODE node;
  fio_s *io;
  channel_s *channel;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata);
  void *udata;
  uint32_t ref;
  fio_lock_i lock;
  uint8_t disabled; /* TODO: do we need this one? */
} subscription_s;

/* *****************************************************************************
Subscription Object API
***************************************************************************** */

/* a mock callback for subscriptions */
FIO_SFUNC void subscription_mock_callback(fio_msg_s *msg) { (void)msg; }

/* creates a new subscription */
FIO_IFUNC subscription_s *subscription_new(fio_s *io,
                                           channel_s *channel,
                                           void (*on_message)(fio_msg_s *),
                                           void (*on_unsubscribe)(void *),
                                           void *udata) {
  subscription_s *s = (io ? fio_malloc : malloc)(sizeof(*s));
  FIO_ASSERT_ALLOC(s);
  *s = (subscription_s){
      .node = FIO_LIST_INIT(s->node),
      .io = io,
      .channel = channel,
      .on_message = (on_message ? on_message : subscription_mock_callback),
      .on_unsubscribe = on_unsubscribe,
      .udata = udata,
      .ref = 1,
  };
  return s;
}

/* we count subscription reference counts to make sure the udata is valid */
FIO_IFUNC subscription_s *subscription_dup(subscription_s *s) {
  FIO_ASSERT(fio_atomic_add_fetch(&s->ref, 1),
             "subscription reference count overflow detected!");
  return s;
}

/* calls the on_unsubscribe callback. */
FIO_SFUNC void subscription_on_unsubscribe___task(void *fnp, void *udata) {
  union {
    void *p;
    void (*fn)(void *udata);
  } u = {.p = fnp};
  u.fn(udata);
}

/* free the udata (and subscription) only after all callbacks return */
FIO_IFUNC void subscription_free(subscription_s *s) {
  if (fio_atomic_sub_fetch(&s->ref, 1))
    return;
  union {
    void *p;
    void (*fn)(void *udata);
  } u = {.fn = s->on_unsubscribe};
  if (u.p) {
    fio_queue_push(FIO_QUEUE_USER,
                   subscription_on_unsubscribe___task,
                   u.p,
                   s->udata);
  }
  (s->io ? fio_free : free)(s);
}

/* *****************************************************************************
Message Delivery (to subscription)
***************************************************************************** */

/* an internal (temporary) message object. */
typedef struct {
  volatile size_t flag;
  letter_s *letter;
  fio_msg_s msg;
} fio_msg_internal_s;

/* returns the letter object associated with the message. */
FIO_IFUNC letter_s *fio_msg2letter(fio_msg_s *msg) {
  fio_msg_internal_s *mi = FIO_PTR_FROM_FIELD(fio_msg_internal_s, msg, msg);
  return mi->letter;
}

/** Defers the current callback, so it will be called again for the message. */
void fio_message_defer(fio_msg_s *msg) {
  fio_msg_internal_s *mi = FIO_PTR_FROM_FIELD(fio_msg_internal_s, msg, msg);
  mi->flag = 1;
}

/** calls the callback within a lock, using a fio_msg_s pointer. */
FIO_SFUNC void subscription_deliver__task(void *s_, void *l_) {
  subscription_s *s = (subscription_s *)s_;
  letter_s *l = (letter_s *)l_;
  fio_msg_internal_s mi = {
      .flag = 0,
      .letter = l,
      .msg =
          {
              .io = s->io,
              .channel =
                  {
                      .buf = letter_channel(l),
                      .len = letter_channel_len(l),
                  },
              .message =
                  {
                      .buf = letter_message(l),
                      .len = letter_message_len(l),
                  },
              .udata = s->udata,
              .filter = letter_filter(l),
              .is_json =
                  ((l->buf[0] & FIO_PUBSUB_JSON_BIT) / FIO_PUBSUB_JSON_BIT),
          },
  };
  fio_lock_i *lock = (s->io ? &s->io->lock : &s->lock);
  if (fio_trylock(lock))
    goto reschedule;
  s->on_message(&mi.msg);
  fio_unlock(lock);
  if (mi.flag)
    goto reschedule;

  fio_undup(s->io);
  subscription_free(s);
  letter_free(l);
  return;
reschedule:
  fio_queue_push((s->io ? FIO_QUEUE_IO(s->io) : FIO_QUEUE_USER),
                 subscription_deliver__task,
                 s,
                 l);
}

/* schedules a letter delivery to a subscription */
FIO_IFUNC void subscription_deliver(subscription_s *s, letter_s *l) {
  if (s->disabled)
    return;
  fio_queue_s *q = FIO_QUEUE_USER;
  if (s->io) {
    if (s->io == l->from || !fio_is_valid(s->io))
      return;
    q = FIO_QUEUE_IO(s->io);
    fio_dup(s->io);
  }
  fio_queue_push(q,
                 subscription_deliver__task,
                 subscription_dup(s),
                 letter_dup2(l));
}

/* *****************************************************************************
Channel Object API
***************************************************************************** */

/* creates a new channel object. */
FIO_IFUNC channel_s *channel_new(channel_type_e channel_type,
                                 int32_t filter,
                                 char *name,
                                 size_t name_len) {
  channel_s *c = malloc(sizeof(*c) + name_len);
  FIO_ASSERT_ALLOC(c);
  *c = (channel_s){
      .subscriptions = FIO_LIST_INIT(c->subscriptions),
      .type = channel_type,
      .filter = filter,
      .name_len = name_len,
      .name = (char *)(c + 1),
  };
  if (name_len)
    memcpy(c->name, name, name_len);
  return c;
}

/* frees a channel, making sure all subscriptions are destroyed. */
FIO_IFUNC void channel_free(channel_s *channel) {
  if (!channel)
    return;
  /* make sure no subscriptions are leaked during termination */
  FIO_LIST_EACH(subscription_s, node, &channel->subscriptions, s) {
    s->channel = NULL;
    subscription_free(s);
  }
  free(channel);
}

/* tests to see if two channel objects are the same. */
FIO_IFUNC _Bool channel_is_eq(channel_s *a, channel_s *b) {
  return a->filter == b->filter && a->name_len == b->name_len &&
         !memcmp(a->name, b->name, a->name_len);
}

/* hashing helper (hashes a channel name using it's filter as "salt". */
FIO_IFUNC uint64_t channel2hash(channel_s ch) {
  return fio_risky_hash(ch.name, ch.name_len, ch.filter);
}

/* *****************************************************************************
Postoffice
***************************************************************************** */

FIO_SFUNC void postoffice_on_channel_added(channel_s *);
FIO_SFUNC void postoffice_on_channel_removed(channel_s *);

#define FIO_MAP_NAME           channel_store
#define FIO_MAP_TYPE           channel_s *
#define FIO_MAP_TYPE_CMP(a, b) channel_is_eq(a, b)
#define FIO_MAP_TYPE_COPY(dest, src)                                           \
  do {                                                                         \
    ((dest) = (src));                                                          \
    postoffice_on_channel_added((src));                                        \
  } while (0)
#define FIO_MAP_TYPE_DESTROY(ch)                                               \
  do {                                                                         \
    postoffice_on_channel_removed((ch));                                       \
    channel_free((ch));                                                        \
  } while (0)
#define FIO_MAP_TYPE_DISCARD(ch)                                               \
  do {                                                                         \
    channel_free((ch));                                                        \
  } while (0)
#include "fio-stl.h"

/** The postoffice data store */
static struct {
  uint8_t filter_local;
  uint8_t filter_ipc;
  uint8_t filter_cluster;
  channel_store_s channels[CHANNEL_TYPE_NONE];
  fio_pubsub_engine_s *engines[FIO_PUBSUB_ENGINE_LIMIT];
  char ipc_url[40];                 /* inter-process address  - buffer */
  char cluster_url[40];             /* machine cluster address - buffer */
  fio_protocol_s ipc;               /* inter-process communication protocol */
  fio_protocol_s ipc_listen;        /* accepts inter-process connections */
  fio_protocol_s cluster;           /* machine cluster communication protocol */
  fio_protocol_s cluster_listen;    /* accepts machine cluster connections */
  fio_protocol_s cluster_discovery; /* network cluster discovery */
  fio_tls_s *cluster_server_tls;
  fio_tls_s *cluster_client_tls;
  uintptr_t subscription_handles[8];
  fio_s mock_io;
} postoffice = {
    .filter_local = (FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT |
                     FIO_PUBSUB_SIBLINGS_BIT),
    .filter_ipc = (~(uint8_t)FIO_PUBSUB_PROCESS_BIT),
    .filter_cluster = FIO_PUBSUB_CLUSTER_BIT,
    .channels =
        {
            FIO_MAP_INIT,
            FIO_MAP_INIT,
        },
};

/* subscribe using a subscription object and a channel */
FIO_IFUNC void postoffice_subscribe(subscription_s *s) {
  if (!s)
    return;
  if (s->disabled || !s->channel || (s->io && !fio_is_valid(s->io)))
    goto error;
  const uint64_t hash = channel2hash(*s->channel);
  s->channel =
      channel_store_set_if_missing(&postoffice.channels[s->channel->type],
                                   hash,
                                   s->channel);
  FIO_LIST_PUSH(&s->channel->subscriptions, &s->node);
  return;

error:
  channel_free(s->channel);
  subscription_free(s);
}

/* unsubscribe using a subscription object */
FIO_IFUNC void postoffice_unsubscribe(subscription_s *s) {
  if (!s)
    return;
  channel_s *ch = s->channel;
  s->disabled = 1;
  FIO_LIST_REMOVE(&s->node);
  subscription_free(s);
  if (!ch || !FIO_LIST_IS_EMPTY(&ch->subscriptions))
    return;
  const uint64_t hash = channel2hash(ch[0]);
  channel_store_remove(&postoffice.channels[ch->type], hash, ch, NULL);
}

/* deliver a letter to all subscriptions in the relevant channels */
FIO_IFUNC void postoffice_deliver2process(letter_s *l) {
  channel_s ch_key = {
      .filter = letter_filter(l),
      .name_len = letter_channel_len(l),
      .name = letter_channel(l),
  };
  const uint64_t hash = channel2hash(ch_key);
  channel_s *ch = channel_store_get(postoffice.channels, hash, &ch_key);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, s) {
      subscription_deliver(s, l);
    }
  }
  if (!channel_store_count(&postoffice.channels[CHANNEL_TYPE_PATTERN]))
    return;

  fio_str_info_s name = {
      .buf = letter_channel(l),
      .len = letter_channel_len(l),
  };
  FIO_MAP_EACH(channel_store, &postoffice.channels[CHANNEL_TYPE_PATTERN], pos) {
    if (pos->obj->filter != letter_filter(l))
      continue;
    fio_str_info_s pat = {
        .buf = pos->obj->name,
        .len = pos->obj->name_len,
    };
    if (FIO_PUBSUB_PATTERN_MATCH(pat, name)) {
      FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, s) {
        subscription_deliver(s, l);
      }
    }
  }
}

FIO_SFUNC void postoffice_deliver2io___task(void *io_, void *l_) {
  letter_write(io_, l_);
  letter_free(l_);
  fio_free2(io_);
}

FIO_IFUNC void postoffice_deliver2ipc(letter_s *l) {
  FIO_LIST_EACH(fio_s, timeouts, &postoffice.ipc.reserved.ios, io) {
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   postoffice_deliver2io___task,
                   fio_dup2(io),
                   letter_dup2(l));
  }
}

FIO_IFUNC void postoffice_deliver2cluster(letter_s *l) {
  FIO_LIST_EACH(fio_s, timeouts, &postoffice.cluster.reserved.ios, io) {
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   postoffice_deliver2io___task,
                   fio_dup2(io),
                   letter_dup2(l));
  }
}

/* *****************************************************************************
Pub/Sub - Subscribe / Unsubscribe
***************************************************************************** */

/* perform subscription in system thread */
FIO_SFUNC void fio_subscribe___task(void *ch_, void *s_) {
  channel_s *ch = ch_;
  subscription_s *s = s_;
  s->channel = ch;
  postoffice_subscribe(s);
}

void fio_subscribe___(void); /* sublimetext marker */
/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * The on_unsubscribe callback will be called on failure.
 */
void fio_subscribe FIO_NOOP(subscribe_args_s args) {
  channel_s *ch = channel_new(
      fio_ct_if(args.is_pattern, CHANNEL_TYPE_PATTERN, CHANNEL_TYPE_NAMED),
      args.filter,
      args.channel.buf,
      args.channel.len);
  subscription_s *s = subscription_new(args.io,
                                       NULL,
                                       args.on_message,
                                       args.on_unsubscribe,
                                       args.udata);
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_subscribe___task, ch, s);
  if (!args.subscription_handle_ptr) {
    fio_env_set(args.io,
                .type = (-1LL - args.is_pattern - (!!args.filter)),
                .name = args.channel,
                .on_close = ((void (*)(void *))postoffice_unsubscribe),
                .udata = s);
  } else {
    *args.subscription_handle_ptr = (uintptr_t)s;
  }
}

void fio_unsubscribe___(void); /* sublimetext marker */

FIO_SFUNC void fio_unsubscribe___task(void *s, void *ignr_) {
  (void)ignr_;
  postoffice_unsubscribe(s);
}
/**
 * Cancels an existing subscriptions.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 *
 * Returns -1 if the subscription could not be found. Otherwise returns 0.
 */
int fio_unsubscribe FIO_NOOP(subscribe_args_s args) {
  if (args.filter) {
    args.is_pattern = 0;
    args.channel.buf = (char *)&args.filter;
    args.channel.len = sizeof(args.filter);
  }
  if (!args.subscription_handle_ptr) {
    return fio_env_remove(args.io,
                          .type = (-1LL - args.is_pattern - (!!args.filter)),
                          .name = args.channel);
  }
  ((subscription_s **)args.subscription_handle_ptr)[0]->disabled = 1;
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 fio_unsubscribe___task,
                 ((void **)args.subscription_handle_ptr)[0]);
  return 0;
}

/* *****************************************************************************
Pub/Sub - Publish
***************************************************************************** */
FIO_SFUNC void fio_publish___task(void *letter_, void *ignr_) {
  letter_s *l = letter_;
  if ((l->buf[0] & postoffice.filter_local))
    postoffice_deliver2process(l);
  if ((l->buf[0] & postoffice.filter_ipc))
    postoffice_deliver2ipc(l);
  if ((l->buf[0] & postoffice.filter_cluster))
    postoffice_deliver2cluster(l);
  letter_free(l);
  (void)ignr_;
}

/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
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
void fio_publish___(void); /* SublimeText marker*/
void fio_publish FIO_NOOP(fio_publish_args_s args) {
  const fio_pubsub_engine_s *engines[3] = {
      FIO_PUBSUB_DEFAULT,
      FIO_PUBSUB_LOCAL,
  };
  if (!args.engine)
    args.engine = engines[(!!args.filter)];
  if ((uintptr_t)(args.engine) > 0XFF)
    goto external_engine;
  letter_s *l =
      letter_author(args.from,
                    fio_rand64(),
                    args.filter,
                    args.channel.buf,
                    args.channel.len,
                    args.message.buf,
                    args.message.len,
                    (uint8_t)(uintptr_t)args.engine |
                        ((0x100 - args.is_json) & FIO_PUBSUB_JSON_BIT));
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_publish___task, l);
  return;
external_engine:
  args.engine->publish(args.engine, args.channel, args.message, args.is_json);
}

/* *****************************************************************************
 * Message metadata (advance usage API)
 **************************************************************************** */

/**
 * The number of different metadata callbacks that can be attached.
 *
 * Effects performance.
 *
 * The default value should be enough for the following metadata objects:
 * - WebSocket server headers.
 * - WebSocket client (header + masked message copy).
 * - EventSource (SSE) encoded named channel and message.
 */
#ifndef FIO_PUBSUB_METADATA_LIMIT
#define FIO_PUBSUB_METADATA_LIMIT 4
#endif

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id);

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
                             void (*cleanup)(void *));

/**
 * Removed the metadata callback.
 *
 * Removal might be delayed if live metatdata exists.
 */
void fio_message_metadata_remove(int id);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

#if 0
#define FIO_PUBSUB_ENGINE_LIMIT 16
struct fio_pubsub_engine_s {
  /** Called after the engine was detached, may be used for cleanup. */
  void (*detached)(const fio_pubsub_engine_s *eng);
  /** Subscribes to a channel. Called ONLY in the Root (master) process. */
  void (*subscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel);
  /** Unsubscribes to a channel. Called ONLY in the Root (master) process. */
  void (*unsubscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel);
  /** Subscribes to a pattern. Called ONLY in the Root (master) process. */
  void (*psubscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel);
  /** Unsubscribe to a pattern. Called ONLY in the Root (master) process. */
  void (*punsubscribe)(const fio_pubsub_engine_s *eng, fio_str_info_s channel);
  /** Publishes a message through the engine. Might be called in any worker. */
  void (*publish)(const fio_pubsub_engine_s *eng,
                  fio_str_info_s channel,
                  fio_str_info_s msg,
                  uint8_t is_json);
};
#endif

static void fio_pubsub_mock_detached(const fio_pubsub_engine_s *eng) {
  (void)eng;
}
static void fio_pubsub_mock_subscribe(const fio_pubsub_engine_s *eng,
                                      fio_str_info_s channel) {
  (void)eng;
  (void)channel;
}
static void fio_pubsub_mock_unsubscribe(const fio_pubsub_engine_s *eng,
                                        fio_str_info_s channel) {
  (void)eng;
  (void)channel;
}
static void fio_pubsub_mock_psubscribe(const fio_pubsub_engine_s *eng,
                                       fio_str_info_s channel) {
  (void)eng;
  (void)channel;
}
static void fio_pubsub_mock_punsubscribe(const fio_pubsub_engine_s *eng,
                                         fio_str_info_s channel) {
  (void)eng;
  (void)channel;
}
static void fio_pubsub_mock_publish(const fio_pubsub_engine_s *eng,
                                    fio_str_info_s channel,
                                    fio_str_info_s msg,
                                    uint8_t is_json) {
  (void)eng;
  (void)channel;
  (void)msg;
  (void)is_json;
}

static void fio_pubsub_attach___task(void *engine, void *ignr_) {
  (void)ignr_;
  fio_pubsub_engine_s *e = (fio_pubsub_engine_s *)engine;
  if (!e->detached)
    e->detached = fio_pubsub_mock_detached;
  if (!e->subscribe)
    e->subscribe = fio_pubsub_mock_subscribe;
  if (!e->unsubscribe)
    e->unsubscribe = fio_pubsub_mock_unsubscribe;
  if (!e->psubscribe)
    e->psubscribe = fio_pubsub_mock_psubscribe;
  if (!e->punsubscribe)
    e->punsubscribe = fio_pubsub_mock_punsubscribe;
  if (!e->publish)
    e->publish = fio_pubsub_mock_publish;

  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT; ++i) {
    if (postoffice.engines[i])
      continue;
    postoffice.engines[i] = engine;
    return;
  }
  e->detached(e);
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
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_pubsub_attach___task, engine);
}

static void fio_pubsub_detach___task(void *engine, void *ignr_) {
  (void)ignr_;
  fio_pubsub_engine_s *e = (fio_pubsub_engine_s *)engine;
  int i = 0;

  do {
    if (postoffice.engines[i] == engine)
      break;
  } while ((++i) < FIO_PUBSUB_ENGINE_LIMIT);

  while (i < (FIO_PUBSUB_ENGINE_LIMIT - 1) && postoffice.engines[i + 1]) {
    postoffice.engines[i] = postoffice.engines[i + 1];
    ++i;
  }
  postoffice.engines[i] = NULL;
  e->detached(e);
}
/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine) {
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_pubsub_detach___task, engine);
}

static void fio_pubsub_resubscribe_all___task(void *engine, void *ignr_) {
  (void)ignr_;
  fio_pubsub_engine_s *e = (fio_pubsub_engine_s *)engine;
  FIO_MAP_EACH(channel_store, &postoffice.channels[CHANNEL_TYPE_NAMED], pos) {
    if (pos->obj->filter)
      continue;
    fio_str_info_s ch = {pos->obj->name, pos->obj->name_len};
    e->subscribe(e, ch);
  }
  FIO_MAP_EACH(channel_store, &postoffice.channels[CHANNEL_TYPE_PATTERN], pos) {
    if (pos->obj->filter)
      continue;
    fio_str_info_s ch = {pos->obj->name, pos->obj->name_len};
    e->psubscribe(e, ch);
  }
}

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe to all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way.
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_resubscribe_all(fio_pubsub_engine_s *engine) {
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_pubsub_resubscribe_all___task, engine);
}

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine) {
  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT; ++i) {
    if (postoffice.engines[i] == engine)
      return 1;
  }
  return 0;
}

/* *****************************************************************************
Pubsub IPC and cluster protocol callbacks
***************************************************************************** */

FIO_SFUNC void pubsub_cluster_on_letter(letter_s *l) {
  /* TODO: test for and discard duplicates */
  postoffice_deliver2process(l);
  postoffice_deliver2ipc(l);
  postoffice_deliver2cluster(l);
}

FIO_SFUNC void pubsub_ipc_on_letter_master(letter_s *l) {
  if ((l->buf[0] & FIO_PUBSUB_ROOT_BIT))
    postoffice_deliver2process(l);
  if ((l->buf[0] & FIO_PUBSUB_SIBLINGS_BIT))
    postoffice_deliver2ipc(l);
  if ((l->buf[0] & FIO_PUBSUB_CLUSTER_BIT))
    postoffice_deliver2cluster(l);
}

FIO_SFUNC void pubsub_ipc_on_letter_worker(letter_s *l) {
  postoffice_deliver2process(l);
}

FIO_SFUNC void pubsub_ipc_on_data_master(fio_s *io) {
  letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_master);
}

FIO_SFUNC void pubsub_ipc_on_data_worker(fio_s *io) {
  letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_worker);
}

FIO_SFUNC void pubsub_cluster_on_data(fio_s *io) {
  if (letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_master))
    fio_close(io);
}

FIO_SFUNC void pubsub_ipc_on_close(void *udata) { letter_parser_free(udata); }

FIO_SFUNC void pubsub_ipc_on_close_in_child(void *udata) {
  letter_parser_free(udata);
  if (fio_data.running) {
    FIO_LOG_ERROR(
        "(%d) pub/sub connection to master process lost while running.",
        (int)fio_data.pid);
    fio_stop();
  }
}

FIO_SFUNC void pubsub_connection_ping(fio_s *io) {
  letter_s *l =
      letter_author(NULL, 0, -1, NULL, 0, NULL, 0, FIO_PUBSUB_PING_BIT);
  letter_write(io, l);
  letter_free(l);
}

FIO_SFUNC void pubsub_cluster_on_close(void *udata) {
  /* TODO: allow re-connections from this address */
  letter_parser_free(udata);
}

FIO_SFUNC void pubsub_listener_ipc_on_close(void *udata) {
  (void)udata;
  if (fio_data.is_master)
    FIO_LOG_DEBUG2("(%d) PostOffice stopped listening for IPC @ %s",
                   fio_data.pid,
                   postoffice.ipc_url);
}

FIO_SFUNC void pubsub_listener_cluster_on_close(void *udata) {
  (void)udata;
  if (fio_data.is_master)
    FIO_LOG_DEBUG2("(%d) PostOffice stopped listening for remote machines @ %s",
                   fio_data.pid,
                   postoffice.cluster_url);
}

FIO_SFUNC void pubsub_ipc_on_new_connection(fio_s *io) {
  if (!fio_data.is_master)
    return;
  int fd = accept(io->fd, NULL, NULL);
  if (fd == -1)
    return;
  io = fio_attach_fd(fd, fio_udata_get(io), letter_parser_new(), NULL);
  FIO_LOG_DDEBUG2("(%d) PostOffice accepted connection @ %p (%d).",
                  (int)fio_data.pid,
                  io,
                  io->fd);
}

/* *****************************************************************************
Postoffice add / remove hooks
***************************************************************************** */

/* callback called when a channel is added to a channel_store map */
FIO_SFUNC void postoffice_on_channel_added(channel_s *ch) {
  FIO_LOG_DDEBUG2("(%d) PostOffice opened channel (filter %ld): %.*s",
                  fio_data.pid,
                  (long)ch->filter,
                  (int)ch->name_len,
                  ch->name);
  if (ch->filter)
    return;
  fio_str_info_s cmds[] = {
      {"subscribe", 9},
      {"psubscribe", 10},
  };
  fio_publish(.engine = FIO_PUBSUB_ROOT,
              .filter = -1,
              .channel = cmds[ch->type == CHANNEL_TYPE_PATTERN],
              .message = {ch->name, ch->name_len});
  FIO_LOG_DDEBUG2("(%d) PostOffice channel added: %.*s",
                  fio_data.pid,
                  (int)ch->name_len,
                  ch->name);
}

/* callback called when a channel is freed from a channel_store map */
FIO_SFUNC void postoffice_on_channel_removed(channel_s *ch) {
  FIO_LOG_DDEBUG2("(%d) PostOffice closed channel (filter %ld): %.*s",
                  fio_data.pid,
                  (long)ch->filter,
                  (int)ch->name_len,
                  ch->name);
  if (ch->filter)
    return;
  fio_str_info_s cmds[] = {
      {"unsubscribe", 11},
      {"punsubscribe", 12},
  };
  fio_publish(.engine = FIO_PUBSUB_ROOT,
              .filter = -1,
              .channel = cmds[ch->type == CHANNEL_TYPE_PATTERN],
              .message = {ch->name, ch->name_len});
  FIO_LOG_DDEBUG2("(%d) PostOffice channel removed: %.*s",
                  fio_data.pid,
                  (int)ch->name_len,
                  ch->name);
}

/* registers a child's subscribe event and calls subscribe for engines */
FIO_SFUNC void postoffice_on_global_subscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_subscribe(.io = fio_msg2letter(msg)->from,
                  .channel = msg->message,
                  .is_pattern = 0);
    return;
  }
  /* global channel created. */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT && postoffice.engines[i]; ++i) {
    postoffice.engines[i]->subscribe(postoffice.engines[i], msg->message);
  }
}

/* registers a child's psubscribe event and calls psubscribe for engines */
FIO_SFUNC void postoffice_on_global_psubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_subscribe(.io = fio_msg2letter(msg)->from,
                  .channel = msg->message,
                  .is_pattern = 1);
    return;
  }
  /* global channel created. */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT && postoffice.engines[i]; ++i) {
    postoffice.engines[i]->psubscribe(postoffice.engines[i], msg->message);
  }
}

/* registers a child's unsubscribe event and calls unsubscribe for engines */
FIO_SFUNC void postoffice_on_global_unsubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_unsubscribe(.io = fio_msg2letter(msg)->from,
                    .channel = msg->message,
                    .is_pattern = 0);
    return;
  }
  /* channel removed. */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT && postoffice.engines[i]; ++i) {
    postoffice.engines[i]->unsubscribe(postoffice.engines[i], msg->message);
  }
}

/* registers a child's pubsubscribe event and calls pubsubscribe for engines */
FIO_SFUNC void postoffice_on_global_pubsubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_unsubscribe(.io = fio_msg2letter(msg)->from,
                    .channel = msg->message,
                    .is_pattern = 1);
    return;
  }
  /* global channel removed. */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
  for (int i = 0; i < FIO_PUBSUB_ENGINE_LIMIT && postoffice.engines[i]; ++i) {
    postoffice.engines[i]->punsubscribe(postoffice.engines[i], msg->message);
  }
}

/* *****************************************************************************
Pubsub Init
***************************************************************************** */

FIO_SFUNC void postoffice_pre__start(void *ignr_) {
  (void)ignr_;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT;
  postoffice.filter_ipc = FIO_PUBSUB_SIBLINGS_BIT;
  postoffice.filter_cluster = FIO_PUBSUB_CLUSTER_BIT;
  postoffice.ipc.on_data = pubsub_ipc_on_data_master;
  postoffice.ipc.on_close = pubsub_ipc_on_close;
  postoffice.ipc.on_timeout = pubsub_connection_ping;
  postoffice.ipc.timeout = 300;
  postoffice.ipc_listen.on_data = pubsub_ipc_on_new_connection;
  postoffice.ipc_listen.on_close = pubsub_listener_ipc_on_close;
  postoffice.ipc_listen.on_timeout = FIO_PING_ETERNAL;
  postoffice.cluster.on_data = pubsub_cluster_on_data;
  postoffice.cluster.on_close = pubsub_ipc_on_close;
  postoffice.cluster.on_timeout = pubsub_connection_ping;
  postoffice.cluster.timeout = 300;
  postoffice.cluster_listen.on_data = pubsub_ipc_on_new_connection;
  postoffice.cluster_listen.on_close = pubsub_listener_cluster_on_close;
  postoffice.cluster_listen.on_timeout = FIO_PING_ETERNAL;
  postoffice.mock_io.protocol = &postoffice.ipc;
  postoffice.mock_io.env = ENV_SAFE_INIT;
  postoffice.mock_io.fd = -1;
  postoffice.mock_io.state = FIO_IO_OPEN;
  postoffice.mock_io.lock = FIO_LOCK_INIT;
  if (fio_data.workers) {
    int fd =
        fio_sock_open2(postoffice.ipc_url, FIO_SOCK_SERVER | FIO_SOCK_NONBLOCK);
    FIO_ASSERT(fd != -1,
               "failed to create IPC listening socket.",
               (int)fio_data.pid);
    fio_attach_fd(fd, &postoffice.ipc_listen, &postoffice.ipc, NULL);
    FIO_LOG_DEBUG2("listening for pub/sub IPC @ %s", postoffice.ipc_url);
  }
}

FIO_SFUNC void postoffice_forked_child(void *ignr_) {
  (void)ignr_;
  for (fio_protocol_s *pr = &postoffice.ipc;
       pr <= &postoffice.cluster_discovery;
       ++pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      fio_sock_close(io->fd);
      io->fd = -1;
      fio_close_now_unsafe(io);
    }
  }
  for (int i = 0; postoffice.subscription_handles[i]; ++i) {
    fio_unsubscribe(.subscription_handle_ptr =
                        postoffice.subscription_handles + i);
    postoffice.subscription_handles[i] = 0;
  }
  // perform all callbacks before replacing them
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;

  postoffice.ipc.on_data = pubsub_ipc_on_data_worker;
  postoffice.ipc.on_close = pubsub_ipc_on_close_in_child;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT;
  postoffice.filter_ipc = ~(uint8_t)FIO_PUBSUB_PROCESS_BIT;
  postoffice.ipc.timeout = -1;
  postoffice.filter_cluster = 0;
  int fd =
      fio_sock_open2(postoffice.ipc_url, FIO_SOCK_CLIENT | FIO_SOCK_NONBLOCK);
  FIO_ASSERT(fd != -1,
             "(%d) failed to connect to master process.",
             (int)fio_data.pid);
  fio_s *io = fio_attach_fd(fd, &postoffice.ipc, letter_parser_new(), NULL);
  (void)io;
  FIO_LOG_DDEBUG2("(%d) connecting IPC to PostOffice %p (fd %d)",
                  (int)fio_data.pid,
                  (void *)io,
                  fd);
}

FIO_SFUNC void postoffice_on_finish(void *ignr_) {
  (void)ignr_;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT;
}

FIO_SFUNC void postoffice_at_exit(void *ignr_) {
  (void)ignr_;
  env_safe_destroy(&postoffice.mock_io.env);
  for (int i = 0; i < CHANNEL_TYPE_NONE; ++i) {
    channel_store_destroy(postoffice.channels + i);
  }
  if (!fio_data.is_master)
    return;

  if (!memcmp(postoffice.ipc_url, "unix:", 5) |
      !memcmp(postoffice.ipc_url, "file:", 5)) {
    fio_url_s url =
        fio_url_parse(postoffice.ipc_url, strlen(postoffice.ipc_url));
    FIO_LOG_DDEBUG2("(%d) PostOffice unlinking %s", fio_data.pid, url.path.buf);
    unlink(url.path.buf);
  }
}

FIO_SFUNC void postoffice_initialize(void) {
  for (fio_protocol_s *pr = &postoffice.ipc;
       pr <= &postoffice.cluster_discovery;
       ++pr) {
    fio_protocol_validate(pr);
    pr->reserved.flags |= 1;
    FIO_LIST_PUSH(&fio_data.protocols, &pr->reserved.protocols);
  }
  fio_state_callback_add(FIO_CALL_PRE_START, postoffice_pre__start, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, postoffice_forked_child, NULL);
  fio_state_callback_add(FIO_CALL_ON_FINISH, postoffice_on_finish, NULL);
  fio_state_callback_add(FIO_CALL_AT_EXIT, postoffice_at_exit, NULL);
  subscribe_args_s to_subscribe[] = {
      {
          .filter = -1,
          .channel = {"subscribe", 9},
          .on_message = postoffice_on_global_subscribe,
      },
      {
          .filter = -1,
          .channel = {"psubscribe", 10},
          .on_message = postoffice_on_global_psubscribe,
      },
      {
          .filter = -1,
          .channel = {"unsubscribe", 11},
          .on_message = postoffice_on_global_unsubscribe,
      },
      {
          .filter = -1,
          .channel = {"punsubscribe", 12},
          .on_message = postoffice_on_global_pubsubscribe,
      },
      {0},
  };
  for (int i = 0; to_subscribe[i].on_message; ++i) {
    to_subscribe[i].subscription_handle_ptr =
        postoffice.subscription_handles + i;
    fio_subscribe FIO_NOOP(to_subscribe[i]);
  }
  size_t pos = 0;
#if FIO_OS_POSIX
  memcpy(postoffice.ipc_url, "unix://./fio-pubsub-", 20);
  pos += 20;
  pos += fio_ltoa(postoffice.ipc_url + pos, ((uint64_t)fio_rand64() >> 24), 32);
  memcpy(postoffice.ipc_url + pos, ".sock", 5);
  pos += 5;
#else
  memcpy(postoffice.ipc_url, "tcp://127.0.0.1:9999", 20);
  pos += 20;
#endif
  postoffice.ipc_url[pos] = 0;
}

/* *****************************************************************************
 * TODO: clusterfy the local network using UDP broadcasting for node discovery.
 **************************************************************************** */
#if 0
/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *server_tls, fio_tls_s *client_tls);
#endif

#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, letter)(void) {
  struct test_info {
    uint64_t id;
    char *channel;
    char *msg;
    int32_t filter;
  } test_info[] = {
      {
          42,
          "My Channel",
          "My channel Message",
          0,
      },
      {
          0,
          NULL,
          "My filter Message",
          1,
      },
      {0},
  };
  for (int i = 0;
       test_info[i].msg || test_info[i].channel || test_info[i].filter;
       ++i) {
    letter_s *l =
        letter_author((fio_s *)(test_info + i),
                      test_info[i].id,
                      test_info[i].filter,
                      test_info[i].channel,
                      (test_info[i].channel ? strlen(test_info[i].channel) : 0),
                      test_info[i].msg,
                      (test_info[i].msg ? strlen(test_info[i].msg) : 0),
                      (uint8_t)(uintptr_t)FIO_PUBSUB_LOCAL);
    FIO_ASSERT(letter_id(l) == test_info[i].id,
               "message ID identity error, %llu != %llu",
               letter_id(l),
               test_info[i].id);
    FIO_ASSERT(letter_is_filter(l) == !!test_info[i].filter,
               "letter filter flag author error");
    if (letter_is_filter(l)) {
      FIO_ASSERT(letter_filter(l) == test_info[i].filter,
                 "filter identity error %d != %d",
                 letter_filter(l),
                 test_info[i].filter);
    }
    if (test_info[i].msg) {
      FIO_ASSERT(letter_message_len(l) == strlen(test_info[i].msg),
                 "letter message length error");
      FIO_ASSERT(
          !memcmp(letter_message(l), test_info[i].msg, letter_message_len(l)),
          "message identity error (%s != %.*s)",
          test_info[i].msg,
          (int)letter_message_len(l),
          letter_message(l));
    } else {
      FIO_ASSERT(!letter_message_len(l),
                 "letter message length error %d != 0",
                 letter_message_len(l));
    }
    if (test_info[i].channel) {
      FIO_ASSERT(letter_channel_len(l) == strlen(test_info[i].channel),
                 "letter channel length error");
      FIO_ASSERT(letter_channel(l) && !memcmp(letter_channel(l),
                                              test_info[i].channel,
                                              letter_channel_len(l)),
                 "channel identity error (%s != %.*s)",
                 test_info[i].channel,
                 (int)l->channel_len,
                 letter_channel(l));
    } else {
      FIO_ASSERT(!letter_channel_len(l), "letter channel length error");
    }
    letter_free(l);
  }
}
#endif /* TEST */
/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Post fork cleanup
***************************************************************************** */
static void fio___after_fork(void) {
  fio_malloc_after_fork();
  fio___after_fork___core();
  fio___after_fork___io();
}
/* *****************************************************************************
State data initialization
***************************************************************************** */

FIO_DESTRUCTOR(fio_cleanup_at_exit) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  env_safe_destroy(&fio_data.env);
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_close_wakeup_pipes();
  if (fio_data.io_wake_io) {
    fio_free2(fio_data.io_wake_io);
    fio_data.io_wake_io = NULL;
  }
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_monitor_destroy();
  fio_cli_end();
  fio_invalidate_all();
  for (int i = 0; i < FIO_CALL_NEVER; ++i)
    fio_state_callback_clear((callback_type_e)i);
  for (int i = 0; i < CHANNEL_TYPE_NONE; ++i)
    channel_store_destroy(postoffice.channels + i);

  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_queue_destroy(fio_tasks);
  fio_queue_destroy(fio_tasks + 1);
}

FIO_CONSTRUCTOR(fio_data_state_init) {
  FIO_LOG_DEBUG2("initializing facio.io IO state.");
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.master = fio_data.pid = getpid();
  fio_data.tick = fio_time2milli(fio_time_real());
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.timers = (fio_timer_queue_s)FIO_TIMER_QUEUE_INIT;
  fio_tasks[0] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_tasks[0]);
  fio_tasks[1] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_tasks[1]);
  fio_monitor_init();
  fio_protocol_validate(&FIO_PROTOCOL_HIJACK);
  fio_validity_map_reserve(&fio_data.valid, 300);
  FIO_PROTOCOL_HIJACK.on_timeout = FIO_PING_ETERNAL;
  FIO_PROTOCOL_HIJACK.reserved.protocols =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.protocols);
  FIO_PROTOCOL_HIJACK.reserved.ios =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.ios);
  FIO_PROTOCOL_HIJACK.reserved.flags |= 1;
#if FIO_VALIDATE_IO_MUTEX
  fio_data.valid_lock = (fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT;
#endif
  postoffice_initialize();
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
}
/* *****************************************************************************
Testing
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */
#ifdef TEST

FIO_SFUNC void fio_test___on_message(fio_msg_s *msg) {
  size_t *i = msg->udata;
  FIO_LOG_DEBUG("fio_test___on_message called");
  FIO_ASSERT(msg->channel.len == 10, "channel name length error");
  FIO_ASSERT(!memcmp("my channel", msg->channel.buf, msg->channel.len),
             "channel name error");
  FIO_ASSERT(msg->message.len == 7, "message length error");
  FIO_ASSERT(!memcmp("payload", msg->message.buf, msg->message.len),
             "message payload error");
  fio_unsubscribe(.channel = {"my channel", 10});
  i[0] += 1;
}

FIO_SFUNC void fio_test___on_unsubscribe(void *udata) {
  size_t *i = udata;
  i[1] += 1;
  FIO_LOG_DEBUG("fio_test___on_unsubscribe called");
}

FIO_SFUNC int fio_test___task_timed(void *u1, void *u2) {
  fio_stop();
  (void)u1;
  (void)u2;
  return -1;
}
FIO_SFUNC void fio_test___task(void *u1, void *u2) {
  fio_run_every(.fn = fio_test___task_timed, .every = 2000, .repetitions = 1);
  (void)u1;
  (void)u2;
}
void fio_test(void) {
  FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  size_t subscription_flags[2] = {0};
  fprintf(stderr, "Testing facil.io IO-Core framework modules.\n");
  FIO_NAME_TEST(io, letter)();
  FIO_NAME_TEST(io, state)();
  fio_subscribe(.channel = {"my channel", 10},
                .on_message = fio_test___on_message,
                .on_unsubscribe = fio_test___on_unsubscribe,
                .udata = subscription_flags);
  fio_subscribe(.channel = {"my channel", 10},
                .on_message = fio_test___on_message,
                .on_unsubscribe = fio_test___on_unsubscribe,
                .udata = subscription_flags); /* should overwrite first */
  fio_publish(.channel = {"my channel", 10}, .message = {"payload", 7});
  fio_defer(fio_test___task, NULL, NULL);
  fio_start(.threads = -2, .workers = 0);
  FIO_ASSERT(subscription_flags[0] == 1,
             "subscription on_message never called");
  FIO_ASSERT(subscription_flags[1] == 2,
             "subscription on_unsubscribe never called");
}
#endif
