#include "redis_connection.h"
#include "fio_list.h"
#include "spnlock.inc"
#include <signal.h>
#include <string.h>
#include <strings.h>

/* *****************************************************************************
Memory and Data structures - The protocol object.
***************************************************************************** */
#ifndef REDIS_POOL_SIZES /* per protocol */
#define REDIS_POOL_SIZES 256
#endif

#ifndef REDIS_READ_BUFFER /* during network data events */
#define REDIS_READ_BUFFER 2048
#endif

/* a protocol ID string */
static const char *REDIS_PROTOCOL_ID =
    "Redis Protocol for the facil.io framework";

/* a callback container */
typedef struct {
  fio_list_s node;
  void (*on_response)(intptr_t uuid, const resp_object_s *response,
                      void *udata);
  void *udata;
} callback_s;

typedef struct {
  protocol_s protocol;
  /* the RESP parser */
  resp_parser_pt parser;
  /* The callbacks list.
   * We don't neet locks since we'll be using the facil.io protocol locking
   * mechanism.
   */
  fio_list_s callbacks;
  /* The on_open callback */
  void (*on_open)(intptr_t uuid);
  /* The on_open callback */
  void (*on_close)(void *udata);
  void *on_close_udata;
  /* The on_pubsub callback */
  void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg, void *udata);
  void *on_pubsub_udata;
  /* Fallback / default handler for messages. */
  void (*fallback)(intptr_t uuid, resp_object_s *msg, void *udata);
  void *fallback_udata;
} redis_protocol_s;

/************************
Allocation / Deallocation
Using the memory pools
********************** */

static spn_lock_i callback_pool_lock = SPN_LOCK_INIT;
static callback_s callback_pool_mem[REDIS_POOL_SIZES];
static fio_list_s callbacks_pool = FIO_LIST_INIT_STATIC(callbacks_pool);
static uint8_t callbacks_pool_is_init = 0;
static inline void callbacks_pool_init(void) {
  if (callbacks_pool_is_init)
    return;
  callbacks_pool_is_init = 1;
  for (size_t i = 0; i < REDIS_POOL_SIZES; i++) {
    fio_list_add(&callbacks_pool, &callback_pool_mem[i].node);
  }
}
static callback_s *callback_alloc(void) {
  callback_s *c;
  spn_lock(&callback_pool_lock);
  if (!callbacks_pool_is_init)
    callbacks_pool_init();
  c = fio_list_pop(callback_s, node, callbacks_pool);
  spn_unlock(&callback_pool_lock);
  if (!c)
    c = malloc(sizeof(*c));
  return c;
}

static void callback_free(callback_s *c) {
  if ((uintptr_t)c >= (uintptr_t)callback_pool_mem &&
      (uintptr_t)c < (uintptr_t)(callback_pool_mem + REDIS_POOL_SIZES)) {
    spn_lock(&callback_pool_lock);
    fio_list_push(callback_s, node, callbacks_pool, c);
    spn_unlock(&callback_pool_lock);
    return;
  }
  free(c);
}

/* *****************************************************************************
The Protocol Callbacks and initialization
***************************************************************************** */

/** called when the connection was closed, but will not run concurrently */
static void redis_on_close_protocol(protocol_s *pr) {
  redis_protocol_s *r = (redis_protocol_s *)pr;
  if (r->on_close)
    r->on_close(r->on_close_udata);
  callback_s *cl;
  while ((cl = fio_list_pop(callback_s, node, r->callbacks)))
    callback_free(cl);
  resp_parser_destroy(r->parser);
  free(r);
}

/** called when a connection's timeout was reached */
static void redis_ping(intptr_t uuid, protocol_s *pr) {
  /* We cannow write directly to the socket in case `redis_send` has scheduled
   * callbacks. */
  resp_object_s *cmd = resp_str2obj("PING", 4);
  cmd = resp_arr2obj(1, &cmd);
  redis_send(.uuid = uuid, .cmd = cmd, .move = 1);
  (void)pr;
}

void redis_on_shutdown(intptr_t uuid, protocol_s *pr) {
  /* We cannow write directly to the socket in case `redis_send` has scheduled
   * callbacks. */
  resp_object_s *cmd = resp_str2obj("QUIT", 4);
  cmd = resp_arr2obj(1, &cmd);
  redis_send(.uuid = uuid, .cmd = cmd, .move = 1);
  (void)pr;
}

