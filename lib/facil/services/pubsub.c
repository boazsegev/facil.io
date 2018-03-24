/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "facil.h"
#include "fio_llist.h"
#include "fiobj.h"
#include "pubsub.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fio_mem.h"

/* used later on */
static int pubsub_glob_match(uint8_t *data, size_t data_len, uint8_t *pattern,
                             size_t pat_len);

#define PUBSUB_FACIL_CLUSTER_CHANNEL_FILTER ((int32_t)-1)
#define PUBSUB_FACIL_CLUSTER_PATTERN_FILTER ((int32_t)-2)
#define PUBSUB_FACIL_CLUSTER_CHANNEL_SUB_FILTER ((int32_t)-3)
#define PUBSUB_FACIL_CLUSTER_PATTERN_SUB_FILTER ((int32_t)-4)
#define PUBSUB_FACIL_CLUSTER_CHANNEL_UNSUB_FILTER ((int32_t)-5)
#define PUBSUB_FACIL_CLUSTER_PATTERN_UNSUB_FILTER ((int32_t)-6)

/* *****************************************************************************
The Hash Map (macros and the include instruction for `fio_hashmap.h`)
***************************************************************************** */

/* the hash key type for string keys */
typedef struct {
  uintptr_t hash;
  FIOBJ obj;
} fio_hash_key_s;

static inline int fio_hash_fiobj_keys_eq(fio_hash_key_s a, fio_hash_key_s b) {
  if (a.obj == b.obj)
    return 1;
  fio_cstr_s sa = fiobj_obj2cstr(a.obj);
  fio_cstr_s sb = fiobj_obj2cstr(b.obj);
  return sa.len == sb.len && !memcmp(sa.data, sb.data, sa.len);
}
/* define the macro to set the key type */
#define FIO_HASH_KEY_TYPE fio_hash_key_s
/* the macro that returns the key's hash value */
#define FIO_HASH_KEY2UINT(key) ((key).hash)
/* Compare the keys using length testing and `memcmp` */
#define FIO_HASH_COMPARE_KEYS(k1, k2)                                          \
  ((k1).obj == (k2).obj || fio_hash_fiobj_keys_eq((k1), (k2)))
/* an "all bytes are zero" invalid key */
#define FIO_HASH_KEY_INVALID ((fio_hash_key_s){.obj = FIOBJ_INVALID})
/* tests if a key is the invalid key */
#define FIO_HASH_KEY_ISINVALID(key) ((key).obj == FIOBJ_INVALID && !key.hash)
/* creates a persistent copy of the key's string */
#define FIO_HASH_KEY_COPY(key)                                                 \
  ((fio_hash_key_s){.hash = (key).hash, .obj = fiobj_dup((key).obj)})
/* frees the allocated string */
#define FIO_HASH_KEY_DESTROY(key) (fiobj_free((key).obj))

#define FIO_OBJ2KEY(fiobj)                                                     \
  ((fio_hash_key_s){.hash = fiobj_obj2hash((fiobj)), .obj = (fiobj)})

#include "fio_hashmap.h"

/* *****************************************************************************
Channel and Client Data Structures
***************************************************************************** */

typedef struct {
  /* clients are nodes in a list. */
  fio_ls_embd_s node;
  /* a reference counter (how many messages pending) */
  size_t ref;
  /* a subscription counter (protection against multiple unsubscribe calls) */
  size_t sub_count;
  /* a pointer to the channel data */
  void *parent;
  /** The on message callback. the `*msg` pointer is to a temporary object. */
  void (*on_message)(pubsub_message_s *msg);
  /** An optional callback for when a subscription is fully canceled. */
  void (*on_unsubscribe)(void *udata1, void *udata2);
  /** Opaque user data#1 */
  void *udata1;
  /** Opaque user data#2 .. using two allows some allocations to be avoided. */
  void *udata2;
  /** Task lock (per client-channel combination */
  spn_lock_i lock;
} client_s;

