# Server Writing Tools in C

Writing servers in C is repetitive and often involves copying a the code from [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) and making a mess of things along the way.

Here you will find tools to write HTTP, Websockets and custom network applications with ease and speed, using a comfortable framework for writing network services in C.

**Writing HTTP and Websocket services in C? Easy!**

Websockets and HTTP are super common, so I wrote an API to support them.

The framework's code is heavily documented and you can use Doxygen to create automated documentation for the API.

```c
#include "websockets.h" // auto-includes "http.h"
// Concurrency using thread? How many threads in the thread pool?
#define THREAD_COUNT 4
// Concurrency using processes? (more then a single process can be used)
#define PROCESS_COUNT 1
// Our websocket service - echo.
void ws_echo(ws_s* ws, char* data, size_t size, int is_text) {
  // echos the data to the current websocket
  Websocket.write(ws, data, size, is_text);
}
// our HTTP callback - hello world.
void on_request(struct HttpRequest* request) {
  if (request->upgrade) {
    // "upgrade" to the Websocket protocol, if requested
    websocket_upgrade(.request = request, .on_message = ws_echo);
    return;
  }
  struct HttpResponse* response = HttpResponse.create(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}
// Our main function will start the HTTP server
int main(int argc, char const* argv[]) {
  start_http_server(on_request, NULL,
              .port = "8080", // default is "3000"
              .threads = THREAD_COUNT,
              .processes = PROCESS_COUNT );
  return 0;
}
```

There's an example with cookies and custom headers further on.

**Writing custom network services in C? Easy!**

Here's a quick example for an echo server using the `lib-server` framework's API. We'll only use the `on_data` event (for incoming data) as we don't have any handshake (`on_open`) clean up (`on_close`) or shutdown (`on_shutdown`) events we need to handle.

There's an example with a few more details further on.

```c
#include "lib-server.h"
#include <stdio.h>
#include <string.h>
// Concurrency using thread? How many threads in the thread pool?
#define THREAD_COUNT 4
// Concurrency using processes? (more then a single process can be used)
#define PROCESS_COUNT 1
// a simple echo... this is the main callback
void on_data(server_pt server, uint64_t fd_uuid) {
  char buff[1024];
  ssize_t incoming = 0;
  while ((incoming = Server.read(server, fd_uuid, buff, 1024)) > 0) {
    Server.write(server, fd_uuid, buff, incoming);  // echo the data.
    if (!strncasecmp(buff, "bye", 3)) {             // check for keyword "bye"
      Server.close(server, fd_uuid);  // closes the connection (on keyword)
    }
  }
}
// running the server
int main(void) {
  // We'll create the echo protocol object. We'll only use the on_data callback.
  struct Protocol protocol = {.on_data = on_data,
                              .service = "echo"};
  // This macro will call Server.listen(settings) with the settings we provide.
  start_server(.protocol = &protocol, .timeout = 10, .port = "3000",
               .threads = THREAD_COUNT, .processes = PROCESS_COUNT);
}
```

**SSL/TLS?** - possible, but you'll have to write it in.

Since most web applications (Node.js, Ruby etc') end up running behind load balancers and proxies, it is often that the SSL layer is handled by intermediaries.

But, if you need to expose the application directly to the web, it is possible to implement SSL/TLS support using `lib-server`'s read-write hooks (`Server.rw_hooks`).

I did not write this implementation since I'm still looking into OpenSSL alternatives (which has a difficult API and I fear for it's thread safety as far as concurrency goes) and since it isn't a priority for many use-cases (such as fast micro-services running behind a proxy that manages the SSL/TLS layer).

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
 async_p async = Async.new(8);
 // send a task
 Async.run(async, say_hi, NULL);
 // wait for all tasks to finish, closing the threads, clearing the memory.
 Async.finish(async);
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

## [`lib-server`](docs/lib-server.md) - a server building library.

Writing server code is fun... but in limited and controlled amounts... after all, much of it is simple code being repeated endlessly, connecting one piece of code with a different piece of code.

`lib-server` is aimed at writing unix based (linux/BSD) servers application (network services), such as web applications. It uses `libreact` as the reactor, `libasync` to handle tasks and `libbuffer` for a user lever network buffer and writing data asynchronously.

`lib-server` might not be optimized to your liking, but it's all working great for me. Besides, it's code is heavily commented code, easy to edit and tweak. To offer some comparison, `ev.c` from `libev` has ~5000 lines, while `libreact` is less then 400 lines (~260 lines of actual code)...

Using `lib-server` is super simple. It's based on Protocol structure and callbacks, so that we can dynamically change protocols and support stuff like HTTP upgrade requests (i.e. switching from HTTP to Websockets). Here's a simple echo server example:

