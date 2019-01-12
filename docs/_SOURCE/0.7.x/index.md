---
title: facil.io - a mini-framework for C web applications
sidebar: 0.7.x/_sidebar.md
toc: false
---
# {{{title}}}

A Web application in C? It's as easy as:

```c
#include "http.h" /* the HTTP facil.io extension */

// We'll use this callback in `http_listen`, to handles HTTP requests
void on_request(http_s *request);

// These will contain pre-allocated values that we will use often
FIOBJ HTTP_HEADER_X_DATA;

// Listen to HTTP requests and start facil.io
int main(int argc, char const **argv) {
  // allocating values we use often
  HTTP_HEADER_X_DATA = fiobj_str_new("X-Data", 6);
  // listen on port 3000 and any available network binding (NULL == 0.0.0.0)
  http_listen("3000", NULL, .on_request = on_request, .log = 1);
  // start the server
  fio_start(.threads = 1);
  // deallocating the common values
  fiobj_free(HTTP_HEADER_X_DATA);
  // these can be used, but we're ignoring them here.
  (void)argc; (void)argv;
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

## facil.io - more than a powerful HTTP/Websockets server library.

[facil.io](http://facil.io) is an evented Network library written in C. It provides high performance TCP/IP network services by using an evented design that was tested to provide an easy solution to [the C10K problem](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) includes a mini-framework for Web Applications, with a fast HTTP / WebSocket server, integrated Pub/Sub, optional Redis connectivity, easy JSON handling, [Mustache](http://mustache.github.io) template rendering and more nifty tidbits.

[facil.io](http://facil.io) powers the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine) and it can easily power your application as well.

[facil.io](http://facil.io) should work on Linux / BSD / macOS (and possibly CYGWIN) and is continuously tested on both Linux and macOS.

[facil.io](http://facil.io) is a source code library, making it easy to incorporate into any project. The API was designed for simplicity and extendability, which means writing new extensions and custom network protocols is easy.

[facil.io](http://facil.io)'s core library is a two-file library (`fio.h` and `fio.c`), making it easy to incorporate networking solutions into any project.

[facil.io](http://facil.io) supports both single-threaded and multi-threaded operation modes as well as a hybrid mode (multi-process with either a single-threaded or multi-threaded workers).

I used this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

## An easy chat-room example

Here's a simple WebSocket chat-room example:

```c
#include "fio_cli.h"
#include "http.h"

/* Chat-room channel name */
static fio_str_info_s CHAT_CHANNEL = {.len = 8, .data = "chatroom"};

/* *****************************************************************************
WebSocket callbacks
***************************************************************************** */

/* We'll subscribe to the channel's chat channel when a new connection opens */
static void on_open_websocket(ws_s *ws) {
  /* subscription - easy as pie */
  websocket_subscribe(ws, .channel = CHAT_CHANNEL, .force_text = 1);
  /* notify everyone about new (named) visitors */
  FIOBJ nickname = (FIOBJ)websocket_udata_get(ws);
  fio_str_info_s n = fiobj_obj2cstr(nickname);
  FIOBJ msg = fiobj_str_new(n.data, n.len);
  fiobj_str_write(msg, " joined the chat.", 17);
  pubsub_publish(.channel = CHAT_CHANNEL, .message = fiobj_obj2cstr(msg));
  /* cleanup */
  fiobj_free(msg);
}

/* Free the nickname, if any. */
static void on_close_websocket(intptr_t uuid, void *udata) {
  FIOBJ nickname = (FIOBJ)udata;
  fio_str_info_s n = fiobj_obj2cstr(nickname);
  /* send notification */
  FIOBJ msg = fiobj_str_new(n.data, n.len);
  fiobj_str_write(msg, " left the chat.", 15);
  fio_publish(.channel = CHAT_CHANNEL, .message = fiobj_obj2cstr(msg));
  /* cleanup */
  fiobj_free(msg);
  fiobj_free(nickname);
  (void)uuid;
}

/* Received a message from a client, format message for chat . */
static void handle_websocket_messages(ws_s *ws, fio_str_info_s data,
                                      uint8_t is_text) {
  FIOBJ nickname = (FIOBJ)websocket_udata_get(ws);
  fio_str_info_s n = fiobj_obj2cstr(nickname);
  /* allocates a dynamic string. knowing the buffer size is faster */
  FIOBJ msg = fiobj_str_buf(n.len + 2 + data.len);
  fiobj_str_write(msg, n.data, n.len);
  fiobj_str_write(msg, ": ", 2);
  fiobj_str_write(msg, data.data, data.len);
  fio_publish(.channel = CHAT_CHANNEL, .message = fiobj_obj2cstr(msg));
  fiobj_free(msg);
  (void)(ws);
  (void)(is_text);
}

/* *****************************************************************************
HTTP Handling (Upgrading to WebSocket)
***************************************************************************** */

/* Answers simple HTTP requests */
static void answer_http_request(http_s *h) {
  http_set_header2(h, (fio_str_info_s){.data = "Server", .len = 6},
                   (fio_str_info_s){.data = "facil.example", .len = 13});
  http_set_header(h, HTTP_HEADER_CONTENT_TYPE, http_mimetype_find("txt", 3));
  /* this both sends the response and frees the http handler. */
  http_send_body(h, "This is a simple Websocket chatroom example.", 44);
}

/* tests that the target protocol is "websockets" and upgrades the connection */
static void answer_http_upgrade(http_s *h, char *target, size_t len) {
  /* test for target protocol name */
  if (len != 9 || memcmp(target, "websocket", 9)) {
    http_send_error(h, 400);
    return;
  }
  FIOBJ n;
  fio_str_info_s path = fiobj_obj2cstr(h->path);
  if (path.len > 1) {
    n = fiobj_str_new(path.data + 1, path.len - 1);
  } else {
    n = fiobj_str_new("Guest", 5);
  }
  // Websocket upgrade will use our existing response.
  if (http_upgrade2ws(
          h, .on_open = on_open_websocket, .on_close = on_close_websocket,
          .on_message = handle_websocket_messages, .udata = (void *)n))
    fiobj_free(n);
}

/* *****************************************************************************
The main function
***************************************************************************** */
int main(int argc, char const *argv[]) {
  fio_cli_start(
      argc, argv, 0, 0, "WebSocket chat room example using facil.io",
      FIO_CLI_INT("-t number of threads"),
      FIO_CLI_INT("-w number of workers"),
      FIO_CLI_INT("-p port number to listen on (0 == unix socket)"),
      FIO_CLI_STRING("-b address binding"),
      FIO_CLI_STRING("-www a public folder from which to serve files"),
      FIO_CLI_BOOL("-v logs requests to STDERR"));

  fio_cli_set_default("-p", "3000");

  if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
                  .on_request = answer_http_request,
                  .on_upgrade = answer_http_upgrade,
                  .log = fio_cli_get_bool("-v"),
                  .public_folder = fio_cli_get("-www")) == -1) {
    perror("Couldn't listen for HTTP / WebSocket connections");
    exit(1);
  }
  fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));
  fio_cli_end();
}
```

## Further reading

The code in this project is heavily commented and the header files could (and probably should) be used as the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is ... well ... less helpful and harder to navigate (I'll leave it at that for now).

I hope you find the documentation on this website helpful.
