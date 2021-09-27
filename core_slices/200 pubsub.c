/* *****************************************************************************
Pub/Sub
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Letter / Message Object
***************************************************************************** */

/*
Letter network format in bytes:
| 1 special info byte | 3 little endian channel length (24 bit value) |
| 4 little endian Filter | 4 little endian message length (32 bit values) |
| 8 message UUID |
| X bytes channel name + 1 NUL terminator|
| X bytes message length + 1 NUL terminator|
*/

#define LETTER_HEADER_LENGTH 20 /* without NUL terminators */

enum {
  FIO_PUBSUB_PROCESS_BIT = 1,
  FIO_PUBSUB_ROOT_BIT = 2,
  FIO_PUBSUB_SIBLINGS_BIT = 4,
  FIO_PUBSUB_CLUSTER_BIT = 8,
  FIO_PUBSUB_PING_BIT = 64,
  FIO_PUBSUB_JSON_BIT = 128,
} letter_info_bits_e;

typedef struct {
  fio_s *from;
  int32_t filter;
  uint32_t channel_len;
  uint32_t message_len;
  char buf[];
} letter_s;

#define FIO_REF_NAME      letter
#define FIO_REF_FLEX_TYPE char
#include "fio-stl.h"

/* allocates a new letter. */
FIO_IFUNC letter_s *letter_new(fio_s *from,
                               int32_t filter,
                               uint32_t channel_len,
                               uint32_t message_len) {
  size_t len = (LETTER_HEADER_LENGTH + 2) + message_len + channel_len;
  letter_s *l = letter_new2(len);
  FIO_ASSERT_ALLOC(l);
  l->from = from;
  l->filter = filter;
  l->channel_len = channel_len;
  l->message_len = message_len;
  return l;
}

/* allocates a new letter, and writes data to header */
FIO_IFUNC letter_s *letter_author(fio_s *from,
                                  uint64_t message_id,
                                  int32_t filter,
                                  char *channel,
                                  uint32_t channel_len,
                                  char *message,
                                  uint32_t message_len,
                                  uint8_t flags) {
  if ((channel_len >> 24) || (message_len >> 27))
    goto len_error;
  letter_s *l = letter_new(from, filter, channel_len, message_len);
  FIO_ASSERT_ALLOC(l);
  l->buf[0] = flags;
  fio_u2buf32_little(l->buf + 1, channel_len);
  fio_u2buf32_little(l->buf + 4, message_len); /* overwrite extra byte */
  fio_u2buf32_little(l->buf + 8, filter);
  fio_u2buf64_little(l->buf + 12, message_id);
  if (channel_len && channel) {
    memcpy(l->buf + LETTER_HEADER_LENGTH, channel, channel_len);
  }
  l->buf[LETTER_HEADER_LENGTH + channel_len] = 0;
  if (message_len && message) {
    memcpy(l->buf + LETTER_HEADER_LENGTH + 1 + channel_len,
           message,
           message_len);
  }
  l->buf[(LETTER_HEADER_LENGTH + 1) + channel_len + message_len] = 0;
  return l;

len_error:
  FIO_LOG_ERROR("(pubsub) payload too big (channel length of %u bytes, message "
                "length of %u bytes)",
                (unsigned int)channel_len,
                (unsigned int)message_len);
  return NULL;
}

/* frees a letter's reference. */
#define letter_free letter_free2

/* returns 1 if a letter is bound to a filter, otherwise 0. */
FIO_IFUNC int32_t letter_is_filter(letter_s *l) { return !!l->filter; }

/* returns a letter's ID (may be 0 for internal letters) */
FIO_IFUNC uint64_t letter_id(letter_s *l) {
  return fio_buf2u64_little(l->buf + 12);
}

/* returns a letter's channel (if none, returns the filter's address) */
FIO_IFUNC char *letter_channel(letter_s *l) {
  return l->buf + LETTER_HEADER_LENGTH;
}

/* returns a letter's message length (if any) */
FIO_IFUNC size_t letter_message_len(letter_s *l) { return l->message_len; }

/* returns a letter's channel length (if any) */
FIO_IFUNC size_t letter_channel_len(letter_s *l) { return l->channel_len; }

/* returns a letter's filter (if any) */
FIO_IFUNC int32_t letter_filter(letter_s *l) { return l->filter; }

/* returns a letter's message */
FIO_IFUNC char *letter_message(letter_s *l) {
  return l->buf + LETTER_HEADER_LENGTH + 1 + l->channel_len;
}

/* returns a letter's length */
FIO_IFUNC size_t letter_len(letter_s *l) {
  return LETTER_HEADER_LENGTH + 2 + l->channel_len + l->message_len;
}

/* write a letter to an IO object */
FIO_IFUNC void letter_write(fio_s *io, letter_s *l) {
  if (io == l->from)
    return;
  fio_write2(io,
             .buf = (char *)letter_dup2(l),
             .offset = (uintptr_t)(((letter_s *)0)->buf),
             .len = letter_len(l),
             .dealloc = (void (*)(void *))letter_free2);
}

/* *****************************************************************************
Letter Reading, Parsing and Sending
***************************************************************************** */

/* a letter parser object */
typedef struct {
  letter_s *letter;
  size_t pos;
  char buf[LETTER_HEADER_LENGTH + 2]; /* minimal message length */
} letter_parser_s;

/* a new letter parser */
FIO_IFUNC letter_parser_s *letter_parser_new(void) {
  letter_parser_s *p = fio_malloc(sizeof(*p));
  FIO_ASSERT_ALLOC(p);
  p->letter = NULL;
  p->pos = 0;
  return p;
}

