---
title: facil.io - FIOBJ Numbers API
sidebar: 0.6.x/_sidebar.md
---
# {{{title}}}

### Numbers API (Integers)

#### `fiobj_num_new`

```c
FIOBJ fiobj_num_new(intptr_t num);
```

Creates a Number object. Remember to use `fiobj_free`.

#### `fiobj_num_tmp`

```c
FIOBJ fiobj_num_tmp(intptr_t num);
```

Creates a temporary Number object. Avoid using `fiobj_free`.

### Float API (Double)

#### `fiobj_float_new`

```c
FIOBJ fiobj_float_new(double num);
```

Creates a Float object. Remember to use `fiobj_free`. 

#### `fiobj_float_set`

```c
void fiobj_float_set(FIOBJ obj, double num);
```

Mutates a Float object's value. Effects every object's reference! 

#### `fiobj_float_tmp`

```c
FIOBJ fiobj_float_tmp(double num);
```

Creates a temporary Float object. Avoid using `fiobj_free`.

### Numerical Helpers: not FIOBJ specific, but included as part of the library

#### `fio_atol`

```c
int64_t fio_atol(char **pstr);
```

A helper function that converts between String data to a signed int64_t.

Numbers are assumed to be in base 10.

The `0x##` (or `x##`) and `0b##` (or `b##`) are recognized as base 16 and
base 2 (binary MSB first) respectively.

The pointer will be updated to point to the first byte after the number.

#### `fio_atof`

```c
double fio_atof(char **pstr);
```

A helper function that converts between String data to a signed double.

#### `fio_ltoa`

```c
size_t fio_ltoa(char *dest, int64_t num, uint8_t base);
```

A helper function that converts between a signed int64_t to a string.

No overflow guard is provided, make sure there's at least 66 bytes available
(for base 2).

Supports base 2, base 10 and base 16. An unsupported base will silently
default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
beginning of the string).

Returns the number of bytes actually written (excluding the NUL terminator).

#### `fio_ftoa`

```c
size_t fio_ftoa(char *dest, double num, uint8_t base);
```

A helper function that converts between a double to a string.

No overflow guard is provided, make sure there's at least 130 bytes available
(for base 2).

Supports base 2, base 10 and base 16. An unsupported base will silently
default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
beginning of the string).

Returns the number of bytes actually written (excluding the NUL terminator).

#### `fio_ltocstr`

```c
fio_cstr_s fio_ltocstr(long);
```

Converts a number to a temporary, thread local, C string object

The `fio_str_s` object is only valid until the function is called again within the same thread or the thread terminates.

#### `fio_ftocstr`

```c
fio_cstr_s fio_ftocstr(double);
```
Converts a float to a temporary, thread local, C string object

The `fio_str_s` object is only valid until the function is called again within the same thread or the thread terminates.
