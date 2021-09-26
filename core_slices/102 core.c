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
  FIO_LOG_DEBUG2("(%d) IO wakeup fio_s freed", (int)fio_data.pid);
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
