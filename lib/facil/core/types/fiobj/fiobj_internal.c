/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#include "fiobj_internal.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Internal API required across the board
***************************************************************************** */

void fiobj_simple_dealloc(fiobj_s *o) { fiobj_dealloc(o); }

void fiobj_noop_free(fiobj_s *obj) { OBJ2HEAD(obj)->ref = (uintptr_t)-1; }

int fiobj_noop_true(const fiobj_s *obj) {
  return 1;
  (void)obj;
}
int fiobj_noop_false(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
fio_cstr_s fiobj_noop_str(const fiobj_s *obj) {
  return (fio_cstr_s){.length = 0, .data = ""};
  (void)obj;
}
int64_t fiobj_noop_i(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
double fiobj_noop_f(const fiobj_s *obj) {
  return 0;
  (void)obj;
}

/** always 0. */
int fiobj_noop_is_eq(const fiobj_s *self, const fiobj_s *other) {
  return 0;
  (void)self;
  (void)other;
}

size_t fiobj_noop_count(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
fiobj_s *fiobj_noop_unwrap(const fiobj_s *obj) {
  return (fiobj_s *)obj;
  (void)obj;
}
size_t fiobj_noop_each1(fiobj_s *obj, size_t start_at,
                        int (*task)(fiobj_s *obj, void *arg), void *arg) {
  return 0;
  (void)obj;
  (void)start_at;
  (void)task;
  (void)arg;
}

/* *****************************************************************************
Invalid Object VTable - unused, still considering...
***************************************************************************** */

// static void fiobj_noop_free_invalid(fiobj_s *obj) { (void)obj; }
// static int64_t fiobj_noop_i_invalid(const fiobj_s *obj) {
//   return ((int64_t)(obj) ^ 3);
// }
// struct fiobj_vtable_s FIOBJ_VTABLE_INVALID = {
//     .name = "Invalid Class - not a facil.io Object",
//     .free = fiobj_noop_free_invalid,
//     .is_true = fiobj_noop_false,
//     .to_str = fiobj_noop_str,
//     .to_i = fiobj_noop_i_invalid,
//     .to_f = fiobj_noop_f,
//     .is_eq = fiobj_noop_is_eq,
//     .count = fiobj_noop_count,
//     .unwrap = fiobj_noop_unwrap,
//     .each1 = fiobj_noop_each1,
// };

/* *****************************************************************************
Internal API required across the board
***************************************************************************** */

/* *****************************************************************************
Hashing (SipHash copy)
***************************************************************************** */

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) &&                 \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/* the algorithm was designed as little endian... so, byte swap 64 bit. */
#define sip_local64(i)                                                         \
  (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                            \
      (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |                 \
      (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |         \
      (((i)&0xFF000000000000ULL) >> 40) | (((i)&0xFF00000000000000ULL) >> 56)
#else
/* no need */
#define sip_local64(i) (i)
#endif

/* 64Bit left rotation, inlined. */
#define lrot64(i, bits)                                                        \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))

uint64_t fiobj_sym_hash(const void *data, size_t len) {
  /* initialize the 4 words */
  uint64_t v0 = (0x0706050403020100ULL ^ 0x736f6d6570736575ULL);
  uint64_t v1 = (0x0f0e0d0c0b0a0908ULL ^ 0x646f72616e646f6dULL);
  uint64_t v2 = (0x0706050403020100ULL ^ 0x6c7967656e657261ULL);
  uint64_t v3 = (0x0f0e0d0c0b0a0908ULL ^ 0x7465646279746573ULL);
  const uint64_t *w64 = data;
  uint8_t len_mod = len & 255;
  union {
    uint64_t i;
    uint8_t str[8];
  } word;

#define hash_map_SipRound                                                      \
  do {                                                                         \
    v2 += v3;                                                                  \
    v3 = lrot64(v3, 16) ^ v2;                                                  \
    v0 += v1;                                                                  \
    v1 = lrot64(v1, 13) ^ v0;                                                  \
    v0 = lrot64(v0, 32);                                                       \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = lrot64(v1, 17) ^ v2;                                                  \
    v3 = lrot64(v3, 21) ^ v0;                                                  \
    v2 = lrot64(v2, 32);                                                       \
  } while (0);

  while (len >= 8) {
    word.i = sip_local64(*w64);
    v3 ^= word.i;
    /* Sip Rounds */
    hash_map_SipRound;
    hash_map_SipRound;
    v0 ^= word.i;
    w64 += 1;
    len -= 8;
  }
  word.i = 0;
  uint8_t *pos = word.str;
  uint8_t *w8 = (void *)w64;
  switch (len) { /* fallthrough is intentional */
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

  /* last round */
  v3 ^= word.i;
  hash_map_SipRound;
  hash_map_SipRound;
  v0 ^= word.i;
  /* Finalization */
  v2 ^= 0xff;
  /* d iterations of SipRound */
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  /* XOR it all together */
  v0 ^= v1 ^ v2 ^ v3;
#undef hash_map_SipRound
  return v0;
}
