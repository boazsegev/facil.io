/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef SIMPLE_DIRTY_TYPES_H
#define SIMPLE_DIRTY_TYPES_H
/* use the GNU SOURCE extensions */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* Use the standard library. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Allow some inline functions to be created, without requiring their use. */
#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

/* *****************************************************************************
 * A simple String (up to 4GB long).
 */

typedef struct {
  uint32_t length;
  uint32_t capacity;
  char str[];
} string_s;

static inline UNUSED_FUNC string_s *string_new_buf(uint32_t capacity) {
  string_s *str = malloc(sizeof(*str) + capacity);
  if (!str)
    return NULL;
  str->capacity = capacity;
  str->length = 0;
  return str;
}

static inline UNUSED_FUNC string_s *string_new_cpy(const void *src, uint32_t len) {
  string_s *str = malloc(sizeof(*str) + len + 1);
  if (!str)
    return NULL;
  str->capacity = len + 1;
  str->length = len;
  memcpy(str->str, src, len);
  str->str[str->length] = 0;
  return str;
}

static inline UNUSED_FUNC string_s *string_new_cstr(const void *src) {
  return string_new_cpy(src, strlen(src));
}

static inline UNUSED_FUNC string_s *string_dup(const string_s *src) {
  string_s *str = malloc(sizeof(*str) + src->capacity);
  if (!str)
    return NULL;
  memcpy(str, src, src->length + sizeof(*src));
  return str;
}

/** Adds data to the string object. Returns the updates string object.
 * The memory address for the string might change if allocation of new memory is
 * required.
 *
 * Returns NULL on failure.
 */
static inline UNUSED_FUNC string_s *string_concat(string_s *dest, const void *src,
                                               uint32_t len) {
  if (dest->capacity <= dest->length + len) {
    dest = realloc(dest, dest->length + len + sizeof(*dest) + 1);
    if (!dest)
      return NULL;
  }
  memcpy(dest->str + dest->length, src, len);
  dest->length += len;
  dest->str[dest->length] = 0;
  return dest;
}

/* *****************************************************************************
 * A simple stack of void * (pointers or intptr_t integers) - push & pop.
 */

#define STACK_INITIAL_CAPACITY 32

typedef struct {
  size_t capacity;
  size_t length;
  void *data[];
} stack_s;

/** creates a new stack. free the stack using `free`. */
static inline UNUSED_FUNC stack_s *stack_new(void) {
  stack_s *stack =
      malloc(sizeof(*stack) + (sizeof(void *) * STACK_INITIAL_CAPACITY));
  stack->capacity = STACK_INITIAL_CAPACITY;
  stack->length = 0;
  return stack;
}

/** Returns the newest member of the stack, removing it from the stack. */
static inline UNUSED_FUNC void *stack_pop(stack_s *stack) {
  if (stack->length == 0)
    return NULL;
  stack->length -= 1;
  return stack->data[stack->length];
}
/** returns the topmost (newest) object of the stack without removing it. */
static inline UNUSED_FUNC void *stack_peek(stack_s *stack) {
  return (stack->length ? stack->data[stack->length - 1] : NULL);
}
/** returns the number of objects in the stack. */
static inline UNUSED_FUNC size_t stack_count(stack_s *stack) {
  return stack->length;
}

/** Adds a new object to the tsack, reallocating the stack if necessary.
  * Return the new stack pointer. Returns NULL of failure. */
static inline UNUSED_FUNC stack_s *stack_push(stack_s *stack, void *ptr) {
  if (stack->length == stack->capacity) {
    stack->capacity = stack->capacity << 1;
    stack = realloc(stack, sizeof(*stack) + (sizeof(void *) * stack->capacity));
    if (!stack)
      return NULL;
  }
  stack->data[stack->length] = ptr;
  stack->length += 1;
  return stack;
}

#if 1 || defined(DEBUG) && DEBUG == 1

#include <assert.h>
#include <time.h>

UNUSED_FUNC static inline void sdstack_test(void) {
  const size_t test_size = (1024UL * 1024 * 4);
  stack_s *stack = stack_new();
  clock_t start, end;
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    stack = stack_push(stack, (void *)i);
    if (stack == NULL)
      perror("couldn't push to stack"), exit(6);
  }
  end = clock();
  fprintf(stderr, "Filled the stack with %lu objects. %lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  // fprintf(stderr, "stack has %lu objects. peek shows %lu\n", stack.count,
  //         (uintptr_t)sds_peek(&stack));
  for (uintptr_t i = 0; i < test_size; i++) {
    assert(stack_pop(stack) == (void *)(test_size - 1) - i);
  }
  end = clock();
  assert(stack->length == 0);
  fprintf(stderr, "Emptied the stack form all %lu objects and asserted. "
                  "%lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    stack = stack_push(stack, (void *)i);
  }
  end = clock();
  fprintf(stderr, "Re-Filled the stack with %lu objects. "
                  "%lu CPU cycles\n",
          test_size, end - start);
  start = clock();
  for (uintptr_t i = 0; i < test_size; i++) {
    stack_pop(stack);
  }
  end = clock();
  fprintf(stderr, "Re-emptied the stack form all %lu objects "
                  "(no assertions). %lu CPU "
                  "cycles\n",
          test_size, end - start);
  start = clock();
  free(stack);
  end = clock();
  fprintf(stderr, "Cleared the stacks memory "
                  "(no assertions). %lu CPU "
                  "cycles\n",
          end - start);
}

