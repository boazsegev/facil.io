/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_DICT_H
/**
`fio_dict_s` Is based on a 4-bit trie structure, allowing for fast no-collisions
key-value matching while avoiding any hashing.

It's memory intensive... very memory intensive... but it has 0 collision risk
and offers fairly high performance.

Just to offer some insight, a single key-value pair for the key "hello" will
require ~1,360 bytes. Add the key "bye!" ad you'll add ~1,088 bytes more... but
the key "hello1" will cost only 272 bytes... brrr.
*/
#define H_FIO_DICT_H

#include <stdint.h>
#include <stdlib.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct fio_dict_s {
  struct fio_dict_s *parent;
  struct fio_dict_s *trie[16];
  unsigned used : 1;
  unsigned trie_val : 4;
} fio_dict_s;

#define FIO_DICT_INIT                                                          \
  (fio_dict_s) { .parent = NULL }
#define FIO_DICT_INIT_STATIC                                                   \
  { .parent = NULL }

#ifndef fio_node2obj
#define fio_node2obj(type, member, ptr)                                        \
  ((type *)((uintptr_t)(ptr) - (uintptr_t)(&(((type *)0)->member))))
#endif

/* *****************************************************************************
API
***************************************************************************** */

/** Returns the `fio_dict_s *` object associated with the key, NULL if none. */
fio_dict_s *fio_dict_get(fio_dict_s *dict, void *key, size_t key_len);

/** Returns the old `fio_dict_s *` object associated with the key, if any.*/
fio_dict_s *fio_dict_set(fio_dict_s *dict, void *key, size_t key_len,
                         fio_dict_s *node);

/** Removes and returns the specified `fio_dict_s *` object.*/
fio_dict_s *fio_dict_remove(fio_dict_s *node);

/** Returns a `fio_dict_s *` dictionary (or NULL) of all `prefix` children. */
fio_dict_s *fio_dict_step(fio_dict_s *dict, uint8_t prefix);

/** Returns a `fio_dict_s *` dictionary (or NULL) of all `prefix` children. */
fio_dict_s *fio_dict_prefix(fio_dict_s *dict, void *prefix, size_t len);

/**
 * Creates a `fio_dict_s *` dictionary (if missing) for the `prefix`...
 *
 * After calling this function a node MUST be added to this dictionary, or
 * memory leaks will occure.
 */
fio_dict_s *fio_dict_ensure_prefix(fio_dict_s *dict, void *prefix, size_t len);

/** Traverses a dictionary, performing an action for each item. */
void fio_dict_each(fio_dict_s *dict,
                   void (*action)(fio_dict_s *node, void *arg), void *arg);

/** Performing an action for each item matching the glob pattern. */
void fio_dict_each_match_glob(fio_dict_s *dict, void *pattern, size_t len,
                              void (*action)(fio_dict_s *node, void *arg),
                              void *arg);

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
int fio_glob_match(uint8_t *data, size_t data_len, uint8_t *pattern,
                   size_t pat_len);

#define fio_dict_isempty(dict)                                                 \
  (!((dict)->trie[0] || (dict)->trie[1] || (dict)->trie[2] ||                  \
     (dict)->trie[3] || (dict)->trie[4] || (dict)->trie[5] ||                  \
     (dict)->trie[6] || (dict)->trie[7] || (dict)->trie[8] ||                  \
     (dict)->trie[9] || (dict)->trie[10] || (dict)->trie[11] ||                \
     (dict)->trie[12] || (dict)->trie[13] || (dict)->trie[14] ||               \
     (dict)->trie[15]))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
