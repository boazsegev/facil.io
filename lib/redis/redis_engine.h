#ifndef H_REDIS_ENGINE_H
#define H_REDIS_ENGINE_H
#include "pubsub.h"

/**
See the {pubsub.h} file for documentation about engines.

The engine is active only after facil.io starts running.

function names speak for themselves ;-)
*/
pubsub_engine_s *redis_engine_create(const char *address, const char *port);

/**
See the {redis_connection.h} file for documentation about Redis connections and
the {resp.h} file to learn more about sending RESP messages.

Returns -1 on error, otherwise return's the connection's facil.io uuid.
function names speak for themselves ;-)
*/
intptr_t redis_engine_get_redis(pubsub_engine_s *engine);

/**
See the {pubsub.h} file for documentation about engines.

function names speak for themselves ;-)
*/
void redis_engine_destroy(pubsub_engine_s *engine);
#endif
