/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http2_parser.h"

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) &&                 \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

#ifdef __BIG_ENDIAN__
#define b2i16(s)                                                               \
  (((uint32_t)((s)[1]) << 8) | ((uint32_t)((s)[0]))
#define b2i32(s)                                                               \
  (((uint32_t)((s)[3]) << 24) | ((uint32_t)((s)[2]) << 16) |                   \
   ((uint32_t)((s)[1]) << 8) | ((uint32_t)((s)[0])))
#else
#define b2i16(s) (((uint32_t)((s)[0]) << 8) | ((uint32_t)((s)[1])))
#define b2i32(s)                                                               \
  (((uint32_t)((s)[0]) << 24) | ((uint32_t)((s)[1]) << 16) |                   \
   ((uint32_t)((s)[2]) << 8) | ((uint32_t)((s)[3])))
#endif

/* *****************************************************************************
Parser data structure
***************************************************************************** */

/** an opaque parser pointer type for the HTTP 2 parser. */
typedef struct http2_parser_s {
  /* http2 settings. */
  const struct http2_parser_create_args_s settings;
  /* http2 persistent data. */
  uint32_t required_stream_id;
  uint32_t reserverd_local;
  uint32_t reserverd_remote;
  uint8_t end_of_stream;
  /* frame parsing state */
  struct h2parser_state_s {
    size_t expecting;
    size_t missing;
    unsigned length : 24;
    unsigned type : 8;
    unsigned flags : 8;
    unsigned rsv : 1;
    unsigned id : 31;
  } state;
  uint8_t frame[];
} http2_parser_s;

typedef struct {
  struct {
    unsigned length : 24;
    unsigned type : 8;
    unsigned flags : 8;
    unsigned rsv : 1;
    unsigned id : 31;
  } head;
  uint8_t *data;
} http2_frame_s;

/* *****************************************************************************
Mock Callbacks
***************************************************************************** */

/* *****************************************************************************
Create / Destroy / initialize
***************************************************************************** */

/** resets an HTTP/2 parser (uses reference counting). */
void http2_parser_reset(http2_parser_pt p) {
  *p = (http2_parser_s){.settings = p->settings};
}

/** creates an HTTP/2 parser */
#undef http2_parser_create
http2_parser_pt http2_parser_create(struct http2_parser_create_args_s args);

/** destroy an HTTP/2 parser (uses reference counting). */
void http2_parser_destroy(http2_parser_pt);

/* *****************************************************************************
Frame handling
***************************************************************************** */

static int h2p_unwrap_frame(http2_parser_pt p) {
  if (p->required_stream_id && p->state.type != 0x9)
    return H2ERR_PROTOCOL_ERROR;
  switch (p->state.type) {
  case 0x0: { /* Data frames */
    if (!p->state.id)
      return H2ERR_PROTOCOL_ERROR;
    uint8_t *data = p->frame;
    size_t len = p->state.length;
    if (p->state.flags & 0x8) {
      /* padding (PADDED) */
      data += p->frame[0];
      len -= p->frame[0];
    }
    p->settings.callbacks->on_body(p, p->settings.udata, p->state.id, data,
                                   len);
    if (p->state.flags & 0x1) /* final frame (END_STREAM) */
      p->settings.callbacks->on_finalized(p, p->settings.udata, p->state.id);
    break;
  }
  case 0x1: { /* Header frames */
    break;
  }
  case 0x2: { /* PRIORITY frames */
    if (!p->state.id)
      return H2ERR_PROTOCOL_ERROR;
    uint32_t dep_id = b2i32(p->frame);
    p->settings.callbacks->on_priority(p, p->settings.udata, p->state.id,
                                       (dep_id & 0x7FFF), (dep_id >> 31),
                                       p->frame[5]);
    break;
  }
  case 0x3: { /* RST_STREAM frames */
    if (!p->state.id)
      return H2ERR_PROTOCOL_ERROR;
    p->settings.callbacks->on_reset_stream(p, p->settings.udata, p->state.id,
                                           b2i32(p->frame));
    break;
  }
  case 0x4: { /* SETTINGS frames */
    if (p->state.id)
      return H2ERR_PROTOCOL_ERROR;
    p->settings.callbacks->on_settings(p, p->settings.udata, b2i16((p->frame)),
                                       b2i32((p->frame + 2)),
                                       p->state.flags & 0x1);
    break;
  }
  case 0x5: { /* PUSH_PROMISE frames */
    break;
  }
  case 0x6: { /* PING frames */
    if (p->state.length != 8)
      return H2ERR_FRAME_SIZE_ERROR;
    if (p->state.id)
      return H2ERR_PROTOCOL_ERROR;
    union {
      uint64_t i;
      uint8_t s[8];
    } payload;
    for (size_t i = 0; i < 8; i++) {
      payload.s[i] = p->frame[i];
    }
    p->settings.callbacks->on_ping(p, p->settings.udata, payload.i,
                                   p->state.flags & 0x1);
    break;
  }
  case 0x7: { /* GOAWAY frames */
    break;
  }
  case 0x8: { /* WINDOW_UPDATE frames */
    break;
  }
  case 0x9: { /* CONTINUATION frames */
    break;
  }
  }
  return 0;
}

/* *****************************************************************************
Feeding the parser (direct write + notification)
***************************************************************************** */

/**
 * Gets the current position in the HTTP/2 parser's buffer.
 *
 * This changes with every read according to the amount of data the parser
 * requires to complete it's next step.
 */
void *http2_parser_buffer(http2_parser_pt p) {
  return p->frame + (p->state.expecting - p->state.missing);
}

/**
 * Gets the current capacity for the HTTP/2 parser's buffer.
 *
 * This changes with every read according to the amount of data the parser
 * requires to complete it's next step.
 */
size_t http2_parser_capacity(http2_parser_pt p) { return p->state.missing; }

/**
 * Signals the parser to process incominng data `length` long.
 *
 * This updates the `http2_parser_buffer` and `http2_parser_capacity` values.
 */
enum http2_err_enum http2_parser_review(http2_parser_pt p, size_t length) {
  struct {
    union {
      uint8_t len_bytes[3];
      unsigned length : 24;
    };
    unsigned type : 8;
    unsigned flags : 8;
    union {
      uint8_t id_bytes[4];
      struct {
        unsigned rsv : 1;
        unsigned id : 31;
      };
    };
  } translator;
  enum http2_err_enum ret = H2ERR_PROTOCOL_ERROR;
  if (p->state.missing > length)
    goto error;
  p->state.missing -= length;
  if (p->state.missing)
    return H2ERR_OK;

  if (p->state.length == 0) {
/* starting a new frame */
#ifdef __BIG_ENDIAN__
    translator.len_bytes[0] = p->frame[0];
    translator.len_bytes[1] = p->frame[1];
    translator.len_bytes[2] = p->frame[2];
    translator.id_bytes[0] = p->frame[5];
    translator.id_bytes[1] = p->frame[6];
    translator.id_bytes[2] = p->frame[7];
    translator.id_bytes[3] = p->frame[8];
#else
    translator.len_bytes[0] = p->frame[2];
    translator.len_bytes[1] = p->frame[1];
    translator.len_bytes[2] = p->frame[0];
    translator.id_bytes[0] = p->frame[8];
    translator.id_bytes[1] = p->frame[7];
    translator.id_bytes[2] = p->frame[6];
    translator.id_bytes[3] = p->frame[5];
#endif
    translator.type = p->frame[3];
    translator.flags = p->frame[4];
    p->state.missing = p->state.expecting = translator.length;
    if (translator.length > p->settings.max_frame_size)
      goto error;
    if (translator.length)
      return H2ERR_OK;
  }

  /* handle frame */
  if ((ret = h2p_unwrap_frame(p)))
    goto error;
  /* reset parser */
  p->state = (struct h2parser_state_s){.expecting = 9, .missing = 9};
  return H2ERR_OK;
error:
  /* reset parser */
  p->state = (struct h2parser_state_s){.expecting = 9, .missing = 9};
  return ret;
}
