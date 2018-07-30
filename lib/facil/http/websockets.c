/*
copyright: Boaz Segev, 2016-2018
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.h"

#include "fio_llist.h"
#include "fiobj.h"

#include "evio.h"
#include "fio_base64.h"
#include "fio_sha1.h"
#include "http.h"
#include "http_internal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "fio_mem.h"

#include "websocket_parser.h"

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) &&                 \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

/*******************************************************************************
Buffer management - update to change the way the buffer is handled.
*/
struct buffer_s {
  void *data;
  size_t size;
};

#pragma weak create_ws_buffer
/** returns a buffer_s struct, with a buffer (at least) `size` long. */
struct buffer_s create_ws_buffer(ws_s *owner);

#pragma weak resize_ws_buffer
/** returns a buffer_s struct, with a buffer (at least) `size` long. */
struct buffer_s resize_ws_buffer(ws_s *owner, struct buffer_s);

#pragma weak free_ws_buffer
/** releases an existing buffer. */
void free_ws_buffer(ws_s *owner, struct buffer_s);

/** Sets the initial buffer size. (4Kb)*/
#define WS_INITIAL_BUFFER_SIZE 4096UL

/*******************************************************************************
Buffer management - simple implementation...
Since Websocket connections have a long life expectancy, optimizing this part of
the code probably wouldn't offer a high performance boost.
*/

// buffer increments by 4,096 Bytes (4Kb)
#define round_up_buffer_size(size) (((size) >> 12) + 1) << 12

struct buffer_s create_ws_buffer(ws_s *owner) {
  (void)(owner);
  struct buffer_s buff;
  buff.size = WS_INITIAL_BUFFER_SIZE;
  buff.data = malloc(buff.size);
  return buff;
}

struct buffer_s resize_ws_buffer(ws_s *owner, struct buffer_s buff) {
  buff.size = round_up_buffer_size(buff.size);
  void *tmp = realloc(buff.data, buff.size);
  if (!tmp) {
    free_ws_buffer(owner, buff);
    buff.data = NULL;
    buff.size = 0;
  }
  buff.data = tmp;
  return buff;
}
void free_ws_buffer(ws_s *owner, struct buffer_s buff) {
  (void)(owner);
  if (buff.data)
    free(buff.data);
}

#undef round_up_buffer_size

/*******************************************************************************
Create/Destroy the websocket object (prototypes)
*/

static ws_s *new_websocket();
static void destroy_ws(ws_s *ws);

/*******************************************************************************
The Websocket object (protocol + parser)
*/
struct ws_s {
  /** The Websocket protocol */
  protocol_s protocol;
  /** connection data */
  intptr_t fd;
  /** callbacks */
  void (*on_message)(ws_s *ws, char *data, size_t size, uint8_t is_text);
  void (*on_shutdown)(ws_s *ws);
  void (*on_ready)(ws_s *ws);
  void (*on_open)(ws_s *ws);
  void (*on_close)(intptr_t uuid, void *udata);
  /** Opaque user data. */
  void *udata;
  /** The maximum websocket message size */
  size_t max_msg_size;
  /** active pub/sub subscriptions */
  fio_ls_s subscriptions;
  /** socket buffer. */
  struct buffer_s buffer;
  /** data length (how much of the buffer actually used). */
  size_t length;
  /** message buffer. */
  FIOBJ msg;
  /** latest text state. */
  uint8_t is_text;
  /** websocket connection type. */
  uint8_t is_client;
};

/**
The Websocket Protocol Identifying String. Used for the `each` function.
*/
char *WEBSOCKET_ID_STR = "websockets";

/* *****************************************************************************
Create/Destroy the websocket subscription objects
***************************************************************************** */

static inline void clear_subscriptions(ws_s *ws) {
  while (fio_ls_any(&ws->subscriptions)) {
    facil_unsubscribe(fio_ls_pop(&ws->subscriptions));
  }
}

/* *****************************************************************************
Callbacks - Required functions for websocket_parser.h
***************************************************************************** */

