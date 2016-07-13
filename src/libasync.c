/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "libasync.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <string.h>

#ifdef __has_include
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define HAVE_X86Intrin
#define sched_yield() _mm_pause()
// see: https://software.intel.com/en-us/node/513411
// and: https://software.intel.com/sites/landingpage/IntrinsicsGuide/
#endif
#endif

/* *****************************************************************************
Performance options.
*/

#ifndef ASYNC_TASK_POOL_SIZE
#define ASYNC_TASK_POOL_SIZE 170
#endif

/* Spinlock vs. Mutex data protection. */
#ifndef ASYNC_USE_SPINLOCK
#define ASYNC_USE_SPINLOCK 1
#endif

/* use pipe for wakeup if == 0 else, use nanosleep when no tasks. */
#ifndef ASYNC_NANO_SLEEP
#define ASYNC_NANO_SLEEP 16777216  // 8388608  // 1048576  // 524288
#endif

/* Sentinal thread to respawn crashed threads - limited crash resistance. */
#ifndef ASYNC_USE_SENTINEL
#define ASYNC_USE_SENTINEL 0
#endif

/* *****************************************************************************
Forward declarations - used for functions that might be needed before they are
defined.
*/

// the actual working thread
static void* worker_thread_cycle(void*);

// A thread sentinal (optional - edit the ASYNC_USE_SENTINEL macro to use or
// disable)
static void* sentinal_thread(void*);

/******************************************************************************
Portability - used to help port this to different frameworks (i.e. Ruby).
*/

#ifndef THREAD_TYPE
#define THREAD_TYPE pthread_t

static void* join_thread(THREAD_TYPE thr) {
  void* ret;
  pthread_join(thr, &ret);
  return ret;
}

static int create_thread(THREAD_TYPE* thr,
                         void* (*thread_func)(void*),
                         void* async) {
  return pthread_create(thr, NULL, thread_func, async);
}

#endif
/******************************************************************************
Data Types
*/

/**
A task
*/
typedef struct {
  void (*task)(void*);
  void* arg;
} task_s;

/**
A task node
*/
typedef struct async_task_ns {
  task_s task;
  struct async_task_ns* next;
} async_task_ns;

/******************************************************************************
Core Data
*/

typedef struct {
#if !defined(ASYNC_USE_SPINLOCK) || ASYNC_USE_SPINLOCK != 1
  /* if using mutex */
  pthread_mutex_t lock;
#endif

  /* task management*/
  async_task_ns memory[ASYNC_TASK_POOL_SIZE];
  async_task_ns* pool;
  async_task_ns* tasks;
  async_task_ns** pos;

  /* thread management*/
  size_t thread_count;

#if ASYNC_NANO_SLEEP == 0
  /* when using pipes */
  struct {
    int in;
    int out;
  } io;
#endif

#if defined(ASYNC_USE_SPINLOCK) && ASYNC_USE_SPINLOCK == 1  // use spinlock
  /* if using spinlock */
  atomic_flag lock;
#endif

  /* state management*/
  struct {
    unsigned run : 1;
  } flags;

  /** the threads array, must be last */
  THREAD_TYPE threads[];
} async_data_s;

static async_data_s* async;

/******************************************************************************
Core Data initialization and lock/unlock
*/

#if defined(ASYNC_USE_SPINLOCK) && ASYNC_USE_SPINLOCK == 1  // use spinlock
#define lock_async_init() (atomic_flag_clear(&(async->lock)), 0)
#define lock_async_destroy() ;
#define lock_async()                               \
  while (atomic_flag_test_and_set(&(async->lock))) \
    sched_yield();
#define unlock_async() (atomic_flag_clear(&(async->lock)))

#else  // Using Mutex
#define lock_async_init() (pthread_mutex_init(&((async)->lock), NULL))
#define lock_async_destroy() (pthread_mutex_destroy(&((async)->lock)))
#define lock_async() (pthread_mutex_lock(&((async)->lock)))
#define unlock_async() (pthread_mutex_unlock(&((async)->lock)))
#endif

static inline void async_free(void) {
#if ASYNC_NANO_SLEEP == 0
  if (async->io.in) {
    close(async->io.in);
    close(async->io.out);
  }
#endif
  lock_async_destroy();
  munmap(async, (sizeof(async_data_s) +
                 (sizeof(THREAD_TYPE) * (async->thread_count))));
  async = NULL;
}

