#ifndef H_FIO_SIMPLE_HASH_H
/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

/**
 * A simple ordered Hash Table implementation, with a minimal API and zero hash
 * collision protection.
 *
 * Unique keys are required. Full key collisions aren't handled, instead the old
 * value is replaced and returned.
 *
 * Partial key collisions are handled by seeking forward and attempting to find
 * a close enough spot. If a close enough spot isn't found, rehashing is
 * initiated and memory consumption increases.
 *
 * The Hash Table is ordered using an internal ordered array of data containers
 * with duplicates of the key data (to improve cache locality).
 *
 * The file was written to be compatible with C++ as well as C, hence some
 * pointer casting.
 */
#define H_FIO_SIMPLE_HASH_H

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
#include <string.h>

/**
 * extra collision protection can be obtained by defining ALL of the following:
 * * FIO_HASH_KEY_TYPE              - the type used for keys.
 * * FIO_HASH_KEY_INVALID         - an invalid key with it's bytes set to zero.
 * * FIO_HASH_KEY2UINT(key)         - converts a key to a hash value number.
 * * FIO_HASH_COMPARE_KEYS(k1, k2)  - compares two key.
 * * FIO_HASH_KEY_ISINVALID(key)    - tests for an invalid key.
 * * FIO_HASH_KEY_COPY(key)         - creates a persistent copy of the key.
 * * FIO_HASH_KEY_DESTROY(key)      - destroys (or frees) the key's copy.
 *
 * Note: FIO_HASH_COMPARE_KEYS will be used to compare against
 *       FIO_HASH_KEY_INVALID as well as valid keys.
 *
 * Note: Before freeing the Hash, FIO_HASH_KEY_DESTROY should be called for
 *       every key. This is NOT automatic. see the FIO_HASH_FOR_EMPTY(h) macro.
 */
#if !defined(FIO_HASH_COMPARE_KEYS) || !defined(FIO_HASH_KEY_TYPE) ||          \
    !defined(FIO_HASH_KEY2UINT) || !defined(FIO_HASH_KEY_INVALID) ||           \
    !defined(FIO_HASH_KEY_ISINVALID) || !defined(FIO_HASH_KEY_COPY) ||         \
    !defined(FIO_HASH_KEY_DESTROY)
#define FIO_HASH_KEY_TYPE uint64_t
#define FIO_HASH_KEY_INVALID 0
#define FIO_HASH_KEY2UINT(key) (key)
#define FIO_HASH_COMPARE_KEYS(k1, k2) ((k1) == (k2))
#define FIO_HASH_KEY_ISINVALID(key) ((key) == 0)
#define FIO_HASH_KEY_COPY(key) (key)
#define FIO_HASH_KEY_DESTROY(key) ((void)0)
#elif !defined(FIO_HASH_NO_TEST)
#define FIO_HASH_NO_TEST 1
#endif

#ifndef FIO_HASH_INITIAL_CAPACITY
/* MUST be a power of 2 */
#define FIO_HASH_INITIAL_CAPACITY 4
#endif

#ifndef FIO_HASH_MAX_MAP_SEEK
/* MUST be a power of 2 */
#define FIO_HASH_MAX_MAP_SEEK (256)
#endif

#ifndef FIO_HASH_REALLOC /* NULL ptr indicates new allocation */
#define FIO_HASH_REALLOC(ptr, original_size, new_size, valid_data_length)      \
  realloc((ptr), (new_size))
#endif
#ifndef FIO_HASH_CALLOC
#define FIO_HASH_CALLOC(size, count) calloc((size), (count))
#endif
#ifndef FIO_HASH_FREE
#define FIO_HASH_FREE(ptr, size) free((ptr))
#endif

/* *****************************************************************************
Hash API
***************************************************************************** */

/** The Hash Table container type. */
typedef struct fio_hash_s fio_hash_s;

/** Allocates and initializes internal data and resources. */
FIO_FUNC void fio_hash_new(fio_hash_s *hash);

/** Allocates and initializes internal data and resources with the requested
 * capacity. */