typedef struct {
  /* the root for the client's list */
  fio_ls_embd_s clients;
  /** The channel name. */
  FIOBJ name;
  /** Use pattern matching for channel subscription. */
  unsigned use_pattern : 1;
  /** Use pattern matching for channel subscription. */
  unsigned publish2cluster : 1;
} channel_s;

static fio_hash_s patterns;
static fio_hash_s channels;
static fio_hash_s clients;
static fio_hash_s engines;
static spn_lock_i lock = SPN_LOCK_INIT;
static spn_lock_i engn_lock = SPN_LOCK_INIT;

/* *****************************************************************************
Channel and Client Management
***************************************************************************** */

/* for engine thingy */
static void pubsub_on_channel_create(channel_s *ch);
/* for engine thingy */
static void pubsub_on_channel_destroy(channel_s *ch);

static void pubsub_deferred_unsub(void *cl_, void *ignr) {
  client_s *cl = cl_;
  cl->on_unsubscribe(cl->udata1, cl->udata2);
  free(cl);
  (void)ignr;
}

static inline void client_test4free(client_s *cl) {
  if (spn_sub(&cl->ref, 1)) {
    /* client is still being used. */
    return;
  }
  if (cl->on_unsubscribe) {
    /* we'll call the callback before freeing the object. */
    defer(pubsub_deferred_unsub, cl, NULL);
    return;
  }
  free(cl);
}

static inline uint64_t client_compute_hash(client_s client) {
  return (((((uint64_t)(client.on_message) *
             ((uint64_t)client.udata1 ^ 0x736f6d6570736575ULL)) >>
            5) |
           (((uint64_t)(client.on_unsubscribe) *
             ((uint64_t)client.udata1 ^ 0x736f6d6570736575ULL))
            << 47)) ^
          ((uint64_t)client.udata2 ^ 0x646f72616e646f6dULL));
}

static client_s *pubsub_client_new(client_s client, channel_s channel) {
  if (!client.on_message || !channel.name) {
    fprintf(stderr,
            "ERROR: (pubsub) subscription request failed. missing on of:\n"
            "       1. channel name.\n"
            "       2. massage handler.\n");
    if (client.on_unsubscribe)
      client.on_unsubscribe(client.udata1, client.udata2);
    return NULL;
  }
  uint64_t channel_hash = fiobj_obj2hash(channel.name);
  uint64_t client_hash = client_compute_hash(client);
  spn_lock(&lock);
  /* ignore if client exists. */
  client_s *cl = fio_hash_find(
      &clients, (fio_hash_key_s){.hash = client_hash, .obj = channel.name});
  if (cl) {
    cl->sub_count++;
    spn_unlock(&lock);
    return cl;
  }
  /* no client, we need a new client */
  cl = malloc(sizeof(*cl));
  if (!cl) {
    perror("FATAL ERROR: (pubsub) client memory allocation error");
    exit(errno);
  }
  *cl = client;
  cl->ref = 1;
  cl->sub_count = 1;

  fio_hash_insert(
      &clients, (fio_hash_key_s){.hash = client_hash, .obj = channel.name}, cl);

  /* test for existing channel */
  fio_hash_s *ch_hashmap = (channel.use_pattern ? &patterns : &channels);
  channel_s *ch = fio_hash_find(
      ch_hashmap, (fio_hash_key_s){.hash = channel_hash, .obj = channel.name});
  if (!ch) {
    /* open new channel */
    ch = malloc(sizeof(*ch));
    if (!ch) {
      perror("FATAL ERROR: (pubsub) channel memory allocation error");
      exit(errno);
    }
    *ch = (channel_s){
        .name = fiobj_dup(channel.name),
        .clients = FIO_LS_INIT(ch->clients),
        .use_pattern = channel.use_pattern,
        .publish2cluster = channel.publish2cluster,
    };
    fio_hash_insert(ch_hashmap,
                    (fio_hash_key_s){.hash = channel_hash, .obj = channel.name},
                    ch);
    pubsub_on_channel_create(ch);
  } else {
    /* channel exists */
  }
  cl->parent = ch;
  fio_ls_embd_push(&ch->clients, &cl->node);
  spn_unlock(&lock);
  return cl;
}

