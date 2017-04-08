---
layout: default
title: facil.io - the C WebApp mini-framework
---

# {{ page.title }}

[facil.io](http://facil.io) is the C implementation for the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine), which pretty much explains what [facil.io](http://facil.io) is all about...

[facil.io](http://facil.io) is a dedicated Linux / BSD (and macOS) network services library written in C. It's evented design is based on [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and [The C10K problem paper](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) provides a TCP/IP oriented solution for common network service tasks such as HTTP / Websocket servers, web applications and high performance backend servers.

[facil.io](http://facil.io) prefers a TCP/IP specialized solution over a generic one (although it can be easily adopted for UDP and other approaches).

[facil.io](http://facil.io) includes a number of libraries that work together for a common goal. Some of the libraries (i.e. the thread-pool library `libasync`, the socket library `libsock` and the reactor core `libreact`) can be used independently while others are designed to work together using a modular approach.

I got to use this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

## Further reading

The code in this project is heavily commented and the header files could (and probably should) be used for the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is ... well ... less helpful and harder to navigate (I'll leave it at that for now).

The documentation in this folder includes:

* A [Getting Started Guide](getting_started.md) with example for WebApps utilizing the HTTP / Websocket protocols as well as a custom protocol.

* The core [`libserver` API documentation](libserver.md).

    This documents the main library API and should be used when writing custom protocols. This API is (mostly) redundant when using the `http` or `websockets` protocol extensions.

* The [`http` extension API documentation]() (Please help me write this).

    The `http` protocol extension allows quick access to the HTTP protocol necessary for writing web applications.

    Although the `libserver` API is still accessible, the `http_request_s` and `http_response_s` objects and API provide abstractions for the higher level HTTP protocol and should be preferred.

* The [`websockets` extension API documentation]() (Please help me write this).

    The `websockets` protocol extension allows quick access to the HTTP and Websockets protocols necessary for writing real-time web applications.

    Although the `libserver` API is still accessible, the `http_request_s`, `http_response_s` and `ws_s` objects and API provide abstractions for the higher level HTTP and Websocket protocols and should be preferred.

* Core documentation that documents the libraries used internally.

    The core documentation can be safely ignored by anyone using the `libserver`, `http` or `websockets` frameworks.

    The core libraries include (coming soon):

    * [`libreact`](./libreact.md) - The reactor core functionality (EPoll and KQueue abstractions).

    * [`libasync`](./libasync.md) - The thread pool and task management core functionality.

    * [`libsock`](./libsock.md) - A sockets library that resolves common issues such as fd collisions and user land buffer.

---

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `libreact`, that could mean `lib-server` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_request_s` struct.

* If you want to help me write a new SSL/TLS library or have an SSL/TLS solution we can fit into `lib-server` (as source code).
