#ifndef H_FIO_SIMPLE_HASH_H
/*
Copyright: Boaz Segev, 2017
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
#define FIO_HASH_COMPARE_KEYS(k1, k2) ((k1) == (k2))
#define FIO_HASH_KEY_TYPE uint64_t
#define FIO_HASH_KEY2UINT(key) (key)
#define FIO_HASH_KEY_INVALID 0
#define FIO_HASH_KEY_ISINVALID(key) ((key) == 0)
#define FIO_HASH_KEY_COPY(key) (key)
#define FIO_HASH_KEY_DESTROY(key) 0
#endif

#ifndef FIO_HASH_INITIAL_CAPACITY
/* MUST be a power of 2 */
#define FIO_HASH_INITIAL_CAPACITY 16
#endif

#ifndef FIO_HASH_MAX_MAP_SEEK
/* MUST be a power of 2 */
#define FIO_HASH_MAX_MAP_SEEK (256)
#endif

/* *****************************************************************************
Hash API
***************************************************************************** */

/** The Hash Table container type. */
typedef struct fio_hash_s fio_hash_s;

/** Allocates and initializes internal data and resources. */
FIO_FUNC void fio_hash_new(fio_hash_s *hash);

/** Deallocates any internal resources. */
FIO_FUNC void fio_hash_free(fio_hash_s *hash);

/**
 * Inserts an object to the Hash Map Table, rehashing if required, returning the
 * old object if it exists.
 *
 * Set obj to NULL to remove an existing data (the existing object will be
 * returned).
 */
FIO_FUNC void *fio_hash_insert(fio_hash_s *hash, FIO_HASH_KEY_TYPE key,
                               void *obj);

/** Locates an object in the Hash Map Table according to the hash key value. */
FIO_FUNC inline void *fio_hash_find(fio_hash_s *hash, FIO_HASH_KEY_TYPE key);

/** Returns the number of elements currently in the Hash Table. */
FIO_FUNC inline size_t fio_hash_count(const fio_hash_s *hash);

/** Forces a rehashing of the hash. */
FIO_FUNC void fio_hash_rehash(fio_hash_s *hash);

/**
 * Returns a temporary theoretical Hash map capacity.
 * This could be used for testig performance and memory consumption.
 */
FIO_FUNC inline size_t fio_hash_capa(const fio_hash_s *hash);

/**
 * A macro for a `for` loop that iterates over all the hashed objetcs (in
 * order).
 *
 * `hash` a pointer to the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 */
#define FIO_HASH_FOR_LOOP(hash, i)

/**
 * A macro for a `for` loop that iterates over all the hashed objetcs (in
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
 * Free the object manually (if required). The key will be freed automatically
 * (if required).
 */
#define FIO_HASH_FOR_EMPTY(hash, i)

/**
 * A macro for a `for` loop that will iterate over all the hashed objetcs (in
 * order) and empties the hash, later calling `fio_hash_free` to free the hash.
 *
 * `hash` a pointer to the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 *
 * Free the object manually (if required). The key will be freed automatically
 * (if required).
 */
#define FIO_HASH_FOR_EMPTY(hash, i)

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
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Hash Map.
 *
 * Returns the updated hash map capacity.
 */
FIO_FUNC inline size_t fio_hash_compact(fio_hash_s *hash);

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
       !FIO_HASH_KEY_ISINVALID(container->key); ++container)

#undef FIO_HASH_FOR_EMPTY
#define FIO_HASH_FOR_EMPTY(hash, container)                                    \
  for (fio_hash_data_ordered_s *container = (hash)->ordered;                   \
       !FIO_HASH_KEY_ISINVALID(container->key) ||                              \
       (((hash)->pos = (hash)->count = 0) != 0 ||                              \
        (free((hash)->map),                                                    \
         ((hash)->map =                                                        \
              (fio_hash_data_s *)calloc(sizeof(*(hash)->map), (hash)->capa)),  \
         0) != 0);                                                             \
       FIO_HASH_KEY_DESTROY(container->key),                                   \
                               container->key = FIO_HASH_KEY_INVALID,          \
                               container->obj = NULL, (++container))

