/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef BSCRYPT
/**
The bscrypt library supplies some **basic** cryptographic functions.

Read the README file for more details.

All functions will be available using the prefix `bscrypt_`, i.e.:

      char buffer[13] = {0};
      bscrypt_base64_encode(buffer, "My String", 9);


*/
#define BSCRYPT "0.0.1"

#include "base64.h"
#include "bscrypt-common.h"
#include "hex.h"
#include "misc.h"
#include "random.h"
#include "sha1.h"
#include "sha2.h"
#include "siphash.h"
#include "xor-crypt.h"

#if defined(DEBUG) && DEBUG == 1
#define bscrypt_test()                                                         \
  {                                                                            \
    bscrypt_test_sha1();                                                       \
    bscrypt_test_sha2();                                                       \
    bscrypt_test_base64();                                                     \
    bscrypt_test_random();                                                     \
    bscrypt_test_siphash();                                                    \
  }
#else
#define bscrypt_test()                                                         \
  fprintf(stderr,                                                              \
          "Debug mode not enabled, define DEBUG as 1 in the compiler.\n");
#endif

/* end include gate */
#endif
