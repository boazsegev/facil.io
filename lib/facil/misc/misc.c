/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "misc.h"
/* ***************************************************************************
Other helper functions
*/

#ifdef HAS_UNIX_FEATURES
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
Allocates memory and dumps the whole file into the memory allocated.

Remember to call `free` when done.

Returns the number of bytes allocated. On error, returns 0 and sets the
container pointer to NULL.

This function has some Unix specific properties that resolve links and user
folder referencing.
*/
fdump_s *bscrypt_fdump(const char *file_path, size_t size_limit) {
  struct stat f_data;
  int file = -1;
  fdump_s *container = NULL;
  size_t file_path_len;
  if (file_path == NULL || (file_path_len = strlen(file_path)) == 0 ||
      file_path_len > PATH_MAX)
    return NULL;

  char real_public_path[PATH_MAX + 1];
  real_public_path[PATH_MAX] = 0;
  if (file_path[0] == '~' && getenv("HOME") && file_path_len <= PATH_MAX) {
    strcpy(real_public_path, getenv("HOME"));
    memcpy(real_public_path + strlen(real_public_path), file_path + 1,
           file_path_len);
    file_path = real_public_path;
  }

  if (stat(file_path, &f_data))
    goto error;
  if (size_limit == 0 || (size_t)f_data.st_size < size_limit)
    size_limit = f_data.st_size;
  container = malloc(size_limit + sizeof(fdump_s) + 1);
  if (!container)
    goto error;
  file = open(file_path, O_RDONLY);
  if (file < 0)
    goto error;
  if (read(file, container->data, size_limit) != (ssize_t)size_limit)
    goto error;
  close(file);
  container->length = size_limit;
  container->data[size_limit] = 0;
  return container;
error:
  if (container)
    free(container), (container = NULL);
  if (file >= 0)
    close(file);
  return 0;
}

#endif /* HAS_UNIX_FEATURES */

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *bscrypt_gmtime(const time_t *timer, struct tm *tmbuf) {
  // static char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  // static char * Months = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  // "Jul",
  // "Aug", "Sep", "Oct", "Nov", "Dec"};
  static uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // leap year
  };
  if (*timer < 0)
    return gmtime_r(timer, tmbuf);
  ssize_t tmp;
  tmbuf->tm_gmtoff = 0;
  tmbuf->tm_zone = "UTC";
  tmbuf->tm_isdst = 0;
  tmbuf->tm_year = 70; // tm_year == The number of years since 1900
  tmbuf->tm_mon = 0;
  // for seconds up to weekdays, we build up, as small values clean up larger
  // values.
  tmp = ((ssize_t)*timer);
  tmbuf->tm_sec = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_min = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_hour = tmp % 24;
  tmp = tmp / 24;
  // day of epoch was a thursday. Add + 3 so sunday == 0...
  tmbuf->tm_wday = (tmp + 3) % 7;
// tmp == number of days since epoch
#define DAYS_PER_400_YEARS ((400 * 365) + 97)
  while (tmp >= DAYS_PER_400_YEARS) {
    tmbuf->tm_year += 400;
    tmp -= DAYS_PER_400_YEARS;
  }
#undef DAYS_PER_400_YEARS
#define DAYS_PER_100_YEARS ((100 * 365) + 24)
  while (tmp >= DAYS_PER_100_YEARS) {
    tmbuf->tm_year += 100;
    tmp -= DAYS_PER_100_YEARS;
    if (((tmbuf->tm_year / 100) & 3) ==
        0) // leap century divisable by 400 => add leap
      --tmp;
  }
#undef DAYS_PER_100_YEARS
#define DAYS_PER_32_YEARS ((32 * 365) + 8)
  while (tmp >= DAYS_PER_32_YEARS) {
    tmbuf->tm_year += 32;
    tmp -= DAYS_PER_32_YEARS;
  }
#undef DAYS_PER_32_YEARS
#define DAYS_PER_8_YEARS ((8 * 365) + 2)
  while (tmp >= DAYS_PER_8_YEARS) {
    tmbuf->tm_year += 8;
    tmp -= DAYS_PER_8_YEARS;
  }
#undef DAYS_PER_8_YEARS
#define DAYS_PER_4_YEARS ((4 * 365) + 1)
  while (tmp >= DAYS_PER_4_YEARS) {
    tmbuf->tm_year += 4;
    tmp -= DAYS_PER_4_YEARS;
  }
#undef DAYS_PER_4_YEARS
  while (tmp >= 365) {
    tmbuf->tm_year += 1;
    tmp -= 365;
    if ((tmbuf->tm_year & 3) == 0) { // leap year
      if (tmp > 0) {
        --tmp;
        continue;
      } else {
        tmp += 365;
        --tmbuf->tm_year;
        break;
      }
    }
  }
  tmbuf->tm_yday = tmp;
  if ((tmbuf->tm_year & 3) == 1) {
    // regular year
    for (size_t i = 0; i < 12; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  } else {
    // leap year
    for (size_t i = 12; i < 24; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  }
  tmbuf->tm_mday = tmp;
  return tmbuf;
}
