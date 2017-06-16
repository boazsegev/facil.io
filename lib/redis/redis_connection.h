#ifndef H_REDIS_CONNECTION_H
#define H_REDIS_CONNECTION_H
#include "facil.h"
#include "resp.h"

/* *****************************************************************************
Sending Commands or Responses
***************************************************************************** */

/** The following arguments (`struct` members) can be used to `redis_send` */
struct redis_send_args_s {
  /** The Redis connection's uuid (facil.io's connection ID). REQUIRED.*/
  intptr_t uuid;
  /** A RESP array with the command/response and it's arguments. REQUIRED.*/
  resp_object_s *cmd;
  /** An optional callback that will receive the response. */
  void (*on_response)(intptr_t uuid, const resp_object_s *response,
                      void *udata);
  /** an opaque user data that's passed along to the `on_response` callback.
  */ void *udata;
  /**
   * If set, `redis_send` will automatically deallocate the array when done.
   *
   * Setting this flag should be prefered.
   */
  uint8_t move;
};

/**
 * Sends RESP messages. Returns -1 on a known error and 0 if the message was
 * successfully validated to be sent.
 *
 * Servers implementations should probably write directly to the socket using
 * `sock_write` or `sock_write2`. This function is more client oriented and
 * manages a callback system to handle responses.
 *
 * Since this is an evented framework, this doesn't mean the data was
 actually * sent. That's why there's the `on_response` callback... ;-)
 */
int redis_send(struct redis_send_args_s);
#define redis_send(...) redis_send((struct redis_send_args_s){__VA_ARGS__})

/* *****************************************************************************
Connectivity
***************************************************************************** */

/**
 * This function can be used as a function pointer for both `facil_connect` and
 * `facil_listen` calls.
 *
 * The protocol can be used to implement both a client and a server.
 *
 * To implement more features, set up a wrapper function to initialize
 * features such as the `pubsub_handler` (the callback used for pub/sub
 * messages).
 *
 */
protocol_s *redis_create_protocol(intptr_t uuid, void *ignored);
#define redis_create_protocol                                                  \
  ((protocol_s * (*)(intptr_t, void *)) redis_create_protocol)

/**
 * Sets a the `on_close` event callback.
 *
 * `udata` is a user opaque pointer that's simply passed along.
 */
void redis_on_close(intptr_t uuid, void (*on_close)(void *udata), void *udata);

/**
 * Sets a the `on_close` event callback assuming the protocol for the socket is
 * locked (see {facil_protocol_try_lock}).
 *
 * `udata` is a user opaque pointer that's simply passed along.
 */
void redis_on_close2(protocol_s *pr, void (*on_close)(void *udata),
                     void *udata);

/**
 * Sets a the `fallback_handler` event callback assuming the protocol for the
 * socket is locked (see {facil_protocol_try_lock}), i.e., within an
 * `on_connect` wrapper function for {facil_connect} or {facil_listen}.
 *
 * `udata` is a user opaque pointer that's simply passed along.
 *
 * This callback is called when data was received and no callback was specified
 * (i.e., as a default `on_response` for {redis_send} or when implementing a
 * server)
 */
void redis_fallback_handler(protocol_s *pr,
                            void (*fallback_handler)(intptr_t uuid,
                                                     resp_object_s *msg,
                                                     void *udata),
                            void *udata);

/* *****************************************************************************
Pub/Sub streams
***************************************************************************** */

/**
 * Sets a the `on_pubsub` event callback.
 *
 * `udata` is a user opaque pointer that's simply passed along.
 *
 * Once this function had been called, the connection is assumed to have Pub/Sub
 * semantics.
 *
 * For example, incoming messages that follow the message format (an array of 3
 * strings, the first one being "message") will be forwarded to the callback.
 *
 * This introduces a possible collision with the Redis `LRANGE` command, which
 * precludes using the same connection for both subscriptions and other Redis
 * commands (this is also enforced the the Redis server implementation).
 */
void redis_on_pubsub(intptr_t uuid,
                     void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg,
                                       void *udata),
                     void *udata);
/**
 * Sets a the `on_pubsub` event callback assuming the protocol for the socket is
 * locked (see {facil_protocol_try_lock}).
 */
void redis_on_pubsub2(protocol_s *protocol,
                      void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg,
                                        void *udata),
                      void *udata);

#endif
