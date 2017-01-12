/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef MEMPOOL_H

/* *****************************************************************************
A simple `mmap` based localized memory pool (localized `malloc` alternative).

The memory pool is localized to the same object file (C file). See NOTICE.

The issue: objects that have a long life, such as Websocket / HTTP2 protocol
objects or server wide strings with reference counts, cause memory fragmentation
when allocated on the heap alongside objects that have a short life.

This is a common issue when using `malloc` in long running processes for all
allocations.

This issue effects long running processes (such as servers) while it's effect on
short lived proccesses are less accute and could often be ignored.

To circumvent this issue, a seperate memory allocation method is used for
long-lived objects.

This memory pool allocates large blocks of memory (~2Mb at a time), minimizing
small memory fragmentation by both reserving large memory blocks and seperating
memory locality between long lived objects and short lived objects.

The memory pool isn't expected to be faster than the system's `malloc`
(although, sometimes it might perform better). However, selective use of this
memory pool could improve concurrency (each pool has a seperate lock, unlike
`malloc`'s system lock') as well as help with memory fragmentation.

================================================================================
NOTICE:

The memory pool is attached to the specific file. in which `mempool.h` is
included.

The memory shoule NEVER be freed from a different file.

However, it's easy to work around this limitation by wrapping the `mempool_`
functions using proper `create` / `destroy` functions for any objects.

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
 * when possible.
 *
 * On error returns NULL (the old pointer data remains allocated and valid)
 * otherwise returns a new pointer (either equal to the old or after
 * deallocating the old one).
 */
static __unused void *mempool_realloc(void *ptr, size_t new_size);

#if defined(DEBUG) && DEBUG == 1
/** Tests the memory pool, both testing against issues / corruption and testing
 * it's performance against the system's `malloc`.
 */
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
#define MEMPOOL_ORDERING_LIMIT 32
#define MEMPOOL_RETURN_MEM_TO_SYSTEM 1

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
    uint32_t reserved1; /* used to make the offset 16 bytes long */
    uint32_t ahead;
    uint32_t behind;
    uint32_t reserved2; /* used to make the offset 16 bytes long */
  } offset;
  /** Used for the free slices linked list. */
  struct mempool_reserved_slice_s *next;
  struct mempool_reserved_slice_s *prev;
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

/* *****************************************************************************
Memory Block Allocation / Deallocation
*/

#define MEMPOOL_ALLOC_SPECIAL(target, size)                                    \
  do {                                                                         \
    target = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,              \
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);                         \
    if (target == MAP_FAILED)                                                  \
      target == NULL;                                                          \
  } while (0);

