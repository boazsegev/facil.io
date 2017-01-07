/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef MEMPOOL_H

/*
REALLOC ISN'T IMPLEMENTED YET.
*/

/* *****************************************************************************
A simple `mmap` based memory pool - `malloc` alternative.

The issue: objects that have a long life, such as Websocket / HTTP2 connection
objects or server wide strings with reference counts, cause memory fragmentation
when allocated on the heap alongside objects that have a short life.

This is a common issue when using `malloc` for all allocations.

This issue effects long running processes (such as servers) while it's effect on
short lived proccesses are less accute.

To circumvent this issue, a seperate memory allocation method is used for
long-lived objects.

This memory pool allocates large blocks of memory (~2Mb at a time), minimizing
small memory fragmentation by both reserving large memory blocks and seperating
memory locality between long lived objects and short lived objects.

================================================================================
NOTICE:

The memory pool is attached to the specific file. in which `mempool.h` is
included.

The memory shoule NEVER be freed from a different file.

However, it's easy to work around this limitation by wrapping the `mempool_`
functions using a static functions.

================================================================================

This file requires the "spnlock.h" library file as well. Together these files
can be used also seperately from the `facil.io` library.

*/
#define MEMPOOL_H
MEMPOOL_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/* *****************************************************************************
********************************************************************************
API Declerations
********************************************************************************
***************************************************************************** */

/** Allocates memory from the pool. */
static __unused void *mempool_malloc(size_t size);
/**
 * Frees the memory, releasing it back to the pool (or, sometimes, the system).
 */
static __unused void mempool_free(void *ptr);
/**
 * Behaves the same a the systems `realloc`, attempting to resize the memory
 * when possible. On error returns NULL (the old pointer data remains allocated
 * and valid) otherwise returns a new pointer (either equal to the old or after
 * releasing the old one).
 */
static __unused void *mempool_realloc(void *ptr, size_t new_size);

#if defined(DEBUG) && DEBUG == 1
static __unused void mempool_test(void);
#endif

/* *****************************************************************************
********************************************************************************
Implementation
********************************************************************************
***************************************************************************** */

/* *****************************************************************************
Memory block allocation
*/

#define MEMPOOL_BLOCK_SIZE (1UL << 21)

/* Will we use mmap or malloc? */
// clang-format off
#ifdef __has_include
/* check for unix support */
# if __has_include(<unistd.h>) && __has_include(<sys/mman.h>)
#  define HAS_UNIX_FEATURES
# endif
#endif
// clang-format on

#ifdef HAS_UNIX_FEATURES
#include <sys/mman.h>
#include <unistd.h>

/* *****************************************************************************
spnlock.h (can also be embeded instead of included)
*/
#include "spnlock.h"

/* *****************************************************************************
Memory slices, tree and helpers
*/

typedef struct mempool_reserved_slice_s {
  struct mempool_reserved_slice_s_offset { /** offset from this slice */
    uint32_t ahead;
    uint32_t behind;
  } offset;
  struct mempool_reserved_slice_s
      *next; /** Used for the free slices linked list. */
} mempool_reserved_slice_s;

static struct {
  mempool_reserved_slice_s *available;
  spn_lock_i lock;
} mempool_reserved_pool = {.available = NULL, .lock = SPN_LOCK_INIT};

#define MEMPOOL_LOCK() spn_lock(&mempool_reserved_pool.lock)
#define MEMPOOL_UNLOCK() spn_unlock(&mempool_reserved_pool.lock)

#define MEMPOOL_USED_MARKER ((uint32_t)(~0UL << 21))
#define MEMPOOL_INDI_MARKER ((uint32_t)0xF7F7F7F7UL)
#define MEMPOOL_SIZE_MASK (MEMPOOL_BLOCK_SIZE - 1)

#define MEMPOOL_SLICE2PTR(slice)                                               \
  ((void *)(((uintptr_t)(slice)) +                                             \
            (sizeof(struct mempool_reserved_slice_s_offset))))
#define MEMPOOL_PTR2SLICE(ptr)                                                 \
  ((mempool_reserved_slice_s *)(((uintptr_t)(ptr)) -                           \
                                (sizeof(                                       \
                                    struct mempool_reserved_slice_s_offset))))
#define MEMPOOL_SLICE_NEXT(slice)                                              \
  ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) +                         \
                                ((slice)->offset.ahead & MEMPOOL_SIZE_MASK)))
