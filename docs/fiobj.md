# Dynamic Type System: `fiobj_*`

`facil.io` offers a dynamic type system that makes it a breeze to mix object types together.

This dynamic type system is an independent module within the `facil.io` core and can be used separately.

### The Problem

C doesn't lend itself easily to the dynamic types that are often used in languages such as Javascript. This makes it harder to use an optimized C backend (server) when the frontend (client / browser) expects multi-type responses such as JSON objects.

Often this is resolved using the `void` pointer while maintaining a system of type expectations that safeguards against type mismatching. However, typecasting into different types can be dangerous when the types don't match.

Having a local application crash at Runtime is bad. But having a server crash when mishandling a response is arguably worse.

### The Solution

`facil.io` offers the static `fiobj_s` type object. This type contains only a single public data member - it's actual type (using an `enum`).

This offers the following advantages (among others):

* Saves you precious development time.

* Using `switch` statements will warn about unhandled cases and offer a strong type mismatching protection (this is achieved by having a static type system).

* Allows deep integration with `facil.io` services, reducing the need to translate from one type to another.

* Allows for "typeless" actions, such as collection iteration (`fiobj_each2`), simple conversion (`fiobj_obj2num` and `fiobj_obj2cstr`), deallocation (`fiobj_free`). reference counting (`fiobj_dup`) and equality checks (`fiobj_iseq`).

* Offers non-recursive iteration and an *optional* (disabled by default) cyclic nesting protection.

* Offers JSON parsing and formatting to and from `fiobj_s *`.

## API Considerations

This is a short summery regarding the API and it's use. The `fiobj_*` API is well documented in the header files, so only main guidelines are mentioned.

### Functional Access

All object access should be functional, except for type testing. Although this requirement can be circumvented, using the functional interface should be preferred.

For example:

```c
/* this will work */
fiobj_s * str = fiobj_str_buf(7); /* add 1 for NUL terminator */
fio_cstr_s raw_str = fiobj_obj2cstr(str);
memcpy(raw_str.buffer, "Hello!", 6);
fiobj_str_resize(str, 6);
// ...
fiobj_free(str);

/* this is better */
fiobj_s * str = fiobj_str_buf(7); /* add 1 for NUL terminator */
fiobj_str_write(str, "Hello!", 6);
// ...
fiobj_free(str);

/* for simple strings, this is the best */
fiobj_s * str = fiobj_str_new("Hello!", 6);
// ...
fiobj_free(str);

/* for more complex cases, printf style is supported */
fiobj_s * str = fiobj_str_buf(0);
fiobj_str_write2(str, "%s %d" , "Hello!", 42);
// ...
fiobj_free(str);
```

### Ownership Follows Nesting

An object's memory should *always* be managed by it's "owner". This usually means the calling function.

*However*, when an object is nested within another object (i.e., placed in an Array or a Hash), **the ownership of the object is transferred**.

In the following example, the String nested within the Array is freed when the Array is freed:

```c
fiobj_s * ary = fiobj_ary_new();
fiobj_s * str = fiobj_str_new("Hello!", 6);
fiobj_ary_push(ary, str);
// ...
fiobj_free(ary);
```
Hashes follow the same rule. However...

It's important to note that Symbol objects (Hash keys) aren't transferred to the Hash (they are used to access and store data, but they are not the data itself).

When calling `fiobj_hash_set`, we are storing a *value* in the Hash, the key is what we use to access that value. This is why **the key's ownership remains with the calling function**. i.e.:

```c
fiobj_s * h = fiobj_hash_new();
static __thread fiobj_s * ID = NULL;
if(!ID)
  ID = fiobj_sym_new("id", 2);
/* By placing the Number in the Hash, it will be deallocated together with the Hash */
fiobj_hash_set(h, ID, fiobj_num_new(42));
// ...
fiobj_free(h); /* Although we free the Hash, the ID remains in the memory */

if(0) {
  // I assume ID will be reused, but if it's temporary, we need to free it
  fiobj_free(ID);
  ID = NULL;
}
```

