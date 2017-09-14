/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "defer.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* *****************************************************************************
Compile time settings
***************************************************************************** */

#ifndef DEFER_QUEUE_BUFFER
#define DEFER_QUEUE_BUFFER 4096
#endif
#ifndef DEFER_THROTTLE
#define DEFER_THROTTLE 524287UL
#endif

/* *****************************************************************************
Data Structures
***************************************************************************** */

/* task node data */
typedef struct {
  void (*func)(void *, void *);
  void *arg1;
  void *arg2;
} task_s;

/* a single linked list for tasks */
typedef struct task_node_s {
  task_s task;
  struct task_node_s *next;
} task_node_s;

/* static memory allocation for a task node buffer */
static task_node_s tasks_buffer[DEFER_QUEUE_BUFFER];

/* the state machine - this holds all the data about the task queue and pool */
static struct {
  /* the next task to be performed */
  task_node_s *first;
  /* a pointer to the linked list's tail, where the next task will be stored */
  task_node_s **last;
  /* a linked list for task nodes (staticly allocated) */
  task_node_s *pool;
  /* a lock for the state machine, used for multi-threading support */
  spn_lock_i lock;
  /* a flag indicating whether the task pool was initialized */
  unsigned char initialized;
} deferred = {.first = NULL,
              .last = &deferred.first,
              .pool = NULL,
              .lock = 0,
              .initialized = 0};

/* *****************************************************************************
API
***************************************************************************** */

/** Defer an execution of a function for later. */
int defer(void (*func)(void *, void *), void *arg1, void *arg2) {
  /* must have a task to defer */
  if (!func)
    goto call_error;
  task_node_s *task;
  /* lock the state machine, to grab/create a task and place it at the tail */
  spn_lock(&deferred.lock);
  if (deferred.pool) {
    task = deferred.pool;
    deferred.pool = deferred.pool->next;
  } else if (deferred.initialized) {
    task = malloc(sizeof(task_node_s));
    if (!task)
      goto error;
  } else
    goto initialize;

schedule:
  *deferred.last = task;
  deferred.last = &task->next;
  task->task.func = func;
  task->task.arg1 = arg1;
  task->task.arg2 = arg2;
  task->next = NULL;
  spn_unlock(&deferred.lock);
  return 0;

error:
  spn_unlock(&deferred.lock);
  perror("ERROR CRITICAL: defer can't allocate task");
  kill(0, SIGINT), exit(errno);

call_error:
  return -1;

initialize:
  /* initialize the task pool using all the items in the static buffer */
  /* also assign `task` one of the tasks from the pool and schedule the task */
  deferred.initialized = 1;
  task = tasks_buffer;
  deferred.pool = tasks_buffer + 1;
  for (size_t i = 1; i < (DEFER_QUEUE_BUFFER - 1); i++) {
    tasks_buffer[i].next = &tasks_buffer[i + 1];
  }
  tasks_buffer[DEFER_QUEUE_BUFFER - 1].next = NULL;
  goto schedule;
}

/** Performs all deferred functions until the queue had been depleted. */
void defer_perform(void) {
  task_node_s *tmp;
  task_s task;
restart:
  spn_lock(&deferred.lock); /* remember never to perform tasks within a lock! */
  tmp = deferred.first;
  if (tmp) {
    deferred.first = tmp->next;
    if (!deferred.first)
      deferred.last = &deferred.first;
    task = tmp->task;
    if (tmp >= tasks_buffer && tmp < tasks_buffer + DEFER_QUEUE_BUFFER) {
      tmp->next = deferred.pool;
      deferred.pool = tmp;
    } else {
      free(tmp);
    }
    spn_unlock(&deferred.lock);
    /* perform the task outside the lock. */
    task.func(task.arg1, task.arg2);
    /* I used `goto` to optimize assembly instruction flow. maybe it helps. */
    goto restart;
  }
  spn_unlock(&deferred.lock);
}

