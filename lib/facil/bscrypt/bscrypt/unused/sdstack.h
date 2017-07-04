/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

#ifndef SDSTACK_H
/**
# Simple Dirty ( or Dynamic) Stack
---

This is a short one file library for a simple dynamic stack that holds only
pointers (or pointer sized numbers).

It's meant as a short-life push pop storage (probably for flattenning recursive
algorithms). It will cause memory fragmentation if held for a long while, since
memory is only allocated (recycled but never released) throughout the lifetime
of the object.

*/
#define SDSTACK_H

/**
 * Use this value to initialize an sdstack_s struct object. Remember to freethe
 * stack's memory using `sds_clear`
 */
#define SDSTACK_INIT                                                           \
  (sdstack_s) { .count = 0, .head = NULL, .pool = NULL }

/* a pointer array block, used internally. */
typedef struct ___sdstack_node_s {
  struct ___sdstack_node_s *next;
  void *pointers[1024];
} ___sdstack_node_s;

/** The sdstack_s struct is used to store the dynamic stack. */
typedef struct {
  size_t count;
  ___sdstack_node_s *head;
  ___sdstack_node_s *pool;
} sdstack_s;

/** Pushes an object to the stack. */
UNUSED_FUNC static inline int sds_push(sdstack_s *restrict stack,
                                    void *restrict pointer) {
  if ((stack->count & 1023) == 0) {
    ___sdstack_node_s *tmp;
    if (stack->pool == NULL)
      tmp = malloc(sizeof(*tmp));
    else {
      tmp = stack->pool;
      stack->pool = tmp->next;
    }
    if (tmp == NULL)
      return -1;
    tmp->next = stack->head;
    stack->head = tmp;
  }
  stack->head->pointers[stack->count & 1023] = pointer;
  stack->count += 1;
  return 0;
}

/** Pops an object from the stack. */
UNUSED_FUNC static inline void *sds_pop(sdstack_s *restrict stack) {
  if (stack->count == 0)
    return NULL;
  stack->count -= 1;
  void *ret = stack->head->pointers[stack->count & 1023];
  if ((stack->count & 1023) == 0) {
    ___sdstack_node_s *tmp = stack->head->next;
    stack->head->next = stack->pool;
    stack->pool = stack->head;
    stack->head = tmp;
  }
  return ret;
}

/** Returns the object currently on top of the stack. */
UNUSED_FUNC static inline void *sds_peek(sdstack_s *restrict stack) {
  if (stack->head)
    return stack->head->pointers[stack->count & 1023];
  return NULL;
}

/** Returns true (non 0) if the stack in empty. */
UNUSED_FUNC static inline int sds_is_empty(const sdstack_s *restrict stack) {
  return stack->count == 0;
}

/** Clears the stack data, freeing it's memory. */
UNUSED_FUNC static inline void sds_clear(sdstack_s *restrict stack) {
  ___sdstack_node_s *tmp;
  tmp = stack->head;
  while ((tmp = stack->head)) {
    stack->head = stack->head->next;
    free(tmp);
  }
  while ((tmp = stack->pool)) {
    stack->pool = stack->pool->next;
    free(tmp);
  }
  stack->count = 0;
  return;
}

#if defined(DEBUG) && DEBUG == 1

#include <assert.h>
#include <time.h>

UNUSED_FUNC static inline void sds_test(void) {
  const size_t test_size = (1024UL * 1024 * 4);
  sdstack_s stack = SDSTACK_INIT;
  clock_t start, end;
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    if (sds_push(&stack, (void *)i))
      perror("couldn't push to stack"), exit(6);
  }
  end = clock();
  fprintf(stderr, "Filled the stack with %lu objects. %lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  // fprintf(stderr, "stack has %lu objects. peek shows %lu\n", stack.count,
  //         (uintptr_t)sds_peek(&stack));
  for (uintptr_t i = 0; i < test_size; i++) {
    assert(sds_pop(&stack) == (void *)(test_size - 1) - i);
  }
  end = clock();
  assert(stack.count == 0);
  fprintf(stderr, "Emptied the stack form all %lu objects and asserted. "
                  "%lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    sds_push(&stack, (void *)i);
  }
  end = clock();
  fprintf(stderr, "Re-Filled the stack with %lu objects. "
                  "%lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    sds_pop(&stack);
  }
  end = clock();
  fprintf(stderr, "Re-emptied the stack form all %lu objects "
                  "(no assertions). %lu CPU "
                  "cycles\n",
          test_size, end - start);
  start = clock();
  sds_clear(&stack);
  end = clock();
  fprintf(stderr, "Cleared the stacks memory "
                  "(no assertions). %lu CPU "
                  "cycles\n",
          end - start);
}

#endif

#endif
