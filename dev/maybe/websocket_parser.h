/*
copyright: Boaz Segev, 2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_WEBSOCKET_PARSER_H
/**\file
An extraction of the Websocket message parser and the Websocket message
formatter from the Websocket protocol suite.
*/
#define H_WEBSOCKET_PARSER_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* *****************************************************************************
API - Internal Helpers
***************************************************************************** */

/** used internally to mask and unmask client messages. */
void websocket_xmask(void *msg, size_t len, uint32_t mask);

/* *****************************************************************************
API - Message Wrapping
***************************************************************************** */

/** returns the length of the buffer required to wrap a message `len` long */
static inline size_t websocket_wrapped_len(uint64_t len);

/**
 * Wraps a Websocket server message and writes it to the target buffer.
 *
 * The `first` and `last` flags can be used to support message fragmentation.
 *
 * * target: the target buffer to write to.
 * * msg:    the message to be wrapped.
 * * len:    the message length.
 * * text:   set to 1 to indicate this is a UTF-8 message or 0 for binary.
 * * first:  set to 1 if `msg` points the begining of the message.
 * * last:   set to 1 if `msg + len` ends the message.
 * * rsv:    accepts a 3 bit value for the rsv websocket message bits.
 *
 * Returns the number of bytes written. Always `websocket_wrapped_len(len)`
 */
static size_t websocket_server_wrap(void *target, void *msg, size_t len,
                                    char text, char first, char last,
                                    unsigned char rsv);

/**
 * Wraps a Websocket client message and writes it to the target buffer.
 *
 * The `first` and `last` flags can be used to support message fragmentation.
 *
 * * target: the target buffer to write to.
 * * msg:    the message to be wrapped.
 * * len:    the message length.
 * * text:   set to 1 to indicate this is a UTF-8 message or 0 for binary.
 * * first:  set to 1 if `msg` points the begining of the message.
 * * last:   set to 1 if `msg + len` ends the message.
 * * rsv:    accepts a 3 bit value for the rsv websocket message bits.
 *
 * Returns the number of bytes written. Always `websocket_wrapped_len(len) + 4`
 */
static size_t websocket_client_wrap(void *target, void *msg, size_t len,
                                    char text, char first, char last,
                                    unsigned char rsv);

/* *****************************************************************************
API - Parsing (unwrapping)
***************************************************************************** */

/**
 * Returns the minimal buffer required for the next (upcoming) message.
 *
 * On protocol error, the value 0 is returned (no buffer required).
 */
inline static size_t websocket_buffer_required(void *buffer, size_t len);

/**
 * This argumennt structure sets the callbacks and data used for parsing.
 */
struct websocket_consume_args_s {
  void (*on_unwrapped)(void *udata, void *msg, size_t len, char first,
                       char last, char text, unsigned char rsv);
  void (*on_ping)(void *udata, void *msg, size_t len);
  void (*on_pong)(void *udata, void *msg, size_t len);
  void (*on_close)(void *udata);
  void (*on_error)(void *udata);
  uint8_t require_masking;
};

/* *****************************************************************************

                                Implementation

***************************************************************************** */

/* *****************************************************************************
Message masking
***************************************************************************** */
/** used internally to mask and unmask client messages. */
void websocket_xmask(void *msg, size_t len, uint32_t mask) {
  const uint64_t xmask = (((uint64_t)mask) << 32) | mask;
  while (len >= 8) {
    *((uint64_t *)msg) ^= xmask;
    len -= 8;
    msg = (void *)((uintptr_t)msg + 8);
  }
  if (len >= 4) {
    *((uint32_t *)msg) ^= mask;
    len -= 4;
    msg = (void *)((uintptr_t)msg + 4);
  }
  switch (len) {
  case 3:
    ((uint8_t *)msg)[2] ^= ((uint8_t *)(&mask))[2];
  /* fallthrough */
  case 2:
    ((uint8_t *)msg)[1] ^= ((uint8_t *)(&mask))[1];
  /* fallthrough */
  case 1:
    ((uint8_t *)msg)[0] ^= ((uint8_t *)(&mask))[0];
    /* fallthrough */
  }
}

