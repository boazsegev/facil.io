/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "redis_engine.h"
#include "fio_llist.h"
#include "fiobj4sock.h"
#include "resp_parser.h"
#include "spnlock.inc"

#define REDIS_READ_BUFFER 8192
/* *****************************************************************************
The Redis Engine and Callbacks Object
***************************************************************************** */

typedef struct {
  uintptr_t id_protection;
  pubsub_engine_s en;
  struct {
    protocol_s protocol;
    uintptr_t uuid;
    resp_parser_s parser;
    size_t is_pub;
    fiobj_s *str;
    fiobj_s *ary;
    uintptr_t ary_count;
    uintptr_t buf_pos;
  } pub_data, sub_data;
  fio_ls_embd_s callbacks;
  spn_lock_i lock;
  char *address;
  char *port;
  char *auth;
  size_t auth_len;
  size_t ref;
  uint8_t ping_int;
  uint8_t flag;
  uint8_t buf[];
} redis_engine_s;

typedef struct {
  fio_ls_embd_s node;
  void (*callback)(pubsub_engine_s *e, fiobj_s *reply, void *udata);
  void *udata;
  size_t cmd_len;
  uint8_t cmd[];
} redis_callbacks_s;

#define sub2redis(pr) FIO_LS_EMBD_OBJ(redis_engine_s, sub_data.protocol, (pr))
#define pub2redis(pr) FIO_LS_EMBD_OBJ(redis_engine_s, pub_data.protocol, (pr))
#define en2redis(e) FIO_LS_EMBD_OBJ(redis_engine_s, en, (e))

/* *****************************************************************************
Command / Callback handling
***************************************************************************** */

static void redis_attach_cmd(redis_engine_s *r, redis_callbacks_s *cmd) {
  spn_lock(&r->lock);
  fio_ls_embd_push(&r->callbacks, &cmd->node);
  spn_unlock(&r->lock);
}

static void redis_send_cmd(redis_engine_s *r) {
  spn_lock(&r->lock);
  fio_ls_embd_s *p = r->callbacks.next;
  spn_unlock(&r->lock);
  if (p == &r->callbacks) {
    return;
  }
  redis_callbacks_s *cmd = FIO_LS_EMBD_OBJ(redis_callbacks_s, node, p);
  sock_write2(.uuid = r->pub_data.uuid, .buffer = cmd->cmd,
              .length = cmd->cmd_len, .dealloc = SOCK_DEALLOC_NOOP);
}
/* *****************************************************************************
Connection Establishment
***************************************************************************** */

static void redis_on_auth(pubsub_engine_s *e, fiobj_s *reply, void *udata) {
  if (reply->type != FIOBJ_T_TRUE) {
    fio_cstr_s s = fiobj_obj2cstr(reply);
    fprintf(stderr,
            "WARNING: (RedisConnection) Authentication FAILED.\n"
            "        %.*s\n",
            (int)s.len, s.data);
  }
}

static void redis_on_pub_connect(intptr_t uuid, void *pr) {
  pub2redis(pr)->pub_data.uuid = uuid;

  if (pub2redis(pr)->auth) {
    redis_callbacks_s *cb = malloc(sizeof(*cb) + pub2redis(pr)->auth_len);
    *cb = (redis_callbacks_s){.cmd_len = pub2redis(pr)->auth_len,
                              .callback = redis_on_auth};
    memcpy(cb->cmd, pub2redis(pr)->auth, pub2redis(pr)->auth_len);
    spn_lock(&pub2redis(pr)->lock);
    fio_ls_embd_unshift(&pub2redis(pr)->callbacks, &cb->node);
    spn_unlock(&pub2redis(pr)->lock);
    redis_send_cmd(pub2redis(pr));
  }
  facil_attach(uuid, pr);
  facil_set_timeout(uuid, pub2redis(pr)->ping_int);
}
static void redis_on_sub_connect(intptr_t uuid, void *pr) {
  sub2redis(pr)->sub_data.uuid = uuid;
  if (sub2redis(pr)->auth)
    sock_write2(.uuid = uuid, .buffer = sub2redis(pr)->auth,
                .length = sub2redis(pr)->auth_len,
                .dealloc = SOCK_DEALLOC_NOOP);
  facil_attach(uuid, pr);
  facil_set_timeout(uuid, pub2redis(pr)->ping_int);
  pubsub_engine_resubscribe(&sub2redis(pr)->en);
}

