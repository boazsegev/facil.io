
#define FIO_STR_NAME fio_str
#define FIO_CLI 1
#define FIO_LOG 1
#include <fio-stl.h>

#include <fio.h>

#include <errno.h>
#include <time.h>

#define ALLOW_UNALIGNED_MEMORY_ACCESS 0

static inline void *fio_memchr_naive(const void *buffer_, const int c,
                                     size_t buffer_len) {
  const char *buffer = (const char *)buffer_;
  while (buffer_len--) {
    if (*buffer == c)
      return (void *)(buffer);
    ++buffer;
  }
  return NULL;
}

static inline void *fio_memchr2_naive(const void *buffer_, size_t buffer_len,
                                      const int c1, const int c2) {
  const char *buffer = (const char *)buffer_;
  while (buffer_len--) {
    if (*buffer == (char)c1 || *buffer == (char)c2)
      return (void *)(buffer);
    ++buffer;
  }
  return NULL;
}

/**
 * Finds the first occurance of up to 8 characters.
 */
static inline void *fio_memchrx_naive(const void *buffer,
                                      const size_t buffer_len,
                                      const void *markers_,
                                      const uint8_t markers_count) {
  if (!buffer || !buffer_len)
    return NULL;
  FIO_ASSERT(markers_count <= 8,
             "cannot search for more then 8 characters at a time");
  if (markers_count > 8)
    abort();
  const uint8_t *buf = (const uint8_t *)buffer;
  const uint8_t *brk = (const uint8_t *)markers_;
  size_t pos = 0;

  while (pos < buffer_len) {
    switch (markers_count) {
    case 8:
      if (buf[pos] == brk[7])
        goto found;
      /* fallthrough */
    case 7:
      if (buf[pos] == brk[6])
        goto found;
      /* fallthrough */
    case 6:
      if (buf[pos] == brk[5])
        goto found;
      /* fallthrough */
    case 5:
      if (buf[pos] == brk[4])
        goto found;
      /* fallthrough */
    case 4:
      if (buf[pos] == brk[3])
        goto found;
      /* fallthrough */
    case 3:
      if (buf[pos] == brk[2])
        goto found;
      /* fallthrough */
    case 2:
      if (buf[pos] == brk[1])
        goto found;
      /* fallthrough */
    case 1:
      if (buf[pos] == brk[0])
        goto found;
      /* fallthrough */
    }
    ++pos;
  }
  return NULL;

found:
  return (void *)(buf + pos);
}

/**
 * Finds the first occurance of up to 8 characters.
 */
