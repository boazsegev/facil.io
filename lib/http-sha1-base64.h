#ifndef HTTP_SHA1_BASE64_H
#define HTTP_SHA1_BASE64_H
#include <stdlib.h>

/*******************************************************************************
SHA-1 encoding
*/

/**
the `sha1` type will contain all the sha1 data, managing it's encoding. If it's
stack allocated, no freeing will be required.
*/
typedef struct sha1info sha1;
/**
Initialize or reset the `sha1` object. This must be performed before hashing
data using sha1.
*/
void sha1_init(sha1*);
/**
Writes data to the sha1 buffer.
*/
int sha1_write(sha1* s, const char* data, size_t len);
/**
Finalizes the SHA1 hash, returning the Hashed data.

`sha1_result` can be called for the same object multiple times, but the
finalization will only be performed the first time this function is called.
*/
char* sha1_result(sha1* s);

/*******************************************************************************
Base64 encoding
*/

/**
This will encode a byte array (data) of a specified length (len) and
place
the encoded data into the target byte buffer (target). The target buffer
MUST
have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is
added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4)
bytes.

Returns the number of bytes actually written to the target buffer
(including
the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.
*/
int base64_encode(char* target, const char* data, int len);

#endif
