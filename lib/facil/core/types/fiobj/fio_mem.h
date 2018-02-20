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
 * Allocated memory is always zeroed out and aligned on a 16 byte boundary.
 *
 * The memory allocator assumes multiple concurrent allocation/deallocation,
 * short life spans (memory is freed shortly after it was allocated) and small
 * allocations (realloc almost always copies data).
 *
 * These assumptions allow the allocator to ignore fragmentation within a
 * memory "block", waiting for the whole "block" to be freed before it's memory
 * is recycled.
 *
 * This allocator should NOT be used for objects with a long life-span, because
 * even a single persistent object will prevent the re-use of the whole memory
 * block (128Kb by default) from which it was allocated.
 *
 * A memory "block" can include any number of memory pages that are a multiple
 * of 2 (up to 1Mb of memory). However, the default value, set by
 * MEMORY_BLOCK_SIZE, is either 12Kb or 64Kb (set at end of header).
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
 * Small allocations are difrinciated by their memory alignment. If a memory
 * allocation is placed 8 bytes after whole block alignment, the memory was
 * allocated directly using `mmap` (and it might be using a whole page, 4096
 * bytes, as a header!).
 *
 * The allocator uses `mmap` when requesting memory from the system and for
 * allocations bigger than MEMORY_BLOCK_ALLOC_LIMIT (a quarter of a block).
 *
 * To replace the system's `malloc` function family compile with the
 * `FIO_OVERRIDE_MALLOC` defined (`-DFIO_OVERRIDE_MALLOC`).
 *
 * When using tcmalloc or jemalloc, define `FIO_FORCE_MALLOC` to prevent
 * `fio_mem` from compiling (`-DFIO_FORCE_MALLOC`). Function wrappers will be
 * compiled just in case, so calls to `fio_malloc` will be routed to `malloc`.
 *
 */
#define H_FIO_MEM_H

#include <stdlib.h>

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

/**
 * Re-allocates memory. An attept to avoid copying the data is made only for
 * memory allocations larger than 64Kb.
 *
 * This variation is slightly faster as it might copy less data
 */
void *fio_realloc2(void *ptr, size_t original_size, size_t new_size);

/** Tests the facil.io memory allocator. */
void fio_malloc_test(void);

/** If defined, `malloc` will be used instead of the fio_malloc functions */
#if FIO_FORCE_MALLOC
#define fio_malloc(size) malloc((size))
#define fio_calloc(size, count) calloc((size), (count))
#define fio_free(ptr) free((ptr))
#define fio_realloc(ptr, new_size) realloc((ptr), (new_size))
#define fio_realloc2(ptr, old_size, new_size) realloc((ptr), (new_size))
#define fio_malloc_test()

/* allows local override as well as global override */
#elif FIO_OVERRIDE_MALLOC
#define malloc(size) fio_malloc((size))
#define free(ptr) fio_free((ptr))
#define realloc(ptr, size) fio_realloc((ptr), (size))
#define calloc(size, count) fio_calloc(size, count)

#endif

#ifndef FIO_MEM_MAX_BLOCKS_PER_CORE
/**
 * The maximum number of available memory blocks that will be pooled before
 * memory is returned to the system.
 */
#define FIO_MEM_MAX_BLOCKS_PER_CORE 32 /* approx. 2Mb per CPU core */
#endif

/** Allocator default settings. */
#ifndef FIO_MEMORY_BLOCK_SIZE
#define FIO_MEMORY_BLOCK_SIZE ((uintptr_t)1 << 17) /* 17 == 128Kb */
#endif
#ifndef FIO_MEMORY_BLOCK_MASK
#define FIO_MEMORY_BLOCK_MASK (FIO_MEMORY_BLOCK_SIZE - 1) /* 0b111... */
#endif
#ifndef FIO_MEMORY_BLOCK_SLICES
#define FIO_MEMORY_BLOCK_SLICES (FIO_MEMORY_BLOCK_SIZE >> 4) /* 16b slices */
#endif
#ifndef FIO_MEMORY_BLOCK_ALLOC_LIMIT
#define FIO_MEMORY_BLOCK_ALLOC_LIMIT ((FIO_MEMORY_BLOCK_SIZE >> 2) + 4096)
#endif

#endif /* H_FIO_MEM_H */
