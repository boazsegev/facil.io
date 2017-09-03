/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* MUST be a power of 2 */
#define HASH_INITIAL_CAPACITY 16

#include <errno.h>
/* *****************************************************************************
Internal Map Array
We avoid the fiobj_ary_s to prevent entanglement
***************************************************************************** */

static inline void fio_map_reset(fio_map_s *map, uintptr_t capa) {
  /* It's better to reallocate using calloc than manually zero out memory */
  /* Maybe there's enough zeroed out pages available in the system */
  map->capa = capa;
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
  uintptr_t i = 0;
  const uintptr_t limit = h->map.capa > FIOBJ_HASH_MAX_MAP_SEEK
                              ? FIOBJ_HASH_MAX_MAP_SEEK
                              : (h->map.capa >> 1);
  while (i < limit) {
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
  fio_map_reset(&h->map, h->mask + 1);
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
Couplet alloc + Couplet VTable
***************************************************************************** */

static void fiobj_couplet_dealloc(fiobj_s *o) {
  fiobj_dealloc(obj2couplet(o)->name);
  fiobj_dealloc(obj2couplet(o)->obj);
  free(&OBJ2HEAD(o));
}

static size_t fiobj_couplet_each1(fiobj_s *o, size_t start_at,
                                  int (*task)(fiobj_s *obj, void *arg),
                                  void *arg) {
  if (obj2couplet(o)->obj == NULL)
    return 0;
  return OBJ2HEAD(obj2couplet(o)->obj)
      .vtable->each1(obj2couplet(o)->obj, start_at, task, arg);
}

static int fiobj_coup_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != FIOBJ_T_COUPLET)
    return 0;
  OBJ2HEAD(obj2couplet(self)->name)
      .vtable->is_eq(obj2couplet(self)->name, obj2couplet(other)->name);
  if (obj2couplet(self)->obj == obj2couplet(other)->obj)
    return 1;
  if (!obj2couplet(self)->obj || !obj2couplet(other)->obj)
    return 0;
  return OBJ2HEAD(obj2couplet(self)->obj)
      .vtable->is_eq(obj2couplet(self)->obj, obj2couplet(other)->obj);
}

/** Returns the number of elements in the Array. */
static size_t fiobj_couplet_count_items(fiobj_s *o) {
  if (obj2couplet(o)->obj == NULL)
    return 0;
  return OBJ2HEAD(obj2couplet(o)->obj).vtable->count(obj2couplet(o)->obj);
}

static struct fiobj_vtable_s FIOBJ_VTABLE_COUPLET = {
    .free = fiobj_couplet_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fiobj_noop_str,
    .is_eq = fiobj_coup_is_eq,
    .count = fiobj_couplet_count_items,
    .each1 = fiobj_couplet_each1,
};

static inline fiobj_s *fiobj_couplet_alloc(void *sym, void *obj) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_couplet_s));
  if (!head)
    perror("ERROR: fiobj hash couldn't allocate couplet"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_COUPLET,
  };
  *(fio_couplet_s *)(HEAD2OBJ(head)) =
      (fio_couplet_s){.type = FIOBJ_T_COUPLET, .name = sym, .obj = obj};
  return HEAD2OBJ(head);
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

/* *****************************************************************************
Hash alloc + VTable
***************************************************************************** */

static void fiobj_hash_dealloc(fiobj_s *h) {
  while (fio_ls_pop(&obj2hash(h)->items))
    ;
  free(obj2hash(h)->map.data);
  obj2hash(h)->map.data = NULL;
  obj2hash(h)->map.capa = 0;
  free(&OBJ2HEAD(h));
}

static size_t fiobj_hash_each1(fiobj_s *o, const size_t start_at,
                               int (*task)(fiobj_s *obj, void *arg),
                               void *arg) {
  if (start_at >= obj2hash(o)->count)
    return obj2hash(o)->count;
  size_t i = 0;
  fio_ls_s *pos = obj2hash(o)->items.next;
  while (pos != &obj2hash(o)->items && start_at > i) {
    pos = pos->next;
    ++i;
  }
  while (pos != &obj2hash(o)->items) {
    ++i;
    if (task(pos->obj, arg) == -1)
      return i;
    pos = pos->next;
  }
  return i;
}

static int fiobj_hash_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != FIOBJ_T_HASH)
    return 0;
  if (obj2hash(self)->count != obj2hash(other)->count)
    return 0;
  fio_ls_s *pos = obj2hash(self)->items.next;
  while (pos != &obj2hash(self)->items) {
    if (!fio_hash_find((fio_hash_s *)other,
                       obj2sym(obj2couplet(pos->obj)->name)->hash))
      return 0;
    pos = pos->next;
  }
  return 1;
}

