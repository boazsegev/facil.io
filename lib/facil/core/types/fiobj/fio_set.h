/*
Copyright: Boaz Segev, 2018
License: MIT
*/

/**
 * A simple ordered Set implementation, with a minimal API.
 *
 * ##  What is a Set?
 *
 * A Set (also reffered to as a "Bag") is a data-storage that promises the data
 * in the Set is unique (no two objects are equal).
 *
 * ##  What is it used for?
 *
 * The Set is often used to implement other data structures, such as HashMaps or
 * caching schemes.
 *
 * In effect, a HashMap is a Set where each object stored in the set is a
 * key-value couplet and only the key is tested for equality when comparing
 * objects.
 *
 * ##  About facil.io's Set
 *
 * The Set is ordered using an internal ordered array of hash values and
 * objects.
 *
 * Duplicates of the hash value (but not the objects) are placed in a mapping
 * array to improve cache locality when seeking for existing objects - only full
 * hash identity prompts an object identity test (to test for full collisions).
 *
 * The Set's object types are adjustable using macros. By default, Objects are a
 * `void *` type. This makes it easy to implement more complex data structures
 * that are derived from the Set (such as HashMaps or caching schemes), but it
 * also means that objects aren't tested for identity (since `void *` data is
 * opaque and unknown).
 *
 * The Set requires each object to have a unique hash integer value that will be
 * used for mapping the objects for quick access. By default, hash values are a
 * `uintptr_t` type.
 *
 * Partial hash collisions are handled by seeking forward and attempting to find
 * a close enough spot. If a close enough spot isn't found, rehashing is
 * initiated and memory consumption increases.
 *
 * The Set is protected against full hash collisions only when the
 * FIO_SET_COMPARE_OBJ(o1, o2) macro is defined. Otherwise collisions are the
 * same as object identity.
 *
 * facil.io's implementation automatically protects against full collisions
 * without the need to keep any removed objects alive. This is performed by
 * detecting collisions and rehashing the Set when required.
 *
 * The file was written to be mostly compatible with C++ as well as C, hence
 * some pointer casting.
 */
#define H_FIO_SIMPLE_SET_H

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
 * By defining ALL THREE of the following macros, hashing collision protection
 * can be adjusted:
 * * FIO_SET_HASH_TYPE              - the type of the hash value.
 * * FIO_SET_HASH2UINTPTR(hash)     - converts the hash value to a uintptr_t.
 * * FIO_SET_COMPARE_HASH(h1, h2)   - compares two hash values (1 == equal).
 * * FIO_SET_HASH_INVALID           - an invalid Hash value, all bytes are 0.
 *
 * Note: FIO_SET_HASH_TYPE should, normaly be left alone (uintptr_t is enough).
 *       Also, the hash value 0 is reserved to indicate an empty slot.
 *
 * By defining ALL of the following macros, the Set's type and functionality can
 * be adjusted:
 * * FIO_SET_NAME(name)             - allows a prefixe to be added to name.
 * * FIO_SET_OBJECT_TYPE            - the type used for set objects.
 *
 * The following macros allow further control over the Set's behavior:
 * * FIO_SET_COMPARE_OBJ(o1, o2)    - compares two objects (1 == equal).
 * * FIO_SET_OBJ_COPY(dest, obj)    - creates a persistent copy of the object.
 * * FIO_SET_OBJ_DESTROY(obj)       - destroys (or frees) the object's copy.
 *
 * Note: FIO_SET_NAME(name) defaults to: fio_ptr_set_##name`.
 *
 * Note: FIO_SET_COMPARE_OBJ will be used to compare against invalid as well as
 *       valid objects. Invalid objects have their bytes all zero.
 *       FIO_SET_OBJ_DESTROY should zero out unused objects or somehow mark them
 *       as invalid.
 *
 * Note: Before freeing the Set, FIO_SET_OBJ_DESTROY will be automatically
 *       called for every existing object.
 */
#ifndef FIO_SET_NAME
#define FIO_SET_NAME(name) fio_ptr_set_##name
#endif

