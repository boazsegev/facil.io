/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "fio_llist.h"
#include "fiobj4sock.h"
#include "redis_engine.h"
#include "resp_parser.h"

#define REDIS_READ_BUFFER 8192
/* *****************************************************************************
The Redis Engine and Callbacks Object
***************************************************************************** */

typedef struct {
  uintptr_t id_protection;
  pubsub_engine_s en;
  struct redis_engine_internal_s {
    protocol_s protocol;
    uintptr_t uuid;
    resp_parser_s parser;
    uintptr_t is_pub;
    FIOBJ str;
    FIOBJ ary;
    uintptr_t ary_count;
    uintptr_t buf_pos;
  } pub_data, sub_data;
  fio_ls_embd_s callbacks;
  spn_lock_i lock;
  char *address;
  char *port;
  char *auth;
  FIOBJ last_ch;
  size_t auth_len;
  size_t ref;
  uint8_t ping_int;
  uint8_t sent;
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

/** converst from a `pubsub_engine_s` to a `redis_engine_s`. */
#define en2redis(e) FIO_LS_EMBD_OBJ(redis_engine_s, en, (e))

/** converst from a `protocol_s` to a `redis_engine_s`. */
#define prot2redis(prot)                                                       \
  ((FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, protocol, (prot))->is_pub) \
       ? FIO_LS_EMBD_OBJ(redis_engine_s, pub_data.protocol, (pr))              \
       : FIO_LS_EMBD_OBJ(redis_engine_s, sub_data.protocol, (pr)))

/** converst from a `resp_parser_s` to the internal data structure. */
#define parser2data(prsr)                                                      \
  FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, (prsr))

#define parser2redis(prsr)                                                     \
  (parser2data(prsr)->is_pub                                                   \
       ? FIO_LS_EMBD_OBJ(redis_engine_s, pub_data.parser, (prsr))              \
       : FIO_LS_EMBD_OBJ(redis_engine_s, sub_data.parser, (prsr)))

/** cleans up and frees the engine data. */
static inline void redis_free(redis_engine_s *r) {
  if (spn_sub(&r->ref, 1))
    return;
  fiobj_free(r->pub_data.ary ? r->pub_data.ary : r->pub_data.str);
  fiobj_free(r->sub_data.ary ? r->sub_data.ary : r->sub_data.str);
  fiobj_free(r->last_ch);
  while (fio_ls_embd_any(&r->callbacks)) {
    free(FIO_LS_EMBD_OBJ(redis_commands_s, node,
                         fio_ls_embd_pop(&r->callbacks)));
  }
  free(r);
}

/**
 * Defined later, converts FIOBJ objects into a RESP string (client mode).
 *
 * Don't call `fiobj_free`, object will self-destruct.
 */
static FIOBJ fiobj2resp_tmp(FIOBJ obj1, FIOBJ obj2);

/* *****************************************************************************
Command / Callback handling
***************************************************************************** */

/* the deferred callback handler */
static void redis_perform_callback(void *e, void *cmd_) {
  redis_commands_s *cmd = cmd_;
  FIOBJ reply = (FIOBJ)cmd->node.next;
  if (cmd->callback)
    cmd->callback(e, reply, cmd->udata);
  fiobj_free(reply);
  // fprintf(stderr, "Handled: %s\n", cmd->cmd);
  free(cmd);
}

/* send the necxt command in the queue */
static void redis_send_cmd_queue(void *r_, void *ignr) {
  redis_engine_s *r = r_;
  if (!r->pub_data.uuid)
    return;
  spn_lock(&r->lock);
  if (r->sent == 0 && fio_ls_embd_any(&r->callbacks)) {
    redis_commands_s *cmd =
        FIO_LS_EMBD_OBJ(redis_commands_s, node, r->callbacks.prev);
    intptr_t uuid = r->pub_data.uuid;
    r->sent = 1;
    spn_unlock(&r->lock);
    // fprintf(stderr, "Sending: %s\n", cmd->cmd);
    sock_write2(.uuid = uuid, .buffer = cmd->cmd, .length = cmd->cmd_len,
                .dealloc = SOCK_DEALLOC_NOOP);
  } else {
    r->sent = 0;
    spn_unlock(&r->lock);
  }
  (void)ignr;
}

