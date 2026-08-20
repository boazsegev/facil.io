#ifndef H_HTTP2_H
/*
Copyright: Devstroop, 2026
License: MIT

Feel free to copy, use and enjoy according to the license provided.

This is a static-include implementation of the HTTP/2 connection semantics
(RFC 9113): stream state machine, flow control, SETTINGS handling, error
mapping and HEADERS + CONTINUATION reassembly.

The framing layer (http2_parser.h) delivers frames and this layer validates
them against the connection / stream state, integrates HPACK (hpack.h) and
emits outgoing frames through a user provided callback.

Usage:
  #include "http2.h"
  static int http2_connection_emit(http2_connection_s *c, uint32_t length,
                                   uint8_t type, uint8_t flags,
                                   uint32_t stream, const uint8_t *payload);
  static void http2_on_headers(http2_connection_s *c, uint32_t stream,
                               const hpack_header_s *fields, size_t count,
                               int trailers);
  ... (all required callbacks)

  http2_connection_s conn;
  http2_connection_init(&conn, HTTP2_CONNECTION_SERVER, 4096);
  // ... http2_connection_parse(&conn, data, len) per socket read ...
*/
#define H_HTTP2_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "http2_parser.h"
#include "hpack.h"

/* *****************************************************************************
Constants
***************************************************************************** */

/** connection role: server (receives odd stream ids) or client (even). */
#define HTTP2_CONNECTION_SERVER 1
#define HTTP2_CONNECTION_CLIENT 0

/** default limits (anti-DoS, see issue #2). */
#define HTTP2_CONN_DEFAULT_MAX_CONCURRENT_STREAMS 100
#define HTTP2_CONN_DEFAULT_MAX_HEADER_FIELDS 100
#define HTTP2_CONN_DEFAULT_MAX_PING_OUTSTANDING 8

/* *****************************************************************************
Types
***************************************************************************** */

typedef struct http2_connection_s http2_connection_s;
typedef struct http2_stream_s http2_stream_s;

/** stream states (RFC 9113 §5.1). */
enum {
  HTTP2_STREAM_IDLE = 0,
  HTTP2_STREAM_RESERVED_LOCAL,
  HTTP2_STREAM_RESERVED_REMOTE,
  HTTP2_STREAM_OPEN,
  HTTP2_STREAM_HALF_CLOSED_LOCAL,
  HTTP2_STREAM_HALF_CLOSED_REMOTE,
  HTTP2_STREAM_CLOSED
};

typedef struct http2_stream_s {
  uint32_t id;
  uint8_t state;
  int64_t recv_window; /* our advertised window for this stream */
  int64_t send_window; /* the peer's advertised window for this stream */
} http2_stream_s;

struct http2_connection_s {
  http2_parser_s parser;
  hpack_context_s hpack_dec; /* decodes incoming header blocks */
  hpack_context_s hpack_enc; /* encodes outgoing header blocks */
  struct {
    uint32_t header_table_size; /* SETTINGS_HEADER_TABLE_SIZE (peer's) */
    uint32_t enable_push;       /* SETTINGS_ENABLE_PUSH (peer's) */
    uint32_t max_concurrent;    /* SETTINGS_MAX_CONCURRENT_STREAMS (peer's) */
    uint32_t initial_window;    /* SETTINGS_INITIAL_WINDOW_SIZE (peer's) */
    uint32_t max_frame_size;    /* SETTINGS_MAX_FRAME_SIZE (peer's) */
    uint32_t max_header_list;   /* SETTINGS_MAX_HEADER_LIST_SIZE (peer's) */
  } peer;
  struct {
    uint32_t enable_push;        /* our SETTINGS_ENABLE_PUSH */
    uint32_t max_concurrent;     /* our SETTINGS_MAX_CONCURRENT_STREAMS */
    uint32_t initial_window;     /* our SETTINGS_INITIAL_WINDOW_SIZE */
    uint32_t max_frame_size;     /* our SETTINGS_MAX_FRAME_SIZE */
    uint32_t max_header_list;    /* our SETTINGS_MAX_HEADER_LIST_SIZE */
    uint32_t max_header_fields;  /* anti-DoS: per-message header count cap */
    uint32_t max_ping_outstanding; /* anti-DoS: unACKed PING cap */
  } local;
  struct http2_connection_protected_read_only_state_s {
    uint32_t last_local_stream;  /* highest locally initiated stream id */
    uint32_t last_remote_stream; /* highest peer initiated stream id */
    uint32_t goaway_stream;   /* last stream id in a received GOAWAY */
    int64_t recv_window;      /* our connection flow-control window */
    int64_t send_window;      /* the peer's connection flow-control window */
    size_t open_count;        /* streams in open / half-closed states */
    uint8_t role;             /* HTTP2_CONNECTION_SERVER / _CLIENT */
    uint8_t state;            /* 0 = active, 1 = goaway (sent or received),
                               * 2 = closed */
    uint8_t preface_len;      /* client preface bytes validated so far */
    uint8_t first_frame;      /* the first frame must be SETTINGS */
    uint8_t pings_outstanding; /* unACKed PINGs we sent */
    uint32_t header_block_stream; /* stream awaiting CONTINUATION (0 = none) */
    uint8_t header_block_end_stream; /* END_STREAM of the pending block */
    uint8_t header_block_push; /* the pending block is a PUSH_PROMISE */
    uint32_t header_block_promised; /* promised stream of the pending block */
  } state;
  http2_stream_s *streams; /* sorted by id, dynamic array */
  size_t stream_count;
  size_t stream_cap;
  /* caller data */
  void *udata;
};

/* *****************************************************************************
Required Callbacks (MUST be implemented by including file)
***************************************************************************** */

/** Called when a complete header block (HEADERS + CONTINUATION) was decoded.
 * The fields are only valid during the callback (they point into the HPACK
 * scratch buffer) and should be copied if retained. `trailers` is non-zero
 * for header blocks received after the request body was completed. */
static void http2_on_headers(http2_connection_s *c, uint32_t stream,
                             const hpack_header_s *fields, size_t count,
                             int trailers, int end_stream);

/** Called when a DATA frame was received (after padding stripping).
 * The data is only valid during the callback. `payload_len` is the full
 * frame payload size (including padding) that was charged to the flow
 * control windows - it should be used for the WINDOW_UPDATE replenishment.
 * `end_stream` is the flag. */
static void http2_on_data(http2_connection_s *c, uint32_t stream,
                          uint8_t *data, uint32_t length,
                          uint32_t payload_len, int end_stream);

/** Called when a PUSH_PROMISE frame was received (client role only).
 * The decoded promise header block (the promised request headers) is passed
 * in `fields` - they are only valid during the callback. */
static void http2_on_push_promise(http2_connection_s *c, uint32_t stream,
                                  uint32_t promised_stream,
                                  const hpack_header_s *fields, size_t count);

/** Called when a GOAWAY frame was received. */
static void http2_on_goaway(http2_connection_s *c, uint32_t last_stream,
                            uint32_t error);

/** Called when the peer's SETTINGS changed (after applying the values). */
static void http2_on_settings(http2_connection_s *c);

/** Called when the peer's WINDOW_UPDATE frame (or SETTINGS INITIAL_WINDOW_SIZE
 * change) replenished our send window for `stream` (0 = connection). This
 * allows the application to flush flow-control buffered data. */
static void http2_on_window_update(http2_connection_s *c, uint32_t stream,
                                   uint32_t increment);

/** Called when a frame should be sent. Returns 0 on success (any other value
 * stops the parser / aborts the operation).
 *
 * Note: `type == 0xff` is a pseudo-frame carrying raw bytes (the client
 * connection preface, 24 octets) that should be written as-is. */
static int http2_connection_emit(http2_connection_s *c, uint32_t length,
                                 uint8_t type, uint8_t flags, uint32_t stream,
                                 const uint8_t *payload);

