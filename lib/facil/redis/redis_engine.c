/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#define FIO_INCLUDE_LINKED_LIST
#define FIO_INCLUDE_STR
#include <fio.h>

#include "fiobj.h"
#include "fiobj4sock.h"

#include "redis_engine.h"
#include "resp_parser.h"

#define REDIS_READ_BUFFER 8192
/* *****************************************************************************
The Redis Engine and Callbacks Object
***************************************************************************** */

typedef struct {
  pubsub_engine_s en;
  struct redis_engine_internal_s {
    fio_protocol_s protocol;
    intptr_t uuid;
    resp_parser_s parser;
    void (*on_message)(struct redis_engine_internal_s *parser, FIOBJ msg);
    FIOBJ str;
    FIOBJ ary;
    uintptr_t ary_count;
    uintptr_t buf_pos;
  } pub_data, sub_data;
  subscription_s *publication_forwarder;
  subscription_s *cmd_forwarder;
  subscription_s *cmd_reply;
  char *address;
  char *port;
  char *auth;
  FIOBJ last_ch;
  size_t auth_len;
  size_t ref;
  fio_ls_embd_s queue;
  fio_lock_i lock;
  uint8_t ping_int;
  uint8_t flag;
  uint8_t buf[];
} redis_engine_s;

typedef struct {
  fio_ls_embd_s node;
  void (*callback)(pubsub_engine_s *e, FIOBJ reply, void *udata);
  void *udata;
  size_t cmd_len;
  uint8_t cmd[];
} redis_commands_s;

/** converts from a publishing protocol to an `redis_engine_s`. */
#define pub2redis(pr) FIO_LS_EMBD_OBJ(redis_engine_s, pub_data, (pr))
/** converts from a subscribing protocol to an `redis_engine_s`. */
#define sub2redis(pr) FIO_LS_EMBD_OBJ(redis_engine_s, pub_data, (pr))

/** converts from a `resp_parser_s` to the internal data structure. */
#define parser2data(prsr)                                                      \
  FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, (prsr))

/* releases any resources used by an internal engine*/
static inline void redis_internal_reset(struct redis_engine_internal_s *i) {
  i->buf_pos = 0;
  i->parser = (resp_parser_s){.obj_countdown = 0, .expecting = 0};
  fiobj_free(i->str);
  i->str = FIOBJ_INVALID;
  fiobj_free(i->ary);
  i->ary = FIOBJ_INVALID;
  i->ary_count = 0;
  i->uuid = -1;
}

/** cleans up and frees the engine data. */
static inline void redis_free(redis_engine_s *r) {
  if (fio_atomic_sub(&r->ref, 1))
    return;
  redis_internal_reset(&r->pub_data);
  redis_internal_reset(&r->sub_data);
  fiobj_free(r->last_ch);
  while (fio_ls_embd_any(&r->queue)) {
    fio_free(
        FIO_LS_EMBD_OBJ(redis_commands_s, node, fio_ls_embd_pop(&r->queue)));
  }
  fio_unsubscribe(r->publication_forwarder);
  r->publication_forwarder = NULL;
  fio_unsubscribe(r->cmd_forwarder);
  r->cmd_forwarder = NULL;
  fio_unsubscribe(r->cmd_reply);
  r->cmd_reply = NULL;
  fio_free(r);
}

/* *****************************************************************************
Simple RESP formatting
***************************************************************************** */

/**
 * Converts FIOBJ objects into a RESP string (client mode).
 *
 * Don't call `fiobj_free`, object will self-destruct.
 */
