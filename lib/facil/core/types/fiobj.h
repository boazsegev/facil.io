#ifndef H_FACIL_IO_OBJECTS_H
/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

/**
This facil.io core library provides wrappers around complex and (or) dynamic
types, abstracting some complexity and making dynamic type related tasks easier.

The library attempts to provide rudementry protection against cyclic references
(i.e., nesting an Array within itself)... however, something's wrong with the
protection for now and the code might overflowing somewhere when this happens...
so don't do it.
*/
#define H_FACIL_IO_OBJECTS_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* *****************************************************************************
The Object type (`fiobj_s`) and it's variants
***************************************************************************** */

/* FIO Object types */
typedef enum {
  /** A simple flag object indicating a NULL (or nil) object */
  FIOBJ_T_NULL,
  /** A simple flag object indicating a TRUE value. */
  FIOBJ_T_TRUE,
  /** A simple flag object indicating a FALSE value. */
  FIOBJ_T_FALSE,
  /** A signed numerical object containing an `int64_t`. */
  FIOBJ_T_NUMBER,
  /** A signed numerical object containing a `double`. */
  FIOBJ_T_FLOAT,
  /** A String object. */
  FIOBJ_T_STRING,
  /** A Symbol object. This object contains an immutable String. */
  FIOBJ_T_SYMBOL,
  /** An Array object. */
  FIOBJ_T_ARRAY,
  /** A Hash Table object. Hash keys MUST be Symbol objects. */
  FIOBJ_T_HASH,
  /** A Hash Table key-value pair. See `fiobj_each2`. */
  FIOBJ_T_COUPLET,
  /** An IO object containing an `intptr_t` as a `fd` (File Descriptor). */
  FIOBJ_T_IO,
  /** A temporary File object containing a `FILE *`. */
  FIOBJ_T_FILE,
} fiobj_type_en;

typedef struct fiobj_s { fiobj_type_en type; } fiobj_s;
typedef fiobj_s *fiobj_pt;

/* *****************************************************************************
Helper macros
***************************************************************************** */

/** returns TRUE (1) if the object is NULL */
#define FIOBJ_ISNULL(o) ((o) == NULL || (o)->type == FIOBJ_T_NULL)

/** returns TRUE (1) if the object is either NULL or FALSE  */
#define FIOBJ_FALSE(o)                                                         \
  ((o) == NULL || (o)->type == FIOBJ_T_FALSE || (o)->type == FIOBJ_T_NULL)

/** returns TRUE (1) if the object isn't NULL, FALSE, 0, or an empty String  */
#define FIOBJ_TRUE(o)                                                          \
  ((o) && ((o)->type == FIOBJ_T_TRUE ||                                        \
           ((o)->type == FIOBJ_T_NUMBER && fiobj_obj2num((o)) != 0) ||         \
           ((o)->type == FIOBJ_T_FLOAT && fiobj_obj2float((o)) != 0) ||        \
           ((o)->type == FIOBJ_T_STRING && fiobj_obj2cstr((o))[0] != 0) ||     \
           (o)->type == FIOBJ_T_SYMBOL || (o)->type == FIOBJ_T_ARRAY ||        \
           (o)->type == FIOBJ_T_HASH || (o)->type == FIOBJ_T_IO ||             \
           (o)->type == FIOBJ_T_FILE))

/* *****************************************************************************
Generic Object API
***************************************************************************** */

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
 * a Hashe object is passed along, it's children (nested objects) are
 * also freed.
 */
void fiobj_free(fiobj_s *);

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
int64_t fiobj_obj2num(fiobj_s *obj);

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
double fiobj_obj2float(fiobj_s *obj);

/** A string information type, reports anformation about a C string. */
typedef struct {
  union {
    uint64_t len;
    uint64_t length;
  };
  union {
    const void *buffer;
    const uint8_t *bytes;
    const char *data;
    const char *value;
    const char *name;
  };
} fio_cstr_s;

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
fio_cstr_s fiobj_obj2cstr(fiobj_s *obj);

