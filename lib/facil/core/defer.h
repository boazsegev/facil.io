/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_DEFER_H
/**
A library for deferring execution of code.

Deferred execution can be multi-threaded, although threads aren't managed by the
library.

All deferred execution is shared among the same process and inherited by any
forked process.

The defer library could produce a single page "memory leak", since the last task
buffer is nenver freed (it's left "on call" for new events). To avoid this leak
call `defer_clear_queue` before exiting the program.
*/
#define H_DEFER_H
#define LIB_DEFER_VERSION_MAJOR 0
#define LIB_DEFER_VERSION_MINOR 1
#define LIB_DEFER_VERSION_PATCH 2

/* child process reaping is can be enabled as a default */
#ifndef NO_CHILD_REAPER
#define NO_CHILD_REAPER 0
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* *****************************************************************************
Core API
***************************************************************************** */

/** Defer an execution of a function for later. Returns -1 on error.*/
int defer(void (*func)(void *, void *), void *arg1, void *arg2);

/** Performs all deferred functions until the queue had been depleted. */
void defer_perform(void);

/** returns true if there are deferred functions waiting for execution. */
int defer_has_queue(void);

/** Clears the queue without performing any of the tasks. */
void defer_clear_queue(void);

/* *****************************************************************************
Thread Pool support
***************************************************************************** */

/** an opaque thread pool type */
typedef struct defer_pool *pool_pt;

/**
 * Starts a thread pool that will run deferred tasks in the background.
 *
 * The `defer_idle` callback will be used to handle waiting threads. It can be
 * used to sleep, or run background tasks. It will run concurrently and
 * continuesly for all the threads in the pool that are idling.
 *
 * If `defer_idle` is NULL, a fallback to `nanosleep` will be used.
 */
pool_pt defer_pool_start(unsigned int thread_count);

/** Signals a running thread pool to stop. Returns immediately. */
void defer_pool_stop(pool_pt pool);

/**
 * Waits for a running thread pool, joining threads and finishing all tasks.
 *
 * This function MUST be called in order to free the pool's data (the
 * `pool_pt`).
 */
void defer_pool_wait(pool_pt pool);

/** Returns TRUE (1) if the pool is hadn't been signaled to finish up. */
int defer_pool_is_active(pool_pt pool);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer to a function and a single argument that should be executed
 * within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
void *defer_new_thread(void *(*thread_func)(void *), pool_pt pool);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory asociated with a thread indentifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
void defer_free_thread(void *p_thr);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `defer_new_thread` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
int defer_join_thread(void *p_thr);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Throttles or reschedules the current running thread. Default implementation
 * simply micro-sleeps.
 */
void defer_thread_throttle(unsigned long microsec);

/**
 * OVERRIDE THIS to replace the default nanosleep implementation.
 *
 * A thread entering this function should wait for new evennts.
 */
void defer_thread_wait(pool_pt pool, void *p_thr);

/**
 * OVERRIDE THIS to replace the default implementation (which does nothing).
 *
 * This should signal a single waiting thread to wake up (a new task entered the
 * queue).
 */
void defer_thread_signal(void);

/** Call this function after forking, to make sure no locks are engaged. */
void defer_on_fork(void);

#ifdef DEBUG
/** minor testing facilities */
void defer_test(void);
#endif

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif
