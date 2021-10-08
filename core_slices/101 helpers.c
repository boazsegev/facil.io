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

fio_tls_functions_s FIO_TLS_DEFAULT;

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

/* Called to perform a non-blocking `read`, same as the system call. */
static ssize_t mock_tls_default_read(int fd,
                                     void *buf,
                                     size_t len,
                                     fio_tls_s *tls) {
  (void)tls;
  return fio_sock_read(fd, buf, len);
}
/** Called to perform a non-blocking `write`, same as the system call. */
static ssize_t mock_tls_default_write(int fd,
                                      const void *buf,
                                      size_t len,
                                      fio_tls_s *tls) {
  (void)tls;
  return fio_sock_write(fd, buf, len);
}
/** Sends any unsent internal data. Returns 0 only if all data was sent. */
static int mock_tls_default_flush(int fd, fio_tls_s *tls) {
  return 0;
  (void)fd;
  (void)tls;
}
static fio_tls_s *mock_tls_default_dup(fio_tls_s *tls, fio_s *io) {
  return tls;
  (void)io;
}
static void mock_tls_default_free(fio_tls_s *tls) { (void)tls; }

static fio_tls_s *fio_tls_dup_server(fio_tls_s *tls, fio_s *io);
static fio_tls_s *fio_tls_dup_client(fio_tls_s *tls, fio_s *io);

FIO_IFUNC void fio_protocol_validate(fio_protocol_s *p) {
  if ((p->reserved.flags & 8))
    return;
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
  if (p->tls_functions.dup == fio_tls_dup_server)
    p->tls_functions = FIO_FUNCTIONS.TLS_SERVER;
  else if (p->tls_functions.dup == fio_tls_dup_client)
    p->tls_functions = FIO_FUNCTIONS.TLS_CLIENT;
  else {
    if (!p->tls_functions.read)
      p->tls_functions.read = mock_tls_default_read;
    if (!p->tls_functions.write)
      p->tls_functions.write = mock_tls_default_write;
    if (!p->tls_functions.flush)
      p->tls_functions.flush = mock_tls_default_flush;
    if (!p->tls_functions.dup)
      p->tls_functions.dup = mock_tls_default_dup;
    if (!p->tls_functions.free)
      p->tls_functions.free = mock_tls_default_free;
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
