/* *****************************************************************************
Copyright: Boaz Segev, 2018-2020
License: MIT

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
External STL features published
***************************************************************************** */
#define FIO_EXTERN_COMPLETE 1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>

/* *****************************************************************************
Internal STL features in use (unpublished)
***************************************************************************** */
#define FIO_STR_SMALL fio_str
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

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 1000
#endif

#ifndef FIO_MAX_ADDR_LEN
#define FIO_MAX_ADDR_LEN 48
#endif

/* *****************************************************************************
Section Start Marker












                               Task Queues













***************************************************************************** */

/* *****************************************************************************
Task Queues
***************************************************************************** */

#define FIO_QUEUE
#include <fio-stl.h>

static fio_queue_s fio___task_queue = FIO_QUEUE_INIT(fio___task_queue);
static fio_queue_s fio___io_task_queue = FIO_QUEUE_INIT(fio___io_task_queue);
static fio_timer_queue_s fio___timer_tasks = FIO_TIMER_QUEUE_INIT;

static inline void fio___defer_clear_io_queue(void) {
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

/* *****************************************************************************
Section Start Marker












                               State Callbacks













***************************************************************************** */

typedef struct {
  void (*func)(void *);
  void *arg;
} fio___state_task_s;

#define FIO_MAP_NAME fio_state_tasks
#define FIO_MAP_TYPE fio___state_task_s
#define FIO_MAP_TYPE_CMP(a, b) (a.func == b.func && a.arg == b.arg)
#include <fio-stl.h>

static fio_state_tasks_s fio_state_tasks_array[FIO_CALL_NEVER];
static fio_lock_i fio_state_tasks_array_lock[FIO_CALL_NEVER + 1];

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e e,
                            void (*func)(void *),
                            void *arg) {
  if ((uintptr_t)e > FIO_CALL_NEVER)
    return;
  uint64_t hash = (uint64_t)func ^ (uint64_t)arg;
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
  uint64_t hash = (uint64_t)func ^ (uint64_t)arg;
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
  if ((uintptr_t)e > FIO_CALL_NEVER)
    return;
  fio___state_task_s *ary = NULL;
  size_t len = 0;
  /* copy task queue */
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  if (fio_state_tasks_array[e].w) {
    ary = fio_calloc(sizeof(*ary), fio_state_tasks_array[e].w);
    FIO_ASSERT_ALLOC(ary);
    FIO_MAP_EACH2(fio_state_tasks, (fio_state_tasks_array + e), pos) {
      if (!pos->hash || !pos->obj.func)
        continue;
      ary[len++] = pos->obj;
    }
  }
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
  /* perform copied tasks */
  switch (e) {
  case FIO_CALL_ON_INITIALIZE: /* fallthrough */
  case FIO_CALL_PRE_START:     /* fallthrough */
  case FIO_CALL_BEFORE_FORK:   /* fallthrough */
  case FIO_CALL_AFTER_FORK:    /* fallthrough */
  case FIO_CALL_IN_CHILD:      /* fallthrough */
  case FIO_CALL_IN_MASTER:     /* fallthrough */
  case FIO_CALL_ON_START:      /* fallthrough */
  case FIO_CALL_ON_IDLE:       /* fallthrough */
    /* perform tasks in order */
    for (size_t i = 0; i < len; ++i) {
      ary[i].func(ary[i].arg);
    }
    break;
  case FIO_CALL_ON_SHUTDOWN:     /* fallthrough */
  case FIO_CALL_ON_FINISH:       /* fallthrough */
  case FIO_CALL_ON_PARENT_CRUSH: /* fallthrough */
  case FIO_CALL_ON_CHILD_CRUSH:  /* fallthrough */
  case FIO_CALL_AT_EXIT:         /* fallthrough */
  case FIO_CALL_NEVER:           /* fallthrough */
    /* perform tasks in reverse */
    while (len--) {
      ary[len].func(ary[len].arg);
    }
    break;
  }
  fio_free(ary);
}

/* *****************************************************************************
Section Start Marker












                           IO Data Array (state machine)













***************************************************************************** */

/* *****************************************************************************
Event deferring (declarations)
***************************************************************************** */

