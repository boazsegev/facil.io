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
#define DEFER_THROTTLE 1048574UL
#endif
#ifndef DEFER_THROTTLE_LIMIT
#define DEFER_THROTTLE_LIMIT 2097148UL
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
  unsigned char state;
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
  task_s ret = (task_s){.func = NULL};
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

void defer_on_fork(void) { deferred.lock = SPN_LOCK_INIT; }

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
  defer_thread_signal();
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

/* thread pool data container */
struct defer_pool {
  volatile unsigned int flag;
  unsigned int count;
  struct thread_msg_s {
    pool_pt pool;
    void *thrd;
  } threads[];
};

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

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory asociated with a thread indentifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
#pragma weak defer_free_thread
void defer_free_thread(void *p_thr) {
  pthread_detach(*((pthread_t *)p_thr));
  free(p_thr);
}

#pragma weak defer_join_thread
int defer_join_thread(void *p_thr) {
  if (!p_thr)
    return -1;
  pthread_join(*((pthread_t *)p_thr), NULL);
  defer_free_thread(p_thr);
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

#pragma weak defer_free_thread
void defer_free_thread(void *p_thr) { void(p_thr); }

#pragma weak defer_join_thread
int defer_join_thread(void *p_thr) {
  (void)p_thr;
  return -1;
}

#pragma weak defer_thread_throttle
void defer_thread_throttle(unsigned long microsec) { return; }

#endif /* DEBUG || pthread default */

/**
 * A thread entering this function should wait for new evennts.
 */
#pragma weak defer_thread_wait
void defer_thread_wait(pool_pt pool, void *p_thr) {
  size_t throttle = (pool->count) * DEFER_THROTTLE;
  if (!throttle || throttle > DEFER_THROTTLE_LIMIT)
    throttle = DEFER_THROTTLE_LIMIT;
  if (throttle == DEFER_THROTTLE)
    throttle <<= 1;
  throttle_thread(throttle);
  (void)p_thr;
}

/**
 * This should signal a single waiting thread to wake up (a new task entered the
 * queue).
 */
#pragma weak defer_thread_signal
void defer_thread_signal(void) { (void)0; }

/* a thread's cycle. This is what a worker thread does... repeatedly. */
static void *defer_worker_thread(void *pool_) {
  struct thread_msg_s volatile *data = pool_;
  signal(SIGPIPE, SIG_IGN);
  /* perform any available tasks */
  defer_perform();
  /* as long as the flag is true, wait for and perform tasks. */
  do {
    defer_thread_wait(data->pool, data->thrd);
    defer_perform();
  } while (data->pool->flag);
  return NULL;
}

/** Signals a running thread pool to stop. Returns immediately. */
void defer_pool_stop(pool_pt pool) {
  pool->flag = 0;
  for (size_t i = 0; i < pool->count; ++i) {
    defer_thread_signal();
  }
}

/** Returns TRUE (1) if the pool is hadn't been signaled to finish up. */
int defer_pool_is_active(pool_pt pool) { return (int)pool->flag; }

/**
 * Waits for a running thread pool, joining threads and finishing all tasks.
 *
 * This function MUST be called in order to free the pool's data (the
 * `pool_pt`).
 */
void defer_pool_wait(pool_pt pool) {
  while (pool->count) {
    pool->count--;
    defer_join_thread(pool->threads[pool->count].thrd);
  }
  free(pool);
}

/** The logic behind `defer_pool_start`. */
static inline pool_pt defer_pool_initialize(unsigned int thread_count,
                                            pool_pt pool) {
  pool->flag = 1;
  pool->count = 0;
  while (pool->count < thread_count &&
         (pool->threads[pool->count].pool = pool) &&
         (pool->threads[pool->count].thrd = defer_new_thread(
              defer_worker_thread, (pool_pt)(pool->threads + pool->count))))

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
  pool_pt pool =
      malloc(sizeof(*pool) + (thread_count * sizeof(*pool->threads)));
  if (!pool)
    return NULL;

  return defer_pool_initialize(thread_count, pool);
}

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

void defer_test(void) {
  clock_t start, end;
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
          (unsigned long)(end - start), (unsigned long)i_count,
          (unsigned long)count_dealloc, (unsigned long)count_alloc);

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
          (unsigned long)(end - start), (unsigned long)i_count,
          (unsigned long)count_dealloc, (unsigned long)count_alloc);

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
          (unsigned long)(end - start), (unsigned long)i_count,
          (unsigned long)count_dealloc, (unsigned long)count_alloc);

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
            DEFER_TEST_THREAD_COUNT, (unsigned long)(end - start),
            (unsigned long)i_count, (unsigned long)count_dealloc,
            (unsigned long)count_alloc);
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
          (unsigned long)(end - start), (unsigned long)i_count,
          (unsigned long)count_dealloc, (unsigned long)count_alloc);

  fprintf(stderr, "calling defer_perform.\n");
  defer(text_task, NULL, NULL);
  defer_perform();
  fprintf(stderr,
          "defer_perform returned. i_count = %lu, %lu/%lu free/malloc\n",
          (unsigned long)i_count, (unsigned long)count_dealloc,
          (unsigned long)count_alloc);
  defer_clear_queue();
  fprintf(stderr, "* Defer cleared queue: %lu/%lu free/malloc\n",
          (unsigned long)count_dealloc, (unsigned long)count_alloc);
}

#endif
