/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "xor-crypt.h"
/*****************************************************************************
Useful Macros
*/

/** 32Bit left rotation, inlined. */
#define left_rotate32(i, bits)                                                 \
  (((uint32_t)(i) << (bits)) | ((uint32_t)(i) >> (32 - (bits))))
/** 32Bit right rotation, inlined. */
#define right_rotate32(i, bits)                                                \
  (((uint32_t)(i) >> (bits)) | ((uint32_t)(i) << (32 - (bits))))
/** 64Bit left rotation, inlined. */
#define left_rotate64(i, bits)                                                 \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))
/** 64Bit right rotation, inlined. */
#define right_rotate64(i, bits)                                                \
  (((uint64_t)(i) >> (bits)) | ((uint64_t)(i) << (64 - (bits))))
/** unknown size element - left rotation, inlined. */
#define left_rotate(i, bits) (((i) << (bits)) | ((i) >> (sizeof((i)) - (bits))))
/** unknown size element - right rotation, inlined. */
#define right_rotate(i, bits)                                                  \
  (((i) >> (bits)) | ((i) << (sizeof((i)) - (bits))))
/** inplace byte swap 16 bit integer */
#define bswap16(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFU) << 8) | (((i)&0xFF00U) >> 8);                           \
  } while (0);
/** inplace byte swap 32 bit integer */
#define bswap32(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFUL) << 24) | (((i)&0xFF00UL) << 8) |                       \
          (((i)&0xFF0000UL) >> 8) | (((i)&0xFF000000UL) >> 24);                \
  } while (0);
/** inplace byte swap 64 bit integer */
#define bswap64(i)                                                             \
  do {                                                                         \
    (i) = (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                    \
          (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |             \
          (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |     \
          (((i)&0xFF000000000000ULL) >> 40) |                                  \
          (((i)&0xFF00000000000000ULL) >> 56);                                 \
  } while (0);

#ifdef HAVE_X86Intrin
#undef bswap64
#define bswap64(i)                                                             \
  { __asm__("bswapq %0" : "+r"(i) :); }

// shadow sched_yield as _mm_pause for spinwait
#define sched_yield() _mm_pause()
#endif

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
int bscrypt_xor_crypt(xor_key_s *key, void *target, const void *source,
                      size_t length) {
  if (!source || !key)
    return -1;
  if (!length)
    return 0;
  if (!target)
    target = (void *)source;
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
    while ((key->length - key->position >= 8) // we have 8 bytes for key.
           && ((i + 8) <= length)             // we have 8 bytes for stream.
           && (((uintptr_t)((uintptr_t)target + i)) & 7) ==
                  0 // target memory is aligned.
           && (((uintptr_t)((uintptr_t)source + i)) & 7) ==
                  0 // source memory is aligned.
           && ((uintptr_t)(key->key + key->position) & 7) == 0 // key aligned.
           ) {
      // fprintf(stderr, "XOR optimization used i= %lu, key pos = %lu.\n", i,
      //         key->position);
      *((uint64_t *)((uintptr_t)target + i)) =
          *((uint64_t *)((uintptr_t)source + i)) ^
          *((uint64_t *)(key->key + key->position));
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
      *((uint8_t *)((uintptr_t)target + i)) =
          *((uint8_t *)((uintptr_t)source + i)) ^
          *((uint8_t *)(key->key + key->position));
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
Similar to the bscrypt_xor_crypt except with a fixed key size of 128bits.
*/
int bscrypt_xor128_crypt(uint64_t *key, void *target, const void *source,
                         size_t length, int (*on_cycle)(uint64_t *key)) {
  length = length & 31;
  uint8_t pos = 0;
  for (size_t i = 0; i < (length >> 3); i++) {
    ((uint64_t *)target)[0] = ((uint64_t *)source)[0] ^ key[pos++];
    target = (void *)((uintptr_t)target + 8);
    source = (void *)((uintptr_t)source + 8);
    if (pos < 2)
      continue;
    if (on_cycle && on_cycle(key))
      return -1;
    pos = 0;
  }
  length = length & 7;
  for (size_t i = 0; i < length; i++) {
    ((uint64_t *)target)[i] = ((uint64_t *)source)[i] ^ key[pos];
  }
  return 0;
}
/**
Similar to the bscrypt_xor_crypt except with a fixed key size of 256bits.
*/
int bscrypt_xor256_crypt(uint64_t *key, void *target, const void *source,
                         size_t length, int (*on_cycle)(uint64_t *key)) {
  for (size_t i = 0; i < (length >> 5); i++) {
    ((uint64_t *)target)[0] = ((uint64_t *)source)[0] ^ key[0];
    ((uint64_t *)target)[1] = ((uint64_t *)source)[1] ^ key[1];
    ((uint64_t *)target)[2] = ((uint64_t *)source)[2] ^ key[2];
    ((uint64_t *)target)[3] = ((uint64_t *)source)[3] ^ key[3];
    target = (void *)((uintptr_t)target + 32);
    source = (void *)((uintptr_t)source + 32);
    if (on_cycle && on_cycle(key))
      return -1;
  }
  length = length & 31;
  uint8_t pos = 0;
  for (size_t i = 0; i < (length >> 3); i++) {
    ((uint64_t *)target)[0] = ((uint64_t *)source)[0] ^ key[pos++];
    target = (void *)((uintptr_t)target + 8);
    source = (void *)((uintptr_t)source + 8);
  }
  length = length & 7;
  for (size_t i = 0; i < length; i++) {
    ((uint64_t *)target)[i] = ((uint64_t *)source)[i] ^ key[pos];
  }
  return 0;
}
