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
  /// The read only part of the pipe used to push tasks.
  int in;
  /// The write only part of the pipe used to push tasks.
  int out;
  /// the number of threads in the array.
  int count;
  /// a callback used whenever a new thread a spawned.
  void (*init_thread)(struct Async*, void*);
  /// a pointer for the callback.
  void* arg;
  /// an object mutex (locker).
  pthread_mutex_t locker;
  /// an array to `pthread_t` objects `count` long.
  pthread_t thread_pool[];
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
  int in = async->in;    // keep a copy of the pipe's address on the stack
  int out = async->out;  // keep a copy of the pipe's address on the stack
  while (read(in, &task, sizeof(struct Task)) > 0) {
    if (!task.task) {
      close(in);
      close(out);
      break;
    }
    task.task(task.arg);
  }
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
// a single task performance, for busy waiting
static int perform_single_task(async_p async) {
  fprintf(stderr,
          "Warning: event queue overloaded!\n"
          "Perfoming out of band tasks, failure could occure.\n"
          "Consider adding process workers of threads for concurrency.\n"
          "\n");
  struct Task task = {};
  if (read(async->in, &task, sizeof(struct Task)) > 0) {
    if (!task.task) {
      close(async->in);
      close(async->out);
      return 0;
    }
    pthread_mutex_unlock(&async->locker);
    task.task(task.arg);
    pthread_mutex_lock(&async->locker);
    return 0;
  } else
    return -1;
}

/////////////////////
// Queue pipe extension management
struct ExtQueueData {
  struct {
    int in;
    int out;
  } io;
  async_p async;
};
static void* extended_queue_thread(void* _data) {
  struct ExtQueueData* data = _data;
  struct Task task;
  int i;
  // make sure to ignore broken pipes
  signal(SIGPIPE, SIG_IGN);
  // get the core out pipe flags (blocking state not important)
  i = fcntl(data->async->out, F_GETFL, NULL);
  // change the original queue writer object to a blocking state
  fcntl(data->io.out, F_SETFL, i & (~O_NONBLOCK));
  // make sure the reader doesn't block
  fcntl(data->io.in, F_SETFL, (O_NONBLOCK));
  while (1) {
    // we're checking the status of our queue, so we don't want stuff to be
    // added while we review.
    pthread_mutex_lock(&data->async->locker);
    i = read(data->io.in, &task, sizeof(struct Task));
    if (i <= 0) {
      // we're done, return the async object to it's previous status
      i = data->async->out;
      data->async->out = data->io.out;
      data->io.out = i;
      // get the core pipe flags (blocking state not important)
      i = fcntl(data->io.out, F_GETFL, NULL);
      // return the writer to a non-blocking state.
      fcntl(data->async->out, F_SETFL, i | O_NONBLOCK);
      // unlock the queue
      pthread_mutex_unlock(&data->async->locker);
      // close the extra pipes
      close(data->io.in);
      close(data->io.out);
      // free the data object
      free(data);
      return 0;
    }
    // unlock the queue - let it be filled.
    pthread_mutex_unlock(&data->async->locker);
    // write to original queue in a blocking manner.
    if (write(data->io.out, &task, sizeof(struct Task)) <= 0) {
      // there was an error while writing - it could be that we're shutting
      // down.
      // close the extra pipes (no need to swap, as all pipes are broken anyway)
      close(data->io.in);
      close(data->io.out);
      free(data);
      return (void*)-1;
    };
  }
}

static int extend_queue(async_p async, struct Task* task) {
  // create the data carrier
  struct ExtQueueData* data = malloc(sizeof(struct ExtQueueData));
  if (!data)
    return -1;
  // create the extra pipes
  if (pipe(&data->io.in)) {
    free(data);
    return -1;
  };
  // set the data's async pointer
  data->async = async;
  // get the core out pipe flags (blocking state not important)
  int flags = fcntl(async->out, F_GETFL, NULL);
  // make the new task writer object non-blocking
  fcntl(data->io.out, F_SETFL, flags | O_NONBLOCK);
  // swap the two writers
  async->out = async->out + data->io.out;
  data->io.out = async->out - data->io.out;
  async->out = async->out - data->io.out;
  // write the task to the new queue - otherwise, our thread might quit before
  // it starts.
  if (write(async->out, task, sizeof(struct Task)) <= 0) {
    // failed to write the new task... can this happen?

    // swap the two writers back
    async->out = async->out + data->io.out;
    data->io.out = async->out - data->io.out;
    async->out = async->out - data->io.out;
    // close
    close(data->io.in);
    close(data->io.out);
    // free
    free(data);
    // inform about the error
    return -1;
  }
  // create a thread that listens to the new queue and pushes it to the old
  // queue
  pthread_t thr;
  if (pthread_create(&thr, NULL, extended_queue_thread, data)) {
    // // damn, we were so close to finish... but got an error
    // swap the two writers back
    async->out = async->out + data->io.out;
    data->io.out = async->out - data->io.out;
    async->out = async->out - data->io.out;
    // close
    close(data->io.in);
    close(data->io.out);
    // free
    free(data);
    // inform about the error
    return -1;
  };
  return 0;
}

/////////////////////
// the functions

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

  // create the mutex
  if (pthread_mutex_init(&async->locker, NULL)) {
    close(io[0]);
    close(io[1]);
    free(async);
    return NULL;
  };

  // setup the struct data
  async->count = threads;
  async->init_thread = on_init;
  async->in = io[0];
  async->out = io[1];
  async->arg = arg;

  // make sure write isn't blocking, otherwise we might deadlock.
  int flags = fcntl(async->out, F_GETFL, NULL);
  fcntl(async->out, F_SETFL, flags | O_NONBLOCK);
  // disable SIGPIPE isn't required, as the main thread isn't likely to make
  // this mistake... but still... it might...
  signal(SIGPIPE, SIG_IGN);

  // create the thread pool - CHANGE this to re move sentinal thread
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
  int written = 0;
  // "busy" wait for the task buffer to complete tasks by performing tasks in
  // the buffer
  pthread_mutex_lock(&self->locker);
  while ((written = write(self->out, &package, sizeof(struct Task))) !=
         sizeof(struct Task)) {
    if (written > 0) {
      // this is fatal to the Async engine, as a partial write will now mess up
      // all the task-data!  --- This shouldn't be possible because it's all
      // powers of 2. (buffer size is power of 2 and struct size is power of 2).
      fprintf(
          stderr,
          "FATAL: Async queue corruption, cannot continue processing data.\n");
      exit(2);
    }
    if (!extend_queue(self, &package))
      break;
    // closed pipe or other error, return error
    if (perform_single_task(self))
      return -1;
  }
  pthread_mutex_unlock(&self->locker);
  return 0;
}

static void async_signal(struct Async* self) {
  struct Task package = {.task = 0, .arg = 0};
  pthread_mutex_lock(&self->locker);
  while (write(self->out, &package, sizeof(struct Task)) !=
         sizeof(struct Task)) {
    if (!extend_queue(self, &package))
      break;
    // closed pipe, return error
    if (perform_single_task(self))
      break;
  }
  pthread_mutex_unlock(&self->locker);
}

static void async_wait(struct Async* self) {
  for (int i = 0; i < self->count; i++) {
    pthread_join(self->thread_pool[i], NULL);
  }
  close(self->in);
  close(self->out);
  pthread_mutex_destroy(&self->locker);
  free(self);
}

static void async_finish(struct Async* self) {
  async_signal(self);
  async_wait(self);
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
                               .signal = async_signal,
                               .wait = async_wait,
                               .finish = async_finish,
                               .kill = async_kill};
