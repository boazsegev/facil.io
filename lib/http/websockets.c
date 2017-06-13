/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "fio_list.h"

#include "bscrypt.h"
#include "pubsub.h"
#include "websockets.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
  buff.size = round_up_buffer_size(WS_INITIAL_BUFFER_SIZE);
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
struct Websocket {
  /** The Websocket protocol */
  protocol_s protocol;
  /** connection data */
  intptr_t fd;
  /** callbacks */
  void (*on_message)(ws_s *ws, char *data, size_t size, uint8_t is_text);
  void (*on_shutdown)(ws_s *ws);
  void (*on_ready)(ws_s *ws);
  void (*on_close)(ws_s *ws);
  /** Opaque user data. */
  void *udata;
  /** The maximum websocket message size */
  size_t max_msg_size;
  /** active pub/sub subscriptions */
  fio_list_s subscriptions;
  /** message buffer. */
  struct buffer_s buffer;
  /** message length (how much of the buffer actually used). */
  size_t length;
  /** parser. */
  struct {
    union {
      unsigned len1 : 16;
      unsigned long long len2 : 64;
      char bytes[8];
    } psize;
    size_t length;
    size_t received;
    char mask[4];
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
    } head, head2;
    struct {
      unsigned size : 7;
      unsigned masked : 1;
    } sdata;
    struct {
      unsigned has_mask : 1;
      unsigned at_mask : 2;
      unsigned has_len : 1;
      unsigned at_len : 3;
      unsigned has_head : 1;
    } state;
    unsigned client : 1;
  } parser;
};

/**
The Websocket Protocol Identifying String. Used for the `each` function.
*/
char *WEBSOCKET_ID_STR = "websockets";

/**
A thread localized buffer used for reading and parsing data from the socket.
*/
#define WEBSOCKET_READ_MAX 4096
static __thread struct {
  int pos;
  char buffer[WEBSOCKET_READ_MAX];
} read_buffer;

/* *****************************************************************************
Create/Destroy the websocket subscription objects
***************************************************************************** */

typedef struct {
  fio_list_s node;
  pubsub_sub_pt sub;
} subscription_s;

static inline subscription_s *create_subscription(ws_s *ws, pubsub_sub_pt sub) {
  subscription_s *s = malloc(sizeof(*s));
  s->sub = sub;
  fio_list_add(&ws->subscriptions, &s->node);
  return s;
}

static inline void free_subscription(subscription_s *s) {
  fio_list_remove(&s->node);
  free(s);
}

static inline void clear_subscriptions(ws_s *ws) {
  subscription_s *s;
  fio_list_for_each(subscription_s, node, s, ws->subscriptions) {
    pubsub_unsubscribe(s->sub);
    free_subscription(s);
  }
}

/*******************************************************************************
The Websocket Protocol implementation
*/

#define ws_protocol(fd) ((ws_s *)(server_get_protocol(fd)))

static void ws_ping(intptr_t fd, protocol_s *ws) {
  (void)(ws);
  sock_write2(.uuid = fd, .buffer = "\x89\x00", .length = 2, .move = 1,
              .dealloc = SOCK_DEALLOC_NOOP);
}

static void on_close(protocol_s *_ws) { destroy_ws((ws_s *)_ws); }

static void on_ready(intptr_t fduuid, protocol_s *ws) {
  (void)(fduuid);
  if (ws && ws->service == WEBSOCKET_ID_STR && ((ws_s *)ws)->on_ready)
    ((ws_s *)ws)->on_ready((ws_s *)ws);
}

// static void on_open(intptr_t fd, protocol_s *ws, void *callback) {
//   (void)(fd);
//   if (callback && ws && ws->service == WEBSOCKET_ID_STR)
//     ((void (*)(void *))callback)(ws);
// }

static void on_shutdown(intptr_t fd, protocol_s *ws) {
  (void)(fd);
  if (ws && ((ws_s *)ws)->on_shutdown)
    ((ws_s *)ws)->on_shutdown((ws_s *)ws);
}

