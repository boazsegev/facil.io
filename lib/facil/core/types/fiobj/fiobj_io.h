/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#if !defined(H_FIOBJ_IO_H) && (defined(__unix__) || defined(__APPLE__) ||      \
                               defined(__linux__) || defined(__CYGWIN__))

/**
 * A dynamic type for reading / writing to a local file,  a temporary file or an
 * in-memory string.
 *
 * Supports basic reak, write, seek, puts and gets operations.
 *
 * Writing is always performed at the end of the stream / memory buffer,
 * ignoring the current seek position.
 */
#define H_FIOBJ_IO_H

#include "fiobject.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The local IO abstraction type indentifier. */
extern const uintptr_t FIOBJ_T_IO;

/* *****************************************************************************
Creating the IO object
***************************************************************************** */

/** Creates a new local in-memory IO object */
fiobj_s *fiobj_io_newstr(void);

/**
 * Creates a IO object from an existing buffer. The buffer will be deallocated
 * using the provided `dealloc` function pointer. Use a NULL `dealloc` function
 * pointer if the buffer is static and shouldn't be freed.
 */
fiobj_s *fiobj_io_newstr2(void *buffer, uintptr_t length,
                          void (*dealloc)(void *));

/** Creates a new local tempfile IO object */
fiobj_s *fiobj_io_newtmpfile(void);

/** Creates a new local file IO object */
fiobj_s *fiobj_io_newfd(int fd);

/* *****************************************************************************
Reading API
***************************************************************************** */

/**
 * Reads up to `length` bytes and returns a temporary(!) buffer object (not NUL
 * terminated).
 *
 * The C string object will be invalidate the next time a function call to the
 * IO object is made.
 */
fio_cstr_s fiobj_io_read(fiobj_s *io, intptr_t length);

/**
 * Reads until the `token` byte is encountered or until the end of the stream.
 *
 * Returns a temporary(!) C string including the end of line marker.
 *
 * Careful when using this call on large file streams, as the whole file
 * stream might be loaded into the memory.
 *
 * The C string object will be invalidate the next time a function call to the
 * IO object is made.
 */
fio_cstr_s fiobj_io_read2ch(fiobj_s *io, uint8_t token);

/**
 * Reads a line (until the '\n' byte is encountered) or until the end of the
 * available data.
 *
 * Returns a temporary(!) buffer object (not NUL terminated) including the end
 * of line marker.
 *
 * Careful when using this call on large file streams, as the whole file stream
 * might be loaded into the memory.
 *
 * The C string object will be invalidate the next time a function call to the
 * IO object is made.
 */
#define fiobj_io_gets(io) fiobj_io_read2ch((io), '\n');

/**
 * Moves the reading position to the requested position.
 */
void fiobj_io_seek(fiobj_s *io, intptr_t position);

/**
 * Reads up to `length` bytes starting at `start_at` position and returns a
 * temporary(!) buffer object (not NUL terminated) string object. The reading
 * position is ignored and unchanged.
 *
 * The C string object will be invalidate the next time a function call to the
 * IO object is made.
 */
fio_cstr_s fiobj_io_pread(fiobj_s *io, intptr_t start_at, uintptr_t length);

/* *****************************************************************************
Writing API
***************************************************************************** */

/**
 * Writes `length` bytes at the end of the IO stream, ignoring the reading
 * position.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_write(fiobj_s *io, void *buffer, uintptr_t length);

/**
 * Writes `length` bytes at the end of the IO stream, ignoring the reading
 * position, adding an EOL marker ("\r\n") to the end of the stream.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_puts(fiobj_s *io, void *buffer, uintptr_t length);

#if DEBUG
void fiobj_io_test(char *filename);
#endif

#ifdef __cplusplus
}
#endif

#endif
