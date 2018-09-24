---
title: facil.io - Linked Lists
sidebar: 0.6.x/_sidebar.md
---
# A Simple Linked List

Linked lists come in two main flavors:

1. Generic (or independent) Linked Lists:

    Where the list's node structure has a pointer that points to the object being added to the list. This requires objects to be allocated separately from list node, but allows a single object to belong to many lists (one-to-many).

2. Embedded Linked Lists:

    Where the list's node structure needs to be embedded within the object, thereby minimizing memory allocation and improving memory locality, while creating a one-to-one restriction (one object can belong to one list).

The single file library `fio_llist.h` offers both flavors in a simple to use package.

The API is practically the same for both, the only difference is in the prefix (`fio_ls_X` vs. `fio_ls_embd_X`).


## Data Structure and Initialization.

The container for a list is the same as a node, however, it must be initialized so that it links to itself (see `FIO_LS_INIT`). i.e.:

```c
fio_ls_s my_list = FIO_LS_INIT(my_list);
```

The linked list container is a simple data structure shown here... however, it is best to **access the data using the API and avoid accessing the data directly**. This will both protect the program from future changes to the data structures and minimize possible errors. 

```c
/** an embeded linked list. */
typedef struct fio_ls_embd_s {
  struct fio_ls_embd_s *prev;
  struct fio_ls_embd_s *next;
} fio_ls_embd_s;

/** an independent linked list. */
typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  const void *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)  { .next = &(name), .prev = &(name) }
```

The container can be dynamically allocated, placed of the stack or embedded in a dynamic object.

## Generic Linked List API

Note that the API is comprised of **inline static functions that act like macros**, so there is no performance to be gained by accessing the data structure directly (only integrity to be lost).

### `fio_ls_push`

```c
void fio_ls_push(fio_ls_s *pos, const void *obj)
```

Adds an object to the list's head.

### `fio_ls_unshift`

```c
void fio_ls_unshift(fio_ls_s *pos, const void *obj)
```


Adds an object to the list's tail. 

### `fio_ls_pop`

```c
void *fio_ls_pop(fio_ls_s *list)
```

Removes an object from the list's head.

### `fio_ls_shift`

```c
void *fio_ls_shift(fio_ls_s *list)
```

Removes an object from the list's tail.

### `fio_ls_remove`

```c
void *fio_ls_remove(fio_ls_s *node)
```

Removes a node from the list, returning the contained object.


### `fio_ls_is_empty`

```c
int fio_ls_is_empty(fio_ls_s *list)
```

Tests if the list is empty.

### `fio_ls_any`

```c
int fio_ls_any(fio_ls_s *list)
```

Tests if the list is NOT empty (contains any nodes).

### `FIO_LS_FOR`

```c
FIO_LS_FOR(list, pos)
```
 
Iterates through the list using a `for` loop.

Access the data with `pos->obj` (`pos` can be named however you please).

## Embedded List API

### `fio_ls_embd_push`

```c
void fio_ls_embd_push(fio_ls_embd_s *dest, fio_ls_embd_s *node)
```

Adds a node to the list's head.

### `fio_ls_embd_unshift`

```c
fio_ls_embd_unshift(fio_ls_embd_s *dest, fio_ls_embd_s *node)
```

Adds a node to the list's tail.

### `fio_ls_embd_pop`

```c
fio_ls_embd_s *fio_ls_embd_pop(fio_ls_embd_s *list)
```

Removes a node from the list's head.

### `fio_ls_embd_shift`

```c
fio_ls_embd_s *fio_ls_embd_shift(fio_ls_embd_s *list)
```

Removes a node from the list's tail.

### `fio_ls_embd_remove`

```c
fio_ls_embd_s *fio_ls_embd_remove(fio_ls_embd_s *node)
```

Removes a node from it's container list.

### `fio_ls_embd_is_empty`

```c
fio_ls_embd_is_empty(fio_ls_embd_s *list)
```

Tests if the list is empty.

### `fio_ls_embd_any`

```c
int fio_ls_embd_any(fio_ls_embd_s *list)
```

Tests if the list is NOT empty (contains any nodes).

### `FIO_LS_EMBD_FOR`

```c
FIO_LS_EMBD_FOR(list, node)
```

Iterates through the list using a `for` loop.

Access the data with `pos->obj` (`pos` can be named however you pleas..

### `FIO_LS_EMBD_OBJ`

```c
FIO_LS_EMBD_OBJ(type, member, plist) \
        ((type *)((uintptr_t)(plist) - (uintptr_t)(&(((type *)0)->member))))
```

Takes a list pointer `plist` and returns a pointer to it's container.

This uses pointer offset calculations and can be used to calculate any struct's pointer (not just list containers) as an offset from a pointer of one of it's members.

This is a very useful macro to have around.