/* free a letter parser */
FIO_IFUNC void letter_parser_free(letter_parser_s *parser) {
  letter_free(parser->letter);
  fio_free(parser);
}

/* pings are ignored (no pong required) */
FIO_SFUNC void letter_read_ping_callback(letter_s *letter) { (void)letter; }

/* forwards letters to callback, returns 0. Returns -1 on error. */
FIO_IFUNC int letter_read(fio_s *io,
                          letter_parser_s *parser,
                          void (*callback)(letter_s *)) {
  void (*callbacks[2])(letter_s *) = {callback, letter_read_ping_callback};
  for (;;) {
    ssize_t r;
    if (parser->letter) {
      letter_s *const letter = parser->letter;
      const size_t to_read = letter_len(parser->letter);
      while (parser->pos < to_read) {
        r = fio_read(io, letter->buf + parser->pos, to_read - parser->pos);
        if (r <= 0)
          return 0;
        parser->pos += r;
      }
      callbacks[((letter->buf[0] & FIO_PUBSUB_PING_BIT) / FIO_PUBSUB_PING_BIT)](
          letter);
      letter_free(letter);
      parser->letter = NULL;
      parser->pos = 0;
    }
    r = fio_read(io,
                 parser->buf + parser->pos,
                 (LETTER_HEADER_LENGTH + 2) - parser->pos);
    if (r <= 0)
      return 0;
    parser->pos += r;
    if (parser->pos < LETTER_HEADER_LENGTH)
      return 0;
    uint32_t channel_len = fio_buf2u32_little(parser->buf + 1);
    channel_len &= 0xFFFFFF;
    uint32_t message_len = fio_buf2u32_little(parser->buf + 4);
    int32_t filter = fio_buf2u32_little(parser->buf + 8);
    parser->letter = letter_new(io, filter, channel_len, message_len);
    if (!parser->letter) {
      return -1;
    }
    memcpy(parser->letter->buf, parser->buf, parser->pos);
  }
}

/* *****************************************************************************
Pub/Sub - defaults and builtin pub/sub engines
***************************************************************************** */

/** Used to publish the message exclusively to the root / master process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_ROOT =
    (fio_pubsub_engine_s *)FIO_PUBSUB_ROOT_BIT;
/** Used to publish the message only within the current process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_PROCESS =
    (fio_pubsub_engine_s *)FIO_PUBSUB_PROCESS_BIT;
/** Used to publish the message except within the current process. */
const fio_pubsub_engine_s *const FIO_PUBSUB_SIBLINGS =
    (fio_pubsub_engine_s *)FIO_PUBSUB_SIBLINGS_BIT;
/** Used to publish the message for this process, its siblings and root. */
const fio_pubsub_engine_s *const FIO_PUBSUB_LOCAL =
    (fio_pubsub_engine_s *)(FIO_PUBSUB_SIBLINGS_BIT | FIO_PUBSUB_PROCESS_BIT |
                            FIO_PUBSUB_ROOT_BIT);
/** Used to publish the message to any possible publishers. */
const fio_pubsub_engine_s *const FIO_PUBSUB_CLUSTER =
    (fio_pubsub_engine_s *)(FIO_PUBSUB_CLUSTER_BIT | FIO_PUBSUB_SIBLINGS_BIT |
                            FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT);

/** The default engine (settable). Initial default is FIO_PUBSUB_LOCAL. */
const fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;

/**
 * The pattern matching callback used for pattern matching.
 *
 * Returns 1 on a match or 0 if the string does not match the pattern.
 *
 * By default, the value is set to `fio_glob_match` (see facil.io's C STL).
 */
uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s,
                                    fio_str_info_s) = fio_glob_match;

/* *****************************************************************************
Channel / Subscription Objects
***************************************************************************** */

/* channels can be either named (filters + exact strings) or pattern based. */
typedef enum {
  CHANNEL_TYPE_NAMED,
  CHANNEL_TYPE_PATTERN,
  CHANNEL_TYPE_NONE,
} channel_type_e;

/* The channel object */
typedef struct {
  FIO_LIST_HEAD subscriptions;
  channel_type_e type;
  int32_t filter;
  size_t name_len;
  char *name;
} channel_s;

/* The subscription object */
typedef struct {
  FIO_LIST_NODE node;
  fio_s *io;
  channel_s *channel;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata);
  void *udata;
  uint32_t ref;
  fio_lock_i lock;
  uint8_t disabled; /* TODO: do we need this one? */
} subscription_s;

/* *****************************************************************************
Subscription Object API
***************************************************************************** */

/* creates a new subscription */
FIO_IFUNC subscription_s *subscription_new(fio_s *io,
                                           channel_s *channel,
                                           void (*on_message)(fio_msg_s *),
                                           void (*on_unsubscribe)(void *),
                                           void *udata) {
  subscription_s *s = (io ? fio_malloc : malloc)(sizeof(*s));
  FIO_ASSERT_ALLOC(s);
  *s = (subscription_s){
      .node = FIO_LIST_INIT(s->node),
      .io = io,
      .channel = channel,
      .on_message = on_message,
      .on_unsubscribe = on_unsubscribe,
      .udata = udata,
      .ref = 1,
  };
  return s;
}

/* we count subscription reference counts to make sure the udata is valid */
FIO_IFUNC subscription_s *subscription_dup(subscription_s *s) {
  FIO_ASSERT(fio_atomic_add_fetch(&s->ref, 1),
             "subscription reference count overflow detected!");
  return s;
}

/* calls the on_unsubscribe callback. */
FIO_SFUNC void subscription_on_unsubscribe___task(void *fnp, void *udata) {
  union {
    void *p;
    void (*fn)(void *udata);
  } u = {.p = fnp};
  u.fn(udata);
}

