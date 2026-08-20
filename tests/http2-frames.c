/*
Copyright: Devstroop, 2026
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio.h"

/* the HTTP/2 frame parser (single header implementation) */
#include "http2_parser.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void http2_frames__fail(const char *test, int line) {
  fprintf(stderr, "* HTTP/2 FRAMES TEST FAILED: %s (line %d).\n", test, line);
  exit(-1);
}

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond))                                                               \
      http2_frames__fail(#cond, __LINE__);                                     \
  } while (0)

/* test state shared with the parser callbacks */
typedef struct frame_test_s {
  http2_parser_s parser;
  uint32_t length;
  uint8_t type;
  uint8_t flags;
  uint32_t stream;
  uint8_t *payload;
  uint8_t payload_buf[1 << 16];
  int frames;
  int errors;
  int stop;
} frame_test_s;

static frame_test_s ft;

static int http2_on_frame(http2_parser_s *parser, uint32_t length, uint8_t type,
                          uint8_t flags, uint32_t stream, uint8_t *payload) {
  (void)parser;
  ft.length = length;
  ft.type = type;
  ft.flags = flags;
  ft.stream = stream;
  ft.payload = payload;
  if (length)
    memcpy(ft.payload_buf, payload, length);
  ++ft.frames;
  return ft.stop;
}

static int http2_on_error(http2_parser_s *parser) {
  (void)parser;
  ++ft.errors;
  return 0;
}

/* serializes a frame header + payload into dest, returns total length */
static size_t frame_build(uint8_t *dest, uint32_t length, uint8_t type,
                          uint8_t flags, uint32_t stream, const uint8_t *payload) {
  dest[0] = (uint8_t)(length >> 16);
  dest[1] = (uint8_t)(length >> 8);
  dest[2] = (uint8_t)length;
  dest[3] = type;
  dest[4] = flags;
  dest[5] = (uint8_t)(stream >> 24);
  dest[6] = (uint8_t)(stream >> 16);
  dest[7] = (uint8_t)(stream >> 8);
  dest[8] = (uint8_t)stream;
  if (length && payload)
    memcpy(dest + 9, payload, length);
  return 9 + length;
}

static void ft_reset(void) {
  memset(&ft, 0, sizeof(ft));
  ft.parser = (http2_parser_s)HTTP2_PARSER_INIT;
}

static void http2_test_single_frame(void) {
  uint8_t buf[1 << 10];
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  size_t len = frame_build(buf, 4, HTTP2_FRAME_DATA, 0, 1, payload);
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, len);
  CHECK(used == len);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 4);
  CHECK(ft.type == HTTP2_FRAME_DATA);
  CHECK(ft.flags == 0);
  CHECK(ft.stream == 1);
  CHECK(ft.payload == buf + 9); /* zero-copy path */
  CHECK(!memcmp(ft.payload_buf, payload, 4));
}

static void http2_test_split_header(void) {
  uint8_t buf[1 << 10];
  size_t len = frame_build(buf, 4, HTTP2_FRAME_HEADERS, HTTP2_FLAG_END_HEADERS,
                           3, (const uint8_t *)"\x01\x02\x03\x04");
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, 4);
  CHECK(used == 4);
  CHECK(ft.frames == 0);
  used = http2_parse(&ft.parser, buf + 4, len - 4);
  CHECK(used == len - 4);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 4);
  CHECK(ft.type == HTTP2_FRAME_HEADERS);
  CHECK(ft.flags == HTTP2_FLAG_END_HEADERS);
  CHECK(ft.stream == 3);
}

static void http2_test_split_payload(void) {
  uint8_t buf[1 << 12];
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  size_t len = frame_build(buf, 8, HTTP2_FRAME_DATA, HTTP2_FLAG_END_STREAM, 5,
                           payload);
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, 12); /* header + 3 payload bytes */
  CHECK(used == 12);
  CHECK(ft.frames == 0);
  used = http2_parse(&ft.parser, buf + 12, 3);
  CHECK(used == 3);
  CHECK(ft.frames == 0);
  used = http2_parse(&ft.parser, buf + 15, len - 15);
  CHECK(used == len - 15);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 8);
  CHECK(ft.stream == 5);
  CHECK(ft.payload == ft.parser.state.buf); /* accumulation path */
  CHECK(!memcmp(ft.payload_buf, payload, 8));
  /* a second frame right after the accumulated one */
  size_t len2 = frame_build(buf + len, 0, HTTP2_FRAME_PING, HTTP2_FLAG_ACK, 0,
                            NULL);
  used = http2_parse(&ft.parser, buf + len, len2);
  CHECK(used == len2);
  CHECK(ft.frames == 2);
  CHECK(ft.type == HTTP2_FRAME_PING);
  CHECK(ft.length == 0);
  CHECK(ft.payload == NULL);
}