static void websocket_on_unwrapped(void *ws_p, void *msg, uint64_t len,
                                   char first, char last, char text,
                                   unsigned char rsv) {
  ws_s *ws = ws_p;
  if (last && first) {
    ws->on_message(ws, msg, len, (uint8_t)text);
    return;
  }
  if (first) {
    ws->is_text = (uint8_t)text;
    if (ws->msg == FIOBJ_INVALID)
      ws->msg = fiobj_str_buf(len);
    fiobj_str_resize(ws->msg, 0);
  }
  fiobj_str_write(ws->msg, msg, len);
  if (last) {
    fio_cstr_s s = fiobj_obj2cstr(ws->msg);
    ws->on_message(ws, (char *)s.data, s.len, ws->is_text);
  }

  (void)rsv;
}
static void websocket_on_protocol_ping(void *ws_p, void *msg_, uint64_t len) {
  ws_s *ws = ws_p;
  if (msg_) {
    void *buff = malloc(len + 16);
    len = (((ws_s *)ws)->is_client
               ? websocket_client_wrap(buff, msg_, len, 10, 1, 1, 0)
               : websocket_server_wrap(buff, msg_, len, 10, 1, 1, 0));
    sock_write2(.uuid = ws->fd, .buffer = buff, .length = len);
  } else {
    if (((ws_s *)ws)->is_client) {
      sock_write2(.uuid = ws->fd, .buffer = "\x89\x80mask", .length = 2,
                  .dealloc = SOCK_DEALLOC_NOOP);
    } else {
      sock_write2(.uuid = ws->fd, .buffer = "\x89\x00", .length = 2,
                  .dealloc = SOCK_DEALLOC_NOOP);
    }
  }
}
static void websocket_on_protocol_pong(void *ws_p, void *msg, uint64_t len) {
  (void)len;
  (void)msg;
  (void)ws_p;
}
static void websocket_on_protocol_close(void *ws_p) {
  ws_s *ws = ws_p;
  sock_close(ws->fd);
}
static void websocket_on_protocol_error(void *ws_p) {
  ws_s *ws = ws_p;
  sock_close(ws->fd);
}

/*******************************************************************************
The Websocket Protocol implementation
*/

#define ws_protocol(fd) ((ws_s *)(server_get_protocol(fd)))

static void ws_ping(intptr_t fd, protocol_s *ws) {
  (void)(ws);
  if (((ws_s *)ws)->is_client) {
    sock_write2(.uuid = fd, .buffer = "\x89\x80MASK", .length = 6,
                .dealloc = SOCK_DEALLOC_NOOP);
  } else {
    sock_write2(.uuid = fd, .buffer = "\x89\x00", .length = 2,
                .dealloc = SOCK_DEALLOC_NOOP);
  }
}

static void on_close(intptr_t uuid, protocol_s *_ws) {
  destroy_ws((ws_s *)_ws);
  (void)uuid;
}

static void on_ready(intptr_t fduuid, protocol_s *ws) {
  (void)(fduuid);
  if (ws && ws->service == WEBSOCKET_ID_STR && ((ws_s *)ws)->on_ready)
    ((ws_s *)ws)->on_ready((ws_s *)ws);
}

static uint8_t on_shutdown(intptr_t fd, protocol_s *ws) {
  (void)(fd);
  if (ws && ((ws_s *)ws)->on_shutdown)
    ((ws_s *)ws)->on_shutdown((ws_s *)ws);
  if (((ws_s *)ws)->is_client) {
    sock_write2(.uuid = fd, .buffer = "\x8a\x80MASK", .length = 6,
                .dealloc = SOCK_DEALLOC_NOOP);
  } else {
    sock_write2(.uuid = fd, .buffer = "\x8a\x00", .length = 2,
                .dealloc = SOCK_DEALLOC_NOOP);
  }
  return 0;
}

