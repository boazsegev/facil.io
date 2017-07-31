/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
No-op for vtable
***************************************************************************** */

fio_cstr_s fiobj_noop_str(fiobj_s *obj) {
  return (fio_cstr_s){.data = NULL};
  (void)obj;
}
int64_t fiobj_noop_i(fiobj_s *obj) {
  return 0;
  (void)obj;
}
double fiobj_noop_f(fiobj_s *obj) {
  return 0;
  (void)obj;
}

size_t fiobj_noop_count(fiobj_s *obj) {
  return 0;
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

void fiobj_simple_dealloc(fiobj_s *o) { free(&OBJ2HEAD(o)); }

/* *****************************************************************************
Generic Object API
***************************************************************************** */

/**
 * Returns a Number's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * A type error results in 0.
 */
int64_t fiobj_obj2num(fiobj_s *obj) {
  if (!obj)
    return 0;
  return OBJ2HEAD(obj).vtable->to_i(obj);
}

/**
 * Returns a Float's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * A type error results in 0.
 */
double fiobj_obj2float(fiobj_s *obj) {
  if (!obj)
    return 0;
  return OBJ2HEAD(obj).vtable->to_f(obj);
}

/**
 * Returns a C String (NUL terminated) using the `fio_cstr_s` data type.
 */
fio_cstr_s fiobj_obj2cstr(fiobj_s *obj) {
  if (!obj)
    return (fio_cstr_s){.buffer = NULL, .len = 0};
  return OBJ2HEAD(obj).vtable->to_str(obj);
}

/**
 * Single layer iteration using a callback for each nested fio object.
 */
size_t fiobj_each1(fiobj_s *obj, size_t start_at,
                   int (*task)(fiobj_s *obj, void *arg), void *arg) {
  return OBJ2HEAD(obj).vtable->each1(obj, start_at, task, arg);
}

/* *****************************************************************************
Object Iteration (`fiobj_each2`)
***************************************************************************** */

static __thread fiobj_s *fiobj_cyclic_protection = NULL;
fiobj_s *fiobj_each_get_cyclic(void) { return fiobj_cyclic_protection; }

static uint8_t already_processed(fiobj_s *nested, fiobj_s *obj) {
#if FIOBJ_NESTING_PROTECTION
  size_t end = obj2ary(nested)->end;
  for (size_t i = obj2ary(nested)->start; i < end; i++) {
    if (obj2ary(nested)->arry[i] == obj)
      return 1;
  }
#endif
  OBJREF_ADD(obj);
  fiobj_ary_push(nested, obj);
  return 0;
}

struct fiobj_each2_task_s {
  fiobj_s *nested;
  int (*task)(fiobj_s *obj, void *arg);
  void *arg;
  fiobj_s *child;
};

int fiobj_each2_task_wrapper(fiobj_s *obj, void *data_) {
  struct fiobj_each2_task_s *data = data_;
  if (!obj || !OBJ2HEAD(obj).vtable->count(obj))
    return data->task(obj, data->arg);

  data->child = obj->type == FIOBJ_T_COUPLET ? obj2couplet(obj)->obj : obj;
  if (already_processed(data->nested, data->child)) {
    data->child = NULL;
    fiobj_cyclic_protection = obj;
    int ret = data->task(NULL, data->arg);
    fiobj_cyclic_protection = NULL;
    return ret;
  }
  data->task(obj, data->arg);
  return -1;
}
/**
 * Deep itteration using a callback for each fio object, including the parent.
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each2(fiobj_s *obj, int (*task)(fiobj_s *obj, void *arg),
                 void *arg) {
  /* optimize simple items */
  if (!obj || OBJ2HEAD(obj).vtable->count(obj) == 0) {
    task(obj, arg);
    return;
  }
  /* Prepare for layerd iteration */
  uintptr_t count = 0;
  fiobj_s *nested = fiobj_ary_new2(64);
  fiobj_s *state = fiobj_ary_new2(128);
  struct fiobj_each2_task_s data = {.task = task, .arg = arg, .nested = nested};
  OBJREF_ADD(obj);
  fiobj_ary_push(nested, obj);
  if (task(obj, arg) == -1)
    goto finish;
rebase:
  /* resume ? */
  data.child = NULL;
  count =
      OBJ2HEAD(obj).vtable->each1(obj, count, fiobj_each2_task_wrapper, &data);
  if (data.child) {
    fiobj_ary_push(state, obj);
    fiobj_ary_push(state, (void *)count);
    count = 0;
    obj = data.child;
    goto rebase;
  }

  /* any more nested layers left to handle? */
  if (fiobj_ary_count(state)) {
    count = (uintptr_t)fiobj_ary_pop(state);
    obj = fiobj_ary_pop(state);
    goto rebase;
  }

finish:
  while ((obj = fiobj_ary_pop(nested)))
    fiobj_dealloc(obj);
  fiobj_dealloc(nested);
  fiobj_dealloc(state);
  return;
}