/* later */
static void websocket_write_impl(intptr_t fd, void *data, size_t len, char text,
                                 char first, char last, char client);
static size_t websocket_encode(void *buff, void *data, size_t len, char text,
                               char first, char last, char client);

/* read data from the socket, parse it and invoke the websocket events. */
static void on_data(intptr_t sockfd, protocol_s *_ws) {
#define ws ((ws_s *)_ws)
  if (ws == NULL || ws->protocol.service != WEBSOCKET_ID_STR)
    return;
  ssize_t len = 0;
  ssize_t data_len = 0;
  while ((len = sock_read(sockfd, read_buffer.buffer, WEBSOCKET_READ_MAX)) >
         0) {
    data_len = 0;
    read_buffer.pos = 0;
    while (read_buffer.pos < len) {
      // collect the frame's head
      if (!ws->parser.state.has_head) {
        ws->parser.state.has_head = 1;
        *((char *)(&(ws->parser.head))) = read_buffer.buffer[read_buffer.pos];
        // save a copy if it's the first head in a fragmented message
        if (!(*(char *)(&ws->parser.head2))) {
          ws->parser.head2 = ws->parser.head;
        }
        // advance
        read_buffer.pos++;
        // go back to the `while` head, to review if there's more data
        continue;
      }

      // save the mask and size information
      if (!ws->parser.state.at_len && !ws->parser.state.has_len) {
        // uint8_t tmp = ws->parser.sdata.masked;
        *((char *)(&(ws->parser.sdata))) = read_buffer.buffer[read_buffer.pos];
        // ws->parser.sdata.masked |= tmp;
        // set length
        ws->parser.state.at_len = (ws->parser.sdata.size == 127
                                       ? 7
                                       : ws->parser.sdata.size == 126 ? 1 : 0);
        if (!ws->parser.state.at_len) {
          ws->parser.length = ws->parser.sdata.size;
          ws->parser.state.has_len = 1;
        }
        read_buffer.pos++;
        continue;
      }

      // check that if we need to collect the length data
      if (!ws->parser.state.has_len) {
      // avoiding a loop so we don't mixup the meaning of "continue" and
      // "break"
      collect_len:
////////// NOTICE: Network Byte Order requires us to translate the data
#ifdef __BIG_ENDIAN__
        if ((ws->parser.state.at_len == 1 && ws->parser.sdata.size == 126) ||
            (ws->parser.state.at_len == 7 && ws->parser.sdata.size == 127)) {
          ws->parser.psize.bytes[ws->parser.state.at_len] =
              read_buffer.buffer[read_buffer.pos++];
          ws->parser.state.has_len = 1;
          ws->parser.length = (ws->parser.sdata.size == 126)
                                  ? ws->parser.psize.len1
                                  : ws->parser.psize.len2;
        } else {
          ws->parser.psize.bytes[ws->parser.state.at_len++] =
              read_buffer.buffer[read_buffer.pos++];
          if (read_buffer.pos < len)
            goto collect_len;
        }
#else
        if (ws->parser.state.at_len == 0) {
          ws->parser.psize.bytes[ws->parser.state.at_len] =
              read_buffer.buffer[read_buffer.pos++];
          ws->parser.state.has_len = 1;
          ws->parser.length = (ws->parser.sdata.size == 126)
                                  ? ws->parser.psize.len1
                                  : ws->parser.psize.len2;
        } else {
          ws->parser.psize.bytes[ws->parser.state.at_len--] =
              read_buffer.buffer[read_buffer.pos++];
          if (read_buffer.pos < len)
            goto collect_len;
        }
#endif
        // check message size limit
        if (ws->max_msg_size <
            ws->length + (ws->parser.length - ws->parser.received)) {
          // close connection!
          fprintf(stderr, "ERROR Websocket: Payload too big, review limits.\n");
          sock_close(sockfd);
          return;
        }
        continue;
      }

      // check that the data is masked and that we didn't colleced the mask yet
      if (ws->parser.sdata.masked && !(ws->parser.state.has_mask)) {
      // avoiding a loop so we don't mixup the meaning of "continue" and "break"
      collect_mask:
        if (ws->parser.state.at_mask == 3) {
          ws->parser.mask[ws->parser.state.at_mask] =
              read_buffer.buffer[read_buffer.pos++];
          ws->parser.state.has_mask = 1;
          ws->parser.state.at_mask = 0;
        } else {
          ws->parser.mask[ws->parser.state.at_mask++] =
              read_buffer.buffer[read_buffer.pos++];
          if (read_buffer.pos < len)
            goto collect_mask;
          else
            continue;
        }
        // since it's possible that there's no more data (0 length frame),
        // we don't use `continue` (check while loop) and we process what we
        // have.
      }

      // Now that we know everything about the frame, let's collect the data

      // How much data in the buffer is part of the frame?
      data_len = len - read_buffer.pos;
      if (data_len + ws->parser.received > ws->parser.length)
        data_len = ws->parser.length - ws->parser.received;

      // a note about unmasking: since ws->parser.state.at_mask is only 2 bits,
      // it will wrap around (i.e. 3++ == 0), so no modulus is required :-)
      // unmask:
      if (ws->parser.sdata.masked) {
        for (int i = 0; i < data_len; i++) {
          read_buffer.buffer[i + read_buffer.pos] ^=
              ws->parser.mask[ws->parser.state.at_mask++];
        }
      } else if (ws->parser.client == 0) {
        // enforce masking unless acting as client, also for security reasons...
        fprintf(stderr, "ERROR Websockets: unmasked frame, disconnecting.\n");
        sock_close(sockfd);
        return;
      }
      // Copy the data to the Websocket buffer - only if it's a user message
      if (data_len &&
          (ws->parser.head.op_code == 1 || ws->parser.head.op_code == 2 ||
           (!ws->parser.head.op_code && (ws->parser.head2.op_code == 1 ||
                                         ws->parser.head2.op_code == 2)))) {
        // review and resize the buffer's capacity - it can only grow.
        if (ws->length + ws->parser.length - ws->parser.received >
            ws->buffer.size) {
          ws->buffer = resize_ws_buffer(ws, ws->buffer);
          if (!ws->buffer.data) {
            // no memory.
            websocket_close(ws);
            return;
          }
        }
        // copy here
        memcpy((uint8_t *)ws->buffer.data + ws->length,
               read_buffer.buffer + read_buffer.pos, data_len);
        ws->length += data_len;
      }
      // set the frame's data received so far (copied or not)
      ws->parser.received += data_len;

      // check that we have collected the whole of the frame.
      if (ws->parser.length > ws->parser.received) {
        read_buffer.pos += data_len;
        // fprintf(stderr, "%p websocket has %lu out of %lu\n", (void *)ws,
        //         ws->parser.received, ws->parser.length);
        continue;
      }

      // we have the whole frame, time to process the data.
      // pings, pongs and other non-user messages are handled independently.
      if (ws->parser.head.op_code == 0 || ws->parser.head.op_code == 1 ||
          ws->parser.head.op_code == 2) {
        /* a user data frame - make sure we got the `fin` flag, or an error
         * occured */
        if (!ws->parser.head.fin) {
          /* This frame was a partial message. */
          goto reset_state;
        }
        /* This was the last frame */
        if (ws->on_message) /* call the on_message callback */
          ws->on_message(ws, ws->buffer.data, ws->length,
                         ws->parser.head2.op_code == 1);
        goto reset_parser;
      } else if (ws->parser.head.op_code == 8) {
        /* op-code == close */
        websocket_close(ws);
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code == 9) {
        /* ping */
        // write Pong - including ping data...
        websocket_write_impl(sockfd, read_buffer.buffer + read_buffer.pos,
                             data_len, 10, 1, 1, ws->parser.client);
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code == 10) {
        /* pong */
        // do nothing... almost
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code > 2 && ws->parser.head.op_code < 8) {
        /* future control frames. ignore. */
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else {
        /* WTF? */
        // fprintf(stderr, "%p websocket reached a WTF?! state..."
        //                 "op1: %i , op2: %i\n",
        //         (void *)ws, ws->parser.head.op_code,
        //         ws->parser.head2.op_code);
        fprintf(stderr, "ERROR Websockets: protocol error, disconnecting.\n");
        sock_close(sockfd);
        return;
      }

    reset_parser:
      ws->length = 0;
      // clear the parser's multi-frame state
      *((char *)(&(ws->parser.head2))) = 0;
      ws->parser.sdata.masked = 0;
    reset_state:
      // move the pos marker along - in case we have more then one frame in the
      // buffer
      read_buffer.pos += data_len;
      // reset parser state
      ws->parser.state.has_len = 0;
      ws->parser.state.at_len = 0;
      ws->parser.state.has_mask = 0;
      ws->parser.state.at_mask = 0;
      ws->parser.state.has_head = 0;
      ws->parser.sdata.size = 0;
      *((char *)(&(ws->parser.head))) = 0;
      ws->parser.received = ws->parser.length = ws->parser.psize.len2 =
          data_len = 0;
    }
  }
#undef ws
}

/*******************************************************************************
Create/Destroy the websocket object
*/

static ws_s *new_websocket() {
  // allocate the protocol object
  ws_s *ws = malloc(sizeof(*ws));
  *ws = (ws_s){
      .protocol.service = WEBSOCKET_ID_STR,
      .protocol.ping = ws_ping,
      .protocol.on_data = on_data,
      .protocol.on_close = on_close,
      .protocol.on_ready = on_ready,
      .protocol.on_shutdown = on_shutdown,
      .subscriptions = FIO_LIST_INIT_STATIC(ws->subscriptions),
  };
  return ws;
}
static void destroy_ws(ws_s *ws) {
  clear_subscriptions(ws);
  if (ws->on_close)
    ws->on_close(ws);
  free_ws_buffer(ws, ws->buffer);
  free(ws);
}

/*******************************************************************************
Writing to the Websocket
*/
#define WS_MAX_FRAME_SIZE 65532 // should be less then `unsigned short`

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

static void websocket_mask(void *dest, void *data, size_t len) {
  /* a semi-random 4 byte mask */
  uint32_t mask = ((rand() << 7) ^ ((uintptr_t)dest >> 13));
  /* place mask at head of data */
  dest = (uint8_t *)dest + 4;
  memcpy(dest, &mask, 4);
  /* TODO: optimize this */
  for (size_t i = 0; i < len; i++) {
    ((uint8_t *)dest)[i] = ((uint8_t *)data)[i] ^ ((uint8_t *)(&mask))[i & 3];
  }
}
static size_t websocket_encode(void *buff, void *data, size_t len, char text,
                               char first, char last, char client) {
  if (len < 126) {
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
      unsigned size : 7;
      unsigned masked : 1;
    } head = {.op_code = (first ? (!text ? 2 : text) : 0),
              .fin = last,
              .size = len,
              .masked = client};
    memcpy(buff, &head, 2);
    if (client) {
      websocket_mask((uint8_t *)buff + 2, data, len);
      len += 4;
    } else
      memcpy((uint8_t *)buff + 2, data, len);
    return len + 2;
  } else if (len < (1UL << 16)) {
    /* head is 4 bytes */
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
      unsigned size : 7;
      unsigned masked : 1;
      unsigned length : 16;
    } head = {.op_code = (first ? (text ? 1 : 2) : 0),
              .fin = last,
              .size = 126,
              .masked = client,
              .length = htons(len)};
    memcpy(buff, &head, 4);
    if (client) {
      websocket_mask((uint8_t *)buff + 4, data, len);
      len += 4;
    } else
      memcpy((uint8_t *)buff + 4, data, len);
    return len + 4;
  }
  /* Really Long Message  */
  struct {
    unsigned op_code : 4;
    unsigned rsv3 : 1;
    unsigned rsv2 : 1;
    unsigned rsv1 : 1;
    unsigned fin : 1;
    unsigned size : 7;
    unsigned masked : 1;
  } head = {
      .op_code = (first ? (text ? 1 : 2) : 0),
      .fin = last,
      .size = 127,
      .masked = client,
  };
  memcpy(buff, &head, 2);
  ((size_t *)((uint8_t *)buff + 2))[0] = bswap64(len);
  if (client) {
    websocket_mask((uint8_t *)buff + 10, data, len);
    len += 4;
  } else
    memcpy((uint8_t *)buff + 10, data, len);
  return len + 10;
}

