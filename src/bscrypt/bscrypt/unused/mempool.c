/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mempool.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

/* *****************************************************************************
A Simple busy lock implementation ... (spnlock.h) Included here to make portable
*/
#ifndef SIMPLE_SPN_LOCK_H
#define SIMPLE_SPN_LOCK_H
SIMPLE_SPN_LOCK_H

/* allow of the unused flag */
#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

#include <stdint.h>
#include <stdlib.h>

/*********
 * manage the way threads "wait" for the lock to release
 */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
/* nanosleep seems to be the most effective and efficient reschedule */
#include <time.h>
#define reschedule_thread()                                                    \
  {                                                                            \
    static const struct timespec tm = {.tv_nsec = 1};                          \
    nanosleep(&tm, NULL);                                                      \
  }

#else /* no effective rescheduling, just spin... */
#define reschedule_thread()

#endif
/* end `reschedule_thread` block*/

/*********
 * The spin lock core functions (spn_trylock, spn_unlock, is_spn_locked)
 */

/* prefer C11 standard implementation where available (trust the system) */
#if defined(__has_include)
#if __has_include(<stdatomic.h>)
#define SPN_TMP_HAS_ATOMICS 1
#include <stdatomic.h>
typedef atomic_bool spn_lock_i;
#define SPN_LOCK_INIT ATOMIC_VAR_INIT(0)
/** returns 1 if the lock was busy (TRUE == FAIL). */
UNUSED_FUNC static inline int spn_trylock(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  return atomic_exchange(lock, 1);
}
/** Releases a lock. */
UNUSED_FUNC static inline void spn_unlock(spn_lock_i *lock) {
  atomic_store(lock, 0);
  __asm__ volatile("" ::: "memory");
}
/** returns a lock's state (non 0 == Busy). */
UNUSED_FUNC static inline int spn_is_locked(spn_lock_i *lock) {
  return atomic_load(lock);
}
#endif
#endif

/* Chack if stdatomic was available */
#ifdef SPN_TMP_HAS_ATOMICS
#undef SPN_TMP_HAS_ATOMICS

#else
/* Test for compiler builtins */

