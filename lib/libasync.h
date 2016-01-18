/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIBASYNC_H
#define LIBASYNC_H

#define LIBASYNC_VERSION 0.1.0

// defines the type async_p (async pointer).
// the structure is defined to contain the thread pool's internal data as well
// as provide type safety.
typedef struct Async* async_p;

// The Async global variable contains the public API offered by the library.
extern const struct AsyncAPI {
  // the Async.new(threads, on_init_thread) creates a thread pool and
  // returns a pointer to a new Async struct object.
  //
  // **threads** (int) is the number of worker threads to be spawned.
  //
  // **on_thread_init** (func)(async_p, void*) is a callback that each new
  // working thread will call when it is first initialized.
  //
  // **arg** (void *) a pointer with datat that will be passed tp the init
  // callback.
  //
  // Forking the thread pool results in undefined behavior (create after `fork`)
  // although it's fairly likely that only the main process will handle tasks.
  // This is due to the fact that the thread pool uses pipes instead of mutexes.
  //
  // More than one pool can be created.
  //
  // Threads that crash are re-spawned by a "watch-dog" (sentinal) thread.
  //
  // returns NULL on failure.
  async_p (*new)(int threads,
                 void (*on_thread_init)(async_p async, void* arg),
                 void* arg);
  // Asyn.run(async, task, arg) sends tasks to the asynchronous event queue.
  int (*run)(async_p self, void (*task)(void*), void* arg);
  // Async.finish(async) will gracefully close down the async object,
  // completing any scheduled tasks and freeing any related resources.
  //
  // This function will wait for all scheduled tasks to complete before it
  // returns.
  void (*finish)(async_p self);
  // Async.kill(async) will destroy the async object, freeing all memory and
  // destroying the queue. Some background tasks might run to completion, but
  // not all.
  void (*kill)(async_p self);
} Async;

#endif /* end of include guard: LIBASYNC_H */
