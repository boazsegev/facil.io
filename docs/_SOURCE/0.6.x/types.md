---
title: facil.io - Core Types
sidebar: 0.6.x/_sidebar.md
---
# Core types in facil.io 

At it's core, facil.io utilizes a number of core types that make it easier to develop network applications.

These types are divided into three categories:

1. Core types: [dynamic arrays](fio_ary), [hash maps](fio_hashmap) and [linked lists](fio_llist).

1. Core Object types: the [facil.io object types (`FIOBJ`)](fiobj).

1. Network / API specific types: these are specific types that are used in specific function calls or situations, such as `protocol_s`, `http_s` etc'.

    These types will be documented along with their specific API / extension in the [Modules](modules) section. 

Here I will provide an overview for the first two categories, core types and objects.

## Core types overview

Many tasks in C could require a Linked List, a Dynamic Array or a Hashmap.

This is why facil.io includes these three core types as single file libraries that use macros and inline-functions.

These core types are fairly easy to use and the single file libraries are well documented.

### Memory Ownership

Note that unlike the FIOBJ object system, facil.io's core types don't have prior information about the memory model of the objects they store.

For this reason, although objects are stored within a `fio_ls_s`, `fio_ary_s` or `fio_hash_s`, they should be freed manually once they are ejected from their container.

On the other hand, FIOBJ Arrays and Hashmaps know the FIOBJ memory model and can (and will) free any objects they hold when appropriate.

### Type Memory Management

All the core types are divided into two parts:

1. A container which must be initialized (and perhaps destroyed).

1. Internal data that must be allocated and deallocated.

The container could be placed on the stack as well as allocated on the heap.

For example, here's an array with a container placed on the stack:

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
  // free the internal data to avoid memory leaks
  fio_ary_free(&ary);
}
```

Here's the same example, with the array container dynamically allocated:

```c
#include "fio_ary.h"

int main(void) {
  // The array container.
  fio_ary_s *ary = malloc(sizeof(*ary));
  if (!ary)
    exit(-1);
  // allocate and initialize internal data
  fio_ary_new(ary, 0);
  // perform some actions
  fio_ary_push(ary, (void *)1);
  FIO_ARY_FOR(ary, pos) {
    printf("index: %lu == %lu\n", (unsigned long)pos.i, (unsigned long)pos.obj);
  }
  // free the internal data to avoid memory leaks
  fio_ary_free(ary);
  free(ary);
}
```
### Core Types API

The API for the core types is documented within the source files. To read more about each type click the links here:

1. [Dynamic Arrays](fio_ary)

1. [HashMaps](fio_hashmap)

1. [Linked Lists](fio_llist).


