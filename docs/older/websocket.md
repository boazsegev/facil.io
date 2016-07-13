# The Websocket `lib-server` extension

The Websocket library extension (which requires the HTTP extension) adds support for the Websocket Protocol, allowing real-time communication for web-applications (including browser based clients an).

The Websocket extension is comprised of two main parts:

* [The Websocket Protocol (`struct HttpProtocol`) global state].

* [The Websocket Connection (`struct HttpProtocol`) instance state].

The following information is a quick guide. For updated and full information, read the code and it's comments.

At the end of this file you will find a short example for a simple "Hello World" web service.

## The Websocket API

To implement the Websocket protocol in a way that is optimized for your application, such as securing limits for incoming message sizes etc', some settings are available.

These settings can be set at any time, but will only take affect after they had been set.

The global settings are available using the global `Websocket` object:

```c
extern struct {
  int max_msg_size; // Sets the (global) maximum websocket message size/buffer per client.
  unsigned char timeout; // Sets the (global) timeout for websocket connections (ping).
  // ...
} Websocket;
```

### The `max_msg_size` global property

Websocket messages don't have a practical limitation on their size, much like HTTP uploads. This could expose Websocket services to DoS attacks that upload large amounts of data and cause resource starvation.

The `max_msg_size` allows us to set a practical global (and dynamic) limit on incoming Websocket messages.

To set a limit, set `max_msg_size` to the maximum number of Bytes allowed per message.

The default limit value is ~256KB (`Websocket.max_msg_size = 262144`).

Use:

```c
Websocket.max_msg_size = 1024*1024*2 // ~2Mb

```

Please note that this limit is the limit per message. It is easy to stream a 200Gb file to the server by using fragmentation, i.e. uploading 128Kb at a time and having the server merge the pieces of the data together after they were received (perhaps saving them to a temporary file). This approach of using fragmentation is safer and it allows easier recovery from disconnections (by sending only the missing data).

### The `timeout` semi-global property

Websocket connections are designed to persist over time, but servers, routers and all intermediaries have an interest in clearing out stale connections and preventing network errors (i.e. half closed sockets) from causing system resource starvation (the number of allowed connections on a system is always limited and each open connection, active or not, takes system resources).

This interest means that most systems have a timeout setup. If no data was sent or received during the timeout period, the connection is assumed to have died and is forcefully closed.

i.e. Heroku's, router sets the first timeout window to 30 seconds (usually the HTTP response / upgrade timeout) and 55 seconds for later timeout windows, forcing Websocket connections to close after 55 seconds of inactivity.

The `timeout` semi-global property allows us to deal with both the issue of stale connections and the issue of our "host" system's timeout limits.

Instead of closing the connection automatically, the `timeout` property causes a Websocket `ping` to be sent. If the connection was lost, this will cause the TCP/IP connection chain to collapse (intermediaries that fail to forward the packet will shutdown the connection). Once the TCP/IP connection chain collapsed, future attempt to `write` to the Websocket (including `ping` packets) will fail and flag the connection for closure.

This allows us to use `timeout` both to keep a connection alive and to close "dead" (half-closed) connections.

Use:

```c
// Heroku's timeout is 55 seconds, but we should keep a safe distance.
Websocket.timeout = 45
```

It should be noted that `timeout` uses fuzzy timeout counting that might be enforced a second or two later then expected (and maybe later then the 2 second fuzzy window when the server experiences heavy load).

Also, the `timeout` property isn't strictly global. Each new Websocket connection inherits the `timeout` property value. Dynamic updates effect only new connections.

### `websocket_upgrade(...)`

This macro is actually a short cut for the `Websocket.upgrade` function and allows to easily pass arguments to the function.

```c
#define websocket_upgrade(...) \
  Websocket.upgrade((struct WebsocketSettings){__VA_ARGS__})

```

To Upgrade a connection, the minimal requirement is to pass a pointer to the `HttpRequest` object and a callback that handles any `on_message`

The `on_message` callback should expect 4 arguments:

1. `ws_s* ws` - a pointer to the Websocket connection instance.
2. `char* buffer` - the buffer containing the Websocket message. This buffer will be automatically recycled once the `on_message` callback returns.
3. `size_t size` - the amount of data present in the buffer.
4. `int is_text` - a flag indicating if the data is valid UTF-8 encoded text (`is_text == 1`) or binary data (`is_text==0`).

All other callbacks, such as the `on_open`, `on_close` and `on_shutdows` take only a single argument, the `ws_s *`.

Other, optional, settings are defined under the `WebsocketSettings` struct:

```c
struct WebsocketSettings {
  /**
  The on_message callback will be called whenever a websocket message is
  received for this connection.

  The data received points to the websocket's message buffer and it will be
  overwritten once the function exits (it cannot be saved for later, but it can
  be copied).
  */
  void (*on_message)(ws_s* ws, char* data, size_t size, int is_text);
  /**
  The (optional) on_open callback will be called once the websocket connection
  is
  established.
  */
  void (*on_open)(ws_s* ws);
  /**
  The (optional) on_shutdown callback will be called if a websocket connection
  is still open while the server is shutting down (called before `on_close`).
  */
  void (*on_shutdown)(ws_s* ws);
  /**
  The (optional) on_close callback will be called once a websocket connection is
  terminated or failed to be established.
  */
  void (*on_close)(ws_s* ws);
  /** The (required) HttpRequest to be converted ("upgraded") to a websocket
   * connection. */
  struct HttpRequest* request;
  /**
  The (optional) HttpResponse to be used for sending the upgrade response.

  Using this object allows cookies to be set before "upgrading" the connection.

  The ownership of the response object will remain unchanged - so if you have
  created the response object, you should free it.
  */
  struct HttpResponse* response;
  /** Opaque user data. */
  void* udata;
};
```

### `void* Websocket.get_udata(ws_s* ws)`

Returns the opaque user data associated with the websocket.

The opaque data can be set either before or after a connection was established.

### `void* Websocket.set_udata(ws_s* ws, void* udata)`

Sets the opaque user data associated with the websocket. returns the old value, if any.

### `server_pt Websocket.get_server(ws_s* ws)`

Returns the the `server_pt` for the Server object that owns the connection. This allows access to the underlying `lib-server` API.

### `uint64_t Websocket.get_uuid(ws_s* ws)`

Returns the the connection's UUID. This is the same as the `lib-server` assigned UUID and can be used to extract the `fd` for the underlying socket (which you probably don't want to do).

### `int Websocket.write(ws_s* ws, void* data, size_t size, char is_text)`

Writes data to the websocket. Returns -1 on failure (0 on success).

Set the `is_text` argument to 1 if the data is a valid UTF-8 encoded text string. To send binary data, set the `is_text` argument to 0.

### `void Websocket.close(ws_s* ws)`

Closes a websocket connection.

### `void Websocket.each(...)`

Performs a task on each Websocket connection that shares the same process, except the Websocket connection pointed to by `ws_originator`.

The full function arguments are:

```c
void Websocket.each(ws_s* ws_originator,
                    void (*task)(ws_s* ws_target, void* arg),
                    void* arg,
                    void (*on_finish)(ws_s* ws_originator, void* arg));
```

`ws_originator` can be NULL if the task should be performed on all the connections, including the one scheduling the task.

### `long Websocket.count(ws_s* ws);`

Counts the number of websocket connections in the process (forked processes are ignored).

## A Quick Example

Here's a simple "Hello World" using the Http extensions:

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