/* *****************************************************************************
Connection API
***************************************************************************** */

/**
 * Initializes the connection object. `role` is HTTP2_CONNECTION_SERVER or
 * HTTP2_CONNECTION_CLIENT. `hpack_table_size` is the initial HPACK dynamic
 * table size (the SETTINGS_HEADER_TABLE_SIZE value).
 *
 * In server role the initial SETTINGS frame is emitted immediately (it must
 * be the first frame sent, RFC 9113 §3.5). In client role
 * `http2_connection_send_preface` should be called before any other frame.
 */
static void http2_connection_init(http2_connection_s *c, uint8_t role,
                                  size_t hpack_table_size);

/** Destroys the connection object. */
static void http2_connection_destroy(http2_connection_s *c);

/**
 * Parses incoming data. Returns the number of bytes consumed. In server role
 * the client connection preface is validated first. Connection errors result
 * in a GOAWAY frame (emitted) and the connection state is set to closed.
 */
static size_t http2_connection_parse(http2_connection_s *c, void *buffer,
                                     size_t length);

/** Sends the client connection preface followed by the initial SETTINGS frame
 * (client role only). Returns 0 on success. */
static MAYBE_UNUSED int http2_connection_send_preface(http2_connection_s *c);

/**
 * Sends a DATA frame with the payload, respecting the connection and stream
 * flow-control windows (a partial send is performed when the payload exceeds
 * the available windows). Returns the number of payload bytes emitted, 0 when
 * the flow-control windows are exhausted, or -1 on error.
 */
static ssize_t http2_connection_send_data(http2_connection_s *c,
                                          uint32_t stream, const uint8_t *data,
                                          size_t length, int end_stream);

/** Encodes and sends a header block (HEADERS + CONTINUATION frames as needed).
 * Returns 0 on success, -1 on error. */
static int http2_connection_send_headers(http2_connection_s *c,
                                         uint32_t stream,
                                         const hpack_header_s *fields,
                                         size_t count, int end_stream);

/** Encodes and sends a PUSH_PROMISE (RFC 9113 §6.6) on `stream`, promising a
 * server-initiated (even) `promised` stream. The promised request header
 * block is encoded into the connection's HPACK context, keeping it in sync
 * with subsequent header blocks. Returns 0 on success, -1 on error. */
static MAYBE_UNUSED int http2_connection_send_push_promise(http2_connection_s *c,
                                              uint32_t stream,
                                              uint32_t promised,
                                              const hpack_header_s *fields,
                                              size_t count);

/** Sends a RST_STREAM frame (and closes the stream). Returns 0 or -1. */
static MAYBE_UNUSED int http2_connection_send_rst(http2_connection_s *c, uint32_t stream,
                                     uint32_t error);

/** Sends a GOAWAY frame. No new streams will be accepted after this. */
static void http2_connection_send_goaway(http2_connection_s *c,
                                         uint32_t error);

/** Sends a PING frame with an 8 octet opaque payload. Returns 0 or -1 when
 * the outstanding PING cap was reached (anti-DoS). */
static int http2_connection_send_ping(http2_connection_s *c,
                                      const uint8_t opaque[8]);

/** Sends a WINDOW_UPDATE frame, advertising `amount` additional bytes of
 * receive capacity for the connection (stream == 0) or a stream. Returns 0
 * or -1 (window overflow is a caller error). */
static int http2_connection_window_update(http2_connection_s *c,
                                          uint32_t stream, uint32_t amount);

/** Updates the local SETTINGS and emits the change. Only the following are
 * meaningful: ENABLE_PUSH, MAX_CONCURRENT_STREAMS, INITIAL_WINDOW_SIZE,
 * MAX_FRAME_SIZE, MAX_HEADER_LIST_SIZE. */
static MAYBE_UNUSED void http2_connection_setting_set(http2_connection_s *c, uint8_t id,
                                         uint32_t value);

/** Connection error: emits a GOAWAY frame and closes the connection. */
static void http2_connection_abort(http2_connection_s *c, uint32_t error);

/* *****************************************************************************
Implementation
***************************************************************************** */

/* sends a 4 byte big-endian value in a frame payload */
static inline void http2__be32(uint8_t *dest, uint32_t v) {
  dest[0] = (uint8_t)(v >> 24);
  dest[1] = (uint8_t)(v >> 16);
  dest[2] = (uint8_t)(v >> 8);
  dest[3] = (uint8_t)v;
}

/* serializes the local SETTINGS payload */
static size_t http2__settings_payload(http2_connection_s *c, uint8_t *dest) {
  size_t len = 0;
  for (int i = 0; i < 5; ++i) {
    uint8_t id = 0;
    uint32_t val = 0;
    switch (i) {
    case 0: id = HTTP2_SETTING_ENABLE_PUSH; val = c->local.enable_push; break;
    case 1: id = HTTP2_SETTING_MAX_CONCURRENT_STREAMS; val = c->local.max_concurrent; break;
    case 2: id = HTTP2_SETTING_INITIAL_WINDOW_SIZE; val = c->local.initial_window; break;
    case 3: id = HTTP2_SETTING_MAX_FRAME_SIZE; val = c->local.max_frame_size; break;
    case 4: id = HTTP2_SETTING_MAX_HEADER_LIST_SIZE; val = c->local.max_header_list; break;
    }
    dest[len++] = 0;
    dest[len++] = id;
    http2__be32(dest + len, val);
    len += 4;
  }
  return len;
}

static void http2__emit_settings(http2_connection_s *c) {
  uint8_t payload[6 * 5];
  size_t len = http2__settings_payload(c, payload);
  http2_connection_emit(c, (uint32_t)len, HTTP2_FRAME_SETTINGS, 0, 0, payload);
}

static void http2_connection_init(http2_connection_s *c, uint8_t role,
                                  size_t hpack_table_size) {
  memset(c, 0, sizeof(*c));
  c->state.role = role;
  c->state.first_frame = 1;
  c->state.recv_window = HTTP2_DEFAULT_WINDOW;
  c->state.send_window = HTTP2_DEFAULT_WINDOW;
  c->peer.initial_window = HTTP2_DEFAULT_WINDOW;
  c->peer.max_frame_size = HTTP2_DEFAULT_FRAME_SIZE;
  c->peer.enable_push = 1; /* RFC 9113 §6.5.2 - ENABLE_PUSH defaults to 1 */
  c->local.enable_push = 0;
  c->local.max_concurrent = HTTP2_CONN_DEFAULT_MAX_CONCURRENT_STREAMS;
  c->local.initial_window = HTTP2_DEFAULT_WINDOW;
  c->local.max_frame_size = HTTP2_DEFAULT_FRAME_SIZE;
  c->local.max_header_list = HTTP2_DEFAULT_HEADER_LIST_SIZE;
  c->local.max_header_fields = HTTP2_CONN_DEFAULT_MAX_HEADER_FIELDS;
  c->local.max_ping_outstanding = HTTP2_CONN_DEFAULT_MAX_PING_OUTSTANDING;
  hpack_context_init(&c->hpack_dec, hpack_table_size);
  hpack_context_init(&c->hpack_enc, hpack_table_size);
  hpack_context_limit_set(&c->hpack_dec, c->local.max_header_list);
  c->parser.state.max_frame = c->local.max_frame_size;
  if (role == HTTP2_CONNECTION_SERVER)
    http2__emit_settings(c); /* the server's SETTINGS must be the first frame */
}

static MAYBE_UNUSED int http2_connection_send_preface(http2_connection_s *c) {
  if (c->state.role == HTTP2_CONNECTION_SERVER || c->state.state == 2)
    return -1;
  if (http2_connection_emit(c, HTTP2_PREFACE_LEN, 0xff, 0, 0,
                            (const uint8_t *)HTTP2_PREFACE))
    return -1;
  http2__emit_settings(c);
  return 0;
}

static void http2_connection_destroy(http2_connection_s *c) {
  free(c->streams);
  c->streams = NULL;
  c->stream_count = c->stream_cap = 0;
  hpack_context_destroy(&c->hpack_dec);
  hpack_context_destroy(&c->hpack_enc);
}

/* stream lookup (binary search over the sorted stream array) */
static http2_stream_s *http2__stream_find(http2_connection_s *c,
                                          uint32_t id) {
  size_t lo = 0, hi = c->stream_count;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (c->streams[mid].id < id)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < c->stream_count && c->streams[lo].id == id)
    return &c->streams[lo];
  return NULL;
}

/* inserts a stream (keeps the array sorted) */
static http2_stream_s *http2__stream_add(http2_connection_s *c, uint32_t id,
                                         uint8_t state) {
  size_t lo = 0, hi = c->stream_count;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (c->streams[mid].id < id)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < c->stream_count && c->streams[lo].id == id)
    return &c->streams[lo];
  if (c->stream_count == c->stream_cap) {
    c->stream_cap = c->stream_cap ? c->stream_cap * 2 : 8;
    c->streams = (http2_stream_s *)realloc(
        c->streams, c->stream_cap * sizeof(*c->streams));
  }
  memmove(c->streams + lo + 1, c->streams + lo,
          (c->stream_count - lo) * sizeof(*c->streams));
  ++c->stream_count;
  c->streams[lo] = (http2_stream_s){
      .id = id,
      .state = state,
      .recv_window = c->local.initial_window,
      .send_window = c->peer.initial_window,
  };
  return &c->streams[lo];
}

/* counts streams in open / half-closed states (against the concurrency cap) */
static size_t http2__stream_open_count(http2_connection_s *c) {
  size_t count = 0;
  for (size_t i = 0; i < c->stream_count; ++i) {
    uint8_t st = c->streams[i].state;
    if (st == HTTP2_STREAM_OPEN || st == HTTP2_STREAM_HALF_CLOSED_LOCAL ||
        st == HTTP2_STREAM_HALF_CLOSED_REMOTE)
      ++count;
  }
  return count;
}

/* marks a stream closed (RFC 9113 §5.1) */
static void http2__stream_close(http2_connection_s *c, uint32_t id) {
  http2_stream_s *s = http2__stream_find(c, id);
  if (s)
    s->state = HTTP2_STREAM_CLOSED;
}

static void http2_connection_abort(http2_connection_s *c, uint32_t error) {
  if (c->state.state == 2)
    return;
  uint8_t payload[8];
  http2__be32(payload, c->state.last_remote_stream);
  http2__be32(payload + 4, error);
  http2_connection_emit(c, 8, HTTP2_FRAME_GOAWAY, 0, 0, payload);
  c->state.state = 2;
}

/* stream error: emit RST_STREAM and close the stream (RFC 9113 §5.4.2) */
static void http2__stream_rst(http2_connection_s *c, uint32_t stream,
                              uint32_t error) {
  uint8_t payload[4];
  http2__be32(payload, error);
  http2_connection_emit(c, 4, HTTP2_FRAME_RST_STREAM, 0, stream, payload);
  http2__stream_close(c, stream);
}

static ssize_t http2_connection_send_data(http2_connection_s *c,
                                          uint32_t stream, const uint8_t *data,
                                          size_t length, int end_stream) {
  if (c->state.state == 2)
    return -1;
  http2_stream_s *s = http2__stream_find(c, stream);
  if (!s || (s->state != HTTP2_STREAM_OPEN &&
             s->state != HTTP2_STREAM_HALF_CLOSED_LOCAL &&
             s->state != HTTP2_STREAM_HALF_CLOSED_REMOTE))
    return -1;
  size_t limit = (size_t)c->state.send_window;
  if ((int64_t)limit > s->send_window)
    limit = (size_t)s->send_window;
  if (limit > c->peer.max_frame_size)
    limit = c->peer.max_frame_size;
  if (limit > length)
    limit = length;
  if (!limit) {
    if (end_stream && s->state != HTTP2_STREAM_CLOSED) {
      if (http2_connection_emit(c, 0, HTTP2_FRAME_DATA,
                                HTTP2_FLAG_END_STREAM, stream, NULL))
        return -1;
      if (s->state == HTTP2_STREAM_OPEN)
        s->state = HTTP2_STREAM_HALF_CLOSED_LOCAL;
      else
        s->state = HTTP2_STREAM_CLOSED;
    }
    return 0;
  }
  uint8_t flags = end_stream ? HTTP2_FLAG_END_STREAM : 0;
  if (http2_connection_emit(c, (uint32_t)limit, HTTP2_FRAME_DATA, flags,
                            stream, data))
    return -1;
  c->state.send_window -= (int64_t)limit;
  s->send_window -= (int64_t)limit;
  if (end_stream) {
    if (s->state == HTTP2_STREAM_OPEN)
      s->state = HTTP2_STREAM_HALF_CLOSED_LOCAL;
    else
      s->state = HTTP2_STREAM_CLOSED;
  }
  return (ssize_t)limit;
}

static int http2_connection_send_headers(http2_connection_s *c,
                                         uint32_t stream,
                                         const hpack_header_s *fields,
                                         size_t count, int end_stream) {
  if (c->state.state == 2)
    return -1;
  http2_stream_s *s = http2__stream_find(c, stream);
  if (!s) {
    /* the client initiates new streams with its own HEADERS */
    if (c->state.role != HTTP2_CONNECTION_CLIENT || !(stream & 1) ||
        stream <= c->state.last_local_stream || c->state.state)
      return -1;
    c->state.last_local_stream = stream;
    s = http2__stream_add(c, stream, HTTP2_STREAM_OPEN);
  }
  if (s->state != HTTP2_STREAM_OPEN &&
      s->state != HTTP2_STREAM_HALF_CLOSED_LOCAL &&
      s->state != HTTP2_STREAM_RESERVED_LOCAL &&
      s->state != HTTP2_STREAM_RESERVED_REMOTE)
    return -1;
  size_t used = 0;
  size_t cap = 16 + (count * 32);
  for (size_t i = 0; i < count; ++i) {
    cap += fields[i].name.len + fields[i].value.len;
    if (cap > ((size_t)1 << 31))
      return -1;
  }
  uint8_t *block = (uint8_t *)malloc(cap ? cap : 1);
  ssize_t blen =
      hpack_context_encode(&c->hpack_enc, block, cap, fields, count, &used);
  if (blen < 0 || (size_t)blen > cap) {
    free(block);
    return -1;
  }
  /* split the block into HEADERS + CONTINUATION frames (at least one HEADERS
   * frame is always emitted, even for an empty block) */
  size_t off = 0;
  uint8_t fflags = end_stream ? HTTP2_FLAG_END_STREAM : 0;
  if (!blen) {
    if (http2_connection_emit(c, 0, HTTP2_FRAME_HEADERS,
                              fflags | HTTP2_FLAG_END_HEADERS, stream, NULL)) {
      free(block);
      return -1;
    }
  } else {
    while (off < (size_t)blen) {
      size_t chunk = (size_t)blen - off;
      if (chunk > c->peer.max_frame_size)
        chunk = c->peer.max_frame_size;
      uint8_t ftype =
          (off == 0) ? HTTP2_FRAME_HEADERS : HTTP2_FRAME_CONTINUATION;
      uint8_t cflags = (off == 0) ? fflags : 0;
      if (off + chunk == (size_t)blen)
        cflags |= HTTP2_FLAG_END_HEADERS;
      if (http2_connection_emit(c, (uint32_t)chunk, ftype, cflags, stream,
                                block + off)) {
        free(block);
        return -1;
      }
      off += chunk;
    }
  }
  free(block);
  if (end_stream) {
    if (s->state == HTTP2_STREAM_OPEN)
      s->state = HTTP2_STREAM_HALF_CLOSED_LOCAL;
    else
      s->state = HTTP2_STREAM_CLOSED;
  } else if (s->state == HTTP2_STREAM_RESERVED_LOCAL) {
    s->state = HTTP2_STREAM_HALF_CLOSED_LOCAL;
  }
  return 0;
}

