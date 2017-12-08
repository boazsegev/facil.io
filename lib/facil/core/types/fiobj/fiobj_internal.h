#ifndef FIOBJECT_INTERNAL_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
This header includes all the internal rescources / data and types required to
create object types.
*/
#define FIOBJECT_INTERNAL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fiobject.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Atomic add / subtract
***************************************************************************** */

/* C11 Atomics are defined? */
#if defined(__ATOMIC_RELAXED)
#define SPN_LOCK_BUILTIN(...) __atomic_exchange_n(__VA_ARGS__, __ATOMIC_ACQ_REL)
/** An atomic addition operation */
#define spn_add(...) __atomic_add_fetch(__VA_ARGS__, __ATOMIC_ACQ_REL)
/** An atomic subtraction operation */
#define spn_sub(...) __atomic_sub_fetch(__VA_ARGS__, __ATOMIC_ACQ_REL)

/* Select the correct compiler builtin method. */
#elif defined(__has_builtin)

#if __has_builtin(__sync_fetch_and_or)
#define SPN_LOCK_BUILTIN(...) __sync_fetch_and_or(__VA_ARGS__)
/** An atomic addition operation */
#define spn_add(...) __sync_add_and_fetch(__VA_ARGS__)
/** An atomic subtraction operation */
#define spn_sub(...) __sync_sub_and_fetch(__VA_ARGS__)

#else
#error Required builtin "__sync_swap" or "__sync_fetch_and_or" missing from compiler.
#endif /* defined(__has_builtin) */

#elif __GNUC__ > 3
#define SPN_LOCK_BUILTIN(...) __sync_fetch_and_or(__VA_ARGS__)
/** An atomic addition operation */
#define spn_add(...) __sync_add_and_fetch(__VA_ARGS__)
/** An atomic subtraction operation */
#define spn_sub(...) __sync_sub_and_fetch(__VA_ARGS__)

#else
#error Required builtin "__sync_swap" or "__sync_fetch_and_or" not found.
#endif

/* *****************************************************************************
Simple List - Used for fiobj_s * objects, but can be used for anything really.
***************************************************************************** */

typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const fiobj_s *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/** Adds an object to the list's head. */
static inline __attribute__((unused)) void fio_ls_push(fio_ls_s *pos,
                                                       const fiobj_s *obj) {
  /* prepare item */
  fio_ls_s *item = (fio_ls_s *)malloc(sizeof(*item));
  if (!item)
    perror("ERROR: fiobj list couldn't allocate memory"), exit(errno);
  *item = (fio_ls_s){.prev = pos, .next = pos->next, .obj = obj};
  /* inject item */
  pos->next->prev = item;
  pos->next = item;
}

/** Adds an object to the list's tail. */
static inline __attribute__((unused)) void fio_ls_unshift(fio_ls_s *pos,
                                                          const fiobj_s *obj) {
  pos = pos->prev;
  fio_ls_push(pos, obj);
}

/** Removes an object from the list's head. */
static inline __attribute__((unused)) fiobj_s *fio_ls_pop(fio_ls_s *list) {
  if (list->next == list)
    return NULL;
  fio_ls_s *item = list->next;
  const fiobj_s *ret = item->obj;
  list->next = item->next;
  list->next->prev = list;
  free(item);
  return (fiobj_s *)ret;
}

/** Removes an object from the list's tail. */
static inline __attribute__((unused)) fiobj_s *fio_ls_shift(fio_ls_s *list) {
  if (list->prev == list)
    return NULL;
  fio_ls_s *item = list->prev;
  const fiobj_s *ret = item->obj;
  list->prev = item->prev;
  list->prev->next = list;
  free(item);
  return (fiobj_s *)ret;
}

/** Removes an object from the containing node. */
static inline __attribute__((unused)) fiobj_s *fio_ls_remove(fio_ls_s *node) {
  const fiobj_s *ret = node->obj;
  node->next->prev = node->prev->next;
  node->prev->next = node->next->prev;
  free(node);
  return (fiobj_s *)ret;
}

/* *****************************************************************************
Memory Page Size
***************************************************************************** */

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <unistd.h>

size_t __attribute__((weak)) fiobj_memory_page_size(void) {
  static size_t page_size = 0;
  if (page_size)
    return page_size;
  page_size = sysconf(_SC_PAGESIZE);
  if (!page_size)
    page_size = 4096;
  return page_size;
}
#pragma weak fiobj_memory_page_size

#else
#define fiobj_memory_page_size() 4096

#endif

/* *****************************************************************************
Type VTable (virtual function table)
***************************************************************************** */

/**
 * Each type must define a complete virtual function table and point to the
 * table from it's topmost element in it's `struct`.
 */