static void deferred_on_close(void *uuid_, void *pr_);
static void deferred_on_shutdown(void *arg, void *arg2);
static void deferred_on_ready(void *arg, void *arg2);
static void deferred_on_data(void *uuid, void *arg2);
static void deferred_ping(void *arg, void *arg2);

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
    fio_queue_push_urgent(&fio___task_queue,
                          .fn = fio_uuid_env_obj_call_callback_task,
                          .udata1 = u.p,
                          .udata2 = o.data);
  }
}

#define FIO_MAP_NAME fio___uuid_env
#define FIO_MAP_TYPE fio_uuid_env_obj_s
#define FIO_MAP_TYPE_DESTROY(o) fio_uuid_env_obj_call_callback((o))
#define FIO_MAP_DESTROY_AFTER_COPY 0

/* destroy discarded keys when overwriting existing data (const_name support) */
#define FIO_MAP_KEY fio_str_s /* the small string type */
#define FIO_MAP_KEY_COPY(dest, src) (dest) = (src)
#define FIO_MAP_KEY_DESTROY(k) fio_str_destroy(&k)
#define FIO_MAP_KEY_DISCARD FIO_MAP_KEY_DESTROY
#define FIO_MAP_KEY_CMP(a, b) fio_str_is_eq(&(a), &(b))
#include <fio-stl.h>

/* *****************************************************************************
State machine types and data
***************************************************************************** */

typedef struct fio_packet_s fio_packet_s;