static void on_data(intptr_t sockfd, protocol_s *ws_) {
  ws_s *const ws = (ws_s *)ws_;
  if (ws == NULL || ws->protocol.service != WEBSOCKET_ID_STR)
    return;
  struct websocket_packet_info_s info =
      websocket_buffer_peek(ws->buffer.data, ws->length);
  const uint64_t raw_length = info.packet_length + info.head_length;
  /* test expected data amount */
  if (ws->max_msg_size < raw_length) {
    /* too big */
    websocket_close(ws);
    return;
  }
  /* test buffer capacity */
  if (raw_length > ws->buffer.size) {
    ws->buffer.size = (size_t)raw_length;
    ws->buffer = resize_ws_buffer(ws, ws->buffer);
    if (!ws->buffer.data) {
      // no memory.
      websocket_close(ws);
      return;
    }
  }

  const ssize_t len = sock_read(sockfd, (uint8_t *)ws->buffer.data + ws->length,
                                ws->buffer.size - ws->length);
  if (len <= 0) {
    return;
  }
  ws->length = websocket_consume(ws->buffer.data, ws->length + len, ws,
                                 (~(ws->is_client) & 1));

  facil_force_event(sockfd, FIO_EVENT_ON_DATA);
}

static void on_data_first(intptr_t sockfd, protocol_s *ws_) {
  ws_s *const ws = (ws_s *)ws_;
  if (ws->on_open)
    ws->on_open(ws);
  ws->protocol.on_data = on_data;
  ws->protocol.on_ready = on_ready;

  if (ws->length) {
    ws->length = websocket_consume(ws->buffer.data, ws->length, ws,
                                   (~(ws->is_client) & 1));
  }
  evio_add_write(sock_uuid2fd(sockfd), (void *)sockfd);

  facil_force_event(sockfd, FIO_EVENT_ON_DATA);
}

/* later */
static void websocket_write_impl(intptr_t fd, void *data, size_t len, char text,
                                 char first, char last, char client);

/*******************************************************************************
Create/Destroy the websocket object
*/

static ws_s *new_websocket(intptr_t uuid) {
  // allocate the protocol object
  ws_s *ws = malloc(sizeof(*ws));
  *ws = (ws_s){
      .protocol.service = WEBSOCKET_ID_STR,
      .protocol.ping = ws_ping,
      .protocol.on_data = on_data_first,
      .protocol.on_close = on_close,
      .protocol.on_ready = NULL /* filled in after `on_open` */,
      .protocol.on_shutdown = on_shutdown,
      .subscriptions = FIO_LS_INIT(ws->subscriptions),
      .is_client = 0,
      .fd = uuid,
  };
  return ws;
}
static void destroy_ws(ws_s *ws) {
  if (ws->on_close)
    ws->on_close(ws->fd, ws->udata);
  if (ws->msg)
    fiobj_free(ws->msg);
  clear_subscriptions(ws);
  free_ws_buffer(ws, ws->buffer);
  free(ws);
}

void websocket_attach(intptr_t uuid, http_settings_s *http_settings,
                      websocket_settings_s *args, void *data, size_t length) {
  ws_s *ws = new_websocket(uuid);
  if (!ws) {
    perror("FATAL ERROR: couldn't allocate Websocket protocol object");
    exit(errno);
  }
  // we have an active websocket connection - prep the connection buffer
  ws->buffer = create_ws_buffer(ws);
  // Setup ws callbacks
  ws->on_open = args->on_open;
  ws->on_close = args->on_close;
  ws->on_message = args->on_message;
  ws->on_ready = args->on_ready;
  ws->on_shutdown = args->on_shutdown;
  // setup any user data
  ws->udata = args->udata;
  if (http_settings) {
    // client mode?
    ws->is_client = http_settings->is_client;
    // buffer limits
    ws->max_msg_size = http_settings->ws_max_msg_size;
    // update the timeout
    facil_set_timeout(uuid, http_settings->ws_timeout);
  } else {
    ws->max_msg_size = (1024 * 256);
    facil_set_timeout(uuid, 40);
  }

  if (data && length) {
    if (length > ws->buffer.size) {
      ws->buffer.size = length;
      ws->buffer = resize_ws_buffer(ws, ws->buffer);
      if (!ws->buffer.data) {
        // no memory.
        facil_attach(uuid, (protocol_s *)ws);
        websocket_close(ws);
        return;
      }
    }
    memcpy(ws->buffer.data, data, length);
    ws->length = length;
  }
  // update the protocol object, cleaning up the old one
  facil_attach(uuid, (protocol_s *)ws);
  // allow the on_open and on_data to take over the control.
  facil_force_event(uuid, FIO_EVENT_ON_DATA);
}

