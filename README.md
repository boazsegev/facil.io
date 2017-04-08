# facil.io - the C WebApp mini-framework

[facil.io](http://facil.io) is the C implementation for the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine), which makes it easy to guess what [facil.io](http://facil.io) is all about...

[facil.io](http://facil.io) is a dedicated Linux / BSD (and macOS) network services library written in C. It's evented design is based on [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and [The C10K problem paper](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) provides a TCP/IP oriented solution for common network service tasks such as HTTP / Websocket servers, web applications and high performance backend servers.

You can read more about [facil.io](http://facil.io) on the [facil.io](http://facil.io) website.

## Three Quick Examples

Writing HTTP and Websocket services in C is easy with [facil.io](http://facil.io).

### HTTP/1.1

The simplest example, of course, would be the famous "Hello World" application... this is so easy, it's practically boring (so we add custom headers and cookies):

```c
#include "http.h"

void on_request(http_request_s* request) {
  http_response_s response = http_response_init(request);
  http_response_set_cookie(&response, .name = "my_cookie", .value = "data");
  http_response_write_header(&response, .name = "X-Data", .value = "my data");
  http_response_write_body(&response, "Hello World!\r\n", 14);
  http_response_finish(&response);
}

int main() {
  char* public_folder = NULL;
  // listen on port 3000, any available network binding (NULL == 0.0.0.0)
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder);
  // start the server
  server_run(.threads = 16);
}
```

### Websockets

[facil.io](http://facil.io) really shines when it comes to Websockets and real-time applications, where the `kqueue`/`epoll` engine gives the framework a high performance running start.

Here's a full-fledge example of a Websocket echo server, a Websocket broadcast server and an HTTP "Hello World" (with an optional static file service) all rolled into one:

```c
// update the demo.c file to use the existing folder structure and makefile
#include "websockets.h"  // includes the "http.h" header

#include <stdio.h>
#include <stdlib.h>

/* ******************************
The Websocket echo implementation
*/

void ws_open(ws_s* ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", ws);
}

void ws_echo(ws_s* ws, char* data, size_t size, uint8_t is_text) {
  // echos the data to the current websocket
  websocket_write(ws, data, size, 1);
}

void ws_shutdown(ws_s* ws) {
  websocket_write(ws, "Shutting Down", 13, 1);
}

void ws_close(ws_s* ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", ws);
}

/* ***********************************
The Websocket Broadcast implementation
*/

/* websocket broadcast data */
struct ws_data {
  size_t size;
  char data[];
};
/* free the websocket broadcast data */
void free_wsdata(ws_s* ws, void* arg) {
  free(arg);
}
/* the broadcast "task" performed by websocket_each */
void ws_get_broadcast(ws_s* ws, void* arg) {
  struct ws_data* data = arg;
  websocket_write(ws, data->data, data->size, 1);  // echo
}
/* The websocket broadcast server's on_message callback */

void ws_broadcast(ws_s* ws, char* data, size_t size, uint8_t is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data* msg = malloc(sizeof(* msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  websocket_each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  websocket_write(ws, data, size, 1);
}

/* ********************
The HTTP implementation
*/

void on_request(http_request_s* request) {
  // to log we will start a response.
  http_response_s response = http_response_init(request);
  http_response_log_start(&response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade && !strcmp(request->path, "/broadcast")) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown, .response = &response);

    return;
  }
  // other upgrade requests will have the following properties:
  if (request->upgrade) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .on_open = ws_open, .on_close = ws_close, .timeout = 4,
                      .on_shutdown = ws_shutdown, .response = &response);
    return;
  }
  // HTTP response
  http_response_write_body(&response, "Hello World!", 12);
  http_response_finish(&response);
}

/****************
The main function
*/

#define THREAD_COUNT 1
int main(int argc, char const* argv[]) {
  const char* public_folder = NULL;
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder, .log_static = 1);
  server_run(.threads = THREAD_COUNT);
  return 0;
}
```

### A Custom Protocol (an Echo example)

[facil.io](http://facil.io)'s API is designed for both simplicity and an object oriented approach, using network protocol objects and structs to avoid bloating function arguments and to provide sensible default behavior.

Here's a simple Echo example (test with telnet to port `"3000"`).

```c
#include "libserver.h"

// Performed whenever there's pending incoming data on the socket
static void perform_echo(intptr_t uuid, protocol_s * prt) {
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    sock_write(uuid, buffer, len + 6);
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid); // closes after `write` had completed.
      return;
    }
  }
}
// performed whenever "timeout" is reached.
static void echo_ping(intptr_t uuid, protocol_s * prt) {
  sock_write(uuid, "Server: Are you there?\n", 23);
}
// performed during server shutdown, before closing the socket.
static void echo_on_shutdown(intptr_t uuid, protocol_s * prt) {
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}
// performed after the socket was closed and the currently running task had completed.
static void destroy_echo_protocol(protocol_s * echo_proto) {
  if (echo_proto) // always error check, even if it isn't needed.
    free(echo_proto);
  fprintf(stderr, "Freed Echo protocol at %p\n", echo_proto);
}
// performed whenever a new connection is accepted.
static inline protocol_s *create_echo_protocol(intptr_t uuid, void * _ ) {
  // create a protocol object
  protocol_s * echo_proto = malloc(sizeof(* echo_proto));
  // set the callbacks
  * echo_proto = (protocol_s){
      .on_data = perform_echo,
      .on_shutdown = echo_on_shutdown,
      .ping = echo_ping,
      .on_close = destroy_echo_protocol,
  };
  // write data to the socket and set timeout
  sock_write(uuid, "Echo Service: Welcome. Say \"bye\" to disconnect.\n", 48);
  server_set_timeout(uuid, 10);
  // print log
  fprintf(stderr, "New Echo connection %p for socket UUID %p\n", echo_proto,
          (void * )uuid);
  // return the protocol object to attach it to the socket.
  return echo_proto;
}
// creates and runs the server
int main(int argc, char const * argv[]) {
  // listens on port 3000 for echo services.
  server_listen(.port = "3000", .on_open = create_echo_protocol);
  // starts and runs the server
  server_run(.threads = 10);
  return 0;
}
```

---

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `libreact`, that could mean `lib-server` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_request_s` struct.

* If you want to help me write a new SSL/TLS library or have an SSL/TLS solution we can fit into `lib-server` (as source code).
