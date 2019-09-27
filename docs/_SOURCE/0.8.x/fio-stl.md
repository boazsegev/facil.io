---
title: facil.io - 0.8.x C STL - a Simple Template Library for C
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

At the core of the facil.io library is a single header Simple Template Library for C.

The Simple Template Library is a "jack knife" library, that uses MACROS to generate code for different common types, such as Hash Maps, Arrays, Linked Lists, Binary-Safe Strings, etc'.

The Simple Template Library also offers common functional primitives, such as bit operations, atomic operations, CLI parsing, JSON, and a custom memory allocator.

In other words, all the common building blocks one could need in a C project are placed in this single header file.

The header could be included multiple times with different results, creating different types or exposing different helper functions.

## A Lower Level API Notice for facil.io Application Developers

>> **The core library is probably not the API most facil.io application developers need to focus on**.
>>
>> This API is used to power the higher level API offered by the facil.io framework. If you're developing a facil.io application, use the higher level API when possible.

## Simple Template Library (STL) Overview

The core library is a single file library (`fio-stl.h`).

The core library includes a Simple Template Library for common types, such as:

* [Linked Lists](#linked-lists) - defined by `FIO_LIST_NAME`

* [Dynamic Arrays](#dynamic-arrays) - defined by `FIO_ARRAY_NAME`

* [Hash Maps / Sets](#hash-maps-sets) - defined by `FIO_MAP_NAME`

* [Binary Safe Dynamic Strings](#dynamic-strings) - defined by `FIO_STRING_NAME`

* [Binary Safe Small (non-dynamic) Strings](#small-non-dynamic-strings) - defined by `FIO_SMALL_STR_NAME`

* [Reference counting / Type wrapper](#reference-counting-type-wrapping) - defined by `FIO_REF_NAME`

* Soft / Dynamic Types (FIOBJ) - defined by `FIO_FIOBJ`

In addition, the core library includes helpers for common tasks, such as:

* [Pointer Tagging](#pointer-tagging-support) - defined by `FIO_PTR_TAG(p)`/`FIO_PTR_UNTAG(p)`

* [Logging and Assertion (without heap allocation)](#logging-and-assertions) - defined by `FIO_LOG`

* [Atomic operations](#atomic-operations) - defined by `FIO_ATOMIC`

* [Bit-Byte Operations](##bit-byte-operations) - defined by `FIO_BITWISE` and `FIO_BITMAP`

* [Data Hashing (using Risky Hash)](#risky-hash-data-hashing) - defined by `FIO_RISKY_HASH`

* [Pseudo Random Generation](#pseudo-random-generation) - defined by `FIO_RAND`

* [String / Number conversion](#string-number-conversion) - defined by `FIO_ATOL`

* [URL (URI) parsing](#url-uri-parsing) - defined by `FIO_URL`

* [Command Line Interface helpers](#cli-command-line-interface) - defined by `FIO_CLI`

* [Custom Memory Allocation](#memory-allocation) - defined by `FIO_MALLOC`

* [Basic Socket / IO Helpers](#basic-socket-io-helpers) - defined by `FIO_SOCK`

* [Custom JSON Parser](#custom-json-parser) - defined by `FIO_JSON`

## Testing the Library (`FIO_TEST_CSTL`)

To test the library, define the `FIO_TEST_CSTL` macro and include the header. A testing function called `fio_test_dynamic_types` will be defined. Call that function in your code to test the library.

-------------------------------------------------------------------------------

## Version and Common Helper Macros

The facil.io C STL (Simple Template Library) offers a number of common helper macros that are also used internally. These are automatically included once the `fio-stl.h` is included.

### Version Macros

The facil.io C STL library follows [semantic versioning](https://semver.org) and supports macros that will help detect and validate it's version.

#### `FIO_VERSION_MAJOR`

Translates to the STL's major version number.

MAJOR version upgrades require a code review and possibly significant changes. Even functions with the same name might change their behavior.

#### `FIO_VERSION_MINOR`

Translates to the STL's minor version number.

Please review your code before adopting a MINOR version upgrade.

#### `FIO_VERSION_PATCH`

Translates to the STL's patch version number.

PATCH versions should be adopted as soon as possible (they contain bug fixes).

#### `FIO_VERSION_BETA`

Translates to the STL's beta version number.

#### `FIO_VERSION_STRING`

Translates to the STL's version as a String (i.e., 0.8.0.beta1.

#### `FIO_VERSION_GUARD`

If the `FIO_VERSION_GUARD` macro is defined in **a single** translation unit (C file) **before** including `fio-stl.h` for the first time, then the version macros become available using functions as well: `fio_version_major`, `fio_version_minor`, etc'.

#### `FIO_VERSION_VALIDATE`

By adding the `FIO_VERSION_GUARD` functions, a version test could be performed during runtime (which can be used for static libraries), using the macro `FIO_VERSION_VALIDATE()`.

### Pointer Arithmetics

#### `FIO_PTR_MATH_LMASK`

```c
#define FIO_PTR_MATH_LMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & (((uintptr_t)1 << (bits)) - 1)))
```

Masks a pointer's left-most bits, returning the right bits.

#### `FIO_PTR_MATH_RMASK`

```c
#define FIO_PTR_MATH_RMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & ((~(uintptr_t)0) << bits)))
```

Masks a pointer's right-most bits, returning the left bits.

#### `FIO_PTR_MATH_ADD`

```c
#define FIO_PTR_MATH_ADD(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) + (uintptr_t)(offset)))
```

Add offset bytes to pointer, updating the pointer's type.

#### `FIO_PTR_MATH_SUB`

```c
#define FIO_PTR_MATH_SUB(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) - (uintptr_t)(offset)))
```

Subtract X bytes from pointer, updating the pointer's type.

#### `FIO_PTR_FROM_FIELD`

```c
#define FIO_PTR_FROM_FIELD(T_type, field, ptr)                                 \
  FIO_PTR_MATH_SUB(T_type, ptr, (&((T_type *)0)->field))
```

Find the root object (of a `struct`) from it's field.

### Default Memory Allocation

By setting these macros, the default (system's) memory allocator could be set.

When facil.io's memory allocator is defined (using `FIO_MALLOC`), these macros will be automatically overwritten to use the custom memory allocator.

#### `FIO_MEM_CALLOC`

```c
#define FIO_MEM_CALLOC(size, units) calloc((size), (units))
```

Allocates size X units of bytes, where all bytes equal zero.

#### `FIO_MEM_REALLOC`

```c
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len) realloc((ptr), (new_size))
```

Reallocates memory, copying (at least) `copy_len` if neccessary.

#### `FIO_MEM_FREE`

```c
#define FIO_MEM_FREE(ptr, size) free((ptr))
```

Frees allocated memory.

#### `FIO_MALLOC_TMP_USE_SYSTEM`

When defined, temporarily bypasses the `FIO_MEM_CALLOC` macros and uses the system's `calloc`, `realloc` and `free` functions.

### Naming and Misc. Macros

#### `FIO_IFUNC`

```c
#define FIO_IFUNC static inline __attribute__((unused))
```

Marks a function as `static`, `inline` and possibly unused.

#### `FIO_SFUNC`

```c
#define FIO_SFUNC static __attribute__((unused))
```

Marks a function as `static` and possibly unused.

#### `FIO_NAME`

```c
#define FIO_NAME(prefix, postfix)
```

Used for naming functions and variables resulting in: prefix_postfix

#### `FIO_NAME2`

```c
#define FIO_NAME2(prefix, postfix)
```

Sets naming convention for conversion functions, i.e.: foo2bar

#### `FIO_NAME_BL`

```c
#define FIO_NAME_BL(prefix, postfix) 
```

Sets naming convention for boolean testing functions, i.e.: foo_is_true


-------------------------------------------------------------------------------

## Linked Lists

Doubly Linked Lists are an incredibly common and useful data structure.

### Linked Lists Performance

Linked Lists use pointers in order to provide fast add/remove operations with O(1) speeds. This O(1) operation ignores the object allocation time and suffers from poor memory locality, but it's still very fast.

However, Linked Lists suffer from slow seek/find and iteration operations.

Seek/find has a worst case scenario O(n) cost and iteration suffers from a high likelihood of CPU cache misses, resulting in degraded performance.

### Linked Lists Overview

Before creating linked lists, the library header should be included at least once.

To create a linked list type, create a `struct` that includes a `FIO_LIST_NODE` typed element somewhere within the structure. For example:

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
```

Next define the `FIO_LIST_NAME` macro. The linked list helpers and types will all be prefixed by this name. i.e.:

```c
#define FIO_LIST_NAME my_list /* results in (example): my_list_push(...) */
```

Optionally, define the `FIO_LIST_TYPE` macro to point at the correct linked-list structure type. By default, the type for linked lists will be `<FIO_LIST_NAME>_s`.

An example were we need to define the `FIO_LIST_TYPE` macro will follow later on.

Optionally, define the `FIO_LIST_NODE_NAME` macro to point the linked list's node. By default, the node for linked lists will be `node`.

Finally, include the `fio-stl.h` header to create the linked list helpers.

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element 
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
// create linked list helper functions
#define FIO_LIST_NAME my_list
#include "fio-stl.h"

void example(void) {
  FIO_LIST_HEAD list = FIO_LIST_INIT(list);
  for (int i = 0; i < 10; ++i) {
    my_list_s *n = malloc(sizeof(*n));
    n->i = i;
    my_list_push(&list, n);
  }
  int i = 0;
  while (my_list_any(&list)) {
    my_list_s *n = my_list_shift(&list);
    if (i != n->i) {
      fprintf(stderr, "list error - value mismatch\n"), exit(-1);
    }
    free(n);
    ++i;
  }
  if (i != 10) {
    fprintf(stderr, "list error - count error\n"), exit(-1);
  }
}
```

**Note**:

Each node is limited to a single list (an item can't belong to more then one list, unless it's a list of pointers to that item).

Items with more then a single node can belong to more then one list. i.e.:

```c
// list element 
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
// list 1 
#define FIO_LIST_NAME my_list
#include "fio-stl.h"
// list 2 
#define FIO_LIST_NAME my_list2
#define FIO_LIST_TYPE my_list_s
#define FIO_LIST_NODE_NAME node2
#include "fio-stl.h"
```

### Linked Lists (embeded) - API


#### `FIO_LIST_INIT(head)`

```c
#define FIO_LIST_INIT(obj)                                                     \
  { .next = &(obj), .prev = &(obj) }
```

This macro initializes an uninitialized node (assumes the data in the node is junk). 

#### `LIST_any`

```c
int LIST_any(FIO_LIST_HEAD *head)
```
Returns a non-zero value if there are any linked nodes in the list. 

#### `LIST_is_empty`

```c
int LIST_is_empty(FIO_LIST_HEAD *head)
```
Returns a non-zero value if the list is empty. 

#### `LIST_remove`

```c
FIO_LIST_TYPE *LIST_remove(FIO_LIST_TYPE *node)
```
Removes a node from the list, Returns NULL if node isn't linked. 

#### `LIST_push`

```c
FIO_LIST_TYPE *LIST_push(FIO_LIST_HEAD *head, FIO_LIST_TYPE *node)
```
Pushes an existing node to the end of the list. Returns node. 

#### `LIST_pop`

```c
FIO_LIST_TYPE *LIST_pop(FIO_LIST_HEAD *head)
```
Pops a node from the end of the list. Returns NULL if list is empty. 

#### `LIST_unshift`

```c
FIO_LIST_TYPE *LIST_unshift(FIO_LIST_HEAD *head, FIO_LIST_TYPE *node)
```
Adds an existing node to the beginning of the list. Returns node.

#### `LIST_shift`

```c
FIO_LIST_TYPE *LIST_shift(FIO_LIST_HEAD *head)
```
Removed a node from the start of the list. Returns NULL if list is empty. 

#### `LIST_root`

```c
FIO_LIST_TYPE *LIST_root(FIO_LIST_HEAD *ptr)
```
Returns a pointer to a list's element, from a pointer to a node. 

#### `FIO_LIST_EACH`

_Note: macro, name unchanged, works for all lists_

```c
#define FIO_LIST_EACH(type, node_name, head, pos)                              \
  for (type *pos = FIO_PTR_FROM_FIELD(type, node_name, (head)->next),          \
            *next____p_ls =                                                    \
                FIO_PTR_FROM_FIELD(type, node_name, (head)->next->next);       \
       pos != FIO_PTR_FROM_FIELD(type, node_name, (head));                     \
       (pos = next____p_ls),                                                   \
            (next____p_ls = FIO_PTR_FROM_FIELD(type, node_name,                \
                                               next____p_ls->node_name.next)))
```

Loops through every node in the linked list (except the head).

The `type` name should reference the list type.

`node_name` should indicate which node should be used for iteration.

`head` should point at the head of the list (usually a `FIO_LIST_HEAD` variable).

`pos` can be any temporary variable name that will contain the current position in the iteration.

The list **can** be mutated during the loop, but this is not recommended. Specifically, removing `pos` is safe, but pushing elements ahead of `pos` might result in an endless loop.

_Note: this macro won't work with pointer tagging_

-------------------------------------------------------------------------------

## Dynamic Arrays

Dynamic arrays are extremely common and useful data structures.

In essence, Arrays are blocks of memory that contain all their elements "in a row". They grow (or shrink) as more items are added (or removed).

Items are accessed using a numerical `index` indicating the element's position within the array.

Indexes are zero based (first element == 0).

### Dynamic Array Performance

Seeking time is an extremely fast O(1). Arrays are also very fast to iterate since they enjoy high memory locality.

Adding and editing items is also a very fast O(1), especially if enough memory was previously reserved. Otherwise, memory allocation and copying will slow performance.

However, arrays suffer from slow find operations. Find has a worst case scenario O(n) cost.

They also suffer from slow item removal (except, in our case, for `pop` / `unshift` operations), since middle-element removal requires memory copying when fixing the "hole" made in the array.

A common solution is to reserve a value for "empty" elements and `set` the element's value instead of `remove` the element.

**Note**: unlike some dynamic array implementations, this STL implementation doesn't grow exponentially. Using the `ARY_reserve` function is highly encouraged for performance.


### Dynamic Array Overview

To create a dynamic array type, define the type name using the `FIO_ARRAY_NAME` macro. i.e.:

```c
#define FIO_ARRAY_NAME int_ary
```

Next (usually), define the `FIO_ARRAY_TYPE` macro with the element type. The default element type is `void *`. For example:

```c
#define FIO_ARRAY_TYPE int
```

For complex types, define any (or all) of the following macros:

```c
// set to adjust element copying 
#define FIO_ARRAY_TYPE_COPY(dest, src)  
// set for element cleanup 
#define FIO_ARRAY_TYPE_DESTROY(obj)     
// set to adjust element comparison 
#define FIO_ARRAY_TYPE_CMP(a, b)        
// to be returned when `index` is out of bounds / holes 
#define FIO_ARRAY_TYPE_INVALID 0 
// set ONLY if the invalid element is all zero bytes 
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1     
// should the object be destroyed when copied to an `old` pointer?
#define FIO_ARRAY_DESTROY_AFTER_COPY 1 
// when array memory grows, how many extra "spaces" should be allocated?
#define FIO_ARRAY_PADDING 4 
// should the array growth be exponential? (ignores FIO_ARRAY_PADDING)
#define FIO_ARRAY_EXPONENTIAL 0 
```

To create the type and helper functions, include the Simple Template Library header.

For example:

```c
typedef struct {
  int i;
  float f;
} foo_s;

#define FIO_ARRAY_NAME ary
#define FIO_ARRAY_TYPE foo_s
#define FIO_ARRAY_TYPE_CMP(a,b) (a.i == b.i && a.f == b.f)
#include "fio-stl.h"

void example(void) {
  ary_s a = FIO_ARRAY_INIT;
  foo_s *p = ary_push(&a, (foo_s){.i = 42});
  FIO_ARRAY_EACH(&a, pos) { // pos will be a pointer to the element
    fprintf(stderr, "* [%zu]: %p : %d\n", (size_t)(pos - ary_to_a(&a)), pos->i);
  }
  ary_destroy(&a);
}
```

### Dynamic Arrays - API

#### The Array Type (`ARY_s`)

```c
typedef struct {
  FIO_ARRAY_TYPE *ary;
  uint32_t capa;
  uint32_t start;
  uint32_t end;
} FIO_NAME(FIO_ARRAY_NAME, s); /* ARY_s in these docs */
```

The array type should be considered opaque. Use the helper functions to updated the array's state when possible, even though the array's data is easily understood and could be manually adjusted as needed.

#### `FIO_ARRAY_INIT`

````c
#define FIO_ARRAY_INIT  {0}
````

This macro initializes an uninitialized array object.

#### `ARY_destroy`

````c
void ARY_destroy(ARY_s * ary);
````

Destroys any objects stored in the array and frees the internal state.

#### `ARY_new`

````c
ARY_s * ARY_new(void);
````

Allocates a new array object on the heap and initializes it's memory.

#### `ARY_free`

````c
void ARY_free(ARY_s * ary);
````

Frees an array's internal data AND it's container!

#### `ARY_count`

````c
uint32_t ARY_count(ARY_s * ary);
````

Returns the number of elements in the Array.

#### `ARY_capa`

````c
uint32_t ARY_capa(ARY_s * ary);
````

Returns the current, temporary, array capacity (it's dynamic).

#### `ARY_reserve`

```c
uint32_t ARY_reserve(ARY_s * ary, int32_t capa);
```

Reserves a minimal capacity for the array.

If `capa` is negative, new memory will be allocated at the beginning of the array rather then it's end.

Returns the array's new capacity.

**Note**: the reserved capacity includes existing data / capacity. If the requested reserved capacity is equal (or less) then the existing capacity, nothing will be done.

#### `ARY_concat`

```c
ARY_s * ARY_concat(ARY_s * dest, ARY_s * src);
```

Adds all the items in the `src` Array to the end of the `dest` Array.

The `src` Array remain untouched.

Always returns the destination array (`dest`).

#### `ARY_set`

```c
FIO_ARRAY_TYPE * ARY_set(ARY_s * ary,
                       int32_t index,
                       FIO_ARRAY_TYPE data,
                       FIO_ARRAY_TYPE *old);
```

Sets `index` to the value in `data`.

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

If `old` isn't NULL, the existing data will be copied to the location pointed to by `old` before the copy in the Array is destroyed.

Returns a pointer to the new object, or NULL on error.

#### `ARY_get`

```c
FIO_ARRAY_TYPE ARY_get(ARY_s * ary, int32_t index);
```

Returns the value located at `index` (no copying is performed).

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

**Reminder**: indexes are zero based (first element == 0).

#### `ARY_find`

```c
int32_t ARY_find(ARY_s * ary, FIO_ARRAY_TYPE data, int32_t start_at);
```

Returns the index of the object or -1 if the object wasn't found.

If `start_at` is negative (i.e., -1), than seeking will be performed in reverse, where -1 == last index (-2 == second to last, etc').

#### `ARY_remove`
```c
int ARY_remove(ARY_s * ary, int32_t index, FIO_ARRAY_TYPE *old);
```

Removes an object from the array, MOVING all the other objects to prevent "holes" in the data.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns 0 on success and -1 on error.

This action is O(n) where n in the length of the array. It could get expensive.

#### `ARY_remove2`

```c
uint32_t ARY_remove2(ARY_S * ary, FIO_ARRAY_TYPE data);
```

Removes all occurrences of an object from the array (if any), MOVING all the existing objects to prevent "holes" in the data.

Returns the number of items removed.

This action is O(n) where n in the length of the array. It could get expensive.

#### `ARY_compact`
```c
void ARY_compact(ARY_s * ary);
```

Attempts to lower the array's memory consumption.

#### `ARY_to_a`

```c
FIO_ARRAY_TYPE * ARY_to_a(ARY_s * ary);
```

Returns a pointer to the C array containing the objects.

#### `ARY_push`

```c
FIO_ARRAY_TYPE * ARY_push(ARY_s * ary, FIO_ARRAY_TYPE data);
```

 Pushes an object to the end of the Array. Returns a pointer to the new object or NULL on error.

#### `ARY_pop`

```c
int ARY_pop(ARY_s * ary, FIO_ARRAY_TYPE *old);
```

Removes an object from the end of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_unshift`

```c
FIO_ARRAY_TYPE *ARY_unshift(ARY_s * ary, FIO_ARRAY_TYPE data);
```

Unshifts an object to the beginning of the Array. Returns a pointer to the new object or NULL on error.

This could be expensive, causing `memmove`.

#### `ARY_shift`

```c
int ARY_shift(ARY_s * ary, FIO_ARRAY_TYPE *old);
```

Removes an object from the beginning of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_each`

```c
uint32_t ARY_each(ARY_s * ary, int32_t start_at,
                               int (*task)(FIO_ARRAY_TYPE obj, void *arg),
                               void *arg);
```

Iteration using a callback for each entry in the array.

The callback task function must accept an the entry data as well as an opaque user pointer.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position (number of items processed + starting point).

#### `FIO_ARRAY_EACH`

```c
#define FIO_ARRAY_EACH(array, pos)                                               \
  if ((array)->ary)                                                            \
    for (__typeof__((array)->ary) start__tmp__ = (array)->ary,                 \
                                  pos = ((array)->ary + (array)->start);       \
         pos < (array)->ary + (array)->end;                                    \
         (pos = (array)->ary + (pos - start__tmp__) + 1),                      \
                                  (start__tmp__ = (array)->ary))
```

Iterates through the list using a `for` loop.

Access the object with the pointer `pos`. The `pos` variable can be named however you please.

It's possible to edit elements within the loop, but avoid editing the array itself (adding / removing elements). Although I hope it's possible, I wouldn't count on it and it could result in items being skipped or unending loops.

-------------------------------------------------------------------------------

## Hash Maps / Sets

Hash maps and sets are extremely useful and common mapping / dictionary primitives, also sometimes known as "dictionary".

Hash maps use both a `hash` and a `key` to identify a `value`. The `hash` value is calculated by feeding the key's data to a hash function (such as Risky Hash or SipHash).

A hash map without a `key` is known as a Set or a Bag. It uses only a `hash` (often calculated using the `value`) to identify a `value`, sometimes requiring a `value` equality test as well. This approach often promises a collection of unique values (no duplicate values).

Some map implementations support a FIFO limited storage, which could be used for limited-space caching.

### Map Performance

Seeking time is usually a fast O(1), although partial or full `hash` collisions may increase the cost of the operation.

Adding, editing and removing items is also a very fast O(1), especially if enough memory was previously reserved. However, memory allocation and copying will slow performance, especially when the map need to grow or requires de-fragmentation.

Iteration in this implementation doesn't enjoy memory locality, except for small maps or where the order of insertion randomly produces neighboring hashes. Maps are implemented using an array. The objects are accessed by order of insertion, but they are stored out of order (according to their hash value).

This map implementation has protection features against too many full collisions or non-random hashes. When the map detects a possible "attack", it will start overwriting existing data instead of trying to resolve collisions. This can be adjusted using the `FIO_MAP_MAX_FULL_COLLISIONS` macro.

### Map Overview 

To create a map, define `FIO_MAP_NAME`.

To create a hash map (rather then a set), also define `FIO_MAP_KEY` (containing the key's type).

Other helpful macros to define might include:


- `FIO_MAP_TYPE`, which defaults to `void *`
- `FIO_MAP_TYPE_INVALID`, which defaults to `((FIO_MAP_TYPE){0})`
- `FIO_MAP_TYPE_COPY(dest, src)`, which defaults to `(dest) = (src)`
- `FIO_MAP_TYPE_DESTROY(obj)`
- `FIO_MAP_TYPE_CMP(a, b)`, which defaults to `1`
- `FIO_MAP_KEY`
- `FIO_MAP_KEY_INVALID`
- `FIO_MAP_KEY_COPY(dest, src)`
- `FIO_MAP_KEY_DESTROY(obj)`
- `FIO_MAP_KEY_CMP(a, b)`
- `FIO_MAP_MAX_FULL_COLLISIONS`, which defaults to `96`


- `FIO_MAP_DESTROY_AFTER_COPY` - uses "smart" defaults to decide if to destroy an object after it was copied (when using `set` / `remove` / `pop` with a pointer to contain `old` object)
- `FIO_MAP_TYPE_DISCARD(obj)` - Handles discarded element data (i.e., insert without overwrite in a Set).
- `FIO_MAP_KEY_DISCARD(obj)` - Handles discarded element data (i.e., when overwriting an existing value in a hash map).
- `FIO_MAP_MAX_ELEMENTS` - The maximum number of elements allowed before removing old data (FIFO).
- `FIO_MAP_MAX_SEEK` -  The maximum number of bins to rotate when (partial/full) collisions occur. Limited to a maximum of 255.

To limit the number of elements in a map (FIFO), allowing it to behave similarly to a caching primitive, define: `FIO_MAP_MAX_ELEMENTS`.

if `FIO_MAP_MAX_ELEMENTS` is `0`, then the theoretical maximum number of elements should be: `(1 << 32) - 1`. In practice, the safe limit should be calculated as `1 << 31`.

Example:

```c
/* TODO */
```

### Hash Map / Set - API (initialization)

#### `MAP_new`

```c
FIO_MAP_PTR MAP_new(void);
```

Allocates a new map on the heap.

#### `MAP_free`

```c
void MAP_free(MAP_PTR m);
```

Frees a map that was allocated on the heap.

#### `FIO_MAP_INIT`

```c
#define FIO_MAP_INIT { .map = NULL }
```

This macro initializes a map object - often used for maps placed on the stack.

#### `MAP_destroy`

```c
void MAP_destroy(MAP_PTR m);
```

Destroys the map's internal data and re-initializes it.


### Hash Map - API (hash map only)

#### `MAP_find` (hash map)

```c
FIO_MAP_TYPE MAP_find(FIO_MAP_PTR m,
                      FIO_MAP_HASH hash,
                      FIO_MAP_KEY key);
```
Returns the object in the hash map (if any) or FIO_MAP_TYPE_INVALID.

#### `MAP_insert` (hash map)

```c
FIO_MAP_TYPE MAP_insert(FIO_MAP_PTR m,
               FIO_MAP_HASH hash,
               FIO_MAP_KEY key,
               FIO_MAP_TYPE obj,
               FIO_MAP_TYPE *old);
```


Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location.

#### `MAP_remove` (hash map)

```c
int MAP_remove(FIO_MAP_PTR m,
               FIO_MAP_HASH hash,
               FIO_MAP_KEY key,
               FIO_MAP_TYPE *old);
```

Removes an object from the hash map.

If `old` is given, existing data will be copied to that location.

Returns 0 on success or -1 if the object couldn't be found.

### Set - API (set only)

#### `MAP_find` (set)

```c
FIO_MAP_TYPE MAP_find(FIO_MAP_PTR m,
                       FIO_MAP_HASH hash,
                       FIO_MAP_TYPE obj);
```

Returns the object in the hash map (if any) or `FIO_MAP_TYPE_INVALID`.

#### `MAP_insert` (set)

```c
FIO_MAP_TYPE MAP_insert(FIO_MAP_PTR m,
                         FIO_MAP_HASH hash,
                         FIO_MAP_TYPE obj);
```

Inserts an object to the hash map, returning the existing or new object.

If `old` is given, existing data will be copied to that location.

#### `MAP_overwrite` (set)

```c
void MAP_overwrite(FIO_MAP_PTR m,
                    FIO_MAP_HASH hash,
                    FIO_MAP_TYPE obj,
                    FIO_MAP_TYPE *old);
```

Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location.


#### `MAP_remove` (set)

```c
int MAP_remove(FIO_MAP_PTR m, FIO_MAP_HASH hash,
               FIO_MAP_TYPE obj, FIO_MAP_TYPE *old);
```

Removes an object from the hash map.

If `old` is given, existing data will be copied to that location.

Returns 0 on success or -1 if the object couldn't be found.

### Hash Map / Set - API (common)

#### `MAP_count`

```c
uintptr_t MAP_count(FIO_MAP_PTR m);
```

Returns the number of objects in the map.

#### `MAP_capa`

```c
uintptr_t MAP_capa(FIO_MAP_PTR m);
```

Returns the current map's theoretical capacity.

#### `MAP_reserve`

```c
uintptr_t MAP_reserve(FIO_MAP_PTR m, uint32_t capa);
```

Reserves a minimal capacity for the hash map.

#### `MAP_last`

```c
FIO_MAP_TYPE MAP_last(FIO_MAP_PTR m);
```

Allows a peak at the Set's last element.

Remember that objects might be destroyed if the Set is altered (`FIO_MAP_TYPE_DESTROY` / `FIO_MAP_KEY_DESTROY`).

#### `MAP_pop`

```c
void MAP_pop(FIO_MAP_PTR m, FIO_MAP_TYPE * old);
```

Allows the Hash to be momentarily used as a stack, destroying the last object added (using `FIO_MAP_TYPE_DESTROY` / `FIO_MAP_KEY_DESTROY`).

If `old` is given, existing data will be copied to that location.

#### `MAP_compact`

```c
void MAP_compact(FIO_MAP_PTR m);
```

Attempts to lower the map's memory consumption.

#### `MAP_rehash`

```c
int MAP_rehash(FIO_MAP_PTR m);
```

Rehashes the Hash Map / Set. Usually this is performed automatically, no need to call the function.

#### `MAP_each`

```c
uint32_t FIO_NAME(FIO_MAP_NAME, each)(FIO_MAP_PTR m,
                                      int32_t start_at,
                                      int (*task)(FIO_MAP_TYPE obj, void *arg),
                                      void *arg);
```

Iteration using a callback for each element in the map.

The callback task function must accept an element variable as well as an opaque user pointer.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position, i.e., the number of items processed + the starting point.

#### `MAP_each_get_key`

```c
FIO_MAP_KEY FIO_NAME(FIO_MAP_NAME, each_get_key)(void);
```

Returns the current `key` within an `each` task.

Only available within an `each` loop.

_Note: For sets, returns the hash value, for hash maps, returns the key value._

#### `FIO_MAP_EACH`

```c
#define FIO_MAP_EACH(map_, pos_)                                               \
  for (__typeof__((map_)->map) prev__ = NULL,                                  \
                               pos_ = (map_)->map + (map_)->head;              \
       (map_)->head != (uint32_t)-1 &&                                         \
       (prev__ == NULL || pos_ != (map_)->map + (map_)->head);                 \
       (prev__ = pos_), pos_ = (map_)->map + pos_->next)
```

A macro for a `for` loop that iterates over all the Map's objects (in order).

Use this macro for small Hash Maps / Sets.

`map` is a pointer to the Hash Map / Set variable and `pos` is a temporary variable name to be created for iteration.

`pos->hash` is the hashing value and `pos->obj` is the object's data.

For hash maps, use `pos->obj.key` and `pos->obj.value` to access the stored data.

_Note: this macro doesn't work with pointer tagging_.

-------------------------------------------------------------------------------

## Dynamic Strings

To create a dynamic string type, define the type name using the `FIO_STRING_NAME`
macro.

The type (`FIO_STRING_NAME_s`) and the functions will be automatically defined.

### String Type information

#### `STR_s`

The core type, created by the macro, is the `STR_s` type - where `STR` is replaced by `FIO_STRING_NAME`. i.e.:

```c
#define FIO_STRING_NAME my_str
#include <fio-stl.h>
// results in: my_str_s - i.e.:
void hello(void){
  my_str_s msg = FIO_STRING_INIT;
  my_str_write(&msg, "Hello World", 11);
  printf("%s\n", my_str_data(&msg));
  my_str_destroy(&msg);
}
```

The type should be considered **opaque** and **must never be accessed directly**.

The type's attributes should be accessed ONLY through the accessor functions: `STR_info`, `STR_len`, `STR2ptr`, `STR_capa`, etc'.

This is because: Small strings that fit into the type directly use the type itself for memory (except the first and last bytes). Larger strings use the type fields for the string's meta-data. Depending on the string's data, the type behaves differently.

#### `fio_str_info_s`

Some functions return information about a string's state using the `fio_str_info_s` type.

This helper type is defined like so:

```c
typedef struct fio_str_info_s {
  size_t capa; /* String capacity, if the string is writable. */
  size_t len;  /* String length, if any. */
  char *data;  /* Pointer to the string's first byte, if the string is valid . */
} fio_str_info_s;
```

This information type, accessible using the `STR_info` function, allows direct access and manipulation of the string data.

Changes in string length should be followed by a call to `STR_resize`.

The data in the string object is always NUL terminated. However, string data might contain binary data, where NUL is a valid character, so using C string functions isn't advised.

#### String allocation alignment / `FIO_STRING_NO_ALIGN`

Memory allocators have allocation alignment concerns that require minimum space to be allocated.

The default `STR_s` type makes use of this extra space for small strings, fitting them into the type.

To prevent this behavior and minimize the space used by the `STR_s` type, set the `FIO_STRING_NO_ALIGN` macro to `1`.

```c
#define FIO_STRING_NAME big_string
#define FIO_STRING_NO_ALIGN 1
#include <fio-stl.h>
// ...
big_string_s foo = FIO_STRING_INIT;
```

This could save memory when strings aren't short enough to be contained within the type.

This could also save memory, potentially, if the string type will be wrapped / embedded within other data-types (i.e., using `FIO_REF_NAME` for reference counting).

### String API - Initialization and Destruction

#### `FIO_STRING_INIT`

This value should be used for initialization. It should be considered opaque, but is defined as:

```c
#define FIO_STRING_INIT { .special = 1 }
```

For example:

```c
#define FIO_STRING_NAME fio_str
#include <fio-stl.h>
void example(void) {
  // on the stack
  fio_str_s str = FIO_STRING_INIT;
  // .. 
  fio_str_destroy(&str);
}
```

#### `FIO_STRING_INIT_EXISTING`

This macro allows the container to be initialized with existing data.

```c
#define FIO_STRING_INIT_EXISTING(buffer, length, capacity, dealloc_)              \
  {                                                                            \
    .data = (buffer), .len = (length), .capa = (capacity),                     \
    .dealloc = (dealloc_)                                                      \
  }
```
The `capacity` value should exclude the space required for the NUL character (if exists).

`dealloc` should be a function pointer to a memory deallocation function, such as `free`, `fio_free` or `NULL` (doesn't deallocate the memory).

#### `FIO_STRING_INIT_STATIC`

This macro allows the string container to be initialized with existing static data, that shouldn't be freed.

```c
#define FIO_STRING_INIT_STATIC(buffer)                                            \
  { .data = (char *)(buffer), .len = strlen((buffer)), .dealloc = NULL }
```

#### `FIO_STRING_INIT_STATIC2`

This macro allows the string container to be initialized with existing static data, that shouldn't be freed.

```c
#define FIO_STRING_INIT_STATIC2(buffer, length)                                   \
  { .data = (char *)(buffer), .len = (length), .dealloc = NULL }
```

#### `STR_destroy`

```c
void STR_destroy(FIO_STRING_PTR s);
```

Frees the String's resources and reinitializes the container.

Note: if the container isn't allocated on the stack, it should be freed separately using the appropriate `free` function, such as `STR_free`.

#### `STR_new`

```c
FIO_STRING_PTR STR_new(void);
```

Allocates a new String object on the heap.

#### `STR_free`

```c
void STR_free(FIO_STRING_PTR s);
```

Destroys the string and frees the container (if allocated with `STR_new`).

#### `STR_detach`

```c
char * STR_detach(FIO_STRING_PTR s);
```

Returns a C string with the existing data, **re-initializing** the String.

**Note**: the String data is removed from the container, but the container is **not** freed.

Returns NULL if there's no String data.

### String API - String state (data pointers, length, capacity, etc')

#### `STR_info`

```c
fio_str_info_s STR_info(const FIO_STRING_PTR s);
```

Returns the String's complete state (capacity, length and pointer). 

#### `STR_len`

```c
size_t STR_len(FIO_STRING_PTR s);
```

Returns the String's length in bytes.

#### `STR2ptr`

```c
char *STR2ptr(FIO_STRING_PTR s);
```

Returns a pointer (`char *`) to the String's content (first character in the string).

#### `STR_capa`

```c
size_t STR_capa(FIO_STRING_PTR s);
```

Returns the String's existing capacity (total used & available memory).

#### `STR_freeze`

```c
void STR_freeze(FIO_STRING_PTR s);
```

Prevents further manipulations to the String's content.

#### `STR_is_frozen`

```c
uint8_t STR_is_frozen(FIO_STRING_PTR s);
```

Returns true if the string is frozen.

#### `STR_is_eq`

```c
int STR_is_eq(const FIO_STRING_PTR str1, const FIO_STRING_PTR str2);
```

Binary comparison returns `1` if both strings are equal and `0` if not.

#### `STR_hash`

```c
uint64_t STR_hash(const FIO_STRING_PTR s);
```

Returns the string's Risky Hash value.

Note: Hash algorithm might change without notice.

### String API - Memory management

#### `STR_resize`

```c
fio_str_info_s STR_resize(FIO_STRING_PTR s, size_t size);
```

Sets the new String size without reallocating any memory (limited by existing capacity).

Returns the updated state of the String.

Note: When shrinking, any existing data beyond the new size may be corrupted or lost.

#### `STR_compact`

```c
void STR_compact(FIO_STRING_PTR s);
```

Performs a best attempt at minimizing memory consumption.

Actual effects depend on the underlying memory allocator and it's implementation. Not all allocators will free any memory.

#### `STR_reserve`

```c
fio_str_info_s STR_reserve(FIO_STRING_PTR s, size_t amount);
```

Reserves at least `amount` of bytes for the string's data (reserved count includes used data).

Returns the current state of the String.

### String API - UTF-8 State

#### `STR_utf8_valid`

```c
size_t STR_utf8_valid(FIO_STRING_PTR s);
```

Returns 1 if the String is UTF-8 valid and 0 if not.

#### `STR_utf8_len`

```c
size_t STR_utf8_len(FIO_STRING_PTR s);
```

Returns the String's length in UTF-8 characters.

#### `STR_utf8_select`

```c
int STR_utf8_select(FIO_STRING_PTR s, intptr_t *pos, size_t *len);
```

Takes a UTF-8 character selection information (UTF-8 position and length) and updates the same variables so they reference the raw byte slice information.

If the String isn't UTF-8 valid up to the requested selection, than `pos` will be updated to `-1` otherwise values are always positive.

The returned `len` value may be shorter than the original if there wasn't enough data left to accommodate the requested length. When a `len` value of `0` is returned, this means that `pos` marks the end of the String.

Returns -1 on error and 0 on success.

### String API - Content Manipulation and Review

#### `STR_write`

```c
fio_str_info_s STR_write(FIO_STRING_PTR s, const void *src, size_t src_len);
```

Writes data at the end of the String.

#### `STR_write_i`

```c
fio_str_info_s STR_write_i(FIO_STRING_PTR s, int64_t num);
```

Writes a number at the end of the String using normal base 10 notation.

#### `STR_concat` / `STR_join`

```c
fio_str_info_s STR_concat(FIO_STRING_PTR dest, FIO_STRING_PTR const src);
```

Appends the `src` String to the end of the `dest` String. If `dest` is empty, the resulting Strings will be equal.

`STR_join` is an alias for `STR_concat`.


#### `STR_replace`

```c
fio_str_info_s STR_replace(FIO_STRING_PTR s,
                           intptr_t start_pos,
                           size_t old_len,
                           const void *src,
                           size_t src_len);
```

Replaces the data in the String - replacing `old_len` bytes starting at `start_pos`, with the data at `src` (`src_len` bytes long).

Negative `start_pos` values are calculated backwards, `-1` == end of String.

When `old_len` is zero, the function will insert the data at `start_pos`.

If `src_len == 0` than `src` will be ignored and the data marked for replacement will be erased.

#### `STR_vprintf`

```c
fio_str_info_s STR_vprintf(FIO_STRING_PTR s, const char *format, va_list argv);
```

Writes to the String using a vprintf like interface.

Data is written to the end of the String.

#### `STR_printf`

```c
fio_str_info_s STR_printf(FIO_STRING_PTR s, const char *format, ...);
```

Writes to the String using a printf like interface.

Data is written to the end of the String.

#### `STR_readfd`

```c
fio_str_info_s STR_readfd(FIO_STRING_PTR s,
                            int fd,
                            intptr_t start_at,
                            intptr_t limit);
```

Reads data from a file descriptor `fd` at offset `start_at` and pastes it's contents (or a slice of it) at the end of the String. If `limit == 0`, than the data will be read until EOF.

The file should be a regular file or the operation might fail (can't be used for sockets).

Currently implemented only on POSIX systems.

#### `STR_readfile`

```c
fio_str_info_s STR_readfile(FIO_STRING_PTR s,
                            const char *filename,
                            intptr_t start_at,
                            intptr_t limit);
```

Opens the file `filename` and pastes it's contents (or a slice ot it) at the end of the String. If `limit == 0`, than the data will be read until EOF.

If the file can't be located, opened or read, or if `start_at` is beyond the EOF position, NULL is returned in the state's `data` field.

Works on POSIX systems only.

### String API - Base64 support

#### `STR_write_b64enc`

```c
fio_str_info_s STR_write_b64enc(FIO_STRING_PTR s,
                                const void *data,
                                size_t data_len,
                                uint8_t url_encoded);
```

Writes data at the end of the String, encoding the data as Base64 encoded data.

#### `STR_write_b64dec`

```c
fio_str_info_s STR_write_b64dec(FIO_STRING_PTR s,
                                const void *encoded,
                                size_t encoded_len);
```

Writes decoded Base64 data to the end of the String.


### String API - escaping / JSON encoding support

#### `STR_write_escape`

```c
fio_str_info_s STR_write_escape(FIO_STRING_PTR s,
                                const void *data,
                                size_t data_len);

```

Writes data at the end of the String, escaping the data using JSON semantics.

The JSON semantic are common to many programming languages, promising a UTF-8 String while making it easy to read and copy the string during debugging.

#### `STR_write_unescape`

```c
fio_str_info_s STR_write_unescape(FIO_STRING_PTR s,
                                  const void *escaped,
                                  size_t len);
```

Writes an escaped data into the string after unescaping the data.

-------------------------------------------------------------------------------

## Small (non-dynamic) Strings

The "Small" String helpers use a small **footprint** to store a binary safe string that doesn't change over time and can be destroyed.

This type was designed to store string information for Hash Maps where most strings might be very short (less than 16 chars on 64 bit systems or less than 8 chars on 32 bit systems).

This approach minimizes memory allocation and improves locality by copying the string data onto the bytes normally used to store the string pointer and it's length.

To create a small, non-dynamic, string type, define the type name using the `FIO_SMALL_STR_NAME` macro.

The type (`FIO_SMALL_STR_NAME_s`) and the functions will be automatically defined.

```c
#define FIO_SMALL_STR_NAME key
#include "../../facilio/lib/facil/fio-stl.h"

#define FIO_MAP_NAME map
#define FIO_MAP_TYPE uintptr_t
#define FIO_MAP_KEY key_s
#define FIO_MAP_KEY_INVALID (key_s)FIO_SMALL_STR_INIT
#define FIO_MAP_KEY_DESTROY(k) key_destroy(&k)
/* destroy discarded keys when overwriting existing data (duplicate keys aren't copied): */
#define FIO_MAP_KEY_DISCARD(k) key_destroy(&k)
#include "../../facilio/lib/facil/fio-stl.h"

void example(void) {
  map_s m = FIO_MAP_INIT;
  for (int overwrite = 0; overwrite < 2; ++overwrite) {
    for (int i = 0; i < 10; ++i) {
      char buf[128] = "a long key will require memory allocation: ";
      size_t len = fio_ltoa(buf + 43, i, 16) + 43;
      key_s k;
      key_set_copy(&k, buf, len);
      map_set(&m, key_hash(&k, (uint64_t)&m), k, (uintptr_t)i, NULL);
    }
  }
  for (int i = 0; i < 10; ++i) {
    char buf[128] = "embed: "; /* short keys fit in pointer + length type */
    size_t len = fio_ltoa(buf + 7, i, 16) + 7;
    key_s k;
    key_set_copy(&k, buf, len);
    map_set(&m, key_hash(&k, (uint64_t)&m), k, (uintptr_t)i, NULL);
  }
  FIO_MAP_EACH(&m, pos) {
    fprintf(stderr, "[%d] %s - memory allocated: %s\n", (int)pos->obj.value,
            key2ptr(&pos->obj.key),
            (key_is_allocated(&pos->obj.key) ? "yes" : "no"));
  }
  map_destroy(&m);
}
```

### Small String Initialization

The small string object fits perfectly in one `char * ` pointer and one `size_t` value, meaning it takes as much memory as storing a string's location and length.

However, the type information isn't stored as simply as one might imagine, allowing short strings to be stored within the object itself, improving locality.

small strings aren't dynamically allocated and their initialization is performed using the `FIO_SMALL_STR_INIT` macro (for empty strings) or the `SMALL_STR_set_const` and `SMALL_STR_set_copy` functions (see example above).

### Small String API

#### `SMALL_STR_set_const`

```c
void SMALL_STR_set_const(FIO_SMALL_STR_PTR s, const char *str, size_t len);
```

Initializes the container with the provided static / constant string.

The string will be copied to the container **only** if it will fit in the
container itself. Otherwise, the supplied pointer will be used as is and it
should remain valid until the string is destroyed.

#### `SMALL_STR_set_copy`

```c
void SMALL_STR_set_copy(FIO_SMALL_STR_PTR s, const char *str, size_t len);
```

Initializes the container with the provided dynamic string.

The string is always copied and the final string must be destroyed (using the
`destroy` function).

#### `SMALL_STR_destroy`

```c
void SMALL_STR_destroy(FIO_SMALL_STR_PTR s);
```

Frees the String's resources and reinitializes the container.

Note: if the container isn't allocated on the stack, it should be freed
separately using the appropriate `free` function.

#### `SMALL_STR_info`

```c
fio_str_info_s SMALL_STR_info(const FIO_SMALL_STR_PTR s);
```

Returns information regarding the embedded string.

#### `SMALL_STR2ptr`

```c
const char *SMALL_STR2ptr(const FIO_SMALL_STR_PTR s);
```

Returns a pointer (`char *`) to the String's content.

#### `SMALL_STR_len`

```c
size_t SMALL_STR_len(const FIO_SMALL_STR_PTR s);
```

Returns the String's length in bytes.

#### `SMALL_STR_allocated`

```c
int SMALL_STR_allocated(const FIO_SMALL_STR_PTR s);
```

Returns 1 if memory was allocated and (the String must be destroyed).

#### `SMALL_STR_hash`

```c
uint64_t SMALL_STR_hash(const FIO_SMALL_STR_PTR s, uint64_t seed);
```

Returns the String's Risky Hash.

#### `SMALL_STR_eq`

```c
int SMALL_STR_eq(const FIO_SMALL_STR_PTR str1, const FIO_SMALL_STR_PTR str2);
```

Binary comparison returns `1` if both strings are equal and `0` if not.

-------------------------------------------------------------------------------

## Reference Counting / Type Wrapping

If the `FIO_REF_NAME` macro is defined, then reference counting helpers can be
defined for any named type.

By default, `FIO_REF_TYPE` will equal `FIO_REF_NAME_s`, using the naming
convention in this library.

In addition, the `FIO_REF_METADATA` macro can be defined with any type, allowing
metadata to be attached and accessed using the helper function
`FIO_REF_metadata(object)`.

If the `FIO_REF_CONSTRUCTOR_ONLY` macro is defined, the reference counter constructor (`TYPE_new`) will be the only constructor function.  When set, the reference counting functions will use `X_new` and `X_free`. Otherwise (assuming `X_new` and `X_free` are already defined), the reference counter will define `X_new2` and `X_free2` instead.

Note: requires the atomic operations to be defined (`FIO_ATOMIC`).

Reference counting adds the following functions:

#### `REF_new` / `REF_new2`

```c
FIO_REF_TYPE * REF_new2(void)
// or, if FIO_REF_CONSTRUCTOR_ONLY is defined
FIO_REF_TYPE * REF_new(void)
```

Allocates a new reference counted object, initializing it using the
`FIO_REF_INIT(object)` macro.

If `FIO_REF_METADATA` is defined, than the metadata is initialized using the
`FIO_REF_METADATA_INIT(metadata)` macro.

#### `REF_up_ref`

```c
FIO_REF_TYPE * REF_up_ref(FIO_REF_TYPE * object)
```

Increases an object's reference count (an atomic operation, thread-safe).

#### `REF_free` / `REF_free2`

```c
void REF_free2(FIO_REF_TYPE * object)
// or, if FIO_REF_CONSTRUCTOR_ONLY is defined
void REF_free(FIO_REF_TYPE * object)
```

Frees an object or decreases it's reference count (an atomic operation,
thread-safe).

Before the object is freed, the `FIO_REF_DESTROY(object)` macro will be called.

If `FIO_REF_METADATA` is defined, than the metadata is also destroyed using the
`FIO_REF_METADATA_DESTROY(metadata)` macro.


#### `REF_metadata`

```c
FIO_REF_METADATA * REF_metadata(FIO_REF_TYPE * object)
```

If `FIO_REF_METADATA` is defined, than the metadata is accessible using this
inlined function.

-------------------------------------------------------------------------------

## Pointer Tagging Support:

Pointer tagging allows types created using this library to have their pointers "tagged".

This is when creating / managing dynamic types, where some type data could be written to the pointer data itself.

**Note**: pointer tagging can't automatically tag "pointers" to objects placed on the stack.

#### `FIO_PTR_TAG`

Supports embedded pointer tagging / untagging for the included types.

Should resolve to a tagged pointer value. i.e.: `((uintptr_t)(p) | 1)`

#### `FIO_PTR_UNTAG`

Supports embedded pointer tagging / untagging for the included types.

Should resolve to an untagged pointer value. i.e.: `((uintptr_t)(p) | ~1UL)`

**Note**: `FIO_PTR_UNTAG` might be called more then once or on untagged pointers. For this reason, `FIO_PTR_UNTAG` should always return the valid pointer, even if called on an untagged pointer.

#### `FIO_PTR_TAG_TYPE`

If the FIO_PTR_TAG_TYPE is defined, then functions returning a type's pointer will return a pointer of the specified type instead.

-------------------------------------------------------------------------------

## Logging and Assertions:

If the `FIO_LOG_LENGTH_LIMIT` macro is defined (it's recommended that it be
greater than 128), than the `FIO_LOG2STDERR` (weak) function and the
`FIO_LOG2STDERR2` macro will be defined.

#### `FIO_LOG_LEVEL`

An application wide integer with a value of either:

- `FIO_LOG_LEVEL_NONE` (0)
- `FIO_LOG_LEVEL_FATAL` (1)
- `FIO_LOG_LEVEL_ERROR` (2)
- `FIO_LOG_LEVEL_WARNING` (3)
- `FIO_LOG_LEVEL_INFO` (4)
- `FIO_LOG_LEVEL_DEBUG` (5)

The initial value can be set using the `FIO_LOG_LEVEL_DEFAULT` macro. By
default, the level is 4 (`FIO_LOG_LEVEL_INFO`) for normal compilation and 5
(`FIO_LOG_LEVEL_DEBUG`) for DEBUG compilation.

#### `FIO_LOG2STDERR(msg, ...)`

This `printf` style **function** will log a message to `stderr`, without
allocating any memory on the heap for the string (`fprintf` might).

The function is defined as `weak`, allowing it to be overridden during the
linking stage, so logging could be diverted... although, it's recommended to
divert `stderr` rather then the logging function.

#### `FIO_LOG2STDERR2(msg, ...)`

This macro routs to the `FIO_LOG2STDERR` function after prefixing the message
with the file name and line number in which the error occurred.

#### `FIO_LOG_DEBUG(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level.

#### `FIO_LOG_INFO(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level.

#### `FIO_LOG_WARNING(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level.

#### `FIO_LOG_ERROR(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level.

#### `FIO_LOG_FATAL(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level.

#### `FIO_ASSERT(cond, msg, ...)`

Reports an error unless condition is met, printing out `msg` using
`FIO_LOG_FATAL` and exiting (not aborting) the application.

In addition, a `SIGINT` will be sent to the process and any of it's children
before exiting the application, supporting debuggers everywhere :-)

#### `FIO_ASSERT_ALLOC(cond, msg, ...)`

Reports an error unless condition is met, printing out `msg` using
`FIO_LOG_FATAL` and exiting (not aborting) the application.

In addition, a `SIGINT` will be sent to the process and any of it's children
before exiting the application, supporting debuggers everywhere :-)

#### `FIO_ASSERT_DEBUG(cond, msg, ...)`

Reports an error unless condition is met, printing out `msg` using
`FIO_LOG_FATAL` and aborting (not exiting) the application.

In addition, a `SIGINT` will be sent to the process and any of it's children
before aborting the application, because consistency is important.

**Note**: `msg` MUST be a string literal.

-------------------------------------------------------------------------------

## Atomic operations:

If the `FIO_ATOMIC` macro is defined than the following macros will be defined:

#### `fio_atomic_xchange(p_obj, value)`

Atomically sets the object pointer to by `p_obj` to `value`, returning the
previous value.

#### `fio_atomic_add(p_obj, value)`
#### `fio_atomic_sub(p_obj, value)`
#### `fio_atomic_and(p_obj, value)`
#### `fio_atomic_xor(p_obj, value)`
#### `fio_atomic_or(p_obj, value)`
#### `fio_atomic_nand(p_obj, value)`

Atomically operates on the object pointer to by `p_obj`, returning the new
value.

#### `fio_lock_i`

A spinlock type based on a volatile unsigned char.

#### `fio_lock(fio_lock_i *)`

Busy waits for a lock to become available.

#### `fio_trylock(fio_lock_i *)`

Returns 0 on success and 1 on failure.

#### `fio_unlock(fio_lock_i *)`

Unlocks the lock, no matter which thread owns the lock.

-------------------------------------------------------------------------------

## Bit-Byte operations:

If the `FIO_BITWISE` macro is defined than the following macros will be
defined:

#### Byte Swapping

- `fio_bswap16(i)`
- `fio_bswap32(i)`
- `fio_bswap64(i)`

#### Bit rotation (left / right)

- `fio_lrot32(i, bits)`
- `fio_rrot32(i, bits)`
- `fio_lrot64(i, bits)`
- `fio_rrot64(i, bits)`
- `FIO_LROT(i, bits)`
- `FIO_RROT(i, bits)`

#### Numbers to Numbers (network ordered)

- `fio_lton16(i)`
- `fio_ntol16(i)`
- `fio_lton32(i)`
- `fio_ntol32(i)`
- `fio_lton64(i)`
- `fio_ntol64(i)`

On big-endian systems, these macros a NOOPs, whereas on little-endian systems
these macros flip the byte order.

#### Bytes to Numbers (native / reversed / network ordered)

Big Endian (default):

- `fio_buf2u16(buffer)`
- `fio_buf2u32(buffer)`
- `fio_buf2u64(buffer)`

Little Endian:

- `fio_buf2u16_little(buffer)`
- `fio_buf2u32_little(buffer)`
- `fio_buf2u64_little(buffer)`

Native Byte Order:

- `fio_buf2u16_local(buffer)`
- `fio_buf2u32_local(buffer)`
- `fio_buf2u64_local(buffer)`

Reversed Byte Order:

- `fio_buf2u16_bswap(buffer)`
- `fio_buf2u32_bswap(buffer)`
- `fio_buf2u64_bswap(buffer)`

#### Numbers to Bytes (native / reversed / network ordered)

Big Endian (default):

- `fio_u2buf16(buffer, i)`
- `fio_u2buf32(buffer, i)`
- `fio_u2buf64(buffer, i)`

Little Endian:

- `fio_u2buf16_little(buffer, i)`
- `fio_u2buf32_little(buffer, i)`
- `fio_u2buf64_little(buffer, i)`

Native Byte Order:

- `fio_u2buf16_local(buffer, i)`
- `fio_u2buf32_local(buffer, i)`
- `fio_u2buf64_local(buffer, i)`

Reversed Byte Order:

- `fio_u2buf16_bswap(buffer, i)`
- `fio_u2buf32_bswap(buffer, i)`
- `fio_u2buf64_bswap(buffer, i)`

#### Constant Time Bit Operations

- `fio_ct_true(condition)`
- `fio_ct_false(condition)`
- `fio_ct_if(bool, a_if_true, b_if_false)`
- `fio_ct_if2(condition, a_if_true, b_if_false)`

-------------------------------------------------------------------------------

## Bitmap helpers

If the `FIO_BITMAP` macro is defined than the following macros will be
defined.

In addition, the `FIO_ATOMIC` will be assumed to be defined, as setting bits in
the bitmap is implemented using atomic operations.

#### Bitmap helpers
- `fio_bitmap_get(void *map, size_t bit)`
- `fio_bitmap_set(void *map, size_t bit)`   (an atomic operation, thread-safe)
- `fio_bitmap_unset(void *map, size_t bit)` (an atomic operation, thread-safe)

-------------------------------------------------------------------------------

## Risky Hash (data hashing):

If the `FIO_RISKY_HASH` macro is defined than the following static function will
be defined:

#### `fio_risky_hash`

```c
uint64_t fio_risky_hash(const void *data, size_t len, uint64_t seed)
```

This is a non-streaming implementation of the RiskyHash algorithm.

This function will produce a 64 bit hash for X bytes of data.

#### `fio_risky_mask`

```c
void fio_risky_mask(char *buf, size_t len, uint64_t key, uint64_t nonce);
```

Masks data using a Risky Hash and a counter mode nonce.

Used for mitigating memory access attacks when storing "secret" information in memory.

Keep the nonce information in a different memory address then the secret. For example, if the secret is on the stack, store the nonce on the heap or using a static variable.

Don't use the same nonce-secret combination for other data.

This is **not** a cryptographically secure encryption. Even **if** the algorithm was secure, it would provide no more then a 32 bit level encryption, which isn't strong enough for any cryptographic use-case.

However, this could be used to mitigate memory probing attacks. Secrets stored in the memory might remain accessible after the program exists or through core dump information. By storing "secret" information masked in this way, it mitigates the risk of secret information being recognized or deciphered.


-------------------------------------------------------------------------------

## Pseudo Random Generation

If the `FIO_RAND` macro is defined, the following, non-cryptographic
psedo-random generator functions will be defined.

The random generator functions are automatically re-seeded with either data from
the system's clock or data from `getrusage` (when available).

The facil.io random generator functions are both faster and more random then the standard `rand` on my computer (you can test it for yours).

I designed it in the hopes of achieving a cryptographically safe PRNG, but it wasn't cryptographically analyzed and should be considered as a non-cryptographic PRNG.

**Note**: bitwise operations (`FIO_BITWISE`) and Risky Hash (`FIO_RISKY_HASH`)
are automatically defined along with `FIO_RAND`, since they are required by the
algorithm.

#### `fio_rand64`

```c
uint64_t fio_rand64(void)
```

Returns 64 random bits. Probably **not** cryptographically safe.

#### `fio_rand_bytes`

```c
void fio_rand_bytes(void *data_, size_t len)
```

Writes `len` random Bytes to the buffer pointed to by `data`. Probably **not**
cryptographically safe.

#### `fio_rand_feed2seed`

```c
static void fio_rand_feed2seed(void *buf_, size_t len);
```

An internal function (accessible from the translation unit) that allows a program to feed random data to the PRNG (`fio_rand64`).

The random data will effect the random seed on the next reseeding.

Limited to 1023 bytes of data per function call.

#### `fio_rand_reseed`

```c
void fio_rand_reseed(void);
```

Forces the random generator state to rotate. SHOULD be called after `fork` to prevent the two processes from outputting the same random numbers (until a reseed is called automatically).

-------------------------------------------------------------------------------

## String / Number conversion

If the `FIO_ATOL` macro is defined, the following functions will be defined:

#### `SFUNC int64_t fio_atol(char **pstr)`

A helper function that converts between String data to a signed int64_t.

Numbers are assumed to be in base 10. Octal (`0###`), Hex (`0x##`/`x##`) and
binary (`0b##`/ `b##`) are recognized as well. For binary Most Significant Bit
must come first.

The most significant difference between this function and `strtol` (aside of API
design), is the added support for binary representations.

#### `SFUNC double fio_atof(char **pstr)`

A helper function that converts between String data to a signed double.

#### `SFUNC size_t fio_ltoa(char *dest, int64_t num, uint8_t base)`

A helper function that writes a signed int64_t to a string.

No overflow guard is provided, make sure there's at least 68 bytes available
(for base 2).

Offers special support for base 2 (binary), base 8 (octal), base 10 and base 16
(hex). An unsupported base will silently default to base 10. Prefixes are
automatically added (i.e., "0x" for hex and "0b" for base 2).

Returns the number of bytes actually written (excluding the NUL terminator).


#### `size_t fio_ftoa(char *dest, double num, uint8_t base)`

A helper function that converts between a double to a string.

No overflow guard is provided, make sure there's at least 130 bytes available
(for base 2).

Supports base 2, base 10 and base 16. An unsupported base will silently default
to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the beginning of the
string).

Returns the number of bytes actually written (excluding the NUL terminator).


-------------------------------------------------------------------------------

## URL (URI) parsing

URIs (Universal Resource Identifier), commonly referred to as URL (Uniform Resource Locator), are a common way to describe network and file addresses.

A common use case for URIs is within the command line interface (CLI), allowing a client to point at a resource that may be local (i.e., `file:///users/etc/my.conf`) or remote (i.e. `http://example.com/conf`).

By defining `FIO_URL`, the following types and functions will be defined:

#### `fio_url_s`

```c
/** the result returned by `fio_url_parse` */
typedef struct {
  fio_str_info_s scheme;
  fio_str_info_s user;
  fio_str_info_s password;
  fio_str_info_s host;
  fio_str_info_s port;
  fio_str_info_s path;
  fio_str_info_s query;
  fio_str_info_s target;
} fio_url_s;
```

The `fio_url_s` contains a information about a URL (or, URI).

When the information is returned from `fio_url_parse`, the strings in the `fio_url_s` (i.e., `url.scheme.buf`) are **not NUL terminated**, since the parser is non-destructive, with zero-copy and zero-allocation.

#### `fio_url_parse`

```c
fio_url_s fio_url_parse(const char *url, size_t len);
```

Parses the URI returning it's components and their lengths (no decoding performed, **doesn't accept decoded URIs**).

The returned string are **not NUL terminated**, they are merely locations within the original (unmodified) string.

This function attempts to accept many different formats, including any of the following:

* `/complete_path?query#target`

  i.e.: `/index.html?page=1#list`

* `host:port/complete_path?query#target`

  i.e.:
  - `example.com`
  - `example.com:8080`
  - `example.com/index.html`
  - `example.com:8080/index.html`
  - `example.com:8080/index.html?key=val#target`

* `user:password@host:port/path?query#target`

  i.e.: `user:1234@example.com:8080/index.html`

* `username[:password]@host[:port][...]`

  i.e.: `john:1234@example.com`

* `schema://user:password@host:port/path?query#target`

  i.e.: `http://example.com/index.html?page=1#list`

Invalid formats might produce unexpected results. No error testing performed.

The `file` and `unix` schemas are special in the sense that they produce no `host` (only `path`).

-------------------------------------------------------------------------------

## CLI (command line interface)

The simple template library includes a CLI parser, since parsing command line
arguments is a common task.

By defining `FIO_CLI`, the following functions will be defined.

In addition, `FIO_CLI` automatically includes the `FIO_ATOL` flag, since CLI
parsing depends on the `fio_atol` function.

#### `fio_cli_start(argc, argv, unnamed_min, unnamed_max, description, ...)`

The **macro** shadows the `fio_cli_start` function and defines the CLI interface
to be parsed. i.e.,

      int main(int argc, char const *argv[]) {
        fio_cli_start(argc, argv, 0, -1,
                      "this is a CLI example.",
                      FIO_CLI_PRINT_HEADER("CLI type validation"),
                      FIO_CLI_STRING("-str -s any data goes here"),
                      FIO_CLI_INT("-int -i numeral data goes here"),
                      FIO_CLI_BOOL("-bool -b flag (boolean) only - no data"),
                      FIO_CLI_PRINT("This test allows for unlimited arguments "
                                    "that will simply pass-through"));
        if (fio_cli_get("-s"))
          fprintf(stderr, "String: %s\n", fio_cli_get("-s"));

        if (fio_cli_get("-i"))
          fprintf(stderr, "Integer: %d\n", fio_cli_get_i("-i"));

        fprintf(stderr, "Boolean: %d\n", fio_cli_get_i("-b"));

        if (fio_cli_unnamed_count()) {
          fprintf(stderr, "Printing unlisted / unrecognized arguments:\n");
          for (size_t i = 0; i < fio_cli_unnamed_count(); ++i) {
            fprintf(stderr, "%s\n", fio_cli_unnamed(i));
          }
        }

        fio_cli_end();
        return 0;
      }

The `fio_cli_start` macro accepts the `argc` and `argv`, as received by the
`main` functions, a maximum and minimum number of unspecified CLI arguments
(beneath which or after which the parser will fail), an application description
string and a variable list of (specified) command line arguments.

Command line arguments can be either String, Integer or Boolean, as indicated by
the `FIO_CLI_STRING("-arg [-alias] desc.")`, `FIO_CLI_INT("-arg [-alias]
desc.")` and `FIO_CLI_BOOL("-arg [-alias] desc.")` macros. Extra descriptions or
text can be added using the `FIO_CLI_PRINT_HEADER(str)` and `FIO_CLI_PRINT(str)`
macros.

#### `fio_cli_end()`

Clears the CLI data storage.

#### `char const *fio_cli_get(char const *name);`

Returns the argument's value as a string, or NULL if the argument wasn't
provided.

#### `int fio_cli_get_i(char const *name);`

Returns the argument's value as an integer, or 0 if the argument wasn't
provided.

#### `fio_cli_get_bool(name)`

True the argument was boolean and provided.

#### `unsigned int fio_cli_unnamed_count(void)`

Returns the number of unrecognized arguments (arguments unspecified, in
`fio_cli_start`).

#### `char const *fio_cli_unnamed(unsigned int index)`

Returns a String containing the unrecognized argument at the stated `index`
(indexes are zero based).

#### `void fio_cli_set(char const *name, char const *value)`

Sets a value for the named argument (but **not** it's aliases).

#### `fio_cli_set_default(name, value)`

Sets a value for the named argument (but **not** it's aliases) **only if** the
argument wasn't set by the user.

-------------------------------------------------------------------------------

## Memory Allocation

The simple template library includes a fast, concurrent, memory allocator
designed for shot-medium object life-spans.

It's ideal if all long-term allocations are performed during the start-up phase
or using a different memory allocator.

By defining `FIO_MALLOC`, the following functions will be defined.

#### `void * fio_malloc(size_t size)`

Allocates memory using a per-CPU core block memory pool. Memory is zeroed out.

Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT (16Kb when using 32Kb blocks)
will be redirected to `mmap`, as if `fio_mmap` was called.

#### `void * fio_calloc(size_t size_per_unit, size_t unit_count)`

Same as calling `fio_malloc(size_per_unit * unit_count)`;

Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT (16Kb when using 32Kb blocks)
will be redirected to `mmap`, as if `fio_mmap` was called.

#### `void fio_free(void *ptr)`

Frees memory that was allocated using this library.

#### `void * fio_realloc(void *ptr, size_t new_size)`

Re-allocates memory. An attempt to avoid copying the data is made only for big
memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).

#### `void * fio_realloc2(void *ptr, size_t new_size, size_t copy_length)`

Re-allocates memory. An attempt to avoid copying the data is made only for big
memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).

This variation is slightly faster as it might copy less data.

#### `void * fio_mmap(size_t size)`

Allocates memory directly using `mmap`, this is preferred for objects that both
require almost a page of memory (or more) and expect a long lifetime.

However, since this allocation will invoke the system call (`mmap`), it will be
inherently slower.

`fio_free` can be used for deallocating the memory.

#### `void fio_malloc_after_fork(void)`

Never fork a multi-threaded process. Doing so might corrupt the memory
allocation system. The risk is more relevant for child processes.

However, if a multi-threaded process, calling this function from the child
process would perform a best attempt at mitigating any arising issues (at the
expense of possible leaks).

#### `FIO_MALLOC_FORCE_SYSTEM`

If `FIO_MALLOC_FORCE_SYSTEM` is defined, the facil.io memory allocator functions will simply pass requests through to the system's memory allocator (`calloc` / `free`) rather then use the facil.io custom allocator.

#### `FIO_MALLOC_OVERRIDE_SYSTEM`

If `FIO_MALLOC_OVERRIDE_SYSTEM` is defined, the facil.io memory allocator will replace the system's memory allocator.

#### `FIO_MEMORY_ARENA_COUNT_MAX`

Sets the maximum number of memory arenas to initialize. Defaults to 64.

When set to `0` the number of arenas will always match the maximum number of detected CPU cores.

#### `FIO_MEMORY_ARENA_COUNT_DEFAULT`

The default number of memory arenas to initialize when CPU core detection fails or isn't available. Defaults to `5`.

Normally, facil.io tries to initialize as many memory allocation arenas as the number of CPU cores. This value will only be used if core detection isn't available or fails.

-------------------------------------------------------------------------------

## Basic Socket / IO Helpers

The facil.io standard library provides a few simple IO / Sockets helpers for POSIX systems.

By defining `FIO_SOCK` on a POSIX system, the following functions will be defined.

#### `fio_sock_open`

```c
int fio_sock_open(const char *restrict address,
                 const char *restrict port,
                 uint16_t flags);
```

Creates a new socket according to the provided flags.

The `port` string will be ignored when `FIO_SOCK_UNIX` is set.

The `address` can be NULL for Server sockets (`FIO_SOCK_SERVER`) when binding to all available interfaces (this is actually recommended unless network filtering is desired).

The `flag` integer can be a combination of any of the following flags:

*  `FIO_SOCK_TCP` - Creates a TCP/IP socket.

*  `FIO_SOCK_UDP` - Creates a UDP socket.

*  `FIO_SOCK_UNIX ` - Creates a Unix socket. If an existing file / Unix socket exists, they will be deleted and replaced.

*  `FIO_SOCK_SERVER` - Initializes a Server socket. For TCP/IP and Unix sockets, the new socket will be listening for incoming connections (`listen` will be automatically called).

*  `FIO_SOCK_CLIENT` - Initializes a Client socket, calling `connect` using the `address` and `port` arguments.

*  `FIO_SOCK_NONBLOCK` - Sets the new socket to non-blocking mode.

If neither `FIO_SOCK_SERVER` nor `FIO_SOCK_CLIENT` are specified, the function will default to a server socket.

#### `fio_sock_poll`

```c
typedef struct {
  void (*on_ready)(int fd, void *udata);
  void (*on_data)(int fd, void *udata);
  void (*on_error)(int fd, void *udata);
  void *udata;
  int timeout;
  struct pollfd *fds;
} fio_sock_poll_args;

int fio_sock_poll(fio_sock_poll_args args);
#define fio_sock_poll(...) fio_sock_poll((fio_sock_poll_args){__VA_ARGS__})
```

The `fio_sock_poll` function is shadowed by the `fio_sock_poll` MACRO, which allows the function to accept the following "named arguments":

* `on_ready`:

    This callback will be called if a socket can be written to and the socket is polled for the **W**rite event.

        // callback example:
        void on_ready(int fd, void *udata);

* `on_data`:

    This callback will be called if data is available to be read from a socket and the socket is polled for the **R**ead event.

        // callback example:
        void on_data(int fd, void *udata);

* `on_error`:

    This callback will be called if an error occurred when polling the file descriptor.

        // callback example:
        void on_error(int fd, void *udata);

* `timeout`:

    Polling timeout in milliseconds.

        // type:
        int timeout;

* `udata`:

    Opaque user data.

        // type:
        void *udata;

* `fds`:

    A list of `struct pollfd` with file descriptors to be polled. The list MUST end with a `struct pollfd` containing an empty `events` field (and no empty `events` field should appear in the middle of the list).

    Use the `FIO_SOCK_POLL_LIST(...)`, `FIO_SOCK_POLL_RW(fd)`, `FIO_SOCK_POLL_R(fd)` and `FIO_SOCK_POLL_W(fd)` macros to build the list.


The `fio_sock_poll` function uses the `poll` system call to poll a simple IO list.

The list must end with a `struct pollfd` with it's `events` set to zero. No other member of the list should have their `events` data set to zero.

It is recommended to use the `FIO_SOCK_POLL_LIST(...)` and
`FIO_SOCK_POLL_[RW](fd)` macros. i.e.:

```c
int io_fd = fio_sock_open(NULL, "8888", FIO_SOCK_UDP | FIO_SOCK_NONBLOCK | FIO_SOCK_SERVER);
int count = fio_sock_poll(.on_ready = on_ready,
                    .on_data = on_data,
                    .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(io_fd)));
```

**Note**: The `poll` system call should perform reasonably well for light loads (short lists). However, for complex IO needs or heavier loads, use the system's native IO API, such as `kqueue` or `epoll`.


#### `fio_sock_address_new`

```c
struct addrinfo *fio_sock_address_new(const char *restrict address,
                                      const char *restrict port,
                                      int sock_type);
```

Attempts to resolve an address to a valid IP6 / IP4 address pointer.

The `sock_type` element should be a socket type, such as `SOCK_DGRAM` (UDP) or `SOCK_STREAM` (TCP/IP).

The address should be freed using `fio_sock_address_free`.

#### `fio_sock_address_free`

```c
void fio_sock_address_free(struct addrinfo *a);
```

Frees the pointer returned by `fio_sock_address_new`.

#### `fio_sock_set_non_block`

```c
int fio_sock_set_non_block(int fd);
```

Sets a file descriptor / socket to non blocking state.

#### `fio_sock_open_local`

```c
int fio_sock_open_local(struct addrinfo *addr);
```

Creates a new network socket and binds it to a local address.

#### `fio_sock_open_remote`

```c
int fio_sock_open_remote(struct addrinfo *addr, int nonblock);
```

Creates a new network socket and connects it to a remote address.

#### `fio_sock_open_unix`

```c
int fio_sock_open_unix(const char *address, int is_client, int nonblock);
```

Creates a new Unix socket and binds it to a local address.

-------------------------------------------------------------------------------

## Custom JSON Parser

The facil.io JSON parser is a non-strict parser, with support for trailing commas in collections, new-lines in strings, extended escape characters, comments, and octal, hex and binary numbers.

The parser allows for streaming data and decouples the parsing process from the resulting data-structure by calling static callbacks for JSON related events.

To use the JSON parser, define `FIO_JSON` before including the `fio-slt.h` file and later define the static callbacks required by the parser (see list of callbacks).

**Note**: the JSON parser and the FIOBJ soft types can't be implemented in the same translation unit, since the FIOBJ soft types already define JSON callbacks and use the JSON parser to provide JSON support.

#### `JSON_MAX_DEPTH`

```c
#ifndef JSON_MAX_DEPTH
/** Maximum allowed JSON nesting level. Values above 64K might fail. */
#define JSON_MAX_DEPTH 512
#endif
```
The JSON parser isn't recursive, but it allocates a nesting bitmap on the stack, which consumes stack memory.

To ensure the stack isn't abused, the parser will limit JSON nesting levels to a customizable `JSON_MAX_DEPTH` number of nesting levels.

#### `fio_json_parser_s`

```c
typedef struct {
  /** level of nesting. */
  uint32_t depth;
  /** expectation bit flag: 0=key, 1=colon, 2=value, 4=comma/closure . */
  uint8_t expect;
  /** nesting bit flags - dictionary bit = 0, array bit = 1. */
  uint8_t nesting[(JSON_MAX_DEPTH + 7) >> 3];
} fio_json_parser_s;
```

The JSON parser type. Memory must be initialized to 0 before first uses (see `FIO_JSON_INIT`).

The type should be considered opaque. To add user data to the parser, use C-style inheritance and pointer arithmetics or simple type casting.

i.e.:

```c
typedef struct {
  fio_json_parser_s private;
  int my_data;
} my_json_parser_s;
// void use_in_callback (fio_json_parser_s * p) {
//    my_json_parser_s *my = (my_json_parser_s *)p;
// }
```

#### `FIO_JSON_INIT`

```c
#define FIO_JSON_INIT                                                          \
  { .depth = 0 }
```

A convenient macro that could be used to initialize the parser's memory to 0.

### JSON parser API
 
#### `fio_json_parse`

```c
size_t fio_json_parse(fio_json_parser_s *parser,
                      const char *buffer,
                      const size_t len);
```

Returns the number of bytes consumed before parsing stopped (due to either error or end of data). Stops as close as possible to the end of the buffer or once an object parsing was completed.

Zero (0) is a valid number and may indicate that the buffer's memory contains a partial object that can't be fully parsed just yet.

**Note!**: partial Numeral objects may be result in errors, as the number 1234 may be fragmented as 12 and 34 when streaming data. facil.io doesn't protect against this possible error.

### JSON Required Callbacks

The JSON parser requires the following callbacks to be defined as static functions.

#### `fio_json_on_null`

```c
static void fio_json_on_null(fio_json_parser_s *p);
```

A NULL object was detected

#### `fio_json_on_true`

```c
static void fio_json_on_true(fio_json_parser_s *p);
```

A TRUE object was detected

#### `fio_json_on_false`

```c
static void fio_json_on_false(fio_json_parser_s *p);
```

A FALSE object was detected

#### `fio_json_on_number`

```c
static void fio_json_on_number(fio_json_parser_s *p, long long i);
```

A Number was detected (long long).

#### `fio_json_on_float`

```c
static void fio_json_on_float(fio_json_parser_s *p, double f);
```

A Float was detected (double).

#### `fio_json_on_string`

```c
static void fio_json_on_string(fio_json_parser_s *p, const void *start, size_t len);
```

A String was detected (int / float). update `pos` to point at ending


#### `fio_json_on_start_object`

```c
static int fio_json_on_start_object(fio_json_parser_s *p);
```

A dictionary object was detected, should return 0 unless error occurred.

#### `fio_json_on_end_object`

```c
static void fio_json_on_end_object(fio_json_parser_s *p);
```

A dictionary object closure detected

#### `fio_json_on_start_array`

```c
static int fio_json_on_start_array(fio_json_parser_s *p);
```
An array object was detected, should return 0 unless error occurred.

#### `fio_json_on_end_array`

```c
static void fio_json_on_end_array(fio_json_parser_s *p);
```

An array closure was detected

#### `fio_json_on_json`

```c
static void fio_json_on_json(fio_json_parser_s *p);
```

The JSON parsing is complete (JSON data parsed so far contains a valid JSON object).

#### `void`

```c
static void fio_json_on_error(fio_json_parser_s *p);
```

The JSON parsing should stop with an error.

