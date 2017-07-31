/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "sha2.h"
/*****************************************************************************
Useful Macros
*/

/** 32Bit left rotation, inlined. */
#define left_rotate32(i, bits)                                                 \
  (((uint32_t)(i) << (bits)) | ((uint32_t)(i) >> (32 - (bits))))
/** 32Bit right rotation, inlined. */
#define right_rotate32(i, bits)                                                \
  (((uint32_t)(i) >> (bits)) | ((uint32_t)(i) << (32 - (bits))))
/** 64Bit left rotation, inlined. */
#define left_rotate64(i, bits)                                                 \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))
/** 64Bit right rotation, inlined. */
#define right_rotate64(i, bits)                                                \
  (((uint64_t)(i) >> (bits)) | ((uint64_t)(i) << (64 - (bits))))
/** unknown size element - left rotation, inlined. */
#define left_rotate(i, bits) (((i) << (bits)) | ((i) >> (sizeof((i)) - (bits))))
/** unknown size element - right rotation, inlined. */
#define right_rotate(i, bits)                                                  \
  (((i) >> (bits)) | ((i) << (sizeof((i)) - (bits))))
/** inplace byte swap 16 bit integer */
#define bswap16(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFU) << 8) | (((i)&0xFF00U) >> 8);                           \
  } while (0);
/** inplace byte swap 32 bit integer */
#define bswap32(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFUL) << 24) | (((i)&0xFF00UL) << 8) |                       \
          (((i)&0xFF0000UL) >> 8) | (((i)&0xFF000000UL) >> 24);                \
  } while (0);
