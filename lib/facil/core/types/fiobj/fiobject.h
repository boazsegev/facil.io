#ifndef H_FIOBJECT_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
This facil.io core library provides wrappers around complex and (or) dynamic
types, abstracting some complexity and making dynamic type related tasks easier.


The library offers a rudementry protection against cyclic references using the
`FIOBJ_NESTING_PROTECTION` flag (i.e., nesting an Array within itself)...
however, this isn't fully tested and the performance price is high.
*/
#define H_FIOBJECT_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
Core Types
***************************************************************************** */

/** The dynamic facil.io object type. */
typedef struct { uintptr_t type; } fiobj_s;
typedef fiobj_s *fiobj_pt;

/** A string information type, reports anformation about a C string. */
typedef struct {
  union {
    uint64_t len;
    uint64_t length;
  };
  union {
    void *buffer;
    uint8_t *bytes;
    char *data;
    char *value;
    char *name;
  };
} fio_cstr_s;

/**
 * Sets the default state for nesting protection.
 *
 * NOTICE: facil.io's default makefile will disables nesting protection.
 *
 * This effects traversing functions, such as `fiobj_each2`, `fiobj_dup`,
 * `fiobj_free` etc'.
 */
#ifndef FIOBJ_NESTING_PROTECTION
#define FIOBJ_NESTING_PROTECTION 0
#endif

/* *****************************************************************************
Generic Object API
***************************************************************************** */

/** Returns a C string naming the objects dynamic type. */
const char *fiobj_type_name(const fiobj_s *obj);

/**
 * Copy by reference(!) - increases an object's (and any nested object's)
 * reference count.
 *
 * Always returns the value passed along.
 *
 * Future implementations might provide `fiobj_dup2` providing a deep copy.
 *
 * We don't need this feature just yet, so I'm not working on it.
 */
fiobj_s *fiobj_dup(fiobj_s *);

/**
 * Decreases an object's reference count, releasing memory and
 * resources.
 *
 * This function affects nested objects, meaning that when an Array or
 * a Hash object is passed along, it's children (nested objects) are
 * also freed.
 */
void fiobj_free(fiobj_s *);

/**
 * Attempts to return the object's current reference count.
 *
 * This is mostly for testing rather than normal library operations.
 */
uintptr_t fiobj_reference_count(const fiobj_s *);

/**
 * Tests if an object evaluates as TRUE.
 *
 * This is object type specific. For example, empty strings might evaluate as
 * FALSE, even though they aren't a boolean type.
 */
int fiobj_is_true(const fiobj_s *);

/**
 * Returns an Object's numerical value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * Hashes and Arrays return their object count.
 *
 * IO and File objects return their underlying file descriptor.
 *
 * A type error results in 0.
 */
int64_t fiobj_obj2num(const fiobj_s *obj);

/**
 * Returns a Float's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * Hashes and Arrays return their object count.
 *
 * IO and File objects return their underlying file descriptor.
 *
 * A type error results in 0.
 */
double fiobj_obj2float(const fiobj_s *obj);

/**
 * Returns a C String (NUL terminated) using the `fio_cstr_s` data type.
 *
 * The Sting in binary safe and might contain NUL bytes in the middle as well as
 * a terminating NUL.
 *
 * If a Symbol, a Number or a Float are passed to the function, they
 * will be parsed as a *temporary*, thread-safe, String.
 *
 * Numbers will be represented in base 10 numerical data.
 *
 * A type error results in NULL (i.e. object isn't a String).
 */
fio_cstr_s fiobj_obj2cstr(const fiobj_s *obj);

/**
 * Single layer iteration using a callback for each nested fio object.
 *
 * Accepts any `fiobj_s *` type but only collections (Arrays and Hashes) are
 * processed. The container itself (the Array or the Hash) is **not** processed
 * (unlike `fiobj_each2`).
 *
 * The callback task function must accept an object and an opaque user pointer.
 *
 * Hash objects pass along a `FIOBJ_T_COUPLET` object, containing
 * references for both the key (Symbol) and the object (any object).
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the "stop" position, i.e., the number of items processed + the
 * starting point.
 */
size_t fiobj_each1(fiobj_s *, size_t start_at,
                   int (*task)(fiobj_s *obj, void *arg), void *arg);

/**
 * Deep iteration using a callback for each fio object, including the parent.
 *
 * Accepts any `fiobj_s *` type.
 *
 * Collections (Arrays, Hashes) are deeply probed and shouldn't be edited
 * during an `fiobj_each2` call (or weird things may happen).
 *
 * The callback task function must accept an object and an opaque user pointer.
 *
 * If `FIOBJ_NESTING_PROTECTION` is equal to 1 and a cyclic (or recursive)
 * nesting is detected, a NULL pointer (not a NULL object) will be used instead
 * of the original (cyclic) object and the original (cyclic) object will be
 * available using the `fiobj_each_get_cyclic` function.
 *
 * Hash objects pass along a `FIOBJ_T_COUPLET` object, containing
 * references for both the key (Symbol) and the object (any object).
 *
 * Notice that when passing collections to the function, the collection itself
 * is sent to the callback followed by it's children (if any). This is true also
 * for nested collections (a nested Hash will be sent first, followed by the
 * nested Hash's children and then followed by the rest of it's siblings.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each2(fiobj_s *, int (*task)(fiobj_s *obj, void *arg), void *arg);

/** Within `fiobj_each2`, this will return the current cyclic object, if any. */
fiobj_s *fiobj_each_get_cyclic(void);

/**
 * Deeply compare two objects. No hashing or recursive functio n calls are
 * involved.
 *
 * Uses a similar algorithm to `fiobj_each2`, except adjusted to two objects.
 *
 * Hash order will be tested when comapring Hashes.
 *
 * KNOWN ISSUES:
 *
 * * Cyclic nesting will cause this function to hang (much like `fiobj_each2`).
 *
 *   If `FIOBJ_NESTING_PROTECTION` is set, then cyclic nesting might produce
 *   false positives.
 *
 * * Hash order will be tested as well as the Hash content, which means that
 * equal Hashes might be considered unequal if their order doesn't match.
 *
 */
int fiobj_iseq(const fiobj_s *obj1, const fiobj_s *obj2);

/* *****************************************************************************
Helpers: not fiobj_s specific, but since they're used internally, they're here.
***************************************************************************** */

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10.
 *
 * The `0x##` (or `x##`) and `0b##` (or `b##`) are recognized as base 16 and
 * base 2 (binary MSB first) respectively.
 *
 * The pointer will be updated to point to the first byte after the number.
 */
int64_t fio_atol(char **pstr);

/** A helper function that convers between String data to a signed double. */
double fio_atof(char **pstr);

/**
 * A helper function that convers between a signed int64_t to a string.
 *
 * No overflow guard is provided, make sure there's at least 66 bytes available
 * (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
 */
size_t fio_ltoa(char *dest, int64_t num, uint8_t base);

/**
 * A helper function that convers between a double to a string.
 *
 * No overflow guard is provided, make sure there's at least 130 bytes available
 * (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
 */
size_t fio_ftoa(char *dest, double num, uint8_t base);

#ifdef DEBUG
void fiobj_test(void);
int fiobj_test_json_str(char const *json, size_t len, uint8_t print_result);
#endif

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif
