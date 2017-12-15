/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#include "siphash.h"
#include <stdint.h>
#include <stdlib.h>

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

#ifndef UNUSED_FUNC
#   define UNUSED_FUNC __attribute__((unused))
#endif
// clang-format on

/** 64Bit left rotation, inlined. */
#define _lrot64(i, bits)                                                       \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))

#ifdef __BIG_ENDIAN__
/* the algorithm was designed as little endian */
/** inplace byte swap 64 bit integer */
#define sip_local64(i)                                                         \
  (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                            \
      (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |                 \
      (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |         \
      (((i)&0xFF000000000000ULL) >> 40) | (((i)&0xFF00000000000000ULL) >> 56)

#else
/** no need */
#define sip_local64(i) (i)
#endif

uint64_t siphash24(const void *data, size_t len, uint64_t iv_key[2]) {
  /* initialize the 4 words */
  uint64_t v0 = iv_key[0] ^ 0x736f6d6570736575ULL;
  uint64_t v1 = iv_key[1] ^ 0x646f72616e646f6dULL;
  uint64_t v2 = iv_key[0] ^ 0x6c7967656e657261ULL;
  uint64_t v3 = iv_key[1] ^ 0x7465646279746573ULL;
  const uint64_t *w64 = data;
  uint8_t len_mod = len & 255;
  union {
    uint64_t i;
    uint8_t str[8];
  } word;

#define _bs_map_SipRound                                                       \
  do {                                                                         \
    v2 += v3;                                                                  \
    v3 = _lrot64(v3, 16) ^ v2;                                                 \
    v0 += v1;                                                                  \
    v1 = _lrot64(v1, 13) ^ v0;                                                 \
    v0 = _lrot64(v0, 32);                                                      \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = _lrot64(v1, 17) ^ v2;                                                 \
    v3 = _lrot64(v3, 21) ^ v0;                                                 \
    v2 = _lrot64(v2, 32);                                                      \
  } while (0);

  while (len >= 8) {
    word.i = sip_local64(*w64);
    v3 ^= word.i;
    /* Sip Rounds */
    _bs_map_SipRound;
    _bs_map_SipRound;
    v0 ^= word.i;
    w64 += 1;
    len -= 8;
  }
  word.i = 0;
  uint8_t *pos = word.str;
  uint8_t *w8 = (void *)w64;
  switch (len) { // fallthrough is intentional
  case 7:
    pos[6] = w8[6];
  case 6:
    pos[5] = w8[5];
  case 5:
    pos[4] = w8[4];
  case 4:
    pos[3] = w8[3];
  case 3:
    pos[2] = w8[2];
  case 2:
    pos[1] = w8[1];
  case 1:
    pos[0] = w8[0];
  }
  word.str[7] = len_mod;
  // word.i = sip_local64(word.i);

  /* last round */
  v3 ^= word.i;
  _bs_map_SipRound;
  _bs_map_SipRound;
  v0 ^= word.i;
  /* Finalization */
  v2 ^= 0xff;
  /* d iterations of SipRound */
  _bs_map_SipRound;
  _bs_map_SipRound;
  _bs_map_SipRound;
  _bs_map_SipRound;
  /* XOR it all together */
  v0 ^= v1 ^ v2 ^ v3;
#undef _bs_map_SipRound
  return v0;
}

#undef sip_local64
#undef _lrot64

#if defined(DEBUG) && DEBUG == 1

#include <stdio.h>
#include <time.h>

void bscrypt_test_siphash(void) {
  uint64_t result =
      siphash24("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e",
                15, SIPHASH_DEFAULT_KEY);
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "SipHash simple test %s\n",
          (result == 0xa129ca6149be45e5ULL) ? "passed" : "FAILED");
  clock_t start;
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    __asm__ volatile("" ::: "memory");
    result = siphash24("The quick brown fox jumps over the lazy dog ", 43,
                       SIPHASH_DEFAULT_KEY);
  }
  fprintf(stderr, "bscrypt 100K SipHash: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);
  fprintf(stderr, "===================================\n");
}

#endif