/* free the udata (and subscription) only after all callbacks return */
FIO_IFUNC void subscription_free(subscription_s *s) {
  if (fio_atomic_sub_fetch(&s->ref, 1))
    return;
  union {
    void *p;
    void (*fn)(void *udata);
  } u = {.fn = s->on_unsubscribe};
  if (u.p) {
    fio_queue_push(FIO_QUEUE_USER,
                   subscription_on_unsubscribe___task,
                   u.p,
                   s->udata);
  }
  (s->io ? fio_free : free)(s);
}

/* *****************************************************************************
Message Delivery (to subscription)
***************************************************************************** */

/* an internal (temporary) message object. */
typedef struct {
  volatile size_t flag;
  letter_s *letter;
  fio_msg_s msg;
} fio_msg_internal_s;

/* returns the letter object associated with the message. */
FIO_IFUNC letter_s *fio_msg2letter(fio_msg_s *msg) {
  fio_msg_internal_s *mi = FIO_PTR_FROM_FIELD(fio_msg_internal_s, msg, msg);
  return mi->letter;
}

/** Defers the current callback, so it will be called again for the message. */
void fio_message_defer(fio_msg_s *msg) {
  fio_msg_internal_s *mi = FIO_PTR_FROM_FIELD(fio_msg_internal_s, msg, msg);
  mi->flag = 1;
}

/** calls the callback within a lock, using a fio_msg_s pointer. */
FIO_SFUNC void subscription_deliver__task(void *s_, void *l_) {
  subscription_s *s = (subscription_s *)s_;
  letter_s *l = (letter_s *)l_;
  fio_msg_internal_s mi = {
      .flag = 0,
      .letter = l,
      .msg =
          {
              .io = s->io,
              .channel =
                  {
                      .buf = letter_channel(l),
                      .len = letter_channel_len(l),
                  },
              .message =
                  {
                      .buf = letter_message(l),
                      .len = letter_message_len(l),
                  },
              .udata = s->udata,
              .filter = letter_filter(l),
              .is_json =
                  ((l->buf[0] & FIO_PUBSUB_JSON_BIT) / FIO_PUBSUB_JSON_BIT),
          },
  };
  fio_lock_i *lock = (s->io ? &s->io->lock : &s->lock);
  if (fio_trylock(lock))
    goto reschedule;
  s->on_message(&mi.msg);
  fio_unlock(lock);
  if (mi.flag)
    goto reschedule;

  fio_undup(s->io);
  subscription_free(s);
  letter_free(l);
  return;
reschedule:
  fio_queue_push((s->io ? FIO_QUEUE_IO(s->io) : FIO_QUEUE_USER),
                 subscription_deliver__task,
                 s,
                 l);
}

/* schedules a letter delivery to a subscription */
FIO_IFUNC void subscription_deliver(subscription_s *s, letter_s *l) {
  if (s->disabled)
    return;
  fio_queue_s *q = FIO_QUEUE_USER;
  if (s->io) {
    if (s->io == l->from || !fio_is_valid(s->io))
      return;
    q = FIO_QUEUE_IO(s->io);
    fio_dup(s->io);
  }
  fio_queue_push(q,
                 subscription_deliver__task,
                 subscription_dup(s),
                 letter_dup2(l));
}

/* *****************************************************************************
Channel Object API
***************************************************************************** */

/* creates a new channel object. */
FIO_IFUNC channel_s *channel_new(channel_type_e channel_type,
                                 int32_t filter,
                                 char *name,
                                 size_t name_len) {
  channel_s *c = malloc(sizeof(*c) + name_len);
  FIO_ASSERT_ALLOC(c);
  *c = (channel_s){
      .subscriptions = FIO_LIST_INIT(c->subscriptions),
      .type = channel_type,
      .filter = filter,
      .name_len = name_len,
      .name = (char *)(c + 1),
  };
  if (name_len)
    memcpy(c->name, name, name_len);
  return c;
}

/* frees a channel, making sure all subscriptions are destroyed. */
FIO_IFUNC void channel_free(channel_s *channel) {
  if (!channel)
    return;
  /* make sure no subscriptions are leaked during termination */
  FIO_LIST_EACH(subscription_s, node, &channel->subscriptions, s) {
    s->channel = NULL;
    subscription_free(s);
  }
  free(channel);
}

/* tests to see if two channel objects are the same. */
FIO_IFUNC _Bool channel_is_eq(channel_s *a, channel_s *b) {
  return a->filter == b->filter && a->name_len == b->name_len &&
         !memcmp(a->name, b->name, a->name_len);
}

/* hashing helper (hashes a channel name using it's filter as "salt". */
FIO_IFUNC uint64_t channel2hash(channel_s ch) {
  return fio_risky_hash(ch.name, ch.name_len, ch.filter);
}

/* *****************************************************************************
Postoffice
***************************************************************************** */

FIO_SFUNC void postoffice_on_channel_added(channel_s *);
FIO_SFUNC void postoffice_on_channel_removed(channel_s *);

#define FIO_MAP_NAME           channel_store
#define FIO_MAP_TYPE           channel_s *
#define FIO_MAP_TYPE_CMP(a, b) channel_is_eq(a, b)
#define FIO_MAP_TYPE_COPY(dest, src)                                           \
  do {                                                                         \
    ((dest) = (src));                                                          \
    postoffice_on_channel_added((src));                                        \
  } while (0)
