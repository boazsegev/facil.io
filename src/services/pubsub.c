/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "facil.h"
#include "fio_hash_table.h"
#include "fio_list.h"
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
  fio_ht_node_s channels;
  const pubsub_engine_s *engine;
  char *name;
  uint32_t len;
  unsigned use_pattern : 1;
} channel_s;

typedef struct pubsub_sub_s {
  fio_ht_node_s clients;
  void (*on_message)(pubsub_sub_pt s, pubsub_message_s msg, void *udata);
  void *udata;
  channel_s *parent;
  volatile uint64_t active;
  uint32_t ref;
} client_s;

static fio_ht_s pubsub_channels = FIO_HASH_TABLE_STATIC(pubsub_channels);
static fio_ht_s pubsub_patterns = FIO_HASH_TABLE_STATIC(pubsub_patterns);

spn_lock_i pubsub_GIL = SPN_LOCK_INIT;

static inline uint64_t atomic_bump(volatile uint64_t *i) {
  return __sync_add_and_fetch(i, 1ULL);
}
static inline uint64_t atomic_cut(volatile uint64_t *i) {
  return __sync_sub_and_fetch(i, 1ULL);
}

/* *****************************************************************************
Helpers
***************************************************************************** */

static void pubsub_free_client(void *client_, void *ignr) {
  client_s *client = client_;
  if (!atomic_cut(&client->active))
    free(client);
  (void)ignr;
}

static void pubsub_free_msg(void *msg_, void *ignr) {
  msg_s *msg = msg_;
  if (!atomic_cut(&msg->ref)) {
    free(msg);
  }
  (void)ignr;
}

static void pubsub_deliver_msg(void *client_, void *msg_) {
  client_s *cl = client_;
  msg_s *msg = msg_;
  cl->on_message(cl,
                 (struct pubsub_message_s){
                     .channel.name = msg->pub.channel.name,
                     .channel.len = msg->pub.channel.len,
                     .msg.data = msg->pub.msg.data,
                     .msg.len = msg->pub.msg.len,
                 },
                 cl->udata);
  pubsub_free_msg(msg, NULL);
  pubsub_free_client(cl, NULL);
}

/* *****************************************************************************
Pattern Matching
***************************************************************************** */

/* returns 1 on match, 0 on non-match */
static int pubsub_match_pattern(char *pattern, size_t pattern_len, char *name,
                                size_t name_len) {
  /* based on the same matching behavior used by Redis
  https://github.com/antirez/redis/blob/d680eb6dbdf2d2030cb96edfb089be1e2a775ac1/src/util.c#L47
  */
  while (pattern_len && name_len) {
    if (*pattern == '*') {
      /* eat up all '*' and match whatever */
      while (*pattern == '*' && pattern_len) {
        pattern_len--;
        pattern++;
      }
      if (!pattern_len)
        return 1;
      /* test the "tail" using recursion for every starting point :-/ */
      while (name_len) {
        if (pubsub_match_pattern(pattern, pattern_len, name++, name_len--))
          return 1;
      }
      return 0;
    }
    if (*pattern == '[') {
      int state = 1, match = 0;
      pattern++;
      pattern_len--;
      if (*pattern == '^') {
        state = 0;
        pattern++;
        pattern_len--;
      }
      while (*pattern != ']' && !match) {
        if (*pattern == '\\') {
          pattern++;
          pattern_len--;
        }
        if (pattern_len < 2)
          return 0; /* pattern error */
        if (pattern[1] == '-') {
          if (pattern_len < 4)
            return 0; /* pattern error */
          char end, start = *pattern;
          pattern += 2;
          if (*pattern == '\\') {
            pattern++;
            pattern_len--;
          }
          end = *pattern;
          if (start > end) {
            char tmp;
            tmp = start;
            start = end;
            end = tmp;
          }
          if (*name >= start && *name <= end)
            match = 1;
        } else if (*pattern == *name)
          match = 1;
        pattern++;
        pattern_len--;
      }
      /* in case matching was cut short. */
      while (*pattern != ']' && pattern_len) {
        pattern++;
        pattern_len--;
      }
      if (match != state)
        return 0;
      if (!pattern_len && name_len)
        return 0;
      name++;
      name_len--;
      pattern++;
      pattern_len--;
      continue;
    } else if (*pattern == '\\' && pattern_len > 1) {
      pattern++;
      pattern_len--;
    }
    if (*pattern != *name)
      return 0;
    pattern++;
    pattern_len--;
    name++;
    name_len--;
  }
  if (!name_len) {
    while (*pattern == '*' && pattern_len) {
      pattern++;
      pattern_len--;
    }
  }
  if (!name_len && !pattern_len)
    return 1;
  return 0;
}

/* *****************************************************************************
Default Engine / Cluster Support
***************************************************************************** */

/** Used internally to identify oub/sub cluster messages. */
#define FACIL_PUBSUB_CLUSTER_MESSAGE_ID ((uint32_t)-5)