#undef FIO_HASH_FOR_FREE
#define FIO_HASH_FOR_FREE(hash, container)                                     \
  for (fio_hash_data_ordered_s *container = (hash)->ordered;                   \
       !FIO_HASH_KEY_ISINVALID(container->key) ||                              \
       ((fio_hash_free(hash), 0) != 0);                                        \
       FIO_HASH_KEY_DESTROY(container->key), (++container))

/* *****************************************************************************
Hash allocation / deallocation.
***************************************************************************** */

FIO_FUNC void fio_hash_new(fio_hash_s *h) {
  *h = (fio_hash_s){
      .mask = (FIO_HASH_INITIAL_CAPACITY - 1),
      .map =
          (fio_hash_data_s *)calloc(sizeof(*h->map), FIO_HASH_INITIAL_CAPACITY),
      .ordered = (fio_hash_data_ordered_s *)calloc(sizeof(*h->ordered),
                                                   FIO_HASH_INITIAL_CAPACITY),
      .capa = FIO_HASH_INITIAL_CAPACITY,
  };
  if (!h->map || !h->ordered)
    perror("ERROR: Hash Table couldn't allocate memory"), exit(errno);
  h->ordered[0] =
      (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
}

FIO_FUNC void fio_hash_free(fio_hash_s *h) {
  free(h->map);
  free(h->ordered);
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
                              : (hash->capa >> 1);
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
  if (obj && hash->pos + 1 >= hash->capa)
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
    hash->ordered[hash->pos] =
        (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
    return NULL;
  }

  /* an object exists, this is a "replace/delete" operation */
  void *old = (void *)info->obj->obj;
  if (!obj) {
    /* it was a delete operation */
    hash->count--;
    if (info->obj == hash->ordered + hash->pos - 1) {
      /* we removed the last ordered element, no need to keep both holes. */
      --hash->pos;
      info->obj->obj = NULL;
      info->obj = NULL;
      hash->ordered[hash->pos] =
          (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
      return old;
    }
  }
  info->obj->obj = (fio_hash_data_s *)obj;
  return old;
}

/* attempts to rehash the hashmap. */
FIO_FUNC void fio_hash_rehash(fio_hash_s *h) {
retry_rehashing:
  h->mask = ((h->mask) << 1) | (1 | (FIO_HASH_INITIAL_CAPACITY - 1));
  {
    /* It's better to reallocate using calloc than manually zero out memory */
    /* Maybe there's enough zeroed out pages available in the system */
    h->capa = h->mask + 1;
    free(h->map);
    h->map = (fio_hash_data_s *)calloc(sizeof(*h->map), h->capa);
    if (!h->map)
      perror("HashMap Allocation Failed"), exit(errno);
    /* the ordered list doesn't care about initialized memory, so realloc */
    /* will be faster. */
    h->ordered = (fio_hash_data_ordered_s *)realloc(
        h->ordered, h->capa * sizeof(*h->ordered));
    if (!h->ordered)
      perror("HashMap Reallocation Failed"), exit(errno);
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
        h->ordered[reader].obj = NULL;
        h->ordered[writer] = old;
        ++writer;
      } else {
        FIO_HASH_KEY_DESTROY(h->ordered[reader].key);
      }
      ++reader;
    }
    h->pos = writer;
    h->ordered[h->pos] =
        (fio_hash_data_ordered_s){.key = FIO_HASH_KEY_INVALID, .obj = NULL};
  }
}

FIO_FUNC inline size_t fio_hash_each(fio_hash_s *hash, size_t start_at,
                                     int (*task)(FIO_HASH_KEY_TYPE key,
                                                 void *obj, void *arg),
                                     void *arg) {
  if (start_at >= hash->count)
    return hash->count;
  size_t count = 0;
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
  while (hash->mask && hash->mask >= hash->count)
    hash->mask = hash->mask >> 1;
  if (hash->mask + 1 < FIO_HASH_INITIAL_CAPACITY)
    hash->mask = (FIO_HASH_INITIAL_CAPACITY - 1);
  fio_hash_rehash(hash);

  return hash->capa;
}

#endif /* H_FIO_SIMPLE_HASH_H */
