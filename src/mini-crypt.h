/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_SHA1_BASE64_H
#define HTTP_SHA1_BASE64_H
#include <stdlib.h>
#include <stdint.h>

/** \file
This file contains these basic cryptographic functions:

- Base64 encoding/decoding
- SHA-1 hashing (considered less secure, use SHA-2/SHA-3 instead).
- SHA-2 (coming soon)
- SHA-3 (on the wish list)

The following is a list of Hashing and Encryption methods that should be avoided
and (unless required for some of my projects, such as for websockets), will NOT
be supported:

- SHA-1 (Used for Websockets, but is better avoided).
- RC4
- MD5

All functions will be available under the MiniCrypt global object, i.e.:

      char buffer[13] = {0};
      MiniCrypt.base64_encode(buffer, "My String", 9);


*/

/*******************************************************************************
Helper types / structs
*/

/**
SHA-1 hashing container.

The `sha1_s` type will contain all the sha1 data required to perform the hasing,
managing it's encoding. If it's stack allocated, no freeing will be required.

Use, for example:

    #include "http-sha1-base64.h"
    sha1_s sha1;
    MiniCrypt.sha1_init(&sha1);
    MiniCrypt.sha1_write(&sha1,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = MiniCrypt.sha1_result(&sha1);
*/
typedef struct {
  union {
    uint32_t i[80];
    unsigned char str[64];
  } buffer;
  union {
    uint64_t i;
    unsigned char str[8];
  } msg_length;
  union {
    uint32_t i[5];
    char str[21];
  } digest;
  unsigned buffer_pos : 6;
  unsigned initialized : 1;
  unsigned finalized : 1;
} sha1_s;

/*******************************************************************************
API Gateway (the MiniCrypt global object)
*/

extern struct MiniCrypt__API___ {
  /*****************************************************************************
  SHA-1 encoding
  */

  /**
  Initialize or reset the `sha1` object. This must be performed before hashing
  data using sha1.
  */
  void (*sha1_init)(sha1_s*);
  /**
  Writes data to the sha1 buffer.
  */
  int (*sha1_write)(sha1_s* s, const char* data, size_t len);
  /**
  Finalizes the SHA1 hash, returning the Hashed data.

  `sha1_result` can be called for the same object multiple times, but the
  finalization will only be performed the first time this function is called.
  */
  char* (*sha1_result)(sha1_s* s);

  /*****************************************************************************
  Base64 encoding
  */

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
  int (*base64_encode)(char* target, const char* data, int len);

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
  int (*base64_decode)(char* target, char* encoded, int base64_len);

} MiniCrypt;

/* end include gate */
#endif
