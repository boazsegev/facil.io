#ifndef H_FIOBJ_STR_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#define H_FIOBJ_STR_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/** String type identifier */
extern const uintptr_t FIOBJ_T_STRING;

/** Static String type identifier */
extern const uintptr_t FIOBJ_T_STRING_STATIC;

#define FIOBJ_IS_STRING(obj)                                                   \
  (FIOBJ_TYPE(obj) == FIOBJ_T_STRING ||                                        \
   FIOBJ_TYPE(obj) == FIOBJ_T_STRING_STATIC)

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
FIOBJ fiobj_str_copy(FIOBJ src);

/**
 * Creates a String object. Remember to use `fiobj_free`.
 *
 * The ownership of the memory indicated by `str` will now "move" to the object,
 * so `free` will be called by the `fiobj` library as needed.
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
 * If the file can't be located, opened or read, or if `start_at` is beyond the
 * EOF position, NULL is returned.
 *
 * Remember to use `fiobj_free`.
 *
 * NOTE: Requires a UNIX system, otherwise always returns NULL.
 */
FIOBJ fiobj_str_readfile(const char *filename, size_t start_at, size_t limit);

/* *****************************************************************************
API: Editing a String
***************************************************************************** */

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
 * Grabs the string's internal buffer, emptying the existing string data.
 * `fiobj_free` is still required (unless the string is a `tmp` string).
 */
void *fiobj_str_steal(FIOBJ str);

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
