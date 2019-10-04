/* *****************************************************************************
Copyright: Boaz Segev, 2019
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#include <fiobj_io.h> /* fio.h might not be available in FIOBJ_STANDALONE */

/* *****************************************************************************
IO type definitions and compile-time settings.
***************************************************************************** */

/** The type ID for the FIOBJ Stream type. */
// #define FIOBJ_T_IO 51

/** The limit after which memory storage is switched to file storage. */
// #define FIOBJ_IO_MAX_MEMORY_STORAGE ((1<<16)-16) /* just shy of 64KB */

/* *****************************************************************************







Memory based IO







***************************************************************************** */

typedef struct {
  char *buf;
  size_t capa;
  size_t len;
  size_t pos;
  int fd;
} fiobj_io_s;

#define FIOBJ_IO_INIT (fiobj_io_s){.buf = NULL, .fd = -1};

#define FIOBJ2IO(io_) ((fiobj_io_s *)FIOBJ_PTR_UNTAG(io_))

FIO_IFUNC void fiobj_io_init(fiobj_io_s *io) { *io = FIOBJ_IO_INIT; }
/**
 * Destroys the fiobj_io_s data.
 *
 * The storage type (memory vs. tmpfile) is managed automatically.
 */
FIO_IFUNC void fiobj_io_destroy(fiobj_io_s *io) {
  FIO_MEM_FREE(io->buf, io->capa);
  if (io->fd != -1)
    close(io->fd);
  *io = FIOBJ_IO_INIT;
}

/* *****************************************************************************
Creating the Data Stream object
***************************************************************************** */

FIOBJ fiobj_io_mem_new_slice(fiobj_io_s *io, size_t start_at, size_t limit) {
  if (!limit) {
    if (start_at >= io->len)
      return FIOBJ_INVALID;
    limit = io->len - start_at;
  }
  if (!limit || start_at >= io->len)
    return FIOBJ_INVALID;
  if (limit + start_at > io->len)
    limit = io->len - start_at;
  FIOBJ io_ = fiobj_io_new();
  FIO_ASSERT_ALLOC(FIOBJ2IO(io_));
  *FIOBJ2IO(io_) = (fiobj_io_s){
      .fd = -1,
      .buf = FIO_MEM_CALLOC(1, limit + 1),
      .capa = limit,
      .pos = 0,
      .len = limit,
  };
  FIO_ASSERT_ALLOC(FIOBJ2IO(io_)->buf);
  memcpy(FIOBJ2IO(io_)->buf, io->buf + start_at, limit);
  return io_;
}

/* *****************************************************************************
Saving the Data Stream object
***************************************************************************** */

/**
 * Saves the data in the Stream object to `filename`.
 *
 * This will fail if the existing IO object isn't "seekable" (i.e., doesn't
 * represent a file or memory).
 *
 * Returns -1 on error.
 */
// int fiobj_io_save(FIOBJ io, const char *filename);

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
FIO_SFUNC fio_str_info_s fiobj_io_mem_read(fiobj_io_s *io, intptr_t len) {
  if (io->len <= io->pos)
    return (fio_str_info_s){.buf = NULL};
  if (len < 0) {
    len = (io->len - io->pos) + len + 1;
    if (len <= 0)
      return (fio_str_info_s){.buf = NULL};
  }
  if (len == 0 || len + io->pos > io->len)
    len = io->len - io->pos;
  fio_str_info_s r =
      (fio_str_info_s){.buf = (io->buf + io->pos), .len = len, .capa = 0};
  io->pos += len;
  return r;
}

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
 */
FIO_SFUNC fio_str_info_s fiobj_io_mem_read2ch(fiobj_io_s *io, uint8_t token) {
  if (io->len <= io->pos)
    return (fio_str_info_s){.buf = NULL};
  char *tmp = memchr(io->buf + io->pos, token, io->len - io->pos);
  if (!tmp)
    tmp = io->buf + (io->len);
  else
    ++tmp;
  fio_str_info_s r = (fio_str_info_s){.buf = (io->buf + io->pos),
                                      .len = (tmp - (io->buf + io->pos)),
                                      .capa = 0};
  io->pos = (tmp - io->buf);
  return r;
}

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
 */
#define fiobj_io_mem_gets(io) fiobj_io_mem_read2ch((io), '\n');

