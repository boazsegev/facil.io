/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef bscrypt_MISC_H
#define bscrypt_MISC_H
#include "bscrypt-common.h"
#include <time.h>
/* *****************************************************************************
C++ extern
*/
#if defined(__cplusplus)
extern "C" {
#endif

/* ***************************************************************************
Miscellaneous helper functions

i.e. file content dumping and GMT time alternative to `gmtime_r`.
*/

#ifdef HAS_UNIX_FEATURES

/**
File dump data.

This struct (or, a pointer to this struct) is returned
by the `bscrypt.fdump`
function on success.

To free the pointer returned, simply call `free`.
*/
typedef struct {
  size_t length;
  char data[];
} fdump_s;

/**
Allocates memory and dumps the whole file into the memory allocated.

!!!: Remember to call `free` when done.

Returns the number of bytes allocated.

On error, returns 0 and sets the container pointer to NULL.

This function has some Unix specific properties that resolve links and user
folder referencing.
*/
fdump_s *bscrypt_fdump(const char *file_path, size_t size_limit);

#endif /* HAS_UNIX_FEATURES */

/**
A faster (yet less localized) alternative to
`gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *bscrypt_gmtime(const time_t *timer, struct tm *tmbuf);

/* *****************************************************************************
C++ extern finish
*/
#if defined(__cplusplus)
}
#endif

#endif