/** returns true if there are deferred functions waiting for execution. */
int defer_has_queue(void) { return deferred.first != NULL; }

/* *****************************************************************************
Thread Pool Support
***************************************************************************** */

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) ||           \
    defined(DEBUG)
#include <pthread.h>

/* `weak` functions can be overloaded to change the thread implementation. */

#pragma weak defer_new_thread
void *defer_new_thread(void *(*thread_func)(void *), pool_pt arg) {
  pthread_t *thread = malloc(sizeof(*thread));
  if (thread == NULL || pthread_create(thread, NULL, thread_func, (void *)arg))
    goto error;
  return thread;
error:
  free(thread);
  return NULL;
}

#pragma weak defer_join_thread
int defer_join_thread(void *p_thr) {
  if (!p_thr)
    return -1;
  pthread_join(*(pthread_t *)p_thr, NULL);
  free(p_thr);
  return 0;
}

#pragma weak defer_thread_throttle
void defer_thread_throttle(unsigned long microsec) {
  throttle_thread(microsec);
}

#else /* No pthreads... BYO thread implementation. This one simply fails. */

#pragma weak defer_new_thread
void *defer_new_thread(void *(*thread_func)(void *), void *arg) {
  (void)thread_func;
  (void)arg;
  return NULL;
}
#pragma weak defer_join_thread
int defer_join_thread(void *p_thr) {
  (void)p_thr;
  return -1;
}

#pragma weak defer_thread_throttle
void defer_thread_throttle(unsigned long microsec) { return; }

#endif /* DEBUG || pthread default */

struct defer_pool {
  unsigned int flag;
  unsigned int count;
  void *threads[];
};

/* a thread's cycle. This is what a worker thread does... repeatedly. */
static void *defer_worker_thread(void *pool_) {
  volatile pool_pt pool = pool_;
  signal(SIGPIPE, SIG_IGN);
  /* the throttle replaces conditional variables for better performance */
  size_t throttle = (pool->count) * DEFER_THROTTLE;
  if (!throttle || throttle > 1572864UL)
    throttle = 1572864UL;
  /* perform any available tasks */
  defer_perform();
  /* as long as the flag is true, wait for and perform tasks. */
  do {
    throttle_thread(throttle);
    defer_perform();
  } while (pool->flag);
  return NULL;
}

/** Signals a running thread pool to stop. Returns immediately. */
void defer_pool_stop(pool_pt pool) { pool->flag = 0; }

/** Returns TRUE (1) if the pool is hadn't been signaled to finish up. */
int defer_pool_is_active(pool_pt pool) { return pool->flag; }

/** Waits for a running thread pool, joining threads and finishing all tasks. */
void defer_pool_wait(pool_pt pool) {
  while (pool->count) {
    pool->count--;
    defer_join_thread(pool->threads[pool->count]);
  }
}

/** The logic behind `defer_pool_start`. */
static inline pool_pt defer_pool_initialize(unsigned int thread_count,
                                            pool_pt pool) {
  pool->flag = 1;
  pool->count = 0;
  while (pool->count < thread_count &&
         (pool->threads[pool->count] =
              defer_new_thread(defer_worker_thread, pool)))
    pool->count++;
  if (pool->count == thread_count) {
    return pool;
  }
  defer_pool_stop(pool);
  return NULL;
}

/** Starts a thread pool that will run deferred tasks in the background. */
pool_pt defer_pool_start(unsigned int thread_count) {
  if (thread_count == 0)
    return NULL;
  pool_pt pool = malloc(sizeof(*pool) + (thread_count * sizeof(void *)));
  if (!pool)
    return NULL;
  return defer_pool_initialize(thread_count, pool);
}

/* *****************************************************************************
Child Process support (`fork`)
***************************************************************************** */

/* forked `defer` workers use a global thread pool object. */
static pool_pt forked_pool;

/* handles the SIGINT and SIGTERM signals by shutting down workers */
static void sig_int_handler(int sig) {
  if (sig != SIGINT && sig != SIGTERM)
    return;
  if (!forked_pool)
    return;
  defer_pool_stop(forked_pool);
}

