/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

#include "fiobject.h"

#include "fio_ary.h"
#include <assert.h>
/* *****************************************************************************
Array Type
***************************************************************************** */

typedef struct {
  fiobj_object_header_s head;
  fio_ary_s ary;
} fiobj_ary_s;

#define obj2ary(o) ((fiobj_ary_s *)(o))

/* *****************************************************************************
VTable
***************************************************************************** */

static void fiobj_ary_dealloc(FIOBJ o, void (*task)(FIOBJ, void *), void *arg) {
  FIO_ARY_FOR(&obj2ary(o)->ary, i) { task((FIOBJ)i.obj, arg); }
  fio_ary_free(&obj2ary(o)->ary);
  free(FIOBJ2PTR(o));
}

static size_t fiobj_ary_each1(FIOBJ o, size_t start_at,
                              int (*task)(FIOBJ obj, void *arg), void *arg) {
  return fio_ary_each(&obj2ary(o)->ary, start_at, (int (*)(void *, void *))task,
                      arg);
}

static size_t fiobj_ary_is_eq(const FIOBJ self, const FIOBJ other) {
  fio_ary_s *a = &obj2ary(self)->ary;
  fio_ary_s *b = &obj2ary(other)->ary;
  if (fio_ary_count(a) != fio_ary_count(b))
    return 0;
  return 1;
}

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(const FIOBJ ary) {
  assert(FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return fio_ary_count(&obj2ary(ary)->ary);
}

static size_t fiobj_ary_is_true(const FIOBJ ary) {
  return fiobj_ary_count(ary) > 0;
}

fio_cstr_s fiobject___noop_to_str(FIOBJ o);
intptr_t fiobject___noop_to_i(FIOBJ o);
double fiobject___noop_to_f(FIOBJ o);

const fiobj_object_vtable_s FIOBJECT_VTABLE_ARRAY = {
    .class_name = "Array",
    .dealloc = fiobj_ary_dealloc,
    .is_eq = fiobj_ary_is_eq,
    .is_true = fiobj_ary_is_true,
    .count = fiobj_ary_count,
    .each = fiobj_ary_each1,
    .to_i = fiobject___noop_to_i,
    .to_f = fiobject___noop_to_f,
    .to_str = fiobject___noop_to_str,
};

/* *****************************************************************************
Allocation
***************************************************************************** */

static FIOBJ fiobj_ary_alloc(size_t capa, size_t start_at) {
  fiobj_ary_s *ary = malloc(sizeof(*ary));
  if (!ary) {
    perror("ERROR: fiobj array couldn't allocate memory");
    exit(errno);
  }
  *ary = (fiobj_ary_s){
      .head =
          {
              .ref = 1, .type = FIOBJ_T_ARRAY,
          },
  };
  fio_ary_new(&ary->ary, capa);
  ary->ary.start = ary->ary.end = start_at;
  return (FIOBJ)ary;
}

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
FIOBJ fiobj_ary_new(void) { return fiobj_ary_alloc(32, 8); }
/** Creates a mutable empty Array object with the requested capacity. */
FIOBJ fiobj_ary_new2(size_t capa) { return fiobj_ary_alloc(capa, 0); }

/* *****************************************************************************
Array direct entry access API
***************************************************************************** */

/** Returns the current, temporary, array capacity (it's dynamic). */
size_t fiobj_ary_capa(FIOBJ ary) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return fio_ary_capa(&obj2ary(ary)->ary);
}

/**
 * Returns a TEMPORARY pointer to the begining of the array.
 *
 * This pointer can be used for sorting and other direct access operations as
 * long as no other actions (insertion/deletion) are performed on the array.
 */
FIOBJ *fiobj_ary2ptr(FIOBJ ary) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return (FIOBJ *)(obj2ary(ary)->ary.arry + obj2ary(ary)->ary.start);
}

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
FIOBJ fiobj_ary_index(FIOBJ ary, int64_t pos) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return (FIOBJ)fio_ary_index(&obj2ary(ary)->ary, pos);
}