static void pubsub_perform_publish(void *msg_, void *ignr) {
  msg_s *msg = msg_;
  channel_s *channel, *pattern;
  client_s *cl;

  spn_lock(&pubsub_GIL);
  if (msg->pub.use_pattern) {
    fio_ht_for_each(channel_s, channels, channel, pubsub_channels) {
      if (channel->engine == msg->pub.engine &&
          pubsub_match_pattern(msg->pub.channel.name, msg->pub.channel.len,
                               channel->name, channel->len)) {
        fio_ht_for_each(client_s, clients, cl, channel->clients) {
          atomic_bump(&msg->ref);
          atomic_bump(&cl->active);
          defer(pubsub_deliver_msg, cl, msg);
        }
        fio_ht_for_each(channel_s, channels, pattern, pubsub_patterns) {
          if (pattern->engine == msg->pub.engine &&
              pubsub_match_pattern(pattern->name, pattern->len, channel->name,
                                   channel->len)) {
            fio_ht_for_each(client_s, clients, cl, pattern->clients) {
              atomic_bump(&msg->ref);
              atomic_bump(&cl->active);
              defer(pubsub_deliver_msg, cl, msg);
            }
          }
        }
      }
    }
  } else {
    channel = (void *)fio_ht_find(
        &pubsub_channels,
        (fio_ht_hash(msg->pub.channel.name, msg->pub.channel.len) ^
         (intptr_t)msg->pub.engine));
    if (channel) {
      channel = fio_ht_object(channel_s, channels, channel);
      fio_ht_for_each(client_s, clients, cl, channel->clients) {
        atomic_bump(&msg->ref);
        atomic_bump(&cl->active);
        defer(pubsub_deliver_msg, cl, msg);
      }
    }
    fio_ht_for_each(channel_s, channels, channel, pubsub_patterns) {
      if (channel->engine == msg->pub.engine &&
          pubsub_match_pattern(channel->name, channel->len,
                               msg->pub.channel.name, msg->pub.channel.len)) {
        fio_ht_for_each(client_s, clients, cl, channel->clients) {
          atomic_bump(&msg->ref);
          atomic_bump(&cl->active);
          defer(pubsub_deliver_msg, cl, msg);
        }
      }
    }
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

static int pubsub_cluster_subscribe(struct pubsub_subscribe_args args) {
  return 0;
  (void)args;
}

static void pubsub_cluster_unsubscribe(struct pubsub_subscribe_args a) {
  (void)a;
}

static const pubsub_engine_s PUBSUB_CLUSTER_ENGINE = {
    .publish = pubsub_cluster_publish,
    .subscribe = pubsub_cluster_subscribe,
    .unsubscribe = pubsub_cluster_unsubscribe,
};

/* *****************************************************************************
External Engine Bridge
***************************************************************************** */

static void pubsub_engine_bridge(pubsub_sub_pt s, pubsub_message_s msg,
                                 void *udata) {
  pubsub_cluster_publish((struct pubsub_publish_args){
      .engine = udata,
      .channel.name = msg.channel.name,
      .channel.len = msg.channel.len,
      .msg.data = msg.msg.data,
      .msg.len = msg.msg.len,
      .push2cluster = ((struct pubsub_engine_s *)udata)->push2cluster,
  });
  (void)s;
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
  if (!args.engine)
    args.engine = &PUBSUB_CLUSTER_ENGINE;
  fio_ht_s *chlist = args.use_pattern ? &pubsub_patterns : &pubsub_channels;
  channel_s *ch;
  client_s *client;
  uint64_t ch_hash = (fio_ht_hash(args.channel.name, args.channel.len) ^
                      (uintptr_t)args.engine);
  uint64_t cl_hash = (fio_ht_hash(&args.on_message, (sizeof(void *) << 1)));
  spn_lock(&pubsub_GIL);
  ch = (void *)fio_ht_find(chlist, ch_hash);
  if (ch) {
    ch = fio_ht_object(channel_s, channels, ch);
    goto found_channel;
  }
  if (args.engine->subscribe((struct pubsub_subscribe_args){
          .engine = args.engine,
          .channel.name = args.channel.name,
          .channel.len = args.channel.len,
          .on_message = pubsub_engine_bridge,
          .udata = (void *)args.engine,
          .use_pattern = args.use_pattern,
      }))
    goto error;
  ch = malloc(sizeof(*ch) + args.channel.len + 1);
  *ch = (channel_s){
      .clients = FIO_HASH_TABLE_INIT(ch->clients),
      .name = (char *)(ch + 1),
      .len = args.channel.len,
      .use_pattern = args.use_pattern,
      .engine = args.engine,
  };
  memcpy(ch->name, args.channel.name, args.channel.len);
  ch->name[args.channel.len] = 0;
  fio_ht_add(chlist, &ch->channels, ch_hash);
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
      .udata = args.udata,
      .parent = ch,
      .active = 0,
      .ref = 0,
  };
  fio_ht_add(&ch->clients, &client->clients, cl_hash);

found_client:

  client->ref++;
  atomic_bump(&client->active);
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
    atomic_cut(&client->active);
    return;
  }
  fio_ht_remove(&client->clients);
  if (client->parent->clients.count) {
    spn_unlock(&pubsub_GIL);
    defer(pubsub_free_client, client, NULL);
    return;
  }
  channel_s *ch = client->parent;
  fio_ht_remove(&ch->channels);
  spn_unlock(&pubsub_GIL);
  defer(pubsub_free_client, client, NULL);
  ch->engine->unsubscribe((struct pubsub_subscribe_args){
      .engine = ch->engine,
      .channel.name = ch->name,
      .channel.len = ch->len,
      .on_message = pubsub_engine_bridge,
      .udata = (void *)ch->engine,
      .use_pattern = ch->use_pattern,
  });
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
    PUBSUB_CLUSTER_ENGINE.publish(args);
  return args.engine->publish(args);
}