/** inplace byte swap 64 bit integer */
#define bswap64(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                    \
          (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |             \
          (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |     \
          (((i)&0xFF000000000000ULL) >> 40) |                                  \
          (((i)&0xFF00000000000000ULL) >> 56);                                 \
  } while (0);
/** get the byte swap value of a 128 bit ...??? */
#define gbswap128(c)                                                           \
  (((*((__uint128_t *)(c))) & 0xFFULL) << 120) |                               \
      (((*((__uint128_t *)(c))) & 0xFF00ULL) << 104) |                         \
      (((*((__uint128_t *)(c))) & 0xFF0000ULL) << 88) |                        \
      (((*((__uint128_t *)(c))) & 0xFF000000ULL) << 72) |                      \
      (((*((__uint128_t *)(c))) & 0xFF00000000ULL) << 56) |                    \
      (((*((__uint128_t *)(c))) & 0xFF0000000000ULL) << 40) |                  \
      (((*((__uint128_t *)(c))) & 0xFF000000000000ULL) << 24) |                \
      (((*((__uint128_t *)(c))) & 0xFF00000000000000ULL) << 8) |               \
      (((*((__uint128_t *)(c))) & 0xFF0000000000000000ULL) >> 8) |             \
      (((*((__uint128_t *)(c))) & 0xFF000000000000000000ULL) >> 24) |          \
      (((*((__uint128_t *)(c))) & 0xFF00000000000000000000ULL) >> 40) |        \
      (((*((__uint128_t *)(c))) & 0xFF0000000000000000000000ULL) >> 56) |      \
      (((*((__uint128_t *)(c))) & 0xFF000000000000000000000000ULL) >> 72) |    \
      (((*((__uint128_t *)(c))) & 0xFF00000000000000000000000000ULL) >> 88) |  \
      (((*((__uint128_t *)(c))) & 0xFF0000000000000000000000000000ULL) >> 104)

#ifdef HAVE_X86Intrin
#undef bswap64
#define bswap64(i)                                                             \
  { __asm__("bswapq %0" : "+r"(i) :); }

// shadow sched_yield as _mm_pause for spinwait
#define sched_yield() _mm_pause()
#endif

/* ***************************************************************************
SHA-2 hashing
*/

static const uint8_t sha2_padding[128] = {0x80, 0};

/* SHA-224 and SHA-256 constants */
static uint32_t sha2_256_words[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/* SHA-512 and friends constants */
static uint64_t sha2_512_words[] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f,
    0xe9b5dba58189dbbc, 0x3956c25bf348b538, 0x59f111f1b605d019,
    0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242,
    0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
    0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3,
    0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 0x2de92c6f592b0275,
    0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f,
    0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
    0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc,
    0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6,
    0x92722c851482353b, 0xa2bfe8a14cf10364, 0xa81a664bbc423001,
    0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
    0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99,
    0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb,
    0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc,
    0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915,
    0xc67178f2e372532b, 0xca273eceea26619c, 0xd186b8c721c0c207,
    0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba,
    0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
    0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a,
    0x5fcb6fab3ad6faec, 0x6c44198c4a475817};

/* Specific Macros for the SHA-2 processing */

#define Ch(x, y, z) (((x) & (y)) ^ ((~(x)) & z))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define Eps0_32(x)                                                             \
  (right_rotate32((x), 2) ^ right_rotate32((x), 13) ^ right_rotate32((x), 22))
#define Eps1_32(x)                                                             \
  (right_rotate32((x), 6) ^ right_rotate32((x), 11) ^ right_rotate32((x), 25))
#define Omg0_32(x)                                                             \
  (right_rotate32((x), 7) ^ right_rotate32((x), 18) ^ (((x) >> 3)))
#define Omg1_32(x)                                                             \
  (right_rotate32((x), 17) ^ right_rotate32((x), 19) ^ (((x) >> 10)))

#define Eps0_64(x)                                                             \
  (right_rotate64((x), 28) ^ right_rotate64((x), 34) ^ right_rotate64((x), 39))
#define Eps1_64(x)                                                             \
  (right_rotate64((x), 14) ^ right_rotate64((x), 18) ^ right_rotate64((x), 41))
#define Omg0_64(x)                                                             \
  (right_rotate64((x), 1) ^ right_rotate64((x), 8) ^ (((x) >> 7)))
#define Omg1_64(x)                                                             \
  (right_rotate64((x), 19) ^ right_rotate64((x), 61) ^ (((x) >> 6)))

#ifdef __BIG_ENDIAN__
/** Converts a 4 byte string to a uint32_t word. careful with alignment! */
#define str2word32(c) (*((uint32_t *)(c)))
#define str2word64(c) (*((uint64_t *)(c)))
#else
/**
Converts a 4 byte string to a Big Endian uint32_t word. (ignores alignment!)
*/
#define str2word32(c)                                                          \
  (((*((uint32_t *)(c))) & 0xFFUL) << 24) |                                    \
      (((*((uint32_t *)(c))) & 0xFF00UL) << 8) |                               \
      (((*((uint32_t *)(c))) & 0xFF0000UL) >> 8) |                             \
      (((*((uint32_t *)(c))) & 0xFF000000UL) >> 24)
#define str2word64(c)                                                          \
  (((*((uint64_t *)(c))) & 0xFFULL) << 56) |                                   \
      (((*((uint64_t *)(c))) & 0xFF00ULL) << 40) |                             \
      (((*((uint64_t *)(c))) & 0xFF0000ULL) << 24) |                           \
      (((*((uint64_t *)(c))) & 0xFF000000ULL) << 8) |                          \
      (((*((uint64_t *)(c))) & 0xFF00000000ULL) >> 8) |                        \
      (((*((uint64_t *)(c))) & 0xFF0000000000ULL) >> 24) |                     \
      (((*((uint64_t *)(c))) & 0xFF000000000000ULL) >> 40) |                   \
      (((*((uint64_t *)(c))) & 0xFF00000000000000ULL) >> 56);
#endif

/**
Process the buffer once full.
*/
static inline void perform_all_rounds(sha2_s *s, const uint8_t *data) {
  if (s->type & 1) { /* 512 derived type */
    // process values for the 64bit words
    uint64_t a = s->digest.i64[0];
    uint64_t b = s->digest.i64[1];
    uint64_t c = s->digest.i64[2];
    uint64_t d = s->digest.i64[3];
    uint64_t e = s->digest.i64[4];
    uint64_t f = s->digest.i64[5];
    uint64_t g = s->digest.i64[6];
    uint64_t h = s->digest.i64[7];
    uint64_t t1, t2, w[80];
    w[0] = str2word64(data);
    w[1] = str2word64(data + 8);
    w[2] = str2word64(data + 16);
    w[3] = str2word64(data + 24);
    w[4] = str2word64(data + 32);
    w[5] = str2word64(data + 40);
    w[6] = str2word64(data + 48);
    w[7] = str2word64(data + 56);
    w[8] = str2word64(data + 64);
    w[9] = str2word64(data + 72);
    w[10] = str2word64(data + 80);
    w[11] = str2word64(data + 88);
    w[12] = str2word64(data + 96);
    w[13] = str2word64(data + 104);
    w[14] = str2word64(data + 112);
    w[15] = str2word64(data + 120);

#define perform_single_round(i)                                                \
  t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];                \
  t2 = Eps0_64(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

#define perform_4rounds(i)                                                     \
  perform_single_round(i);                                                     \
  perform_single_round(i + 1);                                                 \
  perform_single_round(i + 2);                                                 \
  perform_single_round(i + 3);

    perform_4rounds(0);
    perform_4rounds(4);
    perform_4rounds(8);
    perform_4rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[i] = Omg1_64(w[i - 2]) + w[i - 7] + Omg0_64(w[i - 15]) + w[i - 16];        \
  t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];                \
  t2 = Eps0_64(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(16);
    perform_4rounds(20);
    perform_4rounds(24);
    perform_4rounds(28);
    perform_4rounds(32);
    perform_4rounds(36);
    perform_4rounds(40);
    perform_4rounds(44);
    perform_4rounds(48);
    perform_4rounds(52);
    perform_4rounds(56);
    perform_4rounds(60);
    perform_4rounds(64);
    perform_4rounds(68);
    perform_4rounds(72);
    perform_4rounds(76);

    s->digest.i64[0] += a;
    s->digest.i64[1] += b;
    s->digest.i64[2] += c;
    s->digest.i64[3] += d;
    s->digest.i64[4] += e;
    s->digest.i64[5] += f;
    s->digest.i64[6] += g;
    s->digest.i64[7] += h;
    return;
  } else {
    // process values for the 32bit words
    uint32_t a = s->digest.i32[0];
    uint32_t b = s->digest.i32[1];
    uint32_t c = s->digest.i32[2];
    uint32_t d = s->digest.i32[3];
    uint32_t e = s->digest.i32[4];
    uint32_t f = s->digest.i32[5];
    uint32_t g = s->digest.i32[6];
    uint32_t h = s->digest.i32[7];
    uint32_t t1, t2, w[64];

    w[0] = str2word32(data);
    w[1] = str2word32(data + 4);
    w[2] = str2word32(data + 8);
    w[3] = str2word32(data + 12);
    w[4] = str2word32(data + 16);
    w[5] = str2word32(data + 20);
    w[6] = str2word32(data + 24);
    w[7] = str2word32(data + 28);
    w[8] = str2word32(data + 32);
    w[9] = str2word32(data + 36);
    w[10] = str2word32(data + 40);
    w[11] = str2word32(data + 44);
    w[12] = str2word32(data + 48);
    w[13] = str2word32(data + 52);
    w[14] = str2word32(data + 56);
    w[15] = str2word32(data + 60);

#undef perform_single_round
#define perform_single_round(i)                                                \
  t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];                \
  t2 = Eps0_32(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(0);
    perform_4rounds(4);
    perform_4rounds(8);
    perform_4rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[i] = Omg1_32(w[i - 2]) + w[i - 7] + Omg0_32(w[i - 15]) + w[i - 16];        \
  t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];                \
  t2 = Eps0_32(a) + Maj(a, b, c);                                              \
  h = g;                                                                       \
  g = f;                                                                       \
  f = e;                                                                       \
  e = d + t1;                                                                  \
  d = c;                                                                       \
  c = b;                                                                       \
  b = a;                                                                       \
  a = t1 + t2;

    perform_4rounds(16);
    perform_4rounds(20);
    perform_4rounds(24);
    perform_4rounds(28);
    perform_4rounds(32);
    perform_4rounds(36);
    perform_4rounds(40);
    perform_4rounds(44);
    perform_4rounds(48);
    perform_4rounds(52);
    perform_4rounds(56);
    perform_4rounds(60);

    s->digest.i32[0] += a;
    s->digest.i32[1] += b;
    s->digest.i32[2] += c;
    s->digest.i32[3] += d;
    s->digest.i32[4] += e;
    s->digest.i32[5] += f;
    s->digest.i32[6] += g;
    s->digest.i32[7] += h;
  }
}

/*****************************************************************************
API
*/

/**
Initialize/reset the SHA-2 object.

SHA-2 is actually a family of functions with different variants. When
initializing the SHA-2 container, you must select the variant you intend to
apply. The following are valid options (see the sha2_variant enum):

- SHA_512 (== 0)
- SHA_384
- SHA_512_224
- SHA_512_256
- SHA_256
- SHA_224

*/
sha2_s bscrypt_sha2_init(sha2_variant variant) {
  if (variant == SHA_256) {
    return (sha2_s){
        .type = SHA_256,
        .digest.i32[0] = 0x6a09e667,
        .digest.i32[1] = 0xbb67ae85,
        .digest.i32[2] = 0x3c6ef372,
        .digest.i32[3] = 0xa54ff53a,
        .digest.i32[4] = 0x510e527f,
        .digest.i32[5] = 0x9b05688c,
        .digest.i32[6] = 0x1f83d9ab,
        .digest.i32[7] = 0x5be0cd19,
    };
  } else if (variant == SHA_384) {
    return (sha2_s){
        .type = SHA_384,
        .digest.i64[0] = 0xcbbb9d5dc1059ed8,
        .digest.i64[1] = 0x629a292a367cd507,
        .digest.i64[2] = 0x9159015a3070dd17,
        .digest.i64[3] = 0x152fecd8f70e5939,
        .digest.i64[4] = 0x67332667ffc00b31,
        .digest.i64[5] = 0x8eb44a8768581511,
        .digest.i64[6] = 0xdb0c2e0d64f98fa7,
        .digest.i64[7] = 0x47b5481dbefa4fa4,
    };
  } else if (variant == SHA_512) {
    return (sha2_s){
        .type = SHA_512,
        .digest.i64[0] = 0x6a09e667f3bcc908,
        .digest.i64[1] = 0xbb67ae8584caa73b,
        .digest.i64[2] = 0x3c6ef372fe94f82b,
        .digest.i64[3] = 0xa54ff53a5f1d36f1,
        .digest.i64[4] = 0x510e527fade682d1,
        .digest.i64[5] = 0x9b05688c2b3e6c1f,
        .digest.i64[6] = 0x1f83d9abfb41bd6b,
        .digest.i64[7] = 0x5be0cd19137e2179,
    };
  } else if (variant == SHA_224) {
    return (sha2_s){
        .type = SHA_224,
        .digest.i32[0] = 0xc1059ed8,
        .digest.i32[1] = 0x367cd507,
        .digest.i32[2] = 0x3070dd17,
        .digest.i32[3] = 0xf70e5939,
        .digest.i32[4] = 0xffc00b31,
        .digest.i32[5] = 0x68581511,
        .digest.i32[6] = 0x64f98fa7,
        .digest.i32[7] = 0xbefa4fa4,
    };
  } else if (variant == SHA_512_224) {
    return (sha2_s){
        .type = SHA_512_224,
        .digest.i64[0] = 0x8c3d37c819544da2,
        .digest.i64[1] = 0x73e1996689dcd4d6,
        .digest.i64[2] = 0x1dfab7ae32ff9c82,
        .digest.i64[3] = 0x679dd514582f9fcf,
        .digest.i64[4] = 0x0f6d2b697bd44da8,
        .digest.i64[5] = 0x77e36f7304c48942,
        .digest.i64[6] = 0x3f9d85a86a1d36c8,
        .digest.i64[7] = 0x1112e6ad91d692a1,
    };
  } else if (variant == SHA_512_256) {
    return (sha2_s){
        .type = SHA_512_256,
        .digest.i64[0] = 0x22312194fc2bf72c,
        .digest.i64[1] = 0x9f555fa3c84c64c2,
        .digest.i64[2] = 0x2393b86b6f53b151,
        .digest.i64[3] = 0x963877195940eabd,
        .digest.i64[4] = 0x96283ee2a88effe3,
        .digest.i64[5] = 0xbe5e1e2553863992,
        .digest.i64[6] = 0x2b0199fc2c85b8aa,
        .digest.i64[7] = 0x0eb72ddc81c52ca2,
    };
  }
  fprintf(stderr, "bscrypt SHA2 ERROR - variant unknown\n");
  exit(2);
}

