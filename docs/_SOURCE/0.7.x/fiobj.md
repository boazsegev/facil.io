---
title: facil.io - a Dynamic Type System using facil.io objects.
sidebar: 0.7.x/_sidebar.md
---
# Dynamic Type System: facil.io objects (`FIOBJ`)

In this page you will find a general overview. For detailed API information please visit the Core or Type pages.

To use the facil.io FIOBJ API, include the file `fiobj.h`

## Overview

`facil.io` offers a dynamic type system that makes it a breeze to mix object types together.

This dynamic type system is an independent module within the `facil.io` core and can be used separately.

The `FIOBJ` type API is divided by it's inner types (tested using `FIOBJ_TYPE(obj)` or `FIOBJ_TYPE_IS(obj, type)`):

* [Core and Generic API](fiobj_core)
* [Primitive Types](fiobj_primitives)
* [Number / Float](fiobj_numbers)
* [String](fiobj_str)
* [Array](fiobj_ary)
* [Hash](fiobj_hash)
* [Data](fiobj_data)
* [JSON](fiobj_json)
* [Mustache](fiobj_mustache)

### Why we need dynamic types?

C doesn't lend itself easily to the dynamic types that are often used in languages such as Javascript. This makes it harder to use an optimized C backend (server) when the frontend (client / browser) expects multi-type responses such as JSON objects.

To resolve this difference in expectations, `facil.io` offers the `FIOBJ` type system.

This is an opaque type that can be tested using `FIOBJ_TYPE(obj)` or `FIOBJ_TYPE_IS(obj, type)`.

This offers the following advantages (among others):

* Saves you precious development time.

* Allows deep integration with `facil.io` services, reducing the need to translate from one type to another.

* Allows for "typeless" actions, such as collection iteration (`fiobj_each2`), simple conversion (`fiobj_obj2num` and `fiobj_obj2cstr`), deallocation (`fiobj_free`). reference counting (`fiobj_dup`) and equality checks (`fiobj_iseq`).

* Offers JSON parsing and formatting to and from `FIOBJ`.

* Offers non-recursive iteration.

## API Considerations

This is a short summery regarding the API and it's use. The `fiobj_*` API is well documented in the header files, so only main guidelines are mentioned.

### Functional Access

All object access should be functional, or using the macros provided. Although this requirement can be circumvented, using the functional interface should be preferred.

For example:

```c
#include "fiobj.h"
/* this will work */
FIOBJ str = fiobj_str_buf(6); /* automatically adds room for the NUL terminator */
fio_str_info_s raw_str = fiobj_obj2cstr(str);
memcpy(raw_str.buffer, "Hello!", 6);
fiobj_str_resize(str, 6);
// ...
fiobj_free(str);

/* this is better */
FIOBJ str = fiobj_str_buf(6);
fiobj_str_write(str, "Hello!", 6);
// ...
fiobj_free(str);

/* for simple strings, one line will do */
FIOBJ str = fiobj_str_new("Hello!", 6);
// ...
fiobj_free(str);

/* for more complex cases, printf style is supported */
FIOBJ str = fiobj_str_buf(1); // note that 0 == whole memory page
fiobj_str_printf("%s %d" , "Hello!", 42)
// ...
fiobj_free(str);
```

### Ownership Follows Nesting

An object's memory should *always* be managed by it's "owner". This usually means the calling function.

*However*, when an object is nested within another object (i.e., placed in an Array or set as the *value* for a Hash or an HTTP header), **the ownership of the object is transferred**.

In the following example, the String nested within the Array is freed when the Array is freed:

```c
FIOBJ ary = fiobj_ary_new();
FIOBJ str = fiobj_str_new("Hello!", 6);
fiobj_ary_push(ary, str);
// ...
fiobj_free(ary);
```
Hashes follow the same rule. However...

It's important to note that **Hash keys ownership isn't transferred to the Hash** (keys are used to access and store data, but they are not the data itself).

When calling `fiobj_hash_set`, we are storing a *value* in the Hash, the *key* is what we use to access that value. This is why **the key's ownership remains with the calling function**. i.e.:

```c
FIOBJ h = fiobj_hash_new();
FIOBJ key = fiobj_str_new("life", 4);
/* By placing the Number in the Hash, it will be deallocated together with the Hash */
fiobj_hash_set(h, key, fiobj_num_new(42));
// ...
fiobj_free(h); /* Free the Hash and it's data, but NOT the key */
// ...
/* eventually we need to free the key */
fiobj_free(key);
```

### Passing By Reference

All objects are passed along by reference. The `dup` (duplication) process simply increases the reference count.

This is a very powerful tool. In the following example, `str2` is a "copy" **by reference** of `str`. By editing `str2` we're also editing `str`:

```c
FIOBJ str = fiobj_str_new("Hello!", 6);
FIOBJ str2 = fiobj_dup(str);
/* We'll edit str2 to say "Hello There!" instead of "Hello!" */
fiobj_str_resize(str2, 5);
fiobj_str_write(str2, " There!", 7);
/* This prints "Hello There!" because str was edited by reference! */
printf("%s\n", fiobj_obj2cstr(str).data);
/* we need to free both references to free the memory */
fiobj_free(str);
fiobj_free(str2);
```

An independent copy can be created using an object's specific copy function. This example  create a new, independent, object instead of referencing the old one:

```c
FIOBJ str = fiobj_str_new("Hello!", 6);
/* create a copy instead of a reference */
FIOBJ str2 = fiobj_str_copy(str);
/* this is the same as */
FIOBJ str3 = fiobj_str_new(fiobj_obj2cstr(str).data, fiobj_obj2cstr(str).len);
// ...
fiobj_free(str);
fiobj_free(str2);
fiobj_free(str3);
```

Copy by reference produces a deep reference adjustment, so Arrays and Hashes can be safely copied by reference.

```c
FIOBJ ary = fiobj_ary_new();
fiobj_ary_push(ary, fiobj_str_new("Hello!", 6));
FIOBJ ary_copy = fiobj_dup(ary);
// ...
fiobj_free(ary);
// all the items in ary2 are still accessible.
fprintf(stderr, "%s\n", fiobj_obj2cstr( fiobj_ary_index(ary_copy, -1) ).buffer );
fiobj_free(ary_copy);
```

### Cyclic Nesting Errors

Cyclic protection is unsupported mostly because of performance concerns, but also because cyclic nesting is impractical for network applications (for example, how would a cyclic object be formatted into JSON?).  

Cyclic nesting should be avoided. For example, the following code will (at best case scenario) crash:

```c
FIOBJ ary = fiobj_ary_new();
FIOBJ ary2 = fiobj_ary_new();
// cyclic nesting
fiobj_ary_push(ary, ary2);
fiobj_ary_push(ary2, ary);
// free might crash or produce unexpected results
fiobj_free(ary);
// each2 will cycle forever
fiobj_each2(ary2, ...);
```

## Independence

The `FIOBJ` module is independent and can be extracted from `facil.io` by copying the `fiobj.h` file (under `lib/facil/core/types`) and all the files in the `lib/facil/core/types/fiobj` folder.

Place these files in your project and use to your heart's content.

The module is licensed under the same MIT license offered by the rest of the `facil.io` source code.