/** Destroys a client (and empty channels as well) */
static int pubsub_client_destroy(client_s *client) {
  if (!client || !client->parent)
    return -1;
  channel_s *ch = client->parent;

  fio_hash_s *ch_hashmap = (ch->use_pattern ? &patterns : &channels);
  uint64_t channel_hash = fiobj_obj2hash(ch->name);
  uint64_t client_hash = client_compute_hash(*client);
  uint8_t is_ch_any;
  spn_lock(&lock);
  if ((client->sub_count -= 1)) {
    spn_unlock(&lock);
    return 0;
  }
  fio_ls_embd_remove(&client->node);
  fio_hash_insert(&clients,
                  (fio_hash_key_s){.hash = client_hash, .obj = ch->name}, NULL);
  is_ch_any = fio_ls_embd_any(&ch->clients);
  if (is_ch_any) {
    /* channel still has client - we should keep it */
    (void)0;
  } else {
    channel_s *test = fio_hash_insert(
        ch_hashmap, (fio_hash_key_s){.hash = channel_hash, .obj = ch->name},
        NULL);
    if (test != ch) {
      fprintf(stderr,
              "FATAL ERROR: (pubsub) channel database corruption detected.\n");
      exit(-1);
    }
    if (ch_hashmap->capa > 32 && (ch_hashmap->pos >> 1) > ch_hashmap->count) {
      fio_hash_compact(ch_hashmap);
    }
  }
  if ((clients.pos >> 1) > clients.count) {
    // fprintf(stderr, "INFO: (pubsub) reducing client hash map %zu",
    //         (size_t)clients.capa);
    fio_hash_compact(&clients);
    // fprintf(stderr, " => %zu (%zu clients)\n", (size_t)clients.capa,
    //         (size_t)clients.count);
  }
  spn_unlock(&lock);
  client_test4free(client);
  if (is_ch_any) {
    return 0;
  }
  pubsub_on_channel_destroy(ch);
  fiobj_free(ch->name);
  free(ch);
  return 0;
}

/** finds a pointer to an existing client (matching registration details) */
static inline client_s *pubsub_client_find(client_s client, channel_s channel) {
  /* the logic is written twice due to locking logic (we don't want to release
   * the lock for `pubsub_client_new`)
   */
  if (!client.on_message || !channel.name) {
    return NULL;
  }
  uint64_t client_hash = client_compute_hash(client);
  spn_lock(&lock);
  client_s *cl = fio_hash_find(
      &clients, (fio_hash_key_s){.hash = client_hash, .obj = channel.name});
  spn_unlock(&lock);
  return cl;
}

/* *****************************************************************************
Subscription API
***************************************************************************** */

/**
 * Subscribes to a specific channel.
 *
 * Returns a subscription pointer or NULL (failure).
 */
#undef pubsub_subscribe
pubsub_sub_pt pubsub_subscribe(struct pubsub_subscribe_args args) {
  channel_s channel = {
      .name = args.channel,
      .clients = FIO_LS_INIT(channel.clients),
      .use_pattern = args.use_pattern,
      .publish2cluster = 1,
  };
  client_s client = {.on_message = args.on_message,
                     .on_unsubscribe = args.on_unsubscribe,
                     .udata1 = args.udata1,
                     .udata2 = args.udata2};
  return (pubsub_sub_pt)pubsub_client_new(client, channel);
}
#define pubsub_subscribe(...)                                                  \
  pubsub_subscribe((struct pubsub_subscribe_args){__VA_ARGS__})

