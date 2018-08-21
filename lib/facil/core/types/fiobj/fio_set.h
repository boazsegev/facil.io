/*
Copyright: Boaz Segev, 2018
License: MIT
*/

/**
 * A simple ordered Set implementation, with a minimal API.
 *
 * ##  What is a Set?
 *
 * A Set (also reffered to as a "Bag") is simple data-storage that promises the
 * data in the Set is unique (no two objects are equal).
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
 * hash collisions prompt an identity test.
 *
 * The Set's object types are adjustable using macros. By default, Objects are a
 * `void *` type. This makes it easy to implement more complex data structures
 * that are derived from the Set (such as HashMaps or caching schemes).
 *
 * The Set also requires each object to have a unique hash integer value that
 * will be used for mapping the objects for quick access. By default, hash
 * values are a `uint64_t` type.
 *
 * Partial hash collisions are handled by seeking forward and attempting to find
 * a close enough spot. If a close enough spot isn't found, rehashing is
 * initiated and memory consumption increases.
 *
 * Full hash collisions could break the Set after objects are removed (deleted)
 * from the Set, because the object is removed and equality tests can't be
 * performed. However, facil.io's implementation automatically protects against
 * this scenarion by detecting collisions and rehashing the Set when required.
 *
 * The file was written to be compatible with C++ as well as C, hence some
 * pointer casting.
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
 * * FIO_SET_HASH_TYPE              - the type of the hash value. Should be an
 *                                    integer value.
 * * FIO_SET_COMPARE_HASH(h1, h2)   - compares two hash values (1 == equal).
 * * FIO_SET_HASH_INVALID           - an invalid Hash value (defaults to 0).
 *
 * By defining ALL of the following macros, the Set's type and functionality can
 * be adjusted:
 * * FIO_SET_NAME(name)             - allows a prefixe to be added to name.
 * * FIO_SET_OBJECT_TYPE            - the type used for set objects.
 * * FIO_SET_OBJECT_INVALID         - an invalid object, it's bytes set to zero.
 * * FIO_SET_OBJ2HASH(obj)          - converts an object to a hash value number.
 *
 * The following macros allow further control over the Set's behavior:
 * * FIO_SET_COMPARE_OBJ(o1, o2)    - compares two objects (1 == equal).
 * * FIO_SET_OBJ_ISINVALID(obj)     - tests for an invalid object.
 * * FIO_SET_OBJ_COPY(obj)          - creates a persistent copy of the object.
 * * FIO_SET_OBJ_DESTROY(obj)       - destroys (or frees) the object's copy.
 *
 * Note: FIO_SET_NAME(name) defaults to: fio_ptr_set_##name`.
 *
 * Note: FIO_SET_OBJ2HASH must never be 0 for valid objects.
 *
 * Note: FIO_SET_COMPARE will be used to compare against
 *       FIO_SET_OBJECT_INVALID as well as valid keys.
 *
 * Note: Before freeing the Set, FIO_SET_OBJ_DESTROY should be called for
 *       every object. This is NOT automatic. see the FIO_SET_FOR_LOOP(h) macro.
 */
#if !defined(FIO_SET_OBJECT_TYPE) || !defined(FIO_SET_NAME) ||                 \
    !defined(FIO_SET_OBJ2HASH) || !defined(FIO_SET_OBJECT_INVALID)
#define FIO_SET_NAME(name) fio_ptr_set_##name
#define FIO_SET_OBJECT_TYPE void *
#define FIO_SET_OBJECT_INVALID NULL
#define FIO_SET_OBJ2HASH(obj) ((FIO_SET_HASH_TYPE)(obj))
#elif !defined(FIO_SET_NO_TEST)
#define FIO_SET_NO_TEST 1
#endif

#if !defined(FIO_SET_COMPARE_OBJ)
#define FIO_SET_COMPARE_OBJ(o1, o2) ((o1) == (o2))
#endif

#if !defined(FIO_SET_OBJ_ISINVALID)
#define FIO_SET_OBJ_ISINVALID(obj)                                             \
  FIO_SET_COMPARE_OBJ((obj), FIO_SET_OBJECT_INVALID)
#endif

/** test for a pre-defined object copy */
#if !defined(FIO_SET_OBJ_COPY) || !defined(FIO_SET_OBJ_DESTROY)
#define FIO_SET_OBJ_COPY(obj) (obj)
#define FIO_SET_OBJ_DESTROY(obj) ((void)0)
#endif