/**
 * Returns the current reading position. Returns -1 on error.
 */
FIO_IFUNC intptr_t fiobj_io_mem_pos(fiobj_io_s *io) { return io->pos; }

/**
 * Returns the known length of the stream (this might not always be true).
 */
FIO_IFUNC intptr_t fiobj_io_mem_len(fiobj_io_s *io) { return io->len; }

/**
 * Dumps the content of the IO object into a string, IGNORINg the
 * FIOBJ_IO_MAX_MEMORY_STORAGE limitation(!). Attempts to return the reading
 * position to it's original location.
 */
fio_str_info_s fiobj_io_mem_2cstr(fiobj_io_s *io) {
  return (fio_str_info_s){.buf = io->buf, .len = io->len};
}

/**
 * Moves the reading position to the requested position. Negative vaulues are
 * computed from the end of the stream, where -1 == EOF.
 * (-1 == EOF, -2 == EOF -1, ... ).
 */
FIO_SFUNC void fiobj_io_mem_seek(fiobj_io_s *io, intptr_t pos) {
  if (pos < 0) {
    pos = io->len + pos + 1;
    if (pos < 0)
      pos = 0;
  }
  if ((uintptr_t)pos > io->len)
    pos = io->len;
  io->pos = pos;
}

/**
 * Calls `fiobj_io_seek` and `fiobj_io_mem_read`, attempting to move the
 * reading position to `start_at` before reading any data.
//  */
FIO_SFUNC fio_str_info_s fiobj_io_mem_pread(fiobj_io_s *io, intptr_t pos,
                                            uintptr_t len) {

  if (pos < 0) {
    pos = io->len + pos + 1;
    if (pos < 0)
      pos = 0;
  }
  if ((uintptr_t)pos >= io->len)
    return (fio_str_info_s){.buf = NULL};
  if (pos + len > io->len)
    len = io->len - pos;
  return (fio_str_info_s){.buf = io->buf, .len = len};
}

/* *****************************************************************************
Writing API
***************************************************************************** */

FIO_SFUNC fio_str_info_s fiobj_io_mem_pre_write(fiobj_io_s *io, uintptr_t len) {
  if (io->len + len >= io->capa) {
    const size_t new_capa = ((io->len + len + 1) + 15) & (~(uintptr_t)15);
    io->buf = FIO_MEM_REALLOC(io->buf, io->capa, new_capa, io->len);
    FIO_ASSERT_ALLOC(io->buf);
    io->capa = new_capa;
  }
  /* We don't use `capa` (.capa = io->capa - io->len) */
  return (fio_str_info_s){.buf = io->buf + io->len, .len = 0};
}
/**
 * Writes UP TO `len` bytes at the end of the IO stream, ignoring the
 * reading position.
 *
 * Behaves and returns the same value as the system call `write`.
 */
FIO_IFUNC intptr_t fiobj_io_mem_write(fiobj_io_s *io, const void *buf,
                                      uintptr_t len) {
  fio_str_info_s i = fiobj_io_mem_pre_write(io, len);
  memcpy(i.buf, buf, len);
  i.buf[len] = 0;
  io->pos = io->len += len;
  return len;
}

/**
 * Writes `length` bytes at the end of the Data Stream stream, ignoring the
 * reading position, adding an EOL marker ("\r\n") to the end of the stream.
 *
 * Behaves and returns the same value as the system call `write`.
 */
FIO_IFUNC intptr_t fiobj_io_mem_puts(fiobj_io_s *io, const void *buf,
                                     uintptr_t len) {
  fio_str_info_s i = fiobj_io_mem_pre_write(io, len);
  memcpy(i.buf, buf, len);
  i.buf[len++] = '\r';
  i.buf[len++] = '\n';
  i.buf[len] = 0;
  io->pos = io->len += len;
  return len;
}

/* *****************************************************************************







File Decriptor based IO







***************************************************************************** */
#if FIO_HAVE_UNIX_TOOLS

/* *****************************************************************************
Creating the Data Stream object
***************************************************************************** */

/**
 * Creates a new local IO object.
 *
 * The storage type (memory vs. tmpfile) is managed automatically.
 */
