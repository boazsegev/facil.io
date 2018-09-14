---
title: facil.io - a mini-framework for C web applications
sidebar: 0.7.x/_sidebar.md
toc: false
---
# {{ page.title }}

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
  HTTP_HEADER_X_DATA = fiobj_str_static("X-Data", 6);
  // listen on port 3000 and any available network binding (NULL == 0.0.0.0)
  http_listen("3000", NULL, .on_request = on_request, .log = 1);
  // start the server
  facil_run(.threads = 1);
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

## facil.io - more than a powerful HTTP/Websockets server library.

[facil.io](http://facil.io) is an evented Network library written in C. It provides high performance TCP/IP network services by using an evented design that was tested to provide an easy solution to [the C10K problem](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) includes a mini-framework for Web Applications, with a fast HTTP / WebSocket server, integrated Pub/Sub, optional Redis connectivity, easy JSON handling and more nifty tidbits.

[facil.io](http://facil.io) powers the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine) and it can easily power your application as well.

[facil.io](http://facil.io) should work on Linux / BSD / macOS (and possibly CYGWIN) and is continuously tested on both Linux and macOS.

[facil.io](http://facil.io) is a source code library, making it easy to incorporate into any project. The API was designed for simplicity and extendability, which means writing new extensions and custom network protocols is easy.

[facil.io](http://facil.io)'s core library is a two-file library (`fio.h` and `fio.c`), making it easy to incorporate networking solutions into any project.

I used this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

## An easy chatroom example

Here's a simple Websocket chatroom example:

```c
#include "http.h"
#include "pubsub.h"
#include <string.h>

/* *****************************************************************************
Nicknames
***************************************************************************** */

struct nickname {
  size_t len;
  char nick[];
};

/* This initalization requires GNU gcc / clang ...
 * ... it's a default name for unimaginative visitors.
 */
static struct nickname MISSING_NICKNAME = {.len = 7, .nick = "unknown"};
static FIOBJ CHAT_CHANNEL;
/* *****************************************************************************
Websocket callbacks
***************************************************************************** */

/* We'll subscribe to the channel's chat channel when a new connection opens */
static void on_open_websocket(ws_s *ws) {
  /* subscription - easy as pie */
  websocket_subscribe(ws, .channel = CHAT_CHANNEL, .force_text = 1);
  /* notify everyone about new (named) visitors */
  struct nickname *n = websocket_udata(ws);
  if (n) {
    FIOBJ msg = fiobj_str_new(n->nick, n->len);
    fiobj_str_write(msg, " joined the chat.", 17);
    pubsub_publish(.channel = CHAT_CHANNEL, .message = msg);
    /* cleanup */
    fiobj_free(msg);
  }
}

/* Free the nickname, if any. */
static void on_close_websocket(ws_s *ws) {
  struct nickname *n = websocket_udata(ws);
  if (n) {
    /* send notification */
    FIOBJ msg = fiobj_str_new(n->nick, n->len);
    fiobj_str_write(msg, " left the chat.", 15);
    pubsub_publish(.channel = CHAT_CHANNEL, .message = msg);
    /* cleanup */
    fiobj_free(msg);
    free(n);
  }
}

/* Received a message from a client, format message for chat . */
static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  struct nickname *n = websocket_udata(ws);
  if (!n)
    n = &MISSING_NICKNAME;

  /* allocates a dynamic string. knowing the buffer size is faster */
  FIOBJ msg = fiobj_str_buf(n->len + 2 + size);
  fiobj_str_write(msg, n->nick, n->len);
  fiobj_str_write(msg, ": ", 2);
  fiobj_str_write(msg, data, size);
  if (pubsub_publish(.channel = CHAT_CHANNEL, .message = msg))
    fprintf(stderr, "Failed to publish\n");
  fiobj_free(msg);
  (void)(ws);
  (void)(is_text);
}

/* *****************************************************************************
HTTP Handling (Upgrading to Websocket)
***************************************************************************** */

/* Answers simple HTTP requests */
static void answer_http_request(http_s *h) {
  http_set_header2(h, (fio_str_info_s){.name = "Server", .len = 6},
                   (fio_str_info_s){.value = "facil.example", .len = 13});
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
  struct nickname *n = NULL;
  fio_str_info_s path = fiobj_obj2cstr(h->path);
  if (path.len > 1) {
    n = malloc(path.len + sizeof(*n));
    n->len = path.len - 1;
    memcpy(n->nick, path.data + 1, path.len); /* will copy the NUL byte */
  }
  // Websocket upgrade will use our existing response.
  if (http_upgrade2ws(.http = h, .on_open = on_open_websocket,
                      .on_close = on_close_websocket,
                      .on_message = handle_websocket_messages, .udata = n))
    free(n);
}

/* Our main function, we'll start up the server */
int main(void) {
  const char *port = "3000";
  const char *public_folder = NULL;
  uint32_t threads = 1;
  uint32_t workers = 1;
  uint8_t print_log = 0;
  CHAT_CHANNEL = fiobj_sym_new("chat", 4);

  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .on_upgrade = answer_http_upgrade, .log = print_log,
                  .public_folder = public_folder) == -1)
    perror("Couldn't initiate Websocket service"), exit(1);
  facil_run(.threads = threads, .processes = workers);

  fiobj_free(CHAT_CHANNEL);
}
```

## Further reading

The code in this project is heavily commented and the header files could (and probably should) be used as the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is ... well ... less helpful and harder to navigate (I'll leave it at that for now).

I hope you find the documentation on this website helpful.
