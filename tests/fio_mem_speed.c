#include "fio_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEST_CYCLES_START 0
#define TEST_CYCLES_END 512

static size_t test_mem_functions(void *(*malloc_func)(size_t),
                                 void *(*calloc_func)(size_t, size_t),
                                 void *(*realloc_func)(void *, size_t),
                                 void (*free_func)(void *)) {
  size_t clock_alloc = 0, clock_realloc = 0, clock_free = 0, clock_calloc = 0,
         clock_round = 0, errors = 0;
  for (int i = TEST_CYCLES_START; i < TEST_CYCLES_END; ++i) {
    fwrite(".", 1, 1, stderr);
    void **pointers = calloc_func(sizeof(*pointers), 4096);
    clock_t start;
    start = clock();
    for (int j = 0; j < 4096; ++j) {
      pointers[j] = malloc_func(i << 4);
      if (i) {
        if (!pointers[j])
          ++errors;
        // else
        //   ((char *)pointers[j])[0] = '1';
      }
    }
    clock_alloc += clock() - start;

    start = clock();
    for (int j = 0; j < 4096; ++j) {
      void *tmp = realloc_func(pointers[j], i << 5);
      if (tmp) {
        pointers[j] = tmp;
        ((char *)pointers[j])[0] = '1';
      } else if (i)
        ++errors;
    }
    clock_realloc += clock() - start;

    start = clock();
    for (int j = 0; j < 4096; ++j) {
      free_func(pointers[j]);
      pointers[j] = NULL;
    }
    clock_free += clock() - start;

    start = clock();
    for (int j = 0; j < 4096; ++j) {
      pointers[j] = calloc_func(16, i);
      if (i) {
        if (!pointers[j])
          ++errors;
        else
          ((char *)pointers[j])[0] = '1';
      }
    }
    clock_calloc += clock() - start;

    for (int j = 0; j < 4096; ++j) {
      free_func(pointers[j]);
    }

    free_func(pointers);
  }
  clock_alloc /= TEST_CYCLES_END - TEST_CYCLES_START;
  clock_realloc /= TEST_CYCLES_END - TEST_CYCLES_START;
  clock_free /= TEST_CYCLES_END - TEST_CYCLES_START;
  clock_calloc /= TEST_CYCLES_END - TEST_CYCLES_START;
  fprintf(stderr, "\n* Avrg. clock count for malloc: %zu\n", clock_alloc);
  fprintf(stderr, "* Avrg. clock count for calloc: %zu\n", clock_calloc);
  fprintf(stderr, "* Avrg. clock count for realloc: %zu\n", clock_realloc);
  fprintf(stderr, "* Avrg. clock count for free: %zu\n", clock_free);
  fprintf(stderr, "* errors: %zu\n", errors);
  return clock_alloc + clock_realloc + clock_free + clock_calloc + clock_round;
}

int main(void) {
#if DEBUG
  fprintf(
      stderr,
      "\n=== WARNING: performance tests using the DEBUG mode are invalid. \n");
#endif
  fprintf(stderr, "===== Testing system memory allocator:\n");
  size_t system = test_mem_functions(malloc, calloc, realloc, free);
  fprintf(stderr, "===== Testing facil.io memory allocator:\n");
  size_t fio =
      test_mem_functions(fio_malloc, fio_calloc, fio_realloc, fio_free);
  return fio > system;
}