#define MEMPOOL_DEALLOC_SPECIAL(target, size) munmap((target), (size))

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
  if (size & 15) {
    size = (size & (~16)) + 16;
  }
  size += sizeof(struct mempool_reserved_slice_s_offset);

  mempool_reserved_slice_s *slice = NULL;

  if (size > (MEMPOOL_BLOCK_SIZE - (sizeof(mempool_reserved_slice_s) << 1)))
    goto alloc_indi;
  slice = mempool_reserved_pool.available;
  MEMPOOL_LOCK();
  while (slice && slice->offset.ahead < size)
    slice = slice->next;
  if (slice) {
    /* remove slice from available memory list */
    if (slice->next)
      slice->next->prev = slice->prev;
    if (slice->prev)
      slice->prev->next = slice->next;
    else
      mempool_reserved_pool.available = slice->next;
    slice->next = NULL;
    slice->prev = NULL;
  } else {
    MEMPOOL_ALLOC_SPECIAL(slice, MEMPOOL_BLOCK_SIZE);
    // fprintf(stderr, "Allocated Block at %p\n", slice);
    slice->offset.behind = 0;
    slice->offset.ahead =
        MEMPOOL_BLOCK_SIZE - (sizeof(struct mempool_reserved_slice_s_offset));

    mempool_reserved_slice_s *tmp =
        (mempool_reserved_slice_s
             *)(((uintptr_t)(slice)) + MEMPOOL_BLOCK_SIZE -
                (sizeof(struct mempool_reserved_slice_s_offset)));
    tmp->offset.ahead = 0;
    tmp->offset.behind = slice->offset.ahead;
  }

  if (!slice) {
    MEMPOOL_UNLOCK();
    fprintf(stderr, "mempool: no memory\n");
    return NULL;
  }

  if (slice->offset.ahead > (size + sizeof(mempool_reserved_slice_s))) {
    /* cut the slice in two */
    mempool_reserved_slice_s *tmp =
        (mempool_reserved_slice_s *)(((uintptr_t)slice) + size);
    tmp->offset.behind = size;
    tmp->offset.ahead = slice->offset.ahead - size;
    slice->offset.ahead = size;
    /* inform higher neighbor about any updates */
    ((mempool_reserved_slice_s *)(((uintptr_t)tmp) + tmp->offset.ahead))
        ->offset.behind = tmp->offset.ahead;
    /* place the new slice in the available memory list */
    uint16_t limit = MEMPOOL_ORDERING_LIMIT;
    tmp->next = NULL;
    tmp->prev = NULL;
    mempool_reserved_slice_s **pos = &mempool_reserved_pool.available;
    while (limit && *pos && ((*pos)->offset.ahead < tmp->offset.ahead)) {
      tmp->prev = *pos;
      pos = &(*pos)->next;
      --limit;
    }
    if (*pos) {
      tmp->next = *pos;
      tmp->next->prev = tmp;
      *pos = tmp;
    } else {
      *pos = tmp;
    }
  }

  slice->offset.ahead |= MEMPOOL_USED_MARKER;
  MEMPOOL_UNLOCK();
  slice->next = NULL;
  slice->prev = NULL;
  // mempool_reserved_slice_s *tmp =
  //     (void *)((uintptr_t)slice + (slice->offset.ahead &
  //     MEMPOOL_SIZE_MASK));
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
 * Frees the memory, releasing it back to the pool (or, sometimes, the
 * system).
 */
static __unused void mempool_free(void *ptr) {
  if (!ptr)
    return;
  mempool_reserved_slice_s **pos, *slice = MEMPOOL_PTR2SLICE(ptr), *tmp;

  if (slice->offset.behind == MEMPOOL_INDI_MARKER)
    goto alloc_indi;
  if ((slice->offset.ahead & MEMPOOL_USED_MARKER) != MEMPOOL_USED_MARKER)
    goto error;

  MEMPOOL_LOCK();
  slice->offset.ahead &= MEMPOOL_SIZE_MASK;
  /* merge slice with upper boundry */
  while ((tmp = (mempool_reserved_slice_s *)(((uintptr_t)slice) +
                                             slice->offset.ahead))
             ->offset.ahead &&
         (tmp->offset.ahead & MEMPOOL_USED_MARKER) == 0) {
    /* extract merged slice from list */
    if (tmp->next)
      tmp->next->prev = tmp->prev;
    if (tmp->prev)
      tmp->prev->next = tmp->next;
    else
      mempool_reserved_pool.available = tmp->next;

    tmp->next = NULL;
    tmp->prev = NULL;
    slice->offset.ahead += tmp->offset.ahead;
  }
  /* merge slice with lower boundry */
  while (slice->offset.behind &&
         ((tmp = (mempool_reserved_slice_s *)(((uintptr_t)slice) -
                                              slice->offset.behind))
              ->offset.ahead &
          MEMPOOL_USED_MARKER) == 0) {
    /* extract merged slice from list */
    if (tmp->next)
      tmp->next->prev = tmp->prev;
    if (tmp->prev)
      tmp->prev->next = tmp->next;
    else
      mempool_reserved_pool.available = tmp->next;

    tmp->next = NULL;
    tmp->prev = NULL;
    tmp->offset.ahead += slice->offset.ahead;

    slice = tmp;
  }

  /* return memory to system, if the block is no longer required. */
  if (MEMPOOL_RETURN_MEM_TO_SYSTEM && mempool_reserved_pool.available &&
      slice->offset.behind == 0 &&
      ((mempool_reserved_slice_s *)(((uintptr_t)slice) + slice->offset.ahead))
              ->offset.ahead == 0) {
    MEMPOOL_UNLOCK();
    // fprintf(
    //     stderr, "DEALLOCATED BLOCK %p, size review %u == %lu %s\n", slice,
    //     slice->offset.ahead,
    //     MEMPOOL_BLOCK_SIZE - sizeof(struct mempool_reserved_slice_s_offset),
    //     (slice->offset.ahead ==
    //      MEMPOOL_BLOCK_SIZE - sizeof(struct mempool_reserved_slice_s_offset))
    //         ? "passed."
    //         : "FAILED.");
    MEMPOOL_DEALLOC_SPECIAL(slice, MEMPOOL_BLOCK_SIZE);
    return;
  }

  /* inform higher neighbor about any updates */
  // fprintf(stderr, "slice: %p -> %u\n", slice, slice->offset.ahead);
  ((mempool_reserved_slice_s *)(((uintptr_t)slice) + slice->offset.ahead))
      ->offset.behind = slice->offset.ahead;

  /* place slice in list */
  uint8_t limit = MEMPOOL_ORDERING_LIMIT;
  slice->next = NULL;
  slice->prev = NULL;
  pos = &mempool_reserved_pool.available;
  while (limit && *pos && ((*pos)->offset.ahead < slice->offset.ahead)) {
    slice->prev = *pos;
    pos = &(*pos)->next;
    --limit;
  }
  if (*pos) {
    slice->next = *pos;
    slice->next->prev = slice;
    *pos = slice;
  } else {
    *pos = slice;
  }

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
}
/**
 * Behaves the same a the systems `realloc`, attempting to resize the memory
 * when possible. On error returns NULL (the old pointer data remains allocated
 * and valid) otherwise returns a new pointer (either equal to the old or after
 * deallocating the old one).
 */
