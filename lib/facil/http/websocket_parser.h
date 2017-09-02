/*
copyright: Boaz Segev, 2017
license: MIT

Feel free to copy, use and enjoy according to the license specified.
*/
#ifndef H_WEBSOCKET_PARSER_H
/**\file

A single file Websocket message parser and Websocket message wrapper, decoupled
from any IO layer.

Notice that this header file library includes static funnction declerations that
must be implemented by the including file (the callbacks).

*/
#define H_WEBSOCKET_PARSER_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* *****************************************************************************
API - Internal Helpers
***************************************************************************** */

/** used internally to mask and unmask client messages. */
inline static void websocket_xmask(void *msg, uint64_t len, uint32_t mask);

/* *****************************************************************************
API - Message Wrapping
***************************************************************************** */

/** returns the length of the buffer required to wrap a message `len` long */
static inline __attribute__((unused)) uint64_t
websocket_wrapped_len(uint64_t len);

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
inline static uint64_t __attribute__((unused))
websocket_server_wrap(void *target, void *msg, uint64_t len,
                      unsigned char opcode, unsigned char first,
                      unsigned char last, unsigned char rsv);

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
 * * client: set to 1 to use client mode (data  masking).
 *
 * Returns the number of bytes written. Always `websocket_wrapped_len(len) + 4`
 */
inline static __attribute__((unused)) uint64_t
websocket_client_wrap(void *target, void *msg, uint64_t len,
                      unsigned char opcode, unsigned char first,
                      unsigned char last, unsigned char rsv);

/* *****************************************************************************
Callbacks - Required functions that must be inplemented to use this header
***************************************************************************** */

static void websocket_on_unwrapped(void *udata, void *msg, uint64_t len,
                                   char first, char last, char text,
                                   unsigned char rsv);
static void websocket_on_protocol_ping(void *udata, void *msg, uint64_t len);
static void websocket_on_protocol_pong(void *udata, void *msg, uint64_t len);
static void websocket_on_protocol_close(void *udata);
static void websocket_on_protocol_error(void *udata);

/* *****************************************************************************
API - Parsing (unwrapping)
***************************************************************************** */

/** the returned value for `websocket_buffer_required` */
struct websocket_packet_info_s {
  /** the expected packet length */
  uint64_t packet_length;
  /** the packet's "head" size (before the data) */
  uint8_t head_length;
  /** a flag indicating if the packet is masked */
  uint8_t masked;
};

/**
 * Returns all known information regarding the upcoming message.
 *
 * @returns a struct websocket_packet_info_s.
 *
 * On protocol error, the `head_length` value is 0 (no valid head detected).
 */
inline static struct websocket_packet_info_s
websocket_buffer_peek(void *buffer, uint64_t len);

/**
 * Consumes the data in the buffer, calling any callbacks required.
 *
 * Returns the remaining data in the existing buffer (can be 0).
 *
 * Notice: if there's any remaining data in the buffer, `memmove` is used to
 * place the data at the begining of the buffer.
 */
inline static __attribute__((unused)) uint64_t
websocket_consume(void *buffer, uint64_t len, void *udata,
                  uint8_t require_masking);

/* *****************************************************************************

                                Implementation

***************************************************************************** */