static void websocket_write_impl(intptr_t fd, void *data, size_t len,
                                 char text, /* TODO: add client masking */
                                 char first, char last, char client) {
  if (len < 126) {
    sock_buffer_s *sbuff = sock_buffer_checkout();
    sbuff->len =
        websocket_encode(sbuff->buf, data, len, text, first, last, client);
    sock_buffer_send(fd, sbuff);
    // // was:
    // char buff[len + (client ? 6 : 2)];
    // len = websocket_encode(buff, data, len, text, first, last, client);
    // sock_write(fd, buff, len);
  } else if (len <= WS_MAX_FRAME_SIZE) {
    if (len >= BUFFER_PACKET_SIZE - 8) { // can't feet in sock buffer
      /* head MUST be 4 bytes */
      void *buff = malloc(len + 4);
      len = websocket_encode(buff, data, len, text, first, last, client);
      sock_write2(.uuid = fd, .buffer = buff, .length = len, .move = 1);
    } else {
      sock_buffer_s *sbuff = sock_buffer_checkout();
      sbuff->len =
          websocket_encode(sbuff->buf, data, len, text, first, last, client);
      sock_buffer_send(fd, sbuff);
    }
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
  uint32_t type;
  while (len) {
    type = utf8d[*str];
    state = utf8d[256 + state * 16 + type];
    if (state == UTF8_REJECT)
      return 0;
    len--;
    str++;
  }
  return state == 0;
}
/* *****************************************************************************
Subscription handling
***************************************************************************** */

static void websocket_on_pubsub_message_direct(pubsub_message_s *msg) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_WRITE);
  if (!pr) {
    if (errno == EBADF)
      return;
    pubsub_defer(msg);
    return;
  }
  websocket_write((ws_s *)pr, msg->msg.data, msg->msg.len,
                  msg->msg.len >= (2 << 14)
                      ? 0
                      : validate_utf8((uint8_t *)msg->msg.data, msg->msg.len));
  facil_protocol_unlock(pr, FIO_PR_LOCK_WRITE);
}