#define FIO_MAP_TYPE_DESTROY(ch)                                               \
  do {                                                                         \
    postoffice_on_channel_removed((ch));                                       \
    channel_free((ch));                                                        \
  } while (0)
#define FIO_MAP_TYPE_DISCARD(ch)                                               \
  do {                                                                         \
    channel_free((ch));                                                        \
  } while (0)
#include "fio-stl.h"

/** The postoffice data store */
static struct {
  uint8_t filter_local;
  uint8_t filter_ipc;
  uint8_t filter_cluster;
  channel_store_s channels[CHANNEL_TYPE_NONE];
  char ipc_url[40];                 /* inter-process address  - buffer */
  char cluster_url[40];             /* machine cluster address - buffer */
  fio_protocol_s ipc;               /* inter-process communication protocol */
  fio_protocol_s ipc_listen;        /* accepts inter-process connections */
  fio_protocol_s cluster;           /* machine cluster communication protocol */
  fio_protocol_s cluster_listen;    /* accepts machine cluster connections */
  fio_protocol_s cluster_discovery; /* network cluster discovery */
  fio_tls_s *cluster_server_tls;
  fio_tls_s *cluster_client_tls;
  uintptr_t subscription_handles[8];
  fio_s mock_io;
} postoffice = {
    .filter_local = (FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT |
                     FIO_PUBSUB_SIBLINGS_BIT),
    .filter_ipc = (~(uint8_t)FIO_PUBSUB_PROCESS_BIT),
    .filter_cluster = FIO_PUBSUB_CLUSTER_BIT,
    .channels =
        {
            FIO_MAP_INIT,
            FIO_MAP_INIT,
        },
};

/* subscribe using a subscription object and a channel */
FIO_IFUNC void postoffice_subscribe(subscription_s *s) {
  if (!s)
    return;
  if (s->disabled || !s->channel || (s->io && !fio_is_valid(s->io)))
    goto error;
  const uint64_t hash = channel2hash(*s->channel);
  s->channel =
      channel_store_set_if_missing(&postoffice.channels[s->channel->type],
                                   hash,
                                   s->channel);
  FIO_LIST_PUSH(&s->channel->subscriptions, &s->node);
  return;

error:
  channel_free(s->channel);
  subscription_free(s);
}

/* unsubscribe using a subscription object */
FIO_IFUNC void postoffice_unsubscribe(subscription_s *s) {
  if (!s)
    return;
  channel_s *ch = s->channel;
  s->disabled = 1;
  FIO_LIST_REMOVE(&s->node);
  subscription_free(s);
  if (!ch || !FIO_LIST_IS_EMPTY(&ch->subscriptions))
    return;
  const uint64_t hash = channel2hash(ch[0]);
  channel_store_remove(&postoffice.channels[ch->type], hash, ch, NULL);
}

/* deliver a letter to all subscriptions in the relevant channels */
FIO_IFUNC void postoffice_deliver2process(letter_s *l) {
  channel_s ch_key = {
      .filter = letter_filter(l),
      .name_len = letter_channel_len(l),
      .name = letter_channel(l),
  };
  const uint64_t hash = channel2hash(ch_key);
  channel_s *ch = channel_store_get(postoffice.channels, hash, &ch_key);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, s) {
      subscription_deliver(s, l);
    }
  }
  if (!channel_store_count(&postoffice.channels[CHANNEL_TYPE_PATTERN]))
    return;

  fio_str_info_s name = {
      .buf = letter_channel(l),
      .len = letter_channel_len(l),
  };
  FIO_MAP_EACH(channel_store, &postoffice.channels[CHANNEL_TYPE_PATTERN], pos) {
    if (pos->obj->filter != letter_filter(l))
      continue;
    fio_str_info_s pat = {
        .buf = pos->obj->name,
        .len = pos->obj->name_len,
    };
    if (FIO_PUBSUB_PATTERN_MATCH(pat, name)) {
      FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, s) {
        subscription_deliver(s, l);
      }
    }
  }
}

FIO_SFUNC void postoffice_deliver2io___task(void *io_, void *l_) {
  letter_write(io_, l_);
  letter_free(l_);
  fio_free2(io_);
}

FIO_IFUNC void postoffice_deliver2ipc(letter_s *l) {
  FIO_LIST_EACH(fio_s, timeouts, &postoffice.ipc.reserved.ios, io) {
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   postoffice_deliver2io___task,
                   fio_dup2(io),
                   letter_dup2(l));
  }
}

FIO_IFUNC void postoffice_deliver2cluster(letter_s *l) {
  FIO_LIST_EACH(fio_s, timeouts, &postoffice.cluster.reserved.ios, io) {
    fio_queue_push(FIO_QUEUE_SYSTEM,
                   postoffice_deliver2io___task,
                   fio_dup2(io),
                   letter_dup2(l));
  }
}

/* *****************************************************************************
Pub/Sub - Subscribe / Unsubscribe
***************************************************************************** */

/* perform subscription in system thread */
FIO_SFUNC void fio_subscribe___task(void *ch_, void *s_) {
  channel_s *ch = ch_;
  subscription_s *s = s_;
  s->channel = ch;
  postoffice_subscribe(s);
}

void fio_subscribe___(void); /* sublimetext marker */
/**
 * Subscribes to either a filter OR a channel (never both).
 *
 * The on_unsubscribe callback will be called on failure.
 */