static void http2_test_multi_frame(void) {
  uint8_t buf[1 << 12];
  uint8_t *p = buf;
  size_t len = 0;
  for (int i = 0; i < 5; ++i) {
    const uint8_t payload[] = {0xaa, 0xbb, 0xcc};
    size_t l = frame_build(p, 3, HTTP2_FRAME_RST_STREAM, 0, (uint32_t)(i + 1),
                           payload);
    p += l;
    len += l;
  }
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, len);
  CHECK(used == len);
  CHECK(ft.frames == 5);
  CHECK(ft.stream == 5);
  CHECK(ft.length == 3);
}

static void http2_test_stream_id(void) {
  uint8_t buf[1 << 10];
  uint32_t sid = 0x7fffffff; /* 31 bit maximum */
  size_t len = frame_build(buf, 0, HTTP2_FRAME_GOAWAY, 0, sid, NULL);
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, len);
  CHECK(used == len);
  CHECK(ft.stream == sid);
}

static void http2_test_empty_payload(void) {
  uint8_t buf[1 << 10];
  size_t len = frame_build(buf, 0, HTTP2_FRAME_WINDOW_UPDATE, 0, 0, NULL);
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, len);
  CHECK(used == len);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 0);
  CHECK(ft.payload == NULL);
}

static void http2_test_oversized_frame(void) {
  uint8_t buf[HTTP2_PARSER_BUFFER + 64];
  /* a frame that exceeds the maximum frame size */
  size_t len = frame_build(buf, HTTP2_PARSER_BUFFER + 1, HTTP2_FRAME_DATA, 0, 1,
                           NULL);
  ft_reset();
  size_t used = http2_parse(&ft.parser, buf, len);
  CHECK(used == 9);
  CHECK(ft.frames == 0);
  CHECK(ft.errors == 1);
  /* frames of exactly the maximum size are accepted */
  size_t len2 =
      frame_build(buf, HTTP2_PARSER_BUFFER, HTTP2_FRAME_DATA, 0, 2, NULL);
  ft_reset();
  used = http2_parse(&ft.parser, buf, len2);
  CHECK(used == len2);
  CHECK(ft.frames == 1);
  CHECK(ft.length == HTTP2_PARSER_BUFFER);
  /* custom maximum */
  ft_reset();
  ft.parser.state.max_frame = 128;
  size_t len3 = frame_build(buf, 129, HTTP2_FRAME_DATA, 0, 3, NULL);
  used = http2_parse(&ft.parser, buf, len3);
  CHECK(used == 9);
  CHECK(ft.errors == 1);
}

static void http2_test_large_frame_split(void) {
  /* a frame accumulated in pieces - must fit the internal buffer */
  uint8_t buf[1 << 15];
  memset(buf, 0x5a, sizeof(buf));
  size_t len = frame_build(buf, 12000, HTTP2_FRAME_DATA, 0, 7, buf + 9);
  ft_reset();
  ft.parser.state.max_frame = 12000;
  size_t used = http2_parse(&ft.parser, buf, 5000);
  CHECK(used == 5000);
  CHECK(ft.frames == 0);
  used = http2_parse(&ft.parser, buf + 5000, len - 5000);
  CHECK(used == len - 5000);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 12000);
  CHECK(ft.payload == ft.parser.state.buf);
  /* a frame larger than the internal buffer only works when the whole frame
   * arrives in a single call (zero-copy path) */
  ft_reset();
  ft.parser.state.max_frame = 20000;
  size_t len2 = frame_build(buf, 20000, HTTP2_FRAME_DATA, 0, 8, buf + 9);
  used = http2_parse(&ft.parser, buf, len2);
  CHECK(used == len2);
  CHECK(ft.frames == 1);
  CHECK(ft.length == 20000);
  CHECK(ft.stream == 8);
  CHECK(ft.payload == buf + 9); /* zero-copy */
}

static void http2_test_stop_semantics(void) {
  uint8_t buf[1 << 10];
  size_t len = 0;
  for (int i = 0; i < 3; ++i) {
    size_t l = frame_build(buf + len, 1, HTTP2_FRAME_DATA, 0, (uint32_t)i + 1,
                           (const uint8_t *)"\x01");
    len += l;
  }
  ft_reset();
  ft.stop = 1;
  size_t first = http2_parse(&ft.parser, buf, len);
  CHECK(first == 10);
  CHECK(ft.frames == 1);
  CHECK(ft.stream == 1);
  /* resubmit the reminder to continue - all remaining frames are parsed */
  ft.stop = 0;
  size_t used = http2_parse(&ft.parser, buf + first, len - first);
  CHECK(used == len - first);
  CHECK(ft.frames == 3);
  CHECK(ft.stream == 3);
}

static void http2_test_parser(void) {
  http2_test_single_frame();
  http2_test_split_header();
  http2_test_split_payload();
  http2_test_multi_frame();
  http2_test_stream_id();
  http2_test_empty_payload();
  http2_test_oversized_frame();
  http2_test_large_frame_split();
  http2_test_stop_semantics();
}

int main(void) {
  fprintf(stderr, "* Running HTTP/2 frame parser tests.\n");
  http2_test_parser();
  fprintf(stderr, "* HTTP/2 frame parser tests complete.\n");
  return 0;
}