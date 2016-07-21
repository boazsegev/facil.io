/*
(un)copyright: Boaz segev, 2016
license: MIT except for any non-public-domain algorithms, which are subject to
their own licenses.

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

#include "bscrypt/bscrypt-common.h"
#include "bscrypt/base64.h"
#include "bscrypt/hex.h"
#include "bscrypt/misc.h"
#include "bscrypt/random.h"
#include "bscrypt/sha1.h"
#include "bscrypt/sha2.h"
#include "bscrypt/xor-crypt.h"

#if defined(BSCRYPT_TEST) && BSCRYPT_TEST == 1
#define bscrypt_test()     \
  {                        \
    bscrypt_test_sha1();   \
    bscrypt_test_sha2();   \
    bscrypt_test_base64(); \
    bscrypt_test_random(); \
  }
#endif

/* end include gate */
#endif