void fio_subscribe FIO_NOOP(subscribe_args_s args) {
  channel_s *ch = channel_new(
      fio_ct_if(args.is_pattern, CHANNEL_TYPE_PATTERN, CHANNEL_TYPE_NAMED),
      args.filter,
      args.channel.buf,
      args.channel.len);
  subscription_s *s = subscription_new(args.io,
                                       NULL,
                                       args.on_message,
                                       args.on_unsubscribe,
                                       args.udata);
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_subscribe___task, ch, s);
  if (!args.subscription_handle_ptr) {
    fio_env_set(args.io,
                .type = (-1LL - args.is_pattern - (!!args.filter)),
                .name = args.channel,
                .on_close = ((void (*)(void *))postoffice_unsubscribe),
                .udata = s);
  } else {
    *args.subscription_handle_ptr = (uintptr_t)s;
  }
}

void fio_unsubscribe___(void); /* sublimetext marker */

FIO_SFUNC void fio_unsubscribe___task(void *s, void *ignr_) {
  (void)ignr_;
  postoffice_unsubscribe(s);
}
/**
 * Cancels an existing subscriptions.
 *
 * Accepts the same arguments as `fio_subscribe`, except the `udata` and
 * callback details are ignored (no need to provide `udata` or callback
 * details).
 *
 * Returns -1 if the subscription could not be found. Otherwise returns 0.
 */
int fio_unsubscribe FIO_NOOP(subscribe_args_s args) {
  if (args.filter) {
    args.is_pattern = 0;
    args.channel.buf = (char *)&args.filter;
    args.channel.len = sizeof(args.filter);
  }
  if (!args.subscription_handle_ptr) {
    return fio_env_remove(args.io,
                          .type = (-1LL - args.is_pattern - (!!args.filter)),
                          .name = args.channel);
  }
  ((subscription_s **)args.subscription_handle_ptr)[0]->disabled = 1;
  fio_queue_push(FIO_QUEUE_SYSTEM,
                 fio_unsubscribe___task,
                 ((void **)args.subscription_handle_ptr)[0]);
  return 0;
}

/* *****************************************************************************
Pub/Sub - Publish
***************************************************************************** */
FIO_SFUNC void fio_publish___task(void *letter_, void *ignr_) {
  letter_s *l = letter_;
  if ((l->buf[0] & postoffice.filter_local))
    postoffice_deliver2process(l);
  if ((l->buf[0] & postoffice.filter_ipc))
    postoffice_deliver2ipc(l);
  if ((l->buf[0] & postoffice.filter_cluster))
    postoffice_deliver2cluster(l);
  letter_free(l);
  (void)ignr_;
}

/**
 * Publishes a message to the relevant subscribers (if any).
 *
 * See `fio_publish_args_s` for details.
 *
 * By default the message is sent using the FIO_PUBSUB_CLUSTER engine (all
 * processes, including the calling process).
 *
 * To limit the message only to other processes (exclude the calling process),
 * use the FIO_PUBSUB_SIBLINGS engine.
 *
 * To limit the message only to the calling process, use the
 * FIO_PUBSUB_PROCESS engine.
 *
 * To publish messages to the pub/sub layer, the `.filter` argument MUST be
 * equal to 0 or missing.
 */
void fio_publish___(void); /* SublimeText marker*/
void fio_publish FIO_NOOP(fio_publish_args_s args) {
  const fio_pubsub_engine_s *engines[3] = {
      FIO_PUBSUB_DEFAULT,
      FIO_PUBSUB_LOCAL,
  };
  if (!args.engine)
    args.engine = engines[(!!args.filter)];
  if ((uintptr_t)(args.engine) > 0XFF)
    goto external_engine;
  letter_s *l =
      letter_author(args.from,
                    fio_rand64(),
                    args.filter,
                    args.channel.buf,
                    args.channel.len,
                    args.message.buf,
                    args.message.len,
                    (uint8_t)(uintptr_t)args.engine |
                        ((0x100 - args.is_json) & FIO_PUBSUB_JSON_BIT));
  fio_queue_push(FIO_QUEUE_SYSTEM, fio_publish___task, l);
  return;
external_engine:
  args.engine->publish(args.engine, args.channel, args.message, args.is_json);
}

/* *****************************************************************************
 * Message metadata (advance usage API)
 **************************************************************************** */

/**
 * The number of different metadata callbacks that can be attached.
 *
 * Effects performance.
 *
 * The default value should be enough for the following metadata objects:
 * - WebSocket server headers.
 * - WebSocket client (header + masked message copy).
 * - EventSource (SSE) encoded named channel and message.
 */
#ifndef FIO_PUBSUB_METADATA_LIMIT
#define FIO_PUBSUB_METADATA_LIMIT 4
#endif

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id);

/**
 * It's possible to attach metadata to facil.io named messages (filter == 0)
 * before they are published.
 *
 * This allows, for example, messages to be encoded as network packets for
 * outgoing protocols (i.e., encoding for WebSocket transmissions), improving
 * performance in large network based broadcasting.
 *
 * Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.
 *
 * The callback should return a `void *` pointer.
 *
 * To remove a callback, call `fio_message_metadata_remove` with the returned
 * value.
 *
 * The cluster messaging system allows some messages to be flagged as JSON and
 * this flag is available to the metadata callback.
 *
 * Returns a positive number on success (the metadata ID) or zero (0) on
 * failure.
 */
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *));

/**
 * Removed the metadata callback.
 *
 * Removal might be delayed if live metatdata exists.
 */
void fio_message_metadata_remove(int id);

/* *****************************************************************************
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 **************************************************************************** */

/**
 * Attaches an engine, so it's callback can be called by facil.io.
 *
 * The `subscribe` callback will be called for every existing channel.
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_attach(fio_pubsub_engine_s *engine);

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine);

/**
 * Engines can ask facil.io to call the `subscribe` callback for all active
 * channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 *
 * NOTE: the root (master) process will call `subscribe` for any channel in any
 * process, while all the other processes will call `subscribe` only for their
 * own channels. This allows engines to use the root (master) process as an
 * exclusive subscription process.
 */
