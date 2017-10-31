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

#include "fiobj.h"
#include "resp.h"

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
`fiobj_s` => RESP (formatting): implemented seperately; can be safely removed.
***************************************************************************** */

/**
 * Returns a **new** String object containing a RESP representation of `obj`.
 *
 * Returns NULL on failur.
 *
 * Obviously, RESP objects and `fiobj_s` objects aren't fully compatible,
 * meaning that the RESP_OK and RESP_ERR aren't implemented.
 *
 * Also, `FIOBJ_T_HASH` objects are rendered as a flattened Array of `[key,
 * value, key, value, ...]`, any `FIOBJ_T_FLOAT` will be converted to an Integet
 * (the decimal information discarded) and any other object is converted to a
 * String. `FIOBJ_T_IO` and `FIOBJ_T_FILE` are ignored.
 *
 * No `parser` argument is provided and extensions aren't supported for this
 * format.
 *
 * To write a RESP OK or Error don;t use this function. Instead, simply ctreate
 * a new String "OK" or "-error message".
 */
fiobj_pt resp_fioformat(fiobj_pt obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
