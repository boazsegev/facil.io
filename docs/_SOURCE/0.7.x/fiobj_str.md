---
title: facil.io - FIOBJ String API
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

See the [core FIOBJ functions](`fiobj_core`) for accessing the String data ([`fiobj_obj2cstr`](fiobj_core#fiobj_obj2cstr)) and freeing the String object ([`fiobj_free`](fiobj_core#fiobj_free)).

### FIOBJ String Initialization

#### `fiobj_str_new`

```c
FIOBJ fiobj_str_new(const char *str, size_t len);
```

Creates a String object. Remember to use `fiobj_free`.

#### `fiobj_str_buf`

```c
FIOBJ fiobj_str_buf(size_t capa);
```

Creates a String object with pre-allocation for Strings up to `capa` long.

If `capa` is zero, **a whole memory page** will be allocated.

Remember to use `fiobj_free`.

#### `fiobj_str_copy`

```c
static inline FIOBJ fiobj_str_copy(FIOBJ src);
```


Creates a copy from an existing String. Remember to use `fiobj_free`.

Implemented as:

```c
static inline FIOBJ fiobj_str_copy(FIOBJ src) {
  fio_str_info_s s = fiobj_obj2cstr(src);
  return fiobj_str_new(s.data, s.len);
}
```

#### `fiobj_str_move`

```c
FIOBJ fiobj_str_move(char *str, size_t len, size_t capacity);
```

Creates a String object. Remember to use `fiobj_free`.

It's possible to wrap a previously allocated memory block in a FIOBJ String object, as long as it was allocated using `fio_malloc`.

The ownership of the memory indicated by `str` will "move" to the object and will be freed (using `fio_free`) once the object's reference count drops to zero.

Note: The original memory MUST be allocated using `fio_malloc` (NOT the system's `malloc`) and it will be freed using `fio_free`.

#### `fiobj_str_tmp`

```c
FIOBJ fiobj_str_tmp(void);
```

Returns a thread-static temporary string. Avoid calling `fiobj_dup` or `fiobj_free`.

## String Manipulation and Data

#### `fiobj_str_freeze`

```c
void fiobj_str_freeze(FIOBJ str);
```

Prevents the String object from being changed.

When a String is used as a key for a Hash, it is automatically frozen to prevent the Hash from becoming broken.

#### `fiobj_str_capa_assert`

```c
size_t fiobj_str_capa_assert(FIOBJ str, size_t size);
```

Confirms the requested capacity is available and allocates as required.

Returns updated capacity.

#### `fiobj_str_capa`

```c
size_t fiobj_str_capa(FIOBJ str);
```

Returns a String's capacity, if any. This should include the NUL byte.

#### `fiobj_str_resize`

```c
void fiobj_str_resize(FIOBJ str, size_t size);
```

Resizes a String object, allocating more memory if required.

#### `fiobj_str_compact`

```c
void fiobj_str_compact(FIOBJ str);
```

Performs a best attempt at minimizing memory consumption.

Actual effects depend on the underlying memory allocator and it's implementation. Not all allocators will free any memory.

#### `fiobj_str_compact`

```c
#define fiobj_str_minimize(str) fiobj_str_compact((str))
```

Alias for `fiobj_str_compact`.

#### `fiobj_str_clear`

```c
void fiobj_str_clear(FIOBJ str);
```


Empties a String's data, but keeps the memory used for that data available.


#### `fiobj_str_write`

```c
size_t fiobj_str_write(FIOBJ dest, const char *data, size_t len);
```

Writes data at the end of the string, resizing the string as required.

Returns the new length of the String.


#### `fiobj_str_write_i`

```c
size_t fiobj_str_write_i(FIOBJ dest, int64_t num)
```

Writes a number at the end of the String using normal base 10 notation.

Returns the new length of the String

#### `fiobj_str_printf`

```c
size_t fiobj_str_printf(FIOBJ dest, const char *format, ...);
```

Writes data at the end of the string using a `printf` like interface, resizing the string as required.

Returns the new length of the String

#### `fiobj_str_vprintf`

```c
size_t fiobj_str_vprintf(FIOBJ dest, const char *format, va_list argv);
```

Writes data at the end of the string using a `vprintf` like interface, resizing the string as required.

Returns the new length of the String.

#### `fiobj_str_concat`

```c
size_t fiobj_str_concat(FIOBJ dest, FIOBJ source);
```

Writes data at the end of the string, resizing the string as required.

Remember to call `fiobj_free` to free the source (when done with it).

Returns the new length of the String.

#### `fiobj_str_join`

```c
#define fiobj_str_join(dest, src) fiobj_str_concat((dest), (src))
```

Alias for [`fiobj_str_concat`](#fiobj_str_concat).


#### `fiobj_str_readfile`

```c
size_t fiobj_str_readfile(FIOBJ dest, const char *filename, intptr_t start_at,
                          intptr_t limit);
```

Dumps the contents of `filename` at the end of the String.

If `limit == 0`, than the data will be read until EOF.

If the file can't be located, opened or read, or if `start_at` is out of bounds (i.e., beyond the EOF position), FIOBJ_INVALID is returned.

If `start_at` is negative, it will be computed from the end of the file.

Remember to use `fiobj_free`.

NOTE: Requires a POSIX system, or might silently fail.

### Hashing

#### `fiobj_str_hash`

```c
uint64_t fiobj_str_hash(FIOBJ o);
```

Calculates a String's hash value for possible use as a Hash Map key.

Hash values use the default hashing function defined at compile time. This is usually SipHash 1-3, but could be set to a different hashing function (such as RiskyHash).

**Note**:

Since FIOBJ hashes are impervious to [hash flooding attacks](https://medium.freecodecamp.org/hash-table-attack-8e4371fc5261) by design and don't require a strong hashing function. However, objects often contain network (external) information, and it would be better to use a strong hashing function.
