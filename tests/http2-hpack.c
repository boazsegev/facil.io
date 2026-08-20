/*
Copyright: Boaz Segev, 2018-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio.h"

/* the HPACK parser (single header implementation) */
#include "hpack.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void hpack__fail(const char *test, int line) {
  fprintf(stderr, "* HTTP/2 HPACK TEST FAILED: %s (line %d).\n", test, line);
  exit(-1);
}

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond))                                                               \
      hpack__fail(#cond, __LINE__);                                            \
  } while (0)

static void hpack_test_round_trip(void) {
  hpack_context_s enc;
  hpack_context_s enc_ref;
  hpack_context_s dec;
  hpack_context_s dec_cnt;
  hpack_context_init(&enc, 4096);
  hpack_context_init(&enc_ref, 4096);
  hpack_context_init(&dec, 4096);
  hpack_context_init(&dec_cnt, 4096);
  size_t used = 0;
  hpack_header_s fields[16];
  size_t field_count = 0;
  uint8_t buf[1 << 16];

  static const hpack_header_s list_a[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = ":path", .len = 5},
       .value = {.data = "/index.html", .len = 11}},
      {.name = {.data = ":authority", .len = 10},
       .value = {.data = "www.example.com", .len = 15}},
      {.name = {.data = "accept", .len = 6},
       .value = {.data = "text/html", .len = 10}},
      {.name = {.data = "user-agent", .len = 10},
       .value = {.data = "facil.io", .len = 8}},
  };
  static const hpack_header_s list_b[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "POST", .len = 4}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "https", .len = 5}},
      {.name = {.data = ":path", .len = 5},
       .value = {.data = "/submit", .len = 7}},
      {.name = {.data = ":authority", .len = 10},
       .value = {.data = "www.example.com", .len = 15}},
      {.name = {.data = "content-type", .len = 12},
       .value = {.data = "application/x-www-form-urlencoded", .len = 33}},
      {.name = {.data = "cookie", .len = 6},
       .value = {.data = "a=1; b=2", .len = 8}},
  };
  static const hpack_header_s list_c[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = ":path", .len = 5},
       .value = {.data = "/index.html", .len = 11}},
      {.name = {.data = ":authority", .len = 10},
       .value = {.data = "www.example.com", .len = 15}},
      {.name = {.data = "accept", .len = 6},
       .value = {.data = "application/json", .len = 16}},
      {.name = {.data = "user-agent", .len = 10},
       .value = {.data = "facil.io", .len = 8}},
  };
  static const hpack_header_s *lists[] = {list_a, list_b, list_c};
  static const size_t list_lens[] = {6, 6, 6};

  for (size_t i = 0; i < 3; ++i) {
    ssize_t len = hpack_context_encode(&enc, buf, sizeof(buf), lists[i],
                                       list_lens[i], &used);
    CHECK(len > 0);
    CHECK((size_t)len == used);
    /* a measure-only call (NULL destination) must behave exactly like an
     * encoding, mutating the table state identically and reporting the exact
     * message length for the same table state */
    ssize_t mlen =
        hpack_context_encode(&enc_ref, NULL, 0, lists[i], list_lens[i], &used);
    CHECK(mlen == len);
    CHECK(used == (size_t)len);
    CHECK(enc_ref.dyn_len == enc.dyn_len);
    CHECK(enc_ref.dyn_size == enc.dyn_size);
    /* decode and compare */
    CHECK(hpack_context_decode(&dec, buf, (size_t)len, fields, 16,
                               &field_count) == 0);
    CHECK(field_count == list_lens[i]);
    for (size_t j = 0; j < field_count; ++j) {
      CHECK(fields[j].name.len == lists[i][j].name.len);
      CHECK(fields[j].value.len == lists[i][j].value.len);
      CHECK(memcmp(fields[j].name.data, lists[i][j].name.data,
                   fields[j].name.len) == 0);
      CHECK(memcmp(fields[j].value.data, lists[i][j].value.data,
                   fields[j].value.len) == 0);
    }
    /* count-only decode - the twin context decodes every block in count-only
     * mode, keeping its dynamic table in lockstep with the real decoder */
    CHECK(hpack_context_decode(&dec_cnt, buf, (size_t)len, NULL, 0,
                               &field_count) == 0);
    CHECK(field_count == list_lens[i]);
  }
  /* both contexts should end up with the same dynamic table state */
  CHECK(enc.dyn_len == dec.dyn_len);
  CHECK(enc.dyn_size == dec.dyn_size);
  CHECK(dec_cnt.dyn_len == dec.dyn_len);
  CHECK(dec_cnt.dyn_size == dec.dyn_size);

  /* a too-small buffer must report the required size, advance the table state
   * identically and return -1 */
  {
    hpack_context_s enc_a;
    hpack_context_s enc_b;
    hpack_context_init(&enc_a, 4096);
    hpack_context_init(&enc_b, 4096);
    ssize_t full =
        hpack_context_encode(&enc_a, buf, sizeof(buf), lists[0], list_lens[0],
                             &used);
    ssize_t small =
        hpack_context_encode(&enc_b, buf, 4, lists[0], list_lens[0], &used);
    CHECK(full > 0);
    CHECK(small == -1);
    CHECK(used == (size_t)full);
    CHECK(enc_a.dyn_len == enc_b.dyn_len);
    CHECK(enc_a.dyn_size == enc_b.dyn_size);
    hpack_context_destroy(&enc_a);
    hpack_context_destroy(&enc_b);
  }

  hpack_context_destroy(&enc);
  hpack_context_destroy(&enc_ref);
  hpack_context_destroy(&dec);
  hpack_context_destroy(&dec_cnt);
}