static __unused void *mempool_realloc(void *ptr, size_t size) {
  if (!size)
    return NULL;
  if (size & 15) {
    size = (size & (~16)) + 16;
  }
  size += sizeof(struct mempool_reserved_slice_s_offset);

  mempool_reserved_slice_s *tmp = NULL, *slice = MEMPOOL_PTR2SLICE(ptr);

  if (slice->offset.behind == MEMPOOL_INDI_MARKER)
    goto realloc_indi;
  if ((slice->offset.ahead & MEMPOOL_USED_MARKER) != MEMPOOL_USED_MARKER)
    goto error;

  slice->offset.ahead &= MEMPOOL_SIZE_MASK;

  MEMPOOL_LOCK();
  /* merge slice with upper boundry */
  while ((tmp = (mempool_reserved_slice_s *)(((uintptr_t)slice) +
                                             slice->offset.ahead))
             ->offset.ahead &&
         (tmp->offset.ahead & MEMPOOL_USED_MARKER) == 0) {
    /* extract merged slice from list */
    if (tmp->next)
      tmp->next->prev = tmp->prev;
    if (tmp->prev)
      tmp->prev->next = tmp->next;
    else
      mempool_reserved_pool.available = tmp->next;

    tmp->next = NULL;
    tmp->prev = NULL;
    slice->offset.ahead += tmp->offset.ahead;
  }

  /* inform higher neighbor about any updates */
  ((mempool_reserved_slice_s *)(((uintptr_t)slice) + slice->offset.ahead))
      ->offset.behind = slice->offset.ahead;

  if ((slice->offset.ahead) > size + sizeof(mempool_reserved_slice_s)) {
    /* cut the slice in two */
    tmp = (mempool_reserved_slice_s *)(((uintptr_t)slice) + size);
    tmp->offset.behind = size;
    tmp->offset.ahead = slice->offset.ahead - size;
    slice->offset.ahead = size;
    /* inform higher neighbor about any updates */
    ((mempool_reserved_slice_s *)(((uintptr_t)tmp) + tmp->offset.ahead))
        ->offset.behind = tmp->offset.ahead;
    /* place the new slice in the available memory list */
    tmp->next = NULL;
    tmp->prev = NULL;
    mempool_reserved_slice_s **pos = &mempool_reserved_pool.available;
    uint8_t limit = MEMPOOL_ORDERING_LIMIT;
    while (limit && *pos && ((*pos)->offset.ahead < tmp->offset.ahead)) {
      tmp->prev = *pos;
      pos = &(*pos)->next;
      --limit;
    }
    if (*pos) {
      tmp->next = *pos;
      tmp->next->prev = tmp;
      *pos = tmp;
    } else {
      *pos = tmp;
    }

    slice->offset.ahead |= MEMPOOL_USED_MARKER;
    MEMPOOL_UNLOCK();
    return ptr;
  }
  slice->offset.ahead |= MEMPOOL_USED_MARKER;
  MEMPOOL_UNLOCK();

  if ((slice->offset.ahead & MEMPOOL_SIZE_MASK) < size) {
    void *new_mem =
        mempool_malloc(size - sizeof(struct mempool_reserved_slice_s_offset));
    if (!new_mem)
      return NULL;
    memcpy(new_mem, ptr, slice->offset.ahead & MEMPOOL_SIZE_MASK);
    mempool_free(ptr);
    ptr = new_mem;
  }
  return ptr;

realloc_indi:
  /* indi doesn't shrink */
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
********************************************************************************
TESTING
********************************************************************************
***************************************************************************** */

#if defined(DEBUG) && DEBUG == 1

#define MEMTEST_SLICE 32

#include <time.h>
static void mempool_stats(void) {
  fprintf(stderr, "* Pool object: %lu bytes\n"
                  "* Alignment: %lu \n"
                  "* Minimal Allocation Size (including header): %lu\n"
                  "* Minimal Allocation Space (no header): %lu\n"
                  "* Header size: %lu\n",
          sizeof(mempool_reserved_pool),
          sizeof(struct mempool_reserved_slice_s_offset),
          sizeof(mempool_reserved_slice_s),
          sizeof(mempool_reserved_slice_s) -
              sizeof(struct mempool_reserved_slice_s_offset),
          sizeof(struct mempool_reserved_slice_s_offset));
}

static void mempool_speedtest(size_t memtest_repeats, void *(*mlk)(size_t),
                              void (*fr)(void *),
                              void *(*ralc)(void *, size_t)) {
  void **pntrs = mlk(memtest_repeats * sizeof(*pntrs));
  clock_t start, end, mlk_time, fr_time, zr_time;
  mlk_time = 0;
  fr_time = 0;
  zr_time = 0;
  struct timespec start_test, end_test;
  clock_gettime(CLOCK_MONOTONIC, &start_test);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    __asm__ volatile("" ::: "memory");
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr, "* Doing nothing: %lu CPU cycles.\n", mlk_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    // fprintf(stderr, "malloc %lu\n", i);
    pntrs[i] = mlk(MEMTEST_SLICE);
    *((uint8_t *)pntrs[i]) = 1;
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Allocating %lu consecutive blocks %d each: %lu CPU cycles.\n",
          memtest_repeats, MEMTEST_SLICE, mlk_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i += 2) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i += 2) {
    pntrs[i] = mlk(MEMTEST_SLICE);
  }
  end = clock();
  mlk_time = end - start;

  fprintf(stderr,
          "* Freeing %lu Fragmented (single space) blocks %d each: %lu CPU "
          "cycles.\n",
          memtest_repeats / 2, MEMTEST_SLICE, fr_time);

  fprintf(stderr, "* Allocating %lu Fragmented (single space) blocks %d "
                  "bytes each: %lu CPU "
                  "cycles.\n",
          memtest_repeats / 2, MEMTEST_SLICE, mlk_time);

  mlk_time = 0;
  fr_time = 0;

  for (size_t xtimes = 0; xtimes < 100; xtimes++) {
    start = clock();
    for (size_t i = 0; i < memtest_repeats; i += 7) {
      fr(pntrs[i]);
    }
    end = clock();
    fr_time += end - start;

    start = clock();
    for (size_t i = 0; i < memtest_repeats; i += 7) {
      pntrs[i] = mlk(MEMTEST_SLICE);
    }
    end = clock();
    mlk_time += end - start;
  }

  fprintf(stderr,
          "* 100X Freeing %lu Fragmented (7 spaces) blocks %d each: %lu CPU "
          "cycles.\n",
          memtest_repeats / 7, MEMTEST_SLICE, fr_time);

  fprintf(stderr, "* 100X Allocating %lu Fragmented (7 spaces) blocks %d "
                  "bytes each: %lu CPU "
                  "cycles.\n",
          memtest_repeats / 7, MEMTEST_SLICE, mlk_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    memset(pntrs[i], 170, MEMTEST_SLICE);
  }
  end = clock();
  zr_time = end - start;
  fprintf(stderr, "* Set bits (0b10) for %lu consecutive blocks %dB "
                  "each: %lu CPU cycles.\n",
          memtest_repeats, MEMTEST_SLICE, zr_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;
  fprintf(stderr, "* Freeing %lu consecutive blocks %d each: %lu CPU cycles.\n",
          memtest_repeats, MEMTEST_SLICE, fr_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    pntrs[i] = mlk(MEMTEST_SLICE);
  }
  end = clock();
  start = clock();
  for (size_t i = 0; i < memtest_repeats; i += 2) {
    fr(pntrs[i]);
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Freeing every other block %dB X %lu blocks: %lu CPU cycles.\n",
          MEMTEST_SLICE, memtest_repeats >> 1, mlk_time);

  start = clock();
  for (size_t i = 1; i < memtest_repeats; i += 2) {
    pntrs[i] = ralc(pntrs[i], MEMTEST_SLICE << 1);
    if (pntrs[i] == NULL)
      fprintf(stderr, "REALLOC RETURNED NULL - Memory leaked during test\n");
  }
  end = clock();
  mlk_time = end - start;
  fprintf(
      stderr,
      "* Reallocating every other block %dB X %lu blocks: %lu CPU cycles.\n",
      MEMTEST_SLICE, memtest_repeats >> 1, mlk_time);

  start = clock();
  for (size_t i = 1; i < memtest_repeats; i += 2) {
    fr(pntrs[i]);
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Freeing every other block %dB X %lu blocks: %lu CPU cycles.\n",
          MEMTEST_SLICE, memtest_repeats >> 1, mlk_time);

  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    pntrs[i] = mlk(MEMTEST_SLICE);
  }
  end = clock();
  mlk_time = end - start;
  fprintf(stderr,
          "* Allocating %lu consecutive blocks %d each: %lu CPU cycles.\n",
          memtest_repeats, MEMTEST_SLICE, mlk_time);
  start = clock();
  for (size_t i = 0; i < memtest_repeats; i++) {
    fr(pntrs[i]);
  }
  end = clock();
  fr_time = end - start;
  fprintf(stderr, "* Freeing %lu consecutive blocks %d each: %lu CPU cycles.\n",
          memtest_repeats, MEMTEST_SLICE, fr_time);
  fprintf(stderr, "* Freeing pointer array %p.\n", pntrs);
  fr(pntrs);

  clock_gettime(CLOCK_MONOTONIC, &end_test);
  uint64_t msec_for_test =
      (end_test.tv_nsec < start_test.tv_nsec)
          ? ((end_test.tv_sec -= 1), (start_test.tv_nsec - end_test.tv_nsec))
          : (end_test.tv_nsec - start_test.tv_nsec);
  uint64_t sec_for_test = end_test.tv_sec - start_test.tv_sec;

  fprintf(stderr, "Finished test in %llum, %llus %llu mili.sec.\n",
          sec_for_test / 60, sec_for_test - (((sec_for_test) / 60) * 60),
          msec_for_test / 1000000);
}

