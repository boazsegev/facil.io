#include "minicrypt.h"

/*******************************************************************************
Setup
*/

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

/*****************************************************************************
Useful Macros
*/

/** 32Bit left rotation, inlined. */
#define left_rotate32(i, bits) \
  (((uint32_t)(i) << (bits)) | ((uint32_t)(i) >> (32 - (bits))))
/** 32Bit right rotation, inlined. */
#define right_rotate32(i, bits) \
  (((uint32_t)(i) >> (bits)) | ((uint32_t)(i) << (32 - (bits))))
/** 64Bit left rotation, inlined. */
#define left_rotate64(i, bits) \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))
/** 64Bit right rotation, inlined. */
#define right_rotate64(i, bits) \
  (((uint64_t)(i) >> (bits)) | ((uint64_t)(i) << (64 - (bits))))
/** unknown size element - left rotation, inlined. */
#define left_rotate(i, bits) (((i) << (bits)) | ((i) >> (sizeof((i)) - (bits))))
/** unknown size element - right rotation, inlined. */
#define right_rotate(i, bits) \
  (((i) >> (bits)) | ((i) << (sizeof((i)) - (bits))))
/** inplace byte swap 16 bit integer */
#define bswap16(i)                                   \
  do {                                               \
    (i) = (((i)&0xFFU) << 8) | (((i)&0xFF00U) >> 8); \
  } while (0);
/** inplace byte swap 32 bit integer */
#define bswap32(i)                                              \
  do {                                                          \
    (i) = (((i)&0xFFUL) << 24) | (((i)&0xFF00UL) << 8) |        \
          (((i)&0xFF0000UL) >> 8) | (((i)&0xFF000000UL) >> 24); \
  } while (0);
