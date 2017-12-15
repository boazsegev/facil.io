#ifndef H_FIO_SIPHASH_H
#define H_FIO_SIPHASH_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <sys/types.h>

/**
 * The Hashing function used by dynamic facil.io objects.
 *
 * Currently implemented using SipHash.
 */
uint64_t fiobj_sym_hash(const void *data, size_t len);

#endif
