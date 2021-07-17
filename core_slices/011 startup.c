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
  FIO_ASSERT(pid != -1, "system call `fork` failed.");
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_unlock(GIL);
  if (pid) {
    int status = 0;
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
  (void)ignr_;
#else
  FIO_ASSERT(0, "Children reaping is only supported on POSIX systems.");
#endif
}
/**
 * Initializes zombie reaping for the process. Call before `fio_start` to enable
 * global zombie reaping.
 */
void fio_reap_children(void) {
#if FIO_OS_POSIX
  fio_signal_monitor(SIGCHLD, fio___reap_children, NULL);
#else
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
