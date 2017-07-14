/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

#include <errno.h>
/* *****************************************************************************
Internal Map Array
We avoid the fiobj_ary_s to prevent entanglement
***************************************************************************** */

static inline void fio_map_reset(fio_map_s *map, uintptr_t capa) {
  /* It's better to reallocate using calloc than manually zero out memory */
  /* Maybe there's enough zeroed out pages available in the system */
  /* capacity should be page alighned */
  map->capa = ((((capa) >> 12) + 1) << 12);
  free(map->data);
  map->data = calloc(sizeof(*map->data), map->capa);
  if (!map->data)
    perror("HashMap Allocation Failed"), exit(errno);
}

/* *****************************************************************************
Internal HashMap
***************************************************************************** */
static inline uintptr_t fio_map_cuckoo_steps(uintptr_t step) {
  // return ((step * (step + 1)) >> 1);
  return (step * 3);
}

/* seeks the hash's position in the map */
static map_info_s *fio_hash_seek(fio_hash_s *h, uintptr_t hash) {
  /* TODO: consider implementing Robing Hood reordering during seek */
  map_info_s *pos = h->map.data + (hash & h->mask);
  map_info_s *end = pos + fio_map_cuckoo_steps(FIOBJ_HASH_MAX_MAP_SEEK);
  uintptr_t i = 0;
  while (pos < end) {
    if (!pos->hash || pos->hash == hash)
      return pos;
    pos = h->map.data +
          (((hash & h->mask) + fio_map_cuckoo_steps(i++)) & h->mask);
  }
  return NULL;
}

/* finds an object in the map */
static void *fio_hash_find(fio_hash_s *h, uintptr_t hash) {
  map_info_s *info = fio_hash_seek(h, hash);
  if (!info || !info->container)
    return NULL;
  return info->container->obj;
}

/* inserts an object to the map, rehashing if required, returning old object.
 * set obj to NULL to remove existing data.
 */
static void *fio_hash_insert(fio_hash_s *h, uintptr_t hash, void *obj) {
  map_info_s *info = fio_hash_seek(h, hash);
  if (!info)
    return (void *)(-1);
  if (!info->container) {
    /* a fresh object */
    if (obj == NULL)
      return NULL; /* nothing to delete */
    /* create container and set hash */
    fio_ls_unshift(&h->items, obj);
    *info = (map_info_s){.hash = hash, .container = h->items.prev};
    h->count++;
    return NULL;
  }
  /* a container object exists, this is a "replace/delete" operation */
  if (!obj) {
    /* delete */
    h->count--;
    obj = fio_ls_remove(info->container);
    *info = (map_info_s){.hash = hash}; /* hash is set to seek over position */
    return obj;
  }
  /* replace */
  void *old = info->container->obj;
  info->container->obj = obj;
  return old;
}

/* attempts to rehash the hashmap. */
void fiobj_hash_rehash(fiobj_s *h_) {
  fio_hash_s *h = obj2hash(h_);
// fprintf(stderr,
//         "- Rehash with "
//         "length/capacity == %lu/%lu\n",
//         h->count, h->map.capa);
retry_rehashing:
  h->mask = ((h->mask) << 1) | 1;
  fio_map_reset(&h->map, h->mask);
  fio_ls_s *pos = h->items.next;
  while (pos != &h->items) {
    /* can't use fio_hash_insert, because we're recycling containers */
    uintptr_t pos_hash = obj2sym(obj2couplet(pos->obj)->name)->hash;
    map_info_s *info = fio_hash_seek(h, pos_hash);
    if (!info) {
      goto retry_rehashing;
    }
    *info = (map_info_s){.hash = pos_hash, .container = pos};
    pos = pos->next;
  }
}

/* *****************************************************************************
Hash API
***************************************************************************** */
#define obj2hash3(o) ((fio_hash_s *)(o))
/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void) {
  fiobj_head_s *head = malloc(sizeof(*head) + sizeof(fio_hash_s));
  head->ref = 1;
  *((fio_hash_s *)HEAD2OBJ(head)) = (fio_hash_s){
      .type = FIOBJ_T_HASH,
      .mask = 1023,
      .items = FIO_LS_INIT((((fio_hash_s *)HEAD2OBJ(head))->items)),
  };
  fio_map_reset(&obj2hash3(HEAD2OBJ(head))->map,
                obj2hash3(HEAD2OBJ(head))->mask);
  return HEAD2OBJ(head);
}
void fiobj_hash_free(fiobj_s *h) {
  fiobj_s *coup;
  while ((coup = fio_ls_pop(&obj2hash3(h)->items))) {
    /* these deallocations go inside the `each2` */
    fiobj_free(obj2couplet(coup)->obj);
    fiobj_dealloc(obj2couplet(coup)->name);
    fiobj_dealloc(coup);
  }
  free(obj2hash3(h)->map.data);
  obj2hash3(h)->map.data = NULL;
  obj2hash3(h)->map.capa = 0;
  free(&OBJ2HEAD(h));
}

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(fiobj_s *hash) {
  if (!hash || hash->type != FIOBJ_T_HASH)
    return 0;
  return obj2hash3(hash)->count;
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
  // TODO: shoule we use preemptive rehashing? - NO! has negative impact!
  /*
  if (obj2hash3(hash)->count >= (((obj2hash3(hash)->mask + 1) << 1) / 3))
    fiobj_hash_rehash(hash);
  */
  fiobj_s *coup = fiobj_alloc(FIOBJ_T_COUPLET, 0, NULL);
  ((fio_couplet_s *)coup)->name = fiobj_dup(sym);
  ((fio_couplet_s *)coup)->obj = obj;
  fiobj_s *old = fio_hash_insert((fio_hash_s *)hash, obj2sym(sym)->hash, coup);
  while (old == (void *)-1) {
    fiobj_hash_rehash(hash);
    old = fio_hash_insert((fio_hash_s *)hash, obj2sym(sym)->hash, coup);
    // fprintf(stderr, "WARN: (fiobj Hash) collision limit reached"
    //                 " - forced rehashing\n");
  }
  if (old) {
    fiobj_free(obj2couplet(old)->obj);
    fiobj_dealloc(obj2couplet(old)->name);
    fiobj_dealloc(old);
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
  fiobj_s *coup = fio_hash_insert((fio_hash_s *)hash, obj2sym(sym)->hash, NULL);
  if (!coup)
    return NULL;
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
  fiobj_s *coup = fio_hash_find((fio_hash_s *)hash, obj2sym(sym)->hash);
  if (!coup)
    return NULL;
  return fiobj_couplet2obj(coup);
}

/**
 * Returns a temporary handle to the object associated with the Symbol C string.
 *
 * This function takes a C string instead of a Symbol, which is slower if a
 * Symbol can be cached but faster if a Symbol must be created.
 *
 * Returns NULL if no object is asociated with this String data.
 */
fiobj_s *fiobj_hash_get2(fiobj_s *hash, const char *str, size_t len) {
  if (hash->type != FIOBJ_T_HASH || str == NULL) {
    return NULL;
  }
  uintptr_t hashed_sym = fiobj_sym_hash(str, len);
  fiobj_s *coup = fio_hash_find((fio_hash_s *)hash, hashed_sym);
  if (!coup)
    return NULL;
  return fiobj_couplet2obj(coup);
}

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash_haskey(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return 0;
  }
  fiobj_s *coup = fio_hash_find((fio_hash_s *)hash, obj2sym(sym)->hash);
  if (!coup)
    return 0;
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
