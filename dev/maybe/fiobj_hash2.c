/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj/fiobj_types.h"

/* Hash2 */
typedef struct {
  fiobj_type_en type;
  /* an ordered array for all the couplets. pos 0 is reserved (NULL)*/
  fiobj_s *list;
  /* an ordered array for all the couplets. pos 0 is reserved (NULL)*/
  fiobj_s *index;
  /* the Hash mask for index selection. */
  uint64_t mask;
  /* a counter for removed items. */
  uint64_t removed;
} fio_hash2_s;

#define obj2hash2(o) ((fio_hash2_s *)(o))

#define HASH2_MAX_COLLIDE_SEQ 8

fiobj_s **fiobj_hash2_seek(fio_hash2_s *h, uintptr_t hash) {
  uint64_t pos = obj2ary((h->index))->start + (h->mask & hash);
  uint64_t end = obj2ary((h->index))->end;
  /* limit seek */
  if (end > pos + (HASH2_MAX_COLLIDE_SEQ << 1))
    end = pos + (HASH2_MAX_COLLIDE_SEQ << 1);
  while (pos < end) {
    if (!obj2ary((h->list))->arry[pos])
      return NULL;
    if (obj2ary((h->list))->arry[pos] == (fiobj_s *)hash)
      return (fiobj_s **)obj2ary((h->list))->arry[pos + 1];
    pos += 2;
  }
  return NULL;
}

int fiobj_hash2_insert(fio_hash2_s *h, fiobj_s *obj, uintptr_t hash) {
  fiobj_s **tmp = NULL;
  uint64_t pos = obj2ary((h->index))->start + (h->mask & hash);
  uint64_t end = obj2ary((h->index))->end;
  /* limit seek */
  if (end > pos + (HASH2_MAX_COLLIDE_SEQ << 1))
    end = pos + (HASH2_MAX_COLLIDE_SEQ << 1);
  while (pos < end) {
    if (!obj2ary((h->index))->arry[pos])
      goto add_link;
    if (obj2ary((h->index))->arry[pos] == (fiobj_s *)hash)
      goto recycle_link;
    pos += 2;
  }
  return -1;

add_link:
  obj2ary((h->index))->arry[pos] = (fiobj_s *)hash;
  fiobj_ary_push(h->list, obj);
  obj2ary((h->index))->arry[pos + 1] =
      (fiobj_s *)obj2ary((h->list))->arry + obj2ary((h->list))->end - 1;
  return 0;

recycle_link:
  tmp = (fiobj_s **)obj2ary((h->list))->arry[pos + 1];
  if (!(*tmp)) {
    /* there was a couplet here, but it was removed... */
    if (h->removed)
      h->removed--;
  } else {
    /* free old data */
    fiobj_free(((fio_couplet_s *)(*tmp))->obj);
    fiobj_dealloc(((fio_couplet_s *)(*tmp))->name);
    fiobj_dealloc((*tmp));
  }
  *tmp = obj;
  return 0;
}

int fiobj_hash2_rehash(fio_hash2_s *h) {
  obj2ary((h->index))->start = 0;
  obj2ary((h->index))->end = 0;
  h->removed = 0;
  uint64_t reader = obj2ary((h->list))->start;
  uint64_t writer = obj2ary((h->list))->start;
  while (reader < obj2ary((h->list))->end) {
    if (!obj2ary((h->list))->arry[reader]) {
      reader++;
      continue;
    }
    obj2ary((h->list))->arry[writer] = obj2ary((h->list))->arry[reader++];
    if (fiobj_hash2_insert(h, obj2ary((h->list))->arry[writer],
                           obj2sym(obj2ary((h->list))->arry[writer])->hash))
      return -1;
    writer++;
  }
  return 0;
}

/* *****************************************************************************
Hash API
***************************************************************************** */
/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash2_new(void) {
  fiobj_head_s *head = malloc(sizeof(*head) + sizeof(fio_hash2_s));
  head->ref = 1;
  *((fio_hash2_s *)HEAD2OBJ(head)) = (fio_hash2_s){
      .type = FIOBJ_T_HASH /*type*/,
      .list = fiobj_ary_new(),
      .index = fiobj_ary_new(),
      .mask = 1023,
  };
  fio_hash2_s *h = (fio_hash2_s *)HEAD2OBJ(head);
  /* */
  /* setting the `start` to 0 promises memmove won't be called.*/
  obj2ary((h->index))->start = 0;
  obj2ary((h->index))->end = 0;
  obj2ary((h->list))->start = 0;
  obj2ary((h->list))->end = 0;
  return HEAD2OBJ(head);
}

/** Returns the number of elements in the Hash. */
size_t fiobj_hash2_count(fiobj_s *hash) {
  if (!hash || hash->type != FIOBJ_T_HASH)
    return 0;
  return (fiobj_ary_count(obj2hash2(hash)->list) - obj2hash2(hash)->removed);
}

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash2_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    fiobj_dealloc((fiobj_s *)obj);
    return -1;
  }
  fiobj_s *coup = fiobj_alloc(FIOBJ_T_COUPLET, 0, NULL);
  ((fio_couplet_s *)coup)->name = fiobj_dup(sym);
  ((fio_couplet_s *)coup)->obj = obj;
  if (fiobj_hash2_count(hash) >= (((obj2hash2(hash)->mask) << 1) / 3))
    goto force_rehash;
  while (fiobj_hash2_insert(obj2hash2(hash), coup, obj2sym(sym)->hash)) {
  force_rehash:
    obj2hash2(hash)->mask = (obj2hash2(hash)->mask << 1) | 1;
    fiobj_hash2_rehash(obj2hash2(hash));
  }
  return 0;
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash2_remove(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL)
    return NULL;
  fiobj_s **pos = fiobj_hash2_seek(obj2hash2(hash), obj2sym(sym)->hash);
  if (!pos)
    return NULL;
  fiobj_s *coup = *pos;
  if (!coup)
    return NULL;
  *pos = NULL;
  fiobj_s *ret = fiobj_couplet2obj(coup);
  fiobj_dealloc(((fio_couplet_s *)coup)->name);
  fiobj_dealloc((fiobj_s *)coup);
  return ret;
}

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash2_delete(fiobj_s *hash, fiobj_s *sym) {
  fiobj_s *obj = fiobj_hash2_remove(hash, sym);
  if (!obj)
    return -1;
  fiobj_free(obj);
  return 0;
}

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL
 * if none.
 */
fiobj_s *fiobj_hash2_get(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return NULL;
  }
  fiobj_s **pos = fiobj_hash2_seek(obj2hash2(hash), obj2sym(sym)->hash);
  if (!pos)
    return NULL;
  fiobj_s *coup = *pos;
  if (!coup)
    return NULL;
  return fiobj_couplet2obj(coup);
}

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash2_haskey(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return 0;
  }
  fiobj_s **pos = fiobj_hash2_seek(obj2hash2(hash), obj2sym(sym)->hash);
  if (!pos)
    return 0;
  fiobj_s *coup = *pos;
  if (!coup)
    return 0;
  return 1;
}
