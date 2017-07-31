/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef bscrypt_SHA2_H
#define bscrypt_SHA2_H
#include "bscrypt-common.h"
/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
SHA-2 hashing
*/

/**
SHA-2 function variants.

This enum states the different SHA-2 function variants. placing SHA_512 at the
beginning is meant to set this variant as the default (in case a 0 is passed).
*/
typedef enum {
  SHA_512 = 1,
  SHA_512_256 = 3,
  SHA_512_224 = 5,
  SHA_384 = 7,
  SHA_256 = 2,
  SHA_224 = 4,
} sha2_variant;

/**
SHA-2 hashing container - you should ignore the contents of this struct.

The `sha2_s` type will contain all the SHA-2 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    #include "mini-crypt.h"
    sha2_s sha2;
    bscrypt.sha2_init(&sha2, SHA_512);
    bscrypt.sha2_write(&sha2,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = bscrypt.sha2_result(&sha2);

*/
typedef struct {
  /* notice: we're counting bits, not bytes. max length: 2^128 bits */
  bits128_u length;
  uint8_t buffer[128];
  union {
    uint32_t i32[16];
    uint64_t i64[8];
    uint8_t str[65]; /* added 64+1 for the NULL byte.*/
  } digest;
  sha2_variant type;
} sha2_s;

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
sha2_s bscrypt_sha2_init(sha2_variant variant);
/**
Writes data to the SHA-2 buffer.
*/
void bscrypt_sha2_write(sha2_s *s, const void *data, size_t len);
/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *bscrypt_sha2_result(sha2_s *s);

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 512 variant.
*/
static inline UNUSED_FUNC char *bscrypt_sha2_512(sha2_s *s, const void *data,
                                                 size_t len) {
  *s = bscrypt_sha2_init(SHA_512);
  bscrypt_sha2_write(s, data, len);
  return bscrypt_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 256 variant.
*/
static inline UNUSED_FUNC char *bscrypt_sha2_256(sha2_s *s, const void *data,
                                                 size_t len) {
  *s = bscrypt_sha2_init(SHA_256);
  bscrypt_sha2_write(s, data, len);
  return bscrypt_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 384 variant.
*/
static inline UNUSED_FUNC char *bscrypt_sha2_384(sha2_s *s, const void *data,
                                                 size_t len) {
  *s = bscrypt_sha2_init(SHA_384);
  bscrypt_sha2_write(s, data, len);
  return bscrypt_sha2_result(s);
}

#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_sha2(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