struct fiobj_vtable_s {
  /** class name as a C string */
  const char *name;
  /** deallocate an object - should deallocate parent only
   *
   * Note that nested objects, such as contained by Arrays and Hash maps, are
   * handled using `each1` and handled accoring to their reference count.
   */
  void (*const free)(fiobj_s *);
  /** object should evaluate as true/false? */
  int (*const is_true)(const fiobj_s *);
  /** object value as String */
  fio_cstr_s (*const to_str)(const fiobj_s *);
  /** object value as Integer */
  int64_t (*const to_i)(const fiobj_s *);
  /** object value as Float */
  double (*const to_f)(const fiobj_s *);
  /**
   * returns 1 if objects are equal, 0 if unequal.
   *
   * `self` and `other` are never NULL.
   *
   * objects that enumerate (`count > 0`) should only test
   * themselves (not their children). any nested objects will be tested
   * seperately.
   *
   * wrapping objects should forward the function call to the wrapped objectd
   * (similar to `count` and `each1`) after completing any internal testing.
   */
  int (*const is_eq)(const fiobj_s *self, const fiobj_s *other);
  /**
   * return the number of nested object
   *
   * wrapping objects should forward the function call to the wrapped objectd
   * (similar to `each1`).
   */
  size_t (*const count)(const fiobj_s *o);
  /**
   * return either `self` or a wrapped object.
   * (if object wrapping exists, i.e. Hash couplet, return nested object)
   */
  fiobj_s *(*const unwrap)(const fiobj_s *obj);
  /**
   * perform a task for the object's children (-1 stops iteration)
   * returns the number of items processed + `start_at`.
   *
   * wrapping objects should forward the function call to the wrapped objectd
   * (similar to `count`).
   */
  size_t (*const each1)(fiobj_s *, size_t start_at,
                        int (*task)(fiobj_s *obj, void *arg), void *arg);
};

// extern struct fiobj_vtable_s FIOBJ_VTABLE_INVALID; // unused just yet
/* *****************************************************************************
VTable (virtual function table) common implememntations
***************************************************************************** */

/** simple deallocation (`free`). */
void fiobj_simple_dealloc(fiobj_s *o);
/** no deallocation (eternal objects). */
void fiobj_noop_free(fiobj_s *obj);
/** always true. */
int fiobj_noop_true(const fiobj_s *obj);
/** always false. */
int fiobj_noop_false(const fiobj_s *obj);
/** NULL C string. */
fio_cstr_s fiobj_noop_str(const fiobj_s *obj);
/** always 0. */
int64_t fiobj_noop_i(const fiobj_s *obj);
/** always 0. */
double fiobj_noop_f(const fiobj_s *obj);
/** always 0. */
size_t fiobj_noop_count(const fiobj_s *obj);
/** always 0. */
int fiobj_noop_is_eq(const fiobj_s *self, const fiobj_s *other);
/** always self. */
fiobj_s *fiobj_noop_unwrap(const fiobj_s *obj);
/** always 0. */
size_t fiobj_noop_each1(fiobj_s *obj, size_t start_at,
                        int (*task)(fiobj_s *obj, void *arg), void *arg);

/* *****************************************************************************
The Object type head and management
***************************************************************************** */

typedef struct { uintptr_t ref; } fiobj_head_s;

#define OBJ2HEAD(o) (((fiobj_head_s *)(o)) - 1)
#define HEAD2OBJ(o) ((fiobj_s *)(((fiobj_head_s *)(o)) + 1))

#define OBJREF_ADD(o) spn_add(&(OBJ2HEAD((o))->ref), 1)
#define OBJREF_REM(o) spn_sub(&(OBJ2HEAD((o))->ref), 1)

#define OBJVTBL(o) ((struct fiobj_vtable_s *)(((fiobj_s *)(o))->type))

// #define PTR2OBJ(o) (((o) << 1) | 1)
// #define OBJ2PTR(o) (((o)&1) ? ((o) >> 1) : (o))

/* *****************************************************************************
Internal API required across the board
***************************************************************************** */

/** Allocates memory for the fiobj_s's data structure */
static inline fiobj_s *fiobj_alloc(size_t size) {
  fiobj_head_s *head = (fiobj_head_s *)malloc(size + sizeof(head));
  if (!head)
    return NULL;
  *head = (fiobj_head_s){.ref = 1};
  return HEAD2OBJ(head);
};

/** Deallocates the fiobj_s's data structure. */
static inline void fiobj_dealloc(fiobj_s *obj) { free(OBJ2HEAD(obj)); }

/** The Hashing function used by dynamic facil.io objects. */
uint64_t fiobj_sym_hash(const void *data, size_t len);

#endif
