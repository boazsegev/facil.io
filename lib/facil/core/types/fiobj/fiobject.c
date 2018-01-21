/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

/**
This facil.io core library provides wrappers around complex and (or) dynamic
types, abstracting some complexity and making dynamic type related tasks easier.
*/
#include "fiobject.h"

#include "fio_ary.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* *****************************************************************************
the `fiobj_each2` function
***************************************************************************** */
struct task_packet_s {
  int (*task)(FIOBJ obj, void *arg);
  void *arg;
  fio_ary_s *stack;
  FIOBJ next;
  uintptr_t *counter;
  uint8_t stop;
  uint8_t incomplete;
};

static int fiobj_task_wrapper(FIOBJ o, void *p_) {
  struct task_packet_s *p = p_;
  ++*p->counter;
  int ret = p->task(o, p->arg);
  if (ret == -1) {
    p->stop = 1;
    return -1;
  }
  if (FIOBJ_IS_ALLOCATED(o) && FIOBJECT2VTBL(o)->each) {
    p->incomplete = 1;
    p->next = o;
    return -1;
  }
  return 0;
}
/**
 * Single layer iteration using a callback for each nested fio object.
 *
 * Accepts any `FIOBJ ` type but only collections (Arrays and Hashes) are
 * processed. The container itself (the Array or the Hash) is **not** processed
 * (unlike `fiobj_each2`).
 *
 * The callback task function must accept an object and an opaque user pointer.
 *
 * Hash objects pass along a `FIOBJ_T_COUPLET` object, containing
 * references for both the key and the object. Keys shouldn't be altered once
 * placed as a key (or the Hash will break). Collections (Arrays / Hashes) can't
 * be used as keeys.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the "stop" position, i.e., the number of items processed + the
 * starting point.
 */
size_t fiobj_each2(FIOBJ o, int (*task)(FIOBJ obj, void *arg), void *arg) {
  if (!o || !FIOBJ_IS_ALLOCATED(o) || (FIOBJECT2VTBL(o)->each == NULL)) {
    task(o, arg);
    return 1;
  }
  /* run task for root object */
  if (task(o, arg) == -1)
    return 1;
  uintptr_t pos = 0;
  fio_ary_s stack = FIO_ARY_INIT;
  size_t count = 1;
  struct task_packet_s packet = {
      .task = task, .arg = arg, .stack = &stack, .counter = &count,
  };
  fio_ary_new(&stack, 0);
  fio_ary_push(&stack, (void *)pos);
  fio_ary_push(&stack, (void *)o);
  do {
    o = (FIOBJ)fio_ary_pop(&stack);
    pos = (uintptr_t)fio_ary_pop(&stack);
    if (!pos)
      packet.next = 0;
    packet.incomplete = 0;
    pos = FIOBJECT2VTBL(o)->each(o, pos, fiobj_task_wrapper, &packet);
    if (packet.stop)
      goto finish;
    if (packet.incomplete) {
      fio_ary_push(&stack, (void *)pos);
      fio_ary_push(&stack, (void *)o);
    }

    if (packet.next) {
      fio_ary_push(&stack, (void *)0);
      fio_ary_push(&stack, (void *)packet.next);
    }

  } while (fio_ary_count(&stack));
finish:
  fio_ary_free(&stack);
  return count;
}

/* *****************************************************************************
Free complex objects (objects with nesting)
***************************************************************************** */

static void fiobj_dealloc_task(FIOBJ o, void *stack_) {
  // if (!o)
  //   fprintf(stderr, "* WARN: freeing a NULL no-object\n");
  // else
  //   fprintf(stderr, "* freeing object %s\n", fiobj_obj2cstr(o).data);
  if (!o || !FIOBJ_IS_ALLOCATED(o))
    return;
  if (OBJREF_REM(o))
    return;
  if (!FIOBJECT2VTBL(o)->each || !FIOBJECT2VTBL(o)->count(o)) {
    FIOBJECT2VTBL(o)->dealloc(o, NULL, NULL);
    return;
  }
  fio_ary_s *s = stack_;
  fio_ary_push(s, (void *)o);
}
/**
 * Decreases an object's reference count, releasing memory and
 * resources.
 *
 * This function affects nested objects, meaning that when an Array or
 * a Hash object is passed along, it's children (nested objects) are
 * also freed.
 */
void fiobj_free_complex_object(FIOBJ o) {
  fio_ary_s stack = FIO_ARY_INIT;
  do {
    FIOBJECT2VTBL(o)->dealloc(o, fiobj_dealloc_task, &stack);
  } while ((o = (FIOBJ)fio_ary_pop(&stack)));
  fio_ary_free(&stack);
}

