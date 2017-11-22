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

/* child process reaping is enabled by default */
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

/** Starts a thread pool that will run deferred tasks in the background. */
pool_pt defer_pool_start(unsigned int thread_count);
/** Signals a running thread pool to stop. Returns immediately. */
void defer_pool_stop(pool_pt pool);
/** Waits for a running thread pool, joining threads and finishing all tasks. */
void defer_pool_wait(pool_pt pool);
/** Returns TRUE (1) if the pool is hadn't been signaled to finish up. */
int defer_pool_is_active(pool_pt pool);

/**
OVERRIDE THIS to replace the default pthread implementation.

Accepts a pointer to a function and a single argument that should be executed
within a new thread.

The function should allocate memory for the thread object and return a pointer
to the allocated memory that identifies the thread.

On error NULL should be returned.
*/
void *defer_new_thread(void *(*thread_func)(void *), pool_pt pool);

/**
OVERRIDE THIS to replace the default pthread implementation.

Accepts a pointer returned from `defer_new_thread` (should also free any
allocated memory) and joins the associated thread.

Return value is ignored.
*/
int defer_join_thread(void *p_thr);

/**
OVERRIDE THIS to replace the default pthread implementation.

Throttles or reschedules the current running thread. Default implementation
simply micro-sleeps.
*/
void defer_thread_throttle(unsigned long microsec);

/* *****************************************************************************
Child Process support (`fork`)
***************************************************************************** */

/**
OVERRIDE THIS to replace the default `fork` implementation or to inject hooks
into the forking function.

Behaves like the system's `fork`.
*/
int defer_new_child(void);

/**
 * Forks the process, starts up a thread pool and waits for all tasks to run.
 * All existing tasks will run in all processes (multiple times).
 *
 * It's possible to synchronize workload across processes by using a pipe (or
 * pipes) and a self-scheduling event that reads instructions from the pipe.
 *
 * This function will use SIGINT or SIGTERM to signal all the children processes
 * to finish up and exit. It will also setup a child process reaper (which will
 * remain active for the application's lifetime).
 *
 * Returns 0 on success, -1 on error and a positive number if this is a child
 * process that was forked.
 */
int defer_perform_in_fork(unsigned int process_count,
                          unsigned int thread_count);
/** Returns TRUE (1) if the forked thread pool hadn't been signaled to finish
 * up. */
int defer_fork_is_active(void);
/** Returns the process number for the current working proceess. 0 == parent. */
int defer_fork_pid(void);

#ifdef DEBUG
/** minor testing facilities */
void defer_test(void);
#endif

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif
