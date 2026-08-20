/*
Copyright: Devstroop, 2026
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio.h"

/* the HTTP/2 connection core (single header implementation) */
#include "http2.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void http2_conn__fail(const char *test, int line) {
  fprintf(stderr, "* HTTP/2 CONN TEST FAILED: %s (line %d).\n", test, line);
  exit(-1);
}

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond))                                                               \
      http2_conn__fail(#cond, __LINE__);                                       \
  } while (0)

/* the connection under test */
static http2_connection_s conn;

/* encoder used to build the test header blocks */
static hpack_context_s hpack_enc;

/* captured output */
static uint8_t out_buf[1 << 16];
static size_t out_len;
static size_t out_frames;

/* received events */
static hpack_header_s got_fields[64];
static size_t got_count;
static uint32_t got_stream;
static int got_trailers;
static uint8_t got_data[1 << 14];
static uint32_t got_data_len;
static uint32_t got_payload_len;
static int got_end_stream;
static uint32_t got_promised;
static uint32_t got_goaway_stream;
static uint32_t got_goaway_error;
static int settings_changed;

static int http2_connection_emit(http2_connection_s *c, uint32_t length,
                                 uint8_t type, uint8_t flags, uint32_t stream,
                                 const uint8_t *payload) {
  (void)c;
  CHECK(out_len + 9 + length <= sizeof(out_buf));
  out_buf[out_len] = (uint8_t)(length >> 16);
  out_buf[out_len + 1] = (uint8_t)(length >> 8);
  out_buf[out_len + 2] = (uint8_t)length;
  out_buf[out_len + 3] = type;
  out_buf[out_len + 4] = flags;
  out_buf[out_len + 5] = (uint8_t)(stream >> 24);
  out_buf[out_len + 6] = (uint8_t)(stream >> 16);
  out_buf[out_len + 7] = (uint8_t)(stream >> 8);
  out_buf[out_len + 8] = (uint8_t)stream;
  if (length)
    memcpy(out_buf + out_len + 9, payload, length);
  out_len += 9 + length;
  ++out_frames;
  return 0;
}

static void http2_on_headers(http2_connection_s *c, uint32_t stream,
                             const hpack_header_s *fields, size_t count,
                             int trailers, int end_stream) {
  (void)c;
  (void)end_stream;
  got_stream = stream;
  got_count = count < 64 ? count : 64;
  for (size_t i = 0; i < got_count; ++i)
    got_fields[i] = fields[i];
  got_trailers = trailers;
}

static void http2_on_data(http2_connection_s *c, uint32_t stream,
                          uint8_t *data, uint32_t length,
                          uint32_t payload_len, int end_stream) {
  (void)c;
  got_stream = stream;
  memcpy(got_data, data, length);
  got_data_len = length;
  got_payload_len = payload_len;
  got_end_stream = end_stream;
}

static void http2_on_push_promise(http2_connection_s *c, uint32_t stream,
                                  uint32_t promised_stream,
                                  const hpack_header_s *fields, size_t count) {
  (void)c;
  (void)fields;
  (void)count;
  got_stream = stream;
  got_promised = promised_stream;
}

static void http2_on_goaway(http2_connection_s *c, uint32_t last_stream,
                            uint32_t error) {
  (void)c;
  got_goaway_stream = last_stream;
  got_goaway_error = error;
}

static void http2_on_settings(http2_connection_s *c) {
  (void)c;
  ++settings_changed;
}

static void http2_on_window_update(http2_connection_s *c, uint32_t stream,
                                   uint32_t increment) {
  (void)c;
  (void)stream;
  (void)increment;
}

/* builds a raw frame in buf, returns the total length */
static size_t conn_frame(uint8_t *buf, uint32_t length, uint8_t type,
                         uint8_t flags, uint32_t stream,
                         const uint8_t *payload) {
  buf[0] = (uint8_t)(length >> 16);
  buf[1] = (uint8_t)(length >> 8);
  buf[2] = (uint8_t)length;
  buf[3] = type;
  buf[4] = flags;
  buf[5] = (uint8_t)(stream >> 24);
  buf[6] = (uint8_t)(stream >> 16);
  buf[7] = (uint8_t)(stream >> 8);
  buf[8] = (uint8_t)stream;
  if (length && payload)
    memcpy(buf + 9, payload, length);
  return 9 + length;
}

/* a valid HPACK encoded request header block:
 * :method GET, :path /, :scheme http, :authority test.local, x-test: 42 */
static size_t block_basic(uint8_t *dest) {
  static const hpack_header_s fields[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = "x-test", .len = 6}, .value = {.data = "42", .len = 2}},
  };
  size_t used = 0;
  ssize_t blen = hpack_context_encode(&hpack_enc, dest, 1024, fields, 4,
                                      &used);
  CHECK(blen >= 0);
  return (size_t)blen;
}

/* a valid pushed request header block: :method GET, :path /push, :scheme
 * http, :authority test.local */
static size_t block_push(uint8_t *dest) {
  static const hpack_header_s fields[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5},
       .value = {.data = "/push", .len = 5}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = ":authority", .len = 10},
       .value = {.data = "test.local", .len = 10}},
  };
  size_t used = 0;
  ssize_t blen = hpack_context_encode(&hpack_enc, dest, 1024, fields, 4,
                                      &used);
  CHECK(blen >= 0);
  return (size_t)blen;
}

