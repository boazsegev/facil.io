/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef MEMPOOL_H

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/* *****************************************************************************
********************************************************************************
Decleration
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

#define MEMPOOL_BLOCK_SIZE (1 << 21)

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
Memory slices and tree
*/

typedef struct mempool_reserved_slice_s {
  struct mempool_reserved_slice_s_offset { /** offset from this slice */
    uint32_t ahead;
    uint32_t behind;
  } offset;
  struct mempool_reserved_slice_s
      *next; /** Used for the free slices linked list. */
} mempool_reserved_slice_s;

#define MEMPOOL_USED_MARKER ((uint32_t)(~0UL << 21))
#define MEMPOOL_INDI_MARKER ((uint32_t)0xF7F7F7F7UL)
#define MEMPOOL_USED_MASK (MEMPOOL_BLOCK_SIZE - 1)

#define MEMPOOL_SLICE_SIZE (sizeof(mempool_reserved_slice_s))
#define MEMPOOL_SLICE_OFFSET (sizeof(mempool_reserved_slice_s_offset))
#define MEMPOOL_SLICE2PTR(slice) (((uintptr_t)(slice)) + MEMPOOL_SLICE_OFFSET)
#define MEMPOOL_PTR2SLICE(ptr)                                                 \
  ((mempool_reserved_slice_s)(((uintptr_t)(ptr)) - MEMPOOL_SLICE_OFFSET))
#define MEMPOOL_SLICE_NEXT(slice)                                              \
  ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) +                         \
                                ((slice)->offset.ahead & MEMPOOL_USED_MASK)))
#define MEMPOOL_SLICE_PREV(slice)                                              \
  ((mempool_reserved_slice_s *)(((uintptr_t)(slice)) -                         \
                                ((slice)->offset.behind)))

#define MEMPOOL_SLICE_SET_USED(slice)                                          \
  ((slice)->offset.ahead |= MEMPOOL_USED_MARKER)
#define MEMPOOL_SLICE_IS_USED(slice) ((slice)->offset.ahead & MEMPOOL_USED_MASK)

#define MEMPOOL_SLICE_SET_FREE(slice)                                          \
  ((slice)->offset.ahead &= MEMPOOL_USED_MASK)
#define MEMPOOL_SLICE_IS_FREE(slice)                                           \
  ((slice)->offset.ahead & MEMPOOL_USED_MARKER)

#define MEMPOOL_SLICE_SET_INDI(slice)                                          \
  ((slice)->offset.behind = MEMPOOL_INDI_MARKER)
#define MEMPOOL_SLICE_IS_INDI(slice)                                           \
  ((slice)->offset.behind == MEMPOOL_INDI_MARKER)

static struct {
  mempool_reserved_slice_s *available;
  spn_lock_i lock;
} mempool_reserved_pool = {.lock = SPN_LOCK_INIT};

#define MEMPOOL_LOCK() spn_lock(&mempool_reserved_pool.lock)
#define MEMPOOL_UNLOCK() spn_unlock(&mempool_reserved_pool.lock)

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

#define MEMPOOL_DEALLOC_SPECIAL(target, size) munmap(ptr, size);

#else

#define MEMPOOL_ALLOC_SPECIAL(target, size)                                    \
  do {                                                                         \
    target = malloc(size);                                                     \
  } while (0);

#define MEMPOOL_DEALLOC_SPECIAL(target, size) free(target);

#endif

#define MEMPOOL_ALLOC_BLOCK(target)                                            \
  do {                                                                         \
    MEMPOOL_ALLOC_SPECIAL(target, MEMPOOL_BLOCK_SIZE);                         \
    (mempool_reserved_slice_s) target.offset.behind = 0;                       \
    (mempool_reserved_slice_s) target.offset.ahead =                           \
        MEMPOOL_BLOCK_SIZE - MEMPOOL_SLICE_OFFSET;                             \
    MEMPOOL_SLICE_NEXT(target)->offset.ahead = 0;                              \
                                                                               \
  } while (0);

/* *****************************************************************************
Memory block slicing and memory pool list maintanence.
*/

static inline void
mempool_reserved_slice_push(mempool_reserved_slice_s *slice) {
  MEMPOOL_SLICE_SET_FREE(slice);
  mempool_reserved_slice_s **pos = &mempool_reserved_pool.available;
  while (*pos && (*pos)->offset.ahead <= slice->offset.ahead)
    pos = &(*pos)->next;
  *pos = slice;
}

static inline void mempool_reserved_slice_pop(mempool_reserved_slice_s *slice) {
  mempool_reserved_slice_s **pos = &mempool_reserved_pool.available;
  while (*pos && *pos != slice)
    pos = &(*pos)->next;
  if (*pos) {
    *pos = (*pos)->next;
    MEMPOOL_SLICE_SET_USED(slice);
  }
}

static inline void
mempool_reserved_slice_free(mempool_reserved_slice_s *slice) {}

static inline void mempool_reserved_slice_cut(mempool_reserved_slice_s *slice,
                                              size_t size) {}

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
