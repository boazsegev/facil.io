/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "facil.h"
#include "fio_dict.h"
#include "fio_hash_table.h"
#include "pubsub.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* *****************************************************************************
Data Structures / State
***************************************************************************** */

typedef struct {
  volatile uint64_t ref;
  struct pubsub_publish_args pub;
} msg_s;

typedef struct {
  fio_ht_s clients;
  const pubsub_engine_s *engine;
  struct {
    char *name;
    uint32_t len;
  };
  union {
    fio_dict_s dict;
    fio_list_s list;
  } channels;
  unsigned use_pattern : 1;
} channel_s;

typedef struct pubsub_sub_s {
  fio_ht_node_s clients;
  void (*on_message)(pubsub_sub_pt s, pubsub_message_s msg, void *udata1,
                     void *udata2);
  void *udata1;
  void *udata2;
  channel_s *parent;
  volatile uint64_t active;
  uint32_t ref;
} client_s;

static fio_dict_s pubsub_channels = FIO_DICT_INIT_STATIC;
static fio_list_s pubsub_patterns = FIO_LIST_INIT_STATIC(pubsub_patterns);

spn_lock_i pubsub_GIL = SPN_LOCK_INIT;

/* *****************************************************************************
Helpers
***************************************************************************** */

static void pubsub_free_client(void *client_, void *ignr) {
  client_s *client = client_;
  if (!spn_sub(&client->active, 1))
    free(client);
  (void)ignr;
}

static void pubsub_free_msg(void *msg_, void *ignr) {
  msg_s *msg = msg_;
  if (!spn_sub(&msg->ref, 1)) {
    free(msg);
  }
  (void)ignr;
}

static void pubsub_deliver_msg(void *client_, void *msg_) {
  client_s *cl = client_;
  msg_s *msg = msg_;
  cl->on_message(cl,
                 (struct pubsub_message_s){
                     .engine = msg->pub.engine,
                     .channel.name = msg->pub.channel.name,
                     .channel.len = msg->pub.channel.len,
                     .msg.data = msg->pub.msg.data,
                     .msg.len = msg->pub.msg.len,
                     .use_pattern = msg->pub.use_pattern,
                 },
                 cl->udata1, cl->udata2);
  pubsub_free_msg(msg, NULL);
  pubsub_free_client(cl, NULL);
}

/* *****************************************************************************
Pattern Matching
***************************************************************************** */

/* TODO: pattern matching using trie dictionary */

/* *****************************************************************************
Default Engine / Cluster Support
***************************************************************************** */

/** Used internally to identify oub/sub cluster messages. */
#define FACIL_PUBSUB_CLUSTER_MESSAGE_ID ((int32_t)-1)

/*
a `ppublish` / `publish` found this channel as a match...
Pubblish to clients and test against subscribed patterns.
*/
static void pubsub_publish_matched_channel(fio_dict_s *ch_, void *msg_) {
  msg_s *msg = msg_;
  client_s *cl;
  if (ch_) {
    channel_s *channel = fio_node2obj(channel_s, channels, ch_);
    fio_ht_for_each(client_s, clients, cl, channel->clients) {
      spn_add(&msg->ref, 1);
      spn_add(&cl->active, 1);
      defer(pubsub_deliver_msg, cl, msg);
    }
  }
  channel_s *pattern;
  fio_list_for_each(channel_s, channels.list, pattern, pubsub_patterns) {
    if (fio_glob_match((uint8_t *)msg->pub.channel.name, msg->pub.channel.len,
                       (uint8_t *)pattern->name, pattern->len)) {
      fio_ht_for_each(client_s, clients, cl, pattern->clients) {
        spn_add(&msg->ref, 1);
        spn_add(&cl->active, 1);
        defer(pubsub_deliver_msg, cl, msg);
      }
    }
  }
}

static void pubsub_perform_publish(void *msg_, void *ignr) {
  msg_s *msg = msg_;
  fio_dict_s *channel;
  spn_lock(&pubsub_GIL);

  if (msg->pub.use_pattern) {
    fio_dict_each_match_glob(fio_dict_prefix(&pubsub_channels,
                                             (void *)&msg->pub.engine,
                                             sizeof(void *)),
                             msg->pub.channel.name, msg->pub.channel.len,
                             pubsub_publish_matched_channel, msg);
  } else {
    channel = (void *)fio_dict_get(fio_dict_prefix(&pubsub_channels,
                                                   (void *)&msg->pub.engine,
                                                   sizeof(void *)),
                                   msg->pub.channel.name, msg->pub.channel.len);
    pubsub_publish_matched_channel(channel, msg);
  }

  spn_unlock(&pubsub_GIL);
  pubsub_free_msg(msg, NULL);
  (void)ignr;
}

