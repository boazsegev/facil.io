/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "sha1.h"
/*****************************************************************************
Useful Macros - Not all of them are used here, but it's a copy-paste convenience
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

static const uint8_t sha1_padding[64] = {0x80, 0};

#ifdef __BIG_ENDIAN__
/** Converts a 4 byte string to a uint32_t word. careful with alignment! */
#define str2word(c) (*((uint32_t *)(c)))
#else
/**
Converts a 4 byte string to a Big Endian uint32_t word. (ignores alignment!)
*/
#define str2word(c)                                                            \
  (((*((uint32_t *)(c))) & 0xFFUL) << 24) |                                    \
      (((*((uint32_t *)(c))) & 0xFF00UL) << 8) |                               \
      (((*((uint32_t *)(c))) & 0xFF0000UL) >> 8) |                             \
      (((*((uint32_t *)(c))) & 0xFF000000UL) >> 24)
#endif
/**
Process the buffer once full.
*/
static inline void perform_all_rounds(sha1_s *s, const uint8_t *buffer) {
  /* collect data */
  uint32_t a = s->digest.i[0];
  uint32_t b = s->digest.i[1];
  uint32_t c = s->digest.i[2];
  uint32_t d = s->digest.i[3];
  uint32_t e = s->digest.i[4];
  uint32_t t, w[16];
  /* copy data to words, performing byte swapping as needed */
  w[0] = str2word(buffer);
  w[1] = str2word(buffer + 4);
  w[2] = str2word(buffer + 8);
  w[3] = str2word(buffer + 12);
  w[4] = str2word(buffer + 16);
  w[5] = str2word(buffer + 20);
  w[6] = str2word(buffer + 24);
  w[7] = str2word(buffer + 28);
  w[8] = str2word(buffer + 32);
  w[9] = str2word(buffer + 36);
  w[10] = str2word(buffer + 40);
  w[11] = str2word(buffer + 44);
  w[12] = str2word(buffer + 48);
  w[13] = str2word(buffer + 52);
  w[14] = str2word(buffer + 56);
  w[15] = str2word(buffer + 60);
/* perform rounds */
#define perform_single_round(num)                                              \
  t = left_rotate32(a, 5) + e + w[num] + ((b & c) | ((~b) & d)) + 0x5A827999;  \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;

#define perform_four_rounds(i)                                                 \
  perform_single_round(i);                                                     \
  perform_single_round(i + 1);                                                 \
  perform_single_round(i + 2);                                                 \
  perform_single_round(i + 3);

  perform_four_rounds(0);
  perform_four_rounds(4);
  perform_four_rounds(8);
  perform_four_rounds(12);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = left_rotate32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^               \
                             w[(i - 14) & 15] ^ w[(i - 16) & 15]),             \
                            1);                                                \
  t = left_rotate32(a, 5) + e + w[(i)&15] + ((b & c) | ((~b) & d)) +           \
      0x5A827999;                                                              \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(16);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = left_rotate32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^               \
                             w[(i - 14) & 15] ^ w[(i - 16) & 15]),             \
                            1);                                                \
  t = left_rotate32(a, 5) + e + w[(i)&15] + (b ^ c ^ d) + 0x6ED9EBA1;          \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(20);
  perform_four_rounds(24);
  perform_four_rounds(28);
  perform_four_rounds(32);
  perform_four_rounds(36);

#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = left_rotate32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^               \
                             w[(i - 14) & 15] ^ w[(i - 16) & 15]),             \
                            1);                                                \
  t = left_rotate32(a, 5) + e + w[(i)&15] + ((b & (c | d)) | (c & d)) +        \
      0x8F1BBCDC;                                                              \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;

  perform_four_rounds(40);
  perform_four_rounds(44);
  perform_four_rounds(48);
  perform_four_rounds(52);
  perform_four_rounds(56);
#undef perform_single_round
#define perform_single_round(i)                                                \
  w[(i)&15] = left_rotate32((w[(i - 3) & 15] ^ w[(i - 8) & 15] ^               \
                             w[(i - 14) & 15] ^ w[(i - 16) & 15]),             \
                            1);                                                \
  t = left_rotate32(a, 5) + e + w[(i)&15] + (b ^ c ^ d) + 0xCA62C1D6;          \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;
  perform_four_rounds(60);
  perform_four_rounds(64);
  perform_four_rounds(68);
  perform_four_rounds(72);
  perform_four_rounds(76);

  /* store data */
  s->digest.i[4] += e;
  s->digest.i[3] += d;
  s->digest.i[2] += c;
  s->digest.i[1] += b;
  s->digest.i[0] += a;
}