#define MEMPOOL_SLICE_PREV(slice)                                              \
  ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) -                         \
                                ((slice)->offset.behind)))

/* *****************************************************************************
Memory Block Allocation / Deallocation
*/

#define MEMPOOL_ALLOC_SPECIAL(target, size)                                    \
  do {                                                                         \
    target = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,              \
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);                         \
    if (target == MAP_FAILED)                                                  \
      target == NULL;                                                          \
    fprintf(stderr, "allocated %lu bytes at %p\n", (size_t)size, target);      \
  } while (0);

#define MEMPOOL_DEALLOC_SPECIAL(target, size) munmap((target), (size));

#else

#define MEMPOOL_ALLOC_SPECIAL(target, size)                                    \
  do {                                                                         \
    target = malloc(size);                                                     \
  } while (0);

#define MEMPOOL_DEALLOC_SPECIAL(target, size) free((target));

#endif

/* *****************************************************************************
Helpers: Memory block slicing and memory pool list maintanence.
*/

/* *****************************************************************************
API implementation
*/

static __unused void *mempool_malloc(size_t size) {
  if (!size)
    return NULL;
  size += sizeof(struct mempool_reserved_slice_s_offset);
  if (size & 7) {
    size = size + 8 - (size & 7);
  }
  mempool_reserved_slice_s **pos, *slice = NULL;

  if (size > (MEMPOOL_BLOCK_SIZE - (sizeof(mempool_reserved_slice_s) << 1)))
    goto alloc_indi;
  pos = &mempool_reserved_pool.available;
  MEMPOOL_LOCK();
  while (*pos && (*pos)->offset.ahead < size)
    pos = &(*pos)->next;
  if (*pos) {
    slice = *pos;
    *pos = slice->next;
  } else {
    MEMPOOL_ALLOC_SPECIAL(slice, MEMPOOL_BLOCK_SIZE);
    slice->offset.behind = 0;
    slice->offset.ahead =
        MEMPOOL_BLOCK_SIZE - (sizeof(struct mempool_reserved_slice_s_offset));

    ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) + MEMPOOL_BLOCK_SIZE -
                                  (sizeof(
                                      struct mempool_reserved_slice_s_offset))))
        ->offset.ahead = 0;
    ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) + MEMPOOL_BLOCK_SIZE -
                                  (sizeof(
                                      struct mempool_reserved_slice_s_offset))))
        ->offset.behind = slice->offset.ahead;
    slice->next = NULL;
  }

  if (!slice) {
    MEMPOOL_UNLOCK();
    fprintf(stderr, "mempool: no memory\n");
    return NULL;
  }

  if (slice->offset.ahead > (size + sizeof(mempool_reserved_slice_s))) {
    mempool_reserved_slice_s *tmp =
        (mempool_reserved_slice_s *)(((uintptr_t)slice) + size);
    tmp->offset.behind = size;
    tmp->offset.ahead = slice->offset.ahead - size;
    slice->offset.ahead = size;
    pos = &mempool_reserved_pool.available;
    while (*pos && (*pos)->offset.ahead < tmp->offset.ahead)
      pos = &(*pos)->next;
    tmp->next = *pos;
    *pos = tmp;
  }

  MEMPOOL_UNLOCK();
  slice->next = NULL;
  slice->offset.ahead |= MEMPOOL_USED_MARKER;
  // mempool_reserved_slice_s *tmp =
  //     (void *)((uintptr_t)slice + (slice->offset.ahead & MEMPOOL_SIZE_MASK));
  // fprintf(stderr, "Allocated %lu bytes at: %u <- %p -> %u."
  //                 "next: %u <- %p -> %u\n ",
  //         size, slice->offset.behind, slice,
  //         (uint32_t)(slice->offset.ahead & MEMPOOL_SIZE_MASK),
  //         tmp->offset.behind, tmp,
  //         (uint32_t)(tmp->offset.ahead & MEMPOOL_SIZE_MASK));
  return MEMPOOL_SLICE2PTR(slice);
alloc_indi:
  MEMPOOL_ALLOC_SPECIAL(slice, size);
  if (slice) {
    slice->offset.ahead = size;
    slice->offset.behind = MEMPOOL_INDI_MARKER;
  }
  return MEMPOOL_SLICE2PTR(slice);
}
/**
 * Frees the memory, releasing it back to the pool (or, sometimes, the system).
 */
