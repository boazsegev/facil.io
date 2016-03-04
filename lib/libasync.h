/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_ASYNC_2_H
#define LIB_ASYNC_2_H

#define LIB_ASYNC_VERSION "0.2.0"

typedef struct Async* async_p;

extern struct __ASYNC_API__ {
  /**
Creates a new Async object (a thread pool) and returns a pointer using the
`aync_p` (Async Pointer) type.

Requires the number of new threads to be initialized. Use:

    async_p async = Async.new(8);

  */
  async_p (*new)(int threads);

  /**
Signals an Async object to finish up.

The threads in the thread pool will continue perfoming all the tasks in the
queue until the queue is empty. Once the queue is empty, the threads will exit.
If new tasks are created after the signal, they will be added to the queue and
processed until the last thread is done. Once the last thread exists, future
tasks won't be processed.

Use:

    Async.signal(async);

  */
  void (*signal)(async_p);

  /**
Waits for an Async object to finish up (joins all the threads in the thread
pool).

This function will wait forever or until a signal is received and all the tasks
in the queue have been processed.

Use:

    Async.wait(async);

  */
  void (*wait)(async_p);

  /**
Schedules a task to be performed by an Async thread pool group.

The Task should be a function such as `void task(void *arg)`.

Use:

    void task(void * arg) { printf("%s", arg); }

    char arg[] = "Demo Task";

    Async.run(async, task, arg);

  */
  int (*run)(async_p async, void (*task)(void*), void* arg);

  /**
Both signals for an Async object to finish up and waits for it to finish. This
is akin to calling both `signal` and `wait` in succession:

    Async.signal(async);
    Async.wait(async);

Use:

    Async.finish(async);

  */
  void (*finish)(async_p);
} Async;

#endif
