# Server Writing Tools in C - Change Log

Note: This change log is incomplete. I started it quite late, as interest in the libraries started to warrant better documentation of any changes made.

Although the libraries in this repo are designed to work together, they are also designed to work separately. Hence, the change logs for each library are managed separately. Here are the different libraries and changes:

* [Lib-React (`libreact`)](#lib-react) - The Reactor library.

* [Lib-Sock (`libsock`)](#lib-sock) - The socket helper library (development incomplete).

* [Lib-Async (`libasync`)](#lib-async) - The thread pool and tasking library.

* [Lib-Server (`lib-server`)](#lib-server) - The server writing library.

* [MiniCrypt (`mini-crypt`)](#minicrypt) - Common simple crypto library (development incomplete).

* [HTTP Protocol (`http`)](#http_protocol) - including HttpRequest, HttpResponse etc'.

* [Websocket extension (`mini-crypt`)](#websocket_extension) - Websocket Protocol for the basic HTTP implementation provided.

## General notes and future plans

Changes I plan to make in future versions:

* I plan to change the pool (request pool, packet pool, etc') implementations for the different libraries, so as to minimize any possible memory fragmentation that occur when `malloc` and `free` are used.

     Once I'm done, I'll deprecate the `objpool` library.

* Review code for use of Atomic types when Mutex use is avoidable (especially `libsock`, `lib-server` and `libasync`).

* Remove / fix server task container pooling (`FDTask` and `GroupTask` pools).

* Move `libsock`, `lib-server` and `http` away from `FILE *` and into `fd` data for file sending, possibly for leveraging the OS [`sendfile`](http://linux.die.net/man/2/sendfile) optimization (need to resolve issues with offset and possible file duplications as well as the [linux](http://linux.die.net/man/2/sendfile) vs. [bsd](https://www.freebsd.org/cgi/man.cgi?query=sendfile&sektion=2) implementations).

## A note about version numbers

I attempt to follow semantic versioning, except that the libraries are still under development, so version numbers get updated only when a significant change occurs.

Libraries with versions less then 0.1.0 have missing features (i.e. `libsock` is missing the `connect` implementation and `mini-crypt` is missing almost everything except what little published functions it offers).

Minor bug fixes, implementation optimizations and short git push corrections (I pushed bad code and fixed it in the same day or two) might not prompt a change in version numbers (not even the really minor ones).

Significant issue fixes cause the tiny version to bump.

API breaking changes always cause version bumps (could be a tiny version bump for tiny API changes).

... In other words, since these libraries are still being developed, test before adopting any updates.

## Lib-React

### V. 0.2.1

Baseline (changes not logged before this point in time).

## Lib-Sock (development incomplete)

### V. 0.0.2

* fixed situations in which the `send_packet` might not close a file (if the packet buffer references a `FILE`) before returning an error.

* The use of `sock_free_packet` is now required for any unused Packet object pointers checked out using `sock_checkout_packet`.

    This requirement allows the pool management to minimize memory fragmentation for long running processes.

* `libsock` memory requirements are now higher, as the user land buffer's Packet memory pool is pre-allocated to minimize memory fragmentation.

* Corrected documentation mistakes, such as the one stating that the `sock_send_packet` function will not handle the Packet object's memory on error (it does handle the memory, **always**).

### V. 0.0.1

Baseline (changes not logged before this point in time).

## Lib-Async

### V. 0.3.0

Baseline (changes not logged before this point in time).

## Lib-Server

### V. 0.3.3

* fixed situations in which the `sendfile` might not close the file before returning an error.

* There was a chance that the `on_data` callback might return after the connection was disconnected and a new connection was established for the same `fd`. This could have caused the `busy` flag to be cleared even if the new connection was actually busy. This potential issue had been fixed by checking the connection against the UUID counter before clearing the `busy` flag.

* reminder: The `Server.rw_hooks` feature is deprecated. Use `libsock`'s TLC (Transport Layer Callbacks) features instead.

### V. 0.3.2

Baseline (changes not logged before this point in time).

## MiniCrypt (development incomplete)

### V. 0.1.1

Baseline (changes not logged before this point in time).

## HTTP Protocol

### Date 20160609

* Rewrote the HttpResponse implementation to leverage `libsock`'s direct user-land buffer packet injection, minimizing user land data-copying.

* rewrote the HTTP `sendfile` handling for public folder settings.

* Fixed an issue related to the new pooling scheme, where old data would persist in some pooled request objects.

### Date 20160608

* The HttpRequest object is now being pooled within the request library (not the HTTP protocol implementation) using Atomics (less mutex locking) and minimizing memory fragmentation by pre-initializing the buffer on first request (preventing memory allocated after the first request from getting "stuck behind" any of the pool members).

### Date 20160607

Baseline (changes not logged before this point in time).

## Websocket extension

### Date 20160607

Baseline (changes not logged before this point in time).