/**
 * This helper searches for an existing subscription.
 *
 * Use with care, NEVER call `pubsub_unsubscribe` more times than you have
 * called `pubsub_subscribe`, since the subscription handle memory is realesed
 * onnce the reference count reaches 0.
 *
 * Returns a subscription pointer or NULL (none found).
 */
#undef pubsub_find_sub
pubsub_sub_pt pubsub_find_sub(struct pubsub_subscribe_args args) {
  channel_s channel = {.name = args.channel, .use_pattern = args.use_pattern};
  client_s client = {.on_message = args.on_message,
                     .on_unsubscribe = args.on_unsubscribe,
                     .udata1 = args.udata1,
                     .udata2 = args.udata2};
  return (pubsub_sub_pt)pubsub_client_find(client, channel);
}
#define pubsub_find_sub(...)                                                   \
  pubsub_find_sub((struct pubsub_subscribe_args){__VA_ARGS__})

/**
 * Unsubscribes from a specific subscription.
 *
 * Returns 0 on success and -1 on failure.
 */
int pubsub_unsubscribe(pubsub_sub_pt subscription) {
  if (!subscription)
    return -1;
  return pubsub_client_destroy((client_s *)subscription);
}

/**
 * Publishes a message to a channel belonging to a pub/sub service (engine).
 *
 * Returns 0 on success and -1 on failure.
 */
#undef pubsub_publish
int pubsub_publish(struct pubsub_message_s m) {
  if (!m.channel || !m.message)
    return -1;
  if (!m.engine) {
    m.engine = PUBSUB_DEFAULT_ENGINE;
    if (!m.engine) {
      m.engine = PUBSUB_CLUSTER_ENGINE;
      if (!m.engine) {
        fprintf(stderr,
                "FATAL ERROR: (pubsub) engine pointer data corrupted! \n");
        exit(-1);
      }
    }
  }
  return m.engine->publish(m.engine, m.channel, m.message);
  // We don't call `fiobj_free` because the data isn't placed into an accessible
  // object.
}
#define pubsub_publish(...)                                                    \
  pubsub_publish((struct pubsub_message_s){__VA_ARGS__})

/* *****************************************************************************
Engine handling and Management
***************************************************************************** */

/* runs in lock(!) let'm all know */
static void pubsub_on_channel_create(channel_s *ch) {
  if (ch->publish2cluster)
    PUBSUB_CLUSTER_ENGINE->subscribe(PUBSUB_CLUSTER_ENGINE, ch->name,
                                     ch->use_pattern);
  spn_lock(&engn_lock);
  FIO_HASH_FOR_LOOP(&engines, e_) {
    if (!e_ || !e_->obj)
      continue;
    pubsub_engine_s *e = e_->obj;
    e->subscribe(e, ch->name, ch->use_pattern);
  }
  spn_unlock(&engn_lock);
}

/* runs in lock(!) let'm all know */
static void pubsub_on_channel_destroy(channel_s *ch) {
  if (ch->publish2cluster)
    PUBSUB_CLUSTER_ENGINE->unsubscribe(PUBSUB_CLUSTER_ENGINE, ch->name,
                                       ch->use_pattern);
  spn_lock(&engn_lock);
  FIO_HASH_FOR_LOOP(&engines, e_) {
    if (!e_ || !e_->obj)
      continue;
    pubsub_engine_s *e = e_->obj;
    e->unsubscribe(e, ch->name, ch->use_pattern);
  }
  spn_unlock(&engn_lock);
}

/** Registers an engine, so it's callback can be called. */
void pubsub_engine_register(pubsub_engine_s *engine) {
  spn_lock(&engn_lock);
  fio_hash_insert(
      &engines,
      (fio_hash_key_s){.hash = (uintptr_t)engine, .obj = FIOBJ_INVALID},
      engine);
  spn_unlock(&engn_lock);
}

