/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "libasync.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <fcntl.h>

////////////////////
// types used

struct Async {
  /// an array to `pthread_t` objects `count` long.
  pthread_t* thread_pool;
  /// the number of threads in the array.
  int count;
  /// The read only part of the pipe used to push tasks.
  int in;
  /// The write only part of the pipe used to push tasks.
  int out;
  /// a callback used whenever a new thread a spawned.
  void (*init_thread)(struct Async*, void*);
  /// a pointer for the callback.
  void* arg;
};

// A task structure.
struct Task {
  void (*task)(void*);
  void* arg;
};

/////////////////////
// the thread loop functions

// manage thread error signals
static void on_signal(int sig) {
  void* array[22];
  size_t size;
  char** strings;
  size_t i;
  size = backtrace(array, 22);
  strings = backtrace_symbols(array, size);
  perror("\nERROR");
  fprintf(
      stderr,
      "Async: task in thread pool raised an error (%d-%s). Backtrace (%zd):\n",
      errno, sig == SIGSEGV
                 ? "SIGSEGV"
                 : sig == SIGFPE ? "SIGFPE" : sig == SIGILL
                                                  ? "SIGILL"
                                                  : sig == SIGPIPE ? "SIGPIPE"
                                                                   : "unknown",
      size);
  for (i = 2; i < size; i++)
    fprintf(stderr, "%s\n", strings[i]);
  free(strings);
  fprintf(stderr, "\n");
  // pthread_exit(0); // for testing
  pthread_exit((void*)on_signal);
}

// the main thread loop function
static void* thread_loop(struct Async* async) {
  struct Task task = {};
  signal(SIGSEGV, on_signal);
  signal(SIGFPE, on_signal);
  signal(SIGILL, on_signal);
  signal(SIGPIPE, SIG_IGN);
#ifdef SIGBUS
  signal(SIGBUS, on_signal);
#endif
#ifdef SIGSYS
  signal(SIGSYS, on_signal);
#endif
#ifdef SIGXFSZ
  signal(SIGXFSZ, on_signal);
#endif
  if (async->init_thread)
    async->init_thread(async, async->arg);
  int in = async->in;  // no fear of async from being freed before...
  while (read(in, &task, sizeof(struct Task)) > 0) {
    if (!task.task)
      break;
    task.task(task.arg);
  }
  close(in);
  return 0;
}

// the thread's "watch-dog" / sentinal
static void* thread_sentinal(struct Async* async) {
  // do we need a sentinal that will reinitiate the thread if a task causes it
  // to fail?
  // signal(int, void (*)(int))
  pthread_t active_thread;
  void* thread_error = (void*)on_signal;
  int in = async->in;
  while (thread_error && (fcntl(in, F_GETFL) >= 0)) {
    pthread_create(&active_thread, NULL, (void* (*)(void*))thread_loop, async);
    pthread_join(active_thread, &thread_error);
    if (thread_error) {
      perror("Async: thread sentinal reinitiating worker thread");
    }
  }
  return 0;
}

/////////////////////
// the functions

// creates a new aync object
static struct Async* async_new(int threads,
                               void (*on_init)(struct Async* self, void* arg),
                               void* arg) {
  if (threads <= 0)
    return NULL;
  // create the tasking pipe.
  int io[2];
  if (pipe(io))
    return NULL;

  // allocate the memory
  size_t memory_required = sizeof(struct Async) + (sizeof(pthread_t) * threads);
  struct Async* async = malloc(memory_required);
  if (!async) {
    close(io[0]);
    close(io[1]);
    return NULL;
  }
  // setup the struct data
  async->count = threads;
  async->init_thread = on_init;
  async->thread_pool = (void*)(async + 1);
  async->in = io[0];
  async->out = io[1];
  async->arg = arg;
  // create the thread pool
  for (int i = 0; i < threads; i++) {
    if ((pthread_create(async->thread_pool + i, NULL,
                        (void* (*)(void*))thread_sentinal, async)) < 0) {
      for (int j = 0; j < i; j++) {
        pthread_cancel(async->thread_pool[j]);
      }
      close(io[0]);
      close(io[1]);
      free(async);
      return NULL;
    }
  }
  // return the pointer
  return async;
}
static int async_run(struct Async* self, void (*task)(void*), void* arg) {
  if (!(task && self))
    return -1;
  struct Task package = {.task = task, .arg = arg};
  return write(self->out, &package, sizeof(struct Task));
}
static void async_finish(struct Async* self) {
  struct Task package = {.task = 0, .arg = 0};
  if (write(self->out, &package, sizeof(struct Task)))
    ;
  for (int i = 0; i < self->count; i++) {
    pthread_join(self->thread_pool[i], NULL);
  }
  close(self->in);
  close(self->out);
  free(self);
}
static void async_kill(struct Async* self) {
  struct Task package = {.task = 0, .arg = 0};
  if (write(self->out, &package, sizeof(struct Task)))
    ;
  for (int i = 0; i < self->count; i++) {
    pthread_cancel(self->thread_pool[i]);
  }
  close(self->in);
  close(self->out);
  free(self);
}

////////////
// API gateway

// the API gateway
const struct AsyncAPI Async = {.new = async_new,
                               .run = async_run,
                               .finish = async_finish,
                               .kill = async_kill};