#if !defined(FIO_SET_OBJECT_TYPE)
#define FIO_SET_OBJECT_TYPE void *
#elif !defined(FIO_SET_NO_TEST)
#define FIO_SET_NO_TEST 1
#endif

/* The default Set has opaque objects that can't be compared */
#if !defined(FIO_SET_COMPARE_OBJ)
#define FIO_SET_COMPARE_OBJ(o1, o2) (1)
#endif

/** object copy required? */
#ifndef FIO_SET_OBJ_COPY
#define FIO_SET_OBJ_COPY(dest, obj) ((dest) = (obj))
#endif

/** object destruction required? */
#ifndef FIO_SET_OBJ_DESTROY
#define FIO_SET_OBJ_DESTROY(obj) ((void)0)
#endif

/** test for a pre-defined hash value type */
#ifndef FIO_SET_HASH_TYPE
#define FIO_SET_HASH_TYPE uintptr_t
#endif

/** test for a pre-defined hash to integer conversion */
#ifndef FIO_SET_HASH2UINTPTR
#define FIO_SET_HASH2UINTPTR(hash) ((uintptr_t)(hash))
#endif

/** test for a pre-defined invalid hash value (all bytes are 0) */
#ifndef FIO_SET_HASH_INVALID
#define FIO_SET_HASH_INVALID ((FIO_SET_HASH_TYPE)0)
#endif

/** test for a pre-defined hash comparison */
#ifndef FIO_SET_COMPARE_HASH
#define FIO_SET_COMPARE_HASH(h1, h2) (h1 == h2)
#endif

/* Customizable memory management */
#ifndef FIO_SET_REALLOC /* NULL ptr indicates new allocation */
#define FIO_SET_REALLOC(ptr, original_size, new_size, valid_data_length)       \
  realloc((ptr), (new_size))
#endif
#ifndef FIO_SET_CALLOC
#define FIO_SET_CALLOC(size, count) calloc((size), (count))
#endif
#ifndef FIO_SET_FREE
#define FIO_SET_FREE(ptr, size) free((ptr))
#endif

/* The maximum number of bins to rotate when partial collisions occure */
#ifndef FIO_SET_MAX_MAP_SEEK
#define FIO_SET_MAX_MAP_SEEK (96)
#endif

/* Prime numbers are better */
#ifndef FIO_SET_CUCKOO_STEPS
#define FIO_SET_CUCKOO_STEPS 11
#endif

/* *****************************************************************************
Set API
***************************************************************************** */

/** The Set container type. By default: fio_ptr_set_s */
typedef struct FIO_SET_NAME(s) FIO_SET_NAME(s);

#ifndef FIO_SET_INIT
/** Initializes the set */
#define FIO_SET_INIT                                                           \
  { .capa = 0 }
#endif

/** Deallocates any internal resources. Doesn't free any objects! */
FIO_FUNC void FIO_SET_NAME(free)(FIO_SET_NAME(s) * set);

/** Locates an object in the Set, if it exists. */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
    FIO_SET_NAME(find)(FIO_SET_NAME(s) * set,
                       const FIO_SET_HASH_TYPE hash_value,
                       FIO_SET_OBJECT_TYPE obj);

/**
 * Inserts an object to the Set, rehashing if required, returning the new (or
 * old) object's pointer.
 *
 * If the object already exists in the set, no action is performed (the old
 * object is returned).
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
    FIO_SET_NAME(insert)(FIO_SET_NAME(s) * set,
                         const FIO_SET_HASH_TYPE hash_value,
                         FIO_SET_OBJECT_TYPE obj);

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
    FIO_SET_NAME(overwrite)(FIO_SET_NAME(s) * set,
                            const FIO_SET_HASH_TYPE hash_value,
                            FIO_SET_OBJECT_TYPE obj);

/**
 * Removes an object from the Set, rehashing if required.
 */
FIO_FUNC inline void FIO_SET_NAME(remove)(FIO_SET_NAME(s) * set,
                                          const FIO_SET_HASH_TYPE hash_value,
                                          FIO_SET_OBJECT_TYPE obj);

