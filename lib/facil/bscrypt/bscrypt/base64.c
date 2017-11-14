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
  /* walk backwards, allowing fo inplace decoding (target == data) */
  int groups = len / 3;
  const int mod = len - (groups * 3);
  const int target_size = (groups + (mod != 0)) * 4;
  char *writer = target + target_size - 1;
  const char *reader = data + len - 1;
  writer[1] = 0;
  switch (mod) {
  case 2: {
    char tmp2 = *(reader--);
    char tmp1 = *(reader--);
    *(writer--) = '=';
    *(writer--) = base64_encodes[((tmp2 & 15) << 2)];
    *(writer--) = base64_encodes[((tmp1 & 3) << 4) | ((tmp2 >> 4) & 15)];
    *(writer--) = base64_encodes[(tmp1 >> 2) & 63];
  } break;
  case 1: {
    char tmp1 = *(reader--);
    *(writer--) = '=';
    *(writer--) = '=';
    *(writer--) = base64_encodes[(tmp1 & 3) << 4];
    *(writer--) = base64_encodes[(tmp1 >> 2) & 63];
  } break;
  }
  while (groups) {
    groups--;
    const char tmp3 = *(reader--);
    const char tmp2 = *(reader--);
    const char tmp1 = *(reader--);
    *(writer--) = base64_encodes[tmp3 & 63];
    *(writer--) = base64_encodes[((tmp2 & 15) << 2) | ((tmp3 >> 6) & 3)];
    *(writer--) = base64_encodes[(((tmp1 & 3) << 4) | ((tmp2 >> 4) & 15))];
    *(writer--) = base64_encodes[(tmp1 >> 2) & 63];
  }
  return target_size;
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
  if (!target)
    target = encoded;
  if (base64_len <= 0) {
    target[0] = 0;
    return 0;
  }
  int written = 0;
  char tmp1, tmp2, tmp3, tmp4;
  while (*encoded == '\r' || *encoded == '\n' || *encoded == ' ') {
    base64_len--;
    encoded++;
  }
  while (encoded[base64_len - 1] == '\r' || encoded[base64_len - 1] == '\n' ||
         encoded[base64_len - 1] == ' ' || encoded[base64_len - 1] == 0) {
    base64_len--;
  }
  while (base64_len >= 4) {
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    tmp3 = *(encoded++);
    tmp4 = *(encoded++);
    *(target++) = (base64_decodes[(size_t)tmp1] << 2) |
                  (base64_decodes[(size_t)tmp2] >> 4);
    *(target++) = (base64_decodes[(size_t)tmp2] << 4) |
                  (base64_decodes[(size_t)tmp3] >> 2);
    *(target++) =
        (base64_decodes[(size_t)tmp3] << 6) | (base64_decodes[(size_t)tmp4]);
    // make sure we don't loop forever.
    base64_len -= 4;
    // count written bytes
    written += 3;
    // skip white space
    while (base64_len &&
           (*encoded == '\r' || *encoded == '\n' || *encoded == ' ')) {
      base64_len--;
      encoded++;
    }
  }
  // deal with the "tail" of the mis-encoded stream - this shouldn't happen
  tmp1 = 0;
  tmp2 = 0;
  tmp3 = 0;
  tmp4 = 0;
  switch (base64_len) {
  case 1:
    tmp1 = *(encoded++);
    *(target++) = base64_decodes[(size_t)tmp1];
    written += 1;
    break;
  case 2:
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    *(target++) = (base64_decodes[(size_t)tmp1] << 2) |
                  (base64_decodes[(size_t)tmp2] >> 6);
    *(target++) = (base64_decodes[(size_t)tmp2] << 4);
    written += 2;
    break;
  case 3:
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    tmp3 = *(encoded++);
    *(target++) = (base64_decodes[(size_t)tmp1] << 2) |
                  (base64_decodes[(size_t)tmp2] >> 6);
    *(target++) = (base64_decodes[(size_t)tmp2] << 4) |
                  (base64_decodes[(size_t)tmp3] >> 2);
    *(target++) = base64_decodes[(size_t)tmp3] << 6;
    written += 3;
    break;
  }
  if (encoded[-1] == '=') {
    target--;
    written--;
    if (encoded[-2] == '=') {
      target--;
      written--;
    }
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
      {"", ""},
      {"f", "Zg=="},
      {"fo", "Zm8="},
      {"foo", "Zm9v"},
      {"foob", "Zm9vYg=="},
      {"fooba", "Zm9vYmE="},
      {"foobar", "Zm9vYmFy"},
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
