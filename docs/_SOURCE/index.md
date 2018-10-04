---
title: facil.io - The C Web Application Framework
toc: false
---
# {{{ title }}}

* [facil.io](http://facil.io) is an evented Network library written in C. It provides high performance TCP/IP network services by using an evented design that was tested to provide an easy solution to [the C10K problem](http://www.kegel.com/c10k.html).

* [facil.io](http://facil.io) includes a mini-framework for Web Applications, with a fast HTTP / WebSocket server, integrated Pub/Sub, optional Redis connectivity, easy JSON handling and more nifty tidbits.

* [facil.io](http://facil.io) powers the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine) and it can easily power your application as well.

* [facil.io](http://facil.io) should work on Linux / BSD / macOS (and possibly CYGWIN) and is continuously tested on both Linux and macOS.

* [facil.io](http://facil.io) is a source code library, making it easy to incorporate into any project. The API was designed for simplicity and extendability, which means writing new extensions and custom network protocols is easy.

I used this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

### A Web application in C? It's as easy as...

```c
#include "http.h" /* the HTTP facil.io extension */

// We'll use this callback in `http_listen`, to handles HTTP requests
void on_request(http_s *request);

// These will contain pre-allocated values that we will use often
FIOBJ HTTP_HEADER_X_DATA;

// Listen to HTTP requests and start facil.io
int main(void) {
  // allocating values we use often
  HTTP_HEADER_X_DATA = fiobj_str_new("X-Data", 6);
  // listen on port 3000 and any available network binding (NULL == 0.0.0.0)
  http_listen("3000", NULL, .on_request = on_request, .log = 1);
  // start the server
  fio_start(.threads = 1);
  // deallocating the common values
  fiobj_free(HTTP_HEADER_X_DATA);
}

// Easy HTTP handling
void on_request(http_s *request) {
  http_set_cookie(request, .name = "my_cookie", .name_len = 9, .value = "data",
                  .value_len = 4);
  http_set_header(request, HTTP_HEADER_CONTENT_TYPE,
                  http_mimetype_find("txt", 3));
  http_set_header(request, HTTP_HEADER_X_DATA, fiobj_str_new("my data", 7));
  http_send_body(request, "Hello World!\r\n", 14);
}
```

*(Written using version 0.7.0)*

### Creating a Web Application Using the Latest Release

Using the latest release is often the best / safest choice.

Either copy the source code files from the [latest release on GitHub](https://github.com/boazsegev/facil.io/releases/latest) to an existing / new project, or use the following script to create a new boiler-plate Web Application:

```bash
bash <(curl -s https://raw.githubusercontent.com/boazsegev/facil.io/master/scripts/new/app) appname
```

### Creating a Web Application Using the Edge Version

The edge version is the development version and may be broken at any given moment and it's API is unstable.

Having said that, it's usually richer in bug fixes and features that wait to be released as part of a stable release.

Either copy the source code files from the master branch to an existing / new project, or use the following script to create a new boiler-plate Web Application:

```bash
export FIO_BRANCH=master
bash <(curl -s https://raw.githubusercontent.com/boazsegev/facil.io/master/scripts/new/app) appname
```

---

## Forking, Contributing and all that Jazz

Sure, why not.

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_s` struct.

* If you want to help integrate an SSL/TLS library into `facil`, that would be great.

* If you can add Solaris or Windows completion ports support (to be added to `fio.c`). This could improve facil.io's performance on these platforms (which currently fallback on `poll`).
