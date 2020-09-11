/* ************************************************************************** */
#ifndef FIO_MAX_SOCK_CAPACITY /* Development inclusion - ignore line */
#include "0003 main api.h"    /* Development inclusion - ignore line */
#endif                        /* Development inclusion - ignore line */
/* ************************************************************************** */

/* *****************************************************************************
 * Pub/Sub / Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 *
 **************************************************************************** */

/* *****************************************************************************
 * Cluster Messages and Pub/Sub
 **************************************************************************** */

/** An opaque subscription type. */
typedef struct subscription_s subscription_s;

/** A pub/sub engine data structure. See details later on. */
typedef struct fio_pubsub_engine_s fio_pubsub_engine_s;

/** The default engine (settable). Initial default is FIO_PUBSUB_CLUSTER. */
extern fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT;
/** Used to publish the message to all clients in the cluster. */
#define FIO_PUBSUB_CLUSTER ((fio_pubsub_engine_s *)1)
/** TODO: Used to publish the message only within the current machine. */
#define FIO_PUBSUB_LOCAL ((fio_pubsub_engine_s *)2)
/** Used to publish the message except within the current process. */
#define FIO_PUBSUB_SIBLINGS ((fio_pubsub_engine_s *)3)
/** Used to publish the message only within the current process. */
#define FIO_PUBSUB_PROCESS ((fio_pubsub_engine_s *)4)
/** Used to publish the message exclusively to the root / master process. */
#define FIO_PUBSUB_ROOT ((fio_pubsub_engine_s *)5)

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct fio_msg_s {
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /**
   * A channel name, allowing for pub/sub patterns.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field might be used for internal data.
   */
  fio_str_info_s channel;
  /**
   * A connection (if any) to which the subscription belongs.
   *
   * The connection uuid 0 marks an un-bound (non-connection related)
   * subscription.
   */
  intptr_t uuid;
  /**
   * The connection's protocol (if any).
   *
   * If the subscription is bound to a connection, the protocol will be locked
   * using a task lock and will be available using this pointer.
   */
  fio_protocol_s *pr;
  /**
   * The actual message.
   *
   * NOTE: the channel and msg strings should be considered immutable. The .capa
   * field is reserved for internal data and must NOT be used.
   **/
  fio_str_info_s msg;
  /** The `udata1` argument associated with the subscription. */
  void *udata1;
  /** The `udata1` argument associated with the subscription. */
  void *udata2;
  /** flag indicating if the message is JSON data or binary/text. */
  uint8_t is_json;
} fio_msg_s;

/**
 * The pattern matching callback used for pattern matching.
 *
 * Returns 1 on a match or 0 if the string does not match the pattern.
 *
 * By default, the value is set to `fio_glob_match` (see facil.io's C STL).
 */
extern uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s pattern,
                                           fio_str_info_s channel);

/**
 * Possible arguments for the fio_subscribe method.
 *
 * NOTICE: passing protocol objects to the `udata` is not safe. This is because
 * protocol objects might be destroyed or invalidated according to both network
 * events (socket closure) and internal changes (i.e., `fio_attach` being
 * called). The preferred way is to add the `uuid` to the `udata` field and call
 * `fio_protocol_try_lock`.
 */
typedef struct {
  /**
   * If `filter` is set, all messages that match the filter's numerical value
   * will be forwarded to the subscription's callback.
   *
   * Subscriptions can either require a match by filter or match by channel.
   * This will match the subscription by filter.
   *
   * Filer channels are cluster bound, are NOT forwarded to engines and can be
   * used for inter process communication (IPC).
   */
  int32_t filter;
  /**
   * If `channel` is set, all messages where `filter == 0` and the channel is an
   * exact match will be forwarded to the subscription's callback.
   *
   * Subscriptions can either require a match by filter or match by channel.
   * This will match the subscription by channel (only messages with no `filter`
   * will be received.
   */
  fio_str_info_s channel;
  /**
   * A connection (if any) to which the subscription should be bound.
   *
   * The connection uuid 0 isn't a valid uuid for subscriptions.
   */
  intptr_t uuid;
  /**
   * The callback will be called for each message forwarded to the subscription.
   */
  void (*on_message)(fio_msg_s *msg);
  /** An optional callback for when a subscription is fully canceled. */
  void (*on_unsubscribe)(void *udata1, void *udata2);
  /** The udata values are ignored and made available to the callback. */
  void *udata1;
  /** The udata values are ignored and made available to the callback. */
  void *udata2;
  /** If set, the string is treated as a pattern*/
  uint8_t is_pattern;
} subscribe_args_s;

