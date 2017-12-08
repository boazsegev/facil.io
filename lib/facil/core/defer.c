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

#ifndef DEFER_THROTTLE
#define DEFER_THROTTLE 524287UL
#endif
#ifndef DEFER_THROTTLE_LIMIT
#define DEFER_THROTTLE_LIMIT 1572864UL
#endif

#ifndef DEFER_QUEUE_BLOCK_COUNT
#if UINTPTR_MAX <= 0xFFFFFFFF
/* Almost a page of memory on most 32 bit machines: ((4096/4)-4)/3 */
#define DEFER_QUEUE_BLOCK_COUNT 340
#else
/* Almost a page of memory on most 64 bit machines: ((4096/8)-4)/3 */
#define DEFER_QUEUE_BLOCK_COUNT 168
#endif
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

/* task queue block */
typedef struct queue_block_s {
  task_s tasks[DEFER_QUEUE_BLOCK_COUNT];
  struct queue_block_s *next;
  size_t write;
  size_t read;
  uint8_t state;
} queue_block_s;

static queue_block_s static_queue;

/* the state machine - this holds all the data about the task queue and pool */
static struct {
  /* a lock for the state machine, used for multi-threading support */
  spn_lock_i lock;
  /* current active block to pop tasks */
  queue_block_s *reader;
  /* current active block to push tasks */
  queue_block_s *writer;
} deferred = {.reader = &static_queue, .writer = &static_queue};

/* *****************************************************************************
Internal Data API
***************************************************************************** */

#if DEBUG
static size_t count_alloc, count_dealloc;
#define COUNT_ALLOC spn_add(&count_alloc, 1)
#define COUNT_DEALLOC spn_add(&count_dealloc, 1)
#define COUNT_RESET                                                            \
  do {                                                                         \
    count_alloc = count_dealloc = 0;                                           \
  } while (0)
#else
#define COUNT_ALLOC
#define COUNT_DEALLOC
#define COUNT_RESET
#endif

static inline void push_task(task_s task) {
  spn_lock(&deferred.lock);

  /* test if full */
  if (deferred.writer->state &&
      deferred.writer->write == deferred.writer->read) {
    /* return to static buffer or allocate new buffer */
    if (static_queue.state == 2) {
      deferred.writer->next = &static_queue;
    } else {
      deferred.writer->next = malloc(sizeof(*deferred.writer->next));
      COUNT_ALLOC;
      if (!deferred.writer->next)
        goto critical_error;
    }
    deferred.writer = deferred.writer->next;
    deferred.writer->write = 0;
    deferred.writer->read = 0;
    deferred.writer->state = 0;
    deferred.writer->next = NULL;
  }

  /* place task and finish */
  deferred.writer->tasks[deferred.writer->write++] = task;
  /* cycle buffer */
  if (deferred.writer->write == DEFER_QUEUE_BLOCK_COUNT) {
    deferred.writer->write = 0;
    deferred.writer->state = 1;
  }
  spn_unlock(&deferred.lock);
  return;

critical_error:
  spn_unlock(&deferred.lock);
  perror("ERROR CRITICAL: defer can't allocate task");
  kill(0, SIGINT);
  exit(errno);
}

static inline task_s pop_task(void) {
  task_s ret = (task_s){NULL};
  queue_block_s *to_free = NULL;
  /* lock the state machine, to grab/create a task and place it at the tail
  */ spn_lock(&deferred.lock);

  /* empty? */
  if (deferred.reader->write == deferred.reader->read &&
      !deferred.reader->state)
    goto finish;
  /* collect task */
  ret = deferred.reader->tasks[deferred.reader->read++];
  /* cycle */
  if (deferred.reader->read == DEFER_QUEUE_BLOCK_COUNT) {
    deferred.reader->read = 0;
    deferred.reader->state = 0;
  }
  /* did we finish the queue in the buffer? */
  if (deferred.reader->write == deferred.reader->read) {
    if (deferred.reader->next) {
      to_free = deferred.reader;
      deferred.reader = deferred.reader->next;
    } else {
      deferred.reader->write = deferred.reader->read = deferred.reader->state =
          0;
    }
    goto finish;
  }

finish:
  if (to_free == &static_queue) {
    static_queue.state = 2;
  }
  spn_unlock(&deferred.lock);

  if (to_free && to_free != &static_queue) {
    free(to_free);
    COUNT_DEALLOC;
  }
  return ret;
}

static inline void clear_tasks(void) {
  spn_lock(&deferred.lock);
  while (deferred.reader) {
    queue_block_s *tmp = deferred.reader;
    deferred.reader = deferred.reader->next;
    if (tmp != &static_queue) {
      COUNT_DEALLOC;
      free(tmp);
    }
  }
  static_queue = (queue_block_s){.next = NULL};
  deferred.reader = deferred.writer = &static_queue;
  spn_unlock(&deferred.lock);
}

