---
title: facil.io - the C WebApp mini-framework
---
# {{ page.title }}

A Web application in C? It's as easy as:

```c
#include "http.h"

void on_request(http_request_s* request) {
  http_response_s * response = http_response_create(request);
  // http_response_log_start(response); // logging ?
  http_response_set_cookie(response, .name = "my_cookie", .value = "data");
  http_response_write_header(response, .name = "X-Data", .value = "my data");
  http_response_write_body(response, "Hello World!\r\n", 14);
  http_response_finish(response);
}

int main(void) {
  char* public_folder = NULL;
  // listen on port 3000, any available network binding (NULL ~= 0.0.0.0)
  http_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder, .log_static = 0);
  // start the server
  facil_run(.threads = 16);
}
```

## facil.io - more than a powerful HTTP/Websockets server library.

[facil.io](http://facil.io) is a C mini-framework for web applications and includes a fast HTTP and Websocket server, a native Pub/Sub solution, an optional Redis pub/sub engine, support for custom protocols and some nifty tidbits.

[facil.io](http://facil.io) powers the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine) and it can easily power your application as well.

[facil.io](http://facil.io) provides high performance TCP/IP network services to Linux / BSD (and macOS) by using an evented design that was tested with tens of thousands of connections and provides an easy solution to [the C10K problem](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) prefers a TCP/IP specialized solution over a generic one (although it can be easily adopted for Unix sockets, UDP and other approaches).

[facil.io](http://facil.io) includes a number of libraries that work together for a common goal. Some of the libraries (i.e. the thread-pool library `defer`, the socket library `sock`, the evented IO core `evio`, and [the dynamic type library](fiobj.md)) can be used independently while others are designed to work together using a modular approach.

I used this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

## An easy chatroom example

Here's a simple Websocket chatroom example:

```c
/* including the Websocket extension automatically includes the facil.io core */
#include "websockets.h"
/* We'll use the process cluster pub/sub service */
#include "pubsub.h"
/* We'll leverage the dynamic type library in this example */
#include "fiobj.h"

/* *****************************************************************************
Websocket callbacks
***************************************************************************** */

/* We'll subscribe to the channel's chat channel when a new connection opens */
static void on_open_websocket(ws_s *ws) {
  websocket_subscribe(ws, .channel.name = "chat", .force_text = 1);
}

/* Free the nickname */
static void on_close_websocket(ws_s *ws) {
  if (websocket_udata(ws))
    fiobj_free(websocket_udata(ws));
}

/* Copy the nickname and the data to format a nicer message. */
static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  fiobj_s * nickname = websocket_udata(ws);
  fiobj_s * msg = fiobj_str_copy(nickname);
  fiobj_str_write(msg, ": ", 2);
  fiobj_str_write(msg, data, size);
  fio_cstr_s cmsg = fiobj_obj2cstr(msg);
  pubsub_publish(.channel = {.name = "chat", .len = 4},
                 .msg = {.data = (char * )cmsg.data, .len = cmsg.len});
  fiobj_free(msg);
  (void)(ws);
  (void)(is_text);
}

/* *****************************************************************************
HTTP Handling (Upgrading to Websocket)
***************************************************************************** */

static void answer_http_request(http_request_s *request) {
  http_response_s * response = http_response_create(request);
  // We'll match the dynamic logging settings with the static logging ones.
  if (request->settings->log_static)
    http_response_log_start(response);

  http_response_write_header(response, .name = "Server", .name_len = 6,
                             .value = "facil.example", .value_len = 13);

  // the upgrade header value has a quick access pointer.
  if (request->upgrade) {
    fiobj_s * nickname = NULL;
    // We'll use the request path as the nickname, if it's available
    if (request->path_len > 1) {
      nickname = fiobj_str_new(request->path + 1, request->path_len - 1);
    } else {
      nickname = fiobj_str_new("unknown", 7);
    }
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_open = on_open_websocket,
                      .on_close = on_close_websocket,
                      .on_message = handle_websocket_messages,
                      .udata = nickname);
    return;
  }
  //     ****  Normal HTTP request, no Websockets ****
  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);

  http_response_write_body(response, "This is a Websocket chatroom example.",
                           37);
  // this both sends and frees the response.
  http_response_finish(response);
}

/* *****************************************************************************
The main function, where we setup facil.io and start it up.
***************************************************************************** */
int main(void) {
  const char * port = "3000";
  const char * public_folder = NULL;
  uint32_t threads = 1;
  uint32_t workers = 1;
  uint8_t print_log = 0;

  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .log_static = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Websocket service"), exit(1);

  facil_run(.threads = threads, .processes = workers);
}
```

## Further reading

The code in this project is heavily commented and the header files could (and probably should) be used for the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is ... well ... less helpful and harder to navigate (I'll leave it at that for now).

The documentation in this folder includes:

* A [Getting Started Guide](getting_started.md) with example for WebApps utilizing the HTTP / Websocket protocols as well as a custom protocol.

* A [quick guide to `facil.io`'s dynamic type library](fiobj.md) with quick examples get you started.

* The core [`facil.io` API documentation](facil.md).

    This documents the main library API and should be used when writing custom protocols. This API is (mostly) redundant when using the `http` or `websockets` protocol extensions.

* The [`http` extension API documentation]() (Please help me write this).

    The `http` protocol extension allows quick access to the HTTP protocol necessary for writing web applications.

    Although the `libserver` API is still accessible, the `http_request_s` and `http_response_s` objects and API provide abstractions for the higher level HTTP protocol and should be preferred.

* The [`websockets` extension API documentation]() (Please help me write this).

    The `websockets` protocol extension allows quick access to the HTTP and Websockets protocols necessary for writing real-time web applications.

    Although the `libserver` API is still accessible, the `http_request_s`, `http_response_s` and `ws_s` objects and API provide abstractions for the higher level HTTP and Websocket protocols and should be preferred.

* Core documentation that documents the libraries used internally.

    The core documentation can be safely ignored by anyone using the `facil.io`, `http` or `websockets` frameworks.

    The core libraries include (coming soon):

    * [`defer`](./defer.md) - A simple event-loop with the added functionality of a thread pool and `fork`ing support.

    * [`sock`](./sock.md) - A sockets library that resolves common issues such as fd collisions and user land buffer.

    * [`evio`](./evio.md) - an edge triggered `kqueue` / `epoll` abstraction / wrapper with an overridable callback design, allowing fast access to these APIs while maintaining portability enhancing ease of use (at the expense of complexity).

---

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `evio`, that could mean `facil.io` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties that implies).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_request_s` struct.

* If you want to help me write a new SSL/TLS library or have an SSL/TLS solution we can fit into `lib-server` (as source code).
