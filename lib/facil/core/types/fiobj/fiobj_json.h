#ifndef H_FIOBJ_JSON_H
#define H_FIOBJ_JSON_H

/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fiobj_ary.h"
#include "fiobj_hash.h"
#include "fiobj_numbers.h"
#include "fiobj_primitives.h"
#include "fiobj_str.h"
#include "fiobj_sym.h"

#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
JSON API
***************************************************************************** */

/** Limit JSON nesting, we can handle more, but this is mostly for security.  */
#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH 24
#endif

/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
size_t fiobj_json2obj(fiobj_s **pobj, const void *data, size_t len);
/* Formats an object into a JSON string. Remember to `fiobj_free`. */
fiobj_s *fiobj_obj2json(fiobj_s *, uint8_t pretty);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
