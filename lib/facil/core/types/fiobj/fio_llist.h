#ifndef H_FIO_SIMPLE_LIST_H
/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

/* *****************************************************************************
Simple Linked List - Used for existing objects (not embeddable).
***************************************************************************** */

/**
This header includes all the internal rescources / data and types required to
create object types.
*/
#define H_FIO_SIMPLE_LIST_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* *****************************************************************************
Data Structure and Initialization.
***************************************************************************** */

/** an embeded linked list. */
typedef struct fio_ls_embd_s {
  struct fio_ls_embd_s *prev;
  struct fio_ls_embd_s *next;
} fio_ls_embd_s;

/** an independent linked list. */
typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const void *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/* *****************************************************************************
Independent List API
***************************************************************************** */

/** Adds an object to the list's head. */
FIO_FUNC inline void fio_ls_push(fio_ls_s *pos, const void *obj);

/** Adds an object to the list's tail. */
FIO_FUNC inline void fio_ls_unshift(fio_ls_s *pos, const void *obj);

/** Removes an object from the list's head. */
FIO_FUNC inline void *fio_ls_pop(fio_ls_s *list);

/** Removes an object from the list's tail. */
FIO_FUNC inline void *fio_ls_shift(fio_ls_s *list);

/** Removes a node from the list, returning the contained object. */
FIO_FUNC inline void *fio_ls_remove(fio_ls_s *node);

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_is_empty(fio_ls_s *list);

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_any(fio_ls_s *list);

/**
 * Iterates through the list using a `for` loop.
 *
 * Access the data with `pos->obj` (`pos` can be named however you pleas..
 */
#define FIO_LS_FOR(list, pos)

/* *****************************************************************************
Embedded List API
***************************************************************************** */

/** Adds a node to the list's head. */
FIO_FUNC inline void fio_ls_embd_push(fio_ls_embd_s *dest, fio_ls_embd_s *node);

/** Adds a node to the list's tail. */
FIO_FUNC inline void fio_ls_embd_unshift(fio_ls_embd_s *dest,
                                         fio_ls_embd_s *node);

/** Removes a node from the list's head. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list);

/** Removes a node from the list's tail. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list);

/** Removes a node from the containing node. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node);

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_embd_is_empty(fio_ls_embd_s *list);

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_embd_any(fio_ls_embd_s *list);

/**
 * Iterates through the list using a `for` loop.
 *
 * Access the data with `pos->obj` (`pos` can be named however you pleas..
 */
#define FIO_LS_EMBD_FOR(list, node)

/**
 * Takes a list pointer `plist` and returns a pointer to it's container.
 *
 * This uses pointer offset calculations and can be used to calculate any
 * struct's pointer (not just list containers) as an offset from a pointer of
 * one of it's members.
 *
 * Very useful.
 */
#define FIO_LS_EMBD_OBJ(type, member, plist)                                   \
  ((type *)((uintptr_t)(plist) - (uintptr_t)(&(((type *)0)->member))))

/* *****************************************************************************
Independent Implementation
***************************************************************************** */

/** Adds an object to the list's head. */
FIO_FUNC inline void fio_ls_push(fio_ls_s *pos, const void *obj) {
  /* prepare item */
  fio_ls_s *item = (fio_ls_s *)malloc(sizeof(*item));
  if (!item) {
    perror("ERROR: simple list couldn't allocate memory");
    exit(errno);
  }
  *item = (fio_ls_s){.prev = pos, .next = pos->next, .obj = obj};
  /* inject item */
  pos->next->prev = item;
  pos->next = item;
}

/** Adds an object to the list's tail. */
FIO_FUNC inline void fio_ls_unshift(fio_ls_s *pos, const void *obj) {
  pos = pos->prev;
  fio_ls_push(pos, obj);
}

/** Removes an object from the list's head. */
FIO_FUNC inline void *fio_ls_pop(fio_ls_s *list) {
  if (list->next == list)
    return NULL;
  fio_ls_s *item = list->next;
  const void *ret = item->obj;
  list->next = item->next;
  list->next->prev = list;
  free(item);
  return (void *)ret;
}

/** Removes an object from the list's tail. */
FIO_FUNC inline void *fio_ls_shift(fio_ls_s *list) {
  if (list->prev == list)
    return NULL;
  fio_ls_s *item = list->prev;
  const void *ret = item->obj;
  list->prev = item->prev;
  list->prev->next = list;
  free(item);
  return (void *)ret;
}

/** Removes an object from the containing node. */
FIO_FUNC inline void *fio_ls_remove(fio_ls_s *node) {
  const void *ret = node->obj;
  node->next->prev = node->prev;
  node->prev->next = node->next;
  free(node);
  return (void *)ret;
}

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_is_empty(fio_ls_s *list) {
  return list->next == list;
}

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_any(fio_ls_s *list) { return list->next != list; }

#undef FIO_LS_FOR
#define FIO_LS_FOR(list, pos)                                                  \
  for (fio_ls_s *pos = (list)->next; pos != (list); pos = pos->next)

/* *****************************************************************************
Embeded List Implementation
***************************************************************************** */

/** Adds a node to the list's head. */
FIO_FUNC inline void fio_ls_embd_push(fio_ls_embd_s *dest,
                                      fio_ls_embd_s *node) {
  node->next = dest->next;
  node->prev = dest->next->prev;
  dest->next->prev = node;
  dest->next = node;
}

/** Adds a node to the list's tail. */
FIO_FUNC inline void fio_ls_embd_unshift(fio_ls_embd_s *dest,
                                         fio_ls_embd_s *node) {
  fio_ls_embd_push(dest->prev, node);
}

/** Removes a node from the list's head. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list) {
  if (list->next == list)
    return NULL;
  fio_ls_embd_s *item = list->next;
  list->next = item->next;
  list->next->prev = list;
  return item;
}

/** Removes a node from the list's tail. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list) {
  if (list->prev == list)
    return NULL;
  fio_ls_embd_s *item = list->prev;
  list->prev = item->prev;
  list->prev->next = list;
  return item;
}

/** Removes a node from the containing node. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
  return node;
}

/** Tests if the list is empty. */
FIO_FUNC inline int fio_ls_embd_is_empty(fio_ls_embd_s *list) {
  return list->next == list;
}

/** Tests if the list is NOT empty (contains any nodes). */
FIO_FUNC inline int fio_ls_embd_any(fio_ls_embd_s *list) {
  return list->next != list;
}

#undef FIO_LS_EMBD_FOR
#define FIO_LS_EMBD_FOR(list, node)                                            \
  for (fio_ls_embd_s *node = (list)->next; node != (list); node = node->next)

#undef FIO_FUNC

#endif
