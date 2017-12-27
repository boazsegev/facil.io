# Core types in facil.io 

At it's core, facil.io utilizes a number of core types, that make it easier to develop network application.

These types are divided into three categories:

1. Core types: [dynamic arrays](fio_ary.md), [hash maps](fio_hashmap.md) and [linked lists](fio_list.md).

1. Core object: the [facil.io object types (`fiobj_s *`)](fiobj.md).

1. Network / API related types: these are specific types that are used in specific function calls or situations, such as `protocol_s`, `http_s` etc'.

    These types will be documented along with their specific API / extension. 

Here I will provide an overview for the first two categories, core types and objects.

## Core types overview

It's very common in C to require a Linked List, a Dynamic Array or a Hashmap.

This is why facil.io includes these three core types as single file libraries that use macros and inline-functions\*.

\* The 4 bit trie / dictionary type that will not be documented here as it might be removed in future releases.

### Type Memory Management

All the core types are divided into two parts:

1. A container which must be initialized (and perhaps destroyed).

1. internal data that must be allocates and deallocated.

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

1. [Dynamic Arrays](fio_ary.md)

1. [HashMaps](fio_hashmap.md)

1. [Linked Lists](fio_list.md).