/** test for a pre-defined hash type */
#if !defined(FIO_SET_HASH_TYPE) || !defined(FIO_SET_HASH_INVALID)
#define FIO_SET_HASH_TYPE uint64_t
#define FIO_SET_HASH_INVALID 0
#endif

#ifndef FIO_SET_COMPARE_HASH
#define FIO_SET_COMPARE_HASH(h1, h2) (h1 == h2)
#endif

/* MUST be a power of 2 */
#ifndef FIO_SET_MAX_MAP_SEEK
#define FIO_SET_MAX_MAP_SEEK (256)
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
    FIO_SET_NAME(find)(FIO_SET_NAME(s) * set, FIO_SET_OBJECT_TYPE obj);

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 */
FIO_FUNC FIO_SET_OBJECT_TYPE *FIO_SET_NAME(insert)(FIO_SET_NAME(s) * set,
                                                   FIO_SET_OBJECT_TYPE obj);

/**
 * Removes an object from the Set, rehashing if required.
 */
FIO_FUNC void FIO_SET_NAME(remove)(FIO_SET_NAME(s) * set,
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
FIO_FUNC void FIO_SET_NAME(pop)(FIO_SET_NAME(s) * set);

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
 * to skip any `FIO_SET_OBJ_ISINVALID(pos->obj)`.
 */
#define FIO_SET_FOR_LOOP(set, pos)
#endif

/* *****************************************************************************
Hash Table Internal Data Structures
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
    FIO_SET_NAME(_find_map_pos_)(FIO_SET_NAME(s) * set, FIO_SET_OBJECT_TYPE obj,
                                 const FIO_SET_HASH_TYPE hashed_obj) {
  if (!set->map)
    return NULL;

  /* make sure collisions don't effect seeking */
  if (set->has_collisions && set->pos != set->count) {
    FIO_SET_NAME(rehash)(set);
  }

  FIO_SET_NAME(_map_s_) *pos = set->map + (hashed_obj & set->mask);
  uintptr_t i = 0;
  const uintptr_t limit = set->capa > FIO_SET_MAX_MAP_SEEK
                              ? FIO_SET_MAX_MAP_SEEK
                              : ((set->capa >> 1) | 1);
  while (i < limit) {
    if (FIO_SET_COMPARE_HASH(FIO_SET_HASH_INVALID, pos->hash) && !pos->pos)
      return pos;
    if (pos->hash == hashed_obj) {
      if (!pos->pos || FIO_SET_COMPARE_OBJ(pos->pos->obj, obj))
        return pos;
      set->has_collisions = 1;
    }
    pos = set->map + (((hashed_obj & set->mask) + ((i++) * 3)) & set->mask);
  }
  return NULL;
}

/** Removes "holes" from the Set's internal Array - MUST re-hash afterwards. */
FIO_FUNC inline void FIO_SET_NAME(_compact_ordered_array_)(FIO_SET_NAME(s) *
                                                           set) {
  if (!set->ordered || !set->pos || set->count == set->pos)
    return;
  FIO_SET_NAME(_ordered_s_) *reader = set->ordered;
  FIO_SET_NAME(_ordered_s_) *writer = set->ordered;
  const FIO_SET_NAME(_ordered_s_) *end = set->ordered + set->pos;
  for (; reader && (reader < end); ++reader) {
    if (FIO_SET_COMPARE_HASH(reader->hash, FIO_SET_HASH_INVALID) ||
        FIO_SET_OBJ_ISINVALID(reader->obj)) {
      FIO_SET_OBJ_DESTROY(reader->obj);
      continue;
    }
    *writer = *reader;
    ++writer;
  }
  /* fix any possible counting errors as well as resetting position */
  if (set->count != (uintptr_t)(writer - set->ordered))
    fprintf(stderr, "ERROR: Set length counting error detected %zu != %zu.\n",
            (size_t)set->count, (size_t)(writer - set->ordered));
  set->pos = set->count = (writer - set->ordered);
}

/* *****************************************************************************
Set Implementation
***************************************************************************** */

