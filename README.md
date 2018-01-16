# facil.io - a mini-framework for C web applications
[![GitHub](https://img.shields.io/badge/GitHub-Open%20Source-blue.svg)](https://github.com/boazsegev/facil.io)
[![Build Status](https://travis-ci.org/boazsegev/facil.io.svg?branch=reHTTP)](https://travis-ci.org/boazsegev/facil.io)

[facil.io](http://facil.io) is a C mini-framework for web applications and includes:

* A fast HTTP/1.1 and Websocket static file + application server.
* Support for custom network protocols for both server and client connections.
* Dynamic types designed with web applications in mind (Strings, Hashes, Arrays etc').
* JSON parsing and formatting for easy network communication.
* A pub/sub process cluster engine for local and Websocket pub/sub.
* Optional connectivity with Redis.

[facil.io](http://facil.io) powers the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine) and it can easily power your application as well.

[facil.io](http://facil.io) provides high performance TCP/IP network services to Linux / BSD (and macOS) by using an evented design (as well as thread pool and forking support) and provides an easy solution to [the C10K problem](http://www.kegel.com/c10k.html).

You can read more about [facil.io](http://facil.io) on the [facil.io](http://facil.io) website.

```c
#include "http.h" /* the HTTP facil.io extension */

// We'll use this callback in `http_listen`, to handles HTTP requests
void on_request(http_s *request);

// These will contain pre-allocated values that we will use often
FIOBJ HTTP_X_DATA;

// Listen to HTTP requests and start facil.io
int main(int argc, char const **argv) {
  // allocating values we use often
  HTTP_X_DATA = fiobj_str_static("X-Data", 6);
  // listen on port 3000 and any available network binding (NULL == 0.0.0.0)
  http_listen("3000", NULL, .on_request = on_request, .log = 1);
  // start the server
  facil_run(.threads = 1);
  // deallocating the common values
  fiobj_free(HTTP_X_DATA);
}

// Easy HTTP handling
void on_request(http_s *request) {
  http_set_cookie(request, .name = "my_cookie", .name_len = 9, .value = "data",
                  .value_len = 4);
  http_set_header(request, HTTP_HEADER_CONTENT_TYPE,
                  http_mimetype_find("txt", 3));
  http_set_header(request, HTTP_X_DATA, fiobj_str_new("my data", 7));
  http_send_body(request, "Hello World!\r\n", 14);
}
```

## Using `facil.io` in your project

It's possible to either start a new project with `facil.io` or simply add it to an existing one. GNU `make` is the default build system and CMake is also supported. Notice that `facil.io` requires some C11 support from the compiler.

### Starting a new project with `facil.io`

To start a new project using the `facil.io` framework, run the following command in the terminal (change `appname` to whatever you want):

     $ bash <(curl -s https://raw.githubusercontent.com/boazsegev/facil.io/master/scripts/new/app) appname

You can [review the script here](scripts/new/app). In short, it will create a new folder, download a copy of the stable branch, add some demo boiler plate code and run `make clean` (which is required to build the `tmp` folder structure).

Next, edit the `makefile` to remove any generic features you don't need, such as the `DUMP_LIB` feature, the `DEBUG` flag or the `DISAMS` disassembler and start development.

Credit to @benjcal for suggesting the script.

**Notice: The *master* branch is the development branch. Please select the latest release tag for the latest stable release version.**

### Adding facil.io to an existing project

[facil.io](http://facil.io) is a source code library, so it's easy to copy the source code into an existing project and start using the library right away.

The `make libdump` command will dump all the relevant files in a single folder called `libdump`, and you can copy them all or divide them into header ands source files.

### Using `facil.io` as a CMake submodule

[facil.io](http://facil.io) also supports both `git` and CMake submodules. Credit to @OwenDelahoy (PR#8).

First, add the repository as a submudule using `git`:

    git submodule add https://github.com/boazsegev/facil.io.git

Then add the following line the project's `CMakeLists.txt`

    add_subdirectory(facil.io)

## More Examples

The examples folder includes examples for a [telnet echo protocol](examples/telnet-echo.c), a [super fast DIY HTTP/1.1 server](examples/fast-http.c) as well as an example for [Websocket pub/sub with Redis](examples/pubsub-chat.c).

You can find more information on the [facil.io](http://facil.io) website

---

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `evio`, that could mean `facil` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_request_s` struct.

* If you want to help me write a new SSL/TLS library or have an SSL/TLS solution we can fit into `facil` (as source code).

* If you want to help promote the library, that would be great as well. Perhaps publish [benchmarks](https://github.com/TechEmpower/FrameworkBenchmarks)) or share your story.

* Writing documentation into the `facil.io` website would be great. I keep the source code documentation fairly updated, but sometimes I can be a lazy bastard.