/*******************************************************************************
Writing to the Websocket
*/
#define WS_MAX_FRAME_SIZE                                                      \
  (FIO_MEMORY_BLOCK_ALLOC_LIMIT - 4096) // should be less then `unsigned short`

// clang-format off
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#   if defined(__has_include)
#     if __has_include(<endian.h>)
#      include <endian.h>
#     elif __has_include(<sys/endian.h>)
#      include <sys/endian.h>
#     endif
#   endif
#   if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
                __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __BIG_ENDIAN__
#   endif
#endif
// clang-format on

#ifdef __BIG_ENDIAN__
/** byte swap 64 bit integer */
#define bswap64(i) (i)

#else
// TODO: check for __builtin_bswap64
/** byte swap 64 bit integer */
#define bswap64(i)                                                             \
  ((((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                           \
   (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |                    \
   (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |            \
   (((i)&0xFF000000000000ULL) >> 40) | (((i)&0xFF00000000000000ULL) >> 56))
#endif

static void websocket_write_impl(intptr_t fd, void *data, size_t len, char text,
                                 char first, char last, char client) {
  if (len <= WS_MAX_FRAME_SIZE) {
    void *buff = fio_malloc(len + 16);
    len = (client ? websocket_client_wrap(buff, data, len, (text ? 1 : 2),
                                          first, last, 0)
                  : websocket_server_wrap(buff, data, len, (text ? 1 : 2),
                                          first, last, 0));
    sock_write2(.uuid = fd, .buffer = buff, .length = len, .dealloc = fio_free);
  } else {
    /* frame fragmentation is better for large data then large frames */
    while (len > WS_MAX_FRAME_SIZE) {
      websocket_write_impl(fd, data, WS_MAX_FRAME_SIZE, text, first, 0, client);
      data = ((uint8_t *)data) + WS_MAX_FRAME_SIZE;
      first = 0;
      len -= WS_MAX_FRAME_SIZE;
    }
    websocket_write_impl(fd, data, len, text, first, 1, client);
  }
  return;
}

/* *****************************************************************************
UTF-8 testing. This part was practically copied from:
https://stackoverflow.com/a/22135005/4025095
and
http://bjoern.hoehrmann.de/utf-8/decoder/dfa
***************************************************************************** */
/* Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de> */
/* See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details. */

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 00..1f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 20..3f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 40..5f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 60..7f
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   9,   9,   9,   9,   9,   9,
    9,   9,   9,   9,   9,   9,   9,   9,   9,   9, // 80..9f
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7, // a0..bf
    8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2, // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x3, 0x3, 0x4, 0x3, 0x3, // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
    0x8, 0x8, 0x8, 0x8, 0x8, // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4,
    0x6, 0x1, 0x1, 0x1, 0x1, // s0..s0
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   0,   1,   1,   1,   1,
    1,   0,   1,   0,   1,   1,   1,   1,   1,   1, // s1..s2
    1,   2,   1,   1,   1,   1,   1,   2,   1,   2,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   2,   1,   1,   1,   1,   1,   1,   1,   1, // s3..s4
    1,   2,   1,   1,   1,   1,   1,   1,   1,   2,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   3,   1,   3,   1,   1,   1,   1,   1,   1, // s5..s6
    1,   3,   1,   1,   1,   1,   1,   3,   1,   3,   1,
    1,   1,   1,   1,   1,   1,   3,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1, // s7..s8
};

static inline uint32_t validate_utf8(uint8_t *str, size_t len) {
  uint32_t state = 0;
  while (len) {
    uint32_t type = utf8d[*str];
    state = utf8d[256 + state * 16 + type];
    if (state == UTF8_REJECT)
      return 0;
    len--;
    str++;
  }
  return state == 0;
}

/* *****************************************************************************
Multi-client broadcast optimizations
***************************************************************************** */

static void websocket_optimize_free(facil_msg_s *msg, void *metadata) {
  fiobj_free((FIOBJ)metadata);
  (void)msg;
}

static inline facil_msg_metadata_s websocket_optimize(fio_cstr_s msg,
                                                      unsigned char opcode) {
  FIOBJ out = fiobj_str_buf(msg.len + 10);
  fiobj_str_resize(out,
                   websocket_server_wrap(fiobj_obj2cstr(out).data, msg.data,
                                         msg.len, opcode, 1, 1, 0));
  facil_msg_metadata_s ret = {
      .on_finish = websocket_optimize_free,
      .metadata = (void *)out,
  };
  return ret;
}
static facil_msg_metadata_s
websocket_optimize_generic(facil_msg_s *msg, FIOBJ raw_ch, FIOBJ raw_msg) {
  fio_cstr_s tmp = fiobj_obj2cstr(raw_msg);
  unsigned char opcode = 2;
  if (tmp.len <= (2 << 19) && validate_utf8((uint8_t *)tmp.data, tmp.len)) {
    opcode = 1;
  }
  facil_msg_metadata_s ret = websocket_optimize(tmp, opcode);
  ret.type_id = WEBSOCKET_OPTIMIZE_PUBSUB;
  return ret;
  (void)msg;
  (void)raw_ch;
}

static facil_msg_metadata_s
websocket_optimize_text(facil_msg_s *msg, FIOBJ raw_ch, FIOBJ raw_msg) {
  fio_cstr_s tmp = fiobj_obj2cstr(raw_msg);
  facil_msg_metadata_s ret = websocket_optimize(tmp, 1);
  ret.type_id = WEBSOCKET_OPTIMIZE_PUBSUB_TEXT;
  return ret;
  (void)msg;
  (void)raw_ch;
}

static facil_msg_metadata_s
websocket_optimize_binary(facil_msg_s *msg, FIOBJ raw_ch, FIOBJ raw_msg) {
  fio_cstr_s tmp = fiobj_obj2cstr(raw_msg);
  facil_msg_metadata_s ret = websocket_optimize(tmp, 2);
  ret.type_id = WEBSOCKET_OPTIMIZE_PUBSUB_BINARY;
  return ret;
  (void)msg;
  (void)raw_ch;
}

/**
 * Enables (or disables) broadcast optimizations.
 *
 * When using WebSocket pub/sub system is originally optimized for either
 * non-direct transmission (messages are handled by callbacks) or direct
 * transmission to 1-3 clients per channel (on avarage), meaning that the
 * majority of the messages are meant for a single recipient (or multiple
 * callback recipients) and only some are expected to be directly transmitted to
 * a group.
 *
 * However, when most messages are intended for direct transmission to more than
 * 3 clients (on avarage), certain optimizations can be made to improve memory
 * consumption (minimize duplication or WebSocket network data).
 *
 * This function allows enablement (or disablement) of these optimizations.
 * These optimizations include:
 *
 * * WEBSOCKET_OPTIMIZE_PUBSUB - optimize all direct transmission messages,
 *                               best attempt to detect Text vs. Binary data.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_TEXT - optimize direct pub/sub text messages.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_BINARY - optimize direct pub/sub binary messages.
 *
 * Note: to disable an optimization it should be disabled the same amount of
 * times it was enabled - multiple optimization enablements for the same type
 * are merged, but reference counted (disabled when reference is zero).
 */
void websocket_optimize4broadcasts(intptr_t type, int enable) {
  static intptr_t generic = 0;
  static intptr_t text = 0;
  static intptr_t binary = 0;
  facil_msg_metadata_s (*callback)(facil_msg_s *, FIOBJ, FIOBJ);
  intptr_t *counter;
  switch ((0 - type)) {
  case (0 - WEBSOCKET_OPTIMIZE_PUBSUB):
    counter = &generic;
    callback = websocket_optimize_generic;
    break;
  case (0 - WEBSOCKET_OPTIMIZE_PUBSUB_TEXT):
    counter = &text;
    callback = websocket_optimize_text;
    break;
  case (0 - WEBSOCKET_OPTIMIZE_PUBSUB_BINARY):
    counter = &binary;
    callback = websocket_optimize_binary;
    break;
  default:
    return;
  }
  if (enable) {
    if (spn_add(counter, 1) == 1) {
      facil_message_metadata_set(callback, 1);
    }
  } else {
    if (spn_sub(counter, 1) == 0) {
      facil_message_metadata_set(callback, 0);
    }
  }
}

/* *****************************************************************************
Subscription handling
***************************************************************************** */

typedef struct {
  void (*on_message)(websocket_pubsub_notification_s notification);
  void (*on_unsubscribe)(void *udata);
  void *udata;
} websocket_sub_data_s;

static void websocket_on_unsubscribe(void *u1, void *u2) {
  websocket_sub_data_s *d = u2;
  (void)u1;
  if (d->on_unsubscribe) {
    d->on_unsubscribe(d->udata);
  }
  free(d);
}

static inline void websocket_on_pubsub_message_direct_internal(facil_msg_s *msg,
                                                               uint8_t txt) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_WRITE);
  if (!pr) {
    if (errno == EBADF)
      return;
    facil_message_defer(msg);
    return;
  }
  FIOBJ message = FIOBJ_INVALID;
  FIOBJ pre_wrapped = FIOBJ_INVALID;
  switch (txt) {
  case 0:
    pre_wrapped =
        (FIOBJ)facil_message_metadata(msg, WEBSOCKET_OPTIMIZE_PUBSUB_BINARY);
    break;
  case 1:
    pre_wrapped =
        (FIOBJ)facil_message_metadata(msg, WEBSOCKET_OPTIMIZE_PUBSUB_TEXT);
    break;
  case 2:
    pre_wrapped = (FIOBJ)facil_message_metadata(msg, WEBSOCKET_OPTIMIZE_PUBSUB);
    break;
  default:
    break;
  }
  if (pre_wrapped) {
    // fprintf(stderr, "INFO: WebSocket Pub/Sub optimized for broadcast\n");
    fiobj_send_free((intptr_t)msg->udata1, fiobj_dup(pre_wrapped));
    goto finish;
  }
  fio_cstr_s tmp;
  if (FIOBJ_TYPE_IS(msg->msg, FIOBJ_T_STRING)) {
    message = fiobj_dup(msg->msg);
    tmp = fiobj_obj2cstr(message);
    if (txt == 2) {
      /* unknown text state */
      txt =
          (tmp.len >= (2 << 14) ? 0
                                : validate_utf8((uint8_t *)tmp.data, tmp.len));
    }
  } else {
    message = fiobj_obj2json(msg->msg, 0);
    tmp = fiobj_obj2cstr(message);
  }
  websocket_write((ws_s *)pr, tmp.data, tmp.len, txt & 1);
  fiobj_free(message);
finish:
  facil_protocol_unlock(pr, FIO_PR_LOCK_WRITE);
}

static void websocket_on_pubsub_message_direct(facil_msg_s *msg) {
  websocket_on_pubsub_message_direct_internal(msg, 2);
}

static void websocket_on_pubsub_message_direct_txt(facil_msg_s *msg) {
  websocket_on_pubsub_message_direct_internal(msg, 1);
}

static void websocket_on_pubsub_message_direct_bin(facil_msg_s *msg) {
  websocket_on_pubsub_message_direct_internal(msg, 0);
}

static void websocket_on_pubsub_message(facil_msg_s *msg) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF)
      return;
    facil_message_defer(msg);
    return;
  }
  websocket_sub_data_s *d = msg->udata2;

  if (d->on_message)
    d->on_message((websocket_pubsub_notification_s){
        .ws = (ws_s *)pr,
        .channel = msg->channel,
        .message = msg->msg,
    });
  facil_protocol_unlock(pr, FIO_PR_LOCK_TASK);
}

