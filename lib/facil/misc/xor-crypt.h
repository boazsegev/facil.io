/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef bscrypt_XOR_CRYPT_H
#define bscrypt_XOR_CRYPT_H
#include "bscrypt-common.h"
/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
XOR encryption
*/

typedef struct xor_key_s xor_key_s;
/**
The `xor_key_s` type is a struct containing XOR key data. This will be used for
all the encryption/decription functions that use XOR key data, such as the
`bscrypt_xor_crypt` function.
*/
struct xor_key_s {
  /** A pointer to a string containing the key. */
  uint8_t *key;
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
  int (*on_cycle)(xor_key_s *key);
};

typedef struct xor_key_128bit_s xor_key_128bit_s;
/**
The `xor_key_128bit_s` type is a struct containing an XOR key data. This will be
used
for
all the encryption/decription functions that use XOR key data, such as the
`bscrypt.xor_crypt` function.
*/
struct xor_key_128bit_s {
  /** A pointer to a string containing the key. */
  uint64_t key[2];
  /**
  An optional callback to be called whenever the XOR key finished a cycle and
  the `position` is being reset to 0.

  The function should return 0 on sucess and -1 on failure. Failue will cause
  endryption/decryption to fail.
  */
  int (*on_cycle)(xor_key_128bit_s *key);
};

/**
Uses an XOR key `xor_key_s` to encrypt / decrypt the data provided.

Encryption/decryption can be destructive (the target and the source can point
to the same object).

The key's `on_cycle` callback option should be utilized to re-calculate the
key every cycle. Otherwise, XOR encryption should be avoided.

A more secure encryption would be easier to implement using seperate
`xor_key_s` objects for encryption and decription.

If `target` is NULL, the source will be used as the target (destructive mode).

Returns -1 on error and 0 on success.
*/
int bscrypt_xor_crypt(xor_key_s *key, void *target, const void *source,
                      size_t length);

/**
Similar to the bscrypt_xor_crypt except with a fixed key size of 128bits.
*/
int bscrypt_xor128_crypt(uint64_t *key, void *target, const void *source,
                         size_t length, int (*on_cycle)(uint64_t *key));
/**
Similar to the bscrypt_xor_crypt except with a fixed key size of 256bits.
*/
int bscrypt_xor256_crypt(uint64_t *key, void *target, const void *source,
                         size_t length, int (*on_cycle)(uint64_t *key));

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