/* use clang builtins if available - trust the compiler */
#if defined(__clang__)
#if defined(__has_builtin) && __has_builtin(__sync_swap)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
UNUSED_FUNC static inline int spn_trylock(spn_lock_i *lock) {
  return __sync_swap(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif
/* use gcc builtins if available - trust the compiler */
#elif defined(__GNUC__) &&                                                     \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
UNUSED_FUNC static inline int spn_trylock(spn_lock_i *lock) {
  return __sync_fetch_and_or(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif

/* Check if compiler builtins were available, if not, try assembly*/
#if SPN_TMP_HAS_BUILTIN
#undef SPN_TMP_HAS_BUILTIN

/* use Intel's asm if on Intel - trust Intel's documentation */
#elif defined(__amd64__) || defined(__x86_64__) || defined(__x86__) ||         \
    defined(__i386__) || defined(__ia64__) || defined(_M_IA64) ||              \
    defined(__itanium__) || defined(__i386__)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
UNUSED_FUNC static inline int spn_trylock(spn_lock_i *lock) {
  spn_lock_i tmp;
  __asm__ volatile("xchgb %0,%1" : "=r"(tmp), "=m"(*lock) : "0"(1) : "memory");
  return tmp;
}

/* use SPARC's asm if on SPARC - trust the design */
#elif defined(__sparc__) || defined(__sparc)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns TRUE (non-zero) if the lock was busy (TRUE == FAIL). */
UNUSED_FUNC static inline int spn_trylock(spn_lock_i *lock) {
  spn_lock_i tmp;
  __asm__ volatile("ldstub    [%1], %0" : "=r"(tmp) : "r"(lock) : "memory");
  return tmp; /* return 0xFF if the lock was busy, 0 if free */
}

#else
/* I don't know how to provide green thread safety on PowerPC or ARM */
#error "Couldn't implement a spinlock for this system / compiler"
#endif /* types and atomic exchange */
/** Initialization value in `free` state. */
#define SPN_LOCK_INIT 0

/** Releases a lock. */
UNUSED_FUNC static inline void spn_unlock(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  *lock = 0;
}
/** returns a lock's state (non 0 == Busy). */
UNUSED_FUNC static inline int spn_is_locked(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  return *lock;
}

#endif /* has atomics */
#include <stdio.h>
/** Busy waits for the lock. */
UNUSED_FUNC static inline void spn_lock(spn_lock_i *lock) {
  while (spn_trylock(lock)) {
    reschedule_thread();
  }
}

/* *****************************************************************************
spnlock.h finished
*/
#endif

/* *****************************************************************************
The memory pool slices. Overhead 8 bytes, minimum allocation 16 bytes (+ 8).
*/
#define MEMSLICE_SIZE ((uint32_t)(1UL << 21))
#define OFFSET_LIMIT (MEMSLICE_SIZE - 1)
typedef struct slice_s {
  struct slice_s_offset { /* 64bit alignment also where pointers are 32bits. */
    uint32_t ahead;
    uint32_t behind;
  } offset;
  struct slice_s *next; /** Used for the free block linked list. */
} slice_s;

/* *****************************************************************************
Slice navigation / properties.
*/

#define SLICE_USED_MARKER ((uint32_t)(~0UL << 21))
#define SLICE_INDI_MARKER ((uint32_t)0xF7F7F7F7UL)
#define SLICE_USED_MASK (MEMSLICE_SIZE - 1)

#define slice_size(slice) ((slice)->offset.ahead & SLICE_USED_MASK)
#define slice_indi_size(slice) ((slice)->offset.ahead)
#define slice_set_size(slice, size)                                            \
  ((slice)->offset.ahead = ((size) | SLICE_USED_MARKER))
#define slice_set_indisize(slice, size) ((slice)->offset.ahead = (size))

#define slice_is_used(slice)                                                   \
  (((slice)->offset.ahead & ~(SLICE_USED_MASK)) == SLICE_USED_MARKER)
#define slice_is_free(slice) (((slice)->offset.ahead & ~(SLICE_USED_MASK)) == 0)
#define slice_is_indi(slice) ((slice)->offset.behind == SLICE_INDI_MARKER)
#define slice_set_used(slice) ((slice)->offset.ahead |= SLICE_USED_MARKER)
#define slice_set_free(slice) ((slice)->offset.ahead &= SLICE_USED_MASK)

static inline slice_s *slice_ahead(slice_s *slice) {
  return ((slice_s *)(((uintptr_t)slice) +
                      (slice->offset.ahead & SLICE_USED_MASK)));
}
static inline slice_s *slice_behind(slice_s *slice) {
  return slice->offset.behind == 0
             ? NULL
             : ((slice_s *)(((uintptr_t)slice) - slice->offset.behind));
}

static inline int slice_is_whole(slice_s *slice) {
  return slice->offset.behind == 0 &&
         ((slice_s *)(((uintptr_t)slice) +
                      (slice->offset.ahead & SLICE_USED_MASK)))
                 ->offset.ahead == 0;
}

static inline void slice_notify(slice_s *slice) {
  slice_s *ahead = slice_ahead(slice);
  ahead->offset.behind = slice_size(slice);
}

static inline void *slice_buffer(slice_s *slice) {
  return (void *)(((uintptr_t)slice) + sizeof(struct slice_s_offset));
}
static inline slice_s *buffer2slice(void *ptr) {
  return (void *)(((uintptr_t)ptr) - sizeof(struct slice_s_offset));
}

/* *****************************************************************************
Slice list/tree.
*/

static inline slice_s *_slice_extract(slice_s **slice) {
  slice_s *ret = *slice;
  slice_set_used(ret);
  *slice = ret->next;
  return ret;
}

static slice_s *slice_remove(slice_s **tree, slice_s *slice) {
  while (*tree) {
    if (*tree == slice) {
      return _slice_extract(tree);
    }
    tree = &(*tree)->next;
  }
  return NULL;
}

static slice_s *slice_pop(slice_s **tree, uint32_t size) {
  // fprintf(stderr, "Slice pop called for %p, search for size %u\n", tree,
  // size);
  while (*tree && slice_size(*tree) < size) {
    tree = &(*tree)->next;
  }
  return *tree ? _slice_extract(tree) : NULL;
}

/* places a childless slice on top of the tree */
static void slice_push(slice_s **tree, slice_s *slice) {
  slice_set_free(slice);
  slice->next = *tree;
  *tree = slice;
}

/* *****************************************************************************
The memory pool data structures and related helper macros / functions.
*/

/** The root of the memory pool. */
struct bs_mmpl_s {
  void *(*alloc)(size_t size, void *arg); /* NUST start same as settings */
  void (*unalloc)(void *ptr, size_t size, void *arg);
  void *arg;
  slice_s *available;
  spn_lock_i lock;
};

#define lock(pool) spn_lock(&((pool)->lock))
#define unlock(pool) spn_unlock(&((pool)->lock))

/* *****************************************************************************
Slice allocation / free
*/

static slice_s *allocate_slice(bs_mmpl_ptr pool) {
  slice_s *slice = pool->alloc(MEMSLICE_SIZE, pool->arg);
  if (slice == NULL)
    return NULL;
  /* zero out first and last slices. */
  *((slice_s *)((uintptr_t)slice + MEMSLICE_SIZE - sizeof(*slice))) =
      (slice_s){.offset.ahead = 0};
  *slice = (slice_s){.offset.ahead = 0, .offset.behind = 0};
  slice_set_size(slice, MEMSLICE_SIZE - sizeof(*slice));
  slice_notify(slice);
  return slice;
}

static slice_s *allocate_indi(bs_mmpl_ptr pool, size_t size) {
  slice_s *slice = pool->alloc(size, pool->arg);
  if (slice == NULL)
    return NULL;
  /* zero out first and list slices. */
  *slice = (slice_s){.offset.ahead = size, .offset.behind = SLICE_INDI_MARKER};
  return slice;
}

/** merges with "ahead" if possible */
static int slice_expand(bs_mmpl_ptr pool, slice_s *slice) {
  slice_s *ahead = slice_ahead(slice);
  if (slice_size(ahead) == 0 || slice_is_used(ahead))
    return -1;
  slice_set_size(slice, slice_size(slice) + slice_size(ahead));
  slice_remove(&(pool->available), ahead);
  slice_notify(slice); /* (updates the "ahead" neighbore) */
  return 0;
}

// void bs_mempool_print(void* _branch);

static void free_slice(bs_mmpl_ptr pool, slice_s *slice) {
  if (!slice_is_used(slice))
    goto other;
  slice_expand(pool, slice); /* merges with "ahead" if possible */
  slice_s *behind = slice_behind(slice);
  if (behind && slice_size(behind) && slice_is_free(behind)) {
    slice_set_size(behind, slice_size(slice) + slice_size(behind));
    slice = behind;
    slice_set_free(slice);
    slice_notify(slice); /* (updates the "ahead" neighbore) */
    /* merge with "behind" */
    // if (slice_remove(&(pool->available), behind) == NULL) {
    //   fprintf(stderr, "ERROR!!! couldn't remove slice from memory pool %p\n",
    //           behind);
    //   // bs_mempool_print(pool->available);
    //   exit(6); }
  } else {
    slice_push(&(pool->available), slice);
  }
  if (!slice_is_whole(slice)) {
    // slice_push(&pool->available, slice);
    return;
  }
  slice_remove(&(pool->available), slice);
  pool->unalloc(slice, MEMSLICE_SIZE, pool->arg);
  return;
other:
  if (slice_is_indi(slice)) {
    pool->unalloc(slice, slice_indi_size(slice), pool->arg);
    return;
  }
  errno = EFAULT;
  raise(SIGSEGV); /* support longjmp rescue */
  exit(EFAULT);
}

/**
 * takes a peice of a slice (or the whole thing), returning the rest to the
 * available pool.
 *
 * UNSAFE(?): always assumes `slice_size(slice) >= size` is
 */
static slice_s *slice_cut(bs_mmpl_ptr pool, slice_s *slice, size_t size) {
  if (slice_size(slice) - size <= sizeof(slice))
    return slice;
  slice_s *nslice = (slice_s *)(((uintptr_t)slice) + size);
  *nslice = (slice_s){};
  slice_set_size(nslice, slice_size(slice) - size);
  slice_set_size(slice, size);
  nslice->offset.behind = size;
  slice_notify(nslice);
  slice_push(&pool->available, nslice);
  return slice;
}

/* *****************************************************************************
Default memory allocation behavior
*/

// clang-format off
#ifndef BSMMPL_USE_MMAP
#   if defined(__has_include) && __has_include(<sys/mman.h>)
#      include <sys/mman.h>
#      define BSMMPL_USE_MMAP 1
#   else
#      warning bsmmlp memory pool will allocate memory block using malloc.
#   endif
#endif
// clang-format on

#if defined(BSMMPL_USE_MMAP) && BSMMPL_USE_MMAP

static void *default_alloc(size_t size, void *arg) {
  void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ret == MAP_FAILED)
    return NULL;
  return ret;
}

static void default_unalloc(void *ptr, size_t size, void *arg) {
  munmap(ptr, size);
}

#else

static void *default_alloc(size_t size, void *_arg) { return malloc(size); }
static void default_unalloc(void *ptr, size_t _size, void *_arg) { free(ptr); }

#endif

/* *****************************************************************************
Initialization
*/

/** creates the memory pool object */
#undef bs_mempool_create
bs_mmpl_ptr bs_mempool_create(struct bs_mempool_settings settings) {
  if (settings.alloc == NULL || settings.unalloc == NULL) {
    settings.alloc = default_alloc;
    settings.unalloc = default_unalloc;
  }

  slice_s *root = allocate_slice((bs_mmpl_ptr)&settings);
  bs_mmpl_ptr pool = slice_buffer(root);
  pool->alloc = settings.alloc;
  pool->unalloc = settings.unalloc;
  pool->arg = settings.arg;
  pool->available = NULL;
  slice_cut(pool, root, sizeof(root->offset) + sizeof(struct bs_mmpl_s));
  slice_set_used(root);
  return pool;
}
#define bs_mempool_create(...)                                                 \
  bs_mempool_create((struct bs_mempool_settings){__VA_ARGS__})

/* *****************************************************************************
Allocation.
*/
void *bs_malloc(bs_mmpl_ptr pool, size_t size) {
  slice_s *slice = NULL;
  if (size < ((sizeof(void *) << 1))) /* minimal block size */
    size = ((sizeof(void *) << 1));
  else if (size & 7) /* align to 8 bytes */
    size = ((size >> 3) + 1) << 3;
  size += sizeof(struct slice_s_offset);
  if (size > MEMSLICE_SIZE - 1024)
    return slice_buffer(allocate_indi(pool, size));
  lock(pool);
  slice = slice_pop(&pool->available, size);
  if (slice == NULL)
    slice = allocate_slice(pool);
  if (slice == NULL)
    return NULL;
  slice_cut(pool, slice, size);
  unlock(pool);
  return slice_buffer(slice);
}

/* *****************************************************************************
De-Allocation
*/

void bs_free(bs_mmpl_ptr pool, void *ptr) {
  slice_s *slice = buffer2slice(ptr);
  lock(pool);
  free_slice(pool, slice);
  unlock(pool);
}

/* *****************************************************************************
Reallocation (resizing)
*/
void *bs_realloc(bs_mmpl_ptr pool, void *ptr, size_t new_size) {
  slice_s *slice = buffer2slice(ptr);
  uint32_t original_size = slice_size(slice);
  new_size += sizeof(struct slice_s_offset);
  if (new_size < ((sizeof(void *) << 1))) /* minimal block size */
    new_size = ((sizeof(void *) << 1));
  else if (new_size & 7) /* align to 8 bytes */
    new_size = ((new_size >> 3) + 1) << 3;
  lock(pool);
  while ((slice_size(slice) < new_size) && slice_expand(pool, slice) == 0)
    ;
  if (slice_size(slice) < new_size) {
    unlock(pool);
    void *new_mem = bs_malloc(pool, new_size - sizeof(struct slice_s_offset));
    memcpy(new_mem, ptr, original_size - sizeof(struct slice_s_offset));
    bs_free(pool, ptr);
    return new_mem;
  }
  slice_cut(pool, slice, new_size);
  unlock(pool);
  return slice_buffer(slice);
}

/* *****************************************************************************
Testing
*/

#if defined(DEBUG) && DEBUG == 1

#define MEMTEST_SLICE 48
#define MEMTEST_REPEATS (1024 * 1024)

#include <stdio.h>
#include <time.h>
static void bs_mempool_stats(void) {
  fprintf(stderr, "bsmmpl properties (hardcoded):\n"
                  "* Pool object: %lu bytes\n"
                  "* Alignment: %lu (memory border)\n"
                  "* Minimal Allocateion Size (including header): %lu\n"
                  "* Minimal Allocation Space (no header): %lu\n"
                  "* Header size: %lu\n",
          sizeof(struct bs_mmpl_s), sizeof(struct slice_s_offset),
          sizeof(slice_s), sizeof(slice_s) - sizeof(struct slice_s_offset),
          sizeof(struct slice_s_offset));
}

static void bs_mempool_speedtest(void *(*mlk)(size_t), void (*fr)(void *)) {
  void **pntrs = mlk(MEMTEST_REPEATS * sizeof(*pntrs));
  clock_t start, end, mlk_time, fr_time, zr_time;
  mlk_time = 0;
  fr_time = 0;
  zr_time = 0;

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    __asm__ volatile("" ::: "memory");
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr, "* Doing nothing: %lu CPU cycles.\n", mlk_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    // fprintf(stderr, "malloc %lu\n", i);
    pntrs[i] = mlk(MEMTEST_SLICE);
    *((uint8_t *)pntrs[i]) = 1;
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Allocating %d consecutive blocks %d each: %lu CPU cycles.\n",
          MEMTEST_REPEATS, MEMTEST_SLICE, mlk_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i += 2) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i += 2) {
    pntrs[i] = mlk(MEMTEST_SLICE);
  }
  end = clock();
  mlk_time = end - start;

  fprintf(stderr,
          "* Freeing %d Fragmented (single space) blocks %d each: %lu CPU "
          "cycles.\n",
          MEMTEST_REPEATS / 2, MEMTEST_SLICE, fr_time);

  fprintf(stderr, "* Allocating %d Fragmented (single space) blocks %d "
                  "each: %lu CPU "
                  "cycles.\n",
          MEMTEST_REPEATS / 2, MEMTEST_SLICE, mlk_time);

  mlk_time = 0;
  fr_time = 0;

  for (size_t xtimes = 0; xtimes < 100; xtimes++) {
    start = clock();
    for (size_t i = 0; i < MEMTEST_REPEATS; i += 7) {
      fr(pntrs[i]);
    }
    end = clock();
    fr_time += end - start;

    start = clock();
    for (size_t i = 0; i < MEMTEST_REPEATS; i += 7) {
      pntrs[i] = mlk(MEMTEST_SLICE);
    }
    end = clock();
    mlk_time += end - start;
  }

  fprintf(stderr,
          "* 100X Freeing %d Fragmented (7 spaces) blocks %d each: %lu CPU "
          "cycles.\n",
          MEMTEST_REPEATS / 7, MEMTEST_SLICE, fr_time);

  fprintf(stderr, "* 100X Allocating %d Fragmented (7 spaces) blocks %d "
                  "each: %lu CPU "
                  "cycles.\n",
          MEMTEST_REPEATS / 7, MEMTEST_SLICE, mlk_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    memset(pntrs[i], 255, MEMTEST_SLICE);
  }
  end = clock();
  zr_time = end - start;
  fprintf(stderr, "* Zero out %d consecutive blocks %d "
                  "each: %lu CPU cycles.\n",
          MEMTEST_REPEATS, MEMTEST_SLICE, zr_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;
  fprintf(stderr, "* Freeing %d consecutive blocks %d each: %lu CPU cycles.\n",
          MEMTEST_REPEATS, MEMTEST_SLICE, fr_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    pntrs[i] = mlk(MEMTEST_SLICE);
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Allocating %d consecutive blocks %d each: %lu CPU cycles.\n",
          MEMTEST_REPEATS, MEMTEST_SLICE, mlk_time);
  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;
  fprintf(stderr, "* Freeing %d consecutive blocks %d each: %lu CPU cycles.\n",
          MEMTEST_REPEATS, MEMTEST_SLICE, fr_time);
  fprintf(stderr, "* Freeing pointer array %p.\n", pntrs);
  fr(pntrs);
  fprintf(stderr, "Done.\n");
}

#undef MEMTEST_SLICE
#undef MEMTEST_REPEATS

static bs_mmpl_ptr test_pool = NULL;

static void *_bs_mmpl_wrapper_bs_malloc(size_t size) {
  return bs_malloc(test_pool, size);
}
static void _bs_mmpl_wrapper_bs_free(void *ptr) { bs_free(test_pool, ptr); }
static void *_bs_mmpl_wrapper_malloc(size_t size) { return malloc(size); }
static void _bs_mmpl_wrapper_free(void *ptr) { free(ptr); }

void bs_mempool_test(void) {
  if (test_pool == NULL)
    test_pool = bs_mempool_create();
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "Starting memory pool test\n");
  bs_mempool_stats();
  fprintf(stderr, "*****************************\n");
  bs_mempool_speedtest(_bs_mmpl_wrapper_bs_malloc, _bs_mmpl_wrapper_bs_free);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test (to compare)\n");
  fprintf(stderr, "*****************************\n");
  bs_mempool_speedtest(_bs_mmpl_wrapper_malloc, _bs_mmpl_wrapper_free);

  // fprintf(stderr, "*****************************\n");
  // fprintf(stderr, "Stressing the system\n");
  // fprintf(stderr, "*****************************\n");
  // size_t repeat = 1024 * 1024;
  // size_t unit = 16;
  // bs_mmpl_ptr pool = test_pool;
  // while (repeat >= 1024) {
  //   fprintf(stderr,
  //           "Stress allocation/deallocation including "
  //           "1:3 fragmentation:\n * %lu X %lu bytes\n",
  //           repeat, unit);
  //   void** ptrs = bs_malloc(pool, repeat * sizeof(void*));
  //   for (size_t i = 0; i < repeat; i++) {
  //     ptrs[i] = bs_malloc(pool, unit);
  //   }
  //   for (size_t i = 0; i < repeat; i += 3) {
  //     bs_free(pool, ptrs[i]);
  //   }
  //   for (size_t i = 1; i < repeat; i += 3) {
  //     bs_free(pool, ptrs[i]);
  //   }
  //   for (size_t i = 2; i < repeat; i += 3) {
  //     bs_free(pool, ptrs[i]);
  //   }
  //   for (size_t i = 0; i < repeat; i++) {
  //     ptrs[i] = bs_malloc(pool, unit);
  //   }
  //   for (size_t i = 0; i < repeat; i++) {
  //     bs_free(pool, ptrs[i]);
  //   }
  //   bs_free(pool, ptrs);
  //
  //   unit <<= 1;
  //   repeat >>= 1;
  // }
}

// void bs_mempool_print(void* _branch) {
//   if (_branch == NULL) {
//     fprintf(stderr, "<empty>");
//     return;
//   }
//   slice_s* branch = _branch;
//   while (branch) {
//     fprintf(stderr, "*****************************\n");
//     fprintf(stderr, "    |--    %p (%u)  --|        \n", branch,
//             slice_size(branch));
//     fprintf(stderr, "   %p (%u)               %p (%u)      \n",
//             branch->not_bigger,
//             (branch->not_bigger ? slice_size(branch->not_bigger) : 0),
//             branch->bigger, (branch->bigger ? slice_size(branch->bigger) :
//             0));
//     if (branch->not_bigger)
//       bs_mempool_print(branch->not_bigger);
//     branch = branch->bigger;
//   }
// }

#endif