static int pubsub_cluster_publish(struct pubsub_publish_args args) {
  msg_s *msg = malloc(sizeof(*msg) + args.channel.len + args.msg.len + 2);
  if (!msg)
    return -1;
  *msg = (msg_s){.ref = 1, .pub = args};
  struct pubsub_publish_args *pub = &msg->pub;
  pub->msg.data = (char *)(pub + 1);
  memcpy(pub->msg.data, args.msg.data, args.msg.len);
  pub->msg.data[args.msg.len] = 0;
  if (pub->channel.name) {
    pub->channel.name = pub->msg.data + args.msg.len + 1;
    memcpy(pub->channel.name, args.channel.name, args.channel.len);
    pub->channel.name[args.channel.len] = 0;
  }
  pub->use_pattern = args.use_pattern;
  if (args.push2cluster)
    facil_cluster_send(FACIL_PUBSUB_CLUSTER_MESSAGE_ID, msg,
                       sizeof(*msg) + args.channel.len + args.msg.len + 2);
  defer(pubsub_perform_publish, msg, NULL);
  return 0;
}

static void pubsub_cluster_handle_publishing(void *data, uint32_t len) {
  msg_s *msg = data;
  if (len != sizeof(*msg) + msg->pub.channel.len + msg->pub.msg.len + 2) {
    fprintf(stderr,
            "ERROR: (pub/sub) cluster message size error. Message ignored.\n");
    return;
  }
  msg = malloc(len);
  if (!msg) {
    fprintf(
        stderr,
        "ERROR: (pub/sub) cluster message allocation error.Message ignored.\n");
    return;
  }
  memcpy(msg, data, len);
  msg->ref = 1;
  msg->pub.msg.data = (char *)(msg + 1);
  if (msg->pub.channel.name)
    msg->pub.channel.name = msg->pub.msg.data + msg->pub.msg.len + 1;
  defer(pubsub_perform_publish, msg, NULL);
}

void pubsub_cluster_init(void) {
  /* facil functions are thread safe, so we can call this without a guard. */
  facil_cluster_set_handler(FACIL_PUBSUB_CLUSTER_MESSAGE_ID,
                            pubsub_cluster_handle_publishing);
}

static int pubsub_cluster_subscribe(const pubsub_engine_s *eng, const char *ch,
                                    size_t ch_len, uint8_t use_pattern) {
  return 0;
  (void)ch;
  (void)ch_len;
  (void)use_pattern;
  (void)eng;
}

static void pubsub_cluster_unsubscribe(const pubsub_engine_s *eng,
                                       const char *ch, size_t ch_len,
                                       uint8_t use_pattern) {
  (void)ch;
  (void)ch_len;
  (void)use_pattern;
  (void)eng;
}

static int pubsub_cluster_eng_publish(const pubsub_engine_s *eng,
                                      const char *ch, size_t ch_len,
                                      const char *msg, size_t msg_len,
                                      uint8_t use_pattern) {
  pubsub_cluster_publish((struct pubsub_publish_args){
      .engine = eng,
      .channel.name = (char *)ch,
      .channel.len = ch_len,
      .msg.data = (char *)msg,
      .msg.len = msg_len,
      .use_pattern = use_pattern,
      .push2cluster = eng->push2cluster,
  });
  return 0;
}

static const pubsub_engine_s PUBSUB_CLUSTER_ENGINE = {
    .publish = pubsub_cluster_eng_publish,
    .subscribe = pubsub_cluster_subscribe,
    .unsubscribe = pubsub_cluster_unsubscribe,
    .push2cluster = 1};

/* *****************************************************************************
External Engine Bridge
***************************************************************************** */

#undef pubsub_engine_distribute

void pubsub_engine_distribute(pubsub_message_s msg) {
  pubsub_cluster_publish((struct pubsub_publish_args){
      .engine = msg.engine,
      .channel.name = msg.channel.name,
      .channel.len = msg.channel.len,
      .msg.data = msg.msg.data,
      .msg.len = msg.msg.len,
      .push2cluster = msg.engine->push2cluster,
      .use_pattern = msg.use_pattern,
  });
}

/* *****************************************************************************
API
***************************************************************************** */

