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
/** Used to publish the message only within the current process. */
#define FIO_PUBSUB_PROCESS ((fio_pubsub_engine_s *)2)
/** Used to publish the message except within the current process. */
#define FIO_PUBSUB_SIBLINGS ((fio_pubsub_engine_s *)3)
/** Used to publish the message exclusively to the root / master process. */
#define FIO_PUBSUB_ROOT ((fio_pubsub_engine_s *)4)

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
   * field might be used for internal data.
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
 * Pattern matching callback type - should return 0 unless channel matches
 * pattern.
 */
typedef int (*fio_match_fn)(fio_str_info_s pattern, fio_str_info_s channel);

extern fio_match_fn FIO_MATCH_GLOB;

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
   * The the `match` function allows pattern matching for channel names.
   *
   * When using a match function, the channel name is considered to be a pattern
   * and each pub/sub message (a message where filter == 0) will be tested
   * against that pattern.
   *
   * Using pattern subscriptions extensively could become a performance concern,
   * since channel names are tested against each distinct pattern rather than
   * leveraging a hashmap for possible name matching.
   */
  fio_match_fn match;
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
} subscribe_args_s;

/** Publishing and on_message callback arguments. */
typedef struct fio_publish_args_s {
  /** The pub/sub engine that should be used to forward this message. */
  fio_pubsub_engine_s const *engine;
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** The pub/sub target channnel. */
  fio_str_info_s channel;
  /** The pub/sub message. */
  fio_str_info_s message;
  /** flag indicating if the message is JSON data or binary/text. */
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
 * receive a link to the subscription object.
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

/** Finds the message's metadata by it's type ID. Returns the data or NULL. */
void *fio_message_metadata(fio_msg_s *msg, intptr_t type_id);

/**
 * Defers the current callback, so it will be called again for the message.
 */
void fio_message_defer(fio_msg_s *msg);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/** Contains message metadata, set by message extensions. */
typedef struct fio_msg_metadata_s fio_msg_metadata_s;
struct fio_msg_metadata_s {
  /**
   * The type ID should be used to identify the metadata's actual structure.
   *
   * Negative ID values are reserved for internal use.
   */
  intptr_t type_id;
  /**
   * This method will be called by facil.io to cleanup the metadata resources.
   *
   * Don't alter / call this method, this data is reserved.
   */
  void (*on_finish)(fio_msg_s *msg, void *metadata);
  /** The pointer to be disclosed to the `fio_message_metadata` function. */
  void *metadata;
};

/**
 * Pub/Sub Metadata callback type.
 */
typedef fio_msg_metadata_s (*fio_msg_metadata_fn)(fio_str_info_s ch,
                                                  fio_str_info_s msg,
                                                  uint8_t is_json);

/**
 * It's possible to attach metadata to facil.io pub/sub messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * The callback should return a valid metadata object. If the `.metadata` field
 * returned is NULL than the result will be ignored.
 *
 * To remove a callback, set the `enable` flag to false (`0`).
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 */
void fio_message_metadata_callback_set(fio_msg_metadata_fn callback,
                                       int enable);

/**
 * facil.io can be linked with external Pub/Sub services using "engines".
 *
 * Only unfiltered messages and subscriptions (where filter == 0) will be
 * forwarded to external Pub/Sub services.
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
 *           .engine = FIO_PROCESS_ENGINE,
 *           .channel = channel_name,
 *           .message = msg_body );
 *
 * IMPORTANT: The `subscribe` and `unsubscribe` callbacks are called from within
 *            an internal lock. They MUST NEVER call pub/sub functions except by
 *            exiting the lock using `fio_defer`.
 */
struct fio_pubsub_engine_s {
  /** Should subscribe channel. Failures are ignored. */
  void (*subscribe)(const fio_pubsub_engine_s *eng,
                    fio_str_info_s channel,
                    fio_match_fn match);
  /** Should unsubscribe channel. Failures are ignored. */
  void (*unsubscribe)(const fio_pubsub_engine_s *eng,
                      fio_str_info_s channel,
                      fio_match_fn match);
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
