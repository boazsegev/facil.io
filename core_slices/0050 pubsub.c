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












The postoffice publishes letters to "subscribers" ...

The design divides three classes of subscribers:

- Filter subscribers: accept any message that matches a numeral value.
- Names subscribers: accept any message who's channel name is an exact match.
- Pattern subscribers: accept any message who's channel name matches a pattern.

Messages are published to a numeral channel (filter) OR a named channel.

The postoffice forwards all messages to the appropriate subscribers.

IPC (inter process communication):
=====

Worker processes connect to the Master process and subscribe to messages. Those
messages are then sent to the child processes.

The Master process does NOT subscribe with the child processes. Instead, all
published messages (that aren't limited to the single  process) are sent to the
master process.
***************************************************************************** */

/* *****************************************************************************
Cluster IO message types (letter types)
***************************************************************************** */

/* Info byte bits:
 * [1-4] pub/sub/unsub and error message types.
 * [4] filter vs. named.
 * [5-6] fowarding type
 * [7] always zero.
 * [8] JSON marker.
 */

typedef enum fio_cluster_message_type_e {
  /* message type, 4th bit == filter */
  FIO_CLUSTER_MSG_PUB_NAME = 0x00,      /* 0b0000 */
  FIO_CLUSTER_MSG_PUB_FILTER = 0x04,    /* 0b0100 */
  FIO_CLUSTER_MSG_SUB_NAME = 0x01,      /* 0b0001 */
  FIO_CLUSTER_MSG_SUB_PATTERN = 0x02,   /* 0b0010 */
  FIO_CLUSTER_MSG_SUB_FILTER = 0x05,    /* 0b0101 */
  FIO_CLUSTER_MSG_UNSUB_NAME = 0x03,    /* 0b0011 */
  FIO_CLUSTER_MSG_UNSUB_PATTERN = 0x08, /* 0b1000 */
  FIO_CLUSTER_MSG_UNSUB_FILTER = 0x06,  /* 0b0110 */
  /* error message type, bits 2-4 of info */
  FIO_CLUSTER_MSG_ERROR = 0x09, /* 0b1001 */
  FIO_CLUSTER_MSG_PING = 0x0A,  /* 0b1010 */
  FIO_CLUSTER_MSG_PONG = 0x0B,  /* 0b1011 */
} fio_cluster_message_type_e;

typedef enum fio_cluster_message_forwarding_e {
  /* (u)subscibing message type, bits 5-6 of info */
  FIO_CLUSTER_MSG_FORARDING_NONE = 0x00,
  FIO_CLUSTER_MSG_FORARDING_GLOBAL = 0x20, /* 0b010000 */
  FIO_CLUSTER_MSG_FORARDING_LOCAL = 0x40   /* 0b100000 */
} fio_cluster_message_forwarding_e;

typedef struct channel_s channel_s;

typedef struct fio_collection_s fio_collection_s;

/** The default engine (settable). */
fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;

/* *****************************************************************************
Cluster IO Registry
***************************************************************************** */

#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_OMAP_NAME          fio_cluster_uuid
#define FIO_MAP_TYPE           intptr_t
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))
#include "fio-stl.h"

static fio_lock_i fio_cluster_registry_lock;
static fio_cluster_uuid_s fio_cluster_registry;

/* Unix Socket name */
static fio_str_s fio___cluster_name = FIO_STR_INIT;

/* *****************************************************************************
Slow memory allocator, for channel names and pub/sub registry.

Allows the use of the following MACROs:
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
***************************************************************************** */

#define FIO_MEMORY_NAME                      fio___slow
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG   21 /* 2Mb */
#define FIO_MEMORY_CACHE_SLOTS               2 /* small system allocation cache */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 3 /* 0.25Mb allocation blocks */
#define FIO_MEMORY_ENABLE_BIG_ALLOC          1 /* we might need this */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS    0 /* nothing to protect */
#define FIO_MEMORY_ARENA_COUNT               1 /* not musch contention, really */
#include "fio-stl.h"

/* *****************************************************************************
 * Glob Matching
 **************************************************************************** */
uint8_t (*FIO_PUBSUB_PATTERN_MATCH)(fio_str_info_s,
                                    fio_str_info_s) = fio_glob_match;

/* *****************************************************************************
Postoffice message format (letter) - internal API
***************************************************************************** */

/** 8 bit type, 24 bit header length, 32 bit body length */
#define FIO___LETTER_INFO_BYTES 8

struct fio_letter_s {
  /* Note: reference counting and other data is stored before the object */
  /** protocol header - must be first */
  uint8_t info[FIO___LETTER_INFO_BYTES];
  /** message buffer */
  char buf[];
};

typedef struct {
  intptr_t from;
  void *meta[FIO_PUBSUB_METADATA_LIMIT];
} fio_letter_metadata_s;

typedef struct {
  fio_letter_s *l;
  fio_msg_s m;
  volatile intptr_t marker;
} fio_msg_wrapper_s;

/** Creates a new fio_letter_s object. */
FIO_IFUNC fio_letter_s *fio_letter_new_buf(uint8_t *info);
/** Creates a new fio_letter_s object and copies the data into the object. */
FIO_IFUNC fio_letter_s *fio_letter_new_copy(uint8_t type,
                                            int32_t filter,
                                            fio_str_info_s header,
                                            fio_str_info_s body);

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *letter);
/** Returns all information about the fio_letter_s object. */
fio_letter_info_s fio_letter_info(fio_letter_s *letter);
/** Returns a letter's metadata object */
FIO_IFUNC fio_letter_metadata_s *fio_letter_metadata(fio_letter_s *l);

/** helpers. */
/* Returns the fio_letter_s message type according to its info field */
FIO_IFUNC fio_cluster_message_type_e fio__letter2type(uint8_t *info);
/* Returns the fio_letter_s forwarding type according to its info field */
FIO_IFUNC fio_cluster_message_forwarding_e fio__letter2fwd(uint8_t *info);
/* Returns the fio_letter_s header according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2header(uint8_t *info);
/* Returns the fio_letter_s body according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2body(uint8_t *info);

/** small quick helpers. */
FIO_IFUNC uint32_t fio__letter_header_len(uint8_t *info);
FIO_IFUNC uint32_t fio__letter_body_len(uint8_t *info);
FIO_IFUNC fio_letter_s *fio___msg2letter(fio_msg_s *msg);
FIO_IFUNC uint8_t fio__letter_is_json(uint8_t *info);
FIO_IFUNC uint8_t fio__letter_is_filter(uint8_t *info);
FIO_IFUNC void fio__letter_set_json(uint8_t *info);

/** Writes the requested details to the  `info` object. Returns -1 on error. */
FIO_IFUNC int fio___letter_write_info(uint8_t *dest_info,
                                      uint8_t type,
                                      int32_t filter,
                                      uint32_t header_len,
                                      uint32_t body_len);

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
FIO_IFUNC void *fio_letter_msg_metadata(fio_letter_s *l, int id);
/* *****************************************************************************
Cluster - informing the master process about new chunnels
***************************************************************************** */

/** sends a letter to all connected peers and frees the letter */
FIO_SFUNC void fio_cluster_send_letter_to_peers(fio_letter_s *l);
/** informs the master process of a new subscription channel */
FIO_SFUNC void fio___cluster_inform_master_sub(channel_s *ch);
/** informs the master process when a subscription channel is obselete */
FIO_SFUNC void fio___cluster_inform_master_unsub(channel_s *ch);

/* *****************************************************************************
Cluster Letter Publishing Functions
***************************************************************************** */

/* publishes a letter in the current process */
FIO_SFUNC void fio_letter_publish(fio_letter_s *l);

/* *****************************************************************************
Cluster IO Letter Processing Function(s)
***************************************************************************** */

/* Forwards the "letter" to the postofficce system*/
FIO_SFUNC void fio_letter_process_worker(fio_letter_s *l, intptr_t uuid_from);
/* Forwards the "letter" to the postofficce system*/
FIO_SFUNC void fio_letter_process_master(fio_letter_s *l, intptr_t uuid_from);

/* *****************************************************************************
Cluster IO Protocol - Declarations
***************************************************************************** */