static void hpack_test_binary_fuzz(void) {
  hpack_context_s enc;
  hpack_context_s dec;
  hpack_context_init(&enc, 4096);
  hpack_context_init(&dec, 4096);
  uint32_t state = 0xDEADBEEF;
  hpack_header_s fields[64];
  size_t field_count = 0;
  uint8_t buf[1 << 16];
  for (size_t round = 0; round < 128; ++round) {
    hpack_header_s list[24];
    char name_buf[24][64];
    char value_buf[24][64];
    size_t count = (state >> 24) % 20 + 1;
    for (size_t i = 0; i < count; ++i) {
      state = state * 1664525 + 1013904223;
      size_t name_len = (state >> 16) % 24 + 1;
      for (size_t j = 0; j < name_len; ++j) {
        state = state * 1664525 + 1013904223;
        name_buf[i][j] = (char)('a' + (state % 26));
      }
      name_buf[i][0] = (i == 0 && (state & 1)) ? ':' : name_buf[i][0];
      state = state * 1664525 + 1013904223;
      size_t value_len = (state >> 16) % 40 + 1;
      for (size_t j = 0; j < value_len; ++j) {
        state = state * 1664525 + 1013904223;
        value_buf[i][j] = (char)('a' + (state % 26));
      }
      list[i].name.data = name_buf[i];
      list[i].name.len = name_len;
      list[i].value.data = value_buf[i];
      list[i].value.len = value_len;
    }
    size_t used = 0;
    ssize_t len =
        hpack_context_encode(&enc, buf, sizeof(buf), list, count, &used);
    CHECK(len > 0);
    CHECK(hpack_context_decode(&dec, buf, (size_t)len, fields, 64,
                               &field_count) == 0);
    CHECK(field_count == count);
    for (size_t j = 0; j < field_count; ++j) {
      CHECK(fields[j].name.len == list[j].name.len);
      CHECK(fields[j].value.len == list[j].value.len);
      CHECK(memcmp(fields[j].name.data, list[j].name.data,
                   fields[j].name.len) == 0);
      CHECK(memcmp(fields[j].value.data, list[j].value.data,
                   fields[j].value.len) == 0);
    }
  }
  hpack_context_destroy(&enc);
  hpack_context_destroy(&dec);
}

int main(void) {
  fprintf(stderr, "* Running HTTP/2 HPACK tests.\n");
  hpack_test_round_trip();
  hpack_test_binary_fuzz();
#if DEBUG
  hpack_test();
#else
  fprintf(stderr, "* DEBUG not set - skipping hpack_test() self tests.\n");
#endif
  fprintf(stderr, "* HTTP/2 HPACK tests complete.\n");
  return 0;
}