/* *****************************************************************************
Message wrapping
***************************************************************************** */

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

/** returns the length of the buffer required to wrap a message `len` long */
static inline size_t websocket_wrapped_len(uint64_t len) {
  if (len < 126)
    return len + 2;
  if (len < (1UL << 16))
    return len + 4;
  return len + 10;
}

/**
 * Wraps a Websocket server message and writes it to the target buffer.
 *
 * The `first` and `last` flags can be used to support message fragmentation.
 *
 * * target: the target buffer to write to.
 * * msg:    the message to be wrapped.
 * * len:    the message length.
 * * opcode: set to 1 for UTF-8 message, 2 for binary, etc'.
 * * first:  set to 1 if `msg` points the begining of the message.
 * * last:   set to 1 if `msg + len` ends the message.
 * * client: set to 1 to use client mode (data  masking).
 *
 * Further opcode values:
 * * %x0 denotes a continuation frame
 * *  %x1 denotes a text frame
 * *  %x2 denotes a binary frame
 * *  %x3-7 are reserved for further non-control frames
 * *  %x8 denotes a connection close
 * *  %x9 denotes a ping
 * *  %xA denotes a pong
 * *  %xB-F are reserved for further control frames
 *
 * Returns the number of bytes written. Always `websocket_wrapped_len(len)`
 */
static size_t websocket_server_wrap(void *target, void *msg, size_t len,
                                    char opcode, char first, char last,
                                    unsigned char rsv) {
  if (len < 126) {
    ((uint8_t *)target)[0] =
        /*opcode*/ ((first ? opcode : 0) << 4) |
        /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
    ((uint8_t *)target)[1] = len;
    memcpy(((uint8_t *)target) + 2, msg, len);
    return len + 2;
  } else if (len < (1UL << 16)) {
    /* head is 4 bytes */
    ((uint8_t *)target)[0] =
        /*opcode*/ ((first ? opcode : 0) << 4) |
        /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
    ((uint8_t *)target)[1] = 126;
    ((uint16_t *)target)[1] = htons(len);
    memcpy((uint8_t *)target + 4, msg, len);
    return len + 4;
  }
  /* Really Long Message  */
  ((uint8_t *)target)[0] =
      /*opcode*/ ((first ? opcode : 0) << 4) |
      /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
  ((uint8_t *)target)[1] = 127;
  ((uint64_t *)((uint8_t *)target + 2))[0] = bswap64(len);
  memcpy((uint8_t *)target + 10, msg, len);
  return len + 10;
}

/**
 * Wraps a Websocket client message and writes it to the target buffer.
 *
 * The `first` and `last` flags can be used to support message fragmentation.
 *
 * * target: the target buffer to write to.
 * * msg:    the message to be wrapped.
 * * len:    the message length.
 * * opcode: set to 1 for UTF-8 message, 2 for binary, etc'.
 * * first:  set to 1 if `msg` points the begining of the message.
 * * last:   set to 1 if `msg + len` ends the message.
 *
 * Returns the number of bytes written. Always `websocket_wrapped_len(len) + 4`
 */
static size_t websocket_client_wrap(void *target, void *msg, size_t len,
                                    char opcode, char first, char last,
                                    unsigned char rsv) {
  uint32_t mask = rand() + 0x01020408;
  if (len < 126) {
    ((uint8_t *)target)[0] =
        /*opcode*/ ((first ? opcode : 0) << 4) |
        /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
    ((uint8_t *)target)[1] = len;
    ((uint8_t *)target)[1] |= 128;
    ((uint32_t *)((uint8_t *)target + 2))[0] = mask;
    memcpy(((uint8_t *)target) + 6, msg, len);
    websocket_xmask((uint8_t *)target + 6, len, mask);
    return len + 6;
  } else if (len < (1UL << 16)) {
    /* head is 4 bytes */
    ((uint8_t *)target)[0] =
        /*opcode*/ ((first ? opcode : 0) << 4) |
        /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
    ((uint8_t *)target)[1] = 126 | 128;
    ((uint16_t *)target)[1] = htons(len);
    ((uint32_t *)((uint8_t *)target + 4))[0] = mask;
    memcpy((uint8_t *)target + 8, msg, len);
    websocket_xmask((uint8_t *)target + 8, len, mask);
    return len + 8;
  }
  /* Really Long Message  */
  ((uint8_t *)target)[0] =
      /*opcode*/ ((first ? opcode : 0) << 4) |
      /*rsv*/ ((rsv & 3) << 1) | /*fin*/ (last & 1);
  ((uint8_t *)target)[1] = 255;
  ((uint64_t *)((uint8_t *)target + 2))[0] = bswap64(len);
  ((uint32_t *)((uint8_t *)target + 10))[0] = mask;
  memcpy((uint8_t *)target + 14, msg, len);
  websocket_xmask((uint8_t *)target + 14, len, mask);
  return len + 14;
}

