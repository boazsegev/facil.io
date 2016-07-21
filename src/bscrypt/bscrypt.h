/*
(un)copyright: Boaz segev, 2016
license: MIT except for any non-public-domain algorithms, which are subject to
their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef BSCRYPT
/**
The bscrypt library supplies the following, basic, cryptographic functions:

- SHA-1 hashing (considered less secure, use SHA-2/SHA-3 instead).
- SHA-2 hashing
- SHA-3 hashing (on the wish list)

Non-cryptographic, but related, algorithms also supported are:

- Base64 encoding/decoding (non cryptographic, but often used in relation).
- HEX encoding/decoding (non cryptographic, but often used in relation).

The following is a list of Hashing and Encryption methods that should be avoided
and (unless required for some of my projects, such as for websockets), will NOT
be supported:

- SHA-1 (Used for Websockets, but is better avoided).
- RC4 (not supplied)
- MD5 (not supplied)

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
