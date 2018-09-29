---
title: facil.io - FIOBJ Generic Functions
sidebar: 0.6.x/_sidebar.md
---
# {{{title}}}

Some `FIOBJ` functions can be called for any `FIOBJ` object, regardless of their type.

These generic functions are listed here.


### FIOBJ Memory Management

#### `fiobj_dup`

```c
FIOBJ fiobj_dup(FIOBJ);
```

Heuristic copy with a preference for copy by reference(!), to minimize allocations.

This usually means the only action performed is reference count increase.

Always returns the value passed along.

#### `fiobj_free`

```c
void fiobj_free(FIOBJ);
```

Frees the object and any of it's "children".

This function affects nested objects, meaning that when an Array or a Hash object is passed along, it's children (the nested objects) are also freed.

For Hash objects, only the value objects are deallocated. The `keys` aren't "owned" by the Hash Map and therefore they aren't automatically allocated.

### FIOBJ Soft Type Recognition

#### `fiobj_type`

```c
fiobj_type_enum fiobj_type(FIOBJ o);
```

Returns the object's type.

Valid return values are:
`FIOBJ_T_NULL` - Object is the primitive `null`
* `FIOBJ_T_TRUE` - Object is the primitive `true` 
* `FIOBJ_T_FALSE` - Object is the primitive `false` 
* `FIOBJ_T_NUMBER` - Object is a number.
* `FIOBJ_T_FLOAT` - Object is a floating point number (`double`).
* `FIOBJ_T_STRING` - Object is a binary String.
* `FIOBJ_T_ARRAY` - Object is a FIOBJ array.
* `FIOBJ_T_HASH` - Object is a FIOBJ hash.
* `FIOBJ_T_DATA` - Object is a data stream, either wrapping a temporary file or a memory block.
* `FIOBJ_T_UNKNOWN` - Object type is unknown (a user's type).

#### `fiobj_type_is`

```c
size_t fiobj_type_is(FIOBJ o, fiobj_type_enum type)
```

This is faster than getting the type, since parts of the switch statement are optimized away (they are calculated during compile time).

#### `fiobj_type_name`

```c
const char *fiobj_type_name(const FIOBJ obj);
```
Returns a C string naming the objects dynamic type.

### Object Equality / Truthfulness

#### `fiobj_is_true`

```c
int fiobj_is_true(const FIOBJ);
```

Tests if an object evaluates as TRUE.

This is object type specific. For example, empty strings might evaluate as FALSE, even though they aren't a boolean type.

#### `fiobj_iseq`

```c
int fiobj_iseq(const FIOBJ obj1, const FIOBJ obj2);
```

Deeply compares two objects. No hashing or recursive function calls are
involved.

Uses a similar algorithm to `fiobj_each2`, except adjusted to two objects.

Hash objects are order sensitive. To be equal, Hash keys must match in order.

Returns 1 if true and 0 if false.

### Object Conversion

#### `fiobj_obj2num`

```c
intptr_t fiobj_obj2num(const FIOBJ obj);
```

Returns an Object's numerical value.

If a String is passed to the function, it will be parsed assuming base 10
numerical data.

Hashes and Arrays return their object count.

IO objects return the length of their data.

A type error results in 0.

#### `fiobj_obj2float`

```c
double fiobj_obj2float(const FIOBJ obj);
```

Returns a Float's value.

If a String is passed to the function, they will benparsed assuming base 10
numerical data.

A type error results in 0.

#### `fiobj_obj2cstr`

```c
fio_cstr_s fiobj_obj2cstr(const FIOBJ obj);
```

Returns a C String (NUL terminated) using the `fio_cstr_s` data type.

The Sting in binary safe and might contain NUL bytes in the middle as well as
a terminating NUL.

If a a Number or a Float are passed to the function, they
will be parsed as a *temporary*, thread-safe, String.

Numbers will be represented in base 10 numerical data.

A type error results in NULL (i.e. object can't be represented automatically as a String).

#### `fiobj_obj2hash`

```c
uint64_t fiobj_obj2hash(const FIOBJ o);
```

Calculates an Objects's SipHash value for possible use as a HashMap key.

The Object MUST answer to the fiobj_obj2cstr, or the result is unusable. In other woords, Hash Objects and Arrays can NOT be used for Hash keys.

### Iteration

#### `fiobj_each1`

```c
size_t fiobj_each1(FIOBJ, size_t start_at,
                  int (*task)(FIOBJ obj, void *arg), void *arg);

```

Single layer iteration using a callback for each nested fio object.

Accepts any `FIOBJ` type but only collections (Arrays and Hashes) are
processed. The container itself (the Array or the Hash) is **not** processed
(unlike `fiobj_each2`).

The callback task function must accept an object and an opaque user pointer.

Hash objects pass along only the value object. The keys can be accessed using the [`fiobj_hash_key_in_loop`](#fiobj_hash_key_in_loop) function.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the "stop" position, i.e., the number of items processed + the
starting point.

#### `fiobj_each2`

```c
size_t fiobj_each2(FIOBJ, int (*task)(FIOBJ obj, void *arg), void *arg);
```

Deep iteration using a callback for each fio object, including the parent.

Accepts any `FIOBJ ` type.

Collections (Arrays, Hashes) are deeply probed and shouldn't be edited
during an `fiobj_each2` call (or weird things may happen).

The callback task function must accept an object and an opaque user pointer.

Hash objects keys are available using the [`fiobj_hash_key_in_loop`](#fiobj_hash_key_in_loop) function.

Notice that when passing collections to the function, the collection itself
is sent to the callback followed by it's children (if any). This is true also
for nested collections (a nested Hash will be sent first, followed by the
nested Hash's children and then followed by the rest of it's siblings.

If the callback returns -1, the loop is broken. Any other value is ignored.

#### `fiobj_hash_key_in_loop`

```c
FIOBJ fiobj_hash_key_in_loop(void);
```

Returns the key for the object in the current `fiobj_each` loop (if any).
