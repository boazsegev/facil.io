/*
Copyright: Boaz segev, 2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_REDIS_CONNECTION_H
#define H_REDIS_CONNECTION_H
#include "facil.h"
#include "resp.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
Connectivity
***************************************************************************** */

/**
To create a new Redis connection, create a Redis context object using the
following optings and the `redis_create_context` function.

The context should be passed along as the `udata` parameter to either
`facil_connect` or `facil_listen`.

The `on_connect` / `on_open` argument for `facil_connect` and
`facil_listen`should be the `redis_create_protocol` function pointer.

The `on_fail` / `on_finish` argument for `facil_connect` and
`facil_listen`should be the `redis_protocol_cleanup` function pointer.
*/

struct redis_context_args {
  /** REQUIRED: the RESP parser used by the connection. */
  resp_parser_pt parser;
  /** REQUIRED: called when the RESP messages are received. */
  void (*on_message)(intptr_t uuid, resp_object_s *msg, void *udata);
  /** called when the Redix connection closes or fails to open. */
  void (*on_close)(intptr_t uuid, void *udata);
  /** called when the Redix connection opens. */
  void (*on_open)(intptr_t uuid, void *udata);
  /** Authentication string (password). */
  char *auth;
  /** Authentication string (password) length. */
  size_t auth_len;
  /** Opaque user data. */
  void *udata;
  /** PING intervals. */
  uint8_t ping;
  /** The context, not the computer... */
  uint8_t autodestruct;
};

void *redis_create_context(struct redis_context_args);

#define redis_create_context(...)                                              \
  redis_create_context((struct redis_context_args){__VA_ARGS__})

/**
 * This function is used as a function pointer for the `facil_connect` and
 * calls (the `on_connect` callback).
 */
protocol_s *redis_create_client_protocol(intptr_t uuid, void *settings);

/**
 * This function is used as a function pointer for the `facil_listen` calls (the
 * `on_open` callbacks).
 */
protocol_s *redis_create_server_protocol(intptr_t uuid, void *settings);

/**
 * This function is used as a function pointer for both `facil_connect` and
 * `facil_listen` calls (the `on_fail` / `on_finish` callbacks).
 *
 * It's main responsibility is to free the context and protocol resources.
 */
void redis_protocol_cleanup(intptr_t uuid, void *settings);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
