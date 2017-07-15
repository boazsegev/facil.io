/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#ifndef H_FIOBJ_TYPES_INTERNAL_H
#define H_FIOBJ_TYPES_INTERNAL_H

#include "spnlock.inc"

#include "fiobj.h"

#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

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
  fio_ls_s *item = malloc(sizeof(*item));
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
The Object type head and management
***************************************************************************** */

typedef struct { uint64_t ref; } fiobj_head_s;

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
fiobj_s *fiobj_alloc(fiobj_type_en type, uint64_t len, void *buffer);
void fiobj_dealloc(fiobj_s *obj);

uint64_t fiobj_sym_hash(const void *data, size_t len);
#endif