static MAYBE_UNUSED int http2_connection_send_push_promise(http2_connection_s *c,
                                              uint32_t stream,
                                              uint32_t promised,
                                              const hpack_header_s *fields,
                                              size_t count) {
  if (c->state.state == 2)
    return -1;
  /* server role only, and only when the client allows server push
   * (RFC 9113 §6.5.2, §8.4) */
  if (c->state.role != HTTP2_CONNECTION_SERVER || !c->peer.enable_push)
    return -1;
  /* promised streams are server initiated - even, unused ids */
  if (!promised || (promised & 1) || promised <= c->state.last_local_stream)
    return -1;
  /* the promise must be attached to an active client stream (§6.6) */
  http2_stream_s *s = http2__stream_find(c, stream);
  if (!s || (s->state != HTTP2_STREAM_OPEN &&
             s->state != HTTP2_STREAM_HALF_CLOSED_REMOTE))
    return -1;
  http2__stream_add(c, promised, HTTP2_STREAM_RESERVED_LOCAL);
  c->state.last_local_stream = promised;
  size_t used = 0;
  size_t cap = 16 + (count * 32);
  for (size_t i = 0; i < count; ++i) {
    cap += fields[i].name.len + fields[i].value.len;
    if (cap > ((size_t)1 << 31))
      return -1;
  }
  uint8_t *block = (uint8_t *)malloc(cap ? cap : 1);
  if (!block)
    return -1;
  ssize_t blen =
      hpack_context_encode(&c->hpack_enc, block, cap, fields, count, &used);
  if (blen < 0 || (size_t)blen > cap) {
    free(block);
    return -1;
  }
  /* split the block into PUSH_PROMISE + CONTINUATION frames (§6.6, §6.10) */
  size_t off = 0;
  do {
    size_t chunk = (size_t)blen - off;
    if (chunk > c->peer.max_frame_size)
      chunk = c->peer.max_frame_size;
    uint8_t cflags =
        (off + chunk == (size_t)blen) ? HTTP2_FLAG_END_HEADERS : 0;
    int r;
    if (off == 0) {
      /* the promised stream id prefixes the first fragment */
      uint8_t *payload = (uint8_t *)malloc(chunk + 4);
      if (!payload) {
        free(block);
        return -1;
      }
      http2__be32(payload, promised);
      memcpy(payload + 4, block + off, chunk);
      r = http2_connection_emit(c, (uint32_t)(chunk + 4),
                                HTTP2_FRAME_PUSH_PROMISE, cflags, stream,
                                payload);
      free(payload);
    } else {
      r = http2_connection_emit(c, (uint32_t)chunk, HTTP2_FRAME_CONTINUATION,
                                cflags, stream, block + off);
    }
    if (r) {
      free(block);
      return -1;
    }
    off += chunk;
  } while (off < (size_t)blen);
  free(block);
  return 0;
}

static MAYBE_UNUSED int http2_connection_send_rst(http2_connection_s *c, uint32_t stream,
                                     uint32_t error) {
  http2_stream_s *s = http2__stream_find(c, stream);
  if (!s || s->state == HTTP2_STREAM_CLOSED || s->state == HTTP2_STREAM_IDLE)
    return -1;
  uint8_t payload[4];
  http2__be32(payload, error);
  if (http2_connection_emit(c, 4, HTTP2_FRAME_RST_STREAM, 0, stream, payload))
    return -1;
  http2__stream_close(c, stream);
  return 0;
}

static MAYBE_UNUSED void http2_connection_send_goaway(http2_connection_s *c,
                                          uint32_t error) {
  if (c->state.state)
    return;
  uint8_t payload[8];
  http2__be32(payload, c->state.last_remote_stream);
  http2__be32(payload + 4, error);
  http2_connection_emit(c, 8, HTTP2_FRAME_GOAWAY, 0, 0, payload);
  c->state.state = 1;
}

static MAYBE_UNUSED int http2_connection_send_ping(http2_connection_s *c,
                                       const uint8_t opaque[8]) {
  if (c->state.state == 2)
    return -1;
  if (c->state.pings_outstanding >= c->local.max_ping_outstanding)
    return -1;
  if (http2_connection_emit(c, 8, HTTP2_FRAME_PING, 0, 0, opaque))
    return -1;
  ++c->state.pings_outstanding;
  return 0;
}

static int http2_connection_window_update(http2_connection_s *c,
                                          uint32_t stream, uint32_t amount) {
  if (c->state.state == 2)
    return -1;
  int64_t *window;
  if (!stream) {
    window = &c->state.recv_window;
  } else {
    http2_stream_s *s = http2__stream_find(c, stream);
    if (!s)
      return -1;
    window = &s->recv_window;
  }
  if (*window + (int64_t)amount > 0x7fffffff)
    return -1;
  uint8_t payload[4];
  http2__be32(payload, amount);
  if (http2_connection_emit(c, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, stream,
                            payload))
    return -1;
  *window += (int64_t)amount;
  return 0;
}

static MAYBE_UNUSED void http2_connection_setting_set(http2_connection_s *c, uint8_t id,
                                         uint32_t value) {
  if (c->state.state == 2)
    return;
  /* the parser can only buffer frames up to its internal capacity, so the
   * advertised SETTINGS_MAX_FRAME_SIZE is clamped accordingly */
  if (id == HTTP2_SETTING_MAX_FRAME_SIZE && value > HTTP2_PARSER_BUFFER)
    value = HTTP2_PARSER_BUFFER;
  uint8_t payload[6];
  payload[0] = 0;
  payload[1] = id;
  http2__be32(payload + 2, value);
  http2_connection_emit(c, 6, HTTP2_FRAME_SETTINGS, 0, 0, payload);
  switch (id) {
  case HTTP2_SETTING_ENABLE_PUSH:
    c->local.enable_push = value;
    break;
  case HTTP2_SETTING_MAX_CONCURRENT_STREAMS:
    c->local.max_concurrent = value;
    break;
  case HTTP2_SETTING_INITIAL_WINDOW_SIZE:
    c->local.initial_window = value;
    break;
  case HTTP2_SETTING_MAX_FRAME_SIZE:
    c->local.max_frame_size = value;
    c->parser.state.max_frame = value;
    break;
  case HTTP2_SETTING_MAX_HEADER_LIST_SIZE:
    c->local.max_header_list = value;
    hpack_context_limit_set(&c->hpack_dec, value);
    break;
  }
}

/* verifies the received frame against the stream state (RFC 9113 §5.1).
 * Returns 0 = allowed, 1 = stream error (RST_STREAM), 2 = connection error */