static inline void *fio_memchr(const void *buffer, const int brk,
                               const size_t buffer_len) {
  if (!buffer || !buffer_len)
    return NULL;
  const uint8_t *buf = (const uint8_t *)buffer;
  size_t pos = 0;

  /* too short for this mess */
  if (buffer_len <= 15)
    goto tail;

  /* align memory */
  while ((uintptr_t)(buf + pos) & 3) {
    if (buf[pos] == brk)
      goto found;
    ++pos;
  }

  { /* 4 bytes */
    const uint32_t wanted = 0x01010101ULL * brk;
    const uint32_t eq = ~(*(uint32_t *)(buf + pos) ^ wanted);
    if ((((eq & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq & 0x80808080ULL)))
      goto tail;
    pos += 4;
  }

  { /* 8 byte steps*/
    const uint64_t wanted = 0x0101010101010101ULL * brk;
    while (pos + 8 <= buffer_len) {
      const uint64_t eq = ~(*(uint64_t *)(buf + pos) ^ wanted);
      if ((((eq & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq & 0x8080808080808080ULL)))
        goto tail;
      pos += 8;
    }
  }

tail:

  while (pos < buffer_len) {
    if (buf[pos] == brk)
      goto found;
    ++pos;
  }
  return NULL;

found:
  return (void *)(buf + pos);
}

/**
 * Finds the first occurance of up to 8 characters.
 */
static inline void *fio_memchr2(const void *buffer, const size_t buffer_len,
                                const int brk1, const int brk2) {
  if (!buffer || !buffer_len)
    return NULL;
  const uint8_t *buf = (const uint8_t *)buffer;
  size_t pos = 0;

  /* too short for this mess */
  if (buffer_len <= 15)
    goto tail;

  /* align memory */
  while ((uintptr_t)(buf + pos) & 3) {
    if (buf[pos] == brk1 || buf[pos] == brk2)
      return (void *)(buf + pos);
    ++pos;
  }

  { /* 4 bytes */
    const uint32_t wanted = 0x01010101ULL * brk1;
    const uint32_t eq = ~(*(uint32_t *)(buf + pos) ^ wanted);
    if ((((eq & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq & 0x80808080ULL)))
      goto tail;
  }
  { /* 4 bytes */
    const uint32_t wanted = 0x01010101ULL * brk2;
    const uint32_t eq = ~(*(uint32_t *)(buf + pos) ^ wanted);
    if ((((eq & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq & 0x80808080ULL)))
      goto tail;
  }

  { /* 8 byte steps*/
    const uint64_t wanted1 = 0x0101010101010101ULL * brk1;
    const uint64_t wanted2 = 0x0101010101010101ULL * brk2;
    while (pos + 8 <= buffer_len) {
      const uint64_t eq1 = ~(*(uint64_t *)(buf + pos) ^ wanted1);
      const uint64_t eq2 = ~(*(uint64_t *)(buf + pos) ^ wanted2);
      if ((((eq1 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq1 & 0x8080808080808080ULL)) ||
          (((eq2 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq2 & 0x8080808080808080ULL)))
        goto tail;
      pos += 8;
    }
  }

tail:

  while (pos < buffer_len) {
    if (buf[pos] == brk1 || buf[pos] == brk2)
      return (void *)(buf + pos);
    ++pos;
  }
  return NULL;
}

/**
 * Finds the first occurance of up to 8 characters.
 */
static inline void *fio_memchr4(const void *buffer, const size_t buffer_len,
                                const int brk1, const int brk2, const int brk3,
                                const int brk4) {
  if (!buffer || !buffer_len)
    return NULL;
  const uint8_t *buf = (const uint8_t *)buffer;
  size_t pos = 0;

  /* too short for this mess */
  if (buffer_len <= 15)
    goto tail;

  /* align memory */
  while ((uintptr_t)(buf + pos) & 3) {
    if (buf[pos] == brk1 || buf[pos] == brk2 || buf[pos] == brk3 ||
        buf[pos] == brk4)
      goto found;
    ++pos;
  }

  { /* 4 bytes */
    const uint32_t eq1 =
        ~(*(uint32_t *)(buf + pos) ^ 0x01010101ULL * (uint8_t)brk1);
    const uint32_t eq2 =
        ~(*(uint32_t *)(buf + pos) ^ 0x01010101ULL * (uint8_t)brk2);
    const uint32_t eq3 =
        ~(*(uint32_t *)(buf + pos) ^ 0x01010101ULL * (uint8_t)brk3);
    const uint32_t eq4 =
        ~(*(uint32_t *)(buf + pos) ^ 0x01010101ULL * (uint8_t)brk4);
    if ((((eq1 & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq1 & 0x80808080ULL)) ||
        (((eq2 & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq2 & 0x80808080ULL)) ||
        (((eq3 & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq3 & 0x80808080ULL)) ||
        (((eq4 & 0x7f7f7f7fULL) + 0x01010101ULL) & (eq4 & 0x80808080ULL)))
      goto tail;
    pos += 4;
  }

  { /* 8 byte steps*/
    const uint64_t wanted1 = 0x0101010101010101ULL * brk1;
    const uint64_t wanted2 = 0x0101010101010101ULL * brk2;
    const uint64_t wanted3 = 0x0101010101010101ULL * brk3;
    const uint64_t wanted4 = 0x0101010101010101ULL * brk4;
    while (pos + 8 <= buffer_len) {
      const uint64_t eq1 = ~(*(uint64_t *)(buf + pos) ^ wanted1);
      const uint64_t eq2 = ~(*(uint64_t *)(buf + pos) ^ wanted2);
      const uint64_t eq3 = ~(*(uint64_t *)(buf + pos) ^ wanted3);
      const uint64_t eq4 = ~(*(uint64_t *)(buf + pos) ^ wanted4);
      if ((((eq1 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq1 & 0x8080808080808080ULL)) ||
          (((eq2 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq2 & 0x8080808080808080ULL)) ||
          (((eq3 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq3 & 0x8080808080808080ULL)) ||
          (((eq4 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq4 & 0x8080808080808080ULL)))
        goto tail;
      pos += 8;
    }
  }

tail:

  while (pos < buffer_len) {
    if (buf[pos] == brk1 || buf[pos] == brk2 || buf[pos] == brk3 ||
        buf[pos] == brk4)
      goto found;
    ++pos;
  }
  return NULL;

found:
  return (void *)(buf + pos);
}

/**
 * Finds the first occurance of up to 8 characters.
 */
static inline void *fio_memchrx(const void *buffer, const size_t buffer_len,
                                const void *markers_,
                                const uint8_t markers_count) {
  if (!buffer || !buffer_len)
    return NULL;
  FIO_ASSERT(markers_count <= 8,
             "cannot search for more then 8 characters at a time");
  if (markers_count > 8)
    abort();
  const uint8_t *buf = (const uint8_t *)buffer;
  const uint8_t *brk = (const uint8_t *)markers_;
  uint64_t wanted[markers_count];
  uint64_t eq[markers_count];
  size_t pos = 0;

#if !ALLOW_UNALIGNED_MEMORY_ACCESS || (!__x86_64__ && !__aarch64__)
  /* too short for this mess */
  if (buffer_len <= 15)
    goto tail;
  /* align memory */
  while ((uintptr_t)(buf + pos) & 7) {
    switch (markers_count) {
    case 8:
      if (buf[pos] == brk[7])
        goto found;
      /* fallthrough */
    case 7:
      if (buf[pos] == brk[6])
        goto found;
      /* fallthrough */
    case 6:
      if (buf[pos] == brk[5])
        goto found;
      /* fallthrough */
    case 5:
      if (buf[pos] == brk[4])
        goto found;
      /* fallthrough */
    case 4:
      if (buf[pos] == brk[3])
        goto found;
      /* fallthrough */
    case 3:
      if (buf[pos] == brk[2])
        goto found;
      /* fallthrough */
    case 2:
      if (buf[pos] == brk[1])
        goto found;
      /* fallthrough */
    case 1:
      if (buf[pos] == brk[0])
        goto found;
      /* fallthrough */
    }
    ++pos;
  }
#endif

  switch (markers_count) {
  case 8:
    wanted[7] = 0x0101010101010101ULL * brk[7]; /* fallthrough */
  case 7:
    wanted[6] = 0x0101010101010101ULL * brk[6]; /* fallthrough */
  case 6:
    wanted[5] = 0x0101010101010101ULL * brk[5]; /* fallthrough */
  case 5:
    wanted[4] = 0x0101010101010101ULL * brk[4]; /* fallthrough */
  case 4:
    wanted[3] = 0x0101010101010101ULL * brk[3]; /* fallthrough */
  case 3:
    wanted[2] = 0x0101010101010101ULL * brk[2]; /* fallthrough */
  case 2:
    wanted[1] = 0x0101010101010101ULL * brk[1]; /* fallthrough */
  case 1:
    wanted[0] = 0x0101010101010101ULL * brk[0]; /* fallthrough */
  }
  while (pos + 8 <= buffer_len) {
    switch (markers_count) {
    case 8:
      eq[7] = ~(*(uint64_t *)(buf + pos) ^ wanted[7]); /* fallthrough */
    case 7:
      eq[6] = ~(*(uint64_t *)(buf + pos) ^ wanted[6]); /* fallthrough */
    case 6:
      eq[5] = ~(*(uint64_t *)(buf + pos) ^ wanted[5]); /* fallthrough */
    case 5:
      eq[4] = ~(*(uint64_t *)(buf + pos) ^ wanted[4]); /* fallthrough */
    case 4:
      eq[3] = ~(*(uint64_t *)(buf + pos) ^ wanted[3]); /* fallthrough */
    case 3:
      eq[2] = ~(*(uint64_t *)(buf + pos) ^ wanted[2]); /* fallthrough */
    case 2:
      eq[1] = ~(*(uint64_t *)(buf + pos) ^ wanted[1]); /* fallthrough */
    case 1:
      eq[0] = ~(*(uint64_t *)(buf + pos) ^ wanted[0]); /* fallthrough */
    }

    switch (markers_count) {
    case 8:
      if ((((eq[7] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[7] & 0x8080808080808080ULL)))
        goto tail;
    case 7:
      if ((((eq[6] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[6] & 0x8080808080808080ULL)))
        goto tail;
    case 6:
      if ((((eq[5] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[5] & 0x8080808080808080ULL)))
        goto tail;
    case 5:
      if ((((eq[4] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[4] & 0x8080808080808080ULL)))
        goto tail;
    case 4:
      if ((((eq[3] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[3] & 0x8080808080808080ULL)))
        goto tail;
    case 3:
      if ((((eq[2] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[2] & 0x8080808080808080ULL)))
        goto tail;
    case 2:
      if ((((eq[1] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[1] & 0x8080808080808080ULL)))
        goto tail;
    case 1:
      if ((((eq[0] & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
           (eq[0] & 0x8080808080808080ULL)))
        goto tail;
    }
    pos += 8;
  }

tail:

  while (pos < buffer_len) {
    switch (markers_count) {
    case 8:
      if (buf[pos] == brk[7])
        goto found;
      /* fallthrough */
    case 7:
      if (buf[pos] == brk[6])
        goto found;
      /* fallthrough */
    case 6:
      if (buf[pos] == brk[5])
        goto found;
      /* fallthrough */
    case 5:
      if (buf[pos] == brk[4])
        goto found;
      /* fallthrough */
    case 4:
      if (buf[pos] == brk[3])
        goto found;
      /* fallthrough */
    case 3:
      if (buf[pos] == brk[2])
        goto found;
      /* fallthrough */
    case 2:
      if (buf[pos] == brk[1])
        goto found;
      /* fallthrough */
    case 1:
      if (buf[pos] == brk[0])
        goto found;
      /* fallthrough */
    }
    ++pos;
  }
  return NULL;

found:
  return (void *)(buf + pos);
}

static inline void *seek4_strpbrk(const void *buffer, const int c,
                                  size_t buffer_len) {
  char brks[5] = {1, 55, 66, c, 0};
  return strpbrk((char *)buffer, brks);
  (void)buffer_len;
}

static inline void *seek2_loop(const void *buffer, const int c,
                               size_t buffer_len) {
  return fio_memchr2_naive(buffer, buffer_len, '\r', c);
}

static inline void *seek2_fio(const void *buffer, const int c,
                              size_t buffer_len) {
  return fio_memchr2(buffer, buffer_len, '\r', c);
}

static inline void *seek2x_fio(const void *buffer, const int c,
                               size_t buffer_len) {
  uint8_t brks[4] = {'\r', c};
  return fio_memchrx(buffer, buffer_len, brks, 4);
}

static inline void *seek4_loop(const void *buffer, const int c,
                               size_t buffer_len) {
  uint8_t brks[4] = {1, 55, 66, c};
  return fio_memchrx_naive(buffer, buffer_len, brks, 4);
}

static inline void *seek4_fio(const void *buffer, const int c,
                              size_t buffer_len) {
  return fio_memchr4(buffer, buffer_len, 1, 55, 66, c);
}

static inline void *seek4x_fio(const void *buffer, const int c,
                               size_t buffer_len) {
  uint8_t brks[4] = {1, 55, 66, c};
  return fio_memchrx(buffer, buffer_len, brks, 4);
}

static inline void *seek4_print_title(const void *buffer, const int c,
                                      size_t buffer_len) {
  (void)buffer;
  (void)buffer_len;
  (void)c;
  return 0;
}

#define RUNS 8
int main(int argc, char const **argv) {

  fio_cli_start(
      argc, argv, 1, 0,
      "This program tests the memchr speed against a custom implementation. "
      "It's meant to be used against defferent data to test how seeking "
      "performs in different circumstances.\n use: appname <filename>",
      "-c the char to be tested against (only the fist char in the string");
  if (fio_cli_unnamed_count()) {
    fio_cli_set_default("-f", fio_cli_unnamed(0));
  } else {
    fio_cli_set_default("-f", __FILE__);
  }
  fio_cli_set_default("-c", "\n");
  // fio_cli_set_default(name, value)
  clock_t start, end;
  char *pos;
  size_t stop;
  size_t count;

  fprintf(stderr, "Size of longest word found %lu\n",
          sizeof(unsigned long long));

  struct {
    void *(*func)(const void *buffer, const int brk, const size_t buffer_len);
    const char *name;
  } seek_funcs[] = {
      {.func = seek4_print_title, .name = "Testing single character seeking "},
      {.func = fio_memchr_naive, .name = "seek1 (basic loop)"},
      {.func = memchr, .name = "memchr (compiler)"},
      {.func = fio_memchr, .name = "fio_memchr (64 bit word at a time)"},
      {.func = seek4_print_title, .name = "Testing 2 character multi-seeking"},
      {.func = seek2_loop, .name = "seek 2 chars, naive loop"},
      {.func = seek2_fio, .name = "seek 2 chars, fio_memchr2"},
      {.func = seek2x_fio, .name = "seek 2 chars, fio_memchrx"},
      {.func = seek4_print_title, .name = "Testing 4 character multi-seeking"},
      {.func = seek4_loop, .name = "seek 4 chars, naive loop"},
      {.func = seek4_fio, .name = "seek 4 chars, fio_memchr4"},
      {.func = seek4x_fio, .name = "seek 4 chars, fio_memchrx"},
      {.func = seek4_strpbrk, .name = "seek 4 chars, seek4_strpbrk"},
      {.func = NULL, .name = NULL},
  };
  size_t func_pos = 0;

  uint8_t char2find = fio_cli_get("-c")[0];
  fio_str_s str = FIO_STR_INIT;
  fio_str_info_s data = fio_str_readfile(&str, fio_cli_get("-f"), 0, 0);
  if (!data.len) {
    fprintf(stderr, "ERROR: Couldn't open file %s\n", fio_cli_get("-f"));
    perror("reported error:");
    exit(-1);
  }
  fprintf(stderr, "Starting to test file with %lu bytes\n",
          (unsigned long)data.len);
  if (0) {
    /* dump to file, if you want to test readfile */
    fio_str_info_s d = data;
    int f = open("./dump_", O_WRONLY | O_CREAT);
    FIO_ASSERT(f != -1, "no file_");
    while (d.len) {
      size_t to_write = 1 << 22;
      if (to_write > d.len)
        to_write = d.len;
      write(f, d.data, to_write);
      d.len -= to_write;
      d.data += to_write;
    }
    close(f);
  }

  while (seek_funcs[func_pos].func) {
    if (seek_funcs[func_pos].func == seek4_print_title) {
      fprintf(stderr, "\n\n===========\n%s\n", seek_funcs[func_pos].name);
      func_pos++;
      continue;
    }
    fprintf(stderr, "\nTesting %s:\n  (", seek_funcs[func_pos].name);
    size_t avrg = 0;
    for (size_t i = 0; i < RUNS; i++) {
      pos = data.data;
      stop = data.len;

      count = 0;
      if (!pos || !stop)
        perror("WTF?!"), exit(errno);

      start = clock();
      while ((pos = seek_funcs[func_pos].func(pos, char2find, stop))) {
        stop = (data.len - 1) - (pos - data.data);
        ++count;
        ++pos;
      }
      end = clock();
      avrg += end - start;
      if (i)
        fprintf(stderr, " + ");
      fprintf(stderr, "%lu", end - start);
    }
    fprintf(stderr, ")/%d\n === finding %lu items in %zu bytes took %lfs\n",
            RUNS, count, data.len, (avrg / RUNS) / (1.0 * CLOCKS_PER_SEC));
    func_pos++;
  }
  fprintf(stderr, "\n");

  fio_str_destroy(&str);
  fio_cli_end();
}