void fiobj_io_fd_tmpfile(fiobj_io_s *io) {
#ifdef P_tmpdir
  if (P_tmpdir[sizeof(P_tmpdir) - 1] == '/') {
    char name_template[] = P_tmpdir "facil_io_tmpfile_XXXXXXXX";
    io->fd = mkstemp(name_template);
  } else {
    char name_template[] = P_tmpdir "/facil_io_tmpfile_XXXXXXXX";
    io->fd = mkstemp(name_template);
  }
#else
  char name_template[] = "/tmp/facil_io_tmpfile_XXXXXXXX";
  io->fd = mkstemp(name_template);
#endif
}

/**
 * Creates a new IO object for the specified `fd`.
 *
 * The `fd`'s "ownership" is transffered to the IO object, so the `fd` shouldn't
 * be accessed directly (only using the IO object's API).
 *
 * NOTE 1: Not all functionality is supported on all `fd` types. Pipes and
 * sockets don't `seek` and behave differently than regular files.
 *
 * NOTE 2: facil.io conection uuids shouldn't be used with a FIOBJ IO object,
 * since they manage a user land buffer while the FIOBJ IO will directly make
 * system-calls.
 */
FIO_IFUNC void fiobj_io_fd_from_fd(fiobj_io_s *io, int fd) {
  if (fd == -1) {
    fiobj_io_fd_tmpfile(io);
    return;
  }
  io->fd = fd;
  lseek(fd, SEEK_SET, 0);
  return;
}

FIOBJ fiobj_io_fd_new_slice(fiobj_io_s *io, size_t start_at, size_t limit) {
  struct stat s = {0};
  if (fstat(io->fd, &s) == -1)
    return FIOBJ_INVALID;

  if (!limit) {
    if ((long long)start_at >= s.st_size)
      return FIOBJ_INVALID;
    limit = s.st_size - start_at;
  }
  if (!limit || (long long)start_at >= s.st_size)
    return FIOBJ_INVALID;
  if (limit + start_at > (size_t)s.st_size)
    limit = s.st_size - start_at;
  FIOBJ io_ = fiobj_io_new2(limit);
  FIO_ASSERT_ALLOC(FIOBJ2IO(io_));
  char buffer[65536];
  while (limit >= 65536) {
    if (pread(io->fd, buffer, 65536, start_at) == -1)
      goto read_error;
    fiobj_io_write(io_, buffer, 65536);
    limit -= 65536;
    start_at += 65536;
  }
  if (limit) {
    if (pread(io->fd, buffer, limit, start_at) == -1)
      goto read_error;
    fiobj_io_write(io_, buffer, limit);
  }
  return io_;
read_error:
  fiobj_io_free(io_);
  return FIOBJ_INVALID;
}

/* *****************************************************************************
Reading API
***************************************************************************** */

/**
 * Returns the current reading position. Returns -1 on error.
 */
FIO_IFUNC intptr_t fiobj_io_fd_pos(fiobj_io_s *io) {
  return (intptr_t)(lseek(io->fd, 0, SEEK_CUR));
}

/**
 * Returns the known length of the stream (this might not always be true).
 */
FIO_IFUNC intptr_t fiobj_io_fd_len(fiobj_io_s *io) {
  struct stat s = {0};
  if (fstat(io->fd, &s) == -1)
    return 0;
  return s.st_size;
}

/**
 * Dumps the content of the IO object into a string, IGNORINg the
 * FIOBJ_IO_MAX_MEMORY_STORAGE limitation(!). Attempts to return the reading
 * position to it's original location.
 */
fio_str_info_s fiobj_io_fd_2cstr(fiobj_io_s *io) {
  intptr_t len = fiobj_io_fd_len(io);
  if (!len) {
    /* unseekable? empty? */
    goto unseekable;
  }
  intptr_t old_pos = lseek(io->fd, 0, SEEK_CUR);
  if (old_pos == -1)
    goto unseekable;
  lseek(io->fd, 0, SEEK_SET);
  if (io->buf) {
    FIO_MEM_FREE(io->buf, io->capa);
  }
  io->buf = FIO_MEM_CALLOC(sizeof(char), len + 1);
  FIO_ASSERT_ALLOC(io->buf);
  io->capa = len + 1;
  io->len = len;
  ssize_t tmp;
  size_t total = 0;
  while (len >= FIOBJ_IO_MAX_FD_RW) {
    tmp = read(io->fd, io->buf + total, FIOBJ_IO_MAX_FD_RW);
    if (tmp <= 0)
      goto eof;
    total += tmp;
    len -= tmp;
  }
  if (len) {
    tmp = read(io->fd, io->buf + total, len);
    if (tmp <= 0)
      goto eof;
    total += tmp;
    len -= tmp;
  }
eof:
  io->len = total;
  io->pos = 0;
  lseek(io->fd, old_pos, SEEK_SET);
  return (fio_str_info_s){.buf = io->buf, .len = len};

unseekable:
  return (fio_str_info_s){.buf = NULL};
}