```c
#include "lib-server.h"
#include <stdio.h>
#include <string.h>

// we don't have to, but printing stuff is nice...
void on_open(server_pt server, uint64_t fd_uuid) {
  printf("A connection was accepted on socket UUID #%llu.\n", fd_uuid);
}
// server_pt is just a nicely typed pointer, which is there for type-safty.
// The Server API is used by calling `Server.api_call` (often with the pointer).
void on_close(server_pt server, uint64_t fd_uuid) {
  printf("Socket UUID #%llu is now disconnected.\n", fd_uuid);
}

// a simple echo... this is the main callback
void on_data(server_pt server, uint64_t fd_uuid) {
  // We'll assign a reading buffer on the stack
  char buff[1024];
  ssize_t incoming = 0;
  // Read everything, this is edge triggered, `on_data` won't be called
  // again until all the data was read.
  while ((incoming = Server.read(server, fd_uuid, buff, 1024)) > 0) {
    // since the data is stack allocated, we'll write a copy
    // otherwise, we'd avoid a copy using Server.write_move
    Server.write(server, fd_uuid, buff, incoming);
    if (!memcmp(buff, "bye", 3)) {
      // closes the connection automatically AFTER all the buffer was sent.
      Server.close(server, fd_uuid);
    }
  }
}

// running the server
int main(void) {
  // We'll create the echo protocol object. It will be the server's default
  struct Protocol protocol = {.on_open = on_open,
                              .on_close = on_close,
                              .on_data = on_data,
                              .service = "echo"};
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(settings) with the settings we provide)
  start_server(.protocol = &protocol, .timeout = 10, .threads = 8);
}

// easy :-)
```

Using this library requires all the minor libraries written to support it: `libasync`, `libbuffer` (which you can use separately with a few changes) and `libreact`. This means you will need all the `.h` and `.c` files except the HTTP related files. `mini-crypt` and `lib-tls-server` aren't required (nor did I finish writing them).

### A word about concurrency

It should be notes that network applications always have to keep concurrency in mind. For instance, the connection might be closed by one machine while the other is still preparing (or writing) it's response.

If you will use `lib-server`'s multi-threading mode, this concurrency could affect your code (i.e., `on_close` might be called to release resources still in use by `on_data` which might not have finished processing the data). There are plenty of possible solutions for these race conditions, but it is important to note that these race conditions exist.

In addition to multi-threading, `lib-server` allows us to easily setup the network service's concurrency using processes (`fork`ing), which act differently then threads (i.e. memory space isn't shared, so that processes don't share accepted connections).

`lib-server` prevents some race conditions from taking place. For example, `on_data`, `fd_task`s and non-blocking `each` tasks will never run concurrently for the same original connection...

...but, this does not prevent critical callback (such as `on_close` `ping` and `on_shutdown`) from being performed even while a task (such as `on_data`) is running - this is because the execution for these critical callbacks cannot be delayed.

In other words, assume everything could run concurrently. `lib-server` will do it's best to prevent collisions, but it is a generic library, so it might not know what to expect from your application.

## [`http`](docs/http.md) - a protocol for the web

All these libraries are used in a Ruby server I'm re-writing, which has native websocket support ([Iodine](https://github.com/boazsegev/iodine)) - but since the HTTP protocol layer doesn't enter "Ruby-land" before the request parsing is complete, I ended up writing a light HTTP "protocol" in C, following to the `lib-server`'s protocol specs.

The code is just a few helper settings and mega-functions (I know, refactoring will make it easier to maintain). The HTTP parser destructively edits the received headers and forwards a `struct HttpRequest` object to the `on_request` callback. This minimizes data copying and speeds up the process.

The HTTP protocol provides a built-in static file service and allows us to limit incoming request data sizes in order to protect server resources. The header size limit is hard-set during compilation (it's 8KB, which is also the limit on some proxies), securing the server from bloated data DoS attacks. The incoming data size limit is dynamic.

Here's a "Hello World" HTTP server (with a stub to add static file services).

```c
// update the tryme.c file to use the existing folder structure and makefile
#include "http.h"

void on_request(struct HttpRequest request) {
  struct HttpResponse* response = HttpResponse.new(req); // a helper object
  HttpResponse.write_header2(response, "X-Data", "my data");
  HttpResponse.set_cookie(response, (struct HttpCookie){
    .name = "my_cookie",
    .value = "data"
  });
  HttpResponse.write_body(response, "Hello World!\r\n", 14);
  HttpResponse.destroy(response);
}

int main()
{
  char * public_folder = NULL
  start_http_server(on_request, public_folder, .threads = 16);
}
```

Using this library requires all the `http-` prefixed files (`http-mime-types`, `http-request`, `http-status`, `http-objpool`, etc') as well as `lib-server` and all the files it requires. The `http` files are in a separate folder and the makefile in this project supports subfolders. You might want to place all the files in the same folder if you use these source files in a different project.

## [`Websocket`](src/http/websockets.h) - for real-time web applications

At some point I decided to move all the network logic from my [Ruby Iodine project](https://github.com/boazsegev/iodine) to C. This was, in no small part, so I could test my code and debug it with more ease (especially since I still test the network aspect using ad-hock code snippets and benchmarking tools).

This was when the `Websockets` library was born. It builds off the `http` server and allows us to either "upgrade" the HTTP protocol to Websockets or continue with an HTTP response.

Building a Websocket server in C just got super easy, here's both a Wesockets echo and a Websockets broadcast example -notice that broadcasting is a resource intensive task, requiring at least O(n) operations, where n == server capacity:

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

The Websockets implementation uses the `mini-crypt` library for the Base64 encoding and SHA-1 hashing that are part of the protocol's handshake.

---

That's it for now. I'm still working on these as well as on `mini-crypt` and SSL/TLS support (adding OpenSSL might be easy if you know the OpenSSL framework, but I think their API is terrible and I'm looking into alternatives).

## Forking, Contributing and all that Jazz

Sure, why not. If you can add Solaris or Windows support to `libreact`, that could mean `lib-server` would become available for use on these platforms as well (as well as the HTTP protocol implementation and all the niceties).

If you encounter any issues, open an issue (or, even better, a pull request with a fix) - that would be great :-)

If you want to help me write a new SSL/TLS library or have an SSL/TLS solution you can fit into `lib-server`, hit me up.
