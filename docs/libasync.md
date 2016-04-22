# The Async Library - Thread Pool and Task management

The Async library, `libasync` is a Thread Pool and Task management abstraction and is part of`lib-server`'s core.

You don't need to read this file if you're looking to utilize `lib-server`.

This file is here as quick reference to anyone interested in maintaining `lib-server` or learning about how it's insides work.

## The Async object: `async_p`

The Async API uses a `async_p` object (an opaque pointer), for storing a single thread-pool and it's associated tasks (and task pool).

The Async container object **must** be created using [`async_p Async.create(int threads)`](#async_p-asynccreateint-threads) and it is automatically destroyed once [`void Async.finish(async_p async)`](#void-asyncfinishasync_p-async) **or** [`void Async.wait(async_p async)`](#void-asyncwaitasync_p-async) had returned.

## The Async API

The following are the functions used for creating the thread pool, signaling (and waiting) for it's completion and scheduling tasks to be performed.

#### `async_p Async.create(int threads)`

Creates a new Async object (a thread pool) and returns a pointer using the `aync_p` (Async Pointer) type.

Returns `NULL` on error.

Requires the number of new threads to be initialized for the thread-pool.

Use:

```c
async_p async = Async.create(8);
```

#### `void Async.signal(async_p async)`

Signals an Async object to finish up.

The threads in the thread pool will continue performing all the tasks in the queue until the queue is empty. Once the queue is empty, the threads will exit. If new tasks are created after the signal, they will be added to the queue and processed until the last thread is done. Once the last thread exists, future tasks won't be processed.

Use:

```c
async_p async = Async.create(8);
```

#### `void Async.wait(async_p async)`

Waits for an Async object to finish up (joins all the threads in the thread
pool).

Once all the tasks were performed, the Async object will be destroyed and the function will return.

This function will wait forever or until a signal is received and all the tasks in the queue have been processed.

Use:

```c
async_p async = Async.create(8);
/...
Async.signal(async);
Async.wait(async);
```

#### `void Async.finish(async_p async)`

Both signals for an Async object to finish up and waits for it to finish. This is akin to calling both `signal` and `wait` in succession.

Use:

```c
async_p async = Async.create(8);
/...
Async.finish(async);
```

#### `int Async.run(async_p async, void (*task)(void*), void* arg)`

Schedules a task to be performed by an Async thread pool group.

The Task should be a function such as `void task(void *arg)`.

Returns -1 on error and 0 on success.

Use:

```c
void task(void * arg) { printf("%s", arg); }

char arg[] = "Demo Task";

Async.run(async, task, arg);
```

## A Quick Example

The following example isn't very interesting, but it's good enough to demonstrate the API:

```c
#include "libasync.h"
#include <stdio.h>
#include <pthread.h>

// an example task
void say_hi(void* arg) {
 printf("%d Hi from thread %p!\n", (int)arg, pthread_self());
}

// an example usage
int main(void) {
 // create the thread pool with 8 threads.
 async_p async = Async.new(8);
 // send some tasks
 for (int i = 0; i<16; i++)
   Async.run(async, say_hi, (void*)i);
 printf("Sent all the tasks using the main thread (%p)!\n", pthread_self());
 // wait for all tasks to finish, closing the threads, clearing the memory.
 Async.finish(async);
}
```