void fio_pubsub_reattach(fio_pubsub_engine_s *eng);

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);

/* *****************************************************************************
Pubsub IPC and cluster protocol callbacks
***************************************************************************** */

FIO_SFUNC void pubsub_cluster_on_letter(letter_s *l) {
  /* TODO: test for and discard duplicates */
  postoffice_deliver2process(l);
  postoffice_deliver2ipc(l);
  postoffice_deliver2cluster(l);
}

FIO_SFUNC void pubsub_ipc_on_letter_master(letter_s *l) {
  if ((l->buf[0] & FIO_PUBSUB_ROOT_BIT))
    postoffice_deliver2process(l);
  if ((l->buf[0] & FIO_PUBSUB_SIBLINGS_BIT))
    postoffice_deliver2ipc(l);
  if ((l->buf[0] & FIO_PUBSUB_CLUSTER_BIT))
    postoffice_deliver2cluster(l);
}

FIO_SFUNC void pubsub_ipc_on_letter_worker(letter_s *l) {
  postoffice_deliver2process(l);
}

FIO_SFUNC void pubsub_ipc_on_data_master(fio_s *io) {
  letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_master);
}

FIO_SFUNC void pubsub_ipc_on_data_worker(fio_s *io) {
  letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_worker);
}

FIO_SFUNC void pubsub_cluster_on_data(fio_s *io) {
  if (letter_read(io, fio_udata_get(io), pubsub_ipc_on_letter_master))
    fio_close(io);
}

FIO_SFUNC void pubsub_ipc_on_close(void *udata) { letter_parser_free(udata); }

FIO_SFUNC void pubsub_ipc_on_close_in_child(void *udata) {
  letter_parser_free(udata);
  if (fio_data.running) {
    FIO_LOG_ERROR(
        "(%d) pub/sub connection to master process lost while running.",
        (int)fio_data.pid);
    fio_stop();
  }
}

FIO_SFUNC void pubsub_connection_ping(fio_s *io) {
  letter_s *l =
      letter_author(NULL, 0, -1, NULL, 0, NULL, 0, FIO_PUBSUB_PING_BIT);
  letter_write(io, l);
  letter_free(l);
}

FIO_SFUNC void pubsub_cluster_on_close(void *udata) {
  /* TODO: allow re-connections from this address */
  letter_parser_free(udata);
}

FIO_SFUNC void pubsub_listener_ipc_on_close(void *udata) {
  (void)udata;
  if (fio_data.is_master)
    FIO_LOG_DEBUG2("(%d) PostOffice stopped listening for IPC @ %s",
                   fio_data.pid,
                   postoffice.ipc_url);
}

FIO_SFUNC void pubsub_listener_cluster_on_close(void *udata) {
  (void)udata;
  if (fio_data.is_master)
    FIO_LOG_DEBUG2("(%d) PostOffice stopped listening for remote machines @ %s",
                   fio_data.pid,
                   postoffice.cluster_url);
}

FIO_SFUNC void pubsub_ipc_on_new_connection(fio_s *io) {
  if (!fio_data.is_master)
    return;
  int fd = accept(io->fd, NULL, NULL);
  if (fd == -1)
    return;
  io = fio_attach_fd(fd, fio_udata_get(io), letter_parser_new(), NULL);
  FIO_LOG_DDEBUG2("(%d) PostOffice accepted connection @ %p (%d).",
                  (int)fio_data.pid,
                  io,
                  io->fd);
}

/* *****************************************************************************
Postoffice add / remove hooks
***************************************************************************** */

FIO_SFUNC void postoffice_on_channel_added(channel_s *ch) {
  FIO_LOG_DDEBUG2("(%d) PostOffice opened channel (filter %ld): %.*s",
                  fio_data.pid,
                  (long)ch->filter,
                  (int)ch->name_len,
                  ch->name);
  if (ch->filter)
    return;
  fio_str_info_s cmds[] = {
      {"subscribe", 9},
      {"psubscribe", 10},
  };
  fio_publish(.engine = FIO_PUBSUB_ROOT,
              .filter = -1,
              .channel = cmds[ch->type == CHANNEL_TYPE_PATTERN],
              .message = {ch->name, ch->name_len});
  FIO_LOG_DDEBUG2("(%d) PostOffice channel added: %.*s",
                  fio_data.pid,
                  (int)ch->name_len,
                  ch->name);
}
FIO_SFUNC void postoffice_on_channel_removed(channel_s *ch) {
  FIO_LOG_DDEBUG2("(%d) PostOffice closed channel (filter %ld): %.*s",
                  fio_data.pid,
                  (long)ch->filter,
                  (int)ch->name_len,
                  ch->name);
  if (ch->filter)
    return;
  fio_str_info_s cmds[] = {
      {"unsubscribe", 11},
      {"punsubscribe", 12},
  };
  fio_publish(.engine = FIO_PUBSUB_ROOT,
              .filter = -1,
              .channel = cmds[ch->type == CHANNEL_TYPE_PATTERN],
              .message = {ch->name, ch->name_len});
  FIO_LOG_DDEBUG2("(%d) PostOffice channel removed: %.*s",
                  fio_data.pid,
                  (int)ch->name_len,
                  ch->name);
}

FIO_SFUNC void postoffice_mock_callback(fio_msg_s *msg) { (void)msg; }