/* *****************************************************************************
Is Equal?
***************************************************************************** */
#include "fiobj_hash.h"

static inline int fiobj_iseq_simple(const FIOBJ o, const FIOBJ o2) {
  if (o == o2)
    return 1;
  if (!o || !o2)
    return 0; /* they should have compared equal before. */
  if (!FIOBJ_IS_ALLOCATED(o) || !FIOBJ_IS_ALLOCATED(o2))
    return 0; /* they should have compared equal before. */
  if (FIOBJECT2HEAD(o)->type != FIOBJECT2HEAD(o2)->type)
    return 0; /* non-type equality is a barriar to equality. */
  if (!FIOBJECT2VTBL(o)->is_eq(o, o2))
    return 0;
  return 1;
}

int fiobj_iseq____internal_complex__task(FIOBJ o, void *ary_) {
  fio_ary_s *ary = ary_;
  fio_ary_push(ary, (void *)o);
  if (fiobj_hash_key_in_loop())
    fio_ary_push(ary, (void *)fiobj_hash_key_in_loop());
  return 0;
}

/** used internally for complext nested tests (Array / Hash types) */
int fiobj_iseq____internal_complex__(FIOBJ o, FIOBJ o2) {
  // if (FIOBJECT2VTBL(o)->each && FIOBJECT2VTBL(o)->count(o))
  //   return int fiobj_iseq____internal_complex__(const FIOBJ o, const FIOBJ
  //   o2);
  fio_ary_s left = FIO_ARY_INIT, right = FIO_ARY_INIT, queue = FIO_ARY_INIT;
  do {
    fiobj_each1(o, 0, fiobj_iseq____internal_complex__task, &left);
    fiobj_each1(o2, 0, fiobj_iseq____internal_complex__task, &right);
    while (fio_ary_count(&left)) {
      o = (FIOBJ)fio_ary_pop(&left);
      o2 = (FIOBJ)fio_ary_pop(&right);
      if (!fiobj_iseq_simple(o, o2))
        goto unequal;
      if (FIOBJ_IS_ALLOCATED(o) && FIOBJECT2VTBL(o)->each &&
          FIOBJECT2VTBL(o)->count(o)) {
        fio_ary_push(&queue, (void *)o);
        fio_ary_push(&queue, (void *)o2);
      }
    }
    o2 = (FIOBJ)fio_ary_pop(&queue);
    o = (FIOBJ)fio_ary_pop(&queue);
    if (!fiobj_iseq_simple(o, o2))
      goto unequal;
  } while (o);
  fio_ary_free(&left);
  fio_ary_free(&right);
  fio_ary_free(&queue);
  return 1;
unequal:
  fio_ary_free(&left);
  fio_ary_free(&right);
  fio_ary_free(&queue);
  return 0;
}

/* *****************************************************************************
Defaults / NOOPs
***************************************************************************** */

void fiobject___noop_dealloc(FIOBJ o, void (*task)(FIOBJ, void *), void *arg) {
  (void)o;
  (void)task;
  (void)arg;
}
void fiobject___simple_dealloc(FIOBJ o, void (*task)(FIOBJ, void *),
                               void *arg) {
  free(FIOBJ2PTR(o));
  (void)task;
  (void)arg;
}

uintptr_t fiobject___noop_count(FIOBJ o) {
  (void)o;
  return 0;
}
size_t fiobject___noop_is_eq(FIOBJ o1, FIOBJ o2) {
  (void)o1;
  (void)o2;
  return 0;
}

fio_cstr_s fiobject___noop_to_str(FIOBJ o) {
  (void)o;
  return (fio_cstr_s){.len = 0, .data = NULL};
}
intptr_t fiobject___noop_to_i(FIOBJ o) {
  (void)o;
  return 0;
}
double fiobject___noop_to_f(FIOBJ o) {
  (void)o;
  return 0;
}

#if DEBUG

#include "fiobj_ary.h"
#include "fiobj_numbers.h"

static int fiobject_test_task(FIOBJ o, void *arg) {
  ++((uintptr_t *)arg)[0];
  if (!o)
    fprintf(stderr, "* WARN: counting a NULL no-object\n");
  // else
  //   fprintf(stderr, "* counting object %s\n", fiobj_obj2cstr(o).data);
  return 0;
  (void)o;
}

