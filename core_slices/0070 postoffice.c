/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0201 postoffice.c
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************
Section Start Marker












                                 Post Office













***************************************************************************** */

/* *****************************************************************************
 * Data Structures - Channel / Subscriptions data
 **************************************************************************** */

typedef enum fio_cluster_message_type_e {
  FIO_CLUSTER_MSG_FORWARD,
  FIO_CLUSTER_MSG_JSON,
  FIO_CLUSTER_MSG_ROOT,
  FIO_CLUSTER_MSG_ROOT_JSON,
  FIO_CLUSTER_MSG_PUBSUB_SUB,
  FIO_CLUSTER_MSG_PUBSUB_UNSUB,
  FIO_CLUSTER_MSG_PATTERN_SUB,
  FIO_CLUSTER_MSG_PATTERN_UNSUB,
  FIO_CLUSTER_MSG_SHUTDOWN,
  FIO_CLUSTER_MSG_ERROR,
  FIO_CLUSTER_MSG_PING,
} fio_cluster_message_type_e;

typedef struct fio_collection_s fio_collection_s;

#ifndef __clang__ /* clang might misbehave by assumming non-alignment */
#pragma pack(1)   /* https://gitter.im/halide/Halide/archives/2018/07/24 */
#endif
typedef struct {
  size_t name_len;
  char *name;
  volatile size_t ref;
  FIO_LIST_HEAD subscriptions;
  fio_collection_s *parent;
  fio_match_fn match;
  fio_lock_i lock;
} channel_s;
#ifndef __clang__
#pragma pack()
#endif

struct subscription_s {
  FIO_LIST_HEAD node;
  channel_s *parent;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata1, void *udata2);
  void *udata1;
  void *udata2;
  /** reference counter. */
  volatile uintptr_t ref;
  /** prevents the callback from running concurrently for multiple messages. */
  fio_lock_i lock;
  fio_lock_i unsubscribed;
};

/* Use `malloc` / `free`, because channles might have a long life. */

/** Used internally by the Set object to create a new channel. */
static channel_s *fio_channel_copy(channel_s *src) {
  channel_s *dest = malloc(sizeof(*dest) + src->name_len + 1);
  FIO_ASSERT_ALLOC(dest);
  dest->name_len = src->name_len;
  dest->match = src->match;
  dest->parent = src->parent;
  dest->name = (char *)(dest + 1);
  if (src->name_len)
    memcpy(dest->name, src->name, src->name_len);
  dest->name[src->name_len] = 0;
  FIO_LIST_INIT(dest->subscriptions);
  dest->ref = 1;
  dest->lock = FIO_LOCK_INIT;
  return dest;
}
/** Frees a channel (reference counting). */
static void fio_channel_free(channel_s *ch) {
  if (!ch)
    return;
  if (fio_atomic_sub(&ch->ref, 1))
    return;
  free(ch);
}
/** Increases a channel's reference count. */
FIO_SFUNC void fio_channel_dup(channel_s *ch) {
  if (!ch)
    return;
  fio_atomic_add(&ch->ref, 1);
}
/** Tests if two channels are equal. */
static int fio_channel_cmp(channel_s *ch1, channel_s *ch2) {
  return ch1->name_len == ch2->name_len && ch1->match == ch2->match &&
         !memcmp(ch1->name, ch2->name, ch1->name_len);
}
/* pub/sub channels and core data sets have a long life, so avoid fio_malloc */
#define FIO_FORCE_MALLOC_TMP         1
#define FIO_MAP_NAME                 fio_ch_set
#define FIO_MAP_TYPE                 channel_s *
#define FIO_MAP_TYPE_CMP(o1, o2)     fio_channel_cmp((o1), (o2))
#define FIO_MAP_TYPE_DESTROY(obj)    fio_channel_free((obj))
#define FIO_MAP_TYPE_COPY(dest, src) ((dest) = fio_channel_copy((src)))
#include <fio-stl.h>

#define FIO_FORCE_MALLOC_TMP 1
#define FIO_ARRAY_NAME       fio_meta_ary
#define FIO_ARRAY_TYPE       fio_msg_metadata_fn
#include <fio-stl.h>

#define FIO_FORCE_MALLOC_TMP     1
#define FIO_MAP_NAME             fio_engine_set
#define FIO_MAP_TYPE             fio_pubsub_engine_s *
#define FIO_MAP_TYPE_CMP(k1, k2) ((k1) == (k2))
#include <fio-stl.h>

struct fio_collection_s {
  fio_ch_set_s channels;
  fio_lock_i lock;
};

#define COLLECTION_INIT                                                        \
  { .channels = FIO_MAP_INIT, .lock = FIO_LOCK_INIT }

static struct {
  fio_collection_s filters;
  fio_collection_s pubsub;
  fio_collection_s patterns;
  struct {
    fio_engine_set_s set;
    fio_lock_i lock;
  } engines;
  struct {
    fio_meta_ary_s ary;
    fio_lock_i lock;
  } meta;
} fio_postoffice = {
    .filters = COLLECTION_INIT,
    .pubsub = COLLECTION_INIT,
    .patterns = COLLECTION_INIT,
    .engines.lock = FIO_LOCK_INIT,
    .meta.lock = FIO_LOCK_INIT,
};

/** used to contain the message before it's passed to the handler */
typedef struct {
  fio_msg_s msg;
  size_t marker;
  size_t meta_len;
  fio_msg_metadata_s *meta;
} fio_msg_client_s;

/** used to contain the message internally while publishing */
typedef struct {
  fio_str_info_s channel;
  fio_str_info_s data;
  uintptr_t ref; /* internal reference counter */
  int32_t filter;
  int8_t is_json;
  size_t meta_len;
  fio_msg_metadata_s meta[];
} fio_msg_internal_s;

/** The default engine (settable). */
fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;
/* *****************************************************************************
Cluster forking handler
***************************************************************************** */

static void fio_pubsub_on_fork(void) {
  fio_postoffice.filters.lock = FIO_LOCK_INIT;
  fio_postoffice.pubsub.lock = FIO_LOCK_INIT;
  fio_postoffice.patterns.lock = FIO_LOCK_INIT;
  fio_postoffice.engines.lock = FIO_LOCK_INIT;
  fio_postoffice.meta.lock = FIO_LOCK_INIT;
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.filters.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.pubsub.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
}

/* *****************************************************************************
Test state callbacks
***************************************************************************** */
#ifdef TEST

/* State callback tests */
FIO_SFUNC void FIO_NAME_TEST(io, postoffice)(void) {
  fprintf(stderr, "* testing pub/sub postoffice.\n");
  fprintf(stderr, "TODO.\n");
  /* TODO */
}
#endif /* TEST */