typedef struct {
  fio_protocol_s pr;
  fio_letter_s *msg;
  int32_t consumed;
  char info[FIO___LETTER_INFO_BYTES];
} fio__cluster_pr_s;

/* Open a new (client) cluster coonection */
FIO_SFUNC void fio__cluster_connect(void *ignr_);

/* de-register new cluster connections */
FIO_SFUNC void fio___cluster_on_close(intptr_t, fio_protocol_s *);

/* logs connection readiness only once - (debug) level logging. */
FIO_SFUNC void fio___cluster_on_ready(intptr_t, fio_protocol_s *);

/* handle streamed data */
FIO_IFUNC void fio___cluster_on_data_internal(intptr_t,
                                              fio_protocol_s *,
                                              void (*handler)(fio_letter_s *,
                                                              intptr_t));
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_master(intptr_t, fio_protocol_s *);
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_worker(intptr_t, fio_protocol_s *);

/* performs a `ping`. */
FIO_SFUNC void fio___cluster_on_timeout(intptr_t, fio_protocol_s *);

/**
 * Sends a set of messages (letters) with all the subscriptions in the process.
 *
 * Called by fio__cluster_connect.
 */
FIO_IFUNC void fio__cluster___send_all_subscriptions(intptr_t uuid);

/* *****************************************************************************
Cluster IO Listening Protocol - Declarations
***************************************************************************** */

/* starts listening (Pre-Start task) */
FIO_SFUNC void fio___cluster_listen(void *ignr_);

/* accept and register new cluster connections */
FIO_SFUNC void fio___cluster_listen_accept(intptr_t srv, fio_protocol_s *pr);

/* delete the uunix socket file until restarting */
FIO_SFUNC void fio___cluster_listen_on_close(intptr_t uuid, fio_protocol_s *pr);

FIO_SFUNC fio_protocol_s fio__cluster_listen_protocol = {
    .on_data = fio___cluster_listen_accept,
    .on_close = fio___cluster_listen_on_close,
    .on_timeout = FIO_PING_ETERNAL,
};

/* *****************************************************************************
 * Postoffice types - Channel / Subscriptions data
 **************************************************************************** */

#ifndef __clang__ /* clang might misbehave by assumming non-alignment */
#pragma pack(1)   /* https://gitter.im/halide/Halide/archives/2018/07/24 */
#endif
struct channel_s {
  size_t name_len;
  char *name;
  FIO_LIST_HEAD subscriptions;
  fio_collection_s *parent;
  fio_lock_i lock;
};
#ifndef __clang__
#pragma pack()
#endif

/* reference count wrappers for channels - slower lifetimes... */
#define FIO_REF_NAME      channel
#define FIO_REF_FLEX_TYPE char
#define FIO_REF_CONSTRUCTOR_ONLY
/* use the slower local allocator */
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
#include <fio-stl.h>

struct subscription_s {
  FIO_LIST_HEAD node;
  channel_s *parent;
  intptr_t uuid;
  void (*on_message)(fio_msg_s *msg);
  void (*on_unsubscribe)(void *udata1, void *udata2);
  void *udata1;
  void *udata2;
  /** prevents the callback from running concurrently for multiple messages. */
  fio_lock_i lock;
  fio_lock_i unsubscribed;
};

/* reference count wrappers for subscriptions - fio_malloc (connection life) */
#define FIO_REF_NAME subscription
#define FIO_REF_DESTROY(obj)                                                   \
  do {                                                                         \
    fio_defer((obj).on_unsubscribe, (obj).udata1, (obj).udata2);               \
    channel_free((obj).parent);                                                \
  } while (0)
#define FIO_REF_CONSTRUCTOR_ONLY
#include <fio-stl.h>

/* Use `malloc` / `free`, because channles might have a long life. */

/** Used internally by the Set object to create a new channel. */
static channel_s *channel_copy(channel_s *src) {
  channel_s *dest = channel_new(src->name_len + 1);
  FIO_ASSERT_ALLOC(dest);
  dest->name_len = src->name_len;
  dest->parent = src->parent;
  dest->name = (char *)(dest + 1);
  if (src->name_len)
    memcpy(dest->name, src->name, src->name_len);
  dest->name[src->name_len] = 0;
  FIO_LIST_INIT(dest->subscriptions);
  dest->lock = FIO_LOCK_INIT;
  fio___cluster_inform_master_sub(dest);
  return dest;
}

/** Tests if two channels are equal. */
static int channel_cmp(channel_s *ch1, channel_s *ch2) {
  return ch1->name_len == ch2->name_len &&
         !memcmp(ch1->name, ch2->name, ch1->name_len);
}
/* pub/sub channels and core data sets have a long life, so avoid fio_malloc */
#define FIO_UMAP_NAME            fio_ch_set
#define FIO_MAP_TYPE             channel_s *
#define FIO_MAP_TYPE_CMP(o1, o2) channel_cmp((o1), (o2))
#define FIO_MAP_TYPE_DESTROY(obj)                                              \
  do {                                                                         \
    fio___cluster_inform_master_unsub((obj));                                  \
    channel_free((obj));                                                       \
  } while (0)
#define FIO_MAP_TYPE_COPY(dest, src) ((dest) = channel_copy((src)))
/* use the slower local allocator */
#define FIO_MEM_REALLOC_(p, osz, nsz, cl) fio___slow_realloc2((p), (nsz), (cl))
#define FIO_MEM_FREE_(p, sz)              fio___slow_free((p))
#define FIO_MEM_REALLOC_IS_SAFE_          fio___slow_realloc_is_safe()
#include <fio-stl.h>

/* engine sets are likely to remain static for the lifetime of the process */
#define FIO_OMAP_NAME            fio_engine_set
#define FIO_MAP_TYPE             fio_pubsub_engine_s *
#define FIO_MAP_TYPE_CMP(k1, k2) ((k1) == (k2))
/* use the system allocator for stuff that stays forever */
#define FIO_MALLOC_TMP_USE_SYSTEM
#include <fio-stl.h>

struct fio_collection_s {
  fio_ch_set_s channels;
  fio_lock_i lock;
};

#define COLLECTION_INIT                                                        \
  { .channels = FIO_MAP_INIT, .lock = FIO_LOCK_INIT }

static struct {
  fio_collection_s filters;
  fio_collection_s named;
  fio_collection_s patterns;
  struct {
    fio_engine_set_s set;
    fio_lock_i lock;
  } engines;
  struct {
    struct {
      intptr_t ref;
      fio_msg_metadata_fn builder;
      void (*cleanup)(void *);
    } callbacks[FIO_PUBSUB_METADATA_LIMIT];
  } meta;
} fio_postoffice = {
    .filters = COLLECTION_INIT,
    .named = COLLECTION_INIT,
    .patterns = COLLECTION_INIT,
    .engines.lock = FIO_LOCK_INIT,
};

/* *****************************************************************************
 * Postoffice types - message metadata
 **************************************************************************** */

static void *fio___msg_metadata_fn_noop(fio_str_info_s ch,
                                        fio_str_info_s msg,
                                        uint8_t is_json) {
  return NULL;
  (void)ch;
  (void)msg;
  (void)is_json;
}

static void fio___msg_metadata_fn_cleanup_noop(void *ig_) {
  return;
  (void)ig_;
}

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
void *fio_message_metadata(fio_msg_s *msg, int id) {
  if (!msg)
    return NULL;
  return fio_letter_msg_metadata(fio___msg2letter(msg), id);
}

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
                             void (*cleanup)(void *)) {
  int id = 0;
  int first = FIO_PUBSUB_METADATA_LIMIT;

  if (!builder && !cleanup)
    goto no_room;
  if (!builder)
    builder = fio___msg_metadata_fn_noop;
  if (!cleanup)
    cleanup = fio___msg_metadata_fn_cleanup_noop;

  for (id = 0; id < FIO_PUBSUB_METADATA_LIMIT; ++id) {
    if (!fio_atomic_add(&fio_postoffice.meta.callbacks[id].ref, 1) &&
        first >= FIO_PUBSUB_METADATA_LIMIT) {
      first = id;
    } else if (fio_postoffice.meta.callbacks[id].builder == builder &&
               fio_postoffice.meta.callbacks[id].cleanup == cleanup) {
      break;
    } else {
      fio_atomic_sub(&fio_postoffice.meta.callbacks[id].ref, 1);
    }
  }
  if (first < FIO_PUBSUB_METADATA_LIMIT) {
    if (id >= FIO_PUBSUB_METADATA_LIMIT)
      id = first;
    else
      fio_atomic_sub(&fio_postoffice.meta.callbacks[first].ref, 1);
  }
  if (id < FIO_PUBSUB_METADATA_LIMIT) {
    fio_postoffice.meta.callbacks[id].builder = builder;
    fio_postoffice.meta.callbacks[id].cleanup = cleanup;
  }
  if (id < FIO_PUBSUB_METADATA_LIMIT)
    return (++id);
no_room:
  id = 0;
  return id;
}

