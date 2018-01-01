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

void fiobj_simple_dealloc(FIOBJ o) { fiobj_dealloc(o); }

void fiobj_noop_free(FIOBJ obj) { OBJ2HEAD(obj)->ref = (uintptr_t)-1; }

int fiobj_noop_true(const FIOBJ obj) {
  return 1;
  (void)obj;
}
int fiobj_noop_false(const FIOBJ obj) {
  return 0;
  (void)obj;
}
fio_cstr_s fiobj_noop_str(const FIOBJ obj) {
  return (fio_cstr_s){.length = 0, .data = ""};
  (void)obj;
}
int64_t fiobj_noop_i(const FIOBJ obj) {
  return 0;
  (void)obj;
}
double fiobj_noop_f(const FIOBJ obj) {
  return 0;
  (void)obj;
}

/** always 0. */
int fiobj_noop_is_eq(const FIOBJ self, const FIOBJ other) {
  return 0;
  (void)self;
  (void)other;
}

size_t fiobj_noop_count(const FIOBJ obj) {
  return 0;
  (void)obj;
}
FIOBJ fiobj_noop_unwrap(const FIOBJ obj) {
  return (FIOBJ)obj;
  (void)obj;
}
size_t fiobj_noop_each1(FIOBJ obj, size_t start_at,
                        int (*task)(FIOBJ obj, void *arg), void *arg) {
  return 0;
  (void)obj;
  (void)start_at;
  (void)task;
  (void)arg;
}

/* *****************************************************************************
Invalid Object VTable - unused, still considering...
***************************************************************************** */

// static void fiobj_noop_free_invalid(FIOBJ obj) { (void)obj; }
// static int64_t fiobj_noop_i_invalid(const FIOBJ obj) {
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
