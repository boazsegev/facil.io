# facil.io

[facil.io](http://facil.io) is the C implementation for the [HTTP/Websockets Ruby Iodine server](https://github.com/boazsegev/iodine), which pretty much explains what [facil.io](http://facil.io) is all about...

[facil.io](http://facil.io) is a dedicated Linux / BSD (and macOS) network services library written in C. It's evented design is based on [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and [The C10K problem paper](http://www.kegel.com/c10k.html).

[facil.io](http://facil.io) provides a TCP/IP oriented solution for common network service tasks such as HTTP / Websocket servers, web applications and high performance backend servers.

[facil.io](http://facil.io) prefers a TCP/IP specialized solution over a generic one (although it can be easily adopted for UDP and other approaches).

[facil.io](http://facil.io) includes a number of libraries that work together for a common goal. Some of the libraries (i.e. the thread-pool library `libasync`, the socket library `libsock` and the reactor core `libreact`) can be used independently while others are designed to work together using a modular approach.

I got to use this library (including the HTTP server) on Linux, Mac OS X and FreeBSD (I had to edit the `makefile` for each environment).

**Writing HTTP and Websocket services in C? Easy!**

Websockets and HTTP are super common, so `facil.io` comes with HTTP and Websocket extensions, allowing us to easily write HTTP/1.1 and Websocket services.

The framework's code is heavily documented using comments. You can use Doxygen to create automated documentation for the API.

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
  // listen on port 3000, any available network binding (0.0.0.0)
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder);
  // start the server
  server_run(.threads = 16);
}
```

But `facil.io` really shines when it comes to Websockets and real-time applications, where the `kqueue`/`epoll` engine gives the framework a high performance running start.

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

**Simple API (Echo example)**

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


**SSL/TLS?** - possible, but you'll have to write it in.

Since most web applications (Node.js, Ruby etc') end up running behind load balancers and proxies, it is often that the SSL layer is handled by intermediaries.

But, if you need to expose the application directly to the web, it is possible to implement SSL/TLS support using `libsock`'s read/write hooks.

Since `libserver` utilizes `libsock` for socket communication (leveraging it's user-land buffer and other features), any RW hooks assigned be utilized for the specified connection.

Using `libsock`'s read-write hooks (`sock_rw_hook_set`) allows us to use an underlaying TLS/SSL library to send data securely. Use `sock_uuid2fd` to convert a connection's UUID to it's system assigned `fd`.

I did not write a TLS implementation since I'm still looking into OpenSSL alternatives (which has a difficult API and I fear for it's thread safety as far as concurrency goes) and since it isn't a priority for many use-cases (such as fast micro-services running behind a load-balancer/proxy that manages the SSL/TLS layer).

---

## The libraries in the repo

[facil.io](http://facil.io) is comprised of the following libraries that can be found in this repository:

* [`libasync`](docs/libasync.md) - A native POSIX (`pthread`) thread pool.

* [`libreact`](docs/libreact.md) - KQueue/EPoll abstraction.

* [`libsock`](docs/libsock.md) - Non-Blocking socket abstraction with `libreact` support.

* [`libserver`](docs/libserver.md) - a server building library.

* The HTTP/1.1 protocol (for `libserver`).

* The Websocket protocol (for `libserver` and HTTP/1.1).

## A word about concurrency

It should be notes that network applications always have to keep concurrency in mind. For instance, the connection might be closed by one machine while the other is still preparing (or writing) it's response.

Worst, while the response is being prepared, a new client might connect to the system with the newly available (same) file descriptor, so the finalized response might get sent to the wrong client!

`libsock` and `libserver` protect us from such scenarios.

If you will use `libserver`'s multi-threading mode, it's concurrency will be limited to the `on_ready`, `ping` and `on_shutdown` callbacks. These callbacks should avoid using/setting any protocol specific information, or collisions might ensue.

All other callbacks (`on_data`, `on_close` and any server tasks initiated with `server_each` or `server_task`) will be performed sequentially for each connection, protecting a connection's data from corruption. While two concurrent connections might perform tasks at the same time, no single connection will perform more then one task at a time (unless you ask it to do su, using `async_run`).

In addition to multi-threading, `libserver` allows us to easily setup the network service's concurrency using processes (`fork`ing), which act differently then threads (i.e. memory space isn't shared, so that processes don't share accepted connections).

For best results, assume everything could run concurrently. `libserver` will do it's best to prevent collisions, but it is a generic library, so it might not know what to expect from your application.

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

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `libreact`, that could mean `lib-server` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

Hit me up if you want to:

* Help me write HPACK / HTTP2 protocol support.

* Help me design / write a generic HTTP routing helper library for the `http_request_s` struct.

* If you want to help me write a new SSL/TLS library or have an SSL/TLS solution we can fit into `lib-server` (as source code).
