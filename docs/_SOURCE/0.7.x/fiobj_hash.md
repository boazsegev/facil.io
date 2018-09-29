---
title: facil.io - FIOBJ Hash Map API
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

### Hash Creation


#### `fiobj_hash_new`

```c
FIOBJ fiobj_hash_new(void);
```

Creates a mutable empty Hash object. Use `fiobj_free` when done.

Notice that these Hash objects are optimized for smaller collections and retain order of object insertion.

#### `fiobj_hash_new2`

```c
FIOBJ fiobj_hash_new2(size_t capa);
```

Creates a mutable empty Hash object with an initial capacity of `capa`. Use `fiobj_free` when done.

This allows optimizations for larger (or smaller) collections.


### Hash properties and state

#### `fiobj_hash_capa`

```c
size_t fiobj_hash_capa(const FIOBJ hash);
```

Returns a temporary theoretical Hash map capacity. This could be used for testing performance and memory consumption.

#### `fiobj_hash_count`

```c
size_t fiobj_hash_count(const FIOBJ hash);
```

Returns the number of elements in the Hash.

### Populating and Managing the Hash

#### `fiobj_hash_set`

```c
int fiobj_hash_set(FIOBJ hash, FIOBJ key, FIOBJ obj);
```

Sets a key-value pair in the Hash, duplicating the Symbol and **moving** the ownership of the object to the Hash.

Returns -1 on error.

#### `fiobj_hash_pop`

```c
FIOBJ fiobj_hash_pop(FIOBJ hash, FIOBJ *key);
```


Allows the Hash to be used as a stack.

If a pointer `key` is provided, it will receive ownership of the key (remember to free).

Returns FIOBJ_INVALID on error.

Returns and object if successful (remember to free).

#### `fiobj_hash_replace`

```c
FIOBJ fiobj_hash_replace(FIOBJ hash, FIOBJ key, FIOBJ obj);
```

Replaces the value in a key-value pair, returning the old value (and it's ownership) to the caller.

A return value of FIOBJ_INVALID indicates that no previous object existed (but a new key-value pair was created.

Errors are silently ignored.

Remember to free the returned object.

#### `fiobj_hash_remove`

```c
FIOBJ fiobj_hash_remove(FIOBJ hash, FIOBJ key);
```

Removes a key-value pair from the Hash, if it exists, returning the old object (instead of freeing it).

#### `fiobj_hash_remove2`

```c
FIOBJ fiobj_hash_remove2(FIOBJ hash, uint64_t key_hash);
```

Removes a key-value pair from the Hash, if it exists, returning the old object (instead of freeing it).

#### `fiobj_hash_delete`

```c
int fiobj_hash_delete(FIOBJ hash, FIOBJ key);
```

Deletes a key-value pair from the Hash, if it exists, freeing the associated object.

Returns -1 on type error or if the object never existed.

#### `fiobj_hash_delete2`

```c
int fiobj_hash_delete2(FIOBJ hash, uint64_t key_hash);
```

Deletes a key-value pair from the Hash, if it exists, freeing the associated object.

This function takes a `uint64_t` Hash value (see `fio_siphash`) to perform a lookup in the HashMap, which is slightly faster than the other variations.

Returns -1 on type error or if the object never existed.

#### `fiobj_hash_get`

```c
FIOBJ fiobj_hash_get(const FIOBJ hash, FIOBJ key);
```


Returns a temporary handle to the object associated with the Symbol, FIOBJ_INVALID if none.

#### `fiobj_hash_get2`

```c
FIOBJ fiobj_hash_get2(const FIOBJ hash, uint64_t key_hash);
```

Returns a temporary handle to the object associated hashed key value.

This function takes a `uint64_t` Hash value (see `fio_siphash`) to
perform a lookup in the HashMap, which is slightly faster than the other
variations.

Returns FIOBJ_INVALID if no object is associated with this hashed key value.

#### `fiobj_hash_haskey`

```c
int fiobj_hash_haskey(const FIOBJ hash, FIOBJ key);
```

Returns 1 if the key (Symbol) exists in the Hash, even if it's value is NULL.

#### `fiobj_hash_clear`

```c
void fiobj_hash_clear(const FIOBJ hash);
```

Empties the Hash.

### Rehashing

Rehashing is usually performed automatically, as needed, and shouldn't be performed manually unless there is a known reason to do so.

#### `fiobj_hash_rehash`

```c
void fiobj_hash_rehash(FIOBJ h);
```

Attempts to rehash the Hash Map.