static void redis_attach_cmd(redis_engine_s *r, redis_commands_s *cmd) {
  uint8_t schedule = 0;
  spn_lock(&r->lock);
  fio_ls_embd_push(&r->callbacks, &cmd->node);
  if (r->sent == 0) {
    schedule = 1;
  }
  spn_unlock(&r->lock);
  if (schedule) {
    defer(redis_send_cmd_queue, r, NULL);
  }
}

static void redis_cmd_reply(redis_engine_s *r, FIOBJ reply) {
  uint8_t schedule = 0;
  spn_lock(&r->lock);
  r->sent = 0;
  fio_ls_embd_s *node = fio_ls_embd_shift(&r->callbacks);
  schedule = fio_ls_embd_any(&r->callbacks);
  spn_unlock(&r->lock);
  if (!node) {
    /* TODO: possible ping? from server?! not likely... */
    fprintf(stderr,
            "WARNING: (redis) received a reply when no command was sent\n");
    return;
  }
  node->next = (void *)fiobj_dup(reply);
  if (schedule)
    defer(redis_send_cmd_queue, r, NULL);
  defer(redis_perform_callback, &r->en,
        FIO_LS_EMBD_OBJ(redis_commands_s, node, node));
}

/* *****************************************************************************
Connection Establishment
***************************************************************************** */

static void redis_on_auth(pubsub_engine_s *e, FIOBJ reply, void *udata) {
  if (FIOBJ_TYPE_IS(reply, FIOBJ_T_TRUE)) {
    fio_cstr_s s = fiobj_obj2cstr(reply);
    fprintf(stderr,
            "WARNING: (redis) Authentication FAILED.\n"
            "        %.*s\n",
            (int)s.len, s.data);
  }
  (void)e;
  (void)udata;
}

static void redis_on_pub_connect(intptr_t uuid, void *pr) {
  redis_engine_s *r = prot2redis(pr);
  if (r->pub_data.uuid)
    sock_close(r->pub_data.uuid);
  r->pub_data.uuid = uuid;
  facil_attach(uuid, pr);
  facil_set_timeout(uuid, r->ping_int);

  if (!facil_is_running() || !r->flag) {
    sock_close(uuid);
    return;
  }

  if (r->auth) {
    redis_commands_s *cmd = malloc(sizeof(*cmd) + r->auth_len);
    *cmd =
        (redis_commands_s){.cmd_len = r->auth_len, .callback = redis_on_auth};
    memcpy(cmd->cmd, r->auth, r->auth_len);
    spn_lock(&r->lock);
    fio_ls_embd_unshift(&r->callbacks, &cmd->node);
    r->sent = 1;
    spn_unlock(&r->lock);
    sock_write2(.uuid = uuid, .buffer = cmd->cmd, .length = cmd->cmd_len,
                .dealloc = SOCK_DEALLOC_NOOP);
  } else {
    r->sent = 0;
    defer(redis_send_cmd_queue, r, NULL);
  }
  fprintf(stderr, "INFO: (redis %d) publishing connection established.\n",
          (int)getpid());
}
static void redis_on_pub_connect_fail(intptr_t uuid, void *pr);
static void redis_on_sub_connect(intptr_t uuid, void *pr) {
  redis_engine_s *r = prot2redis(pr);
  r->sub_data.uuid = uuid;
  facil_attach(uuid, pr);
  facil_set_timeout(uuid, r->ping_int);

  if (!facil_is_running() || !r->flag) {
    sock_close(uuid);
    return;
  }

  if (r->auth)
    sock_write2(.uuid = uuid, .buffer = r->auth, .length = r->auth_len,
                .dealloc = SOCK_DEALLOC_NOOP);
  pubsub_engine_resubscribe(&r->en);
  if (!r->pub_data.uuid) {
    spn_add(&r->ref, 1);
    redis_on_pub_connect_fail(uuid, pr);
  }
  fprintf(stderr, "INFO: (redis %d) subscription connection established.\n",
          (int)getpid());
}