/* a valid response header block: :status 200, content-length: 11 */
static size_t block_response(uint8_t *dest) {
  static const hpack_header_s fields[] = {
      {.name = {.data = ":status", .len = 7},
       .value = {.data = "200", .len = 3}},
      {.name = {.data = "content-length", .len = 14},
       .value = {.data = "11", .len = 2}},
  };
  size_t used = 0;
  ssize_t blen = hpack_context_encode(&hpack_enc, dest, 1024, fields, 2,
                                      &used);
  CHECK(blen >= 0);
  return (size_t)blen;
}

static void conn_reset(void) {
  memset(&got_fields, 0, sizeof(got_fields));
  got_count = 0;
  got_stream = 0;
  got_trailers = 0;
  got_data_len = 0;
  got_payload_len = 0;
  got_end_stream = 0;
  got_promised = 0;
  got_goaway_stream = 0;
  got_goaway_error = 0;
  settings_changed = 0;
  out_len = 0;
  out_frames = 0;
}

static uint32_t last_goaway_error(void) {
  size_t pos = 0, hdr = 0;
  for (size_t i = 0; i < out_frames; ++i) {
    hdr = pos;
    uint32_t flen = (uint32_t)((out_buf[pos] << 16) | (out_buf[pos + 1] << 8) |
                               out_buf[pos + 2]);
    pos += 9 + flen;
  }
  if (!out_frames)
    return 0;
  return (uint32_t)((out_buf[hdr + 13] << 24) | (out_buf[hdr + 14] << 16) |
                    (out_buf[hdr + 15] << 8) | out_buf[hdr + 16]);
}

/* the error code carried by the last RST_STREAM frame */
static uint32_t stream_rst_error(void) {
  size_t pos = 0, hdr = 0;
  for (size_t i = 0; i < out_frames; ++i) {
    hdr = pos;
    uint32_t flen = (uint32_t)((out_buf[pos] << 16) | (out_buf[pos + 1] << 8) |
                               out_buf[pos + 2]);
    pos += 9 + flen;
  }
  if (!out_frames)
    return 0;
  return (uint32_t)((out_buf[hdr + 9] << 24) | (out_buf[hdr + 10] << 16) |
                    (out_buf[hdr + 11] << 8) | out_buf[hdr + 12]);
}

/* the last-stream-id carried by the last GOAWAY frame */
static uint32_t last_goaway_stream(void) {
  size_t pos = 0, hdr = 0;
  for (size_t i = 0; i < out_frames; ++i) {
    hdr = pos;
    uint32_t flen = (uint32_t)((out_buf[pos] << 16) | (out_buf[pos + 1] << 8) |
                               out_buf[pos + 2]);
    pos += 9 + flen;
  }
  if (!out_frames)
    return 0;
  return (uint32_t)((out_buf[hdr + 9] << 24) | (out_buf[hdr + 10] << 16) |
                    (out_buf[hdr + 11] << 8) | out_buf[hdr + 12]);
}

static void conn_init(void) {
  conn_reset();
  http2_connection_destroy(&conn);
  memset(&conn, 0, sizeof(conn));
  hpack_context_destroy(&hpack_enc);
  memset(&hpack_enc, 0, sizeof(hpack_enc));
  hpack_context_init(&hpack_enc, 4096);
  http2_connection_init(&conn, HTTP2_CONNECTION_SERVER, 4096);
}


/* full client handshake: preface + client SETTINGS (RFC 9113 §3.5) */
static void conn_handshake(void) {
  conn_init();
  /* the server's initial SETTINGS was emitted at init */
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_SETTINGS);
  conn_reset();
  size_t used = http2_connection_parse(&conn, (void *)HTTP2_PREFACE,
                                       HTTP2_PREFACE_LEN);
  CHECK(used == HTTP2_PREFACE_LEN);
  conn_reset();
  uint8_t fb[16];
  size_t sl = conn_frame(fb, 0, HTTP2_FRAME_SETTINGS, 0, 0, NULL);
  used = http2_connection_parse(&conn, fb, sl);
  CHECK(used == sl);
  /* the ACK for the server's initial SETTINGS is emitted */
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_SETTINGS);
  CHECK(out_buf[4] == HTTP2_FLAG_ACK);
  conn_reset();
}

/* ---------------------------------------------------------------------------
preface and SETTINGS
--------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------
preface and SETTINGS
--------------------------------------------------------------------------- */

static void test_preface(void) {
  /* an invalid preface is a connection error */
  conn_init();
  conn_reset();
  const char *bad = "GET / HTTP/1.1\r\n\r\n";
  size_t used = http2_connection_parse(&conn, (void *)bad, strlen(bad));
  CHECK(used == 0);
  CHECK(conn.state.state == 2);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_GOAWAY);
  CHECK((uint32_t)((out_buf[13] << 24) | (out_buf[14] << 16) |
                   (out_buf[15] << 8) | out_buf[16]) == HTTP2_ERROR_PROTOCOL);
  /* a partial preface is consumed but not validated yet */
  conn_init();
  used = http2_connection_parse(&conn, (void *)HTTP2_PREFACE, 10);
  CHECK(used == 10);
  CHECK(conn.state.state == 0);
  used = http2_connection_parse(&conn, (void *)(HTTP2_PREFACE + 10),
                                HTTP2_PREFACE_LEN - 10);
  CHECK(used == HTTP2_PREFACE_LEN - 10);
  CHECK(conn.state.state == 0);
}

static void test_settings(void) {
  conn_handshake();
  /* client SETTINGS + ACK exchange */
  uint8_t buf[64];
  uint8_t payload[6 * 2];
  payload[0] = 0;
  payload[1] = HTTP2_SETTING_ENABLE_PUSH;
  payload[2] = 0;
  payload[3] = 0;
  payload[4] = 0;
  payload[5] = 1;
  payload[6] = 0;
  payload[7] = HTTP2_SETTING_INITIAL_WINDOW_SIZE;
  payload[8] = 0;
  payload[9] = 0;
  payload[10] = 0x40;
  payload[11] = 0x00;
  size_t len = conn_frame(buf, 12, HTTP2_FRAME_SETTINGS, 0, 0, payload);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(settings_changed == 1);
  CHECK(conn.peer.enable_push == 1);
  CHECK(conn.peer.initial_window == 0x4000);
  /* an ACK was emitted */
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_SETTINGS);
  CHECK(out_buf[4] == HTTP2_FLAG_ACK);
  /* SETTINGS with a payload that isn't a multiple of 6 -> FRAME_SIZE_ERROR */
  conn_handshake();
  len = conn_frame(buf, 7, HTTP2_FRAME_SETTINGS, 0, 0, NULL);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FRAME_SIZE);
  /* SETTINGS ACK with a payload -> FRAME_SIZE_ERROR */
  conn_handshake();
  len = conn_frame(buf, 6, HTTP2_FRAME_SETTINGS, HTTP2_FLAG_ACK, 0, payload);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* SETTINGS on a stream -> PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 0, HTTP2_FRAME_SETTINGS, 0, 1, NULL);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* invalid ENABLE_PUSH -> PROTOCOL_ERROR */
  conn_handshake();
  payload[5] = 2;
  len = conn_frame(buf, 6, HTTP2_FRAME_SETTINGS, 0, 0, payload);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* invalid INITIAL_WINDOW_SIZE -> FLOW_CONTROL_ERROR */
  conn_handshake();
  uint8_t bad_iw[6];
  bad_iw[0] = 0;
  bad_iw[1] = HTTP2_SETTING_INITIAL_WINDOW_SIZE;
  bad_iw[2] = 0x80;
  bad_iw[3] = 0x00;
  bad_iw[4] = 0x00;
  bad_iw[5] = 0x00;
  len = conn_frame(buf, 6, HTTP2_FRAME_SETTINGS, 0, 0, bad_iw);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FLOW_CONTROL);
  /* invalid MAX_FRAME_SIZE -> PROTOCOL_ERROR */
  conn_init();
  http2_connection_parse(&conn, (void *)HTTP2_PREFACE, HTTP2_PREFACE_LEN);
  conn_reset();
  uint8_t bad_mfs[6];
  bad_mfs[0] = 0;
  bad_mfs[1] = HTTP2_SETTING_MAX_FRAME_SIZE;
  bad_mfs[2] = 0;
  bad_mfs[3] = 0;
  bad_mfs[4] = 0x10;
  bad_mfs[5] = 0x00;
  len = conn_frame(buf, 6, HTTP2_FRAME_SETTINGS, 0, 0, bad_mfs);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* the first frame must be SETTINGS */
  conn_init();
  http2_connection_parse(&conn, (void *)HTTP2_PREFACE, HTTP2_PREFACE_LEN);
  conn_reset();
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
}

/* ---------------------------------------------------------------------------
stream state machine
--------------------------------------------------------------------------- */

static void test_stream_states(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  /* HEADERS on stream 1 opens it */
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_count == 4);
  CHECK(got_stream == 1);
  CHECK(conn.stream_count == 1);
  CHECK(conn.streams[0].state == HTTP2_STREAM_OPEN);
  /* an even stream id from a client is a connection error */
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 2, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
}

static void test_stream_end_stream(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  /* HEADERS with END_STREAM -> half-closed (remote) */
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS | HTTP2_FLAG_END_STREAM, 1,
                          blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[0].state == HTTP2_STREAM_HALF_CLOSED_REMOTE);
  /* DATA after END_STREAM -> STREAM_CLOSED stream error */
  len = conn_frame(buf, 3, HTTP2_FRAME_DATA, 0, 1,
                   (const uint8_t *)"\x01\x02\x03");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK((uint32_t)((out_buf[9] << 24) | (out_buf[10] << 16) |
                   (out_buf[11] << 8) | out_buf[12]) ==
        HTTP2_ERROR_STREAM_CLOSED);
  CHECK(conn.streams[0].state == HTTP2_STREAM_CLOSED);
  /* DATA on the closed stream is still a stream error */
  conn_reset();
  len = conn_frame(buf, 3, HTTP2_FRAME_DATA, 0, 1,
                   (const uint8_t *)"\x01\x02\x03");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
}

static void test_stream_idle_frames(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  /* DATA on an idle stream -> connection PROTOCOL_ERROR */
  size_t len = conn_frame(buf, 3, HTTP2_FRAME_DATA, 0, 1,
                          (const uint8_t *)"\x01\x02\x03");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* WINDOW_UPDATE on an idle stream -> connection PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x05");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
}

static void test_stream_rst(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  CHECK(conn.streams[0].state == HTTP2_STREAM_OPEN);
  /* RST_STREAM closes the stream */
  conn_reset();
  len = conn_frame(buf, 4, HTTP2_FRAME_RST_STREAM, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x08");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[0].state == HTTP2_STREAM_CLOSED);
  /* RST_STREAM on an idle stream -> connection PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 4, HTTP2_FRAME_RST_STREAM, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x08");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* RST_STREAM with a bad length -> connection FRAME_SIZE_ERROR */
  conn_handshake();
  len = conn_frame(buf, 3, HTTP2_FRAME_RST_STREAM, 0, 1,
                   (const uint8_t *)"\x00\x00\x00");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FRAME_SIZE);
}

static void test_stream_priority(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  /* a self-dependency -> connection PROTOCOL_ERROR */
  size_t len = conn_frame(buf, 5, HTTP2_FRAME_PRIORITY, 0, 1,
                          (const uint8_t *)"\x80\x00\x00\x01\x1f");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* a valid PRIORITY on an idle stream is fine */
  conn_handshake();
  len = conn_frame(buf, 5, HTTP2_FRAME_PRIORITY, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x00\x1f");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  /* PRIORITY with a bad length on a non-existent stream -> connection
   * FRAME_SIZE_ERROR (a stream error can't target an idle stream) */
  len = conn_frame(buf, 4, HTTP2_FRAME_PRIORITY, 0, 3,
                   (const uint8_t *)"\x00\x00\x00\x00");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FRAME_SIZE);
}

/* ---------------------------------------------------------------------------
HEADERS + CONTINUATION reassembly and padding
--------------------------------------------------------------------------- */

static void test_continuation(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  /* HEADERS without END_HEADERS, then CONTINUATION with the rest */
  size_t len = conn_frame(buf, (uint32_t)(blen - 1), HTTP2_FRAME_HEADERS, 0, 1,
                          blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_count == 0);
  CHECK(conn.state.header_block_stream == 1);
  len = conn_frame(buf, 1, HTTP2_FRAME_CONTINUATION, HTTP2_FLAG_END_HEADERS, 1,
                   blk + blen - 1);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_count == 4);
  CHECK(conn.state.header_block_stream == 0);
  /* CONTINUATION without a pending HEADERS -> PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 1, HTTP2_FRAME_CONTINUATION, HTTP2_FLAG_END_HEADERS, 1,
                   blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* CONTINUATION on a different stream -> PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, (uint32_t)(blen - 1), HTTP2_FRAME_HEADERS, 0, 1, blk);
  http2_connection_parse(&conn, buf, len);
  len = conn_frame(buf, 1, HTTP2_FRAME_CONTINUATION, HTTP2_FLAG_END_HEADERS, 3,
                   blk + blen - 1);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* a frame other than CONTINUATION during a block -> PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, (uint32_t)(blen - 1), HTTP2_FRAME_HEADERS, 0, 1, blk);
  http2_connection_parse(&conn, buf, len);
  len = conn_frame(buf, 1, HTTP2_FRAME_DATA, 0, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
}

static void test_padding(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  /* a padded HEADERS frame (4 pad bytes, pad length byte first) */
  uint8_t padded[64];
  padded[0] = 4;
  memcpy(padded + 1, blk, blen);
  memset(padded + 1 + blen, 0, 4);
  size_t len = conn_frame(buf, (uint32_t)(1 + blen + 4), HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS | HTTP2_FLAG_PADDED, 1,
                          padded);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_count == 4);
  /* padding covering the whole payload -> PROTOCOL_ERROR */
  conn_handshake();
  padded[0] = 4;
  len = conn_frame(buf, 5, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS | HTTP2_FLAG_PADDED, 1, padded);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
}

/* ---------------------------------------------------------------------------
flow control
--------------------------------------------------------------------------- */

static void test_flow_control_recv(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  /* 5 bytes of DATA reduce both windows */
  conn_reset();
  len = conn_frame(buf, 5, HTTP2_FRAME_DATA, 0, 1,
                   (const uint8_t *)"\x01\x02\x03\x04\x05");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_data_len == 5);
  CHECK(!memcmp(got_data, "\x01\x02\x03\x04\x05", 5));
  CHECK(conn.state.recv_window == HTTP2_DEFAULT_WINDOW - 5);
  CHECK(conn.streams[0].recv_window == HTTP2_DEFAULT_WINDOW - 5);
  /* advertising consumption restores the windows */
  CHECK(http2_connection_window_update(&conn, 0, 5) == 0);
  CHECK(http2_connection_window_update(&conn, 1, 5) == 0);
  CHECK(conn.state.recv_window == HTTP2_DEFAULT_WINDOW);
  CHECK(conn.streams[0].recv_window == HTTP2_DEFAULT_WINDOW);
  CHECK(out_frames == 2);
  CHECK(out_buf[3] == HTTP2_FRAME_WINDOW_UPDATE);
  /* a DATA frame exceeding the stream window -> FLOW_CONTROL_ERROR */
  conn_reset();
  uint8_t big[70009];
  memset(big, 0x41, sizeof(big));
  big[0] = (uint8_t)(70000 >> 16);
  big[1] = (uint8_t)(70000 >> 8);
  big[2] = (uint8_t)70000;
  big[3] = HTTP2_FRAME_DATA;
  big[4] = 0;
  big[5] = 0;
  big[6] = 0;
  big[7] = 0;
  big[8] = 1;
  conn.parser.state.max_frame = 70000; /* let the frame reach flow control */
  size_t blen2 = 9 + 70000;
  used = http2_connection_parse(&conn, big, blen2);
  CHECK(used == blen2);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK((uint32_t)((out_buf[9] << 24) | (out_buf[10] << 16) |
                   (out_buf[11] << 8) | out_buf[12]) ==
        HTTP2_ERROR_FLOW_CONTROL);
}
static void test_flow_control_send(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  /* the peer sets INITIAL_WINDOW_SIZE to 0 */
  uint8_t payload[6];
  payload[0] = 0;
  payload[1] = HTTP2_SETTING_INITIAL_WINDOW_SIZE;
  payload[2] = 0;
  payload[3] = 0;
  payload[4] = 0;
  payload[5] = 0;
  size_t len = conn_frame(buf, 6, HTTP2_FRAME_SETTINGS, 0, 0, payload);
  http2_connection_parse(&conn, buf, len);
  CHECK(conn.peer.initial_window == 0);
  /* open a stream (request) */
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  /* sending DATA is blocked by the exhausted stream window */
  conn_reset();
  const uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  ssize_t sent = http2_connection_send_data(&conn, 1, data, 10, 0);
  CHECK(sent == 0);
  CHECK(out_frames == 0);
  /* the peer's WINDOW_UPDATE (stream) resumes sending */
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x05");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[0].send_window == 5);
  sent = http2_connection_send_data(&conn, 1, data, 10, 0);
  CHECK(sent == 5);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_DATA);
  CHECK(out_len == 9 + 5);
  /* the stream window is exhausted again - blocked */
  sent = http2_connection_send_data(&conn, 1, data, 10, 0);
  CHECK(sent == 0);
  /* a WINDOW_UPDATE for the connection also lets data flow */
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 0,
                   (const uint8_t *)"\x00\x00\x00\x0a");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.send_window == HTTP2_DEFAULT_WINDOW - 5 + 10);
  /* the stream window is still exhausted - still blocked */
  sent = http2_connection_send_data(&conn, 1, data, 10, 0);
  CHECK(sent == 0);
  /* a stream WINDOW_UPDATE lets data flow again */
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x0a");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[0].send_window == 10);
  sent = http2_connection_send_data(&conn, 1, data, 10, 0);
  CHECK(sent == 10);
  /* WINDOW_UPDATE with a 0 increment -> stream PROTOCOL_ERROR */
  conn_reset();
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x00");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  /* WINDOW_UPDATE (connection) with a 0 increment -> connection error */
  conn_reset();
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 0,
                   (const uint8_t *)"\x00\x00\x00\x00");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* WINDOW_UPDATE overflowing the 2^31-1 limit -> FLOW_CONTROL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 4, HTTP2_FRAME_WINDOW_UPDATE, 0, 0,
                   (const uint8_t *)"\x7f\xff\xff\xff");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FLOW_CONTROL);
}

/* ---------------------------------------------------------------------------
PING / GOAWAY / PUSH_PROMISE
--------------------------------------------------------------------------- */

static void test_ping(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  /* a PING is ACKed with the same payload */
  size_t len = conn_frame(buf, 8, HTTP2_FRAME_PING, 0, 0,
                          (const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07\x08");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_PING);
  CHECK(out_buf[4] == HTTP2_FLAG_ACK);
  CHECK(!memcmp(out_buf + 9, "\x01\x02\x03\x04\x05\x06\x07\x08", 8));
  /* a PING with a bad length -> connection FRAME_SIZE_ERROR */
  conn_handshake();
  len = conn_frame(buf, 7, HTTP2_FRAME_PING, 0, 0,
                   (const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_FRAME_SIZE);
  /* a PING on a stream -> connection PROTOCOL_ERROR */
  conn_handshake();
  len = conn_frame(buf, 8, HTTP2_FRAME_PING, 0, 1,
                   (const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07\x08");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
}

static void test_goaway(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  /* receiving a GOAWAY stops new streams */
  size_t len = conn_frame(buf, 8, HTTP2_FRAME_GOAWAY, 0, 0,
                          (const uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x08");
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_goaway_stream == 0);
  CHECK(got_goaway_error == 8);
  CHECK(conn.state.state == 1);
  /* a GOAWAY with a bad length -> connection FRAME_SIZE_ERROR */
  conn_handshake();
  len = conn_frame(buf, 7, HTTP2_FRAME_GOAWAY, 0, 0,
                   (const uint8_t *)"\x00\x00\x00\x00\x00\x00\x00");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  /* sending a GOAWAY prevents new streams */
  conn_handshake();
  http2_connection_send_goaway(&conn, HTTP2_ERROR_NO_ERROR);
  CHECK(out_buf[3] == HTTP2_FRAME_GOAWAY);
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  size_t used2 = http2_connection_parse(&conn, buf, len);
  CHECK(used2 == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
}

static void test_push_promise(void) {
  /* the server must reject PUSH_PROMISE */
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_push(blk);
  uint8_t pp[4 + 64];
  pp[0] = 0;
  pp[1] = 0;
  pp[2] = 0;
  pp[3] = 2;
  memcpy(pp + 4, blk, blen);
  size_t len = conn_frame(buf, (uint32_t)(4 + blen), HTTP2_FRAME_PUSH_PROMISE,
                          0, 1, pp);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
  /* the client accepts a PUSH_PROMISE for a new even stream */
  http2_connection_destroy(&conn);
  memset(&conn, 0, sizeof(conn));
  http2_connection_init(&conn, HTTP2_CONNECTION_CLIENT, 4096);
  conn_reset();
  /* the server's first frame must be SETTINGS (clears first_frame) */
  uint8_t sf[16];
  size_t sl = conn_frame(sf, 0, HTTP2_FRAME_SETTINGS, 0, 0, NULL);
  used = http2_connection_parse(&conn, sf, sl);
  CHECK(used == sl);
  conn_reset();
  /* the client opens stream 1 via its send path (half-closed local) */
  const hpack_header_s req[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
  };
  CHECK(http2_connection_send_headers(&conn, 1, req, 3, 1) == 0);
  CHECK(conn.streams[0].state == HTTP2_STREAM_HALF_CLOSED_LOCAL);
  conn_reset();
  len = conn_frame(buf, (uint32_t)(4 + blen), HTTP2_FRAME_PUSH_PROMISE,
                   HTTP2_FLAG_END_HEADERS, 1, pp);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_promised == 2);
  CHECK(conn.stream_count == 2);
  CHECK(conn.streams[0].state == HTTP2_STREAM_HALF_CLOSED_LOCAL);
  CHECK(conn.streams[1].state == HTTP2_STREAM_RESERVED_REMOTE);
  /* the promised stream's response HEADERS moves it to closed */
  size_t rlen = block_response(blk);
  len = conn_frame(buf, (uint32_t)rlen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS | HTTP2_FLAG_END_STREAM, 2, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[1].state == HTTP2_STREAM_CLOSED);
}

/* ---------------------------------------------------------------------------
limits and anti-DoS
--------------------------------------------------------------------------- */

static void test_max_concurrent(void) {
  conn_handshake();
  conn.local.max_concurrent = 1;
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.streams[0].state == HTTP2_STREAM_OPEN);
  /* a second concurrent stream is refused */
  conn_reset();
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 3, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK((uint32_t)((out_buf[9] << 24) | (out_buf[10] << 16) |
                   (out_buf[11] << 8) | out_buf[12]) ==
        HTTP2_ERROR_REFUSED_STREAM);
  CHECK(conn.streams[1].state == HTTP2_STREAM_CLOSED);
  /* a retransmitted HEADERS on the refused stream is a stream error */
  conn_reset();
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 3, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
}

static void test_max_header_fields(void) {
  conn_handshake();
  conn.local.max_header_fields = 1;
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk); /* 2 fields */
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK((uint32_t)((out_buf[9] << 24) | (out_buf[10] << 16) |
                   (out_buf[11] << 8) | out_buf[12]) ==
        HTTP2_ERROR_ENHANCE_YOUR_CALM);
}

static void test_ping_cap(void) {
  conn_handshake();
  /* sending more PINGs than the cap allows is refused */
  const uint8_t opaque[8] = {0};
  for (size_t i = 0; i < conn.local.max_ping_outstanding; ++i)
    CHECK(http2_connection_send_ping(&conn, opaque) == 0);
  CHECK(http2_connection_send_ping(&conn, opaque) == -1);
  /* an ACK frees a slot */
  uint8_t buf[1 << 10];
  size_t len = conn_frame(buf, 8, HTTP2_FRAME_PING, HTTP2_FLAG_ACK, 0, opaque);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(http2_connection_send_ping(&conn, opaque) == 0);
}

/* ---------------------------------------------------------------------------
sending side
--------------------------------------------------------------------------- */

static void test_send_headers(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  /* respond with a small header block - a single HEADERS frame */
  conn_reset();
  const hpack_header_s resp[] = {
      {.name = {.data = ":status", .len = 7},
       .value = {.data = "200", .len = 3}},
  };
  CHECK(http2_connection_send_headers(&conn, 1, resp, 1, 1) == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_HEADERS);
  CHECK(out_buf[4] == (HTTP2_FLAG_END_HEADERS | HTTP2_FLAG_END_STREAM));
  CHECK(conn.streams[0].state == HTTP2_STREAM_HALF_CLOSED_LOCAL);
  /* a large block is split into HEADERS + CONTINUATION frames */
  conn_handshake();
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  hpack_header_s big[32];
  char values[32][8];
  for (size_t i = 0; i < 32; ++i) {
    big[i].name.data = "x-large-header";
    big[i].name.len = 14;
    snprintf(values[i], sizeof(values[i]), "v-%zu", i);
    big[i].value.data = values[i];
    big[i].value.len = strlen(values[i]);
  }
  conn_reset();
  conn.peer.max_frame_size = 64; /* force splitting */
  CHECK(http2_connection_send_headers(&conn, 1, big, 32, 0) == 0);
  CHECK(out_frames >= 2);
  CHECK(out_buf[3] == HTTP2_FRAME_HEADERS);
  CHECK((out_buf[4] & HTTP2_FLAG_END_HEADERS) == 0);
  /* the last emitted frame is a CONTINUATION with END_HEADERS */
  size_t last_off = 0;
  size_t pos = 0;
  for (size_t i = 0; i < out_frames; ++i) {
    uint32_t flen = (uint32_t)((out_buf[pos] << 16) | (out_buf[pos + 1] << 8) |
                               out_buf[pos + 2]);
    last_off = pos;
    pos += 9 + flen;
  }
  CHECK(out_buf[last_off + 3] == HTTP2_FRAME_CONTINUATION);
  CHECK(out_buf[last_off + 4] & HTTP2_FLAG_END_HEADERS);
  /* sending on a closed stream is an error */
  len = conn_frame(buf, 4, HTTP2_FRAME_RST_STREAM, 0, 1,
                   (const uint8_t *)"\x00\x00\x00\x00");
  size_t used4 = http2_connection_parse(&conn, buf, len);
  CHECK(used4 == len);
  CHECK(conn.streams[0].state == HTTP2_STREAM_CLOSED);
  int r = http2_connection_send_headers(&conn, 1, resp, 1, 0);
  CHECK(r == -1);
}

static void test_send_data(void) {
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  /* a large payload is limited by the frame size and windows */
  conn_reset();
  uint8_t big[70000];
  memset(big, 0x42, sizeof(big));
  ssize_t sent = http2_connection_send_data(&conn, 1, big, sizeof(big), 1);
  CHECK(sent == HTTP2_DEFAULT_FRAME_SIZE);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_DATA);
  CHECK(conn.state.send_window == HTTP2_DEFAULT_WINDOW - HTTP2_DEFAULT_FRAME_SIZE);
  CHECK(conn.streams[0].state == HTTP2_STREAM_HALF_CLOSED_LOCAL);
}

/* ---------------------------------------------------------------------------
review regressions
-------------------------------------------------------------------------- */

static void test_stream_id_namespaces(void) {
  /* local and peer stream-id namespaces are tracked independently: a push
   * promise reserving an even (peer) stream id must not move the local
   * namespace, and vice versa */
  http2_connection_destroy(&conn);
  memset(&conn, 0, sizeof(conn));
  http2_connection_init(&conn, HTTP2_CONNECTION_CLIENT, 4096);
  conn_reset();
  /* the server's first frame must be SETTINGS */
  uint8_t sf[16];
  size_t sl = conn_frame(sf, 0, HTTP2_FRAME_SETTINGS, 0, 0, NULL);
  size_t used = http2_connection_parse(&conn, sf, sl);
  CHECK(used == sl);
  conn_reset();
  /* open stream 1 (client namespace) */
  const hpack_header_s req[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
  };
  CHECK(http2_connection_send_headers(&conn, 1, req, 3, 0) == 0);
  conn_reset();
  /* a PUSH_PROMISE reserving stream 100 moves only the peer namespace */
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_push(blk);
  uint8_t pp[4 + 64];
  pp[0] = 0;
  pp[1] = 0;
  pp[2] = 0;
  pp[3] = 100;
  memcpy(pp + 4, blk, blen);
  size_t len = conn_frame(buf, (uint32_t)(4 + blen), HTTP2_FRAME_PUSH_PROMISE,
                          HTTP2_FLAG_END_HEADERS, 1, pp);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_promised == 100);
  /* the peer opening stream 102 (even, past 100) is accepted */
  size_t rlen = block_response(blk);
  len = conn_frame(buf, (uint32_t)rlen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 102, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(got_stream == 102);
  /* the client namespace is untouched - stream 3 is still available */
  CHECK(http2_connection_send_headers(&conn, 3, req, 3, 0) == 0);
  /* and the client cannot initiate a new stream in the peer namespace */
  CHECK(http2_connection_send_headers(&conn, 104, req, 3, 0) == -1);
}

static void test_continuation_large_stream(void) {
  /* the fragmented header block state must track the full 32-bit stream id:
   * stream 257 with the old uint8_t state would alias to 1 */
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)(blen - 1), HTTP2_FRAME_HEADERS, 0,
                          257, blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.header_block_stream == 257);
  len = conn_frame(buf, 1, HTTP2_FRAME_CONTINUATION, HTTP2_FLAG_END_HEADERS,
                   257, blk + blen - 1);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_count == 4);
  CHECK(got_stream == 257);
}

static void test_data_padding_flow(void) {
  /* the flow-control windows are charged the full padded payload, while the
   * data callback only sees the unpadded bytes */
  conn_handshake();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  http2_connection_parse(&conn, buf, len);
  conn_reset();
  /* payload: pad-len(1) + 5 data + 2 padding = 8 bytes */
  uint8_t padded[8];
  padded[0] = 2;
  memcpy(padded + 1, "\x01\x02\x03\x04\x05", 5);
  memset(padded + 6, 0, 2);
  len = conn_frame(buf, 8, HTTP2_FRAME_DATA, HTTP2_FLAG_PADDED, 1, padded);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(got_data_len == 5);
  CHECK(got_payload_len == 8);
  CHECK(!memcmp(got_data, "\x01\x02\x03\x04\x05", 5));
  CHECK(conn.state.recv_window == HTTP2_DEFAULT_WINDOW - 8);
  CHECK(conn.streams[0].recv_window == HTTP2_DEFAULT_WINDOW - 8);
}

static void test_header_validation(void) {
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen;
  size_t len;
  size_t used;
  /* uppercase field name -> stream error */
  conn_handshake();
  static const hpack_header_s upper[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = "X-Test", .len = 6},
       .value = {.data = "y", .len = 1}},
  };
  blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk), upper, 2,
                                      NULL);
  CHECK(blen != (size_t)-1);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK(stream_rst_error() == HTTP2_ERROR_PROTOCOL);
  /* a pseudo header after a regular field -> stream error */
  conn_handshake();
  static const hpack_header_s late[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = "x-test", .len = 6},
       .value = {.data = "y", .len = 1}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
  };
  blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk), late, 3,
                                      NULL);
  CHECK(blen != (size_t)-1);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK(stream_rst_error() == HTTP2_ERROR_PROTOCOL);
  /* an unknown pseudo header -> stream error */
  conn_handshake();
  static const hpack_header_s unknown[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":bogus", .len = 6},
       .value = {.data = "x", .len = 1}},
  };
  blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk), unknown, 2,
                                      NULL);
  CHECK(blen != (size_t)-1);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK(stream_rst_error() == HTTP2_ERROR_PROTOCOL);
  /* a request missing its mandatory pseudo headers -> stream error */
  conn_handshake();
  static const hpack_header_s missing[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
  };
  blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk), missing, 1,
                                      NULL);
  CHECK(blen != (size_t)-1);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK(stream_rst_error() == HTTP2_ERROR_PROTOCOL);
  /* a connection-specific field is a connection error */
  conn_handshake();
  static const hpack_header_s connspec[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = "connection", .len = 10},
       .value = {.data = "keep-alive", .len = 11}},
  };
  blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk), connspec, 4,
                                      NULL);
  CHECK(blen != (size_t)-1);
  len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                   HTTP2_FLAG_END_HEADERS, 1, blk);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_PROTOCOL);
}