FIO_FUNC void fio_hash_new2(fio_hash_s *hash, size_t capa);

/** Deallocates any internal resources. */
FIO_FUNC void fio_hash_free(fio_hash_s *hash);

/** Locates an object in the Hash Map Table according to the hash key value. */
FIO_FUNC inline void *fio_hash_find(fio_hash_s *hash, FIO_HASH_KEY_TYPE key);

/**
 * Inserts an object to the Hash Map Table, rehashing if required, returning the
 * old object if it exists.
 *
 * Set obj to NULL to remove an existing data (the existing object will be
 * returned).
 */
FIO_FUNC void *fio_hash_insert(fio_hash_s *hash, FIO_HASH_KEY_TYPE key,
                               void *obj);

/**
 * Allows the Hash to be momenterally used as a stack, poping the last element
 * entered.
 *
 * If a pointer to `key` is provided, the element's key will be placed in it's
 * place.
 *
 * Remember that keys are likely to be freed as well (`FIO_HASH_KEY_DESTROY`).
 */
FIO_FUNC void *fio_hash_pop(fio_hash_s *hash, FIO_HASH_KEY_TYPE *key);

/**
 * Allows a peak at the Hash's last element.
 *
 * If a pointer to `key` is provided, the element's key will be placed in it's
 * place.
 *
 * Remember that keys might be destroyed if the Hash is altered
 * (`FIO_HASH_KEY_DESTROY`).
 */
FIO_FUNC void *fio_hash_last(fio_hash_s *hash, FIO_HASH_KEY_TYPE *key);

/** Returns the number of elements currently in the Hash Table. */
FIO_FUNC inline size_t fio_hash_count(const fio_hash_s *hash);

/**
 * Returns a temporary theoretical Hash map capacity.
 * This could be used for testing performance and memory consumption.
 */
FIO_FUNC inline size_t fio_hash_capa(const fio_hash_s *hash);

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Hash Map.
 *
 * Returns the updated hash map capacity.
 */
FIO_FUNC inline size_t fio_hash_compact(fio_hash_s *hash);

/** Forces a rehashing of the hash. */
FIO_FUNC void fio_hash_rehash(fio_hash_s *hash);

/**
 * Iteration using a callback for each entry in the Hash Table.
 *
 * The callback task function must accept the hash key, the entry data and an
 * opaque user pointer:
 *
 *     int example_task(FIO_HASH_KEY_TYPE key, void *obj, void *arg) {return 0;}
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
FIO_FUNC inline size_t fio_hash_each(fio_hash_s *hash, const size_t start_at,
                                     int (*task)(FIO_HASH_KEY_TYPE key,
                                                 void *obj, void *arg),
                                     void *arg);

/**
 * A macro for a `for` loop that iterates over all the hashed objects (in
 * order).
 *
 * `hash` a pointer to the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 */
#define FIO_HASH_FOR_LOOP(hash, i)

/**
 * A macro for a `for` loop that will iterate over all the hashed objects (in
 * order) and empties the hash, later calling `fio_hash_free` to free the hash
 * (but not the container).
 *
 * `hash` a pointer to the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 *
 * Free the objects and the Hash Map container manually (if required). Custom
 * keys will be freed automatically when using this macro.
 *
 */
#define FIO_HASH_FOR_FREE(hash, i)

/**
 * A macro for a `for` loop that iterates over all the hashed objects (in
 * order) and empties the hash.
 *
 * This will also reallocate the map's memory (to zero out the data), so if this
 * is performed before calling `fio_hash_free`, use FIO_HASH_FOR_FREE instead.
 *
 * `hash` a pointer to the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 *
 * Free the objects and the Hash Map container manually (if required). Custom
 * keys will be freed automatically when using this macro.
 *
 */
#define FIO_HASH_FOR_EMPTY(hash, i)

/* *****************************************************************************
Hash Table Internal Data Structures
***************************************************************************** */

typedef struct fio_hash_data_ordered_s {
  FIO_HASH_KEY_TYPE key; /* another copy for memory cache locality */
  void *obj;
} fio_hash_data_ordered_s;