void fio_message_metadata_remove(int id) {
  if ((--id) >= FIO_PUBSUB_METADATA_LIMIT)
    goto error;
  if (fio_atomic_sub_fetch(&fio_postoffice.meta.callbacks[id].ref, 1) < 0)
    goto error;
  fio_postoffice.meta.callbacks[id].builder = fio___msg_metadata_fn_noop;
  return;
error:
  if (id < FIO_PUBSUB_METADATA_LIMIT && id >= 0)
    fio_atomic_exchange(&fio_postoffice.meta.callbacks[id].ref, 0);
  FIO_LOG_ERROR(
      "fio_message_metadata_remove called for an invalied (freed?) ID %d",
      id + 1);
}

/* *****************************************************************************
Postoffice message format (letter) - Implementation
***************************************************************************** */

/* define new2, free2 and dup */
#define FIO_REF_NAME      fio_letter
#define FIO_REF_FLEX_TYPE char
#define FIO_REF_METADATA  fio_letter_metadata_s
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {                      \
      if (fio_postoffice.meta.callbacks[i].ref) {                              \
        fio_postoffice.meta.callbacks[i].cleanup((m).meta);                    \
        fio_atomic_sub(&fio_postoffice.meta.callbacks[i].ref, 1);              \
      }                                                                        \
    }                                                                          \
  } while (0)
#include <fio-stl.h>

/** Creates a new fio_letter_s object. */
FIO_IFUNC fio_letter_s *fio_letter_new_buf(uint8_t *info) {
  fio_letter_s *letter =
      fio_letter_new2(fio__letter_body_len(info) + fio__letter_is_filter(info) +
                      fio__letter_header_len(info) + 2);
  if (letter) {
    memcpy(letter->info, info, FIO___LETTER_INFO_BYTES * sizeof(uint8_t));
  }
  return letter;
}

/** Creates a new fio_letter_s object and copies the data into the object. */
FIO_IFUNC fio_letter_s *fio_letter_new_copy(uint8_t type,
                                            int32_t filter,
                                            fio_str_info_s header,
                                            fio_str_info_s body) {
  fio_letter_s *l = NULL;
  uint8_t info[FIO___LETTER_INFO_BYTES];
  if (fio___letter_write_info(info, type, filter, header.len, body.len))
    return l;
  l = fio_letter_new_buf(info);
  if (!l)
    return l;
  const size_t filter_offset = (!!filter) << 2;
  if (filter)
    fio_u2buf32_little(l->buf, (int32_t)filter);
  if (header.len)
    memcpy(l->buf + filter_offset, header.buf, header.len);
  l->buf[header.len + filter_offset] = 0;
  if (body.len)
    memcpy(l->buf + header.len + 1 + filter_offset, body.buf, body.len);
  l->buf[header.len + 1 + body.len + filter_offset] = 0;
  return l;
}

/** Increases the object reference count. */
fio_letter_s *fio_letter_dup(fio_letter_s *letter) {
  return fio_letter_dup2(letter);
}

/** Frees object when the reference count reaches zero. */
void fio_letter_free(fio_letter_s *letter) { fio_letter_free2(letter); }

/** Returns the total length of the letter. */
size_t fio_letter_len(fio_letter_s *l) {
  return (sizeof(*l) + fio__letter_header_len(l->info) +
          fio__letter_body_len(l->info) + 2) +
         fio__letter_is_filter(l->info);
}

/** Returns all information about the fio_letter_s object. */
fio_letter_info_s fio_letter_info(fio_letter_s *letter) {
  fio_letter_info_s r = {
      .header = fio__letter2header(letter->info),
      .body = fio__letter2body(letter->info),
  };
  return r;
}

/* Returns the fio_letter_s type according to its info field */
FIO_IFUNC fio_cluster_message_type_e fio__letter2type(uint8_t *info) {
  return (fio_cluster_message_type_e)(info[0] & 0x0F); /* bits 1-4 */
}

FIO_IFUNC fio_cluster_message_forwarding_e fio__letter2fwd(uint8_t *info) {
  return (fio_cluster_message_forwarding_e)(info[0] & 0x70); /* bits 5-7 */
}

FIO_IFUNC uint8_t fio__letter_is_json(uint8_t *info) { return info[0] >> 7; }

FIO_IFUNC uint8_t fio__letter_is_filter(uint8_t *info) { return (info[0] & 4); }

/* Returns the fio_letter_s type according to its info field */
FIO_IFUNC void fio__letter_set_json(uint8_t *info) { info[0] |= 128; }

/* Returns the fio_letter_s header according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2header(uint8_t *info) {
  fio_str_info_s r = {
      .len = fio__letter_header_len(info),
      .buf = (char *)(info + FIO___LETTER_INFO_BYTES) +
             fio__letter_is_filter(info),
  };
  return r;
}

/* Returns the fio_letter_s body according to its info field */
FIO_IFUNC fio_str_info_s fio__letter2body(uint8_t *info) {
  fio_str_info_s r = {
      .len = fio__letter_body_len(info),
      .buf = (char *)(info + FIO___LETTER_INFO_BYTES) + 1 +
             fio__letter_is_filter(info) + fio__letter_header_len(info),
  };
  return r;
}

FIO_IFUNC uint32_t fio__letter_header_len(uint8_t *info) {
  return (((uint32_t)info[1] << 16) | ((uint32_t)info[2] << 8) |
          ((uint32_t)info[3]));
}

FIO_IFUNC uint32_t fio__letter_body_len(uint8_t *info) {
  return fio_buf2u32_little((const void *)(info + 4));
}

FIO_IFUNC fio_letter_s *fio___msg2letter(fio_msg_s *msg) {
  return FIO_PTR_FROM_FIELD(fio_msg_wrapper_s, m, msg)->l;
}

/** Writes the requested details to the  `info` object. Returns -1 on error. */
FIO_IFUNC int fio___letter_write_info(uint8_t *info,
                                      uint8_t type,
                                      int32_t filter,
                                      uint32_t header_len,
                                      uint32_t body_len) {
  if ((header_len & ((~(uint32_t)0) << 24)) ||
      (body_len & ((~(uint32_t)0) << 30)))
    return -1;
  info[0] = type | (!!filter);
  info[1] = (header_len >> 16) & 0xFF;
  info[2] = (header_len >> 8) & 0xFF;
  info[3] = (header_len)&0xFF;
  fio_u2buf32_little(info + 4, body_len);
  return 0;
}

/**
 * Increases a message's reference count, returning the published "letter".
 */
fio_letter_s *fio_message_dup(fio_msg_s *msg) {
  if (!msg)
    return NULL;
  return fio_letter_dup2(fio___msg2letter(msg));
}

/**
 * Finds the message's metadata by it's ID. Returns the data or NULL.
 *
 * The ID is the value returned by fio_message_metadata_callback_set.
 *
 * Note: numeral channels don't have metadata attached.
 */
FIO_IFUNC void *fio_letter_msg_metadata(fio_letter_s *l, int id) {
  if (!l || !id || (unsigned int)id > FIO_PUBSUB_METADATA_LIMIT)
    return NULL;
  --id;
  return fio_letter_metadata(l)->meta[id];
}

/* *****************************************************************************
 * Postoffice types - Channel / Subscriptions accessors
 **************************************************************************** */