/** Deallocates any internal resources. Doesn't free any objects! */
FIO_FUNC void FIO_SET_NAME(free)(FIO_SET_NAME(s) * s) {
  FIO_SET_FREE(s->map, s->capa * sizeof(*s->map));
  FIO_SET_FREE(s->ordered, s->capa * sizeof(*s->ordered));
  *s = (FIO_SET_NAME(s)){.map = NULL};
}

/** Locates an object in the Set, if it exists. */
FIO_FUNC inline FIO_SET_OBJECT_TYPE *
FIO_SET_NAME(find)(FIO_SET_NAME(s) * set, FIO_SET_OBJECT_TYPE obj) {
  const FIO_SET_HASH_TYPE hashed_obj = FIO_SET_OBJ2HASH(obj);
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, obj, hashed_obj);
  if (!pos || !pos->pos)
    return NULL;
  return &pos->pos->obj;
}

/**
 * Inserts an object to the Set, rehashing if required, returning the new
 * object's pointer.
 */
FIO_FUNC FIO_SET_OBJECT_TYPE *FIO_SET_NAME(insert)(FIO_SET_NAME(s) * set,
                                                   FIO_SET_OBJECT_TYPE obj) {
  if (FIO_SET_OBJ_ISINVALID(obj))
    return NULL;

  /* automatic fragmentation protection */
  if (FIO_SET_NAME(is_fragmented)(set))
    FIO_SET_NAME(rehash)(set);

  /* locate future position, rehashing until a position is available */
  const FIO_SET_HASH_TYPE hashed_obj = FIO_SET_OBJ2HASH(obj);
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, obj, hashed_obj);
  while (!pos) {
    set->mask = (set->mask << 1) | 1;
    FIO_SET_NAME(rehash)(set);
    pos = FIO_SET_NAME(_find_map_pos_)(set, obj, hashed_obj);
  }

  /* overwriting / new */
  if (pos->pos) {
    /* overwrite existing object */
    FIO_SET_OBJ_DESTROY(pos->pos->obj);
  } else {
    /* insert into new slot */
    pos->pos = set->ordered + set->pos;
    ++set->pos;
    ++set->count;
  }

  /* store object at position */
  pos->hash = hashed_obj;
  pos->pos->hash = hashed_obj;
  pos->pos->obj = FIO_SET_OBJ_COPY(obj);
  return &pos->pos->obj;
}

/**
 * Removes an object from the Set, rehashing if required.
 */
