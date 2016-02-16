# Server Tools for C

After years in [Ruby land](https://www.ruby-lang.org/en/) I decided to learn [Rust](https://www.rust-lang.org), only to re-discover that I actually quite enjoy writing in C and that C's reputation as "unsafe" or "hard" is undeserved and hides C's power.

So I decided to brush up my C programming skills. My purpose is to, eventually, implement my Ruby [Iodine](https://github.com/boazsegev/iodine) project (an alternative to [EventMachine](https://github.com/eventmachine/eventmachine), which is a wonderful library with what I feel to be a horrid API) in C, making it both faster and allowing it to support way more concurrent connections.

Anyway, Along the way I wrote:

## [`libasync`](/lib/libasync.h) - A native POSIX (`pthread`) thread pool.

 `libasync` uses pipes instead of mutexes, making it both super simple and moving a lot of the context switching optimizations to the kernel layer (which I assume to be well enough optimized).

 `libasync` threads are guarded by "sentinel" threads, so that segmentation faults and errors in any given task won't break the system apart. This was meant to give a basic layer of protection to any server implementation, but I would recommend that it be removed for any other uses (it's a matter or changing one line of code).

 Using `libasync` is super simple and would look something like this:

 ```
 #include "libasync.h"
 #include <stdio.h>
 // this optional function will just print a message when a new thread starts
 void on_new_thread(async_p async, void* arg) {
   printf("%s", (char*)arg);
 }

 // an example task
 void say_hi(void* arg) {
   printf("Hi!\n");
 }
 // This one will fail with safe kernels...
 // On windows you might get a blue screen...
 void evil_code(void* arg) {
   char* rewrite = arg;
   while (1) {
     rewrite[0] = '0';
     rewrite++;
   }
 }

 // an example usage
 int main(void) {
   // this message will be printed by each new thread.
   char msg[] = "*** A new thread is born :-)\n";
   // create the thread pool with 8 threads.
   // the callback is optional (we can pass NULL)
   async_p async = Async.new(8, on_new_thread, msg);
   // send a task
   Async.run(async, say_hi, NULL);
   // an evil task will demonstrate the sentinel at work.
   Async.run(async, evil_code, NULL);
   // wait for all tasks to finish, closing the threads, clearing the memory.
   Async.finish(async);
 }
 ```

 Notice that the last line of output should be our "new thread is born" message, as a new worker thread replaces the one that `evil_code` caused to crash.

 \* Don't run this code on machines with no runtime memory protection (i.e. some windows machines)... our `evil_code` example is somewhat evil.

## [`libreact`](/lib/libreact.h) - KQueue/EPoll abstraction.

It's true, some server programs still use `select` and `poll`... but they really shouldn't be (don't get me started).

When using [`libevent`](http://libevent.org) or [`libev`](http://software.schmorp.de/pkg/libev.html) you could end up falling back on `select` if you're not careful. `libreact`, on the other hand, will simply refuse to compile if neither kqueue nor epoll are available (windows Overlapping IO support would be interesting to write, I guess, but I don't have Windows).

Since I mentioned `libevent` or `libev`, I should point out that even a simple inspection shows that these are amazing and well tested libraries (how did they make those nice benchmark graphs?!)... but I hated their API (or documentation).

It seems to me, that since both `libevent` and `libev` are so all-encompassing, they end up having too many options and functions... I, on the other hand, am a fan of well designed abstractions, even at the price of control. I mean, you're writing a server that should handle 100K concurrent connections - do you really need to manage the socket polling timeouts ("ticks")?! Are you really expecting more than a second to pass with no events?

P.S.

What I would love to write, but I need to learn more before I do so, is a signal based reactor that will be work with all POSIX compilers, using [`sigaction`](http://www.gnu.org/software/libc/manual/html_node/Signal-Actions.html#Signal-Actions) and message pipes... but I want to improve on my site-reading skills first (I'm a musician at heart).

## [`libbuffer`](/lib/libbuffer.h) - a network buffer manager.

It is well known that `send` and `write` don't really send or write everything you ask of them. They do, sometimes, if it's not too much of a bother, but slow network connections (and the advantages of non-blocking IO) often cause them to just return early, with partially sent messages...

Too often we write `send(fd, buffer, len)` and the computer responds with "`len`? noway... too much... how about 2Kb?"...

Hence, a user-land based buffer that keeps track of what was actually sent is required (either that or have your thread wait for the network connection and slow down the whole server application).

`libbuffer` is both "packet" based and zero-copy oriented (although, if you prefer, it will copy the data). In other words, `libbuffer` is a bin-tree wrapper with some comfortable helpers.

`libbuffer` is super friendly, you can even ask it to close the connection once all the data was sent, if you want it to.

## [`lib-server`](/lib/lib-server.h) - a server building library.

Writing server code is fun... but in limited and controlled amounts... after all, much of it simple code being repeated endlessly, connecting one piece of code with a different piece of code.

`lib-server` is aimed at writing unix based (linux/BSD) servers. It uses `libreact` as the reactor, `libasync` to handle some tasks (the `on_data` callback will be performed asynchronously) and `libbuffer` for easily writing data asynchronously.

`lib-server` might not be optimized to your liking, but it's all working great for me. Besides, it's code is heavily commented code, easy to edit and tweak. To offer some comparison, `ev.c` from `libev` has ~5000 lines (and there's no server just yet, while `libreact` is less then 400 lines)...

Using `lib-server` is super simple to use. It's based on Protocol structure and callbacks, so that we can dynamically change protocols and support stuff like HTTP upgrade requests. Here's a simple example:

```
#include "lib-server.h"
#include <stdio.h>
#include <string.h>

// we don't have to, but printing stuff is nice...
void on_open(server_pt server, int sockfd) {
  printf("A connection was accepted on socket #%d.\n", sockfd);
}
// server_pt is just a nicely typed pointer, which is there for type-safty.
// The Server API is used by calling `Server.api_call` (often with the pointer).
void on_close(server_pt server, int sockfd) {
  printf("Socket #%d is now disconnected.\n", sockfd);
}

// a simple echo... this is the main callback
void on_data(server_pt server, int sockfd) {
  // We'll assign a reading buffer on the stack
  char buff[1024];
  ssize_t incoming = 0;
  // Read everything, this is edge triggered, `on_data` won't be called
  // again until all the data was read.
  while ((incoming = Server.read(sockfd, buff, 1024)) > 0) {
    // since the data is stack allocated, we'll write a copy
    // otherwise, we'd avoid a copy using Server.write_move
    Server.write(server, sockfd, buff, incoming);
    if (!memcmp(buff, "bye", 3)) {
      // closes the connection automatically AFTER all the buffer was sent.
      Server.close(server, sockfd);
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
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = &protocol, .timeout = 10, .threads = 8);
}

// easy :-)
```

## [`http-protocol`](https://github.com/boazsegev/c-server-tools/blob/master/lib/http-protocol.c) - a protocol for the web

All these libraries are used in a Ruby server I'm writing, which has native websocket support ([Iodine](https://github.com/boazsegev/iodine)) - but since the HTTP protocol layer doesn't enter "Ruby-land" before the request parsing is complete, I ended up writing a light HTTP "protocol" according to the `lib-server`'s protocol specs.

The code is just a few mega-functions (I know, it needs refactoring) that parse the HTTP request byte by byte and forward it to an `on_request` callback (using an HTTPRequest struct). There's also a built-in static file service and a default on_requests that basically echoes the HTTP request as a response.

Here's a "Hello World" HTTP server (with static file services, if you want them).

```c
#include "http-protocol.h"

void on_request(struct HttpRequest* req) {
  // a simple "Hello World"
  static char hello[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout = 1\r\n"
      "\r\n"
      "Hello World\r\n";
  Server.write(req->server, req->sockfd, hello, sizeof(hello));
}

void on_request()
int main() {
  struct HttpProtocol protocol = HttpProtocol();
  protocol.on_request = on_request;
  protocol.public_folder = "www";
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = (struct Protocol*)(&protocol), .timeout = 1,
               .threads = 8);
}
```

Using this library requires all the `http-` prefixed files (`http-mime-types`, `http-request`, `http-status`).

## A note about versions

v.0.1.0 is uses the main thread for the reactor pattern and optionally creates worker threads for the actual tasks (see the server settings).

v.0.2.0 runs the reactor within the worker thread pool, which helps multi-process concurrency by preventing the reactor from accepting new connections when all the working threads are busy.

This also means that using a single working thread with v.0.2.0 is totally single threaded as far as thread safety is concerned (even pings and closure callbacks will be called in order).

---

That's it for now. I might work on these more later, but I'm super excited to be going back to my music school, Berklee, so I'll probably forget all about computers for a while... but I'll be back tinkering away at some point.