/**
 * This helper returns a temporary String with the subscription's channel (or a
 * string representing the filter).
 *
 * To keep the string beyond the lifetime of the subscription, copy the string.
 */
fio_str_info_s fio_subscription_channel(subscription_s *s) {
  fio_str_info_s i = {0};
  if (!s || !s->parent)
    return i;
  i.buf = s->parent->name;
  i.len = s->parent->name_len;
  return i;
}

/* *****************************************************************************
Creating Subscriptions
***************************************************************************** */

/* Sublime Text marker */
void fio_subscribe___(void);
subscription_s *fio_subscribe FIO_NOOP(subscribe_args_s args) {
  subscription_s *s = NULL;
  if (!args.on_message)
    goto error;
  s = subscription_new();
  *s = (subscription_s){
      .uuid = args.uuid,
      .on_message = args.on_message,
      .on_unsubscribe = args.on_unsubscribe,
      .udata1 = args.udata1,
      .udata2 = args.udata2,
      .lock = FIO_LOCK_INIT,
      .unsubscribed = FIO_LOCK_INIT,
  };
  fio_collection_s *t;
  char buf[4 + 1];

  if (args.filter) {
    t = &fio_postoffice.filters;
    args.channel.buf = buf;
    args.channel.len = sizeof(args.filter);
    fio_u2buf32_little(buf, args.filter);
  }
  if (args.is_pattern) {
    t = &fio_postoffice.patterns;
  } else {
    t = &fio_postoffice.named;
  }

  uint64_t hash = fio_risky_hash(args.channel.buf,
                                 args.channel.len,
                                 (uint64_t)(uintptr_t)t);
  channel_s *c, channel = {.name = args.channel.buf,
                           .name_len = args.channel.len,
                           .parent = t};
  fio_lock(&t->lock);
  c = fio_ch_set_set_if_missing(&t->channels, hash, &channel);
  s->parent = c;
  FIO_LIST_PUSH(&c->subscriptions, &s->node);
  channel_dup(c);
  fio_unlock(&t->lock);
  if (!args.uuid)
    return s;
  fio_uuid_env_set(args.uuid,
                   .type = -3 + !args.is_pattern,
                   .name = {.buf = c->name, .len = c->name_len},
                   .udata = s,
                   .on_close = (void (*)(void *))fio_unsubscribe,
                   .const_name = 1);

  s = NULL;
  return s;

error:
  fio_defer(args.on_unsubscribe, args.udata1, args.udata2);
  return s;
}

void fio_unsubscribe(subscription_s *s) {
  channel_s *c = s->parent;
  fio_trylock(&s->unsubscribed);
  fio_collection_s *t = c->parent;
  fio_lock(&c->lock);
  FIO_LIST_REMOVE(&s->node);
  if (c->subscriptions.next == &c->subscriptions) {
    uint64_t hash =
        fio_risky_hash(c->name, c->name_len, (uint64_t)(uintptr_t)t);
    fio_lock(&t->lock);
    fio_ch_set_remove(&t->channels, hash, c, NULL); /* calls channel_free */
    fio_unlock(&t->lock);
  }
  fio_unlock(&c->lock);
  subscription_free(s); /* may call channel_free for subscription's reference */
}

void fio_unsubscribe_uuid FIO_NOOP(subscribe_args_s args) {
  fio_uuid_env_remove(args.uuid,
                      .type = (-3 + !args.is_pattern),
                      .name = args.channel);
}

/* *****************************************************************************
Cluster Letter Publishing Functions
***************************************************************************** */

/**Defers the current callback, so it will be called again for the message. */
void fio_message_defer(fio_msg_s *msg) {
  fio_msg_wrapper_s *m = FIO_PTR_FROM_FIELD(fio_msg_wrapper_s, m, msg);
  m->marker = -1;
}

FIO_SFUNC int fio_letter_publish_task_perform_inner(subscription_s *s,
                                                    fio_letter_s *l,
                                                    uint32_t filter) {
  fio_protocol_s *pr = NULL;
  fio_msg_wrapper_s data = {.l = l};
  if (fio_trylock(&s->lock))
    goto reschedule;
  if (s->uuid && !(pr = protocol_try_lock(s->uuid, FIO_PR_LOCK_TASK))) {
    if (errno == EWOULDBLOCK)
      goto reschedule_locked;
    goto done;
  }
  data.m = (fio_msg_s){
      .filter = filter,
      .channel = fio__letter2header(l->info),
      .uuid = s->uuid,
      .pr = pr,
      .msg = fio__letter2body(l->info),
      .udata1 = s->udata1,
      .udata2 = s->udata2,
      .is_json = fio__letter_is_json(l->info),
  };
  s->on_message(&data.m);
  if (data.marker)
    goto reschedule_pr_locked;

done:
  if (pr)
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
  fio_unlock(&s->lock);
  fio_letter_free2(l);
  subscription_free(s);
  return 0;
reschedule_pr_locked:
  if (pr)
    protocol_unlock(pr, FIO_PR_LOCK_TASK);
reschedule_locked:
  fio_unlock(&s->lock);
reschedule:
  return -1;
}

FIO_SFUNC void fio_letter_publish_task_perform(void *s_, void *l_) {
  subscription_s *s = s_;
  fio_letter_s *l = l_;
  if (fio_letter_publish_task_perform_inner(s, l, 0))
    fio_defer(fio_letter_publish_task_perform, s_, l_);
}

FIO_SFUNC void fio_letter_publish_task_perform_filter(void *s_, void *l_) {
  subscription_s *s = s_;
  fio_letter_s *l = l_;
  uint32_t f = fio_buf2u32_little(l->buf);
  if (fio_letter_publish_task_perform_inner(s, l, f))
    fio_defer(fio_letter_publish_task_perform_filter, s_, l_);
}

FIO_SFUNC void fio_letter_publish_task_filter(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.filters.lock))
    goto reschedule;
  channel_s channel = {.name = l->buf, .name_len = 4};
  channel_s *ch = fio_ch_set_get(
      &fio_postoffice.filters.channels,
      fio_risky_hash(channel.name,
                     channel.name_len,
                     (uint64_t)(uintptr_t)&fio_postoffice.filters),
      &channel);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, i) {
      fio_defer(fio_letter_publish_task_perform_filter,
                subscription_dup(i),
                fio_letter_dup2(l));
    }
  }
  fio_unlock(&fio_postoffice.filters.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_filter, l_, ignr_);
}

FIO_SFUNC void fio_letter_publish_task_named(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.named.lock))
    goto reschedule;
  channel_s channel = {.name = fio__letter2header(l->info).buf,
                       .name_len = fio__letter2header(l->info).len};
  channel_s *ch =
      fio_ch_set_get(&fio_postoffice.named.channels,
                     fio_risky_hash(channel.name,
                                    channel.name_len,
                                    (uint64_t)(uintptr_t)&fio_postoffice.named),
                     &channel);
  if (ch) {
    FIO_LIST_EACH(subscription_s, node, &ch->subscriptions, i) {
      fio_defer(fio_letter_publish_task_perform,
                subscription_dup(i),
                fio_letter_dup2(l));
    }
  }
  fio_unlock(&fio_postoffice.named.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_named, l_, ignr_);
}

FIO_SFUNC void fio_letter_publish_task_pattern(void *l_, void *ignr_) {
  fio_letter_s *l = l_;
  if (fio_trylock(&fio_postoffice.patterns.lock))
    goto reschedule;
  fio_str_info_s ch_name = fio__letter2header(l->info);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, pos) {
    // pos->obj->name
    fio_str_info_s pat = {.buf = pos->obj->name, .len = pos->obj->name_len};
    if (FIO_PUBSUB_PATTERN_MATCH(pat, ch_name)) {
      FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, i) {
        fio_defer(fio_letter_publish_task_perform,
                  subscription_dup(i),
                  fio_letter_dup2(l));
      }
    }
  }
  fio_unlock(&fio_postoffice.patterns.lock);
  fio_letter_free2(l);
  return;
reschedule:
  fio_defer(fio_letter_publish_task_pattern, l_, ignr_);
}