static void redis_deferred_connect(void *r_, void *is_pub);
static void redis_on_pub_connect_fail(intptr_t uuid, void *pr) {
  redis_engine_s *r = prot2redis(pr);
  if ((facil_parent_pid() == getpid() && !r->sub_data.uuid) || r->flag == 0 ||
      !facil_is_running()) {
    r->pub_data.uuid = 0;
    redis_free(r);
    return;
  }
  r->pub_data.uuid = 0;
  /* we defer publishing by a cycle, so subsciptions race a bit faster */
  defer(redis_deferred_connect, r, (void *)1);
  (void)uuid;
}
static void redis_on_sub_connect_fail(intptr_t uuid, void *pr) {
  redis_engine_s *r = prot2redis(pr);
  if (!facil_is_running() || !r->flag) {
    redis_free(r);
    return;
  }
  if (facil_parent_pid() != getpid()) {
    /* respawned as worker */
    redis_on_pub_connect_fail(uuid, pr);
    return;
  }
  r->sub_data.uuid = 0;
  defer(redis_deferred_connect, r, (void *)0);
  (void)uuid;
}

static void redis_deferred_connect(void *r_, void *is_pub) {
  redis_engine_s *r = r_;
  if (is_pub) {
    facil_connect(.address = r->address, .port = r->port,
                  .on_connect = redis_on_pub_connect,
                  .udata = &r->pub_data.protocol,
                  .on_fail = redis_on_pub_connect_fail);

  } else {
    facil_connect(.address = r->address, .port = r->port,
                  .on_connect = redis_on_sub_connect,
                  .udata = &r->sub_data.protocol,
                  .on_fail = redis_on_sub_connect_fail);
  }
}

/* *****************************************************************************
The Protocol layer (connection handling)
***************************************************************************** */