/** Unregisters an engine, so it could be safely destroyed. */
void pubsub_engine_deregister(pubsub_engine_s *engine) {
  spn_lock(&engn_lock);
  if (PUBSUB_DEFAULT_ENGINE == engine)
    PUBSUB_DEFAULT_ENGINE = (pubsub_engine_s *)PUBSUB_CLUSTER_ENGINE;
  void *old = fio_hash_insert(
      &engines,
      (fio_hash_key_s){.hash = (uintptr_t)engine, .obj = FIOBJ_INVALID}, NULL);
  fio_hash_compact(&engines);
  spn_unlock(&engn_lock);
  if (!old)
    fprintf(stderr, "Deregister error, not registered?\n");
}

/**
 * Engines can ask facil.io to resubscribe to all active channels.
 *
 * This allows engines that lost their connection to their Pub/Sub service to
 * resubscribe all the currently active channels with the new connection.
 *
 * CAUTION: This is an evented task... try not to free the engine's memory while
 * resubscriptions are under way...
 */
void pubsub_engine_resubscribe(pubsub_engine_s *eng) {
  spn_lock(&lock);
  FIO_HASH_FOR_LOOP(&channels, i) {
    channel_s *ch = i->obj;
    eng->subscribe(eng, ch->name, 0);
  }
  FIO_HASH_FOR_LOOP(&patterns, i) {
    channel_s *ch = i->obj;
    eng->subscribe(eng, ch->name, 1);
  }
  spn_unlock(&lock);
}

/* *****************************************************************************
PUBSUB_PROCESS_ENGINE: Single Process Engine and `pubsub_defer`
***************************************************************************** */

typedef struct {
  size_t ref;
  FIOBJ channel;
  FIOBJ msg;
} msg_wrapper_s;

typedef struct {
  msg_wrapper_s *wrapper;
  pubsub_message_s msg;
} msg_container_s;

static void msg_wrapper_free(msg_wrapper_s *m) {
  if (spn_sub(&m->ref, 1))
    return;
  fiobj_free(m->channel);
  fiobj_free(m->msg);
  fio_free(m);
}

/* calls a client's `on_message` callback */
void pubsub_en_process_deferred_on_message(void *cl_, void *m_) {
  msg_wrapper_s *m = m_;
  client_s *cl = cl_;
  if (spn_trylock(&cl->lock)) {
    defer(pubsub_en_process_deferred_on_message, cl, m);
    return;
  }
  msg_container_s arg = {.wrapper = m,
                         .msg = {
                             .channel = m->channel,
                             .message = m->msg,
                             .subscription = (pubsub_sub_pt)cl,
                             .udata1 = cl->udata1,
                             .udata2 = cl->udata2,
                         }};
  cl->on_message(&arg.msg);
  spn_unlock(&cl->lock);
  msg_wrapper_free(m);
  client_test4free(cl_);
}

/* Must subscribe channel. Failures are ignored. */
void pubsub_en_process_subscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                 uint8_t use_pattern) {
  (void)eng;
  (void)channel;
  (void)use_pattern;
}

