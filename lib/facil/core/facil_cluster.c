/* *****************************************************************************
 * Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 **************************************************************************** */

#include "fio_mem.h"
#include "spnlock.inc"

// #include "facil_cluster.h"

#include "facil.h"

#include "fio_llist.h"
#include "fio_tmpfile.h"
#include "fiobj4sock.h"

#include <signal.h>

/* *****************************************************************************
 * Types
 **************************************************************************** */
#ifndef H_FACIL_CLUSTER_H
/** This contains message metadata, set by message extensions. */
typedef struct facil_msg_metadata_s {
  size_t type_id;
  struct facil_msg_metadata_s *next;
} facil_msg_metadata_s;

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct facil_msg_s {
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** A channel name, allowing for pub/sub patterns. */
  FIOBJ channel;
  /** The actual message. */
  FIOBJ msg;
  /** Metadata can be set by message extensions. */
  facil_msg_metadata_s *meta;
} facil_msg_s;

/**
 * Pattern matching callback type - should return 0 unless channel matches
 * pattern.
 */
typedef int (*facil_match_fn)(FIOBJ pattern, FIOBJ channel);

/**
 * Signals all workers to shutdown, which might invoke a respawning of the
 * workers unless the shutdown signal was received.
 *
 * NOT signal safe.
 */
void facil_cluster_signal_children(void);

#endif

/* *****************************************************************************
 * Data Structures - Clients / Subscriptions data
 **************************************************************************** */

#include "fio_hashmap.h"

#define CLUSTER_READ_BUFFER 16384

typedef struct cluster_pr_s {
  protocol_s protocol;
  FIOBJ channel;
  FIOBJ msg;
  void (*handler)(struct cluster_pr_s *pr);
  void (*sender)(FIOBJ data);
  intptr_t uuid;
  uint32_t exp_channel;
  uint32_t exp_msg;
  uint32_t type;
  int32_t filter;
  uint32_t length;
  uint8_t buffer[CLUSTER_READ_BUFFER];
} cluster_pr_s;

/* *****************************************************************************
 * Data Structures - Core Structures
 **************************************************************************** */

struct cluster_data_s {
  intptr_t listener;
  intptr_t client;
  fio_ls_s clients;
  fio_hash_s subscribers;
  spn_lock_i lock;
  char name[128];
} cluster_data = {.clients = FIO_LS_INIT(cluster_data.clients),
                  .subscribers = FIO_HASH_INIT,
                  .lock = SPN_LOCK_INIT};

static void cluster_data_cleanup(int delete_file) {
  if (delete_file && cluster_data.name[0]) {
#if DEBUG
    fprintf(stderr, "* INFO: (%d) CLUSTER UNLINKING\n", getpid());
#endif
    unlink(cluster_data.name);
  }
  while (fio_ls_any(&cluster_data.clients)) {
    intptr_t uuid = (intptr_t)fio_ls_pop(&cluster_data.clients);
    if (uuid > 0) {
      sock_close(uuid);
    }
  }
  cluster_data = (struct cluster_data_s){
      .lock = SPN_LOCK_INIT,
      .clients = (fio_ls_s)FIO_LS_INIT(cluster_data.clients),
      .subscribers = cluster_data.subscribers,
  };
}

static int cluster_init(void) {
  cluster_data_cleanup(0);
  /* create a unique socket name */
  char *tmp_folder = getenv("TMPDIR");
  uint32_t tmp_folder_len = 0;
  if (!tmp_folder || ((tmp_folder_len = (uint32_t)strlen(tmp_folder)) > 100)) {
#ifdef P_tmpdir
    tmp_folder = P_tmpdir;
    if (tmp_folder)
      tmp_folder_len = (uint32_t)strlen(tmp_folder);
#else
    tmp_folder = "/tmp/";
    tmp_folder_len = 5;
#endif
  }
  if (tmp_folder_len >= 100) {
    tmp_folder_len = 0;
  }
  if (tmp_folder_len) {
    memcpy(cluster_data.name, tmp_folder, tmp_folder_len);
    if (cluster_data.name[tmp_folder_len - 1] != '/')
      cluster_data.name[tmp_folder_len++] = '/';
  }
  memcpy(cluster_data.name + tmp_folder_len, "facil-io-sock-", 14);
  tmp_folder_len += 14;
  tmp_folder_len += fio_ltoa(cluster_data.name + tmp_folder_len, getpid(), 8);
  cluster_data.name[tmp_folder_len] = 0;

  /* remove if existing */
  unlink(cluster_data.name);
  return 0;
}

/* *****************************************************************************
 * Data Structures - Handler / Subscription management
 **************************************************************************** */

typedef struct {
  void (*on_message)(int32_t filter, FIOBJ, FIOBJ);
  FIOBJ channel;
  FIOBJ msg;
  int32_t filter;
} cluster_msg_data_s;

static void cluster_deferred_handler(void *msg_data_, void *ignr) {
  cluster_msg_data_s *data = msg_data_;
  data->on_message(data->filter, data->channel, data->msg);
  fiobj_free(data->channel);
  fiobj_free(data->msg);
  fio_free(data);
  (void)ignr;
}

static void cluster_forward_msg2handlers(cluster_pr_s *c) {
  spn_lock(&cluster_data.lock);
  void *target_ =
      fio_hash_find(&cluster_data.subscribers, (FIO_HASH_KEY_TYPE)c->filter);
  spn_unlock(&cluster_data.lock);
  if (target_) {
    cluster_msg_data_s *data = fio_malloc(sizeof(*data));
    if (!data) {
      perror("FATAL ERROR: (facil.io cluster) couldn't allocate memory");
      exit(errno);
    }
    *data = (cluster_msg_data_s){
        .on_message = ((cluster_msg_data_s *)(&target_))->on_message,
        .channel = fiobj_dup(c->channel),
        .msg = fiobj_dup(c->msg),
        .filter = c->filter,
    };
    defer(cluster_deferred_handler, data, NULL);
  }
}

/* *****************************************************************************
 * Cluster Protocol callbacks
 **************************************************************************** */

#ifdef __BIG_ENDIAN__
inline static uint32_t cluster_str2uint32(uint8_t *str) {
  return ((str[0] & 0xFF) | ((((uint32_t)str[1]) << 8) & 0xFF00) |
          ((((uint32_t)str[2]) << 16) & 0xFF0000) |
          ((((uint32_t)str[3]) << 24) & 0xFF000000));
}
inline static void cluster_uint2str(uint8_t *dest, uint32_t i) {
  dest[0] = i & 0xFF;
  dest[1] = (i >> 8) & 0xFF;
  dest[2] = (i >> 16) & 0xFF;
  dest[3] = (i >> 24) & 0xFF;
}
#else
inline static uint32_t cluster_str2uint32(uint8_t *str) {
  return (((((uint32_t)str[0]) << 24) & 0xFF000000) |
          ((((uint32_t)str[1]) << 16) & 0xFF0000) |
          ((((uint32_t)str[2]) << 8) & 0xFF00) | (str[3] & 0xFF));
}
inline static void cluster_uint2str(uint8_t *dest, uint32_t i) {
  dest[0] = (i >> 24) & 0xFF;
  dest[1] = (i >> 16) & 0xFF;
  dest[2] = (i >> 8) & 0xFF;
  dest[3] = i & 0xFF;
}
#endif

enum cluster_message_type_e {
  CLUSTER_MESSAGE_FORWARD,
  CLUSTER_MESSAGE_JSON,
  CLUSTER_MESSAGE_SHUTDOWN,
  CLUSTER_MESSAGE_ERROR,
  CLUSTER_MESSAGE_PING,
};