static int http2__validate_frame(http2_connection_s *c, http2_stream_s *s,
                                 uint8_t type) {
  switch (s ? s->state : HTTP2_STREAM_IDLE) {
  case HTTP2_STREAM_IDLE:
    if (type == HTTP2_FRAME_HEADERS || type == HTTP2_FRAME_PRIORITY)
      return 0;
    if (c->state.role == HTTP2_CONNECTION_CLIENT &&
        type == HTTP2_FRAME_PUSH_PROMISE)
      return 0;
    return 2; /* any other frame on an idle stream */
  case HTTP2_STREAM_OPEN:
    if (type == HTTP2_FRAME_DATA || type == HTTP2_FRAME_HEADERS ||
        type == HTTP2_FRAME_PRIORITY || type == HTTP2_FRAME_RST_STREAM ||
        type == HTTP2_FRAME_WINDOW_UPDATE || type == HTTP2_FRAME_CONTINUATION)
      return 0;
    if (c->state.role == HTTP2_CONNECTION_CLIENT &&
        type == HTTP2_FRAME_PUSH_PROMISE)
      return 0;
    return 2;
  case HTTP2_STREAM_HALF_CLOSED_LOCAL:
    if (type == HTTP2_FRAME_DATA || type == HTTP2_FRAME_HEADERS ||
        type == HTTP2_FRAME_PRIORITY || type == HTTP2_FRAME_RST_STREAM ||
        type == HTTP2_FRAME_WINDOW_UPDATE || type == HTTP2_FRAME_CONTINUATION)
      return 0;
    if (c->state.role == HTTP2_CONNECTION_CLIENT &&
        type == HTTP2_FRAME_PUSH_PROMISE)
      return 0;
    return 2;
  case HTTP2_STREAM_HALF_CLOSED_REMOTE:
    if (type == HTTP2_FRAME_PRIORITY || type == HTTP2_FRAME_RST_STREAM ||
        type == HTTP2_FRAME_WINDOW_UPDATE)
      return 0;
    if (type == HTTP2_FRAME_DATA || type == HTTP2_FRAME_HEADERS ||
        type == HTTP2_FRAME_CONTINUATION)
      return 1; /* STREAM_CLOSED */
    return 2;
  case HTTP2_STREAM_RESERVED_REMOTE:
    if (type == HTTP2_FRAME_HEADERS || type == HTTP2_FRAME_PRIORITY ||
        type == HTTP2_FRAME_RST_STREAM)
      return 0;
    return 2;
  case HTTP2_STREAM_RESERVED_LOCAL:
    if (type == HTTP2_FRAME_PRIORITY || type == HTTP2_FRAME_RST_STREAM ||
        type == HTTP2_FRAME_WINDOW_UPDATE)
      return 0;
    if (type == HTTP2_FRAME_DATA || type == HTTP2_FRAME_HEADERS ||
        type == HTTP2_FRAME_CONTINUATION)
      return 1; /* STREAM_CLOSED */
    return 2;
  case HTTP2_STREAM_CLOSED:
    if (type == HTTP2_FRAME_PRIORITY || type == HTTP2_FRAME_RST_STREAM ||
        type == HTTP2_FRAME_WINDOW_UPDATE)
      return 0;
    if (type == HTTP2_FRAME_DATA || type == HTTP2_FRAME_HEADERS ||
        type == HTTP2_FRAME_CONTINUATION)
      return 1; /* STREAM_CLOSED */
    return 2;
  }
  return 2;
}

/* applies a received SETTINGS payload (RFC 9113 §6.5.2) */
static int http2__apply_settings(http2_connection_s *c, const uint8_t *payload,
                                 uint32_t length) {
  for (uint32_t i = 0; i < length; i += 6) {
    uint16_t id = (uint16_t)((payload[i] << 8) | payload[i + 1]);
    uint32_t value = http2_parser_be32(payload + i + 2);
    switch (id) {
    case HTTP2_SETTING_HEADER_TABLE_SIZE:
      /* the value limits our encoder's dynamic table (RFC 9113 §6.5.2). It
       * is clamped to the protocol-enforced maximum - the encoder may use a
       * smaller table than the peer allows (RFC 7541 §4.2) - and the HPACK
       * update result is propagated (it can only fail if the clamp was
       * bypassed). */
      c->peer.header_table_size = value;
      if (value > c->hpack_enc.dyn_protocol)
        value = (uint32_t)c->hpack_enc.dyn_protocol;
      if (hpack_context_update(&c->hpack_enc, value))
        return HTTP2_ERROR_PROTOCOL;
      break;
    case HTTP2_SETTING_ENABLE_PUSH:
      if (value > 1)
        return HTTP2_ERROR_PROTOCOL;
      /* a server must not send SETTINGS_ENABLE_PUSH with a value of 1
       * (RFC 9113 §6.5.2) */
      if (c->state.role == HTTP2_CONNECTION_CLIENT && value == 1)
        return HTTP2_ERROR_PROTOCOL;
      c->peer.enable_push = value;
      break;
    case HTTP2_SETTING_MAX_CONCURRENT_STREAMS:
      c->peer.max_concurrent = value;
      break;
    case HTTP2_SETTING_INITIAL_WINDOW_SIZE:
      if (value > 0x7fffffff)
        return HTTP2_ERROR_FLOW_CONTROL;
      {
        int64_t delta = (int64_t)value - (int64_t)c->peer.initial_window;
        c->peer.initial_window = value;
        for (size_t i2 = 0; i2 < c->stream_count; ++i2) {
          if (c->streams[i2].state != HTTP2_STREAM_CLOSED &&
              c->streams[i2].state != HTTP2_STREAM_IDLE) {
            c->streams[i2].send_window += delta;
            if (c->streams[i2].send_window < 0)
              return HTTP2_ERROR_FLOW_CONTROL;
            if (delta > 0)
              http2_on_window_update(c, c->streams[i2].id,
                                     (uint32_t)delta);
          }
        }
      }
      break;
    case HTTP2_SETTING_MAX_FRAME_SIZE:
      if (value < HTTP2_DEFAULT_FRAME_SIZE || value > 0xffffff)
        return HTTP2_ERROR_PROTOCOL;
      c->peer.max_frame_size = value;
      break;
    case HTTP2_SETTING_MAX_HEADER_LIST_SIZE:
      c->peer.max_header_list = value;
      break;
    }
  }
  return 0;
}

/* the first frame must be SETTINGS (RFC 9113 §3.5) */
static int http2__check_first_frame(http2_connection_s *c, uint8_t type) {
  if (!c->state.first_frame)
    return 0;
  c->state.first_frame = 0;
  if (type != HTTP2_FRAME_SETTINGS)
    return HTTP2_ERROR_PROTOCOL;
  return 0;
}

/* connection-specific header fields must not be sent over HTTP/2
 * (RFC 9113 §8.2.2) */
static int http2__field_connection_specific(const hpack_header_s *f) {
  static const char *const banned[] = {"connection", "keep-alive",
                                       "proxy-connection", "transfer-encoding",
                                       "upgrade"};
  size_t n = f->name.len;
  if (!n)
    return 0;
  for (size_t i = 0; i < sizeof(banned) / sizeof(*banned); ++i) {
    size_t bl = strlen(banned[i]);
    if (n == bl && !memcmp(f->name.data, banned[i], bl))
      return 1;
  }
  /* TE is only allowed with the value "trailers" (RFC 9113 §8.2.2) */
  if (n == 2 && !memcmp(f->name.data, "te", 2))
    return !(f->value.len == 7 && !memcmp(f->value.data, "trailers", 7));
  return 0;
}

/* the RFC 7541 §4.1 header-list size (32 + name + value per field) */
static uint64_t http2__header_list_size(const hpack_header_s *fields,
                                        size_t count) {
  uint64_t total = 0;
  for (size_t i = 0; i < count; ++i) {
    total += 32 + fields[i].name.len + fields[i].value.len;
    if (total > 0xffffffff)
      return total; /* saturate */
  }
  return total;
}

/* validates a decoded header block (RFC 9113 §8.1.2, §8.2, §8.3.1).
 * kind: 0 = request (server role), 1 = response (client role),
 *       2 = push promise request (client role), 3 = trailers.
 * Returns 0 = ok, 1 = malformed (stream error PROTOCOL),
 *         2 = connection error PROTOCOL. */
