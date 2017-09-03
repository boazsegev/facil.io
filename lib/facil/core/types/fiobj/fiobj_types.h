/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#ifndef H_FIOBJ_TYPES_INTERNAL_H
#define H_FIOBJ_TYPES_INTERNAL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fiobj.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

/* *****************************************************************************
Atomic add / subtract
***************************************************************************** */

/* Select the correct compiler builtin method. */
#if defined(__has_builtin)

#if __has_builtin(__atomic_exchange_n)
#define SPN_LOCK_BUILTIN(...) __atomic_exchange_n(__VA_ARGS__, __ATOMIC_ACQ_REL)
/** An atomic addition operation */
#define spn_add(...) __atomic_add_fetch(__VA_ARGS__, __ATOMIC_ACQ_REL)
/** An atomic subtraction operation */
#define spn_sub(...) __atomic_sub_fetch(__VA_ARGS__, __ATOMIC_ACQ_REL)

#elif __has_builtin(__sync_fetch_and_or)
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
Simple List - I will slowly move any external dependencies to allow independance
***************************************************************************** */

typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  fiobj_s *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/** Adds an object to the list's head. */
static inline __attribute__((unused)) void fio_ls_push(fio_ls_s *pos,
                                                       fiobj_s *obj) {
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
                                                          fiobj_s *obj) {
  pos = pos->prev;
  fio_ls_push(pos, obj);
}

/** Removes an object from the list's head. */
static inline __attribute__((unused)) fiobj_s *fio_ls_pop(fio_ls_s *list) {
  if (list->next == list)
    return NULL;
  fio_ls_s *item = list->next;
  fiobj_s *ret = item->obj;
  list->next = item->next;
  list->next->prev = list;
  free(item);
  return ret;
}

/** Removes an object from the list's tail. */
static inline __attribute__((unused)) fiobj_s *fio_ls_shift(fio_ls_s *list) {
  if (list->prev == list)
    return NULL;
  fio_ls_s *item = list->prev;
  fiobj_s *ret = item->obj;
  list->prev = item->prev;
  list->prev->next = list;
  free(item);
  return ret;
}

/** Removes an object from the containing node. */
static inline __attribute__((unused)) fiobj_s *fio_ls_remove(fio_ls_s *node) {
  fiobj_s *ret = node->obj;
  node->next->prev = node->prev->next;
  node->prev->next = node->next->prev;
  free(node);
  return ret;
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
Object types
***************************************************************************** */

/* Number */
typedef struct {
  fiobj_type_en type;
  int64_t i;
} fio_num_s;

/* Float */
typedef struct {
  fiobj_type_en type;
  double f;
} fio_float_s;

/* String */
typedef struct {
  fiobj_type_en type;
  uint64_t capa;
  uint64_t len;
  uint8_t is_static;
  char *str;
} fio_str_s;

/* Symbol */
typedef struct {
  fiobj_type_en type;
  uintptr_t hash;
  uint64_t len;
  char str[];
} fio_sym_s;

/* IO */
typedef struct {
  fiobj_type_en type;
  intptr_t fd;
} fio_io_s;

/* Array */
typedef struct {
  fiobj_type_en type;
  uint64_t start;
  uint64_t end;
  uint64_t capa;
  fiobj_s **arry;
} fio_ary_s;

/* *****************************************************************************
Hash types
***************************************************************************** */
/* MUST be a power of 2 */
#define FIOBJ_HASH_MAX_MAP_SEEK (256)

typedef struct {
  uintptr_t hash;
  fio_ls_s *container;
} map_info_s;

typedef struct {
  uintptr_t capa;
  map_info_s *data;
} fio_map_s;

typedef struct {
  fiobj_type_en type;
  uintptr_t count;
  uintptr_t mask;
  fio_ls_s items;
  fio_map_s map;
} fio_hash_s;

/* Hash node */
typedef struct {
  fiobj_type_en type;
  fiobj_s *name;
  fiobj_s *obj;
} fio_couplet_s;

void fiobj_hash_rehash(fiobj_s *h);

/* *****************************************************************************
Type VTable (virtual function table)
***************************************************************************** */
struct fiobj_vtable_s {
  /* deallocate an object */
  void (*free)(fiobj_s *);
  /* object value as String */
  fio_cstr_s (*to_str)(fiobj_s *);
  /* object value as Integer */
  int64_t (*to_i)(fiobj_s *);
  /* object value as Float */
  double (*to_f)(fiobj_s *);
  /* true if object is equal. nested objects must be ignored (test container) */
  int (*is_eq)(fiobj_s *self, fiobj_s *other);
  /* return the number of nested object */
  size_t (*count)(fiobj_s *);
  /* perform a task for the object's children (-1 stops iteration)
   * returns the number of items processed + `start_at`.
   */
  size_t (*each1)(fiobj_s *, size_t start_at,
                  int (*task)(fiobj_s *obj, void *arg), void *arg);
};

fio_cstr_s fiobj_noop_str(fiobj_s *obj);
int64_t fiobj_noop_i(fiobj_s *obj);
double fiobj_noop_f(fiobj_s *obj);
size_t fiobj_noop_count(fiobj_s *obj);
size_t fiobj_noop_each1(fiobj_s *obj, size_t start_at,
                        int (*task)(fiobj_s *obj, void *arg), void *arg);
void fiobj_simple_dealloc(fiobj_s *o);

/* *****************************************************************************
The Object type head and management
***************************************************************************** */

typedef struct {
  uint64_t ref;
  struct fiobj_vtable_s *vtable;
} fiobj_head_s;

#define OBJ2HEAD(o) (((fiobj_head_s *)(o)) - 1)[0]
#define HEAD2OBJ(o) ((fiobj_s *)(((fiobj_head_s *)(o)) + 1))

#define OBJREF_ADD(o) spn_add(&OBJ2HEAD((o)).ref, 1)
#define OBJREF_REM(o) spn_sub(&OBJ2HEAD((o)).ref, 1)

#define obj2io(o) ((fio_io_s *)(o))
#define obj2ary(o) ((fio_ary_s *)(o))
#define obj2str(o) ((fio_str_s *)(o))
#define obj2sym(o) ((fio_sym_s *)(o))
#define obj2num(o) ((fio_num_s *)(o))
#define obj2hash(o) ((fio_hash_s *)(o))
#define obj2float(o) ((fio_float_s *)(o))
#define obj2couplet(o) ((fio_couplet_s *)(o))

/* *****************************************************************************
Internal API required across the board
***************************************************************************** */
void fiobj_dealloc(fiobj_s *obj);
uint64_t fiobj_sym_hash(const void *data, size_t len);

#endif