/** called when a data is available, but will not run concurrently */
static void redis_on_data_deferred(intptr_t uuid, protocol_s *pr, void *d);
static void redis_on_data(intptr_t uuid, protocol_s *pr) {
  redis_protocol_s *r = (redis_protocol_s *)pr;
  uint8_t buffer[REDIS_READ_BUFFER];
  ssize_t len, limit, pos;
  resp_object_s *msg;
  limit = len = sock_read(uuid, buffer, REDIS_READ_BUFFER);
  if (len <= 0)
    return;
  pos = 0;
  while (len) {
    msg = resp_parser_feed(r->parser, buffer + pos, (size_t *)&len);
    if (msg) {
      // { // for debugging.
      //   uint8_t tmp[127] = {0};
      //   size_t blen = 127;
      //   resp_format(r->parser, tmp, &blen, msg);
      //   fprintf(stderr,
      //           "Got a response type %d (%lu long):\n%.*s\n Original:
      //           %.*s\n", msg->type, blen, (int)blen, tmp, (int)len, buffer +
      //           pos);
      // }
      if (r->on_pubsub && msg->type == RESP_PUBSUB) {
        /* *** PuB / Sub *** */
        r->on_pubsub(uuid, (resp_array_s *)msg, r->on_pubsub_udata);
      } else {
        /* *** Normal *** */
        if (msg->type == RESP_PUBSUB)
          msg->type = RESP_ARRAY;
        callback_s *c = fio_list_pop(callback_s, node, r->callbacks);
        if (c) {
          if (c->on_response)
            c->on_response(uuid, msg, c->udata);
          else if (r->fallback)
            r->fallback(uuid, msg, r->fallback_udata);
          callback_free(c);
        } else if (r->fallback) {
          r->fallback(uuid, (resp_object_s *)msg, r->fallback_udata);
        }
      }
      resp_free_object(msg);
      msg = NULL;
    }
    if (len == limit) {
      /* fragment events, it's edge polling, so we need to read everything */
      facil_defer(.uuid = uuid, .task = redis_on_data_deferred,
                  .task_type = FIO_PR_LOCK_TASK);
      return;
    }
    pos += len;
    limit = len = limit - len;
  }
}
static void redis_on_data_deferred(intptr_t uuid, protocol_s *pr, void *d) {
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    return;
  redis_on_data(uuid, pr);
  (void)d;
}
/**
 * This function can be used as a function pointer for both `facil_connect` and
 * `facil_listen` calls.
 */
#undef redis_create_protocol
protocol_s *redis_create_protocol(intptr_t uuid, void *ignored) {
  redis_protocol_s *r = malloc(sizeof(*r));
  *r = (redis_protocol_s){
      .protocol =
          {
              .service = REDIS_PROTOCOL_ID,
              .on_data = redis_on_data,
              .on_close = redis_on_close_protocol,
              .ping = redis_ping,
          },
      .callbacks = FIO_LIST_INIT(r->callbacks),
      .parser = resp_parser_new(),
  };
  return &r->protocol;
  (void)uuid;
  (void)ignored;
}

/* *****************************************************************************
Setting callbacks
***************************************************************************** */
static void mock_fallback(intptr_t uuid, resp_object_s *msg, void *udata) {
  (void)uuid;
  (void)msg;
  (void)udata;
}

typedef struct {
  enum {
    CB_ON_CLOSE,
    CB_ON_PUBSUB,
  } type;
  union {
    void (*on_close)(void *udata);
    void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg, void *udata);
    callback_s *on_response;
  };
  void *udata;
  size_t len;
} callback_set_s;

static void set_callback(intptr_t uuid, protocol_s *pr, void *udata) {
  redis_protocol_s *r = (void *)pr;
  callback_set_s *c = udata;
  switch (c->type) {
  case CB_ON_CLOSE:
    r->on_close = c->on_close;
    r->on_close_udata = c->udata;
    break;
  case CB_ON_PUBSUB:
    r->on_pubsub = c->on_pubsub;
    r->on_pubsub_udata = c->udata;
    break;
  }
  free(c);
  (void)uuid;
}

static void set_callback_fallback(intptr_t uuid, void *udata) {
  free(udata);
  (void)uuid;
}

/**
 * Sets a the on_close event callback.
 */
void redis_on_close(intptr_t uuid, void (*on_close)(void *udata), void *udata) {
  redis_protocol_s *r = (void *)facil_protocol_try_lock(uuid, FIO_PR_LOCK_TASK);
  if (r) {
    if (r->protocol.service == REDIS_PROTOCOL_ID) {
      r->on_close = on_close;
      r->on_close_udata = udata;
    }
    facil_protocol_unlock(&r->protocol, FIO_PR_LOCK_TASK);
    return;
  }

  callback_set_s *c = malloc(sizeof(*c));
  if (!c) {
    perror("ERROR (Redis), no memory");
    kill(0, SIGINT);
    return;
  }
  *c = (callback_set_s){
      .type = CB_ON_CLOSE, .on_close = on_close, .udata = udata};
  facil_defer(.uuid = uuid, .task = set_callback, .arg = c,
              .fallback = set_callback_fallback, .task_type = FIO_PR_LOCK_TASK);
}
/**
 * Sets a the `on_pubsub` event callback assuming the protocol for the socket is
 * locked (see {facil_protocol_try_lock}).
 */