static int http2__validate_headers(http2_connection_s *c,
                                   const hpack_header_s *fields, size_t count,
                                   int kind) {
  (void)c;
  uint8_t pseudo_end = 0;
  uint8_t has_method = 0, has_scheme = 0, has_path = 0, has_authority = 0,
          has_status = 0, connect = 0;
  const char *authority_data = NULL;
  size_t authority_len = 0;
  for (size_t i = 0; i < count; ++i) {
    fio_str_info_s n = fields[i].name;
    fio_str_info_s v = fields[i].value;
    if (!n.data || !n.len)
      return 1; /* empty field name */
    if (n.data[0] == ':') {
      /* pseudo-headers must precede regular fields (RFC 9113 §8.1.2.1) */
      if (pseudo_end || kind == 3)
        return 1;
      if (n.len == 7 && !memcmp(n.data, ":method", 7)) {
        if (has_method)
          return 1;
        has_method = 1;
        if (v.len == 6 && !memcmp(v.data, "CONNECT", 6))
          connect = 1;
      } else if (n.len == 5 && !memcmp(n.data, ":path", 5)) {
        if (has_path || kind == 1)
          return 1;
        has_path = 1;
      } else if (n.len == 7 && !memcmp(n.data, ":scheme", 7)) {
        if (has_scheme || kind == 1)
          return 1;
        has_scheme = 1;
      } else if (n.len == 10 && !memcmp(n.data, ":authority", 10)) {
        if (has_authority)
          return 1;
        has_authority = 1;
        authority_data = v.data;
        authority_len = v.len;
      } else if (n.len == 7 && !memcmp(n.data, ":status", 7)) {
        if (has_status || (kind != 1))
          return 1;
        /* :status must be a 3 digit code (RFC 9113 §8.3.2) */
        if (v.len != 3 || v.data[0] < '0' || v.data[0] > '9' ||
            v.data[1] < '0' || v.data[1] > '9' || v.data[2] < '0' ||
            v.data[2] > '9')
          return 1;
        has_status = 1;
      } else {
        return 1; /* unknown pseudo-header */
      }
    } else {
      /* field names are lowercase in HTTP/2 (RFC 9113 §8.1.2) */
      for (size_t j = 0; j < n.len; ++j)
        if (n.data[j] >= 'A' && n.data[j] <= 'Z')
          return 1;
      /* connection-specific fields are a connection error (RFC 9113 §8.2.2) */
      if (http2__field_connection_specific(fields + i))
        return 2;
      /* a Host field must match :authority (RFC 9113 §8.3.1) */
      if ((kind == 0 || kind == 2) && n.len == 4 &&
          !memcmp(n.data, "host", 4)) {
        if (has_authority &&
            (authority_len != v.len ||
             memcmp(authority_data, v.data, v.len)))
          return 1;
      }
      pseudo_end = 1;
    }
  }
  /* mandatory pseudo-headers (RFC 9113 §8.3.1, §8.5) */
  if (kind == 0 || kind == 2) {
    if (connect)
      return (!has_method || !has_authority || has_scheme || has_path) ? 1 : 0;
    if (!has_method || !has_scheme || !has_path)
      return 1;
  } else if (kind == 1) {
    if (!has_status)
      return 1;
  }
  return 0;
}

/* handles a complete header block (HEADERS + CONTINUATION) */
static int http2__handle_header_block(http2_connection_s *c, uint32_t stream,
                                      const uint8_t *data, uint32_t length,
                                      int end_stream, int trailers) {
  hpack_header_s fields[256];
  size_t field_count = 0;
  if (hpack_context_decode(&c->hpack_dec, data, length, fields, 256,
                           &field_count)) {
    return HTTP2_ERROR_COMPRESSION; /* connection error (RFC 9113 §4.3) */
  }
  if (field_count > c->local.max_header_fields) {
    http2__stream_rst(c, stream, HTTP2_ERROR_ENHANCE_YOUR_CALM);
    return 0;
  }
  /* the header-list size is measured like the HPACK dynamic table entries,
   * 32 + name + value per field (RFC 9113 §10.5.1) */
  if (http2__header_list_size(fields, field_count) > c->local.max_header_list) {
    http2__stream_rst(c, stream, HTTP2_ERROR_ENHANCE_YOUR_CALM);
    return 0;
  }
  /* validate the decoded fields (RFC 9113 §8.1.2, §8.2, §8.3.1) */
  int kind;
  if (c->state.role == HTTP2_CONNECTION_CLIENT) {
    int has_pseudo = 0;
    for (size_t i = 0; i < field_count; ++i)
      if (fields[i].name.len && fields[i].name.data[0] == ':') {
        has_pseudo = 1;
        break;
      }
    kind = has_pseudo ? 1 : 3; /* response or trailers */
  } else {
    kind = trailers ? 3 : 0; /* trailers or request */
  }
  int vr = http2__validate_headers(c, fields, field_count, kind);
  if (vr == 1) {
    http2__stream_rst(c, stream, HTTP2_ERROR_PROTOCOL);
    return 0;
  }
  if (vr == 2)
    return HTTP2_ERROR_PROTOCOL; /* connection error (§8.2.2) */
  http2_on_headers(c, stream, fields, field_count, trailers, end_stream);
  http2_stream_s *s = http2__stream_find(c, stream);
  if (s && end_stream) {
    if (s->state == HTTP2_STREAM_RESERVED_REMOTE)
      s->state = HTTP2_STREAM_CLOSED;
    else if (s->state == HTTP2_STREAM_OPEN)
      s->state = HTTP2_STREAM_HALF_CLOSED_REMOTE;
    else if (s->state == HTTP2_STREAM_HALF_CLOSED_LOCAL)
      s->state = HTTP2_STREAM_CLOSED;
  } else if (s && s->state == HTTP2_STREAM_RESERVED_REMOTE) {
    s->state = HTTP2_STREAM_HALF_CLOSED_LOCAL;
  }
  return 0;
}

