# The Evented IO Library - KQueue/EPoll abstraction

The `evio.h` library, is a KQueue/EPoll abstraction for edge triggered events and is part of [`facil.io`'s](./facil.md) core.

It should be noted that exactly like `epoll` and `kqueue`, `evio.h` might produce unexpected results if forked after initialized, since this will cause the `epoll`/`kqueue` data to be shared across processes, even though these processes will not have access to new file descriptors (i.e. `fd` 90 on one process might reference file "A" while on a different process the same `fd` (90) might reference file "B").

This documentation isn't relevant for `facil.io` users. `facil.io` implements `evio.h` callbacks and `evio.h` cannot be used without removing `facil.h` anf `facil.c` from the project.

This file is here as quick reference to anyone interested in maintaining `facil.io` or learning about how it's insides work.

## Event Callbacks

Event callbacks are defined during the linking stage and are hardwired once compilation is complete.

`void evio_on_data(void *)` - called when the file descriptor has incoming data. This is edge triggered and will not be called again unless all the previous data was consumed.

`void evio_on_ready(void *)` - called when the file descriptor is ready to send data (outgoing).

`void evio_on_error(void *)` - called when a file descriptor raises an error while being polled.

`void evio_on_close(void *)` - called when a file descriptor was closed REMOTELY. `evio_on_close` will NOT get called when a connection is closed locally, unless using `sock.h`'s callback, the `sock_on_close` function.

**Notice**: Both EPoll and KQueue will **not** raise an event for an `fd` that was closed using the native `close` function, so unless using `sock.h` or calling `evio_on_close`, the `evio_on_close` callback will only be called for remote events.

**Notice**: The `on_open` event is missing by design, as it is expected that any initialization required for the `on_open` event will be performed before attaching the file descriptor (socket/timer/pipe) to `evio` using [`evio_add`]().

## The `evio` API

Coming soon... until then, please review the header file for documentation.