void fiobj_test_core(void) {
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  fprintf(stderr, "=== Testing Primitives\n");
  FIOBJ o = fiobj_null();
  TEST_ASSERT(o == (FIOBJ)FIOBJ_T_NULL, "fiobj_null isn't NULL!\n");
  TEST_ASSERT(FIOBJ_TYPE(0) == FIOBJ_T_NULL, "NULL isn't NULL!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(0, FIOBJ_T_NULL), "NULL isn't NULL! (2)\n");
  TEST_ASSERT(!FIOBJ_IS_ALLOCATED(fiobj_null()),
              "fiobj_null claims to be allocated!\n");
  TEST_ASSERT(!FIOBJ_IS_ALLOCATED(fiobj_true()),
              "fiobj_true claims to be allocated!\n");
  TEST_ASSERT(!FIOBJ_IS_ALLOCATED(fiobj_false()),
              "fiobj_false claims to be allocated!\n");
  TEST_ASSERT(FIOBJ_TYPE(fiobj_true()) == FIOBJ_T_TRUE,
              "fiobj_true isn't FIOBJ_T_TRUE!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(fiobj_true(), FIOBJ_T_TRUE),
              "fiobj_true isn't FIOBJ_T_TRUE! (2)\n");
  TEST_ASSERT(FIOBJ_TYPE(fiobj_false()) == FIOBJ_T_FALSE,
              "fiobj_false isn't FIOBJ_T_TRUE!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(fiobj_false(), FIOBJ_T_FALSE),
              "fiobj_false isn't FIOBJ_T_TRUE! (2)\n");
  fiobj_free(o); /* testing for crash*/
  fprintf(stderr, "* passed.\n");
  fprintf(stderr, "=== Testing fioj_each2\n");
  o = fiobj_ary_new2(4);
  FIOBJ tmp = fiobj_ary_new();
  fiobj_ary_push(o, tmp);
  fiobj_ary_push(o, fiobj_true());
  fiobj_ary_push(o, fiobj_null());
  fiobj_ary_push(o, fiobj_num_new(10));
  fiobj_ary_push(tmp, fiobj_num_new(13));
  fiobj_ary_push(tmp, fiobj_hash_new());
  FIOBJ key = fiobj_str_new("my key", 6);
  fiobj_hash_set(fiobj_ary_entry(tmp, -1), key, fiobj_true());
  fiobj_free(key);
  /* we have root array + 4 children (w/ array) + 2 children (w/ hash) + 1 */
  uintptr_t count = 0;
  size_t each_ret = 0;
  TEST_ASSERT(fiobj_each2(o, fiobject_test_task, (void *)&count) == 8,
              "fiobj_each1 didn't count everything... (%d != %d)", (int)count,
              (int)each_ret);
  TEST_ASSERT(count == 8, "Something went wrong with the counter task... (%d)",
              (int)count)
  fprintf(stderr, "* passed.\n");
  fprintf(stderr, "=== Testing fioj_iseq with nested items\n");
  FIOBJ o2 = fiobj_ary_new2(4);
  tmp = fiobj_ary_new();
  fiobj_ary_push(o2, tmp);
  fiobj_ary_push(o2, fiobj_true());
  fiobj_ary_push(o2, fiobj_null());
  fiobj_ary_push(o2, fiobj_num_new(10));
  fiobj_ary_push(tmp, fiobj_num_new(13));
  fiobj_ary_push(tmp, fiobj_hash_new());
  key = fiobj_str_new("my key", 6);
  fiobj_hash_set(fiobj_ary_entry(tmp, -1), key, fiobj_true());
  fiobj_free(key);
  TEST_ASSERT(!fiobj_iseq(o, FIOBJ_INVALID),
              "Array and FIOBJ_INVALID can't be equal!");
  TEST_ASSERT(!fiobj_iseq(o, fiobj_null()),
              "Array and fiobj_null can't be equal!");
  TEST_ASSERT(fiobj_iseq(o, o2), "Arrays aren't euqal!");
  fiobj_free(o);
  fiobj_free(o2);
  TEST_ASSERT(fiobj_iseq(fiobj_null(), fiobj_null()),
              "fiobj_null() not equal to self!");
  TEST_ASSERT(fiobj_iseq(fiobj_false(), fiobj_false()),
              "fiobj_false() not equal to self!");
  TEST_ASSERT(fiobj_iseq(fiobj_true(), fiobj_true()),
              "fiobj_true() not equal to self!");
  TEST_ASSERT(!fiobj_iseq(fiobj_null(), fiobj_false()),
              "fiobj_null eqal to fiobj_false!");
  TEST_ASSERT(!fiobj_iseq(fiobj_null(), fiobj_true()),
              "fiobj_null eqal to fiobj_true!");
  fprintf(stderr, "* passed.\n");
}

#endif