typedef struct cluster_msg_s {
  facil_msg_s message;
  size_t ref;
} cluster_msg_s;

static inline FIOBJ cluster_wrap_message(uint32_t ch_len, uint32_t msg_len,
                                         uint32_t type, int32_t filter,
                                         uint8_t *ch_data, uint8_t *msg_data) {
  FIOBJ buf = fiobj_str_buf(ch_len + msg_len + 16);
  fio_cstr_s f = fiobj_obj2cstr(buf);
  cluster_uint2str(f.bytes, ch_len);
  cluster_uint2str(f.bytes + 4, msg_len);
  cluster_uint2str(f.bytes + 8, type);
  cluster_uint2str(f.bytes + 12, (uint32_t)filter);
  if (ch_len && ch_data) {
    memcpy(f.bytes + 16, ch_data, ch_len);
  }
  if (msg_len && msg_data) {
    memcpy(f.bytes + 16 + ch_len, msg_data, msg_len);
  }
  fiobj_str_resize(buf, ch_len + msg_len + 16);
  return buf;
}

static uint8_t cluster_on_shutdown(intptr_t uuid, protocol_s *pr_) {
  cluster_pr_s *p = (cluster_pr_s *)pr_;
  p->sender(
      cluster_wrap_message(0, 0, CLUSTER_MESSAGE_SHUTDOWN, 0, NULL, NULL));
  return 255;
  (void)pr_;
  (void)uuid;
}

static void cluster_on_data(intptr_t uuid, protocol_s *pr_) {
  cluster_pr_s *c = (cluster_pr_s *)pr_;
  ssize_t i =
      sock_read(uuid, c->buffer + c->length, CLUSTER_READ_BUFFER - c->length);
  if (i <= 0)
    return;
  c->length += i;
  i = 0;
  do {
    if (!c->exp_channel && !c->exp_msg) {
      if (c->length - i < 16)
        break;
      c->exp_channel = cluster_str2uint32(c->buffer + i);
      c->exp_msg = cluster_str2uint32(c->buffer + i + 4);
      c->type = cluster_str2uint32(c->buffer + i + 8);
      c->filter = (int32_t)cluster_str2uint32(c->buffer + i + 12);
      if (c->exp_channel) {
        if (c->exp_channel >= (1024 * 1024 * 16)) {
          fprintf(stderr,
                  "FATAL ERROR: (%d) cluster message name too long (16Mb "
                  "limit): %u\n",
                  getpid(), (unsigned int)c->exp_channel);
          exit(1);
          return;
        }
        c->channel = fiobj_str_buf(c->exp_channel);
      }
      if (c->exp_msg) {
        if (c->exp_msg >= (1024 * 1024 * 64)) {
          fprintf(stderr,
                  "FATAL ERROR: (%d) cluster message data too long (64Mb "
                  "limit): %u\n",
                  getpid(), (unsigned int)c->exp_msg);
          exit(1);
          return;
        }
        c->msg = fiobj_str_buf(c->exp_msg);
      }
      i += 16;
    }
    if (c->exp_channel) {
      if (c->exp_channel + i > c->length) {
        fiobj_str_write(c->channel, (char *)c->buffer + i,
                        (size_t)(c->length - i));
        c->exp_channel -= (c->length - i);
        i = c->length;
        break;
      } else {
        fiobj_str_write(c->channel, (char *)c->buffer + i, c->exp_channel);
        i += c->exp_channel;
        c->exp_channel = 0;
      }
    }
    if (c->exp_msg) {
      if (c->exp_msg + i > c->length) {
        fiobj_str_write(c->msg, (char *)c->buffer + i, (size_t)(c->length - i));
        c->exp_msg -= (c->length - i);
        i = c->length;
        break;
      } else {
        fiobj_str_write(c->msg, (char *)c->buffer + i, c->exp_msg);
        i += c->exp_msg;
        c->exp_msg = 0;
      }
    }
    c->handler(c);
    fiobj_free(c->msg);
    fiobj_free(c->channel);
    c->msg = FIOBJ_INVALID;
    c->channel = FIOBJ_INVALID;
  } while (c->length > i);
  c->length -= i;
  if (c->length) {
    memmove(c->buffer, c->buffer + i, c->length);
  }
  (void)pr_;
}