FIO_SFUNC void postoffice_on_global_subscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_subscribe(.io = fio_msg2letter(msg)->from,
                  .channel = msg->message,
                  .is_pattern = 0,
                  .on_message = postoffice_mock_callback);
    return;
  }
  /* global channel created. */
  /* TODO: call engine callbacks */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
}

FIO_SFUNC void postoffice_on_global_psubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_subscribe(.io = fio_msg2letter(msg)->from,
                  .channel = msg->message,
                  .is_pattern = 1,
                  .on_message = postoffice_mock_callback);
    return;
  }
  /* global channel created. */
  /* TODO: call engine callbacks */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
}
FIO_SFUNC void postoffice_on_global_unsubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_unsubscribe(.io = fio_msg2letter(msg)->from,
                    .channel = msg->message,
                    .is_pattern = 0,
                    .on_message = postoffice_mock_callback);
    return;
  }
  /* channel removed. */
  /* TODO: call engine callbacks */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
}
FIO_SFUNC void postoffice_on_global_pubsubscribe(fio_msg_s *msg) {
  /* child process subscribe ? */
  if (fio_msg2letter(msg)->from) {
    fio_unsubscribe(.io = fio_msg2letter(msg)->from,
                    .channel = msg->message,
                    .is_pattern = 1,
                    .on_message = postoffice_mock_callback);
    return;
  }
  /* global channel removed. */
  /* TODO: call engine callbacks */
  FIO_LOG_DDEBUG2("(%d) PostOffice global channel %s: %s",
                  (int)fio_data.pid,
                  msg->channel.buf,
                  msg->message.buf);
}

/* *****************************************************************************
Pubsub Init
***************************************************************************** */

FIO_SFUNC void postoffice_pre__start(void *ignr_) {
  (void)ignr_;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT;
  postoffice.filter_ipc = FIO_PUBSUB_SIBLINGS_BIT;
  postoffice.filter_cluster = FIO_PUBSUB_CLUSTER_BIT;
  postoffice.ipc.on_data = pubsub_ipc_on_data_master;
  postoffice.ipc.on_close = pubsub_ipc_on_close;
  postoffice.ipc.on_timeout = pubsub_connection_ping;
  postoffice.ipc.timeout = 300;
  postoffice.ipc_listen.on_data = pubsub_ipc_on_new_connection;
  postoffice.ipc_listen.on_close = pubsub_listener_ipc_on_close;
  postoffice.ipc_listen.on_timeout = FIO_PING_ETERNAL;
  postoffice.cluster.on_data = pubsub_cluster_on_data;
  postoffice.cluster.on_close = pubsub_ipc_on_close;
  postoffice.cluster.on_timeout = pubsub_connection_ping;
  postoffice.cluster.timeout = 300;
  postoffice.cluster_listen.on_data = pubsub_ipc_on_new_connection;
  postoffice.cluster_listen.on_close = pubsub_listener_cluster_on_close;
  postoffice.cluster_listen.on_timeout = FIO_PING_ETERNAL;
  postoffice.mock_io.protocol = &postoffice.ipc;
  postoffice.mock_io.env = ENV_SAFE_INIT;
  postoffice.mock_io.fd = -1;
  postoffice.mock_io.state = FIO_IO_OPEN;
  postoffice.mock_io.lock = FIO_LOCK_INIT;
  if (fio_data.workers) {
    int fd =
        fio_sock_open2(postoffice.ipc_url, FIO_SOCK_SERVER | FIO_SOCK_NONBLOCK);
    FIO_ASSERT(fd != -1,
               "failed to create IPC listening socket.",
               (int)fio_data.pid);
    fio_attach_fd(fd, &postoffice.ipc_listen, &postoffice.ipc, NULL);
    FIO_LOG_DEBUG2("listening for pub/sub IPC @ %s", postoffice.ipc_url);
  }
}

FIO_SFUNC void postoffice_forked_child(void *ignr_) {
  (void)ignr_;
  for (fio_protocol_s *pr = &postoffice.ipc;
       pr <= &postoffice.cluster_discovery;
       ++pr) {
    FIO_LIST_EACH(fio_s, timeouts, &pr->reserved.ios, io) {
      fio_sock_close(io->fd);
      io->fd = -1;
      fio_close_now_unsafe(io);
    }
  }
  for (int i = 0; postoffice.subscription_handles[i]; ++i) {
    fio_unsubscribe(.subscription_handle_ptr =
                        postoffice.subscription_handles + i);
    postoffice.subscription_handles[i] = 0;
  }
  // perform all callbacks before replacing them
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;

  postoffice.ipc.on_data = pubsub_ipc_on_data_worker;
  postoffice.ipc.on_close = pubsub_ipc_on_close_in_child;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT;
  postoffice.filter_ipc = ~(uint8_t)FIO_PUBSUB_PROCESS_BIT;
  postoffice.ipc.timeout = -1;
  postoffice.filter_cluster = 0;
  int fd =
      fio_sock_open2(postoffice.ipc_url, FIO_SOCK_CLIENT | FIO_SOCK_NONBLOCK);
  FIO_ASSERT(fd != -1,
             "(%d) failed to connect to master process.",
             (int)fio_data.pid);
  fio_s *io = fio_attach_fd(fd, &postoffice.ipc, letter_parser_new(), NULL);
  (void)io;
  FIO_LOG_DDEBUG2("(%d) connecting IPC to PostOffice %p (fd %d)",
                  (int)fio_data.pid,
                  (void *)io,
                  fd);
}

FIO_SFUNC void postoffice_on_finish(void *ignr_) {
  (void)ignr_;
  postoffice.filter_local = FIO_PUBSUB_PROCESS_BIT | FIO_PUBSUB_ROOT_BIT;
}

