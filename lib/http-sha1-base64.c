#include "http-sha1-base64.h"

/*******************************************************************************
Setup
*/

#include <stdio.h>

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

/*******************************************************************************
SHA-1 encoding
*/

/**
Bit rotation, inlined.
*/
#define left_rotate(i, bits) (((i) << (bits)) | ((i) >> (32 - (bits))))
/**
Initialize/reset the SHA-1 object.
*/
void sha1_init(sha1_s* s) {
  s->digest.i[0] = 0x67452301;
  s->digest.i[1] = 0xefcdab89;
  s->digest.i[2] = 0x98badcfe;
  s->digest.i[3] = 0x10325476;
  s->digest.i[4] = 0xc3d2e1f0;
  s->digest.str[20] = 0;  // a NULL byte, if wanted
  // s->initialized = 1;
  s->finalized = 0;
  s->buffer_pos = 0;
  s->msg_length.i = 0;
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
  uint32_t t;
  for (int i = 0; i < 80; i++) {
    if (i >= 16) {
      // update the word value
      t = s->buffer.i[i - 3] ^ s->buffer.i[i - 8] ^ s->buffer.i[i - 14] ^
          s->buffer.i[i - 16];
      s->buffer.i[i] = left_rotate(t, 1);
    }

    if (i < 20) {
      t = ((b & c) | ((~b) & d)) + 0x5A827999;
    } else if (i < 40) {
      t = (b ^ c ^ d) + 0x6ED9EBA1;
    } else if (i < 60) {
      t = ((b & (c | d)) | (c & d)) + 0x8F1BBCDC;
    } else {
      t = (b ^ c ^ d) + 0xCA62C1D6;
    }
    t += left_rotate(a, 5) + e + s->buffer.i[i];
    e = d;
    d = c;
    c = left_rotate(b, 30);
    b = a;
    a = t;
  }
  s->digest.i[4] += e;
  s->digest.i[3] += d;
  s->digest.i[2] += c;
  s->digest.i[1] += b;
  s->digest.i[0] += a;
}
/**
Add a single byte to the buffer and check the buffer's status.
*/
static int sha1_add_byte(sha1_s* s, unsigned char byte) {
// add a byte to the buffer, consider network byte order .
#ifdef __BIG_ENDIAN__
  s->buffer.str[s->buffer_pos] = byte;
#else
  s->buffer.str[s->buffer_pos ^ 3] = byte;
#endif
  // update buffer position
  s->buffer_pos++;
  // review chunk (512 bits) processing
  if (s->buffer_pos == 0) {
    // s->buffer_pos wraps at 63 back to 0, so each 0 is the 512 bits
    // (64 bytes) chunk marker to be processed.
    sha1_process_buffer(s);
  }
  // returns the buffer's possition
  return s->buffer_pos;
}
/**
Write data to the buffer.
*/
int sha1_write(sha1_s* s, const char* data, size_t len) {
  if (!s || s->finalized)
    return -1;
  if (!s->initialized)
    sha1_init(s);
  // msg length is in bits, not bytes.
  s->msg_length.i += (len << 3);
  // add each byte to the sha1 hash's buffer... network byte issues apply
  while (len--)
    sha1_add_byte(s, *(data++));
  return 0;
}
/**
Finalize the SHA-1 object and return the resulting hash.
*/
char* sha1_result(sha1_s* s) {
  // finalize the data if itw asn't finalized before
  if (!s->finalized) {
    // set the finalized flag
    s->finalized = 1;
    // pad the buffer
    sha1_add_byte(s, 0x80);
    // add 0 utill we reach the buffer's last 8 bytes (used for length data)
    while (sha1_add_byte(s, 0) < 56)
      ;
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

#ifndef __BIG_ENDIAN__
    // change back to little endian, if needed? - seems it isn't required
    unsigned char t;
    for (int i = 0; i < 5; i++) {
      // reverse byte order for each uint32 "word".
      t = s->digest.str[i * 4];  // switch first and last bytes
      s->digest.str[i * 4] = s->digest.str[(i * 4) + 3];
      s->digest.str[(i * 4) + 3] = t;
      t = s->digest.str[(i * 4) + 1];  // switch median bytes
      s->digest.str[(i * 4) + 1] = s->digest.str[(i * 4) + 2];
      s->digest.str[(i * 4) + 2] = t;
    }
#endif
  }
  // fprintf(stderr, "result requested, in hex, is:");
  // for (int i = 0; i < 20; i++)
  //   fprintf(stderr, "%02x", (unsigned int)(s->digest.str[i] & 0xFF));
  // fprintf(stderr, "\r\n");
  return s->digest.str;
}

/*******************************************************************************
Base64
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
MUST
have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is
added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4)
bytes.

Returns the number of bytes actually written to the target buffer
(including
the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.
*/
int base64_encode(char* target, const char* data, int len) {
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
  } else if (len == 1) {
    target[0] = base64_encodes[section->byte1.data];
    target[1] = base64_encodes[section->byte1.tail << 4];
    target[2] = '=';
    target[3] = '=';
  }
  written += 4;
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
the raw data's length isn't devisable by 3. Hence, the target buffer should be,
at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding the
NULL terminator byte).
*/
int base64_decode(char* target, char* encoded, int base64_len) {
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