/* publishes a letter in the current process */
FIO_SFUNC void fio_letter_publish(fio_letter_s *l) {
  if (fio__letter_is_filter(l->info)) {
    fio_letter_publish_task_filter(fio_letter_dup2(l), NULL);
  } else {
    fio_letter_metadata_s *meta = fio_letter_metadata(l);
    register fio_str_info_s ch = fio__letter2header(l->info);
    register fio_str_info_s msg = fio__letter2body(l->info);
    register uint8_t is_json = fio__letter_is_json(l->info);
    for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {
      if (!fio_postoffice.meta.callbacks[i].ref)
        continue;
      if (!fio_atomic_add(&fio_postoffice.meta.callbacks[i].ref, 1)) {
        /* removed before we could request a reference to the object */
        fio_atomic_sub(&fio_postoffice.meta.callbacks[i].ref, 1);
        continue;
      }
      meta->meta[i] =
          fio_postoffice.meta.callbacks[i].builder(ch, msg, is_json);
    }
    fio_letter_publish_task_named(fio_letter_dup2(l), NULL);
    fio_letter_publish_task_pattern(fio_letter_dup2(l), NULL);
  }
}

/* *****************************************************************************
Cluster Letter Publishing API
***************************************************************************** */

// typedef struct fio_publish_args_s {
// fio_pubsub_engine_s const *engine;
// int32_t filter;
// fio_str_info_s channel;
// fio_str_info_s message;
// uint8_t is_json;
// } fio_publish_args_s;

/* Sublime Text Marker*/
void fio_publish___(void);
/**
 * Publishes a message to the relevant subscribers (if any).
 */
void fio_publish FIO_NOOP(fio_publish_args_s args) {
  if (!args.engine)
    args.engine = FIO_PUBSUB_DEFAULT;
  if (args.filter && (uintptr_t)args.engine >= 7)
    args.engine = FIO_PUBSUB_LOCAL;
  uint8_t type =
      args.filter ? FIO_CLUSTER_MSG_PUB_FILTER : FIO_CLUSTER_MSG_PUB_NAME;
  fio_letter_s *l = NULL;
  if ((uintptr_t)args.engine <= 7) {
    l = fio_letter_new_copy(type, args.filter, args.channel, args.message);
    FIO_ASSERT_ALLOC(l);
    if (args.is_json)
      fio__letter_set_json(l->info);
  }

  switch ((uintptr_t)args.engine) {
  case 1: /* FIO_PUBSUB_CLUSTER */
    /** Used to publish the message to all clients in the cluster. */
    fio_letter_metadata(l)->from = -1; /* subscribed connections will forward */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_GLOBAL;
    if (!fio_data->is_master)
      fio_cluster_send_letter_to_peers(fio_letter_dup2(l));
    break;

  case 2: /* TODO: FIO_PUBSUB_LOCAL */
    /** Used to publish the message to all clients in the local cluster. */
    fio_letter_metadata(l)->from = -1; /* subscribed connections will forward */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_LOCAL;
    if (!fio_data->is_master)
      fio_cluster_send_letter_to_peers(fio_letter_dup2(l));
    break;

  case 3: /* FIO_PUBSUB_SIBLINGS */
    /** Used to publish the message except within the current process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_LOCAL;
    fio_cluster_send_letter_to_peers(l); /* don't process the letter */
    return;

  case 4: /* FIO_PUBSUB_PROCESS */
    /** Used to publish the message only within the current process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_NONE;
    break;

  case 5: /* FIO_PUBSUB_ROOT */
    /** Used to publish the message exclusively to the root / master process. */
    l->info[0] |= FIO_CLUSTER_MSG_FORARDING_NONE;
    if (!fio_data->is_master) {
      fio_cluster_send_letter_to_peers(l); /* don't process the letter */
      return;
    }
    break;
  default:
    if (!args.filter)
      args.engine->publish(args.engine,
                           args.channel,
                           args.message,
                           args.is_json);
    return;
  }
  fio_letter_publish(l);
  fio_letter_free2(l);
}

/* *****************************************************************************
Cluster - informing the master process about new chunnels
***************************************************************************** */

FIO_SFUNC void fio_cluster_send_letter_to_peers(fio_letter_s *l) {
  fio_lock(&fio_cluster_registry_lock);
  FIO_MAP_EACH(fio_cluster_uuid, &fio_cluster_registry, i) {
    fio_write2(i->obj,
               .data.buf = fio_letter_dup2(l),
               .len = fio_letter_len(l),
               .dealloc = (void (*)(void *))fio_letter_free);
  }
  fio_unlock(&fio_cluster_registry_lock);
  fio_letter_free(l);
}

