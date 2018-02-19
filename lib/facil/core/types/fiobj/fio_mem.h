/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_MEM_H

/**
 * This is a custom memory allocator the utilizes memory pools to allow for
 * concurrent memory allocations across threads.
 *
 * Allocated memory is always aligned on a 16 byte boundary.
 *
 * The memory allocator assumes high turn-around rates (fast
 * allocation/deallocation) and short life spans (memory is freed shortly after
 * it was allocated).
 *
 * These assumptions allow the allocator to ignore fragmentation within a
 * memory "block", waiting for the whole "block" to be freed before it's memory
 * is recycled.
 *
 * This allocator should NOT be used for objects with a long life-span, because
 * even a single persistent object will prevent the re-use of the whole memory
 * block (64Kb by default) from which it was allocated.
 *
 * A memory "block" includes 16 pages of memory (a page is assumed to be 4,096
 * bytes in length).
 *
 * Each block includes a header that uses reference counters and position
 * markers.
 *
 * The position marker (`pos`) marks the next available byte (counted in
 * multiples of 16).
 *
 * The reference counter (`ref`) counts how many pointers reference memory in
 * the block (including the "arean" that "owns" the block).
 *
 * Except for the position marker (`pos`) that acts the same as `sbrk`, there's
 * no way to know which "slices" are allocated and which "slices" are available.
 *
 * Small allocations (less than 60Kb) are never allocated on a block
 * boundary. If a memory allocation is aligned by a whole block, the memory was
 * allocated directly using `mmap` (and it uses a whole page, 4096 bytes, as a
 * header).
 *
 * The allocator uses `mmap` when requesting memory from the system and for
 * allocations bigger than block-page (60Kb).
 */
#define H_FIO_MEM_H

#include <stdlib.h>

/** If defined, `malloc` will be used instead of the fio_malloc functions */
#ifdef FIO_FORCE_MALLOC
#define fio_malloc(size) malloc((size))
#define fio_calloc(size, count) calloc((size), (count))
#define fio_free(ptr) free((ptr))
#define fio_realloc(ptr, new_size) realloc((ptr), (new_size))
#define fio_malloc_test()
#else

/** Allocates memory using a per-CPU core block memory pool. */
void *fio_malloc(size_t size);

/** Allocates memory using a per-CPU core block memory pool. memory is zeroed
 * out. */
void *fio_calloc(size_t size, size_t count);

/** Frees memory that was allocated using this library. */
void fio_free(void *ptr);

/**
 * Re-allocates memory. An attept to avoid copying the data is made only for
 * memory allocations larger than 64Kb.
 */
void *fio_realloc(void *ptr, size_t new_size);

/** Tests the facil.io memory allocator. */
void fio_malloc_test(void);

#endif

#ifndef FIO_MEM_MAX_BLOCKS_PER_CORE
/**
 * The maximum number of available memory blocks that will be pooled before
 * memory is returned to the system.
 */
#define FIO_MEM_MAX_BLOCKS_PER_CORE 32 /* approx. 2Mb per CPU core */
#endif

#endif /* H_FIO_MEM_H */