/*
Zombie Reaping
With thanks to Dr Graham D Shaw.
http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
*/
void reap_child_handler(int sig) {
  (void)(sig);
  int old_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = old_errno;
}

/* initializes zombie reaping for the process */
inline static void reap_children(void) {
  struct sigaction sa;
  sa.sa_handler = reap_child_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) == -1) {
    perror("Child reaping initialization failed");
    kill(0, SIGINT), exit(errno);
  }
}

/* a global process identifier (0 == root) */
static int defer_fork_pid_id = 0;

/**
 * Forks the process, starts up a thread pool and waits for all tasks to run.
 * All existing tasks will run in all processes (multiple times).
 *
 * Returns 0 on success, -1 on error and a positive number if this is a child
 * process that was forked.
 */
int defer_perform_in_fork(unsigned int process_count,
                          unsigned int thread_count) {
  if (forked_pool)
    return -1; /* we're already running inside an active `fork` */

  /* we use a placeholder while initializing the forked thread pool, so calls to
   * `defer_fork_is_active` don't fail.
   */
  static struct defer_pool pool_placeholder = {.count = 1, .flag = 1};

  /* setup signal handling */
  struct sigaction act, old, old_term, old_pipe;
  pid_t *pids = NULL;
  int ret = 0;
  unsigned int pids_count;

  act.sa_handler = sig_int_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if (sigaction(SIGINT, &act, &old)) {
    perror("couldn't set signal handler");
    goto finish;
  };

  if (sigaction(SIGTERM, &act, &old_term)) {
    perror("couldn't set signal handler");
    goto finish;
  };

  act.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &act, &old_pipe)) {
    perror("couldn't set signal handler");
    goto finish;
  };

  /* setup zomie reaping */
  reap_children();

  if (!process_count)
    process_count = 1;
  --process_count;

  /* for `process_count == 0` nothing happens */
  pids = calloc(process_count, sizeof(*pids));
  if (process_count && !pids)
    goto finish;
  for (pids_count = 0; pids_count < process_count; pids_count++) {
    if (!(pids[pids_count] = fork())) {
      defer_fork_pid_id = pids_count + 1;
      forked_pool = &pool_placeholder;
      forked_pool = defer_pool_start(thread_count);
      defer_pool_wait(forked_pool);
      defer_perform();
      defer_perform();
      return 1;
    }
    if (pids[pids_count] == -1) {
      ret = -1;
      goto finish;
    }
  }

  forked_pool = &pool_placeholder;
  forked_pool = defer_pool_start(thread_count);

  defer_pool_wait(forked_pool);
  forked_pool = NULL;

  defer_perform();

finish:
  if (pids) {
    for (size_t j = 0; j < pids_count; j++) {
      kill(pids[j], SIGINT);
    }
    for (size_t j = 0; j < pids_count; j++) {
      waitpid(pids[j], NULL, 0);
    }
    free(pids);
  }
  sigaction(SIGINT, &old, &act);
  sigaction(SIGTERM, &old_term, &act);
  sigaction(SIGTERM, &old_pipe, &act);
  return ret;
}

/** Returns TRUE (1) if the forked thread pool hadn't been signaled to finish
 * up. */
int defer_fork_is_active(void) { return forked_pool && forked_pool->flag; }

/** Returns the process number for the current working proceess. 0 == parent. */
int defer_fork_pid(void) { return defer_fork_pid_id; }

/* *****************************************************************************
Test
***************************************************************************** */
#ifdef DEBUG

#include <stdio.h>

#include <pthread.h>
#define DEFER_TEST_THREAD_COUNT 128

static spn_lock_i i_lock = 0;
static size_t i_count = 0;

static void sample_task(void *unused, void *unused2) {
  (void)(unused);
  (void)(unused2);
  spn_lock(&i_lock);
  i_count++;
  spn_unlock(&i_lock);
}