/** informs the master process of a new subscription channel */
FIO_SFUNC void fio___cluster_inform_master_sub(channel_s *ch) {
  register fio_str_info_s ch_info = {.buf = ch->name, .len = ch->name_len};
  int32_t filter = 0;
  if (fio_data->is_master)
    goto inform_engines;
  uint8_t type;
  if (ch->parent == &fio_postoffice.named)
    type = FIO_CLUSTER_MSG_SUB_NAME;
  else if (ch->parent == &fio_postoffice.filters) {
    filter = fio_buf2u32_little(ch->name);
    ch_info.len = 0;
    type = FIO_CLUSTER_MSG_SUB_FILTER;
  } else
    type = FIO_CLUSTER_MSG_SUB_PATTERN;
  fio_letter_s *l =
      fio_letter_new_copy(type, filter, ch_info, (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_cluster_send_letter_to_peers(l);
inform_engines:
  if (ch->parent == &fio_postoffice.filters)
    return;
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(fio_engine_set, &fio_postoffice.engines.set, i) {
    i->obj->subscribe(i->obj, ch_info, ch->parent == &fio_postoffice.patterns);
  }
  fio_unlock(&fio_postoffice.engines.lock);
}
/** informs the master process when a subscription channel is obselete */
FIO_SFUNC void fio___cluster_inform_master_unsub(channel_s *ch) {
  register fio_str_info_s ch_info = {.buf = ch->name, .len = ch->name_len};
  int32_t filter = 0;
  if (fio_data->is_master)
    goto inform_engines;
  uint8_t type;
  if (ch->parent == &fio_postoffice.named)
    type = FIO_CLUSTER_MSG_UNSUB_NAME;
  else if (ch->parent == &fio_postoffice.filters) {
    filter = fio_buf2u32_little(ch->name);
    ch_info.len = 0;
    type = FIO_CLUSTER_MSG_UNSUB_FILTER;
  } else
    type = FIO_CLUSTER_MSG_UNSUB_PATTERN;
  fio_letter_s *l =
      fio_letter_new_copy(type, filter, ch_info, (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_cluster_send_letter_to_peers(l);
inform_engines:
  if (ch->parent == &fio_postoffice.filters)
    return;
  fio_lock(&fio_postoffice.engines.lock);
  FIO_MAP_EACH(fio_engine_set, &fio_postoffice.engines.set, i) {
    i->obj->unsubscribe(i->obj,
                        ch_info,
                        ch->parent == &fio_postoffice.patterns);
  }
  fio_unlock(&fio_postoffice.engines.lock);
}

/* *****************************************************************************
Cluster Letter Processing
***************************************************************************** */

FIO_SFUNC void fio___cluster_uuid_on_message_internal(fio_msg_s *msg) {
  fio_letter_s *l = fio___msg2letter(msg);
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  if (!meta->from || meta->from == msg->uuid)
    return;
  fio_write2(msg->uuid,
             .data.buf = fio_letter_dup2(l),
             .len = fio_letter_len(l),
             .dealloc = (void (*)(void *))fio_letter_free);
}

FIO_SFUNC void fio_letter_process_master(fio_letter_s *l, intptr_t from) {
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  switch (fio__letter2type(l->info)) {
  case FIO_CLUSTER_MSG_PUB_NAME: /* fallthrough */
  case FIO_CLUSTER_MSG_PUB_FILTER:
    if (fio__letter2fwd(l->info))
      meta->from = from;
    fio_letter_publish(l);
    break;
  case FIO_CLUSTER_MSG_SUB_FILTER:
    fio_subscribe(.uuid = from,
                  .filter = fio_buf2u32_little(l->buf),
                  .on_message = fio___cluster_uuid_on_message_internal);
    break;
  case FIO_CLUSTER_MSG_UNSUB_FILTER:
    fio_unsubscribe_uuid(.uuid = from, .filter = fio_buf2u32_little(l->buf));
    break;
  case FIO_CLUSTER_MSG_SUB_NAME:
    fio_subscribe(.uuid = from,
                  .channel = fio__letter2header(l->info),
                  .on_message = fio___cluster_uuid_on_message_internal);
    break;
  case FIO_CLUSTER_MSG_UNSUB_NAME:
    fio_unsubscribe_uuid(.uuid = from, .channel = fio__letter2header(l->info));
    break;
  case FIO_CLUSTER_MSG_SUB_PATTERN:
    fio_subscribe(.uuid = from,
                  .channel = fio__letter2header(l->info),
                  .on_message = fio___cluster_uuid_on_message_internal,
                  .is_pattern = 1);
    break;
  case FIO_CLUSTER_MSG_UNSUB_PATTERN:
    fio_unsubscribe_uuid(.uuid = from,
                         .channel = fio__letter2header(l->info),
                         .is_pattern = 1);
    break;
  case FIO_CLUSTER_MSG_ERROR:
    break;
  case FIO_CLUSTER_MSG_PING:
    break;
  case FIO_CLUSTER_MSG_PONG:
    break;
  }
  fio_letter_free2(l);
}
FIO_SFUNC void fio_letter_process_worker(fio_letter_s *l, intptr_t from) {
  fio_letter_metadata_s *meta = fio_letter_metadata(l);
  switch (fio__letter2type(l->info)) {
  case FIO_CLUSTER_MSG_PUB_FILTER: /* fallthrough */
  case FIO_CLUSTER_MSG_PUB_NAME:
    meta->from = from;
    fio_letter_publish(l);
    break;
  case FIO_CLUSTER_MSG_SUB_FILTER:    /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_FILTER:  /* fallthrough */
  case FIO_CLUSTER_MSG_SUB_NAME:      /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_NAME:    /* fallthrough */
  case FIO_CLUSTER_MSG_SUB_PATTERN:   /* fallthrough */
  case FIO_CLUSTER_MSG_UNSUB_PATTERN: /* fallthrough */
  case FIO_CLUSTER_MSG_ERROR:
    break;
  case FIO_CLUSTER_MSG_PING:
    break;
  case FIO_CLUSTER_MSG_PONG:
    break;
  }
  fio_letter_free2(l);
}

/* *****************************************************************************
Cluster IO Protocol
***************************************************************************** */

/* Open a new (client) cluster coonection */
FIO_SFUNC void fio__cluster_connect(void *ignr_) {
  (void)ignr_;
  FIO_LOG_DEBUG2("(%d) connecting to cluster.", fio_getpid());
  int fd = fio_sock_open(fio_str2ptr(&fio___cluster_name),
                         NULL,
                         FIO_SOCK_NONBLOCK | FIO_SOCK_UNIX | FIO_SOCK_CLIENT);
  if (fd == -1) {
    FIO_LOG_FATAL("(%d) worker process couldn't connect to master process",
                  fio_getpid());
    kill(0, SIGINT);
  }
  fio__cluster_pr_s *pr = malloc(sizeof(*pr));
  if (!pr) {
    FIO_LOG_FATAL("(%d) worker process couldn't connect to master process",
                  fio_getpid());
    kill(0, SIGINT);
  }
  *pr = (fio__cluster_pr_s){
      .pr =
          {
              .on_data = fio___cluster_on_data_worker,
              .on_ready = fio___cluster_on_ready,
#if !FIO_DISABLE_HOT_RESTART
              .on_shutdown = mock_on_shutdown_eternal,
#endif
              .on_close = fio___cluster_on_close,
              .on_timeout = fio___cluster_on_timeout,
          },
  };
  /* attach pub/sub protocol for incoming messages */
  fio_attach_fd(fd, &pr->pr);
  /* add to registry, making publishing available */
  intptr_t uuid = fd2uuid(fd);
  if (uuid != -1) {
    fio_lock(&fio_cluster_registry_lock);
    fio_cluster_uuid_set(&fio_cluster_registry,
                         fio_risky_hash(&uuid, sizeof(uuid), 0),
                         uuid,
                         NULL);
    fio_unlock(&fio_cluster_registry_lock);
  }
}

/* de-register new cluster connections */
FIO_SFUNC void fio___cluster_on_close(intptr_t uuid, fio_protocol_s *pr) {
  fio_lock(&fio_cluster_registry_lock);
  fio_cluster_uuid_remove(&fio_cluster_registry,
                          fio_risky_hash(&uuid, sizeof(uuid), 0),
                          uuid,
                          NULL);
  fio_unlock(&fio_cluster_registry_lock);
  if (((fio__cluster_pr_s *)pr)->msg) {
    fio_letter_free2(((fio__cluster_pr_s *)pr)->msg);
  }
  free(pr);
  if (pr->on_data == fio___cluster_on_data_worker && fio_data->active) {
    FIO_LOG_ERROR("(%d) lost cluster connection - master crushed?",
                  fio_getpid());
    fio_stop();
    kill(0, SIGINT);
  }
}

/* logs connection subsribe exiting only once + (debug) level logging. */
FIO_SFUNC void fio___cluster_on_ready(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  protocol->on_ready = mock_on_ev;
  fio__cluster___send_all_subscriptions(uuid);
  FIO_LOG_DEBUG2("(%d) worker process connected to cluster socket",
                 fio_getpid());
}

/* performs a `ping`. */
FIO_SFUNC void fio___cluster_on_timeout(intptr_t uuid,
                                        fio_protocol_s *protocol) {
  // FIO_CLUSTER_MSG_PING
  fio_letter_s *l = fio_letter_new_copy(FIO_CLUSTER_MSG_PING,
                                        0,
                                        (fio_str_info_s){0},
                                        (fio_str_info_s){0});
  FIO_ASSERT_ALLOC(l);
  fio_write2(uuid,
             .data.buf = l,
             .len = fio_letter_len(l),
             .dealloc = (void (*)(void *))fio_letter_free);
  (void)uuid;
  (void)protocol;
}

/* handle streamed data */
FIO_IFUNC void fio___cluster_on_data_internal(intptr_t uuid,
                                              fio_protocol_s *protocol,
                                              void (*handler)(fio_letter_s *,
                                                              intptr_t)) {
  fio__cluster_pr_s *p = (fio__cluster_pr_s *)protocol;
  ssize_t r = 0;
  if (p->consumed <= 0) {
    uint8_t buf[8192];
    if (p->consumed) {
      r = 0 - p->consumed;
      memcpy(buf, p->info, r);
      r = fio_read(uuid, buf + r, 8192 - r);
    } else {
      r = fio_read(uuid, buf, 8192);
    }
    uint8_t *pos = buf;
    /* we might have caught a number of messages... or only fragments */
    while (r > 0) {
      if (r < FIO___LETTER_INFO_BYTES) {
        /* header was broken */
        p->consumed = 0 - r;
        memcpy(p->info, pos, r);
        fio_force_event(uuid, FIO_EVENT_ON_DATA);
        return;
      }
      fio_letter_s *l = fio_letter_new_buf((uint8_t *)pos);
      uint32_t expect = fio_letter_len(l);
      /* copy data to the protocol and wait for the rest to arrive */
      if (expect > (uint32_t)r) {
        p->consumed = expect;
        /* (re)copy the info bytes?, is math really faster than 8 bytes? */
        if (r > FIO___LETTER_INFO_BYTES)
          memcpy(l->buf,
                 (pos + FIO___LETTER_INFO_BYTES),
                 (r - FIO___LETTER_INFO_BYTES));
        p->msg = l;
        break;
      }
      if (expect > FIO___LETTER_INFO_BYTES)
        memcpy(l->buf,
               (pos + FIO___LETTER_INFO_BYTES),
               (expect - FIO___LETTER_INFO_BYTES));
      handler(l, uuid);
      r -= expect;
      pos += expect;
    }
  }
  if (p->msg) {
    int64_t expect = fio_letter_len(p->msg);
    while (p->consumed < expect && (r = fio_read(uuid,
                                                 p->msg->buf + p->consumed,
                                                 expect - p->consumed)) > 0) {
      p->consumed += r;
      if (p->consumed < expect)
        continue;
      handler(p->msg, uuid);
      p->consumed = 0;
      p->msg = NULL;
    }
  }
}
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_master(intptr_t uuid, fio_protocol_s *pr) {
  fio___cluster_on_data_internal(uuid, pr, fio_letter_process_master);
}
/* handle streamed data */
FIO_SFUNC void fio___cluster_on_data_worker(intptr_t uuid, fio_protocol_s *pr) {
  fio___cluster_on_data_internal(uuid, pr, fio_letter_process_worker);
}

/**
 * Sends a set of messages (letters) with all the subscriptions in the process.
 *
 * Called by fio__cluster_connect.
 */
FIO_IFUNC void fio__cluster___send_all_subscriptions(intptr_t uuid) {
  fio_lock(&fio_postoffice.filters.lock);
  fio_lock(&fio_postoffice.named.lock);
  fio_lock(&fio_postoffice.patterns.lock);

  /* calculate required buffer length */
  uint8_t *buf = NULL;
  uint8_t *pos = NULL;
  size_t len = (FIO___LETTER_INFO_BYTES + 2 + 4) *
               fio_ch_set_count(&fio_postoffice.filters.channels);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    len += FIO___LETTER_INFO_BYTES + 2 + i->obj->name_len;
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, i) {
    len += FIO___LETTER_INFO_BYTES + 2 + i->obj->name_len;
  }

  /* allocate memory */
  pos = buf = fio_malloc(len);
  FIO_ASSERT_ALLOC(buf);

  /* write all messages to the buffer as one long stream of data */
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.filters.channels, i) {
    fio___letter_write_info(pos, FIO_CLUSTER_MSG_SUB_FILTER, 1, 0, 0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, 4);
    pos[FIO___LETTER_INFO_BYTES + 4] = 0;
    pos += FIO___LETTER_INFO_BYTES + 4 + 2;
  }

  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    fio___letter_write_info(pos,
                            FIO_CLUSTER_MSG_SUB_NAME,
                            0,
                            i->obj->name_len,
                            0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, i->obj->name_len);
    pos[FIO___LETTER_INFO_BYTES + i->obj->name_len] = 0;
    pos += FIO___LETTER_INFO_BYTES + i->obj->name_len + 2;
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.patterns.channels, i) {
    fio___letter_write_info(pos,
                            FIO_CLUSTER_MSG_SUB_PATTERN,
                            0,
                            i->obj->name_len,
                            0);
    memcpy(pos + FIO___LETTER_INFO_BYTES, i->obj->name, i->obj->name_len);
    pos[FIO___LETTER_INFO_BYTES + i->obj->name_len] = 0;
    pos += FIO___LETTER_INFO_BYTES + i->obj->name_len + 2;
  }

  fio_unlock(&fio_postoffice.filters.lock);
  fio_unlock(&fio_postoffice.named.lock);
  fio_unlock(&fio_postoffice.patterns.lock);

  /* send data */
  fio_write2(uuid, .data.buf = buf, .len = len, .dealloc = fio_free);
}
/* *****************************************************************************
Cluster IO Listening Protocol
***************************************************************************** */

/* starts listening (Pre-Start task) */
FIO_SFUNC void fio___cluster_listen(void *ignr_) {
  int fd = fio_sock_open(fio_str2ptr(&fio___cluster_name),
                         NULL,
                         FIO_SOCK_NONBLOCK | FIO_SOCK_UNIX | FIO_SOCK_SERVER);
  FIO_ASSERT(fd != -1, "facil.io IPC failed to create Unix Socket");
  fio_attach_fd(fd, &fio__cluster_listen_protocol);
  FIO_LOG_DEBUG("(%d) cluster listening on %p@%s",
                fio_data->parent,
                (void *)fio_fd2uuid(fd),
                fio_str2ptr(&fio___cluster_name));
  (void)ignr_;
}

/* accept and register new cluster connections */
FIO_SFUNC intptr_t fio___cluster_listen_accept_uuid(intptr_t srv) {
  FIO_LOG_DEBUG2("(%d) cluster attempting to accept connection",
                 fio_data->parent,
                 fio_str2ptr(&fio___cluster_name));
  intptr_t uuid = fio_accept(srv);
  if (uuid == -1) {
    FIO_LOG_ERROR("(%d) cluster couldn't accept client connection! (%s)",
                  fio_data->parent,
                  strerror(errno));
    return uuid;
  }
  fio_lock(&fio_cluster_registry_lock);
  fio_cluster_uuid_set(&fio_cluster_registry,
                       fio_risky_hash(&uuid, sizeof(uuid), 0),
                       uuid,
                       NULL);
  fio_unlock(&fio_cluster_registry_lock);
  fio_timeout_set(uuid, 55); /* setup ping from master */
  return uuid;
}

/* accept and register new cluster connections */
FIO_SFUNC void fio___cluster_listen_accept(intptr_t srv, fio_protocol_s *p_) {
  intptr_t uuid = fio___cluster_listen_accept_uuid(srv);
  if (uuid == -1)
    return;

  fio__cluster_pr_s *pr = malloc(sizeof(*pr));
  *pr = (fio__cluster_pr_s){
      .pr =
          {
              .on_data = fio___cluster_on_data_master,
              .on_shutdown = mock_on_shutdown_eternal,
              .on_close = fio___cluster_on_close,
              .on_timeout = fio___cluster_on_timeout,
          },
  };
  fio_attach(uuid, &pr->pr);
  FIO_LOG_DEBUG2("(%d) cluster acccepted a new client connection",
                 fio_data->parent);
  (void)p_;
}

/* delete the unix socket file until restarting */
FIO_SFUNC void fio___cluster_listen_on_close(intptr_t uuid,
                                             fio_protocol_s *pr) {
  if (fio_is_master() && fio_str_len(&fio___cluster_name)) {
    FIO_LOG_DEBUG2("(%d) cluster deleteing listening socket.",
                   fio_data->parent);
    unlink(fio_str2ptr(&fio___cluster_name));
    fio_cluster_registry_lock = FIO_LOCK_INIT;
    fio_cluster_uuid_destroy(&fio_cluster_registry);
  }
  (void)uuid;
  (void)pr;
}

/* *****************************************************************************
Pub/Sub Engine Management
***************************************************************************** */

static void fio___engine_subscribe_noop(const fio_pubsub_engine_s *eng,
                                        fio_str_info_s channel,
                                        uint8_t is_pattern) {
  return;
  (void)eng;
  (void)channel;
  (void)is_pattern;
}
/** Should unsubscribe channel. Failures are ignored. */
static void fio___engine_unsubscribe_noop(const fio_pubsub_engine_s *eng,
                                          fio_str_info_s channel,
                                          uint8_t is_pattern) {
  return;
  (void)eng;
  (void)channel;
  (void)is_pattern;
}
/** Should publish a message through the engine. Failures are ignored. */
static void fio___engine_publish_noop(const fio_pubsub_engine_s *eng,
                                      fio_str_info_s channel,
                                      fio_str_info_s msg,
                                      uint8_t is_json) {
  return;
  (void)eng;
  (void)channel;
  (void)msg;
  (void)is_json;
}

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
void fio_pubsub_attach(fio_pubsub_engine_s *engine) {
  if (!engine->subscribe && !engine->unsubscribe && !!engine->publish)
    return;
  if (!engine->subscribe)
    engine->subscribe = fio___engine_subscribe_noop;
  if (!engine->unsubscribe)
    engine->unsubscribe = fio___engine_unsubscribe_noop;
  if (!engine->publish)
    engine->publish = fio___engine_publish_noop;
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_set_if_missing(&fio_postoffice.engines.set,
                                (uint64_t)(uintptr_t)engine,
                                engine);
  fio_unlock(&fio_postoffice.engines.lock);
  fio_pubsub_reattach(engine);
}

/** Detaches an engine, so it could be safely destroyed. */
void fio_pubsub_detach(fio_pubsub_engine_s *engine) {
  if (FIO_PUBSUB_DEFAULT == engine)
    FIO_PUBSUB_DEFAULT = FIO_PUBSUB_CLUSTER;
  fio_lock(&fio_postoffice.engines.lock);
  fio_engine_set_remove(&fio_postoffice.engines.set,
                        (uint64_t)(uintptr_t)engine,
                        engine,
                        NULL);
  fio_unlock(&fio_postoffice.engines.lock);
}

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
void fio_pubsub_reattach(fio_pubsub_engine_s *eng) {
  fio_lock(&fio_postoffice.named.lock);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    register fio_str_info_s c = {.buf = i->obj->name, .len = i->obj->name_len};
    eng->subscribe(eng, c, 0);
  }
  fio_unlock(&fio_postoffice.named.lock);
  fio_lock(&fio_postoffice.patterns.lock);
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, i) {
    register fio_str_info_s c = {.buf = i->obj->name, .len = i->obj->name_len};
    eng->subscribe(eng, c, 1);
  }
  fio_unlock(&fio_postoffice.patterns.lock);
}