/**
 * Allows a peak at the Set's last element.
 *
 * Remember that objects might be destroyed if the Set is altered
 * (`FIO_SET_OBJ_DESTROY`).
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *FIO_SET_NAME(last)(FIO_SET_NAME(s) * set);

/**
 * Allows the Hash to be momenterally used as a stack, destroying the last
 * object added (`FIO_SET_OBJ_DESTROY`).
 */
FIO_FUNC inline void FIO_SET_NAME(pop)(FIO_SET_NAME(s) * set);

/** Returns the number of object currently in the Set. */
FIO_FUNC inline size_t FIO_SET_NAME(count)(const FIO_SET_NAME(s) * set);

/**
 * Returns a temporary theoretical Set capacity.
 * This could be used for testing performance and memory consumption.
 */
FIO_FUNC inline size_t FIO_SET_NAME(capa)(const FIO_SET_NAME(s) * set);

/**
 * Requires that a Set contains the minimal requested theoretical capacity.
 *
 * Returns the actual (temporary) theoretical capacity.
 */
FIO_FUNC inline size_t FIO_SET_NAME(capa_require)(FIO_SET_NAME(s) * set,
                                                  size_t min_capa);

/**
 * Returns non-zero if the Set is fragmented (more than 50% holes).
 */
FIO_FUNC inline size_t FIO_SET_NAME(is_fragmented)(const FIO_SET_NAME(s) * set);

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Set.
 *
 * Returns the updated Set capacity.
 */
FIO_FUNC inline size_t FIO_SET_NAME(compact)(FIO_SET_NAME(s) * set);

/** Forces a rehashing of the Set. */
FIO_FUNC void FIO_SET_NAME(rehash)(FIO_SET_NAME(s) * set);

#ifndef FIO_SET_FOR_LOOP
/**
 * A macro for a `for` loop that iterates over all the Set's objects (in order).
 *
 * `set` is a pointer to the Set variable and `pos` is a temporary variable
 * name to be created for iteration.
 *
 * `pos->hash` is the hashing value and `pos->obj` is the object's data.
 *
 * Since the Set might have "holes" (objects that were removed), it is important
 * to skip any `FIO_SET_COMPARE_HASH(pos->hash, FIO_SET_HASH_INVALID)`.
 */
#define FIO_SET_FOR_LOOP(set, pos)
#endif

/* *****************************************************************************
Set Internal Data Structures
***************************************************************************** */

typedef struct FIO_SET_NAME(_ordered_s_) {
  FIO_SET_HASH_TYPE hash;
  FIO_SET_OBJECT_TYPE obj;
} FIO_SET_NAME(_ordered_s_);

typedef struct FIO_SET_NAME(_map_s_) {
  FIO_SET_HASH_TYPE hash; /* another copy for memory cache locality */
  FIO_SET_NAME(_ordered_s_) * pos;
} FIO_SET_NAME(_map_s_);

/* the information in the Hash Map structure should be considered READ ONLY. */
struct FIO_SET_NAME(s) {
  uintptr_t count;
  uintptr_t capa;
  uintptr_t pos;
  uintptr_t mask;
  FIO_SET_NAME(_ordered_s_) * ordered;
  FIO_SET_NAME(_map_s_) * map;
  uint8_t has_collisions;
};

#undef FIO_SET_FOR_LOOP
#define FIO_SET_FOR_LOOP(set, container)                                       \
  for (__typeof__((set)->ordered) container = (set)->ordered;                  \
       container && (container < ((set)->ordered + (set)->pos)); ++container)

/* *****************************************************************************
Internal Helpers
***************************************************************************** */