void redis_on_close2(protocol_s *pr, void (*on_close)(void *udata),
                     void *udata) {
  redis_protocol_s *r = (void *)pr;
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    return;
  r->on_close = on_close;
  r->on_close_udata = udata;
}

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
                            void *udata) {
  redis_protocol_s *r = (void *)pr;
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    return;
  if (!fallback_handler)
    fallback_handler = mock_fallback;
  r->fallback = fallback_handler;
  r->fallback_udata = udata;
}

/**
 * Sets a the `on_pubsub` event callback. This will be called when messages
 * arrive through the use of the Pub/Sub semantics.
 *
 * If `udata` is set (NULL is ignored), than the function will also behave the
 * same as `redis_udata_set`.
 */
void redis_on_pubsub(intptr_t uuid,
                     void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg,
                                       void *udata),
                     void *udata) {
  redis_protocol_s *r = (void *)facil_protocol_try_lock(uuid, FIO_PR_LOCK_TASK);
  if (r) {
    if (r->protocol.service == REDIS_PROTOCOL_ID) {
      r->on_pubsub = on_pubsub;
      r->on_pubsub_udata = udata;
    }
    facil_protocol_unlock(&r->protocol, FIO_PR_LOCK_TASK);
    return;
  }

  callback_set_s *c = malloc(sizeof(*c));
  if (!c) {
    perror("ERROR (Redis), no memory");
    kill(0, SIGINT);
    return;
  }
  *c = (callback_set_s){
      .type = CB_ON_PUBSUB, .on_pubsub = on_pubsub, .udata = udata};
  facil_defer(.uuid = uuid, .task = set_callback, .arg = c,
              .fallback = set_callback_fallback, .task_type = FIO_PR_LOCK_TASK);
}
/**
 * Sets a the `on_pubsub` event callback assuming the protocol for the socket is
 * locked (see {facil_protocol_try_lock}).
 */
void redis_on_pubsub2(protocol_s *pr,
                      void (*on_pubsub)(intptr_t uuid, const resp_array_s *msg,
                                        void *udata),
                      void *udata) {
  redis_protocol_s *r = (void *)pr;
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    return;
  r->on_pubsub = on_pubsub;
  r->on_pubsub_udata = udata;
}

/* *****************************************************************************
Sending Commands or Responses
***************************************************************************** */

// struct redis_send_args_s {
//   intptr_t uuid;
//   const resp_array_s *cmd;
//   void (*on_response)(intptr_t uuid, const resp_object_s *response,
//                       void *udata);
//   void *udata;
//   uint8_t move;
// };
static void redis_perform_send_fallback(intptr_t uuid, void *args_) {
  struct redis_send_args_s *args = args_;
  resp_free_object((resp_object_s *)args->cmd);
  free(args);
  (void)uuid;
}

static void redis_perform_send(intptr_t uuid, protocol_s *pr, void *args_) {
  struct redis_send_args_s *args = args_;
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    goto finish;
  redis_protocol_s *r = (redis_protocol_s *)pr;
  size_t len = 0;
  resp_format(r->parser, NULL, &len, args->cmd);
  if (!len)
    goto finish;
  void *buff = malloc(len);
  if (!buff)
    goto finish;

  resp_format(r->parser, buff, &len, args->cmd);

  sock_write2(.uuid = uuid, .buffer = buff, .length = len, .move = 1);

  callback_s *cb = callback_alloc();
  cb->on_response = args->on_response;
  cb->udata = args->udata;
  fio_list_shift(callback_s, node, r->callbacks);

finish:
  resp_free_object(args->cmd);
  free(args);
}

/**
 * Sends RESP messages. Returns -1 on a known error and 0 if the message was
 * successfully validated to be sent.
 */
#undef redis_send
int redis_send(struct redis_send_args_s args) {
  if (!args.cmd || !args.uuid || !sock_isvalid(args.uuid))
    goto error;
  struct redis_send_args_s *c = malloc(sizeof(*c));
  *c = args;
  if (!args.move)
    resp_dup_object(args.cmd);
  facil_defer(.uuid = args.uuid, .task = redis_perform_send,
              .fallback = redis_perform_send_fallback, .arg = c,
              .task_type = FIO_PR_LOCK_TASK);
  return 0;
error:
  if (args.move && args.cmd)
    resp_free_object(args.cmd);
  return -1;
}
