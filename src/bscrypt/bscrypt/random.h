/*
(un)copyright: Boaz segev, 2016
license: MIT except for any non-public-domain algorithms, which are subject to
their own licenses.

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

uint32_t bscrypt_rand32(void);

uint64_t bscrypt_rand64(void);

bits128_u bscrypt_rand128(void);

bits256_u bscrypt_rand256(void);

void bscrypt_rand_bytes(void* target, size_t length);

#if defined(BSCRYPT_TEST) && BSCRYPT_TEST == 1
void bscrypt_test_random(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
