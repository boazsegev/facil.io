#ifndef H_FACIL_IO_OBJECTS_H
/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

/**
This facil.io core library provides wrappers around complex and (or) dynamic
types, abstracting some complexity and making dynamic type related tasks easier.
*/
#define H_FACIL_IO_OBJECTS_H

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
  FIOBJ_T_SYM,
  /** An Array object. */
  FIOBJ_T_ARRAY,
  /** A Hash Table object. Hash keys MUST be Symbol objects. */
  FIOBJ_T_HASH,
  /** A Hash Table key-value pair. This is available when using `fiobj_each`. */
  FIOBJ_T_HASH_COUPLET,
  /** An IO object containing an `intptr_t` as a `fd` (File Descriptor). */
  FIOBJ_T_IO,
  /** A temporary File object containing a `FILE *`. */
  FIOBJ_T_FILE,
} fiobj_type_en;

typedef struct fiobj_s { fiobj_type_en type; } fiobj_s;

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
           (o)->type == FIOBJ_T_SYM || (o)->type == FIOBJ_T_ARRAY ||           \
           (o)->type == FIOBJ_T_HASH || (o)->type == FIOBJ_T_IO ||             \
           (o)->type == FIOBJ_T_FILE))

/* *****************************************************************************
Generic Object API
***************************************************************************** */

/**
 * Increases an object's (and any nested object's) reference count.
 *
 * Future implementations might provide a (deep) copy for Arrays and Hashes...
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
 * Performes a task for each fio object.
 *
 * Collections (Arrays, Hashes) are deeply probed while being marginally
 * protected from cyclic references. Simpler objects are simply passed along.
 *
 * The callback task function should accept an object and an opaque user pointer
 * that is simply passed along.
 *
 * When a cyclic reference is detected, NULL is passed along instead of the
 * offending object.
 * The callback's `name` parameter is only set for
 * Hash pairs, indicating the source of the object is a Hash. Arrays and other
 * objects will pass along a NULL pointer for the `name` argument.
 *
 * Notice that when passing collections to the function, both the collection
 * itself and it's nested objects will be passed to the callback task function.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each(fiobj_s *, int (*task)(fiobj_s *obj, void *arg), void *arg);

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

/**
 * Returns a Number's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
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
 * A type error results in 0.
 */
double fiobj_obj2float(fiobj_s *obj);

/* *****************************************************************************
String API
***************************************************************************** */

/** A string information type, contains data about the length and the pointer */
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
} fio_string_s;

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

/** Creates a String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_new(const char *str, size_t len);

/**
 * Returns a C String (NUL terminated) using the `fio_string_s` data type.
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
fio_string_s fiobj_obj2cstr(fiobj_s *obj);

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
 * Pushes an object to the end of the Array.
 *
 * The Array now owns the object. Use `fiobj_dup` to push a copy if
 * required.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj);

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

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary);

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
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
 */
void fiobj_ary_set(fiobj_s *ary, fiobj_s *obj, int64_t pos);

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

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving** the
 * ownership of the object to the Hash.
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
 * If object is a Hash couplet (occurs in `fiobj_each`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2sym(fiobj_s *obj);

/**
 * If object is a Hash couplet (occurs in `fiobj_each`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(fiobj_s *obj);

#endif