/** inplace byte swap 64 bit integer */
#define bswap64(i)                                                         \
  do {                                                                     \
    (i) = (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                \
          (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |         \
          (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) | \
          (((i)&0xFF000000000000ULL) >> 40) |                              \
          (((i)&0xFF00000000000000ULL) >> 56);                             \
  } while (0);

/* ***************************************************************************
Machine specific changes
*/
// #ifdef __linux__
// #undef bswap16
// #undef bswap32
// #undef bswap64
// #include <machine/bswap.h>
// #endif
#ifdef HAVE_X86Intrin
// #undef bswap16
/*
#undef bswap32
#define bswap32(i) \
  { __asm__("bswap %k0" : "+r"(i) :); }
*/
#undef bswap64
#define bswap64(i) \
  { __asm__("bswapq %0" : "+r"(i) :); }

// shadow sched_yield as _mm_pause for spinwait
#define sched_yield() _mm_pause()
#endif

/* ***************************************************************************
SHA-1 hashing
*/

/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
void minicrypt_sha1_init(sha1_s* s) {
  memset(s, 0, sizeof(*s));
  s->digest.i[0] = 0x67452301;
  s->digest.i[1] = 0xefcdab89;
  s->digest.i[2] = 0x98badcfe;
  s->digest.i[3] = 0x10325476;
  s->digest.i[4] = 0xc3d2e1f0;
  s->initialized = 1;
}

/**
Process the buffer once full.
*/
static void sha1_process_buffer(sha1_s* s) {
  uint32_t a = s->digest.i[0];
  uint32_t b = s->digest.i[1];
  uint32_t c = s->digest.i[2];
  uint32_t d = s->digest.i[3];
  uint32_t e = s->digest.i[4];
  uint32_t t, w[80];
#define sha1_round(num)                                                       \
  w[num] = s->buffer.i[(num)];                                                \
  t = left_rotate32(a, 5) + e + w[num] + ((b & c) | ((~b) & d)) + 0x5A827999; \
  e = d;                                                                      \
  d = c;                                                                      \
  c = left_rotate32(b, 30);                                                   \
  b = a;                                                                      \
  a = t;
  sha1_round(0);
  sha1_round(1);
  sha1_round(2);
  sha1_round(3);
  sha1_round(4);
  sha1_round(5);
  sha1_round(6);
  sha1_round(7);
  sha1_round(8);
  sha1_round(9);
  sha1_round(10);
  sha1_round(11);
  sha1_round(12);
  sha1_round(13);
  sha1_round(14);
  sha1_round(15);
#undef sha1_round
#define sha1_round(i)                                                       \
  w[i] = left_rotate32((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);   \
  t = left_rotate32(a, 5) + e + w[i] + ((b & c) | ((~b) & d)) + 0x5A827999; \
  e = d;                                                                    \
  d = c;                                                                    \
  c = left_rotate32(b, 30);                                                 \
  b = a;                                                                    \
  a = t;
  sha1_round(16);
  sha1_round(17);
  sha1_round(18);
  sha1_round(19);
#undef sha1_round
#define sha1_round(i)                                                     \
  w[i] = left_rotate32((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
  t = left_rotate32(a, 5) + e + w[i] + (b ^ c ^ d) + 0x6ED9EBA1;          \
  e = d;                                                                  \
  d = c;                                                                  \
  c = left_rotate32(b, 30);                                               \
  b = a;                                                                  \
  a = t;
  sha1_round(20);
  sha1_round(21);
  sha1_round(22);
  sha1_round(23);
  sha1_round(24);
  sha1_round(25);
  sha1_round(26);
  sha1_round(27);
  sha1_round(28);
  sha1_round(29);
  sha1_round(30);
  sha1_round(31);
  sha1_round(32);
  sha1_round(33);
  sha1_round(34);
  sha1_round(35);
  sha1_round(36);
  sha1_round(37);
  sha1_round(38);
  sha1_round(39);
#undef sha1_round
#define sha1_round(i)                                                          \
  w[i] = left_rotate32((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);      \
  t = left_rotate32(a, 5) + e + w[i] + ((b & (c | d)) | (c & d)) + 0x8F1BBCDC; \
  e = d;                                                                       \
  d = c;                                                                       \
  c = left_rotate32(b, 30);                                                    \
  b = a;                                                                       \
  a = t;
  sha1_round(40);
  sha1_round(41);
  sha1_round(42);
  sha1_round(43);
  sha1_round(44);
  sha1_round(45);
  sha1_round(46);
  sha1_round(47);
  sha1_round(48);
  sha1_round(49);
  sha1_round(50);
  sha1_round(51);
  sha1_round(52);
  sha1_round(53);
  sha1_round(54);
  sha1_round(55);
  sha1_round(56);
  sha1_round(57);
  sha1_round(58);
  sha1_round(59);
#undef sha1_round
#define sha1_round(i)                                                     \
  w[i] = left_rotate32((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
  t = left_rotate32(a, 5) + e + w[i] + (b ^ c ^ d) + 0xCA62C1D6;          \
  e = d;                                                                  \
  d = c;                                                                  \
  c = left_rotate32(b, 30);                                               \
  b = a;                                                                  \
  a = t;
  sha1_round(60);
  sha1_round(61);
  sha1_round(62);
  sha1_round(63);
  sha1_round(64);
  sha1_round(65);
  sha1_round(66);
  sha1_round(67);
  sha1_round(68);
  sha1_round(69);
  sha1_round(70);
  sha1_round(71);
  sha1_round(72);
  sha1_round(73);
  sha1_round(74);
  sha1_round(75);
  sha1_round(76);
  sha1_round(77);
  sha1_round(78);
  sha1_round(79);
  // store data
  s->digest.i[4] += e;
  s->digest.i[3] += d;
  s->digest.i[2] += c;
  s->digest.i[1] += b;
  s->digest.i[0] += a;
}

/**
Add a single byte to the buffer and check the buffer's status.
*/
#ifdef __BIG_ENDIAN__
#define sha1_add_byte(s, byte) s->buffer.str[s->buffer_pos++] = byte;
#else
#define sha1_add_byte(s, byte) s->buffer.str[(s->buffer_pos++) ^ 3] = byte;
#endif

#define sha1_review_buffer(s) \
  if (s->buffer_pos == 0)     \
    sha1_process_buffer(s);

/**
Writes data to the sha1 buffer.
*/
int minicrypt_sha1_write(sha1_s* s, const char* data, size_t len) {
  if (!s || s->finalized)
    return -1;
  if (!s->initialized)
    minicrypt_sha1_init(s);
  // msg length is in bits, not bytes.
  s->msg_length.i += (len << 3);
  // add each byte to the sha1 hash's buffer... network byte issues apply
  while (len--) {
    sha1_add_byte(s, *(data++));
    sha1_review_buffer(s);
  }
  return 0;
}
/**
Finalizes the SHA1 hash, returning the Hashed data.

`sha1_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char* minicrypt_sha1_result(sha1_s* s) {
  // finalize the data if it wasn't finalized before
  if (!s->finalized) {
    // set the finalized flag
    s->finalized = 1;
    // pad the buffer
    sha1_add_byte(s, 0x80);
    // add 0 utill we reach the buffer's last 8 bytes (used for length data)
    while (s->buffer_pos > 56) {  // make sure we're not at the buffer's end
      sha1_add_byte(s, 0);
    }
    sha1_review_buffer(s);  // make sure the buffer isn't full
    while (s->buffer_pos != 56) {
      sha1_add_byte(s, 0);
    }

// add the total length data (this will cause the buffer to be processed).
// this must the number in BITS, encoded as a BIG ENDIAN 64 bit number.
// the 3 in the shifting == x8, translating bytes into bits.
// every time we add a byte, only the last 8 bits are added.
#ifdef __BIG_ENDIAN__
    // add length data, byte by byte
    sha1_add_byte(s, s->msg_length.str[0]);
    sha1_add_byte(s, s->msg_length.str[1]);
    sha1_add_byte(s, s->msg_length.str[2]);
    sha1_add_byte(s, s->msg_length.str[3]);
    sha1_add_byte(s, s->msg_length.str[4]);
    sha1_add_byte(s, s->msg_length.str[5]);
    sha1_add_byte(s, s->msg_length.str[6]);
    sha1_add_byte(s, s->msg_length.str[7]);
#else
    // add length data, reverse byte order (little endian)
    sha1_add_byte(s, s->msg_length.str[7]);
    sha1_add_byte(s, s->msg_length.str[6]);
    sha1_add_byte(s, s->msg_length.str[5]);
    sha1_add_byte(s, s->msg_length.str[4]);
    sha1_add_byte(s, s->msg_length.str[3]);
    sha1_add_byte(s, s->msg_length.str[2]);
    sha1_add_byte(s, s->msg_length.str[1]);
    sha1_add_byte(s, s->msg_length.str[0]);
#endif
    sha1_process_buffer(s);
// change back to little endian
// reverse byte order for each uint32 "word".
#ifndef __BIG_ENDIAN__
    bswap32(s->digest.i[0]);
    bswap32(s->digest.i[1]);
    bswap32(s->digest.i[2]);
    bswap32(s->digest.i[3]);
    bswap32(s->digest.i[4]);
#endif
  }
  // fprintf(stderr, "result requested, in hex, is:");
  // for (size_t i = 0; i < 20; i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return (char*)s->digest.str;
}

#undef sha1_review_buffer
/* ***************************************************************************
SHA-2 hashing
*/

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

#define Eps0_32(x) \
  (right_rotate32((x), 2) ^ right_rotate32((x), 13) ^ right_rotate32((x), 22))
#define Eps1_32(x) \
  (right_rotate32((x), 6) ^ right_rotate32((x), 11) ^ right_rotate32((x), 25))
#define Omg0_32(x) \
  (right_rotate32((x), 7) ^ right_rotate32((x), 18) ^ (((x) >> 3)))
#define Omg1_32(x) \
  (right_rotate32((x), 17) ^ right_rotate32((x), 19) ^ (((x) >> 10)))

#define Eps0_64(x) \
  (right_rotate64((x), 28) ^ right_rotate64((x), 34) ^ right_rotate64((x), 39))
#define Eps1_64(x) \
  (right_rotate64((x), 14) ^ right_rotate64((x), 18) ^ right_rotate64((x), 41))
#define Omg0_64(x) \
  (right_rotate64((x), 1) ^ right_rotate64((x), 8) ^ (((x) >> 7)))
#define Omg1_64(x) \
  (right_rotate64((x), 19) ^ right_rotate64((x), 61) ^ (((x) >> 6)))

#ifdef __BIG_ENDIAN__
#define sha2_set_byte(p, pos, byte) (p)->buffer.str[(pos)] = byte;
#else
#define sha2_set_byte(p, pos, byte) \
  (p)->buffer.str[(pos) ^ ((p)->type_512 ? 7 : 3)] = byte;
#endif

#define sha2_set_byte64(byte)
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
void minicrypt_sha2_init(sha2_s* s, sha2_variant variant) {
  memset(s, 0, sizeof(*s));
  if (variant == SHA_224) {
    s->type_512 = 0;
    s->digest.i32[0] = 0xc1059ed8;
    s->digest.i32[1] = 0x367cd507;
    s->digest.i32[2] = 0x3070dd17;
    s->digest.i32[3] = 0xf70e5939;
    s->digest.i32[4] = 0xffc00b31;
    s->digest.i32[5] = 0x68581511;
    s->digest.i32[6] = 0x64f98fa7;
    s->digest.i32[7] = 0xbefa4fa4;
    s->digest.str[32] = 0;  // NULL.
  } else if (variant == SHA_256) {
    s->type_512 = 0;
    s->digest.i32[0] = 0x6a09e667;
    s->digest.i32[1] = 0xbb67ae85;
    s->digest.i32[2] = 0x3c6ef372;
    s->digest.i32[3] = 0xa54ff53a;
    s->digest.i32[4] = 0x510e527f;
    s->digest.i32[5] = 0x9b05688c;
    s->digest.i32[6] = 0x1f83d9ab;
    s->digest.i32[7] = 0x5be0cd19;
    s->digest.str[32] = 0;  // NULL.
  } else if (variant == SHA_384) {
    s->type_512 = 1;
    s->digest.i64[0] = 0xcbbb9d5dc1059ed8;
    s->digest.i64[1] = 0x629a292a367cd507;
    s->digest.i64[2] = 0x9159015a3070dd17;
    s->digest.i64[3] = 0x152fecd8f70e5939;
    s->digest.i64[4] = 0x67332667ffc00b31;
    s->digest.i64[5] = 0x8eb44a8768581511;
    s->digest.i64[6] = 0xdb0c2e0d64f98fa7;
    s->digest.i64[7] = 0x47b5481dbefa4fa4;
  } else if (variant == SHA_512_224) {
    s->type_512 = 1;
    s->digest.i64[0] = 0x8c3d37c819544da2;
    s->digest.i64[1] = 0x73e1996689dcd4d6;
    s->digest.i64[2] = 0x1dfab7ae32ff9c82;
    s->digest.i64[3] = 0x679dd514582f9fcf;
    s->digest.i64[4] = 0x0f6d2b697bd44da8;
    s->digest.i64[5] = 0x77e36f7304c48942;
    s->digest.i64[6] = 0x3f9d85a86a1d36c8;
    s->digest.i64[7] = 0x1112e6ad91d692a1;
  } else if (variant == SHA_512_256) {
    s->type_512 = 1;
    s->digest.i64[0] = 0x22312194fc2bf72c;
    s->digest.i64[1] = 0x9f555fa3c84c64c2;
    s->digest.i64[2] = 0x2393b86b6f53b151;
    s->digest.i64[3] = 0x963877195940eabd;
    s->digest.i64[4] = 0x96283ee2a88effe3;
    s->digest.i64[5] = 0xbe5e1e2553863992;
    s->digest.i64[6] = 0x2b0199fc2c85b8aa;
    s->digest.i64[7] = 0x0eb72ddc81c52ca2;
  } else {
    s->type_512 = 1;
    s->digest.i64[0] = 0x6a09e667f3bcc908;
    s->digest.i64[1] = 0xbb67ae8584caa73b;
    s->digest.i64[2] = 0x3c6ef372fe94f82b;
    s->digest.i64[3] = 0xa54ff53a5f1d36f1;
    s->digest.i64[4] = 0x510e527fade682d1;
    s->digest.i64[5] = 0x9b05688c2b3e6c1f;
    s->digest.i64[6] = 0x1f83d9abfb41bd6b;
    s->digest.i64[7] = 0x5be0cd19137e2179;
  }
  s->type = variant;
  s->initialized = 1;
}

/**
Process the buffer once full.
*/
static void sha2_process_buffer(sha2_s* s) {
  if (s->type_512) {
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

    size_t i = 0;
    for (; i < 16; i++) {
      w[i] = s->buffer.i64[i];
      t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];
      t2 = Eps0_64(a) + Maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    for (; i < 80; i++) {
      w[i] = Omg1_64(w[i - 2]) + w[i - 7] + Omg0_64(w[i - 15]) + w[i - 16];
      t1 = h + Eps1_64(e) + Ch(e, f, g) + sha2_512_words[i] + w[i];
      t2 = Eps0_64(a) + Maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    s->digest.i64[0] += a;
    s->digest.i64[1] += b;
    s->digest.i64[2] += c;
    s->digest.i64[3] += d;
    s->digest.i64[4] += e;
    s->digest.i64[5] += f;
    s->digest.i64[6] += g;
    s->digest.i64[7] += h;

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
    size_t i = 0;
    for (; i < 16; i++) {
      w[i] = s->buffer.i32[i];
      t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];
      t2 = Eps0_32(a) + Maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    for (; i < 64; i++) {
      w[i] = Omg1_32(w[i - 2]) + w[i - 7] + Omg0_32(w[i - 15]) + w[i - 16];
      t1 = h + Eps1_32(e) + Ch(e, f, g) + sha2_256_words[i] + w[i];
      t2 = Eps0_32(a) + Maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    s->digest.i32[0] += a;
    s->digest.i32[1] += b;
    s->digest.i32[2] += c;
    s->digest.i32[3] += d;
    s->digest.i32[4] += e;
    s->digest.i32[5] += f;
    s->digest.i32[6] += g;
    s->digest.i32[7] += h;
    // reset buffer count
    s->buffer_pos = 0;
  }
}

/**
Writes data to the SHA-2 buffer.
*/
int minicrypt_sha2_write(sha2_s* s, const void* data, size_t len) {
  if (!s || s->finalized)
    return -1;
  if (!s->initialized)
    minicrypt_sha2_init(s, SHA_512);
  // msg length is in up to 128 bits long...
  s->msg_length.i += (__uint128_t)len << 3;
  // add each byte to the sha1 hash's buffer... network byte issues apply
  while (len--) {
    // add a byte to the buffer, consider network byte order .
    sha2_set_byte(s, s->buffer_pos, *((uint8_t*)(data++)));
    // update buffer position
    ++s->buffer_pos;
    // review chunk (1024/512 bits) processing
    if ((!s->type_512 && s->buffer_pos == 64) ||
        (s->type_512 && s->buffer_pos == 0)) {
      // s->buffer_pos wraps at 127 back to 0, so each 0 is the 1024 bits
      // (128 bytes) chunk marker to be processed.
      sha2_process_buffer(s);
    }
  }
  return 0;
}

/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char* minicrypt_sha2_result(sha2_s* s) {
  // finalize the data if it wasn't finalized before
  if (!s->finalized) {
    // set the finalized flag
    s->finalized = 1;

    // start padding
    sha2_set_byte(s, s->buffer_pos++, 0x80);

    // pad the message
    if (s->type_512)
      while (s->buffer_pos != 112) {
        if (!s->buffer_pos)
          sha2_process_buffer(s);
        sha2_set_byte(s, s->buffer_pos, 0);
        ++s->buffer_pos;
      }
    else
      while (s->buffer_pos != 56) {
        if (s->buffer_pos == 64)
          sha2_process_buffer(s);
        sha2_set_byte(s, s->buffer_pos, 0);
        ++s->buffer_pos;
      }

// add the total length data (this will cause the buffer to be processed).
// this must the number in BITS, encoded as a BIG ENDIAN 64 bit number.
// the 3 in the shifting == x8, translating bytes into bits.
// every time we add a byte, only the last 8 bits are added.
#ifdef __BIG_ENDIAN__
    // add length data, byte by byte.
    // add length data, byte by byte
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[0]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[1]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[2]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[3]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[4]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[5]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[6]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[7]);
    if (s->type_512) {
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[8]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[9]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[10]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[11]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[12]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[13]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[14]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[15]);
    }
#else
    // add length data, reverse byte order (little endian)
    // fprintf(stderr, "The %s bytes are relevant\n",
    //         (s->msg_length.str[15] ? "last" : "first"));
    if (s->type_512) {
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[15]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[14]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[13]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[12]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[11]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[10]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[9]);
      sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[8]);
    }
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[7]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[6]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[5]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[4]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[3]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[2]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[1]);
    sha2_set_byte(s, s->buffer_pos++, s->msg_length.str[0]);
#endif
    sha2_process_buffer(s);

#ifndef __BIG_ENDIAN__
    // change back to little endian
    if (s->type_512) {
      bswap64(s->digest.i64[0]);
      bswap64(s->digest.i64[1]);
      bswap64(s->digest.i64[2]);
      bswap64(s->digest.i64[3]);
      bswap64(s->digest.i64[4]);
      bswap64(s->digest.i64[5]);
      bswap64(s->digest.i64[6]);
      bswap64(s->digest.i64[7]);
    } else {
      bswap32(s->digest.i32[0]);
      bswap32(s->digest.i32[1]);
      bswap32(s->digest.i32[2]);
      bswap32(s->digest.i32[3]);
      bswap32(s->digest.i32[4]);
      bswap32(s->digest.i32[5]);
      bswap32(s->digest.i32[6]);
      bswap32(s->digest.i32[7]);
      // bswap32(s->digest.i32[8]);
      // bswap32(s->digest.i32[9]);
      // bswap32(s->digest.i32[10]);
      // bswap32(s->digest.i32[11]);
    }
#endif
    // set NULL bytes for SHA_224
    if (s->type == SHA_224 || s->type == SHA_512_224)
      s->digest.str[28] = 0;
    // set NULL bytes for SHA_256
    else if (s->type == SHA_512_256)
      s->digest.str[32] = 0;
    // set NULL bytes for SHA_384
    else if (s->type == SHA_384)
      s->digest.str[48] = 0;
  }
  // fprintf(stderr, "SHA-2 result requested, in hex, is:");
  // for (size_t i = 0; i < (s->type_512 ? 64 : 32); i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return (char*)s->digest.str;
}

/* ***************************************************************************
Base64 encoding
*/

/** the base64 encoding array */
static char base64_encodes[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/**
a base64 decoding array
*/
static unsigned base64_decodes[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  62, 0,  0,  0,  63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,
    0,  0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0};

/**
a union used for Base64 parsing
*/
union base64_parser_u {
  struct {
    unsigned tail : 2;
    unsigned data : 6;
  } byte1;
  struct {
    unsigned prev : 8;
    unsigned tail : 4;
    unsigned head : 4;
  } byte2;
  struct {
    unsigned prev : 16;
    unsigned data : 6;
    unsigned head : 2;
  } byte3;
  char bytes[3];
};

/**
This will encode a byte array (data) of a specified length (len) and
place the encoded data into the target byte buffer (target). The target buffer
MUST have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4)
bytes.

Returns the number of bytes actually written to the target buffer
(including the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.
*/
int minicrypt_base64_encode(char* target, const char* data, int len) {
  int written = 0;
  // // optional implementation: allow a non writing, length computation.
  // if (!target)
  //   return (len % 3) ? (((len + 3) / 3) * 4) : (len / 3);
  // use a union to avoid padding issues.
  union base64_parser_u* section;
  while (len >= 3) {
    section = (void*)data;
    target[0] = base64_encodes[section->byte1.data];
    target[1] =
        base64_encodes[(section->byte1.tail << 4) | (section->byte2.head)];
    target[2] =
        base64_encodes[(section->byte2.tail << 2) | (section->byte3.head)];
    target[3] = base64_encodes[section->byte3.data];

    target += 4;
    data += 3;
    len -= 3;
    written += 4;
  }
  section = (void*)data;
  if (len == 2) {
    target[0] = base64_encodes[section->byte1.data];
    target[1] =
        base64_encodes[(section->byte1.tail << 4) | (section->byte2.head)];
    target[2] = base64_encodes[section->byte2.tail << 2];
    target[3] = '=';
    target += 4;
    written += 4;
  } else if (len == 1) {
    target[0] = base64_encodes[section->byte1.data];
    target[1] = base64_encodes[section->byte1.tail << 4];
    target[2] = '=';
    target[3] = '=';
    target += 4;
    written += 4;
  }
  target[0] = 0;  // NULL terminator
  return written;
}

/**
This will decode a Base64 encoded string of a specified length (len) and
place the decoded data into the target byte buffer (target).

The target buffer MUST have enough room for the expected data.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

If the target buffer is NULL, the encoded string will be destructively edited
and the decoded data will be placed in the original string's buffer.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3. Hence, the target buffer should
be, at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).
*/
int minicrypt_base64_decode(char* target, char* encoded, int base64_len) {
  if (base64_len <= 0)
    return -1;
  if (!target)
    target = encoded;
  union base64_parser_u section;
  int written = 0;
  // base64_encodes
  // a struct that will be used to read the data.
  while (base64_len >= 4) {
    base64_len -= 4;  // make sure we don't loop forever.
    // copying the data allows us to write destructively to the same buffer
    section.byte1.data = base64_decodes[(unsigned char)(*encoded)];
    encoded++;
    section.byte1.tail = (base64_decodes[(unsigned char)(*encoded)] >> 4);
    section.byte2.head = base64_decodes[(unsigned char)(*encoded)];
    encoded++;
    section.byte2.tail = (base64_decodes[(unsigned char)(*encoded)] >> 2);
    section.byte3.head = base64_decodes[(unsigned char)(*encoded)];
    encoded++;
    section.byte3.data = base64_decodes[(unsigned char)(*encoded)];
    encoded++;
    // write to the target buffer
    *(target++) = section.bytes[0];
    *(target++) = section.bytes[1];
    *(target++) = section.bytes[2];
    // count written bytes
    written += section.bytes[2] ? 3 : section.bytes[1] ? 2 : 1;
  }
  // deal with the "tail" of the encoded stream
  if (base64_len) {
    // zero out data
    section.bytes[0] = 0;
    section.bytes[1] = 0;
    section.bytes[2] = 0;
    // byte 1 + 2 (2 might be padding)
    section.byte1.data = base64_decodes[(unsigned char)*(encoded++)];
    if (--base64_len) {
      section.byte1.tail = base64_decodes[(unsigned char)(*encoded)] >> 4;
      section.byte2.head = base64_decodes[(unsigned char)(*encoded)];
      encoded++;
      if (--base64_len) {
        section.byte2.tail = base64_decodes[(unsigned char)(*encoded)] >> 4;
        section.byte3.head = base64_decodes[(unsigned char)(*encoded)];
        // --base64_len;  // will always be 0 at this point (or it was 4)
      }
    }
    // write to the target buffer
    *(target++) = section.bytes[0];
    if (section.bytes[1] || section.bytes[2])
      *(target++) = section.bytes[1];
    if (section.bytes[2])
      *(target++) = section.bytes[2];
    // count written bytes
    written += section.bytes[2] ? 3 : section.bytes[1] ? 2 : 1;
  }
  *target = 0;
  return written;
}

/* ***************************************************************************
Hex Conversion
*/

/*
#define hex2i(h) \
  (((h) >= '0' && (h) <= '9') ? ((h) - '0') : (((h) | 32) - 'a' + 10))
*/

#define i2hex(hi) (((hi) < 10) ? ('0' + (hi)) : ('A' + ((hi)-10)))

/* Credit to Jonathan Leffler for the idea */
#define hex2i(c)                                                          \
  (((c) >= '0' && (c) <= '9') ? ((c)-48) : (((c) >= 'a' && (c) <= 'f') || \
                                            ((c) >= 'A' && (c) <= 'F'))   \
                                               ? (((c) | 32) - 87)        \
                                               : ({                       \
                                                   return -1;             \
                                                   0;                     \
                                                 }))

/**
Returns 1 if the string is HEX encoded (no non-valid hex values). Returns 0 if
it isn't.
*/
int minicrypt_is_hex(const char* string, size_t length) {
  // for (size_t i = 0; i < length; i++) {
  //   if (isxdigit(string[i]) == 0)
  //     return 0;
  char c;
  for (size_t i = 0; i < length; i++) {
    c = string[i];
    if ((!isspace(c)) &&
        (c < '0' || c > 'z' ||
         !((c >= 'a') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))))
      return 0;
  }
  return 1;
}
/**
This will convert the string (byte stream) to a Hex string. This is not
cryptography, just conversion for pretty print.

The target buffer MUST have enough room for the expected data. The expected
data is double the length of the string + 1 byte for the NULL terminator
byte.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).
*/
int minicrypt_str2hex(char* target, const char* string, size_t length) {
  if (!target)
    return -1;
  size_t i = length;
  target[(length << 1) + 1] = 0;
  // go in reverse, so that target could be same as string.
  while (i) {
    --i;
    target[(i << 1) + 1] = i2hex(string[i] & 0x0F);
    target[(i << 1)] = i2hex(((uint8_t*)string)[i] >> 4);
  }
  return (length << 1);
}

/**
This will convert a Hex string to a byte string. This is not cryptography,
just conversion for pretty print.

The target buffer MUST have enough room for the expected data. The expected
data is half the length of the Hex string + 1 byte for the NULL terminator
byte.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

If the target buffer is NULL, the encoded string will be destructively
edited
and the decoded data will be placed in the original string's buffer.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).
*/
int minicrypt_hex2str(char* target, char* hex, size_t length) {
  if (!target)
    target = hex;
  size_t i = 0;
  size_t written = 0;
  while (i + 1 < length) {
    if (isspace(hex[i])) {
      ++i;
      continue;
    }
    target[written] = (hex2i(hex[i]) << 4) | hex2i(hex[i + 1]);
    ++written;
    i += 2;
  }
  if (i < length && !isspace(hex[i])) {
    target[written] = hex2i(hex[i + 1]);
    ++written;
  }

  target[written] = 0;
  return written;
}

#undef hex2i
#undef i2hex

/* ***************************************************************************
XOR encryption
*/

/**
Uses an XOR key `xor_key_s` to encrypt / decrypt the data provided.

Encryption/decryption can be destructive (the target and the source can point
to the same object).

The key's `on_cycle` callback option should be utilized to er-calculate the
key every cycle. Otherwise, XOR encryption should be avoided.

A more secure encryption would be easier to implement using seperate
`xor_key_s` objects for encryption and decription.

If `target` is NULL, the source will be used as the target (destructive mode).

Returns -1 on error and 0 on success.
*/
int minicrypt_xor_crypt(xor_key_s* key,
                        void* target,
                        const void* source,
                        size_t length) {
  if (!source || !key)
    return -1;
  if (!length)
    return 0;
  if (!target)
    target = (void*)source;
  if (key->on_cycle) {
    /* loop to provide vector initialization when needed. */
    while (key->position >= key->length) {
      if (key->on_cycle(key))
        return -1;
      key->position -= key->length;
    }
  } else if (key->position >= key->length)
    key->position = 0; /* no callback? no need for vector alterations. */
  size_t i = 0;

  /* start decryption */
  while (length > i) {
    while ((key->length - key->position >= 8)  // we have 8 bytes for key.
           && ((i + 8) <= length)              // we have 8 bytes for stream.
           && (((uintptr_t)(target + i)) & 7) == 0  // target memory is aligned.
           && (((uintptr_t)(source + i)) & 7) == 0  // source memory is aligned.
           && ((uintptr_t)(key->key + key->position) & 7) == 0  // key aligned.
           ) {
      // fprintf(stderr, "XOR optimization used i= %lu, key pos = %lu.\n", i,
      //         key->position);
      *((uint64_t*)(target + i)) =
          *((uint64_t*)(source + i)) ^ *((uint64_t*)(key->key + key->position));
      key->position += 8;
      i += 8;
      if (key->position < key->length)
        continue;
      if (key->on_cycle && key->on_cycle(key))
        return -1;
      key->position = 0;
    }

    if (i < length) {
      // fprintf(stderr, "XOR single byte.\n");
      *((uint8_t*)(target + i)) =
          *((uint8_t*)(source + i)) ^ *((uint8_t*)(key->key + key->position));
      ++i;
      ++key->position;
      if (key->position == key->length) {
        if (key->on_cycle && key->on_cycle(key))
          return -1;
        key->position = 0;
      }
    }
  }
  return 0;
}

/**
Similar to the minicrypt_xor_crypt except with a fixed key size of 128bits.
*/
int minicrypt_xor128_crypt(uint64_t* key,
                           void* target,
                           const void* source,
                           size_t length,
                           int (*on_cycle)(uint64_t* key)) {
  length = length & 31;
  uint8_t pos = 0;
  for (size_t i = 0; i < (length >> 3); i++) {
    ((uint64_t*)target)[0] = ((uint64_t*)source)[0] ^ key[pos++];
    target += 8;
    source += 8;
    if (pos < 2)
      continue;
    if (on_cycle && on_cycle(key))
      return -1;
    pos = 0;
  }
  length = length & 7;
  for (size_t i = 0; i < length; i++) {
    ((uint64_t*)target)[i] = ((uint64_t*)source)[i] ^ key[pos];
  }
  return 0;
}
/**
Similar to the minicrypt_xor_crypt except with a fixed key size of 192bits.
*/
int minicrypt_xor192_crypt(uint64_t* key,
                           void* target,
                           const void* source,
                           size_t length,
                           int (*on_cycle)(uint64_t* key)) {
  length = length & 31;
  uint8_t pos = 0;
  for (size_t i = 0; i < (length >> 3); i++) {
    ((uint64_t*)target)[0] = ((uint64_t*)source)[0] ^ key[pos++];
    target += 8;
    source += 8;
    if (pos < 3)
      continue;
    if (on_cycle && on_cycle(key))
      return -1;
    pos = 0;
  }
  length = length & 7;
  for (size_t i = 0; i < length; i++) {
    ((uint64_t*)target)[i] = ((uint64_t*)source)[i] ^ key[pos];
  }
  return 0;
}
/**
Similar to the minicrypt_xor_crypt except with a fixed key size of 256bits.
*/
int minicrypt_xor256_crypt(uint64_t* key,
                           void* target,
                           const void* source,
                           size_t length,
                           int (*on_cycle)(uint64_t* key)) {
  for (size_t i = 0; i < (length >> 5); i++) {
    ((uint64_t*)target)[0] = ((uint64_t*)source)[0] ^ key[0];
    ((uint64_t*)target)[1] = ((uint64_t*)source)[1] ^ key[1];
    ((uint64_t*)target)[2] = ((uint64_t*)source)[2] ^ key[2];
    ((uint64_t*)target)[3] = ((uint64_t*)source)[3] ^ key[3];
    target += 32;
    source += 32;
    if (on_cycle && on_cycle(key))
      return -1;
  }
  length = length & 31;
  uint8_t pos = 0;
  for (size_t i = 0; i < (length >> 3); i++) {
    ((uint64_t*)target)[0] = ((uint64_t*)source)[0] ^ key[pos++];
    target += 8;
    source += 8;
  }
  length = length & 7;
  for (size_t i = 0; i < length; i++) {
    ((uint64_t*)target)[i] = ((uint64_t*)source)[i] ^ key[pos];
  }
  return 0;
}

/* ***************************************************************************
AES GCM - AES is only implemented in GCM mode, as it seems to be the superior
mode of choice at the moment (mid 2016).
*/

typedef union {
  uint64_t dwords[2];
  uint32_t words[4];
  uint8_t matrix[4][4];
  uint8_t stream[16];
} aes128_state_u;

typedef struct {
  /** The key string data */
  uint8_t key[32];
  /** initialization vector, up to 96 bits */
  struct {
    /** static. part of the handshake, not sent with TLS packet, can be
     * global... maybe a function pointer...?
     */
    uint32_t salt;
    /** sent with TLS packet. MUST be unique for each packet (global counter?)
     */
    uint64_t explicit;
  } nonce;
  /** 128 bit mode vs. 256 bit mode */
  uint16_t bits;
  /** tag_len can be 128, 120, 112, 104, or 96 */
  uint8_t tag_len;
} aes_key_s;

// static inline GHASH128(bits128_u block) {}

/* ***************************************************************************
Random ... (why is this not a system call?)
*/

/* rand generator management */
static int _rand_fd_ = -1;
static void close_rand_fd(void) {
  if (_rand_fd_ >= 0)
    close(_rand_fd_);
  _rand_fd_ = -1;
}
static void init_rand_fd(void) {
  if (_rand_fd_ < 0) {
    while ((_rand_fd_ = open("/dev/urandom", O_RDONLY)) == -1) {
      if (errno == ENXIO)
        perror("minicrypt fatal error, caanot initiate random generator"),
            exit(-1);
      sched_yield();
    }
  }
  atexit(close_rand_fd);
}
/* rand function template */
#define MAKE_RAND_FUNC(type, func_name)             \
  type func_name(void) {                            \
    if (_rand_fd_ < 0)                              \
      init_rand_fd();                               \
    type ret;                                       \
    while (read(_rand_fd_, &ret, sizeof(type)) < 0) \
      sched_yield();                                \
    return ret;                                     \
  }
/* rand functions */
MAKE_RAND_FUNC(uint32_t, minicrypt_rand32);
MAKE_RAND_FUNC(uint64_t, minicrypt_rand64);
MAKE_RAND_FUNC(bits128_u, minicrypt_rand128);
MAKE_RAND_FUNC(bits256_u, minicrypt_rand256);
/* clear template */
#undef MAKE_RAND_FUNC

void minicrypt_rand_bytes(void* target, size_t length) {
  if (_rand_fd_ < 0)
    init_rand_fd();
  while (read(_rand_fd_, target, length) < 0)
    sched_yield();
}

/* ***************************************************************************
Other helper functions
*/

/**
Allocates memory and dumps the whole file into the memory allocated.

Remember to call `free` when done.

Returns the number of bytes allocated. On error, returns 0 and sets the
container pointer to NULL.

This function has some Unix specific properties that resolve links and user
folder referencing.
*/
fdump_s* minicrypt_fdump(const char* file_path, size_t size_limit) {
  struct stat f_data;
  int file = -1;
  fdump_s* container = NULL;
  size_t file_path_len;
  if (file_path == NULL || (file_path_len = strlen(file_path)) == 0 ||
      file_path_len > PATH_MAX)
    return NULL;

  char real_public_path[PATH_MAX + 1];
  real_public_path[PATH_MAX] = 0;
  if (file_path[0] == '~' && getenv("HOME") && file_path_len <= PATH_MAX) {
    strcpy(real_public_path, getenv("HOME"));
    memcpy(real_public_path + strlen(real_public_path), file_path + 1,
           file_path_len);
    file_path = real_public_path;
  }

  if (stat(file_path, &f_data))
    goto error;
  if (size_limit == 0 || f_data.st_size < size_limit)
    size_limit = f_data.st_size;
  container = malloc(size_limit + sizeof(fdump_s));
  container->length = size_limit;
  if (!container)
    goto error;
  file = open(file_path, O_RDONLY);
  if (file < 0)
    goto error;
  if (read(file, container->data, size_limit) < size_limit)
    goto error;
  close(file);
  return container;
error:
  if (container)
    free(container), (container = NULL);
  if (file >= 0)
    close(file);
  return 0;
}

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm* minicrypt_gmtime(const time_t* timer, struct tm* tmbuf) {
  // static char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  // static char * Months = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  // "Jul",
  // "Aug", "Sep", "Oct", "Nov", "Dec"};
  static uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,  // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31   // leap year
  };
  if (*timer < 0)
    return gmtime_r(timer, tmbuf);
  ssize_t tmp;
  tmbuf->tm_gmtoff = 0;
  tmbuf->tm_zone = "UTC";
  tmbuf->tm_isdst = 0;
  tmbuf->tm_year = 70;  // tm_year == The number of years since 1900
  tmbuf->tm_mon = 0;
  // for seconds up to weekdays, we build up, as small values clean up larger
  // values.
  tmp = ((ssize_t)*timer);
  tmbuf->tm_sec = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_min = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_hour = tmp % 24;
  tmp = tmp / 24;
  // day of epoch was a thursday. Add + 3 so sunday == 0...
  tmbuf->tm_wday = (tmp + 3) % 7;
// tmp == number of days since epoch
#define DAYS_PER_400_YEARS ((400 * 365) + 97)
  while (tmp >= DAYS_PER_400_YEARS) {
    tmbuf->tm_year += 400;
    tmp -= DAYS_PER_400_YEARS;
  }
#undef DAYS_PER_400_YEARS
#define DAYS_PER_100_YEARS ((100 * 365) + 24)
  while (tmp >= DAYS_PER_100_YEARS) {
    tmbuf->tm_year += 100;
    tmp -= DAYS_PER_100_YEARS;
    if (((tmbuf->tm_year / 100) & 3) ==
        0)  // leap century divisable by 400 => add leap
      --tmp;
  }
#undef DAYS_PER_100_YEARS
#define DAYS_PER_32_YEARS ((32 * 365) + 8)
  while (tmp >= DAYS_PER_32_YEARS) {
    tmbuf->tm_year += 32;
    tmp -= DAYS_PER_32_YEARS;
  }
#undef DAYS_PER_32_YEARS
#define DAYS_PER_8_YEARS ((8 * 365) + 2)
  while (tmp >= DAYS_PER_8_YEARS) {
    tmbuf->tm_year += 8;
    tmp -= DAYS_PER_8_YEARS;
  }
#undef DAYS_PER_8_YEARS
#define DAYS_PER_4_YEARS ((4 * 365) + 1)
  while (tmp >= DAYS_PER_4_YEARS) {
    tmbuf->tm_year += 4;
    tmp -= DAYS_PER_4_YEARS;
  }
#undef DAYS_PER_4_YEARS
  while (tmp >= 365) {
    tmbuf->tm_year += 1;
    tmp -= 365;
    if ((tmbuf->tm_year & 3) == 0) {  // leap year
      if (tmp > 0) {
        --tmp;
        continue;
      } else {
        tmp += 365;
        --tmbuf->tm_year;
        break;
      }
    }
  }
  tmbuf->tm_yday = tmp;
  if ((tmbuf->tm_year & 3) == 1) {
    // regular year
    for (size_t i = 0; i < 12; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  } else {
    // leap year
    for (size_t i = 12; i < 24; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  }
  tmbuf->tm_mday = tmp;
  return tmbuf;
}
