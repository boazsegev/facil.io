#ifndef H_FIOBJ_STR_H
/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/
#define H_FIOBJ_STR_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIOBJ_IS_STRING(obj) FIOBJ_TYPE_IS((obj), FIOBJ_T_STRING)

/* *****************************************************************************
API: Creating a String Object
***************************************************************************** */

/** Creates a String object. Remember to use `fiobj_free`. */
FIOBJ fiobj_str_new(const char *str, size_t len);

/** Creates a buffer String object. capa includes NUL.
 *
 * Remember to use `fiobj_free`.
 */
FIOBJ fiobj_str_buf(size_t capa);

/**
 * Creates a static String object from a static C string. Remember `fiobj_free`.
 *
 * This variation avoids allocating memory for an existing static String.
 *
 * The object still needs to be frees, but the string isn't copied and isn't
 * freed.
 *
 * NOTICE: static strings can't be written to.
 */
FIOBJ fiobj_str_static(const char *str, size_t len);

/** Creates a copy from an existing String. Remember to use `fiobj_free`. */
static inline __attribute__((unused)) FIOBJ fiobj_str_copy(FIOBJ src) {
  fio_cstr_s s = fiobj_obj2cstr(src);
  return fiobj_str_new(s.data, s.len);
}

/**
 * Creates a String object. Remember to use `fiobj_free`.
 *
 * The ownership of the memory indicated by `str` will now "move" to the object.
 *
 * The original memory MUST be allocated using `fio_malloc` (NOT the system's
 * `malloc`) and it will be freed by the `fiobj` library using `fio_free`.
 */
FIOBJ fiobj_str_move(char *str, size_t len, size_t capacity);

/**
 * Returns a thread-static temporary string. Avoid calling `fiobj_dup` or
 * `fiobj_free`.
 */
FIOBJ fiobj_str_tmp(void);

/** Creates a String object using a printf like interface. */
__attribute__((format(printf, 1, 0))) FIOBJ fiobj_strvprintf(const char *format,
                                                             va_list argv);

/** Creates a String object using a printf like interface. */
__attribute__((format(printf, 1, 2))) FIOBJ fiobj_strprintf(const char *format,
                                                            ...);

/** Dumps the `filename` file's contents into a new String. If `limit == 0`,
 * than the data will be read until EOF.
 *
 * If the file can't be located, opened or read, or if `start_at` is out of
 * bounds (i.e., beyond the EOF position), FIOBJ_INVALID is returned.
 *
 * If `start_at` is negative, it will be computed from the end of the file.
 *
 * Remember to use `fiobj_free`.
 *
 * NOTE: Requires a UNIX system, otherwise always returns FIOBJ_INVALID.
 */
FIOBJ fiobj_str_readfile(const char *filename, intptr_t start_at,
                         intptr_t limit);

/* *****************************************************************************
API: Editing a String
***************************************************************************** */

/**
 * Prevents the String object from being changed.
 *
 * When a String is used as a key for a Hash, it is automatically frozenn to
 * prevent the Hash from becoming broken.
 *
 * A call to `fiobj_str_hash` or `fiobj_obj2hash` will automactically freeze the
 * String.
 */
void fiobj_str_freeze(FIOBJ str);

/**
 * Confirms the requested capacity is available and allocates as required.
 *
 * Returns updated capacity.
 */
size_t fiobj_str_capa_assert(FIOBJ str, size_t size);

/** Return's a String's capacity, if any. This should include the NUL byte. */
size_t fiobj_str_capa(FIOBJ str);

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(FIOBJ str, size_t size);

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(FIOBJ str);

/** Empties a String's data. */
void fiobj_str_clear(FIOBJ str);

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write(FIOBJ dest, const char *data, size_t len);

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
__attribute__((format(printf, 2, 3))) size_t
fiobj_str_write2(FIOBJ dest, const char *format, ...);

/**
 * Writes data at the end of the string, resizing the string as required.
 *
 * Remember to call `fiobj_free` to free the source (when done with it).
 *
 * Returns the new length of the String.
 */
size_t fiobj_str_join(FIOBJ dest, FIOBJ source);

/* *****************************************************************************
API: String Values
***************************************************************************** */

/**
 * Calculates a String's SipHash value for possible use as a HashMap key.
 *
 * Hashing the String's value automatically freezes the string, preventing
 * future changes.
 */
uint64_t fiobj_str_hash(FIOBJ o);

#if DEBUG
void fiobj_test_string(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