static void cluster_ping(intptr_t uuid, protocol_s *pr_) {
  FIOBJ ping = cluster_wrap_message(0, 0, CLUSTER_MESSAGE_PING, 0, NULL, NULL);
  fiobj_send_free(uuid, ping);
  (void)pr_;
}

static void cluster_data_cleanup(int delete_file);

static void cluster_on_close(intptr_t uuid, protocol_s *pr_) {
  cluster_pr_s *c = (cluster_pr_s *)pr_;
  if (facil_parent_pid() == getpid()) {
    /* a child was lost, respawning is handled elsewhere. */
    spn_lock(&cluster_data.lock);
    FIO_LS_FOR(&cluster_data.clients, pos) {
      if (pos->obj == (void *)uuid) {
        fio_ls_remove(pos);
        break;
      }
    }
    spn_unlock(&cluster_data.lock);
  } else if (cluster_data.client == uuid) {
    /* no shutdown message received - parent crashed. */
    if (c->type != CLUSTER_MESSAGE_SHUTDOWN && facil_is_running()) {
      if (FACIL_PRINT_STATE) {
        fprintf(stderr, "* FATAL ERROR: (%d) Parent Process crash detected!\n",
                getpid());
      }
      facil_core_callback_force(FIO_CALL_ON_PARENT_CRUSH);
      cluster_data_cleanup(1);
      kill(getpid(), SIGINT);
    }
  }
  fiobj_free(c->msg);
  fiobj_free(c->channel);
  fio_free(c);
  (void)uuid;
}

static inline protocol_s *
cluster_alloc(intptr_t uuid, void (*handler)(struct cluster_pr_s *pr),
              void (*sender)(FIOBJ data)) {
  cluster_pr_s *p = fio_mmap(sizeof(*p));
  if (!p) {
    perror("FATAL ERROR: Cluster protocol allocation failed");
    exit(errno);
  }
  p->protocol = (protocol_s){
      .service = "_facil.io_cluster_",
      .ping = cluster_ping,
      .on_close = cluster_on_close,
      .on_shutdown = cluster_on_shutdown,
      .on_data = cluster_on_data,
  };
  p->uuid = uuid;
  p->handler = handler;
  p->sender = sender;
  return &p->protocol;
}

/* *****************************************************************************
 * Master (server) IPC Connections
 **************************************************************************** */

static void cluster_server_sender(FIOBJ data) {
  spn_lock(&cluster_data.lock);
  FIO_LS_FOR(&cluster_data.clients, pos) {
    if ((intptr_t)pos->obj > 0) {
      fiobj_send_free((intptr_t)pos->obj, fiobj_dup(data));
    }
  }
  spn_unlock(&cluster_data.lock);
  fiobj_free(data);
}

static void cluster_server_handler(struct cluster_pr_s *pr) {
  /* what to do? */
  fio_cstr_s cs = fiobj_obj2cstr(pr->channel);
  fio_cstr_s ms = fiobj_obj2cstr(pr->msg);
  cluster_server_sender(cluster_wrap_message(cs.len, ms.len, pr->type,
                                             pr->filter, cs.bytes, ms.bytes));
  cluster_forward_msg2handlers(pr);
}

