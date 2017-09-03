/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"

/* *****************************************************************************
NULL, TRUE, FALSE VTable
***************************************************************************** */

static int fiobj_simple_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != self->type)
    return 0;
  return 1;
}

static fio_cstr_s fio_true2str(fiobj_s *obj) {
  return (fio_cstr_s){.buffer = "true", .len = 4};
  (void)obj;
}
static fio_cstr_s fio_false2str(fiobj_s *obj) {
  return (fio_cstr_s){.buffer = "false", .len = 5};
  (void)obj;
}
static fio_cstr_s fio_null2str(fiobj_s *obj) {
  return (fio_cstr_s){.buffer = "null", .len = 4};
  (void)obj;
}
static int64_t fio_true2i(fiobj_s *obj) {
  return 1;
  (void)obj;
}
static double fio_true2f(fiobj_s *obj) {
  return 1;
  (void)obj;
}

static struct fiobj_vtable_s FIOBJ_VTABLE_NULL = {
    .free = fiobj_simple_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fio_null2str,
    .is_eq = fiobj_simple_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};
static struct fiobj_vtable_s FIOBJ_VTABLE_TRUE = {
    .free = fiobj_simple_dealloc,
    .to_i = fio_true2i,
    .to_f = fio_true2f,
    .to_str = fio_true2str,
    .is_eq = fiobj_simple_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};
static struct fiobj_vtable_s FIOBJ_VTABLE_FALSE = {
    .free = fiobj_simple_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fio_false2str,
    .is_eq = fiobj_simple_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

/* *****************************************************************************
NULL, TRUE, FALSE API
***************************************************************************** */

inline static fiobj_s *fiobj_simple_alloc(fiobj_type_en t,
                                          struct fiobj_vtable_s *vt) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fiobj_s));
  if (!head)
    perror("ERROR: fiobj primitive couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = vt,
  };
  HEAD2OBJ(head)->type = t;
  return HEAD2OBJ(head);
}

/** Retruns a NULL object. */
fiobj_s *fiobj_null(void) {
  return fiobj_simple_alloc(FIOBJ_T_NULL, &FIOBJ_VTABLE_NULL);
}

/** Retruns a FALSE object. */
fiobj_s *fiobj_false(void) {
  return fiobj_simple_alloc(FIOBJ_T_FALSE, &FIOBJ_VTABLE_FALSE);
}

/** Retruns a TRUE object. */
fiobj_s *fiobj_true(void) {
  return fiobj_simple_alloc(FIOBJ_T_TRUE, &FIOBJ_VTABLE_TRUE);
}
