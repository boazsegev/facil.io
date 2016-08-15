#ifndef BS_SIPHASH_H
#define BS_SIPHASH_H
#include <stdlib.h>
#include <stdint.h>

#define SIPHASH_DEFAULT_KEY                                                    \
  (uint64_t[]) { 0x0706050403020100, 0x0f0e0d0c0b0a0908 }
uint64_t siphash24(const void *data, size_t len, uint64_t iv_key[2]);

#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_siphash(void);

#endif

#endif
