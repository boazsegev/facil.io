/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_FIO_SHA2_H
#define H_FIO_SHA2_H

// clang-format off
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#   if defined(__has_include)
#     if __has_include(<endian.h>)
#      include <endian.h>
#     elif __has_include(<sys/endian.h>)
#      include <sys/endian.h>
#     endif
#   endif
#   if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
                __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __BIG_ENDIAN__
#   endif
#endif

#ifndef UNUSED_FUNC
#   define UNUSED_FUNC __attribute__((unused))
#endif
// clang-format on

#include <stdlib.h>
#include <stdint.h>

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
    fio_sha2_init(&sha2, SHA_512);
    fio_sha2_write(&sha2,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = fio_sha2_result(&sha2);

*/
typedef struct {
  /* notice: we're counting bits, not bytes. max length: 2^128 bits */
  union {
    uint8_t bytes[16];
    uint8_t matrix[4][4];
    uint32_t words_small[4];
    uint64_t words[2];
#if defined(__SIZEOF_INT128__)
    __uint128_t i;
#endif
  } length;
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
sha2_s fio_sha2_init(sha2_variant variant);
/**
Writes data to the SHA-2 buffer.
*/
void fio_sha2_write(sha2_s *s, const void *data, size_t len);
/**
Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *fio_sha2_result(sha2_s *s);

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 512 variant.
*/
static inline UNUSED_FUNC char *fio_sha2_512(sha2_s *s, const void *data,
                                             size_t len) {
  *s = fio_sha2_init(SHA_512);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 256 variant.
*/
static inline UNUSED_FUNC char *fio_sha2_256(sha2_s *s, const void *data,
                                             size_t len) {
  *s = fio_sha2_init(SHA_256);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

/**
An SHA2 helper function that performs initialiation, writing and finalizing.
Uses the SHA2 384 variant.
*/
static inline UNUSED_FUNC char *fio_sha2_384(sha2_s *s, const void *data,
                                             size_t len) {
  *s = fio_sha2_init(SHA_384);
  fio_sha2_write(s, data, len);
  return fio_sha2_result(s);
}

#if defined(DEBUG) && DEBUG == 1
void fio_sha2_test(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