/* frame dispatch (the http2_parser.h callback) */
static int http2__on_frame(http2_connection_s *c, uint32_t length, uint8_t type,
                           uint8_t flags, uint32_t stream, uint8_t *payload) {
  int r = http2__check_first_frame(c, type);
  if (r)
    return r;
  switch (type) {
  case HTTP2_FRAME_DATA: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    if (c->state.header_block_stream)
      return HTTP2_ERROR_PROTOCOL; /* a header block is in progress */
    http2_stream_s *s = http2__stream_find(c, stream);
    int vr = http2__validate_frame(c, s, type);
    if (vr == 1) {
      http2__stream_rst(c, stream, HTTP2_ERROR_STREAM_CLOSED);
      return 0;
    }
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    /* padding (RFC 9113 §6.1) */
    uint32_t pos = 0;
    /* flow control applies to the entire DATA payload, padding included
     * (RFC 9113 §6.9) - the full size is charged to the windows, while the
     * application callback receives only the stripped data */
    uint32_t fc_len = length;
    if (flags & HTTP2_FLAG_PADDED) {
      if (!length)
        return HTTP2_ERROR_PROTOCOL;
      uint32_t pad = payload[0];
      if (pad + 1 >= length)
        return HTTP2_ERROR_PROTOCOL;
      ++pos;
      length -= pad + 1; /* the pad length octet itself */
    }
    /* flow control (RFC 9113 §6.9) */
    if ((int64_t)fc_len > c->state.recv_window ||
        (int64_t)fc_len > s->recv_window) {
      http2__stream_rst(c, stream, HTTP2_ERROR_FLOW_CONTROL);
      return 0;
    }
    c->state.recv_window -= (int64_t)fc_len;
    s->recv_window -= (int64_t)fc_len;
    http2_on_data(c, stream, payload + pos, length, fc_len,
                  flags & HTTP2_FLAG_END_STREAM);
    if (flags & HTTP2_FLAG_END_STREAM) {
      if (s->state == HTTP2_STREAM_OPEN)
        s->state = HTTP2_STREAM_HALF_CLOSED_REMOTE;
      else if (s->state == HTTP2_STREAM_HALF_CLOSED_LOCAL)
        s->state = HTTP2_STREAM_CLOSED;
    }
    return 0;
  }
  case HTTP2_FRAME_HEADERS: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    if (c->state.header_block_stream)
      return HTTP2_ERROR_PROTOCOL; /* a header block is already in progress */
    http2_stream_s *s = http2__stream_find(c, stream);
    int vr = http2__validate_frame(c, s, type);
    if (vr == 1) {
      http2__stream_rst(c, stream, HTTP2_ERROR_STREAM_CLOSED);
      return 0;
    }
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    if (!s) {
      /* a new stream - validate the stream id (RFC 9113 §5.1.1) */
      if (stream <= c->state.last_remote_stream)
        return HTTP2_ERROR_PROTOCOL; /* ids must strictly increase */
      if (c->state.role == HTTP2_CONNECTION_SERVER && !(stream & 1))
        return HTTP2_ERROR_PROTOCOL; /* client streams must be odd */
      if (c->state.role == HTTP2_CONNECTION_CLIENT && (stream & 1))
        return HTTP2_ERROR_PROTOCOL; /* server streams must be even */
      if (c->state.state)
        return HTTP2_ERROR_PROTOCOL; /* no new streams after GOAWAY */
      if (http2__stream_open_count(c) >= c->local.max_concurrent) {
        /* too many concurrent streams (RFC 9113 §5.1.2) */
        http2__stream_add(c, stream, HTTP2_STREAM_CLOSED);
        http2__stream_rst(c, stream, HTTP2_ERROR_REFUSED_STREAM);
        return 0;
      }
      c->state.last_remote_stream = stream;
      s = http2__stream_add(c, stream, HTTP2_STREAM_OPEN);
    }
    uint32_t pad_pos = 0;
    uint32_t blen = length;
    if (flags & HTTP2_FLAG_PADDED) {
      if (!blen)
        return HTTP2_ERROR_PROTOCOL;
      uint32_t pad = payload[0];
      if (pad + 1 >= blen)
        return HTTP2_ERROR_PROTOCOL;
      ++pad_pos;
      blen -= pad + 1; /* the pad length octet itself */
    }
    if (flags & HTTP2_FLAG_END_HEADERS) {
      int trailers = (s->state == HTTP2_STREAM_HALF_CLOSED_REMOTE);
      int r2 = http2__handle_header_block(
          c, stream, payload + pad_pos, blen, flags & HTTP2_FLAG_END_STREAM,
          trailers);
      if (r2)
        return r2;
    } else {
      /* the block continues in CONTINUATION frames (RFC 9113 §6.2) */
      c->state.header_block_stream = stream;
      c->state.header_block_end_stream = flags & HTTP2_FLAG_END_STREAM;
      hpack_context_decode_start(&c->hpack_dec);
      if (hpack_context_decode_add(&c->hpack_dec, payload + pad_pos, blen))
        return HTTP2_ERROR_COMPRESSION;
    }
    return 0;
  }
  case HTTP2_FRAME_CONTINUATION: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    if (!c->state.header_block_stream ||
        c->state.header_block_stream != stream)
      return HTTP2_ERROR_PROTOCOL; /* no HEADERS in progress (§6.10) */
    http2_stream_s *s = http2__stream_find(c, stream);
    int vr = http2__validate_frame(c, s, type);
    if (vr == 1) {
      c->state.header_block_stream = 0;
      http2__stream_rst(c, stream, HTTP2_ERROR_STREAM_CLOSED);
      return 0;
    }
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    if (hpack_context_decode_add(&c->hpack_dec, payload, length))
      return HTTP2_ERROR_COMPRESSION;
    if (flags & HTTP2_FLAG_END_HEADERS) {
      int trailers = (s->state == HTTP2_STREAM_HALF_CLOSED_REMOTE);
      uint8_t *block;
      size_t blen;
      uint32_t pp_stream = c->state.header_block_promised;
      uint8_t pp = c->state.header_block_push;
      c->state.header_block_stream = 0;
      c->state.header_block_push = 0;
      c->state.header_block_promised = 0;
      if (hpack_context_decode_end(&c->hpack_dec, &block, &blen)) {
        return HTTP2_ERROR_COMPRESSION;
      }
      int r2;
      if (pp) {
        /* the completed block is a PUSH_PROMISE (RFC 9113 §6.6) */
        hpack_header_s fields[256];
        size_t field_count = 0;
        if (hpack_context_decode(&c->hpack_dec, block, blen, fields, 256,
                                 &field_count))
          return HTTP2_ERROR_COMPRESSION;
        if (field_count > c->local.max_header_fields) {
          http2__stream_rst(c, pp_stream, HTTP2_ERROR_ENHANCE_YOUR_CALM);
          return 0;
        }
        if (http2__header_list_size(fields, field_count) >
            c->local.max_header_list) {
          http2__stream_rst(c, pp_stream, HTTP2_ERROR_ENHANCE_YOUR_CALM);
          return 0;
        }
int pp_vr = http2__validate_headers(c, fields, field_count, 2);
      if (pp_vr == 1) {
        http2__stream_rst(c, pp_stream, HTTP2_ERROR_PROTOCOL);
        return 0;
      }
      if (pp_vr == 2)
        return HTTP2_ERROR_PROTOCOL; /* connection error (§8.2.2) */
      http2_on_push_promise(c, stream, pp_stream, fields, field_count);
      r2 = 0;
    } else {
        r2 = http2__handle_header_block(
            c, stream, block, (uint32_t)blen, c->state.header_block_end_stream,
            trailers);
      }
      if (r2)
        return r2;
    }
    return 0;
  }
  case HTTP2_FRAME_PRIORITY: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    http2_stream_s *s = http2__stream_find(c, stream);
    if (length != 5) {
      /* stream error when the stream exists (§6.3) */
      if (s)
        http2__stream_rst(c, stream, HTTP2_ERROR_FRAME_SIZE);
      else
        return HTTP2_ERROR_FRAME_SIZE;
      return 0;
    }
    int vr = http2__validate_frame(c, s, type);
    if (vr == 1)
      return 0;
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    /* self-dependency (RFC 9113 §5.3.1) */
    if ((http2_parser_be32(payload) & 0x7fffffff) == stream)
      return HTTP2_ERROR_PROTOCOL;
    return 0;
  }
  case HTTP2_FRAME_RST_STREAM: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    if (length != 4)
      return HTTP2_ERROR_FRAME_SIZE; /* connection error (§6.4) */
    http2_stream_s *s = http2__stream_find(c, stream);
    if (!s)
      return HTTP2_ERROR_PROTOCOL; /* RST_STREAM on an idle stream */
    int vr = http2__validate_frame(c, s, type);
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    if (vr == 0)
      s->state = HTTP2_STREAM_CLOSED;
    return 0;
  }
  case HTTP2_FRAME_SETTINGS: {
    if (stream)
      return HTTP2_ERROR_PROTOCOL;
    if (flags & HTTP2_FLAG_ACK) {
      if (length)
        return HTTP2_ERROR_FRAME_SIZE; /* ACK must be empty (§6.5.3) */
      return 0;
    }
    if (length % 6)
      return HTTP2_ERROR_FRAME_SIZE; /* multiple of 6 (§6.5) */
    int r2 = http2__apply_settings(c, payload, length);
    if (r2)
      return r2;
    http2_connection_emit(c, 0, HTTP2_FRAME_SETTINGS, HTTP2_FLAG_ACK, 0, NULL);
    http2_on_settings(c);
    return 0;
  }
  case HTTP2_FRAME_PUSH_PROMISE: {
    if (!stream)
      return HTTP2_ERROR_PROTOCOL;
    if (c->state.role == HTTP2_CONNECTION_SERVER)
      return HTTP2_ERROR_PROTOCOL; /* servers must not receive it (§6.6) */
    if (length < 4)
      return HTTP2_ERROR_FRAME_SIZE; /* connection error (§6.6) */
    http2_stream_s *s = http2__stream_find(c, stream);
    if (!s || (s->state != HTTP2_STREAM_OPEN &&
               s->state != HTTP2_STREAM_HALF_CLOSED_LOCAL))
      return HTTP2_ERROR_PROTOCOL;
    uint32_t pos = 0;
    uint32_t len = length;
    if (flags & HTTP2_FLAG_PADDED) {
      if (!len)
        return HTTP2_ERROR_PROTOCOL;
      uint32_t pad = payload[0];
      if (pad + 1 >= len)
        return HTTP2_ERROR_PROTOCOL;
      ++pos;
      len -= pad + 1; /* the pad length octet itself */
    }
    if (len < 4)
      return HTTP2_ERROR_FRAME_SIZE;
    uint32_t promised = http2_parser_be32(payload + pos) & 0x7fffffff;
    /* the promised stream is server initiated, so it must be even and idle */
    if ((promised & 1) || promised <= c->state.last_remote_stream)
      return HTTP2_ERROR_PROTOCOL;
    c->state.last_remote_stream = promised;
    http2__stream_add(c, promised, HTTP2_STREAM_RESERVED_REMOTE);
    pos += 4;
    len -= 4;
    if (flags & HTTP2_FLAG_END_HEADERS) {
      hpack_header_s fields[256];
      size_t field_count = 0;
      if (hpack_context_decode(&c->hpack_dec, payload + pos, len, fields, 256,
                               &field_count))
        return HTTP2_ERROR_COMPRESSION;
      if (field_count > c->local.max_header_fields) {
        http2__stream_rst(c, promised, HTTP2_ERROR_ENHANCE_YOUR_CALM);
        return 0;
      }
      if (http2__header_list_size(fields, field_count) >
          c->local.max_header_list) {
        http2__stream_rst(c, promised, HTTP2_ERROR_ENHANCE_YOUR_CALM);
        return 0;
      }
      int vr = http2__validate_headers(c, fields, field_count, 2);
      if (vr == 1) {
        http2__stream_rst(c, promised, HTTP2_ERROR_PROTOCOL);
        return 0;
      }
      if (vr == 2)
        return HTTP2_ERROR_PROTOCOL; /* connection error (§8.2.2) */
      http2_on_push_promise(c, stream, promised, fields, field_count);
    } else {
      /* the block continues in CONTINUATION frames (RFC 9113 §6.10) */
      c->state.header_block_stream = stream;
      c->state.header_block_push = 1;
      c->state.header_block_promised = promised;
      hpack_context_decode_start(&c->hpack_dec);
      if (hpack_context_decode_add(&c->hpack_dec, payload + pos, len))
        return HTTP2_ERROR_COMPRESSION;
    }
    return 0;
  }
  case HTTP2_FRAME_PING: {
    if (stream)
      return HTTP2_ERROR_PROTOCOL;
    if (length != 8)
      return HTTP2_ERROR_FRAME_SIZE; /* connection error (§6.7) */
    if (flags & HTTP2_FLAG_ACK) {
      if (c->state.pings_outstanding)
        --c->state.pings_outstanding;
      return 0;
    }
    http2_connection_emit(c, 8, HTTP2_FRAME_PING, HTTP2_FLAG_ACK, 0, payload);
    return 0;
  }
  case HTTP2_FRAME_GOAWAY: {
    if (stream)
      return HTTP2_ERROR_PROTOCOL;
    if (length < 8)
      return HTTP2_ERROR_FRAME_SIZE; /* connection error (§6.8) */
    c->state.goaway_stream = http2_parser_be32(payload) & 0x7fffffff;
    c->state.state = 1; /* no new streams after a GOAWAY */
    http2_on_goaway(c, c->state.goaway_stream, http2_parser_be32(payload + 4));
    return 0;
  }
  case HTTP2_FRAME_WINDOW_UPDATE: {
    if (length != 4)
      return HTTP2_ERROR_FRAME_SIZE; /* connection error (§6.9) */
    uint32_t increment = http2_parser_be32(payload) & 0x7fffffff;
    if (!increment) {
      if (!stream)
        return HTTP2_ERROR_PROTOCOL; /* connection error (§6.9) */
      http2__stream_rst(c, stream, HTTP2_ERROR_PROTOCOL);
      return 0;
    }
    if (!stream) {
      if ((int64_t)increment > 0x7fffffff - c->state.send_window)
        return HTTP2_ERROR_FLOW_CONTROL; /* connection error (§6.9.1) */
      c->state.send_window += (int64_t)increment;
      http2_on_window_update(c, 0, increment);
      return 0;
    }
    http2_stream_s *s = http2__stream_find(c, stream);
    if (!s)
      return HTTP2_ERROR_PROTOCOL; /* WINDOW_UPDATE on an idle stream */
    int vr = http2__validate_frame(c, s, type);
    if (vr == 1)
      return 0;
    if (vr == 2)
      return HTTP2_ERROR_PROTOCOL;
    if ((int64_t)increment > 0x7fffffff - s->send_window)
      return HTTP2_ERROR_FLOW_CONTROL; /* stream error (§6.9.1) */
    s->send_window += (int64_t)increment;
    return 0;
  }
  default:
    /* unknown frame types must be ignored (RFC 9113 §4.1) */
    return 0;
  }
}