/**
 * Returns a subscription ID on success and 0 on failure.
 */
#undef websocket_subscribe
uintptr_t websocket_subscribe(struct websocket_subscribe_s args) {
  if (!args.ws)
    goto error;
  websocket_sub_data_s *d = malloc(sizeof(*d));
  if (!d) {
    websocket_close(args.ws);
    goto error;
  }
  *d = (websocket_sub_data_s){
      .udata = args.udata,
      .on_message = args.on_message,
      .on_unsubscribe = args.on_unsubscribe,
  };
  subscription_s *sub = facil_subscribe(
          .channel = args.channel, .match = args.match,
          .on_unsubscribe = websocket_on_unsubscribe,
          .on_message =
              (args.on_message
                   ? websocket_on_pubsub_message
                   : args.force_binary
                         ? websocket_on_pubsub_message_direct_bin
                         : args.force_text
                               ? websocket_on_pubsub_message_direct_txt
                               : websocket_on_pubsub_message_direct),
          .udata1 = (void *)args.ws->fd, .udata2 = d);
  if (!sub) {
    /* don't free `d`, return (`d` freed by callback) */
    return 0;
  }
  fio_ls_push(&args.ws->subscriptions, sub);
  return (uintptr_t)args.ws->subscriptions.prev;
error:
  if (args.on_unsubscribe)
    args.on_unsubscribe(args.udata);
  return 0;
}

