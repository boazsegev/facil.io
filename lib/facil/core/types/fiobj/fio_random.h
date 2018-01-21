/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef fio_RANDOM_H
#define fio_RANDOM_H
/* *****************************************************************************
C++ extern
*/

#include <stdint.h>
#include <stdlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
Random stuff... (why is this not a system call?)
*/

/** returns 32 random bits. */
uint32_t fio_rand32(void);

/** returns 64 random bits. */
uint64_t fio_rand64(void);

/** returns a variable length string of random bytes. */
void fio_rand_bytes(void *target, size_t length);

#if DEBUG
void fio_random_test(void);
#endif

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
