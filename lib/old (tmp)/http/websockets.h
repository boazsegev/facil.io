/*
copyright: Boaz Segev, 2016-2018
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_WEBSOCKETS_H
#define H_WEBSOCKETS_H

#include "http.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
The protocol / service identifier.
*/
extern char *WEBSOCKET_ID_STR;

/** used internally: attaches the Websocket protocol to the socket. */
void websocket_attach(intptr_t uuid, http_settings_s *http_settings,
                      websocket_settings_s *args, void *data, size_t length);

/* *****************************************************************************
Websocket information
***************************************************************************** */

/** Returns the opaque user data associated with the websocket. */
void *websocket_udata(ws_s *ws);

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

/**
 * Counts the number of websocket connections in this process.
 */
size_t websocket_count(void);

/* *****************************************************************************
Websocket Connection Management (write / close)
***************************************************************************** */

/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, void *data, size_t size, uint8_t is_text);
/** Closes a websocket connection. */
void websocket_close(ws_s *ws);

/* *****************************************************************************
Websocket Pub/Sub
=================

API for websocket pub/sub that can be used to publish messages across process
boundries.

Supports pub/sub engines (see {pubsub.h}) that can connect to a backend service
such as Redis.

The default pub/sub engine (if `NULL` or unspecified) will publish the messages
to the process cluster (all the processes in `fio_run`).

To publish to a channel, use the API provided in {pubsub.h}.
***************************************************************************** */

/** Incoming pub/sub messages will be passed along using this data structure. */
typedef struct {
  /** the websocket receiving the message. */
  ws_s *ws;
  /** the channel where the message was published. */
  FIOBJ channel;
  /** the published message. */
  FIOBJ message;
  /** user opaque data. */
  void *udata;
} websocket_pubsub_notification_s;

/** Possible arguments for the {websocket_subscribe} function. */
struct websocket_subscribe_s {
  /** the websocket receiving the message. REQUIRED. */
  ws_s *ws;
  /** the channel where the message was published. */
  FIOBJ channel;
  /**
   * The callback that handles pub/sub notifications.
   *
   * Default: send directly to websocket client.
   */
  void (*on_message)(websocket_pubsub_notification_s notification);
  /**
   * An optional cleanup callback for the `udata`.
   */
  void (*on_unsubscribe)(void *udata);
  /** User opaque data, passed along to the notification. */
  void *udata;
  /** An optional callback for pattern matching. */
  fio_match_fn match;
  /**
   * When using client forwarding (no `on_message` callback), this indicates if
   * messages should be sent to the client as binary blobs, which is the safest
   * approach.
   *
   * Default: tests for UTF-8 data encoding and sends as text if valid UTF-8.
   * Messages above ~32Kb are always assumed to be binary.
   */
  unsigned force_binary : 1;
  /**
   * When using client forwarding (no `on_message` callback), this indicates if
   * messages should be sent to the client as text.
   *
   * `force_binary` has precedence.
   *
   * Default: see above.
   *
   */
  unsigned force_text : 1;
};

/**
 * Subscribes to a channel. See {struct websocket_subscribe_s} for possible
 * arguments.
 *
 * Returns a subscription ID on success and 0 on failure.
 *
 * All subscriptions are automatically revoked once the websocket is closed.
 *
 * If the connections subscribes to the same channel more than once, messages
 * will be merged. However, another subscription ID will be assigned, since two
 * calls to {websocket_unsubscribe} will be required in order to unregister from
 * the channel.
 */
uintptr_t websocket_subscribe(struct websocket_subscribe_s args);

#define websocket_subscribe(wbsckt, ...)                                       \
  websocket_subscribe((struct websocket_subscribe_s){.ws = wbsckt, __VA_ARGS__})

/**
 * Unsubscribes from a channel.
 *
 * Failures are silent.
 *
 * All subscriptions are automatically revoked once the websocket is closed. So
 * only use this function to unsubscribe while the websocket is open.
 */
void websocket_unsubscribe(ws_s *ws, uintptr_t subscription_id);

/** Optimize generic broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB (-32)
/** Optimize text broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB_TEXT (-33)
/** Optimize binary broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB_BINARY (-34)

/**
 * Enables (or disables) broadcast optimizations.
 *
 * When using WebSocket pub/sub system is originally optimized for either
 * non-direct transmission (messages are handled by callbacks) or direct
 * transmission to 1-3 clients per channel (on avarage), meaning that the
 * majority of the messages are meant for a single recipient (or multiple
 * callback recipients) and only some are expected to be directly transmitted to
 * a group.
 *
 * However, when most messages are intended for direct transmission to more than
 * 3 clients (on avarage), certain optimizations can be made to improve memory
 * consumption (minimize duplication or WebSocket network data).
 *
 * This function allows enablement (or disablement) of these optimizations.
 * These optimizations include:
 *
 * * WEBSOCKET_OPTIMIZE_PUBSUB - optimize all direct transmission messages,
 *                               best attempt to detect Text vs. Binary data.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_TEXT - optimize direct pub/sub text messages.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_BINARY - optimize direct pub/sub binary messages.
 *
 * Note: to disable an optimization it should be disabled the same amount of
 * times it was enabled - multiple optimization enablements for the same type
 * are merged, but reference counted (disabled when reference is zero).
 */
void websocket_optimize4broadcasts(intptr_t type, int enable);

/* *****************************************************************************
Websocket Tasks - within a single process scope, NOT and entire cluster
***************************************************************************** */

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
 * DEPRECATION NOTICE: this function will be removed in favor of pub/sub logic.
 *
 * Performs a task on each websocket connection that shares the same process
 * (except the originating `ws_s` connection which is allowed to be NULL).
 */
void __attribute__((deprecated))
websocket_each(struct websocket_each_args_s args);
#define websocket_each(...)                                                    \
  websocket_each((struct websocket_each_args_s){__VA_ARGS__})

/**
 * DEPRECATION NOTICE: this function will be removed in favor of pub/sub logic.
 *
 * The Arguments passed to the `websocket_write_each` function / macro are
 * defined here, for convinience of calling the function.
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
int __attribute__((deprecated))
websocket_write_each(struct websocket_write_each_args_s args);
#define websocket_write_each(...)                                              \
  websocket_write_each((struct websocket_write_each_args_s){__VA_ARGS__})

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