#endif

/* *****************************************************************************
 * A simple HashMap ... find objects by a hash key.
 * Does this stack copy or reference objects? What about data locality?
 */

/** No more then 8 members (constricted hash collisions) per bin. */
#define HASHMAP_MAX_MEMBERS_PER_BIN 8

struct ___hashmap_bin {
  struct __hashmap_store {
    uint64_t hash;
    void *object;
  } members[HASHMAP_MAX_MEMBERS_PER_BIN];
};

typedef struct {
  size_t count;
  size_t bin_mask;
  struct ___hashmap_bin *bins;
} hashmap_s;

/** Allocates a new empty HashMap (hashmap_s) object. */
static inline UNUSED_FUNC hashmap_s *hashmap_new(void) {
  hashmap_s *h = malloc(sizeof(*h));
  *h = (hashmap_s){0};
  h->bins = calloc(sizeof(*h->bins), 1);
  return h;
}
/** Deallocates a HashMap (hashmap_s) object - but DOESN'T deallocate any stored
 * values (use `hashmap_each` and `hashmap_remove` to deallocate any objects).
 */
static inline UNUSED_FUNC void hashmap_destroy(hashmap_s *hashmap) {
  free(hashmap->bins);
  free(hashmap);
}

/** Returns an existing object from the map (if exists). Returns NULL if none
 * found. */
static inline UNUSED_FUNC void *hashmap_get(hashmap_s *hashmap, uint64_t hash) {
  struct ___hashmap_bin *bin;
  bin = hashmap->bins + (hash & hashmap->bin_mask);
  for (size_t i = 0; i < HASHMAP_MAX_MEMBERS_PER_BIN; i++) {
    if (bin->members[i].hash == 0)
      return NULL;
    if (bin->members[i].hash == hash)
      return bin->members[i].object;
  }
  return NULL;
}
/** Sets a hash value key to a new object in the map.
  * Returns the previous value (if exists) or NULL.
  */
static UNUSED_FUNC void *hashmap_set(hashmap_s *hashmap, uint64_t hash,
                                  void *object) {
  void *old_val;
  struct ___hashmap_bin *bin;
restart:
  bin = hashmap->bins + (hash & hashmap->bin_mask);
  for (size_t i = 0; i < HASHMAP_MAX_MEMBERS_PER_BIN; i++) {
    if (bin->members[i].hash == 0 || bin->members[i].hash == hash ||
        bin->members[i].object == NULL) {
      bin->members[i].hash = hash;
      old_val = bin->members[i].object;
      bin->members[i].object = object;
      return old_val;
    }
  }
  hashmap_s tmp = *hashmap;
  hashmap->bin_mask = ((hashmap->bin_mask + 1) << 1) - 1;
  hashmap->bins = calloc(sizeof(*hashmap->bins), hashmap->bin_mask + 1);
  for (size_t i = 0; (i & tmp.bin_mask) == i; i++) {
    for (size_t j = 0; j < HASHMAP_MAX_MEMBERS_PER_BIN; j++) {
      if (tmp.bins[i].members[j].hash == 0)
        break;
      if (tmp.bins[i].members[j].object)
        hashmap_set(hashmap, tmp.bins[i].members[j].hash,
                    tmp.bins[i].members[j].object);
    }
  }
  free(tmp.bins);
  goto restart;
}
/** Removes and returns an existing object from the map (if exists).
  * Returns NULL if no matching hash key was found.
  */
static inline UNUSED_FUNC void *hashmap_remove(hashmap_s *hashmap, uint64_t hash) {
  struct ___hashmap_bin *bin;
  void *old_val;
  bin = hashmap->bins + (hash & hashmap->bin_mask);
  for (size_t i = 0; i < HASHMAP_MAX_MEMBERS_PER_BIN; i++) {
    if (bin->members[i].hash == 0)
      return NULL;
    if (bin->members[i].hash == hash) {
      old_val = bin->members[i].object;
      bin->members[i].object = NULL;
      return old_val;
    }
  }
  return NULL;
}

/** Performs a task for eash [hash,object] pair. If the task returns `-1` the
 * iterations will stop.
 * Returns the number of iterations.
 */
static inline UNUSED_FUNC size_t hashmap_each(hashmap_s *hashmap,
                                           int (*task)(uint64_t hash,
                                                       void *object)) {
  size_t count = 0;
  for (size_t i = 0; (i & hashmap->bin_mask) == i; i++) {
    for (size_t j = 0; j < HASHMAP_MAX_MEMBERS_PER_BIN; j++) {
      if (hashmap->bins[i].members[j].hash == 0)
        break;
      if (hashmap->bins[i].members[j].object && (count += 1) &&
          task(hashmap->bins[i].members[j].hash,
               hashmap->bins[i].members[j].object) == -1)
        return count;
    }
  }
  return count;
}

/* *****************************************************************************
 * A simple string / data store. Stores data and it's reference count.
 */

/* *****************************************************************************
 *
 */

#endif