static inline void async_alloc(size_t threads) {
  async = mmap(NULL, (sizeof(async_data_s) + (sizeof(THREAD_TYPE) * (threads))),
               PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS,
               -1, 0);
  if (async == MAP_FAILED) {
    async = NULL;
  }
  *async = (async_data_s){.flags.run = 1};
  async->pos = &async->tasks;

  if (lock_async_init()) {
    async_free();
    return;
  }

#if ASYNC_NANO_SLEEP == 0  // using pipes?
  if (pipe(&async->io.in)) {
    async_free();
    return;
  }
  fcntl(async->io.out, F_SETFL, O_NONBLOCK);
#endif

  // initialize pool
  for (size_t i = 0; i < ASYNC_TASK_POOL_SIZE - 1; i++) {
    async->memory[i].next = async->memory + i + 1;
  }
  async->memory[ASYNC_TASK_POOL_SIZE - 1].next = NULL;
  async->pool = async->memory;
}

/******************************************************************************
Perfoming tasks
*/

static inline void perform_tasks(void) {
  task_s tsk;
  async_task_ns* t;
  while (async) {
    lock_async();
    t = async->tasks;
    if (t) {
      async->tasks = t->next;
      if (async->tasks == NULL)
        async->pos = &(async->tasks);
      tsk = t->task;
      if (t >= async->memory &&
          (t <= (async->memory + ASYNC_TASK_POOL_SIZE - 1))) {
        t->next = async->pool;
        async->pool = t;
      } else {
        free(t);
      }
      unlock_async();
      tsk.task(tsk.arg);
      continue;
    }
    async->pos = &(async->tasks);
    unlock_async();
    return;
  }
}

/******************************************************************************
Pasuing and resuming threads
*/

static inline void pause_thread() {
#if ASYNC_NANO_SLEEP == 0
  if (async && async->flags.run) {
    uint8_t tmp;
    read(async->io.in, &tmp, 1);
  }
#else
  struct timespec act, tm = {.tv_sec = 0, .tv_nsec = ASYNC_NANO_SLEEP};
  nanosleep(&tm, &act);
// sched_yield();
#endif
}

static inline void wake_thread() {
#if ASYNC_NANO_SLEEP == 0
  write(async->io.out, async, 1);
#endif
}

static inline void wake_all_threads() {
#if ASYNC_NANO_SLEEP == 0
  write(async->io.out, async, async->thread_count + 16);
#endif
}

/******************************************************************************
Worker threads
*/

// on thread failure, a backtrace should be printed (if
// using sentinal)
// manage thread error signals
static void on_err_signal(int sig) {
  void* array[22];
  size_t size;
  char** strings;
  size_t i;
  size = backtrace(array, 22);
  strings = backtrace_symbols(array, size);
  perror("\nERROR");
  fprintf(stderr,
          "Async: Error signal received"
          " - %s (errno %d).\nBacktrace (%zd):\n",
          strsignal(sig), errno, size);
  for (i = 2; i < size; i++)
    fprintf(stderr, "%s\n", strings[i]);
  free(strings);
  fprintf(stderr, "\n");
  // pthread_exit(0); // for testing
  pthread_exit((void*)on_err_signal);
}

// The worker cycle
static void* worker_thread_cycle(void* _) {
  // register error signals when using a sentinal
  if (ASYNC_USE_SENTINEL) {
    signal(SIGSEGV, on_err_signal);
    signal(SIGFPE, on_err_signal);
    signal(SIGILL, on_err_signal);
#ifdef SIGBUS
    signal(SIGBUS, on_err_signal);
#endif
#ifdef SIGSYS
    signal(SIGSYS, on_err_signal);
#endif
#ifdef SIGXFSZ
    signal(SIGXFSZ, on_err_signal);
#endif
  }

  // ignore pipe issues
  signal(SIGPIPE, SIG_IGN);

  // pause for signal for as long as we're active.
  while (async && async->flags.run) {
    perform_tasks();
    pause_thread();
  }
  perform_tasks();
  return 0;
}

// an optional sentinal
static void* sentinal_thread(void* _) {
  THREAD_TYPE thr;
  while (async != NULL && async->flags.run == 1 &&
         create_thread(&thr, worker_thread_cycle, _) == 0)
    join_thread(thr);
  return 0;
}

/******************************************************************************
API
*/

/**
Starts running the global thread pool. Use:

  async_start(8);

*/
ssize_t async_start(size_t threads) {
  async_alloc(threads);
  if (async == NULL)
    return -1;
  // initialize threads
  for (size_t i = 0; i < threads; i++) {
    if (create_thread(
            async->threads + i,
            (ASYNC_USE_SENTINEL ? sentinal_thread : worker_thread_cycle),
            NULL) < 0) {
      async->flags.run = 0;
      wake_all_threads();
      async_free();
      return -1;
    }
    ++async->thread_count;
  }
  signal(SIGPIPE, SIG_IGN);
  return 0;
}

