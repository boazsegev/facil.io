#ifndef H_DEFER_H
/**
A library for deferring execution of code.

Deferred execution can be multi-threaded, although threads aren't managed by the
library.

All deferred execution is shared among the same process and inherited by any
forked process.
*/
#define H_DEFER_H

/* *****************************************************************************
Core API
***************************************************************************** */

/** Defer an execution of a function for later. Returns -1 on error.*/
int defer(void (*func)(void *), void *arg);

/** Performs all deferred functions until the queue had been depleted. */
void defer_perform(void);

/** returns true if there are deferred functions waiting for execution. */
int defer_has_queue(void);

/* *****************************************************************************
Thread Pool support
***************************************************************************** */

/** an opaque thread pool type */
typedef struct defer_pool *pool_pt;

/** Starts a thread pool that will run deferred tasks in the background. */
pool_pt defer_pool_start(unsigned int thread_count);

/** Signals a running thread pool to stop. Returns immediately. */
void defer_pool_signal(pool_pt pool);
/** Waits for a running thread pool, joining threads and finishing all tasks. */
void defer_pool_wait(pool_pt pool);

/**
OVERRIDE THIS to replace the default pthread implementation.

Accepts a pointer to a function and a single argument that should be executed
within a new thread.

The function should allocate memory for the thread object and return a pointer
to the allocated memory that identifies the thread.

On error NULL should be returned.
*/
void *defer_new_thread(void *(*thread_func)(void *), void *arg);

/**
OVERRIDE THIS to replace the default pthread implementation.

Accepts a pointer returned from `defer_new_thread` (should also free any
allocated memory) and joins the associated thread.

Return value is ignored.
*/
int defer_join_thread(void *p_thr);

#endif