static void redis_on_data(intptr_t uuid, protocol_s *pr) {
  redis_engine_s *r = prot2redis(pr);
  struct redis_engine_internal_s *internal =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, protocol, pr);
  uint8_t *buf;
  if (internal->is_pub) {
    buf = r->buf + REDIS_READ_BUFFER;
  } else {
    buf = r->buf;
  }
  ssize_t i = sock_read(uuid, buf + internal->buf_pos,
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

static void redis_pub_on_close(intptr_t uuid, protocol_s *pr) {
  redis_engine_s *r = prot2redis(pr);
  fiobj_free(r->pub_data.ary ? r->pub_data.ary : r->pub_data.str);
  r->pub_data.ary = r->pub_data.str = FIOBJ_INVALID;
  r->pub_data.uuid = 0;
  r->sent = 0;
  if (r->flag && facil_is_running()) {
    fprintf(stderr,
            "WARNING: (redis %d) lost publishing connection to database\n",
            (int)getpid());
    redis_on_pub_connect_fail(uuid, &r->pub_data.protocol);
  } else {
    redis_free(r);
  }
}

static void redis_sub_on_close(intptr_t uuid, protocol_s *pr) {
  redis_engine_s *r = prot2redis(pr);
  fiobj_free(r->sub_data.ary ? r->sub_data.ary : r->sub_data.str);
  r->sub_data.ary = r->sub_data.str = FIOBJ_INVALID;
  r->sub_data.uuid = 0;
  if (r->flag && facil_is_running() && facil_parent_pid() == getpid()) {
    fprintf(stderr,
            "WARNING: (redis %d) lost subscribing connection to database\n",
            (int)getpid());
    redis_on_sub_connect_fail(uuid, &r->sub_data.protocol);
  } else {
    redis_free(r);
  }
}

static void redis_on_shutdown(intptr_t uuid, protocol_s *pr) {
  sock_write2(.uuid = uuid, .buffer = "*1\r\n$4\r\nQUIT\r\n", .length = 14,
              .dealloc = SOCK_DEALLOC_NOOP);
  (void)pr;
}

static void redis_sub_ping(intptr_t uuid, protocol_s *pr) {
  sock_write2(.uuid = uuid, .buffer = "*1\r\n$4\r\nPING\r\n", .length = 14,
              .dealloc = SOCK_DEALLOC_NOOP);
  (void)pr;
}

static void redis_pub_ping(intptr_t uuid, protocol_s *pr) {
  redis_engine_s *r = prot2redis(pr);
  if (r->sent) {
    fprintf(stderr,
            "WARNING: (redis) Redis server unresponsive, disconnecting.\n");
    sock_close(uuid);
    return;
  }
  redis_commands_s *cmd = malloc(sizeof(*cmd) + 15);
  *cmd = (redis_commands_s){.cmd_len = 14};
  memcpy(cmd->cmd, "*1\r\n$4\r\nPING\r\n\0", 15);
  redis_attach_cmd(r, cmd);
}

/* *****************************************************************************
Engine Callbacks
***************************************************************************** */

static void redis_on_subscribe(const pubsub_engine_s *eng, FIOBJ channel,
                               uint8_t use_pattern) {
  redis_engine_s *r = en2redis(eng);
  if (r->sub_data.uuid) {
    fio_cstr_s ch_str = fiobj_obj2cstr(channel);
    FIOBJ cmd = fiobj_str_buf(96 + ch_str.len);
    if (use_pattern)
      fiobj_str_write(cmd, "*2\r\n$10\r\nPSUBSCRIBE\r\n$", 22);
    else
      fiobj_str_write(cmd, "*2\r\n$9\r\nSUBSCRIBE\r\n$", 20);
    fiobj_str_join(cmd, fiobj_num_tmp(ch_str.len));
    fiobj_str_write(cmd, "\r\n", 2);
    fiobj_str_write(cmd, ch_str.data, ch_str.len);
    fiobj_str_write(cmd, "\r\n", 2);
    // {
    //   fio_cstr_s s = fiobj_obj2cstr(cmd);
    //   fprintf(stderr, "%s\n", s.data);
    // }
    fiobj_send_free(r->sub_data.uuid, cmd);
  }
}
static void redis_on_unsubscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                 uint8_t use_pattern) {
  redis_engine_s *r = en2redis(eng);
  if (r->sub_data.uuid) {
    fio_cstr_s ch_str = fiobj_obj2cstr(channel);
    FIOBJ cmd = fiobj_str_buf(96 + ch_str.len);
    if (use_pattern)
      fiobj_str_write(cmd, "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$", 24);
    else
      fiobj_str_write(cmd, "*2\r\n$11\r\nUNSUBSCRIBE\r\n$", 23);
    fiobj_str_join(cmd, fiobj_num_tmp(ch_str.len));
    fiobj_str_write(cmd, "\r\n", 2);
    fiobj_str_write(cmd, ch_str.data, ch_str.len);
    fiobj_str_write(cmd, "\r\n", 2);
    // {
    //   fio_cstr_s s = fiobj_obj2cstr(cmd);
    //   fprintf(stderr, "%s\n", s.data);
    // }
    fiobj_send_free(r->sub_data.uuid, cmd);
  }
}
static int redis_on_publish(const pubsub_engine_s *eng, FIOBJ channel,
                            FIOBJ msg) {
  redis_engine_s *r = en2redis(eng);
  if (FIOBJ_TYPE(msg) == FIOBJ_T_ARRAY || FIOBJ_TYPE(msg) == FIOBJ_T_HASH)
    msg = fiobj_obj2json(msg, 0);
  else
    msg = fiobj_dup(msg);

  fio_cstr_s msg_str = fiobj_obj2cstr(msg);
  fio_cstr_s ch_str = fiobj_obj2cstr(channel);

  redis_commands_s *cmd = malloc(sizeof(*cmd) + ch_str.len + msg_str.len + 96);
  *cmd = (redis_commands_s){.cmd_len = 0};
  memcpy(cmd->cmd, "*3\r\n$7\r\nPUBLISH\r\n$", 18);
  char *buf = (char *)cmd->cmd + 18;
  buf += fio_ltoa((void *)buf, ch_str.len, 10);
  *buf++ = '\r';
  *buf++ = '\n';
  memcpy(buf, ch_str.data, ch_str.len);
  buf += ch_str.len;
  *buf++ = '\r';
  *buf++ = '\n';
  *buf++ = '$';
  msg_str = fiobj_obj2cstr(msg);
  buf += fio_ltoa(buf, msg_str.len, 10);
  *buf++ = '\r';
  *buf++ = '\n';
  memcpy(buf, msg_str.data, msg_str.len);
  buf += msg_str.len;
  *buf++ = '\r';
  *buf++ = '\n';
  *buf = 0;
  // fprintf(stderr, "%s\n", cmd->cmd);
  cmd->cmd_len = (uintptr_t)buf - (uintptr_t)(cmd + 1);
  redis_attach_cmd(r, cmd);
  fiobj_free(msg);
  return 0;
}
/* *****************************************************************************
Object Creation
***************************************************************************** */