/**
 * Moves the reading position to the requested position.
 */
FIO_SFUNC void fiobj_io_fd_seek(fiobj_io_s *io, intptr_t pos) {
  if (pos < 0) {
    size_t len = fiobj_io_fd_len(io);
    if (!len)
      return;
    pos = pos + len + 1;
    if (pos < 0)
      pos = 0;
  }
  lseek(io->fd, pos, SEEK_SET);
}

/**
 * Reads up to `len` bytes and returns a temporary(!) buffer that is **not** NUL
 * terminated.
 *
 * If `len` is zero or negative, it will be computed from the end of the
 * input backwards if possible (0 == EOF).
 *
 * The string information object will be invalidated the next time a function
 * call to the Data Stream object is made.
 */
FIO_SFUNC fio_str_info_s fiobj_io_fd_read(fiobj_io_s *io, intptr_t len) {
  ssize_t l; /* temporary length from `read` */
  if (len < 0) {
    size_t total_len = fiobj_io_fd_len(io);
    if (!total_len) {
      len = 0;
    } else {
      len = total_len + len + 1;
      if (len < 0)
        len = 0;
    }
  }
  if (io->pos < io->len && io->pos + len <= io->len) {
    /* the data is already in the buffer, we can avoid a system call */
    size_t old_pos = io->pos;
    if (len == 0)
      len = io->len - io->pos;
    io->pos += len;
    return (fio_str_info_s){.buf = io->buf + old_pos, .len = len};
  }
  if (io->pos < io->len) {
    /* some data is already in the buffer, not all of it */
    char *tmp = FIO_MEM_CALLOC(len + 1, sizeof(char));
    FIO_ASSERT_ALLOC(tmp);
    memcpy(tmp, io->buf + io->pos, io->len - io->pos);
    FIO_MEM_FREE(io->buf, io->capa);
    io->capa = len + 1;
    io->buf = tmp;
    io->len = io->pos = io->len - io->pos;
    l = read(io->fd, tmp + io->pos, len - io->pos);
    if (l <= 0)
      return (fio_str_info_s){.buf = NULL};
    io->pos = (io->len += l);
    io->buf[io->len] = 0;
    return (fio_str_info_s){.buf = io->buf, .len = io->len};
  }
  /* no relevent data in the buffer */
  if (io->buf) {
    FIO_MEM_FREE(io->buf, io->capa);
  }
  io->capa = len + 1;
  io->buf = FIO_MEM_CALLOC(len + 1, sizeof(char));
  FIO_ASSERT_ALLOC(io->buf);
  l = read(io->fd, io->buf, len);
  if (l <= 0)
    return (fio_str_info_s){.buf = NULL};
  io->pos = io->len = l;
  return (fio_str_info_s){.buf = io->buf, .len = io->len};
}

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
 */