/**
 * Deep itteration using a callback for each fio object, including the parent.
 *
 * Accepts any `fiobj_s *` type.
 *
 * Collections (Arrays, Hashes) are deeply probed while being marginally
 * protected from cyclic references.
 *
 * The callback task function must accept an object and an opaque user pointer.
 *
 * When a cyclic reference is detected, NULL is passed along instead of the
 * offending object.
 *
 * Hash objects will offer a `FIOBJ_T_COUPLET` object, containing
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

/**
 * Deeply compare two objects. No hashing is involved.
 *
 * Uses a similar algorithm to `fiobj_each2`, except adjusted to two objects.
 *
 * Hash order will be ignored when comapring Hashes, however at the moment it
 * isn't (see KNOWN ISSUES).
 *
 * KNOWN ISSUES:
 *
 * * Since Hashes offer ordered access, false negatives occur when comparing two
 *   identical Hash objects that have a different internal order.
 *
 * * Since nested objects aren't followed, there might be a risk regarding false
 *   positives when both nested objects and NULL pointers (not NULL objects) are
 *   used in the same superposition.
 *
 */
int fiobj_iseq(fiobj_s *obj1, fiobj_s *obj2);

/* *****************************************************************************
NULL, TRUE, FALSE API
***************************************************************************** */

/** Retruns a NULL object. Use `fiobj_free` to free memory.*/
fiobj_s *fiobj_null(void);

/** Retruns a TRUE object. Use `fiobj_free` to free memory. */
fiobj_s *fiobj_true(void);

/** Retruns a FALSE object. Use `fiobj_free` to free memory. */
fiobj_s *fiobj_false(void);

/* *****************************************************************************
Number and Float API
***************************************************************************** */

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10.
 *
 * The `0x##` (or `x##`) and `0b##` (or `b##`) are recognized as base 16 and
 * base 2 (binary MSB first) respectively.
 */
int64_t fio_atol(const char *str);

/** A helper function that convers between String data to a signed double. */
double fio_atof(const char *str);

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num);

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num);

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num);

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *target, double num);

/* *****************************************************************************
String API
***************************************************************************** */

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

/** Creates a String object. Remember to `fiobj_free`. */
fiobj_s *fiobj_str_new(const char *str, size_t len);

/**
 * Allocates a new String using `prinf` semantics. Remember to `fiobj_free`.
 *
 * On error returns NULL.
 */
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_strprintf(const char *restrict format, ...);
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_strvprintf(const char *restrict format, va_list argv);

/** Creates a buffer String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_buf(size_t capa);

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(fiobj_s *str, size_t size);

/** Returns a String's capacity, if any. */
size_t fiobj_str_capa(fiobj_s *str);

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(fiobj_s *str);

/** Empties a String's data. */
void fiobj_str_clear(fiobj_s *str);

/**
 * Writes data at the end of the string, resizing the string as required.
 *
 * Returns the new length of the String.
 */
size_t fiobj_str_write(fiobj_s *dest, const char *data, size_t len);
/**
 * Writes data at the end of the string, using `printf` syntaz and resizing the
 * string as required.
 *
 * Returns the new length of the String.
 */
size_t fiobj_str_write2(fiobj_s *dest, const char *format, ...);

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_join(fiobj_s *dest, fiobj_s *obj);

/* *****************************************************************************
Symbol API
***************************************************************************** */

/**
 * Creates a Symbol object. Always use `fiobj_free`.
 *
 * It is better to keep the symbol object than to create a new one each time.
 * This approach prevents the symbol from being deallocated and reallocated as
 * well as minimizes hash value computation.
 */
fiobj_s *fiobj_sym_new(const char *str, size_t len);

/** Allocated a new Symbol using `prinf` semantics. Remember to `fiobj_free`.*/
fiobj_s *fiobj_symprintf(const char *restrict format, ...)
    __attribute__((format(printf, 1, 2)));

