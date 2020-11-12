## Concurrency Management - Weak functions

Weak functions are functions that can be overridden during the compilation / linking stage.

This provides control over some operations such as thread creation and process forking, which could be important when integrating facil.io into a VM engine such as Ruby or JavaScript.

### Forking

#### `fio_fork`

```c
int fio_fork(void);
```

OVERRIDE THIS to replace the default `fork` implementation.

Should behaves like the system's `fork`.

Current implementation simply calls [`fork`](http://man7.org/linux/man-pages/man2/fork.2.html).


### Thread Creation

#### `fio_thread_new`

```c
void *fio_thread_new(void *(*thread_func)(void *), void *arg);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Accepts a pointer to a function and a single argument that should be executed
within a new thread.

The function should allocate memory for the thread object and return a
pointer to the allocated memory that identifies the thread.

On error NULL should be returned.

The default implementation returns a `pthread_t *`.

#### `fio_thread_free`

```c
void fio_thread_free(void *p_thr);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Frees the memory associated with a thread identifier (allows the thread to
run it's course, just the identifier is freed).

#### `fio_thread_join`

```c
int fio_thread_join(void *p_thr);
```

OVERRIDE THIS to replace the default `pthread` implementation.

Accepts a pointer returned from `fio_thread_new` (should also free any
allocated memory) and joins the associated thread.

Return value is ignored.

-------------------------------------------------------------------------------
