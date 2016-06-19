/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
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

/******************************************************************************
Forward declarations - used for functions that might be needed before they are
defined.
*/

// the actual working thread
static void* worker_thread_cycle(void* async);

// A thread sentinal (optional - edit the ASYNC_USE_SENTINEL macro to use or
// disable)
static void* sentinal_thread(void* async);

// signaling to finish
static void async_signal(async_p async);

// the destructor
static void async_destroy(async_p queue);

/******************************************************************************
Portability - used to help port this to different frameworks (i.e. Ruby).
*/

#define THREAD_TYPE pthread_t

#ifndef ASYNC_USE_SENTINEL
#define ASYNC_USE_SENTINEL 0
#endif

#ifndef ASYNC_TASK_POOL_SIZE
#define ASYNC_TASK_POOL_SIZE 170
#endif

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

/**
The Async struct
*/
struct Async {
  /** the task queue - MUST be first in the struct*/
  pthread_mutex_t lock;                  // a mutex for data integrity.
  async_task_ns* volatile tasks;         // active tasks
  async_task_ns* volatile _Atomic pool;  // a task node pool
  async_task_ns** volatile pos;          // the position for new tasks.
  /** The pipe used for thread wakeup */
  struct {
    /** read incoming data (opaque data), used for wakeup */
    int in;
    /** write opaque data (single byte), used for wakeup signaling */
    int out;
  } pipe;
  /** the number of initialized threads */
  int count;
  /** data flags */
  volatile struct {
    /** the running flag */
    unsigned run : 1;
    /** reserved for future use */
    unsigned rsrv : 7;
  } flags;
  /** the task pool */
  async_task_ns task_pool_mem[ASYNC_TASK_POOL_SIZE];
  /** the thread pool */
  THREAD_TYPE threads[];
};

/******************************************************************************
Task Management - add a task and perform al tasks in queue
*/

/**
Schedules a task to be performed by an Async thread pool group.

The Task should be a function such as `void task(void *arg)`.

Use:

  void task(void * arg) { printf("%s", arg); }

  char arg[] = "Demo Task";

  Async.run(async, task, arg);

*/
static int async_run(async_p async, void (*task)(void*), void* arg) {
  async_task_ns* c;  // c == container, this will store the task

  if (!async || !task)
    return -1;
  // get a container from the pool of grab a new container.
  if ((c = atomic_load(&async->pool))) {
    while (!atomic_compare_exchange_weak(&async->pool, &c, c->next))
      ;
  }
  if (c == NULL)
    c = malloc(sizeof(*c));
  if (!c) {
    return -1;
  }
  *c = (async_task_ns){.task.task = task, .task.arg = arg, .next = NULL};
  pthread_mutex_lock(&(async->lock));
  if (async->tasks) {
    *(async->pos) = c;
  } else {
    async->tasks = c;
  }
  async->pos = &(c->next);
  pthread_mutex_unlock(&async->lock);
  // wake up any sleeping threads
  // any activated threads will ask to require the mutex as soon as we write...
  // we need to unlock before we write, or we'll have excess context switches.
  if (write(async->pipe.out, c, 1))
    ;
  return 0;
}

/**
Performs all the existing tasks in the queue.
*/
static void perform_tasks(async_p async) {
  async_task_ns* c = NULL;  // c == container, this will store the task
  task_s task;
  do {
    // grab a task from the queue.
    pthread_mutex_lock(&(async->lock));
    c = async->tasks;
    if (c) {
      // move the queue forward.
      async->tasks = async->tasks->next;
      task = c->task;
    }
    pthread_mutex_unlock(&(async->lock));
    if (c) {
      task = c->task;
      // pool management
      if (c >= async->task_pool_mem &&
          c < async->task_pool_mem + ASYNC_TASK_POOL_SIZE) {
        // move the old task container to the pool.
        while (!atomic_compare_exchange_weak(&async->pool, &c->next, c))
          ;
      } else
        free(c);
      // perform the task
      task.task(task.arg);
    }
  } while (c);
}

/******************************************************************************
The worker threads.
*/

// on thread failure, a backtrace should be printed (if using sentinal)
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
          "Async: task in thread pool raised an error %d-%s (%d). Backtrace "
          "(%zd):\n",
          errno,
          sig == SIGSEGV ? "SIGSEGV" : sig == SIGFPE
                                           ? "SIGFPE"
                                           : sig == SIGILL
                                                 ? "SIGILL"
                                                 : sig == SIGPIPE ? "SIGPIPE"
                                                                  : "unknown",
          sig, size);
  for (i = 2; i < size; i++)
    fprintf(stderr, "%s\n", strings[i]);
  free(strings);
  fprintf(stderr, "\n");
  // pthread_exit(0); // for testing
  pthread_exit((void*)on_err_signal);
}

// The worker cycle
static void* worker_thread_cycle(void* _async) {
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
  signal(SIGCHLD, SIG_IGN);

  // setup signal and thread's local-storage async variable.
  struct Async* async = _async;
  char sig_buf;

  // pause for signal for as long as we're active.
  while (async->flags.run && (read(async->pipe.in, &sig_buf, 1) >= 0)) {
    perform_tasks(async);
    sched_yield();
  }

  perform_tasks(async);
  return 0;
}

