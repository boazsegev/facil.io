/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Number API
***************************************************************************** */

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num) {
  return fiobj_alloc(FIOBJ_T_NUMBER, 0, &num);
}

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num) {
  ((fio_num_s *)target)[0].i = num;
}

/* *****************************************************************************
Float API
***************************************************************************** */

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num) {
  return fiobj_alloc(FIOBJ_T_FLOAT, 0, &num);
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *target, double num) {
  ((fio_float_s *)target)[0].f = num;
}