/**
 * Unsubscribes from a channel.
 */
void websocket_unsubscribe(ws_s *ws, uintptr_t subscription_id) {
  facil_unsubscribe((subscription_s *)((fio_ls_s *)subscription_id)->obj);
  fio_ls_remove((fio_ls_s *)subscription_id);
  (void)ws;
}

/*******************************************************************************
The API implementation
*/

/** Returns the opaque user data associated with the websocket. */
void *websocket_udata(ws_s *ws) { return ws->udata; }

/** Returns the the process specific connection's UUID (see `libsock`). */
intptr_t websocket_uuid(ws_s *ws) { return ws->fd; }

/** Sets the opaque user data associated with the websocket.
 * Returns the old value, if any. */
void *websocket_udata_set(ws_s *ws, void *udata) {
  void *old = ws->udata;
  ws->udata = udata;
  return old;
}

/**
 * Returns 1 if the WebSocket connection is in Client mode (connected to a
 * remote server) and 0 if the connection is in Server mode (a connection
 * established using facil.io's HTTP server).
 */
uint8_t websocket_is_client(ws_s *ws) { return ws->is_client; }

/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, void *data, size_t size, uint8_t is_text) {
  if (sock_isvalid(ws->fd)) {
    websocket_write_impl(ws->fd, data, size, is_text, 1, 1, ws->is_client);
    return 0;
  }
  return -1;
}
/** Closes a websocket connection. */
void websocket_close(ws_s *ws) {
  sock_write2(.uuid = ws->fd, .buffer = "\x88\x00", .length = 2,
              .dealloc = SOCK_DEALLOC_NOOP);
  sock_close(ws->fd);
  return;
}