FIO_IFUNC fio_str_info_s fiobj_io_fd_read2ch(fiobj_io_s *io, uint8_t token) {
  const char *found;
  size_t lest_tested = 0;
  if (io->pos < io->len) {
    /* existing buffer might include the character we seek */
    found = memchr(io->buf + io->pos, token, io->len - io->pos);
    if (found)
      goto found_token;
    memmove(io->buf, io->buf + io->pos, io->len - io->pos);
    io->len -= io->pos;
    io->pos = 0;
  }
  if (io->len == io->pos) {
    io->len = io->pos = 0;
  }
  lest_tested = io->pos;
  if (io->capa - io->len) {
    /* fill existing buffer before growing buffer */
    ssize_t i = read(io->fd, io->buf, io->capa - io->len);
    if (i <= 0)
      goto at_eof;
    io->len += i;
    found = memchr(io->buf + lest_tested, token, io->len - lest_tested);
    if (found)
      goto found_token;
    lest_tested = io->len;
  }
  /* TODO : cycle until capa is maxed out or token is found */
  while (io->capa < FIOBJ_IO_MAX_MEMORY_STORAGE) {
    size_t old_capa = io->capa;
    io->capa = ((io->capa + (8192 - 16)) & (~(size_t)0ULL << 12)) - 16;
    FIO_MEM_REALLOC(io->buf, old_capa, io->capa, io->len);
    (void)old_capa; /* in case it's unused by FIO_MEM_REALLOC */
    ssize_t i = read(io->fd, io->buf, io->capa - io->len);
    if (i <= 0)
      goto at_eof;
    io->len += i;
    found = memchr(io->buf + lest_tested, token, io->len - lest_tested);
    if (found)
      goto found_token;
    lest_tested = io->len;
  }
  /* fallthrough to at_eof, since we can't read any more data from the file. */
at_eof:
  found = (io->len - lest_tested)
              ? (memchr(io->buf + lest_tested, token, io->len - lest_tested))
              : NULL;
  if (found)
    goto found_token;
  {
    const size_t old_pos = io->pos;
    io->pos = io->len;
    return (fio_str_info_s){.buf = io->buf + old_pos, .len = io->pos - old_pos};
  }

found_token:
  (void)found;
  {
    const size_t old_pos = io->pos;
    io->pos = (found - io->buf) + 1;
    return (fio_str_info_s){.buf = io->buf + old_pos, .len = io->pos - old_pos};
  }
}

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
 */
#define fiobj_io_fd_gets(io) fiobj_io_fd_read2ch((io), '\n');

/**
 * Calls `fiobj_io_fd_seek` and `fiobj_io_fd_read`, attempting to move the
 * reading position to `start_at` before reading any data.
 */
fio_str_info_s fiobj_io_fd_pread(fiobj_io_s *io, intptr_t pos, uintptr_t len) {
  if (pos < 0) {
    intptr_t io_len = fiobj_io_fd_len(io);
    pos = io_len + pos + 1;
    if (pos < 0)
      pos = 0;
  }
  if (pos == 0 && len <= 0)
    return fiobj_io2cstr((FIOBJ)io);
  return (fio_str_info_s){.buf = NULL};
}

/* *****************************************************************************
Writing API
***************************************************************************** */

/**
 * Writes UP TO `len` bytes at the end of the IO stream, ignoring the
 * reading position.
 *
 * Behaves and returns the same value as the system call `write`.
 */
FIO_IFUNC intptr_t fiobj_io_fd_write(fiobj_io_s *io, const void *buf,
                                     uintptr_t len) {
  ssize_t total = 0;
  ssize_t tmp = 0;
  while (len > FIOBJ_IO_MAX_FD_RW) {
    tmp = write(io->fd, (const char *)buf + total, FIOBJ_IO_MAX_FD_RW);
    if (tmp <= 0)
      goto failed;
    total += tmp;
    len -= tmp;
  }
  tmp = write(io->fd, (const char *)buf + total, len);
  if (tmp <= 0)
    goto failed;
  total += tmp;
  len -= tmp;
  return total;

failed:
  if (!total)
    return tmp;
  return total;
}

/**
 * Writes `length` bytes at the end of the Data Stream stream, ignoring the
 * reading position, adding an EOL marker ("\r\n") to the end of the stream.
 *
 * Behaves and returns the same value as the system call `write`.
 */
FIO_IFUNC intptr_t fiobj_io_fd_puts(fiobj_io_s *io, const void *buf,
                                    uintptr_t len) {
  intptr_t r = fiobj_io_fd_write(io, buf, len);
  if (r == (intptr_t)len) {
    r += 2;
    fiobj_io_fd_write(io, "\r\n", 2);
  }
  return r;
}

/* *****************************************************************************
Convertting between IO types
***************************************************************************** */

FIO_IFUNC void fiobj_io_write_test_convert(fiobj_io_s *io, uintptr_t len) {
  if (io->fd != -1 || io->len + len <= FIOBJ_IO_MAX_MEMORY_STORAGE)
    return;
  fiobj_io_s tmp = FIOBJ_IO_INIT;
  fiobj_io_fd_tmpfile(&tmp);
  fiobj_io_fd_write(&tmp, io->buf, io->len);
  fiobj_io_destroy(io);
  *io = tmp;
}

