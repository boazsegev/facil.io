/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
Herein are defined some primitive types for the facil.io dynamic object system.
*/
#include "fiobj_primitives.h"
#include "fiobj_internal.h"

/* *****************************************************************************
NULL
***************************************************************************** */

static int fiobj_primitive_is_eq(const fiobj_s *self, const fiobj_s *other) {
  return self == other;
}

static struct fiobj_vtable_s NULL_VTABLE = {
    .name = "NULL",
    /* deallocate an object */
    .free = fiobj_noop_free,
    /* object should evaluate as true/false? */
    .is_true = fiobj_noop_false,
    /* object value as String */
    .to_str = fiobj_noop_str,
    /* object value as Integer */
    .to_i = fiobj_noop_i,
    /* object value as Float */
    .to_f = fiobj_noop_f,
    .is_eq = fiobj_primitive_is_eq,
    /* return the number of nested object */
    .count = fiobj_noop_count,
    /* return a wrapped object (if object wrapping exists, i.e. Hash couplet) */
    .unwrap = fiobj_noop_unwrap,
    /* perform a task for the object's children (-1 stops iteration)
     * returns the number of items processed + `start_at`.
     */
    .each1 = fiobj_noop_each1,
};

/** Identifies the NULL type. */
const uintptr_t FIOBJ_T_NULL = (uintptr_t)(&NULL_VTABLE);

/** Returns a NULL object. */
fiobj_s *fiobj_null(void) {
  static struct {
    fiobj_head_s head;
    struct fiobj_vtable_s *vtable;
  } null_obj = {.head = {.ref = 1}, .vtable = &NULL_VTABLE};
  return (fiobj_s *)(&null_obj.vtable);
}

/* *****************************************************************************
True
***************************************************************************** */

static struct fiobj_vtable_s TRUE_VTABLE = {
    .name = "True",
    /* deallocate an object */
    .free = fiobj_noop_free,
    /* object should evaluate as true/false? */
    .is_true = fiobj_noop_true,
    /* object value as String */
    .to_str = fiobj_noop_str,
    /* object value as Integer */
    .to_i = fiobj_noop_i,
    /* object value as Float */
    .to_f = fiobj_noop_f,
    .is_eq = fiobj_primitive_is_eq,
    /* return the number of nested object */
    .count = fiobj_noop_count,
    /* return a wrapped object (if object wrapping exists, i.e. Hash couplet) */
    .unwrap = fiobj_noop_unwrap,
    /* perform a task for the object's children (-1 stops iteration)
     * returns the number of items processed + `start_at`.
     */
    .each1 = fiobj_noop_each1,
};

/** Identifies the TRUE type. */
const uintptr_t FIOBJ_T_TRUE = (uintptr_t)(&TRUE_VTABLE);

/** Returns a TRUE object. */
fiobj_s *fiobj_true(void) {
  static struct {
    fiobj_head_s head;
    struct fiobj_vtable_s *vtable;
  } obj = {.head = {.ref = 1}, .vtable = &TRUE_VTABLE};
  return (fiobj_s *)(&obj.vtable);
}

/* *****************************************************************************
False
***************************************************************************** */

static struct fiobj_vtable_s FALSE_VTABLE = {
    .name = "False",
    /* deallocate an object */
    .free = fiobj_noop_free,
    /* object should evaluate as true/false? */
    .is_true = fiobj_noop_false,
    /* object value as String */
    .to_str = fiobj_noop_str,
    /* object value as Integer */
    .to_i = fiobj_noop_i,
    /* object value as Float */
    .to_f = fiobj_noop_f,
    .is_eq = fiobj_primitive_is_eq,
    /* return the number of nested object */
    .count = fiobj_noop_count,
    /* return a wrapped object (if object wrapping exists, i.e. Hash couplet) */
    .unwrap = fiobj_noop_unwrap,
    /* perform a task for the object's children (-1 stops iteration)
     * returns the number of items processed + `start_at`.
     */
    .each1 = fiobj_noop_each1,
};

/** Identifies the FALSE type. */
const uintptr_t FIOBJ_T_FALSE = (uintptr_t)(&FALSE_VTABLE);

/** Returns a FALSE object. */
fiobj_s *fiobj_false(void) {
  static struct {
    fiobj_head_s head;
    struct fiobj_vtable_s *vtable;
  } obj = {.head = {.ref = 1}, .vtable = &FALSE_VTABLE};
  return (fiobj_s *)(&obj.vtable);
}