/* the http2_parser.h callback - dispatches frames to the connection logic */
static int http2_on_frame(http2_parser_s *parser, uint32_t length, uint8_t type,
                          uint8_t flags, uint32_t stream, uint8_t *payload) {
  http2_connection_s *c =
      (http2_connection_s *)((uint8_t *)parser - offsetof(http2_connection_s, parser));
  int r = http2__on_frame(c, length, type, flags, stream, payload);
  if (r) {
    http2_connection_abort(c, (uint32_t)r);
    return r;
  }
  return 0;
}

/* the http2_parser.h error callback - a frame exceeded the max frame size */
static int http2_on_error(http2_parser_s *parser) {
  http2_connection_s *c =
      (http2_connection_s *)((uint8_t *)parser - offsetof(http2_connection_s, parser));
  http2_connection_abort(c, HTTP2_ERROR_FRAME_SIZE);
  return 0;
}

static size_t http2_connection_parse(http2_connection_s *c, void *buffer,
                                     size_t length) {
  uint8_t *data = (uint8_t *)buffer;
  size_t consumed = 0;
  if (c->state.state == 2)
    return 0;
  /* client preface validation (RFC 9113 §3.5) */
  if (c->state.role == HTTP2_CONNECTION_SERVER &&
      c->state.preface_len < HTTP2_PREFACE_LEN) {
    size_t take = HTTP2_PREFACE_LEN - c->state.preface_len;
    if (take > length)
      take = length;
    if (memcmp(data, HTTP2_PREFACE + c->state.preface_len, take)) {
      http2_connection_abort(c, HTTP2_ERROR_PROTOCOL);
      return 0;
    }
    c->state.preface_len += take;
    data += take;
    length -= take;
    consumed += take;
    if (c->state.preface_len != HTTP2_PREFACE_LEN)
      return consumed; /* need more data */
  }
  while (length && c->state.state != 2) {
    size_t used = http2_parse(&c->parser, data, length);
    if (!used)
      break;
    data += used;
    length -= used;
    consumed += used;
    if (c->state.state == 2)
      break;
    if (c->parser.state.stage || c->parser.state.head_len)
      break; /* need more data */
  }
  return consumed;
}

#endif /* H_HTTP2_H */