/** Publishing and on_message callback arguments. */
typedef struct fio_publish_args_s {
  /** The pub/sub engine that should be used to forward this message. */
  fio_pubsub_engine_s const *engine;
  /** A numeral / internal chunnel. Negative values are reserved. */
  int32_t filter;
  /** The target named channnel. Only published when filter == 0. */
  fio_str_info_s channel;
  /** The message body / content. */
  fio_str_info_s message;
  /** A flag indicating if the message is JSON data or not. */
  uint8_t is_json;
} fio_publish_args_s;

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure. Returns NULL on
 * success when the subscription is bound to a connnection's uuid.
 *
 * Note: since ownership of the subscription is transferred to a connection's
 * UUID when the subscription is linked to a connection, the caller will not
 * receive a link to the subscription object. NULL is returned instead.
 *
 * See `subscribe_args_s` for details.
 */
subscription_s *fio_subscribe(subscribe_args_s args);

/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * Returns a subscription pointer on success or NULL on failure. Returns NULL on
 * success when the subscription is bound to a connnection's uuid.
 *
 * Note: since ownership of the subscription is transferred to a connection's
 * UUID when the subscription is linked to a connection, the caller will not
 * receive a link to the subscription object.
 *
 * See `subscribe_args_s` for details.
 */
#define fio_subscribe(...) fio_subscribe((subscribe_args_s){__VA_ARGS__})

/**
 * Cancels an existing subscriptions - actual effects might be delayed, for
 * example, if the subscription's callback is running in another thread.
 */
void fio_unsubscribe(subscription_s *subscription);

/**
 * Cancels an existing subscriptions that was bound to a connection's UUID. See
 * `fio_subscribe` and `fio_unsubscribe` for more details.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 */
void fio_unsubscribe_uuid(subscribe_args_s args);

/**
 * Cancels an existing subscriptions that was bound to a connection's UUID. See
 * `fio_subscribe` and `fio_unsubscribe` for more details.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 */
#define fio_unsubscribe_uuid(...)                                              \
  fio_unsubscribe_uuid((subscribe_args_s){__VA_ARGS__})

/**
 * This helper returns a temporary String with the subscription's channel (or a
 * string representing the filter).
 *
 * To keep the string beyond the lifetime of the subscription, copy the string.
 */
fio_str_info_s fio_subscription_channel(subscription_s *subscription);

/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
void fio_publish(fio_publish_args_s args);
/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
#define fio_publish(...) fio_publish((fio_publish_args_s){__VA_ARGS__})
/** for backwards compatibility */
#define pubsub_publish fio_publish

/**
 * Defers the current callback, so it will be called again for the message.
 *
 * After calling this function, the `msg` object must NOT be accessted.
 */
void fio_message_defer(fio_msg_s *msg);

/* *****************************************************************************
 * Message metadata (advance usage API)
 **************************************************************************** */

/**
 * The number of different metadata callbacks that can be attached.
 *
 * Effects performance.
 *
 * The default value should be enough for the following metadata objects:
 * - WebSocket server headers.
 * - WebSocket client (header + masked message copy).
 * - EventSource (SSE) encoded named channel and message.
 */
#ifndef FIO_PUBSUB_METADATA_LIMIT
#define FIO_PUBSUB_METADATA_LIMIT 4
#endif

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id);

/**
 * Pub/Sub Metadata callback type.
 */
typedef void *(*fio_msg_metadata_fn)(fio_str_info_s ch,
                                     fio_str_info_s msg,
                                     uint8_t is_json);