static void test_header_list_size(void) {
  /* the header list is accounted as 32 + name + value per field */
  conn_handshake();
  conn.local.max_header_list = 40; /* below the smallest block_basic field */
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_basic(blk);
  size_t len = conn_frame(buf, (uint32_t)blen, HTTP2_FRAME_HEADERS,
                          HTTP2_FLAG_END_HEADERS, 1, blk);
  size_t used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 0);
  CHECK(out_frames == 1);
  CHECK(out_buf[3] == HTTP2_FRAME_RST_STREAM);
  CHECK(stream_rst_error() == HTTP2_ERROR_ENHANCE_YOUR_CALM);
}

static void test_hpack_scratch_guard(void) {
  /* with a small scratch limit, a fragmented block whose decoded fields
   * overflow the reserved region must fail cleanly (COMPRESSION) instead of
   * corrupting the accumulated block */
conn_handshake();
  conn.local.max_header_list = 4096;
  hpack_context_limit_set(&conn.hpack_dec, conn.local.max_header_list);
  uint8_t buf[4096];
  uint8_t blk[4096];
  hpack_header_s fields[20];
  char names[20][8];
  char values[20][128];
  for (size_t i = 0; i < 20; ++i) {
    snprintf(names[i], sizeof(names[i]), "x-%zu", i);
    fields[i].name.data = names[i];
    fields[i].name.len = strlen(names[i]);
    for (size_t j = 0; j < 120; ++j)
      values[i][j] = (char)(j * 7 + (size_t)i); /* incompressible */
    fields[i].value.data = values[i];
    fields[i].value.len = 120;
  }
  size_t used = 0;
  size_t blen = (size_t)hpack_context_encode(&hpack_enc, blk, sizeof(blk),
                                             fields, 20, &used);
  CHECK(blen != (size_t)-1 && blen > 2);
  /* the decoded fields (20 x 125 bytes) exceed the scratch budget left after
   * the block is reserved (4096 - blen) -> clean COMPRESSION error */
  size_t len = conn_frame(buf, (uint32_t)(blen - 1), HTTP2_FRAME_HEADERS, 0, 1,
                          blk);
  size_t cused = http2_connection_parse(&conn, buf, len);
  CHECK(cused == len);
  len = conn_frame(buf, 1, HTTP2_FRAME_CONTINUATION, HTTP2_FLAG_END_HEADERS, 1,
                   blk + blen - 1);
  cused = http2_connection_parse(&conn, buf, len);
  CHECK(cused == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_error() == HTTP2_ERROR_COMPRESSION);
}