/**
Waits for all the present tasks to complete.

The thread pool will remain active, waiting for new tasts.

This function will wait forever or until a signal is
received and all the tasks in the queue have been processed.

Unline finish (that uses `join`) this is an **active** wait where the waiting
thread acts as a working thread and performs any pending tasks.

Use:

  Async.wait(async);

*/
void async_perform() {
  perform_tasks();
}

/**
Schedules a task to be performed by the thread pool.

The Task should be a function such as `void task(void
*arg)`.

Use:

  void task(void * arg) { printf("%s", arg); }

  char arg[] = "Demo Task";

  async_run(task, arg);

*/
int async_run(void (*task)(void*), void* arg) {
  if (async == NULL)
    return -1;
  async_task_ns* tsk;
  lock_async();
  tsk = async->pool;
  if (tsk) {
    async->pool = tsk->next;
  } else {
    tsk = malloc(sizeof(*tsk));
    if (!tsk)
      goto error;
  }
  *tsk = (async_task_ns){.task.task = task, .task.arg = arg};
  *(async->pos) = tsk;
  async->pos = &(tsk->next);
  unlock_async();
  wake_thread();
  return 0;
error:
  unlock_async();
  return -1;
}

/**
Waits for existing tasks to complete and releases the thread
pool and it's
resources.
*/
void async_join() {
  if (async == NULL)
    return;
  for (size_t i = 0; i < async->thread_count; i++) {
    join_thread(async->threads[i]);
  }
  perform_tasks();
  async_free();
};

/**
Waits for existing tasks to complete and releases the thread
pool and it's
resources.
*/
void async_signal() {
  if (async == NULL)
    return;
  async->flags.run = 0;
  wake_all_threads();
};

/******************************************************************************
Test
*/

#if ASYNC_TEST_INC == 1

#define ASYNC_SPEED_TEST_THREAD_COUNT 120

static size_t _Atomic i_count = 0;

static void sample_task(void* _) {
  __asm__ volatile("" ::: "memory");
  atomic_fetch_add(&i_count, 1);
}

static void sched_sample_task(void* _) {
  for (size_t i = 0; i < 1024; i++) {
    async_run(sample_task, async);
  }
}

static void text_task_text(void* _) {
  __asm__ volatile("" ::: "memory");
  fprintf(stderr, "this text should print before async_finish returns\n");
}

static void text_task(void* _) {
  sleep(2);
  async_run(text_task_text, _);
}

#if ASYNC_USE_SENTINEL == 1
static void evil_task(void* _) {
  __asm__ volatile("" ::: "memory");
  fprintf(stderr, "EVIL CODE RUNNING!\n");
  sprintf(NULL,
          "Never write text to a NULL pointer, this is a terrible idea that "
          "should segfault.\n");
}
#endif

void async_test_library_speed(void) {
  atomic_store(&i_count, 0);
  time_t start, end;
  fprintf(stderr, "Starting Async testing\n");
  if (async_start(ASYNC_SPEED_TEST_THREAD_COUNT) == 0) {
    fprintf(stderr, "Thread count test %s %lu/%d\n",
            (async->thread_count == ASYNC_SPEED_TEST_THREAD_COUNT ? "PASSED"
                                                                  : "FAILED"),
            async->thread_count, ASYNC_SPEED_TEST_THREAD_COUNT);
    start = clock();
    for (size_t i = 0; i < 1024; i++) {
      async_run(sched_sample_task, async);
    }
    async_finish();
    end = clock();
    fprintf(stderr, "Async performance test %lu cycles with i_count = %lu\n",
            end - start, atomic_load(&i_count));
  } else {
    fprintf(stderr, "Async test couldn't be initialized\n");
    exit(-1);
  }
  if (async_start(8)) {
    fprintf(stderr, "Couldn't start thread pool!\n");
    exit(-1);
  }
  fprintf(stderr, "calling async_perform.\n");
  async_run(text_task, NULL);
  sleep(1);
  async_perform();
  fprintf(stderr, "async_perform returned.\n");
  fprintf(stderr, "calling finish.\n");
  async_run(text_task, NULL);
  sleep(1);
  async_finish();
  fprintf(stderr, "finish returned.\n");

#if ASYNC_USE_SENTINEL == 1
  if (async_start(8)) {
    fprintf(stderr, "Couldn't start thread pool!\n");
    exit(-1);
  }
  sleep(1);
  fprintf(stderr, "calling evil task.\n");
  async_run(evil_task, NULL);
  sleep(1);
  fprintf(stderr, "calling finish.\n");
  async_finish();
#endif

  // async_start(8);
  // fprintf(stderr,
  //         "calling a few tasks and sleeping 12 seconds before finishing
  //         up...\n"
  //         "check the processor CPU cycles - are we busy?\n");
  // async_run(sched_sample_task, NULL);
  // sleep(12);
  // async_finish();
}

#endif