static void redis_on_pub_connect_fail(intptr_t uuid, void *pr) {
  pub2redis(pr)->pub_data.uuid =
      facil_connect(.address = pub2redis(pr)->address,
                    .port = pub2redis(pr)->port,
                    .on_connect = redis_on_pub_connect,
                    .udata = &pub2redis(pr)->pub_data.protocol,
                    .on_fail = redis_on_pub_connect_fail);
  (void)uuid;
}
static void redis_on_sub_connect_fail(intptr_t uuid, void *pr) {
  sub2redis(pr)->sub_data.uuid =
      facil_connect(.address = sub2redis(pr)->address,
                    .port = sub2redis(pr)->port,
                    .on_connect = redis_on_sub_connect,
                    .udata = &sub2redis(pr)->sub_data.protocol,
                    .on_fail = redis_on_sub_connect_fail);
}

/* *****************************************************************************
The Protocol layer (connection handling)
***************************************************************************** */

static void redis_pub_on_data(intptr_t uuid, protocol_s *pr) {
  uint8_t *buf = pub2redis(pr)->buf + REDIS_READ_BUFFER;
  ssize_t i = sock_read(uuid, buf + pub2redis(pr)->pub_data.buf_pos,
                        REDIS_READ_BUFFER - pub2redis(pr)->pub_data.buf_pos);
  if (i <= 0)
    return;
  pub2redis(pr)->pub_data.buf_pos += i;
  i = resp_parse(&pub2redis(pr)->pub_data.parser, buf,
                 pub2redis(pr)->pub_data.buf_pos);
  if (i) {
    memmove(buf, buf + pub2redis(pr)->pub_data.buf_pos - i, i);
  }
  pub2redis(pr)->pub_data.buf_pos = i;
}

static void redis_sub_on_data(intptr_t uuid, protocol_s *pr) {
  uint8_t *buf = sub2redis(pr)->buf + REDIS_READ_BUFFER;
  ssize_t i = sock_read(uuid, buf + sub2redis(pr)->sub_data.buf_pos,
                        REDIS_READ_BUFFER - sub2redis(pr)->sub_data.buf_pos);
  if (i <= 0)
    return;
  sub2redis(pr)->sub_data.buf_pos += i;
  i = resp_parse(&sub2redis(pr)->sub_data.parser, buf,
                 sub2redis(pr)->sub_data.buf_pos);
  if (i) {
    memmove(buf, buf + sub2redis(pr)->sub_data.buf_pos - i, i);
  }
  sub2redis(pr)->sub_data.buf_pos = i;
}

static void redis_pub_on_close(intptr_t uuid, protocol_s *pr) {
  if (pub2redis(pr)->flag) {
    fprintf(stderr,
            "WARNING: (redis) lost publishing connection to database\n");
    redis_on_pub_connect_fail(uuid, &pub2redis(pr)->pub_data.protocol);
  } else {
    if (spn_sub(&pub2redis(pr)->ref, 1))
      return;
    free(pub2redis(pr));
  }
}
static void redis_sub_on_close(intptr_t uuid, protocol_s *pr) {
  if (sub2redis(pr)->flag) {
    fprintf(stderr,
            "WARNING: (redis) lost subscribing connection to database\n");
    redis_on_sub_connect_fail(uuid, &sub2redis(pr)->sub_data.protocol);
  } else {
    if (spn_sub(&sub2redis(pr)->ref, 1))
      return;
    free(sub2redis(pr));
  }
}

static void redis_on_shutdown(intptr_t uuid, protocol_s *pr) {
  sock_write2(.uuid = uuid, .buffer = "*1\r\n$4\r\nQUIT\r\n", .length = 14,
              .dealloc = SOCK_DEALLOC_NOOP);
  (void)pr;
}
static void redis_ping(intptr_t uuid, protocol_s *pr) {
  sock_write2(.uuid = uuid, .buffer = "*1\r\n$4\r\nPING\r\n", .length = 14,
              .dealloc = SOCK_DEALLOC_NOOP);
  (void)pr;
}

/* *****************************************************************************
Engine Callbacks
***************************************************************************** */

static void redis_on_subscribe(const pubsub_engine_s *eng, fiobj_s *channel,
                               uint8_t use_pattern);
static void redis_on_unsubscribe(const pubsub_engine_s *eng, fiobj_s *channel,
                                 uint8_t use_pattern);
static int redis_on_publish(const pubsub_engine_s *eng, fiobj_s *channel,
                            fiobj_s *msg);
/* *****************************************************************************
Object Creation
***************************************************************************** */

