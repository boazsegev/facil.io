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

/**
See the {pubsub.h} file for documentation about engines.

The engine is active only after facil.io starts running.

A `ping` will be sent every `timeout` interval or inactivity. The default value
(0) will fallback to facil.io's maximum time of inactivity (5 minutes) before
polling on the connection's protocol.

function names speak for themselves ;-)
*/
pubsub_engine_s *redis_engine_create(const char *address, const char *port,
                                     uint8_t ping_interval);

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
void redis_engine_destroy(pubsub_engine_s *engine);
#endif