/* Must unsubscribe channel. Failures are ignored. */
void pubsub_en_process_unsubscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                   uint8_t use_pattern) {
  (void)eng;
  (void)channel;
  (void)use_pattern;
}
/** Should return 0 on success and -1 on failure. */
int pubsub_en_process_publish(const pubsub_engine_s *eng, FIOBJ channel,
                              FIOBJ msg) {
  uint64_t channel_hash = fiobj_obj2hash(channel);
  msg_wrapper_s *m = fio_malloc(sizeof(*m));
  int ret = -1;
  if (!m) {
    perror("FATAL ERROR: (pubsub) couldn't allocate message wrapper");
    exit(errno);
  }
  *m = (msg_wrapper_s){
      .ref = 1, .channel = fiobj_dup(channel), .msg = fiobj_dup(msg)};
  spn_lock(&lock);
  {
    /* test for direct match */
    channel_s *ch = fio_hash_find(
        &channels, (fio_hash_key_s){.hash = channel_hash, .obj = channel});
    if (ch) {
      ret = 0;
      FIO_LS_EMBD_FOR(&ch->clients, cl_) {
        client_s *cl = FIO_LS_EMBD_OBJ(client_s, node, cl_);
        spn_add(&m->ref, 1);
        spn_add(&cl->ref, 1);
        defer(pubsub_en_process_deferred_on_message, cl, m);
      }
    }
  }
  /* test for pattern match */
  fio_cstr_s ch_str = fiobj_obj2cstr(channel);
  FIO_HASH_FOR_LOOP(&patterns, ch_) {
    channel_s *ch = (channel_s *)ch_->obj;
    fio_cstr_s tmp = fiobj_obj2cstr(ch->name);
    if (pubsub_glob_match(ch_str.bytes, ch_str.len, tmp.bytes, tmp.len)) {
      ret = 0;
      FIO_LS_EMBD_FOR(&ch->clients, cl_) {
        client_s *cl = FIO_LS_EMBD_OBJ(client_s, node, cl_);
        spn_add(&m->ref, 1);
        spn_add(&cl->ref, 1);
        defer(pubsub_en_process_deferred_on_message, cl, m);
      }
    }
  }
  spn_unlock(&lock);
  msg_wrapper_free(m);
  return ret;
  (void)eng;
}

const pubsub_engine_s PUBSUB_PROCESS_ENGINE_S = {
    .subscribe = pubsub_en_process_subscribe,
    .unsubscribe = pubsub_en_process_unsubscribe,
    .publish = pubsub_en_process_publish,
};

const pubsub_engine_s *PUBSUB_PROCESS_ENGINE = &PUBSUB_PROCESS_ENGINE_S;

/**
 * defers message hadling if it can't be performed (i.e., resource is busy) or
 * should be fragmented (allowing large tasks to be broken down).
 *
 * This should only be called from within the `on_message` callback.
 *
 * It's recommended that the `on_message` callback return immediately following
 * this function call, as code might run concurrently.
 *
 * Uses reference counting for zero copy.
 *
 * It's impossible to use a different `on_message` callbck without resorting to
 * memory allocations... so when in need, manage routing withing the
 * `on_message` callback.
 */
void pubsub_defer(pubsub_message_s *msg) {
  msg_container_s *arg = FIO_LS_EMBD_OBJ(msg_container_s, msg, msg);
  spn_add(&arg->wrapper->ref, 1);
  spn_add(&((client_s *)arg->msg.subscription)->ref, 1);
  defer(pubsub_en_process_deferred_on_message, arg->msg.subscription,
        arg->wrapper);
}

/* *****************************************************************************
Cluster Engine
***************************************************************************** */

/* Must subscribe channel. Failures are ignored. */
void pubsub_en_cluster_subscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                 uint8_t use_pattern) {
  if (facil_is_running()) {
    facil_cluster_send((use_pattern ? PUBSUB_FACIL_CLUSTER_PATTERN_SUB_FILTER
                                    : PUBSUB_FACIL_CLUSTER_CHANNEL_SUB_FILTER),
                       channel, FIOBJ_INVALID);
  }
  (void)eng;
}

/* Must unsubscribe channel. Failures are ignored. */
void pubsub_en_cluster_unsubscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                   uint8_t use_pattern) {
  if (facil_is_running()) {
    facil_cluster_send((use_pattern
                            ? PUBSUB_FACIL_CLUSTER_PATTERN_UNSUB_FILTER
                            : PUBSUB_FACIL_CLUSTER_CHANNEL_UNSUB_FILTER),
                       channel, FIOBJ_INVALID);
  }
  (void)eng;
}
/** Should return 0 on success and -1 on failure. */
int pubsub_en_cluster_publish(const pubsub_engine_s *eng, FIOBJ channel,
                              FIOBJ msg) {
  if (facil_is_running()) {
    facil_cluster_send(PUBSUB_FACIL_CLUSTER_CHANNEL_FILTER, channel, msg);
  }
  return PUBSUB_PROCESS_ENGINE->publish(PUBSUB_PROCESS_ENGINE, channel, msg);
  (void)eng;
}