#undef redis_engine_create
pubsub_engine_s *redis_engine_create(struct redis_engine_create_args args) {
  if (!args.port || !args.address)
    return NULL;
  size_t port_len = 0;
  size_t address_len = 0;
  if (args.auth && !args.auth_len)
    args.auth_len = strlen(args.auth);
  if (args.address)
    address_len = strlen(args.address);
  if (args.port)
    port_len = strlen(args.port);
  redis_engine_s *r =
      malloc(sizeof(*r) + REDIS_READ_BUFFER + REDIS_READ_BUFFER + 2 +
             address_len + port_len + (args.auth_len ? args.auth_len + 32 : 0));
  *r = (redis_engine_s){
      .id_protection = 15,
      .port = (char *)r->buf + (REDIS_READ_BUFFER + REDIS_READ_BUFFER),
      .address = (char *)r->buf + (REDIS_READ_BUFFER + REDIS_READ_BUFFER) +
                 port_len + 1,
      .auth = (char *)r->buf + (REDIS_READ_BUFFER + REDIS_READ_BUFFER) +
              port_len + address_len + 2,
      .auth_len = args.auth_len,
      .en =
          {
              .subscribe = redis_on_subscribe,
              .unsubscribe = redis_on_unsubscribe,
              .publish = redis_on_publish,
          },
      .pub_data =
          {
              .is_pub = 1,
              .protocol =
                  {
                      .on_data = redis_pub_on_data,
                      .on_close = redis_pub_on_close,
                      .ping = redis_ping,
                      .on_shutdown = redis_on_shutdown,
                  },
          },
      .sub_data =
          {
              .protocol =
                  {
                      .on_data = redis_sub_on_data,
                      .on_close = redis_sub_on_close,
                      .ping = redis_ping,
                      .on_shutdown = redis_on_shutdown,
                  },
          },
  };
  memcpy(r->port, args.port, port_len);
  r->port[port_len] = 0;
  memcpy(r->address, args.address, address_len);
  r->address[address_len] = 0;
  if (args.auth) {
    char *pos = r->auth;
    pos = memcpy(pos, "*2\r\n$4\r\nAUTH\r\n$", 15);
    pos += fio_ltoa(pos, args.auth_len, 10);
    *pos++ = '\r';
    *pos++ = '\n';
    pos = memcpy(pos, args.auth, args.auth_len);
    pos[0] = 0;
    args.auth_len = (uintptr_t)pos - (uintptr_t)r->auth;
  }
  pubsub_engine_register(&r->en);
  redis_on_pub_connect_fail(0, &r->pub_data.protocol);
  /* we don't need to listen for messages from each process, we publish to the
   * cluster and the channel list is synchronized.
   */
  if (defer_fork_pid() == 0) {
    redis_on_sub_connect_fail(0, &r->sub_data.protocol);
  }
  return &r->en;
}

void redis_engine_destroy(pubsub_engine_s *e) {
  redis_engine_s *r = en2redis(e);
  if (r->id_protection != 15) {
    fprintf(
        stderr,
        "FATAL ERROR: (redis) engine pointer incorrect, protection failure.\n");
    exit(-1);
  }
  pubsub_engine_deregister(e);
  r->flag = 0;
  sock_close(r->pub_data.uuid);
  sock_close(r->sub_data.uuid);
}

/* *****************************************************************************
RESP parser callbacks
***************************************************************************** */

/** a local static callback, called when the RESP message is complete. */
static int resp_on_message(resp_parser_s *parser);

/** a local static callback, called when a Number object is parsed. */
static int resp_on_number(resp_parser_s *parser, int64_t num);
/** a local static callback, called when a OK message is received. */
static int resp_on_okay(resp_parser_s *parser);
/** a local static callback, called when NULL is received. */
static int resp_on_null(resp_parser_s *parser);

/**
 * a local static callback, called when a String should be allocated.
 *
 * `str_len` is the expected number of bytes that will fill the final string
 * object, without any NUL byte marker (the string might be binary).
 *
 * If this function returns any value besides 0, parsing is stopped.
 */
static int resp_on_start_string(resp_parser_s *parser, size_t str_len);
/** a local static callback, called as String objects are streamed. */
static int resp_on_string_chunk(resp_parser_s *parser, void *data, size_t len);
/** a local static callback, called when a String object had finished streaming.
 */
static int resp_on_end_string(resp_parser_s *parser);

/** a local static callback, called an error message is received. */
static int resp_on_err_msg(resp_parser_s *parser, void *data, size_t len);

/**
 * a local static callback, called when an Array should be allocated.
 *
 * `array_len` is the expected number of objects that will fill the Array
 * object.
 *
 * There's no `resp_on_end_array` callback since the RESP protocol assumes the
 * message is finished along with the Array (`resp_on_message` is called).
 * However, just in case a non-conforming client/server sends nested Arrays, the
 * callback should test against possible overflow or nested Array endings.
 *
 * If this function returns any value besides 0, parsing is stopped.
 */
static int resp_on_start_array(resp_parser_s *parser, size_t array_len);

/** a local static callback, called when a parser / protocol error occurs. */
static int resp_on_parser_error(resp_parser_s *parser);