/** Locates an object's map position in the Set, if it exists. */
FIO_FUNC inline FIO_SET_NAME(_map_s_) *
    FIO_SET_NAME(_find_map_pos_)(FIO_SET_NAME(s) * set,
                                 const FIO_SET_HASH_TYPE hash_value,
                                 FIO_SET_OBJECT_TYPE obj) {
  if (set->map) {
    /* make sure collisions don't effect seeking */
    if (set->has_collisions && set->pos != set->count) {
      FIO_SET_NAME(rehash)(set);
    }

    /* O(1) access to object */
    FIO_SET_NAME(_map_s_) *pos =
        set->map + (FIO_SET_HASH2UINTPTR(hash_value) & set->mask);
    if (FIO_SET_COMPARE_HASH(FIO_SET_HASH_INVALID, pos->hash))
      return pos;
    if (FIO_SET_COMPARE_HASH(pos->hash, hash_value)) {
      if (!pos->pos || FIO_SET_COMPARE_OBJ(pos->pos->obj, obj))
        return pos;
      set->has_collisions = 1;
    }

    /* Handle partial / full collisions with cuckoo steps O(x) access time */
    uintptr_t i = FIO_SET_CUCKOO_STEPS;
    const uintptr_t limit =
        FIO_SET_CUCKOO_STEPS * (set->capa > (FIO_SET_MAX_MAP_SEEK << 2)
                                    ? FIO_SET_MAX_MAP_SEEK
                                    : (set->capa >> 2));
    while (i < limit) {
      pos = set->map + ((FIO_SET_HASH2UINTPTR(hash_value) + i) & set->mask);
      if (FIO_SET_COMPARE_HASH(FIO_SET_HASH_INVALID, pos->hash))
        return pos;
      if (FIO_SET_COMPARE_HASH(pos->hash, hash_value)) {
        if (!pos->pos || FIO_SET_COMPARE_OBJ(pos->pos->obj, obj))
          return pos;
        set->has_collisions = 1;
      }
      i += FIO_SET_CUCKOO_STEPS;
    }
  }
  return NULL;
  (void)obj; /* in cases where FIO_SET_COMPARE_OBJ does nothing */
}
#undef FIO_SET_CUCKOO_STEPS

/** Removes "holes" from the Set's internal Array - MUST re-hash afterwards. */
FIO_FUNC inline void FIO_SET_NAME(_compact_ordered_array_)(FIO_SET_NAME(s) *
                                                           set) {
  if (set->count == set->pos)
    return;
  FIO_SET_NAME(_ordered_s_) *reader = set->ordered;
  FIO_SET_NAME(_ordered_s_) *writer = set->ordered;
  const FIO_SET_NAME(_ordered_s_) *end = set->ordered + set->pos;
  for (; reader && (reader < end); ++reader) {
    if (FIO_SET_COMPARE_HASH(reader->hash, FIO_SET_HASH_INVALID)) {
      continue;
    }
    *writer = *reader;
    ++writer;
  }
  /* fix any possible counting errors as well as resetting position */
  set->pos = set->count = (writer - set->ordered);
}

/** (Re)allocates the set's internal, invalidatint the mapping (must rehash) */
FIO_FUNC inline void FIO_SET_NAME(_reallocate_set_mem_)(FIO_SET_NAME(s) * set) {
  FIO_SET_FREE(set->map, set->capa * sizeof(*set->map));
  set->map = (FIO_SET_NAME(_map_s_) *)FIO_SET_CALLOC(sizeof(*set->map),
                                                     (set->mask + 1));
  set->ordered = (FIO_SET_NAME(_ordered_s_) *)FIO_SET_REALLOC(
      set->ordered, (set->capa * sizeof(*set->ordered)),
      ((set->mask + 1) * sizeof(*set->ordered)),
      (set->pos * sizeof(*set->ordered)));
  if (!set->map || !set->ordered) {
    perror("FATAL ERROR: couldn't allocate memory for Set data");
    exit(errno);
  }
  set->capa = set->mask + 1;
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
FIO_SET_NAME(_insert_or_overwrite_)(FIO_SET_NAME(s) * set,
                                    const FIO_SET_HASH_TYPE hash_value,
                                    FIO_SET_OBJECT_TYPE obj, int overwrite) {
  if (FIO_SET_COMPARE_HASH(hash_value, FIO_SET_HASH_INVALID))
    return NULL;

  /* automatic fragmentation protection */
  if (FIO_SET_NAME(is_fragmented)(set))
    FIO_SET_NAME(rehash)(set);

  /* locate future position, rehashing until a position is available */
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, hash_value, obj);

  while (!pos) {
    set->mask = (set->mask << 1) | 1;
    FIO_SET_NAME(rehash)(set);
    pos = FIO_SET_NAME(_find_map_pos_)(set, hash_value, obj);
  }

  /* overwriting / new */
  if (pos->pos) {
    /* overwrite existing object */
    if (!overwrite)
      return &pos->pos->obj;
    FIO_SET_OBJ_DESTROY(pos->pos->obj);
  } else {
    /* insert into new slot */
    pos->pos = set->ordered + set->pos;
    ++set->pos;
    ++set->count;
  }
  /* store object at position */
  pos->hash = hash_value;
  pos->pos->hash = hash_value;
  FIO_SET_OBJ_COPY(pos->pos->obj, obj);

  return &pos->pos->obj;
}
/* *****************************************************************************
Set Implementation
***************************************************************************** */

