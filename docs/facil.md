# Facil.io - a dynamic protocol network services library

`facil.h` is "A Library (or mini-framework) for Dynamic Protocol based Network Services":

* A Library (or a mini framework): meaning, closer to the metal, abstracting only what is required for API simplicity, security, error protection and performance. `facil.io` doesn't enforce any external dependencies.

* For Dynamic Protocol: meaning services can change protocols mid-stream. Example use-case would be the HTTP/1.1 upgrade request to Websockets or HTTP/2. HTTP/2 isn't supported just yet, but the current design is easily extendable.

* Network services: meaning implementing client-server or peer2peer network applications, such as web applications, micro-services, etc'.

`facil.h` utilizes `evio.h`, `defer.h` and `sock.h` to create a simple API wrapper around these minimalistic libraries and managing the "glue" that makes them work together for a whole that's greater than the sum of it's parts.

It's simple, it's awesome, and I love it. But why trust me when it's all clearly open-source and you can test it yourself? ;-)

But if you'd rather write the whole thing yourself, I recommend starting with [Beej's guide](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) as a good source for information.

## Core Concepts And A Quick Example

Facil.io aims to provide a simple API, so that developers can focus on developing their applications rather then learning new APIs.

However, server applications are not the simplest of beasts, so if you encounter some minor complexity, I hope this documentation can help elevate the situation.

### The Protocol: How Network Services Communicate

By nature, network services implement higher layer protocols to communicate (higher then TCP/IP). The HTTP protocol is a common example.

Typically, server applications "react", they read incoming data (known as a request), perform a task and send output (a response).

The base type to handle the demands of protocols is `protocol_s`. This is no more then a struct with information about the callbacks that should be invoked on network events (incoming data, disconnection etc').

Protocol object instances must be unique per connection, and so they are usually dynamically allocated.

```c
typedef struct {
  /**
   * A string to identify the protocol's service (i.e. "http").
   *
   * The string should be a global constant, only a pointer comparison will be
   * used (not `strcmp`).
   */
  const char *service;
  /** called when a data is available, but will not run concurrently */
  void (*on_data)(intptr_t uuid, protocol_s *protocol);
  /** called when the socket is ready to be written to. */
  void (*on_ready)(intptr_t uuid, protocol_s *protocol);
  /** called when the server is shutting down,
   * but before closing the connection. */
  void (*on_shutdown)(intptr_t uuid, protocol_s *protocol);
  /** called when the connection was closed, but will not run concurrently */
  void (*on_close)(protocol_s *protocol);
  /** called when a connection's timeout was reached */
  void (*ping)(intptr_t uuid, protocol_s *protocol);
  /** private metadata used by facil. */
  size_t rsv;
} protocol_s; ///*
```

Protocol object instances should be initiated per connection, since `facil.io` uses a locking mechanism that prevents the same protocol object from running it's callbacks concurrently.

By reviewing the HTTP and Websocket examples in the facil.io codebase, it's easy to see that easily the `protocol_s` "class" can be extended to add more data / features that might be required. "C" objects can use a typecasting stye of inheritance which comes very handy when implementing network protocols.

---

### More documentation coming soon. For now, the comments in the header files should provide great documentation and they can be used with Doxygen (or any documentation creation tool).