/** Returns true (1) if the engine is attached to the system. */
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine) {
  fio_lock(&fio_postoffice.engines.lock);
  engine = fio_engine_set_get(&fio_postoffice.engines.set,
                              (uint64_t)(uintptr_t)engine,
                              engine);
  fio_unlock(&fio_postoffice.engines.lock);
  return engine != NULL;
}

/* *****************************************************************************
Remote Cluster Connections
***************************************************************************** */

/**
 * Broadcasts to the local machine on `port` in order to auto-detect and connect
 * to peers, creating a cluster that shares all pub/sub messages.
 *
 * Retruns -1 on error (i.e., not called from the root/master process).
 *
 * Returns 0 on success.
 */
int fio_pubsub_clusterfy(const char *port, fio_tls_s *tls);

/* *****************************************************************************
Cluster forking handler
***************************************************************************** */

FIO_SFUNC void fio_pubsub_on_fork(void *ignr_) {
  fio_cluster_registry_lock = FIO_LOCK_INIT;
  fio_postoffice.filters.lock = FIO_LOCK_INIT;
  fio_postoffice.named.lock = FIO_LOCK_INIT;
  fio_postoffice.patterns.lock = FIO_LOCK_INIT;
  fio_postoffice.engines.lock = FIO_LOCK_INIT;
  fio___slow_malloc_after_fork();
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.filters.channels, pos) {
    if (!pos->hash)
      continue;
    pos->obj->lock = FIO_LOCK_INIT;
    FIO_LIST_EACH(subscription_s, node, &pos->obj->subscriptions, n) {
      n->lock = FIO_LOCK_INIT;
    }
  }
  FIO_MAP_EACH(fio_ch_set, &fio_postoffice.named.channels, pos) {
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
  (void)ignr_;
}