static void websocket_on_pubsub_message_direct_txt(pubsub_message_s *msg) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_WRITE);
  if (!pr) {
    if (errno == EBADF)
      return;
    pubsub_defer(msg);
    return;
  }
  websocket_write((ws_s *)pr, msg->msg.data, msg->msg.len, 1);
  facil_protocol_unlock(pr, FIO_PR_LOCK_WRITE);
}

static void websocket_on_pubsub_message_direct_bin(pubsub_message_s *msg) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_WRITE);
  if (!pr) {
    if (errno == EBADF)
      return;
    pubsub_defer(msg);
    return;
  }
  websocket_write((ws_s *)pr, msg->msg.data, msg->msg.len, 0);
  facil_protocol_unlock(pr, FIO_PR_LOCK_WRITE);
}

static void websocket_on_pubsub_message(pubsub_message_s *msg) {
  protocol_s *pr =
      facil_protocol_try_lock((intptr_t)msg->udata1, FIO_PR_LOCK_TASK);
  if (!pr) {
    if (errno == EBADF)
      return;
    pubsub_defer(msg);
    return;
  }

  websocket_write((ws_s *)pr, msg->msg.data, msg->msg.len, 0);
  facil_protocol_unlock(pr, FIO_PR_LOCK_TASK);
}

