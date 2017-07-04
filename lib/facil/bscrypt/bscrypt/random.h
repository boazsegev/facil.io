/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef bscrypt_RANDOM_H
#define bscrypt_RANDOM_H
#include "bscrypt-common.h"
/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
Random stuff... (why is this not a system call?)
*/

/** returns 32 random bits. */
uint32_t bscrypt_rand32(void);

/** returns 64 random bits. */
uint64_t bscrypt_rand64(void);

/** returns 128 random bits. */
bits128_u bscrypt_rand128(void);

/** returns 256 random bits. */
bits256_u bscrypt_rand256(void);

/** returns a variable length string of random bytes. */
void bscrypt_rand_bytes(void *target, size_t length);

#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_random(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