#else /* FIO_HAVE_UNIX_TOOLS */
#warning The FIOBJ stream extension (fiobj_io) requires a POSIX system
#define fiobj_io_write_test_convert(...)
#define fiobj_io_fd_tmpfile(io)
#endif /* FIO_HAVE_UNIX_TOOLS */

/* *****************************************************************************







FIOBJ Type (VTable)







***************************************************************************** */

static unsigned char fiobj_io_is_eq(FIOBJ a, FIOBJ b) {
  if (FIOBJ2IO(a)->fd != -1 || FIOBJ2IO(b)->fd)
    return 0; /* can't compare files without deeply effecting the object */
  if (FIOBJ2IO(a)->len == FIOBJ2IO(b)->len &&
      (FIOBJ2IO(a)->len == 0 ||
       !memcmp(FIOBJ2IO(a)->buf, FIOBJ2IO(b)->buf, FIOBJ2IO(a)->len)))
    return 1;
  return 0;
}

static double fiobj_io2f(FIOBJ o) { return (double)fiobj_io_len(o); }

FIO_SFUNC uint32_t fiobj___io_count_noop(FIOBJ o) {
  return 0;
  (void)o;
}

extern FIOBJ_class_vtable_s FIOBJ___IO_CLASS_VTBL;

#define FIO_EXTERN
#define FIO_EXTERN_COMPLETE 1
#define FIO_REF_CONSTRUCTOR_ONLY 1
#define FIO_REF_NAME fiobj_io
#define FIO_REF_DESTROY(io)                                                    \
  do {                                                                         \
    fiobj_io_destroy(&io);                                                     \
  } while (0)
#define FIO_REF_INIT(io)                                                       \
  do {                                                                         \
    io = FIOBJ_IO_INIT;                                                        \
  } while (0)
