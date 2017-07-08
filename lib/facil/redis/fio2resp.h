/*
Copyright: Boaz segev, 2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_FIO2RESP_FORMAT_H
/**
This is a neive implementation of the RESP protocol for Redis.
*/
#define H_FIO2RESP_FORMAT_H

#include "resp.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
`fiobj_s` => RESP (formatting): implemented seperately; can be safely removed.
***************************************************************************** */
typedef struct fiobj_s *fiobj_pt;
/**
 * Formats a `fiobj_s` object into a RESP string.
 *
 * Returns 0 on success and -1 on failur.
 *
 * Accepts a memory buffer `dest` to which the data will be written and a poiner
 * to the size of the buffer.
 *
 * `size` will be updated to include the number of bytes required for the
 * string. This value may be larger than the buffer's size.
 *
 * The string is Binary safe and it ISN'T always NUL terminated.
 *
 * Obviously, RESP objects and `fiobj_s` objects aren't fully compatible,
 * meaning that the RESP_OK and RESP_ERR aren't implemented (use a String
 * instead, starting error strings with a minus sign `-`).
 *
 * Also, `FIOBJ_T_HASH` objects are rendered as a flattened Array of `[key,
 * value, key, value, ...]`, any `FIOBJ_T_FLOAT` will be converted to an Integet
 * (the decimal information discarded) and any other object is converted to a
 * String. `FIOBJ_T_IO` and `FIOBJ_T_FILE` are ignored.
 *
 * No `parser` argument is provided and extensions aren't supported for this
 * format.
 */
int resp_fioformat(uint8_t *dest, size_t *size, fiobj_pt obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