/** Called when a ne client is available */
static void cluster_listen_accept(intptr_t uuid, protocol_s *protocol) {
  (void)protocol;
  /* prevent `accept` backlog in parent */
  intptr_t client;
  while ((client = sock_accept(uuid)) != -1) {
    if (facil_attach(client, cluster_alloc(client, cluster_server_handler,
                                           cluster_server_sender))) {
      perror("FATAL ERROR: (facil.io) failed to attach cluster client");
      exit(errno);
    }
    spn_lock(&cluster_data.lock);
    fio_ls_push(&cluster_data.clients, (void *)client);
    spn_unlock(&cluster_data.lock);
  }
}
/** Called when the connection was closed, but will not run concurrently */
static void cluster_listen_on_close(intptr_t uuid, protocol_s *protocol) {
  free(protocol);
  cluster_data.listener = -1;
  if (facil_parent_pid() == getpid()) {
#if DEBUG
    fprintf(stderr, "* INFO: (%d) stopped listening for cluster connections\n",
            getpid());
#endif
    kill(0, SIGINT);
  }
  (void)uuid;
}
/** called when a connection's timeout was reached */
static void cluster_listen_ping(intptr_t uuid, protocol_s *protocol) {
  sock_touch(uuid);
  (void)protocol;
}

static uint8_t cluster_listen_on_shutdown(intptr_t uuid, protocol_s *pr_) {
  return 255;
  (void)pr_;
  (void)uuid;
}

static void facil_listen2cluster(void *ignore) {
  /* this is called for each `fork`, but we only need this to run once. */
  spn_lock(&cluster_data.lock);
  cluster_init();
  cluster_data.listener = sock_listen(cluster_data.name, NULL);
  spn_unlock(&cluster_data.lock);
  if (cluster_data.listener < 0) {
    perror("FATAL ERROR: (facil.io cluster) failed to open cluster socket.\n"
           "             check file permissions");
    exit(errno);
  }
  protocol_s *p = malloc(sizeof(*p));
  *p = (protocol_s){
      .service = "_facil.io_listen4cluster_",
      .on_data = cluster_listen_accept,
      .on_shutdown = cluster_listen_on_shutdown,
      .ping = cluster_listen_ping,
      .on_close = cluster_listen_on_close,
  };
  if (!p) {
    perror("FATAL ERROR: (facil.io) couldn't allocate cluster server");
    exit(errno);
  }
  if (facil_attach(cluster_data.listener, p)) {
    perror(
        "FATAL ERROR: (facil.io) couldn't attach cluster server to facil.io");
    exit(errno);
  }
#if DEBUG
  fprintf(stderr, "* INFO: (%d) Listening to cluster: %s\n", getpid(),
          cluster_data.name);
#endif
  (void)ignore;
}

static void facil_cluster_cleanup(void *ignore) {
  /* cleanup the cluster data */
  cluster_data_cleanup(facil_parent_pid() == getpid());
  (void)ignore;
}

/* *****************************************************************************
 * Worker (client) IPC connections
 **************************************************************************** */

static void cluster_client_handler(struct cluster_pr_s *pr) {
  /* what to do? */
  cluster_forward_msg2handlers(pr);
}
static void cluster_client_sender(FIOBJ data) {
  fiobj_send_free(cluster_data.client, data);
}

/** The address of the server we are connecting to. */
// char *address;
/** The port on the server we are connecting to. */
// char *port;
/**
 * The `on_connect` callback should return a pointer to a protocol object
 * that will handle any connection related events.
 *
 * Should either call `facil_attach` or close the connection.
 */
void facil_cluster_on_connect(intptr_t uuid, void *udata) {
  cluster_data.client = uuid;
  if (facil_attach(uuid, cluster_alloc(uuid, cluster_client_handler,
                                       cluster_client_sender))) {
    perror("FATAL ERROR: (facil.io) failed to attach cluster connection");
    kill(facil_parent_pid(), SIGINT);
    exit(errno);
  }
  (void)udata;
}
/**
 * The `on_fail` is called when a socket fails to connect. The old sock UUID
 * is passed along.
 */
