/* ************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* ************************************************************************** */

/* *****************************************************************************
IO reactor's work
***************************************************************************** */
static fio_lock_i fio_fork_lock;

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
      fio_defer(deferred_ping, (void *)uuid, NULL);
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
  FIO_LOG_INFO("(%d) starting shutdown sequence", getpid());
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  for (size_t i = 0; i <= fio_data->max_open_fd; ++i) {
    /* test for and call shutdown callback + possible close callback */
    if (fd_data(i).open || fd_data(i).protocol) {
      fio_defer(deferred_on_shutdown, (void *)fd2uuid(i), NULL);
    }
  }
  while (fio_data->max_open_fd) {
    fio___perform_tasks_and_poll();
  }
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
}

/* *****************************************************************************
Sentinal tasks for worker processes
***************************************************************************** */

static void fio_sentinel_task(void *arg1, void *arg2);
static void *fio_sentinel_worker_thread(void *arg) {
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
        fio_state_callback_force(FIO_CALL_ON_CHILD_CRUSH);
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
      fio_defer(fio_sentinel_task, NULL, NULL);
      fio_unlock(&fio_fork_lock);
    }
#endif
  } else {
    FIO_LOG_INFO("worker process %d spawned.", getpid());
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
  fio_data->threads = args.threads;
  fio_data->workers = args.workers;
  fio_data->need_review = 1;
  fio_state_callback_force(FIO_CALL_PRE_START);
  fio_signal_handler_setup();
  FIO_LOG_INFO("\n\t Starting facil.io in %s mode."
               "\n\t Worker(s):  %d"
               "\n\t Threads(s): %d"
               "\n\t Capacity:   %zu"
               "\n\t Root PID:   %zu",
               fio_data->workers ? "cluster" : "single processes",
               fio_data->workers,
               fio_data->threads,
               (size_t)fio_data->capa,
               (size_t)fio_data->parent);
  fio_data->active = 1;
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

/* *****************************************************************************







                    Handle signals and child reaping







***************************************************************************** */

volatile uint8_t fio_signal_children_flag = 0;

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
static void fio___sig_handler_hot_restart(int sig, void *ignr_) {
  (void)(sig);
  (void)ignr_;
  fio_signal_children_flag = 1;
  if (!fio_is_master())
    fio_stop();
}
#endif

static void fio___sig_handler_stop(int sig, void *ignr_) {
  (void)(sig);
  (void)ignr_;
  if (!fio_data->active) {
    if (fio_data->max_open_fd) {
      FIO_LOG_WARNING("Server exit eas signaled (again?) while server is "
                      "shutting down.\n          Pushing things...");
      fio_data->max_open_fd = 0;
    } else {
      FIO_LOG_FATAL("Server exit eas signaled while server not running.");
      exit(-1);
    }
  }
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
#define FIO_MAP_NAME           fio_state_tasks
#define FIO_MAP_TYPE           fio___state_task_s
#define FIO_MAP_TYPE_CMP(a, b) (a.func == b.func && a.arg == b.arg)
#include <fio-stl.h>

static fio_state_tasks_s fio_state_tasks_array[FIO_CALL_NEVER];
static fio_lock_i fio_state_tasks_array_lock[FIO_CALL_NEVER + 1];

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
             (void *)p->num,
             (void *)mask,
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

FIO_IFUNC int
fio_defer_urgent(void (*task)(void *, void *), void *udata1, void *udata2) {
  return fio_queue_push_urgent(
      &fio___task_queue, .fn = task, .udata1 = udata1, .udata2 = udata2);
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
static void
fio___test__defer_io_task(intptr_t uuid, fio_protocol_s *pr, void *udata) {
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