static void test_max_frame_clamp(void) {
  /* the advertised max frame size is clamped to the parser buffer capacity */
  conn_handshake();
  http2_connection_setting_set(&conn, HTTP2_SETTING_MAX_FRAME_SIZE, 1u << 20);
  CHECK(conn.parser.state.max_frame == HTTP2_PARSER_BUFFER);
  /* the emitted SETTINGS payload reflects the clamped value (setting_set
   * emits a single-setting frame) */
  uint32_t p = (uint32_t)((out_buf[11] << 24) | (out_buf[12] << 16) |
                          (out_buf[13] << 8) | out_buf[14]);
  CHECK(p == HTTP2_PARSER_BUFFER);
}

static void test_goaway_remote_stream(void) {
  /* the GOAWAY last-stream-id reflects the peer's namespace: a push promise
   * reserving an even stream id must not leak into it */
  http2_connection_destroy(&conn);
  memset(&conn, 0, sizeof(conn));
  http2_connection_init(&conn, HTTP2_CONNECTION_CLIENT, 4096);
  conn_reset();
  uint8_t sf[16];
  size_t sl = conn_frame(sf, 0, HTTP2_FRAME_SETTINGS, 0, 0, NULL);
  size_t used = http2_connection_parse(&conn, sf, sl);
  CHECK(used == sl);
  conn_reset();
  const hpack_header_s req[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5}, .value = {.data = "/", .len = 1}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
  };
  CHECK(http2_connection_send_headers(&conn, 1, req, 3, 0) == 0);
  conn_reset();
  uint8_t buf[1 << 10];
  uint8_t blk[64];
  size_t blen = block_push(blk);
  uint8_t pp[4 + 64];
  pp[0] = 0;
  pp[1] = 0;
  pp[2] = 0;
  pp[3] = 100;
  memcpy(pp + 4, blk, blen);
  size_t len = conn_frame(buf, (uint32_t)(4 + blen), HTTP2_FRAME_PUSH_PROMISE,
                          HTTP2_FLAG_END_HEADERS, 1, pp);
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  /* a DATA frame on stream 0 is a connection error - the resulting GOAWAY
   * last-stream-id must be the peer's last stream (100), not the client's
   * local stream 1 */
  conn_reset();
  len = conn_frame(buf, 4, HTTP2_FRAME_DATA, 0, 0,
                   (const uint8_t *)"\x01\x02\x03\x04");
  used = http2_connection_parse(&conn, buf, len);
  CHECK(used == len);
  CHECK(conn.state.state == 2);
  CHECK(last_goaway_stream() == 100);
}

/* ---------------------------------------------------------------------------
main
-------------------------------------------------------------------------- */

static void http2_test_conn(void) {
  test_preface();
  test_settings();
  test_stream_states();
  test_stream_end_stream();
  test_stream_idle_frames();
  test_stream_rst();
  test_stream_priority();
  test_continuation();
  test_padding();
  test_flow_control_recv();
  test_flow_control_send();
  test_ping();
  test_goaway();
  test_push_promise();
  test_max_concurrent();
  test_max_header_fields();
  test_ping_cap();
  test_send_headers();
  test_send_data();
  test_stream_id_namespaces();
  test_continuation_large_stream();
  test_data_padding_flow();
  test_header_validation();
  test_header_list_size();
  test_hpack_scratch_guard();
  test_max_frame_clamp();
  test_goaway_remote_stream();
}

int main(void) {
  fprintf(stderr, "* Running HTTP/2 connection tests.\n");
  http2_test_conn();
  fprintf(stderr, "* HTTP/2 connection tests complete.\n");
  return 0;
}