/**
Writes data to the SHA-2 buffer.
*/
void bscrypt_sha2_write(sha2_s *s, const void *data, size_t len) {
  size_t in_buffer;
  size_t partial;
  if (s->type & 1) { /* 512 type derived */
#if defined(HAS_UINT128)
    in_buffer = s->length.i & 127;
    s->length.i += len;
#else
    in_buffer = s->length.words[0] & 127;
    if (s->length.words[0] + len < s->length.words[0]) {
      /* we are at wraping around the 64bit limit */
      s->length.words[1] = (s->length.words[1] << 1) | 1;
    }
    s->length.words[0] += len;
#endif
    partial = 128 - in_buffer;

    if (partial > len) {
      memcpy(s->buffer + in_buffer, data, len);
      return;
    }
    if (in_buffer) {
      memcpy(s->buffer + in_buffer, data, partial);
      len -= partial;
      data = (void *)((uintptr_t)data + partial);
      perform_all_rounds(s, s->buffer);
    }
    while (len >= 128) {
      perform_all_rounds(s, data);
      data = (void *)((uintptr_t)data + 128);
      len -= 128;
    }
    if (len) {
      memcpy(s->buffer + in_buffer, data, len);
    }
    return;
  }
  /* else... NOT 512 bits derived (64bit base) */

  in_buffer = s->length.words[0] & 63;
  partial = 64 - in_buffer;

  s->length.words[0] += len;

  if (partial > len) {
    memcpy(s->buffer + in_buffer, data, len);
    return;
  }
  if (in_buffer) {
    memcpy(s->buffer + in_buffer, data, partial);
    len -= partial;
    data = (void *)((uintptr_t)data + partial);
    perform_all_rounds(s, s->buffer);
  }
  while (len >= 64) {
    perform_all_rounds(s, data);
    data = (void *)((uintptr_t)data + 64);
    len -= 64;
  }
  if (len) {
    memcpy(s->buffer + in_buffer, data, len);
  }
  return;
}