/**
 * It's possible to attach metadata to facil.io named messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.
 *
 * The callback should return a `void *` pointer.
 *
 * To remove a callback, call `fio_message_metadata_remove` with the returned
 * value.
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 *
 * Returns a positive number on success (the metadata ID) or zero (0) on
 * failure.
 */
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *));

/**
 * Removed the metadata callback.
 *
 * Removal might be delayed if live metatdata exists.
 */
void fio_message_metadata_remove(int id);
/* *****************************************************************************
 * Message data duplication and defferal (advance usage API)
 **************************************************************************** */

/**
 * Message content data type (opaque, read only).
 *
 * Subscription messages are all based on a single letter that all subscribers
 * read (if it's "subject" matches their interests).
 *
 * This is type containing the raw letter data, which is network encoded.
 */
typedef struct fio_letter_s fio_letter_s;

/** Letter informatioin, as returned from the `fio_letter_info` function. */
typedef struct {
  fio_str_info_s header;
  fio_str_info_s body;
} fio_letter_info_s;

/**
 * Increases a message's reference count, returning the published "letter".
 *
 * This is performed by reference, the value MUST be considered read-only.
 *
 * Note: subscription messages are all based on a single letter that all
 *       subscribers read (if its "subject" matches their interests).
 *       Letter objects are achitecture agnostice, and may be sent on the net.
 */
fio_letter_s *fio_message_dup(fio_msg_s *msg);

/** Increases a letter's reference count by reference (for read-only access). */
fio_letter_s *fio_letter_dup(fio_letter_s *msg);

/** Decreseas a letter's referencce count and possibly frees its data. */
void fio_letter_free(fio_letter_s *letter);

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *letter);

/** Returns all information about the fio_letter_s object. Read Only! */
fio_letter_info_s fio_letter_info(fio_letter_s *letter);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/**
 * facil.io can be linked with external Pub/Sub services using "engines".
 *
 * Only named messages and subscriptions (where filter == 0) will be forwarded
 * to these "engines".
 *
 * Engines MUST provide the listed function pointers and should be attached
 * using the `fio_pubsub_attach` function.
 *
 * Engines should disconnect / detach, before being destroyed, by using the
 * `fio_pubsub_detach` function.
 *
 * When an engine received a message to publish, it should call the
 * `pubsub_publish` function with the engine to which the message is forwarded.
 * i.e.:
 *
 *       pubsub_publish(
 *           .engine = FIO_PUBSUB_CLUSTER,
 *           .channel = channel_name,
 *           .message = msg_body );
 *
 * Since only the master process guarantes to be subscribed to all the channels
 * in the cluster, it is recommended that engines only use the master process to
 * communicate with a pub/sub backend.
 *
 * IMPORTANT: The `subscribe` and `unsubscribe` callbacks are called from within
 *            an internal lock. They MUST NEVER call pub/sub functions except by
 *            exiting the lock using `fio_defer`.
 */
struct fio_pubsub_engine_s {
  /** Should subscribe channel. Failures are ignored. */
  void (*subscribe)(const fio_pubsub_engine_s *eng,
                    fio_str_info_s channel,
                    uint8_t is_pattern);
  /** Should unsubscribe channel. Failures are ignored. */
  void (*unsubscribe)(const fio_pubsub_engine_s *eng,
                      fio_str_info_s channel,
                      uint8_t is_pattern);
  /** Should publish a message through the engine. Failures are ignored. */
  void (*publish)(const fio_pubsub_engine_s *eng,
                  fio_str_info_s channel,
                  fio_str_info_s msg,
                  uint8_t is_json);
};

/**
 * Attaches an engine, so it's callback can be called by facil.io.
 *
 * The `subscribe` callback will be called for every existing channel.
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_attach(fio_pubsub_engine_s *engine);

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine);

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_reattach(fio_pubsub_engine_s *eng);

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);

/* *****************************************************************************
 * TODO: clusterfy the local network using UDP broadcasting for node discovery.
 **************************************************************************** */

/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *tls);
