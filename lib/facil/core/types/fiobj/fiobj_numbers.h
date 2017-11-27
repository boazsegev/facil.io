#ifndef H_FIOBJ_NUMBERS_H
#define H_FIOBJ_NUMBERS_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
Numbers API (Integers)
***************************************************************************** */

extern const uintptr_t FIOBJ_T_NUMBER;

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num);

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num);

/* *****************************************************************************
Float API (Double)
***************************************************************************** */

extern const uintptr_t FIOBJ_T_FLOAT;

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num);

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *obj, double num);

/* *****************************************************************************
Pointer Wrapping Helper MACROs (uses integers)
***************************************************************************** */

#define fiobj_ptr_wrap(ptr) fiobj_num_new((int64_t)(ptr))
#define fiobj_ptr_unwrap(obj) ((void *)fiobj_obj2num((obj)))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