const pubsub_engine_s PUBSUB_CLUSTER_ENGINE_S = {
    .subscribe = pubsub_en_cluster_subscribe,
    .unsubscribe = pubsub_en_cluster_unsubscribe,
    .publish = pubsub_en_cluster_publish,
};

pubsub_engine_s const *PUBSUB_CLUSTER_ENGINE = &PUBSUB_CLUSTER_ENGINE_S;
pubsub_engine_s *PUBSUB_DEFAULT_ENGINE =
    (pubsub_engine_s *)&PUBSUB_CLUSTER_ENGINE_S;
/* *****************************************************************************
Cluster Initialization and Messaging Protocol
***************************************************************************** */

/* does nothing */
static void pubsub_cluster_on_message_noop(pubsub_message_s *msg) { (void)msg; }

/* registers to the channel  */
static void pubsub_cluster_subscribe2channel(void *ch, void *flag) {
  channel_s channel = {
      .name = (FIOBJ)ch,
      .clients = FIO_LS_INIT(channel.clients),
      .use_pattern = ((uintptr_t)flag & 1),
      .publish2cluster = 0,
  };
  client_s client = {.on_message = pubsub_cluster_on_message_noop};
  pubsub_client_new(client, channel);
  fiobj_free((FIOBJ)ch);
}

/* deregisters from the channel if required */
static void pubsub_cluster_unsubscribe2channel(void *ch, void *flag) {
  channel_s channel = {
      .name = (FIOBJ)ch,
      .clients = FIO_LS_INIT(channel.clients),
      .use_pattern = ((uintptr_t)flag & 1),
      .publish2cluster = 0,
  };
  client_s client = {.on_message = pubsub_cluster_on_message_noop};
  client_s *sub = pubsub_client_find(client, channel);
  pubsub_client_destroy(sub);
  fiobj_free((FIOBJ)ch);
}

static void pubsub_cluster_facil_message(int32_t filter, FIOBJ channel,
                                         FIOBJ message) {
  // fprintf(stderr, "(%d) pubsub message filter %d (%s)\n", getpid(), filter,
  //         fiobj_obj2cstr(channel).name);
  switch (filter) {
  case PUBSUB_FACIL_CLUSTER_CHANNEL_FILTER:
    PUBSUB_PROCESS_ENGINE->publish(PUBSUB_PROCESS_ENGINE, channel, message);
    break;
  case PUBSUB_FACIL_CLUSTER_CHANNEL_SUB_FILTER:
    pubsub_cluster_subscribe2channel((void *)channel, 0);
    break;
  case PUBSUB_FACIL_CLUSTER_PATTERN_SUB_FILTER:
    pubsub_cluster_subscribe2channel((void *)channel, (void *)1);
    break;
  case PUBSUB_FACIL_CLUSTER_CHANNEL_UNSUB_FILTER:
    pubsub_cluster_unsubscribe2channel((void *)channel, 0);
    break;
  case PUBSUB_FACIL_CLUSTER_PATTERN_UNSUB_FILTER:
    pubsub_cluster_unsubscribe2channel((void *)channel, (void *)1);
    break;
  }
  (void)filter;
}

void pubsub_cluster_init(void) {
  facil_cluster_set_handler(PUBSUB_FACIL_CLUSTER_CHANNEL_FILTER,
                            pubsub_cluster_facil_message);
  facil_cluster_set_handler(PUBSUB_FACIL_CLUSTER_CHANNEL_SUB_FILTER,
                            pubsub_cluster_facil_message);
  facil_cluster_set_handler(PUBSUB_FACIL_CLUSTER_PATTERN_SUB_FILTER,
                            pubsub_cluster_facil_message);
  facil_cluster_set_handler(PUBSUB_FACIL_CLUSTER_CHANNEL_UNSUB_FILTER,
                            pubsub_cluster_facil_message);
  facil_cluster_set_handler(PUBSUB_FACIL_CLUSTER_PATTERN_UNSUB_FILTER,
                            pubsub_cluster_facil_message);
}

