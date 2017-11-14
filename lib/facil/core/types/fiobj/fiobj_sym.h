#ifndef H_FIOBJ_SYMBOL_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
 */
#define H_FIOBJ_SYMBOL_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Symbol type identifier */
extern const uintptr_t FIOBJ_T_SYMBOL;

/* *****************************************************************************
Symbol API
***************************************************************************** */

/** Creates a Symbol object. Use `fiobj_free`. */
fiobj_s *fiobj_sym_new(const char *str, size_t len);

/** Creates a Symbol object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_symvprintf(const char *format, va_list argv);

/** Creates a Symbol object using a printf like interface. */
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_symprintf(const char *format, ...);

/**
 * Returns a symbol's identifier.
 *
 * The unique identifier is calculated using SipHash and is equal for all Symbol
 * objects that were created using the same data.
 */
uintptr_t fiobj_sym_id(fiobj_s *sym);

/* *****************************************************************************
Risky Symbol API
***************************************************************************** */

/**
 * Reinitializes a pre-allocated Symbol buffer to set it's final length and
 * calculate it's final hashing value.
 *
 * NEVER use this on a symbol that was already used in other objects, such as a
 * Hash.
 */
fiobj_s *fiobj_sym_reinitialize(fiobj_s *s, const size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
