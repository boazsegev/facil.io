#ifndef H_FIO_SIMPLE_HASH_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
 * A simple ordered Hash Table implementation, with a minimal API.
 *
 * Unique keys are required. Full key collisions aren't handled, instead the old
 * value is replaced and returned.
 *
 * Partial key collisions are handled by seeking forward (in leaps) and
 * attempting to find a close enough spot. If a close enough spot isn't found,
 * rehashing is initiated and memory consumption increases.
 *
 * The Hash Table is ordered using an internal linked list of data containers
 * with duplicates of the hash key data.
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
#include <stdio.h>
#include <stdlib.h>

#ifndef HASH_INITIAL_CAPACITY
/* MUST be a power of 2 */
#define HASH_INITIAL_CAPACITY 16
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
FIO_FUNC void *fio_hash_insert(fio_hash_s *hash, uintptr_t key, void *obj);

/** Locates an object in the Hash Map Table according to the hash key value. */
FIO_FUNC inline void *fio_hash_find(fio_hash_s *hash, uintptr_t key);

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
 * `hash` is the name of the hash table variable and `i` is a temporary variable
 * name to be created for iteration.
 *
 * `i->key` is the key and `i->obj` is the hashed data.
 */
#define FIO_HASH_FOR_LOOP(hash, i)

/**
 * Iteration using a callback for each entry in the Hash Table.
 *
 * The callback task function must accept the hash key, the entry data and an
 * opaque user pointer:
 *
 *     int example_task(uintptr_t key, void *obj, void *arg) {return 0;}
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
FIO_FUNC inline size_t
fio_hash_each(fio_hash_s *hash, const size_t start_at,
              int (*task)(uintptr_t key, void *obj, void *arg), void *arg);

/* *****************************************************************************
Hash Table Internal Data Structures
***************************************************************************** */

typedef struct fio_hash_data_s {
  uintptr_t key; /* another copy for memory cache locality */
  struct fio_hash_data_s *obj;
} fio_hash_data_s;

/* the information in tjhe Hash Map structure should be considered READ ONLY. */
struct fio_hash_s {
  uintptr_t count;
  uintptr_t capa;
  uintptr_t pos;
  uintptr_t mask;
  fio_hash_data_s *ordered;
  fio_hash_data_s *map;
};

#undef FIO_HASH_FOR_LOOP
#define FIO_HASH_FOR_LOOP(hash, container)                                     \
  for (fio_hash_data_s *container = (hash)->ordered; container->key;           \
       ++container)

/* *****************************************************************************
Hash allocation / deallocation.
***************************************************************************** */

FIO_FUNC void fio_hash_new(fio_hash_s *h) {
  *h = (fio_hash_s){
      .mask = (HASH_INITIAL_CAPACITY - 1),
      .map = (fio_hash_data_s *)calloc(sizeof(*h->map), HASH_INITIAL_CAPACITY),
      .ordered =
          (fio_hash_data_s *)calloc(sizeof(*h->ordered), HASH_INITIAL_CAPACITY),
      .capa = HASH_INITIAL_CAPACITY,
  };
  if (!h->map || !h->ordered)
    perror("ERROR: Hash Table couldn't allocate memory"), exit(errno);
  h->ordered[0] = (fio_hash_data_s){.key = 0, .obj = NULL};
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
FIO_FUNC fio_hash_data_s *fio_hash_seek_pos_(fio_hash_s *hash, uintptr_t key) {
  /* TODO: consider implementing Robing Hood reordering during seek? */
  fio_hash_data_s *pos = hash->map + (key & hash->mask);
  uintptr_t i = 0;
  const uintptr_t limit = hash->capa > FIO_HASH_MAX_MAP_SEEK
                              ? FIO_HASH_MAX_MAP_SEEK
                              : (hash->capa >> 1);
  while (i < limit) {
    if (!pos->key || pos->key == key)
      return pos;
    pos = hash->map +
          (((key & hash->mask) + fio_hash_map_cuckoo_steps(i++)) & hash->mask);
  }
  return NULL;
}

/* finds an object in the map */
FIO_FUNC inline void *fio_hash_find(fio_hash_s *hash, uintptr_t key) {
  fio_hash_data_s *info = fio_hash_seek_pos_(hash, key);
  if (!info || !info->obj)
    return NULL;
  return (void *)info->obj->obj;
}

/* inserts an object to the map, rehashing if required, returning old object.
 * set obj to NULL to remove existing data.
 */
FIO_FUNC void *fio_hash_insert(fio_hash_s *hash, uintptr_t key, void *obj) {
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
    /* add object to map */
    *info = (fio_hash_data_s){.key = key, .obj = hash->ordered + hash->pos};

    /* add object to ordered hash */
    hash->ordered[hash->pos] =
        (fio_hash_data_s){.key = key, .obj = (fio_hash_data_s *)obj};

    /* manage counters and mark end position */
    hash->count++;
    hash->pos++;
    hash->ordered[hash->pos] =
        (fio_hash_data_s){.key = 0, .obj = (fio_hash_data_s *)NULL};
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
          (fio_hash_data_s){.key = 0, .obj = (fio_hash_data_s *)NULL};
      return old;
    }
  }
  info->obj->obj = (fio_hash_data_s *)obj;
  return old;
}

/* attempts to rehash the hashmap. */
FIO_FUNC void fio_hash_rehash(fio_hash_s *h) {
retry_rehashing:
  h->mask = ((h->mask) << 1) | 1;
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
    h->ordered =
        (fio_hash_data_s *)realloc(h->ordered, h->capa * sizeof(*h->ordered));
    if (!h->ordered)
      perror("HashMap Reallocation Failed"), exit(errno);
  }
  if (h->pos == h->count) {
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
        fio_hash_data_s old = h->ordered[reader];
        h->ordered[reader].obj = NULL;
        h->ordered[writer] = old;
        ++writer;
      }
      ++reader;
    }
    h->pos = writer;
    h->ordered[h->pos] =
        (fio_hash_data_s){.key = 0, .obj = (fio_hash_data_s *)NULL};
  }
}

FIO_FUNC inline size_t
fio_hash_each(fio_hash_s *hash, size_t start_at,
              int (*task)(uintptr_t key, void *obj, void *arg), void *arg) {
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

#endif /* H_FIO_SIMPLE_HASH_H */
