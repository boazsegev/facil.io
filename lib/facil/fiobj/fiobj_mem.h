/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIOBJ_MEM_H

/**
 * This is a placeholder for facil.io memory allocator functions in fio.h.
 *
 * In cases where the FIOBJ library is extracted from facil.io, these functions
 * will call the system's memory allocator (`malloc`, `free`, etc').
 */
#define H_FIOBJ_MEM_H

#include <stdlib.h>

/**
 * Allocates memory using a per-CPU core block memory pool.
 * Memory is zeroed out.
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT (12,288 bytes when using 32Kb
 * blocks) will be redirected to `mmap`, as if `fio_mmap` was called.
 */
void *fio_malloc(size_t size);

/**
 * same as calling `fio_malloc(size_per_unit * unit_count)`;
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT (12,288 bytes when using 32Kb
 * blocks) will be redirected to `mmap`, as if `fio_mmap` was called.
 */
void *fio_calloc(size_t size_per_unit, size_t unit_count);

/** Frees memory that was allocated using this library. */
void fio_free(void *ptr);

/**
 * Re-allocates memory. An attept to avoid copying the data is made only for big
 * memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 */
void *fio_realloc(void *ptr, size_t new_size);

/**
 * Re-allocates memory. An attept to avoid copying the data is made only for big
 * memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 *
 * This variation is slightly faster as it might copy less data.
 */
void *fio_realloc2(void *ptr, size_t new_size, size_t copy_length);

/**
 * Allocates memory directly using `mmap`, this is prefered for objects that
 * both require almost a page of memory (or more) and expect a long lifetime.
 *
 * However, since this allocation will invoke the system call (`mmap`), it will
 * be inherently slower.
 *
 * `fio_free` can be used for deallocating the memory.
 */
void *fio_mmap(size_t size);

#if FIO_OVERRIDE_MALLOC
#define malloc fio_malloc
#define free fio_free
#define realloc fio_realloc
#define calloc fio_calloc
#endif

#endif /* H_FIOBJ_MEM_H */