/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *bscrypt_sha2_result(sha2_s *s) {
  if (s->type & 1) {
/* 512 bits derived hashing */

#if defined(HAS_UINT128)
    size_t in_buffer = s->length.i & 127;
#else
    size_t in_buffer = s->length.words[0] & 127;
#endif

    if (in_buffer > 111) {
      memcpy(s->buffer + in_buffer, sha2_padding, 128 - in_buffer);
      perform_all_rounds(s, s->buffer);
      memcpy(s->buffer, sha2_padding + 1, 112);
    } else if (in_buffer != 111) {
      memcpy(s->buffer + in_buffer, sha2_padding, 112 - in_buffer);
    } else {
      s->buffer[111] = sha2_padding[0];
    }
/* store the length in BITS - alignment should be promised by struct */
/* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */

#if defined(HAS_UINT128)
    s->length.i = s->length.i << 3;
#else
    s->length.words[1] = (s->length.words[1] << 3) | (s->length.words[0] >> 61);
    s->length.words[0] = s->length.words[0] << 3;
#endif

#ifndef __BIG_ENDIAN__
    bswap64(s->length.words[0]);
    bswap64(s->length.words[1]);
    {
      uint_fast64_t tmp = s->length.words[0];
      s->length.words[0] = s->length.words[1];
      s->length.words[1] = tmp;
    }
#endif

#if defined(HAS_UINT128)
    __uint128_t *len = (__uint128_t *)(s->buffer + 112);
    *len = s->length.i;
#else
    uint64_t *len = (uint64_t *)(s->buffer + 112);
    len[0] = s->length.words[0];
    len[1] = s->length.words[1];
#endif
    perform_all_rounds(s, s->buffer);

/* change back to little endian, if required */
#ifndef __BIG_ENDIAN__
    bswap64(s->digest.i64[0]);
    bswap64(s->digest.i64[1]);
    bswap64(s->digest.i64[2]);
    bswap64(s->digest.i64[3]);
    bswap64(s->digest.i64[4]);
    bswap64(s->digest.i64[5]);
    bswap64(s->digest.i64[6]);
    bswap64(s->digest.i64[7]);
#endif
    // set NULL bytes for SHA_224
    if (s->type == SHA_512_224)
      s->digest.str[28] = 0;
    // set NULL bytes for SHA_256
    else if (s->type == SHA_512_256)
      s->digest.str[32] = 0;
    // set NULL bytes for SHA_384
    else if (s->type == SHA_384)
      s->digest.str[48] = 0;
    s->digest.str[64] = 0; /* sometimes the optimizer messes the NUL sequence */
    // fprintf(stderr, "result requested, in hex, is:");
    // for (size_t i = 0; i < 20; i++)
    //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
    // fprintf(stderr, "\r\n");
    return (char *)s->digest.str;
  }

  size_t in_buffer = s->length.words[0] & 63;
  if (in_buffer > 55) {
    memcpy(s->buffer + in_buffer, sha2_padding, 64 - in_buffer);
    perform_all_rounds(s, s->buffer);
    memcpy(s->buffer, sha2_padding + 1, 56);
  } else if (in_buffer != 55) {
    memcpy(s->buffer + in_buffer, sha2_padding, 56 - in_buffer);
  } else {
    s->buffer[55] = sha2_padding[0];
  }
  /* store the length in BITS - alignment should be promised by struct */
  /* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */
  uint64_t *len = (uint64_t *)(s->buffer + 56);
  *len = s->length.words[0] << 3;
#ifndef __BIG_ENDIAN__
  bswap64(*len);
#endif
  perform_all_rounds(s, s->buffer);

/* change back to little endian, if required */
#ifndef __BIG_ENDIAN__

  bswap32(s->digest.i32[0]);
  bswap32(s->digest.i32[1]);
  bswap32(s->digest.i32[2]);
  bswap32(s->digest.i32[3]);
  bswap32(s->digest.i32[4]);
  bswap32(s->digest.i32[5]);
  bswap32(s->digest.i32[6]);
  bswap32(s->digest.i32[7]);

#endif
  // set NULL bytes for SHA_224
  if (s->type == SHA_224)
    s->digest.str[28] = 0;
  // fprintf(stderr, "SHA-2 result requested, in hex, is:");
  // for (size_t i = 0; i < (s->type_512 ? 64 : 32); i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return (char *)s->digest.str;
}

