#ifndef FIOBJ_ARRAY_H
/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

/**
A dynamic Array type for the fiobj_s dynamic type system.
*/
#define FIOBJ_ARRAY_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
Array creation API
***************************************************************************** */

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
FIOBJ fiobj_ary_new(void);

/** Creates a mutable empty Array object with the requested capacity. */
FIOBJ fiobj_ary_new2(size_t capa);

/* *****************************************************************************
Array direct entry access API
***************************************************************************** */

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(FIOBJ ary);

/** Returns the current, temporary, array capacity (it's dynamic). */
size_t fiobj_ary_capa(FIOBJ ary);

/**
 * Returns a TEMPORARY pointer to the begining of the array.
 *
 * This pointer can be used for sorting and other direct access operations as
 * long as no other actions (insertion/deletion) are performed on the array.
 */
FIOBJ *fiobj_ary2ptr(FIOBJ ary);

/**
 * Returns a temporary object owned by the Array.
 *
 * Wrap this function call within `fiobj_dup` to get a persistent handle. i.e.:
 *
 *     fiobj_dup(fiobj_ary_index(array, 0));
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
FIOBJ fiobj_ary_index(FIOBJ ary, int64_t pos);
/** alias for `fiobj_ary_index` */
#define fiobj_ary_entry(a, p) fiobj_ary_index((a), (p))

/**
 * Sets an object at the requested position.
 */
void fiobj_ary_set(FIOBJ ary, FIOBJ obj, int64_t pos);

/* *****************************************************************************
Array push / shift API
***************************************************************************** */

/**
 * Pushes an object to the end of the Array.
 */
void fiobj_ary_push(FIOBJ ary, FIOBJ obj);

/** Pops an object from the end of the Array. */
FIOBJ fiobj_ary_pop(FIOBJ ary);

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 */
void fiobj_ary_unshift(FIOBJ ary, FIOBJ obj);

/** Shifts an object from the beginning of the Array. */
FIOBJ fiobj_ary_shift(FIOBJ ary);

/* *****************************************************************************
Array compacting (untested)
***************************************************************************** */

/**
 * Removes any NULL *pointers* from an Array, keeping all Objects (including
 * explicit NULL objects) in the array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
void fiobj_ary_compact(FIOBJ ary);

#if DEBUG
void fiobj_test_array(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
