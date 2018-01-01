#ifndef FIOBJ_PRIMITIVES_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
Herein are defined some primitive types for the facil.io dynamic object system.
*/
#define FIOBJ_PRIMITIVES_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
NULL
***************************************************************************** */

/** Identifies the NULL type. */
extern const uintptr_t FIOBJ_T_NULL;

/** Returns a NULL object. */
FIOBJ fiobj_null(void);

/** Tests if a `FIOBJ ` is NULL. */
#define FIOBJ_IS_NULL(o) ((o) == NULL) || (FIOBJ_TYPE(o) == FIOBJ_T_NULL)

/* *****************************************************************************
True
***************************************************************************** */

/** Identifies the TRUE type. */
extern const uintptr_t FIOBJ_T_TRUE;

/** Returns a TRUE object. */
FIOBJ fiobj_true(void);

/* *****************************************************************************
False
***************************************************************************** */

/** Identifies the FALSE type. */
extern const uintptr_t FIOBJ_T_FALSE;

/** Returns a FALSE object. */
FIOBJ fiobj_false(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
