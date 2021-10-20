---
title: facil.io - C STL - a Simple Template Library for C
sidebar: 0.8.x/_sidebar.md
---
# facil.io - C STL - a Simple Template Library for C

At the core of the [facil.io library](https://facil.io) is its powerful Simple Template Library for C (and C++).

The Simple Template Library is a "swiss-army-knife" library, that uses MACROS to generate code for different common types, such as Hash Maps, Arrays, Linked Lists, Binary-Safe Strings, etc'.

The Simple Template Library also offers common functional primitives and helpers, such as bit operations, atomic operations, CLI parsing, JSON, task queues, and a custom memory allocator.

In other words, all the common building blocks one could need in a C project are placed in this single header file.

The header could be included multiple times with different results, creating different types or exposing different functionality.

**Note**: facil.io Web Application Developers get many of the features of the C STL through including the `fio.h` header. See the [facil.io IO Core documentation](fio) for more information.

## OS Support

The library in written and tested on POSIX systems. Windows support was added afterwards, leaving the library with a POSIX oriented design.

Please note I cannot continually test the windows support as I avoid the OS... hence, Windows OS support should be considered unstable.

## Simple Template Library (STL) Overview

The core Simple Template Library (STL) is a single file header library (`fio-stl.h`).


The [testable](#testing-the-library-fio_test_cstl) header library includes a Simple Template Library for the following common types:

* [Linked Lists](#linked-lists) - defined by `FIO_LIST_NAME`

* [Dynamic Arrays](#dynamic-arrays) - defined by `FIO_ARRAY_NAME`

* [Hash Maps / Sets](#hash-tables-and-maps) - defined by `FIO_MAP_NAME`

* [Binary Safe Dynamic Strings](#dynamic-strings) - defined by `FIO_STR_NAME` / `FIO_STR_SMALL`

* [Reference counting / Type wrapper](#reference-counting-and-type-wrapping) - defined by `FIO_REF_NAME`

* [Soft / Dynamic Types (FIOBJ)](#fiobj-soft-dynamic-types) - defined by `FIO_FIOBJ`


In addition, the core Simple Template Library (STL) includes helpers for common tasks, such as:

* [Pointer Arithmetics](#pointer-arithmetics) (included by default)

* [Pointer Tagging](#pointer-tagging-support) - defined by `FIO_PTR_TAG(p)`/`FIO_PTR_UNTAG(p)`

* [Logging and Assertion (without heap allocation)](#logging-and-assertions) - defined by `FIO_LOG`

* [Atomic operations](#atomic-operations) - defined by `FIO_ATOMIC`

* [Bit-Byte Operations](#bit-byte-operations) - defined by `FIO_BITWISE`

* [Bitmap helpers](#bitmap-helpers) - defined by `FIO_BITMAP`

* [Glob Matching](#globe-matching) - defined by `FIO_GLOB_MATCH`

* [Data Hashing (using Risky Hash)](#risky-hash-data-hashing) - defined by `FIO_RISKY_HASH`

* [Pseudo Random Generation](#pseudo-random-generation) - defined by `FIO_RAND`

* [String / Number conversion](#string-number-conversion) - defined by `FIO_ATOL`

* [Time Helpers](#time-helpers) - defined by `FIO_TIME`

* [Task Queues and Timers](#task-queue) - defined by `FIO_QUEUE`

* [Thread Portability Helpers](#threads-portable) - defined by `FIO_THREADS`

* [File Utility Helpers](#file-utility-helpers) - defined by `FIO_FILES`

* [Command Line Interface helpers](#cli-command-line-interface) - defined by `FIO_CLI`

* [URL (URI) parsing](#url-uri-parsing) - defined by `FIO_URL`

* [Custom JSON Parser](#custom-json-parser) - defined by `FIO_JSON`

* [Basic Socket / IO Helpers](#basic-socket-io-helpers) - defined by `FIO_SOCK`

* [Data Stream Containers](#data-stream-container) - defined by `FIO_STREAM`

* [Polling with `poll`](#basic-io-polling) - defined by `FIO_POLL`

* [Signal (pass-through) Monitoring](#signal-monitoring) - defined by `FIO_SIGNAL`

* [Local Memory Allocation](#local-memory-allocation) - defined by `FIO_MEMORY` / `FIO_MALLOC`

### Compilation Modes

The Simple Template Library types and functions could be compiled as either static or extern ("global"), either limiting their scope to a single C file (compilation unit) or exposing them throughout the program.

#### Static Functions by Default

By default, the Simple Template Library will generate static functions where possible.

To change this behavior, `FIO_EXTERN` and `FIO_EXTERN_COMPLETE` could be used to generate externally visible code.

#### `FIO_EXTERN`

If defined, the the Simple Template Library will generate non-static code.

If `FIO_EXTERN` is defined alone, only function declarations and inline functions will be generated.

If `FIO_EXTERN_COMPLETE` is defined, the function definition (the implementation code) will also be generated.

**Note**: the `FIO_EXTERN` will be **automatically undefined** each time the Simple Template Library header is included.

For example, in the header (i.e., `mymem.h`), use:

```c
#define FIO_EXTERN
#define FIO_MALLOC
#include "fio-stl.h"
```

Later, in the implementation file, use:

```c
#define FIO_EXTERN_COMPLETE 1
#include "mymem.h"
#undef FIO_EXTERN_COMPLETE
```

#### `FIO_EXTERN_COMPLETE`

When defined, this macro will force full code generation.

If `FIO_EXTERN_COMPLETE` is set to the value `2`, it will automatically self-destruct (it will undefine itself once used).

#### `FIO_USE_THREAD_MUTEX` and `FIO_USE_THREAD_MUTEX_TMP`

Some modules require thread safety locks, such as the timer module, queue module, memory allocator and socket polling. The facil.io library will default to it's own spin-lock based implementation.

This default choice can be changed so facil.io uses the OS's native `mutex` type (`pthread_mutex_t` on POSIX systems) by setting the `FIO_USE_THREAD_MUTEX` or `FIO_USE_THREAD_MUTEX_TMP` to true (`1`).

The `FIO_USE_THREAD_MUTEX_TMP` macro will alter the default behavior for only a single include statement.

The `FIO_USE_THREAD_MUTEX` macro will alter the default behavior for all future include statements.

-------------------------------------------------------------------------------

## Testing the Library (`FIO_TEST_CSTL`)

To test the library, define the `FIO_TEST_CSTL` macro and include the header. A testing function called `fio_test_dynamic_types` will be defined. Call that function in your code to test the library.

#### `FIO_TEST_CSTL`

Defined the `fio_test_dynamic_types` and enables as many testing features as possible, such as the `FIO_LEAK_COUNTER`.

#### `FIO_LEAK_COUNTER`

Counts allocations and deallocations for custom memory allocators, allowing memory leaks to be detected with certainty.

This also prints out some minimal usage information about each allocator when exiting the program. 

-------------------------------------------------------------------------------

## Version and Common Helper Macros

The facil.io C STL (Simple Template Library) offers a number of common helper macros that are also used internally. These are automatically included once the `fio-stl.h` is included.

### Version Macros

The facil.io C STL library follows [semantic versioning](https://semver.org) and supports macros that will help detect and validate it's version.

#### `FIO_VERSION_MAJOR`

Translates to the STL's major version number.

MAJOR version upgrades require a code review and possibly significant changes. Even functions with the same name might change their behavior.

#### `FIO_VERSION_MINOR`

Translates to the STL's minor version number.

Please review your code before adopting a MINOR version upgrade.

#### `FIO_VERSION_PATCH`

Translates to the STL's patch version number.

PATCH versions should be adopted as soon as possible (they contain bug fixes).

#### `FIO_VERSION_BUILD`

Translates to the STL's build version **string** (i.e., `"beta.1"`), if any.

#### `FIO_VERSION_STRING`

Translates to the STL's version as a string (i.e., `"0.8.0-beta.1"`).

#### `FIO_VERSION_GUARD`

If the `FIO_VERSION_GUARD` macro is defined in **a single** translation unit (C file) **before** including `fio-stl.h` for the first time, then the version macros become available using functions as well: `fio_version_major`, `fio_version_minor`, etc'.

#### `FIO_VERSION_VALIDATE`

By adding the `FIO_VERSION_GUARD` functions, a version test could be performed during runtime (which can be used for static libraries), using the macro `FIO_VERSION_VALIDATE()`.

**Note**: the `FIO_VERSION_VALIDATE()` macro does not test build versions, only API compatibility (Major and Minor and Patch versions during development and Major and Minor versions after a 1.x release).

-------------------------------------------------------------------------------

### Pointer Arithmetics

#### `FIO_PTR_MATH_LMASK`

```c
#define FIO_PTR_MATH_LMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & (((uintptr_t)1 << (bits)) - 1)))
```

Masks a pointer's left-most bits, returning the right bits (i.e., `0x000000FF`).

#### `FIO_PTR_MATH_RMASK`

```c
#define FIO_PTR_MATH_RMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & ((~(uintptr_t)0) << bits)))
```

Masks a pointer's right-most bits, returning the left bits (i.e., `0xFFFFFF00`).

#### `FIO_PTR_MATH_ADD`

```c
#define FIO_PTR_MATH_ADD(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) + (uintptr_t)(offset)))
```

Add offset bytes to pointer's address, updating the pointer's type.

#### `FIO_PTR_MATH_SUB`

```c
#define FIO_PTR_MATH_SUB(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) - (uintptr_t)(offset)))
```

Subtract X bytes from pointer's address, updating the pointer's type.

#### `FIO_PTR_FROM_FIELD`

```c
#define FIO_PTR_FROM_FIELD(T_type, field, ptr)                                 \
  FIO_PTR_MATH_SUB(T_type, ptr, (&((T_type *)0)->field))
```

Find the root object (of a `struct`) from a pointer to its field's (the field's address).

-------------------------------------------------------------------------------

### Default Memory Allocation

By setting these macros, the memory allocator used by facil.io could be changed from the default allocator (either the custom allocator or, if missing, the system's allocator).

When facil.io's memory allocator is defined (using `FIO_MALLOC`), **these macros will be automatically overwritten to use the custom memory allocator**. To use a different allocator, you may redefine the macros.

#### `FIO_MEM_REALLOC`

```c
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len) realloc((ptr), (new_size))
```

Reallocates memory, copying (at least) `copy_len` if necessary.

If `ptr` is `NULL`, behaves like `malloc`.

If `new_size` is 0, behaves like `free`.

#### `FIO_MEM_FREE`

```c
#define FIO_MEM_FREE(ptr, size) free((ptr))
```

Frees allocated memory.

#### `FIO_MALLOC_TMP_USE_SYSTEM`

When defined, temporarily bypasses the `FIO_MEM_REALLOC` macros and uses the system's `realloc` and `free` functions for newly created types.

#### `FIO_MEMORY_DISABLE`

When `FIO_MEMORY_DISABLE` is defined, all (future) custom memory allocators will route to the system's `malloc`. Set this when compiling to test the effects of all custom memory allocators working together.

-------------------------------------------------------------------------------

## Pointer Tagging Support:

Pointer tagging allows types created using this library to have their pointers "tagged".

This is when creating / managing dynamic types, where some type data could be written to the pointer data itself.

**Note**: pointer tagging can't automatically tag "pointers" to objects placed on the stack.

#### `FIO_PTR_TAG`

Supports embedded pointer tagging / untagging for the included types.

Should resolve to a tagged pointer value. i.e.: `((uintptr_t)(p) | 1)`

#### `FIO_PTR_UNTAG`

Supports embedded pointer tagging / untagging for the included types.

Should resolve to an untagged pointer value. i.e.: `((uintptr_t)(p) | ~1UL)`

**Note**: `FIO_PTR_UNTAG` might be called more then once or on untagged pointers. For this reason, `FIO_PTR_UNTAG` should always return the valid pointer, even if called on an untagged pointer.

#### `FIO_PTR_TAG_TYPE`

If the FIO_PTR_TAG_TYPE is defined, then functions returning a type's pointer will return a pointer of the specified type instead.

-------------------------------------------------------------------------------

## Binary Data Informational Types and Helpers

Some informational types and helpers are always defined (similarly to the [Linked Lists Macros](#linked-lists-macros)). These include:

#### `fio_str_info_s`

Some functions use the `fio_str_info_s` type to either collect or return string related information. This helper type is defined as:

```c
typedef struct fio_str_info_s {
  char *buf;   /* The string's buffer (pointer to first byte) or NULL on error. */
  size_t len;  /* The string's length, if any. */
  size_t capa; /* The buffer's capacity. Zero (0) indicates the buffer is read-only. */
} fio_str_info_s;
```

Note that it is often the case that the data in the string object could be binary, where NUL is a valid character, so using C string functions isn't advised.

Also, note that `capa` might be `0` or otherwise less than `len`. This would indicate the data might be non-mutable (overwriting the string might break things).


#### `fio_buf_info_s`

```c
typedef struct fio_buf_info_s {
  char *buf;  /* The buffer's address (may be NULL if no buffer). */
  size_t len; /* The buffer's length, if any. */
} fio_buf_info_s;
```

An information type for reporting/storing buffer data (no `capa`). Note that the buffer may contain binary data and is **not** likely to be NUL terminated.

#### `FIO_STR_INFO_IS_EQ`

```c
#define FIO_STR_INFO_IS_EQ(s1, s2)                                             \
  ((s1).len == (s2).len && (!(s1).len || (s1).buf == (s2).buf ||               \
                            !memcmp((s1).buf, (s2).buf, (s1).len)))
```

This helper MACRO compares two `fio_str_info_s` / `fio_buf_info_s` objects for content content equality.

#### `FIO_STR_INFO1`

```c
#define FIO_STR_INFO1(str) ((fio_str_info_s){(str), strlen((str))})
```

Converts a C String into a `fio_str_info_s`.

#### `FIO_STR_INFO2`

```c
#define FIO_STR_INFO2(str, length) ((fio_str_info_s){(str), (length)})
```

Converts a String with a known length into a `fio_str_info_s`.

#### `FIO_STR_INFO3`

```c
#define FIO_STR_INFO3(str, length, capacity) ((fio_str_info_s){(str), (length), (capacity)})
```

Converts a String with a known length and capacity into a `fio_str_info_s`.

#### `FIO_BUF2STR_INFO`

```c
#define FIO_BUF2STR_INFO(buf_info) ((fio_str_info_s){(buf_info).buf, (buf_info).len})
```

Converts a `fio_buf_info_s` into a `fio_str_info_s`.

#### `FIO_STR2BUF_INFO`

```c
#define FIO_STR2BUF_INFO(str_info) ((fio_buf_info_s){(str_info).buf, (str_info).len})
```

Converts a `fio_buf_info_s` into a `fio_str_info_s`.

-------------------------------------------------------------------------------

## Naming and Misc. Macros

#### `FIO_IFUNC`

```c
#define FIO_IFUNC static inline __attribute__((unused))
```

Marks a function as `static`, `inline` and possibly unused.

#### `FIO_SFUNC`

```c
#define FIO_SFUNC static __attribute__((unused))
```

Marks a function as `static` and possibly unused.

#### `FIO_WEAK`

```c
#define FIO_WEAK __attribute__((weak))
```

Marks a function as weak

#### `FIO_CONSTRUCTOR(fname)`

```c
#define FIO_CONSTRUCTOR(fname) FIO_SFUNC __attribute__((constructor)) void fname (void)
```

Marks a function as a _constructor_ - **if supported**.

When supported by the compiler (i.e., `gcc` / `clang`), this function will execute when the library is loaded or, if statically linked, before `main` is called.

#### `FIO_DESTRUCTOR(fname)`

```c
#define FIO_DESTRUCTOR(fname) FIO_SFUNC __attribute__((destructor)) void fname (void)
```

Marks a function as a _destructor_ - **if supported**.

When supported by the compiler (i.e., `gcc` / `clang`), this function will execute when the library is loaded or, if statically linked, after `main` returns.

#### `FIO_MEMCPY`

```c
#define FIO_MEMCPY memcpy // or __builtin_memcpy if unavailable
```

Makes it easy to override `memcpy` implementations.

#### `FIO_MACRO2STR`

```c
#define FIO_MACRO2STR(macro) FIO_MACRO2STR_STEP2(macro)
```

Converts a macro's content to a string literal.

#### `FIO_NAME`

```c
#define FIO_NAME(prefix, postfix)
```

Used for naming functions and variables resulting in: `prefix_postfix`

This allows macros to be used for naming types and functions.

i.e.:

```c
// the type's name
#define NUM number
// typedef struct { long l; } number_s
typedef struct { long l; } FIO_NAME(NUM, s)

// number_s number_add(number_s a, number_s b)
FIO_NAME(NUM, s) FIO_NAME(NUM, add)(FIO_NAME(NUM, s) a, FIO_NAME(NUM, s) b) {
  a.l += b.l;
  return a;
}
#undef NUM
```

#### `FIO_NAME2`

```c
#define FIO_NAME2(prefix, postfix)
```

Sets naming convention for conversion functions, i.e.: foo2bar

i.e.:

```c
// int64_t a2l(const char * buf)
int64_t FIO_NAME2(a, l)(const char * buf) {
  return fio_atol(&buf);
}
```

#### `FIO_NAME_BL`

```c
#define FIO_NAME_BL(prefix, postfix) 
```

Sets naming convention for boolean functions, i.e.: foo_is_true

i.e.:

```c
// typedef struct { long l; } number_s
typedef struct { long l; } FIO_NAME(number, s)

// int number_is_zero(number_s n)
int FIO_NAME2(number, zero)(FIO_NAME(number, s) n) {
  return (!n.l);
}
```

#### `FIO_NAME_TEST`

```c
#define FIO_NAME_TEST(prefix, postfix) FIO_NAME(fio___test, FIO_NAME(prefix, postfix))
```

Used internally to name test functions.

-------------------------------------------------------------------------------
## Logging and Assertions:

If the `FIO_LOG_LENGTH_LIMIT` macro is defined (it's recommended that it be greater than 128), than the `FIO_LOG2STDERR` (weak) function and the `FIO_LOG2STDERR2` macro will be defined.

#### `FIO_LOG_LEVEL`

An application wide integer with a value of either:

- `FIO_LOG_LEVEL_NONE` (0)
- `FIO_LOG_LEVEL_FATAL` (1)
- `FIO_LOG_LEVEL_ERROR` (2)
- `FIO_LOG_LEVEL_WARNING` (3)
- `FIO_LOG_LEVEL_INFO` (4)
- `FIO_LOG_LEVEL_DEBUG` (5)

The initial value can be set using the `FIO_LOG_LEVEL_DEFAULT` macro. By default, the level is 4 (`FIO_LOG_LEVEL_INFO`) for normal compilation and 5 (`FIO_LOG_LEVEL_DEBUG`) for DEBUG compilation.

**Note**: in **all** of the following `msg` **must** be a string literal (`const char *`).

#### `FIO_LOG2STDERR(msg, ...)`

This `printf` style **function** will log a message to `stderr`, without allocating any memory on the heap for the string (`fprintf` might).

The function is defined as `weak`, allowing it to be overridden during the linking stage, so logging could be diverted... although, it's recommended to divert `stderr` rather then the logging function.

#### `FIO_LOG2STDERR2(msg, ...)`

This macro routs to the `FIO_LOG2STDERR` function after prefixing the message with the file name and line number in which the error occurred.

#### `FIO_LOG_FATAL(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_FATAL`.

#### `FIO_LOG_ERROR(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_ERROR`.

#### `FIO_LOG_SECURITY(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_ERROR`.

#### `FIO_LOG_WARNING(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_WARNING`.

#### `FIO_LOG_INFO(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_INFO`.

#### `FIO_LOG_DEBUG(msg, ...)`

Logs `msg` **if** log level is equal or above requested log level of `FIO_LOG_LEVEL_DEBUG`.

#### `FIO_LOG_DDEBUG(msg, ...)`

Same as `FIO_LOG_DEBUG` if `DEBUG` was defined. Otherwise a no-op.

#### `FIO_ASSERT(cond, msg, ...)`

Reports an error unless condition is met, printing out `msg` using `FIO_LOG_FATAL` and exiting the application using `SIGINT` followed by an exit(-1)`.

The use of `SIGINT` should allow debuggers everywhere to pause execution before exiting the program.

#### `FIO_ASSERT_ALLOC(ptr)`

Reports a failure to allocate memory, exiting the program the same way as `FIO_ASSERT`.

#### `FIO_ASSERT_DEBUG(cond, msg, ...)`

Ignored unless `DEBUG` is defined.

Reports an error unless condition is met, printing out `msg` using `FIO_LOG_FATAL` and aborting (not exiting) the application.

Note, this macro will **only** raise a `SIGINT` signal, but will not exit the program. This is designed to allow debuggers to catch these occurrences and continue execution when possible.

-------------------------------------------------------------------------------
## Atomic operations:

If the `FIO_ATOMIC` macro is defined than the following macros will be defined.

In general, when a function returns a value, it is always the previous value - unless the function name ends with `fetch` or `load`.

#### `fio_atomic_load(p_obj)`

Atomically loads and returns the value stored in the object pointed to by `p_obj`.

#### `fio_atomic_exchange(p_obj, value)`

Atomically sets the object pointer to by `p_obj` to `value`, returning the
previous value.

#### `fio_atomic_add(p_obj, value)`

A MACRO / function that performs `add` atomically.

Returns the previous value.

#### `fio_atomic_sub(p_obj, value)`

A MACRO / function that performs `sub` atomically.

Returns the previous value.

#### `fio_atomic_and(p_obj, value)`

A MACRO / function that performs `and` atomically.

Returns the previous value.

#### `fio_atomic_xor(p_obj, value)`

A MACRO / function that performs `xor` atomically.

Returns the previous value.

#### `fio_atomic_or(p_obj, value)`

A MACRO / function that performs `or` atomically.

Returns the previous value.

#### `fio_atomic_nand(p_obj, value)`

A MACRO / function that performs `nand` atomically.

Returns the previous value.

#### `fio_atomic_add_fetch(p_obj, value)`

A MACRO / function that performs `add` atomically.

Returns the new value.

#### `fio_atomic_sub_fetch(p_obj, value)`

A MACRO / function that performs `sub` atomically.

Returns the new value.

#### `fio_atomic_and_fetch(p_obj, value)`

A MACRO / function that performs `and` atomically.

Returns the new value.

#### `fio_atomic_xor_fetch(p_obj, value)`

A MACRO / function that performs `xor` atomically.

Returns the new value.

#### `fio_atomic_or_fetch(p_obj, value)`

A MACRO / function that performs `or` atomically.

Returns the new value.

#### `fio_atomic_nand_fetch(p_obj, value)`

A MACRO / function that performs `nand` atomically.

Returns the new value.

#### `fio_atomic_compare_exchange_p(p_obj, p_expected, p_desired)`

A MACRO / function that performs a system specific `fio_atomic_compare_exchange` using pointers.

The behavior of this instruction is compiler / CPU architecture specific, where `p_expected` **SHOULD** be overwritten with the latest value of `p_obj`, but **MAY NOT**, depending on system and compiler implementations.

Returns 1 for successful exchange or 0 for failure.

### a SpinLock style MultiLock

Atomic operations lend themselves easily to implementing spinlocks, so the facil.io STL includes one whenever atomic operations are defined (`FIO_ATOMIC`).

Spinlocks are effective for very short critical sections or when a a failure to acquire a lock allows the program to redirect itself to other pending tasks. 

However, in general, spinlocks should be avoided when a task might take a longer time to complete or when the program might need to wait for a high contention lock to become available.

#### `fio_lock_i`

A spinlock type based on a volatile unsigned char.

**Note**: the spinlock contains one main / default lock (`sub == 0`) and 7 sub-locks (`sub >= 1 && sub <= 7`), which could be managed:

- Separately: using the `fio_lock_sublock`, `fio_trylock_sublock` and `fio_unlock_sublock` functions.
- Jointly: using the `fio_trylock_group`, `fio_lock_group` and `fio_unlock_group` functions.
- Collectively: using the `fio_trylock_full`, `fio_lock_full` and `fio_unlock_full` functions.


#### `fio_lock(fio_lock_i *)`

Busy waits for the default lock (sub-lock `0`) to become available.

#### `fio_trylock(fio_lock_i *)`

Attempts to acquire the default lock (sub-lock `0`). Returns 0 on success and 1 on failure.

#### `fio_unlock(fio_lock_i *)`

Unlocks the default lock (sub-lock `0`), no matter which thread owns the lock.

#### `fio_is_locked(fio_lock_i *)`

Returns 1 if the (main) lock is engaged. Otherwise returns 0.

#### `fio_lock_sublock(fio_lock_i *, uint8_t sub)`

Busy waits for a sub-lock to become available.

#### `fio_trylock_sublock(fio_lock_i *, uint8_t sub)`

Attempts to acquire the sub-lock. Returns 0 on success and 1 on failure.

#### `fio_unlock_sublock(fio_lock_i *, uint8_t sub)`

Unlocks the sub-lock, no matter which thread owns the lock.

#### `fio_is_sublocked(fio_lock_i *, uint8_t sub)`

Returns 1 if the specified sub-lock is engaged. Otherwise returns 0.

#### `uint8_t fio_trylock_group(fio_lock_i *lock, const uint8_t group)`

Tries to lock a group of sub-locks.

Combine a number of sub-locks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
macro. i.e.:

```c
if(fio_trylock_group(&lock,
                     FIO_LOCK_SUBLOCK(1) |
                     FIO_LOCK_SUBLOCK(2)) == 0) {
  // act in lock and then release the SAME lock with:
  fio_unlock_group(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2));
}
```

Returns 0 on success and 1 on failure.

#### `void fio_lock_group(fio_lock_i *lock, uint8_t group)`

Busy waits for a group lock to become available - not recommended.

See `fio_trylock_group` for details.

#### `void fio_unlock_group(fio_lock_i *lock, uint8_t group)`

Unlocks a sub-lock group, no matter which thread owns which sub-lock.

#### `fio_trylock_full(fio_lock_i *lock)`

Tries to lock all sub-locks. Returns 0 on success and 1 on failure.

#### `fio_lock_full(fio_lock_i *lock)`

Busy waits for all sub-locks to become available - not recommended.

#### `fio_unlock_full(fio_lock_i *lock)`

Unlocks all sub-locks, no matter which thread owns which lock.

-------------------------------------------------------------------------------

## MultiLock with Thread Suspension

If the `FIO_LOCK2` macro is defined than the multi-lock `fio_lock2_s` type and it's functions will be defined.

The `fio_lock2` locking mechanism follows a bitwise approach to multi-locking, allowing a single lock to contain up to 31 sublocks (on 32 bit machines) or 63 sublocks (on 64 bit machines).

This is a very powerful tool that allows simultaneous locking of multiple sublocks (similar to `fio_trylock_group`) while also supporting a thread "waitlist" where paused threads await their turn to access the lock and enter the critical section.

The default implementation uses `pthread` (POSIX Threads) to access the thread's "ID", pause the thread (using `sigwait`) and resume the thread (with `pthread_kill`).

The default behavior can be controlled using the following MACROS:

* the `FIO_THREAD_T` macro should return a thread type, default: `pthread_t`

* the `FIO_THREAD_ID()` macro should return this thread's FIO_THREAD_T.

* the `FIO_THREAD_PAUSE(id)` macro should temporarily pause thread execution.

* the `FIO_THREAD_RESUME(id)` macro should resume thread execution.

#### `fio_lock2_s`

```c
typedef struct {
  volatile size_t lock;
  fio___lock2_wait_s *waiting; /**/
} fio_lock2_s;
```

The `fio_lock2_s` type **must be considered opaque** and the struct's fields should **never** be accessed directly.

The `fio_lock2_s` type is the lock's type.

#### `fio_trylock2`

```c
uint8_t fio_trylock2(fio_lock2_s *lock, size_t group);
```

Tries to lock a multilock.

Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
macro. i.e.:

```c
if(!fio_trylock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2))) {
  // act in lock
  fio_unlock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2));
}
```

Returns 0 on success and non-zero on failure.

#### `fio_lock2`

```c
void fio_lock2(fio_lock2_s *lock, size_t group);
```

Locks a multilock, waiting as needed.

Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
macro. i.e.:

     fio_lock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2)));

Doesn't return until a successful lock was acquired.

#### `fio_unlock2`

```c
void fio_unlock2(fio_lock2_s *lock, size_t group);
```

Unlocks a multilock, regardless of who owns the locked group.

Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
macro. i.e.:

```c
fio_unlock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2));
````

-------------------------------------------------------------------------------
## Bit-Byte operations:

If the `FIO_BITWISE` macro is defined than the following macros will be
defined:

**Note**: the 128 bit helpers are only available with systems / compilers that support 128 bit types.

#### Byte Swapping

Returns a number of the indicated type with it's byte representation swapped.

- `fio_bswap16(i)`
- `fio_bswap32(i)`
- `fio_bswap64(i)`
- `fio_bswap128(i)`

#### Bit rotation (left / right)

Returns a number with it's bits left rotated (`lrot`) or right rotated (`rrot`) according to the type width specified (i.e., `fio_rrot64` indicates a **r**ight rotation for `uint64_t`).

- `fio_lrot8(i, bits)`
- `fio_rrot8(i, bits)`
- `fio_lrot16(i, bits)`
- `fio_rrot16(i, bits)`
- `fio_lrot32(i, bits)`
- `fio_rrot32(i, bits)`
- `fio_lrot64(i, bits)`
- `fio_rrot64(i, bits)`
- `fio_lrot128(i, bits)`
- `fio_rrot128(i, bits)`

- `FIO_LROT(i, bits)` (MACRO, can be used with any type size)
- `FIO_RROT(i, bits)` (MACRO, can be used with any type size)

#### Numbers to Numbers (network ordered)

On big-endian systems, these macros a NOOPs, whereas on little-endian systems these macros flip the byte order.

- `fio_lton16(i)`
- `fio_ntol16(i)`
- `fio_lton32(i)`
- `fio_ntol32(i)`
- `fio_lton64(i)`
- `fio_ntol64(i)`
- `fio_lton128(i)`
- `fio_ntol128(i)`

#### Bytes to Numbers (native / reversed / network ordered)

Reads a number from an unaligned memory buffer. The number or bits read from the buffer is indicated by the name of the function.

**Big Endian (default)**:

- `fio_buf2u16(buffer)`
- `fio_buf2u32(buffer)`
- `fio_buf2u64(buffer)`
- `fio_buf2u128(buffer)`

**Little Endian**:

- `fio_buf2u16_little(buffer)`
- `fio_buf2u32_little(buffer)`
- `fio_buf2u64_little(buffer)`
- `fio_buf2u128_little(buffer)`

**Native Byte Order**:

- `fio_buf2u16_local(buffer)`
- `fio_buf2u32_local(buffer)`
- `fio_buf2u64_local(buffer)`
- `fio_buf2u128_local(buffer)`

**Reversed Byte Order**:

- `fio_buf2u16_bswap(buffer)`
- `fio_buf2u32_bswap(buffer)`
- `fio_buf2u64_bswap(buffer)`
- `fio_buf2u128_bswap(buffer)`

#### Numbers to Bytes (native / reversed / network ordered)

Writes a number to an unaligned memory buffer. The number or bits written to the buffer is indicated by the name of the function.

**Big Endian (default)**:

- `fio_u2buf16(buffer, i)`
- `fio_u2buf32(buffer, i)`
- `fio_u2buf64(buffer, i)`
- `fio_u2buf128(buffer, i)`

**Little Endian**:

- `fio_u2buf16_little(buffer, i)`
- `fio_u2buf32_little(buffer, i)`
- `fio_u2buf64_little(buffer, i)`
- `fio_u2buf128_little(buffer, i)`

**Native Byte Order**:

- `fio_u2buf16_local(buffer, i)`
- `fio_u2buf32_local(buffer, i)`
- `fio_u2buf64_local(buffer, i)`
- `fio_u2buf128_local(buffer, i)`

**Reversed Byte Order**:

- `fio_u2buf16_bswap(buffer, i)`
- `fio_u2buf32_bswap(buffer, i)`
- `fio_u2buf64_bswap(buffer, i)`
- `fio_u2buf128_bswap(buffer, i)`

#### Constant Time Bit Operations

Performs the operation indicated in constant time.

- `fio_ct_true(condition)`

    Tests if `condition` is non-zero (returns `1` / `0`).

- `fio_ct_false(condition)`

    Tests if `condition` is zero (returns `1` / `0`).

- `fio_ct_if_bool(bool, a_if_true, b_if_false)`

    Tests if `bool == 1` (returns `a` / `b`).

- `fio_ct_if(condition, a_if_true, b_if_false)`

    Tests if `condition` is non-zero (returns `a` / `b`).

- `fio_ct_max(a, b)`

    Returns `a` if a >= `b` (performs a **signed** comparison).

#### Simulating SIMD instructions


- `fio_has_full_byte32(uint32_t row)`

	Detects a byte where all the bits are set (`255`) within a 4 byte vector.

- `fio_has_zero_byte32(uint32_t row)`

	Detects a byte where no bits are set (0) within a 4 byte vector.

- `fio_has_byte32(uint32_t row, uint8_t byte)`

	Detects if `byte` exists within a 4 byte vector.

- `fio_has_full_byte64(uint64_t row)`

	Detects a byte where all the bits are set (`255`) within an 8 byte vector.

- `fio_has_zero_byte64(uint64_t row)`

	Detects a byte where no bits are set (0) within an 8 byte vector.

- `fio_has_byte64(uint64_t row, uint8_t byte)`

	Detects if `byte` exists within an 8 byte vector.

- `fio_has_full_byte128(__uint128_t row)`

    Detects a byte where all the bits are set (`255`) within an 8 byte vector.

- `fio_has_zero_byte128(__uint128_t row)`

    Detects a byte where no bits are set (0) within an 8 byte vector.

- `fio_has_byte128(__uint128_t row, uint8_t byte)`

    Detects if `byte` exists within an 8 byte vector.

#### `fio_popcount` and Hemming 

```c
int fio_popcount(uint64_t n);
```

Returns the number of set bits in the number `n`.

#### `fio_hemming_dist`

```c
#define fio_hemming_dist(n1, n2) fio_popcount(((uint64_t)(n1) ^ (uint64_t)(n2)))
```

Returns the Hemming Distance between the number `n1` and the number `n2`.

Hemming Distance is the number of bits that need to be "flipped" in order for both numbers to be equal.

#### `fio_xmask`

```c
void fio_xmask(char *buf,
               size_t len,
               uint64_t mask);
```

Masks data using a 64 bit mask.

The function may perform significantly better when the buffer's memory is aligned.

#### `fio_xmask2`

```c
uint64_t fio_xmask2(char *buf,
                    size_t len,
                    uint64_t mask,
                    uint64_t nonce);
```

Masks data using a 64 bit mask and a counter mode nonce.

Returns the end state of the mask.

The function may perform significantly better when the buffer's memory is aligned.

**Note**: this function could be used to obfuscate data in locally stored buffers, mitigating risks such as data leaks that may occur when memory is swapped to disk. However, this function should **never** be used as an alternative to actual encryption.

-------------------------------------------------------------------------------

## Bitmap helpers

If the `FIO_BITMAP` macro is defined than the following macros will be
defined.

In addition, the `FIO_ATOMIC` will be assumed to be defined, as setting bits in
the bitmap is implemented using atomic operations.

#### Bitmap helpers
- `fio_bitmap_get(void *map, size_t bit)`
- `fio_bitmap_set(void *map, size_t bit)`   (an atomic operation, thread-safe)
- `fio_bitmap_unset(void *map, size_t bit)` (an atomic operation, thread-safe)

-------------------------------------------------------------------------------
## Risky Hash (data hashing):

[Risky Hash](./riskyhash) is facil.io's fast hashing solution that balances non-cryptographic security consideration and speed. If the `FIO_RISKY_HASH` macro is defined than the following static function will be defined:

#### `fio_risky_hash`

```c
uint64_t fio_risky_hash(const void *data, size_t len, uint64_t seed)
```

This is a non-streaming implementation of the RiskyHash v.3 algorithm.

This function will produce a 64 bit hash for X bytes of data.

#### `fio_risky_mask`

```c
void fio_risky_mask(char *buf, size_t len, uint64_t key, uint64_t nonce);
```

Masks data using a Risky Hash and a counter mode nonce, using `fio_xmask2`.

Used for mitigating memory access attacks when storing "secret" information in memory.

Keep the nonce information in a different memory address then the secret. For example, if the secret is on the stack, store the nonce on the heap or using a static variable.

Don't use the same nonce-secret combination for other data.

This is **not** a cryptographically secure encryption. Even **if** the algorithm was secure, it would provide no more then a 32 bit level encryption, which isn't strong enough for any cryptographic use-case.

However, this could be used to mitigate memory probing attacks. Secrets stored in the memory might remain accessible after the program exists or through core dump information. By storing "secret" information masked in this way, it mitigates the risk of secret information being easily recognized.


-------------------------------------------------------------------------------

## Pseudo Random Generation

If the `FIO_RAND` macro is defined, the following, non-cryptographic psedo-random generator functions will be defined.

The "random" data is initialized / seeded automatically using a small number of functional cycles that collect data and hash it, hopefully resulting in enough jitter entropy.

The data is collected using `getrusage` (or the system clock if `getrusage` is unavailable) and hashed using RiskyHash. The data is then combined with the previous state / cycle.

The CPU "jitter" within the calculation **should** effect `getrusage` in a way that makes it impossible for an attacker to determine the resulting random state (assuming jitter exists).

However, this is unlikely to prove cryptographically safe and isn't likely to produce a large number of entropy bits (even though a small number of bits have a large impact on the final state).

The facil.io random generator functions appear both faster and more random then the standard `rand` on my computer (you can test it for yours).

I designed it in the hopes of achieving a cryptographically safe PRNG, but it wasn't cryptographically analyzed, lacks a good source of entropy and should be considered as a non-cryptographic PRNG.

**Note**: bitwise operations (`FIO_BITWISE`) and Risky Hash (`FIO_RISKY_HASH`) are automatically defined along with `FIO_RAND`, since they are required by the algorithm.

#### `fio_rand64`

```c
uint64_t fio_rand64(void)
```

Returns 64 random bits. Probably **not** cryptographically safe.

#### `fio_rand_bytes`

```c
void fio_rand_bytes(void *data_, size_t len)
```

Writes `len` random Bytes to the buffer pointed to by `data`. Probably **not**
cryptographically safe.

#### `fio_rand_feed2seed`

```c
static void fio_rand_feed2seed(void *buf_, size_t len);
```

An internal function (accessible from the translation unit) that allows a program to feed random data to the PRNG (`fio_rand64`).

The random data will effect the random seed on the next reseeding.

Limited to 1023 bytes of data per function call.

#### `fio_rand_reseed`

```c
void fio_rand_reseed(void);
```

Forces the random generator state to rotate.

SHOULD be called after `fork` to prevent the two processes from outputting the same random numbers (until a reseed is called automatically).

-------------------------------------------------------------------------------
# SHA1

By defining the `FIO_SHA1`, the SHA1 a (broken) Cryptographic Hash functions will be defined and made available.

Do **not** use SHA1 for security concerns, it's broken and hopefully future cryptographic libraries won't include it in their packages... however, for some reason, some protocols require SHA1 (i.e., WebSockets).

#### `fio_sha1`

```c
fio_sha1_s fio_sha1(const void *data, uint64_t len);
```

A simple, non streaming, implementation of the SHA1 hashing algorithm.

#### `fio_sha1_len`

```c
size_t fio_sha1_len(void);
```

Returns the digest length of SHA1 in bytes (which is always 20).

#### `fio_sha1_digest`

```c
uint8_t *fio_sha1_digest(fio_sha1_s *s);
```

Returns the digest of a SHA1 object.

-------------------------------------------------------------------------------
## String / Number conversion

If the `FIO_ATOL` macro is defined, the following functions will be defined:

#### `fio_atol`

```c
int64_t fio_atol(char **pstr);
```

A helper function that converts between String data to a signed int64_t.

Numbers are assumed to be in base 10. Octal (`0###`), Hex (`0x##`/`x##`) and
binary (`0b##`/ `b##`) are recognized as well. For binary Most Significant Bit
must come first.

The most significant difference between this function and `strtol` (aside of API
design), is the added support for binary representations.

#### `fio_atof`

```c
double fio_atof(char **pstr);
```

A helper function that converts between String data to a signed double.

Currently wraps `strtod` with some special case handling.

#### `fio_ltoa`

```c
size_t fio_ltoa(char *dest, int64_t num, uint8_t base);
```

A helper function that writes a signed int64_t to a string.

No overflow guard is provided, make sure there's at least 68 bytes available (for base 2).

Offers special support for base 2 (binary), base 8 (octal), base 10 and base 16 (hex) where prefixes are automatically added if required (i.e.,`"0x"` for hex and `"0b"` for base 2, and `"0"` for octal).

Supports any base up to base 36 (using 0-9,A-Z).

An unsupported base will log an error and print zero.

Returns the number of bytes actually written (excluding the NUL terminator).

#### `fio_ftoa`

```c
size_t fio_ftoa(char *dest, double num, uint8_t base);
```

A helper function that converts between a double to a string.

Currently wraps `snprintf` with some special case handling.

No overflow guard is provided, make sure there's at least 130 bytes available
(for base 2).

Supports base 2, base 10 and base 16. An unsupported base will silently default
to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the beginning of the
string).

Returns the number of bytes actually written (excluding the NUL terminator).


#### `fio_c2i`

```c
uint8_t fio_c2i(unsigned char c);
```

Maps characters to alphanumerical value, where numbers have their natural values (`0-9`) and `A-Z` (or `a-z`) map to the values `10-35`.

Out of bound values return 255.

This allows calculations for up to base 36.

-------------------------------------------------------------------------------
## Threads (portable)

The facil.io `FIO_THREADS` module provides a simple API for threading that is OS portable between POSIX systems and Windows OS.

The POSIX systems implementation uses `pthreads` under the hood.

Please note that due to thread return value and methodology differences, `FIO_THREADS` do not return any value.

The following methods are provided when the `FIO_THREADS` macro is defined before including the `fio-stl.h` header.


### Thread functions

#### `FIO_THREADS_BYO`

* **BYO**: **B**ring **Y**our **O**wn.

If this macro is defined, these thread functions are only declared, but they are **not** defined (implemented).

The implementation expects you to provide your own inline alternatives.

#### `fio_thread_create`
```c
int fio_thread_create(fio_thread_t *thread, void *(*start_function)(void *), void *arg);
```

Starts a new thread, returns 0 on success and -1 on failure.

#### `fio_thread_join`
```c
int fio_thread_join(fio_thread_t thread);
```

Waits for the thread to finish.

#### `fio_thread_detach`
```c
int fio_thread_detach(fio_thread_t thread);
```

Detaches the thread, so thread resources are freed automatically.

#### `fio_thread_exit`
```c
void fio_thread_exit(void);
```

Ends the current running thread.

#### `fio_thread_equal`
```c
int fio_thread_equal(fio_thread_t a, fio_thread_t b);
```

Returns non-zero if both threads refer to the same thread.

#### `fio_thread_current`
```c
fio_thread_t fio_thread_current(void);
```

Returns the current thread.

#### `fio_thread_yield`
```c
void fio_thread_yield(void);
```

Yields thread execution.

### Mutex functions

#### `FIO_THREADS_MUTEX_BYO`

* **BYO**: **B**ring **Y**our **O**wn.

If this macro is defined, these mutex functions are only declared, but they are **not** defined (implemented).

The implementation expects you to provide your own inline alternatives.

#### `FIO_THREAD_MUTEX_INIT`

Statically initializes a Mutex.

#### `fio_thread_mutex_init`
```c
int fio_thread_mutex_init(fio_thread_mutex_t *m);
```

Initializes a simple Mutex.

Or use the static initialization value: `FIO_THREAD_MUTEX_INIT`

#### `fio_thread_mutex_lock`
```c
int fio_thread_mutex_lock(fio_thread_mutex_t *m);
```

Locks a simple Mutex, returning -1 on error.

#### `fio_thread_mutex_trylock`
```c
int fio_thread_mutex_trylock(fio_thread_mutex_t *m);
```

Attempts to lock a simple Mutex, returning zero on success.

#### `fio_thread_mutex_unlock`
```c
int fio_thread_mutex_unlock(fio_thread_mutex_t *m);
```

Unlocks a simple Mutex, returning zero on success or -1 on error.

#### `fio_thread_mutex_destroy`
```c
void fio_thread_mutex_destroy(fio_thread_mutex_t *m);
```

Destroys the simple Mutex (cleanup).



-------------------------------------------------------------------------------
## URL (URI) parsing

URIs (Universal Resource Identifier), commonly referred to as URL (Uniform Resource Locator), are a common way to describe network and file addresses.

A common use case for URIs is within the command line interface (CLI), allowing a client to point at a resource that may be local (i.e., `file:///users/etc/my.conf`) or remote (i.e. `http://example.com/conf`).

By defining `FIO_URL`, the following types and functions will be defined:

#### `fio_url_s`

```c
/** the result returned by `fio_url_parse` */
typedef struct {
  fio_buf_info_s scheme;
  fio_buf_info_s user;
  fio_buf_info_s password;
  fio_buf_info_s host;
  fio_buf_info_s port;
  fio_buf_info_s path;
  fio_buf_info_s query;
  fio_buf_info_s target;
} fio_url_s;
```

The `fio_url_s` contains a information about a URL (or, URI).

When the information is returned from `fio_url_parse`, the strings in the `fio_url_s` (i.e., `url.scheme.buf`) are **not NUL terminated**, since the parser is non-destructive, with zero-copy and zero-allocation.

#### `fio_buf_info_s` - revisited

The `fio_buf_info_s` is used to return information about the parts of the URL's string bufferas detailed above. Since the `fio_url_s` does not return NUL terminated strings, this returned data structure is used.

```c
typedef struct fio_buf_info_s {
  /** The buffer's address (may be NULL if no buffer). */
  char *buf;
  /** The buffer's length, if any. */
  size_t len;
} fio_buf_info_s;
```

See [Binary Data Informational Types and Helpers](#binary-data-informational-types-and-helpers) for more details.

#### `fio_url_parse`

```c
fio_url_s fio_url_parse(const char *url, size_t len);
```

Parses the URI returning it's components and their lengths (no decoding performed, **doesn't accept decoded URIs**).

The returned string are **not NUL terminated**, they are merely locations within the original (unmodified) string.

This function attempts to accept many different formats, including any of the following:

* `/complete_path?query#target`

  i.e.: `/index.html?page=1#list`

* `host:port/complete_path?query#target`

  i.e.:
  - `example.com`
  - `example.com:8080`
  - `example.com/index.html`
  - `example.com:8080/index.html`
  - `example.com:8080/index.html?key=val#target`

* `user:password@host:port/path?query#target`

  i.e.: `user:1234@example.com:8080/index.html`

* `username[:password]@host[:port][...]`

  i.e.: `john:1234@example.com`

* `schema://user:password@host:port/path?query#target`

  i.e.: `http://example.com/index.html?page=1#list`

Invalid formats might produce unexpected results. No error testing performed.

The `file` and `unix` schemas are special in the sense that they produce no `host` (only `path`).

-------------------------------------------------------------------------------
## Custom JSON Parser

The facil.io JSON parser is a non-strict parser, with support for trailing commas in collections, new-lines in strings, extended escape characters, comments, and octal, hex and binary numbers.

The parser allows for streaming data and decouples the parsing process from the resulting data-structure by calling static callbacks for JSON related events.

To use the JSON parser, define `FIO_JSON` before including the `fio-slt.h` file and later define the static callbacks required by the parser (see list of callbacks).

**Note**: the FIOBJ soft types already use the JSON parser. For this reason, another JSON parser can't be implemented in the same translation unit as the FIOBJ implementation. To use another JSON parser, implement it in a different C file then  the one where the FIOBJ types are implemented.

#### `JSON_MAX_DEPTH`

```c
#ifndef JSON_MAX_DEPTH
/** Maximum allowed JSON nesting level. Values above 64K might fail. */
#define JSON_MAX_DEPTH 512
#endif
```
The JSON parser isn't recursive, but it allocates a nesting bitmap on the stack, which consumes stack memory.

To ensure the stack isn't abused, the parser will limit JSON nesting levels to a customizable `JSON_MAX_DEPTH` number of nesting levels.

#### `fio_json_parser_s`

```c
typedef struct {
  /** level of nesting. */
  uint32_t depth;
  /** expectation bit flag: 0=key, 1=colon, 2=value, 4=comma/closure . */
  uint8_t expect;
  /** nesting bit flags - dictionary bit = 0, array bit = 1. */
  uint8_t nesting[(JSON_MAX_DEPTH + 7) >> 3];
} fio_json_parser_s;
```

The JSON parser type. Memory must be initialized to 0 before first uses (see `FIO_JSON_INIT`).

The type should be considered opaque. To add user data to the parser, use C-style inheritance and pointer arithmetics or simple type casting.

i.e.:

```c
typedef struct {
  fio_json_parser_s private;
  int my_data;
} my_json_parser_s;
// void use_in_callback (fio_json_parser_s * p) {
//    my_json_parser_s *my = (my_json_parser_s *)p;
// }
```

#### `FIO_JSON_INIT`

```c
#define FIO_JSON_INIT                                                          \
  { .depth = 0 }
```

A convenient macro that could be used to initialize the parser's memory to 0.

### JSON parser API
 
#### `fio_json_parse`

```c
size_t fio_json_parse(fio_json_parser_s *parser,
                      const char *buffer,
                      const size_t len);
```

Returns the number of bytes consumed before parsing stopped (due to either error or end of data). Stops as close as possible to the end of the buffer or once an object parsing was completed.

Zero (0) is a valid number and may indicate that the buffer's memory contains a partial object that can't be fully parsed just yet.

**Note!**: partial Numeral objects may be result in errors, as the number 1234 may be fragmented as 12 and 34 when streaming data. facil.io doesn't protect against this possible error.


#### `fio_json_parser_is_in_array`

```c
uint8_t fio_json_parser_is_in_array(fio_json_parser_s *parser);
```

Tests the state of the JSON parser.

Returns 1 if the parser is currently within an Array or 0 if it isn't.

**Note**: this Helper function is only available within the parsing code.

#### `fio_json_parser_is_in_object`

```c
uint8_t fio_json_parser_is_in_object(fio_json_parser_s *parser);
```

Tests the state of the JSON parser.

Returns 1 if the parser is currently within an Object or 0 if it isn't.

**Note**: this Helper function is only available within the parsing code.

#### `fio_json_parser_is_key`

```c
uint8_t fio_json_parser_is_key(fio_json_parser_s *parser);
```

Tests the state of the JSON parser.

Returns 1 if the parser is currently parsing a "key" within an object or 0 if it isn't.

**Note**: this Helper function is only available within the parsing code.

#### `fio_json_parser_is_value`

```c
uint8_t fio_json_parser_is_value(fio_json_parser_s *parser);
```

Tests the state of the JSON parser.

Returns 1 if the parser is currently parsing a "value" (within a array, an object or stand-alone) or 0 if it isn't (it's parsing a key).

**Note**: this Helper function is only available within the parsing code.

### JSON Required Callbacks

The JSON parser requires the following callbacks to be defined as static functions.

#### `fio_json_on_null`

```c
static void fio_json_on_null(fio_json_parser_s *p);
```

A `null` object was detected

#### `fio_json_on_true`

```c
static void fio_json_on_true(fio_json_parser_s *p);
```

A `true` object was detected

#### `fio_json_on_false`

```c
static void fio_json_on_false(fio_json_parser_s *p);
```

A `false` object was detected

#### `fio_json_on_number`

```c
static void fio_json_on_number(fio_json_parser_s *p, long long i);
```

A Number was detected (long long).

#### `fio_json_on_float`

```c
static void fio_json_on_float(fio_json_parser_s *p, double f);
```

A Float was detected (double).

#### `fio_json_on_string`

```c
static void fio_json_on_string(fio_json_parser_s *p, const void *start, size_t len);
```

A String was detected (int / float). update `pos` to point at ending


#### `fio_json_on_start_object`

```c
static int fio_json_on_start_object(fio_json_parser_s *p);
```

A dictionary object was detected, should return 0 unless error occurred.

#### `fio_json_on_end_object`

```c
static void fio_json_on_end_object(fio_json_parser_s *p);
```

A dictionary object closure detected

#### `fio_json_on_start_array`

```c
static int fio_json_on_start_array(fio_json_parser_s *p);
```
An array object was detected, should return 0 unless error occurred.

#### `fio_json_on_end_array`

```c
static void fio_json_on_end_array(fio_json_parser_s *p);
```

An array closure was detected

#### `fio_json_on_json`

```c
static void fio_json_on_json(fio_json_parser_s *p);
```

The JSON parsing is complete (JSON data parsed so far contains a valid JSON object).

#### `fio_json_on_error`

```c
static void fio_json_on_error(fio_json_parser_s *p);
```

The JSON parsing should stop with an error.

### JSON Parsing Example - a JSON minifier

The biggest question about parsing JSON is - where do we store the resulting data?

Different parsers solve this question in different ways.

The `FIOBJ` soft-typed object system offers a very effective solution for data manipulation, as it creates a separate object for every JSON element.

However, many parsers store the result in an internal data structure that can't be separated into different elements. These parser appear faster while actually deferring a lot of the heavy lifting to a later stage.

Here is a short example that parses the data and writes it to a new minifed (compact) JSON String result.

```c
#define FIO_JSON
#define FIO_STR_NAME fio_str
#define FIO_LOG
#include "fio-stl.h"

#define FIO_CLI
#include "fio-stl.h"

typedef struct {
  fio_json_parser_s p;
  fio_str_s out;
  uint8_t counter;
  uint8_t done;
} my_json_parser_s;

#define JSON_PARSER_CAST(ptr) FIO_PTR_FROM_FIELD(my_json_parser_s, p, ptr)
#define JSON_PARSER2OUTPUT(p) (&JSON_PARSER_CAST(p)->out)

FIO_IFUNC void my_json_write_seperator(fio_json_parser_s *p) {
  my_json_parser_s *j = JSON_PARSER_CAST(p);
  if (j->counter) {
    switch (fio_json_parser_is_in_object(p)) {
    case 0: /* array */
      if (fio_json_parser_is_in_array(p))
        fio_str_write(&j->out, ",", 1);
      break;
    case 1: /* object */
      // note the reverse `if` statement due to operation ordering
      fio_str_write(&j->out, (fio_json_parser_is_key(p) ? "," : ":"), 1);
      break;
    }
  }
  j->counter |= 1;
}

/** a NULL object was detected */
FIO_JSON_CB void fio_json_on_null(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "null", 4);
}
/** a TRUE object was detected */
static inline void fio_json_on_true(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "true", 4);
}
/** a FALSE object was detected */
FIO_JSON_CB void fio_json_on_false(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "false", 4);
}
/** a Number was detected (long long). */
FIO_JSON_CB void fio_json_on_number(fio_json_parser_s *p, long long i) {
  my_json_write_seperator(p);
  fio_str_write_i(JSON_PARSER2OUTPUT(p), i);
}
/** a Float was detected (double). */
FIO_JSON_CB void fio_json_on_float(fio_json_parser_s *p, double f) {
  my_json_write_seperator(p);
  char buffer[256];
  size_t len = fio_ftoa(buffer, f, 10);
  fio_str_write(JSON_PARSER2OUTPUT(p), buffer, len);
}
/** a String was detected (int / float). update `pos` to point at ending */
FIO_JSON_CB void
fio_json_on_string(fio_json_parser_s *p, const void *start, size_t len) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "\"", 1);
  fio_str_write(JSON_PARSER2OUTPUT(p), start, len);
  fio_str_write(JSON_PARSER2OUTPUT(p), "\"", 1);
}
/** a dictionary object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_object(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "{", 1);
  JSON_PARSER_CAST(p)->counter = 0;
  return 0;
}
/** a dictionary object closure detected */
FIO_JSON_CB void fio_json_on_end_object(fio_json_parser_s *p) {
  fio_str_write(JSON_PARSER2OUTPUT(p), "}", 1);
  JSON_PARSER_CAST(p)->counter = 1;
}
/** an array object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_array(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "[", 1);
  JSON_PARSER_CAST(p)->counter = 0;
  return 0;
}
/** an array closure was detected */
FIO_JSON_CB void fio_json_on_end_array(fio_json_parser_s *p) {
  fio_str_write(JSON_PARSER2OUTPUT(p), "]", 1);
  JSON_PARSER_CAST(p)->counter = 1;
}
/** the JSON parsing is complete */
FIO_JSON_CB void fio_json_on_json(fio_json_parser_s *p) {
  JSON_PARSER_CAST(p)->done = 1;
  (void)p;
}
/** the JSON parsing encountered an error */
FIO_JSON_CB void fio_json_on_error(fio_json_parser_s *p) {
  fio_str_write(
      JSON_PARSER2OUTPUT(p), "--- ERROR, invalid JSON after this point.\0", 42);
}

void run_my_json_minifier(char *json, size_t len) {
  my_json_parser_s p = {{0}};
  fio_json_parse(&p.p, json, len);
  if (!p.done)
    FIO_LOG_WARNING(
        "JSON parsing was incomplete, minification output is partial");
  fprintf(stderr, "%s\n", fio_str2ptr(&p.out));
  fio_str_destroy(&p.out);
}
```

-------------------------------------------------------------------------------
## Local Memory Allocation

The facil.io Simple Template Library includes a fast, concurrent, local memory allocator designed for grouping together objects with similar lifespans.

Multiple allocators can be defined using `FIO_MEMORY_NAME` and including `fio-stl.h` multiple times.

The shortcut `FIO_MALLOC` MACRO will define a local memory allocator shared by any facil.io types that are defined after that macro (in multiple `include` space).

When `FIO_MEMORY_DISABLE` is defined, all custom memory allocators will route to the system's `malloc`.

### Why Use a Local Memory Allocation?

Using a local memory allocator allows types to [enjoy locality and enhanced performance within their allocation subgroup](https://youtu.be/nZNd5FjSquk).

The facil.io Simple Template Library includes a fast, concurrent, memory allocator designed for shot-medium object life-spans.

Multiple allocators can be defined using the `FIO_MEMORY_NAME` macro, allowing different objects types to have different memory allocators, resulting in better cache locality and less contention in multi-threaded programs.

The facil.io allocator also increases security by zero-ing out the memory earlier and always returning zeroed out memory (see default `FIO_MEMORY_INITIALIZE_ALLOCATIONS`).

Reallocated memory might be filled with junk data after the valid data, but this allocator solves this issue by offering [`fio_realloc2`](#fio_realloc2).

Memory allocation overhead is small  ~ 0.006% by default, which is about 1 byte per 16,384 bytes). In addition there's a small per-process overhead for the allocator's state-machine (usually just 1 page / 4Kb per process). 

However, the allocator is designed for common network scenarios where all long-term allocations are performed during the start-up phase or using a different memory allocator. It's also designed to favor smaller allocations and has a limit on the number of bytes it could handle before passing through to `mmap`.

The memory allocator can be used in conjuncture with the system's `malloc` to minimize heap fragmentation (long-life objects use `malloc`, short life objects use `fio_malloc`) or as a memory pool for specific objects.

**Note**: this custom allocator could increase memory fragmentation if long-life allocations are performed periodically (rather than performed during startup). Use [`fio_mmap`](#fio_mmap) or the system's `malloc` for long-term allocations.

### Memory Allocator Overview

To minimize contention, the memory allocator uses allocation "arenas" that can work independently, allowing a number of threads to allocate memory in parallel with other threads (depending on the number of arenas).

The memory allocator collects "chunks" of memory from the system.

Each chunk is divided into "blocks" or used in whole as a "big-blocks".

Each block is assigned to an arena. Big block allocations aren't assigned to arenas and can't be performed in parallel.

Blocks and big-blocks are "sliced" in a similar manner to `sbrk` in order to allocate memory to the user.

A block (or big-block) is returned to the allocator for reuse only when it's memory was fully freed. A leaked allocation will prevent a block / big-block from being released back to the allocator.

If all the blocks in a memory chunk were freed, the chunk is either cached or returned to the system, according to the allocator's settings.

This behavior, including the allocator's default alignment, can be tuned / changed using compile-time macros.

It should be possible to use tcmalloc or jemalloc alongside facil.io's allocator.

It's also possible to prevent facil.io's custom allocator from compiling by defining `FIO_MEMORY_DISABLE` (`-DFIO_MEMORY_DISABLE`).


### Memory Helpers API


#### `fio_memset_aligned`

```c
void fio_memset_aligned(void *dest, uint64_t data, size_t bytes);
```

A machine native CPU word aligned `memset` alternative using a somewhat naive implementation with loop unrolling.

Requires BOTH addresses to be aligned on native memory boundary addresses for a "word" (i.e., 8 bytes on 64 bit machines). This implementation should also work (though probably slower) on CPUs that do not require memory alignment.

This implementation is probably significantly **slower** than the one included with your compiler's C library, especially for larger memory blocks.

On my Intel machine the difference is immediate (this implementation is slower than the compiler's optimized implementation). On my ARM computer, the difference is significant with byte lengths over 65Kb (probably due to CPU caching limits). For smaller buffer sizes, the assumption of a memory aligned address actually minimizes some overhead and allows for competitive performance.


#### `fio_memcpy_aligned`

```c
void fio_memcpy_aligned(void *dest_, const void *src_, size_t bytes);
```

A machine native CPU word aligned `memcpy` alternative using a somewhat naive implementation with loop unrolling.

This can also be used safely as a `memmove` replacement (with overlapping memory regions).

Requires address to be aligned on native memory boundary address for a "word" (i.e., 8 bytes on 64 bit machines). This implementation should also work (though probably slower) on CPUs that do not require memory alignment.

This implementation is probably significantly **slower** than the one included with your compiler's C library, especially for larger memory blocks.

On my Intel machine the difference is immediate (this implementation is slower than the compiler's optimized implementation). On my ARM computer, the difference is significant with byte lengths over 65Kb (probably due to CPU caching limits). For smaller buffer sizes, the assumption of a memory aligned address actually minimizes some overhead and allows for competitive performance.


### The Memory Allocator's API

The functions were designed to be a drop in replacement to the system's memory allocation functions (`malloc`, `free` and friends).

Where some improvement could be made, it was made using an added function name to add improved functionality (such as `fio_realloc2`).

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_malloc`

```c
void * fio_malloc(size_t size);
```

Allocates memory of requested size, using the defined alignment (`FIO_MEMORY_ALIGN_LOG`).

Memory is **always** zeroed out, no need to manually zero the memory after allocation.

Allocations above the allocator's per-arena limit will be redirected to a large allocation chunk if enabled (see `FIO_MEMORY_ENABLE_BIG_ALLOC`).

Allocations above the allocator limit will be redirected to `mmap`, as if `fio_mmap` was called.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_calloc`

```c
void * fio_realloc2(void *ptr, size_t new_size, size_t copy_length);
```

Same as calling `fio_malloc(size_per_unit * unit_count)`.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_realloc`

```c
void * fio_realloc(void *ptr, size_t new_size);
```

Re-allocates memory. An attempt to avoid copying the data is made only for memory allocations that are performed directly against the system (sizes over the allocator limit).

**Note**: when reallocating, junk data may be copied onto the new allocation unit. It is better to use `fio_realloc2`.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_realloc2`

```c
void * fio_realloc2(void *ptr, size_t new_size, size_t copy_length);
```

Re-allocates memory. An attempt to avoid copying the data is made only for memory allocations that are performed directly against the system (sizes over the allocator limit).

This variation could be significantly faster as it will copy less data.

This variation also promises that any memory over `copy_length` is zeroed out.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_mmap`

```c
void * fio_mmap(size_t size);
```

Allocates memory directly using `mmap`, this is preferred for objects that both require almost a page of memory (or more) and expect a long lifetime.

However, since this allocation will invoke the system call (`mmap`), it will be inherently slower.

`fio_free` can be used for deallocating the memory.

**Note**: some overhead is added to the `size` allocation (about the same size as the alignment required), in order to store the allocation size information.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

#### `fio_free`

```c
void fio_free(void *ptr);
```

Frees memory that was allocated using this allocator.

If memory was allocator using a different allocator,behavior is undefined... i.e.: with some bad luck, nothing will happen, not even a memory leak, with some good luck the program will crash and expose the bug.

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.

**Note2**: if `fio_free` is called **after** the allocator had been "destroyed" (cleanup occurred.


#### `fio_malloc_after_fork`

```c
void fio_malloc_after_fork(void);
```

Never fork a multi-threaded process. Doing so might corrupt the memory allocation system. The risk is more relevant for child processes.

However, if a multi-threaded process, calling this function from the child process would perform a best attempt at mitigating any arising issues (at the expense of possible leaks).

**Note**: the prefix `fio` will be different according to the `FIO_MEMORY_NAME` macro, it is used here because this is the prefix defined when using the `FIO_MALLOC` shortcut macro.


### Memory Allocator Creation MACROS

#### `FIO_MALLOC` (shortcut)

This shortcut macros defines a general allocator with the prefix `fio` (i.e., `fio_malloc`, `fio_free`, etc').

The general allocator settings consume more memory to allow for higher relative performance when using the memory pool as a generic allocator rather than an object specific memory pool.

Some setup macros are automatically defined and the `FIO_MEM_REALLOC` macro family are automatically updated.


It is similar to using:

```c
/* for a general allocator, increase system allocation size to 8Gb */
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG 23
/* for a general allocator, increase cache size */
#define FIO_MEMORY_CACHE_SLOTS 8
/* set fragmentation cost at 0.5Mb blocks */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 5
/* support big allocations using undivided memory chunks */
#define FIO_MEMORY_ENABLE_BIG_ALLOC 1
/* secure by default */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 1

#undef FIO_MEM_REALLOC
#undef FIO_MEM_FREE
#undef FIO_MEM_REALLOC_IS_SAFE

/*
* Set if FIO_MEM_REALLOC copies only what was asked,
* and the rest of the memory is initialized (as if returned from calloc).
*/
#define FIO_MEM_REALLOC_IS_SAFE fio_realloc_is_safe()

/** Reallocates memory, copying (at least) `copy_len` if necessary. */
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len) fio_realloc2((ptr), (new_size), (copy_len))
/** Frees allocated memory. */
#define FIO_MEM_FREE(ptr, size) fio_free((ptr))
```

**Note**: this macro also (re)defines the `FIO_MEM_REALLOC_IS_SAFE` macro, allowing you to know if `fio_malloc` (and it's feature of memory being zeroed out) is available.

#### `FIO_MEMORY_NAME`

**REQUIRED**: the prefix for the memory-pool allocator.

This also automatically updates the temporary memory allocation macros (`FIO_MEM_REALLOC_`, etc') so all types defined in the same `include` statement as the allocator will use this allocator instead of the default allocator assigned using `FIO_MEM_REALLOC` (nothe the `_`).

#### `FIO_MALLOC_OVERRIDE_SYSTEM`

Overrides the system's default `malloc` to use this allocator instead.

#### `FIO_MEMORY_ALIGN_LOG`

```c
#define FIO_MEMORY_ALIGN_LOG 4
```

Sets the memory allocation alignment log. This starts with 8 byte alignment (value of 3) and accepts values up to 1024 (value of 10).

The default is 4 (16 byte alignment) which is the X64 requirement for SIMD instructions (SSE).

Allocation alignment, if set, **must** be >= 3 and <= 10.

This macro automatically defines the `FIO_MEMORY_ALIGN_SIZE` macro for internal use.

### Memory Allocator Configuration MACROS

The following compile time MACROS can effect the tuning and configuration of the resulting memory allocator.

#### `FIO_MEMORY_INITIALIZE_ALLOCATIONS`

```c
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT
```

If true, all allocations (including `realloc2` but excluding `realloc`) will return initialized memory memory and memory will be zeroed out earlier.

**Note**: when using `realloc` (vs., `realloc2`), the allocator does not know the size of the original allocation or its copy limits, so the memory isn't guaranteed to be initialized unless using `realloc2` which promises that any memory over `copy_len`is initialized.

The default value is controlled by the macro `FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT`.

#### `FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT`

```c
/* secure by default */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT 1
```

Controls the default behavior for facil.io memory allocators (see `FIO_MEMORY_INITIALIZE_ALLOCATIONS`).

To increase performance, at the expense of the improved security and features provided by an allocator that zeros out memory early and often, set this value to 0.

#### `FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG`

```c
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG 21
```

The logarithmic size of a single allocatiion "chunk" (16 blocks).

Limited to >=17 and <=24.

By default 22, which is a \~2Mb allocation per system call, resulting in a maximum allocation size of 131Kb.

This macro automatically defines the `FIO_MEMORY_SYS_ALLOCATION_SIZE` macro for internal use.

#### `FIO_MEMORY_CACHE_SLOTS`

```c
#define FIO_MEMORY_CACHE_SLOTS 4
```

The number of system allocation "chunks" to cache even if they are not in use.


#### `FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG`

```c
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 2 /* 8 blocks per allocation */
```

The number of blocks per system allocation.

More blocks protect against fragmentation and improve memory leak detection, but lower the maximum number of bytes that can be allocated without reverting to large allocations (see `FIO_MEMORY_ENABLE_BIG_ALLOC`) or `fio_mmap`.

**Range**: 0-4

**Recommended**: depends on object allocation sizes, usually 1 or 2.

#### `FIO_MEMORY_ENABLE_BIG_ALLOC`

```c
#define FIO_MEMORY_ENABLE_BIG_ALLOC 1
```

Uses a whole system allocation to support bigger allocations.

Using big allocations could increase fragmentation costs for large long-life objects and decrease the chance of a memory-leak detection.

However, if there are no bugs in the system and objects have shot/medium lifespans, this could increase performance for larger allocations as it could avoid system calls.

#### `FIO_MEMORY_ARENA_COUNT`

```c
#define FIO_MEMORY_ARENA_COUNT -1
```

Memory arenas mitigate thread contention while using more memory.

Note that at some point arenas are statistically irrelevant (except when benchmarking contention in multi-core machines).

Zero / negative values will result in dynamic selection based on CPU core count.

#### `FIO_MEMORY_ARENA_COUNT_FALLBACK`

```c
#define FIO_MEMORY_ARENA_COUNT_FALLBACK 8
```

Used when the dynamic arena count calculations fails.

**Note**: relevant if `FIO_MEMORY_ARENA_COUNT` is zero/negative, since dynamic arena calculation is performed using CPU core calculation.

#### `FIO_MEMORY_ARENA_COUNT_MAX`

```c
#define FIO_MEMORY_ARENA_COUNT_MAX 32
```

Defines the maximum number of arenas to allocate when using dynamic arena calculation.

**Note**: relevant if `FIO_MEMORY_ARENA_COUNT` is zero/negative, since dynamic arena calculation is performed using CPU core calculation.

#### `FIO_MEMORY_USE_THREAD_MUTEX`

```c
/*
* If arena count isn't linked to the CPU count, threads might busy-spin.
* It is better to slow wait than fast busy spin when the work in the lock is longer...
* and system allocations are performed inside arena locks.
*/
#if FIO_MEMORY_ARENA_COUNT > 0
#define FIO_MEMORY_USE_THREAD_MUTEX 1
#else
#define FIO_MEMORY_USE_THREAD_MUTEX 0
#endif
```

If true, uses a `pthread mutex` instead of a `fio_lock_i` spinlock.

When setting `FIO_USE_THREAD_MUTEX_TMP` or `FIO_USE_THREAD_MUTEX` to true (`1`), than the default value of this macro will be `1`. However, this macro can be used to override the default `FIO_USE_THREAD_MUTEX` / `FIO_USE_THREAD_MUTEX_TMP`.

#### `FIO_MEMORY_WARMUP`

```c
#define FIO_MEMORY_WARMUP 0
```

If set to a number, will allocate memory on startup to the number of arenas indicated.

It is usually better to avoid this unless using a single arena.

#### `FIO_MEMORY_USE_FIO_MEMSET`

```c
#define FIO_MEMORY_USE_FIO_MEMSET 0
```

If true, uses a facil.io custom implementation for an aligned `memset`.

It's recommended to avoid this unless a compiler / system doesn't have its own optimized implementation for zeroing out pages.

#### `FIO_MEMORY_USE_FIO_MEMCOPY`

```c
#define FIO_MEMORY_USE_FIO_MEMCOPY 1
```

If true, uses a facil.io custom implementation for an aligned `memcpy`.

Since the memory is known to be aligned, it's sometimes faster to use the facil.io aligned implementation that the system's generic implementation.

#### `FIO_MEM_PAGE_ALLOC`, `FIO_MEM_PAGE_REALLOC` and `FIO_MEM_PAGE_FREE`

```c
#define FIO_MEM_PAGE_ALLOC(pages, alignment_log)                               \
  FIO_MEM_PAGE_ALLOC_def_func((pages), (alignment_log))

#define FIO_MEM_PAGE_REALLOC(ptr, old_pages, new_pages, alignment_log)         \
  FIO_MEM_PAGE_REALLOC_def_func((ptr), (old_pages), (new_pages), (alignment_log))

#define FIO_MEM_PAGE_FREE(ptr, pages)                                          \
  FIO_MEM_PAGE_FREE_def_func((ptr), (pages))
```

These MACROS, when all of them are defined, allow the memory allocator to collect memory from the system using an alternative method.

This allows the allocator to be used in situations where `mmap` is unavailable.

**Note:** the alignment property for the allocated memory is essential and may me quite large (see `FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG`).

### Debugging the allocator

The following functions will also be defined per-allocator. However, they should be considered experimental and unstable as they are linked to the allocator's internal workings.

#### `fio_malloc_sys_alloc_size`

```c
size_t fio_malloc_sys_alloc_size(void);
```

Returns the allocation size used when allocating memory from the system.

#### `fio_malloc_block_size`

```c
size_t fio_malloc_block_size(void);
```

Returns the block size used for allocations. Blocks are slices of system allocations used for allocating memory to the user.

#### `fio_malloc_cache_slots`

```c
size_t fio_malloc_cache_slots(void);
```

Returns the number of cache slots. Cache slots prevent memory chunks from being returned to the system, assuming that the memory will be needed again by the user.

#### `fio_malloc_alignment`

```c
size_t fio_malloc_alignment(void);
```

Returns the allocation alignment set for the allocator.

#### `fio_malloc_alignment_log`

```c
size_t fio_malloc_alignment_log(void);
```

Returns the allocation alignment log set for the allocator.

#### `fio_malloc_alloc_limit`

```c
size_t fio_malloc_alloc_limit(void);
```

Returns the per-allocation size limit, after which a call to `mmap` will be used.

#### `fio_malloc_arena_alloc_limit`

```c
size_t fio_malloc_arena_alloc_limit(void);
```

Returns the per-allocation size limit for an arena based allocation, after which a big-block allocation or `mmap` will be used.

#### `fio_malloc_print_state`

```c
void fio_malloc_print_state(void);
```

Prints information from the allocator's data structure. May be used for debugging.

#### `fio_malloc_print_settings`

```c
void fio_malloc_print_settings(void);
```

Prints the settings used to define the allocator.

### Reserved `FIO_MEMORY` related macros

The following are reserved macro names:

* `FIO_ALIGN_NEW`

* `FIO_ALIGN`

* `FIO_MEMORY_ALIGN_SIZE`

* `FIO_MEMORY_ALLOC_LIMIT`

* `FIO_MEMORY_BIG_ALLOC_LIMIT`

* `FIO_MEMORY_BLOCK_ALLOC_LIMIT`

* `FIO_MEMORY_BLOCKS_PER_ALLOCATION`

* `FIO_MEMORY_SYS_ALLOCATION_SIZE`

* `FIO_MALLOC_TMP_USE_SYSTEM`


-------------------------------------------------------------------------------
## Time Helpers

By defining `FIO_TIME` or `FIO_QUEUE`, the following time related helpers functions are defined:

#### `fio_time_real`

```c
struct timespec fio_time_real();
```

Returns human (watch) time... this value isn't as safe for measurements.

#### `fio_time_mono`

```c
struct timespec fio_time_mono();
```

Returns monotonic time.

#### `fio_time_nano`

```c
uint64_t fio_time_nano();
```

Returns monotonic time in nano-seconds (now in 1 micro of a second).

#### `fio_time_micro`

```c
uint64_t fio_time_micro();
```

Returns monotonic time in micro-seconds (now in 1 millionth of a second).

#### `fio_time_milli`

```c
uint64_t fio_time_milli();
```

Returns monotonic time in milliseconds.


#### `fio_time2milli`

```c
uint64_t fio_time2milli(struct timespec t);
```

Converts a `struct timespec` to milliseconds.

#### `fio_time2gm`

```c
struct tm fio_time2gm(time_t timer);
```

A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Returns a `struct tm` object filled with the date information.

This function is used internally for the formatting functions: , `fio_time2rfc7231`, `fio_time2rfc2109`, and `fio_time2rfc2822`.

#### `fio_gm2time`

```c
time_t fio_gm2time(struct tm tm)
```

Converts a `struct tm` to time in seconds (assuming UTC).

This function is less localized then the `mktime` / `timegm` library functions.

#### `fio_time2rfc7231`

```c
size_t fio_time2rfc7231(char *target, time_t time);
```

Writes an RFC 7231 date representation (HTTP date format) to target.

Requires 29 characters (for positive, 4 digit years).

The format is similar to DDD, dd, MON, YYYY, HH:MM:SS GMT

i.e.: Sun, 06 Nov 1994 08:49:37 GMT

#### `fio_time2rfc2109`

```c
size_t fio_time2rfc2109(char *target, time_t time);
```

Writes an RFC 2109 date representation to target.

Requires 31 characters (for positive, 4 digit years).

#### `fio_time2rfc2822`

```c
size_t fio_time2rfc2822(char *target, time_t time);
```

Writes an RFC 2822 date representation to target.

Requires 28 or 29 characters (for positive, 4 digit years).

-------------------------------------------------------------------------------
## Task Queue

The Simple Template Library includes a simple, thread-safe, task queue based on a linked list of ring buffers.

Since delayed processing is a common task, this queue is provides an easy way to schedule and perform delayed tasks.

In addition, a Timer type allows timed events to be scheduled and moved (according to their "due date") to an existing Task Queue.

By `FIO_QUEUE`, the following task and timer related helpers are defined:

### Queue Related Types

#### `fio_queue_task_s`

```c
/** Task information */
typedef struct {
  /** The function to call */
  void (*fn)(void *, void *);
  /** User opaque data */
  void *udata1;
  /** User opaque data */
  void *udata2;
} fio_queue_task_s;
```

The `fio_queue_task_s` type contains information about a delayed task. The information is important for the `fio_queue_push` MACRO, where it is used as named arguments for the task information.

#### `fio_queue_s`

```c
/** The queue object - should be considered opaque (or, at least, read only). */
typedef struct {
  fio___task_ring_s *r;
  fio___task_ring_s *w;
  /** the number of tasks waiting to be performed (read-only). */
  size_t count;
  fio_lock_i lock; /* unless FIO_USE_THREAD_MUTEX(_TMP) is true */
  fio___task_ring_s mem;
} fio_queue_s;
```

The `fio_queue_s` object is the queue object.

This object could be placed on the stack or allocated on the heap (using [`fio_queue_new`](#fio_queue_new)).

Once the object is no longer in use call [`fio_queue_destroy`](#fio_queue_destroy) (if placed on the stack) of [`fio_queue_free`](#fio_queue_free) (if allocated using [`fio_queue_new`](#fio_queue_new)).

### Queue API

#### `fio_queue_init`

```c
/** Used to initialize a fio_queue_s object. */
void fio_queue_init(fio_queue_s *q);
```

#### `fio_queue_destroy`

```c
void fio_queue_destroy(fio_queue_s *q);
```

Destroys a queue and re-initializes it, after freeing any used resources.

**Note**:
When using the optional `pthread_mutex_t` implementation or using timers on Windows, the timer object needs to be re-initialized explicitly before re-used after being destroyed (call `fio_queue_init`).

#### `FIO_QUEUE_STATIC_INIT(queue)`

```c
#define FIO_QUEUE_STATIC_INIT(queue)                                           \
  { .r = &(queue).mem, .w = &(queue).mem, .lock = FIO_LOCK_INIT }
```

May be used to initialize global, static memory, queues.

**Note**: while the use `FIO_QUEUE_STATIC_INIT` is possible,  this macro resets a whole page of memory to zero whereas `fio_queue_init` only initializes a few bytes of memory which are the only relevant bytes during initialization.

#### `fio_queue_new`

```c
fio_queue_s *fio_queue_new(void);
```

Creates a new queue object (allocated on the heap).

#### `fio_queue_free`

```c
void fio_queue_free(fio_queue_s *q);
```

Frees a queue object after calling fio_queue_destroy.

#### `fio_queue_push`

```c
int fio_queue_push(fio_queue_s *q, fio_queue_task_s task);
#define fio_queue_push(q, ...)                                                 \
  fio_queue_push((q), (fio_queue_task_s){__VA_ARGS__})

```

Pushes a **valid** (non-NULL) task to the queue.

This function is shadowed by the `fio_queue_push` MACRO, allowing named arguments to be used.

For example:

```c
void tsk(void *, void *);
fio_queue_s q = FIO_QUEUE_INIT(q);
fio_queue_push(q, .fn = tsk);
// ...
fio_queue_destroy(q);
```

Returns 0 if `task.fn == NULL` or if the task was successfully added to the queue.

Returns -1 on error (no memory).


#### `fio_queue_push_urgent`

```c
int fio_queue_push_urgent(fio_queue_s *q, fio_queue_task_s task);
#define fio_queue_push_urgent(q, ...)                                          \
  fio_queue_push_urgent((q), (fio_queue_task_s){__VA_ARGS__})
```

Pushes a task to the head of the queue (LIFO).

Returns -1 on error (no memory).

See [`fio_queue_push`](#fio_queue_push) for details.

#### `fio_queue_pop`

```c
fio_queue_task_s fio_queue_pop(fio_queue_s *q);
```

Pops a task from the queue (FIFO).

Returns a NULL task on error (`task.fn == NULL`).

**Note**: The task isn't performed automatically, it's just returned. This is useful for queues that don't necessarily contain callable functions.

#### `fio_queue_perform`

```c
int fio_queue_perform(fio_queue_s *q);
```

Pops and performs a task from the queue (FIFO).

Returns -1 on error (queue empty).

#### `fio_queue_perform_all`

```c
void fio_queue_perform_all(fio_queue_s *q);
```

Performs all tasks in the queue.

#### `fio_queue_count`

```c
size_t fio_queue_count(fio_queue_s *q);
```

Returns the number of tasks in the queue.

### Timer Related Types

#### `fio_timer_queue_s`

```c
typedef struct {
  fio___timer_event_s *next;
  fio_lock_i lock;
} fio_timer_queue_s;
```

The `fio_timer_queue_s` struct should be considered an opaque data type and accessed only using the functions or the initialization MACRO.

To create a `fio_timer_queue_s` on the stack (or statically):

```c
fio_timer_queue_s foo_timer = FIO_TIMER_QUEUE_INIT;
```

A timer could be allocated dynamically:

```c
fio_timer_queue_s *foo_timer = malloc(sizeof(*foo_timer));
FIO_ASSERT_ALLOC(foo_timer);
*foo_timer = (fio_timer_queue_s)FIO_TIMER_QUEUE_INIT(*foo_timer);
```

#### `FIO_TIMER_QUEUE_INIT`

This is a MACRO used to statically initialize a `fio_timer_queue_s` object.

### Timer API

#### `fio_timer_schedule`

```c
void fio_timer_schedule(fio_timer_queue_s *timer_queue,
                        fio_timer_schedule_args_s args);
```

Adds a time-bound event to the timer queue.

Accepts named arguments using the following argument type and MACRO:

```c
typedef struct {
  /** The timer function. If it returns a non-zero value, the timer stops. */
  int (*fn)(void *, void *);
  /** Opaque user data. */
  void *udata1;
  /** Opaque user data. */
  void *udata2;
  /** Called when the timer is done (finished). */
  void (*on_finish)(void *, void *);
  /** Timer interval, in milliseconds. */
  uint32_t every;
  /** The number of times the timer should be performed. -1 == infinity. */
  int32_t repetitions;
  /** Millisecond at which to start. If missing, filled automatically. */
  uint64_t start_at;
} fio_timer_schedule_args_s;

#define fio_timer_schedule(timer_queue, ...)                                   \
  fio_timer_schedule((timer_queue), (fio_timer_schedule_args_s){__VA_ARGS__})
```

Note, the event will repeat every `every` milliseconds (or the same unites as `start_at` and `now`).

It the scheduler is busy or the event is otherwise delayed, its next scheduling may compensate for the delay by being scheduled sooner.

#### `fio_timer_push2queue` 

```c
/**  */
size_t fio_timer_push2queue(fio_queue_s *queue,
                            fio_timer_queue_s *timer_queue,
                            uint64_t now); // now is in milliseconds
```

Pushes due events from the timer queue to an event queue.

If `now` is `0`, than `fio_time_milli` will be called to supply `now`'s value.

**Note**: all the `start_at` values for all the events in the timer queue will be treated as if they use the same units as (and are relative to) `now`. By default, this unit should be milliseconds, to allow `now` to be zero.

Returns the number of tasks pushed to the queue. A value of `0` indicates no new tasks were scheduled.

#### `fio_timer_next_at`

```c
int64_t fio_timer_next_at(fio_timer_queue_s *timer_queue);
```

Returns the millisecond at which the next event should occur.

If no timer is due (list is empty), returns `-1`.

**Note**: Unless manually specified, millisecond timers are relative to  `fio_time_milli()`.


#### `fio_timer_destroy`

```c
void fio_timer_destroy(fio_timer_queue_s *timer_queue);
```

Clears any waiting timer bound tasks.

**Note**:

The timer queue must NEVER be freed when there's a chance that timer tasks are waiting to be performed in a `fio_queue_s`.

This is due to the fact that the tasks may try to reschedule themselves (if they repeat).

**Note 2**:
When using the optional `pthread_mutex_t` implementation or using timers on Windows, the timer object needs to be reinitialized before re-used after being destroyed.

-------------------------------------------------------------------------------
## CLI (command line interface)

The Simple Template Library includes a CLI parser that provides a simpler API and few more features than the array iteration based `getopt`, such as:

* Auto-generation of the "help" / usage output.

* Argument type testing (String, boolean, and integer types are supported).

* Global Hash map storage and access to the parsed argument values (until `fio_cli_end` is called).

* Support for unnamed options / arguments, including adjustable limits on how many a user may input.

* Array style support and access to unnamed arguments.


By defining `FIO_CLI`, the following functions will be defined.

In addition, `FIO_CLI` automatically includes the `FIO_ATOL` flag, since CLI parsing depends on the `fio_atol` function.

#### `fio_cli_start`

```c
#define fio_cli_start(argc, argv, unnamed_min, unnamed_max, description, ...)  \
  fio_cli_start((argc), (argv), (unnamed_min), (unnamed_max), (description),   \
                (char const *[]){__VA_ARGS__, (char const *)NULL})

/* the shadowed function: */
void fio_cli_start   (int argc, char const *argv[],
                      int unnamed_min, int unnamed_max,
                      char const *description,
                      char const **names);
```

The `fio_cli_start` **macro** shadows the `fio_cli_start` function and defines the CLI interface to be parsed. i.e.,

The `fio_cli_start` macro accepts the `argc` and `argv`, as received by the `main` functions, a maximum and minimum number of unspecified CLI arguments (beneath which or after which the parser will fail), an application description string and a variable list of (specified) command line arguments.

If the minimum number of unspecified CLI arguments is `-1`, there will be no maximum limit on the number of unnamed / unrecognized arguments allowed  

The text `NAME` in the description (all capitals) will be replaced with the executable command invoking the application.

Command line arguments can be either String, Integer or Boolean. Optionally, extra data could be added to the CLI help output. CLI arguments and information is added using any of the following macros:

* `FIO_CLI_STRING("-arg [-alias] [(default_value)] desc.")`

* `FIO_CLI_INT("-arg [-alias] [(default_value)] desc.")`

* `FIO_CLI_BOOL("-arg [-alias] [(1)] desc.")`

* `FIO_CLI_PRINT_HEADER("header text (printed as a header)")`

* `FIO_CLI_PRINT("raw text line (printed as is, with same spacing as arguments)")`

* `FIO_CLI_PRINT_LINE("raw text line (printed as is, no spacing or offset)")`

**Note**: default values may optionally be provided by placing them in parenthesis immediately after the argument name and aliases. Default values that start with `(` must end with `)` (the surrounding parenthesis are ignored). Default values that start with `("` must end with `")` (the surrounding start and end markers are ignored).

```c
int main(int argc, char const *argv[]) {
  fio_cli_start(argc, argv, 0, -1,
                "this is a CLI example for the NAME application.",
                FIO_CLI_PRINT_HEADER("CLI type validation"),
                FIO_CLI_STRING("-str -s (my default str (string\\)) any data goes here"),
                FIO_CLI_INT("-int -i (42) integer data goes here"),
                FIO_CLI_BOOL("-bool -b (1) flag (boolean) only - no data"),
                FIO_CLI_PRINT("This example allows for unlimited arguments "
                              "that will simply pass-through"),
                FIO_CLI_PRINT_LINE("We hope you enjoy the NAME example.")
                );
  if (fio_cli_get("-s")) /* always true when default value is provided */
    fprintf(stderr, "String: %s\n", fio_cli_get("-s"));

  fprintf(stderr, "Integer: %d\n", fio_cli_get_i("-i"));

  fprintf(stderr, "Boolean: %d\n", fio_cli_get_i("-b"));

  if (fio_cli_unnamed_count()) {
    fprintf(stderr, "Printing unlisted / unrecognized arguments:\n");
    for (size_t i = 0; i < fio_cli_unnamed_count(); ++i) {
      fprintf(stderr, "%s\n", fio_cli_unnamed(i));
    }
  }

  fio_cli_end();
  return 0;
}
```

#### `fio_cli_end`

```c
void fio_cli_end(void);
```

Clears the CLI data storage.

#### `fio_cli_get`

```c
char const *fio_cli_get(char const *name);
```

Returns the argument's value as a string, or NULL if the argument wasn't provided.

#### `fio_cli_get_i`

```c
int fio_cli_get_i(char const *name);
```

Returns the argument's value as an integer, or 0 if the argument wasn't provided.

#### `fio_cli_get_bool`

```c
#define fio_cli_get_bool(name) (fio_cli_get((name)) != NULL)
```

Evaluates to true (1) if the argument was boolean and provided. Otherwise evaluated to false (0).

#### `fio_cli_unnamed_count`

```c
unsigned int fio_cli_unnamed_count(void);
```

Returns the number of unrecognized arguments (arguments unspecified, in `fio_cli_start`).

#### `fio_cli_unnamed`

```c
char const *fio_cli_unnamed(unsigned int index);
```

Returns a String containing the unrecognized argument at the stated `index` (indexes are zero based).

#### `fio_cli_set`

```c
void fio_cli_set(char const *name, char const *value);
```

Sets a value for the named argument (but **not** it's aliases).

-------------------------------------------------------------------------------
## Basic Socket / IO Helpers

The facil.io standard library provides a few simple IO / Sockets helpers for POSIX systems.

By defining `FIO_SOCK`, the following functions will be defined.

**Note**:

On Windows that `fd` is a 64 bit number with no promises made as to its value. On POSIX systems the `fd` is a 32 bit number which is sequential. 

Since facil.io prefers the POSIX approach, it will validate the `fd` value for overflow and might fail to open / accept sockets when their value overflows the 32bit type limit set on POSIX machines.

However, for most implementations this should be a non-issue as it seems (from observation, not knowledge) that Windows maps `fd` values to a kernel array (rather than a process specific array) and it is unlikely that any Windows machine will actually open more than 2 Giga "handles" unless it's doing something wrong.

#### `fio_sock_open`

```c
int fio_sock_open(const char *restrict address,
                 const char *restrict port,
                 uint16_t flags);
```

Creates a new socket according to the provided flags.

The `port` string will be ignored when `FIO_SOCK_UNIX` is set.

The `address` can be NULL for Server sockets (`FIO_SOCK_SERVER`) when binding to all available interfaces (this is actually recommended unless network filtering is desired).

The `flag` integer can be a combination of any of the following flags:

*  `FIO_SOCK_TCP` - Creates a TCP/IP socket.

*  `FIO_SOCK_UDP` - Creates a UDP socket.

*  `FIO_SOCK_UNIX ` - Creates a Unix socket (requires a POSIX system). If an existing file / Unix socket exists, they will be deleted and replaced.

*  `FIO_SOCK_SERVER` - Initializes a Server socket. For TCP/IP and Unix sockets, the new socket will be listening for incoming connections (`listen` will be automatically called).

*  `FIO_SOCK_CLIENT` - Initializes a Client socket, calling `connect` using the `address` and `port` arguments.

*  `FIO_SOCK_NONBLOCK` - Sets the new socket to non-blocking mode.

If neither `FIO_SOCK_SERVER` nor `FIO_SOCK_CLIENT` are specified, the function will default to a server socket.

**Note**:

UDP Server Sockets might need to handle traffic from multiple clients, which could require a significantly larger OS buffer then the default buffer offered.

Consider (from [this SO answer](https://stackoverflow.com/questions/2090850/specifying-udp-receive-buffer-size-at-runtime-in-linux/2090902#2090902), see [this blog post](https://medium.com/@CameronSparr/increase-os-udp-buffers-to-improve-performance-51d167bb1360), [this article](http://fasterdata.es.net/network-tuning/udp-tuning/) and [this article](https://access.redhat.com/documentation/en-US/JBoss_Enterprise_Web_Platform/5/html/Administration_And_Configuration_Guide/jgroups-perf-udpbuffer.html)):

```c
int n = 32*1024*1024; /* try for 32Mb */
while (n >= (4*1024*1024) && setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
  /* failed - repeat attempt at 1Mb interval */
  if (n >= (4 * 1024 * 1024)) // OS may have returned max value
    n -= 1024 * 1024;

}
```

#### `fio_sock_open2`

```c
int fio_sock_open(const char *url, uint16_t flags);
```

See [`fio_sock_open`](#fio_sock_open) for details. Accepts a single, URL style string instead of an address / port pair.

The `tcp` / `udp` information **may** appear in the URL schema if missing from the flags (i.e., `tcp://localhost:3000/`);

If a Unix socket URL is detected on a POSIX system, a `FIO_SOCK_UNIX` socket flag will override any `FIO_SOCK_TCP` or 
`FIO_SOCK_UDP` that were originally given.

#### `fio_sock_write`, `fio_sock_read`, `fio_sock_close`

```c
#define fio_sock_write(fd, data, len) write((fd), (data), (len))
#define fio_sock_read(fd, buf, len)   read((fd), (buf), (len))
#define fio_sock_close(fd)            close(fd)
/* on Windows only */
#define accept fio_sock_accept
```

Behaves the same as the POSIX function calls... however, on Windows these will be function wrappers around the WinSock2 API variants. It is better to use these macros / functions for portability.

#### `fio_sock_wait_io`

```c
short fio_sock_wait_io(int fd, short events, int timeout)
```

Uses `poll` to wait until an IO device has one or more of the evens listed in `events` (`POLLIN | POLLOUT`) or `timeout` (in milliseconds) have passed.

Returns 0 on timeout, -1 on error or the events that are valid.

#### `fio_sock_poll`

```c
typedef struct {
  void (*on_ready)(int fd, void *udata);
  void (*on_data)(int fd, void *udata);
  void (*on_error)(int fd, void *udata);
  void *udata;
  int timeout;
  struct pollfd *fds;
} fio_sock_poll_args;

int fio_sock_poll(fio_sock_poll_args args);
#define fio_sock_poll(...) fio_sock_poll((fio_sock_poll_args){__VA_ARGS__})
```

The `fio_sock_poll` function is shadowed by the `fio_sock_poll` MACRO, which allows the function to accept the following "named arguments":

* `on_ready`:

    This callback will be called if a socket can be written to and the socket is polled for the **W**rite event.

        // callback example:
        void on_ready(int fd, void *udata);

* `on_data`:

    This callback will be called if data is available to be read from a socket and the socket is polled for the **R**ead event.

        // callback example:
        void on_data(int fd, void *udata);

* `on_error`:

    This callback will be called if an error occurred when polling the file descriptor.

        // callback example:
        void on_error(int fd, void *udata);

* `timeout`:

    Polling timeout in milliseconds.

        // type:
        int timeout;

* `udata`:

    Opaque user data.

        // type:
        void *udata;

* `fds`:

    A list of `struct pollfd` with file descriptors to be polled. The list **should** end with a `struct pollfd` containing an empty `events` field (and no empty `events` field should appear in the middle of the list).

    Use the `FIO_SOCK_POLL_LIST(...)`, `FIO_SOCK_POLL_RW(fd)`, `FIO_SOCK_POLL_R(fd)` and `FIO_SOCK_POLL_W(fd)` macros to build the list.

* `count`:

    If supplied, should contain the valid length of the `fds` array.

    If `0` or empty and `fds` isn't `NULL`, the length of the `fds` array will be auto-calculated by seeking through the array for a member in which `events == 0`.

    **Note**: this could cause a buffer overflow, which is why the last member of the `fds` array **should** end with a `struct pollfd` member containing an empty `events` field. Use the `FIO_SOCK_POLL_LIST` macro as a helper.

The `fio_sock_poll` function uses the `poll` system call to poll a simple IO list.

The list must end with a `struct pollfd` with it's `events` set to zero. No other member of the list should have their `events` data set to zero.

It is recommended to use the `FIO_SOCK_POLL_LIST(...)` and
`FIO_SOCK_POLL_[RW](fd)` macros. i.e.:

```c
int io_fd = fio_sock_open(NULL, "8888", FIO_SOCK_UDP | FIO_SOCK_NONBLOCK | FIO_SOCK_SERVER);
int count = fio_sock_poll(.on_ready = on_ready,
                    .on_data = on_data,
                    .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(io_fd)));
```

**Note**: The `poll` system call should perform reasonably well for light loads (short lists). However, for complex IO needs or heavier loads, use the system's native IO API, such as `kqueue` or `epoll`.


#### `FIO_SOCK_POLL_RW` (macro)

```c
#define FIO_SOCK_POLL_RW(fd_)                                                  \
  (struct pollfd) { .fd = fd_, .events = (POLLIN | POLLOUT) }
```

This helper macro helps to author a `struct pollfd` member who's set to polling for both read and write events (data availability and/or space in the outgoing buffer).

#### `FIO_SOCK_POLL_R` (macro)

```c
#define FIO_SOCK_POLL_R(fd_)                                                   \
  (struct pollfd) { .fd = fd_, .events = POLLIN }
```

This helper macro helps to author a `struct pollfd` member who's set to polling for incoming data availability.


#### `FIO_SOCK_POLL_W` (macro)

```c
#define FIO_SOCK_POLL_W(fd_)                                                   \
  (struct pollfd) { .fd = fd_, .events = POLLOUT }
```

This helper macro helps to author a `struct pollfd` member who's set to polling for space in the outgoing `fd`'s buffer.

#### `FIO_SOCK_POLL_LIST` (macro)

```c
#define FIO_SOCK_POLL_LIST(...)                                                \
  (struct pollfd[]) {                                                          \
    __VA_ARGS__, (struct pollfd) { .fd = -1 }                                  \
  }
```

This helper macro helps to author a `struct pollfd` array who's last member has an empty `events` value (for auto-length detection).

#### `fio_sock_address_new`

```c
struct addrinfo *fio_sock_address_new(const char *restrict address,
                                      const char *restrict port,
                                      int sock_type);
```

Attempts to resolve an address to a valid IP6 / IP4 address pointer.

The `sock_type` element should be a socket type, such as `SOCK_DGRAM` (UDP) or `SOCK_STREAM` (TCP/IP).

The address should be freed using `fio_sock_address_free`.

#### `fio_sock_address_free`

```c
void fio_sock_address_free(struct addrinfo *a);
```

Frees the pointer returned by `fio_sock_address_new`.

#### `fio_sock_set_non_block`

```c
int fio_sock_set_non_block(int fd);
```

Sets a file descriptor / socket to non blocking state.

#### `fio_sock_open_local`

```c
int fio_sock_open_local(struct addrinfo *addr);
```

Creates a new network socket and binds it to a local address.

#### `fio_sock_open_remote`

```c
int fio_sock_open_remote(struct addrinfo *addr, int nonblock);
```

Creates a new network socket and connects it to a remote address.

#### `fio_sock_open_unix`

```c
int fio_sock_open_unix(const char *address, int is_client, int nonblock);
```

Creates a new Unix socket and binds it to a local address.

**Note**: available only on POSIX systems.

-------------------------------------------------------------------------------
## Basic IO Polling

IO polling using the portable `poll` POSIX function is another area that's of common need and where many solutions are required.

The facil.io standard library provides a persistent polling container for evented management of small IO (file descriptor) collections using the "one-shot" model.

"One-Shot" means that once a specific even has "fired" (occurred), it will no longer be monitored (unless re-submitted). If the same file desciptor is waiting on multiple event, only those events that occurred will be removed from the monitored collection.

There's no real limit on the number of file descriptors that can be monitored, except possible system limits that the system may impose on the `poll` system call. However, performance will degrade significantly as the ratio between inactive vs. active IO objects being monitored increases.

It is recommended to use a system specific polling "engine" (`epoll` / `kqueue`) if polling thousands of persistent file descriptors.

By defining `FIO_POLL`, the following functions will be defined.

**Note**: the same type and range limitations that apply to the Sockets implementation on Windows apply to the `poll` implementation.

### `FIO_POLL` API


#### `fio_poll_s`

```c
typedef struct fio_poll_s fio_poll_s;
```

The `fio_poll_s` type should be considered opaque and should **not** be accessed directly.

#### `FIO_POLL_INIT`

```c
#define FIO_POLL_INIT(...)                                                     \
  /* FIO_POLL_INIT(on_data_func, on_ready_func, on_close_func) */              \
  { .settings = { __VA_ARGS__ }, .lock = FIO_LOCK_INIT }
```

A `fio_poll_s` object initialization macro.

Static initialization may be limited to POSIX systems.

Use: `FIO_POLL_INIT(on_data_func, on_ready_func, on_close_func)`

#### `fio_poll_new`

```c
fio_poll_s *fio_poll_new(fio_poll_settings_s settings);
/* named argument support */
#define fio_poll_new(...) fio_poll_new((fio_poll_settings_s){__VA_ARGS__})
```

Creates a new polling object / queue.

The settings arguments set the `on_data`, `on_ready` and `on_close` callbacks:

```c
typedef struct {
  /** callback for when data is availabl in the incoming buffer. */
  void (*on_data)(int fd, void *udata);
  /** callback for when the outgoing buffer allows a call to `write`. */
  void (*on_ready)(int fd, void *udata);
  /** callback for closed connections and / or connections with errors. */
  void (*on_close)(int fd, void *udata);
} fio_poll_settings_s;
```

Returns NULL on error (no memory).

#### `fio_poll_free`

```c
int fio_poll_free(fio_poll_s *p);
```

Frees the polling object and its resources.

#### `fio_poll_destroy`

```c
void fio_poll_destroy(fio_poll_s *p);
```

Destroys the polling object, freeing its resources.

**Note**: the monitored file descriptors will remain untouched (possibly open). To close all the monitored file descriptors, call `fio_poll_close_and_destroy` instead.

#### `fio_poll_close_and_destroy`

```c
void fio_poll_close_all(fio_poll_s *p);
```

Closes all monitored connections, calling the `on_close` callbacks for all of them.

#### `fio_poll_monitor`

```c
int fio_poll_monitor(fio_poll_s *p, int fd, void *udata, unsigned short flags);
```

Adds a file descriptor to be monitored, adds events to be monitored or updates the monitored file's `udata`.

Possible flags are: `POLLIN` and `POLLOUT`. Other flags may be set but might be ignored.

On systems where `POLLRDHUP` is supported, it is always monitored for.

Monitoring mode is always one-shot. If an event if fired, it is removed from the monitoring state.

Returns -1 on error.

#### `fio_poll_review`

```c
int fio_poll_review(fio_poll_s *p, int timeout);
```

Reviews if any of the monitored file descriptors has any events.

`timeout` is in milliseconds.

Returns the number of events called.

**Note**:

Polling is thread safe, but has different effects on different threads.

Adding a new file descriptor from one thread while polling in a different thread will not poll that IO untill `fio_poll_review` is called again.

#### `fio_poll_forget`

```c
void *fio_poll_forget(fio_poll_s *p, int fd);
```

Stops monitoring the specified file descriptor even if some of it's event's hadn't occurred just yet, returning its `udata` (if any).

### `FIO_POLL` Compile Time Macros

#### `FIO_POLL_HAS_UDATA_COLLECTION`

```c
#ifndef FIO_POLL_HAS_UDATA_COLLECTION
#define FIO_POLL_HAS_UDATA_COLLECTION 1
#endif
```

When set to true (the default value), the `udata` value is unique per file descriptor, using an array of `udata` values.

When false, a global `udata` is used and it is updated whenever a `udata` value is supplied (`NULL` values are ignored).

#### `FIO_POLL_FRAGMENTATION_LIMIT`

```c
#define FIO_POLL_FRAGMENTATION_LIMIT 63
```

When the polling array is fragmented by more than the set value, it will be de-fragmented on the idle cycle (if no events occur).

#### `FIO_POLL_DEBUG`

If defined before the first time `FIO_POLL` is included, this will add debug messages to the polling logic.

-------------------------------------------------------------------------------
## Data Stream Container

Data Stream objects solve the issues that could arise when `write` operations don't write all the data (due to OS buffering). 

Data Streams offer a way to store / concat different data sources (static strings, dynamic strings, files) as a single data stream. This allows the data to be easily written to an IO target (socket / pipe / file) using the `write` operation.

By defining the macro `FIO_STREAM`, the following macros and functions will be defined.

#### `fio_stream_s`

```c
typedef struct {
  /* do not directly acecss! */
  fio_stream_packet_s *next;
  fio_stream_packet_s **pos;
  size_t consumed;
  size_t length;
} fio_stream_s;
```

The `fio_stream_s` type should be considered opaque and only accessed through the following API.

#### `fio_stream_packet_s`

The `fio_stream_packet_s` type should be considered opaque and only accessed through the following API.

This type is used to separate data packing with any updates made to the stream object, allowing data packing to be performed concurrently with stream reading / updating (which requires a lock in multi-threaded applications).


#### `FIO_STREAM_INIT(stream)`

```c
#define FIO_STREAM_INIT(s)                                                     \
  { .next = NULL, .pos = &(s).next }
#endif
```

Object initialization macro.

#### `fio_stream_new`

```c
fio_stream_s *fio_stream_new(void);
```

Allocates a new object on the heap and initializes it's memory.

#### `fio_stream_free`

```c
int fio_stream_free(fio_stream_s *stream);
```

Frees any internal data AND the object's container!

#### `fio_stream_destroy`

```c
void fio_stream_destroy(fio_stream_s *stream);
```

Destroys the object, reinitializing its container.

#### `fio_stream_any`

```c
uint8_t fio_stream_any(fio_stream_s *stream);
````

Returns true if there's any data in the stream.

**Note**: this isn't thread safe, but it often doesn't matter if it is.

#### `fio_stream_length`

```c
size_t fio_stream_length(fio_stream_s *stream);
````

Returns the number of bytes waiting in the stream.

**Note**: this isn't thread safe, but it often doesn't matter if it is.

### Packing data into the stream

#### `fio_stream_pack_data`

```c
fio_stream_packet_s *fio_stream_pack_data(void *buf,
                                          size_t len,
                                          size_t offset,
                                          uint8_t copy_buffer,
                                          void (*dealloc_func)(void *));
```

Packs data into a `fio_stream_packet_s` container.

Can be performed concurrently with other operations.

#### `fio_stream_pack_fd`

```c
fio_stream_packet_s * fio_stream_pack_fd(int fd, size_t len, size_t offset, uint8_t keep_open);
```

Packs a file descriptor into a `fio_stream_packet_s` container. 

#### `fio_stream_add`

```c
void fio_stream_add(fio_stream_s *stream, fio_stream_packet_s *packet);
```

Adds a packet to the stream.

**Note**: this isn't thread safe.

#### `fio_stream_pack_free`

```c
void fio_stream_pack_free(fio_stream_packet_s *packet);
```

Destroys the `fio_stream_packet_s` - call this ONLY if the packed data was never added to the stream using `fio_stream_add`. 


### Reading / Consuming data from the Stream


#### `fio_stream_read`

```c
void fio_stream_read(fio_stream_s *stream, char **buf, size_t *len);
```

Reads data from the stream (if any), leaving the data in the stream **without advancing the reading position** (see [`fio_stream_advance`](#fio_stream_advance).

`buf` MUST point to a buffer with - at least - `len` bytes. This is required in case the packed data is fragmented or references a file and needs to be copied to an available buffer.

On error, or if the stream is empty, `buf` will be set to NULL and `len` will be set to zero.

Otherwise, `buf` may retain the same value or it may point directly to a memory address within the stream's buffer (the original value may be lost) and `len` will be updated to the largest possible value for valid data that can be read from `buf`.

**Note**: this isn't thread safe.

#### `fio_stream_advance`

```c
void fio_stream_advance(fio_stream_s *stream, size_t len);
```

Advances the Stream, so the first `len` bytes are marked as consumed.

**Note**: this isn't thread safe.

### Stream configuration

Besides the (recommended) use of a local allocator using the `FIO_MEMORY` or `FIO_MEM_REALLOC` macro families, the following configuration macros are supported:

#### `FIO_STREAM_COPY_PER_PACKET`

When copying data to the stream, large memory sections will be divided into smaller allocations in order to free memory faster and minimize the direct use of `mmap`.

This macro should be set according to the specific allocator limits. By default, it is set to 96Kb (which is neither here nor there).

-------------------------------------------------------------------------------

## Signal Monitoring

OS signal callbacks are very limited in the actions they are allowed to take. In fact, one of the only actions they are allowed to take is to set a volatile atomic flag.

The facil.io STL offers helpers that perform this very common pattern of declaring a flag, watching a signal, setting a flag and (later) calling a callback outside of the signal handler that would handle the actual event.

When defining `FIO_SIGNAL`, the following function are defined.

#### `fio_signal_monitor`

```c
int fio_signal_monitor(int sig, void (*callback)(int sig, void *), void *udata);
```

Starts to monitor for the specified signal, setting an optional callback.

If the signal is already being monitored, the callback and `udata` pointers are updated.

**Note**: `udata` stands for "user data", it is an opaque pointer that is simply passed along to the callback.

#### `fio_signal_review`

```c
int fio_signal_review(void);
```

Reviews all signals, calling any relevant callbacks.

#### `fio_signal_forget`

```c
int fio_signal_forget(int sig);
```

Stops monitoring the specified signal.

-------------------------------------------------------------------------------
## Globe Matching

By defining the macro `FIO_GLOB_MATCH` the following functions are defined:

#### `fio_glob_match`

```c
uint8_t fio_glob_match(fio_str_info_s pat, fio_str_info_s str);
```

This function is a **binary** glob matching helper.

Returns 1 on a match, otherwise returns 0.

The following patterns are recognized:

* `*` - matches any string, including an empty string.
		
	i.e., the following patterns will match against the string `"String"`:

    `"*"`

    `"*String*"`

    `"S*ing"`

* `?` - matches any single **byte** (does NOT support UTF-8 characters).
		
	i.e., the following patterns will match against the string `"String"`:

    `"?tring"`

    `"Strin?"`

    `"St?ing"`

* `[!...]` or `[^...]` - matches any **byte** that is **not** withing the brackets (does **not** support UTF-8 characters).

    Byte ranges are supported using `'-'` (i.e., `[!0-9]`)

	Use the backslash (`\`) to escape the special `]`, `-` and `\` characters when they are part of the list.
	
	i.e., the following patterns will match against the string `"String"`:

    `"[!a-z]tring"`

    `"[^a-z]tring"`

    `"[^F]tring"` (same as `"[!F]tring"`)

* `[...]` - matches any **byte** that **is** withing the brackets (does **not** support UTF-8 characters).

	Use the backslash (`\`) to escape the special `]`, `-` and `\` characters when they are part of the list.
	
	i.e., the following patterns will match against the string `"String"`:

    `"[A-Z]tring"`

    `"[sS]tring"`


-------------------------------------------------------------------------------

## File Utility Helpers

By defining the macro `FIO_FILES` the following file helper functions are defined:

#### `fio_filename_open`

```c
int fio_filename_open(const char *filename, int flags);
```

Opens `filename`, returning the same as values as `open` on POSIX systems.

If `path` starts with a `"~/"` than it will be relative to the user's Home folder (on Windows, testing for `"~\"`).

#### `fio_filename_is_unsafe`

```c
int fio_filename_is_unsafe(const char *path);
```

Returns 1 if `path` does folds backwards (has "/../" or "//").

#### `fio_filename_tmp`

```c
int fio_filename_tmp(void);
```

Creates a temporary file, returning its file descriptor.

Returns -1 on error.

#### `fio_filename_overwrite`

```c
int fio_filename_overwrite(const char *filename, const void *buf, size_t len);
```

Overwrites `filename` with the data in the buffer.

If `path` starts with a `"~/"` than it will be relative to the user's home folder (on Windows, testing for `"~\"`).

Returns -1 on error or 0 on success. On error, the state of the file is undefined (may be doesn't exit / nothing written / partially written).

#### `fio_fd_write`

```c
ssize_t fio_fd_write(int fd, const void *buf, size_t len);
```

Writes data to a file, returning the number of bytes written.

Returns -1 on error.

Since some systems have a limit on the number of bytes that can be written at a single time, this function fragments the system calls into smaller `write` blocks, allowing large data to be written.

If the file descriptor is non-blocking, test `errno` for `EAGAIN` / `EWOULDBLOCK`.

-------------------------------------------------------------------------------

## Linked Lists

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element 
typedef struct {
  char * data;
  FIO_LIST_NODE node;
} my_list_s;
// create linked list helper functions
#define FIO_LIST_NAME my_list
#include "fio-stl.h"
```

Doubly Linked Lists are an incredibly common and useful data structure.

### Linked Lists Performance

Memory overhead (on 64bit machines) is 16 bytes per node (or 8 bytes on 32 bit machines) for the `next` and `prev` pointers.

Linked Lists use pointers in order to provide fast add/remove operations with O(1) speeds. This O(1) operation ignores the object allocation time and suffers from poor memory locality, but it's still very fast.

However, Linked Lists suffer from slow seek/find and iteration operations.

Seek/find has a worst case scenario O(n) cost and iteration suffers from a high likelihood of CPU cache misses, resulting in degraded performance.

### Linked Lists Macros

Linked List Macros (and arch-type) are always defined by the CSTL and can be used to manage linked lists without creating a dedicated type.

#### `FIO_LIST_NODE` / `FIO_LIST_HEAD`

```c
/** A linked list node type */
#define FIO_LIST_NODE fio_list_node_s
/** A linked list head type */
#define FIO_LIST_HEAD fio_list_node_s
/** A linked list arch-type */
typedef struct fio_list_node_s {
  struct fio_list_node_s *next;
  struct fio_list_node_s *prev;
} fio_list_node_s;

```

These are the basic core types for a linked list node used by the Linked List macros.

#### `FIO_LIST_INIT(head)`

```c
#define FIO_LIST_INIT(obj)                                                     \
  (FIO_LIST_HEAD){ .next = &(obj), .prev = &(obj) }
```

Initializes a linked list.

#### `FIO_LIST_PUSH`

```c
#define FIO_LIST_PUSH(head, n)                                                 \
  do {                                                                         \
    (n)->prev = (head)->prev;                                                  \
    (n)->next = (head);                                                        \
    (head)->prev->next = (n);                                                  \
    (head)->prev = (n);                                                        \
  } while (0)
```

UNSAFE macro for pushing a node to a list.

Note that this macro does not test that the list / data was initialized before reading / writing to the memory pointed to by the list / node.

#### `FIO_LIST_REMOVE`

```c
#define FIO_LIST_REMOVE(n)                                                     \
  do {                                                                         \
    (n)->prev->next = (n)->next;                                               \
    (n)->next->prev = (n)->prev;                                               \
    (n)->next = (n)->prev = (n);                                               \
  } while (0)
```

UNSAFE macro for removing a node from a list.

Note that this macro does not test that the list / data was initialized before reading / writing to the memory pointed to by the list / node.


#### `FIO_LIST_EACH`

```c
#define FIO_LIST_EACH(type, node_name, head, pos)                              \
  for (type *pos = FIO_PTR_FROM_FIELD(type, node_name, (head)->next),          \
            *next____p_ls_##pos =                                              \
                FIO_PTR_FROM_FIELD(type, node_name, (head)->next->next);       \
       pos != FIO_PTR_FROM_FIELD(type, node_name, (head));                     \
       (pos = next____p_ls_##pos),                                             \
            (next____p_ls_##pos =                                              \
                 FIO_PTR_FROM_FIELD(type,                                      \
                                    node_name,                                 \
                                    next____p_ls_##pos->node_name.next)))
```

Loops through every node in the linked list except the head.

This macro allows `pos` to point to the type that the linked list contains (rather than a pointer to the node type).

i.e.,

```c
typedef strcut {
  void * data;
  FIO_LIST_HEAD node;
} ptr_list_s;

/* ... */

FIO_LIST_EACH(ptr_list_s, node, pos) {
  do_something_with(pos->data);
}
```

#### `FIO_LIST_IS_EMPTY`

```c
#define FIO_LIST_IS_EMPTY(head) (!(head) || (head)->next == (head)->prev)
```

Macro for testing if a list is empty.


### Indexed Linked Lists Macros (always defined):


Indexed linked lists are often used to either save memory or making it easier to reallocate the memory used for the whole list. This is performed by listing pointer offsets instead of the whole pointer, allowing the offsets to use smaller type sizes.

For example, an Indexed Linked List might be added to objects in a cache array in order to implement a "least recently used" eviction policy. If the cache holds less than 65,536 members, than a 16 bit index is all that's required, reducing the list's overhead from 2 pointers (16 bytes on 64 bit systems) to a 4 byte overhead per cache member.

#### `FIO_INDEXED_LIST32_HEAD` / `FIO_INDEXED_LIST32_NODE`

```c
/** A 32 bit indexed linked list node type */
#define FIO_INDEXED_LIST32_NODE fio_index32_node_s
#define FIO_INDEXED_LIST32_HEAD uint32_t
/** A 16 bit indexed linked list node type */
#define FIO_INDEXED_LIST16_NODE fio_index16_node_s
#define FIO_INDEXED_LIST16_HEAD uint16_t
/** An 8 bit indexed linked list node type */
#define FIO_INDEXED_LIST8_NODE fio_index8_node_s
#define FIO_INDEXED_LIST8_HEAD uint8_t

/** A 32 bit indexed linked list node type */
typedef struct fio_index32_node_s {
  uint32_t next;
  uint32_t prev;
} fio_index32_node_s;

/** A 16 bit indexed linked list node type */
typedef struct fio_index16_node_s {
  uint16_t next;
  uint16_t prev;
} fio_index16_node_s;

/** An 8 bit indexed linked list node type */
typedef struct fio_index8_node_s {
  uint8_t next;
  uint8_t prev;
} fio_index8_node_s;
```

#### `FIO_INDEXED_LIST_PUSH`

```c
#define FIO_INDEXED_LIST_PUSH(root, node_name, head, i)                        \
  do {                                                                         \
    register const size_t n__ = (i);                                           \
    (root)[n__].node_name.prev = (root)[(head)].node_name.prev;                \
    (root)[n__].node_name.next = (head);                                       \
    (root)[(root)[(head)].node_name.prev].node_name.next = n__;                \
    (root)[(head)].node_name.prev = n__;                                       \
  } while (0)
```

UNSAFE macro for pushing a node to a list.

#### `FIO_INDEXED_LIST_REMOVE`

```c
#define FIO_INDEXED_LIST_REMOVE(root, node_name, i)                            \
  do {                                                                         \
    register const size_t n__ = (i);                                           \
    (root)[(root)[n__].node_name.prev].node_name.next =                        \
        (root)[n__].node_name.next;                                            \
    (root)[(root)[n__].node_name.next].node_name.prev =                        \
        (root)[n__].node_name.prev;                                            \
    (root)[n__].node_name.next = (root)[n__].node_name.prev = n__;             \
  } while (0)
```

UNSAFE macro for removing a node from a list.

#### `FIO_INDEXED_LIST_EACH`

```c
#define FIO_INDEXED_LIST_EACH(root, node_name, head, pos)                      \
  for (size_t pos = (head), stopper___ils___ = 0; !stopper___ils___;           \
       stopper___ils___ = ((pos = (root)[pos].node_name.next) == (head)))
```

Loops through every index in the indexed list, **assuming `head` is valid**.

-------------------------------------------------------------------------------

## Linked List Dynamic Type Definition

### Linked Lists Overview

Before creating linked lists, the library header should be included at least once.

To create a linked list type, create a `struct` that includes a `FIO_LIST_NODE` typed element somewhere within the structure. For example:

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
```

Next define the `FIO_LIST_NAME` macro. The linked list helpers and types will all be prefixed by this name. i.e.:

```c
#define FIO_LIST_NAME my_list /* defines list functions (example): my_list_push(...) */
```

Optionally, define the `FIO_LIST_TYPE` macro to point at the correct linked-list structure type. By default, the type for linked lists will be `<FIO_LIST_NAME>_s`.

An example were we need to define the `FIO_LIST_TYPE` macro will follow later on.

Optionally, define the `FIO_LIST_NODE_NAME` macro to point the linked list's node. By default, the node for linked lists will be `node`.

Finally, include the `fio-stl.h` header to create the linked list helpers.

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element 
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
// create linked list helper functions
#define FIO_LIST_NAME my_list
#include "fio-stl.h"

void example(void) {
  FIO_LIST_HEAD list = FIO_LIST_INIT(list);
  for (int i = 0; i < 10; ++i) {
    my_list_s *n = malloc(sizeof(*n));
    n->i = i;
    my_list_push(&list, n);
  }
  int i = 0;
  while (my_list_any(&list)) {
    my_list_s *n = my_list_shift(&list);
    if (i != n->i) {
      fprintf(stderr, "list error - value mismatch\n"), exit(-1);
    }
    free(n);
    ++i;
  }
  if (i != 10) {
    fprintf(stderr, "list error - count error\n"), exit(-1);
  }
}
```

**Note**:

Each node is limited to a single list (an item can't belong to more then one list, unless it's a list of pointers to that item).

Items with more then a single node can belong to more then one list. i.e.:

```c
// list element 
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  FIO_LIST_NODE node2;
  double d;
} my_list_s;
// list 1 
#define FIO_LIST_NAME my_list
#include "fio-stl.h"
// list 2 
#define FIO_LIST_NAME my_list2
#define FIO_LIST_TYPE my_list_s
#define FIO_LIST_NODE_NAME node2
#include "fio-stl.h"
```

### Linked Lists (embeded) - API


#### `FIO_LIST_INIT(head)`

```c
#define FIO_LIST_INIT(obj)                                                     \
  (FIO_LIST_NODE){ .next = &(obj), .prev = &(obj) }
```

This macro initializes an uninitialized node (assumes the data in the node is junk). 

#### `LIST_any`

```c
int LIST_any(FIO_LIST_HEAD *head)
```
Returns a non-zero value if there are any linked nodes in the list. 

#### `LIST_is_empty`

```c
int LIST_is_empty(FIO_LIST_HEAD *head)
```
Returns a non-zero value if the list is empty. 

#### `LIST_remove`

```c
FIO_LIST_TYPE *LIST_remove(FIO_LIST_TYPE *node)
```
Removes a node from the list, Returns NULL if node isn't linked. 

#### `LIST_push`

```c
FIO_LIST_TYPE *LIST_push(FIO_LIST_HEAD *head, FIO_LIST_TYPE *node)
```
Pushes an existing node to the end of the list. Returns node. 

#### `LIST_pop`

```c
FIO_LIST_TYPE *LIST_pop(FIO_LIST_HEAD *head)
```
Pops a node from the end of the list. Returns NULL if list is empty. 

#### `LIST_unshift`

```c
FIO_LIST_TYPE *LIST_unshift(FIO_LIST_HEAD *head, FIO_LIST_TYPE *node)
```
Adds an existing node to the beginning of the list. Returns node.

#### `LIST_shift`

```c
FIO_LIST_TYPE *LIST_shift(FIO_LIST_HEAD *head)
```
Removed a node from the start of the list. Returns NULL if list is empty. 

#### `LIST_root`

```c
FIO_LIST_TYPE *LIST_root(FIO_LIST_HEAD *ptr)
```
Returns a pointer to a list's element, from a pointer to a node. 

#### `FIO_LIST_EACH`

_Note: macro, name unchanged, works for all lists_

```c
#define FIO_LIST_EACH(type, node_name, head, pos)                              \
  for (type *pos = FIO_PTR_FROM_FIELD(type, node_name, (head)->next),          \
            *next____p_ls =                                                    \
                FIO_PTR_FROM_FIELD(type, node_name, (head)->next->next);       \
       pos != FIO_PTR_FROM_FIELD(type, node_name, (head));                     \
       (pos = next____p_ls),                                                   \
            (next____p_ls = FIO_PTR_FROM_FIELD(type, node_name,                \
                                               next____p_ls->node_name.next)))
```

Loops through every node in the linked list (except the head).

The `type` name should reference the list type.

`node_name` should indicate which node should be used for iteration.

`head` should point at the head of the list (usually a `FIO_LIST_HEAD` variable).

`pos` can be any temporary variable name that will contain the current position in the iteration.

The list **can** be mutated during the loop, but this is not recommended. Specifically, removing `pos` is safe, but pushing elements ahead of `pos` might result in an endless loop.

_Note: this macro won't work with pointer tagging_

-------------------------------------------------------------------------------
## Dynamic Arrays

```c
#define FIO_ARRAY_NAME str_ary
#define FIO_ARRAY_TYPE char *
#define FIO_ARRAY_TYPE_CMP(a,b) (!strcmp((a),(b)))
#include "fio-stl.h"
```

Dynamic arrays are extremely common and useful data structures.

In essence, Arrays are blocks of memory that contain all their elements "in a row". They grow (or shrink) as more items are added (or removed).

Items are accessed using a numerical `index` indicating the element's position within the array.

Indexes are zero based (first element == 0).

### Dynamic Array Performance

Seeking time is an extremely fast O(1). Arrays are also very fast to iterate since they enjoy high memory locality.

Adding and editing items is also a very fast O(1), especially if enough memory was previously reserved. Otherwise, memory allocation and copying will slow performance.

However, arrays suffer from slow find operations. Find has a worst case scenario O(n) cost.

They also suffer from slow item removal (except, in our case, for `pop` / `unshift` operations), since middle-element removal requires memory copying when fixing the "hole" made in the array.

A common solution is to reserve a value for "empty" elements and `set` the element's value instead of `remove` the element.

**Note**: unlike some dynamic array implementations, this STL implementation doesn't grow exponentially. Using the `ARY_reserve` function is highly encouraged for performance.


### Dynamic Array Overview

To create a dynamic array type, define the type name using the `FIO_ARRAY_NAME` macro. i.e.:

```c
#define FIO_ARRAY_NAME int_ary
```

Next (usually), define the `FIO_ARRAY_TYPE` macro with the element type. The default element type is `void *`. For example:

```c
#define FIO_ARRAY_TYPE int
```

For complex types, define any (or all) of the following macros:

```c
// set to adjust element copying 
#define FIO_ARRAY_TYPE_COPY(dest, src)  
// set for element cleanup 
#define FIO_ARRAY_TYPE_DESTROY(obj)     
// set to adjust element comparison 
#define FIO_ARRAY_TYPE_CMP(a, b)        
// to be returned when `index` is out of bounds / holes 
#define FIO_ARRAY_TYPE_INVALID 0 
// set ONLY if the invalid element is all zero bytes 
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1     
// should the object be destroyed when copied to an `old` pointer?
#define FIO_ARRAY_DESTROY_AFTER_COPY 1 
// when array memory grows, how many extra "spaces" should be allocated?
#define FIO_ARRAY_PADDING 4 
// should the array growth be exponential? (ignores FIO_ARRAY_PADDING)
#define FIO_ARRAY_EXPONENTIAL 0 
// optimizes small arrays (mostly tuplets and single item arrays).
// note: values larger than 1 add a memory allocation cost to the array container
#define FIO_ARRAY_ENABLE_EMBEDDED 1
```

To create the type and helper functions, include the Simple Template Library header.

For example:

```c
typedef struct {
  int i;
  float f;
} foo_s;

#define FIO_ARRAY_NAME ary
#define FIO_ARRAY_TYPE foo_s
#define FIO_ARRAY_TYPE_CMP(a,b) (a.i == b.i && a.f == b.f)
#include "fio-stl.h"

void example(void) {
  ary_s a = FIO_ARRAY_INIT;
  foo_s *p = ary_push(&a, (foo_s){.i = 42});
  FIO_ARRAY_EACH(ary, &a, pos) { // pos will be a pointer to the element
    fprintf(stderr, "* [%zu]: %p : %d\n", (size_t)(pos - ary2ptr(&a)), (void *)pos, pos->i);
  }
  ary_destroy(&a);
}
```

### Dynamic Arrays - API

#### The Array Type (`ARY_s`)

```c
typedef struct {
  FIO_ARRAY_TYPE *ary;
  uint32_t capa;
  uint32_t start;
  uint32_t end;
} FIO_NAME(FIO_ARRAY_NAME, s); /* ARY_s in these docs */
```

The array type should be considered opaque. Use the helper functions to updated the array's state when possible, even though the array's data is easily understood and could be manually adjusted as needed.

#### `FIO_ARRAY_INIT`

````c
#define FIO_ARRAY_INIT  {0}
````

This macro initializes an uninitialized array object.

#### `ARY_destroy`

````c
void ARY_destroy(ARY_s * ary);
````

Destroys any objects stored in the array and frees the internal state.

#### `ARY_new`

````c
ARY_s * ARY_new(void);
````

Allocates a new array object on the heap and initializes it's memory.

#### `ARY_free`

````c
void ARY_free(ARY_s * ary);
````

Frees an array's internal data AND it's container!

#### `ARY_count`

````c
uint32_t ARY_count(ARY_s * ary);
````

Returns the number of elements in the Array.

#### `ARY_capa`

````c
uint32_t ARY_capa(ARY_s * ary);
````

Returns the current, temporary, array capacity (it's dynamic).

#### `ARY_reserve`

```c
uint32_t ARY_reserve(ARY_s * ary, int32_t capa);
```

Reserves a minimal capacity for the array.

If `capa` is negative, new memory will be allocated at the beginning of the array rather then it's end.

Returns the array's new capacity.

**Note**: the reserved capacity includes existing data / capacity. If the requested reserved capacity is equal (or less) then the existing capacity, nothing will be done.

#### `ARY_concat`

```c
ARY_s * ARY_concat(ARY_s * dest, ARY_s * src);
```

Adds all the items in the `src` Array to the end of the `dest` Array.

The `src` Array remain untouched.

Always returns the destination array (`dest`).

#### `ARY_set`

```c
FIO_ARRAY_TYPE * ARY_set(ARY_s * ary,
                       int32_t index,
                       FIO_ARRAY_TYPE data,
                       FIO_ARRAY_TYPE *old);
```

Sets `index` to the value in `data`.

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

If `old` isn't NULL, the existing data will be copied to the location pointed to by `old` before the copy in the Array is destroyed.

Returns a pointer to the new object, or NULL on error.

#### `ARY_get`

```c
FIO_ARRAY_TYPE ARY_get(ARY_s * ary, int32_t index);
```

Returns the value located at `index` (no copying is performed).

If `index` is negative, it will be counted from the end of the Array (-1 == last element).

**Reminder**: indexes are zero based (first element == 0).

#### `ARY_find`

```c
int32_t ARY_find(ARY_s * ary, FIO_ARRAY_TYPE data, int32_t start_at);
```

Returns the index of the object or -1 if the object wasn't found.

If `start_at` is negative (i.e., -1), than seeking will be performed in reverse, where -1 == last index (-2 == second to last, etc').

#### `ARY_remove`
```c
int ARY_remove(ARY_s * ary, int32_t index, FIO_ARRAY_TYPE *old);
```

Removes an object from the array, MOVING all the other objects to prevent "holes" in the data.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns 0 on success and -1 on error.

This action is O(n) where n in the length of the array. It could get expensive.

#### `ARY_remove2`

```c
uint32_t ARY_remove2(ARY_S * ary, FIO_ARRAY_TYPE data);
```

Removes all occurrences of an object from the array (if any), MOVING all the existing objects to prevent "holes" in the data.

Returns the number of items removed.

This action is O(n) where n in the length of the array. It could get expensive.

#### `ARY_compact`
```c
void ARY_compact(ARY_s * ary);
```

Attempts to lower the array's memory consumption.

#### `ARY_to_a`

```c
FIO_ARRAY_TYPE * ARY_to_a(ARY_s * ary);
```

Returns a pointer to the C array containing the objects.

#### `ARY_push`

```c
FIO_ARRAY_TYPE * ARY_push(ARY_s * ary, FIO_ARRAY_TYPE data);
```

 Pushes an object to the end of the Array. Returns a pointer to the new object or NULL on error.

#### `ARY_pop`

```c
int ARY_pop(ARY_s * ary, FIO_ARRAY_TYPE *old);
```

Removes an object from the end of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_unshift`

```c
FIO_ARRAY_TYPE *ARY_unshift(ARY_s * ary, FIO_ARRAY_TYPE data);
```

Unshifts an object to the beginning of the Array. Returns a pointer to the new object or NULL on error.

This could be expensive, causing `memmove`.

#### `ARY_shift`

```c
int ARY_shift(ARY_s * ary, FIO_ARRAY_TYPE *old);
```

Removes an object from the beginning of the Array.

If `old` is set, the data is copied to the location pointed to by `old` before the data in the array is destroyed.

Returns -1 on error (Array is empty) and 0 on success.

#### `ARY_each`

```c
uint32_t ARY_each(ARY_s * ary,
                  int (*task)(ARY_each_s * info),
                  void *arg,
                  int32_t start_at);
```

Iteration using a callback for each entry in the array.

The callback task function must accept an an `ARY_each_s` pointer (name matches Array name).

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position (number of items processed + starting point).

The `ARY_each_s` data structure looks like this:

```c
/** Iteration information structure passed to the callback. */
typedef ARY_each_s {
  /** The being iterated. Once set, cannot be safely changed. */
  FIO_ARRAY_PTR const parent;
  /** The current object's index */
  uint64_t index;
  /** Always 1 and may be used to allow type detection. */
  const int64_t items_at_index;
  /** The callback / task called for each index, may be updated mid-cycle. */
  int (*task)(ARY_each_s * info);
  /** The argument passed along to the task. */
  void *arg;
  /** The object / value at the current index. */
  FIO_ARRAY_TYPE value;
} ARY_each_s;
```

#### `ARY_each_next`

```c
FIO_ARRAY_TYPE ARY_each_next(ARY_s* ary,
                             FIO_ARRAY_TYPE **first,
                             FIO_ARRAY_TYPE *pos);

```

Used internally by the `FIO_ARRAY_EACH` macro.

Returns a pointer to the first object if `pos == NULL` and there are objects
in the array.

Returns a pointer to the (next) object in the array if `pos` and `first` are valid.

Returns `NULL` on error or if the array is empty.

**Note**: 
The first pointer is automatically set and it allows object insertions and memory effecting functions to be called from within the loop.

If the object in `pos` (or any object before it) were removed, consider passing `pos-1` to the function, to avoid skipping any elements while looping.

#### `FIO_ARRAY_EACH`

```c
#define FIO_ARRAY_EACH(array_name, array, pos)                                               \
  for (__typeof__(FIO_NAME2(array_name, ptr)((array)))                             \
           first___ = NULL,                                                    \
           pos = FIO_NAME(array_name, each_next)((array), &first___, NULL);    \
       pos;                                                                    \
       pos = FIO_NAME(array_name, each_next)((array), &first___, pos))
```


Iterates through the array using a `for` loop.

Access the object with the pointer `pos`. The `pos` variable can be named however you please.

It is possible to edit the array while iterating, however when deleting `pos`, or objects that are located before `pos`, using the proper array functions, the loop will skip the next item unless `pos` is set to `pos-1`.

**Note**: this macro supports automatic pointer tagging / untagging.

-------------------------------------------------------------------------------
## Hash Tables and Maps

```c
/* Create a binary safe String type for Strings that aren't mutated often */
#define FIO_STR_SMALL str
#include "fio-stl.h"

/* Set the properties for the key-value Hash Map type called `dict_s` */
#define FIO_MAP_NAME                 dict
#define FIO_MAP_ORDERED              0
#define FIO_MAP_TYPE                 str_s
#define FIO_MAP_TYPE_COPY(dest, src) str_init_copy2(&(dest), &(src))
#define FIO_MAP_TYPE_DESTROY(k)      str_destroy(&k)
#define FIO_MAP_TYPE_CMP(a, b)       str_is_eq(&(a), &(b))
#define FIO_MAP_KEY                  FIO_MAP_TYPE
#define FIO_MAP_KEY_COPY             FIO_MAP_TYPE_COPY
#define FIO_MAP_KEY_DESTROY          FIO_MAP_TYPE_DESTROY
#define FIO_MAP_KEY_CMP              FIO_MAP_TYPE_CMP
#include "fio-stl.h"
/** set helper for consistent hash values */
FIO_IFUNC str_s *dict_set2(dict_s *m, str_s key, str_s obj) {
  return dict_set_ptr(m, str_hash(&key, (uint64_t)m), key, obj, NULL, 1);
}
/** get helper for consistent hash values */
FIO_IFUNC str_s *dict_get2(dict_s *m, str_s key) {
  return dict_get_ptr(m, str_hash(&key, (uint64_t)m), key);
}
```

HashMaps (a.k.a., Hash Tables) and sets are extremely useful and common mapping / dictionary primitives, also sometimes known as "dictionary".

Hash maps use both a `hash` and a `key` to identify a `value`. The `hash` value is calculated by feeding the key's data to a hash function (such as Risky Hash or SipHash).

A hash map without a `key` is known as a Set or a Bag. It uses only a `hash` (often calculated using `value`) to identify the `value` in the Set, sometimes requiring a `value` equality test as well. This approach often promises a collection of unique values (no duplicate values).

Some map implementations support a FIFO limited storage, which could be used for naive limited-space caching (though caching solutions may require a more complex data-storage that's slower).

### Ordered Maps, Unordered Maps, Indexing and Performance

The facil.io library offers both ordered and unordered maps. Unordered maps are often faster and use less memory. If iteration is performed, ordered maps might be better.

Ordered hash maps (or hash tables) are defined using `FIO_OMAP_NAME`.

Unordered hash maps (or hash tables) are defined using `FIO_UMAP_NAME`.

Indexing the map allows LRU (least recently used) eviction, but comes at a performance cost in both memory (due to the extra data per object) and speed (due to out of order memory access and increased cache misses).

To enable LRU indexing on the map, define `FIO_MAP_EVICT_LRU` as `1` (true).

Ordered maps are constructed using an ordered Array + an index map that uses 4 or 8 bytes per array index.

Unordered maps are constructed using an unordered Array + an index map that uses 1 byte per array index.

Indexing is performed using a linked list that uses 4 or 8 byte index values instead of pointers.

In addition, each value stores a copy of the hash data, so hash data doesn't need to be recomputed.

The map implementations have protection features against too many full collisions or non-random hashes. When the map detects a possible "attack", it will start overwriting existing data instead of trying to resolve collisions. This can be adjusted using the `FIO_MAP_MAX_FULL_COLLISIONS` macro.

### Map Overview 

To create a map, define `FIO_MAP_NAME`, `FIO_OMAP_NAME` (ordered) **or** `FIO_UMAP_NAME` (unordered).

To create a hash map (rather then a set), also define `FIO_MAP_KEY` (containing the key's type).

To create an unordered map either use `FIO_UMAP_NAME` or define `FIO_MAP_ORDERED`.

Helpful macros to define might include:

- `FIO_MAP_ORDERED`, if `1`, the map will be ordered if `0` unordered.
- `FIO_MAP_TYPE`, which defaults to `void *`
- `FIO_MAP_TYPE_INVALID`, which defaults to `((FIO_MAP_TYPE){0})`
- `FIO_MAP_TYPE_COPY(dest, src)`, which defaults to `(dest) = (src)`
- `FIO_MAP_TYPE_DESTROY(obj)`
- `FIO_MAP_TYPE_CMP(a, b)`, which defaults to `1`
- `FIO_MAP_KEY`
- `FIO_MAP_KEY_INVALID`
- `FIO_MAP_KEY_COPY(dest, src)`
- `FIO_MAP_KEY_DESTROY(obj)`
- `FIO_MAP_KEY_CMP(a, b)`
- `FIO_MAP_MAX_FULL_COLLISIONS`, which defaults to `22`


- `FIO_MAP_DESTROY_AFTER_COPY`, uses "smart" defaults to decide if to destroy an object after it was copied (when using `set` / `remove` / `pop` with a pointer to contain `old` object).
- `FIO_MAP_TYPE_DISCARD(obj)`, handles discarded element data (i.e., insert without overwrite in a Set).
- `FIO_MAP_KEY_DISCARD(obj)`, handles discarded element data (i.e., when overwriting an existing value in a hash map).

- `FIO_MAP_SHOULD_OVERWRITE(older, newer)`, if set it should return `0` (false) to prevent an overwriting instruction. This can be used to compare timestamps between to items and test that `newer` is actually newer than `older`.
- `FIO_MAP_EVICT_LRU`, if set to true (`1`), the `evict` method and the `FIO_MAP_MAX_ELEMENTS` macro will evict members based on the Least Recently Used object.
- `FIO_MAP_MAX_ELEMENTS`, the maximum number of elements allowed before removing old data (FIFO). By default, no auto-eviction is performed.

- `FIO_MAP_HASH`, defaults to `uint64_t`, may be set to `uint32_t` if hash data is 32 bit wide.
- `FIO_MAP_HASH_FN(key)`, replace the cached `hash` for unordered maps with a re-hash calculation. This is good if the caching is dirt cheap but can only be used with unordered maps since the ordered maps double the cached hash with a "hole" marker.
- `FIO_MAP_BIG`, if defined, the maximum theoretical capacity increases to `(1 << 64) -1`.
To limit the number of elements in a map (FIFO, ignoring last access time), allowing it to behave similarly to a simple caching primitive, define: `FIO_MAP_MAX_ELEMENTS`.
- `FIO_MAP_MAX_SEEK` , the maximum number of bins to rotate when partial/full collisions occur (effects the load factor). Limited to a maximum of 255 and should be higher than `FIO_MAP_MAX_FULL_COLLISIONS/4`, by default either `7` or `13`.

If `FIO_MAP_MAX_ELEMENTS` is `0`, then the theoretical maximum number of elements should be: `(1 << 32) - 1`. In practice, the safe limit should be calculated as `1 << 31` or `1 << 30`. The same is true for `FIO_MAP_BIG`, only relative to 64 bits.

### Hash Map / Set - API (initialization)

#### `MAP_new`

```c
FIO_MAP_PTR MAP_new(void);
```

Allocates a new map on the heap.

#### `MAP_free`

```c
void MAP_free(MAP_PTR m);
```

Frees a map that was allocated on the heap.

#### `FIO_MAP_INIT`

```c
#define FIO_MAP_INIT { .map = NULL }
```

This macro initializes a map object - often used for maps placed on the stack.

#### `MAP_destroy`

```c
void MAP_destroy(MAP_PTR m);
```

Destroys the map's internal data and re-initializes it.


### Hash Map - API (hash map only)

#### `MAP_get` (hash map)

```c
FIO_MAP_TYPE MAP_get(FIO_MAP_PTR m,
                     FIO_MAP_HASH hash,
                     FIO_MAP_KEY key);
```
Returns the object in the hash map (if any) or FIO_MAP_TYPE_INVALID.

#### `MAP_get_ptr` (hash map)

```c
FIO_MAP_TYPE *MAP_get_ptr(FIO_MAP_PTR m,
                          FIO_MAP_HASH hash,
                          FIO_MAP_KEY key);
```

Returns a pointer to the object in the hash map (if any) or NULL.

#### `MAP_set` (hash map)

```c
FIO_MAP_TYPE MAP_set(FIO_MAP_PTR m,
                     FIO_MAP_HASH hash,
                     FIO_MAP_KEY key,
                     FIO_MAP_TYPE obj,
                     FIO_MAP_TYPE *old);
```


Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location.

#### `MAP_set_ptr` (hash map)

```c
FIO_MAP_TYPE *MAP_set(FIO_MAP_PTR m,
                      FIO_MAP_HASH hash,
                      FIO_MAP_KEY key,
                      FIO_MAP_TYPE obj,
                      FIO_MAP_TYPE *old,
                      uint8_t overwrite);
```


Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location unless `overwrite` is false (in which case, old data isn't overwritten).

#### `MAP_remove` (hash map)

```c
int MAP_remove(FIO_MAP_PTR m,
               FIO_MAP_HASH hash,
               FIO_MAP_KEY key,
               FIO_MAP_TYPE *old);
```

Removes an object from the hash map.

If `old` is given, existing data will be copied to that location.

Returns 0 on success or -1 if the object couldn't be found.

### Set - API (set only)

#### `MAP_get` (set)

```c
FIO_MAP_TYPE MAP_get(FIO_MAP_PTR m,
                     FIO_MAP_HASH hash,
                     FIO_MAP_TYPE obj);
```

Returns the object in the hash map (if any) or `FIO_MAP_TYPE_INVALID`.

#### `MAP_get_ptr` (set)

```c
FIO_MAP_TYPE *MAP_get_ptr(FIO_MAP_PTR m,
                          FIO_MAP_HASH hash,
                          FIO_MAP_TYPE obj);
```

Returns a pointer to the object in the hash map (if any) or NULL.

#### `set_if_missing` (set)

```c
FIO_MAP_TYPE set_if_missing(FIO_MAP_PTR m,
                            FIO_MAP_HASH hash,
                            FIO_MAP_TYPE obj);
```

Inserts an object to the hash map, returning the existing or new object.

If `old` is given, existing data will be copied to that location.

#### `MAP_set` (set)

```c
FIO_MAP_TYPE MAP_set(FIO_MAP_PTR m,
                     FIO_MAP_HASH hash,
                     FIO_MAP_TYPE obj,
                     FIO_MAP_TYPE *old);
```

Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location.

#### `MAP_set_ptr` (set)

```c
FIO_MAP_TYPE *MAP_set(FIO_MAP_PTR m,
                      FIO_MAP_HASH hash,
                      FIO_MAP_TYPE obj,
                      FIO_MAP_TYPE *old);
```

Inserts an object to the hash map, returning the new object.

If `old` is given, existing data will be copied to that location unless `overwrite` is false (in which case, old data isn't overwritten).

#### `MAP_remove` (set)

```c
int MAP_remove(FIO_MAP_PTR m, FIO_MAP_HASH hash,
               FIO_MAP_TYPE obj, FIO_MAP_TYPE *old);
```

Removes an object from the hash map.

If `old` is given, existing data will be copied to that location.

Returns 0 on success or -1 if the object couldn't be found.

#### `MAP_clear`

```c
void MAP_clear(MAP_PTR m);
```

Removes all elements from the Map without freeing the memory used.

Similar to calling:

```c
size_t capa_was = MAP_capa(m);
MAP_destroy(m);
MAP_reserve(m, capa_was);
```

#### `MAP_evict` (set)

```c
int MAP_evict(FIO_MAP_PTR m, size_t number_of_elements);
```

Evicts (removed) `number_of_elements` from the Map.

Eviction is FIFO based (First In First Out) unless FIO_MAP_EVICT_LRU is defined, in which case the Least Recently Used element will be evicted.

Returns 0 on success or -1 on error (i.e., element number bigger than existing element count).

### Hash Map / Set - API (common)

#### `MAP_count`

```c
uintptr_t MAP_count(FIO_MAP_PTR m);
```

Returns the number of objects in the map.

#### `MAP_capa`

```c
uintptr_t MAP_capa(FIO_MAP_PTR m);
```

Returns the current map's theoretical capacity.

#### `MAP_reserve`

```c
uintptr_t MAP_reserve(FIO_MAP_PTR m, uint32_t capa);
```

Reserves a minimal capacity for the hash map.

#### `MAP_compact`

```c
void MAP_compact(FIO_MAP_PTR m);
```

Attempts to lower the map's memory consumption.

#### `MAP_rehash`

```c
int MAP_rehash(FIO_MAP_PTR m);
```

Rehashes the Hash Map / Set. Usually this is performed automatically, no need to call the function.

#### `MAP_each_next`

```c
MAP_node_s * MAP_each_next(FIO_MAP_PTR m, MAP_node_s ** first, MAP_node_s * pos);
```

Returns a pointer to the (next) object's information in the map.

To access the object information, use:

```c
MAP_node_s * pos = MAP_each_next(map, NULL);
```

- `i->hash` to access the hash value.

- `i->obj` to access the object's data.

   For Hash Maps, use `i->obj.key` and `i->obj.value`.

Returns the first object if `pos == NULL` and there are objects in the map.

Returns the next object if `pos` is valid.

Returns NULL if `pos` was the last object or no object exist.

**Note**:

If `pos` is invalid or `NULL`, a pointer to the first object will be returned.

The value of `first` is required and used to revalidate `pos` in cases where object insertion or memory changes occurred while iterating.

The value of `first` is set automatically by the function. Manually changing this value may result in unexpected behavior such as the loop restarting, terminating early, skipping some objects, reiterating some objects or exploding the screen.

#### `MAP_each`

```c
uint32_t MAP_each(FIO_MAP_PTR m,
                  int32_t start_at,
                  int (*task)(MAP_each_s * data),
                  void *arg);
```

Iteration using a callback for each element in the map.

The callback task function must accept a `MAP_each_s` pointer (actual name matches type name).

When the map is a Hash Map (has both a key and an object), the value can be accessed using `obj->value` and the key using `obj->key`. However, changing or altering the contents of the key might break the Hash Map, so do NOT do that.

If the callback returns -1, the loop is broken. Any other value is ignored.

Returns the relative "stop" position, i.e., the number of items processed + the starting point.

The `MAP_each_s` data structure looks like this:

```c
/** Iteration information structure passed to the callback. */
typedef struct MAP_each_s {
  /** The being iterated. Once set, cannot be safely changed. */
  FIO_MAP_PTR const parent;
  /** The current object's index */
  uint64_t index;
  /** Either 1 (set) or 2 (map), and may be used to allow type detection. */
  const int64_t items_at_index;
  /** The callback / task called for each index, may be updated mid-cycle. */
  int (*task)(struct MAP_each_s * info);
  /** Opaque user data. */
  void *udata;
  /** The object / value at the current index. */
  FIO_MAP_TYPE value;
#ifdef FIO_MAP_KEY
  /** The key used to access the specific value. */
  FIO_MAP_KEY key;
#endif
} MAP_each_s;

/**
 * Iteration using a callback for each element in the map.
 *
 * The callback task function must accept an each_s pointer, see above.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
SFUNC FIO_MAP_SIZE_TYPE
    FIO_NAME(FIO_MAP_NAME, each)(FIO_MAP_PTR map,
                                 int (*task)(FIO_NAME(FIO_MAP_NAME, each_s) *),
                                 void *udata,
                                 ssize_t start_at);

```


#### `MAP_each_get_key`

```c
FIO_MAP_KEY MAP_each_get_key(void);
```

Returns the current `key` within an `each` task.

Only available within an `each` loop.

_Note: For sets, returns the hash value, for hash maps, returns the key value._

#### `FIO_MAP_EACH`

```c
#define FIO_MAP_EACH(map_type, map_p, pos)                                    \
  for (FIO_NAME(map_type, node_s) *pos =                                       \
           FIO_NAME(map_type, each_next)(map_p, NULL);                         \
       pos;                                                                    \
       pos = FIO_NAME(map_type, each_next)(map_p, pos))
```

A macro for a `for` loop that iterates over all the Map's objects (in order).

Use this macro for small Hash Maps / Sets.

- `map_type` is the Map's type name/function prefix, same as FIO_MAP_NAME.

- `map_p` is a pointer to the Hash Map / Set variable.

- `pos` is a temporary variable name to be created for iteration. This
   variable may SHADOW external variables, be aware.

To access the object information, use:

- `pos->hash` to access the hash value.

- `pos->obj` to access the object's data.

   For Hash Maps, use `pos->obj.key` and `pos->obj.value`.

-------------------------------------------------------------------------------
## Dynamic Strings

```c
FIO_STR_NAME fio_str
#include "fio-stl.h"
```

Dynamic Strings are extremely useful, since:

* The can safely store binary data (unlike regular C strings).

* They make it easy to edit String data. Granted, the standard C library can do this too, but this approach offers some optimizations and safety measures that the C library cannot offer due to it's historical design.

To create a dynamic string define the type name using the `FIO_STR_NAME` macro.

Alternatively, the type name could be defined using the `FIO_STR_SMALL` macro, resulting in an alternative data structure with a non-default optimization approach (see details later on).

The type (`FIO_STR_NAME_s`) and the functions will be automatically defined.

For brevities sake, in this documentation they will be listed as `STR_*` functions / types (i.e., `STR_s`, `STR_new()`, etc').

### Optimizations / Flavors

Strings come in two main flavors, Strings optimized for mutability (default) vs. Strings optimized for memory consumption (defined using `FIO_STR_SMALL`).

Both optimizations follow specific use-case performance curves that depend on the length of the String data and effect both editing costs and reading costs differently.

#### When to use the default Dynamic Strings (`FIO_STR_NAME`)

The default optimization stores information about the allocated memory's capacity and it is likely to perform best for most generic use-cases, especially when:

* Multiple `write` operations are required.

* It's pre-known that most strings will be longer than a small container's embedded string limit (`(2 * sizeof(char*)) - 2`) and still fit within the default container's embedded string limit (`((4 + FIO_STR_OPTIMIZE_EMBEDDED) * sizeof(char*)) - 2`).

   This is because short Strings are stored directly within a String's data container, minimizing both memory indirection and memory allocation.

   Strings optimized for mutability, by nature, have a larger data container, allowing longer strings to be stored within a container.

   For example, _on 64bit systems_:

   The default (larger) container requires 32 bytes, allowing Strings of up to 30 bytes to be stored directly within the container. This is in contrast to the smaller container (16 bytes in size).

   Two bytes (2 bytes) are used for metadata and a terminating NUL character (to ensure C string safety), leaving the embedded string capacity at 30 bytes for the default container (and 14 bytes for the small one).

   If it's **pre-known** that most strings are likely to be longer than 14 bytes and shorter than 31 bytes (on 64 bit systems), than the default `FIO_STR_NAME` optimization should perform better.

   **Note**: the default container size can be extended by `sizeof(void*)` units using the `FIO_STR_OPTIMIZE_EMBEDDED` macro (i.e., `#define FIO_STR_OPTIMIZE_EMBEDDED 2` will add 16 bytes to the container on 64 bit systems).

#### Example `FIO_STR_NAME` Use-Case

```c
#define FIO_LOG
#define FIO_QUEUE
#include "fio-stl.h"

#define FIO_STR_NAME fio_str
#define FIO_REF_NAME fio_str
#define FIO_REF_CONSTRUCTOR_ONLY
#include "fio-stl.h"

/* this is NOT thread safe... just an example */
void example_task(void *str_, void *ignore_) {
  fio_str_s *str = (fio_str_s *)str_; /* C++ style cast */
  fprintf(stderr, "%s\n", fio_str2ptr(str));
  fio_str_write(str, ".", 1); /* write will sporadically allocate memory if required. */
  fio_str_free(str);          /* decreases reference count or frees object */
  (void)ignore_;
}

void example(void) {
  fio_queue_s queue = FIO_QUEUE_INIT(queue);
  fio_str_s *str = fio_str_new();
  /* writes to the String */
  fio_str_write(str, "Starting time was: ", 19);
  {
    /* reserves space and resizes String, without writing any data */
    const size_t org_len = fio_str_len(str);
    fio_str_info_s str_info = fio_str_resize(str, 29 + org_len);
    /* write data directly to the existing String buffer */
    size_t r = fio_time2rfc7231(str_info.buf + org_len, fio_time_real().tv_sec);
    FIO_ASSERT(r == 29, "this example self destructs at 9999");
  }
  for (size_t i = 0; i < 10; ++i) {
    /* allow each task to hold a reference to the object */
    fio_queue_push(&queue, .fn = example_task, .udata1 = fio_str_dup(str));
  }
  fio_str_free(str);             /* decreases reference count */
  fio_queue_perform_all(&queue); /* performs all tasks */
  fio_queue_destroy(&queue);
}
```

#### When to use the smaller Dynamic Strings (`FIO_STR_SMALL`)

The classic use-case for the smaller dynamic string type is as a `key` in a Map object. The memory "savings" in these cases could become meaningful.

In addition, the `FIO_STR_SMALL` optimization is likely to perform better than the default when Strings are likely to fit within a small container's embedded string limit (`(2 * sizeof(char*)) - 2`), or when Strings are mostly immutable and likely to be too long for the default container's embedded string limit, **and**:

* Strings are likely to require a single `write` operation; **or**

* Strings will point to static memory (`STR_init_const`).

#### Example `FIO_STR_SMALL` Use-Case

```c
#define FIO_STR_SMALL key /* results in the type name: key_s */
#include "fio-stl.h"

#define FIO_OMAP_NAME map
#define FIO_MAP_TYPE uintptr_t
#define FIO_MAP_KEY key_s /* the small string type */
#define FIO_MAP_KEY_COPY(dest, src) key_init_copy2(&(dest), &(src))
#define FIO_MAP_KEY_DESTROY(k) key_destroy(&k)
#define FIO_MAP_KEY_CMP(a, b) key_is_eq(&(a), &(b))
#include "fio-stl.h"

/* helper for setting values in the map using risky hash with a safe seed */
FIO_IFUNC uintptr_t map_set2(map_s *m, key_s key, uintptr_t value) {
  return map_set(m, key_hash(&key, (uintptr_t)m), key, value, NULL);
}

/* helper for getting values from the map using risky hash with a safe seed */
FIO_IFUNC uintptr_t map_get2(map_s *m, key_s key) {
  return map_get(m, key_hash(&key, (uintptr_t)m), key);
}

void example(void) {
  map_s m = FIO_MAP_INIT;
  /* write the long keys twice, to prove they self-destruct in the Hash-Map */
  for (int overwrite = 0; overwrite < 2; ++overwrite) {
    for (int i = 0; i < 10; ++i) {
      const char *prefix = "a long key will require memory allocation: ";
      key_s k;
      key_init_const(&k, prefix, strlen(prefix)); /* points to string literal */
      key_write_hex(&k, i); /* automatically converted into a dynamic string */
      map_set2(&m, k, (uintptr_t)i);
      key_destroy(&k);
    }
  }
  /* short keys don't allocate external memory (string embedded in the object) */
  for (int i = 0; i < 10; ++i) {
    /* short keys fit in pointer + length type... test assumes 64bit addresses */
    const char *prefix = "embed: ";
    key_s k;
    key_init_const(&k, prefix, strlen(prefix)); /* embeds the (short) string */
    key_write_hex(&k, i); /* automatically converted into a dynamic string */
    map_set2(&m, k, (uintptr_t)i);
    key_destroy(&k);
  }
  FIO_MAP_EACH(&m, pos) {
    fprintf(stderr,
            "[%d] %s - memory allocated: %s\n",
            (int)pos->obj.value,
            key2ptr(&pos->obj.key),
            (key_is_allocated(&pos->obj.key) ? "yes" : "no"));
  }
  map_destroy(&m);
  /* test for memory leaks using valgrind or similar */
}
```
### String Type information

#### `STR_s`

The core type, created by the macro, is the `STR_s` type - where `STR` is replaced by `FIO_STR_NAME`. i.e.:

```c
#define FIO_STR_NAME my_str
#include <fio-stl.h>
// results in: my_str_s - i.e.:
void hello(void){
  my_str_s msg = FIO_STR_INIT;
  my_str_write(&msg, "Hello World", 11);
  printf("%s\n", my_str2ptr(&msg));
  my_str_destroy(&msg);
}
```

The type should be considered **opaque** and **must never be accessed directly**.

The type's attributes should be accessed ONLY through the accessor functions: `STR_info`, `STR_len`, `STR2ptr`, `STR_capa`, etc'.

This is because: Small strings that fit into the type directly use the type itself for memory (except the first and last bytes). Larger strings use the type fields for the string's meta-data. Depending on the string's data, the type behaves differently.

#### `fio_str_info_s` - revisited

Some functions return information about a string's state using the [`fio_str_info_s` type detailed above](#fio_str_info_s). As a reminder, it looks like this:

```c
typedef struct fio_str_info_s {
  char *buf;   /* The string's buffer (pointer to first byte) or NULL on error. */
  size_t len;  /* The string's length, if any. */
  size_t capa; /* The buffer's capacity. Zero (0) indicates the buffer is read-only. */
} fio_str_info_s;
```

This information type, accessible using the `STR_info` function, allows direct access and manipulation of the string data. Changes in string length should be followed by a call to `STR_resize`.

The data in the string object is always NUL terminated. However, string data might contain binary data, where NUL is a valid character, so using C string functions isn't advised.

Equality can be tested using the [`FIO_STR_INFO_IS_EQ` macro](FIO_STR_INFO_IS_EQ).

See [Binary Data Informational Types and Helpers](#binary-data-informational-types-and-helpers) for more details.

#### String allocation alignment / `FIO_STR_NO_ALIGN`

Memory allocators have allocation alignment concerns that require minimum space to be allocated.

The default `STR_s` type makes use of this extra space for small strings, fitting them into the type.

To prevent this behavior and minimize the space used by the `STR_s` type, set the `FIO_STR_NO_ALIGN` macro to `1`.

```c
#define FIO_STR_NAME big_string
#define FIO_STR_NO_ALIGN 1
#include <fio-stl.h>
// ...
big_string_s foo = FIO_STR_INIT;
```

This could save memory when strings aren't short enough to be contained within the type.

This could also save memory, potentially, if the string type will be wrapped / embedded within other data-types (i.e., using `FIO_REF_NAME` for reference counting).

### String API - Initialization and Destruction

#### `FIO_STR_INIT`

This value should be used for initialization. It should be considered opaque, but is defined as:

```c
#define FIO_STR_INIT { .special = 1 }
```

For example:

```c
#define FIO_STR_NAME fio_str
#include <fio-stl.h>
void example(void) {
  // on the stack
  fio_str_s str = FIO_STR_INIT;
  // .. 
  fio_str_destroy(&str);
}
```

#### `FIO_STR_INIT_EXISTING`

This macro allows the container to be initialized with existing data.

```c
#define FIO_STR_INIT_EXISTING(buffer, length, capacity,)              \
  { .buf = (buffer), .len = (length), .capa = (capacity) }
```
The `capacity` value should exclude the space required for the NUL character (if exists).

Memory should be dynamically allocated using the same allocator selected for the String type (see `FIO_MALLOC` / `FIO_MEM_REALLOC` / `FIO_MEM_FREE`).

#### `FIO_STR_INIT_STATIC`

This macro allows the string container to be initialized with existing static data, that shouldn't be freed.

```c
#define FIO_STR_INIT_STATIC(buffer)                                            \
  { .special = 4, .buf = (char *)(buffer), .len = strlen((buffer)) }
```

#### `FIO_STR_INIT_STATIC2`

This macro allows the string container to be initialized with existing static data, that shouldn't be freed.

```c
#define FIO_STR_INIT_STATIC2(buffer, length)                                   \
  { .buf = (char *)(buffer), .len = (length) }
```


#### `STR_init_const`

```c
fio_str_info_s STR_init_const(FIO_STR_PTR s,
                              const char *str,
                              size_t len);
```

Initializes the container with a pointer to the provided static / constant string.

The string will be copied to the container **only** if it will fit in the container itself. 

Otherwise, the supplied pointer will be used as is **and must remain valid until the string is destroyed** (or written to, at which point the data is duplicated).

The final string can be safely be destroyed (using the `STR_destroy` function).

#### `STR_init_copy`

```c
fio_str_info_s STR_init_copy(FIO_STR_PTR s,
                             const char *str,
                             size_t len);
```

Initializes the container with a copy of the `src` string.

The string is always copied and the final string must be destroyed (using the `destroy` function).

#### `STR_init_copy2`

```c
fio_str_info_s STR_init_copy2(FIO_STR_PTR dest,
                             FIO_STR_PTR src);
```

Initializes the `dest` container with a copy of the `src` String object's content.

The `src` metadata, such as `freeze` state, is ignored - resulting in a mutable String object.

The string is always copied and the final string must be destroyed (using the `destroy` function).

#### `STR_destroy`

```c
void STR_destroy(FIO_STR_PTR s);
```

Frees the String's resources and reinitializes the container.

Note: if the container isn't allocated on the stack, it should be freed separately using the appropriate `free` function, such as `STR_free`.

#### `STR_new`

```c
FIO_STR_PTR STR_new(void);
```

Allocates a new String object on the heap.

#### `STR_free`

```c
void STR_free(FIO_STR_PTR s);
```

Destroys the string and frees the container (if allocated with `STR_new`).

#### `STR_detach`

```c
char * STR_detach(FIO_STR_PTR s);
```

Returns a C string with the existing data, **re-initializing** the String.

The returned C string is **always dynamic** and **must be freed** using the same memory allocator assigned to the type (i.e., `free` or `fio_free`, see [`FIO_MALLOC`](#local-memory-allocation), [`FIO_MEM_REALLOC`](#FIO_MEM_REALLOC) and [`FIO_MALLOC_TMP_USE_SYSTEM`](#FIO_MALLOC_TMP_USE_SYSTEM))

**Note**: the String data is removed from the container, but the container is **not** freed.

Returns NULL if there's no String data.

### String API - String state (data pointers, length, capacity, etc')

#### `STR_info`

```c
fio_str_info_s STR_info(const FIO_STR_PTR s);
```

Returns the String's complete state (capacity, length and pointer). 

#### `STR_len`

```c
size_t STR_len(FIO_STR_PTR s);
```

Returns the String's length in bytes.

#### `STR2ptr`

```c
char *STR2ptr(FIO_STR_PTR s);
```

Returns a pointer (`char *`) to the String's content (first character in the string).

#### `STR_capa`

```c
size_t STR_capa(FIO_STR_PTR s);
```

Returns the String's existing capacity (total used & available memory).

#### `STR_freeze`

```c
void STR_freeze(FIO_STR_PTR s);
```

Prevents further manipulations to the String's content.

#### `STR_is_frozen`

```c
uint8_t STR_is_frozen(FIO_STR_PTR s);
```

Returns true if the string is frozen.

#### `STR_is_allocated`

```c
int STR_is_allocated(const FIO_STR_PTR s);
```

Returns 1 if memory was allocated and (the String must be destroyed).

#### `STR_is_eq`

```c
int STR_is_eq(const FIO_STR_PTR str1, const FIO_STR_PTR str2);
```

Binary comparison returns `1` if both strings are equal and `0` if not.

#### `STR_hash`

```c
uint64_t STR_hash(const FIO_STR_PTR s);
```

Returns the string's Risky Hash value.

Note: Hash algorithm might change without notice.

### String API - Memory management

#### `STR_resize`

```c
fio_str_info_s STR_resize(FIO_STR_PTR s, size_t size);
```

Sets the new String size without reallocating any memory (limited by existing capacity).

Returns the updated state of the String.

Note: When shrinking, any existing data beyond the new size may be corrupted or lost.

#### `STR_compact`

```c
void STR_compact(FIO_STR_PTR s);
```

Performs a best attempt at minimizing memory consumption.

Actual effects depend on the underlying memory allocator and it's implementation. Not all allocators will free any memory.

#### `STR_reserve`

```c
fio_str_info_s STR_reserve(FIO_STR_PTR s, size_t amount);
```

Reserves at least `amount` of bytes for the string's data (reserved count includes used data).

Returns the current state of the String.

**Note**: Doesn't exist for `FIO_STR_SMALL` types, since capacity can't be reserved in advance (either use `STR_resize` and write data manually or suffer a performance penalty when performing multiple `write` operations).

### String API - UTF-8 State

#### `STR_utf8_valid`

```c
size_t STR_utf8_valid(FIO_STR_PTR s);
```

Returns 1 if the String is UTF-8 valid and 0 if not.

#### `STR_utf8_len`

```c
size_t STR_utf8_len(FIO_STR_PTR s);
```

Returns the String's length in UTF-8 characters.

#### `STR_utf8_select`

```c
int STR_utf8_select(FIO_STR_PTR s, intptr_t *pos, size_t *len);
```

Takes a UTF-8 character selection information (UTF-8 position and length) and updates the same variables so they reference the raw byte slice information.

If the String isn't UTF-8 valid up to the requested selection, than `pos` will be updated to `-1` otherwise values are always positive.

The returned `len` value may be shorter than the original if there wasn't enough data left to accommodate the requested length. When a `len` value of `0` is returned, this means that `pos` marks the end of the String.

Returns -1 on error and 0 on success.

### String API - Content Manipulation and Review

#### `STR_write`

```c
fio_str_info_s STR_write(FIO_STR_PTR s, const void *src, size_t src_len);
```

Writes data at the end of the String.

#### `STR_write_i`

```c
fio_str_info_s STR_write_i(FIO_STR_PTR s, int64_t num);
```

Writes a number at the end of the String using normal base 10 notation.

#### `STR_write_hex`

```c
fio_str_info_s STR_write_hex(FIO_STR_PTR s, int64_t num);
```

Writes a number at the end of the String using Hex (base 16) notation.

**Note**: the `0x` prefix **is automatically written** before the hex numerals.

#### `STR_concat` / `STR_join`

```c
fio_str_info_s STR_concat(FIO_STR_PTR dest, FIO_STR_PTR const src);
```

Appends the `src` String to the end of the `dest` String. If `dest` is empty, the resulting Strings will be equal.

`STR_join` is an alias for `STR_concat`.


#### `STR_replace`

```c
fio_str_info_s STR_replace(FIO_STR_PTR s,
                           intptr_t start_pos,
                           size_t old_len,
                           const void *src,
                           size_t src_len);
```

Replaces the data in the String - replacing `old_len` bytes starting at `start_pos`, with the data at `src` (`src_len` bytes long).

Negative `start_pos` values are calculated backwards, `-1` == end of String.

When `old_len` is zero, the function will insert the data at `start_pos`.

If `src_len == 0` than `src` will be ignored and the data marked for replacement will be erased.

#### `STR_vprintf`

```c
fio_str_info_s STR_vprintf(FIO_STR_PTR s, const char *format, va_list argv);
```

Writes to the String using a vprintf like interface.

Data is written to the end of the String.

#### `STR_printf`

```c
fio_str_info_s STR_printf(FIO_STR_PTR s, const char *format, ...);
```

Writes to the String using a printf like interface.

Data is written to the end of the String.

#### `STR_readfd`

```c
fio_str_info_s STR_readfd(FIO_STR_PTR s,
                            int fd,
                            intptr_t start_at,
                            intptr_t limit);
```

Reads data from a file descriptor `fd` at offset `start_at` and pastes it's contents (or a slice of it) at the end of the String. If `limit == 0`, than the data will be read until EOF.

The file should be a regular file or the operation might fail (can't be used for sockets).

**Note**: the file descriptor will remain open and should be closed manually.

#### `STR_readfile`

```c
fio_str_info_s STR_readfile(FIO_STR_PTR s,
                            const char *filename,
                            intptr_t start_at,
                            intptr_t limit);
```

Opens the file `filename` and pastes it's contents (or a slice ot it) at the end of the String. If `limit == 0`, than the data will be read until EOF.

If the file can't be located, opened or read, or if `start_at` is beyond the EOF position, NULL is returned in the state's `data` field.

### String API - Base64 support

#### `STR_write_b64enc`

```c
fio_str_info_s STR_write_b64enc(FIO_STR_PTR s,
                                const void *data,
                                size_t data_len,
                                uint8_t url_encoded);
```

Writes data at the end of the String, encoding the data as Base64 encoded data.

#### `STR_write_b64dec`

```c
fio_str_info_s STR_write_b64dec(FIO_STR_PTR s,
                                const void *encoded,
                                size_t encoded_len);
```

Writes decoded Base64 data to the end of the String.


### String API - escaping / JSON encoding support

#### `STR_write_escape`

```c
fio_str_info_s STR_write_escape(FIO_STR_PTR s,
                                const void *data,
                                size_t data_len);

```

Writes data at the end of the String, escaping the data using JSON semantics.

The JSON semantic are common to many programming languages, promising a UTF-8 String while making it easy to read and copy the string during debugging.

#### `STR_write_unescape`

```c
fio_str_info_s STR_write_unescape(FIO_STR_PTR s,
                                  const void *escaped,
                                  size_t len);
```

Writes an escaped data into the string after unescaping the data.

-------------------------------------------------------------------------------
## Reference Counting and Type Wrapping

```c
#define FIO_STR_SMALL fio_str
#define FIO_REF_NAME fio_str
#define FIO_REF_CONSTRUCTOR_ONLY
#include "fio-stl.h"
```

If the `FIO_REF_NAME` macro is defined, then reference counting helpers can be defined for any named type.

**Note**: requires the atomic operations to be defined (`FIO_ATOMIC`).

### Reference Counting Type Macros

The following setup Macros are supported when setting up the reference counting type helpers:

#### `FIO_REF_TYPE`

```c
#define FIO_REF_TYPE FIO_NAME(FIO_REF_NAME, s)
```

The type to be wrapped and reference counted by the `FIO_REF_NAME` wrapper API.

By default, `FIO_REF_TYPE` will equal `FIO_REF_NAME_s`, using the naming convention in this library.

#### `FIO_REF_INIT`

```c
#define FIO_REF_INIT(obj) (obj) = (FIO_REF_TYPE){0}
```

Sets up the default object initializer.

By default initializes the object's memory to zero.

If `FIO_REF_FLEX_TYPE` is defined, the variable `members` may be used during initialization. It's value is the same as the value passed on to the `REF_new` function.

**Note**:  `FIO_REF_FLEX_TYPE` should **not** be used when `FIO_MEM_FREE` macro only frees the number of bytes specified (rather than freeing the whole pointer, as `free` might do). The reference counter type does not store the data passed to the flex-`REF_new` function and frees the same number of bytes as a flex length of `0`.

#### `FIO_REF_DESTROY`

```c
#define FIO_REF_DESTROY(obj)
```

Sets up the default object cleanup. By default does nothing.

#### `FIO_REF_CONSTRUCTOR_ONLY`

By default, the reference counter generator will generate the `new2`, `free2` and `dup2` functions.

However, f the `FIO_REF_CONSTRUCTOR_ONLY` macro is defined, the reference counter will name these functions as `new`, `free` and `dup` instead, making them the type's only and primary constructor / destructor.

#### `FIO_REF_FLEX_TYPE`

If the `FIO_REF_FLEX_TYPE` macro is defined, the constructor will allocate a enough memory for both the type and a `FIO_REF_FLEX_TYPE` array consisting of the specified amount of members (as passed to the constructor's `member` argument).

This allows reference objects structures to include a flexible array of type `FIO_REF_FLEX_TYPE` at the end of the `struct`.

The `members` variable passed to the constructor will also be available to the `FIO_REF_INIT` macro.

#### `FIO_REF_METADATA`

If defined, should be type that will be available as "meta data".

A pointer to this type sill be available using the `REF_metadata` function and will allow "hidden" data to be accessible even though it isn't part of the observable object.

#### `FIO_REF_METADATA_INIT`

```c
#define FIO_REF_METADATA_INIT(meta) (meta) = (FIO_REF_TYPE){0}
```

Sets up object's meta-data initialization (if any). Be default initializes the meta-data object's memory to zero.

#### `FIO_REF_METADATA_DESTROY`

```c
#define FIO_REF_METADATA_DESTROY(meta)
```

### Reference Counting Generated Functions

Reference counting adds the following functions:

#### `REF_new` / `REF_new2`

```c
FIO_REF_TYPE * REF_new2(void)
// or, if FIO_REF_FLEX_TYPE is defined:
FIO_REF_TYPE * REF_new2(size_t members)


// or, if FIO_REF_CONSTRUCTOR_ONLY is defined
FIO_REF_TYPE * REF_new(void) 
FIO_REF_TYPE * REF_new(size_t members) // for FIO_REF_FLEX_TYPE

```

Allocates a new reference counted object, initializing it using the `FIO_REF_INIT(object)` macro.

If `FIO_REF_METADATA` is defined, than the metadata is initialized using the `FIO_REF_METADATA_INIT(metadata)` macro.

#### `REF_dup`

```c
FIO_REF_TYPE * REF_dup(FIO_REF_TYPE * object)
```

Increases an object's reference count (an atomic operation, thread-safe).

#### `REF_free` / `REF_free2`

```c
void REF_free2(FIO_REF_TYPE * object)
// or, if FIO_REF_CONSTRUCTOR_ONLY is defined
void REF_free(FIO_REF_TYPE * object)
```

Frees an object or decreases it's reference count (an atomic operation, thread-safe).

Before the object is freed, the `FIO_REF_DESTROY(object)` macro will be called.

If `FIO_REF_METADATA` is defined, than the metadata is also destroyed using the `FIO_REF_METADATA_DESTROY(metadata)` macro.

#### `REF_metadata`

```c
FIO_REF_METADATA * REF_metadata(FIO_REF_TYPE * object)
```

If `FIO_REF_METADATA` is defined, than the metadata is accessible using this inlined function.

-------------------------------------------------------------------------------
## FIOBJ Soft Dynamic Types

```c
#define FIO_FIOBJ
#define FIOBJ_MALLOC /* an optional local memory allocator for FIOBJ types */
#include "fio-stl.h"
```

The facil.io library includes a dynamic type system that makes it a easy to handle mixed-type tasks, such as JSON object construction.

This soft type system included in the facil.io STL, it is based on the Core types mentioned above and it shares their API (Dynamic Strings, Dynamic Arrays, and Hash Maps).

The soft type system also offers an (optional) * [Local Memory allocator](#local-memory-allocation) for improved performance when defined with the `FIOBJ_MALLOC` macro defined.

The `FIOBJ` API offers type generic functions in addition to the type specific API. An objects underlying type is easily identified using `FIOBJ_TYPE(obj)` or `FIOBJ_TYPE_IS(obj, type)`.

The documentation regarding the `FIOBJ` soft-type system is divided as follows:  

* [`FIOBJ` General Considerations](#fiobj-general-considerations)

* [`FIOBJ` Types and Identification](#fiobj-types-and-identification)

* [`FIOBJ` Core Memory Management](#fiobj-core-memory-management)

* [`FIOBJ` Common Functions](#fiobj-common-functions)

* [Primitive Types](#fiobj-primitive-types)

* [Numbers (Integers)](#fiobj-integers)

* [Floats](#fiobj-floats)

* [Strings](#fiobj-strings)

* [Arrays](#fiobj-arrays)

* [Hash Maps](#fiobj-hash-maps)

* [JSON Helpers](#fiobj-json-helpers)

* [How to Extend the `FIOBJ` Type System](#how-to-extend-the-fiobj-type-system)

In the facil.io web application framework, there are extensions to the core `FIOBJ` primitives, including:

* [IO storage](fiobj_io)

* [Mustache](fiobj_mustache)

### `FIOBJ` General Considerations

1. To use the `FIOBJ` soft types, define the `FIO_FIOBJ` macro and then include the facil.io STL header.

2. To include declarations as globally available symbols (allowing the functions to be called from multiple C files), define `FIOBJ_EXTERN` _before_ including the STL header.

    This also requires that a _single_ C file (translation unit) define `FIOBJ_EXTERN_COMPLETE` _before_ including the header with the `FIOBJ_EXTERN` directive.

3. The `FIOBJ` types use pointer tagging and require that the memory allocator provide allocations on 8 byte memory alignment boundaries (they also assume each byte is 8 bits).

    If the system allocator doesn't provide (at least) 8 byte memory alignment, use the facil.io memory allocator provided (`fio_malloc`).

4. The `FIOBJ` soft type system uses an "**ownership**" memory model.

    This means that Arrays "**own**" their **members** and Hash Maps "**own**" their **values** (but **not** the keys).

    Freeing an Array will free all the objects within the Array. Freeing a Hash Map will free all the values within the Hash Map (but none of the keys).

    Ownership is only transferred if the object is removed from it's container.

    i.e., `fiobj_array_get` does **not** transfer ownership (it just allows temporary "access"). Whereas, `fiobj_array_remove` **does** revoke ownership - either freeing the object or moving the ownership to the pointer provided to hold the `old` value.

### `FIOBJ` Types and Identification

`FIOBJ` objects can contain any number of possible types, including user defined types.

These are the built-in types / classes that the Core `FIOBJ` system includes (before any extensions):

* `FIOBJ_T_INVALID`: indicates an **invalid** type class / type (a `FIOBJ_INVALID` value).

* `FIOBJ_T_PRIMITIVE`: indicates a **Primitive** class / type.

* `FIOBJ_T_NUMBER`: indicates a **Number** class / type.

* `FIOBJ_T_FLOAT`: indicates a **Float** class / type.

* `FIOBJ_T_STRING`: indicates a **String** class / type.

* `FIOBJ_T_ARRAY`: indicates an **Array** class / type.

* `FIOBJ_T_HASH`: indicates a **Hash Map** class / type.

* `FIOBJ_T_OTHER`: (internal) indicates an **Other** class / type. This is designed to indicate an extension / user defined type.

The `FIOBJ_T_PRIMITIVE` class / type resolves to one of the following types:

* `FIOBJ_T_NULL`: indicates a `fiobj_null()` object.

* `FIOBJ_T_TRUE`: indicates a `fiobj_true()` object.

* `FIOBJ_T_FALSE`: indicates a `fiobj_false()` object.

In the facil.io web application framework, there are extensions to the core `FIOBJ` primitives, including:

* [`FIOBJ_T_IO`](fiobj_io)

The following functions / MACROs help identify a `FIOBJ` object's underlying type.

#### `FIOBJ_TYPE(o)`

```c
#define FIOBJ_TYPE(o) fiobj_type(o)
```

#### `FIOBJ_TYPE_IS(o)`

```c
#define FIOBJ_TYPE_IS(o, type) (fiobj_type(o) == type)
```

#### `FIOBJ_TYPE_CLASS(o)`

```c
#define FIOBJ_TYPE_CLASS(o) ((fiobj_class_en)(((uintptr_t)o) & 7UL))
```

Returns the object's type class. This is limited to one of the core types. `FIOBJ_T_PRIMITIVE` and `FIOBJ_T_OTHER` may be returned (they aren't expended to their underlying type).

**Note**: some numbers (`FIOBJ_T_NUMBER` / `FIOBJ_T_FLOAT`) may return `FIOBJ_T_OTHER` when `FIOBJ_TYPE_CLASS` is used, but return their proper type when `FIOBJ_TYPE` is used. This is due to memory optimizations being unavailable for some numerical values.

#### `FIOBJ_IS_INVALID(o)`

```c
#define FIOBJ_IS_INVALID(o) (((uintptr_t)(o)&7UL) == 0)
```

Tests if the object is (probably) a valid FIOBJ

#### `FIOBJ_IS_INVALID(o)`

```c
#define FIOBJ_IS_INVALID(o) (((uintptr_t)(o)&7UL) == 0)
```

#### `FIOBJ_PTR_UNTAG(o)`

```c
#define FIOBJ_PTR_UNTAG(o) ((uintptr_t)o & (~7ULL))
```

Removes the `FIOBJ` type tag from a `FIOBJ` objects, allowing access to the underlying pointer and possible type.

This is made available for authoring `FIOBJ` extensions and **shouldn't** be normally used.

#### `fiobj_type`

```c
size_t fiobj_type(FIOBJ o);
```

Returns an objects type. This isn't limited to known types.

Avoid calling this function directly. Use the MACRO instead.

### `FIOBJ` Core Memory Management

`FIOBJ` objects are **copied by reference** (not by value). Once their reference count is reduced to zero, their memory is freed.

This is extremely important to note, especially in multi-threaded environments. This implied that: **access to a dynamic `FIOBJ` object is _NOT_ thread-safe** and `FIOBJ` objects that may be written to (such as Arrays, Strings and Hash Maps) should **not** be shared across threads (unless properly protected).

The `FIOBJ` soft type system uses an "**ownership**" memory model. When placing a **value** in an Array or a Hash Map, the "ownership" is moved. Freeing the Array / Hash Map will free the object (unless `fiobj_dup` was called). Hash Maps "**own**" their _values_ (but **not** the _keys_).

#### `fiobj_dup`

```c
FIOBJ fiobj_dup(FIOBJ o);
```

Increases an object's reference count and returns it.

#### `fiobj_free`

```c
void fiobj_free(FIOBJ o);
```

Decreases an object's reference count or frees it.

**Note**:

This function is **recursive** and could cause a **stack explosion** error.

In addition, recursive object structures may produce unexpected results (for example, objects are always freed).

The `FIOBJ_MAX_NESTING` nesting limit doesn't apply to `fiobj_free`, making it possible to "expload" the stack if misused.

This places the responsibility on the user / developer, not to exceed the maximum nesting limit (or errors may occur).

When accepting external data, consider using the JSON parser, as it protects against this issue, offering a measure of safety against external data attacks.

### `FIOBJ` Common Functions

#### `fiobj_is_eq`

```c
unsigned char fiobj_is_eq(FIOBJ a, FIOBJ b);
```

Compares two objects.

Note: objects that contain other objects (i.e., Hash Maps) don't support this equality check just yet (feel free to contribute a PR for this).

#### `fiobj2cstr`

```c
fio_str_info_s fiobj2cstr(FIOBJ o);
```

Returns a temporary String representation for any FIOBJ object.

For number objects and floats this is thread safe for up to 256 threads.

For printing Arrays and Hash maps, using a JSON representation will provide more information.

#### `fiobj2i`

```c
intptr_t fiobj2i(FIOBJ o);
```

Returns an integer representation for any FIOBJ object.

#### `fiobj2f`

```c
double fiobj2f(FIOBJ o);
```

Returns a float (double) representation for any FIOBJ object.


#### `fiobj_each1`

```c
uint32_t fiobj_each1(FIOBJ o, int32_t start_at,
                     int (*task)(FIOBJ child, void *arg),
                     void *arg);
```

Performs a task for each element held by the FIOBJ object **directly** (but **not** itself).

If `task` returns -1, the `each` loop will break (stop).

Returns the "stop" position - the number of elements processed + `start_at`.


#### `fiobj_each2`

```c
uint32_t fiobj_each2(FIOBJ o,
                     int (*task)(FIOBJ obj, void *arg),
                     void *arg);
```

Performs a task for each element held by the FIOBJ object (directly or indirectly), **including** itself and any nested elements (a deep task).

The order of performance is by order of appearance, as if all nesting levels were flattened.

If `task` returns -1, the `each` loop will break (stop).

Returns the number of elements processed.

**Note**:

This function is **recursive** and could cause a **stack explosion** error.

The facil.io library attempts to protect against this error by limiting recursive access to `FIOBJ_MAX_NESTING`... however, this also assumes that a user / developer doesn't exceed the maximum nesting limit (or errors may occur).

#### `fiobj_json_find`

```c
FIOBJ fiobj_json_find(FIOBJ object, fio_str_info_s notation);
```

Uses JavaScript (JSON) style notation to find data in an object structure.

For example, `"[0].name"` will return the `"name"` property of the first object in an Array object.

Returns a temporary reference to the object or `FIOBJ_INVALID` on an error.

Use `fiobj_dup` to collect an actual reference to the returned object.

**Note**:

Using the search algorithm with long object names and/or deeper nesting levels might incur a performance penalty due to the fact that the algorithm tests for all possible object name permutations.

i.e., `"name1.name2.name3"` will first be tested as the whole string (`"name1.name2.name3"`), then `"name1.name2" + "name.3"` will be tested, then `"name1" + "name2.name.3"` will be tested for and `"name1" + "name2" + "name3"` will only be attempted last (allowing all permutations to be reviewed rather than assuming a `.` is always a delimiter).

#### `fiobj_json_find2`

```c
#define fiobj_json_find2(object, str, length)                                  \
  fiobj_json_find(object, (fio_str_info_s){.buf = str, .len = length})
```

A macro helper for [`fiobj_json_find`](#fiobj_json_find).

### `FIOBJ` Primitive Types

The `true`, `false` and `null` primitive type functions (in addition to the common functions) are only their simple static constructor / accessor functions.

The primitive types are immutable.

#### `fiobj_true`

```c
FIOBJ fiobj_true(void);
```

Returns the `true` primitive.

#### `fiobj_false`

```c
FIOBJ fiobj_false(void);
```

Returns the `false` primitive.

#### `fiobj_null`

```c
FIOBJ fiobj_null(void);
```

Returns the `nil` / `null` primitive.


### `FIOBJ` Integers

#### `fiobj_num_new`

```c
FIOBJ fiobj_num_new(intptr_t i);
```

Creates a new Number object.

#### `fiobj_num2i`

```c
intptr_t fiobj_num2i(FIOBJ i);
```

Reads the number from a `FIOBJ` Number.

#### `fiobj_num2f`

```c
double fiobj_num2f(FIOBJ i);
```

Reads the number from a `FIOBJ` Number, fitting it in a double.

#### `fiobj_num2cstr`

```c
fio_str_info_s fiobj_num2cstr(FIOBJ i);
```

Returns a String representation of the number (in base 10).

#### `fiobj_num_free`

```c
void fiobj_num_free(FIOBJ i);
```

Frees a `FIOBJ` number (a type specific `fiobj_free` alternative - use only when the type was validated).


### `FIOBJ` Floats

#### `fiobj_float_new`

```c
FIOBJ fiobj_float_new(double i);
```

Creates a new Float (double) object.

#### `fiobj_float2i`

```c
intptr_t fiobj_float2i(FIOBJ i);
```

Reads the number from a `FIOBJ` Float rounding it to an integer.

#### `fiobj_float2f`

```c
double fiobj_float2f(FIOBJ i);
```

Reads the value from a `FIOBJ` Float, as a double.

#### `fiobj_float2cstr`

```c
fio_str_info_s fiobj_float2cstr(FIOBJ i);
```

Returns a String representation of the float.

#### `fiobj_float_free`

```c
void fiobj_float_free(FIOBJ i);
```

Frees a `FIOBJ` Float (a type specific `fiobj_free` alternative - use only when the type was validated).


### `FIOBJ` Strings

`FIOBJ` Strings are based on the core `STR_x` functions. This means that all these core type functions are available also for this type, using the `fiobj_str` prefix (i.e., [`STR_new`](#str_new) becomes [`fiobj_str_new`](#str_new), [`STR_write`](#str_write) becomes [`fiobj_str_write`](#str_write), etc').

In addition, the following `fiobj_str` functions and MACROs are defined:

#### `fiobj_str_new_cstr`

```c
FIOBJ fiobj_str_new_cstr(const char *ptr, size_t len);
```

Creates a new `FIOBJ` string object, copying the data to the new string.


#### `fiobj_str_new_buf`

```c
FIOBJ fiobj_str_new_buf(size_t capa);
```

Creates a new `FIOBJ` string object with (at least) the requested capacity.


#### `fiobj_str_new_copy`

```c
FIOBJ fiobj_str_new_copy(FIOBJ original);
```

Creates a new `FIOBJ` string object, copying the origin ([`fiobj2cstr`](#fiobj2cstr)).


#### `fiobj_str2cstr`

```c
fio_str_info_s fiobj_str2cstr(FIOBJ s);
```

Returns information about the string. Same as [`fiobj_str_info()`](#str_info).

#### `FIOBJ_STR_TEMP_DESTROY(name)`

```c
#define FIOBJ_STR_TEMP_DESTROY(str_name)  \
  FIO_NAME(fiobj_str, destroy)(str_name);
```

Resets a temporary `FIOBJ` String, freeing and any resources allocated.

See the following `FIOBJ_STR_TEMP_XXX` macros for creating temporary FIOBJ strings on the Stack.

#### `FIOBJ_STR_TEMP_VAR(name)`

```c
#define FIOBJ_STR_TEMP_VAR(str_name)                                   \
  struct {                                                             \
    uint64_t i1;                                                       \
    uint64_t i2;                                                       \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;               \
  } FIO_NAME(str_name, __auto_mem_tmp) = {                             \
      0x7f7f7f7f7f7f7f7fULL, 0x7f7f7f7f7f7f7f7fULL, FIO_STR_INIT};     \
  FIOBJ str_name =                                                     \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) | \
              FIOBJ_T_STRING);
```

Creates a temporary `FIOBJ` String object on the stack.

String data might be allocated dynamically, requiring the use of `FIOBJ_STR_TEMP_DESTROY`.

#### `FIOBJ_STR_TEMP_VAR_STATIC(str_name, buf, len)`

```c
#define FIOBJ_STR_TEMP_VAR_STATIC(str_name, buf_, len_)                        \
  struct {                                                                     \
    uint64_t i1;                                                               \
    uint64_t i2;                                                               \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;                       \
  } FIO_NAME(str_name,                                                         \
             __auto_mem_tmp) = {0x7f7f7f7f7f7f7f7fULL,                         \
                                0x7f7f7f7f7f7f7f7fULL,                         \
                                FIO_STR_INIT_STATIC2((buf_), (len_))};         \
  FIOBJ str_name =                                                             \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) |         \
              FIOBJ_T_STRING);
```

Creates a temporary FIOBJ String object on the stack, initialized with a static string.

Editing the String data **will** cause dynamic memory allocation, use `FIOBJ_STR_TEMP_DESTROY` once done.

This variation will cause memory allocation immediately upon editing the String. The buffer _MAY_ be read only.

#### `FIOBJ_STR_TEMP_VAR_EXISTING(str_name, buf, len, capa)`

```c
#define FIOBJ_STR_TEMP_VAR_EXISTING(str_name, buf_, len_, capa_)               \
  struct {                                                                     \
    uint64_t i1;                                                               \
    uint64_t i2;                                                               \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;                       \
  } FIO_NAME(str_name, __auto_mem_tmp) = {                                     \
      0x7f7f7f7f7f7f7f7fULL,                                                   \
      0x7f7f7f7f7f7f7f7fULL,                                                   \
      FIO_STR_INIT_EXISTING((buf_), (len_), (capa_))};                         \
  FIOBJ str_name =                                                             \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) |         \
              FIOBJ_T_STRING);
```

Creates a temporary FIOBJ String object on the stack for a read/write buffer with the specified capacity.

Editing the String data might cause dynamic memory allocation, use `FIOBJ_STR_TEMP_DESTROY` once done.

Remember to manage the buffer's memory once it was de-linked from the temporary string (as the FIOBJ object does **not** take ownership of the memory).

#### `FIOBJ` Strings - Core Type Functions

In addition, all the functions documented above as `STR_x`, are defined as `fiobj_str_x`:

* [`fiobj_str_new`](#str_new) - creates a new empty string.

* [`fiobj_str_free`](#str_free) - frees a FIOBJ known to be a String object.

* [`fiobj_str_destroy`](#str_destroy) - destroys / clears a String, returning it to an empty state. 

* [`fiobj_str_detach`](#str_detach) - destroys / clears a String, returning a `char *` C-String.

* [`fiobj_str_info`](#str_info) - returns information about the string.

* [`fiobj_str_len`](#str_len) - returns the string's length.

* [`fiobj_str2ptr`](#str2ptr) - returns a pointer to the string's buffer.

* [`fiobj_str_capa`](#str_capa) - returns the string's capacity.

* [`fiobj_str_freeze`](#str_freeze) - freezes a string (a soft flag, enforced only by functions).

* [`fiobj_str_is_frozen`](#str_is_frozen) - returns true if the string is frozen.

* [`fiobj_str_is_eq`](#str_is_eq) - returns true if the strings are equal.

* [`fiobj_str_hash`](#str_hash) - returns a string's Risky Hash.

* [`fiobj_str_resize`](#str_resize) - resizes a string (keeping the current buffer).

* [`fiobj_str_compact`](#str_compact) - attempts to minimize memory usage.

* [`fiobj_str_reserve`](#str_reserve) - reserves memory for future `write` operations.

* [`fiobj_str_utf8_valid`](#str_utf8_valid) - tests in a string is UTF8 valid.

* [`fiobj_str_utf8_len`](#str_utf8_len) - returns a string's length in UTF8 characters.

* [`fiobj_str_utf8_select`](#str_utf8_select) - selects a section of the string using UTF8 offsets.

* [`fiobj_str_write`](#str_write) - writes data to the string.

* [`fiobj_str_write_i`](#str_write_i) - writes a base 10 number to the string.

* [`fiobj_str_write_hex`](#str_write_hex) - writes a base 16 (hex) number to the string.

* [`fiobj_str_concat`](#str_concat-str_join) - writes an existing string to the string.

* [`fiobj_str_replace`](#str_replace) - replaces a section of the string.

* [`fiobj_str_vprintf`](#str_vprintf) - writes formatted data to the string.

* [`fiobj_str_printf`](#str_printf) - writes formatted data to the string.

* [`fiobj_str_readfd`](#str_readfd) - writes data from an open file to the string.

* [`fiobj_str_readfile`](#str_readfile) - writes data from an unopened file to the string.

* [`fiobj_str_write_b64enc`](#str_write_b64enc) - encodes and writes data to the string using base 64.

* [`fiobj_str_write_b64dec`](#str_write_b64dec) - decodes and writes data to the string using base 64.

* [`fiobj_str_write_escape`](#str_write_escape) - writes JSON style escaped data to the string.

* [`fiobj_str_write_unescape`](#str_write_unescape) - writes decoded JSON escaped data to the string.


### `FIOBJ` Arrays

`FIOBJ` Arrays are based on the core `ARY_x` functions. This means that all these core type functions are available also for this type, using the `fiobj_array` prefix (i.e., [`ARY_new`](#ary_new) becomes [`fiobj_array_new`](#ary_new), [`ARY_push`](#ary_push) becomes [`fiobj_array_push`](#ary_push), etc').

These functions include:

* [`fiobj_array_new`](#ary_new)

* [`fiobj_array_free`](#ary_free)

* [`fiobj_array_destroy`](#ary_destroy)

* [`fiobj_array_count`](#ary_count)

* [`fiobj_array_capa`](#ary_capa)

* [`fiobj_array_reserve`](#ary_reserve)

* [`fiobj_array_concat`](#ary_concat)

* [`fiobj_array_set`](#ary_set)

* [`fiobj_array_get`](#ary_get)

* [`fiobj_array_find`](#ary_find)

* [`fiobj_array_remove`](#ary_remove)

* [`fiobj_array_remove2`](#ary_remove2)

* [`fiobj_array_compact`](#ary_compact)

* [`fiobj_array_to_a`](#ary_to_a)

* [`fiobj_array_push`](#ary_push)

* [`fiobj_array_pop`](#ary_pop)

* [`fiobj_array_unshift`](#ary_unshift)

* [`fiobj_array_shift`](#ary_shift)

* [`fiobj_array_each`](#ary_each)

### `FIOBJ` Ordered Hash Maps

`FIOBJ` Ordered Hash Maps are based on the core `MAP_x` functions. This means that all these core type functions are available also for this type, using the `fiobj_hash` prefix (i.e., [`MAP_new`](#map_new) becomes [`fiobj_hash_new`](#map_new), [`MAP_set`](#map_set) becomes [`fiobj_hash_set`](#map_set), etc').

In addition, the following `fiobj_hash` functions and MACROs are defined:

#### `fiobj2hash`

```c
uint64_t fiobj2hash(FIOBJ target_hash, FIOBJ value);
```

Calculates an object's hash value for a specific hash map object.

#### `fiobj_hash_set2`

```c
FIOBJ fiobj_hash_set2(FIOBJ hash, FIOBJ key, FIOBJ value);
```

Inserts a value to a hash map, with a default hash value calculation.

#### `fiobj_hash_set_if_missing2`

```c
FIOBJ fiobj_hash_set_if_missing2(FIOBJ hash, FIOBJ key, FIOBJ value);
```

Inserts a value to a hash map, with a default hash value calculation.

If the key already exists in the Hash Map, the value will be freed instead.

#### `fiobj_hash_get2`

```c
FIOBJ fiobj_hash_get2(FIOBJ hash, FIOBJ key);
```

Finds a value in a hash map, with a default hash value calculation.

#### `fiobj_hash_remove2`

```c
int fiobj_hash_remove2(FIOBJ hash, FIOBJ key, FIOBJ *old);
```

Removes a value from a hash map, with a default hash value calculation.

#### `fiobj_hash_set3`

```c
FIOBJ fiobj_hash_set3(FIOBJ hash, const char *key, size_t len, FIOBJ value);
```

Sets a value in a hash map, allocating the key String and automatically calculating the hash value.

#### `fiobj_hash_get3`

```c
FIOBJ fiobj_hash_get3(FIOBJ hash, const char *buf, size_t len);
```

Finds a String value in a hash map, using a temporary String as the key and automatically calculating the hash value.

#### `fiobj_hash_remove3`

```c
int fiobj_hash_remove3(FIOBJ hash, const char *buf, size_t len, FIOBJ *old);
```

Removes a String value in a hash map, using a temporary String as the key and automatically calculating the hash value.

#### `FIOBJ` Hash Map - Core Type Functions

In addition, all the functions documented above as `MAP_x`, are defined as `fiobj_hash_x`:

* [`fiobj_hash_new`](#map_new)

* [`fiobj_hash_free`](#map_free)

* [`fiobj_hash_destroy`](#map_destroy)

* [`fiobj_hash_get`](#map_get-hash-map)

* [`fiobj_hash_get_ptr`](#map_get_ptr)

* [`fiobj_hash_set`](#map_set)

* [`fiobj_hash_set_ptr`](#map_set_ptr)

* [`fiobj_hash_remove`](#map_remove)

* [`fiobj_hash_evict`](#map_evict)

* [`fiobj_hash_count`](#map_count)

* [`fiobj_hash_capa`](#map_capa)

* [`fiobj_hash_reserve`](#map_reserve)

* [`fiobj_hash_compact`](#map_compact)

* [`fiobj_hash_rehash`](#map_rehash)

* [`fiobj_hash_each`](#map_each)

* [`fiobj_hash_each_get_key`](#map_each_get_key)

### `FIOBJ` JSON Helpers

Parsing, editing and outputting JSON in C can be easily accomplished using `FIOBJ` types.

`facil.io` offers the added benefit of complete parsing from JSON to object. This allows the result to be manipulated, updated, sliced or merged with ease. This is in contrast to some parsers that offer a mid-way structures or lazy (delayed) parsing for types such as `true`, `false` and Numbers.

`facil.io` also offers the added benefit of complete formatting from a framework wide object type (`FIOBJ`) to JSON, allowing the same soft type system to be used throughout the project (rather than having a JSON dedicated type system).

This is in addition to `facil.io` support to some JSON extensions such as comments, both C style (both `//` and `/* ... */` and bash style (`#`).

However, there are [faster alternatives as well as slower alternatives out there](json_performance.html) (i.e., the [Qajson4c library](https://github.com/DeHecht/qajson4c) is a wonderful alternative for embedded systems).

#### `fiobj2json`

```c
FIOBJ fiobj2json(FIOBJ dest, FIOBJ o, uint8_t beautify);
```

Returns a JSON valid FIOBJ String, representing the object.

If `dest` is an existing String, the formatted JSON data will be appended to the existing string.

```c
FIOBJ result = fiobj_json_parse2("{\"name\":\"John\",\"surname\":\"Smith\",\"ID\":1}",40, NULL);
FIO_ASSERT( fiobj2cstr(fiobj_hash_get3(result, "name", 4)).len == 4 &&
            !memcmp(fiobj2cstr(fiobj_hash_get3(result, "name", 4)).buf, "John", 4), "result error");

FIOBJ_STR_TEMP_VAR(json_str); /* places string on the stack */
fiobj2json(json_str, result, 1);
FIO_LOG_INFO("updated JSON data to look nicer:\n%s", fiobj2cstr(json_str).buf);
fiobj_free(result);
FIOBJ_STR_TEMP_DESTROY(json_str);
```

#### `fiobj_hash_update_json`

```c
size_t fiobj_hash_update_json(FIOBJ hash, fio_str_info_s str);

size_t fiobj_hash_update_json2(FIOBJ hash, char *ptr, size_t len);
```

Updates a Hash using JSON data.

Parsing errors and non-dictionary object JSON data are silently ignored, attempting to update the Hash as much as possible before any errors encountered.

Conflicting Hash data is overwritten (preferring the new over the old).

Returns the number of bytes consumed. On Error, 0 is returned and no data is consumed.

The `fiobj_hash_update_json2` function is a helper function, it calls `fiobj_hash_update_json` with the provided string information.

#### `fiobj_json_parse`

```c
FIOBJ fiobj_json_parse(fio_str_info_s str, size_t *consumed);

#define fiobj_json_parse2(data_, len_, consumed)                      \
  fiobj_json_parse((fio_str_info_s){.buf = data_, .len = len_}, consumed)
```

Parses a C string for JSON data. If `consumed` is not NULL, the `size_t` variable will contain the number of bytes consumed before the parser stopped (due to either error or end of a valid JSON data segment).

Returns a FIOBJ object matching the JSON valid C string `str`.

If the parsing failed (no complete valid JSON data) `FIOBJ_INVALID` is returned.

`fiobj_json_parse2` is a helper macro, it calls `fiobj_json_parse` with the provided string information.

### How to Extend the `FIOBJ` Type System

The `FIOBJ` source code includes two extensions for the `Float` and `Number` types.

In many cases, numbers and floats can be used without memory allocations. However, when memory allocation is required to store the data, the `FIOBJ_T_NUMBER` and `FIOBJ_T_FLOAT` types are extended using the same techniques described here.

#### `FIOBJ` Extension Requirements

To extend the `FIOBJ` soft type system, there are a number of requirements:

1. A **unique** type ID must be computed.

    Type IDs are `size_t` bits in length. Values under 100 are reserved. Values under 40 are illegal (might break implementation).

2. A static virtual function table object (`FIOBJ_class_vtable_s`) must be fully populated (`NULL` values may break cause a segmentation fault).

3. The unique type construct / destructor must be wrapped using the facil.io reference counting wrapper (using `FIO_REF_NAME`).

    The `FIO_REF_METADATA` should be set to a `FIOBJ_class_vtable_s` pointer and initialized for every object.

4. The unique type wrapper must use pointer tagging as described bellow (`FIO_PTR_TAG`).

5. A public API should be presented.

#### `FIOBJ` Pointer Tagging

The `FIOBJ` types is often identified by th a bit "tag" added to the pointer. All extension types **must** be tagged as `FIOBJ_T_OTHER`.

The facil.io memory allocator (`fio_malloc`), as well as most system allocators, promise a 64 bit allocation alignment. The `FIOBJ` types leverage this behavior by utilizing the least significant 3 bits that are always zero. However, this implementation might change in the future, so it's better to use the macros `FIOBJ_PTR_TAG` and `FIOBJ_PTR_UNTAG`.

The following macros should be defined for tagging an extension `FIOBJ` type, allowing the `FIO_REF_NAME` constructor / destructor to manage pointer tagging, reference counting and access to the `FIOBJ` virtual table (see later on).

```c
#define FIO_PTR_TAG(p)   FIOBJ_PTR_TAG(p, FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p) FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE FIOBJ
```

#### `FIOBJ` Virtual Function Tables

`FIOBJ` extensions use a virtual function table that is shared by all the objects of that type/class.

Basically, the virtual function table is a `struct` with the **Type ID** and function pointers.

**Type ID** values under 100 are reserved for facil.io and might cause conflicts with the existing type values if used (i.e., `FIOBJ_T_FALSE == 34`).

All function pointers must be populated (where `each1` is only called if `count` returns a non-zero value).

This is the structure of the virtual table:

```c
/** FIOBJ types can be extended using virtual function tables. */
typedef struct {
  /** A unique number to identify object type. */
  size_t type_id;
  /** Test for equality between two objects with the same `type_id` */
  unsigned char (*is_eq)(FIOBJ a, FIOBJ b);
  /** Converts an object to a String */
  fio_str_info_s (*to_s)(FIOBJ o);
  /** Converts an object to an integer */
  intptr_t (*to_i)(FIOBJ o);
  /** Converts an object to a double */
  double (*to_f)(FIOBJ o);
  /** Returns the number of exposed elements held by the object, if any. */
  uint32_t (*count)(FIOBJ o);
  /** Iterates the exposed elements held by the object. See `fiobj_each1`. */
  uint32_t (*each1)(FIOBJ o, int32_t start_at,
                    int (*task)(FIOBJ child, void *arg), void *arg);
  /**
   * Decreases the reference count and/or frees the object, calling `free2` for
   * any nested objects.
   *
   * Returns 0 if the object is still alive or 1 if the object was freed. The
   * return value is currently ignored, but this might change in the future.
   */
  int (*free2)(FIOBJ o);
} FIOBJ_class_vtable_s;
```

#### `FIOBJ` Extension Example

For our example, let us implement a static string extension type. We will use the Type ID 100 because values under 100 are reserved.

Let's call our example header file `fiobj_static.h`, so we can find it later.

The API for this type and the header might look something like this:

```c
#ifndef FIO_STAT_STRING_HEADER_H
/* *****************************************************************************
FIOBJ Static String Extension Header Example
***************************************************************************** */
#define FIO_STAT_STRING_HEADER_H
/* *****************************************************************************
Perliminaries - include the FIOBJ extension, but not it's implementation
***************************************************************************** */
#define FIO_EXTERN
#define FIOBJ_EXTERN
#define FIO_FIOBJ
#include "fio-stl.h"

/* *****************************************************************************
Defining the Type ID and the API
***************************************************************************** */

/** The Static String Type ID */
#define FIOBJ_T_STATIC_STRING 100UL

/** Returns a new static string object. The string is considered immutable. */
FIOBJ fiobj_static_new(const char *str, size_t len);

/** Returns a pointer to the static string. */
const char *fiobj_static2ptr(FIOBJ s);

/** Returns the static strings length. */
size_t fiobj_static_len(FIOBJ s);

#endif
```

**Note**: The header assumes that _somewhere_ there's a C implementation file that includes the `FIOBJ` implementation. That C file defines the `FIOBJ_EXTERN_COMPLETE` macro **before** including the `fio-stl.h` file (as well as defining `FIO_FIOBJ` and `FIOBJ_EXTERN`).

The implementation may look like this.

```c
/* *****************************************************************************
FIOBJ Static String Extension Implementation Example
***************************************************************************** */
#include <fiobj_static.h> // include the header file here, whatever it's called

/* *****************************************************************************
The Virtual Function Table (definitions and table)
***************************************************************************** */

/** Test for equality between two objects with the same `type_id` */
static unsigned char static_string_is_eq(FIOBJ a, FIOBJ b);
/** Converts an object to a String */
static fio_str_info_s static_string_to_s(FIOBJ o);
/** Converts an object to an integer */
static intptr_t static_string_to_i(FIOBJ o);
/** Converts an object to a double */
static double static_string_to_f(FIOBJ o);
/** Returns the number of exposed elements held by the object, if any. */
static uint32_t static_string_count(FIOBJ o);
/** Iterates the exposed elements held by the object. See `fiobj_each1`. */
static uint32_t static_string_each1(FIOBJ o, int32_t start_at,
                                    int (*task)(FIOBJ, void *), void *arg);
/**
 * Decreases the reference count and/or frees the object, calling `free2` for
 * any nested objects (which we don't have for this type).
 *
 * Returns 0 if the object is still alive or 1 if the object was freed. The
 * return value is currently ignored, but this might change in the future.
 */
static int static_string_free2(FIOBJ o);

/** The virtual function table object. */
static const FIOBJ_class_vtable_s FIOBJ___STATIC_STRING_VTABLE = {
    .type_id = FIOBJ_T_STATIC_STRING,
    .is_eq = static_string_is_eq,
    .to_s = static_string_to_s,
    .to_i = static_string_to_i,
    .to_f = static_string_to_f,
    .count = static_string_count,
    .each1 = static_string_each1,
    .free2 = static_string_free2,
};

/* *****************************************************************************
The Static String Type (internal implementation)
***************************************************************************** */

/* leverage the small-string type to hold static string data */
#define FIO_STR_SMALL fiobj_static_string
/* add required pointer tagging */
#define FIO_PTR_TAG(p)   FIOBJ_PTR_TAG(p, FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p) FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE FIOBJ
/* add required reference counter / wrapper type */
#define FIO_REF_NAME fiobj_static_string
#define FIO_REF_CONSTRUCTOR_ONLY
/* initialization - for demonstration purposes, we don't use it here. */
#define FIO_REF_INIT(o)                                                        \
  do {                                                                         \
    o = (fiobj_static_string_s){0};                                            \
    FIOBJ_MARK_MEMORY_ALLOC(); /* mark memory allocation for debugging */      \
  } while (0)
/* cleanup - destroy the object data when the reference count reaches zero. */
#define FIO_REF_DESTROY(o)                                                     \
  do {                                                                         \
    fiobj_static_string_destroy((FIOBJ)&o);                                    \
    FIOBJ_MARK_MEMORY_FREE(); /* mark memory deallocation for debugging */     \
  } while (0)
/* metadata (vtable) definition and initialization. */
#define FIO_REF_METADATA const FIOBJ_class_vtable_s *
/* metadata initialization - required to initialize the vtable. */
#define FIO_REF_METADATA_INIT(m)                                               \
  do {                                                                         \
    m = &FIOBJ___STATIC_STRING_VTABLE;                                         \
  } while (0)
#include <fio-stl.h>

/* *****************************************************************************
The Public API
***************************************************************************** */

/** Returns a new static string object. The string is considered immutable. */
FIOBJ fiobj_static_new(const char *str, size_t len) {
  FIOBJ o = fiobj_static_string_new();
  FIO_ASSERT_ALLOC(FIOBJ_PTR_UNTAG(o));
  fiobj_static_string_init_const(o, str, len);
  return o;
}

/** Returns a pointer to the static string. */
const char *fiobj_static2ptr(FIOBJ o) { return fiobj_static_string2ptr(o); }

/** Returns the static strings length. */
size_t fiobj_static_len(FIOBJ o) { return fiobj_static_string_len(o); }

/* *****************************************************************************
Virtual Function Table Implementation
***************************************************************************** */

/** Test for equality between two objects with the same `type_id` */
static unsigned char static_string_is_eq(FIOBJ a, FIOBJ b) {
  fio_str_info_s ai, bi;
  ai = fiobj_static_string_info(a);
  bi = fiobj_static_string_info(b);
  return (ai.len == bi.len && !memcmp(ai.buf, bi.buf, ai.len));
}
/** Converts an object to a String */
static fio_str_info_s static_string_to_s(FIOBJ o) {
  return fiobj_static_string_info(o);
}
/** Converts an object to an integer */
static intptr_t static_string_to_i(FIOBJ o) {
  fio_str_info_s s = fiobj_static_string_info(o);
  if (s.len)
    return fio_atol(&s.buf);
  return 0;
}
/** Converts an object to a double */
static double static_string_to_f(FIOBJ o) {
  fio_str_info_s s = fiobj_static_string_info(o);
  if (s.len)
    return fio_atof(&s.buf);
  return 0.0L;
}
/** Returns the number of exposed elements held by the object, if any. */
static uint32_t static_string_count(FIOBJ o) {
  return 0;
  (void)o;
}
/** Iterates the exposed elements held by the object. See `fiobj_each1`. */
static uint32_t static_string_each1(FIOBJ o, int32_t start_at,
                                    int (*task)(FIOBJ, void *), void *arg) {
  return 0;
  (void)o; (void)start_at; (void)task; (void)arg;
}
/** Decreases the reference count and/or frees the object. */
static int static_string_free2(FIOBJ o) { return fiobj_static_string_free(o); }
```

Example usage:

```c
#define FIOBJ_EXTERN_COMPLETE // we will place the FIOBJ implementation here.
#include "fiobj_static.h"     // include FIOBJ extension type
int main(void) {
  FIOBJ o = fiobj_static_new("my static string", 16);
  /* example test of virtual table redirection */
  FIO_ASSERT(fiobj2cstr(o).buf == fiobj_static2ptr(o) &&
                 fiobj2cstr(o).len == fiobj_static_len(o),
             "vtable redirection error.");
  fprintf(stderr, "allocated: %s\n", fiobj_static2ptr(o));
  fprintf(stderr, "it's %zu byte long\n", fiobj_static_len(o));
  fprintf(stderr, "object type: %zu\n", FIOBJ_TYPE(o));
  fiobj_free(o);
  FIOBJ_MARK_MEMORY_PRINT(); /* only in DEBUG mode */
}
```

-------------------------------------------------------------------------------

