/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_FILE_CACHE_H
/**
A simple READ ONLY file cache implementation that keeps the last
`FIO_FILE_CACHE_LIMIT` files open and offers a **shared** `fd`.

The file list is sorted (and resorted) by the last time a file was accessed.
*/
#define H_FIO_FILE_CACHE_H
#include <stdlib.h>
#include <sys/stat.h>

#ifndef FIO_FILE_CACHE_LIMIT
/** the limit at which the cache is considered full and files are closed. */
#define FIO_FILE_CACHE_LIMIT 1
#endif

/**
 * An opaque pointer that allows access to the cached file.
 *
 * The only promise made by this library is that the struct will start with the
 * following struct:
 *
 *        struct fio_cfd_s {  struct stat stat;   int fd;   }
 */
typedef struct fio_cfd_s *fio_cfd_pt;

#define fio_cfd_stat(fio_cfd) (((struct stat *)(fio_cfd))[0])
#define fio_cfd_fd(fio_cfd) (((int *)((struct stat *)(fio_cfd) + 1))[0])

/**
 * Opens a file (if it's not in the cache) and returns an `fio_cfd_pt` object.
 *
 * If the file can't be found, `NULL` is returned.
 */
fio_cfd_pt fio_cfd_open(const char *file_name, size_t length);

/** Handles file closure. The file may or may not be actually closed. */
void fio_cfd_close(fio_cfd_pt fio_cfd);

/**
 * Asuuming the `int` pointed to by `pfd` is part of a `fio_cfd_pt` object, this
 * will handle file closure.
 *
 *       fio_cfd_close_pfd( &fio_cfd_fd( fio_cfd ) );
 */
void fio_cfd_close_pfd(int *pfd);

/** Clears the open file cache. Files may remain open if in use. */
void fio_cfd_clear(void);

#endif