static void sched_sample_task(void *unused, void *unused2) {
  (void)(unused);
  (void)(unused2);
  for (size_t i = 0; i < 1024; i++) {
    defer(sample_task, NULL, NULL);
  }
}

static void thrd_sched(void *unused, void *unused2) {
  for (size_t i = 0; i < (1024 / DEFER_TEST_THREAD_COUNT); i++) {
    sched_sample_task(unused, unused2);
  }
}

static void text_task_text(void *unused, void *unused2) {
  (void)(unused);
  (void)(unused2);
  spn_lock(&i_lock);
  fprintf(stderr, "this text should print before defer_perform returns\n");
  spn_unlock(&i_lock);
}

static void text_task(void *a1, void *a2) {
  static const struct timespec tm = {.tv_sec = 2};
  nanosleep(&tm, NULL);
  defer(text_task_text, a1, a2);
}

static void pid_task(void *arg, void *unused2) {
  (void)(unused2);
  fprintf(stderr, "* %d pid is going to sleep... (%s)\n", getpid(),
          arg ? (char *)arg : "unknown");
}

void defer_test(void) {
  time_t start, end;
  fprintf(stderr, "Starting defer testing\n");

  spn_lock(&i_lock);
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  for (size_t i = 0; i < (1024 * 1024); i++) {
    sample_task(NULL, NULL);
  }
  end = clock();
  fprintf(stderr,
          "Deferless (direct call) counter: %lu cycles with i_count = %lu\n",
          end - start, i_count);

  spn_lock(&i_lock);
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  for (size_t i = 0; i < 1024; i++) {
    defer(sched_sample_task, NULL, NULL);
  }
  defer_perform();
  end = clock();
  fprintf(stderr, "Defer single thread: %lu cycles with i_count = %lu\n",
          end - start, i_count);

  spn_lock(&i_lock);
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  pool_pt pool = defer_pool_start(DEFER_TEST_THREAD_COUNT);
  if (pool) {
    for (size_t i = 0; i < DEFER_TEST_THREAD_COUNT; i++) {
      defer(thrd_sched, NULL, NULL);
    }
    // defer((void (*)(void *))defer_pool_stop, pool);
    defer_pool_stop(pool);
    defer_pool_wait(pool);
    end = clock();
    fprintf(stderr,
            "Defer multi-thread (%d threads): %lu cycles with i_count = %lu\n",
            DEFER_TEST_THREAD_COUNT, end - start, i_count);
  } else
    fprintf(stderr, "Defer multi-thread: FAILED!\n");

  spn_lock(&i_lock);
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  for (size_t i = 0; i < 1024; i++) {
    defer(sched_sample_task, NULL, NULL);
  }
  defer_perform();
  end = clock();
  fprintf(stderr, "Defer single thread (2): %lu cycles with i_count = %lu\n",
          end - start, i_count);

  fprintf(stderr, "calling defer_perform.\n");
  defer(text_task, NULL, NULL);
  defer_perform();
  fprintf(stderr, "defer_perform returned. i_count = %lu\n", i_count);
  size_t pool_count = 0;
  task_node_s *pos = deferred.pool;
  while (pos) {
    pool_count++;
    pos = pos->next;
  }
  fprintf(stderr, "defer pool count %lu/%d (%s)\n", pool_count,
          DEFER_QUEUE_BUFFER,
          pool_count == DEFER_QUEUE_BUFFER ? "pass" : "FAILED");
  fprintf(stderr, "press ^C to finish PID test\n");
  defer(pid_task, "pid test", NULL);
  if (defer_perform_in_fork(4, 64) > 0) {
    fprintf(stderr, "* %d finished\n", getpid());
    exit(0);
  };
  fprintf(stderr,
          "   === Defer pool memory footprint %lu X %d = %lu bytes ===\n",
          sizeof(task_node_s), DEFER_QUEUE_BUFFER, sizeof(tasks_buffer));
}

#endif