#undef pubsub_subscribe
pubsub_sub_pt pubsub_subscribe(struct pubsub_subscribe_args args) {
  if (!args.on_message)
    return NULL;
  if (args.channel.name && !args.channel.len)
    args.channel.len = strlen(args.channel.name);
  if (args.channel.len > 1024) {
    return NULL;
  }
  if (!args.engine)
    args.engine = &PUBSUB_CLUSTER_ENGINE;
  channel_s *ch;
  client_s *client;
  uint64_t cl_hash = (uintptr_t)args.on_message *
                     ((uintptr_t)args.udata1 ^ 0x736f6d6570736575ULL);
  cl_hash = ((cl_hash >> 5) | (cl_hash << 59)) *
            ((uintptr_t)args.udata2 ^ 0x646f72616e646f6dULL);

  spn_lock(&pubsub_GIL);
  if (args.use_pattern) {
    fio_list_for_each(channel_s, channels.list, ch, pubsub_patterns) {
      if (ch->engine == args.engine && ch->len == args.channel.len &&
          !memcmp(ch->name, args.channel.name, args.channel.len))
        goto found_channel;
    }
  } else {
    ch = (void *)fio_dict_get(
        fio_dict_prefix(&pubsub_channels, (void *)&args.engine, sizeof(void *)),
        args.channel.name, args.channel.len);
    if (ch) {
      ch = fio_node2obj(channel_s, channels, ch);
      goto found_channel;
    }
  }

  if (args.engine->subscribe(args.engine, args.channel.name, args.channel.len,
                             args.use_pattern))
    goto error;
  ch = malloc(sizeof(*ch) + args.channel.len + 1 + sizeof(void *));
  *ch = (channel_s){
      .clients = FIO_HASH_TABLE_INIT(ch->clients),
      .name = (char *)(ch + 1),
      .len = args.channel.len,
      .use_pattern = args.use_pattern,
      .engine = args.engine,
  };
  memcpy(ch->name, args.channel.name, args.channel.len);

  ch->name[args.channel.len + sizeof(void *)] = 0;

  if (args.use_pattern) {
    fio_list_add(&pubsub_patterns, &ch->channels.list);
  } else {
    fio_dict_set(fio_dict_ensure_prefix(&pubsub_channels, (void *)&args.engine,
                                        sizeof(void *)),
                 args.channel.name, args.channel.len, &ch->channels.dict);
  }
// fprintf(stderr, "Created a new channel for %s\n", args.channel.name);
found_channel:

  client = (void *)fio_ht_find(&ch->clients, cl_hash);
  if (client) {
    client = fio_ht_object(client_s, clients, client);
    goto found_client;
  }

  client = malloc(sizeof(*client));
  if (!client)
    goto error;
  *client = (client_s){
      .on_message = args.on_message,
      .udata1 = args.udata1,
      .udata2 = args.udata2,
      .parent = ch,
      .active = 0,
      .ref = 0,
  };
  fio_ht_add(&ch->clients, &client->clients, cl_hash);

found_client:

  client->ref++;
  spn_add(&client->active, 1);
  spn_unlock(&pubsub_GIL);
  return client;

error:
  spn_unlock(&pubsub_GIL);
  return NULL;
}

void pubsub_unsubscribe(pubsub_sub_pt client) {
  if (!client)
    return;
  spn_lock(&pubsub_GIL);
  client->ref--;
  if (client->ref) {
    spn_unlock(&pubsub_GIL);
    spn_sub(&client->active, 1);
    return;
  }
  fio_ht_remove(&client->clients);
  if (client->parent->clients.count) {
    spn_unlock(&pubsub_GIL);
    defer(pubsub_free_client, client, NULL);
    return;
  }
  channel_s *ch = client->parent;
  if (ch->use_pattern) {
    fio_list_remove(&ch->channels.list);
  } else {
    fio_dict_remove(&ch->channels.dict);
  }
  spn_unlock(&pubsub_GIL);
  defer(pubsub_free_client, client, NULL);
  ch->engine->unsubscribe(ch->engine, ch->name, ch->len, ch->use_pattern);
  fio_ht_rehash(&ch->clients, 0);
  free(ch);
}

#undef pubsub_publish
int pubsub_publish(struct pubsub_publish_args args) {
  if (!args.msg.data)
    return -1;
  if (!args.msg.len)
    args.msg.len = strlen(args.msg.data);
  if (args.channel.name && !args.channel.len)
    args.channel.len = strlen(args.channel.name);
  if (!args.engine) {
    args.engine = &PUBSUB_CLUSTER_ENGINE;
    args.push2cluster = 1;
  } else if (args.push2cluster)
    PUBSUB_CLUSTER_ENGINE.publish(args.engine, args.channel.name,
                                  args.channel.len, args.msg.data, args.msg.len,
                                  args.use_pattern);
  return args.engine->publish(args.engine, args.channel.name, args.channel.len,
                              args.msg.data, args.msg.len, args.use_pattern);
}