// an optional sentinal
static void* sentinal_thread(void* _async) {
  async_p async = _async;
  THREAD_TYPE thr;
  while (async->flags.run && create_thread(&thr, worker_thread_cycle, async))
    join_thread(thr);
  return 0;
}

/******************************************************************************
Signaling and finishing up
*/

/**
Signals an Async object to finish up.

The threads in the thread pool will continue perfoming all the tasks in the
queue until the queue is empty. Once the queue is empty, the threads will
exit.
If new tasks are created after the signal, they will be added to the queue and
processed until the last thread is done. Once the last thread exists, future
tasks won't be processed.

Use:

  Async.signal(async);

*/
static void async_signal(async_p async) {
  async->flags.run = 0;
  // send `async->count` number of wakeup signales (data content is irrelevant)
  if (write(async->pipe.out, async, async->count))
    ;
}

/**
Waits for an Async object to finish up (joins all the threads in the thread
pool) and DESTROYS the Async object (frees it's memory and resources).

This function will wait forever or until a signal is received and all the
tasks in the queue have been processed.

Use:

  Async.wait(async);

*/
static void async_wait(async_p async) {
  if (!async)
    return;
  // wake threads (just in case) by sending `async->count` number of wakeups
  if (async->pipe.out && write(async->pipe.out, async, async->count))
    ;
  // join threads.
  for (size_t i = 0; i < async->count; i++) {
    join_thread(async->threads[i]);
  }
  // perform any pending tasks
  perform_tasks(async);
  // release queue memory and resources.
  async_destroy(async);
}

/**
Both signals for an Async object to finish up and waits for it to finish. This
is akin to calling both `signal` and `wait` in succession:

  Async.signal(async);
  Async.wait(async);

Use:

  Async.finish(async);

returns 0 on success and -1 of error.
*/
static void async_finish(async_p async) {
  async_signal(async);
  async_wait(async);
}

/******************************************************************************
Object creation and destruction
*/

/**
Destroys the Async object, releasing it's memory.
*/
static void async_destroy(async_p async) {
  pthread_mutex_lock(&async->lock);
  async_task_ns* to_free;
  async_task_ns* pos;
  async->pos = NULL;
  // free all tasks
  pos = async->tasks;
  while ((to_free = pos)) {
    pos = pos->next;
    if (to_free >= async->task_pool_mem &&
        to_free < async->task_pool_mem + ASYNC_TASK_POOL_SIZE) {
      //
    } else {
      free(to_free);
    }
  }
  async->tasks = NULL;
  // free task pool - not really needed
  pos = atomic_load(&async->pool);
  while ((to_free = pos)) {
    pos = pos->next;
    if (to_free >= async->task_pool_mem &&
        to_free < async->task_pool_mem + ASYNC_TASK_POOL_SIZE) {
      //
    } else {
      free(to_free);
    }
  }
  atomic_store(&async->pool, NULL);
  // close pipe
  if (async->pipe.in) {
    close(async->pipe.in);
    async->pipe.in = 0;
  }
  if (async->pipe.out) {
    close(async->pipe.out);
    async->pipe.out = 0;
  }
  pthread_mutex_unlock(&async->lock);
  pthread_mutex_destroy(&async->lock);
  free(async);
}

/**
Creates a new Async object (a thread pool) and returns a pointer using the
`aync_p` (Async Pointer) type.

Requires the number of new threads to be initialized. Use:

  async_p async = Async.create(8);

*/
static async_p async_new(int threads) {
  async_p async = malloc(sizeof(*async) + (threads * sizeof(THREAD_TYPE)));
  async->tasks = NULL;
  async->pool = NULL;
  async->pipe.in = 0;
  async->pipe.out = 0;
  if (pthread_mutex_init(&(async->lock), NULL)) {
    free(async);
    return NULL;
  };
  if (pipe((int*)&(async->pipe))) {
    free(async);
    return NULL;
  };
  fcntl(async->pipe.out, F_SETFL, O_NONBLOCK | O_WRONLY);
  *(char*)(&(async->flags)) = 0;
  async->flags.run = 1;
  // create threads
  for (async->count = 0; async->count < threads; async->count++) {
    if (create_thread(
            async->threads + async->count,
            (ASYNC_USE_SENTINEL ? sentinal_thread : worker_thread_cycle),
            async)) {
      // signal
      async_signal(async);
      // wait for threads and destroy object.
      async_wait(async);
      // return error
      return NULL;
    };
  }
  // initiaite a task pool
  for (size_t i = 0; i < ASYNC_TASK_POOL_SIZE - 1; i++) {
    async->task_pool_mem[i] =
        (async_task_ns){.next = async->task_pool_mem + i + 1};
  }
  async->task_pool_mem[ASYNC_TASK_POOL_SIZE - 1] =
      (async_task_ns){.next = NULL};
  atomic_store(&async->pool, async->task_pool_mem);
  return async;
}

/******************************************************************************
API gateway
*/

struct Async_API___ Async = {
    .create = async_new,
    .signal = async_signal,
    .wait = async_wait,
    .finish = async_finish,
    .run = async_run,
};