static FIOBJ fiobj2resp(FIOBJ dest, fio_str_info_s obj1, FIOBJ obj2) {
  if (!obj2 || FIOBJ_IS_NULL(obj2)) {
    fiobj_str_write(dest, "*1\r\n$", 5);
    fiobj_str_write_i(dest, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, obj1.data, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);
  } else if (FIOBJ_TYPE(obj2) == FIOBJ_T_ARRAY) {
    size_t count = fiobj_ary_count(obj2);
    fiobj_str_write(dest, "*", 1);
    fiobj_str_write_i(dest, count + 1);
    fiobj_str_write(dest, "\r\n$", 3);

    fiobj_str_write_i(dest, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, obj1.data, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);

    FIOBJ *ary = fiobj_ary2ptr(obj2);

    for (size_t i = 0; i < count; ++i) {
      fio_str_info_s s = fiobj_obj2cstr(ary[i]);
      fiobj_str_write(dest, "$", 1);
      fiobj_str_write_i(dest, s.len);
      fiobj_str_write(dest, "\r\n", 2);
      fiobj_str_write(dest, s.data, s.len);
      fiobj_str_write(dest, "\r\n", 2);
    }

  } else if (FIOBJ_TYPE(obj2) == FIOBJ_T_HASH) {
    FIOBJ json = fiobj_obj2json(obj2, 0);
    fiobj_str_write(dest, "*2\r\n$", 5);
    fiobj_str_write_i(dest, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, obj1.data, obj1.len);
    fiobj_str_write(dest, "\r\n$", 3);
    fio_str_info_s s = fiobj_obj2cstr(json);
    fiobj_str_write_i(dest, s.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_free(json);

  } else {
    fiobj_str_write(dest, "*2\r\n$", 5);
    fiobj_str_write_i(dest, obj1.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, obj1.data, obj1.len);
    fiobj_str_write(dest, "\r\n$", 3);
    fio_str_info_s s = fiobj_obj2cstr(obj2);
    fiobj_str_write_i(dest, s.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);
  }
  return dest;
}

static inline FIOBJ fiobj2resp_tmp(fio_str_info_s obj1, FIOBJ obj2) {
  return fiobj2resp(fiobj_str_tmp(), obj1, obj2);
}

/* *****************************************************************************
RESP parser callbacks
***************************************************************************** */

/** a local static callback, called when a parser / protocol error occurs. */
static int resp_on_parser_error(resp_parser_s *parser) {
  struct redis_engine_internal_s *i = parser2data(parser);
  FIO_LOG_STATE(
      "ERROR: (redis) parser error - attempting to restart connection.\n");
  fio_close(i->uuid);
  return -1;
}

/** a local static callback, called when the RESP message is complete. */
static int resp_on_message(resp_parser_s *parser) {
  struct redis_engine_internal_s *i = parser2data(parser);
  FIOBJ msg = i->ary ? i->ary : i->str;
  i->on_message(i, msg);
  /* cleanup */
  fiobj_free(msg);
  i->ary = FIOBJ_INVALID;
  i->str = FIOBJ_INVALID;
  return 0;
}

/** a local helper to add parsed objects to the data store. */
static inline void resp_add_obj(struct redis_engine_internal_s *dest, FIOBJ o) {
  if (dest->ary) {
    if (!dest->ary_count)
      FIO_LOG_STATE(
          "ERROR: (redis) array overflow indicates a protocol error.\n");
    fiobj_ary_push(dest->ary, o);
    --dest->ary_count;
  }
  dest->str = o;
}

/** a local static callback, called when a Number object is parsed. */
static int resp_on_number(resp_parser_s *parser, int64_t num) {
  struct redis_engine_internal_s *data = parser2data(parser);
  resp_add_obj(data, fiobj_num_new(num));
  return 0;
}
/** a local static callback, called when a OK message is received. */
static int resp_on_okay(resp_parser_s *parser) {
  struct redis_engine_internal_s *data = parser2data(parser);
  resp_add_obj(data, fiobj_true());
  return 0;
}
/** a local static callback, called when NULL is received. */
static int resp_on_null(resp_parser_s *parser) {
  struct redis_engine_internal_s *data = parser2data(parser);
  resp_add_obj(data, fiobj_null());
  return 0;
}

/**
 * a local static callback, called when a String should be allocated.
 *
 * `str_len` is the expected number of bytes that will fill the final string
 * object, without any NUL byte marker (the string might be binary).
 *
 * If this function returns any value besides 0, parsing is stopped.
 */
static int resp_on_start_string(resp_parser_s *parser, size_t str_len) {
  struct redis_engine_internal_s *data = parser2data(parser);
  resp_add_obj(data, fiobj_str_buf(str_len));
  return 0;
}
/** a local static callback, called as String objects are streamed. */
static int resp_on_string_chunk(resp_parser_s *parser, void *data, size_t len) {
  struct redis_engine_internal_s *i = parser2data(parser);
  fiobj_str_write(i->str, data, len);
  return 0;
}
/** a local static callback, called when a String object had finished
 * streaming.
 */
static int resp_on_end_string(resp_parser_s *parser) {
  return 0;
  (void)parser;
}

/** a local static callback, called an error message is received. */
static int resp_on_err_msg(resp_parser_s *parser, void *data, size_t len) {
  struct redis_engine_internal_s *i = parser2data(parser);
  resp_add_obj(i, fiobj_str_new(data, len));
  return 0;
}

/**
 * a local static callback, called when an Array should be allocated.
 *
 * `array_len` is the expected number of objects that will fill the Array
 * object.
 *
 * There's no `resp_on_end_array` callback since the RESP protocol assumes the
 * message is finished along with the Array (`resp_on_message` is called).
 * However, just in case a non-conforming client/server sends nested Arrays,
 * the callback should test against possible overflow or nested Array endings.
 *
 * If this function returns any value besides 0, parsing is stopped.
 */
static int resp_on_start_array(resp_parser_s *parser, size_t array_len) {
  struct redis_engine_internal_s *i = parser2data(parser);
  if (i->ary) {
    /* this is an error ... */
    FIO_LOG_STATE("ERROR: (redis) RESP protocol violation "
                  "(array within array).\n");
    return -1;
  }
  i->ary = fiobj_ary_new2(array_len);
  i->ary_count = array_len;
  return 0;
}

/* *****************************************************************************
Publication and Command Handling
***************************************************************************** */

/* the deferred callback handler */
static void redis_perform_callback(void *e, void *cmd_) {
  redis_commands_s *cmd = cmd_;
  FIOBJ reply = (FIOBJ)cmd->node.next;
  if (cmd->callback)
    cmd->callback(e, reply, cmd->udata);
  fiobj_free(reply);
  // FIO_LOG_STATE( "Handled: %s\n", cmd->cmd);
  fio_free(cmd);
}

/* attach a command to the queue */
static void redis_attach_cmd(redis_engine_s *r, redis_commands_s *cmd) {
  fio_lock(&r->lock);
  fio_ls_embd_push(&r->queue, &cmd->node);
  fio_write2(r->pub_data.uuid, .data.buffer = cmd->cmd, .length = cmd->cmd_len,
             .after.dealloc = FIO_DEALLOC_NOOP);
  fio_unlock(&r->lock);
}

/** a local static callback, called when the RESP message is complete. */
static void resp_on_pub_message(struct redis_engine_internal_s *i, FIOBJ msg) {
  redis_engine_s *r = pub2redis(i);
  /* publishing / command parser */
  fio_lock(&r->lock);
  fio_ls_embd_s *node = fio_ls_embd_shift(&r->queue);
  fio_unlock(&r->lock);
  if (!node) {
    /* TODO: possible ping? from server?! not likely... */
    FIO_LOG_STATE(
        "WARNING: (redis) received a reply when no command was sent\n");
    return;
  }
  node->next = (void *)fiobj_dup(msg);
  fio_defer(redis_perform_callback, &r->en,
            FIO_LS_EMBD_OBJ(redis_commands_s, node, node));
}

/**
 * Sends a Redis command through the engine's connection.
 *
 * The response will be sent back using the optional callback. `udata` is passed
 * along untouched.
 *
 * The message will be resent on network failures, until a response validates
 * the fact that the command was sent (or the engine is destroyed).
 *
 * Note: NEVER call Pub/Sub commands using this function, as it will violate the
 * Redis connection's protocol (best case scenario, a disconnection will occur
 * before and messages are lost).
 */
// static intptr_t redis_engine_send_internal(
//     pubsub_engine_s *engine, fio_str_info_s command, FIOBJ data,
//     void (*callback)(pubsub_engine_s *e, FIOBJ reply, void *udata),
//     void *udata) {
//   if ((uintptr_t)engine < 4) {
//     FIO_LOG_STATE("WARNING: (redis send) trying to use one of the "
//                   "core engines\n");
//     return -1;
//   }
//   redis_engine_s *r = (redis_engine_s *)engine;
//   FIOBJ tmp = fiobj2resp_tmp(command, data);
//   fio_str_info_s cmd_str = fiobj_obj2cstr(tmp);
//   redis_commands_s *cmd = fio_malloc(sizeof(*cmd) + cmd_str.len + 1);
//   *cmd = (redis_commands_s){
//       .callback = callback, .udata = udata, .cmd_len = cmd_str.len};
//   memcpy(cmd->cmd, cmd_str.data, cmd_str.len + 1);
//   redis_attach_cmd(r, cmd);
//   return 0;
// }

/* *****************************************************************************
Subscription Message Handling
***************************************************************************** */

/** a local static callback, called when the RESP message is complete. */
static void resp_on_sub_message(struct redis_engine_internal_s *i, FIOBJ msg) {
  redis_engine_s *r = sub2redis(i);
  /* subscriotion parser */
  if (FIOBJ_TYPE(msg) != FIOBJ_T_ARRAY) {
    if (FIOBJ_TYPE(msg) != FIOBJ_T_STRING || fiobj_obj2cstr(msg).len != 4 ||
        fiobj_obj2cstr(msg).data[0] != 'P') {
      FIO_LOG_STATE("WARNING: (redis) unexpected data format in "
                    "subscription stream:\n");
      fio_str_info_s tmp = fiobj_obj2cstr(msg);
      FIO_LOG_STATE("     %s\n", tmp.data);
    }
  } else {
    // FIOBJ *ary = fiobj_ary2ptr(msg);
    // for (size_t i = 0; i < fiobj_ary_count(msg); ++i) {
    //   fio_str_info_s tmp = fiobj_obj2cstr(ary[i]);
    //   fprintf(stderr, "(%lu) %s\n", (unsigned long)i, tmp.data);
    // }
    fio_str_info_s tmp = fiobj_obj2cstr(fiobj_ary_index(msg, 0));
    if (tmp.len == 7) { /* "message"  */
      fiobj_free(r->last_ch);
      r->last_ch = fiobj_dup(fiobj_ary_index(msg, 1));
      fio_publish(.channel = fiobj_obj2cstr(r->last_ch),
                  .message = fiobj_obj2cstr(fiobj_ary_index(msg, 2)),
                  .engine = FIO_PUBSUB_CLUSTER);
    } else if (tmp.len == 8) { /* "pmessage" */
      if (!fiobj_iseq(r->last_ch, fiobj_ary_index(msg, 2)))
        fio_publish(.channel = fiobj_obj2cstr(fiobj_ary_index(msg, 2)),
                    .message = fiobj_obj2cstr(fiobj_ary_index(msg, 3)),
                    .engine = FIO_PUBSUB_CLUSTER);
    }
  }
}

/* *****************************************************************************
Connection Callbacks (fio_protocol_s) and Engine
***************************************************************************** */

/** defined later - connects to Redis */
static inline void redis_connect(redis_engine_s *r,
                                 struct redis_engine_internal_s *i);

/** Called when a data is available, but will not run concurrently */
static void redis_on_data(intptr_t uuid, fio_protocol_s *pr) {
  struct redis_engine_internal_s *internal =
      (struct redis_engine_internal_s *)pr;
  uint8_t *buf;
  if (internal->on_message == resp_on_sub_message) {
    buf = sub2redis(pr)->buf + REDIS_READ_BUFFER;
  } else {
    buf = pub2redis(pr)->buf;
  }
  ssize_t i = fio_read(uuid, buf + internal->buf_pos,
                       REDIS_READ_BUFFER - internal->buf_pos);
  if (i <= 0)
    return;
  internal->buf_pos += i;
  i = resp_parse(&internal->parser, buf, internal->buf_pos);
  if (i) {
    memmove(buf, buf + internal->buf_pos - i, i);
  }
  internal->buf_pos = i;
}

/** Called when the connection was closed, but will not run concurrently */
static void redis_on_close(intptr_t uuid, fio_protocol_s *pr) {
  struct redis_engine_internal_s *internal =
      (struct redis_engine_internal_s *)pr;
  redis_internal_reset(internal);
  redis_engine_s *r;
  if (internal->on_message == resp_on_sub_message) {
    r = sub2redis(pr);
    fiobj_free(r->last_ch);
    r->last_ch = FIOBJ_INVALID;
    if (r->flag) {
      /* reconnection for subscription connection. */
      fio_atomic_sub(&r->ref, 1);
      redis_connect(r, internal);
    } else {
      redis_free(r);
    }
  } else {
    r = pub2redis(pr);
    if ((r->flag && r->sub_data.uuid != -1) || fio_ls_embd_any(&r->queue)) {
      /* reconnection for publication / command connection. */
      fio_atomic_sub(&r->ref, 1);
      redis_connect(r, internal);
    } else {
      redis_free(r);
    }
  }
  (void)uuid;
}

/** Called before the facil.io reactor is shut down. */
static uint8_t redis_on_shutdown(intptr_t uuid, fio_protocol_s *pr) {
  fio_write2(uuid, .data.buffer = "*1\r\n$4\r\nQUIT\r\n", .length = 14,
             .after.dealloc = FIO_DEALLOC_NOOP);
  return 0;
  (void)pr;
}

/** Called on connection timeout. */
static void redis_sub_ping(intptr_t uuid, fio_protocol_s *pr) {
  fio_write2(uuid, .data.buffer = "*1\r\n$4\r\nPING\r\n", .length = 14,
             .after.dealloc = FIO_DEALLOC_NOOP);
  (void)pr;
}

/** Called on connection timeout. */
static void redis_pub_ping(intptr_t uuid, fio_protocol_s *pr) {
  redis_engine_s *r = pub2redis(pr);
  if (fio_ls_embd_any(&r->queue)) {
    FIO_LOG_STATE(
        "WARNING: (redis) Redis server unresponsive, disconnecting.\n");
    fio_close(uuid);
    return;
  }
  redis_commands_s *cmd = fio_malloc(sizeof(*cmd) + 15);
  *cmd = (redis_commands_s){.cmd_len = 14};
  memcpy(cmd->cmd, "*1\r\n$4\r\nPING\r\n\0", 15);
  redis_attach_cmd(r, cmd);
}

/* *****************************************************************************
Connecting to Redis
***************************************************************************** */

static void redis_on_auth(pubsub_engine_s *e, FIOBJ reply, void *udata) {
  if (FIOBJ_TYPE_IS(reply, FIOBJ_T_TRUE)) {
    fio_str_info_s s = fiobj_obj2cstr(reply);
    FIO_LOG_STATE("WARNING: (redis) Authentication FAILED.\n"
                  "        %.*s\n",
                  (int)s.len, s.data);
  }
  (void)e;
  (void)udata;
}

static void redis_on_connect(intptr_t uuid, void *i_) {
  struct redis_engine_internal_s *i = i_;
  redis_engine_s *r;
  if (i->uuid != uuid) {
    goto mismatch;
  }

  if (i->on_message == resp_on_sub_message) {
    r = sub2redis(i);
    if (r->auth)
      fio_write2(uuid, .data.buffer = r->auth, .length = r->auth_len,
                 .after.dealloc = FIO_DEALLOC_NOOP);
    fio_pubsub_reattach(&r->en);
    if (r->pub_data.uuid == -1) {
      redis_connect(r, &r->pub_data);
    }
    FIO_LOG_STATE("INFO: (redis %d) subscription connection established.\n",
                  (int)getpid());
  } else {
    r = pub2redis(i);
    if (r->auth) {
      redis_commands_s *cmd = fio_malloc(sizeof(*cmd) + r->auth_len);
      *cmd =
          (redis_commands_s){.cmd_len = r->auth_len, .callback = redis_on_auth};
      memcpy(cmd->cmd, r->auth, r->auth_len);
      fio_lock(&r->lock);
      fio_ls_embd_unshift(&r->queue, &cmd->node);
      fio_unlock(&r->lock);
    }
    fio_lock(&r->lock);
    FIO_LS_EMBD_FOR(&r->queue, node) {
      redis_commands_s *cmd = FIO_LS_EMBD_OBJ(redis_commands_s, node, node);
      fio_write2(uuid, .data.buffer = cmd->cmd, .length = cmd->cmd_len,
                 .after.dealloc = FIO_DEALLOC_NOOP);
    }
    fio_unlock(&r->lock);
    FIO_LOG_STATE("INFO: (redis %d) publicastion connection established.\n",
                  (int)getpid());
  }

  fio_set_timeout(uuid, r->ping_int);
  i->protocol.rsv = 0;
  fio_attach(uuid, &i->protocol);

  return;
mismatch:
  fio_close(uuid);
  if (i->on_message == resp_on_sub_message) {
    r = sub2redis(i);
  } else {
    r = pub2redis(i);
  }
  redis_free(r);
}

static void redis_on_connect_failed(intptr_t uuid, void *i_) {
  struct redis_engine_internal_s *i = i_;
  i->uuid = -1;
  i->protocol.on_close(-1, &i->protocol);
  (void)uuid;
}

static inline void redis_connect(redis_engine_s *r,
                                 struct redis_engine_internal_s *i) {
  fio_atomic_add(&r->ref, 1);
  i->uuid = fio_connect(.address = r->address, .port = r->port,
                        .on_connect = redis_on_connect, .udata = i,
                        .on_fail = redis_on_connect_failed);
}

/* *****************************************************************************
Engine / Bridge Callbacks (Root Process)
***************************************************************************** */

static void redis_on_subscribe_root(const pubsub_engine_s *eng,
                                    fio_str_info_s channel,
                                    fio_match_fn match) {
  redis_engine_s *r = (redis_engine_s *)eng;
  if (r->sub_data.uuid != -1) {
    FIOBJ cmd = fiobj_str_buf(96 + channel.len);
    if (match == FIO_MATCH_GLOB)
      fiobj_str_write(cmd, "*2\r\n$10\r\nPSUBSCRIBE\r\n$", 22);
    else
      fiobj_str_write(cmd, "*2\r\n$9\r\nSUBSCRIBE\r\n$", 20);
    fiobj_str_join(cmd, fiobj_num_tmp(channel.len));
    fiobj_str_write(cmd, "\r\n", 2);
    fiobj_str_write(cmd, channel.data, channel.len);
    fiobj_str_write(cmd, "\r\n", 2);
    // {
    //   fio_str_info_s s = fiobj_obj2cstr(cmd);
    //   fprintf(stderr, "%s\n", s.data);
    // }
    fiobj_send_free(r->sub_data.uuid, cmd);
  }
}

static void redis_on_unsubscribe_root(const pubsub_engine_s *eng,
                                      fio_str_info_s channel,
                                      fio_match_fn match) {
  redis_engine_s *r = (redis_engine_s *)eng;
  if (r->sub_data.uuid != -1) {
    fio_str_s *cmd = fio_str_new2();
    fio_str_capa_assert(cmd, 96 + channel.len);
    if (match == FIO_MATCH_GLOB)
      fio_str_write(cmd, "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$", 24);
    else
      fio_str_write(cmd, "*2\r\n$11\r\nUNSUBSCRIBE\r\n$", 23);
    fio_str_write_i(cmd, channel.len);
    fio_str_write(cmd, "\r\n", 2);
    fio_str_write(cmd, channel.data, channel.len);
    fio_str_write(cmd, "\r\n", 2);
    // {
    //   fio_str_info_s s = fiobj_obj2cstr(cmd);
    //   fprintf(stderr, "%s\n", s.data);
    // }
    fio_str_send_free2(r->sub_data.uuid, cmd);
  }
}

static void redis_on_publish_root(const pubsub_engine_s *eng,
                                  fio_str_info_s channel, fio_str_info_s msg,
                                  uint8_t is_json) {
  redis_engine_s *r = (redis_engine_s *)eng;
  redis_commands_s *cmd = fio_malloc(sizeof(*cmd) + channel.len + msg.len + 96);
  *cmd = (redis_commands_s){.cmd_len = 0};
  memcpy(cmd->cmd, "*3\r\n$7\r\nPUBLISH\r\n$", 18);
  char *buf = (char *)cmd->cmd + 18;
  buf += fio_ltoa((void *)buf, channel.len, 10);
  *buf++ = '\r';
  *buf++ = '\n';
  memcpy(buf, channel.data, channel.len);
  buf += channel.len;
  *buf++ = '\r';
  *buf++ = '\n';
  *buf++ = '$';
  buf += fio_ltoa(buf, msg.len, 10);
  *buf++ = '\r';
  *buf++ = '\n';
  memcpy(buf, msg.data, msg.len);
  buf += msg.len;
  *buf++ = '\r';
  *buf++ = '\n';
  *buf = 0;
  // fprintf(stderr, "%s\n", cmd->cmd);
  cmd->cmd_len = (uintptr_t)buf - (uintptr_t)(cmd + 1);
  redis_attach_cmd(r, cmd);
  return;
  (void)is_json;
}

/* *****************************************************************************
Engine / Bridge Stub Callbacks (Child Process)
***************************************************************************** */

static void redis_on_mock_subscribe_child(const pubsub_engine_s *eng,
                                          fio_str_info_s channel,
                                          fio_match_fn match) {
  /* do nothing, root process is notified about (un)subscriptions by facil.io */
  (void)eng;
  (void)channel;
  (void)match;
}

static void redis_on_publish_child(const pubsub_engine_s *eng,
                                   fio_str_info_s channel, fio_str_info_s msg,
                                   uint8_t is_json) {
  /* forward publication request to Root */
  fio_publish(.filter = -1, .channel = channel, .message = msg,
              .engine = FIO_PUBSUB_ROOT, .is_json = is_json);
  (void)eng;
}

/* *****************************************************************************
Root Publication Handler
***************************************************************************** */

/* listens to filter -1 and publishes and messages */
static void redis_on_internal_publish(fio_msg_s *msg) {
  redis_on_publish_root(msg->udata1, msg->channel, msg->msg, msg->is_json);
}

/* *****************************************************************************
Sending commands using the Root connection
***************************************************************************** */

/* callback from the Redis reply */
static void redis_forward_reply(pubsub_engine_s *e, FIOBJ reply, void *udata) {
  uint8_t *data = udata;
  pubsub_engine_s *engine = (pubsub_engine_s *)fio_str2u64(data + 0);
  if (engine != e)
    return;
  int32_t pid = (int32_t)fio_str2u32(data + 24);
  FIOBJ rp = fiobj_obj2json(reply, 0);
  fio_publish(.filter = (-10 - pid), .channel.data = (char *)data,
              .channel.len = 28, .message = fiobj_obj2cstr(rp), .is_json = 1);
  fiobj_free(rp);
}

/* listens to channel -2 for commands that need to be sent (only ROOT) */
static void redis_on_internal_cmd(fio_msg_s *msg) {
  // void*(void *)fio_str2u64(msg->msg.data);
  pubsub_engine_s *engine = (pubsub_engine_s *)fio_str2u64(msg->msg.data + 0);
  if (engine != msg->udata1)
    return;
  redis_commands_s *cmd = fio_malloc(sizeof(*cmd) + msg->msg.len + 28);
  FIO_ASSERT_ALLOC(cmd);
  *cmd = (redis_commands_s){.callback = redis_forward_reply,
                            .udata = (cmd->cmd + msg->msg.len),
                            .cmd_len = msg->channel.len};
  memcpy(cmd->cmd, msg->msg.data, msg->msg.len);
  memcpy(cmd->cmd + msg->msg.len, msg->channel.data, 28);
  redis_attach_cmd((redis_engine_s *)engine, cmd);
}

/* Listens on filter `-10 -getpid()` for incoming reply data */
static void redis_on_internal_reply(fio_msg_s *msg) {
  pubsub_engine_s *engine = (pubsub_engine_s *)fio_str2u64(msg->msg.data + 0);
  if (engine != msg->udata1)
    return;
  FIOBJ reply;
  fiobj_json2obj(&reply, msg->msg.data, msg->msg.len);
  void (*callback)(pubsub_engine_s *, FIOBJ, void *) = (void (*)(
      pubsub_engine_s *, FIOBJ, void *))fio_str2u64(msg->channel.data + 8);
  void *udata = (void *)fio_str2u64(msg->channel.data + 16);
  callback(engine, reply, udata);
  fiobj_free(reply);
}

/* publishes a Redis command to Root's filter -2 */
intptr_t redis_engine_send(pubsub_engine_s *engine, fio_str_info_s command,
                           FIOBJ data,
                           void (*callback)(pubsub_engine_s *e, FIOBJ reply,
                                            void *udata),
                           void *udata) {
  if ((uintptr_t)engine < 4) {
    fprintf(stderr, "WARNING: (redis send) trying to use one of the "
                    "core engines\n");
    return -1;
  }
  // if(fio_is_master()) {
  // FIOBJ resp = fiobj2resp_tmp(fio_str_info_s obj1, FIOBJ obj2);
  // TODO...
  // } else {
  /* forward publication request to Root */
  fio_str_s tmp = FIO_STR_INIT;
  uint64_t tmp64;
  uint32_t tmp32;
  /* combine metadata */
  tmp64 = fio_lton64((uint64_t)engine);
  fio_str_write(&tmp, &tmp64, sizeof(tmp64));
  tmp64 = fio_lton64((uint64_t)callback);
  fio_str_write(&tmp, &tmp64, sizeof(tmp64));
  tmp64 = fio_lton64((uint64_t)udata);
  fio_str_write(&tmp, &tmp64, sizeof(tmp64));
  tmp32 = fio_lton32((uint32_t)getpid());
  fio_str_write(&tmp, &tmp32, sizeof(tmp32));
  FIOBJ cmd = fiobj2resp_tmp(command, data);
  fio_publish(.filter = -2, .channel = fio_str_info(&tmp),
              .message = fiobj_obj2cstr(cmd), .engine = FIO_PUBSUB_ROOT,
              .is_json = 0);
  fio_str_free(&tmp);
  // }
  return 0;
}

/* *****************************************************************************
Redis Engine Creation
***************************************************************************** */

static void redis_on_facil_start(void *r_) {
  redis_engine_s *r = r_;
  r->flag = 1;
  if (!fio_is_valid(r->sub_data.uuid)) {
    redis_connect(r, &r->sub_data);
  }
}
static void redis_on_facil_shutdown(void *r_) {
  redis_engine_s *r = r_;
  r->flag = 0;
}

static void redis_on_facil_stop(void *r_) {
  redis_engine_s *r = r_;
  r->flag = 0;
}

static void redis_on_engine_fork(void *r_) {
  redis_engine_s *r = r_;
  r->flag = 0;
  r->lock = FIO_LOCK_INIT;
  fio_force_close(r->pub_data.uuid);
  fio_force_close(r->sub_data.uuid);
  while (fio_ls_embd_any(&r->queue)) {
    redis_commands_s *cmd =
        FIO_LS_EMBD_OBJ(redis_commands_s, node, fio_ls_embd_pop(&r->queue));
    fio_free(cmd);
  }
  r->en = (pubsub_engine_s){
      .subscribe = redis_on_mock_subscribe_child,
      .unsubscribe = redis_on_mock_subscribe_child,
      .publish = redis_on_publish_child,
  };
  fio_unsubscribe(r->publication_forwarder);
  r->publication_forwarder = NULL;
  fio_unsubscribe(r->cmd_forwarder);
  r->cmd_forwarder = NULL;
  fio_unsubscribe(r->cmd_reply);
  r->cmd_reply = fio_subscribe(.filter = -10 - (int32_t)getpid(),
                               .on_message = redis_on_internal_reply);
}

pubsub_engine_s *redis_engine_create
FIO_IGNORE_MACRO(struct redis_engine_create_args args) {
  if (getpid() != fio_parent_pid()) {
    fprintf(stderr, "FATAL ERROR: (redis) Redis engine initialization can only "
                    "be performed in the Root process.\n");
  }
  if (!args.address.len && args.address.data)
    args.address.len = strlen(args.address.data);
  if (!args.port.len && args.port.data)
    args.port.len = strlen(args.port.data);
  if (!args.auth.len && args.auth.data)
    args.auth.len = strlen(args.auth.data);
  redis_engine_s *r =
      fio_malloc(sizeof(*r) + args.port.len + 1 + args.address.len + 1 +
                 args.auth.len + 1 + (REDIS_READ_BUFFER * 2));
  FIO_ASSERT_ALLOC(r);
  *r = (redis_engine_s){
      .en =
          {
              .subscribe = redis_on_subscribe_root,
              .unsubscribe = redis_on_unsubscribe_root,
              .publish = redis_on_publish_root,
          },
      .pub_data =
          {
              .protocol =
                  {
                      .on_data = redis_on_data,
                      .on_close = redis_on_close,
                      .on_shutdown = redis_on_shutdown,
                      .ping = redis_pub_ping,
                  },
              .uuid = -1,
              .on_message = resp_on_pub_message,
          },
      .sub_data =
          {
              .protocol =
                  {
                      .on_data = redis_on_data,
                      .on_close = redis_on_close,
                      .on_shutdown = redis_on_shutdown,
                      .ping = redis_sub_ping,
                  },
              .on_message = resp_on_sub_message,
              .uuid = -1,
          },
      .publication_forwarder =
          fio_subscribe(.filter = -1, .udata1 = r,
                        .on_message = redis_on_internal_publish),
      .cmd_forwarder = fio_subscribe(.filter = -2, .udata1 = r,
                                     .on_message = redis_on_internal_cmd),
      .cmd_reply =
          fio_subscribe(.filter = -10 - (uint32_t)getpid(), .udata1 = r,
                        .on_message = redis_on_internal_reply),
      .address = ((char *)(r + 1) + 0),
      .port = ((char *)(r + 1) + args.address.len + 1),
      .auth = ((char *)(r + 1) + args.address.len + args.port.len + 2),
      .auth_len = args.port.len,
      .ref = 1,
      .queue = FIO_LS_INIT(r->queue),
      .lock = FIO_LOCK_INIT,
      .ping_int = args.ping_interval,
      .flag = 1,
  };
  fio_pubsub_attach(&r->en);
  redis_on_facil_start(r);
  fio_state_callback_add(FIO_CALL_IN_CHILD, redis_on_engine_fork, r);
  fio_state_callback_add(FIO_CALL_ON_SHUTDOWN, redis_on_facil_shutdown, r);
  fio_state_callback_add(FIO_CALL_ON_FINISH, redis_on_facil_stop, r);
  /* if restarting */
  fio_state_callback_add(FIO_CALL_PRE_START, redis_on_facil_start, r);

  return &r->en;
}

/* *****************************************************************************
Redis Engine Destruction
***************************************************************************** */

void redis_engine_destroy(pubsub_engine_s *engine) {
  redis_engine_s *r = (redis_engine_s *)engine;
  r->flag = 0;
  fio_pubsub_detach(&r->en);
  fio_state_callback_remove(FIO_CALL_IN_CHILD, redis_on_engine_fork, r);
  fio_state_callback_remove(FIO_CALL_ON_SHUTDOWN, redis_on_facil_shutdown, r);
  fio_state_callback_remove(FIO_CALL_ON_FINISH, redis_on_facil_stop, r);
  fio_state_callback_remove(FIO_CALL_PRE_START, redis_on_facil_start, r);
  redis_free(r);
}
