# Server Tools for C

After years in [Ruby land](https://www.ruby-lang.org/en/) I decided to learn [Rust](https://www.rust-lang.org), only to re-discover that I actually quite enjoy writing in C and that C's reputation as "unsafe" or "hard" is undeserved and hides C's power.

So I decided to brush up my C programming skills. My purpose is to, eventually, implement my Ruby [Iodine](https://github.com/boazsegev/iodine) project (an alternative to [EventMachine](https://github.com/eventmachine/eventmachine), which is a wonderful library with what I feel to be a horrid API) in C, making it both faster and allowing it to support way more concurrent connections.

Anyway, Along the way I wrote:

* [`libasync`](/lib/libasync.h) - a small C library that handles a native POSIX (`pthread`) thread pool.

     `libasync` uses pipes instead of mutexes, making it both super simple and moving a lot of the context switching optimizations to the kernel layer (which I assume to be well enough optimized).

     `libasync` threads are guarded by "sentinel" threads, so that segmentation faults and errors in any given task will break the system apart. This was meant to give a basic layer of protection to any server implementation, but I would recommend that it be removed for any other uses.

     Using `libasync` is super simple and would look something like this (the NULL allows for an initialization callback, ignore it for now):

     ```
     // an example task
     void say_hi(void * arg)
     {
       printf("Hi!");
     }

     // an example usage     
     int main(void)
     {
       // create the thread pool
       async_p async = Async.new(8, NULL); // 8 threads
       // send a task
       Async.run(async, say_hi, NULL);
       // wait for all tasks to finish, closing the threads, clearing the memory.
       Async.finish(async)
     }
     ```

* [`libreact`](/lib/libreact.h) - a small KQueue/EPoll wrapper.

    It's true, some server programs still use `select` and `poll`... but they really shouldn't be (don't get me started).

    When using [`libevent`](http://libevent.org) or [`libev`](http://software.schmorp.de/pkg/libev.html) you could end up falling back on `select` if you're not careful. `libreact`, on the other hand, will simply refuse to compile if neither kqueue nor epoll are available (windows Overlapping IO support would be interesting to write, I guess).

    Since I mentioned `libevent` or `libev`, I should point out that even a simple inspection shows that these are amazing and well tested libraries (how did they make those nice benchmark graphs?!)... but I hated their API (or documentation).

    It seems to me, that since both `libevent` and `libev` are so general targeted, they end up having too many options and functions... I, on the other hand, am a fan of well designed abstractions, even at the price of control. I mean, you're writing a server that should handle 100K concurrent connections - do you really need to manage the socket polling timeouts ("ticks")?! Are you really expecting more than a second to pass with no events?

    P.S.

    What I would love to write, but I need to learn more before I do so, is a signal based reactor that will be work with all POSIX compilers, using `sigaction` and message pipes... but I want to improve on my site-reading skills first (I'm a musician at heart).

* [`libbuffer`](/lib/libbuffer.h) - a network buffer manager.

    It is well known that `send` and `write` don't really send or write everything you ask of them. They do, sometimes, if it's not too much of a bother, but slow network connections (and the advantages of non-blocking IO) often cause them to just return early, with partially sent messages...

    Too often we write `send(fd, buffer, len)` and the computer responds with "`len`? noway... too much... how about 2Kb?"...

    Hence, a user-land based buffer that keeps track of what was actually sent is required.

    `libbuffer` is both "packet" based and zero-copy oriented (although, if you prefer, it will copy the data). In other words, `libbuffer` is a bin-tree wrapper with some comfortable helpers.

    `libbuffer` is super friendly, you can even ask it to close the connection once all the data was sent, if you want it to.

* [`lib-server`](/lib/lib-server.h) - a server building library.

    Writing server code is fun... but in limited and controlled amounts... after all, much of it simple code being repeated endlessly, connecting one piece of code with a different piece of code.

    `lib-server` is aimed at writing unix based (linux/BSD) servers. It uses `libreact` as the reactor, `libasync` to handle some tasks (the `on_data` callback will be performed asynchronously) and `libbuffer` for easily writing data asynchronously.

    `lib-server` might not be optimized to your liking, but it's all working great for me. Besides, it's less than 1000 lines of heavily commented code, easy to edit and tweak. To offer some comparison, `ev.c` from `libev` has ~5000 lines (and there's no server just yet)...

    Using `lib-server` is super simple to use. It's based on Protocol structure and callbacks, so that we can dynamically change protocols and support stuff like HTTP upgrade requests. Here's a simple example:

    ```
    #include <lib-server.h>
    #include <string.h>

    // we don't have to, but printing stuff is nice...
    void on_open(struct Server * server, int sockfd) {
      printf("A connection was accepted on socket %d.\n", sockfd)
    }
    void on_close(struct Server * server, int sockfd) {
      printf("Socket %d is now disconnected.\n", sockfd)
    }

    // a simple echo... this is the main callback
    void on_data(struct Server * server, int sockfd)
    {
      // We'll assign a reading buffer on the stack
      char buff[1024];
      ssize_t incoming = 0;
      // Read everything, this is edge triggered, `on_data` won't be called
      // again until all the data was read.
      while((incoming = Server.read(sockfd, buff, 1024)) > 0) {
        // since the data is stack allocated, we'll write a copy
        // otherwise, we'd avoid a copy using Server.write_move
        Server.write(server, sockfd, buff, incoming);
        if(!memcmp(buff, "bye", 3)) {
          // closes the connection automatically AFTER all the buffer was sent.
          Server.close(server, sockfd);
        }
      }
    }

    // running the server    
    int main(void)
    {
      // We'll create the echo protocol object. It will be the server's default
      struct Protocol protocol = {  .on_open = on_open,
                                    .on_close= on_close,
                                    .on_data = on_data,
                                    .service="echo"     };
      // We'll use the macro start_server, because our settings are simple.
      // (this will call Server.listen(&settings) with the settings we provide)
      start_server(.protocol = &protocol, .timeout = 10, .threads = 8);
    }

    // easy :-)
    ```



That's it for now. I might work on these more later, but I'm super excited to be going back to my music school, Berklee, so I'll probably forget all about computers for a while... but I'll be back tinkering away at some point.