static __unused void mempool_free(void *ptr) {
  if (!ptr)
    return;
  mempool_reserved_slice_s **pos, *slice = MEMPOOL_PTR2SLICE(ptr), *tmp;
  if (slice->offset.behind == MEMPOOL_INDI_MARKER)
    goto alloc_indi;
  if ((slice->offset.ahead & MEMPOOL_USED_MARKER) != MEMPOOL_USED_MARKER)
    goto error;
  slice->offset.ahead &= MEMPOOL_SIZE_MASK;
  MEMPOOL_LOCK();
  /* merge slice with upper boundry */
  while ((tmp = (void *)(((uintptr_t)slice) + slice->offset.ahead))
             ->offset.ahead &&
         (tmp->offset.ahead & MEMPOOL_USED_MARKER) == 0) {
    pos = &mempool_reserved_pool.available;
    while ((*pos) && (*pos) != tmp) {
      pos = &(*pos)->next;
    }
    if (!(*pos)) {
      // fprintf(stderr, "forward %p -> %u, %p -> %u\n", slice,
      //         slice->offset.ahead, tmp, tmp->offset.behind);
      goto error_list;
    }
    *pos = tmp->next;
    slice->offset.ahead += tmp->offset.ahead;
  }
  /* merge slice with lower boundry */
  while (slice->offset.behind &&
         (((tmp = (mempool_reserved_slice_s *)(((uintptr_t)slice) -
                                               slice->offset.behind))
               ->offset.ahead) &
          MEMPOOL_USED_MARKER) == 0) {
    pos = &mempool_reserved_pool.available;
    while ((*pos) && ((*pos) != tmp)) {
      pos = (&(*pos)->next);
    }
    if (!(*pos)) {
      // fprintf(stderr, "backwards %p -> %u,  %u <- %p -> %u\n", slice,
      //         slice->offset.behind, tmp->offset.behind, tmp,
      //         tmp->offset.ahead);
      goto error_list;
    }
    *pos = tmp->next;
    tmp->next = NULL;
    tmp->offset.ahead += slice->offset.ahead;
    slice = tmp;
  }

  /* return memory to system? */
  if (mempool_reserved_pool.available && slice->offset.behind == 0 &&
      ((mempool_reserved_slice_s *)((uintptr_t)slice) + slice->offset.ahead)
              ->offset.ahead == 0) {
    MEMPOOL_DEALLOC_SPECIAL(slice, MEMPOOL_BLOCK_SIZE);
    fprintf(stderr, "DEALLOCATED BLOCK\n");
    return;
  }

  /* inform higher neighbor about any updates */
  ((mempool_reserved_slice_s *)(((uintptr_t)slice) + slice->offset.ahead))
      ->offset.behind = slice->offset.ahead;

  /* place slice in list */
  pos = &mempool_reserved_pool.available;
  while (*pos && (*pos)->offset.ahead < slice->offset.ahead) {
    pos = &(*pos)->next;
  }
  slice->next = *pos;
  *pos = slice;
  MEMPOOL_UNLOCK();
  return;
alloc_indi:
  MEMPOOL_DEALLOC_SPECIAL(slice, slice->offset.ahead);
  return;
error:
  MEMPOOL_UNLOCK();
  if ((slice->offset.ahead & MEMPOOL_USED_MARKER) == 0)
    fprintf(stderr, "mempool: memory being freed is already free.\n");
  else
    fprintf(stderr, "mempool: memory allocation data corrupted. possible "
                    "buffer overflow?\n");
  errno = EFAULT;
  raise(SIGSEGV); /* support longjmp rescue */
  exit(EFAULT);
error_list:
  MEMPOOL_UNLOCK();
  fprintf(stderr, "mempool: memory being freed isn't a member of this memory "
                  "pool (allocated by a different file?).\n");
  errno = EFAULT;
  raise(SIGSEGV); /* support longjmp rescue */
  exit(EFAULT);
}
/**
 * Behaves the same a the systems `realloc`, attempting to resize the memory
 * when possible. On error returns NULL (the old pointer data remains allocated
 * and valid) otherwise returns a new pointer (either equal to the old or after
 * releasing the old one).
 */
