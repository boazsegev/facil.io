#ifndef H_FIOBJ_IO_H
/* *****************************************************************************
Copyright: Boaz Segev, 2019
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#define H_FIOBJ_IO_H

#include <fiobj.h> /* fio.h might not be available in FIOBJ_STANDALONE */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
IO type definitions and compile-time settings.
***************************************************************************** */

/** The type ID for the FIOBJ Stream type. */
#define FIOBJ_T_IO 51

/** The limit after which memory storage is switched to file storage. */
#define FIOBJ_IO_MAX_MEMORY_STORAGE ((1 << 16) - 16) /* just shy of 64KB */

/**
 * The point at which file `write` instructions are looped rather using a single
 * `write`.
 *
 * This is defined since some systems fail when attempting to call `write` with
 * a large value.
 */
#define FIOBJ_IO_MAX_FD_RW (1 << 19) /* about 0.5Mb */

/* *****************************************************************************
Creating the IO (data) Stream object
***************************************************************************** */

/**
 * Creates a new local IO object.
 *
 * The storage type (memory vs. tmpfile) is managed automatically.
 */
FIOBJ fiobj_io_new();

/**
 * Creates a new local IO object pre-calculating the storage type using the
 * expected capacity.
 *
 * The storage type (memory vs. tmpfile) is managed automatically.
 */
FIOBJ fiobj_io_new2(size_t expected);

/**
 * Creates a new IO object for the specified `fd`.
 *
 * The `fd`'s "ownership" is transfered to the IO object, so the `fd` shouldn't
 * be accessed directly (only using the IO object's API).
 *
 * NOTE 1: Not all functionality is supported on all `fd` types. Pipes and
 * sockets don't `seek` and behave differently than regular files.
 *
 * NOTE 2: facil.io connection uuids shouldn't be used with a FIOBJ IO object,
 * since they manage a user land buffer while the FIOBJ IO will directly make
 * system-calls.
 */
FIOBJ fiobj_io_new_fd(int fd);

/**
 * Creates a new object using a "slice" from an existing one.
 *
 * Remember to `fiobj_free` the new object.
 *
 * This will fail if the existing IO object isn't "seekable" (i.e., doesn't
 * represent a file or memory).
 *
 * Returns FIOBJ_INVALID on error.
 */
FIOBJ fiobj_io_new_slice(FIOBJ src, size_t start_at, size_t limit);

/** Frees an IO object (or decreases it's reference count. */
int fiobj_io_free(FIOBJ io);

/* *****************************************************************************
Saving the IO Stream Data to Disk
***************************************************************************** */

/**
 * Saves the data in the Stream object to `filename`.
 *
 * This will fail if the existing IO object isn't "seekable" (i.e., doesn't
 * represent a file or memory).
 *
 * Returns -1 on error.
 */
int fiobj_io_save(FIOBJ io, const char *filename);

/* *****************************************************************************
Reading API
***************************************************************************** */

/**
 * Reads up to `len` bytes and returns a temporary(!) buffer that is **not** NUL
 * terminated.
 *
 * If `len` is zero or negative, it will be computed from the end of the
 * input backwards if possible (0 == EOF, -1 == EOF, -2 == EOF - 1, ...).
 *
 * The string information object will be invalidated the next time a function
 * call to the Data Stream object is made.
 */
fio_str_info_s fiobj_io_read(FIOBJ io, intptr_t len);

/**
 * Reads until the `token` byte is encountered or until the end of the stream.
 *
 * Returns a temporary(!) string information object, including the token marker
 * but **without** a NUL terminator.
 *
 * Careful when using this call on large file streams, as the whole file
 * stream might be loaded into the memory.
 *
 * The string information object will be invalidated the next time a function
 * call to the Data Stream object is made.
 *
 * The search for the token is limited to FIOBJ_IO_MAX_MEMORY_STORAGE bytes,
 * after which the searched data is returned even though it will be missing the
 * token terminator.
 */
fio_str_info_s fiobj_io_read2ch(FIOBJ io, uint8_t token);

/**
 * Reads a line (until the '\n' byte is encountered) or until the end of the
 * available data.
 *
 * Returns a temporary(!) string information object, including the '\n' marker
 * but **without** a NUL terminator.
 *
 * Careful when using this call on large file streams, as the whole file stream
 * might be loaded into the memory.
 *
 * The string information object will be invalidated the next time a function
 * call to the Data Stream object is made.
 *
 * The search for the EOL is limited to FIOBJ_IO_MAX_MEMORY_STORAGE bytes, after
 * which the searched data is returned even though it will be missing the EOL
 * terminator.
 */
#define fiobj_io_gets(io) fiobj_io_read2ch((io), '\n');

/**
 * Returns the current reading position. Returns -1 on error.
 */
intptr_t fiobj_io_pos(FIOBJ io);

/**
 * Returns the known length of the stream (this might not always be true).
 */
intptr_t fiobj_io_len(FIOBJ io);

/**
 * Dumps the content of the IO object into a string, IGNORING the
 * FIOBJ_IO_MAX_MEMORY_STORAGE limitation(!). Attempts to return the reading
 * position to it's original location.
 */
fio_str_info_s fiobj_io2cstr(FIOBJ io);

/**
 * Moves the reading position to the requested position.
 *
 * Negative values are computed from the end of the stream, where -1 == EOF.
 * (-1 == EOF, -2 == EOF -1, ... ).
 */
void fiobj_io_seek(FIOBJ io, intptr_t pos);

/**
 * Calls `fiobj_io_seek` and `fiobj_io_read`, attempting to move the reading
 * position to `start_at` before reading any data.
 */
fio_str_info_s fiobj_io_pread(FIOBJ io, intptr_t start_at, uintptr_t length);

/* *****************************************************************************
Writing API
***************************************************************************** */

/**
 * Writes UP TO `len` bytes at the end of the IO stream, ignoring the
 * reading position.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_write(FIOBJ io, const void *buf, uintptr_t len);

/**
 * Writes `length` bytes at the end of the Data Stream stream, ignoring the
 * reading position, adding an EOL marker ("\r\n") to the end of the stream.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_puts(FIOBJ io, const void *buf, uintptr_t len);

#if TEST || DEBUG
void fiobj_io_test(void);
#else
#define fiobj_io_test()                                                        \
  fprintf(stderr, "* FIOBJ stream extension testing requires DEBUG mode.\n")
#endif /* TEST || DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* H_FIOBJ_IO_H */
