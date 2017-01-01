/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_ASYNC
#define LIB_ASYNC "0.4.0"
#define LIB_ASYNC_VERSION_MAJOR 0
#define LIB_ASYNC_VERSION_MINOR 4
#define LIB_ASYNC_VERSION_PATCH 0

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
// prints out testing and benchmarking data
void async_test_library_speed(void);
#endif

/** \file
This is an easy to use **global** thread pool library. Once the thread pool was
initiated it makes running tasks very simple. i.e.:

    async_start(4); // 4 worker threads
    async_run(task, arg);
    async_finish(); // waits for tasks and releases threads.

Please note, this library isn't fork-friendly - fork **before** you create the
thread pool. In general, mixing `fork` with multi-threading isn't safe nor
trivial - always fork before multi-threading.
*/

/**
Starts running the global thread pool. Use:

  async_start(8, 0);

The `use_sentinal` variable protects threads from crashing and produces error
reports. It isn't effective against all errors, but it should protect against
some.

*/
ssize_t async_start(size_t threads);

/**
Use `async_join` instead.

Performs tasks until the task queue is empty. An empty task queue does NOT mean
all the tasks have completed, since some other threads might be running tasks
that have yet to complete (and these tasks might schedule new tasks as well).

Use:

  async_perform();

*/
void async_perform();

/**
Waits for all the present tasks to complete and threads to exist.

The function won't return unless `async_signal` is called to signal the threads
to stop waiting for new tasks.

After this function returns, the thread pool will remain active, waiting for new
tasts.

Unline finish (that uses `join`) this is an **active** wait where the waiting
thread acts as a working thread and performs any pending tasks and then
spinlocks until the active thread count drops to 0.

Use:

  async_join();

*/
void async_join();

/**
Signals all the threads to finish up and stop waiting for new tasks.

Use:

  async_join();

*/
void async_signal();

/**
Schedules a task to be performed by the thread pool.

The Task should be a function such as `void task(void *arg)`.

Use:

  void task(void * arg) { printf("%s", arg); }

  char arg[] = "Demo Task";

  async_run(task, arg);

*/
int async_run(void (*task)(void *), void *arg);

/**
Same as:

`async_signal(); async_wait();`
*/
#define async_finish()                                                         \
  {                                                                            \
    async_signal();                                                            \
    async_join();                                                              \
  }

#endif /* LIB_ASYNC */