/**
Counts the number of websocket connections.
*/
size_t websocket_count(void) { return facil_count(WEBSOCKET_ID_STR); }

/*******************************************************************************
Each Implementation
*/

/** A task container. */
struct WSTask {
  void (*task)(ws_s *, void *);
  void (*on_finish)(ws_s *, void *);
  void *arg;
};
/** Performs a task on each websocket connection that shares the same process
 */
static void perform_ws_task(intptr_t fd, protocol_s *ws_, void *tsk_) {
  (void)(fd);
  struct WSTask *tsk = tsk_;
  tsk->task((ws_s *)(ws_), tsk->arg);
}
/** clears away a wesbocket task. */
static void finish_ws_task(intptr_t fd, void *arg) {
  struct WSTask *tsk = arg;
  if (tsk->on_finish) {
    protocol_s *ws = facil_protocol_try_lock(fd, FIO_PR_LOCK_TASK);
    if (!ws && errno != EBADF) {
      defer((void (*)(void *, void *))finish_ws_task, (void *)fd, arg);
      return;
    }
    tsk->on_finish((ws_s *)ws, tsk->arg);
    if (ws)
      facil_protocol_unlock(ws, FIO_PR_LOCK_TASK);
  }
  free(tsk);
}

/**
Performs a task on each websocket connection that shares the same process
(except the originating `ws_s` connection which is allowed to be NULL).
 */
