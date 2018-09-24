---
title: facil.io - Core Hash Map Type
sidebar: 0.6.x/_sidebar.md
---
# A Simple Hash Map

## Overview

The Hash Map core type offers an ordered key-value map with a very simple API.

The Hash Map is ordered by order of insertion.

The core Hash Map type is provided in a single file library `fio_hashmap.h`, which allows it's functions to be inlined for maximum performance (similarly to macros).

The Hash Map defaults to `uint64_t` keys, meaning that matching is performed using a simple integer comparison. However, [this could be changed by defining macros **before** including the library file for increased collision protection](#collision-protection).

Much like the Array in [the introduction to the simple core types](types.md), Hash Map containers can be placed on the stack as well as allocated dynamically.

## Types

The Core Hash uses the `fio_hash_s` type.

The data in in the `fio_hash_s` type should be considered *opaque* and shouldn't be accessed directly. Functional access should be preferred.

The `fio_hash_s` utilizes two arrays using internal data types that contain duplicates of the Hash key data, maximizing memory locality for Hash Map operations and minimizing cache misses to increase performance.

```c
typedef struct fio_hash_data_ordered_s {
  FIO_HASH_KEY_TYPE key; /* another copy for memory cache locality */
  void *obj;
} fio_hash_data_ordered_s;

typedef struct fio_hash_data_s {
  FIO_HASH_KEY_TYPE key; /* another copy for memory cache locality */
  struct fio_hash_data_ordered_s *obj;
} fio_hash_data_s;

/* the information in the Hash Map structure should be considered READ ONLY. */
struct fio_hash_s {
  uintptr_t count;
  uintptr_t capa;
  uintptr_t pos;
  uintptr_t mask;
  fio_hash_data_ordered_s *ordered;
  fio_hash_data_s *map;
};
```

## Functions

### `fio_hash_new`

```c
void fio_hash_new(fio_hash_s *hash)
```

Allocates and initializes internal data and resources.

It's important to call this function before using the Hash Map.

Lazy initialization of the Hash Map is possible by initializing the `fio_hash_s` to 0 (the default for static variables).

### `fio_hash_free`

```c
void fio_hash_free(fio_hash_s *hash)
```

Deallocates any internal resources.

It is critical that this function is called to prevent memory leaks. **However**, this function is not enough to prevent memory leaks - custom keys (if any) and object data should also be deallocated (see also `FIO_HASH_FOR_FREE`).

### `fio_hash_insert`

```c
void *fio_hash_insert(fio_hash_s *hash,
                      FIO_HASH_KEY_TYPE key,
                      void *obj)`
```

Inserts an object to the Hash Map Table, rehashing if required, returning the old object if it exists.

Set `obj` to NULL to remove an existing object (the existing object will be returned).

The `FIO_HASH_KEY_TYPE` defaults to `uint64_t`, [this could be changed by defining macros **before** including the library file for increased collision protection](#collision-protection).

### `fio_hash_find`

```c
void *fio_hash_find(fio_hash_s *hash, FIO_HASH_KEY_TYPE key)
```

Locates an object in the Hash Map Table according to the hash key value.

### `fio_hash_count`

```c
size_t fio_hash_count(const fio_hash_s *hash)
```

Returns the number of elements currently in the Hash Table.

### `fio_hash_capa`

```c
size_t fio_hash_capa(const fio_hash_s *hash)
```

Returns a temporary theoretical Hash map capacity.

This could be used for testing performance and memory consumption.

### `fio_hash_compact`

```c
size_t fio_hash_compact(fio_hash_s *hash)
```

Attempts to minimize memory usage by removing empty spaces caused by deleted
items (freeing their custom keys, if any) and rehashing the Hash Map.

Returns the updated hash map capacity.

### `fio_hash_rehash`

```c
void fio_hash_rehash(fio_hash_s *hash)
```

Forces a rehashing of the hash, increasing memory consumption as well as minimizing internal collisions (possibly improving seek times).

This function is called automatically when needed, it's unlikely that you should want to call the function yourself.

### `fio_hash_each`

```c
size_t fio_hash_each(fio_hash_s *hash,
                     const size_t start_at,
                     int (*task)(FIO_HASH_KEY_TYPE key,
                                 void *obj,
                                 void *arg),
                     void *arg);
```

Iteration using a callback for each entry in the Hash Table.

The callback task function must accept the hash key, the entry data and an
opaque user pointer:

```c
int example_task(FIO_HASH_KEY_TYPE key, void *obj, void *arg) {return 0;}
```

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position, i.e., the number of items processed +
the starting point.

### `FIO_HASH_FOR_LOOP`

```c
FIO_HASH_FOR_LOOP(hash, i)
```

A macro for a `for` loop that iterates over all the hashed objects (in
order).

`hash` a pointer to the hash table variable and `i` is a temporary variable
name to be created for iteration.

`i->key` is the key and `i->obj` is the hashed data.

The `i` variable can be names differently (i.e. `FIO_HASH_FOR_LOOP(hash, pos)` for `pos->key` and `pos->obj`).


### `FIO_HASH_FOR_FREE`

```c
FIO_HASH_FOR_FREE(hash, i)
```

A macro for a `for` loop that will iterate over all the hashed objects (in
order) and empties the hash, later calling `fio_hash_free` to free the hash.

`hash` a pointer to the hash table variable and `i` is a temporary variable
name to be created for iteration.

`i->key` is the key and `i->obj` is the hashed data.

Free the objects and the Hash Map container manually (if required). Custom keys will be freed automatically when using this macro.

### `FIO_HASH_FOR_EMPTY`

```c
FIO_HASH_FOR_EMPTY(hash, i)
```

A macro for a `for` loop that iterates over all the hashed objects (in
order) and empties the hash.

This will also reallocate the map's memory (to zero out the data), so if this
is performed before calling `fio_hash_free`, use `FIO_HASH_FOR_FREE` instead.

`hash` a pointer to the hash table variable and `i` is a temporary variable
name to be created for iteration.

`i->key` is the key and `i->obj` is the hashed data.

Free the objects and the Hash Map container manually (if required). Custom keys will be freed automatically when using this macro.

## Collision protection

The Hash Map is collision resistant as long as it's keys are truly unique.

If there's a chance that the default `uint64_t` key type will not be be able to uniquely identify a key, the following macros should **all** be defined, **before** including the `fio_hashmap.h` file in the `.c` file, allowing the default key system to be replaced:

### `FIO_HASH_KEY_TYPE`
  
This macro sets the type used for keys.

### `FIO_HASH_KEY_INVALID`    
    
Empty slots in the Hash Map are initialized so all their bytes are zero.

This macro should signify a static key that has all it's byte set to zero, making it an invalid key (it cannot be used, and objects placed in that slot will be lost).

### `FIO_HASH_KEY2UINT(key)`

This macro should convert the key to a unique unsigned number.

This, in effect, should return the hash value for the key and cannot be zero.

The value is used to determine the location of the key in the map (prior to any collisions) and a good hash will minimize collisions.

### `FIO_HASH_COMPARE_KEYS(k1, k2)`

This macro should compare two keys, excluding their hash values (which were compared using the `FIO_HASH_KEY2UINT` macro).

### `FIO_HASH_KEY_ISINVALID(key)`

Should evaluate as true if the key is an invalid key (all it's bytes set to zero).

### `FIO_HASH_KEY_COPY(key)`

Keys might contain temporary data (such as strings). To allow the Hash Map to test the key even after the temporary data is out of scope, ac opy needs to be created.

This macro should return a FIO_HASH_KEY_TYPE object that contains only persistent data. This could achieved by allocating some of the data using `malloc`.

### `FIO_HASH_KEY_DESTROY(key)`

When the Hash Map is re-hashed, old keys belonging to removed objects are cleared away and need to be destroyed.

This macro allows dynamically allocated memory to be freed (this is the complement of `FIO_HASH_KEY_COPY`).

**Note**: This macro must not end a statement (shouldn't use the `;` marker) or code blocks (`{}`). For multiple actions consider using inline functions.

### Notes

If the `FIO_HASH_KEY_COPY` macro allocated memory dynamically or if there's a need to iterate over the values in the Hash Map before freeing the Hash Map (perhaps to free the object's memory), the `FIO_HASH_FOR_FREE` macro can be used to iterate over the Hash Map, free all the keys and free the Hash Map resources (it ends by calling `fio_hash_free`).

**Note**: These macros are localized to the specific C file in which they were defined and a specific C file can't include the `fio_hashmap.h` more than once. If a few different approaches are required, it can be performed by using different C files and offering function wrappers for the Hash Map functions (wrap `fio_hash_find`, `fio_hash_insert`, etc').

### Example

In this example collisions are forced by setting the `hash` and string length to be equal for all keys, this demonstrates how defining these macros can secure the hash against String key collisions.

(another example can be found in the `pubsub.c` source code) 

```c
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* the hash key type for string keys */
typedef struct {
  size_t hash;
  size_t len;
  char *str;
} fio_hash_key_s;

/* strdup is usually available... but just in case it isn't */
static inline char *my_strdup(char *str, size_t len) {
  char *ret = malloc(len + 1);
  if(!ret)
    exit(-1);
  ret[len] = 0;
  memcpy(ret, str, len);
  return ret;
}

/* define the macro to set the key type */
#define FIO_HASH_KEY_TYPE fio_hash_key_s

/* the macro that returns the key's hash value */
#define FIO_HASH_KEY2UINT(key) ((key).hash)

/* Compare the keys using length testing and `memcmp` */
#define FIO_HASH_COMPARE_KEYS(k1, k2)                                          \
  ((k1).str == (k2).str ||                                                     \
   ((k1).len == (k2).len && !memcmp((k1).str, (k2).str, (k2).len)))

/* an "all bytes are zero" invalid key */
#define FIO_HASH_KEY_INVALID ((fio_hash_key_s){.hash = 0})

/* tests if a key is the invalid key */
#define FIO_HASH_KEY_ISINVALID(key) ((key).str == NULL)

/* creates a persistent copy of the key's string */
#define FIO_HASH_KEY_COPY(key)                                                 \
  ((fio_hash_key_s){.hash = (key).hash,                                        \
                    .len = (key).len,                                          \
                    .str = my_strdup((key).str, (key).len)})

/* frees the allocated string, remove the `fprintf` in production */
#define FIO_HASH_KEY_DESTROY(key)                                              \
  (fprintf(stderr, "freeing %s\n", (key).str), free((key).str))

#include "fio_hashmap.h"

int main(void) {
  fio_hash_s hash;
  fio_hash_key_s key1 = {.hash = 1, .len = 5, .str = "hello"};
  fio_hash_key_s key1_copy = {.hash = 1, .len = 5, .str = "hello"};
  fio_hash_key_s key2 = {.hash = 1, .len = 5, .str = "Hello"};
  fio_hash_key_s key3 = {.hash = 1, .len = 5, .str = "Hell0"};
  fio_hash_new(&hash);
  fio_hash_insert(&hash, key1, key1.str);
  key1.str = "oops";
  if (fio_hash_find(&hash, key1))
    fprintf(stderr,
            "ERROR: string comparison should have failed, instead got: %s\n",
            (char *)fio_hash_find(&hash, key1));
  else if (fio_hash_find(&hash, key1_copy))
    fprintf(stderr, "Hash string comparison passed for %s\n",
            (char *)fio_hash_find(&hash, key1_copy));

  fio_hash_insert(&hash, key2, key2.str);
  fio_hash_insert(&hash, key3, key3.str);
  fio_hash_insert(&hash, key2, NULL); /* deletes the key2 object  */
  fio_hash_rehash(&hash); /* forces the unused key to be destroyed */
  fprintf(stderr, "Did we free %s?\n", key2.str);
  FIO_HASH_FOR_EMPTY(&hash, i) { (void)i->obj; }
  if (fio_hash_find(&hash, key1_copy))
    fprintf(stderr,
            "ERROR: string comparison should have failed, instead got: %s\n",
            (char *)fio_hash_find(&hash, key1));
  fprintf(stderr, "reinserting stuff\n");
  fio_hash_insert(&hash, key2, key2.str);
  fio_hash_insert(&hash, key3, key3.str);
  FIO_HASH_FOR_FREE(&hash, i) { (void)i->obj; }
}
```