/* *****************************************************************************
Message unwrapping
***************************************************************************** */

/**
 * Returns the minimal buffer required for the next (upcoming) message.
 *
 * On protocol error, the value 0 is returned (no buffer required).
 */
inline static size_t websocket_buffer_required(void *buffer, size_t len) {
  if (len < 2)
    return 2;
  size_t ret = (((uint8_t *)buffer)[0] & 127);
  if (ret < 126)
    return ret;
  switch (ret) {
  case 126:
    if (len < 4)
      return 4;
    return htons(((uint16_t *)buffer)[1]);
  case 127:
    if (len < 10)
      return 10;
    return bswap64(((uint64_t *)((uint8_t *)buffer + 2))[0]);
  default:
    return 0;
  }
}

/**
 * Consumes the data in the buffer, calling any callbacks required.
 *
 * Returns the remaining data in the existing buffer (can be 0).
 */
static size_t websocket_consume(void *buffer, size_t len, void *udata,
                                struct websocket_consume_args_s args) {
  size_t border = websocket_buffer_required(buffer, len);
  size_t reminder = len;
  uint8_t *pos = buffer;
  while (border <= reminder) {
    /* parse head */
    uint64_t payload_len;
    char text, first, last;
    unsigned char rsv;
    uint8_t *payload;
    payload_len = pos[1] & 127;
    switch (payload_len) { /* length */
    case 126:
      payload = pos + 6;
      payload_len = ntohs(*((uint32_t *)(pos + 2)));
      break;
    case 127:
      payload = pos + 10;
      payload_len = bswap64(*(uint64_t *)(pos + 2));
      break;
    default:
      break;
    }
    /* unmask? */
    if (pos[1] & 128) {
      /* masked */
      const uint32_t mask = *((uint32_t *)payload);
      payload += 4;
      websocket_xmask(payload + 4, payload_len, mask);
    } else if (args.require_masking) {
      /* error */
      args.on_error(udata);
    }
    /* call callback */
    switch (pos[0] & 15) {
    case 0:
      /* continuation frame */
      args.on_unwrapped(udata, payload, payload_len, 0, (pos[0] >> 7), 0,
                        ((pos[0] >> 4) & 15));
      break;
    case 1:
      /* text frame */
      args.on_unwrapped(udata, payload, payload_len, 1, (pos[0] >> 7), 1,
                        ((pos[0] >> 4) & 15));
      break;
    case 2:
      /* data frame */
      args.on_unwrapped(udata, payload, payload_len, 1, (pos[0] >> 7), 0,
                        ((pos[0] >> 4) & 15));
      break;
    case 8:
      /* close frame */
      args.on_close(udata);
      break;
    case 9:
      /* ping frame */
      args.on_ping(udata, payload, payload_len);
      break;
    case 10:
      /* pong frame */
      args.on_pong(udata, payload, payload_len);
      break;
    default:
      args.on_error(udata);
    }
    /* step forward */
    reminder -= border;
    pos += border;
  }
  /* reset buffer state - support pipelining */
  if (!reminder)
    return 0;
  memmove(buffer, (uint8_t *)buffer + len - reminder, reminder);
  return reminder;
}

#endif