static void redis_on_startup(const pubsub_engine_s *r_) {
  redis_engine_s *r = en2redis(r_);
  /* start adding one connection, so add one reference. */
  spn_add(&r->ref, 1);
  if (facil_parent_pid() == getpid()) {
    defer((void (*)(void *, void *))redis_on_sub_connect_fail, 0,
          &r->sub_data.protocol);
  } else {
    /* workers don't need to subscribe, tha't only on the root process. */
    defer((void (*)(void *, void *))redis_on_pub_connect_fail, 0,
          &r->pub_data.protocol);
  }
}

#undef redis_engine_create
pubsub_engine_s *redis_engine_create(struct redis_engine_create_args args) {
  if (!args.address)
    return NULL;
  if (!args.port)
    args.port = "6379";
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
      .flag = 1,
      .ping_int = args.ping_interval,
      .callbacks = FIO_LS_INIT(r->callbacks),
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
              .on_startup = redis_on_startup,
          },
      .pub_data =
          {
              .is_pub = 1,
              .protocol =
                  {
                      .service = "redis engine publishing connection",
                      .on_data = redis_on_data,
                      .on_close = redis_pub_on_close,
                      .ping = redis_pub_ping,
                      .on_shutdown = redis_on_shutdown,
                  },
          },
      .sub_data =
          {
              .protocol =
                  {
                      .service = "redis engine subscribing connection",
                      .on_data = redis_on_data,
                      .on_close = redis_sub_on_close,
                      .ping = redis_sub_ping,
                      .on_shutdown = redis_on_shutdown,
                  },
          },
      .ref = 1, /* starts with only the user handle */
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
  } else {
    r->auth = NULL;
  }
  pubsub_engine_register(&r->en);
  if (facil_is_running())
    redis_on_startup(&r->en);
  return &r->en;
}

