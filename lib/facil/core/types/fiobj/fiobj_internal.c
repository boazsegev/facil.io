/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#include "fiobj_internal.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Internal API required across the board
***************************************************************************** */

void fiobj_simple_dealloc(fiobj_s *o) { fiobj_dealloc(o); }

void fiobj_noop_free(fiobj_s *obj) { OBJ2HEAD(obj)->ref = (uintptr_t)-1; }

int fiobj_noop_true(const fiobj_s *obj) {
  return 1;
  (void)obj;
}
int fiobj_noop_false(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
fio_cstr_s fiobj_noop_str(const fiobj_s *obj) {
  return (fio_cstr_s){.length = 0, .data = ""};
  (void)obj;
}
int64_t fiobj_noop_i(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
double fiobj_noop_f(const fiobj_s *obj) {
  return 0;
  (void)obj;
}

/** always 0. */
int fiobj_noop_is_eq(const fiobj_s *self, const fiobj_s *other) {
  return 0;
  (void)self;
  (void)other;
}

size_t fiobj_noop_count(const fiobj_s *obj) {
  return 0;
  (void)obj;
}
fiobj_s *fiobj_noop_unwrap(const fiobj_s *obj) {
  return (fiobj_s *)obj;
  (void)obj;
}
size_t fiobj_noop_each1(fiobj_s *obj, size_t start_at,
                        int (*task)(fiobj_s *obj, void *arg), void *arg) {
  return 0;
  (void)obj;
  (void)start_at;
  (void)task;
  (void)arg;
}

/* *****************************************************************************
Invalid Object VTable - unused, still considering...
***************************************************************************** */

// static void fiobj_noop_free_invalid(fiobj_s *obj) { (void)obj; }
// static int64_t fiobj_noop_i_invalid(const fiobj_s *obj) {
//   return ((int64_t)(obj) ^ 3);
// }
// struct fiobj_vtable_s FIOBJ_VTABLE_INVALID = {
//     .name = "Invalid Class - not a facil.io Object",
//     .free = fiobj_noop_free_invalid,
//     .is_true = fiobj_noop_false,
//     .to_str = fiobj_noop_str,
//     .to_i = fiobj_noop_i_invalid,
//     .to_f = fiobj_noop_f,
//     .is_eq = fiobj_noop_is_eq,
//     .count = fiobj_noop_count,
//     .unwrap = fiobj_noop_unwrap,
//     .each1 = fiobj_noop_each1,
// };
