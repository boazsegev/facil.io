/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef bscrypt_HEX_H
#define bscrypt_HEX_H
#include "bscrypt-common.h"
/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
Hex Conversion
*/

/**
Returns 1 if the string is HEX encoded (no non-valid hex values). Returns 0 if
it isn't.
*/
int bscrypt_is_hex(const char *string, size_t length);
/**
This will convert the string (byte stream) to a Hex string. This is not
cryptography, just conversion for pretty print.

The target buffer MUST have enough room for the expected data. The expected
data is double the length of the string + 1 byte for the NULL terminator
byte.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).
*/
int bscrypt_str2hex(char *target, const char *string, size_t length);

/**
This will convert a Hex string to a byte string. This is not cryptography,
just conversion for pretty print.

The target buffer MUST have enough room for the expected data. The expected
data is half the length of the Hex string + 1 byte for the NULL terminator
byte.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

If the target buffer is NULL, the encoded string will be destructively
edited
and the decoded data will be placed in the original string's buffer.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).
*/
int bscrypt_hex2str(char *target, char *hex, size_t length);

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
