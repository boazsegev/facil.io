/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fiobj_internal.h"

/* *****************************************************************************
Numbers Type
***************************************************************************** */

typedef struct {
  struct fiobj_vtable_s *vtable;
  uint64_t i;
} fiobj_num_s;

typedef struct {
  struct fiobj_vtable_s *vtable;
  double f;
} fiobj_float_s;

#define obj2num(o) ((fiobj_num_s *)(o))
#define obj2float(o) ((fiobj_float_s *)(o))

/* *****************************************************************************
Numbers VTable
***************************************************************************** */

static __thread char num_buffer[512];

static int64_t fio_i2i(const fiobj_s *o) { return obj2num(o)->i; }
static int64_t fio_f2i(const fiobj_s *o) {
  return (int64_t)floorl(obj2float(o)->f);
}
static double fio_i2f(const fiobj_s *o) { return (double)obj2num(o)->i; }
static double fio_f2f(const fiobj_s *o) { return obj2float(o)->f; }

static int fio_itrue(const fiobj_s *o) { return (obj2num(o)->i != 0); }
static int fio_ftrue(const fiobj_s *o) { return (obj2float(o)->f != 0); }

static fio_cstr_s fio_i2str(const fiobj_s *o) {
  return (fio_cstr_s){
      .buffer = num_buffer, .len = fio_ltoa(num_buffer, obj2num(o)->i, 10),
  };
}
static fio_cstr_s fio_f2str(const fiobj_s *o) {
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

static int fiobj_i_is_eq(const fiobj_s *self, const fiobj_s *other) {
  return self->type == other->type && obj2num(self)->i == obj2num(other)->i;
}
static int fiobj_f_is_eq(const fiobj_s *self, const fiobj_s *other) {
  return self->type == other->type && obj2float(self)->f == obj2float(other)->f;
}

static struct fiobj_vtable_s FIOBJ_VTABLE_INT = {
    .name = "Number",
    .free = fiobj_simple_dealloc,
    .to_i = fio_i2i,
    .to_f = fio_i2f,
    .to_str = fio_i2str,
    .is_true = fio_itrue,
    .is_eq = fiobj_i_is_eq,
    .count = fiobj_noop_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_noop_each1,
};

const uintptr_t FIOBJ_T_NUMBER = (uintptr_t)&FIOBJ_VTABLE_INT;

static struct fiobj_vtable_s FIOBJ_VTABLE_FLOAT = {
    .name = "Float",
    .free = fiobj_simple_dealloc,
    .to_i = fio_f2i,
    .to_f = fio_f2f,
    .is_true = fio_ftrue,
    .to_str = fio_f2str,
    .is_eq = fiobj_f_is_eq,
    .count = fiobj_noop_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_noop_each1,
};

const uintptr_t FIOBJ_T_FLOAT = (uintptr_t)&FIOBJ_VTABLE_FLOAT;

/* *****************************************************************************
Number API
***************************************************************************** */

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num) {
  fiobj_num_s *o = (fiobj_num_s *)fiobj_alloc(sizeof(*o));
  if (!o)
    perror("ERROR: fiobj number couldn't allocate memory"), exit(errno);
  *o = (fiobj_num_s){
      .vtable = &FIOBJ_VTABLE_INT, .i = num,
  };
  return (fiobj_s *)o;
}

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num) { obj2num(target)->i = num; }

/* *****************************************************************************
Float API
***************************************************************************** */

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num) {
  fiobj_float_s *o = (fiobj_float_s *)fiobj_alloc(sizeof(*o));
  if (!o)
    perror("ERROR: fiobj float couldn't allocate memory"), exit(errno);
  *o = (fiobj_float_s){
      .vtable = &FIOBJ_VTABLE_FLOAT, .f = num,
  };
  return (fiobj_s *)o;
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *obj, double num) { obj2float(obj)->f = num; }