/** Deallocates any internal resources. Doesn't free any objects! */
FIO_FUNC void FIO_SET_NAME(free)(FIO_SET_NAME(s) * s) {
  /* destroy existing valid objects */
  const FIO_SET_NAME(_ordered_s_) *const end = s->ordered + s->pos;
  for (FIO_SET_NAME(_ordered_s_) *pos = s->ordered; pos && pos < end; ++pos) {
    if (!FIO_SET_COMPARE_HASH(FIO_SET_HASH_INVALID, pos->hash)) {
      FIO_SET_OBJ_DESTROY(pos->obj);
    }
  }
  /* free ordered array and hash mapping */
  FIO_SET_FREE(s->map, s->capa * sizeof(*s->map));
  FIO_SET_FREE(s->ordered, s->capa * sizeof(*s->ordered));
  *s = (FIO_SET_NAME(s)){.map = NULL};
}

/** Locates an object in the Set, if it exists. */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
FIO_SET_NAME(find)(FIO_SET_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                   FIO_SET_OBJECT_TYPE obj) {
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, hash_value, obj);
  if (!pos || !pos->pos)
    return NULL;
  return &pos->pos->obj;
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
FIO_SET_NAME(insert)(FIO_SET_NAME(s) * set, const FIO_SET_HASH_TYPE hash_value,
                     FIO_SET_OBJECT_TYPE obj) {
  return FIO_SET_NAME(_insert_or_overwrite_)(set, hash_value, obj, 0);
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 *
 * If the object already exists in the set, it will be destroyed and
 * overwritten.
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
FIO_SET_NAME(overwrite)(FIO_SET_NAME(s) * set,
                        const FIO_SET_HASH_TYPE hash_value,
                        FIO_SET_OBJECT_TYPE obj) {
  return FIO_SET_NAME(_insert_or_overwrite_)(set, hash_value, obj, 1);
}

/**
 * Removes an object from the Set, rehashing if required.
 */
FIO_FUNC inline void FIO_SET_NAME(remove)(FIO_SET_NAME(s) * set,
                                          const FIO_SET_HASH_TYPE hash_value,
                                          FIO_SET_OBJECT_TYPE obj) {
  if (FIO_SET_COMPARE_HASH(hash_value, FIO_SET_HASH_INVALID))
    return;
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, hash_value, obj);
  if (!pos || !pos->pos)
    return;
  FIO_SET_OBJ_DESTROY(pos->pos->obj);
  --set->count;
  pos->pos->hash = FIO_SET_HASH_INVALID;
  if (pos->pos == set->pos + set->ordered - 1) {
    do {
      --set->pos;
    } while (set->pos && FIO_SET_COMPARE_HASH(set->ordered[set->pos - 1].hash,
                                              FIO_SET_HASH_INVALID));
  }
  pos->pos = NULL; /* leave pos->hash set to mark "hole" */
}

