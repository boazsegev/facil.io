/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_FIO_SHA1_H
#define H_FIO_SHA1_H

#include <stdint.h>
#include <stdlib.h>

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

/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
SHA-1 hashing
*/

/**
SHA-1 hashing container - you should ignore the contents of this struct.

The `sha1_s` type will contain all the sha1 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

Use, for example:

    sha1_s sha1;
    fio_sha1_init(&sha1);
    fio_sha1_write(&sha1,
                  "The quick brown fox jumps over the lazy dog", 43);
    char *hashed_result = fio_sha1_result(&sha1);
*/
typedef struct {
  uint64_t length;
  uint8_t buffer[64];
  union {
    uint32_t i[5];
    unsigned char str[21];
  } digest;
} sha1_s;

/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
sha1_s fio_sha1_init(void);
/**
Writes data to the sha1 buffer.
*/
void fio_sha1_write(sha1_s *s, const void *data, size_t len);
/**
Finalizes the SHA1 hash, returning the Hashed data.

`sha1_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char *fio_sha1_result(sha1_s *s);

/**
An SHA1 helper function that performs initialiation, writing and finalizing.
*/
static inline UNUSED_FUNC char *fio_sha1(sha1_s *s, const void *data,
                                         size_t len) {
  *s = fio_sha1_init();
  fio_sha1_write(s, data, len);
  return fio_sha1_result(s);
}

#if defined(DEBUG) && DEBUG == 1
void fio_sha1_test(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
