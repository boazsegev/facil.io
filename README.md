# Server Writing Tools in C

Writing servers in C is repetitive and often involves copying a the code from [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and making a mess of things along the way.

Here you will find tools to write HTTP, Websockets and custom network applications with ease and speed, using a comfortable framework for writing network services in C.

All the libraries here are provided as source code. Although the more complex libraries (i.e. `lib-server` and it's extensions) require the smaller libraries (i.e. the thread-pool library `libasync`, the socket library `libsock` or the reactor core `libreact`), the smaller libraries can be used independently.

**Writing HTTP and Websocket services in C? Easy!**

Websockets and HTTP are super common, so `libserver` comes with HTTP and Websocket extensions, allowing us to easily write HTTP and Websocket services.

The framework's code is heavily documented and you can use Doxygen to create automated documentation for the API.

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

#define THREAD_COUNT 0
int main(int argc, char const* argv[]) {
  const char* public_folder = NULL;
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder, .log_static = 1);
  server_run(.threads = THREAD_COUNT);
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

## Background information

After years in [Ruby land](https://www.ruby-lang.org/en/) I decided to learn [Rust](https://www.rust-lang.org), only to re-discover that I actually quite enjoy writing in C and that C's reputation as "unsafe" or "hard" is undeserved and hides C's power.

So I decided to brush up my C programming skills... like an old man tinkering with his childhood tube box (an old radio, for you youngsters), I tend to come back to the question of web servers, the reactor pattern and evented programming.

Anyway, Along the way I wrote:

## [`libasync`](docs/libasync.md) - A native POSIX (`pthread`) thread pool.

 `libasync` is a simple thread pool that uses POSIX threads (and could be easily ported).

 It uses a combination of a pipe (for wakeup signals) and mutexes (for managing the task queue). I found it more performant then using conditional variables and more portable then using signals (which required more control over the process then an external library should require).

 `libasync` threads can be guarded by "sentinel" threads (it's a simple flag to be set prior to compiling the library's code), so that segmentation faults and errors in any given task won't break the system apart.

 This was meant to give a basic layer of protection to any server implementation, but I would recommend that it be removed for any other uses (it's a matter or changing the definition of `ASYNC_USE_SENTINEL` in `libasync.c`).

 Using `libasync` is super simple and would look something like this:

 ```c
#include "libasync.h"
#include <stdio.h>
#include <pthread.h>

// an example task
void say_hi(void* arg) {
 printf("Hi from thread %p!\n", pthread_self());
}

// an example usage
int main(void) {
 // create the thread pool with 8 threads.
 async_start(8);
 // send a task
 async_run(say_hi, NULL);
 // wait for all tasks to finish, closing the threads, clearing the memory.
 async_finish();
}
 ```

To use this library you only need the `libasync.h` and `libasync.c` files.

## [`libreact`](docs/libreact.md) - KQueue/EPoll abstraction.

It's true, some server programs still use `select` and `poll`... but they really shouldn't be (don't get me started).

When using [`libevent`](http://libevent.org) or [`libev`](http://software.schmorp.de/pkg/libev.html) you could end up falling back on `select` if you're not careful. `libreact`, on the other hand, will simply refuse to compile if neither kqueue nor epoll are available...

I should note that this means that Solaris and Windows support is currently absent, as Solaris's `evpoll` library has significantly different design requirements (each IO needs to re-register after it's events are handled) and Windows is a different beast altogether. This difference makes the abstraction sharing (API) difficult and ineffective.

Since I mentioned `libevent` and `libev`, I should point out that even a simple inspection shows that these are amazing and well tested libraries (how did they make those nice benchmark graphs?!)... but I hated their API (or documentation).

It seems to me, that since both `libevent` and `libev` are so all-encompassing, they end up having too many options and functions... I, on the other hand, am a fan of well designed abstractions, even at the price of control. I mean, you're writing a server that should handle 100K+ (or 1M+) concurrent connections - do you really need to manage the socket polling timeouts ("ticks")?! Are you really expecting more than a second to pass with no events?

To use this library you only need the `libreact.h` and `libreact.c` files.

## [`libsock`](docs/libsock.md) - Non-Blocking socket abstraction with `lib-react` support.

Non-Blocking sockets have a lot of common code that needs to be handled, such as a user level buffer (for all the data that didn't get sent when the socket was busy), delayed disconnection (don't close before sending all the data), file descriptor collision protection (preventing data intended for an old client from being sent to a new client) etc'.

Read through this library's documentation to learn more about using this thread-safe library that provides user level writing buffer and seamless integration with `libreact`.

## [`libserver`](docs/libserver.md) - a server building library.

Writing server code is fun... but in limited and controlled amounts... after all, much of it is simple code being repeated endlessly, connecting one piece of code with a different piece of code.

`libserver` is aimed at writing unix based (linux/BSD) servers application (network services), such as web applications. It uses `libreact` as the reactor, `libasync` to handle tasks and `libbuffer` for a user lever network buffer and writing data asynchronously.

`libserver` was designed to strike a good performance balance (memory, speed etc') while maintaining security (`fd` collision protection etc'). system calls that might not work on some OS versions were disabled (you can enable `sendfile` for mac using a single flag in `libsock.c`) and the thread pool, which has no consumer producer distinction, uses `nanosleep` instead of semaphores or mutexes (this actually performs better).

Many of these can be changed using a simple flag and the code should be commented enough for any required changes to be easily manageable. To offer some comparison, last time I counted, `ev.c` from `libev` had ~5000 lines, while `libreact` was less then 700 lines (~280 lines of actual code, everything else is comments).

Using `libserver` is super simple.

It's based on Protocol structure and callbacks, allowing for many different listening sockets and network services (protocols) to be active concurrently. We can easily and dynamically change protocols mid-stream, supporting stuff like HTTP upgrade requests (i.e. switching from HTTP to Websockets).

There was a simple echo server example at the beginning of this README.

Using this library requires all the minor libraries written to support it: `libasync`, `libsock` and `libreact`. This means you will need all the `.h` and `.c` files except the HTTP related files. `minicrypt` isn't required (nor did I finish writing it).

### A word about concurrency

It should be notes that network applications always have to keep concurrency in mind. For instance, the connection might be closed by one machine while the other is still preparing (or writing) it's response.

Worst, while the response is being prepared, a new client might connect to the system with the newly available (same) file descriptor, so the finalized response might get sent to the wrong client!

`libsock` and `libserver` protect us from such scenarios.

If you will use `libserver`'s multi-threading mode, it's concurrency will be limited to the `on_ready`, `ping` and `on_shutdown` callbacks. These callbacks should avoid using/setting any protocol specific information, or collisions might ensue.

All other callbacks (`on_data`, `on_close` and any server tasks initiated with `server_each` or `server_task`) will be performed sequentially for each connection, protecting a connection's data from corruption. While two concurrent connections might perform tasks at the same time, no single connection will perform more then one task at a time (unless you ask it to do su, using `async_run`).

In addition to multi-threading, `libserver` allows us to easily setup the network service's concurrency using processes (`fork`ing), which act differently then threads (i.e. memory space isn't shared, so that processes don't share accepted connections).

For best results, assume everything could run concurrently. `libserver` will do it's best to prevent collisions, but it is a generic library, so it might not know what to expect from your application.

## [`http`](docs/http.md) - a protocol for the web

All these libraries are used in a Ruby server I'm re-writing, which has native Websocket support ([Iodine](https://github.com/boazsegev/iodine)) - but since the HTTP protocol layer doesn't enter "Ruby-land" before the request parsing is complete, I ended up writing a light HTTP parser in C, and attaching it to the `libserver`'s protocol specs.

I should note the server I'm writing is mostly for x86 architectures and it uses unaligned memory access to 64bit memory "words". If you're going to use the HTTP parser on a machine that requires aligned memory access, some code would need to be reviewed.

The code is just a few helper settings and mega-functions (I know, refactoring will make it easier to maintain). The HTTP parser destructively edits the received headers and forwards a `http_request_s *` object to the `on_request` callback. This minimizes data copying and speeds up the process.

The HTTP protocol provides a built-in static file service and allows us to limit incoming request data sizes in order to protect server resources. The header size limit adjustable, but will be hardcoded during compilation (it's set to 8KB, which is also the limit on some proxies and intermediaries), securing the server from bloated header data DoS attacks. The incoming data size limit is dynamic.

Here's a "Hello World" HTTP server (with a stub to add static file services).

```c
// update the demo.c file to use the existing folder structure and makefile
#include "http.h"

void on_request(http_request_s* request) {
  http_response_s response = http_response_init(request);
  http_response_write_header(&response, .name = "X-Data", .value = "my data");
  http_response_set_cookie(&response, .name = "my_cookie", .value = "data");
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

Using this library requires all the files in the `src` folder for this repository, including the subfolder `http`. The `http` files are in a separate folder and the makefile in this project supports subfolders. You might want to place all the files in the same folder if you use these source files in a different project.

## [`Websocket`](src/http/websockets.h) - for real-time web applications

At some point I decided to move all the network logic from my [Ruby Iodine project](https://github.com/boazsegev/iodine) to C. This was, in no small part, so I could test my code and debug it with more ease (especially since I still test the network aspect using ad-hock code snippets and benchmarking tools).

This was when the `Websockets` library was born. It builds off the `http` server and allows us to either "upgrade" the HTTP protocol to Websockets or continue with an HTTP response.

I should note the the `Websockets` library, similarly to the HTTP parser, uses unaligned memory access to 64bit memory "words". It's good enough for the systems I target, but if you're going to use the library on a machine that requires aligned memory access, some code would need to be reviewed and readjusted.

Using this library, building a Websocket server in C just got super easy, here's both a Wesockets echo and a Websockets broadcast example - notice that broadcasting is a resource intensive task, requiring at least O(n) operations, where n == server capacity:

```c
// update the tryme.c file to use the existing folder structure and makefile
#include "websockets.h" // includes the "http.h" header

#include <stdio.h>
#include <stdlib.h>

/*****************************
The Websocket echo implementation
*/

void ws_open(ws_s* ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", ws);
}

void ws_echo(ws_s* ws, char* data, size_t size, int is_text) {
  // echos the data to the current websocket
  Websocket.write(ws, data, size, 1);
}

void ws_shutdown(ws_s* ws) {
  Websocket.write(ws, "Shutting Down", 13, 1);
}

void ws_close(ws_s* ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", ws);
}

/*****************************
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
/* the broadcast "task" performed by `Websocket.each` */
void ws_get_broadcast(ws_s* ws, void* arg) {
  struct ws_data* data = arg;
  Websocket.write(ws, data->data, data->size, 1);  // echo
}
/* The websocket broadcast server's `on_message` callback */

void ws_broadcast(ws_s* ws, char* data, size_t size, int is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data* msg = malloc(sizeof(* msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  Websocket.each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  Websocket.write(ws, data, size, 1);
}

/*****************************
The HTTP implementation
*/

void on_request(struct HttpRequest* request) {
  if (!strcmp(request->path, "/echo")) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);
    return;
  }
  if (!strcmp(request->path, "/broadcast")) {
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);

    return;
  }
  struct HttpResponse* response = HttpResponse.new(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}

/*****************************
The main function
*/

#define THREAD_COUNT 1
int main(int argc, char const* argv[]) {
  start_http_server(on_request, NULL, .threads = THREAD_COUNT);
  return 0;
}
```

The Websockets implementation uses the `minicrypt` library for the Base64 encoding and SHA-1 hashing that are part of the protocol's handshake. If you're using OpenSSL, you might want to rewrite this part and use the OpenSSL implementation (OpenSSL's implementation should be faster, as it's written in assembly instead of C and more brain-power was invested in optimizing it).

---

That's it for now. I'm still working on these as well as on `minicrypt` and SSL/TLS support (adding OpenSSL might be easy if you know the OpenSSL framework, but I think their API is terrible and I'm looking into alternatives).

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `libreact`, that could mean `lib-server` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

If you want to help me write a new SSL/TLS library or have an SSL/TLS solution you can fit into `lib-server`, hit me up.
