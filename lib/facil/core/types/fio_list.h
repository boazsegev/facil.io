/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FACIL_IO_LIST_H
/**
A simple doubly linked list implementation... Doesn't do much, but does it well.
*/
#define H_FACIL_IO_LIST_H

/** The data structure for a doubly linked list. */
typedef struct fio_list_s {
  struct fio_list_s *prev;
  struct fio_list_s *next;
} fio_list_s;

/** An inline function that initializes a list's head. */
static inline __attribute__((unused)) void fio_list_init(fio_list_s *list) {
  *list = (fio_list_s){.next = list, .prev = list};
}

/** A macro that evaluates to an initialized list head. */
#define FIO_LIST_INIT(name)                                                    \
  (fio_list_s) { .next = &(name), .prev = &(name) }
/** A macro that evaluates to an initialized list head. */
#define FIO_LIST_INIT_STATIC(name)                                             \
  { .next = &(name), .prev = &(name) }

/** Adds a list reference to the list at the specified position. */
static inline __attribute__((unused)) void fio_list_add(fio_list_s *pos,
                                                        fio_list_s *item) {
  /* prepare item */
  item->next = pos->next;
  item->prev = pos;
  /* inject item only after preperation (in case of misuse or memory tearing) */
  pos->next = item;
  item->next->prev = item;
}

/** Removes a list reference from the list. Returns the same reference. */
static inline __attribute__((unused)) fio_list_s *
fio_list_remove(fio_list_s *item) {
  item->next->prev = item->prev;
  item->prev->next = item->next;
  *item = (fio_list_s){.next = item, .prev = item};
  return item;
}
/** Switches two list items. */
static inline __attribute__((unused)) void fio_list_switch(fio_list_s *item1,
                                                           fio_list_s *item2) {
  if (item1 == item2)
    return;
  fio_list_s tmp = *item1;
  *item1 = *item2;
  *item2 = tmp;
  if (item1->next == item2)
    item1->next = item1;
  else
    item1->next->prev = item1;
  if (item1->prev == item2)
    item1->prev = item1;
  else
    item1->prev->next = item1;
}

#ifndef fio_node2obj
/** Takes a node pointer (list/hash/dict, etc') and returns it's container. */
#define fio_node2obj(type, member, ptr)                                        \
  ((type *)((uintptr_t)(ptr) - (uintptr_t)(&(((type *)0)->member))))
#endif

/** Takes a list pointer and returns a pointer to it's container. */
#define fio_list_object(type, member, plist) fio_node2obj(type, member, (plist))

/** iterates the whole list. */
#define fio_list_for_each(type, member, var, head)                             \
  for (fio_list_s *pos = (head).next->next;                                    \
       (&((var) = fio_list_object(type, member, pos->prev))->member !=         \
        &(head)) ||                                                            \
       ((var) = NULL);                                                         \
       (var) = fio_list_object(type, member, pos), pos = pos->next)

/** Removes a member from the end of the list. */
#define fio_list_pop(type, member, head)                                       \
  (((head).prev == &(head))                                                    \
       ? ((type *)(0x0))                                                       \
       : (fio_list_object(type, member, fio_list_remove((head).prev))))

/** Adds a member to the end of the list. */
#define fio_list_push(type, member, head, pitem)                               \
  fio_list_add((head).prev, &(pitem)->member)

/** Removes a member from the beginning of the list. */
#define fio_list_shift(type, member, head)                                     \
  (((head).next == &(head))                                                    \
       ? ((type *)(0x0))                                                       \
       : (fio_list_object(type, member, fio_list_remove((head).next))))

/** Adds a member to the beginning of the list. */
#define fio_list_unshift(type, member, head, pitem)                            \
  fio_list_add((head).next, &(pitem)->member)

/** Tests if the list is empty. */
#define fio_list_is_empty(head) ((head).next == &(head))

/** Tests if the list is NOT empty. */
#define fio_list_any(head) ((head).next != &(head))

#endif
