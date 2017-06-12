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
  /** The pub/sub message. */
  struct {
    char *data;
    uint32_t len;
  } msg;
  /** indicates that pattern matching was used. */
  unsigned use_pattern : 1;
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
  void (*on_message)(pubsub_sub_pt reg, pubsub_message_s msg, void *udata1,
                     void *udata2);
  /** Opaque user data#1 */
  void *udata1;
  /** Opaque user data#2 .. using two allows allocation to be avoided. */
  void *udata2;
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
 * When an engine received a message to publish, they should call the
 * `pubsub_eng_distribute` function. i.e.:
 *
 *       pubsub_engine_distribute(
 *           .engine = self,
 *           .channel.name = "channel 1",
 *           .channel.len = 9,
 *           .msg.data = "hello",
 *           .msg.len = 5,
 *           .push2cluster = self->push2cluster,
 *           .use_pattern = 0 );
 *
 * Engines MUST survive until the pub/sub service is finished using them and
 * there are no more subscriptions.
 */
struct pubsub_engine_s {
  /** Should return 0 on success and -1 on failure. */
  int (*subscribe)(const pubsub_engine_s *eng, const char *ch, size_t ch_len,
                   uint8_t use_pattern);
  /** Return value is ignored. */
  void (*unsubscribe)(const pubsub_engine_s *eng, const char *ch, size_t ch_len,
                      uint8_t use_pattern);
  /** Should return 0 on success and -1 on failure. */
  int (*publish)(const pubsub_engine_s *eng, const char *ch, size_t ch_len,
                 const char *msg, size_t msg_len, uint8_t use_pattern);
  /** Set to TRUE (1) if published messages should propegate to the cluster. */
  unsigned push2cluster : 1;
};

/**
 * The function used by engines to distribute received messages.
 */
void pubsub_engine_distribute(pubsub_message_s msg);
#define pubsub_engine_distribute(...)                                          \
  pubsub_engine_distribute((pubsub_message_s){__VA_ARGS__})

#endif /* H_FACIL_PUBSUB_H */
