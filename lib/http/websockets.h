/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include "http.h"

/**
The Websocket type is an opaque type used by the websocket API to provide
identify a specific Websocket connection and manage it's internal state.
*/
typedef struct Websocket ws_s;

/**
The protocol / service identifier for `libserver`.
*/
extern char *WEBSOCKET_ID_STR;
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
  void (*on_message)(ws_s *ws, char *data, size_t size, uint8_t is_text);
  /**
  The (optional) on_open callback will be called once the websocket connection
  is established and before is is registered with `facil`, so no `on_message`
  events are raised before `on_open` returns.
  */
  void (*on_open)(ws_s *ws);
  /**
  The (optional) on_ready callback will be after a the underlying socket's
  buffer changes it's state from full to available.

  If the socket's buffer is never full, the callback is never called.

  It should be noted that `libsock` manages the socket's buffer overflow and
  implements and augmenting user-land buffer, allowing data to be safely written
  to the websocket without worrying over the socket's buffer.
  */
  void (*on_ready)(ws_s *ws);
  /**
  The (optional) on_shutdown callback will be called if a websocket connection
  is still open while the server is shutting down (called before `on_close`).
  */
  void (*on_shutdown)(ws_s *ws);
  /**
  The (optional) on_close callback will be called once a websocket connection is
  terminated or failed to be established.
  */
  void (*on_close)(ws_s *ws);
  /** The `http_request_s` to be converted ("upgraded") to a websocket
   * connection. Either a request or a response object is required.*/
  http_request_s *request;
  /**
  The (optional) HttpResponse to be used for sending the upgrade response.

  Using this object allows cookies to be set before "upgrading" the connection.

  The ownership of the response object will remain unchanged - so if you have
  created the response object, you should free it.
  */
  http_response_s *response;
  /**
  The maximum websocket message size/buffer (in bytes) for this connection.
  */
  size_t max_msg_size;
  /** Opaque user data. */
  void *udata;
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
#define websocket_upgrade(...)                                                 \
  websocket_upgrade((websocket_settings_s){__VA_ARGS__})

/** Returns the opaque user data associated with the websocket. */
void *websocket_udata(ws_s *ws);
/**
Returns the underlying socket UUID.

This is only relevant for collecting the protocol object from outside of
websocket events, as the socket shouldn't be written to.
*/
intptr_t websocket_uuid(ws_s *ws);
/**
Sets the opaque user data associated with the websocket.

Returns the old value, if any.
*/
void *websocket_udata_set(ws_s *ws, void *udata);
/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, void *data, size_t size, uint8_t is_text);
/** Closes a websocket connection. */
void websocket_close(ws_s *ws);
/**
Counts the number of websocket connections.
*/
size_t websocket_count(void);

/** The named arguments for `websocket_each` */
struct websocket_each_args_s {
  /** The websocket originating the task. It will be excluded for the loop. */
  ws_s *origin;
  /** The task (function) to be performed. This is required. */
  void (*task)(ws_s *ws_target, void *arg);
  /** User opaque data to be passed along. */
  void *arg;
  /** The on_finish callback is always called. Good for cleanup. */
  void (*on_finish)(ws_s *origin, void *arg);
};
/**
Performs a task on each websocket connection that shares the same process
(except the originating `ws_s` connection which is allowed to be NULL).
 */
void websocket_each(struct websocket_each_args_s args);
#define websocket_each(...)                                                    \
  websocket_each((struct websocket_each_args_s){__VA_ARGS__})

/**
The Arguments passed to the `websocket_write_each` function / macro are defined
here, for convinience of calling the function.
*/
struct websocket_write_each_args_s {
  /** The originating websocket client will be excluded from the `write`.
   * Can be NULL. */
  ws_s *origin;
  /** The data to be written to the websocket - required(!) */
  void *data;
  /** The length of the data to be written to the websocket - required(!) */
  size_t length;
  /** Text mode vs. binary mode. Defaults to binary mode. */
  uint8_t is_text;
  /** Set to 1 to send the data to websockets where this application is the
   * client. Defaults to 0 (the data is sent to all clients where this
   * application is the server). */
  uint8_t as_client;
  /** A filter callback, allowing us to exclude some clients.
   * Should return 1 to send data and 0 to exclude. */
  uint8_t (*filter)(ws_s *ws_to, void *arg);
  /** A callback called once all the data was sent. */
  void (*on_finished)(ws_s *ws_origin, void *arg);
  /** A user specified argumernt passed to each of the callbacks. */
  void *arg;
};
/**
Writes data to each websocket connection that shares the same process
(except the originating `ws_s` connection which is allowed to be NULL).

Accepts a sing `struct websocket_write_each_args_s` argument. See the struct
details for possible arguments.
 */
int websocket_write_each(struct websocket_write_each_args_s args);
#define websocket_write_each(...)                                              \
  websocket_write_each((struct websocket_write_each_args_s){__VA_ARGS__})

#endif