#define FIO_REF_METADATA FIOBJ_class_vtable_s *
#define FIO_REF_METADATA_INIT(m)                                               \
  do {                                                                         \
    m = &FIOBJ___IO_CLASS_VTBL;                                                \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_PTR_TAG(p) ((uintptr_t)p | FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p) FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE FIOBJ
#include "fio-stl.h"

FIOBJ_class_vtable_s FIOBJ___IO_CLASS_VTBL = {
    /**
     * MUST return a unique number to identify object type.
     *
     * Numbers (type IDs) under 100 are reserved. Numbers under 40 are illegal.
     */
    .type_id = FIOBJ_T_IO,
    /** Test for equality between two objects with the same `type_id` */
    .is_eq = fiobj_io_is_eq,
    /** Converts an object to a String */
    .to_s = fiobj_io2cstr,
    /** Converts an object to an integer */
    .to_i = fiobj_io_len,
    /** Converts an object to a double */
    .to_f = fiobj_io2f,
    /** Returns the number of exposed elements held by the object, if any. */
    .count = fiobj___io_count_noop,
    /** Iterates the exposed elements held by the object. See `fiobj_each1`. */
    .each1 = NULL,
    /**
     * Decreases the referenmce count and/or frees the object, calling `free2`
     * for any nested objects.
     *
     * Returns 0 if the object is still alive or 1 if the object was freed. The
     * return value is currently ignored, but this might change in the future.
     */
    .free2 = fiobj_io_free,
};

/* *****************************************************************************







External API







***************************************************************************** */

#if FIO_HAVE_UNIX_TOOLS
#define ROUTE_FUNCTION_AND_RETURN(io_, function_name, ...)                     \
  switch (FIOBJ2IO(io_)->fd) {                                                 \
  case -1:                                                                     \
    return fiobj_io_mem_##function_name(__VA_ARGS__);                          \
  /* case -2:  return fiobj_io_slice_##function_name(__VA_ARGS__); */          \
  default:                                                                     \
    return fiobj_io_fd_##function_name(__VA_ARGS__);                           \
  }

#define ROUTE_FUNCTION_NO_RETURN(io_, function_name, ...)                      \
  switch (FIOBJ2IO(io_)->fd) {                                                 \
  case -1:                                                                     \
    fiobj_io_mem_##function_name(__VA_ARGS__);                                 \
    break;                                                                     \
  /* case -2:  fiobj_io_slice_##function_name(__VA_ARGS__); break; */          \
  default:                                                                     \
    fiobj_io_fd_##function_name(__VA_ARGS__);                                  \
    break;                                                                     \
  }

#else /* FIO_HAVE_UNIX_TOOLS */
#define ROUTE_FUNCTION_AND_RETURN(io_, function_name, ...)                     \
  return fiobj_io_mem_##function_name(__VA_ARGS__);

#define ROUTE_FUNCTION_NO_RETURN(io_, function_name, ...)                      \
  fiobj_io_mem_##function_name(__VA_ARGS__);

#define fiobj_io_fd_tmpfile()
#endif
/* *****************************************************************************
Creating the Data Stream object
***************************************************************************** */

/**
 * Creates a new local IO object.
 *
 * The storage type (memory vs. tmpfile) is managed automatically.
 */
FIOBJ fiobj_io_new2(size_t expected) {
  FIOBJ io_ = fiobj_io_new();
  if (expected > FIOBJ_IO_MAX_MEMORY_STORAGE) {
    fiobj_io_fd_tmpfile(FIOBJ2IO(io_));
  }
  return io_;
}

/**
 * Creates a new IO object for the specified `fd`.
 *
 * The `fd`'s "ownership" is transffered to the IO object, so the `fd` shouldn't
 * be accessed directly (only using the IO object's API).
 *
 * NOTE 1: Not all functionality is supported on all `fd` types. Pipes and
 * sockets don't `seek` and behave differently than regular files.
 *
 * NOTE 2: facil.io conection uuids shouldn't be used with a FIOBJ IO object,
 * since they manage a user land buffer while the FIOBJ IO will directly make
 * system-calls.
 */
FIOBJ fiobj_io_new_fd(int fd) {
#if FIO_HAVE_UNIX_TOOLS
  FIOBJ io_ = fiobj_io_new();
  FIOBJ2IO(io_)->fd = fd;
  return io_;
#else /* FIO_HAVE_UNIX_TOOLS */
  return FIOBJ_INVALID;
#endif
}

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
FIOBJ fiobj_io_new_slice(FIOBJ src, size_t start_at, size_t limit) {
  ROUTE_FUNCTION_AND_RETURN(src, new_slice, FIOBJ2IO(src), start_at, limit);
}

/* *****************************************************************************
Saving the Data Stream object
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
fio_str_info_s fiobj_io_read(FIOBJ io, intptr_t len) {
  ROUTE_FUNCTION_AND_RETURN(io, read, FIOBJ2IO(io), len);
}

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
 */
fio_str_info_s fiobj_io_read2ch(FIOBJ io, uint8_t token) {
  ROUTE_FUNCTION_AND_RETURN(io, read2ch, FIOBJ2IO(io), token);
}

/**
 * Returns the current reading position. Returns -1 on error.
 */
intptr_t fiobj_io_pos(FIOBJ io) {
  ROUTE_FUNCTION_AND_RETURN(io, pos, FIOBJ2IO(io));
}

/**
 * Returns the known length of the stream (this might not always be true).
 */
intptr_t fiobj_io_len(FIOBJ io) {
  ROUTE_FUNCTION_AND_RETURN(io, len, FIOBJ2IO(io));
}

/**
 * Dumps the content of the IO object into a string, IGNORINg the
 * FIOBJ_IO_MAX_MEMORY_STORAGE limitation(!). Attempts to return the reading
 * position to it's original location.
 */
fio_str_info_s fiobj_io2cstr(FIOBJ io) {
  ROUTE_FUNCTION_AND_RETURN(io, 2cstr, FIOBJ2IO(io));
}

/**
 * Moves the reading position to the requested position.
 *
 * Negative vaulues are computed from the end of the stream, where -1 == EOF.
 * (-1 == EOF, -2 == EOF -1, ... ).
 */
void fiobj_io_seek(FIOBJ io, intptr_t pos) {
  ROUTE_FUNCTION_NO_RETURN(io, seek, FIOBJ2IO(io), pos);
}

/**
 * Calls `fiobj_io_seek` and `fiobj_io_read`, attempting to move the reading
 * position to `start_at` before reading any data.
 */
fio_str_info_s fiobj_io_pread(FIOBJ io, intptr_t start_at, uintptr_t len) {
  intptr_t old_pos = fiobj_io_pos(io);
  if (old_pos == -1)
    goto unseekable;
  fiobj_io_seek(io, start_at);
  fio_str_info_s i = fiobj_io_read(io, len);
  fiobj_io_seek(io, old_pos);
  return i;
unseekable:
  return (fio_str_info_s){.buf = NULL};
}

/* *****************************************************************************
Writing API
***************************************************************************** */

/**
 * Writes UP TO `len` bytes at the end of the IO stream, ignoring the
 * reading position.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_write(FIOBJ io, const void *buf, uintptr_t len) {
  fiobj_io_write_test_convert(FIOBJ2IO(io), len);
  ROUTE_FUNCTION_AND_RETURN(io, write, FIOBJ2IO(io), buf, len);
}

/**
 * Writes `length` bytes at the end of the Data Stream stream, ignoring the
 * reading position, adding an EOL marker ("\r\n") to the end of the stream.
 *
 * Behaves and returns the same value as the system call `write`.
 */
intptr_t fiobj_io_puts(FIOBJ io, const void *buf, uintptr_t len) {
  fiobj_io_write_test_convert(FIOBJ2IO(io), len);
  ROUTE_FUNCTION_AND_RETURN(io, puts, FIOBJ2IO(io), buf, len);
}

#if TEST || DEBUG
void fiobj_io_test FIO_NOOP(void) {
  fprintf(stderr, "===============\n");
  fprintf(stderr, "* Testing FIOBJ IO extension (not for Socket IO).\n");
#if !FIO_HAVE_UNIX_TOOLS
  fprintf(stderr,
          "* WARNING: non-POSIX system, disk storage / API unavailable.\n");
#endif
  FIOBJ o = fiobj_io_new();
  fiobj_io_write(o, "Hello\r\nWorld", 12);
  FIO_ASSERT(fiobj_io_len(o) == 12, "write failed on empty object!");
  fiobj_io_write(o, " (Earth)", 8);
  FIO_ASSERT(fiobj_io_len(o) == 20, "write failed to append data to object!");
  FIO_ASSERT(fiobj_io_pos(o) == 20,
             "position error (%ld should be zero based).",
             (long)fiobj_io_pos(o));
  if (0) {
    FIOBJ s = fiobj_io_new_slice(o, fiobj_io_len(o), 0);
    FIO_ASSERT(s == FIOBJ_INVALID, "zero length slice should fail");
    s = fiobj_io_new_slice(o, fiobj_io_len(o), 30);
    FIO_ASSERT(s == FIOBJ_INVALID, "zero length slice should fail (2)");
    s = fiobj_io_new_slice(o, 0, 5);
    FIO_ASSERT(s, "memory slice failed");
    FIO_ASSERT(fiobj_io_len(s) == 5, "memory slice length error");
    FIO_ASSERT(fiobj_io_read(s, -1).len == 5, "memory slice read length error");
    fiobj_io_seek(s, 0);
    FIO_ASSERT(!memcmp("Hello", fiobj_io_read(s, -1).buf, 5),
               "memory slice copy error");
    fiobj_io_free(s);
  }

  fiobj_io_seek(o, 0);
  FIO_ASSERT(fiobj_io_pos(o) == 0, "position should be zero after seeking.");
  fiobj_io_seek(o, -1);
  FIO_ASSERT(fiobj_io_pos(o) == 20,
             "position should be at end after seeking to -1.");
  fiobj_io_seek(o, 0);
  fio_str_info_s i = fiobj_io_gets(o);
  FIO_ASSERT(i.len == 7 && !memcmp("Hello\r\n", i.buf, 7),
             "fiobj_io_gets failed to retrieve first line.\n%s", i.buf);
  i = fiobj_io_gets(o);
  FIO_ASSERT(i.len == 13 && !memcmp("World (Earth)", i.buf, 13),
             "fiobj_io_gets failed to retrieve second line (%zu).\n%s", i.len,
             i.buf);
  fiobj_free(o);
  fprintf(stderr, "* WARNING: FIOBJ_IO not fully implemented just yet...\n");
}
#endif