FIO_FUNC void FIO_SET_NAME(remove)(FIO_SET_NAME(s) * set,
                                   FIO_SET_OBJECT_TYPE obj) {
  if (FIO_SET_OBJ_ISINVALID(obj))
    return;
  const FIO_SET_HASH_TYPE hashed_obj = FIO_SET_OBJ2HASH(obj);
  FIO_SET_NAME(_map_s_) *pos =
      FIO_SET_NAME(_find_map_pos_)(set, obj, hashed_obj);
  if (!pos || !pos->pos)
    return;
  FIO_SET_OBJ_DESTROY(pos->pos->obj);
  --set->count;
  pos->pos->obj = FIO_SET_OBJECT_INVALID;
  pos->pos->hash = FIO_SET_HASH_INVALID;
  if (pos->pos == set->pos + set->ordered - 1) {
    do {
      --set->pos;
    } while (set->pos && FIO_SET_OBJ_ISINVALID(set->ordered[set->pos - 1].obj));
  }
  pos->pos = NULL; /* leave pos->hash to mark "hole" */
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
  set->ordered[set->pos - 1].obj = FIO_SET_OBJECT_INVALID;
  set->ordered[set->pos - 1].hash = FIO_SET_HASH_INVALID;
  --(set->count);
  do {
    --(set->pos);
  } while (set->pos && FIO_SET_OBJ_ISINVALID(set->ordered[set->pos - 1].obj));
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
  return (set->count && ((set->pos - set->count) > (set->count >> 1)));
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
  FIO_SET_NAME(_ordered_s_) * end;
  set->has_collisions = 0;
restart:
  FIO_SET_FREE(set->map, set->capa * sizeof(*set->map));
  set->map = (FIO_SET_NAME(_map_s_) *)FIO_SET_CALLOC(sizeof(*set->map),
                                                     (set->mask + 1));
  set->ordered = (FIO_SET_NAME(_ordered_s_) *)FIO_SET_REALLOC(
      set->ordered, set->capa * sizeof(*set->ordered),
      (set->mask + 1) * sizeof(*set->ordered),
      set->pos * sizeof(*set->ordered));
  if (!set->map || !set->ordered) {
    perror("FATAL ERROR: couldn't allocate memory for Set data");
    exit(errno);
  }
  set->capa = set->mask + 1;
  end = set->ordered + set->pos;
  for (FIO_SET_NAME(_ordered_s_) *pos = set->ordered; pos && (pos < end);
       ++pos) {
    FIO_SET_NAME(_map_s_) *mp =
        FIO_SET_NAME(_find_map_pos_)(set, pos->obj, pos->hash);
    if (!mp) {
      set->mask = (set->mask << 1) | 1;
      goto restart;
    }
    mp->pos = pos;
    mp->hash = pos->hash;
  }
}

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
    FIO_SET_NAME(insert)(&s, obj_mem.obj);
    TEST_ASSERT(FIO_SET_NAME(find)(&s, obj_mem.obj),
                "find failed after insert");
    obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.obj);
    TEST_ASSERT(i == obj_mem.i, "insertion != find");
  }
  fprintf(stderr, "* Seeking %lu items\n", FIO_SET_TEXT_COUNT);
  for (unsigned long i = 1; i < FIO_SET_TEXT_COUNT; ++i) {
    obj_mem.i = i;
    obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.obj);
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
    FIO_SET_NAME(remove)(&s, obj_mem.obj);
    TEST_ASSERT(!(FIO_SET_NAME(find)(&s, obj_mem.obj)),
                "Removal failed (still exists).");
  }
  {
    fprintf(stderr, "* Testing for %lu / 2 holes\n", FIO_SET_TEXT_COUNT);
    uintptr_t i = 1;
    FIO_SET_FOR_LOOP(&s, pos) {
      obj_mem.obj = pos->obj;
      if (FIO_SET_OBJ_ISINVALID(pos->obj)) {
        TEST_ASSERT(obj_mem.i == 0, "deleted object still has value.");
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
      FIO_SET_NAME(remove)(&s, obj_mem.obj);
      size_t count = s.count;
      FIO_SET_NAME(insert)(&s, obj_mem.obj);
      TEST_ASSERT(count + 1 == s.count,
                  "Re-adding a removed item should increase count by 1 (%zu + "
                  "1 != %zu).",
                  count, (size_t)s.count);
      obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.obj);
      TEST_ASSERT(obj_mem.i == 1,
                  "Re-adding a removed item should update the item (%p != 1)!",
                  (void *)FIO_SET_NAME(find)(&s, obj_mem.obj));
      FIO_SET_NAME(remove)(&s, obj_mem.obj);
      TEST_ASSERT(count == s.count,
                  "Re-removing an item should decrease count (%zu != %zu).",
                  count, (size_t)s.count);
      TEST_ASSERT(!FIO_SET_NAME(find)(&s, obj_mem.obj),
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
      TEST_ASSERT(!FIO_SET_OBJ_ISINVALID(pos->obj),
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
    FIO_SET_NAME(insert)(&s, obj_mem.obj);
    TEST_ASSERT(FIO_SET_NAME(find)(&s, obj_mem.obj),
                "find failed after insert (2nd round)");
    obj_mem.obj = *FIO_SET_NAME(find)(&s, obj_mem.obj);
    TEST_ASSERT(i == obj_mem.i, "insertion (2nd round) != find");
    TEST_ASSERT(i == s.count, "count error (%lu != %lu) post insertion.", i,
                s.count);
  }
  FIO_SET_NAME(free)(&s);
}

#undef TEST_ASSERT
#endif /* DEBUG Testing */

#undef FIO_SET_OBJECT_TYPE
#undef FIO_SET_OBJECT_INVALID
#undef FIO_SET_OBJ2HASH
#undef FIO_SET_COMPARE_OBJ
#undef FIO_SET_COMPARE_OBJ
#undef FIO_SET_OBJ_COPY
#undef FIO_SET_OBJ_DESTROY
#undef FIO_SET_HASH_TYPE
#undef FIO_SET_COMPARE_HASH
#undef FIO_SET_HASH_INVALID
#undef FIO_SET_MAX_MAP_SEEK
#undef FIO_SET_REALLOC
#undef FIO_SET_CALLOC
#undef FIO_SET_FREE
#undef FIO_SET_NAME
#undef FIO_FUNC
