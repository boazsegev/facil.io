# A Simple Socket Library for non-blocking Berkeley Sockets

This library aims to:

* Resolve the file descriptor collision security risk.

* Resolve issues with partial writes and concurrent write operations by implementing a user land buffer and a spinlock.

* Provide `sendfile` alternatives when sending big data stored on the disk - `sendfile` being broken on some system and lacking support for TLS.

* Provide a solution to the fact that closing a connection locally (using `close`) will prevent event loops and socket polling systems (`epoll`/`kqueue`) from raising the `on_hup` / `on_close` event. `libsock` provides support for local closure notification - this is done by defining an optional `reactor_on_close(intptr_t uuid)` overridable function.

* Provide support for timeout a management callback for server architectures that require the timeout to be reset ("touched") whenever a read/write occurs - this is done by defining an optional `sock_touch(intptr_t uuid)` callback.

`libsock` requires only the following three files from this repository: [`src/spnlock.h`](../src/spnlock.h),  [`src/libsock.h`](../src/libsock.h) and [`src/libsock.c`](../src/libsock.c).

#### The Story Of The Partial `write`

For some unknown reason, there is this little tidbit of information for the `write` function family that is always written in the documentation and is commonly ignored by first time network programmers:

> On success, **the number of bytes written** is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.

It is often missed that the number of bytes actually written might be smaller then the amount we wanted to write.

For this reason, `libsock` implements a user land buffer. All calls to `sock_write` promise to write the whole of the data (careful) unless a connection issue causes them to fail.

The only caveat is for very large amounts of data that exceed the available user land buffer (which you can change from the default ~16Mib), since `libsock` will keep flushing all the sockets (it won't return from the `sock_write` function call) until the data can be fully written to the user-land buffer.

However, this isn't normally an issue, since files can be sent at practically no cost when using `libsock` and large amounts of data are normally handled by files (temporary or otherwise).

#### The `sendfile` Experience

The `sendfile` directive is cool, it allows us to send data directly from a file to a socket, no copying required (although the kernel might or might not copy data).

However, the `sendfile` function call is useless when working with TLS connections, as the TLS implementation is performed in the application layer, meaning that the data needs to be encrypted in the application layer... So, no `sendfile` for TLS connections :-(

Another issue is that [`sendfile` is broken / unavailable on some systems](https://blog.phusion.nl/2015/06/04/the-brokenness-of-the-sendfile-system-call/)...

`libsock` Provides a dual solution for this:

1. On systems where `sendfile` is available, it will be used if no TLS or other read/write hook had been defined.

2. Where `sendfile` can't be used, `libsock` will write the file to the socket (or TLS), loading up to ~16Kb of data at a time.

    The same file (fd) can be sent to multiple clients at once (keeping it open in the application's cache) or automatically closed once it was sent.

This abstraction of the `sendfile` system call elevates the headache related with managing resources when sending big files.

#### Postponing The Timeout

Often, server applications need to implement a timeout review procedure that checks for stale connections.

However, this requires that the server architecture to implement a "write" and "read" logic (if only to set or reset the timeout for the connection) and somehow enforce this API, so that timeout events don't fire prematurely.

`libsock` saves us this extra work by providing an optional callback that allows server architectures to update their timeout state without implementing the whole "read/write" API stack.

By providing a `sock_touch` function that can be overwritten (a weak symbol function), the server architecture need only implement the `sock_touch` function to update any internal data structure or linked list, and `libsock` takes care of the rest.

This means that on any successful `sock_read`, `sock_write` or `sock_flush`, where data was written to the socket layer, `sock_touch` is called.

#### The Lost `on_close` Event

When polling using `kqueue` / `epoll` / the evented library of your choice, it is normal for locally closed sockets (when we call `close`) to close quietly, with no event being raised.

This can be annoying at times and often means that a wrapper is supplied for the `close` function and hopes are that no one calls `close` directly.

However, whenever a connection is closed using `libsock`, a callback `reactor_on_close(intptr_t)` is called.

This is supported by two facts: 1. because `sock_close` acts as a wrapper, calling the callback after closing the connection; 2. because connections are identified using a UUID (not an `fd`), making calls to `close` harder (though possible).

`sock_close` will always call the `reactor_on_close` callback, where you can call the function of your choice and presto: a local `close` will evoke the `on_close` event callback for the evented library of your choice.

The `reactor_on_close` name was chosen to have `libsock` work with `libreact`. This is a convenience, not a requirement.

It should be noted that a default `reactor_on_close` is provided, so there's no need to write one if you don't need one.

#### UUIDs & The File Descriptor Collision Security Risk

These things happen, whether we're using threads or evented programming techniques, the more optimized our code the more likely that we are exposed to file description collision risks.

Here is a quick example:

* Bob connects to his bank to get a bank statement on line.

* Bob receives the file descriptor 12 for the new connection and submits a request to the server.

* A request to the bank's database is performed in a non-blocking manner (evented, threaded, whatever you fancy). But, due to system stress or design or complexity, it will take a longer time to execute.

* Bob's connection is dropped for some reason and file descriptor 12 is released by the system.

* Alice connects to the server and receives the (now available) file descriptor 12 for the new connection (Alice can even negotiate a valid TLS connection at this point).

* The database response arrives and the information is sent to the file descriptor.

* Alice gets Bob's bank statement.

... Hmmm... bad.

The risk might seem unlikely, but it exists. These possible collisions in read/write operations are one of the main reasons why operating systems hold on to a closed socket's file descriptor for a bit longer than they need (often a minute or two).

Hence: `sock.h` uses connection UUIDs to map to the underlying file descriptors.

These UUIDs are the C standard `intptr_t` type, which means that they are easy to handle, store and move around. The UUIDs are a simple system local scheme and shouldn't be shared among systems or processes (collision risks). The only certainly invalid UUID is `-1` (depending on the system's byte-ordering scheme).

In our example:

* Bob connects to his bank to get a bank statement on line.

* Bob receives the file descriptor 12, mapped to the connection UUID 0x114.

* [...] everything the same until Bob's connection drops.

* Alice connects to the server and receives the (now available) file descriptor 12 for the new connection. The connection is mapped to the UUID 0x884.

* The database response arrives but isn't sent because the write operation fails (invalid UUID 0x114).

So, using `sock.h`, Alice will **not** receive Bob's bank statement.

#### TCP/IP as default

`sock.h` defaults to TCP/IP sockets, so calls to `sock_listen` and `sock_connect` will assume TCP/IP.

However, importing other socket types should be possible by using the `sock_open(fd)` function, that allows us to import an existing file descriptor into `sock.h`, assigning this file descriptor a valid UUID.

## A Simple API

The `sock.h` API can be divided into a few different categories:

- General helper functions

- Accepting connections and opening new sockets.

- Sending and receiving data.

- Direct user level buffer API.

- Read/Write Hooks.

---

More information coming soon. Until than, read the header files.
