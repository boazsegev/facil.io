#ifndef H_HTTP2_PARSER_H
/*
Copyright: Devstroop, 2026
License: MIT

Feel free to copy, use and enjoy according to the license provided.

This is a callback based parser for the HTTP/2 framing layer (RFC 9113 §4-6).
It parses the 9 byte frame header and frame payloads, leaving validation and
stream / connection state handling to the callbacks (see http2.c).
*/
#define H_HTTP2_PARSER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Frame type and flag constants (RFC 9113 §6)
***************************************************************************** */

#define HTTP2_FRAME_DATA 0x0
#define HTTP2_FRAME_HEADERS 0x1
#define HTTP2_FRAME_PRIORITY 0x2
#define HTTP2_FRAME_RST_STREAM 0x3
#define HTTP2_FRAME_SETTINGS 0x4
#define HTTP2_FRAME_PUSH_PROMISE 0x5
#define HTTP2_FRAME_PING 0x6
#define HTTP2_FRAME_GOAWAY 0x7
#define HTTP2_FRAME_WINDOW_UPDATE 0x8
#define HTTP2_FRAME_CONTINUATION 0x9

#define HTTP2_FLAG_END_STREAM 0x1
#define HTTP2_FLAG_ACK 0x1
#define HTTP2_FLAG_END_HEADERS 0x4
#define HTTP2_FLAG_PADDED 0x8
#define HTTP2_FLAG_PRIORITY 0x20

/* error codes (RFC 9113 §7) */
#define HTTP2_ERROR_NO_ERROR 0x0
#define HTTP2_ERROR_PROTOCOL 0x1
#define HTTP2_ERROR_INTERNAL 0x2
#define HTTP2_ERROR_FLOW_CONTROL 0x3
#define HTTP2_ERROR_SETTINGS_TIMEOUT 0x4
#define HTTP2_ERROR_STREAM_CLOSED 0x5
#define HTTP2_ERROR_FRAME_SIZE 0x6
#define HTTP2_ERROR_REFUSED_STREAM 0x7
#define HTTP2_ERROR_CANCEL 0x8
#define HTTP2_ERROR_COMPRESSION 0x9
#define HTTP2_ERROR_CONNECT 0xa
#define HTTP2_ERROR_ENHANCE_YOUR_CALM 0xb
#define HTTP2_ERROR_INADEQUATE_SECURITY 0xc
#define HTTP2_ERROR_HTTP_1_1_REQUIRED 0xd

/* settings identifiers (RFC 9113 §6.5.2) */
#define HTTP2_SETTING_HEADER_TABLE_SIZE 0x1
#define HTTP2_SETTING_ENABLE_PUSH 0x2
#define HTTP2_SETTING_MAX_CONCURRENT_STREAMS 0x3
#define HTTP2_SETTING_INITIAL_WINDOW_SIZE 0x4
#define HTTP2_SETTING_MAX_FRAME_SIZE 0x5
#define HTTP2_SETTING_MAX_HEADER_LIST_SIZE 0x6

/* defaults */
#define HTTP2_DEFAULT_WINDOW 65535
#define HTTP2_DEFAULT_FRAME_SIZE 16384
#define HTTP2_DEFAULT_HEADER_LIST_SIZE 65536

/* the connection preface (RFC 9113 §3.5) */
#define HTTP2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define HTTP2_PREFACE_LEN 24

/* *****************************************************************************
Parser Settings
***************************************************************************** */

#ifndef HTTP2_PARSER_BUFFER
/**
 * The size of the internal buffer used to accumulate incomplete frame payloads.
 *
 * The parser accepts frames up to this size in any number of reads. Larger
 * frames are rejected at the frame header, since they could only be delivered
 * when the whole frame is available in a single `http2_parse` call (the
 * zero-copy path) - the connection layer clamps its advertised
 * `SETTINGS_MAX_FRAME_SIZE` to this capacity, so this matches the default
 * `SETTINGS_MAX_FRAME_SIZE` value of 16,384.
 */
#define HTTP2_PARSER_BUFFER HTTP2_DEFAULT_FRAME_SIZE
#endif

/* *****************************************************************************
Parser API
***************************************************************************** */

/** this struct contains the state of the parser. */
typedef struct http2_parser_s {
  struct http2_parser_protected_read_only_state_s {
    ssize_t read;      /* total number of bytes consumed so far */
    uint32_t length;   /* the payload length of the frame being parsed */
    uint8_t type;      /* the frame type of the frame being parsed */
    uint8_t flags;     /* the frame flags of the frame being parsed */
    uint32_t stream;   /* the stream id of the frame being parsed (31 bits) */
    uint32_t max_frame; /* the maximum allowed frame payload length (24-bit) */
    uint8_t stage;     /* 0 = expecting a frame header, 1 = expecting payload */
    uint8_t head[9];   /* partial frame header */
    uint8_t head_len;  /* bytes accumulated in head */
    uint8_t buf[HTTP2_PARSER_BUFFER]; /* partial frame payload */
    size_t buf_len;    /* bytes accumulated in buf */
  } state;
} http2_parser_s;

