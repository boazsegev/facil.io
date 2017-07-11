/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Hash API
***************************************************************************** */

/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void) { return fiobj_alloc(FIOBJ_T_HASH, 0, NULL); }

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(fiobj_s *hash) {
  if (!hash || hash->type != FIOBJ_T_HASH)
    return 0;
  return ((fio_hash_s *)hash)->h.count;
}

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    fiobj_dealloc((fiobj_s *)obj);
    return -1;
  }
  fiobj_s *coup = fiobj_alloc(FIOBJ_T_COUPLET, 0, NULL);
  ((fio_couplet_s *)coup)->name = fiobj_dup(sym);
  ((fio_couplet_s *)coup)->obj = obj;
  coup = (fiobj_s *)fio_ht_add(&((fio_hash_s *)hash)->h,
                               &((fio_couplet_s *)coup)->node,
                               ((fio_sym_s *)sym)->hash);
  if (coup) {
    coup = (fiobj_s *)fio_node2obj(fio_couplet_s, node, coup);
    fiobj_free(((fio_couplet_s *)coup)->obj);
    fiobj_dealloc(((fio_couplet_s *)coup)->name);
    fiobj_dealloc(coup);
  }
  return 0;
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash_remove(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL)
    return NULL;
  fio_couplet_s *coup =
      (void *)fio_ht_pop(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup)
    return NULL;
  coup = fio_node2obj(fio_couplet_s, node, coup);
  fiobj_s *obj = coup->obj;
  coup->obj = NULL;
  fiobj_dealloc(((fio_couplet_s *)coup)->name);
  fiobj_dealloc((fiobj_s *)coup);
  return obj;
}

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(fiobj_s *hash, fiobj_s *sym) {
  fiobj_s *obj = fiobj_hash_remove(hash, sym);
  if (!obj)
    return -1;
  fiobj_free(obj);
  return 0;
}

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL
 * if none.
 */
fiobj_s *fiobj_hash_get(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return NULL;
  }
  fio_couplet_s *coup =
      (void *)fio_ht_find(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup) {
    return NULL;
  }
  coup = fio_node2obj(fio_couplet_s, node, coup);
  return coup->obj;
}

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash_haskey(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return 0;
  }
  fio_couplet_s *coup =
      (void *)fio_ht_find(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup) {
    return 0;
  }
  return 1;
}

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2key(fiobj_s *obj) {
  if (!obj || obj->type != FIOBJ_T_COUPLET)
    return NULL;
  return ((fio_couplet_s *)obj)->name;
}

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(fiobj_s *obj) {
  if (!obj || obj->type != FIOBJ_T_COUPLET)
    return obj;
  return ((fio_couplet_s *)obj)->obj;
}