/* *****************************************************************************
Cluster State and Initialization
***************************************************************************** */

FIO_IFUNC void fio_pubsub_cleanup(void *ignr_) {
  (void)ignr_;
  if (fio_is_master()) {
    unlink(fio_str2ptr(&fio___cluster_name));
  }
  free(fio_str2ptr(&fio___cluster_name));
  fio_str_destroy(&fio___cluster_name);
  fio_cluster_uuid_destroy(&fio_cluster_registry);
  fio_ch_set_destroy(&fio_postoffice.filters.channels);
  fio_ch_set_destroy(&fio_postoffice.named.channels);
  fio_ch_set_destroy(&fio_postoffice.patterns.channels);
  fio_engine_set_destroy(&fio_postoffice.engines.set);
}

FIO_IFUNC void fio_pubsub_init(void) {
/* Set up the Unix Socket name */
#ifdef P_tmpdir
  if (sizeof(P_tmpdir) > 2 && P_tmpdir[sizeof(P_tmpdir) - 2] == '/') {
    fio_str_write(&fio___cluster_name, P_tmpdir, sizeof(P_tmpdir) - 1);
  } else {
    fio_str_write(&fio___cluster_name, P_tmpdir, sizeof(P_tmpdir) - 1);
    fio_str_write(&fio___cluster_name, "/", 1);
  }
#else
  fio_str_write(&fio___cluster_name, "/tmp/", 5);
#endif
  fio_str_printf(&fio___cluster_name, "facil.io.ipc.%d.sock", fio_getpid());
  /* move memory to eternal storage (syatem malloc) */
  {
    size_t len = fio_str_len(&fio___cluster_name);
    void *tmp = malloc(len + 1);
    FIO_ASSERT_ALLOC(tmp);
    memcpy(tmp, fio_str2ptr(&fio___cluster_name), len + 1);
    fio_str_destroy(&fio___cluster_name);
    fio_str_init_const(&fio___cluster_name, tmp, len);
  }
  for (int i = 0; i < FIO_PUBSUB_METADATA_LIMIT; ++i) {
    fio_postoffice.meta.callbacks[i].builder = fio___msg_metadata_fn_noop;
    fio_postoffice.meta.callbacks[i].cleanup =
        fio___msg_metadata_fn_cleanup_noop;
  }
  /* set up callbacks */
  fio_state_callback_add(FIO_CALL_AT_EXIT, fio_pubsub_cleanup, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio_pubsub_on_fork, NULL);
  fio_state_callback_add(FIO_CALL_IN_CHILD, fio__cluster_connect, NULL);
  fio_state_callback_add(FIO_CALL_PRE_START, fio___cluster_listen, NULL);
}

/* *****************************************************************************
Test state callbacks
***************************************************************************** */
#ifdef TEST

FIO_SFUNC void FIO_NAME_TEST(io, postoffice_letter)(void) {
  int32_t filter = -42;
  fio_str_info_s ch = {.buf = "ch", .len = 2};
  fio_str_info_s body = {.buf = "body", .len = 4};
  fio_letter_s *l =
      fio_letter_new_copy(FIO_CLUSTER_MSG_PUB_FILTER, filter, ch, body);
  FIO_ASSERT_ALLOC(l);
  for (int rep = 0; rep < 4; ++rep) {
    FIO_ASSERT((fio__letter_is_filter(l->info) >> 2) == (rep < 2),
               "letter[%d] isn't properly marked as a filter letter",
               rep);
    FIO_ASSERT(!fio__letter_is_filter(l->info) ||
                   (int32_t)fio_buf2u32_little(l->buf) == filter,
               "letter[%d] filter value error (%d != %d)",
               rep,
               (int)fio_buf2u32_little(l->buf),
               (int)filter);
    FIO_ASSERT(fio_letter_info(l).header.len == ch.len,
               "letter[%d] header length error",
               rep);
    FIO_ASSERT(fio_letter_info(l).body.len == body.len,
               "letter[%d] body length error",
               rep);
    FIO_ASSERT(!memcmp(fio_letter_info(l).header.buf, ch.buf, ch.len),
               "letter[%d] header content error",
               rep);
    FIO_ASSERT(!memcmp(fio_letter_info(l).body.buf, body.buf, body.len),
               "letter[%d] body content error: %s != %s",
               rep,
               fio_letter_info(l).body.buf,
               body.buf);
    if (!(rep & 1)) {
      fio_letter_s *tmp = fio_letter_new_buf(l->info);
      FIO_ASSERT_ALLOC(tmp);
      FIO_ASSERT(fio_letter_len(tmp) == fio_letter_len(l),
                 "different letter lengths during copy");
      memcpy(tmp, l, fio_letter_len(tmp));
      fio_letter_free2(l);
      l = tmp;
    } else {
      fio_letter_free2(l);
      if (rep == 1) {
        filter = 0;
        l = fio_letter_new_copy(FIO_CLUSTER_MSG_PUB_NAME, filter, ch, body);
      }
    }
  }
}
/* State callback tests */
FIO_SFUNC void FIO_NAME_TEST(io, postoffice)(void) {
  fprintf(stderr, "* testing pub/sub postoffice.\n");
  FIO_NAME_TEST(io, postoffice_letter)();
  fprintf(stderr, "TODO.\n");
  /* TODO */
}
#endif /* TEST */
