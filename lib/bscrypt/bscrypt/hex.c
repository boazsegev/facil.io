/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hex.h"
#include <ctype.h>

/* ***************************************************************************
Hex Conversion
*/

/*
#define hex2i(h) \
  (((h) >= '0' && (h) <= '9') ? ((h) - '0') : (((h) | 32) - 'a' + 10))
*/

#define i2hex(hi) (((hi) < 10) ? ('0' + (hi)) : ('A' + ((hi)-10)))

/* Credit to Jonathan Leffler for the idea */
#define hex2i(c)                                                               \
  (((c) >= '0' && (c) <= '9')                                                  \
       ? ((c)-48)                                                              \
       : (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))            \
             ? (((c) | 32) - 87)                                               \
             : ({                                                              \
                 return -1;                                                    \
                 0;                                                            \
               }))

/**
Returns 1 if the string is HEX encoded (no non-valid hex values). Returns 0 if
it isn't.
*/
int bscrypt_is_hex(const char *string, size_t length) {
  // for (size_t i = 0; i < length; i++) {
  //   if (isxdigit(string[i]) == 0)
  //     return 0;
  char c;
  for (size_t i = 0; i < length; i++) {
    c = string[i];
    if ((!isspace(c)) &&
        (c < '0' || c > 'z' ||
         !((c >= 'a') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))))
      return 0;
  }
  return 1;
}
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
int bscrypt_str2hex(char *target, const char *string, size_t length) {
  if (!target)
    return -1;
  size_t i = length;
  target[(length << 1) + 1] = 0;
  // go in reverse, so that target could be same as string.
  while (i) {
    --i;
    target[(i << 1) + 1] = i2hex(string[i] & 0x0F);
    target[(i << 1)] = i2hex(((uint8_t *)string)[i] >> 4);
  }
  return (length << 1);
}

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
int bscrypt_hex2str(char *target, char *hex, size_t length) {
  if (!target)
    target = hex;
  size_t i = 0;
  size_t written = 0;
  while (i + 1 < length) {
    if (isspace(hex[i])) {
      ++i;
      continue;
    }
    target[written] = (hex2i(hex[i]) << 4) | hex2i(hex[i + 1]);
    ++written;
    i += 2;
  }
  if (i < length && !isspace(hex[i])) {
    target[written] = hex2i(hex[i + 1]);
    ++written;
  }

  target[written] = 0;
  return written;
}

#undef hex2i
#undef i2hex
