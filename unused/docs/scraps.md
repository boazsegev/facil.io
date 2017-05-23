---
title: facil.io - scratch pad - leave this alone
---

# {{ page.title }}

---
## [`http`](docs/http.md) - a protocol for the web

All these libraries are used in a Ruby server I'm re-writing, which has native Websocket support ([Iodine](https://github.com/boazsegev/iodine)). Concurrency in Ruby is complicated, so I had the HTTP protocol layer avoid "Ruby-land" until the request parsing is complete, writing a light HTTP parser in C and attaching it to the `libserver`'s protocol specs.

I should note the server I'm writing is mostly for x86 architectures and it uses unaligned memory access to 64bit memory "words". If you're going to use the HTTP parser on a machine that requires aligned memory access, some code would need to be reviewed.

The code is just a few helper settings and mega-functions (I know, refactoring will make it easier to maintain). The HTTP parser destructively edits the received headers and forwards a `http_request_s *` object to the `on_request` callback. This minimizes data copying and speeds up the process.

The HTTP protocol provides a built-in static file service and allows us to limit incoming request data sizes in order to protect server resources. The header size limit adjustable, but will be hardcoded during compilation (it's set to 8KB, which is also the limit on some proxies and intermediaries), securing the server from bloated header data DoS attacks. The incoming data size limit is dynamic.

Using this library requires all the files in the `src` folder for this repository, including the subfolder `http`. The `http` files are in a separate folder and the makefile in this project supports subfolders. You might want to place all the files in the same folder if you use these source files in a different project.

## [`Websocket`](src/http/websockets.h) - for real-time web applications

At some point I decided to move all the network logic from my [Ruby Iodine project](https://github.com/boazsegev/iodine) to C. This was, in no small part, so I could test my code and debug it with more ease (especially since I still test the network aspect using ad-hock code snippets and benchmarking tools).

This was when the `Websockets` library was born. It builds off the `http` server and allows us to either "upgrade" the HTTP protocol to Websockets or continue with an HTTP response.

I should note the the `Websockets` library, similarly to the HTTP parser, uses unaligned memory access to 64bit memory "words". It's good enough for the systems I target, but if you're going to use the library on a machine that requires aligned memory access, some code would need to be reviewed and readjusted.

Using this library, building a Websocket server in C just got super easy, as the example at the top of this page already demonstrated.

The Websockets implementation uses the `bscrypt` library for the Base64 encoding and SHA-1 hashing that are part of the protocol's handshake.

---

That's it for now. I'm still working on these as well as on `bscrypt` and SSL/TLS support (adding OpenSSL might be easy if you know the OpenSSL framework, but I think their API is terrible and I'm looking into alternatives).
