#include "redis_engine.h"
#include "redis_connection.h"
#include "resp.h"

#include <string.h>
/* *****************************************************************************
Data Structures / State
***************************************************************************** */

typedef struct {
  pubsub_engine_s engine;
  intptr_t sub;
  intptr_t pub;
  char *address;
  char *port;
} redis_engine_s;

/* *****************************************************************************
Engine Bridge
***************************************************************************** */

static void on_pubsub(intptr_t uuid, const resp_array_s *msg, void *udata) {
  if (msg->len != 3 || msg->array[0]->type != RESP_STRING ||
      msg->array[1]->type != RESP_STRING ||
      msg->array[2]->type != RESP_STRING) {
    fprintf(
        stderr,
        "ERROR: WTF!? Redis engine pubsub received an unparsable message.\n");
    return;
  }
  pubsub_engine_distribute(
          .engine = udata,
          .channel = {.name = (char *)resp_obj2str(msg->array[1])->string,
                      .len = resp_obj2str(msg->array[1])->len},
          .msg = {.data = (char *)resp_obj2str(msg->array[2])->string,
                  .len = resp_obj2str(msg->array[2])->len});
  (void)uuid;
}

/* *****************************************************************************
Connections
***************************************************************************** */

static protocol_s *on_connect(intptr_t uuid, void *en_) {
  protocol_s *pr = redis_create_protocol(uuid, NULL);
  redis_on_pubsub2(pr, on_pubsub, en_);
  return pr;
}

#define CONNECT_SUB(e)                                                         \
  e->sub = facil_connect(.address = e->address, .port = e->port,               \
                         .on_connect = on_connect, .udata = e, )

#define CONNECT_PUB(e)                                                         \
  e->pub = facil_connect(.address = e->address, .port = e->port,               \
                         .on_connect = redis_create_protocol, .udata = e, )

/* *****************************************************************************
Callbacks
***************************************************************************** */

/** Should return 0 on success and -1 on failure. */
static int subscribe(const pubsub_engine_s *eng, const char *ch, size_t ch_len,
                     uint8_t use_pattern) {
  redis_engine_s *e = (redis_engine_s *)eng;
  if (!sock_isvalid(e->sub))
    CONNECT_SUB(e);
  if (!sock_isvalid(e->sub)) {
    fprintf(
        stderr,
        "ERROR: (RedisEngine) cannot connect to Subscription service at %s:%s",
        e->address, e->port);
    return -1;
  }
  resp_object_s *cmd = resp_arr2obj(2, NULL);
  if (!cmd)
    return -1;
  resp_obj2arr(cmd)->array[0] = use_pattern ? resp_str2obj("PSUBSCRIBE", 10)
                                            : resp_str2obj("SUBSCRIBE", 9);
  resp_obj2arr(cmd)->array[1] =
      (ch ? resp_str2obj(ch, ch_len) : resp_nil2obj());
  return redis_send(.cmd = cmd, .uuid = e->sub, .move = 1);
}

/** Return value is ignored. */
static void unsubscribe(const pubsub_engine_s *eng, const char *ch,
                        size_t ch_len, uint8_t use_pattern) {
  redis_engine_s *e = (redis_engine_s *)eng;

  if (!sock_isvalid(e->sub))
    return;
  resp_object_s *cmd = resp_arr2obj(2, NULL);
  if (!cmd)
    return;
  resp_obj2arr(cmd)->array[0] = use_pattern ? resp_str2obj("PUNSUBSCRIBE", 12)
                                            : resp_str2obj("UNSUBSCRIBE", 11);
  resp_obj2arr(cmd)->array[1] =
      (ch ? resp_str2obj(ch, ch_len) : resp_nil2obj());
  redis_send(.cmd = cmd, .uuid = e->sub, .move = 1);
}

/** Should return 0 on success and -1 on failure. */
static int publish(const pubsub_engine_s *eng, const char *ch, size_t ch_len,
                   const char *msg, size_t msg_len, uint8_t use_pattern) {
  redis_engine_s *e = (redis_engine_s *)eng;
  if (!defer_fork_is_active() || !msg || use_pattern)
    return -1;
  if (!ch)
    return -1;
  if (!sock_isvalid(e->pub))
    CONNECT_PUB(e);
  if (!sock_isvalid(e->pub)) {
    fprintf(stderr, "ERROR: (RedisEngine) cannot connect to Redis at %s:%s",
            e->address, e->port);
    return -1;
  }
  resp_object_s *cmd = resp_arr2obj(3, NULL);
  if (!cmd)
    return -1;
  resp_obj2arr(cmd)->array[0] = resp_str2obj("PUBLISH", 7);
  resp_obj2arr(cmd)->array[1] = resp_str2obj(ch, ch_len);
  resp_obj2arr(cmd)->array[2] = resp_str2obj(msg, msg_len);
  return redis_send(.cmd = cmd, .uuid = e->pub, .move = 1);
}

/* *****************************************************************************
Creation
***************************************************************************** */

static void initialize_engine(void *en_, void *ig) {
  redis_engine_s *e = (redis_engine_s *)en_;
  (void)ig;
  CONNECT_SUB(e);
  CONNECT_PUB(e);
}

/**
See the {pubsub.h} file for documentation about engines.

function names speak for themselves ;-)
*/
pubsub_engine_s *redis_engine_create(const char *address, const char *port) {
  if (!port) {
    return NULL;
  }
  size_t addr_len = address ? strlen(address) : 0;
  size_t port_len = strlen(port);
  redis_engine_s *e = malloc(sizeof(*e) + addr_len + port_len + 2);
  *e = (redis_engine_s){.engine = {.subscribe = subscribe,
                                   .unsubscribe = unsubscribe,
                                   .publish = publish},
                        .address = (char *)(e + 1),
                        .port = ((char *)(e + 1) + addr_len + 1)};
  if (address)
    memcpy(e->address, address, addr_len);
  else
    e->address = NULL;
  e->address[addr_len] = 0;
  memcpy(e->port, port, port_len);
  e->port[port_len] = 0;
  defer(initialize_engine, e, NULL);
  return (pubsub_engine_s *)e;
}

/**
See the {redis_connection.h} file for documentation about Redis connections and
the {resp.h} file to learn more about sending RESP messages.

function names speak for themselves ;-)
*/
intptr_t redis_engine_get_redis(pubsub_engine_s *en_) {
  redis_engine_s *e = (redis_engine_s *)en_;
  if (!defer_fork_is_active())
    return -1;
  if (!sock_isvalid(e->pub))
    CONNECT_PUB(e);
  return e->pub;
}

/**
See the {pubsub.h} file for documentation about engines.

function names speak for themselves ;-)
*/
void redis_engine_destroy(pubsub_engine_s *engine) {
  redis_engine_s *e = (redis_engine_s *)engine;
  sock_close(e->pub);
  sock_close(e->sub);
  free(e);
}