void redis_engine_destroy(pubsub_engine_s *e) {
  if (e == PUBSUB_CLUSTER_ENGINE || e == PUBSUB_PROCESS_ENGINE) {
    fprintf(stderr, "WARNING: (redis free) trying to distroy one of the "
                    "core engines\n");
    return;
  }
  redis_engine_s *r = en2redis(e);
  if (r->id_protection != 15) {
    fprintf(
        stderr,
        "FATAL ERROR: (redis) engine pointer incorrect, protection failure.\n");
    exit(-1);
  }
  pubsub_engine_deregister(&r->en);
  r->flag = 0;
  if (r->pub_data.uuid)
    sock_close(r->pub_data.uuid);
  if (r->sub_data.uuid)
    sock_close(r->sub_data.uuid);
  redis_free(r);
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
intptr_t redis_engine_send(pubsub_engine_s *engine, FIOBJ command, FIOBJ data,
                           void (*callback)(pubsub_engine_s *e, FIOBJ reply,
                                            void *udata),
                           void *udata) {
  if (engine == PUBSUB_CLUSTER_ENGINE || engine == PUBSUB_PROCESS_ENGINE) {
    fprintf(stderr, "WARNING: (redis send) trying to use one of the "
                    "core engines\n");
    return -1;
  }
  redis_engine_s *r = en2redis(engine);
  if (r->id_protection != 15) {
    fprintf(stderr,
            "ERROR: (redis) engine pointer incorrect, protection failure.\n");
    return -1;
  }
  FIOBJ tmp = fiobj2resp_tmp(command, data);
  fio_cstr_s cmd_str = fiobj_obj2cstr(tmp);
  redis_commands_s *cmd = malloc(sizeof(*cmd) + cmd_str.len + 1);
  *cmd = (redis_commands_s){
      .callback = callback, .udata = udata, .cmd_len = cmd_str.len};
  memcpy(cmd->cmd, cmd_str.data, cmd_str.len + 1);
  redis_attach_cmd(r, cmd);
  return 0;
}

/* *****************************************************************************
Simple RESP formatting
***************************************************************************** */

/**
 * Converts FIOBJ objects into a RESP string (client mode).
 *
 * Don't call `fiobj_free`, object will self-destruct.
 */
static FIOBJ fiobj2resp_tmp(FIOBJ obj1, FIOBJ obj2) {
  FIOBJ dest = fiobj_str_tmp();
  if (!obj2 || FIOBJ_IS_NULL(obj2)) {
    fio_cstr_s s = fiobj_obj2cstr(obj1);
    fiobj_str_write(dest, "*1\r\n$", 5);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);
  } else if (FIOBJ_TYPE(obj2) == FIOBJ_T_ARRAY) {
    size_t count = fiobj_ary_count(obj2);
    fiobj_str_write(dest, "*", 1);
    fiobj_str_join(dest, fiobj_num_tmp(count + 1));
    fiobj_str_write(dest, "\r\n$", 3);

    fio_cstr_s s = fiobj_obj2cstr(obj1);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);

    FIOBJ *ary = fiobj_ary2ptr(obj2);

    for (size_t i = 0; i < count; ++i) {
      fio_cstr_s s = fiobj_obj2cstr(ary[i]);
      fiobj_str_write(dest, "$", 1);
      fiobj_str_join(dest, fiobj_num_tmp(s.len));
      fiobj_str_write(dest, "\r\n", 2);
      fiobj_str_write(dest, s.data, s.len);
      fiobj_str_write(dest, "\r\n", 2);
    }

  } else if (FIOBJ_TYPE(obj2) == FIOBJ_T_HASH) {
    FIOBJ json = fiobj_obj2json(obj2, 0);
    fio_cstr_s s = fiobj_obj2cstr(obj1);
    fiobj_str_write(dest, "*2\r\n$", 5);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n$", 3);
    s = fiobj_obj2cstr(json);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_free(json);

  } else {
    fio_cstr_s s = fiobj_obj2cstr(obj1);
    fiobj_str_write(dest, "*2\r\n$", 5);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n$", 3);
    s = fiobj_obj2cstr(obj2);
    fiobj_str_join(dest, fiobj_num_tmp(s.len));
    fiobj_str_write(dest, "\r\n", 2);
    fiobj_str_write(dest, s.data, s.len);
    fiobj_str_write(dest, "\r\n", 2);
  }
  return dest;
}

/* *****************************************************************************
RESP parser callbacks
***************************************************************************** */

/** a local static callback, called when a parser / protocol error occurs. */
static int resp_on_parser_error(resp_parser_s *parser) {
  struct redis_engine_internal_s *i =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  fprintf(stderr,
          "ERROR: (redis) parser error - attempting to restart connection.\n");
  sock_close(i->uuid);
  return -1;
}

