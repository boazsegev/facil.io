/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

#include "fiobject.h"

#include "fiobj_hash.h"

#include <assert.h>

typedef struct {
  uint64_t hash;
  FIOBJ key;
} hash_key_s;

static hash_key_s hash_key_copy(hash_key_s key) {
  fiobj_dup(key.key);
  fiobj_str_freeze(key.key);
  return key;
}
static void hash_key_free(hash_key_s key) { fiobj_free(key.key); }

#define FIO_HASH_KEY_TYPE hash_key_s
#define FIO_HASH_KEY_INVALID ((hash_key_s){.hash = 0})
#define FIO_HASH_KEY2UINT(k) ((k).hash)
#define FIO_HASH_COMPARE_KEYS(k1, k2)                                          \
  (!(k2).key || fiobj_iseq((k1).key, (k2).key))
#define FIO_HASH_KEY_ISINVALID(k) ((k).hash == 0 && (k).key == 0)
#define FIO_HASH_KEY_COPY(k) hash_key_copy(k)
#define FIO_HASH_KEY_DESTROY(k) hash_key_free(k)

#include "fio_hashmap.h"

#include <errno.h>

/* *****************************************************************************
Hash types
***************************************************************************** */
typedef struct {
  fiobj_object_header_s head;
  fio_hash_s hash;
} fiobj_hash_s;

#define obj2hash(o) ((fiobj_hash_s *)(FIOBJ2PTR(o)))

void fiobj_hash_rehash(FIOBJ h) {
  assert(h && FIOBJ_TYPE_IS(h, FIOBJ_T_HASH));
  fio_hash_rehash(&obj2hash(h)->hash);
}

/* *****************************************************************************
Hash alloc + VTable
***************************************************************************** */

static void fiobj_hash_dealloc(FIOBJ o, void (*task)(FIOBJ, void *),
                               void *arg) {
  FIO_HASH_FOR_FREE(&obj2hash(o)->hash, i) { task((FIOBJ)i->obj, arg); }
  free(FIOBJ2PTR(o));
}

static __thread FIOBJ each_at_key = 0;

static size_t fiobj_hash_each1(FIOBJ o, const size_t start_at,
                               int (*task)(FIOBJ obj, void *arg), void *arg) {
  assert(o && FIOBJ_TYPE_IS(o, FIOBJ_T_HASH));
  FIOBJ old_each_at_key = each_at_key;
  fio_hash_s *hash = &obj2hash(o)->hash;
  size_t count = 0;
  if (hash->count == hash->pos) {
    /* no holes in the hash, we can work as we please. */
    for (count = start_at; count < hash->count; ++count) {
      each_at_key = hash->ordered[count].key.key;
      if (task((FIOBJ)hash->ordered[count].obj, arg) == -1) {
        ++count;
        goto end;
      }
    }
  } else {
    fio_hash_data_ordered_s *i;
    for (i = hash->ordered;
         count < start_at && i && !FIO_HASH_KEY_ISINVALID(i->key); ++i) {
      /* counting */
      if (!i->obj)
        continue;
      ++count;
    }
    for (; i && !FIO_HASH_KEY_ISINVALID(i->key); ++i) {
      /* performing */
      if (!i->obj)
        continue;
      ++count;
      each_at_key = i->key.key;
      if (task((FIOBJ)i->obj, arg) == -1)
        break;
    }
  }
end:
  each_at_key = old_each_at_key;
  return count;
}

FIOBJ fiobj_hash_key_in_loop(void) { return each_at_key; }

static size_t fiobj_hash_is_eq(const FIOBJ self, const FIOBJ other) {
  if (fio_hash_count(&obj2hash(self)->hash) !=
      fio_hash_count(&obj2hash(other)->hash))
    return 0;
  return 1;
}

/** Returns the number of elements in the Array. */
size_t fiobj_hash_count(const FIOBJ o) {
  assert(o && FIOBJ_TYPE_IS(o, FIOBJ_T_HASH));
  return fio_hash_count(&obj2hash(o)->hash);
}
static size_t fiobj_hash_is_true(const FIOBJ o) {
  return fiobj_hash_count(o) != 0;
}

fio_cstr_s fiobject___noop_to_str(FIOBJ o);
intptr_t fiobject___noop_to_i(FIOBJ o);
double fiobject___noop_to_f(FIOBJ o);

const fiobj_object_vtable_s FIOBJECT_VTABLE_HASH = {
    .class_name = "Hash",
    .dealloc = fiobj_hash_dealloc,
    .is_eq = fiobj_hash_is_eq,
    .count = fiobj_hash_count,
    .each = fiobj_hash_each1,
    .is_true = fiobj_hash_is_true,
    .to_str = fiobject___noop_to_str,
    .to_i = fiobject___noop_to_i,
    .to_f = fiobject___noop_to_f,
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
FIOBJ fiobj_hash_new(void) {
  fiobj_hash_s *h = malloc(sizeof(*h));
  if (!h) {
    perror("ERROR: fiobj hash couldn't allocate memory");
    exit(errno);
  }
  *h = (fiobj_hash_s){.head = {.ref = 1, .type = FIOBJ_T_HASH}};
  fio_hash_new(&h->hash);
  return (FIOBJ)h | FIOBJECT_HASH_FLAG;
}

/**
 * Returns a temporary theoretical Hash map capacity.
 * This could be used for testig performance and memory consumption.
 */
size_t fiobj_hash_capa(const FIOBJ hash) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  return fio_hash_capa(&obj2hash(hash)->hash);
}

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(FIOBJ hash, FIOBJ key, FIOBJ obj) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  hash_key_s k = {.hash = fiobj_obj2hash(key), .key = key};
  FIOBJ old = (FIOBJ)fio_hash_insert(&obj2hash(hash)->hash, k, (void *)obj);
  fiobj_free(old);
  return 0;
}