/**
 * Returns a subscription ID on success and 0 on failure.
 */
#undef websocket_subscribe
uintptr_t websocket_subscribe(struct websocket_subscribe_s args) {
  pubsub_sub_pt sub = pubsub_subscribe(
          .engine = args.engine,
          .channel =
              {
                  .name = (char *)args.channel.name, .len = args.channel.len,
              },
          .on_message =
              (args.on_message
                   ? websocket_on_pubsub_message
                   : args.force_binary
                         ? websocket_on_pubsub_message_direct_bin
                         : args.force_text
                               ? websocket_on_pubsub_message_direct_txt
                               : websocket_on_pubsub_message_direct),
          .udata1 = (void *)args.ws->fd, .udata2 = args.udata);
  if (!sub)
    return 0;
  subscription_s *s = create_subscription(args.ws, sub);
  return (uintptr_t)s;
}

/**
 * Returns the existing subscription's ID (if exists) or 0 (no subscription).
 */
#undef websocket_find_sub
uintptr_t websocket_find_sub(struct websocket_subscribe_s args) {
  pubsub_sub_pt sub = pubsub_find_sub(
          .engine = args.engine,
          .channel =
              {
                  .name = (char *)args.channel.name, .len = args.channel.len,
              },
          .on_message =
              (args.on_message
                   ? websocket_on_pubsub_message
                   : args.force_binary
                         ? websocket_on_pubsub_message_direct_bin
                         : args.force_text
                               ? websocket_on_pubsub_message_direct_txt
                               : websocket_on_pubsub_message_direct),
          .udata1 = (void *)args.ws->fd, .udata2 = args.udata);
  if (!sub)
    return 0;
  subscription_s *s;
  fio_list_for_each(subscription_s, node, s, args.ws->subscriptions) {
    if (s->sub == sub)
      return (uintptr_t)s;
  }
  return 0;
}