static __unused void mempool_test(void) {
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "mempool implementation details:\n");
  mempool_stats();
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test for ~2Mb\n");
  mempool_speedtest((2 << 20) / MEMTEST_SLICE, malloc, free, realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, " mempool memory test for ~2Mb\n");
  mempool_speedtest((2 << 20) / MEMTEST_SLICE, mempool_malloc, mempool_free,
                    mempool_realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test for ~4Mb\n");
  mempool_speedtest((2 << 21) / MEMTEST_SLICE, malloc, free, realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, " mempool memory test for ~4Mb\n");
  mempool_speedtest((2 << 21) / MEMTEST_SLICE, mempool_malloc, mempool_free,
                    mempool_realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test for ~8Mb\n");
  mempool_speedtest((2 << 22) / MEMTEST_SLICE, malloc, free, realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, " mempool memory test for ~8Mb\n");
  mempool_speedtest((2 << 22) / MEMTEST_SLICE, mempool_malloc, mempool_free,
                    mempool_realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "System memory test for ~16Mb\n");
  mempool_speedtest((2 << 23) / MEMTEST_SLICE, malloc, free, realloc);
  fprintf(stderr, "*****************************\n");
  fprintf(stderr, " mempool memory test for ~16Mb\n");
  mempool_speedtest((2 << 23) / MEMTEST_SLICE, mempool_malloc, mempool_free,
                    mempool_realloc);
  fprintf(stderr, "*****************************\n");

  fprintf(stderr, "*****************************\n");
  fprintf(stderr, "Stressing the system\n");
  fprintf(stderr, "*****************************\n");
  size_t repeat = 1024 * 1024 * 16;
  size_t unit = 16;
  struct timespec start_test, end_test;
  clock_t start, end;
  fprintf(stderr, "Stress allocation/deallocation using "
                  "1:5 fragmentation of ~134Mb:\n");
  while (repeat >= 1024) {
    fprintf(stderr, " * %lu X %lu bytes", repeat, unit);
    clock_gettime(CLOCK_MONOTONIC, &start_test);
    start = clock();
    void **ptrs = mempool_malloc(repeat * sizeof(void *));
    for (size_t i = 0; i < repeat; i++) {
      ptrs[i] = mempool_malloc(unit);
    }
    for (size_t i = 0; i < repeat; i += 5) {
      mempool_free(ptrs[i]);
    }
    for (size_t i = 1; i < repeat; i += 5) {
      mempool_free(ptrs[i]);
    }
    for (size_t i = 2; i < repeat; i += 5) {
      mempool_free(ptrs[i]);
    }
    for (size_t i = 3; i < repeat; i += 5) {
      mempool_free(ptrs[i]);
    }
    for (size_t i = 4; i < repeat; i += 5) {
      mempool_free(ptrs[i]);
    }
    for (size_t i = 0; i < repeat; i++) {
      ptrs[i] = mempool_malloc(unit);
    }
    for (size_t i = 0; i < repeat; i++) {
      mempool_free(ptrs[i]);
    }
    mempool_free(ptrs);
    end = clock();
    clock_gettime(CLOCK_MONOTONIC, &end_test);
    uint64_t msec_for_test =
        (end_test.tv_nsec < start_test.tv_nsec)
            ? ((end_test.tv_sec -= 1), (start_test.tv_nsec - end_test.tv_nsec))
            : (end_test.tv_nsec - start_test.tv_nsec);
    uint64_t sec_for_test = end_test.tv_sec - start_test.tv_sec;

    fprintf(stderr, " %llum, %llus %llu mili.sec. ( %lu CPU)\n",
            sec_for_test / 60, sec_for_test - (((sec_for_test) / 60) * 60),
            msec_for_test / 1000000, end - start);

    unit <<= 1;
    repeat >>= 1;
  }
}

#undef MEMTEST_SLICE

#endif

/* *****************************************************************************
Cleanup
*/
#undef MEMPOOL_BLOCK_SIZE
#undef MEMPOOL_ALLOC_SPECIAL
#undef MEMPOOL_DEALLOC_SPECIAL
#undef MEMPOOL_SIZE_MASK
#undef MEMPOOL_USED_MARKER
#undef MEMPOOL_INDI_MARKER
#undef MEMPOOL_SLICE2PTR
#undef MEMPOOL_PTR2SLICE
#undef MEMPOOL_LOCK
#undef MEMPOOL_UNLOCK
#undef MEMPOOL_ORDERING_LIMIT
#endif