/**
 * Allows the Hash to be used as a stack.
 *
 * If a pointer `key` is provided, it will receive ownership of the key
 * (remember to free).
 *
 * Returns FIOBJ_INVALID on error.
 *
 * Returns and object if successful (remember to free).
 */
FIOBJ fiobj_hash_pop(FIOBJ hash, FIOBJ *key) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  hash_key_s k = {.hash = 0, .key = FIOBJ_INVALID};
  FIOBJ old = (FIOBJ)fio_hash_pop(&obj2hash(hash)->hash, &k);
  if (!old)
    return FIOBJ_INVALID;
  if (key)
    *key = k.key;
  else
    fiobj_free(k.key);
  return old;
}

/**
 * Replaces the value in a key-value pair, returning the old value (and it's
 * ownership) to the caller.
 *
 * A return value of NULL indicates that no previous object existed (but a new
 * key-value pair was created.
 *
 * Errors are silently ignored.
 */
FIOBJ fiobj_hash_replace(FIOBJ hash, FIOBJ key, FIOBJ obj) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  hash_key_s k = {.hash = fiobj_obj2hash(key), .key = key};
  FIOBJ old = (FIOBJ)fio_hash_insert(&obj2hash(hash)->hash, k, (void *)obj);
  return old;
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
FIOBJ fiobj_hash_remove(FIOBJ hash, FIOBJ key) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  return (FIOBJ)fio_hash_insert(
      &obj2hash(hash)->hash,
      (hash_key_s){.hash = fiobj_obj2hash(key), .key = key}, NULL);
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
FIOBJ fiobj_hash_remove2(FIOBJ hash, uint64_t hash_value) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  return (FIOBJ)fio_hash_insert(&obj2hash(hash)->hash,
                                (hash_key_s){.hash = hash_value}, NULL);
}

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(FIOBJ hash, FIOBJ key) {
  FIOBJ old = fiobj_hash_remove(hash, key);
  if (!old)
    return -1;
  fiobj_free(old);
  return 0;
}

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * This function takes a `uintptr_t` Hash value (see `fio_siphash`) to
 * perform a lookup in the HashMap, which is slightly faster than the other
 * variations.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete2(FIOBJ hash, uint64_t key_hash) {
  FIOBJ old = fiobj_hash_remove2(hash, key_hash);
  if (!old)
    return -1;
  fiobj_free(old);
  return 0;
}

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL
 * if none.
 */
FIOBJ fiobj_hash_get(const FIOBJ hash, FIOBJ key) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  return (FIOBJ)fio_hash_find(
      &obj2hash(hash)->hash,
      (hash_key_s){.hash = fiobj_obj2hash(key), .key = key});
}

/**
 * Returns a temporary handle to the object associated hashed key value.
 *
 * This function takes a `uintptr_t` Hash value (see `fio_siphash`) to
 * perform a lookup in the HashMap.
 *
 * Returns NULL if no object is asociated with this hashed key value.
 */
FIOBJ fiobj_hash_get2(const FIOBJ hash, uint64_t key_hash) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  return (FIOBJ)fio_hash_find(&obj2hash(hash)->hash, (hash_key_s){
                                                         .hash = key_hash,
                                                     });
}

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash_haskey(const FIOBJ hash, FIOBJ key) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  hash_key_s k = {.hash = fiobj_obj2hash(key), .key = key};
  return (FIOBJ)fio_hash_find(&obj2hash(hash)->hash, k) != 0;
}

/**
 * Empties the Hash.
 */
void fiobj_hash_clear(const FIOBJ hash) {
  assert(hash && FIOBJ_TYPE_IS(hash, FIOBJ_T_HASH));
  FIO_HASH_FOR_EMPTY(&obj2hash(hash)->hash, i) { fiobj_free((FIOBJ)i->obj); }
}

/* *****************************************************************************
Simple Tests
***************************************************************************** */

#if DEBUG
void fiobj_test_hash(void) {
  fprintf(stderr, "=== Testing Hash\n");
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  FIOBJ o = fiobj_hash_new();
  FIOBJ str_key = fiobj_str_new("Hello World!", 12);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_HASH), "Type identification error!\n");
  TEST_ASSERT(fiobj_hash_count(o) == 0, "Hash should be empty!\n");
  fiobj_hash_set(o, str_key, fiobj_true());
  TEST_ASSERT(fiobj_str_write(str_key, "should fail...", 13) == 0,
              "wrote to frozen string?");
  TEST_ASSERT(fiobj_obj2cstr(str_key).len == 12,
              "String was mutated (not frozen)!\n");
  TEST_ASSERT(fiobj_hash_get(o, str_key) == fiobj_true(),
              "full compare didn't get value back");
  TEST_ASSERT(fiobj_hash_get2(o, fiobj_obj2hash(str_key)) == fiobj_true(),
              "hash compare didn't get value back");
  fiobj_hash_delete(o, str_key);
  TEST_ASSERT(fiobj_hash_get2(o, fiobj_obj2hash(str_key)) == 0,
              "item wasn't deleted!");
  fiobj_free(
      str_key); /* note that a copy will remain in the Hash until rehashing. */
  fiobj_free(o);
  fprintf(stderr, "* passed.\n");
}
#endif
