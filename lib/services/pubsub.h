/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FACIL_PUBSUB_H
/**
This pub/sub API is designed to unload pub/sub stress from external messanging
systems onto the local process.

For example, the NULL pub/sub engine, which is routed to the facil_cluster
engine, will only publish a single message per process instead of a message per
client, allowing the cluster communication channel to be less crowded when
possible.

This should allow pub/sub engines, such as Redis, to spread their workload
between all of an application's processes, enhancing overall performance.
*/
#define H_FACIL_PUBSUB_H
#include "facil.h"

/** This function will be called by `facil.io` to initialize the default cluster
 * pub/sub engine. */
void pubsub_cluster_init(void);

/** An opaque pointer used to identify a subscription. */
typedef struct pubsub_sub_s *pubsub_sub_pt;

/** A pub/sub engine data structure. See details later on. */
typedef struct pubsub_engine_s pubsub_engine_s;

/** The information received through a subscription. */
typedef struct pubsub_message_s {
  /** The pub/sub engine farwarding this message. */
  pubsub_engine_s const *engine;
  /** The pub/sub target channnel. */
  struct {
    char *name;
    uint32_t len;
  } channel;
  struct {
    char *data;
    uint32_t len;
  } msg;
} pubsub_message_s;

/** The arguments used for subscribing to a channel. */
struct pubsub_subscribe_args {
  /** The pub/sub engine to use. NULL defaults to the local cluster engine. */
  pubsub_engine_s const *engine;
  /** The channel to subscribe to. */
  struct {
    char *name;
    uint32_t len;
  } channel;
  /** The on message callback */
  void (*on_message)(pubsub_sub_pt s, pubsub_message_s msg, void *udata);
  /** Opaque user data */
  void *udata;
  /** Use pattern matching for channel subscription. */
  unsigned use_pattern : 1;
};

/** The arguments used to publish to channel. */
struct pubsub_publish_args {
  /** The pub/sub engine to use. NULL defaults to the local cluster engine. */
  pubsub_engine_s const *engine;
  /** The channel to publish to. */
  struct {
    char *name;
    uint32_t len;
  } channel;
  /** The data being pushed. */
  struct {
    char *data;
    uint32_t len;
  } msg;
  /** Use pattern matching for channel publication. */
  unsigned use_pattern : 1;
  /**
   * Push the message to the whole cluster, using the cluster engine.
   * Always TRUE unless an engine was specified.
   */
  unsigned push2cluster : 1;
};

/**
 * Subscribes to a specific channel.
 *
 * Returns 0 on success and -1 on failure.
 */ pubsub_sub_pt pubsub_subscribe(struct pubsub_subscribe_args);
#define pubsub_subscribe(...)                                                  \
  pubsub_subscribe((struct pubsub_subscribe_args){__VA_ARGS__})

/**
 * Unsubscribes from a specific channel.
 *
 * Returns 0 on success and -1 on failure.
 */
void pubsub_unsubscribe(pubsub_sub_pt subscription);

/**
 * Publishes a message to a channel belonging to a pub/sub service (engine).
 *
 * Returns 0 on success and -1 on failure.
 */
int pubsub_publish(struct pubsub_publish_args);
#define pubsub_publish(...)                                                    \
  pubsub_publish((struct pubsub_publish_args){__VA_ARGS__})

/**
 * Pub/Sub services (engines) MUST provide the listed function pointers.
 *
 * When sending messages using `on_message` callback, engins MUST:
 * 1. Pass NULL as the first argument (the subscription argument).
 * 2. Pass a pointer to `self` (the engine object) as the third argument.
 */
struct pubsub_engine_s {
  /** Should return 0 on success and -1 on failure. */
  int (*subscribe)(struct pubsub_subscribe_args);
  /** Return value is ignored - nothing should be returned. */
  void (*unsubscribe)(struct pubsub_subscribe_args);
  /** Should return 0 on success and -1 on failure. */
  int (*publish)(struct pubsub_publish_args);
  /** Set to TRUE (1) if published messages should propegate to the cluster. */
  unsigned push2cluster : 1;
};

#endif /* H_FACIL_PUBSUB_H */