/*******************************************************************************
SHA-2 testing
*/
#if defined(DEBUG) && DEBUG == 1

// SHA_512 = 1, SHA_512_256 = 3, SHA_512_224 = 5, SHA_384 = 7, SHA_256 = 2,
//              SHA_224 = 4,

static char *sha2_variant_names[] = {
    "unknown", "SHA_512",     "SHA_256", "SHA_512_256",
    "SHA_224", "SHA_512_224", "none",    "SHA_384",
};

// clang-format off
#if defined(HAVE_OPENSSL)
#  include <openssl/sha.h>
#endif
// clang-format on

void bscrypt_test_sha2(void) {
  sha2_s s;
  char *expect = NULL;
  char *got = NULL;
  char *str = "";
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "bscrypt SHA-2 struct size: %lu\n", sizeof(sha2_s));
  fprintf(stderr, "+ bscrypt");
  // start tests
  s = bscrypt_sha2_init(SHA_224);
  bscrypt_sha2_write(&s, str, 0);
  expect = "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9\x47\x61\x02\xbb\x28\x82\x34\xc4"
           "\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a\xc5\xb3\xe4\x2f";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_256);
  bscrypt_sha2_write(&s, str, 0);
  expect =
      "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27"
      "\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_384);
  bscrypt_sha2_write(&s, str, 0);
  expect = "\x38\xb0\x60\xa7\x51\xac\x96\x38\x4c\xd9\x32\x7e"
           "\xb1\xb1\xe3\x6a\x21\xfd\xb7\x11\x14\xbe\x07\x43\x4c\x0c"
           "\xc7\xbf\x63\xf6\xe1\xda\x27\x4e\xde\xbf\xe7\x6f\x65\xfb"
           "\xd5\x1a\xd2\xf1\x48\x98\xb9\x5b";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_512);
  bscrypt_sha2_write(&s, str, 0);
  expect = "\xcf\x83\xe1\x35\x7e\xef\xb8\xbd\xf1\x54\x28\x50\xd6\x6d"
           "\x80\x07\xd6\x20\xe4\x05\x0b\x57\x15\xdc\x83\xf4\xa9\x21"
           "\xd3\x6c\xe9\xce\x47\xd0\xd1\x3c\x5d\x85\xf2\xb0\xff\x83"
           "\x18\xd2\x87\x7e\xec\x2f\x63\xb9\x31\xbd\x47\x41\x7a\x81"
           "\xa5\x38\x32\x7a\xf9\x27\xda\x3e";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_512_224);
  bscrypt_sha2_write(&s, str, 0);
  expect = "\x6e\xd0\xdd\x02\x80\x6f\xa8\x9e\x25\xde\x06\x0c\x19\xd3"
           "\xac\x86\xca\xbb\x87\xd6\xa0\xdd\xd0\x5c\x33\x3b\x84\xf4";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_512_256);
  bscrypt_sha2_write(&s, str, 0);
  expect = "\xc6\x72\xb8\xd1\xef\x56\xed\x28\xab\x87\xc3\x62\x2c\x51\x14\x06"
           "\x9b\xdd\x3a\xd7\xb8\xf9\x73\x74\x98\xd0\xc0\x1e\xce\xf0\x96\x7a";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  s = bscrypt_sha2_init(SHA_512);
  str = "god is a rotten tomato";
  bscrypt_sha2_write(&s, str, strlen(str));
  expect = "\x61\x97\x4d\x41\x9f\x77\x45\x21\x09\x4e\x95\xa3\xcb\x4d\xe4\x79"
           "\x26\x32\x2f\x2b\xe2\x62\x64\x5a\xb4\x5d\x3f\x73\x69\xef\x46\x20"
           "\xb2\xd3\xce\xda\xa9\xc2\x2c\xac\xe3\xf9\x02\xb2\x20\x5d\x2e\xfd"
           "\x40\xca\xa0\xc1\x67\xe0\xdc\xdf\x60\x04\x3e\x4e\x76\x87\x82\x74";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  // s = bscrypt_sha2_init(SHA_256);
  // str = "The quick brown fox jumps over the lazy dog";
  // bscrypt_sha2_write(&s, str, strlen(str));
  // expect =
  //     "\xd7\xa8\xfb\xb3\x07\xd7\x80\x94\x69\xca\x9a\xbc\xb0\x08\x2e\x4f"
  //     "\x8d\x56\x51\xe4\x6d\x3c\xdb\x76\x2d\x02\xd0\xbf\x37\xc9\xe5\x92";
  // got = bscrypt_sha2_result(&s);
  // if (strcmp(expect, got))
  //   goto error;

  s = bscrypt_sha2_init(SHA_224);
  str = "The quick brown fox jumps over the lazy dog";
  bscrypt_sha2_write(&s, str, strlen(str));
  expect = "\x73\x0e\x10\x9b\xd7\xa8\xa3\x2b\x1c\xb9\xd9\xa0\x9a\xa2"
           "\x32\x5d\x24\x30\x58\x7d\xdb\xc0\xc3\x8b\xad\x91\x15\x25";
  got = bscrypt_sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  fprintf(stderr, " SHA-2 passed.\n");