/**
 * Unsubscribes from a channel.
 */
void websocket_unsubscribe(ws_s *ws, uintptr_t subscription_id) {
  subscription_s *s;
  fio_list_for_each(subscription_s, node, s, ws->subscriptions) {
    if (s == (subscription_s *)subscription_id) {
      pubsub_unsubscribe(s->sub);
      free_subscription(s);
      return;
    }
  }
}

/*******************************************************************************
The API implementation
*/

/** The upgrade */
#undef websocket_upgrade
ssize_t websocket_upgrade(websocket_settings_s settings) {
  // A static data used for all websocket connections.
  static char ws_key_accpt_str[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  // require either a request or a response.
  if (((uintptr_t)settings.request | (uintptr_t)settings.response) ==
      (uintptr_t)NULL)
    return -1;
  if (settings.max_msg_size == 0)
    settings.max_msg_size = 262144; /** defaults to ~250KB */
  if (settings.timeout == 0)
    settings.timeout = 40; /* defaults to 40 seconds */
  // make sure we have a response object.
  http_response_s *response = settings.response;
  if (response == NULL) {
    /* initialize a default upgrade response */
    response = http_response_create(settings.request);
  } else
    settings.request = response->request;
  // allocate the protocol object (TODO: (maybe) pooling)
  ws_s *ws = new_websocket();
  if (!ws)
    goto refuse;

  // setup the socket-server data
  ws->fd = response->request->fd;
  // Setup ws callbacks
  ws->on_close = settings.on_close;
  ws->on_message = settings.on_message;
  ws->on_ready = settings.on_ready;
  ws->on_shutdown = settings.on_shutdown;
  // setup any user data
  ws->udata = settings.udata;
  // buffer limits
  ws->max_msg_size = settings.max_msg_size;
  const char *recv_str;

  recv_str =
      http_request_header_find(settings.request, "sec-websocket-version", 21)
          .value;
  if (recv_str == NULL || recv_str[0] != '1' || recv_str[1] != '3')
    goto refuse;

  http_header_s sec_h =
      http_request_header_find(settings.request, "sec-websocket-key", 17);
  if (sec_h.value == NULL)
    goto refuse;

  // websocket extentions (none)

  // the accept Base64 Hash - we need to compute this one and set it
  // the client's unique string
  // use the SHA1 methods provided to concat the client string and hash
  sha1_s sha1;
  sha1 = bscrypt_sha1_init();
  bscrypt_sha1_write(&sha1, sec_h.value, sec_h.value_len);
  bscrypt_sha1_write(&sha1, ws_key_accpt_str, sizeof(ws_key_accpt_str) - 1);
  // base encode the data
  char websockets_key[32];
  int len =
      bscrypt_base64_encode(websockets_key, bscrypt_sha1_result(&sha1), 20);

  // websocket extentions (none)

  // upgrade taking place, make sure the upgrade headers are valid for the
  // response.
  response->status = 101;
  http_response_write_header(response, .name = "Connection", .name_len = 10,
                             .value = "Upgrade", .value_len = 7);
  http_response_write_header(response, .name = "Upgrade", .name_len = 7,
                             .value = "websocket", .value_len = 9);
  http_response_write_header(response, .name = "sec-websocket-version",
                             .name_len = 21, .value = "13", .value_len = 2);
  // set the string's length and encoding
  http_response_write_header(response, .name = "Sec-WebSocket-Accept",
                             .name_len = 20, .value = websockets_key,
                             .value_len = len);
  // // inform about 0 extension support
  // sec_h = http_request_header_find(settings.request,
  // "sec-websocket-extensions", 24);
  // if (recv_str != NULL)
  //   http_response_write_header(response, .name =
  //   "Sec-Websocket-Extensions",
  //                              .name_length = 24);

  goto cleanup;
refuse:
  // set the negative response
  response->status = 400;
cleanup:
  if (response->status == 101) {
    // send the response
    http_response_finish(response);
    // we have an active websocket connection - prep the connection buffer
    ws->buffer = create_ws_buffer(ws);
    // update the timeout
    facil_set_timeout(ws->fd, settings.timeout);
    // call the on_open callback
    if (settings.on_open)
      settings.on_open(ws);
    // update the protocol object, cleanning up the old one
    facil_attach(ws->fd, (protocol_s *)ws);
    return 0;
  }
  http_response_finish(response);
  destroy_ws(ws);
  return -1;
}
#define websocket_upgrade(...)                                                 \
  websocket_upgrade((websocket_settings_s){__VA_ARGS__})

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
/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, void *data, size_t size, uint8_t is_text) {
  if (sock_isvalid(ws->fd)) {
    websocket_write_impl(ws->fd, data, size, is_text, 1, 1, ws->parser.client);
    return 0;
  }
  return -1;
}
/** Closes a websocket connection. */
void websocket_close(ws_s *ws) {
  sock_write2(.uuid = ws->fd, .buffer = "\x88\x00", .length = 2, .move = 1,
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
void websocket_each(struct websocket_each_args_s args) {
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
                  .task_type = FIO_PR_LOCK_WRITE);
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
  if (((ws_s *)(_ws))->parser.client != multi->as_client)
    return;
  spn_lock(&multi->lock);
  multi->count += 1;
  spn_unlock(&multi->lock);
  sock_write2(.uuid = fd, .buffer = multi->buffer, .length = multi->length,
              .dealloc = ws_reduce_or_free_multi_write, .move = 1);
}

static void ws_check_multi_write(intptr_t fd, protocol_s *_ws, void *arg) {
  struct websocket_multi_write *multi = arg;
  if (((ws_s *)(_ws))->parser.client != multi->as_client)
    return;
  if (multi->if_callback((void *)_ws, multi->arg))
    ws_direct_multi_write(fd, _ws, arg);
}

#undef websocket_write_each
int websocket_write_each(struct websocket_write_each_args_s args) {
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
      .length = websocket_encode(multi->buffer, args.data, args.length,
                                 args.is_text, 1, 1, args.as_client),
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
