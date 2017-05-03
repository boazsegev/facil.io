#include "defer.h"

/* *****************************************************************************
Compile time settings
***************************************************************************** */

#ifndef DEFER_QUEUE_BUFFER
#define DEFER_QUEUE_BUFFER 1024
#endif

/* *****************************************************************************
spinlock / sync for tasks
***************************************************************************** */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#define _GNU_SOURCE
#include <time.h>
#endif /* _GNU_SOURCE */

#include <stdlib.h>

/** locks use a single byte */
typedef volatile unsigned char spn_lock_i;

/** The initail value of an unlocked spinlock. */
#define SPN_LOCK_INIT 0

/*********
 * manage the way threads "wait" for the lock to release
 */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
/* nanosleep seems to be the most effective and efficient reschedule */
#define reschedule_thread()                                                    \
  {                                                                            \
    static const struct timespec tm = {.tv_nsec = 1};                          \
    nanosleep(&tm, NULL);                                                      \
  }
#define throttle_thread()                                                      \
  {                                                                            \
    static const struct timespec tm = {.tv_nsec = 8388608UL};                  \
    nanosleep(&tm, NULL);                                                      \
  }

#else /* no effective rescheduling, just spin... */
#define reschedule_thread()
#define throttle_thread()
#endif

#if !__clang__ && (__GNUC__ > 3)
#define SPN_LOCK_BUILTIN(...) __sync_fetch_and_or(__VA_ARGS__)
#elif !defined(__has_builtin)
#error Required feature check "__has_builtin" missing from this compiler.
#else
#if __has_builtin(__sync_swap)
#define SPN_LOCK_BUILTIN(...) __sync_swap(__VA_ARGS__)
#elif __has_builtin(__sync_fetch_and_or)
#define SPN_LOCK_BUILTIN(...) __sync_fetch_and_or(__VA_ARGS__)
#else
#error Required builtin "__sync_swap" or "__sync_fetch_and_or" missing from compiler.
#endif
#endif

/** returns 1 and 0 if the lock was successfully aquired (TRUE == FAIL). */
static inline int spn_trylock(spn_lock_i *lock) {
  return SPN_LOCK_BUILTIN(lock, 1);
}

/** Releases a lock. */
static inline __attribute__((unused)) void spn_unlock(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  *lock = 0;
}
/** returns a lock's state (non 0 == Busy). */
static inline __attribute__((unused)) int spn_is_locked(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  return *lock;
}
/** Busy waits for the lock. */
static inline __attribute__((unused)) void spn_lock(spn_lock_i *lock) {
  while (spn_trylock(lock)) {
    reschedule_thread();
  }
}

/* *****************************************************************************
Data Structures
***************************************************************************** */

typedef struct {
  void (*func)(void *);
  void *arg;
} task_s;

typedef struct task_node_s {
  task_s task;
  struct task_node_s *next;
} task_node_s;

static task_node_s tasks_buffer[DEFER_QUEUE_BUFFER];

