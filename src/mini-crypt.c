#include "mini-crypt.h"

/*******************************************************************************
Setup
*/

#include <stdio.h>
#include <string.h>

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

/*****************************************************************************
Local functions used in the API.
*/

/* SHA-1 */
static void sha1_init(sha1_s* s);
static int sha1_write(sha1_s* s, const char* data, size_t len);
static char* sha1_result(sha1_s* s);

/* SHA-2 */
static void sha2_init(sha2_s* s, sha2_variant variant);
static int sha2_write(sha2_s* s, const char* data, size_t len);
static char* sha2_result(sha2_s* s);

/* Base64 */
static int base64_encode(char* target, const char* data, int len);
static int base64_decode(char* target, char* encoded, int base64_len);

/*****************************************************************************
The API gateway
*/
struct MiniCrypt__API___ MiniCrypt = {
    /* SHA-1 */
    .sha1_init = sha1_init,
    .sha1_write = sha1_write,
    .sha1_result = sha1_result,

    /* SHA-2 */
    .sha2_init = sha2_init,
    .sha2_write = sha2_write,
    .sha2_result = sha2_result,

    /* Base64 */
    .base64_encode = base64_encode,
    .base64_decode = base64_decode,

};

/*****************************************************************************
Useful Macros
*/

/** 32Bit left rotation, inlined. */
#define left_rotate32(i, bits) (((i) << (bits)) | ((i) >> (32 - (bits))))
/** 32Bit right rotation, inlined. */
#define right_rotate32(i, bits) (((i) >> (bits)) | ((i) << (32 - (bits))))
/** 64Bit left rotation, inlined. */
#define left_rotate64(i, bits) (((i) << (bits)) | ((i) >> (64 - (bits))))
/** 64Bit right rotation, inlined. */
#define right_rotate64(i, bits) (((i) >> (bits)) | ((i) << (64 - (bits))))
/** unknown size element - left rotation, inlined. */
#define left_rotate(i, bits) (((i) << (bits)) | ((i) >> (sizeof((i)) - (bits))))
/** unknown size element - right rotation, inlined. */
#define right_rotate(i, bits) \
  (((i) >> (bits)) | ((i) << (sizeof((i)) - (bits))))

/*******************************************************************************
SHA-1 hashing
*/

/**
Initialize/reset the SHA-1 object.
*/
static void sha1_init(sha1_s* s) {
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
Write data to the buffer.
*/
static int sha1_write(sha1_s* s, const char* data, size_t len) {
  if (!s || s->finalized)
    return -1;
  if (!s->initialized)
    sha1_init(s);
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
Finalize the SHA-1 object and return the resulting hash.
*/
static char* sha1_result(sha1_s* s) {
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
    unsigned char t;
#define switch_bytes(i)                                    \
  t = s->digest.str[(i * 4)];                              \
  s->digest.str[(i * 4)] = s->digest.str[(i * 4) + 3];     \
  s->digest.str[(i * 4) + 3] = t;                          \
  t = s->digest.str[(i * 4) + 1];                          \
  s->digest.str[(i * 4) + 1] = s->digest.str[(i * 4) + 2]; \
  s->digest.str[(i * 4) + 2] = t;
    switch_bytes(0);
    switch_bytes(1);
    switch_bytes(2);
    switch_bytes(3);
    switch_bytes(4);
#undef switch_bytes
#endif
  }
  // fprintf(stderr, "result requested, in hex, is:");
  // for (size_t i = 0; i < 20; i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return s->digest.str;
}

/*******************************************************************************
SHA-2
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
static void sha2_init(sha2_s* s, sha2_variant variant) {
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

/** Write data to be hashed by the SHA-2 object. */
static int sha2_write(sha2_s* s, const char* data, size_t len) {
  if (!s || s->finalized)
    return -1;
  if (!s->initialized)
    sha2_init(s, SHA_512);
  // msg length is in up to 128 bits long...
  s->msg_length.i += (__uint128_t)len << 3;
  // add each byte to the sha1 hash's buffer... network byte issues apply
  while (len--) {
    // add a byte to the buffer, consider network byte order .
    sha2_set_byte(s, s->buffer_pos, *(data++));
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
Finalize the SHA-1 object and return the resulting hash.
*/
static char* sha2_result(sha2_s* s) {
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
    unsigned char t;
    if (s->type_512) {
      for (size_t i = 0; i < 64; i += 8) {
        // reverse byte order for each uint64 "word".
        for (size_t j = 0; j < 4; j++) {
          t = s->digest.str[i + j];  // switch bytes
          s->digest.str[i + j] = s->digest.str[i + (7 - j)];
          s->digest.str[i + (7 - j)] = t;
        }
      }
    } else {
      for (size_t i = 0; i < 32; i += 4) {
        // reverse byte order for each uint32 "word".
        t = s->digest.str[i];  // switch first and last bytes
        s->digest.str[i] = s->digest.str[i + 3];
        s->digest.str[i + 3] = t;
        t = s->digest.str[i + 1];  // switch median bytes
        s->digest.str[i + 1] = s->digest.str[i + 2];
        s->digest.str[i + 2] = t;
      }
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
  return s->digest.str;
}

/*******************************************************************************
SHA-3 TODO
*/

/*******************************************************************************
Base64 encoding/decoding
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
union Base64Parser {
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
place
the encoded data into the target byte buffer (target). The target buffer
MUST have enough room for the expected data, including a terminating NULL
byte.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 5)
bytes.

Returns the number of bytes actually written to the target buffer
(including the Base64 required padding and excluding the NULL terminator).

A NULL terminator char is written to the target buffer.
*/
static int base64_encode(char* target, const char* data, int len) {
  int written = 0;
  // // optional implementation: allow a non writing, length computation.
  // if (!target)
  //   return (len % 3) ? (((len + 3) / 3) * 4) : (len / 3);
  // use a union to avoid padding issues.
  union Base64Parser* section;
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
be,
at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding
the
NULL terminator byte).
*/
static int base64_decode(char* target, char* encoded, int base64_len) {
  if (base64_len <= 0)
    return -1;
  if (!target)
    target = encoded;
  union Base64Parser section;
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