typedef struct fio_hash_data_s {
  FIO_HASH_KEY_TYPE key; /* another copy for memory cache locality */
  struct fio_hash_data_ordered_s *obj;
} fio_hash_data_s;

/* the information in tjhe Hash Map structure should be considered READ ONLY. */
struct fio_hash_s {
  uintptr_t count;
  uintptr_t capa;
  uintptr_t pos;
  uintptr_t mask;
  fio_hash_data_ordered_s *ordered;
  fio_hash_data_s *map;
};

#undef FIO_HASH_FOR_LOOP
#define FIO_HASH_FOR_LOOP(hash, container)                                     \
  for (fio_hash_data_ordered_s *container = (hash)->ordered;                   \
       container && (container < (hash)->ordered + (hash)->pos); ++container)

#undef FIO_HASH_FOR_FREE
#define FIO_HASH_FOR_FREE(hash, container)                                     \
  for (fio_hash_data_ordered_s *container = (hash)->ordered;                   \
       (container && container >= (hash)->ordered &&                           \
        (container < (hash)->ordered + (hash)->pos)) ||                        \
       ((fio_hash_free(hash), (hash)->ordered) != NULL);                       \
       FIO_HASH_KEY_DESTROY(container->key), (++container))

#undef FIO_HASH_FOR_EMPTY
#define FIO_HASH_FOR_EMPTY(hash, container)                                    \
  for (fio_hash_data_ordered_s *container = (hash)->ordered;                   \
       (container && (container < (hash)->ordered + (hash)->pos)) ||           \
       (memset((hash)->map, 0, (hash)->capa * sizeof(*(hash)->map)),           \
        ((hash)->pos = (hash)->count = 0));                                    \
       (FIO_HASH_KEY_DESTROY(container->key),                                  \
        container->key = FIO_HASH_KEY_INVALID, container->obj = NULL),         \
                               (++container))
#define FIO_HASH_INIT                                                          \
  { .capa = 0 }

/* *****************************************************************************
Hash allocation / deallocation.
***************************************************************************** */

/** Allocates and initializes internal data and resources with the requested
 * capacity. */
