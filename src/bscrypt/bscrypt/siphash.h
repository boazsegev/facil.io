/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef BS_SIPHASH_H
#define BS_SIPHASH_H
#include <stdint.h>
#include <stdlib.h>

#define SIPHASH_DEFAULT_KEY                                                    \
  (uint64_t[]) { 0x0706050403020100, 0x0f0e0d0c0b0a0908 }
uint64_t siphash24(const void *data, size_t len, uint64_t iv_key[2]);

#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_siphash(void);

#endif

#endif
