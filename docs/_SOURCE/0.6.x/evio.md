---
title: facil.io - lib evio, kqueue/epoll abstraction in C.
sidebar: 0.6.x/_sidebar.md
---
# The Evented IO Library - KQueue/EPoll abstraction

The `evio.h` library, is a KQueue/EPoll abstraction for edge triggered events and is part of [`facil.io`'s](./facil.md) core.

It should be noted that exactly like `epoll` and `kqueue`, `evio.h` might produce unexpected results if forked after initialized, since this will cause the `epoll`/`kqueue` data to be shared across processes, even though these processes will not have access to new file descriptors (i.e. `fd` 90 on one process might reference file "A" while on a different process the same `fd` (90) might reference file "B").

This documentation isn't relevant for `facil.io` users. `facil.io` implements `evio.h` callbacks and `evio.h` cannot be used without removing `facil.h` and `facil.c` from the project.

This file is here as quick reference to anyone interested in maintaining `facil.io` or learning about how it's insides work.

## Event Callbacks

Event callbacks are defined during the linking stage and are hardwired once compilation is complete.

`void evio_on_data(void *)` - called when the file descriptor has incoming data. This is **one-shot** triggered, meaning it will **not** be called again unless the `evio_add` (or `evio_set_timer`) are called.

`void evio_on_ready(void *)` - called when the file descriptor is ready to send data (outgoing).

`void evio_on_error(void *)` - called when a file descriptor raises an error while being polled.

`void evio_on_close(void *)` - called when a file descriptor was closed REMOTELY. `evio_on_close` will NOT get called when a connection is closed locally, unless using `sock.h`'s callback, the `sock_on_close` function.

**Notice**: Both EPoll and KQueue will **not** raise an event for an `fd` that was closed using the native `close` function, so unless using `sock.h` or calling `evio_on_close`, the `evio_on_close` callback will only be called for remote events.

**Notice**: The `on_open` event is missing by design, as it is expected that any initialization required for the `on_open` event will be performed before attaching the file descriptor (socket/timer/pipe) to `evio` using [`evio_add`]().

## The `evio` API


### `evio_create`

```c
intptr_t evio_create(void)
```

Creates the `epoll` or `kqueue` object.

It's impossible to add or remove file descriptors from the polling system before
calling this method.

Returns -1 on error, otherwise returns a unique value representing the `epoll`
or `kqueue` object. The returned value can be safely ignored.

**NOTE**: Once an `epoll` / `kqueue` object was opened, `fork` should be avoided,
since ALL the events will be shared among the forked processes (while not ALL
the file descriptors are expected to be shared).

### `evio_review`

```c
int evio_review(const int timeout_millisec)
```

Reviews any pending events (up to `EVIO_MAX_EVENTS`) and calls any callbacks.

Waits up to `timeout_millisec` for events to occur.

Returns -1 on error, otherwise returns the number of events handled.

### `evio_close`

```c
void evio_close(void);
```

Closes the `epoll` / `kqueue` object, releasing it's resources (important if
forking!).

### `evio_isactive`

```c
int evio_isactive(void)
```

Returns true if the evio is available for adding or removing file descriptors.


### `evio_add`

```c
int evio_add(int fd, void *callback_arg)
```

Adds a file descriptor to the polling object (ONE SHOT).

Returns -1 on error, otherwise return value is system dependent and can be
safely ignored.

### `evio_open_timer`

```c
intptr_t evio_open_timer(void)
```

Creates a timer file descriptor, system dependent.

Returns -1 on error, or a valid fd on success.

NOTE: Systems have a limit on the number of timers that can be opened.

### `evio_set_timer`

```c
intptr_t evio_set_timer(int fd, void *callback_arg, unsigned long milliseconds)
```

Adds a timer file descriptor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent.

## Known Issues

* On Linux, using `epoll`, the `EPOLLONESHOT` flag doesn't remove the `fd` from `epoll` descriptor, requiring `EPOLL_CTL_MOD` instead of `EPOLL_CTL_ADD`.

    To support a stateless API, where the user doesn't need to remember if a specific `fd` was previously passed to the `evio_add` function, the `epoll` implementation performs two system calls for new connections instead of a single system call (`EPOLL_CTL_ADD` is performed only if `EPOLL_CTL_MOD` fails).
