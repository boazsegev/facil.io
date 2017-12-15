#ifndef H_FIO_SIMPLE_LIST_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

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
Simple List - Used for existing objects (not embeddable).
***************************************************************************** */

typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const void *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/** Adds an object to the list's head. */
inline FIO_FUNC void fio_ls_push(fio_ls_s *pos, const void *obj) {
  /* prepare item */
  fio_ls_s *item = (fio_ls_s *)malloc(sizeof(*item));
  if (!item)
    perror("ERROR: simple list couldn't allocate memory"), exit(errno);
  *item = (fio_ls_s){.prev = pos, .next = pos->next, .obj = obj};
  /* inject item */
  pos->next->prev = item;
  pos->next = item;
}

/** Adds an object to the list's tail. */
inline FIO_FUNC void fio_ls_unshift(fio_ls_s *pos, const void *obj) {
  pos = pos->prev;
  fio_ls_push(pos, obj);
}

/** Removes an object from the list's head. */
inline FIO_FUNC void *fio_ls_pop(fio_ls_s *list) {
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
inline FIO_FUNC void *fio_ls_shift(fio_ls_s *list) {
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
inline FIO_FUNC void *fio_ls_remove(fio_ls_s *node) {
  const void *ret = node->obj;
  node->next->prev = node->prev->next;
  node->prev->next = node->next->prev;
  free(node);
  return (void *)ret;
}

#endif