#undef websocket_each
void __attribute__((deprecated))
websocket_each(struct websocket_each_args_s args) {
  struct WSTask *tsk = malloc(sizeof(*tsk));
  tsk->arg = args.arg;
  tsk->on_finish = args.on_finish;
  tsk->task = args.task;
  facil_each(.origin = (args.origin ? args.origin->fd : -1),
             .service = WEBSOCKET_ID_STR, .task = perform_ws_task, .arg = tsk,
             .on_complete = finish_ws_task);
}
/*******************************************************************************
Multi-Write (direct broadcast) Implementation
*/
struct websocket_multi_write {
  uint8_t (*if_callback)(ws_s *ws_to, void *arg);
  void (*on_finished)(ws_s *ws_origin, void *arg);
  intptr_t origin;
  void *arg;
  spn_lock_i lock;
  /* ... we need to have padding for pointer arithmatics... */
  uint8_t as_client;
  /* ... we need to have padding for pointer arithmatics... */
  size_t count;
  size_t length;
  uint8_t buffer[]; /* starts on border alignment */
};

static void ws_mw_defered_on_finish_fb(intptr_t fd, void *arg) {
  (void)(fd);
  struct websocket_multi_write *fin = arg;
  if (fin->on_finished)
    fin->on_finished(NULL, fin->arg);
  free(fin);
}
static void ws_mw_defered_on_finish(intptr_t fd, protocol_s *ws, void *arg) {
  (void)fd;
  struct websocket_multi_write *fin = arg;
  if (fin->on_finished) {
    fin->on_finished((ws_s *)ws, fin->arg);
  }
  free(fin);
}

static void ws_reduce_or_free_multi_write(void *buff) {
  struct websocket_multi_write *mw = (void *)((uintptr_t)buff - sizeof(*mw));
  spn_lock(&mw->lock);
  mw->count -= 1;
  if (!mw->count) {
    spn_unlock(&mw->lock);
    if (mw->on_finished) {
      facil_defer(.uuid = mw->origin, .task = ws_mw_defered_on_finish,
                  .arg = mw, .fallback = ws_mw_defered_on_finish_fb,
                  .type = FIO_PR_LOCK_WRITE);
    } else
      free(mw);
  } else
    spn_unlock(&mw->lock);
}

static void ws_finish_multi_write(intptr_t fd, void *arg) {
  struct websocket_multi_write *multi = arg;
  (void)(fd);
  ws_reduce_or_free_multi_write(multi->buffer);
}

static void ws_direct_multi_write(intptr_t fd, protocol_s *_ws, void *arg) {
  struct websocket_multi_write *multi = arg;
  if (((ws_s *)(_ws))->is_client != multi->as_client)
    return;
  spn_lock(&multi->lock);
  multi->count += 1;
  spn_unlock(&multi->lock);
  sock_write2(.uuid = fd, .buffer = multi->buffer, .length = multi->length,
              .dealloc = ws_reduce_or_free_multi_write);
}

static void ws_check_multi_write(intptr_t fd, protocol_s *_ws, void *arg) {
  struct websocket_multi_write *multi = arg;
  if (((ws_s *)(_ws))->is_client != multi->as_client)
    return;
  if (multi->if_callback((void *)_ws, multi->arg))
    ws_direct_multi_write(fd, _ws, arg);
}

#undef websocket_write_each
int __attribute__((deprecated))
websocket_write_each(struct websocket_write_each_args_s args) {
  if (!args.data || !args.length)
    return -1;
  struct websocket_multi_write *multi =
      malloc(sizeof(*multi) + args.length + 16 /* max head size + 2 */);
  if (!multi) {
    if (args.on_finished)
      defer((void (*)(void *, void *))args.on_finished, NULL, args.arg);
    return -1;
  }
  *multi = (struct websocket_multi_write){
      .length =
          (args.as_client
               ? websocket_client_wrap(multi->buffer, args.data, args.length,
                                       args.is_text ? 1 : 2, 1, 1, 0)
               : websocket_server_wrap(multi->buffer, args.data, args.length,
                                       args.is_text ? 1 : 2, 1, 1, 0)),
      .if_callback = args.filter,
      .on_finished = args.on_finished,
      .arg = args.arg,
      .origin = (args.origin ? args.origin->fd : -1),
      .as_client = args.as_client,
      .lock = SPN_LOCK_INIT,
      .count = 1,
  };

  facil_each(.origin = multi->origin, .service = WEBSOCKET_ID_STR,
             .task_type = FIO_PR_LOCK_WRITE,
             .task =
                 (args.filter ? ws_check_multi_write : ws_direct_multi_write),
             .arg = multi, .on_complete = ws_finish_multi_write);
  return 0;
}