### Passing By Reference

All objects are passed along by reference. The `dup` (duplication) process simply increases the reference count.

This is a very powerful tool. In the following example, `str2` is a "copy" **by reference** of `str`. By editing `str2` we're also editing `str`:

```c
fiobj_s * str = fiobj_str_new("Hello!", 6);
fiobj_s * str2 = fiobj_dup(str);
/* We'll edit str2 to say "Hello There!" instead of "Hello!" */
fiobj_str_resize(str2, 5);
fiobj_str_write(str2, " There!", 7);
/* This prints "Hello There!" because str was edited by reference! */
printf("%s\n", fiobj_obj2cstr(str).data);
/* we need to free both references to free the memory */
fiobj_free(str);
fiobj_free(str2);
```

If you need a fresh copy, simply create a new object instead of duplicating the old one:

```c
fiobj_s * str = fiobj_str_new("Hello!", 6);
/* create a copy instead of a reference */
fiobj_s * str2 = fiobj_str_copy(str);
/* this is the same as */
fiobj_s * str3 = fiobj_str_new(fiobj_obj2cstr(str).data, fiobj_obj2cstr(str).len);
// ...
fiobj_free(str);
fiobj_free(str2);
fiobj_free(str3);
```

Copy by reference produces a deep reference adjustment, so Arrays and Hashes can be safely copied by reference.

```c
fiobj_s * ary = fiobj_ary_new();
fiobj_ary_push(ary, fiobj_str_new("Hello!", 6));
fiobj_s * ary_copy = fiobj_dup(ary);
// ...
fiobj_free(ary);
// all the items in ary2 are still accessible.
fprintf(stderr, "%s\n", fiobj_obj2cstr( fiobj_ary_entry(ary_copy, -1) ).buffer );
fiobj_free(ary_copy);
```

### Optional Cyclic Nesting Protection

Cyclic protection is disabled by default due to performance concerns. Consider that the the protection layer must keep a list of any Hash or Array processed and test each Array and Hash to see if they were processed before.

To enable the optional cyclic nesting protection, `FIOBJ_NESTING_PROTECTION` must be defined during compile time.

i.e., add `-DFIOBJ_NESTING_PROTECTION` to the compiler flags.

Optionally, you could edit the `fiobj.h` file, but that is less recommended, since updated might overwrite the edit.

Without the optional cyclic nesting protection, the following code will crash:

```c
// FIOBJ_NESTING_PROTECTION == 0 or not defined
fiobj_s * ary = fiobj_ary_new();
fiobj_s * ary2 = fiobj_ary_new();
// cyclic nesting
fiobj_ary_push(ary, ary2);
fiobj_ary_push(ary2, ary);
// free will crash
fiobj_free(ary);
// dup and each2 will cycle forever
fiobj_s * ary3 = fiobj_dup(ary2);
```

However, enabling the optional cyclic nesting protection will protect against cyclic nesting issues:

```c
// FIOBJ_NESTING_PROTECTION == 1
fiobj_s * ary = fiobj_ary_new();
fiobj_s * ary2 = fiobj_ary_new();
// cyclic nesting
fiobj_ary_push(ary, ary2);
fiobj_ary_push(ary2, ary);
// dup and each2 will safely skip cyclic objects
fiobj_s * ary_copy = fiobj_dup(ary2);
// both arrays, that "own" each other, are freed
fiobj_free(ary_copy);
fiobj_free(ary);
```

## Independence

The `fiobj_s` module is independent and can be extracted from `facil.io` by copying the `fiobj.h` file (under `lib/facil/core/types`) and all the files in the `lib/facil/core/types/fiobj` folder.

Place these files in your project and use to your heart's content.

The module is licensed under the same MIT license offered by the rest of the `facil.io` source code.