#define push_task(...) push_task((task_s){__VA_ARGS__})

/* *****************************************************************************
API
***************************************************************************** */

/** Defer an execution of a function for later. */
int defer(void (*func)(void *, void *), void *arg1, void *arg2) {
  /* must have a task to defer */
  if (!func)
    goto call_error;
  push_task(.func = func, .arg1 = arg1, .arg2 = arg2);
  return 0;

call_error:
  return -1;
}

/** Performs all deferred functions until the queue had been depleted. */
void defer_perform(void) {
  task_s task = pop_task();
  while (task.func) {
    task.func(task.arg1, task.arg2);
    task = pop_task();
  }
}

/** Returns true if there are deferred functions waiting for execution. */
int defer_has_queue(void) {
  return deferred.reader->read != deferred.reader->write;
}

/** Clears the queue. */
void defer_clear_queue(void) { clear_tasks(); }

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
  pthread_join(*((pthread_t *)p_thr), NULL);
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

/* thread pool data container */
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
  if (!throttle || throttle > DEFER_THROTTLE_LIMIT)
    throttle = DEFER_THROTTLE_LIMIT;
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

/**
OVERRIDE THIS to replace the default `fork` implementation or to inject hooks
into the forking function.

Behaves like the system's `fork`.
*/
#pragma weak defer_new_child
int defer_new_child(void) { return (int)fork(); }

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
#if !defined(NO_CHILD_REAPER) || NO_CHILD_REAPER == 0
  reap_children();
#endif

  if (!process_count)
    process_count = 1;
  --process_count;

  /* for `process_count == 0` nothing happens */
  pids = calloc(process_count, sizeof(*pids));
  if (process_count && !pids)
    goto finish;
  for (pids_count = 0; pids_count < process_count; pids_count++) {
    if (!(pids[pids_count] = (pid_t)defer_new_child())) {
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

static void single_counter_task(void *unused, void *unused2) {
  (void)(unused);
  (void)(unused2);
  spn_lock(&i_lock);
  i_count++;
  spn_unlock(&i_lock);
  if (i_count < (1024 * 1024))
    defer(single_counter_task, NULL, NULL);
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
          "Deferless (direct call) counter: %lu cycles with i_count = %lu, "
          "%lu/%lu free/malloc\n",
          end - start, i_count, count_dealloc, count_alloc);

  spn_lock(&i_lock);
  COUNT_RESET;
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  defer(single_counter_task, NULL, NULL);
  defer(single_counter_task, NULL, NULL);
  defer_perform();
  end = clock();
  fprintf(stderr,
          "Defer single thread, two tasks: "
          "%lu cycles with i_count = %lu, %lu/%lu "
          "free/malloc\n",
          end - start, i_count, count_dealloc, count_alloc);

  spn_lock(&i_lock);
  COUNT_RESET;
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  for (size_t i = 0; i < 1024; i++) {
    defer(sched_sample_task, NULL, NULL);
  }
  defer_perform();
  end = clock();
  fprintf(stderr,
          "Defer single thread: %lu cycles with i_count = %lu, %lu/%lu "
          "free/malloc\n",
          end - start, i_count, count_dealloc, count_alloc);

  spn_lock(&i_lock);
  COUNT_RESET;
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
            "Defer multi-thread (%d threads): %lu cycles with i_count = %lu, "
            "%lu/%lu free/malloc\n",
            DEFER_TEST_THREAD_COUNT, end - start, i_count, count_dealloc,
            count_alloc);
  } else
    fprintf(stderr, "Defer multi-thread: FAILED!\n");

  spn_lock(&i_lock);
  COUNT_RESET;
  i_count = 0;
  spn_unlock(&i_lock);
  start = clock();
  for (size_t i = 0; i < 1024; i++) {
    defer(sched_sample_task, NULL, NULL);
  }
  defer_perform();
  end = clock();
  fprintf(stderr,
          "Defer single thread (2): %lu cycles with i_count = %lu, %lu/%lu "
          "free/malloc\n",
          end - start, i_count, count_dealloc, count_alloc);

  fprintf(stderr, "calling defer_perform.\n");
  defer(text_task, NULL, NULL);
  defer_perform();
  fprintf(stderr,
          "defer_perform returned. i_count = %lu, %lu/%lu free/malloc\n",
          i_count, count_dealloc, count_alloc);

  fprintf(stderr, "press ^C to finish PID test\n");
  defer(pid_task, "pid test", NULL);
  if (defer_perform_in_fork(4, 64) > 0) {
    fprintf(stderr, "* %d finished\n", getpid());
    exit(0);
  };
  fprintf(stderr, "* Defer queue %lu/%lu free/malloc\n", count_dealloc,
          count_alloc);
  defer_clear_queue();
  fprintf(stderr, "* Defer queue %lu/%lu free/malloc\n", count_dealloc,
          count_alloc);
}

#endif
