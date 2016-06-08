/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef MINI_CRYPT
#define MINI_CRYPT "0.1.1"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/** \file
The MiniCrypt library supplies the following, basic, cryptographic functions:

- SHA-1 hashing (considered less secure, use SHA-2/SHA-3 instead).
- SHA-2 hashing
- SHA-3 hashing (on the wish list)

Non-cryptographic, but related, algorithms also supported are:

- Base64 encoding/decoding (non cryptographic, but often used in relation).

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
SHA-1 hashing container - you should ignore the contents of this struct.

The `sha1_s` type will contain all the sha1 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    #include "mini-crypt.h"
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

/**
SHA-2 function variants.

This enum states the different SHA-2 function variants. placing SHA_512 at the
beginning is meant to set this variant as the default (in case a 0 is passed).
*/
typedef enum {
  SHA_512 = 0,
  SHA_512_256,
  SHA_512_224,
  SHA_384,
  SHA_256,
  SHA_224,
} sha2_variant;

/**
SHA-2 hashing container - you should ignore the contents of this struct.

The `sha2_s` type will contain all the SHA-2 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    #include "mini-crypt.h"
    sha2_s sha2;
    MiniCrypt.sha2_init(&sha2, SHA_512);
    MiniCrypt.sha2_write(&sha2,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = MiniCrypt.sha2_result(&sha2);

*/
typedef struct {
  union {
    uint32_t i32[8];
    uint64_t i64[8];
    char str[65]; /* added 64+1 for the NULL byte.*/
  } digest;
  union {
    uint32_t i32[16];
    uint64_t i64[16];
    char str[128];
  } buffer;
  /* notice: we're counting bits, not bytes. max length: 2^128 bits */
  union {
    __uint128_t i;
    char str[16];
  } msg_length;
  unsigned buffer_pos : 7;
  unsigned initialized : 1;
  unsigned finalized : 1;
  sha2_variant type : 3;
  unsigned type_512 : 1;
} sha2_s;

/**
The `xor_key_s` type is a struct containing XOR key data. This will be used for
all the encryption/decription functions that use XOR key data, such as the
`MiniCrypt.xor_crypt` function.
*/
typedef struct xor_key_s {
  /** A pointer to a string containing the key. */
  uint8_t* key;
  /** The length of the key string */
  size_t length;
  /**
  The vector / position from which to start the next encryption/decryption.

  This value is automatically advanced when encrypting / decrypting data.
  */
  size_t position;
  /**
  An optional callback to be called whenever the XOR key finished a cycle and
  the `position` is being reset to 0.

  The function should return 0 on sucess and -1 on failure. Failue will cause
  endryption/decryption to fail.
  */
  int (*on_cycle)(struct xor_key_s* key);
  /** Opaque user data. */
  void* udata;
} xor_key_s;

/**
File dump data.

This struct (or, a pointer to this struct) is returned by the `MiniCrypt.fdump`
function on success.

To free the pointer returned, simply call `free`.
*/
typedef struct {
  size_t length;
  char data[];
} fdump_s;

/*******************************************************************************
API Gateway (the MiniCrypt global object)
*/

/**
The MiniCrypt global object (member of the struct MiniCrypt__API___) is the
API
namespace gateway fot the MiniCrypt library.

For example:

    #include "mini-crypt.h"
    sha2_s sha2;
    MiniCrypt.sha2_init(&sha2, SHA_512);
    MiniCrypt.sha2_write(&sha2,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = MiniCrypt.sha2_result(&sha2);

*/
extern struct MiniCrypt__API___ {
  /*****************************************************************************
  SHA-1 hashing
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
  SHA-2 hashing
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
  void (*sha2_init)(sha2_s* s, sha2_variant variant);
  /**
  Writes data to the SHA-2 buffer.
  */
  int (*sha2_write)(sha2_s* s, const char* data, size_t len);
  /**
  Finalizes the SHA-2 hash, returning the Hashed data.

  `sha2_result` can be called for the same object multiple times, but the
  finalization will only be performed the first time this function is called.
  */
  char* (*sha2_result)(sha2_s* s);

  /*****************************************************************************
  Base64 encoding
  */

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
  be, at least, `base64_len/4*3 + 3` long.

  Returns the number of bytes actually written to the target buffer (excluding
  the NULL terminator byte).
  */
  int (*base64_decode)(char* target, char* encoded, int base64_len);

  /*****************************************************************************
  Hex Conversion
  */

  /**
  Returns 1 if the string is HEX encoded (no non-valid hex values). Returns 0 if
  it isn't.
  */
  int (*is_hex)(const char* string, size_t length);
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
  int (*str2hex)(char* target, const char* string, size_t length);

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
  int (*hex2str)(char* target, char* hex, size_t length);

  /*****************************************************************************
  Other helper functions
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
  int (*xor_crypt)(xor_key_s* key,
                   void* target,
                   const void* source,
                   size_t length);

  /**
  Allocates memory and dumps the whole file into the memory allocated.
  Remember
  to call `free` when done.

  Returns the number of bytes allocated. On error, returns 0 and sets the
  container pointer to NULL.

  This function has some Unix specific properties that resolve links and user
  folder referencing.
  */
  fdump_s* (*fdump)(const char* file_path, size_t size_limit);
} MiniCrypt;

/* end include gate */
#endif
