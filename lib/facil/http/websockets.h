/*
copyright: Boaz Segev, 2016-2019
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_WEBSOCKETS_H
#define H_WEBSOCKETS_H

#include <http.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/** used internally: attaches the Websocket protocol to the socket. */
void websocket_attach(intptr_t uuid,
                      http_settings_s *http_settings,
                      websocket_settings_s args,
                      void *data_in_buffer,
                      size_t length);

/* *****************************************************************************
Websocket information
***************************************************************************** */

/** Returns the opaque user data associated with the websocket. */
void *websocket_udata_get(ws_s *ws);

/**
 * Sets the opaque user data associated with the websocket.
 *
 * Returns the old value, if any.
 */
void *websocket_udata_set(ws_s *ws, void *udata);

/**
 * Returns the underlying socket UUID.
 *
 * This is only relevant for collecting the protocol object from outside of
 * websocket events, as the socket shouldn't be written to.
 */
intptr_t websocket_uuid(ws_s *ws);

/**
 * Returns 1 if the WebSocket connection is in Client mode (connected to a
 * remote server) and 0 if the connection is in Server mode (a connection
 * established using facil.io's HTTP server).
 */
uint8_t websocket_is_client(ws_s *ws);

/* *****************************************************************************
Websocket Connection Management (write / close)
***************************************************************************** */

/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, fio_str_info_s msg, uint8_t is_text);

/** Closes a websocket connection. */
void websocket_close(ws_s *ws);

/* *****************************************************************************
Websocket Pub/Sub
=================

API for websocket pub/sub that can be used to publish messages across process
boundries.

Supports pub/sub engines (see {fio.h}) that can connect to a backend service
such as Redis.

The default pub/sub engine (if `NULL` or unspecified) will publish the messages
to the process cluster (all the processes in `fio_start`).
***************************************************************************** */

FIO_IFUNC subscribe_args_s
websocket_subscribe_update_args(ws_s *ws, subscribe_args_s args) {
  args.uuid = websocket_uuid(ws);
  if (!args.on_message) {
  }
  return args;
}

/** INTERNAL: helper for the websocket_subscribe macro */
FIO_IFUNC void websocket_subscribe(ws_s *ws, subscribe_args_s args) {
  if (!ws)
    goto err;
  args = websocket_subscribe_update_args(ws, args);
  fio_subscribe FIO_NOOP(args);
  return;
err:
  fio_defer(args.on_unsubscribe, args.udata1, args.udata2);
}

/** INTERNAL: helper for the websocket_unsubscribe macro */
FIO_IFUNC void websocket_unsubscribe(ws_s *ws, subscribe_args_s args) {
  if (!ws)
    return;
  args = websocket_subscribe_update_args(ws, args);
  fio_unsubscribe_uuid FIO_NOOP(args);
}

/**
 * Subscribes the WebSocket's UUID to a channel. See {fio_subscribe} for
 * possible arguments.
 *
 * On failure, the `on_unsubscribe` callback is scheduled.
 *
 * All subscriptions are automatically revoked once the websocket is closed.
 *
 * The `ws` object is avilable through the message's `pr` (`fio_protocol_s`)
 * field. Simply cast to `ws_s*`.
 *
 * If the connections subscribes to the same channel more than once, it may
 * cancle a previous subscription (though, for a time, both subscriptions may
 * exist).
 */
#define websocket_subscribe(wbsckt, ...)                                       \
  websocket_subscribe((wbsckt), (subscribe_args_s){__VA_ARGS__})

/**
 * Revokes a WebSocket's subscription to a channel. See {fio_unsubscribe_uuid}
 * for more details.
 *
 * All subscriptions are automatically revoked once the websocket is closed.
 *
 * The `on_unsubscribe` callback is automatically scheduled.
 */
#define websocket_unsubscribe(wbsckt, ...)                                     \
  websocket_unsubscribe((wbsckt), (subscribe_args_s){__VA_ARGS__})

/** Optimize generic broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB (-32)
/** Optimize text broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB_TEXT (-33)
/** Optimize binary broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB_BINARY (-34)

/**
 * Enables (or disables) broadcast optimizations.
 *
 * This is performed automatically by the `websocket_subscribe` function.
 * However, this function is provided for enabling the pub/sub metadata based
 * optimizations for external connections / subscriptions.
 *
 * This function allows enablement (or disablement) of these optimizations:
 *
 * * WEBSOCKET_OPTIMIZE_PUBSUB - optimize all direct transmission messages,
 *                               best attempt to detect Text vs. Binary data.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_TEXT - optimize direct pub/sub text messages.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_BINARY - optimize direct pub/sub binary messages.
 *
 * Note: to disable an optimization it should be disabled the same amount of
 * times it was enabled - multiple optimization enablements for the same type
 * are merged, but reference counted (disabled when reference is zero).
 *
 * Note2: The pub/sub metadata type ID will match the optimnization type
 * requested (i.e., `WEBSOCKET_OPTIMIZE_PUBSUB`) and the optimized data is a
 * FIOBJ String containing a pre-encoded WebSocket packet ready to be sent.
 * i.e.:
 *
 *     FIOBJ pre_wrapped = (FIOBJ)fio_message_metadata(msg,
 *                               WEBSOCKET_OPTIMIZE_PUBSUB);
 *     fiobj_send_free((intptr_t)msg->udata1, fiobj_dup(pre_wrapped));
 */
void websocket_optimize4broadcasts(intptr_t type, int enable);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