/* ***************************************************************************
SHA-1 hashing
*/

/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
sha1_s bscrypt_sha1_init(void) {
  return (sha1_s){.digest.i[0] = 0x67452301,
                  .digest.i[1] = 0xefcdab89,
                  .digest.i[2] = 0x98badcfe,
                  .digest.i[3] = 0x10325476,
                  .digest.i[4] = 0xc3d2e1f0};
}

/**
Writes data to the sha1 buffer.
*/
void bscrypt_sha1_write(sha1_s *s, const void *data, size_t len) {
  size_t in_buffer = s->length & 63;
  size_t partial = 64 - in_buffer;
  s->length += len;
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

char *bscrypt_sha1_result(sha1_s *s) {
  size_t in_buffer = s->length & 63;
  if (in_buffer > 55) {
    memcpy(s->buffer + in_buffer, sha1_padding, 64 - in_buffer);
    perform_all_rounds(s, s->buffer);
    memcpy(s->buffer, sha1_padding + 1, 56);
  } else if (in_buffer != 55) {
    memcpy(s->buffer + in_buffer, sha1_padding, 56 - in_buffer);
  } else {
    s->buffer[55] = sha1_padding[0];
  }
  /* store the length in BITS - alignment should be promised by struct */
  /* this must the number in BITS, encoded as a BIG ENDIAN 64 bit number */
  uint64_t *len = (uint64_t *)(s->buffer + 56);
  *len = s->length << 3;
#ifndef __BIG_ENDIAN__
  bswap64(*len);
#endif
  perform_all_rounds(s, s->buffer);

/* change back to little endian, if required */
#ifndef __BIG_ENDIAN__
  bswap32(s->digest.i[0]);
  bswap32(s->digest.i[1]);
  bswap32(s->digest.i[2]);
  bswap32(s->digest.i[3]);
  bswap32(s->digest.i[4]);
#endif
  // fprintf(stderr, "result requested, in hex, is:");
  // for (size_t i = 0; i < 20; i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return (char *)s->digest.str;
}

/*******************************************************************************
SHA-1 testing
*/
#if defined(DEBUG) && DEBUG == 1
#include <time.h>

// clang-format off
#if defined(HAVE_OPENSSL)
#  include <openssl/sha.h>
#endif
// clang-format on

void bscrypt_test_sha1(void) {
  struct {
    char *str;
    char hash[21];
  } sets[] = {
      {"The quick brown fox jumps over the lazy dog",
       {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e,
        0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12, 0}}, // a set with
                                                                   // a string
      {"",
       {
           0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
           0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09,
       }},        // an empty set
      {NULL, {0}} // Stop
  };
  int i = 0;
  sha1_s sha1;
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "bscrypt SHA-1 struct size: %lu\n", sizeof(sha1_s));
  fprintf(stderr, "+ bscrypt");
  while (sets[i].str) {
    sha1 = bscrypt_sha1_init();
    bscrypt_sha1_write(&sha1, sets[i].str, strlen(sets[i].str));
    if (strcmp(bscrypt_sha1_result(&sha1), sets[i].hash)) {
      fprintf(stderr,
              ":\n--- bscrypt SHA-1 Test FAILED!\nstring: %s\nexpected: ",
              sets[i].str);
      char *p = sets[i].hash;
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\ngot: ");
      p = bscrypt_sha1_result(&sha1);
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\n");
      return;
    }
    i++;
  }
  fprintf(stderr, " SHA-1 passed.\n");

#ifdef HAVE_OPENSSL
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "bscrypt SHA-1 struct size: %lu\n", sizeof(sha1_s));
  fprintf(stderr, "OpenSSL SHA-1 struct size: %lu\n", sizeof(SHA_CTX));
  fprintf(stderr, "===================================\n");

  unsigned char hash[SHA512_DIGEST_LENGTH + 1];
  hash[SHA512_DIGEST_LENGTH] = 0;
  clock_t start;
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    sha1 = bscrypt_sha1_init();
    bscrypt_sha1_write(&sha1, "The quick brown fox jumps over the lazy dog ",
                       43);
    bscrypt_sha1_result(&sha1);
  }
  fprintf(stderr, "bscrypt 100K SHA-1: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

  hash[SHA_DIGEST_LENGTH] = 0;
  SHA_CTX o_sh1;
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    SHA1_Init(&o_sh1);
    SHA1_Update(&o_sh1, "The quick brown fox jumps over the lazy dog", 43);
    SHA1_Final(hash, &o_sh1);
  }
  fprintf(stderr, "OpenSSL 100K SHA-1: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);

#endif
}
#endif
