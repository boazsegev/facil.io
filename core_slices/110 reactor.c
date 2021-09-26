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
  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  /* do not allow master tasks to run in worker */
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) &&
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  pid_t pid = fio_fork();
  FIO_ASSERT(pid != (pid_t)-1, "system call `fork` failed.");
  if (pid) {
    int status = 0;
    (void)status;
    fio_state_callback_force(FIO_CALL_AFTER_FORK);
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
