/*
Copyright: Boaz Segev, 2016-2018
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_FIO_BASE64_H
#define H_FIO_BASE64_H
/* *****************************************************************************
C++ extern
***************************************************************************** */
#if defined(__cplusplus)
extern "C" {
#endif

/* *****************************************************************************
Base64 (URL) encoding
***************************************************************************** */

/**
This will encode a byte array (data) of a specified length (len) and
place the encoded data into the target byte buffer (target). The target buffer
MUST have enough room for the expected data.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3.

Always assume the target buffer should have room enough for (len*4/3 + 4)
bytes.

Returns the number of bytes actually written to the target buffer
(including the Base64 required padding and excluding a NULL terminator).

A NULL terminator char is NOT written to the target buffer.
*/
int fio_base64_encode(char *target, const char *data, int len);

/**
Same as fio_base64_encode, but using Base64URL encoding.
*/
int fio_base64url_encode(char *target, const char *data, int len);

/**
This will decode a Base64 encoded string of a specified length (len) and
place the decoded data into the target byte buffer (target).

The target buffer MUST have enough room for the expected data.

A NULL byte will be appended to the target buffer. The function will return
the number of bytes written to the target buffer.

If the target buffer is NULL, the encoded string will be destructively edited
and the decoded data will be placed in the original string's buffer.

Base64 encoding always requires 4 bytes for each 3 bytes. Padding is added if
the raw data's length isn't devisable by 3. Hence, the target buffer should
be, at least, `base64_len/4*3 + 3` long.

Returns the number of bytes actually written to the target buffer (excluding
the NULL terminator byte).

Note:
====

The decoder is variation agnostic (will decode Base64, Base64 URL and Base64 XML
variations) and will attempt it's best to ignore invalid data, (in order to
support the MIME Base64 variation in RFC 2045).

This comes at the cost of error
checking, so the encoding isn't validated and invalid input might produce
surprising results.
*/
int fio_base64_decode(char *target, char *encoded, int base64_len);

#if defined(DEBUG) && DEBUG == 1
void fio_base64_test(void);
#endif

/* *****************************************************************************
C++ extern finish
***************************************************************************** */
#if defined(__cplusplus)
}
#endif

#endif
