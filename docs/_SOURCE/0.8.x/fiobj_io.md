---
title: facil.io - The FIOBJ IO (data) Stream Type
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

The `FIOBJ` IO Stream type abstracts away the difference between memory storage and IO (file) storage, allowing access to both using the same IO style API.

The API only allows seekable IO objects o be abstracted (files) and doesn't fit pipes / socket / network access.

## Type Definitions

The `FIOBJ` IO style defined the type `FIOBJ_T_IO`.

The extension uses one of the reserved numeral type values (`51`).

## Compile-time Settings.

The following compile time macro constants are defined.

#### `FIOBJ_IO_MAX_MEMORY_STORAGE`

```c
#define FIOBJ_IO_MAX_MEMORY_STORAGE ((1UL << 16) - 16) /* just shy of 64KB */
```

The limit after which memory storage is switched to file storage.

#### `FIOBJ_IO_MAX_FD_RW`

```c
#define FIOBJ_IO_MAX_FD_RW (1UL << 19) /* about 0.5Mb */
```

The point at which file `write` instructions are looped rather using a single `write`.

This is defined since some systems fail when attempting to call `write` with a large value.

## API

### Creating the IO (data) Stream object


#### `fiobj_io_new`

```c
FIOBJ fiobj_io_new();
```

Creates a new local IO object.

The storage type (memory vs. tmpfile) is managed automatically.


#### `fiobj_io_new2`

```c
FIOBJ fiobj_io_new2(size_t expected);
```

Creates a new local IO object pre-calculating the storage type using the expected capacity.

The storage type (memory vs. tmpfile) is managed automatically.

#### `fiobj_io_new_fd`

```c
FIOBJ fiobj_io_new_fd(int fd);
```

Creates a new IO object for the specified `fd`.

The `fd`'s "ownership" is transfered to the IO object, so the `fd` shouldn't be accessed directly (only using the IO object's API).

**Note 1**: Not all functionality is supported on all `fd` types. Pipes and sockets don't `seek` and behave differently than regular files.

**Note 2**: facil.io connection uuids shouldn't be used with a FIOBJ IO object, since they manage a user land buffer while the FIOBJ IO will directly make system-calls.

#### `fiobj_io_new_slice`

```c
FIOBJ fiobj_io_new_slice(FIOBJ src, size_t start_at, size_t limit);
```

Creates a new object using a "slice" from an existing one.

Remember to `fiobj_free` the new object.

This will fail if the existing IO object isn't "seekable" (i.e., doesn't represent a file or memory).

Returns FIOBJ_INVALID on error.


#### `fiobj_io_free`

```c
int fiobj_io_free(FIOBJ io);
```

Frees an IO object (or decreases it's reference count.

### Saving the IO Stream Data to Disk

#### `fiobj_io_save`

```c
int fiobj_io_save(FIOBJ io, const char *filename);
```

Saves the data in the Stream object to `filename`.

This will fail if the existing IO object isn't "seekable" (i.e., doesn't represent a file or memory).

Returns -1 on error.


### Reading / State API

#### `fiobj_io_read`

```c
fio_str_info_s fiobj_io_read(FIOBJ io, intptr_t len);
```

Reads up to `len` bytes and returns a temporary(!) buffer that is **not** NUL
terminated.

If `len` is zero or negative, it will be computed from the end of the input backwards if possible (0 == EOF, -1 == EOF, -2 == EOF - 1, ...).

The string information object will be invalidated the next time a function call to the Data Stream object is made.


#### `fiobj_io_read2ch`

```c
fio_str_info_s fiobj_io_read2ch(FIOBJ io, uint8_t token);
```


Reads until the `token` byte is encountered or until the end of the stream.

Returns a temporary(!) string information object, including the token marker but **without** a NUL terminator.

Careful when using this call on large file streams, as the whole file stream might be loaded into the memory.

The string information object will be invalidated the next time a function call to the Data Stream object is made.

The search for the token is limited to FIOBJ_IO_MAX_MEMORY_STORAGE bytes, after which the searched data is returned even though it will be missing the token terminator.

#### `fiobj_io_gets`

```c
#define fiobj_io_gets(io) fiobj_io_read2ch((io), '\n');
```

Reads a line (until the '\n' byte is encountered) or until the end of the available data.

Returns a temporary(!) string information object, including the '\n' marker but **without** a NUL terminator.

Careful when using this call on large file streams, as the whole file stream might be loaded into the memory.

The string information object will be invalidated the next time a function call to the Data Stream object is made.

The search for the EOL is limited to FIOBJ_IO_MAX_MEMORY_STORAGE bytes, after which the searched data is returned even though it will be missing the EOL terminator.

#### `fiobj_io_pos`

```c
intptr_t fiobj_io_pos(FIOBJ io);
```

Returns the current reading position. Returns -1 on error.

#### `fiobj_io_len`

```c
intptr_t fiobj_io_len(FIOBJ io);
```

Returns the known length of the stream (this might not always be true).

#### `fiobj_io2cstr`

```c
fio_str_info_s fiobj_io2cstr(FIOBJ io);
```

Dumps the content of the IO object into a string, IGNORING the FIOBJ_IO_MAX_MEMORY_STORAGE limitation(!). Attempts to return the reading position to it's original location.

#### `fiobj_io_seek`

```c
void fiobj_io_seek(FIOBJ io, intptr_t pos);
```


Moves the reading position to the requested position.

Negative values are computed from the end of the stream, where -1 == EOF. (-1 == EOF, -2 == EOF -1, ... ).

#### `fiobj_io_pread`

```c
fio_str_info_s fiobj_io_pread(FIOBJ io, intptr_t start_at, uintptr_t length);
```

Calls `fiobj_io_seek` and `fiobj_io_read`, attempting to move the reading position to `start_at` before reading any data.


### Writing API

#### `fiobj_io_write`

```c
intptr_t fiobj_io_write(FIOBJ io, const void *buf, uintptr_t len);
```

Writes UP TO `len` bytes at the end of the IO stream, ignoring the reading position.

Behaves and returns the same value as the system call `write`.

#### `fiobj_io_puts`

```c
intptr_t fiobj_io_puts(FIOBJ io, const void *buf, uintptr_t len);
```

Writes `length` bytes at the end of the Data Stream stream, ignoring the reading position, adding an EOL marker (`"\r\n"`) to the end of the stream.

Behaves and returns the same value as the system call `write`.
