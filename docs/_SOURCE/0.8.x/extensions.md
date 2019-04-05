---
title: facil.io - core API and extensions
sidebar: 0.8.x/_sidebar.md
---
# {{{ title }}}

facil.io's [core library](fio) (the `fio-stl.h`, `fio.h` and `fio.c` files) is a stand alone three-file library, which can be easily used in different projects.

However, there are many tasks that are common to applications that need an evented networking library such as facil.io.

These bundled extensions to the core library provide common functionality. They aren't required and can be safely deleted without effecting the core library.

The following extensions come bundled along with facio.io:

* The [`TLS`](fio_tls) extension adds seamless SSL/TLS support to the network layer (requires 3rd party libraries).

* The [`FIOBJ`](fiobj) extension - this extension defines dynamic soft types and JSON helpers, making it easier to handle network bound information which often contains mixed types.

* The [`http`](http) extension - this compound module is everything HTTP and it requires the FIOBJ extension.

* The [`websockets`](websockets) extension - this is part of the HTTP module and extends it to support Websocket connections.

* The [`redis`](redis) extension adds Redis connectivity to the core pub/sub service, making horizontal scaling a breeze.
