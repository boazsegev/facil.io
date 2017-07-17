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
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_num_s));
  head->ref = 1;
  *obj2num(HEAD2OBJ(head)) = (fio_num_s){.i = num, .type = FIOBJ_T_NUMBER};
  return HEAD2OBJ(head);
}

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num) { obj2num(target)->i = num; }

/* *****************************************************************************
Float API
***************************************************************************** */

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_float_s));
  head->ref = 1;
  *obj2float(HEAD2OBJ(head)) = (fio_float_s){.f = num, .type = FIOBJ_T_FLOAT};
  return HEAD2OBJ(head);
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *obj, double num) { obj2float(obj)->f = num; }
