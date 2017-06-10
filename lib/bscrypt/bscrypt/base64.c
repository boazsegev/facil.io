/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "base64.h"

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
int bscrypt_base64_encode(char *target, const char *data, int len) {
  int written = 0;
  // // optional implementation: allow a non writing, length computation.
  // if (!target)
  //   return (len % 3) ? (((len + 3) / 3) * 4) : (len / 3);
  // use a union to avoid padding issues.
  union base64_parser_u *section;
  while (len >= 3) {
    section = (void *)data;
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
  section = (void *)data;
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
  target[0] = 0; // NULL terminator
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
int bscrypt_base64_decode(char *target, char *encoded, int base64_len) {
  if (base64_len <= 0)
    return -1;
  if (!target)
    target = encoded;
  union base64_parser_u section;
  int written = 0;
  // base64_encodes
  // a struct that will be used to read the data.
  while (base64_len >= 4) {
    base64_len -= 4; // make sure we don't loop forever.
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

/*******************************************************************************
Base64
*/
#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_base64(void) {
  struct {
    char *str;
    char *base64;
  } sets[] = {
      // {"Man is distinguished, not only by his reason, but by this singular "
      //  "passion from other animals, which is a lust of the mind, that by a "
      //  "perseverance of delight in the continued and indefatigable generation
      //  "
      //  "of knowledge, exceeds the short vehemence of any carnal pleasure.",
      //  "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB"
      //  "0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIG"
      //  "x1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpb"
      //  "iB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xl"
      //  "ZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3V"
      //  "yZS4="},
      {"any carnal pleasure.", "YW55IGNhcm5hbCBwbGVhc3VyZS4="},
      {"any carnal pleasure", "YW55IGNhcm5hbCBwbGVhc3VyZQ=="},
      {"any carnal pleasur", "YW55IGNhcm5hbCBwbGVhc3Vy"},
      {NULL, NULL} // Stop
  };
  int i = 0;
  char buffer[1024];
  fprintf(stderr, "+ bscrypt");
  while (sets[i].str) {
    bscrypt_base64_encode(buffer, sets[i].str, strlen(sets[i].str));
    if (strcmp(buffer, sets[i].base64)) {
      fprintf(stderr,
              ":\n--- bscrypt Base64 Test FAILED!\nstring: %s\nlength: %lu\n "
              "expected: %s\ngot: %s\n\n",
              sets[i].str, strlen(sets[i].str), sets[i].base64, buffer);
      break;
    }
    i++;
  }
  if (!sets[i].str)
    fprintf(stderr, " Base64 encode passed.\n");

  i = 0;
  fprintf(stderr, "+ bscrypt");
  while (sets[i].str) {
    bscrypt_base64_decode(buffer, sets[i].base64, strlen(sets[i].base64));
    if (strcmp(buffer, sets[i].str)) {
      fprintf(stderr,
              ":\n--- bscrypt Base64 Test FAILED!\nbase64: %s\nexpected: "
              "%s\ngot: %s\n\n",
              sets[i].base64, sets[i].str, buffer);
      return;
    }
    i++;
  }
  fprintf(stderr, " Base64 decode passed.\n");
  char buff_b64[] = "any carnal pleasure.";
  size_t b64_len;
  clock_t start = clock();
  for (size_t i = 0; i < 100000; i++) {
    b64_len = bscrypt_base64_encode(buffer, buff_b64, sizeof(buff_b64) - 1);
    bscrypt_base64_decode(buff_b64, buffer, b64_len);
  }
  fprintf(stderr, "bscrypt 100K Base64: %lf\n",
          (double)(clock() - start) / CLOCKS_PER_SEC);
}
#endif
