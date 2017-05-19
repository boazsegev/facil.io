# `libasync` - A native POSIX (`pthread`) thread pool.

`libasync` is a simple thread pool that uses POSIX threads (and could be easily ported).

It normally uses a combination of micro-sleep and spin-locks for load-balancing across threads, since I found it more performant then using conditional variables and more portable then using signals (which required more control over the process then an external library should require).

The library could be easily set to use a combination of a pipe (for wakeup signals) and mutexes (for managing the task queue) by setting the `ASYNC_NANO_SLEEP` value in the `libasync.c` file.

`libasync` threads can be guarded by "sentinel" threads (it's a simple flag to be set prior to compiling the library's code), so that segmentation faults and errors in any given task won't break the system apart.

This was meant to give a basic layer of protection to any server implementation, but I would recommend that it be avoided for any other uses (it's a matter or changing the definition of `ASYNC_USE_SENTINEL` in `libasync.c`).

Using `libasync` is super simple and would look something like this:

```c
#include "libasync.h"
#include <stdio.h>
#include <pthread.h>

// an example task
void say_hi(void* arg) {
printf("Hi from thread %p!\n", pthread_self());
}

// an example usage
int main(void) {
// create the thread pool with 8 threads.
async_start(8);
// send a task
async_run(say_hi, NULL);
// wait for all tasks to finish, closing the threads, clearing the memory.
async_finish();
}
```

To use this library you only need the [`src/spnlock.h`](../src/spnlock.h), [`src/libasync.h`](../src/libasync.h) and [`src/libasync.c`](../src/libasync.c) files.

The API is as simple as can be, preferring simplicity over versatility.

Documentation can be found in the [`src/libasync.h`](../src/libasync.h) file.
