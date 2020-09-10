/* *****************************************************************************
Developent Sugar (ignore)
***************************************************************************** */
#ifndef H_FACIL_IO_H            /* Development inclusion - ignore line */
#define FIO_EXTERN_COMPLETE   1 /* Development inclusion - ignore line */
#define FIOBJ_EXTERN_COMPLETE 1 /* Development inclusion - ignore line */
#define FIO_VERSION_GUARD       /* Development inclusion - ignore line */
#include "0003 main api.h"      /* Development inclusion - ignore line */
#include "0004 overridables.h"  /* Development inclusion - ignore line */
#include "0005 pubsub.h"        /* Development inclusion - ignore line */
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
#if defined(HAVE_KQUEUE)
#define FIO_ENGINE_KQUEUE 1
#elif defined(HAVE_EPOLL)
#define FIO_ENGINE_EPOLL 1
#else
#define FIO_ENGINE_POLL 1
#endif
#endif

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

FIO_IFUNC int
fio_defer_urgent(void (*task)(void *, void *), void *udata1, void *udata2);

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
  void *data;
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
    fio_defer_urgent(fio_uuid_env_obj_call_callback_task, u.p, o.data);
  }
}

#define FIO_MAP_NAME               fio___uuid_env
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
FIO_IFUNC int
fio_defer_urgent(void (*task)(void *, void *), void *udata1, void *udata2);

/* *****************************************************************************
Forking / Cleanup (declarations)
***************************************************************************** */
FIO_IFUNC void fio_defer_on_fork(void);
FIO_IFUNC void fio_state_callback_on_fork(void);
FIO_IFUNC void fio_pubsub_on_fork(void);
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
static void deferred_ping(void *uuid, void *arg2);

/* *****************************************************************************
Polling / Signals (declarations)
***************************************************************************** */
static inline void fio_force_close_in_poll(intptr_t uuid);
FIO_IFUNC void fio_poll_close(void);
static void fio_poll_init(void);
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
  while ((r = uuid_try_lock(uuid, lock_group))) {
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
  if (fio_data->max_open_fd < fd && is_open) {
    fio_data->max_open_fd = fd;
  } else {
    while (fio_data->max_open_fd && !fd_data(fio_data->max_open_fd).open)
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
Destroy the State Machine
***************************************************************************** */

static void __attribute__((destructor)) fio___data_free(void) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_state_callback_clear_all();
  free(fio_data);
}

/* *****************************************************************************
Initialize the State Machine
***************************************************************************** */

static void __attribute__((constructor)) fio___data_new(void) {
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
  fio_data->parent = getpid();
  fio_data->is_master = 1;
  fio_data->lock = FIO_LOCK_INIT;
  fio_mark_time();
  fio_poll_init();
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
  fio_state_callback_clear(FIO_CALL_ON_INITIALIZE);
}