static __unused void *mempool_realloc(void *ptr, size_t size) {
  if (!size)
    return NULL;
  size += sizeof(struct mempool_reserved_slice_s_offset);
  if (size & 7) {
    size = size + 8 - (size & 7);
  }

  mempool_reserved_slice_s *slice = MEMPOOL_PTR2SLICE(ptr);

  if (slice->offset.behind == MEMPOOL_INDI_MARKER)
    goto realloc_indi;
  if ((slice->offset.ahead & MEMPOOL_USED_MARKER) != MEMPOOL_USED_MARKER)
    goto error;

  if ((slice->offset.ahead & MEMPOOL_SIZE_MASK) > size) {
    /* TODO: cut away memory?*/
    return ptr;
  }

  MEMPOOL_LOCK();
  /* TODO: merge upper bound memory slices */
  MEMPOOL_UNLOCK();
  if ((slice->offset.ahead & MEMPOOL_SIZE_MASK) < size) {
    void *new_mem =
        mempool_malloc(size - sizeof(struct mempool_reserved_slice_s_offset));
    memcpy(new_mem, ptr, slice->offset.ahead & MEMPOOL_SIZE_MASK);
    mempool_free(ptr);
    ptr = new_mem;
  }
  return ptr;

realloc_indi:
  if (slice->offset.ahead > size)
    return ptr;
  /* reallocate indi */
  void *new_mem =
      mempool_malloc(size - sizeof(struct mempool_reserved_slice_s_offset));
  if (!new_mem)
    return NULL;
  memcpy(new_mem, ptr, slice->offset.ahead & MEMPOOL_SIZE_MASK);
  mempool_free(ptr);
  return new_mem;
error:
  errno = EFAULT;
  raise(SIGSEGV); /* support longjmp rescue */
  exit(EFAULT);
}

/* *****************************************************************************
Testing
*/

#if defined(DEBUG) && DEBUG == 1

#define MEMTEST_SLICE 48
#define MEMTEST_REPEATS (1024 * 1024)

#include <time.h>
static void mempool_stats(void) {
  fprintf(stderr, "* Pool object: %lu bytes\n"
                  "* Alignment: %lu (memory border)\n"
                  "* Minimal Allocateion Size (including header): %lu\n"
                  "* Minimal Allocation Space (no header): %lu\n"
                  "* Header size: %lu\n",
          sizeof(mempool_reserved_pool),
          sizeof(struct mempool_reserved_slice_s_offset),
          sizeof(mempool_reserved_slice_s),
          sizeof(mempool_reserved_slice_s) -
              sizeof(struct mempool_reserved_slice_s_offset),
          sizeof(struct mempool_reserved_slice_s_offset));
}

static void mempool_speedtest(void *(*mlk)(size_t), void (*fr)(void *)) {
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
                  "bytes each: %lu CPU "
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
                  "bytes each: %lu CPU "
                  "cycles.\n",
          MEMTEST_REPEATS / 7, MEMTEST_SLICE, mlk_time);

  start = clock();
  for (size_t i = 0; i < MEMTEST_REPEATS; i++) {
    memset(pntrs[i], 170, MEMTEST_SLICE);
  }
  end = clock();
  zr_time = end - start;
  fprintf(stderr, "* Set bits to 0b10 for %d consecutive blocks %d bytes "
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

static __unused void mempool_test(void) {
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "mempool implementation details:\n");
  mempool_stats();
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test\n");
  mempool_speedtest(malloc, free);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, " mempool memory test\n");
  mempool_speedtest(mempool_malloc, mempool_free);
  fprintf(stderr, "*****************************\n");

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

#endif

/* *****************************************************************************
Cleanup
*/
#undef MEMPOOL_BLOCK_SIZE
#undef MEMPOOL_ALLOC_SPECIAL
#undef MEMPOOL_ALLOC_BLOCK
#undef MEMPOOL_SLICE_SIZE
#undef MEMPOOL_SLICE_OFFSET
#undef MEMPOOL_USED_MARKER
#undef MEMPOOL_INDI_MARKER
#undef MEMPOOL_USED_MASK
#undef MEMPOOL_SLICE2PTR
#undef MEMPOOL_PTR2SLICE
#undef MEMPOOL_SLICE_NEXT
#undef MEMPOOL_SLICE_PREV
#undef MEMPOOL_SLICE_SET_USED
#undef MEMPOOL_SLICE_IS_USED
#undef MEMPOOL_SLICE_SET_FREE
#undef MEMPOOL_SLICE_IS_FREE
#undef MEMPOOL_SLICE_SET_INDI
#undef MEMPOOL_SLICE_IS_INDI
#undef MEMPOOL_LOCK
#undef MEMPOOL_UNLOCK
#endif
