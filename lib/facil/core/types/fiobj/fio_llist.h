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
#include <stdint.h>
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

/** Removes an object from the containing node. */
FIO_FUNC inline void *fio_ls_remove(fio_ls_s *node) {
  if (node->next == node) {
    /* never remove the list's head */
    return NULL;
  }
  const void *ret = node->obj;
  node->next->prev = node->prev;
  node->prev->next = node->next;
  free(node);
  return (void *)ret;
}

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
  return fio_ls_remove(list->prev);
}

/** Removes an object from the list's tail. */
FIO_FUNC inline void *fio_ls_shift(fio_ls_s *list) {
  return fio_ls_remove(list->next);
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

/** Removes a node from the containing node. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node) {
  if (node->next == node) {
    /* never remove the list's head */
    return NULL;
  }
  node->next->prev = node->prev;
  node->prev->next = node->next;
  return node;
}

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
  return fio_ls_embd_remove(list->prev);
}

/** Removes a node from the list's tail. */
FIO_FUNC inline fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list) {
  return fio_ls_embd_remove(list->next);
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

/* *****************************************************************************
Testing
***************************************************************************** */
#if DEBUG
#include <stdio.h>
#define TEST_LIMIT 1016
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }
/**
 * Removes any FIO_ARY_TYPE_INVALID  *pointers* from an Array, keeping all other
 * data in the array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
FIO_FUNC inline void fio_llist_test(void) {
  fio_ls_s list = FIO_LS_INIT(list);
  size_t counter;
  fprintf(stderr, "=== Testing Core Linked List features (fio_llist.h)\n");
  /* test push/pop */
  for (uintptr_t i = 0; i < TEST_LIMIT; ++i) {
    fio_ls_push(&list, (void *)i);
  }
  TEST_ASSERT(fio_ls_any(&list), "List should be populated after fio_ls_push");
  counter = 0;
  while (fio_ls_any(&list)) {
    TEST_ASSERT(counter < TEST_LIMIT,
                "`fio_ls_any` didn't return false when expected %p<=%p=>%p",
                (void *)list.prev, (void *)&list, (void *)list.next);
    size_t tmp = (size_t)fio_ls_pop(&list);
    TEST_ASSERT(tmp == counter, "`fio_ls_pop` value error (%zu != %zu)", tmp,
                counter);
    ++counter;
  }
  TEST_ASSERT(counter == TEST_LIMIT, "List item count error (%zu != %zu)",
              counter, (size_t)TEST_LIMIT);
  /* test shift/unshift */
  for (uintptr_t i = 0; i < TEST_LIMIT; ++i) {
    fio_ls_unshift(&list, (void *)i);
  }
  TEST_ASSERT(fio_ls_any(&list),
              "List should be populated after fio_ls_unshift");
  counter = 0;
  while (!fio_ls_is_empty(&list)) {
    TEST_ASSERT(counter < TEST_LIMIT,
                "`fio_ls_is_empty` didn't return true when expected %p<=%p=>%p",
                (void *)list.prev, (void *)&list, (void *)list.next);
    size_t tmp = (size_t)fio_ls_shift(&list);
    TEST_ASSERT(tmp == counter, "`fio_ls_shift` value error (%zu != %zu)", tmp,
                counter);
    ++counter;
  }
  TEST_ASSERT(counter == TEST_LIMIT, "List item count error (%zu != %zu)",
              counter, (size_t)TEST_LIMIT);

  /* Re-test for embeded list */

  struct fio_ls_test_s {
    size_t i;
    fio_ls_embd_s node;
  };

  fio_ls_embd_s emlist = FIO_LS_INIT(emlist);

  /* test push/pop */
  for (uintptr_t i = 0; i < TEST_LIMIT; ++i) {
    struct fio_ls_test_s *n = malloc(sizeof(*n));
    n->i = i;
    fio_ls_embd_push(&emlist, &n->node);
  }
  TEST_ASSERT(fio_ls_embd_any(&emlist),
              "List should be populated after fio_ls_embd_push");
  counter = 0;
  while (fio_ls_embd_any(&emlist)) {
    TEST_ASSERT(
        counter < TEST_LIMIT,
        "`fio_ls_embd_any` didn't return false when expected %p<=%p=>%p",
        (void *)emlist.prev, (void *)&emlist, (void *)emlist.next);
    struct fio_ls_test_s *n =
        FIO_LS_EMBD_OBJ(struct fio_ls_test_s, node, fio_ls_embd_pop(&emlist));
    TEST_ASSERT(n->i == counter, "`fio_ls_embd_pop` value error (%zu != %zu)",
                n->i, counter);
    free(n);
    ++counter;
  }
  TEST_ASSERT(counter == TEST_LIMIT, "List item count error (%zu != %zu)",
              counter, (size_t)TEST_LIMIT);
  /* test shift/unshift */
  for (uintptr_t i = 0; i < TEST_LIMIT; ++i) {
    struct fio_ls_test_s *n = malloc(sizeof(*n));
    n->i = i;
    fio_ls_embd_unshift(&emlist, &n->node);
  }
  TEST_ASSERT(fio_ls_embd_any(&emlist),
              "List should be populated after fio_ls_embd_unshift");
  counter = 0;
  while (!fio_ls_embd_is_empty(&emlist)) {
    TEST_ASSERT(
        counter < TEST_LIMIT,
        "`fio_ls_embd_is_empty` didn't return true when expected %p<=%p=>%p",
        (void *)emlist.prev, (void *)&emlist, (void *)emlist.next);
    struct fio_ls_test_s *n =
        FIO_LS_EMBD_OBJ(struct fio_ls_test_s, node, fio_ls_embd_shift(&emlist));
    TEST_ASSERT(n->i == counter, "`fio_ls_embd_shift` value error (%zu != %zu)",
                n->i, counter);
    free(n);
    ++counter;
  }
  TEST_ASSERT(counter == TEST_LIMIT, "List item count error (%zu != %zu)",
              counter, (size_t)TEST_LIMIT);
  fprintf(stderr, "* passed.\n");
}

#undef TEST_LIMIT
#undef TEST_ASSERT
#endif

/* *****************************************************************************
Done
***************************************************************************** */
#undef FIO_FUNC

#endif
