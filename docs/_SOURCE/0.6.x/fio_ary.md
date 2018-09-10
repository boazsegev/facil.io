---
title: facil.io - Core Dynamic Array Type
sidebar: 0.6.x/_sidebar.md
---
# Simple Dynamic Arrays

The simple dynamic array offers a simple data structure that manages only it's own memory (but not the memory of any objects placed in the array).

## Overview

The simple dynamic array type is included in a single file library, `fio_ary.h` that can be used independently as well.

The Core Array is designed to contain `void *` pointers and doesn't manage any memory except it's own.

As a general rule, facil.io doen's make assumptions about allocation / deallocation except where mentioned. A good example is the `sock_write` function that copies the data vs. `sock_write2` which asks about the correct deallocation function to be used (defaults to `free`) and takes ownership of the data.

## Example

Here's a short example from [the introduction to the simple core types](types.md):

```c
#include "fio_ary.h"

int main(void) {
  // The array container.
  fio_ary_s ary;
  // allocate and initialize internal data
  fio_ary_new(&ary, 0);
  // perform some actions
  fio_ary_push(&ary, (void *)1);
  FIO_ARY_FOR(&ary, pos) {
    printf("index: %lu == %lu\n", (unsigned long)pos.i, (unsigned long)pos.obj);
  }
  // free the array's data to avoid memory leaks (doesn't free the objects)
  fio_ary_free(&ary);
}
```

Note that the Array container can be placed on the stack (as well as allocated using `malloc`), but the internal data must be allocated and deallocated using `fio_ary_new` and `fio_ary_free`.

## Types

The Core Array uses the `fio_ary_s` type.

The data in in the `fio_ary_s` type shouldn't be accessed directly. Functional access should be preferred.

```c
typedef struct fio_ary_s {
  size_t start;
  size_t end;
  size_t capa;
  void **arry;
} fio_ary_s;
```

## Functions

### The `fio_ary_new` function

```c
void fio_ary_new(fio_ary_s *ary, size_t capa)
```

Initializes the array and allocates memory for it's internal data.

Note that `capa` indicates the **initial** (or *minimal*) capacity for the array, but the array can grow as long as memory allows.

This will assume that `ary` is uninitialized and overwrite any existing data.

### the `fio_ary_free` function

```c
void fio_ary_free(fio_ary_s *ary)
```

Frees the array's internal data.

If the `ary`'s container (the `fio_ary_s` object) was allocated using `malloc`, a subsequent call to `free(ary)` must be made.

### the `fio_ary_count` function

```c
size_t fio_ary_count(fio_ary_s *ary)
```

Returns the number of elements in the Array.

### the `fio_ary_capa` function

```c
size_t fio_ary_capa(fio_ary_s *ary)
```

Returns the current, temporary, array capacity (it's dynamic).

### the `fio_ary_index` function

```c
void *fio_ary_index(fio_ary_s *ary, int64_t pos)
```

Returns the object placed in the Array, if any. Returns NULL if no data or if
the index is out of bounds.

Negative values are retrived from the end of the array. i.e., `-1`
is the last item.

`fio_ary_entry` is an alias for `fiobj_ary_index`.

### the `fio_ary_set` function

```c
void *fio_ary_set(fio_ary_s *ary, void *data, int64_t pos)
```

Sets an object at the requested position.

Returns the old value, if any.

If an error occurs, the same data passed to the function is returned (test using `fiobj_ary_index`).

### the `fio_ary_push` function

```c
int fio_ary_push(fio_ary_s *ary, void *data)
```

Pushes an object to the end of the Array. Returns -1 on error.

### the `fio_ary_pop` function

```c
void *fio_ary_pop(fio_ary_s *ary)
```

Pops an object from the end of the Array

Returns NULL if the object was NULL or the Array was empty.

### the `fio_ary_unshift` function

```c
int fio_ary_unshift(fio_ary_s *ary, void *data)
```

Unshifts an object to the beginning of the Array. Returns -1 on error.

This could be expensive, causing `memmove`.

### the `fio_ary_shift` function

```c
void *fio_ary_shift(fio_ary_s *ary)
```

Shifts an object from the beginning of the Array.

Returns NULL if the object was NULL or the Array was empty.


### the `fio_ary_compact` function

```c
void fio_ary_compact(fio_ary_s *ary)
```

Removes any NULL *pointers* from an Array, keeping all other data in the
array.

This action is O(n) where n in the length of the array.

### the `FIO_ARY_FOR` macro

```c
FIO_ARY_FOR(ary, pos)
```

Iterates through the list using a `for` loop.

Access the data with `pos.obj` and it's index with `pos.i`.

The `pos` variable can be named however you please (i.e. `FIO_ARY_FOR(&bar, foo)` for `foo.i` and `foo.obj`).

### the `fio_ary_each` function

```c
size_t fio_ary_each(fio_ary_s *ary, size_t start_at,
                                    int (*task)(void *pt, void *arg),
                                    void *arg);
```

Iteration using a callback for each entry in the array.

The callback task function must accept an the entry data as well as an opaque
user pointer.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position, i.e., the number of items processed +
the starting point.