/** a local static callback, called when the RESP message is complete. */
static int resp_on_message(resp_parser_s *parser) {
  struct redis_engine_internal_s *i =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  FIOBJ msg = i->ary ? i->ary : i->str;
  if (i->is_pub) {
    /* publishing / command parser */
    redis_cmd_reply(FIO_LS_EMBD_OBJ(redis_engine_s, pub_data, i), msg);
  } else {
    /* subscriotion parser */
    if (FIOBJ_TYPE(msg) != FIOBJ_T_ARRAY) {
      if (FIOBJ_TYPE(msg) != FIOBJ_T_STRING || fiobj_obj2cstr(msg).len != 4 ||
          fiobj_obj2cstr(msg).data[0] != 'P') {
        fprintf(stderr, "WARNING: (redis) unexpected data format in "
                        "subscription stream:\n");
        fio_cstr_s tmp = fiobj_obj2cstr(msg);
        fprintf(stderr, "     %s\n", tmp.data);
      }
    } else {
      // FIOBJ *ary = fiobj_ary2ptr(msg);
      // for (size_t i = 0; i < fiobj_ary_count(msg); ++i) {
      //   fio_cstr_s tmp = fiobj_obj2cstr(ary[i]);
      //   fprintf(stderr, "(%lu) %s\n", (unsigned long)i, tmp.data);
      // }
      fio_cstr_s tmp = fiobj_obj2cstr(fiobj_ary_index(msg, 0));
      redis_engine_s *r = parser2redis(parser);
      if (tmp.len == 7) { /* "message"  */
        fiobj_free(r->last_ch);
        r->last_ch = fiobj_dup(fiobj_ary_index(msg, 1));
        pubsub_publish(.channel = r->last_ch,
                       .message = fiobj_ary_index(msg, 2),
                       .engine = PUBSUB_CLUSTER_ENGINE);
      } else if (tmp.len == 8) { /* "pmessage" */
        if (!fiobj_iseq(r->last_ch, fiobj_ary_index(msg, 2)))
          pubsub_publish(.channel = fiobj_ary_index(msg, 2),
                         .message = fiobj_ary_index(msg, 3),
                         .engine = PUBSUB_CLUSTER_ENGINE);
      }
    }
  }
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
      fprintf(stderr,
              "ERROR: (redis) array overflow indicates a protocol error.\n");
    fiobj_ary_push(dest->ary, o);
    --dest->ary_count;
  }
  dest->str = o;
}

/** a local static callback, called when a Number object is parsed. */
static int resp_on_number(resp_parser_s *parser, int64_t num) {
  struct redis_engine_internal_s *data =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  resp_add_obj(data, fiobj_num_new(num));
  return 0;
}
/** a local static callback, called when a OK message is received. */
static int resp_on_okay(resp_parser_s *parser) {
  struct redis_engine_internal_s *data =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  resp_add_obj(data, fiobj_true());
  return 0;
}
/** a local static callback, called when NULL is received. */
static int resp_on_null(resp_parser_s *parser) {
  struct redis_engine_internal_s *data =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
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
  struct redis_engine_internal_s *data =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  resp_add_obj(data, fiobj_str_buf(str_len));
  return 0;
}
/** a local static callback, called as String objects are streamed. */
static int resp_on_string_chunk(resp_parser_s *parser, void *data, size_t len) {
  struct redis_engine_internal_s *i =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
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
  struct redis_engine_internal_s *i =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
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
  struct redis_engine_internal_s *i =
      FIO_LS_EMBD_OBJ(struct redis_engine_internal_s, parser, parser);
  if (i->ary) {
    /* this is an error ... */
    fprintf(stderr, "ERROR: (redis) RESP protocol violation "
                    "(array within array).\n");
    return -1;
  }
  i->ary = fiobj_ary_new2(array_len);
  i->ary_count = array_len;
  return 0;
}
