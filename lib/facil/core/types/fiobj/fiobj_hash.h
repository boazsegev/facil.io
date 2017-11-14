#ifndef H_FIOBJ_HASH_H
#define H_FIOBJ_HASH_H
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fiobj_str.h"
#include "fiobj_sym.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MUST be a power of 2 */
#define HASH_INITIAL_CAPACITY 16

#include <errno.h>

/** attempts to rehash the hashmap. */
void fiobj_hash_rehash(fiobj_s *h);

/* *****************************************************************************
Couplets API - the Key-Value pair, created by the Hash object
***************************************************************************** */

/** Couplet type identifier. */
extern const uintptr_t FIOBJ_T_COUPLET;

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2key(const fiobj_s *obj);

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(const fiobj_s *obj);

/* *****************************************************************************
Hash API
***************************************************************************** */

/** Hash type identifier.

The facil.io Hash object is, by default, an insecure (non-collision resistant)
ordered Hash Table implementation.

By being non-collision resistant (comparing only the Hash data), memory
comparison can be avoided and performance increased.

By being ordered it's possible to iterate over key-value pairs in the order in
which they were added to the Hash table, making it possible to output JSON in a
controlled manner.
*/
extern const uintptr_t FIOBJ_T_HASH;

/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are optimized for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void);

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(const fiobj_s *hash);

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
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
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(fiobj_s *hash, fiobj_s *sym);

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL
 * if none.
 */
fiobj_s *fiobj_hash_get(const fiobj_s *hash, fiobj_s *sym);

/**
 * Returns a temporary handle to the object associated with the Symbol C string.
 *
 * This function takes a C string instead of a Symbol, which is slower if a
 * Symbol can be cached but faster if a Symbol must be created.
 *
 * Returns NULL if no object is asociated with this String data.
 */
fiobj_s *fiobj_hash_get2(const fiobj_s *hash, const char *str, size_t len);

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if it's value is NULL.
 */
int fiobj_hash_haskey(const fiobj_s *hash, fiobj_s *sym);

/**
 * Returns a temporary theoretical Hash map capacity.
 * This could be used for testig performance and memory consumption.
 */
size_t fiobj_hash_capa(const fiobj_s *hash);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