/** Returns the number of elements in the Array. */
static size_t fiobj_hash_count_items(fiobj_s *o) { return obj2hash(o)->count; }

static struct fiobj_vtable_s FIOBJ_VTABLE_HASH = {
    .free = fiobj_hash_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fiobj_noop_str,
    .is_eq = fiobj_hash_is_eq,
    .count = fiobj_hash_count_items,
    .each1 = fiobj_hash_each1,
};

/* *****************************************************************************
Hash API
***************************************************************************** */

/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void) {
  fiobj_head_s *head = malloc(sizeof(*head) + sizeof(fio_hash_s));
  if (!head)
    perror("ERROR: fiobj hash couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_HASH,
  };
  *obj2hash(HEAD2OBJ(head)) = (fio_hash_s){
      .type = FIOBJ_T_HASH,
      .mask = (HASH_INITIAL_CAPACITY - 1),
      .items = FIO_LS_INIT((obj2hash(HEAD2OBJ(head))->items)),
      .map.data = calloc(sizeof(map_info_s), HASH_INITIAL_CAPACITY),
      .map.capa = HASH_INITIAL_CAPACITY,
  };
  // fio_map_reset(&obj2hash(HEAD2OBJ(head))->map,
  // obj2hash(HEAD2OBJ(head))->mask);
  return HEAD2OBJ(head);
}

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(fiobj_s *hash) {
  if (!hash || hash->type != FIOBJ_T_HASH)
    return 0;
  return obj2hash(hash)->count;
}

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj) {
  if (hash->type != FIOBJ_T_HASH) {
    fiobj_dealloc((fiobj_s *)obj);
    return -1;
  }
  switch (sym->type) {
  case FIOBJ_T_SYMBOL:
    sym = fiobj_dup(sym);
    break;
  case FIOBJ_T_STRING:
    sym = fiobj_sym_new(obj2str(sym)->str, obj2str(sym)->len);
    break;
  default:
    fiobj_dealloc((fiobj_s *)obj);
    return -1;
  }

  fiobj_s *coup = fiobj_couplet_alloc(sym, obj);
  fiobj_s *old = fio_hash_insert((fio_hash_s *)hash, obj2sym(sym)->hash, coup);
  while (old == (void *)-1) {
    fiobj_hash_rehash(hash);
    old = fio_hash_insert((fio_hash_s *)hash, obj2sym(sym)->hash, coup);
    // fprintf(stderr, "WARN: (fiobj Hash) collision limit reached"
    //                 " - forced rehashing\n");
  }
  if (old) {
    fiobj_free(obj2couplet(old)->obj);
    obj2couplet(old)->obj = NULL;
    fiobj_dealloc(old);
  }
  return 0;
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash_remove(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH) {
    return 0;
  }
  uintptr_t hash_value = 0;
  switch (sym->type) {
  case FIOBJ_T_SYMBOL:
    hash_value = obj2sym(sym)->hash;
    break;
  case FIOBJ_T_STRING:
    hash_value = fiobj_sym_hash(obj2str(sym)->str, obj2str(sym)->len);
    break;
  default:
    return 0;
  }
  fiobj_s *coup = fio_hash_insert((fio_hash_s *)hash, hash_value, NULL);
  if (!coup)
    return NULL;
  fiobj_s *ret = fiobj_couplet2obj(coup);
  obj2couplet(coup)->obj = NULL;
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
  if (hash->type != FIOBJ_T_HASH) {
    return 0;
  }
  uintptr_t hash_value = 0;
  switch (sym->type) {
  case FIOBJ_T_SYMBOL:
    hash_value = obj2sym(sym)->hash;
    break;
  case FIOBJ_T_STRING:
    hash_value = fiobj_sym_hash(obj2str(sym)->str, obj2str(sym)->len);
    break;
  default:
    return 0;
  }
  fiobj_s *coup = fio_hash_find((fio_hash_s *)hash, hash_value);
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
  if (hash->type != FIOBJ_T_HASH) {
    return 0;
  }
  uintptr_t hash_value = 0;
  switch (sym->type) {
  case FIOBJ_T_SYMBOL:
    hash_value = obj2sym(sym)->hash;
    break;
  case FIOBJ_T_STRING:
    hash_value = fiobj_sym_hash(obj2str(sym)->str, obj2str(sym)->len);
    break;
  default:
    return 0;
  }

  fiobj_s *coup = fio_hash_find((fio_hash_s *)hash, hash_value);
  if (!coup)
    return 0;
  return 1;
}