fiobj_s *fiobj_symvprintf(const char *restrict format, va_list argv)
    __attribute__((format(printf, 1, 0)));

/** Returns 1 if both Symbols are equal and 0 if not. */
int fiobj_sym_iseql(fiobj_s *sym1, fiobj_s *sym2);

/* *****************************************************************************
IO API
***************************************************************************** */

/** Wrapps a file descriptor in an IO object. Use `fiobj_free` to close. */
fiobj_s *fio_io_wrap(intptr_t fd);

/**
 * Return an IO's fd.
 *
 * A type error results in -1.
 */
intptr_t fiobj_io_fd(fiobj_s *obj);

/* *****************************************************************************
File API
***************************************************************************** */

/** Wrapps a `FILe` pointer in a File object. Use `fiobj_free` to close. */
fiobj_s *fio_file_wrap(FILE *file);

/**
 * Returns a temporary `FILE` pointer.
 *
 * A type error results in NULL.
 */
FILE *fiobj_file(fiobj_s *obj);

/* *****************************************************************************
Array API
***************************************************************************** */

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void);

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary);

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1` is the
 * last item.
 */
fiobj_s *fiobj_ary_entry(fiobj_s *ary, int64_t pos);

/**
 * Sets an object at the requested position.
 *
 * If the position overflows the current array size, all intermediate
 * positions will be set to NULL and the Array will grow in size.
 *
 * The old object (if any) occupying the same space will be freed.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 *
 * Type errors are silently ignored.
 *
 * For the Array [41], `fiobj_ary_set(ary, fiobj_num_new(42), -10)` will become:
 *    `[42, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 41]`
 */
void fiobj_ary_set(fiobj_s *ary, fiobj_s *obj, int64_t pos);

/**
 * Pushes an object to the end of the Array.
 *
 * The Array now owns the object. If an error occurs or the Array is freed, the
 * object will be freed.
 *
 * Use `fiobj_dup` to push a copy, if required.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj);

/** Pushes a copy of an object to the end of the Array, returning the object.*/
static inline __attribute__((unused)) fiobj_s *
fiobj_ary_push_dup(fiobj_s *ary, fiobj_s *obj) {
  fiobj_ary_push(ary, fiobj_dup(obj));
  return obj;
}

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary);

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 *
 * The Array now owns the object. Use `fiobj_dup` to push a copy if
 * required.
 */
void fiobj_ary_unshift(fiobj_s *ary, fiobj_s *obj);

/** Unshifts a copy to the begining of the Array, returning the object.*/
static inline __attribute__((unused)) fiobj_s *
fiobj_ary_unshift_dup(fiobj_s *ary, fiobj_s *obj) {
  fiobj_ary_unshift(ary, fiobj_dup(obj));
  return obj;
}

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary);

/**
 * Flattens an Array, making it single dimentional.
 *
 * Other Arrays are simply unnested inplace.
 *
 * Hashes are treated as a multi-dimentional Array:
 * `[[key,value],[key,value],..]`.
 */
void fiobj_ary_flatten(fiobj_s *ary);
/* *****************************************************************************
Hash API
***************************************************************************** */

/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void);

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(fiobj_s *hash);

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving** the
 * ownership of the object to the Hash.
 *
 * This implies Symbol objects should be initialized once per application run,
 * when possible.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj);
/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash_remove(fiobj_s *hash, fiobj_s *sym);

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the associated
 * object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(fiobj_s *hash, fiobj_s *sym);

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL if
 * none.
 */
fiobj_s *fiobj_hash_get(fiobj_s *hash, fiobj_s *sym);

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash_haskey(fiobj_s *hash, fiobj_s *sym);

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2key(fiobj_s *obj);

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(fiobj_s *obj);

#ifdef DEBUG
void fiobj_test(void);
#endif

#endif /* H_FACIL_IO_OBJECTS_H */