/**
 * Allows a peak at the Set's last element.
 *
 * Remember that objects might be destroyed if the Set is altered
 * (`FIO_SET_OBJ_DESTROY`).
 */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *FIO_SET_NAME(last)(FIO_SET_NAME(s) * set) {
  if (!set->ordered || !set->pos)
    return NULL;
  return &set->ordered[set->pos - 1].obj;
}

/**
 * Allows the Hash to be momenterally used as a stack, destroying the last
 * object added (`FIO_SET_OBJ_DESTROY`).
 */
FIO_FUNC void FIO_SET_NAME(pop)(FIO_SET_NAME(s) * set) {
  if (!set->ordered || !set->pos)
    return;
  FIO_SET_OBJ_DESTROY(set->ordered[set->pos - 1].obj);
  set->ordered[set->pos - 1].hash = FIO_SET_HASH_INVALID;
  --(set->count);
  do {
    --(set->pos);
  } while (set->pos && FIO_SET_COMPARE_HASH(set->ordered[set->pos - 1].hash,
                                            FIO_SET_HASH_INVALID));
}

/** Returns the number of objects currently in the Set. */
FIO_FUNC inline size_t FIO_SET_NAME(count)(const FIO_SET_NAME(s) * set) {
  return (size_t)set->count;
}

/**
 * Returns a temporary theoretical Set capacity.
 * This could be used for testing performance and memory consumption.
 */
FIO_FUNC inline size_t FIO_SET_NAME(capa)(const FIO_SET_NAME(s) * set) {
  return (size_t)set->capa;
}

/**
 * Requires that a Set contains the minimal requested theoretical capacity.
 *
 * Returns the actual (temporary) theoretical capacity.
 */
FIO_FUNC inline size_t FIO_SET_NAME(capa_require)(FIO_SET_NAME(s) * set,
                                                  size_t min_capa) {
  if (min_capa <= FIO_SET_NAME(capa)(set))
    return FIO_SET_NAME(capa)(set);
  set->mask = 1;
  while (min_capa >= set->mask) {
    set->mask = (set->mask << 1) | 1;
  }
  FIO_SET_NAME(rehash)(set);
  return FIO_SET_NAME(capa)(set);
}

/**
 * Returns non-zero if the Set is fragmented (more than 50% holes).
 */
FIO_FUNC inline size_t FIO_SET_NAME(is_fragmented)(const FIO_SET_NAME(s) *
                                                   set) {
  return ((set->pos - set->count) > (set->count >> 1));
}

/**
 * Attempts to minimize memory usage by removing empty spaces caused by deleted
 * items and rehashing the Set.
 *
 * Returns the updated Set capacity.
 */
FIO_FUNC inline size_t FIO_SET_NAME(compact)(FIO_SET_NAME(s) * set) {
  FIO_SET_NAME(_compact_ordered_array_)(set);
  set->mask = 1;
  while (set->count >= set->mask) {
    set->mask = (set->mask << 1) | 1;
  }
  FIO_SET_NAME(rehash)(set);
  return FIO_SET_NAME(capa)(set);
}

/** Forces a rehashing of the Set. */
FIO_FUNC void FIO_SET_NAME(rehash)(FIO_SET_NAME(s) * set) {
  FIO_SET_NAME(_compact_ordered_array_)(set);
  set->has_collisions = 0;
restart:
  FIO_SET_NAME(_reallocate_set_mem_)(set);
  {
    FIO_SET_NAME(_ordered_s_) const *const end = set->ordered + set->pos;
    for (FIO_SET_NAME(_ordered_s_) *pos = set->ordered; pos < end; ++pos) {
      FIO_SET_NAME(_map_s_) *mp =
          FIO_SET_NAME(_find_map_pos_)(set, pos->hash, pos->obj);
      if (!mp) {
        set->mask = (set->mask << 1) | 1;
        goto restart;
      }
      mp->pos = pos;
      mp->hash = pos->hash;
    }
  }
}

/* *****************************************************************************
Testing
***************************************************************************** */

#if DEBUG && !FIO_SET_NO_TEST
#define FIO_SET_TEXT_COUNT 524288UL
#include <stdio.h>
FIO_FUNC void FIO_SET_NAME(test)(void) {
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }
  FIO_SET_NAME(s) s = FIO_SET_INIT;
  fprintf(stderr, "=== Testing Core Set (fio_set.h)\n");
  fprintf(stderr, "* Inserting %lu items\n", FIO_SET_TEXT_COUNT);
  union {
    FIO_SET_OBJECT_TYPE obj;
    uintptr_t i;
  } obj_mem;
  memset(&obj_mem, 0, sizeof(obj_mem));

  TEST_ASSERT(FIO_SET_NAME(count)(&s) == 0,
              "empty set should have zero objects");
  TEST_ASSERT(FIO_SET_NAME(capa)(&s) == 0, "empty set should have no capacity");
  TEST_ASSERT(!FIO_SET_NAME(is_fragmented)(&s),
              "empty set shouldn't be considered fragmented");
  TEST_ASSERT(!FIO_SET_NAME(last)(&s),
              "empty set shouldn't have a last object");

  for (unsigned long i = 1; i < FIO_SET_TEXT_COUNT; ++i) {
    obj_mem.i = i;
    FIO_SET_NAME(insert)(&s, i, obj_mem.obj);
    TEST_ASSERT(FIO_SET_NAME(find)(&s, i, obj_mem.obj),
                "find failed after insert");
    obj_mem.obj = *FIO_SET_NAME(find)(&s, i, obj_mem.obj);
    TEST_ASSERT(i == obj_mem.i, "insertion != find");
  }
  fprintf(stderr, "* Seeking %lu items\n", FIO_SET_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_SET_TEXT_COUNT; ++i) {
    obj_mem.i = i;
    obj_mem.obj = *FIO_SET_NAME(find)(&s, i, obj_mem.obj);
    TEST_ASSERT((i == obj_mem.i), "insertion != find (seek)");
  }
  {
    fprintf(stderr, "* Testing order for %lu items\n", FIO_SET_TEXT_COUNT);
    uintptr_t i = 1;
    FIO_SET_FOR_LOOP(&s, pos) {
      obj_mem.obj = pos->obj;
      TEST_ASSERT(obj_mem.i == i, "object order mismatch %lu != %lu.",
                  (unsigned long)i, (unsigned long)obj_mem.i);
      ++i;
    }
  }

  fprintf(stderr, "* Removing odd items from %lu items\n", FIO_SET_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_SET_TEXT_COUNT; i += 2) {
    obj_mem.i = i;
    FIO_SET_NAME(remove)(&s, i, obj_mem.obj);
    TEST_ASSERT(!(FIO_SET_NAME(find)(&s, i, obj_mem.obj)),
                "Removal failed (still exists).");
  }
  {
    fprintf(stderr, "* Testing for %lu / 2 holes\n", FIO_SET_TEXT_COUNT);
    uintptr_t i = 1;
    FIO_SET_FOR_LOOP(&s, pos) {
      obj_mem.obj = pos->obj;
      if (FIO_SET_COMPARE_HASH(pos->hash, FIO_SET_HASH_INVALID)) {
        TEST_ASSERT((i & 1) == 1, "deleted object wasn't odd");
      } else {
        TEST_ASSERT(obj_mem.i == i, "deleted object value mismatch %lu != %lu",
                    (unsigned long)i, (unsigned long)obj_mem.i);
      }
      ++i;
    }
    {
      fprintf(stderr, "* Poping two elements (testing pop through holes)\n");
      TEST_ASSERT(FIO_SET_NAME(last)(&s),
                  "Pop `last` 1 failed - no last object");
      obj_mem.obj = *FIO_SET_NAME(last)(&s);
      uintptr_t tmp_i = obj_mem.i;
      TEST_ASSERT(obj_mem.obj, "Pop `last` 1 failed to collect object");
      FIO_SET_NAME(pop)(&s);
      TEST_ASSERT(FIO_SET_NAME(last)(&s),
                  "Pop `last` 2 failed - no last object");
      obj_mem.obj = *FIO_SET_NAME(last)(&s);
      TEST_ASSERT(obj_mem.i != tmp_i,
                  "Pop `last` 2 same as `last` 1 - failed to collect object");
      FIO_SET_NAME(pop)(&s);
    }
    if (1) {
      obj_mem.i = 1;
      FIO_SET_NAME(remove)(&s, obj_mem.i, obj_mem.obj);
      size_t count = s.count;
      FIO_SET_NAME(overwrite)(&s, obj_mem.i, obj_mem.obj);
      TEST_ASSERT(count + 1 == s.count,
                  "Re-adding a removed item should increase count by 1 (%zu + "
                  "1 != %zu).",
                  count, (size_t)s.count);
      obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.i, obj_mem.obj);
      TEST_ASSERT(obj_mem.i == 1,
                  "Re-adding a removed item should update the item (%p != 1)!",
                  (void *)FIO_SET_NAME(find)(&s, obj_mem.i, obj_mem.obj));
      FIO_SET_NAME(remove)(&s, obj_mem.i, obj_mem.obj);
      TEST_ASSERT(count == s.count,
                  "Re-removing an item should decrease count (%zu != %zu).",
                  count, (size_t)s.count);
      TEST_ASSERT(!FIO_SET_NAME(find)(&s, obj_mem.i, obj_mem.obj),
                  "Re-removing a re-added item should update the item!");
    }
  }
  fprintf(stderr, "* Compacting Set to %lu\n", FIO_SET_TEXT_COUNT >> 1);
  FIO_SET_NAME(compact)(&s);
  {
    fprintf(stderr, "* Testing that %lu items are continuous\n",
            FIO_SET_TEXT_COUNT >> 1);
    uintptr_t i = 0;
    FIO_SET_FOR_LOOP(&s, pos) {
      TEST_ASSERT(!FIO_SET_COMPARE_HASH(pos->hash, FIO_SET_HASH_INVALID),
                  "Found a hole after compact.");
      ++i;
    }
    TEST_ASSERT(i == s.count, "count error (%lu != %lu).", i, s.count);
  }

  FIO_SET_NAME(free)(&s);
  TEST_ASSERT(!s.map && !s.ordered && !s.pos && !s.capa,
              "Set not re-initialized after free.");

  FIO_SET_NAME(capa_require)(&s, FIO_SET_TEXT_COUNT);

  TEST_ASSERT(
      s.map && s.ordered && !s.pos && s.capa >= FIO_SET_TEXT_COUNT,
      "capa_require changes state in a bad way (%p, %p, %zu, %zu ?>= %zu)",
      (void *)s.map, (void *)s.ordered, s.pos, s.capa, FIO_SET_TEXT_COUNT);

  for (unsigned long i = 1; i < FIO_SET_TEXT_COUNT; ++i) {
    obj_mem.i = i;
    FIO_SET_NAME(insert)(&s, obj_mem.i, obj_mem.obj);
    TEST_ASSERT(FIO_SET_NAME(find)(&s, obj_mem.i, obj_mem.obj),
                "find failed after insert (2nd round)");
    obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.i, obj_mem.obj);
    TEST_ASSERT(i == obj_mem.i, "insertion (2nd round) != find");
    TEST_ASSERT(i == s.count, "count error (%lu != %lu) post insertion.", i,
                s.count);
  }
  FIO_SET_NAME(free)(&s);
}

#undef TEST_ASSERT
#endif /* DEBUG Testing */

#undef FIO_SET_OBJECT_TYPE
#undef FIO_SET_COMPARE_OBJ
#undef FIO_SET_COMPARE_OBJ
#undef FIO_SET_OBJ_COPY
#undef FIO_SET_OBJ_DESTROY
#undef FIO_SET_HASH_TYPE
#undef FIO_SET_HASH2UINTPTR
#undef FIO_SET_COMPARE_HASH
#undef FIO_SET_HASH_INVALID
#undef FIO_SET_MAX_MAP_SEEK
#undef FIO_SET_REALLOC
#undef FIO_SET_CALLOC
#undef FIO_SET_FREE
#undef FIO_SET_NAME
#undef FIO_FUNC
