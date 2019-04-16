---
title: facil.io - 0.8.x C STL - a Simple Template Library for C
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

At the core of the facil.io library is a single header Simple Template Library for C.

The header could be included multiple times with different results, creating different types or helpers functions.

This makes it east to perform common tasks for C projects - such as creating hash maps, dynamic arrays, linked lists etc'.

## Lower Level API Notice

>> **The core library is probably not the API most developers need to focus on** (although it's good to know and can be helpful).
>>
>> This API is used to power the higher level API offered by the [HTTP / WebSockts extension](./http) and the [dynamic FIOBJ types](./fiobj).

## Simple Template Library (STL) Overview

The core library is a single file library (`fio-stl.h`).

The core library includes a Simple Template Library for common types, such as:

* [Linked Lists](#linked-lists) - defined by `FIO_LIST_NAME`

* [Dynamic Arrays](#dynamic-arrays) - defined by `FIO_ARY_NAME`

* Hash Maps / Sets - defined by `FIO_MAP_NAME`

* Binary Safe Dynamic Strings - defined by `FIO_STR_NAME`

* Reference counting / Type wrapper - defined by `FIO_REF_NAME`

In addition, the core library includes helpers for common tasks, such as:

* Logging and Assertion (without heap allocation) - defined by `FIO_LOG`

* Atomic operations - defined by `FIO_ATOMIC`

* Bit-Byte Operations - defined by `FIO_BITWISE` and `FIO_BITMAP`

* Network byte ordering macros - defined by `FIO_NTOL`

* Data Hashing (using Risky Hash) - defined by `FIO_RISKY_HASH`

* Pseudo Random Generation - defined by `FIO_RAND`

* String / Number conversion - defined by `FIO_ATOL`

* Command Line Interface helpers - defined by `FIO_CLI`

* Custom Memory Allocation - defined by `FIO_MALLOC`

* Pointer Tagging - defined by `FIO_PTR_TAG(p)`/`FIO_PTR_UNTAG(p)`

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


#### `LIST_init`

```c
void LIST_init(FIO_LIST_HEAD *head)
```
Initializes an uninitialized node (assumes the data in the node is junk). 

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

To create a dynamic array type, define the type name using the `FIO_ARY_NAME` macro. i.e.:

```c
#define FIO_ARY_NAME int_ary
```

Next (usually), define the `FIO_ARY_TYPE` macro with the element type. The default element type is `void *`. For example:

```c
#define FIO_ARY_TYPE int
```

For complex types, define any (or all) of the following macros:

```c
#define FIO_ARY_TYPE_COPY(dest, src)  // set to adjust element copying 
#define FIO_ARY_TYPE_DESTROY(obj)     // set for element cleanup 
#define FIO_ARY_TYPE_CMP(a, b)        // set to adjust element comparison 
#define FIO_ARY_TYPE_INVALID 0 // to be returned when `index` is out of bounds / holes 
#define FIO_ARY_TYPE_INVALID_SIMPLE 1 // set ONLY if the invalid element is all zero bytes 
```

To create the type and helper functions, include the Simple Template Library header.

For example:

```c
typedef struct {
  int i;
  float f;
} foo_s;

#define FIO_ARY_NAME ary
#define FIO_ARY_TYPE foo_s
#define FIO_ARY_TYPE_CMP(a,b) (a.i == b.i && a.f == b.f)
#include "fio_cstl.h"

void example(void) {
  ary_s a;
  ary_init(&a);
  foo_s *p = ary_push(&a, (foo_s){.i = 42});
  FIO_ARY_EACH(&a, pos) { // pos will be a pointer to the element
    fprintf(stderr, "* [%zu]: %p : %d\n", (size_t)(pos - ary_to_a(&a)), pos->i);
  }
  ary_destroy(&a);
}
```

### Dynamic Arrays - API

#### The Array Type (`ARY_s`)

```c
typedef struct {
  FIO_ARY_TYPE *ary;
  uint32_t capa;
  uint32_t start;
  uint32_t end;
} FIO_NAME(FIO_ARY_NAME, s); /* ARY_s in these docs */
```

The array type should be considered opaque. Use the helper functions to updated the array's state when possible, even though the array's data is easily understood and could be manually adjusted as needed.

#### `ARY_init`

````c
void ARY_init(ARY_s * ary);
````

Initializes an uninitialized array object.

In addition, the following macro can be used for static initializations:

```c
#define FIO_ARY_INIT  {0}
```

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
FIO_ARY_TYPE * ARY_set(ARY_s * ary,
                       int32_t index,
                       FIO_ARY_TYPE data,
                       FIO_ARY_TYPE *old);
```

Sets `index` to the value in `data`.

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

If `old` isn't NULL, the existing data will be copied to the location pointed to by `old` before the copy in the Array is destroyed.

Returns a pointer to the new object, or NULL on error.

#### `ARY_get`

```c
FIO_ARY_TYPE ARY_get(ARY_s * ary, int32_t index);
```

Returns the value located at `index` (no copying is performed).

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

**Reminder**: indexes are zero based (first element == 0).

#### `ARY_find`

```c
int32_t ARY_find(ARY_s * ary, FIO_ARY_TYPE data, int32_t start_at);
```

Returns the index of the object or -1 if the object wasn't found.

If `start_at` is negative (i.e., -1), than seeking will be performed in reverse, where -1 == last index (-2 == second to last, etc').

#### `ARY_remove`
```c
int ARY_remove(ARY_s * ary, int32_t index, FIO_ARY_TYPE *old);
```

Removes an object from the array, MOVING all the other objects to prevent "holes" in the data.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns 0 on success and -1 on error.

This action is O(n) where n in the length of the array. It could get expensive.

#### `ARY_remove2`

```c
uint32_t ARY_remove2(ARY_S * ary, FIO_ARY_TYPE data);
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
FIO_ARY_TYPE * ARY_to_a(ARY_s * ary);
```

Returns a pointer to the C array containing the objects.

#### `ARY_push`

```c
FIO_ARY_TYPE * ARY_push(ARY_s * ary, FIO_ARY_TYPE data);
```

 Pushes an object to the end of the Array. Returns a pointer to the new object or NULL on error.

#### `ARY_pop`

```c
int ARY_pop(ARY_s * ary, FIO_ARY_TYPE *old);
```

Removes an object from the end of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_unshift`

```c
FIO_ARY_TYPE *ARY_unshift(ARY_s * ary, FIO_ARY_TYPE data);
```

Unshifts an object to the beginning of the Array. Returns a pointer to the new object or NULL on error.

This could be expensive, causing `memmove`.

#### `ARY_shift`

```c
int ARY_shift(ARY_s * ary, FIO_ARY_TYPE *old);
```

Removes an object from the beginning of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_each`

```c
uint32_t ARY_each(ARY_s * ary, int32_t start_at,
                               int (*task)(FIO_ARY_TYPE obj, void *arg),
                               void *arg);
```

Iteration using a callback for each entry in the array.

The callback task function must accept an the entry data as well as an opaque user pointer.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position (number of items processed + starting point).

#### `FIO_ARY_EACH`

```c
#define FIO_ARY_EACH(array, pos)                                               \
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


- `FIO_MAP_TYPE_DISCARD(obj)` - Handles discarded element data (i.e., insert without overwrite in a Set).
- `FIO_MAP_KEY_DISCARD(obj)` - Handles discarded element data (i.e., when overwriting an existing value in a hash map).
- `FIO_MAP_MAX_ELEMENTS` - The maximum number of elements allowed before removing old data (FIFO).
- `FIO_MAP_MAX_SEEK` -  The maximum number of bins to rotate when (partial/full) collisions occur. Limited to a maximum of 255.

To limit the number of elements in a map (FIFO), allowing it to behave similarly to a caching primitive, define: `FIO_MAP_MAX_ELEMENTS`.

if `FIO_MAP_MAX_ELEMENTS` is `0`, then the theoretical maximum number of elements should be: `(1 << 32) - 1`. In practice, the safe limit should be calculated as `1 << 31`.

For the full list of functions see: Hash Map / Set - API

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

#### `MAP_init`

```c
void MAP_init(MAP_PTR m);
```

Initializes a map object - often used for maps placed on the stack.

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

#### Set - API (set only)

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

#### Hash Map / Set - API (common)

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
void MAP_pop(FIO_MAP_PTR m);
```

Allows the Hash to be momentarily used as a stack, destroying the last object added (`FIO_MAP_TYPE_DESTROY` / `FIO_MAP_KEY_DESTROY`).

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

To create a dynamic string type, define the type name using the `FIO_STR_NAME`
macro.

The type (`FIO_STR_NAME_s`) and the functions will be automatically defined.

For the full list of functions see: Dynamic Strings

-------------------------------------------------------------------------------

## Reference Counting / Type Wrapping

If the `FIO_REF_NAME` macro is defined, then referece counting helpers can be
defined for any named type.

By default, `FIO_REF_TYPE` will equal `FIO_REF_NAME_s`, using the naming
convention in this library.

In addition, the `FIO_REF_METADATA` macro can be defined with any type, allowing
metadata to be attached and accessed using the helper function
`FIO_REF_metadata(object)`.

Note: requires the atomic operations to be defined (`FIO_ATOMIC`).

Reference counting adds the following functions:

#### `FIO_REF_TYPE * FIO_REF_NAME_new2(void)`

Allocates a new reference counted object, initializing it using the
`FIO_REF_INIT(object)` macro.

If `FIO_REF_METADATA` is defined, than the metadata is initialized using the
`FIO_REF_METADATA_INIT(metadata)` macro.

#### `FIO_REF_TYPE * FIO_REF_NAME_up_ref(FIO_REF_TYPE * object)`

Increases an object's reference count (an atomic operation, thread-safe).

#### `FIO_REF_NAME_free2(FIO_REF_TYPE * object)`

Frees an object or decreses it's reference count (an atomic operation,
thread-safe).

Before the object is freed, the `FIO_REF_DESTROY(object)` macro will be called.

If `FIO_REF_METADATA` is defined, than the metadata is also destoryed using the
`FIO_REF_METADATA_DESTROY(metadata)` macro.


#### `FIO_REF_METADATA * FIO_REF_NAME_metadata(FIO_REF_TYPE * object)`

If `FIO_REF_METADATA` is defined, than the metadata is accessible using this
inlined function.



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
with the file name and line number in which the error occured.

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
- `fio_lrot(i, bits)`
- `fio_rrot(i, bits)`

#### Bytes to Numbers (network ordered)
- `fio_str2u16(c)`
- `fio_str2u32(c)`
- `fio_str2u64(c)`

#### Numbers to Bytes (network ordered)
- `fio_u2str16(buffer, i)`
- `fio_u2str32(buffer, i)`
- `fio_u2str64(buffer, i)`

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

## Network Byte Ordering Helpers

This are commonly used for file and data storage / transmittions, since they
allow for system-independant formatting.

On big-endian systems, these macros a NOOPs, whereas on little-endian systems
these bacros flip the byte order.

#### `fio_lton16(i)`

Local byte order to Network byte order, 16 bit integer.

#### `fio_ntol16(i)`

Network byte order to Local byte order, 16 bit integer

#### `fio_lton32(i)`

Local byte order to Network byte order, 32 bit integer.

#### `fio_ntol32(i)`

Network byte order to Local byte order, 32 bit integer

#### `fio_lton64(i)`

Local byte order to Network byte order, 62 bit integer.

#### `fio_ntol64(i)`

Network byte order to Local byte order, 62 bit integer

-------------------------------------------------------------------------------

## Risky Hash (data hashing):

If the `FIO_RISKY_HASH` macro is defined than the following static function will
be defined:

#### `uint64_t fio_risky_hash(const void *data, size_t len, uint64_t seed)`

This function will produce a 64 bit hash for X bytes of data.

-------------------------------------------------------------------------------

## Pseudo Random Generation

If the `FIO_RAND` macro is defined, the following, non-cryptographic
psedo-random generator functions will be defined.

The random generator functions are automatically re-seeded with either data from
the system's clock or data from `getrusage` (when available).

**Note**: bitwise operations (`FIO_BITWISE`) and Risky Hash (`FIO_RISKY_HASH`)
are automatically defined along with `FIO_RAND`, since they are required by the
algorithm.

#### `uint64_t fio_rand64(void)`

Returns 64 random bits. Probably not cryptographically safe.

#### `void fio_rand_bytes(void *data_, size_t len)`

Writes `len` random Bytes to the buffer pointed to by `data`. Probably not
cryptographically safe.

-------------------------------------------------------------------------------

* String / Number conversion

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

## CLI (command line interface)

The simple template library includes a CLI parser, since parsing command line
arguments is a common task.

By defining `FIO_CLI`, the following functions will be defined.

In addition, `FIO_CLI` automatically includes tha `FIO_ATOL` flag, since CLI
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

The `fio_cli_start` macro accepts the `argc` and `argv`, as recieved by the
`main` functions, a maximum and minimum number of unspecified CLI arguments
(beneath which or after which the parser will fail), an application description
string and a variable list of (specified) command line argumens.

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

Returns a Strinf containing the unrecognized argument at the stated `index`
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

It's ideal if all long-tem allocations are performed during the start-up phase
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


Allocates memory directly using `mmap`, this is prefered for objects that both
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





-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------

## Hash Maps / Sets

Hash Map and Sets are both mapping / dictionary primitives.

A Set can be viewed as a hash map where the key == value and a Hash Map can be
viewed as a Set where hash valuses map to (key,value) couplets and equality
between couplets only tests equality between keys.

To create a Set, define `FIO_MAP_NAME`.

To create a Hash Map, define `FIO_MAP_KEY` (containing the key's type).

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

To limit the number of elements in a map (FIFO), allowing it to behave similarly
to a caching primitive, define: `FIO_MAP_MAX_ELEMENTS`.

if `FIO_MAP_MAX_ELEMENTS` is `0`, then the theoretical maximum number of
elements should be: (1 << 32) - 1

For the full list of functions see: Hash Map / Set - API

-------------------------------------------------------------------------------

## Dynamic Strings

To create a dynamic string type, define the type name using the `FIO_STR_NAME`
macro.

The type (`FIO_STR_NAME_s`) and the functions will be automatically defined.

For the full list of functions see: Dynamic Strings

-------------------------------------------------------------------------------

## Reference Counting / Type Wrapping

If the `FIO_REF_NAME` macro is defined, then referece counting helpers can be
defined for any named type.

By default, `FIO_REF_TYPE` will equal `FIO_REF_NAME_s`, using the naming
convention in this library.

In addition, the `FIO_REF_METADATA` macro can be defined with any type, allowing
metadata to be attached and accessed using the helper function
`FIO_REF_metadata(object)`.

Note: requires the atomic operations to be defined (`FIO_ATOMIC`).

Reference counting adds the following functions:

#### `FIO_REF_TYPE * FIO_REF_NAME_new2(void)`

Allocates a new reference counted object, initializing it using the
`FIO_REF_INIT(object)` macro.

If `FIO_REF_METADATA` is defined, than the metadata is initialized using the
`FIO_REF_METADATA_INIT(metadata)` macro.

#### `FIO_REF_TYPE * FIO_REF_NAME_up_ref(FIO_REF_TYPE * object)`

Increases an object's reference count (an atomic operation, thread-safe).

#### `FIO_REF_NAME_free2(FIO_REF_TYPE * object)`

Frees an object or decreses it's reference count (an atomic operation,
thread-safe).

Before the object is freed, the `FIO_REF_DESTROY(object)` macro will be called.

If `FIO_REF_METADATA` is defined, than the metadata is also destoryed using the
`FIO_REF_METADATA_DESTROY(metadata)` macro.


#### `FIO_REF_METADATA * FIO_REF_NAME_metadata(FIO_REF_TYPE * object)`

If `FIO_REF_METADATA` is defined, than the metadata is accessible using this
inlined function.

-------------------------------------------------------------------------------

## Logging and Assertions:

If the `FIO_LOG` macro is defined than the following logging helpers will be
added.

The optional `FIO_LOG_LENGTH_LIMIT` macro effects the amount of memory allocated
on the stack for the log message. Note that logging is defsigned to avoid heap
allocations. By default `FIO_LOG_LENGTH_LIMIT` is defined as 1024.

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
with the file name and line number in which the error occured.

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
- `fio_lrot(i, bits)`
- `fio_rrot(i, bits)`

#### Bytes to Numbers (network ordered)
- `fio_str2u16(c)`
- `fio_str2u32(c)`
- `fio_str2u64(c)`

#### Numbers to Bytes (network ordered)
- `fio_u2str16(buffer, i)`
- `fio_u2str32(buffer, i)`
- `fio_u2str64(buffer, i)`

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

## Network Byte Ordering Helpers

This are commonly used for file and data storage / transmittions, since they
allow for system-independant formatting.

On big-endian systems, these macros a NOOPs, whereas on little-endian systems
these bacros flip the byte order.

#### `fio_lton16(i)`

Local byte order to Network byte order, 16 bit integer.

#### `fio_ntol16(i)`

Network byte order to Local byte order, 16 bit integer

#### `fio_lton32(i)`

Local byte order to Network byte order, 32 bit integer.

#### `fio_ntol32(i)`

Network byte order to Local byte order, 32 bit integer

#### `fio_lton64(i)`

Local byte order to Network byte order, 62 bit integer.

#### `fio_ntol64(i)`

Network byte order to Local byte order, 62 bit integer

-------------------------------------------------------------------------------

## Risky Hash (data hashing):

If the `FIO_RISKY_HASH` macro is defined than the following static function will
be defined:

#### `uint64_t fio_risky_hash(const void *data, size_t len, uint64_t seed)`

This function will produce a 64 bit hash for X bytes of data.

-------------------------------------------------------------------------------

## Pseudo Random Generation

If the `FIO_RAND` macro is defined, the following, non-cryptographic
psedo-random generator functions will be defined.

The random generator functions are automatically re-seeded with either data from
the system's clock or data from `getrusage` (when available).

**Note**: bitwise operations (`FIO_BITWISE`) and Risky Hash (`FIO_RISKY_HASH`)
are automatically defined along with `FIO_RAND`, since they are required by the
algorithm.

#### `uint64_t fio_rand64(void)`

Returns 64 random bits. Probably not cryptographically safe.

#### `void fio_rand_bytes(void *data_, size_t len)`

Writes `len` random Bytes to the buffer pointed to by `data`. Probably not
cryptographically safe.

-------------------------------------------------------------------------------

* String / Number conversion

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

## CLI (command line interface)

The simple template library includes a CLI parser, since parsing command line
arguments is a common task.

By defining `FIO_CLI`, the following functions will be defined.

In addition, `FIO_CLI` automatically includes tha `FIO_ATOL` flag, since CLI
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

The `fio_cli_start` macro accepts the `argc` and `argv`, as recieved by the
`main` functions, a maximum and minimum number of unspecified CLI arguments
(beneath which or after which the parser will fail), an application description
string and a variable list of (specified) command line argumens.

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

Returns a Strinf containing the unrecognized argument at the stated `index`
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

It's ideal if all long-tem allocations are performed during the start-up phase
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


Allocates memory directly using `mmap`, this is prefered for objects that both
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