#ifdef HAVE_OPENSSL
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "bscrypt SHA-2 struct size: %lu\n", sizeof(sha2_s));
  fprintf(stderr, "OpenSSL SHA-2/256 struct size: %lu\n", sizeof(SHA256_CTX));
  fprintf(stderr, "OpenSSL SHA-2/512 struct size: %lu\n", sizeof(SHA512_CTX));
  fprintf(stderr, "===================================\n");
  SHA512_CTX s2;
  SHA256_CTX s3;
  unsigned char hash[SHA512_DIGEST_LENGTH + 1];
  hash[SHA512_DIGEST_LENGTH] = 0;
  clock_t start = clock();
  for (size_t i = 0; i < 100000; i++) {
    s = bscrypt_sha2_init(SHA_512);
    bscrypt_sha2_write(&s, "The quick brown fox jumps over the lazy dog", 43);
    bscrypt_sha2_result(&s);
  }
  fprintf(stderr, "bscrypt 100K SHA-2/512: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    SHA512_Init(&s2);
    SHA512_Update(&s2, "The quick brown fox jumps over the lazy dog", 43);
    SHA512_Final(hash, &s2);
  }
  fprintf(stderr, "OpenSSL 100K SHA-2/512: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    s = bscrypt_sha2_init(SHA_256);
    bscrypt_sha2_write(&s, "The quick brown fox jumps over the lazy dog", 43);
    bscrypt_sha2_result(&s);
  }
  fprintf(stderr, "bscrypt 100K SHA-2/256: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

  hash[SHA256_DIGEST_LENGTH] = 0;
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    SHA256_Init(&s3);
    SHA256_Update(&s3, "The quick brown fox jumps over the lazy dog", 43);
    SHA256_Final(hash, &s3);
  }
  fprintf(stderr, "OpenSSL 100K SHA-2/256: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

  fprintf(stderr, "===================================\n");
#endif

  return;

error:
  fprintf(stderr,
          ":\n--- bscrypt SHA-2 Test FAILED!\ntype: "
          "%s (%d)\nstring %s\nexpected:\n",
          sha2_variant_names[s.type], s.type, str);
  while (*expect)
    fprintf(stderr, "%02x", *(expect++) & 0xFF);
  fprintf(stderr, "\ngot:\n");
  while (*got)
    fprintf(stderr, "%02x", *(got++) & 0xFF);
  fprintf(stderr, "\n");
}
#endif
