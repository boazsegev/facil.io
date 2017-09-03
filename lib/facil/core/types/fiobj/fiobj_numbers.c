/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Numbers VTable
***************************************************************************** */

static __thread char num_buffer[512];

static int64_t fio_i2i(fiobj_s *o) { return obj2num(o)->i; }
static int64_t fio_f2i(fiobj_s *o) {
  return (int64_t)floorl(((fio_float_s *)o)->f);
}
static double fio_i2f(fiobj_s *o) { return (double)obj2num(o)->i; }
static double fio_f2f(fiobj_s *o) { return obj2float(o)->f; }

static fio_cstr_s fio_i2str(fiobj_s *o) {
  return (fio_cstr_s){
      .buffer = num_buffer, .len = fio_ltoa(num_buffer, obj2num(o)->i, 10),
  };
}
static fio_cstr_s fio_f2str(fiobj_s *o) {
  if (isnan(obj2float(o)->f))
    return (fio_cstr_s){.buffer = "NaN", .len = 3};
  else if (isinf(obj2float(o)->f)) {
    if (obj2float(o)->f > 0)
      return (fio_cstr_s){.buffer = "Infinity", .len = 8};
    else
      return (fio_cstr_s){.buffer = "-Infinity", .len = 9};
  }
  return (fio_cstr_s){
      .buffer = num_buffer, .len = fio_ftoa(num_buffer, obj2float(o)->f, 10),
  };
}

static int fiobj_i_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != self->type ||
      obj2num(self)->i != obj2num(other)->i)
    return 0;
  return 1;
}
static int fiobj_f_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != self->type ||
      obj2float(self)->f != obj2float(other)->f)
    return 0;
  return 1;
}

static struct fiobj_vtable_s FIOBJ_VTABLE_INT = {
    .free = fiobj_simple_dealloc,
    .to_i = fio_i2i,
    .to_f = fio_i2f,
    .to_str = fio_i2str,
    .is_eq = fiobj_i_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

static struct fiobj_vtable_s FIOBJ_VTABLE_FLOAT = {
    .free = fiobj_simple_dealloc,
    .to_i = fio_f2i,
    .to_f = fio_f2f,
    .to_str = fio_f2str,
    .is_eq = fiobj_f_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

/* *****************************************************************************
Number API
***************************************************************************** */

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_num_s));
  if (!head)
    perror("ERROR: fiobj number couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_INT,
  };
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
  if (!head)
    perror("ERROR: fiobj float couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_FLOAT,
  };
  *obj2float(HEAD2OBJ(head)) = (fio_float_s){.f = num, .type = FIOBJ_T_FLOAT};
  return HEAD2OBJ(head);
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *obj, double num) { obj2float(obj)->f = num; }