/**
 * Sets an object at the requested position.
 */
void fiobj_ary_set(FIOBJ ary, FIOBJ obj, int64_t pos) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  FIOBJ old = (FIOBJ)fio_ary_set(&obj2ary(ary)->ary, (void *)obj, pos);
  fiobj_free(old);
}

/* *****************************************************************************
Array push / shift API
***************************************************************************** */

/**
 * Pushes an object to the end of the Array.
 */
void fiobj_ary_push(FIOBJ ary, FIOBJ obj) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  fio_ary_push(&obj2ary(ary)->ary, (void *)obj);
}

/** Pops an object from the end of the Array. */
FIOBJ fiobj_ary_pop(FIOBJ ary) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return (FIOBJ)fio_ary_pop(&obj2ary(ary)->ary);
}

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 */
void fiobj_ary_unshift(FIOBJ ary, FIOBJ obj) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  fio_ary_unshift(&obj2ary(ary)->ary, (void *)obj);
}

/** Shifts an object from the beginning of the Array. */
FIOBJ fiobj_ary_shift(FIOBJ ary) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  return (FIOBJ)fio_ary_shift(&obj2ary(ary)->ary);
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
void fiobj_ary_compact(FIOBJ ary) {
  assert(ary && FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY));
  fio_ary_compact(&obj2ary(ary)->ary);
}

/* *****************************************************************************
Simple Tests
***************************************************************************** */

#if DEBUG
void fiobj_test_array(void) {
  fprintf(stderr, "=== Testing Array\n");
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  FIOBJ a = fiobj_ary_new2(4);
  TEST_ASSERT(FIOBJ_TYPE_IS(a, FIOBJ_T_ARRAY), "Array type isn't an array!\n");
  TEST_ASSERT(fiobj_ary_capa(a) == 4, "Array capacity ignored!\n");
  fiobj_ary_push(a, fiobj_null());
  TEST_ASSERT(fiobj_ary2ptr(a)[0] == fiobj_null(),
              "Array direct access failed!\n");
  fiobj_ary_push(a, fiobj_true());
  fiobj_ary_push(a, fiobj_false());
  TEST_ASSERT(fiobj_ary_count(a) == 3, "Array count isn't 3\n");
  fiobj_ary_set(a, fiobj_true(), 63);
  TEST_ASSERT(fiobj_ary_count(a) == 64, "Array count isn't 64\n");
  TEST_ASSERT(fiobj_ary_index(a, 0) == fiobj_null(),
              "Array index retrival error for fiobj_null\n");
  TEST_ASSERT(fiobj_ary_index(a, 1) == fiobj_true(),
              "Array index retrival error for fiobj_true\n");
  TEST_ASSERT(fiobj_ary_index(a, 2) == fiobj_false(),
              "Array index retrival error for fiobj_false\n");
  TEST_ASSERT(fiobj_ary_index(a, 3) == 0,
              "Array index retrival error for NULL\n");
  TEST_ASSERT(fiobj_ary_index(a, 63) == fiobj_true(),
              "Array index retrival error for index 63\n");
  TEST_ASSERT(fiobj_ary_index(a, -1) == fiobj_true(),
              "Array index retrival error for index -1\n");
  fiobj_ary_compact(a);
  TEST_ASSERT(fiobj_ary_index(a, -1) == fiobj_true(),
              "Array index retrival error for index -1\n");
  TEST_ASSERT(fiobj_ary_count(a) == 4, "Array compact error\n");
  fiobj_ary_unshift(a, fiobj_false());
  TEST_ASSERT(fiobj_ary_count(a) == 5, "Array unshift error\n");
  TEST_ASSERT(fiobj_ary_shift(a) == fiobj_false(), "Array shift value error\n");
  fiobj_free(a);
  fprintf(stderr, "* passed.\n");
}
#endif
