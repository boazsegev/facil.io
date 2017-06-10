/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef BSMEMPOOL_H
#define BSMEMPOOL_H
#include <stdint.h>
#include <stdlib.h>
/* *****************************************************************************
A basic, variable, memory pool.

This isn't meant to provide a high performance alternative to `malloc` (although
I hope it does), rather it's meant to help fight heap fragmentation issues
related to higher object lifetimes, as well as to provide a memory pool for
variable length objects.

As a side effect, it's probably possible to use this memory pool to sync memory
to a file, providing persistant storage of sorts (if you save a list of
allocated objects).
*/

/** `bs_mmpl_ptr` is an opaque pointer to the specific memory pool. */
typedef struct bs_mmpl_s *bs_mmpl_ptr;

/** These are the settings used to initialize a `bs_mmpl_ptr`. */
struct bs_mempool_settings {
  /**
   * the function to be used when allocating the memory from the system.
   * If `.alloc` and `.unalloc` aren't both specified, a best match will be
   * attempted.
   */
  void *(*alloc)(size_t size, void *arg);
  /**
   * the function to be used when releasing (unallocating) memory back to the
   * system.
   * If `.alloc` and `.unalloc` aren't both specified, a best match will be
   * attempted.
   */
  void (*unalloc)(void *ptr, size_t size, void *arg);
  /** an opaque user data that will be passed to the alloc/unalloc callbacks. */
  void *arg;
};

/**
 * Creates the memory pool object. You might notice no destructor is provided.
 */
bs_mmpl_ptr bs_mempool_create(struct bs_mempool_settings settings);
#define bs_mempool_create(...)                                                 \
  bs_mempool_create((struct bs_mempool_settings){__VA_ARGS__})

/** Allocates memory from the pool. */
void *bs_malloc(bs_mmpl_ptr pool, size_t size);
/**
 * Frees the memory, releasing it back to the pool (or, sometimes, the system).
 */
void bs_free(bs_mmpl_ptr pool, void *ptr);
/**
 * Behaves the same a the systems `realloc`, attempting to resize the memory
 * when possible. On error returns NULL (the old pointer data remains allocated
 * and valid) otherwise returns a new pointer (either equal to the old or after
 * releasing the old one).
 */
void *bs_realloc(bs_mmpl_ptr pool, void *ptr, size_t new_size);

#if defined(DEBUG) && DEBUG == 1
void bs_mempool_test(void);
void bs_mempool_print(void *branch);
#endif

#endif