FIO_FUNC void fio_hash__new__internal__safe_capa(fio_hash_s *h, size_t capa) {
  *h = (fio_hash_s){
      .mask = (capa - 1),
      .map = (fio_hash_data_s *)FIO_HASH_CALLOC(sizeof(*h->map), capa),
      .ordered =
          (fio_hash_data_ordered_s *)FIO_HASH_CALLOC(sizeof(*h->ordered), capa),
      .capa = capa,
  };
  if (!h->map || !h->ordered) {
    perror("ERROR: Hash Table couldn't allocate memory");
    exit(errno);
  }
  h->ordered[0] =
      (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
}

/** Allocates and initializes internal data and resources with the requested
 * capacity. */
FIO_FUNC void fio_hash_new2(fio_hash_s *h, size_t capa) {
  size_t act_capa = 1;
  while (act_capa < capa)
    act_capa = act_capa << 1;
  fio_hash__new__internal__safe_capa(h, act_capa);
}

FIO_FUNC void fio_hash_new(fio_hash_s *h) {
  fio_hash__new__internal__safe_capa(h, FIO_HASH_INITIAL_CAPACITY);
}

FIO_FUNC void fio_hash_free(fio_hash_s *h) {
  FIO_HASH_FREE(h->map, h->capa);
  FIO_HASH_FREE(h->ordered, h->capa);
  *h = (fio_hash_s){.map = NULL};
}

/* *****************************************************************************
Internal HashMap Functions
***************************************************************************** */
FIO_FUNC inline uintptr_t fio_hash_map_cuckoo_steps(uintptr_t step) {
  return (step * 3);
}

/* seeks the hash's position in the map */
FIO_FUNC fio_hash_data_s *fio_hash_seek_pos_(fio_hash_s *hash,
                                             FIO_HASH_KEY_TYPE key) {
  /* TODO: consider implementing Robing Hood reordering during seek? */
  fio_hash_data_s *pos = hash->map + (FIO_HASH_KEY2UINT(key) & hash->mask);
  uintptr_t i = 0;
  const uintptr_t limit = hash->capa > FIO_HASH_MAX_MAP_SEEK
                              ? FIO_HASH_MAX_MAP_SEEK
                              : ((hash->capa >> 1) | 1);
  while (i < limit) {
    if (FIO_HASH_KEY_ISINVALID(pos->key) ||
        (FIO_HASH_KEY2UINT(pos->key) == FIO_HASH_KEY2UINT(key) &&
         FIO_HASH_COMPARE_KEYS(pos->key, key)))
      return pos;
    pos = hash->map + (((FIO_HASH_KEY2UINT(key) & hash->mask) +
                        fio_hash_map_cuckoo_steps(i++)) &
                       hash->mask);
  }
  return NULL;
}

/* finds an object in the map */
FIO_FUNC inline void *fio_hash_find(fio_hash_s *hash, FIO_HASH_KEY_TYPE key) {
  if (!hash->map)
    return NULL;
  fio_hash_data_s *info = fio_hash_seek_pos_(hash, key);
  if (!info || !info->obj)
    return NULL;
  return (void *)info->obj->obj;
}

/* inserts an object to the map, rehashing if required, returning old object.
 * set obj to NULL to remove existing data.
 */
FIO_FUNC void *fio_hash_insert(fio_hash_s *hash, FIO_HASH_KEY_TYPE key,
                               void *obj) {
  /* ensure some space */
  if (obj && hash->pos >= hash->capa)
    fio_hash_rehash(hash);

  /* find where the object belongs in the map */
  fio_hash_data_s *info = fio_hash_seek_pos_(hash, key);
  if (!info && !obj)
    return NULL;
  while (!info) {
    fio_hash_rehash(hash);
    info = fio_hash_seek_pos_(hash, key);
  }

  if (!info->obj) {
    /* a fresh object */

    if (obj == NULL) {
      /* nothing to delete */
      return NULL;
    }

    /* add object to ordered hash */
    hash->ordered[hash->pos] =
        (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_COPY(key), .obj = obj};

    /* add object to map */
    *info = (fio_hash_data_s){.key = hash->ordered[hash->pos].key,
                              .obj = hash->ordered + hash->pos};

    /* manage counters and mark end position */
    hash->count++;
    hash->pos++;
    return NULL;
  }

  if (!obj && !info->obj->obj) {
    /* a delete operation for an empty element */
    return NULL;
  }

  /* an object exists, this is a "replace/delete" operation */
  const void *old = (void *)info->obj->obj;

  if (!obj) {
    /* it was a delete operation */
    if (info->obj == hash->ordered + hash->pos - 1) {
      /* we removed the last ordered element, no need to keep any holes. */
      --hash->pos;
      FIO_HASH_KEY_DESTROY(hash->ordered[hash->pos].key);
      hash->ordered[hash->pos] =
          (fio_hash_data_ordered_s){.obj = NULL, .key = FIO_HASH_KEY_INVALID};
      *info = (fio_hash_data_s){.obj = NULL, .key = FIO_HASH_KEY_INVALID};
      if (hash->pos && !hash->ordered[hash->pos - 1].obj) {
        fio_hash_pop(hash, NULL);
      } else {
        --hash->count;
      }

      return (void *)old;
    }
    --hash->count;
  } else if (!old) {
    /* inserted an item after a previous one was removed. */
    ++hash->count;
  }
  info->obj->obj = obj;

  return (void *)old;
}

/**
 * Allows the Hash to be momenterally used as a stack, poping the last element
 * entered.
 * Remember that keys might have to be freed as well (`FIO_HASH_KEY_DESTROY`).
 */
FIO_FUNC void *fio_hash_pop(fio_hash_s *hash, FIO_HASH_KEY_TYPE *key) {
  if (!hash->pos)
    return NULL;
  --(hash->pos);
  --(hash->count);
  void *old = hash->ordered[hash->pos].obj;
  /* removing hole from hashtable is possible because it's the last element */
  fio_hash_data_s *info =
      fio_hash_seek_pos_(hash, hash->ordered[hash->pos].key);
  if (!info) {
    /* no info is a data corruption error. */
    fprintf(stderr, "FATAL ERROR: (fio_hash) unexpected missing container.\n");
    exit(-1);
  }
  *info = (fio_hash_data_s){.obj = NULL};
  /* cleanup key (or copy to target) and reset the ordered position. */
  if (key)
    *key = hash->ordered[hash->pos].key;
  else
    FIO_HASH_KEY_DESTROY(hash->ordered[hash->pos].key);
  hash->ordered[hash->pos] =
      (fio_hash_data_ordered_s){.obj = NULL, .key = FIO_HASH_KEY_INVALID};
  /* remove any holes from the top (top is kept tight) */
  while (hash->pos && hash->ordered[hash->pos - 1].obj == NULL) {
    --(hash->pos);
    info = fio_hash_seek_pos_(hash, hash->ordered[hash->pos].key);
    if (!info) {
      /* no info is a data corruption error. */
      fprintf(stderr,
              "FATAL ERROR: (fio_hash) unexpected missing container (2).\n");
      exit(-1);
    }
    *info = (fio_hash_data_s){.obj = NULL};
    FIO_HASH_KEY_DESTROY(hash->ordered[hash->pos].key);
    hash->ordered[hash->pos] =
        (fio_hash_data_ordered_s){.obj = NULL, .key = FIO_HASH_KEY_INVALID};
  }
  return old;
}

/**
 * Allows a peak at the Hash's last element.
 *
 * If a pointer to `key` is provided, the element's key will be placed in it's
 * place.
 *
 * Remember that keys might be destroyed if the Hash is altered
 * (`FIO_HASH_KEY_DESTROY`).
 */
FIO_FUNC void *fio_hash_last(fio_hash_s *hash, FIO_HASH_KEY_TYPE *key) {
  if (key)
    *key = hash->ordered[hash->pos - 1].key;
  return hash->ordered[hash->pos - 1].obj;
}

/* attempts to rehash the hashmap. */
FIO_FUNC void fio_hash_rehash(fio_hash_s *h) {
  if (!h->capa) /* lazy initialization */
    h->mask = FIO_HASH_INITIAL_CAPACITY - 1;
retry_rehashing:
  h->mask = ((h->mask) << 1) | 1;
  {
    /* It's better to reallocate using calloc than manually zero out memory */
    /* Maybe there's enough zeroed out pages available in the system */
    FIO_HASH_FREE(h->map, h->capa);
    h->capa = h->mask + 1;
    h->map = (fio_hash_data_s *)FIO_HASH_CALLOC(sizeof(*h->map), h->capa);
    if (!h->map) {
      perror("HashMap Allocation Failed");
      exit(errno);
    }
    /* the ordered list doesn't care about initialized memory, so realloc */
    /* will be faster. */
    h->ordered = (fio_hash_data_ordered_s *)(FIO_HASH_REALLOC(
        h->ordered, ((h->capa >> 1) * sizeof(*h->ordered)),
        ((h->capa) * sizeof(*h->ordered)), ((h->pos) * sizeof(*h->ordered))));
    if (!h->ordered) {
      perror("HashMap Reallocation Failed");
      exit(errno);
    }
  }
  if (!h->count) {
    /* empty hash */
    return;

  } else if (h->pos == h->count) {
    /* the ordered list is fully occupied, no need to rearange. */
    FIO_HASH_FOR_LOOP(h, i) {
      /* can't use fio_hash_insert, because we're recycling containers */
      fio_hash_data_s *place = fio_hash_seek_pos_(h, i->key);
      if (!place) {
        goto retry_rehashing;
      }
      *place = (fio_hash_data_s){.key = i->key, .obj = i};
    }

  } else {
    /* the ordered list has holes, fill 'em up.*/
    size_t reader = 0;
    size_t writer = 0;
    while (reader < h->pos) {
      if (h->ordered[reader].obj) {
        fio_hash_data_s *place = fio_hash_seek_pos_(h, h->ordered[reader].key);
        if (!place) {
          goto retry_rehashing;
        }
        *place = (fio_hash_data_s){.key = h->ordered[reader].key,
                                   .obj = h->ordered + writer};
        fio_hash_data_ordered_s old = h->ordered[reader];
        h->ordered[reader] =
            (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
        h->ordered[writer] = old;
        ++writer;
      } else {
        FIO_HASH_KEY_DESTROY(h->ordered[reader].key);
        h->ordered[reader].key = FIO_HASH_KEY_INVALID;
      }
      ++reader;
    }
    h->pos = writer;
    // h->ordered[h->pos] =
    //     (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
  }
}

FIO_FUNC inline size_t fio_hash_each(fio_hash_s *hash, size_t start_at,
                                     int (*task)(FIO_HASH_KEY_TYPE key,
                                                 void *obj, void *arg),
                                     void *arg) {
  if (start_at >= hash->count)
    return hash->count;
  size_t count = 0;
  if (hash->pos == hash->count) {
    count = start_at;
    while (count < hash->pos) {
      /* no "holes" in the hash. */
      ++count;
      if (task(hash->ordered[count - 1].key,
               (void *)hash->ordered[count - 1].obj, arg) == -1)
        return count;
    }
  } else {
    size_t pos = 0;
    while (count < start_at && pos < hash->pos) {
      if (hash->ordered[pos].obj) {
        ++count;
      }
      ++pos;
    }
    while (pos < hash->pos) {
      if (hash->ordered[pos].obj) {
        ++count;
        if (task(hash->ordered[pos].key, (void *)hash->ordered[pos].obj, arg) ==
            -1)
          return count;
      }
      ++pos;
    }
  }
  return count;
}

/** Returns the number of elements in the Hash. */
FIO_FUNC inline size_t fio_hash_count(const fio_hash_s *hash) {
  if (!hash)
    return 0;
  return hash->count;
}

/**
 * Returns a temporary theoretical Hash map capacity.
 * This could be used for testig performance and memory consumption.
 */
FIO_FUNC inline size_t fio_hash_capa(const fio_hash_s *hash) {
  if (!hash)
    return 0;
  return hash->capa;
}

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Hash Map.
 *
 * Returns the updated hash map capacity.
 */
FIO_FUNC inline size_t fio_hash_compact(fio_hash_s *hash) {
  if (!hash)
    return 0;
  if (hash->count == hash->pos && (hash->count << 1) >= hash->capa)
    return hash->capa;
  while (hash->mask && hash->mask >= hash->count)
    hash->mask = hash->mask >> 1;
  if (hash->mask + 1 < FIO_HASH_INITIAL_CAPACITY)
    hash->mask = (FIO_HASH_INITIAL_CAPACITY - 1);
  while (hash->count >= hash->mask)
    hash->mask = (hash->mask << 1) | 1;
  fio_hash_rehash(hash);

  return hash->capa;
}

#if DEBUG && !FIO_HASH_NO_TEST
#define FIO_HASHMAP_TEXT_COUNT 524288UL
#include <stdio.h>
FIO_FUNC void fio_hash_test(void) {
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  fio_hash_s h = {.capa = 0};
  fprintf(stderr, "=== Testing Core HashMap (fio_hashmap.h)\n");
  fprintf(stderr, "* Inserting %lu items\n", FIO_HASHMAP_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_HASHMAP_TEXT_COUNT; ++i) {
    fio_hash_insert(&h, i, (void *)i);
    TEST_ASSERT((i == (uintptr_t)fio_hash_find(&h, i)), "insertion != find");
  }
  fprintf(stderr, "* Seeking %lu items\n", FIO_HASHMAP_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_HASHMAP_TEXT_COUNT; ++i) {
    TEST_ASSERT((i == (uintptr_t)fio_hash_find(&h, i)), "insertion != find");
  }
  {
    fprintf(stderr, "* Testing order for %lu items\n", FIO_HASHMAP_TEXT_COUNT);
    uintptr_t i = 1;
    FIO_HASH_FOR_LOOP(&h, pos) {
      TEST_ASSERT(pos->key == (uintptr_t)pos->obj, "Key and value mismatch.");
      TEST_ASSERT(pos->key == i, "Key out of order %lu !=  %lu.",
                  (unsigned long)i, (unsigned long)pos->key);
      ++i;
    }
  }
  fprintf(stderr, "* Removing odd items from %lu items\n",
          FIO_HASHMAP_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_HASHMAP_TEXT_COUNT; i += 2) {
    uintptr_t old = (uintptr_t)fio_hash_insert(&h, i, NULL);
    TEST_ASSERT(old == i, "Removal didn't return old value.");
    TEST_ASSERT(!(fio_hash_find(&h, i)), "Removal failed (still exists).");
  }
  if (1) {
    size_t count = h.count;
    size_t pos = h.pos;
    fio_hash_insert(&h, 1, (void *)1);
    TEST_ASSERT(
        count + 1 == h.count,
        "Readding a removed item should increase count by 1 (%zu + 1 != %zu).",
        count, (size_t)h.count);
    TEST_ASSERT(
        pos == h.pos,
        "Readding a removed item shouldn't change the position marker!");
    TEST_ASSERT(fio_hash_find(&h, 1) == (void *)1,
                "Readding a removed item should update the item (%p != 1)!",
                fio_hash_find(&h, 1));
    fio_hash_insert(&h, 1, NULL);
    TEST_ASSERT(count == h.count,
                "Re-removing an item should decrease count (%zu != %zu).",
                count, (size_t)h.count);
    TEST_ASSERT(pos == h.pos,
                "Re-removing an item shouldn't effect the position marker!");
    TEST_ASSERT(!fio_hash_find(&h, 1),
                "Re-removing a re-added item should update the item!");
  }
  {
    fprintf(stderr, "* Testing for %lu / 2 holes\n", FIO_HASHMAP_TEXT_COUNT);
    uintptr_t i = 1;
    FIO_HASH_FOR_LOOP(&h, pos) {
      if (pos->obj) {
        TEST_ASSERT(pos->key == (uintptr_t)pos->obj, "Key and value mismatch.");
        TEST_ASSERT(pos->key == i, "Key out of order %lu !=  %lu.",
                    (unsigned long)i, (unsigned long)pos->key);
      } else {
        TEST_ASSERT(pos->obj == NULL, "old value detected.");
        TEST_ASSERT(pos->key == i, "Key out of order.");
      }
      ++i;
    }
  }
  {
    fprintf(stderr, "* Poping two elements (testing pop through holes)\n");
    FIO_HASH_KEY_TYPE k;
    TEST_ASSERT(fio_hash_pop(&h, &k), "Pop 1 failed to collect object");
    TEST_ASSERT(k, "Pop 1 failed to collect key");
    FIO_HASH_KEY_DESTROY(k);
    TEST_ASSERT(fio_hash_pop(&h, &k), "Pop 2 failed to collect object");
    TEST_ASSERT(k, "Pop 2 failed to collect key");
    FIO_HASH_KEY_DESTROY(k);
  }
  fprintf(stderr, "* Compacting Hash to %lu\n", FIO_HASHMAP_TEXT_COUNT >> 1);
  fio_hash_compact(&h);
  {
    fprintf(stderr, "* Testing that %lu items are continues\n",
            FIO_HASHMAP_TEXT_COUNT >> 1);
    uintptr_t i = 0;
    FIO_HASH_FOR_LOOP(&h, pos) {
      TEST_ASSERT(pos->obj, "Found a hole after compact.");
      TEST_ASSERT(pos->key == (uintptr_t)pos->obj, "Key and value mismatch.");
      ++i;
    }
    TEST_ASSERT(i == h.count, "count error (%lu != %lu).", i, h.count);
  }
  fio_hash_free(&h);
  fprintf(stderr, "* passed... without testing that FIO_HASH_KEY_DESTROY is "
                  "called only once.\n");
}
#endif /* DEBUG Testing */

#undef FIO_FUNC

#endif /* H_FIO_SIMPLE_HASH_H */