void facil_cluster_on_fail(intptr_t uuid, void *udata) {
  perror("FATAL ERROR: (facil.io) unknown cluster connection error");
  kill(facil_parent_pid(), SIGINT);
  exit(errno ? errno : 1);
  (void)udata;
  (void)uuid;
}
/** Opaque user data. */
// void *udata;
/** A non-system timeout after which connection is assumed to have failed. */
// uint8_t timeout;

static void facil_connect2cluster(void *ignore) {
  if (facil_parent_pid() != getpid()) {
    /* this is called for each child. */
    cluster_data.client =
        facil_connect(.address = cluster_data.name, .port = NULL,
                      .on_connect = facil_cluster_on_connect,
                      .on_fail = facil_cluster_on_fail);
  }
  (void)ignore;
}

/* *****************************************************************************
 * Initialization
 **************************************************************************** */

static void facil_connect_after_fork(void *ignore) {
  if (facil_parent_pid() == getpid()) {
    /* prevent `accept` backlog in parent */
    cluster_listen_accept(cluster_data.listener, NULL);
  } else {
    /* this is called for each child. */
  }
  (void)ignore;
}

static void facil_cluster_at_exit(void *ignore) {
  fio_hash_free(&cluster_data.subscribers);
  (void)ignore;
}

void __attribute__((constructor)) facil_cluster_initialize(void) {
  facil_core_callback_add(FIO_CALL_PRE_START, facil_listen2cluster, NULL);
  facil_core_callback_add(FIO_CALL_AFTER_FORK, facil_connect_after_fork, NULL);
  facil_core_callback_add(FIO_CALL_ON_START, facil_connect2cluster, NULL);
  facil_core_callback_add(FIO_CALL_ON_FINISH, facil_cluster_cleanup, NULL);
  facil_core_callback_add(FIO_CALL_AT_EXIT, facil_cluster_at_exit, NULL);
}

/* *****************************************************************************
 * External API
 **************************************************************************** */

void facil_cluster_set_handler(int32_t filter,
                               void (*on_message)(int32_t id, FIOBJ ch,
                                                  FIOBJ msg)) {
  spn_lock(&cluster_data.lock);
  fio_hash_insert(&cluster_data.subscribers, (uint64_t)filter,
                  (void *)(uintptr_t)on_message);
  spn_unlock(&cluster_data.lock);
}

int facil_cluster_send(int32_t filter, FIOBJ ch, FIOBJ msg) {
  if (!facil_is_running()) {
    fprintf(stderr, "ERROR: cluster inactive, can't send message.\n");
    return -1;
  }
  uint32_t type = CLUSTER_MESSAGE_FORWARD;

  if ((!ch || FIOBJ_TYPE_IS(ch, FIOBJ_T_STRING)) &&
      (!msg || FIOBJ_TYPE_IS(msg, FIOBJ_T_STRING))) {
    fiobj_dup(ch);
    fiobj_dup(msg);
  } else {
    type = CLUSTER_MESSAGE_JSON;
    ch = fiobj_obj2json(ch, 0);
    msg = fiobj_obj2json(msg, 0);
  }
  fio_cstr_s cs = fiobj_obj2cstr(ch);
  fio_cstr_s ms = fiobj_obj2cstr(msg);
  if (cluster_data.client > 0) {
    cluster_client_sender(
        cluster_wrap_message(cs.len, ms.len, type, filter, cs.bytes, ms.bytes));
  } else {
    cluster_server_sender(
        cluster_wrap_message(cs.len, ms.len, type, filter, cs.bytes, ms.bytes));
  }
  fiobj_free(ch);
  fiobj_free(msg);
  return 0;
}

/* NOT signal safe. */
void facil_cluster_signal_children(void) {
  if (facil_parent_pid() != getpid()) {
    kill(getpid(), SIGINT);
    return;
  }
  cluster_server_sender(
      cluster_wrap_message(0, 0, CLUSTER_MESSAGE_SHUTDOWN, 0, NULL, NULL));
}