void pubsub_cluster_on_fork(void) {
  lock = SPN_LOCK_INIT;
  FIO_HASH_FOR_LOOP(&clients, pos) {
    if (pos->obj) {
      client_s *c = pos->obj;
      c->lock = SPN_LOCK_INIT;
    }
  }
}

void pubsub_cluster_cleanup(void) {
  FIO_HASH_FOR_FREE(&clients, pos) { pubsub_client_destroy(pos->obj); }
  fio_hash_free(&engines);
  fio_hash_free(&channels);
  fio_hash_free(&patterns);
  clients = (fio_hash_s)FIO_HASH_INIT;
  engines = (fio_hash_s)FIO_HASH_INIT;
  channels = (fio_hash_s)FIO_HASH_INIT;
  patterns = (fio_hash_s)FIO_HASH_INIT;
  lock = SPN_LOCK_INIT;
}

/* *****************************************************************************
Glob Matching Helper
***************************************************************************** */

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
static int pubsub_glob_match(uint8_t *data, size_t data_len, uint8_t *pattern,
                             size_t pat_len) {
  /* adapted and rewritten, with thankfulness, from the code at:
   * https://github.com/opnfv/kvmfornfv/blob/master/kernel/lib/glob.c
   *
   * Original version's copyright:
   * Copyright 2015 Open Platform for NFV Project, Inc. and its contributors
   * Under the MIT license.
   */

  /*
   * Backtrack to previous * on mismatch and retry starting one
   * character later in the string.  Because * matches all characters
   * (no exception for /), it can be easily proved that there's
   * never a need to backtrack multiple levels.
   */
  uint8_t *back_pat = NULL, *back_str = data;
  size_t back_pat_len = 0, back_str_len = data_len;

  /*
   * Loop over each token (character or class) in pat, matching
   * it against the remaining unmatched tail of str.  Return false
   * on mismatch, or true after matching the trailing nul bytes.
   */
  while (data_len) {
    uint8_t c = *data++;
    uint8_t d = *pattern++;
    data_len--;
    pat_len--;

    switch (d) {
    case '?': /* Wildcard: anything goes */
      break;

    case '*':       /* Any-length wildcard */
      if (!pat_len) /* Optimize trailing * case */
        return 1;
      back_pat = pattern;
      back_pat_len = pat_len;
      back_str = --data; /* Allow zero-length match */
      back_str_len = ++data_len;
      break;

    case '[': { /* Character class */
      uint8_t match = 0, inverted = (*pattern == '^');
      uint8_t *cls = pattern + inverted;
      uint8_t a = *cls++;

      /*
       * Iterate over each span in the character class.
       * A span is either a single character a, or a
       * range a-b.  The first span may begin with ']'.
       */
      do {
        uint8_t b = a;

        if (cls[0] == '-' && cls[1] != ']') {
          b = cls[1];

          cls += 2;
          if (a > b) {
            uint8_t tmp = a;
            a = b;
            b = tmp;
          }
        }
        match |= (a <= c && c <= b);
      } while ((a = *cls++) != ']');

      if (match == inverted)
        goto backtrack;
      pat_len -= cls - pattern;
      pattern = cls;

    } break;
    case '\\':
      d = *pattern++;
      pat_len--;
    /*FALLTHROUGH*/
    default: /* Literal character */
      if (c == d)
        break;
    backtrack:
      if (!back_pat)
        return 0; /* No point continuing */
      /* Try again from last *, one character later in str. */
      pattern = back_pat;
      data = ++back_str;
      data_len = --back_str_len;
      pat_len = back_pat_len;
    }
  }
  return !data_len && !pat_len;
}
