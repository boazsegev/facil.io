#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include "http.h"

/**
The Websocket type is an opaque type used by the websocket API to provide
identify a specific Websocket connection and manage it's internal state.
*/
typedef struct Websocket ws_s;
/**
The Websocket Handler contains all the settings required for new websocket
connections.

This struct is used for the named agruments in the `websocket_upgrade`
macro.
*/
struct WebsocketSettings {
  /**
  The (optional) on_message callback will be called whenever a websocket message
  is
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

/** This macro allows easy access to the `Websocket.upgrade` function. The macro
 * allows the use of named arguments, using the WebsocketSettings struct
 * members. i.e.:
 *
 *     on_message(ws_s * ws, char * data) {
 *        ; // ... this is the websocket on_message callback
 *        Websocket.write(ws, data); // a simple echo example
 *     }
 *
 *     on_request(struct HttpRequest* request) {
 *        websocket_upgrade( .request = request, .on_message = on_message);
 *     }
 *
 */
#define websocket_upgrade(...) \
  Websocket.upgrade((struct WebsocketSettings){__VA_ARGS__})
/**
NOT IMPLEMENTED - will cause segfault.
*/
extern struct Websockets_API__ {
  /**
  Sets the (global) maximum websocket message size/buffer per client.
  */
  int max_msg_size;
  /**
  Sets the (global) timeout for websocket connections, a ping will be sent
  whenever the timeout is reached. Connections are only closed when a ping
  cannot be sent (the network layer fails). Pongs aren't reviewed.
  */
  unsigned char timeout;
  /** Upgrades an existing HTTP connection to a Websocket connection. */
  void (*upgrade)(struct WebsocketSettings settings);
  /** Returns the opaque user data associated with the websocket. */
  void* (*get_udata)(ws_s* ws);
  /** Returns the the `server_pt` for the Server object that owns the connection
   */
  server_pt (*get_server)(ws_s* ws);
  /** Returns the the connection's UUID (the Server's connection ID). */
  uint64_t (*get_uuid)(ws_s* ws);
  /** Sets the opaque user data associated with the websocket. returns the old
   * value, if any. */
  void* (*set_udata)(ws_s* ws, void* udata);
  /** Writes data to the websocket. Returns -1 on failure (0 on success). */
  int (*write)(ws_s* ws, void* data, size_t size, char text);
  /** Closes a websocket connection. */
  void (*close)(ws_s* ws);
  /**
  Performs a task on each websocket connection that shares the same process.

  NOT IMPLEMENTED.
   */
  int (*each)(ws_s* ws_originator,
              void (*task)(ws_s* ws_target, void* arg),
              void* arg,
              void (*on_finish)(ws_s* ws_originator, void* arg));
  /**
  */
} Websocket;

#endif