/* *****************************************************************************
Message masking
***************************************************************************** */
/** used internally to mask and unmask client messages. */
void websocket_xmask(void *msg, uint64_t len, uint32_t mask) {
  const uint64_t xmask = (((uint64_t)mask) << 32) | mask;
  while (len >= 8) {
    *((uint64_t *)msg) ^= xmask;
    len -= 8;
    msg = (void *)((uintptr_t)msg + 8);
  }
  switch (len) {
  case 7:
    ((uint8_t *)msg)[6] ^= ((uint8_t *)(&mask))[2];
  /* fallthrough */
  case 6:
    ((uint8_t *)msg)[5] ^= ((uint8_t *)(&mask))[1];
  /* fallthrough */
  case 5:
    ((uint8_t *)msg)[4] ^= ((uint8_t *)(&mask))[0];
  /* fallthrough */
  case 4:
    ((uint8_t *)msg)[3] ^= ((uint8_t *)(&mask))[3];
  /* fallthrough */
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
static inline uint64_t websocket_wrapped_len(uint64_t len) {
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
static uint64_t websocket_server_wrap(void *target, void *msg, uint64_t len,
                                      unsigned char opcode, unsigned char first,
                                      unsigned char last, unsigned char rsv) {
  ((uint8_t *)target)[0] = 0 |
                           /* opcode */ (((first ? opcode : 0) & 15)) |
                           /* rsv */ ((rsv & 7) << 4) |
                           /*fin*/ ((last & 1) << 7);
  if (len < 126) {
    ((uint8_t *)target)[1] = len;
    memcpy(((uint8_t *)target) + 2, msg, len);
    return len + 2;
  } else if (len < (1UL << 16)) {
    /* head is 4 bytes */
    ((uint8_t *)target)[1] = 126;
    ((uint16_t *)target)[1] = htons(len);
    memcpy((uint8_t *)target + 4, msg, len);
    return len + 4;
  }
  /* Really Long Message  */
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
static uint64_t websocket_client_wrap(void *target, void *msg, uint64_t len,
                                      unsigned char opcode, unsigned char first,
                                      unsigned char last, unsigned char rsv) {
  uint32_t mask = rand() + 0x01020408;
  ((uint8_t *)target)[0] = 0 |
                           /* opcode */ (((first ? opcode : 0) & 15)) |
                           /* rsv */ ((rsv & 7) << 4) |
                           /*fin*/ ((last & 1) << 7);
  if (len < 126) {
    ((uint8_t *)target)[1] = len | 128;
    ((uint32_t *)((uint8_t *)target + 2))[0] = mask;
    memcpy(((uint8_t *)target) + 6, msg, len);
    websocket_xmask((uint8_t *)target + 6, len, mask);
    return len + 6;
  } else if (len < (1UL << 16)) {
    /* head is 4 bytes */
    ((uint8_t *)target)[1] = 126 | 128;
    ((uint16_t *)target)[1] = htons(len);
    ((uint32_t *)((uint8_t *)target + 4))[0] = mask;
    memcpy((uint8_t *)target + 8, msg, len);
    websocket_xmask((uint8_t *)target + 8, len, mask);
    return len + 8;
  }
  /* Really Long Message  */
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
 * Returns all known information regarding the upcoming message.
 *
 * @returns a struct websocket_packet_info_s.
 *
 * On protocol error, the `head_length` value is 0 (no valid head detected).
 */
inline static struct websocket_packet_info_s
websocket_buffer_peek(void *buffer, uint64_t len) {
  if (len < 2)
    return (struct websocket_packet_info_s){0, 2, 0};
  const uint8_t mask_f = (((uint8_t *)buffer)[1] >> 7) & 1;
  const uint8_t mask_l = (mask_f << 2);
  uint8_t len_indicator = (((uint8_t *)buffer)[1] & 127);
  if (len < 126)
    return (struct websocket_packet_info_s){len_indicator,
                                            (uint8_t)(2 + mask_l), mask_f};
  switch (len_indicator) {
  case 126:
    if (len < 4)
      return (struct websocket_packet_info_s){0, (uint8_t)(4 + mask_l), mask_f};
    return (struct websocket_packet_info_s){htons(((uint16_t *)buffer)[1]),
                                            (uint8_t)(2 + mask_l), mask_f};
  case 127:
    if (len < 10)
      return (struct websocket_packet_info_s){0, (uint8_t)(10 + mask_l),
                                              mask_f};
    return (struct websocket_packet_info_s){
        bswap64(((uint64_t *)((uint8_t *)buffer + 2))[0]),
        (uint8_t)(10 + mask_l), mask_f};
  default:
    return (struct websocket_packet_info_s){0, 0, 0};
  }
}

/**
 * Consumes the data in the buffer, calling any callbacks required.
 *
 * Returns the remaining data in the existing buffer (can be 0).
 */
static uint64_t websocket_consume(void *buffer, uint64_t len, void *udata,
                                  uint8_t require_masking) {
  struct websocket_packet_info_s info = websocket_buffer_peek(buffer, len);
  if (info.head_length + info.packet_length > len)
    return len;
  uint64_t reminder = len;
  uint8_t *pos = (uint8_t *)buffer;
  while (info.head_length + info.packet_length <= reminder) {
    /* parse head */
    void *payload = (void *)(pos + info.head_length);
    /* unmask? */
    if (info.masked) {
      /* masked */
      const uint32_t mask = ((uint32_t *)payload)[-1];
      websocket_xmask(payload, info.packet_length, mask);
    } else if (require_masking) {
      /* error */
      websocket_on_protocol_error(udata);
    }
    /* call callback */
    switch (pos[0] & 15) {
    case 0:
      /* continuation frame */
      websocket_on_unwrapped(udata, payload, info.packet_length, 0,
                             ((pos[0] >> 7) & 1), 0, ((pos[0] >> 4) & 7));
      break;
    case 1:
      /* text frame */
      websocket_on_unwrapped(udata, payload, info.packet_length, 1,
                             ((pos[0] >> 7) & 1), 1, ((pos[0] >> 4) & 7));
      break;
    case 2:
      /* data frame */
      websocket_on_unwrapped(udata, payload, info.packet_length, 1,
                             ((pos[0] >> 7) & 1), 0, ((pos[0] >> 4) & 7));
      break;
    case 8:
      /* close frame */
      websocket_on_protocol_close(udata);
      break;
    case 9:
      /* ping frame */
      websocket_on_protocol_ping(udata, payload, info.packet_length);
      break;
    case 10:
      /* pong frame */
      websocket_on_protocol_pong(udata, payload, info.packet_length);
      break;
    default:
      websocket_on_protocol_error(udata);
    }
    /* step forward */
    reminder -= info.head_length + info.packet_length;
    pos += info.head_length + info.packet_length;
    info = websocket_buffer_peek(pos, reminder);
  }
  /* reset buffer state - support pipelining */
  if (!reminder)
    return 0;
  memmove(buffer, (uint8_t *)buffer + len - reminder, reminder);
  return reminder;
}

#endif
