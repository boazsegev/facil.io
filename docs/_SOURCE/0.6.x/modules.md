---
title: facil.io - core API and extensions
sidebar: 0.6.x/_sidebar.md
---
# {{{title}}}

facil.io's API is divided into different modules, each of them belonging to one of two groups:

* The Core Modules:

    These modules are required for facil.io to operate properly and they include:

    * [The `facil` library](facil) - this is the hub for everything network related except socket read/write calls.
    
        the `facil` library and it's API is at the heart of everything and it is the glue that holds the different modules together.

    * [The `sock` library](sock) - an abstraction layer over POSIX sockets, which solves many of the issues and pitfalls related to socket programming.

    * [The `defer` library](defer) - a simple event / action queue. It's used internally but can be used directly as well.    

    * [The `evio` library](evio) - the evented *one-shot* I/O module. This module is mostly for internal uses and shouldn't be accessed directly. It's API, although stable, should be considered volatile.

    * [The `FIOBJ` object library](fiobj) - a dynamic type system designed for handling network data (which is, by nature, dynamic).    

    * [The `fio_mem` library](fio_mem) - a simple concurrent memory allocator optimized for typical network use-cases. It can be disabled or replaced with jemalloc / tcmalloc and friends.

* The Extension Modules:

    These modules can be safely ignored or removed if not utilized by an application. These modules include:

    * [The `http` extension](http) - this is compound module is everything HTTP.

    * [The `websockets` extension](websockets) - this is part of the HTTP module and extends it to support Websocket connections.

    * [The `pubsub` extension](pubsub) - this allows messages to be exchanged using the [Publish–Subscribe Pattern](https://en.wikipedia.org/wiki/Publish–subscribe_pattern).

        This powerful tool, which integrates seamlessly with the Websocket library, can allow an application to scale horizontally across machines, as can be demonstrated by the use of the [Redis pub/sub engine](redis).

    * [The `redis_engine` extension](redis) to the pub/sub service.
    
    * [The CLI (`fio_cli`) helper](fio_cli) for handling command line arguments.

    * [The parsers](parsers) some of the extensions, such as the Redis pub/sub engine and the HTTP / Websocket extensions make use of parsers that can be used independently of the facil.io framework.


