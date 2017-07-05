/*
Copyright: Boaz segev, 2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_REDIS_ENGINE_H
/**
This is a simple, optimistic Redis engine that matches the requirements of
facil.io's pub/sub engine design.

The engine is optimistic, meanning the engine will never report a failed
subscription or publication... it will simply try until successful.
*/
#define H_REDIS_ENGINE_H
#include "pubsub.h"
#include "resp.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/** possible arguments for the `redis_engine_create` function call */
struct redis_engine_create_args {
  /** Redis server's address */
  const char *address;
  /** Redis server's port */
  const char *port;
  /** Redis server's password, if any */
  const char *auth;
  /** Redis server's password length (if any). */
  size_t auth_len;
  /** A `ping` will be sent every `ping_interval` interval or inactivity. */
  uint8_t ping_interval;
};

/**
See the {pubsub.h} file for documentation about engines.

The engine is active only after facil.io starts running.

A `ping` will be sent every `ping_interval` interval or inactivity. The default
value (0) will fallback to facil.io's maximum time of inactivity (5 minutes)
before polling on the connection's protocol.

function names speak for themselves ;-)
*/
pubsub_engine_s *redis_engine_create(struct redis_engine_create_args);

#define redis_engine_create(...)                                               \
  redis_engine_create((struct redis_engine_create_args){__VA_ARGS__})

/**
Sends a Redis message through the engine's connection. The response will be sent
back using the optional callback. `udata` is passed along untouched.

The message will be repeated endlessly until a response validates the fact that
it was sent (or the engine is destroyed).
*/
intptr_t redis_engine_send(pubsub_engine_s *engine, resp_object_s *data,
                           void (*callback)(pubsub_engine_s *e,
                                            resp_object_s *reply, void *udata),
                           void *udata);

/**
See the {pubsub.h} file for documentation about engines.

function names speak for themselves ;-)
*/
void redis_engine_destroy(const pubsub_engine_s *engine);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
