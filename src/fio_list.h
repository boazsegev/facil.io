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

typedef struct fio_list_s {
  struct fio_list_s *prev;
  struct fio_list_s *next;
} fio_list_s;

static inline __attribute__((unused)) void fio_list_init(fio_list_s *list) {
  *list = (fio_list_s){.next = list, .prev = list};
}

#define FIO_LIST_INIT(name)                                                    \
  (fio_list_s) { .next = &(name), .prev = &(name) }

static inline __attribute__((unused)) void fio_list_add(fio_list_s *pos,
                                                        fio_list_s *item) {
  /* prepare item */
  item->next = pos->next;
  item->prev = pos;
  /* inject item only after preperation (in case of misuse or memory tearing) */
  pos->next = item;
  item->next->prev = item;
}

static inline __attribute__((unused)) fio_list_s *
fio_list_remove(fio_list_s *item) {
  item->next->prev = item->prev;
  item->prev->next = item->next;
  *item = (fio_list_s){.next = item, .prev = item};
  return item;
}

#define fio_list_object(type, member, plist)                                   \
  ((type *)((uintptr_t)(plist) - (uintptr_t)(&(((type *)0)->member))))

#define fio_list_for_each(type, member, var, head)                             \
  for (fio_list_s *pos = (head).next;                                          \
       pos != &(head) && ((var) = fio_list_object(type, member, pos)) &&       \
       (pos = pos->next);)

#define fio_list_pop(type, member, head)                                       \
  (((head).prev == &(head))                                                    \
       ? ((type *)(0x0))                                                       \
       : (fio_list_object(type, member, fio_list_remove((head).prev))))

#define fio_list_push(type, member, head, pitem)                               \
  fio_list_add((head).prev, &(pitem)->member)

#define fio_list_shift(type, member, head)                                     \
  (((head).next == &(head))                                                    \
       ? ((type *)(0x0))                                                       \
       : (fio_list_object(type, member, fio_list_remove((head).next))))

#define fio_list_unshift(type, member, head, pitem)                            \
  fio_list_add((head).next, &(pitem)->member)

#endif