/* *****************************************************************************
Object Comparison (`fiobj_iseq`)
***************************************************************************** */

static inline int fiobj_iseq_check(fiobj_s *obj1, fiobj_s *obj2) {
  if (obj1 == obj2)
    return 1;
  if (!obj1 || !obj2)
    return 0;
  return OBJ2HEAD(obj1).vtable->is_eq(obj1, obj2);
}

struct fiobj_iseq_data_s {
  fiobj_s *root;
  fiobj_s *nested;
  fiobj_s *parallel;
  uintptr_t count;
  uint8_t is_hash;
  uint8_t err;
};

int fiobj_iseq_task(fiobj_s *obj, void *data_) {
  struct fiobj_iseq_data_s *data = data_;
  if (fiobj_cyclic_protection)
    obj = fiobj_cyclic_protection;

  if (data->root == obj)
    return 0;

  if (data->count >= OBJ2HEAD(data->root).vtable->count(data->root)) {
    data->count = (uintptr_t)fiobj_ary_pop(data->nested);
    data->parallel = fiobj_ary_pop(data->nested);
    data->root = fiobj_ary_pop(data->nested);
    data->is_hash = (data->root->type == FIOBJ_T_HASH);
  }

  fiobj_s *obj2;
  if (data->is_hash) {
    obj2 = fiobj_hash_get(data->parallel, fiobj_couplet2key(obj));
    obj = fiobj_couplet2obj(obj);
  } else {
    obj2 = fiobj_ary_entry(data->parallel, data->count);
  }
  data->count++;
  if (!fiobj_iseq_check(obj, obj2)) {
    data->err = 1;
    return -1;
  }
  if (!fiobj_cyclic_protection && OBJ2HEAD(obj).vtable->count(obj)) {
    fiobj_ary_push(data->nested, data->root);
    fiobj_ary_push(data->nested, data->parallel);
    fiobj_ary_push(data->nested, (fiobj_s *)data->count);
    data->count = 0;
    data->root = obj;
    data->parallel = obj2;
    data->is_hash = (obj->type == FIOBJ_T_HASH);
  }
  return 0;
}

int fiobj_iseq(fiobj_s *obj1, fiobj_s *obj2) {
  if (OBJ2HEAD(obj1).vtable->count(obj1) == 0)
    return fiobj_iseq_check(obj1, obj2);
  if (!fiobj_iseq_check(obj1, obj2))
    return 0;
  struct fiobj_iseq_data_s data = {
      .root = obj1,
      .nested = fiobj_ary_new2(128),
      .is_hash = obj1->type == FIOBJ_T_HASH,
      // .count = OBJ2HEAD(obj1).vtable->count(obj1),
      .parallel = obj2,
  };
  // fiobj_ary_push(data.nested, obj2);
  // fiobj_ary_push(data.nested, (fiobj_s *)OBJ2HEAD(obj1).vtable->count(obj1));
  fiobj_each2(obj1, fiobj_iseq_task, &data);
  fiobj_dealloc(data.nested);
  return data.err == 0;
}