static struct {
  task_node_s *first;
  task_node_s **last;
  task_node_s *pool;
  spn_lock_i lock;
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
int defer(void (*func)(void *), void *arg) {
  if (!func)
    goto call_error;
  task_node_s *task;
  spn_lock(&deferred.lock);
  if (deferred.pool) {
    task = deferred.pool;
    deferred.pool = deferred.pool->next;
  } else if (deferred.initialized) {
    task = malloc(sizeof(task_node_s));
    if (!task)
      goto error;
  } else {
    deferred.initialized = 1;
    task = tasks_buffer;
    deferred.pool = tasks_buffer + 1;
    for (size_t i = 2; i < DEFER_QUEUE_BUFFER; i++) {
      tasks_buffer[i - 1].next = tasks_buffer + i;
    }
  }
  *deferred.last = task;
  deferred.last = &task->next;
  task->task.func = func;
  task->task.arg = arg;
  task->next = NULL;
  spn_unlock(&deferred.lock);
  return 0;
error:
  spn_unlock(&deferred.lock);
call_error:
  return -1;
}

/** Performs all deferred functions until the queue had been depleted. */
void defer_perform(void) {
  task_node_s *tmp;
  task_s task;
restart:
  spn_lock(&deferred.lock);
  tmp = deferred.first;
  if (tmp) {
    deferred.first = tmp->next;
    if (!deferred.first)
      deferred.last = &deferred.first;
    task = tmp->task;
    if (tmp <= tasks_buffer + (DEFER_QUEUE_BUFFER - 1) && tmp >= tasks_buffer) {
      tmp->next = deferred.pool;
      deferred.pool = tmp;
    } else {
      free(tmp);
    }
    spn_unlock(&deferred.lock);
    task.func(task.arg);
    goto restart;
  } else
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

#pragma weak defer_new_thread
void *defer_new_thread(void *(*thread_func)(void *), void *arg) {
  pthread_t *thread = malloc(sizeof(*thread));
  if (thread == NULL || pthread_create(thread, NULL, thread_func, arg))
    goto error;
  return thread;
error:
  free(thread);
  return NULL;
}

#pragma weak defer_new_thread
int defer_join_thread(void *p_thr) {
  if (!p_thr)
    return -1;
  pthread_join(*(pthread_t *)p_thr, NULL);
  free(p_thr);
  return 0;
}

#else /* No pthreads... BYO thread implementation. */

#pragma weak defer_new_thread
void *defer_new_thread(void *(*thread_func)(void *), void *arg) {
  (void)thread_func;
  (void)arg;
  return NULL;
}
#pragma weak defer_new_thread
int defer_join_thread(void *p_thr) {
  (void)p_thr;
  return -1;
}

#endif /* DEBUG || pthread default */

struct defer_pool {
  unsigned int flag;
  unsigned int count;
  void *threads[];
};

#include <stdio.h>
static void *defer_worker_thread(void *pool) {
  do {
    throttle_thread();
    defer_perform();
  } while (((pool_pt)pool)->flag);
  return NULL;
}

void defer_pool_stop(pool_pt pool) { pool->flag = 0; }

void defer_pool_wait(pool_pt pool) {
  while (pool->count) {
    pool->count--;
    defer_join_thread(pool->threads[pool->count]);
  }
}

pool_pt defer_pool_start(unsigned int thread_count) {
  if (thread_count == 0)
    return NULL;
  pool_pt pool = malloc(sizeof(*pool) + (thread_count * sizeof(void *)));
  if (!pool)
    return NULL;
  pool->flag = 1;
  pool->count = 0;
  while (pool->count < thread_count &&
         (pool->threads[pool->count] =
              defer_new_thread(defer_worker_thread, pool)))
    pool->count++;
  if (pool->count == thread_count)
    return pool;
  defer_pool_stop(pool);
  return NULL;
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

static void sample_task(void *unused) {
  (void)(unused);
  spn_lock(&i_lock);
  i_count++;
  spn_unlock(&i_lock);
}

static void sched_sample_task(void *unused) {
  (void)(unused);
  for (size_t i = 0; i < 1024; i++) {
    defer(sample_task, NULL);
  }
}

static void thrd_sched(void *unused) {
  for (size_t i = 0; i < (1024 / DEFER_TEST_THREAD_COUNT); i++) {
    sched_sample_task(unused);
  }
}

static void text_task_text(void *unused) {
  (void)(unused);
  spn_lock(&i_lock);
  fprintf(stderr, "this text should print before defer_perform returns\n");
  spn_unlock(&i_lock);
}

static void text_task(void *_) {
  static const struct timespec tm = {.tv_sec = 2};
  nanosleep(&tm, NULL);
  defer(text_task_text, _);
}

void defer_test(void) {
  spn_lock(&i_lock);
  i_count = 0;
  spn_unlock(&i_lock);
  time_t start, end;
  fprintf(stderr, "Starting defer testing\n");

  start = clock();
  for (size_t i = 0; i < 1024; i++) {
    defer(sched_sample_task, NULL);
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
      defer(thrd_sched, NULL);
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

  fprintf(stderr, "calling defer_perform.\n");
  defer(text_task, NULL);
  defer_perform();
  fprintf(stderr, "defer_perform returned. i_count = %lu\n", i_count);
}

#endif