FIO_SFUNC void postoffice_at_exit(void *ignr_) {
  (void)ignr_;
  env_safe_destroy(&postoffice.mock_io.env);
  for (int i = 0; i < CHANNEL_TYPE_NONE; ++i) {
    channel_store_destroy(postoffice.channels + i);
  }
  if (!fio_data.is_master)
    return;

  if (!memcmp(postoffice.ipc_url, "unix:", 5) |
      !memcmp(postoffice.ipc_url, "file:", 5)) {
    fio_url_s url =
        fio_url_parse(postoffice.ipc_url, strlen(postoffice.ipc_url));
    FIO_LOG_DDEBUG2("(%d) PostOffice unlinking %s", fio_data.pid, url.path.buf);
    unlink(url.path.buf);
  }
}

FIO_SFUNC void postoffice_initialize(void) {
  for (fio_protocol_s *pr = &postoffice.ipc;
       pr <= &postoffice.cluster_discovery;
       ++pr) {
    fio_protocol_validate(pr);
    pr->reserved.flags |= 1;
    FIO_LIST_PUSH(&fio_data.protocols, &pr->reserved.protocols);
  }
  fio_state_callback_add(FIO_CALL_PRE_START, postoffice_pre__start, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, postoffice_forked_child, NULL);
  fio_state_callback_add(FIO_CALL_ON_FINISH, postoffice_on_finish, NULL);
  fio_state_callback_add(FIO_CALL_AT_EXIT, postoffice_at_exit, NULL);
  subscribe_args_s to_subscribe[] = {
      {
          .filter = -1,
          .channel = {"subscribe", 9},
          .on_message = postoffice_on_global_subscribe,
      },
      {
          .filter = -1,
          .channel = {"psubscribe", 10},
          .on_message = postoffice_on_global_psubscribe,
      },
      {
          .filter = -1,
          .channel = {"unsubscribe", 11},
          .on_message = postoffice_on_global_unsubscribe,
      },
      {
          .filter = -1,
          .channel = {"punsubscribe", 12},
          .on_message = postoffice_on_global_pubsubscribe,
      },
      {0},
  };
  for (int i = 0; to_subscribe[i].on_message; ++i) {
    to_subscribe[i].subscription_handle_ptr =
        postoffice.subscription_handles + i;
    fio_subscribe FIO_NOOP(to_subscribe[i]);
  }
  size_t pos = 0;
#if FIO_OS_POSIX
  memcpy(postoffice.ipc_url, "unix://./fio-pubsub-", 20);
  pos += 20;
  pos += fio_ltoa(postoffice.ipc_url + pos, ((uint64_t)fio_rand64() >> 24), 32);
  memcpy(postoffice.ipc_url + pos, ".sock", 5);
  pos += 5;
#else
  memcpy(postoffice.ipc_url, "tcp://127.0.0.1:9999", 20);
  pos += 20;
#endif
  postoffice.ipc_url[pos] = 0;
}

/* *****************************************************************************
 * TODO: clusterfy the local network using UDP broadcasting for node discovery.
 **************************************************************************** */
#if 0
/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *server_tls, fio_tls_s *client_tls);
#endif

#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, letter)(void) {
  struct test_info {
    uint64_t id;
    char *channel;
    char *msg;
    int32_t filter;
  } test_info[] = {
      {
          42,
          "My Channel",
          "My channel Message",
          0,
      },
      {
          0,
          NULL,
          "My filter Message",
          1,
      },
      {0},
  };
  for (int i = 0;
       test_info[i].msg || test_info[i].channel || test_info[i].filter;
       ++i) {
    letter_s *l =
        letter_author((fio_s *)(test_info + i),
                      test_info[i].id,
                      test_info[i].filter,
                      test_info[i].channel,
                      (test_info[i].channel ? strlen(test_info[i].channel) : 0),
                      test_info[i].msg,
                      (test_info[i].msg ? strlen(test_info[i].msg) : 0),
                      (uint8_t)(uintptr_t)FIO_PUBSUB_LOCAL);
    FIO_ASSERT(letter_id(l) == test_info[i].id,
               "message ID identity error, %llu != %llu",
               letter_id(l),
               test_info[i].id);
    FIO_ASSERT(letter_is_filter(l) == !!test_info[i].filter,
               "letter filter flag author error");
    if (letter_is_filter(l)) {
      FIO_ASSERT(letter_filter(l) == test_info[i].filter,
                 "filter identity error %d != %d",
                 letter_filter(l),
                 test_info[i].filter);
    }
    if (test_info[i].msg) {
      FIO_ASSERT(letter_message_len(l) == strlen(test_info[i].msg),
                 "letter message length error");
      FIO_ASSERT(
          !memcmp(letter_message(l), test_info[i].msg, letter_message_len(l)),
          "message identity error (%s != %.*s)",
          test_info[i].msg,
          (int)letter_message_len(l),
          letter_message(l));
    } else {
      FIO_ASSERT(!letter_message_len(l),
                 "letter message length error %d != 0",
                 letter_message_len(l));
    }
    if (test_info[i].channel) {
      FIO_ASSERT(letter_channel_len(l) == strlen(test_info[i].channel),
                 "letter channel length error");
      FIO_ASSERT(letter_channel(l) && !memcmp(letter_channel(l),
                                              test_info[i].channel,
                                              letter_channel_len(l)),
                 "channel identity error (%s != %.*s)",
                 test_info[i].channel,
                 (int)l->channel_len,
                 letter_channel(l));
    } else {
      FIO_ASSERT(!letter_channel_len(l), "letter channel length error");
    }
    letter_free(l);
  }
}
#endif /* TEST */