typedef struct {
  /* fd protocol */
  fio_protocol_s *protocol;
  /** RW hooks. */
  fio_rw_hook_s *rw_hooks;
  /** RW udata. */
  void *rw_udata;
  /* current data to be send */
  fio_packet_s *packet;
  /** the last packet in the queue. */
  fio_packet_s **packet_last;
  /* Data sent so far */
  size_t sent;
  /* timer handler */
  time_t active;
  /* Objects linked to the UUID */
  fio___uuid_env_s env;
  /** The number of pending packets that are in the queue. */
  uint16_t packet_count;
  /** flag for polling state */
  unsigned short polling;
  /* timeout settings */
  uint8_t timeout;
  /* indicates that the fd should be considered scheduled (added to poll) */
  fio_lock_i scheduled;
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

struct fio___data_s {
  /* last `poll` cycle */
  struct timespec last_cycle;
  /* thread list */
  FIO_LIST_HEAD thread_ids;
  /* connection capacity */
  uint32_t capa;
  /* connections counted towards shutdown (NOT while running) */
  uint32_t connection_count;
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
  /* polling and global lock */
  fio_lock_i lock;
  /* fd_data array */
  fio_fd_data_s info[];
} * fio_data;

#define fd_data(fd) (fio_data->info[(uintptr_t)(fd)])
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

#define touchfd(fd) fd_data((fd)).active = fio_data->last_cycle.tv_sec

inline static int uuid_try_lock(intptr_t uuid) {
  if (!uuid_is_valid(uuid))
    goto error;
  if (fio_trylock(&uuid_data(uuid).lock))
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

#define UUID_TRYLOCK(uuid, reschedule_lable, invalid_lable)                    \
  do {                                                                         \
    switch (uuid_try_lock(uuid)) {                                             \
    case 1:                                                                    \
      goto reschedule_lable;                                                   \
    case 2:                                                                    \
      goto invalid_lable;                                                      \
    }                                                                          \
  } while (0)

#define UUID_UNLOCK(uuid)                                                      \
  do {                                                                         \
    fio_unlock(&uuid_data(uuid).lock);                                         \
  } while (0)

/* *****************************************************************************
Public API
***************************************************************************** */

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

/**
 * "Touches" a socket connection, resetting it's timeout counter.
 */
void fio_touch(intptr_t uuid) {
  if (uuid_is_valid(uuid)) {
    touchfd(fio_uuid2fd(uuid));
  }
}

/* *****************************************************************************
Section Start Marker












                             Polling (poll only)













***************************************************************************** */

static int fio_poll_add(intptr_t uuid, uint16_t state);

/* *****************************************************************************
Section Start Marker












                           User Land Buffer / Packets













***************************************************************************** */

/** A noop function for fio_write2 in cases not deallocation is required. */
void FIO_DEALLOC_NOOP(void *arg) { (void)arg; }

/* *****************************************************************************
IO user Packets
***************************************************************************** */

/** User-space socket buffer data */
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

static inline void fio_packet_free(fio_packet_s *packet) {
  if (packet) {
    packet->dealloc(packet->data.buf);
    fio_free(packet);
  }
}

static inline void fio_packet_free_all(fio_packet_s *packet) {
  while (packet) {
    fio_packet_s *t = packet->next;
    packet->dealloc(packet->data.buf);
    fio_free(packet);
    packet = t;
  }
}

/* *****************************************************************************
Writing data from a packet to the IO
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
      fd2uuid(fd),
      fd_data(fd).rw_udata,
      ((uint8_t *)packet->data.buf + packet->offset),
      packet->len);
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
        pread(packet->data.fd,
              buff,
              ((packet->len < BUFFER_FILE_READ_SIZE) ? packet->len
                                                     : BUFFER_FILE_READ_SIZE),
              packet->offset);
    if (asked <= 0)
      goto read_error;
    sent = fd_data(fd).rw_hooks->write(
        fd2uuid(fd), fd_data(fd).rw_udata, buff, asked);
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
    ret = sendfile(packet->data.fd,
                   fd,
                   packet->offset,
                   (size_t)act_sent,
                   NULL,
                   &act_sent,
                   0);
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
Packet createion / allocation
***************************************************************************** */

static inline fio_packet_s *fio_packet_new(void) {
  fio_packet_s *packet = fio_malloc(sizeof(*packet));
  FIO_ASSERT_ALLOC(packet);
  return packet;
}

/**
 * `fio_write2_fn` is the actual function behind the macro `fio_write2`.
 */
static inline fio_packet_s *fio_packet_new2(fio_write_args_s *options,
                                            uint8_t use_sendfile) {
  /* create packet */
  fio_packet_s *packet = fio_malloc(sizeof(*packet));
  *packet = (fio_packet_s){
      .len = options->len,
      .offset = options->offset,
      .data.buf = (void *)options->data.buf,
      .write_func = fio_sock_write_buf,
      .dealloc = (options->after.dealloc ? options->after.dealloc : free)};
  if (options->is_fd) {
    packet->write_func =
        (use_sendfile) ? fio_sock_sendfile_from_fd : fio_sock_write_from_fd;
    packet->dealloc =
        (options->after.dealloc ? options->after.dealloc
                                : (void (*)(void *))fio_sock_perform_close_fd);
  }
  return packet;
}

/* *****************************************************************************
Section Start Marker












                           Core Connection Management













***************************************************************************** */

/* resets connection data, marking it as either open or closed. */
static inline int fio_clear_fd(intptr_t fd, uint8_t is_open) {
  fio_packet_s *packet;
  fio_protocol_s *protocol;
  fio_rw_hook_s *rw_hooks;
  void *rw_udata;
  fio___uuid_env_s env;
  fio_lock(&(fd_data(fd).lock));
  intptr_t uuid = fd2uuid(fd);
  env = fd_data(fd).env;
  packet = fd_data(fd).packet;
  protocol = fd_data(fd).protocol;
  rw_hooks = fd_data(fd).rw_hooks;
  rw_udata = fd_data(fd).rw_udata;
  fd_data(fd) = (fio_fd_data_s){
      .rw_hooks = (fio_rw_hook_s *)&FIO_DEFAULT_RW_HOOKS,
      .open = is_open,
      .lock = fd_data(fd).lock,
      .counter = fd_data(fd).counter + (!is_open),
      .packet_last = &fd_data(fd).packet,
  };
  if (fio_data->max_open_fd < fd) {
    fio_data->max_open_fd = fd;
  } else {
    while (fio_data->max_open_fd && !fd_data(fio_data->max_open_fd).open)
      --fio_data->max_open_fd;
  }
  fio_unlock(&(fd_data(fd).lock));
  if (rw_hooks && rw_hooks->cleanup)
    rw_hooks->cleanup(rw_udata);
  fio_packet_free_all(packet);
  fio___uuid_env_destroy(&env);
  if (protocol && protocol->on_close) {
    fio_defer(deferred_on_close, (void *)uuid, protocol);
  }
  FIO_LOG_DEBUG("FD %d re-initialized (state: %p-%s).",
                (int)fd,
                (void *)fd2uuid(fd),
                (is_open ? "open" : "closed"));
  return 0;
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
  if (uuid_data(uuid).packet || uuid_data(uuid).lock) {
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
  fio_lock(&uuid_data(uuid).lock);
  packet = uuid_data(uuid).packet;
  uuid_data(uuid).packet = NULL;
  uuid_data(uuid).packet_last = &uuid_data(uuid).packet;
  uuid_data(uuid).sent = 0;
  fio_unlock(&uuid_data(uuid).lock);
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
  fio_lock(&uuid_data(uuid).lock);
  fio_clear_fd(fio_uuid2fd(uuid), 0);
  fio_unlock(&uuid_data(uuid).lock);
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

/* *****************************************************************************
Section Start Marker












                           Protocol Management













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
  fio_close(uuid);
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
Protocol Locking (multi-thread safety concerns)
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

enum fio_protocol_lock_e {
  FIO_PR_LOCK_TASK = 0,
  FIO_PR_LOCK_PING = 1,
  FIO_PR_LOCK_DATA = 2,
};

/* Macro for accessing the protocol locking / metadata. */
#define prt_meta(prt) (((union protocol_metadata_union_u *)(&(prt)->rsv))->meta)

/** locks a connection's protocol returns a pointer that need to be unlocked. */
inline static fio_protocol_s *protocol_try_lock(intptr_t uuid, uint8_t sub) {
  UUID_TRYLOCK(uuid, would_block, invalid);
  fio_protocol_s *pr = uuid_data(uuid).protocol;
  uint8_t attempt = fio_trylock_sublock(&prt_meta(pr).lock, sub);
  UUID_UNLOCK(uuid);
  if (attempt)
    goto would_block;
  errno = 0;
  return pr;
would_block:
  errno = EWOULDBLOCK;
  return NULL;
invalid:
  return NULL;
}
/** See `fio_protocol_try_lock` for details. */
inline static void protocol_unlock(fio_protocol_s *pr, uint8_t sub) {
  fio_unlock_sublock(&prt_meta(pr).lock, sub);
}

/**
 * This function allows out-of-task access to a connection's `fio_protocol_s`
 * object by attempting to acquire a locked pointer.
 */
fio_protocol_s *fio_protocol_try_lock(intptr_t uuid) {
  return protocol_try_lock(uuid, FIO_PR_LOCK_TASK);
}

/** Don't unlock what you don't own. See `fio_protocol_try_lock` for details. */
void fio_protocol_unlock(fio_protocol_s *pr) {
  if (!pr)
    return;
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
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
    if (!protocol->ping) {
      protocol->ping = mock_ping;
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

  do {
    UUID_TRYLOCK(uuid, lock_was_busy, error_invalid_uuid);
    fio_protocol_s *old_pr = uuid_data(uuid).protocol;
    uuid_data(uuid).open = 1;
    uuid_data(uuid).protocol = protocol;
    touchfd(fio_uuid2fd(uuid));
    UUID_UNLOCK(uuid);
    if (old_pr) {
      /* protocol replacement */
      fio_defer(deferred_on_close, (void *)uuid, old_pr);
      if (!protocol) {
        /* hijacking */
        fio_poll_remove_fd(fio_uuid2fd(uuid));
        fio_poll_add_write(fio_uuid2fd(uuid));
      }
    } else if (protocol) {
      /* adding a new uuid to the reactor */
      fio_poll_add(fio_uuid2fd(uuid));
    }
    return 0;
  lock_was_busy:
    FIO_THREAD_RESCHEDULE();
  } while (1);

error_invalid_uuid:
  if (protocol)
    fio_defer_push_task(deferred_on_close, (void *)uuid, protocol);
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

/* *****************************************************************************
Connection Read / Write Hooks, for overriding the system calls
***************************************************************************** */

static ssize_t
fio_hooks_default_read(intptr_t uuid, void *udata, void *buf, size_t count) {
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

static inline void fio_rw_hook_validate(fio_rw_hook_s *rw_hooks) {
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

const fio_rw_hook_s FIO_DEFAULT_RW_HOOKS = {
    .read = fio_hooks_default_read,
    .write = fio_hooks_default_write,
    .flush = fio_hooks_default_flush,
    .before_close = fio_hooks_default_before_close,
    .cleanup = fio_hooks_default_cleanup,
};

/* *****************************************************************************












Initialization, Reactor Loops and Cleanup












***************************************************************************** */

static void __attribute__((constructor)) fio___lib_init(void) {
  fio_lock(&fio_state_tasks_array_lock[FIO_CALL_NEVER]);
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
}

static void __attribute__((destructor)) fio___lib_destroy(void) {
  /* clear pending tasks */
  fio_queue_perform_all(&fio___task_queue);
  fio___defer_clear_io_queue();
  /* perform at_exit callbacks */
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_unlock(&fio_state_tasks_array_lock[FIO_CALL_NEVER]);
  /* clear pending tasks (again) */
  fio_queue_perform_all(&fio___task_queue);
  fio___defer_clear_io_queue();
  /* TODO free IO state machine */
}

/* *****************************************************************************












IO Kernal - public API












***************************************************************************** */

/* *****************************************************************************
Task Queues - Public API
***************************************************************************** */

/** The IO task internal wrapper */
static inline int fio___defer_perform_io_task(void) {
  fio_queue_task_s t = fio_queue_pop(&fio___io_task_queue);
  if (!t.fn)
    return -1;
  union {
    void (*t)(void *, void *);
    void (*io)(intptr_t, fio_protocol_s *, void *);
  } u = {.t = t.fn};
  fio_protocol_s *pr = protocol_try_lock((uintptr_t)t.udata1, 0);
  if (!pr && errno == EWOULDBLOCK)
    goto reschedule;
  u.io((uintptr_t)t.udata1, pr, t.udata2);
  return 0;
reschedule:
  fio_queue_push(&fio___io_task_queue, u.t, t.udata1, t.udata2);
  return 0;
}

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
  return fio_queue_push(
      &fio___task_queue, .fn = task, .udata1 = udata1, .udata2 = udata2);
}

/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If an error ocuurs or the connection is closed before the task can run, the
 * task wil be called with a NULL protocol pointer, for resource cleanup.
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
                     .repetitions = repetitions);
}

/**
 * Performs all deferred tasks.
 */
void fio_defer_perform(void) {
  while (fio_queue_count(&fio___task_queue)) {
    fio_queue_perform_all(&fio___task_queue);
    fio_timer_push2queue(&fio___task_queue, &fio___timer_tasks, 0);
  }
}

/** Returns true if there are deferred functions waiting for execution. */
int fio_defer_has_queue(void) {
  return fio_queue_count(&fio___task_queue) > 0 ||
         fio_queue_count(&fio___io_task_queue) > 0;
}

/* *****************************************************************************
Connection Read / Write Hooks, for overriding the system calls
***************************************************************************** */

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
  int replaced = -1;
  uint8_t was_locked;
  intptr_t fd = fio_uuid2fd(uuid);
  fio_rw_hook_validate(rw_hooks);
  /* protect against some fulishness... but not all of it. */
  was_locked = fio_trylock(&fd_data(fd).lock);
  if (fd2uuid(fd) == uuid) {
    fd_data(fd).rw_hooks = rw_hooks;
    fd_data(fd).rw_udata = udata;
    replaced = 0;
  }
  if (!was_locked)
    fio_unlock(&fd_data(fd).lock);
  return replaced;
}

/** Sets a socket hook state (a pointer to the struct). */
int fio_rw_hook_set(intptr_t uuid, fio_rw_hook_s *rw_hooks, void *udata) {
  intptr_t fd = fio_uuid2fd(uuid);
  if (!uuid_is_valid(uuid) || !uuid_data(uuid).open)
    goto invalid_uuid;
  fio_rw_hook_validate(rw_hooks);
  fio_rw_hook_s *old_rw_hooks;
  void *old_udata;
  fio_lock(&fd_data(fd).lock);
  if (fd2uuid(fd) != uuid) {
    goto invalid_uuid;
  }
  old_rw_hooks = fd_data(fd).rw_hooks;
  old_udata = fd_data(fd).rw_udata;
  fd_data(fd).rw_hooks = rw_hooks;
  fd_data(fd).rw_udata = udata;
  fio_unlock(&fd_data(fd).lock);
  if (old_rw_hooks && old_rw_hooks->cleanup)
    old_rw_hooks->cleanup(old_udata);
  return 0;
invalid_uuid:
  fio_unlock(&fd_data(fd).lock);
  if (!rw_hooks->cleanup)
    rw_hooks->cleanup(udata);
  return -1;
}
/* *****************************************************************************












Tests












***************************************************************************** */
#ifdef TEST

/* *****************************************************************************
Test state callbacks
***************************************************************************** */

/* State callback test task */
static void fio___test__state_callbacks__task(void *udata) {
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
             (void *)p->num,
             (void *)mask,
             (p->rev ? "reversed" : ""));
  p->num |= (uint64_t)1ULL << p->bit;
  p->bit += 1;
}

/* State callback tests */
static void fio___test__state_callbacks(void) {
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
    fio_state_callback_add(
        FIO_CALL_ON_IDLE, fio___test__state_callbacks__task, &data);
  }
  fio_state_callback_force(FIO_CALL_ON_IDLE);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* state tests for clean up tasks */
  data.num = 0;
  data.bit = 0;
  data.rev = 1;
  for (size_t i = 0; i < 24; ++i) {
    fio_state_callback_add(
        FIO_CALL_ON_SHUTDOWN, fio___test__state_callbacks__task, &data);
  }
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* restore old state and cleanup */
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_array[i] = fio_state_tasks_array_old[i];
    fio_state_tasks_destroy(fio_state_tasks_array_old + i);
  }
}

/* *****************************************************************************
Test Protocol Locking
***************************************************************************** */

/* protocol locking tests */
static void fio___test__protocol_locks(void) {
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

/* *****************************************************************************
Test IO tasks
***************************************************************************** */

typedef struct {
  size_t nulls;
  size_t with_value;
  intptr_t last_uuid;
} fio___test__defer_io_s;

/** defer IO test task */
static void
fio___test__defer_io_task(intptr_t uuid, fio_protocol_s *pr, void *udata) {
  fio___test__defer_io_s *i = udata;
  if (pr)
    ++(i->with_value);
  else
    ++(i->nulls);
  i->last_uuid = uuid;
}

/** defer IO test */
static void fio___test__defer_io(void) {
  fprintf(stderr, "* testing defer (IO tasks only - main test in STL).\n");
  fio___test__defer_io_s expect = {0}, result = {0};
  for (size_t i = 0; i < fio_data->capa; ++i) {
    if (fd_data(i).protocol)
      ++(expect.with_value);
    else
      ++(expect.nulls);
    expect.last_uuid = fd2uuid(i);
    fio_defer_io_task(fd2uuid(i), fio___test__defer_io_task, &result);
  }
  while (!fio___defer_perform_io_task())
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

/* *****************************************************************************
Test UUID Linking
***************************************************************************** */

FIO_SFUNC void fio_uuid_env_test_on_close(void *obj) {
  fio_atomic_add((uintptr_t *)obj, 1);
}

/*
FIO_SFUNC void fio_uuid_env_test(void) {
  fprintf(stderr, "=== Testing fio_uuid_env\n");
  uintptr_t called = 0;
  uintptr_t removed = 0;
  uintptr_t overwritten = 0;
  intptr_t uuid = fio_socket(
      NULL, "8765", FIO_SOCKET_TCP | FIO_SOCKET_SERVER | FIO_SOCKET_NONBLOCK);
  FIO_ASSERT(uuid != -1, "fio_uuid_env_test failed to create a socket!");
  fio_uuid_env_set(
      uuid, .data = &called, .on_close = fio_uuid_env_test_on_close, .type = 1);
  FIO_ASSERT(called == 0,
             "fio_uuid_env_set failed - on_close callback called too soon!");
  fio_uuid_env_set(uuid,
                   .data = &removed,
                   .on_close = fio_uuid_env_test_on_close,
                   .type = 0);
  fio_uuid_env_set(uuid,
                   .data = &overwritten,
                   .on_close = fio_uuid_env_test_on_close,
                   .type = 0,
                   .name.buf = "abcdefghijklmnopqrstuvwxyz",
                   .name.len = 26);
  fio_uuid_env_set(uuid,
                   .data = &overwritten,
                   .on_close = fio_uuid_env_test_on_close,
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
*/

/* *****************************************************************************
Test Aggragated
***************************************************************************** */

/** Run facil.io IO core tests */
void fio_test(void) {
  fprintf(stderr, "===============\n");
  /* switch to test data set */
  struct fio___data_s *old = fio_data;
  fio_protocol_s proto = {0};
  fio_data = fio_malloc(sizeof(*old) + (sizeof(old->info[0]) * 128));
  fio_data->capa = 128;
  fd_data(1).protocol = &proto;
  fprintf(stderr, "Starting facil.io IO core tests.\n");
  fprintf(stderr, "===============\n");
  fio___test__state_callbacks();
  fprintf(stderr, "===============\n");
  fio___test__protocol_locks();
  fprintf(stderr, "===============\n");
  fio___test__defer_io();
  fprintf(stderr, "===============\n");
  /* free test data set and return normal data set */
  fio_free(fio_data);
  fio_data = old;
  fprintf(stderr, "Done.\n");
}
#endif
