#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include "http_request.h"
#include "http_response.h"

/**
The Websocket type is an opaque type used by the websocket API to provide
identify a specific Websocket connection and manage it's internal state.
*/
typedef struct Websocket ws_s;

/**
The protocol / service identifier for `libserver`.
*/
extern char* WEBSOCKET_ID_STR;
/**
The Websocket Handler contains all the settings required for new websocket
connections.

This struct is used for the named agruments in the `websocket_upgrade`
macro.
*/
typedef struct {
  /**
  The (optional) on_message callback will be called whenever a websocket message
  is
  received for this connection.

  The data received points to the websocket's message buffer and it will be
  overwritten once the function exits (it cannot be saved for later, but it can
  be copied).
  */
  void (*on_message)(ws_s* ws, char* data, size_t size, uint8_t is_text);
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
  /** The `http_request_s` to be converted ("upgraded") to a websocket
   * connection. Either a request or a response object is required.*/
  http_request_s* request;
  /**
  The (optional) HttpResponse to be used for sending the upgrade response.

  Using this object allows cookies to be set before "upgrading" the connection.

  The ownership of the response object will remain unchanged - so if you have
  created the response object, you should free it.
  */
  http_response_s* response;
  /**
  The maximum websocket message size/buffer (in bytes) for this connection.
  */
  size_t max_msg_size;
  /** Opaque user data. */
  void* udata;
  /**
  Timeout for the websocket connections, a ping will be sent
  whenever the timeout is reached. Connections are only closed when a ping
  cannot be sent (the network layer fails). Pongs aren't reviewed.
  */
  uint8_t timeout;
} websocket_settings_s;

/** This macro allows easy access to the `websocket_upgrade` function. The macro
 * allows the use of named arguments, using the `websocket_settings_s` struct
 * members. i.e.:
 *
 *     on_message(ws_s * ws, char * data, size_t size, int is_text) {
 *        ; // ... this is the websocket on_message callback
 *        websocket_write(ws, data, size, is_text); // a simple echo example
 *     }
 *
 *     on_request(http_request_s* request) {
 *        websocket_upgrade( .request = request, .on_message = on_message);
 *     }
 *
 * Returns 0 on sucess and -1 on failure. A response is always sent.
 */
ssize_t websocket_upgrade(websocket_settings_s settings);
#define websocket_upgrade(...) \
  websocket_upgrade((websocket_settings_s){__VA_ARGS__})

/** Returns the opaque user data associated with the websocket. */
void* websocket_get_udata(ws_s* ws);
/** Returns the the process specific connection's UUID (see `libsock`). */
intptr_t websocket_get_fduuid(ws_s* ws);
/** Sets the opaque user data associated with the websocket.
 * Returns the old value, if any. */
void* websocket_set_udata(ws_s* ws, void* udata);
/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s* ws, void* data, size_t size, uint8_t is_text);
/** Closes a websocket connection. */
void websocket_close(ws_s* ws);
/**
Performs a task on each websocket connection that shares the same process
(except the originating `ws_s` connection which is allowed to be NULL).
 */
void websocket_each(ws_s* ws_originator,
                    void (*task)(ws_s* ws_target, void* arg),
                    void* arg,
                    void (*on_finish)(ws_s* ws_originator, void* arg));
/**
Counts the number of websocket connections.
*/
size_t websocket_count(ws_s* ws);

#endif