#define HTTP2_PARSER_INIT                                                      \
  {                                                                            \
    { 0 }                                                                      \
  }

/**
 * Returns the amount of data actually consumed by the parser.
 *
 * The value 0 indicates there wasn't enough data to be parsed and the same
 * buffer (with more data) should be resubmitted.
 *
 * A value smaller than the buffer size indicates that EITHER a frame was
 * detected (and the parser stopped) OR that the leftover could not be consumed
 * because more data was required or because a parsing error occurred.
 *
 * Simply resubmit the reminder of the data to continue parsing.
 *
 * A frame callback automatically stops the parsing process, allowing the user
 * to adjust or refresh the state of the data.
 */
static size_t http2_parse(http2_parser_s *parser, void *buffer, size_t length);

/* *****************************************************************************
Required Callbacks (MUST be implemented by including file)
***************************************************************************** */

/**
 * Called when a complete frame was parsed.
 *
 * The `payload` pointer is only valid during the callback and should be copied
 * if retained. An empty payload (length 0) is reported with payload == NULL.
 *
 * Returns 0 to continue parsing, any other value to stop (the amount of data
 * consumed will be returned).
 */
static int http2_on_frame(http2_parser_s *parser, uint32_t length, uint8_t type,
                          uint8_t flags, uint32_t stream, uint8_t *payload);

/** called when a parsing error occurred (a frame header that exceeds the
 * maximum frame size). The parser will stop and no more data will be consumed.
 */
static int http2_on_error(http2_parser_s *parser);

/* *****************************************************************************
Parser Implementation
***************************************************************************** */

/** parses the 3 octet length and 4 octet stream id (big endian). */
static inline uint32_t http2_parser_be24(const uint8_t *b) {
  return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}
static inline uint32_t http2_parser_be32(const uint8_t *b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) |
         (uint32_t)b[3];
}

static size_t http2_parse(http2_parser_s *parser, void *buffer, size_t length) {
  uint8_t *data = (uint8_t *)buffer;
  size_t consumed = 0;
  if (!parser->state.max_frame)
    parser->state.max_frame = HTTP2_DEFAULT_FRAME_SIZE;
  while (length) {
    if (!parser->state.stage) {
      /* accumulate the 9 byte frame header */
      size_t need = 9 - parser->state.head_len;
      if (length < need) {
        memcpy(parser->state.head + parser->state.head_len, data, length);
        parser->state.head_len += length;
        consumed += length;
        break;
      }
      memcpy(parser->state.head + parser->state.head_len, data, need);
      data += need;
      length -= need;
      consumed += need;
      parser->state.head_len = 0;
      parser->state.length =
          http2_parser_be24(parser->state.head);
      parser->state.type = parser->state.head[3];
      parser->state.flags = parser->state.head[4];
      parser->state.stream = http2_parser_be32(parser->state.head + 5) & 0x7fffffff;
      if (parser->state.length > parser->state.max_frame) {
        parser->state.stage = 0;
        parser->state.buf_len = 0;
        http2_on_error(parser);
        return consumed;
      }
      parser->state.stage = 1;
      if (parser->state.length == 0) {
        /* zero payload - report immediately */
        parser->state.stage = 0;
        if (http2_on_frame(parser, 0, parser->state.type, parser->state.flags,
                           parser->state.stream, NULL))
          return consumed;
        continue;
      }
    }
    /* accumulate and / or report the frame payload */
    if (length >= parser->state.length - parser->state.buf_len) {
      /* the remainder of the frame is available in this buffer */
      size_t take = parser->state.length - parser->state.buf_len;
      uint8_t *payload;
      if (parser->state.buf_len) {
        /* complete the accumulated frame from the internal buffer */
        memcpy(parser->state.buf + parser->state.buf_len, data, take);
        payload = parser->state.buf;
      } else {
        /* zero-copy path - the payload is already in the input buffer */
        payload = data;
      }
      data += take;
      length -= take;
      consumed += take;
      parser->state.stage = 0;
      parser->state.buf_len = 0;
      if (http2_on_frame(parser, parser->state.length, parser->state.type,
                         parser->state.flags, parser->state.stream, payload))
        return consumed;
      continue;
    }
    /* only a partial frame is available - accumulate it. Frames that can't
     * fit the internal buffer are only delivered via the zero-copy path
     * (the whole frame in a single parse call) and are rejected here,
     * preventing an overflow of the accumulation buffer */
    if (parser->state.length > HTTP2_PARSER_BUFFER) {
      parser->state.stage = 0;
      parser->state.buf_len = 0;
      http2_on_error(parser);
      return consumed;
    }
    size_t take = length;
    memcpy(parser->state.buf + parser->state.buf_len, data, take);
    parser->state.buf_len += take;
    consumed += take;
    break;
  }
  parser->state.read += consumed;
  return consumed;
}

#endif /* H_HTTP2_PARSER_H */