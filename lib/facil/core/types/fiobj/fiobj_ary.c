/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fiobj_internal.h"

#include "fio_ary.h"
/* *****************************************************************************
Array Type
***************************************************************************** */

typedef struct {
  struct fiobj_vtable_s *vtable;
  fio_ary_s ary;
} fiobj_ary_s;

#define obj2ary(o) ((fiobj_ary_s *)(o))

/* *****************************************************************************
VTable
***************************************************************************** */

const uintptr_t FIOBJ_T_ARRAY;

static void fiobj_ary_dealloc(fiobj_s *a) {
  fio_ary_free(&obj2ary(a)->ary);
  fiobj_dealloc(a);
}

static size_t fiobj_ary_each1(fiobj_s *o, size_t start_at,
                              int (*task)(fiobj_s *obj, void *arg), void *arg) {
  return fio_ary_each(&obj2ary(o)->ary, start_at, (int (*)(void *, void *))task,
                      arg);
}

static int fiobj_ary_is_eq(const fiobj_s *self, const fiobj_s *other) {
  if (self == other)
    return 1;
  if (!other || other->type != FIOBJ_T_ARRAY ||
      (obj2ary(self)->ary.end - obj2ary(self)->ary.start) !=
          (obj2ary(other)->ary.end - obj2ary(other)->ary.start))
    return 0;
  return 1;
}

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(const fiobj_s *ary) {
  if (!ary)
    return 0;
  assert(ary->type == FIOBJ_T_ARRAY);
  return fio_ary_count(&obj2ary(ary)->ary);
}

static struct fiobj_vtable_s FIOBJ_VTABLE_ARRAY = {
    .name = "Array",
    .free = fiobj_ary_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fiobj_noop_str,
    .is_eq = fiobj_ary_is_eq,
    .count = fiobj_ary_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_ary_each1,
};

const uintptr_t FIOBJ_T_ARRAY = (uintptr_t)(&FIOBJ_VTABLE_ARRAY);

/* *****************************************************************************
Allocation
***************************************************************************** */

static fiobj_s *fiobj_ary_alloc(size_t capa, size_t start_at) {
  fiobj_s *ary = fiobj_alloc(sizeof(fiobj_ary_s));
  if (!ary)
    perror("ERROR: fiobj array couldn't allocate memory"), exit(errno);
  *(obj2ary(ary)) = (fiobj_ary_s){
      .vtable = &FIOBJ_VTABLE_ARRAY,
  };
  fio_ary_new(&obj2ary(ary)->ary, capa);
  obj2ary(ary)->ary.start = obj2ary(ary)->ary.end = start_at;
  return ary;
}

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void) { return fiobj_ary_alloc(32, 8); }
/** Creates a mutable empty Array object with the requested capacity. */
fiobj_s *fiobj_ary_new2(size_t capa) { return fiobj_ary_alloc(capa, 0); }

/* *****************************************************************************
Array direct entry access API
***************************************************************************** */

/** Returns the current, temporary, array capacity (it's dynamic). */
size_t fiobj_ary_capa(fiobj_s *ary) {
  if (!ary)
    return 0;
  assert(ary->type == FIOBJ_T_ARRAY);
  return fio_ary_capa(&obj2ary(ary)->ary);
}

/**
 * Returns a TEMPORARY pointer to the begining of the array.
 *
 * This pointer can be used for sorting and other direct access operations as
 * long as no other actions (insertion/deletion) are performed on the array.
 */
fiobj_s **fiobj_ary2prt(fiobj_s *ary) {
  if (!ary)
    return NULL;
  assert(ary->type == FIOBJ_T_ARRAY);
  return (fiobj_s **)(obj2ary(ary)->ary.arry + obj2ary(ary)->ary.start);
}

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
fiobj_s *fiobj_ary_index(fiobj_s *ary, int64_t pos) {
  if (!ary)
    return NULL;
  assert(ary->type == FIOBJ_T_ARRAY);
  return fio_ary_index(&obj2ary(ary)->ary, pos);
}

/**
 * Sets an object at the requested position.
 */
void fiobj_ary_set(fiobj_s *ary, fiobj_s *obj, int64_t pos) {
  assert(ary && ary->type == FIOBJ_T_ARRAY);
  fiobj_s *old = fio_ary_set(&obj2ary(ary)->ary, obj, pos);
  fiobj_free(old);
}

/* *****************************************************************************
Array push / shift API
***************************************************************************** */

/**
 * Pushes an object to the end of the Array.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj) {
  assert(ary && ary->type == FIOBJ_T_ARRAY);
  fio_ary_push(&obj2ary(ary)->ary, obj);
}

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary) { return fio_ary_pop(&obj2ary(ary)->ary); }

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 */
void fiobj_ary_unshift(fiobj_s *ary, fiobj_s *obj) {
  assert(ary && ary->type == FIOBJ_T_ARRAY);
  fio_ary_unshift(&obj2ary(ary)->ary, obj);
}

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary) {
  if (!ary)
    return NULL;
  assert(ary->type == FIOBJ_T_ARRAY);
  return fio_ary_shift(&obj2ary(ary)->ary);
}

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
void fiobj_ary_compact(fiobj_s *ary) {
  assert(ary && ary->type == FIOBJ_T_ARRAY);
  fio_ary_compact(&obj2ary(ary)->ary);
}
