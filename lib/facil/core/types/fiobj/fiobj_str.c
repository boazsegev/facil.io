/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#endif

#ifdef _SC_PAGESIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#else
#define PAGE_SIZE 4096
#endif

#include "fiobject.h"

#include "fio_siphash.h"
#include "fiobj_numbers.h"
#include "fiobj_str.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#define FIO_OVERRIDE_MALLOC 1
#include "fio_mem.h"

#ifndef PATH_MAX
#define PATH_MAX PAGE_SIZE
#endif

/* *****************************************************************************
String Type
***************************************************************************** */

typedef struct {
  fiobj_object_header_s head;
  uint64_t hash;
  uint8_t is_small;
  uint8_t frozen;
  uint8_t slen;
  intptr_t len;
  uintptr_t capa;
  char *str;
} fiobj_str_s;

#define obj2str(o) ((fiobj_str_s *)(FIOBJ2PTR(o)))

#define STR_INTENAL_OFFSET ((uintptr_t)(&(((fiobj_str_s *)0)->slen) + 1))
#define STR_INTENAL_CAPA ((uintptr_t)(sizeof(fiobj_str_s) - STR_INTENAL_OFFSET))
#define STR_INTENAL_STR(o)                                                     \
  ((char *)((uintptr_t)FIOBJ2PTR(o) + STR_INTENAL_OFFSET))
#define STR_INTENAL_LEN(o) (((fiobj_str_s *)FIOBJ2PTR(o))->slen)

static inline char *fiobj_str_mem_addr(FIOBJ o) {
  if (obj2str(o)->is_small)
    return STR_INTENAL_STR(o);
  return obj2str(o)->str;
}
static inline size_t fiobj_str_getlen(FIOBJ o) {
  if (obj2str(o)->is_small)
    return obj2str(o)->slen;
  return obj2str(o)->len;
}
static inline size_t fiobj_str_getcapa(FIOBJ o) {
  if (obj2str(o)->is_small)
    return STR_INTENAL_CAPA;
  return obj2str(o)->capa;
}
static inline void fiobj_str_setlen(FIOBJ o, size_t len) {
  if (obj2str(o)->is_small) {
    obj2str(o)->slen = len;
    STR_INTENAL_STR(o)[len] = 0;
  } else {
    obj2str(o)->len = len;
    obj2str(o)->str[len] = 0;
    obj2str(o)->hash = 0;
  }
}
static inline fio_cstr_s fiobj_str_get_cstr(const FIOBJ o) {
  if (obj2str(o)->is_small)
    return (fio_cstr_s){.buffer = STR_INTENAL_STR(o),
                        .len = STR_INTENAL_LEN(o)};
  ;
  return (fio_cstr_s){.buffer = obj2str(o)->str, .len = obj2str(o)->len};
}

/* *****************************************************************************
String VTables
***************************************************************************** */

static fio_cstr_s fio_str2str(const FIOBJ o) { return fiobj_str_get_cstr(o); }

static void fiobj_str_dealloc(FIOBJ o, void (*task)(FIOBJ, void *), void *arg) {
  if (obj2str(o)->is_small == 0 && obj2str(o)->capa)
    fio_free(obj2str(o)->str);
  fio_free(FIOBJ2PTR(o));
  (void)task;
  (void)arg;
}

static size_t fiobj_str_is_eq(const FIOBJ self, const FIOBJ other) {
  fio_cstr_s o1 = fiobj_str_get_cstr(self);
  fio_cstr_s o2 = fiobj_str_get_cstr(other);
  return (o1.len == o2.len &&
          (o1.data == o2.data || !memcmp(o1.data, o2.data, o1.len)));
}

static intptr_t fio_str2i(const FIOBJ o) {
  char *pos = fiobj_str_mem_addr(o);
  return fio_atol(&pos);
}
static double fio_str2f(const FIOBJ o) {
  char *pos = fiobj_str_mem_addr(o);
  return fio_atof(&pos);
}

static size_t fio_str2bool(const FIOBJ o) { return fiobj_str_getlen(o) != 0; }

uintptr_t fiobject___noop_count(const FIOBJ o);

const fiobj_object_vtable_s FIOBJECT_VTABLE_STRING = {
    .class_name = "String",
    .dealloc = fiobj_str_dealloc,
    .to_i = fio_str2i,
    .to_f = fio_str2f,
    .to_str = fio_str2str,
    .is_eq = fiobj_str_is_eq,
    .is_true = fio_str2bool,
    .count = fiobject___noop_count,
};

/* *****************************************************************************
String API
***************************************************************************** */

/** Creates a buffer String object. Remember to use `fiobj_free`. */
FIOBJ fiobj_str_buf(size_t capa) {
  if (capa)
    capa = capa + 1;
  else
    capa = PAGE_SIZE;

  fiobj_str_s *s = fio_malloc(sizeof(*s));
  if (!s) {
    perror("ERROR: fiobj string couldn't allocate memory");
    exit(errno);
  }

  if (capa <= STR_INTENAL_CAPA) {
    *s = (fiobj_str_s){
        .head =
            {
                .ref = 1, .type = FIOBJ_T_STRING,
            },
        .is_small = 1,
        .slen = 0,
    };
  } else {
    *s = (fiobj_str_s){
        .head =
            {
                .ref = 1, .type = FIOBJ_T_STRING,
            },
        .len = 0,
        .capa = capa,
        .str = fio_malloc(capa),
    };
    if (!s->str) {
      perror("ERROR: fiobj string couldn't allocate buffer memory");
      exit(errno);
    }
  }
  return ((uintptr_t)s | FIOBJECT_STRING_FLAG);
}

/** Creates a String object. Remember to use `fiobj_free`. */
FIOBJ fiobj_str_new(const char *str, size_t len) {
  FIOBJ s = fiobj_str_buf(len);
  char *mem = fiobj_str_mem_addr(s);
  memcpy(mem, str, len);
  fiobj_str_setlen(s, len);
  return s;
}

/**
 * Creates a String object. Remember to use `fiobj_free`.
 *
 * The ownership of the memory indicated by `str` will now "move" to the
 * object, so `free` will be called by the `fiobj` library as needed.
 */
FIOBJ fiobj_str_move(char *str, size_t len, size_t capacity) {
  fiobj_str_s *s = fio_malloc(sizeof(*s));
  if (!s) {
    perror("ERROR: fiobj string couldn't allocate memory");
    exit(errno);
  }
  *s = (fiobj_str_s){
      .head =
          {
              .ref = 1, .type = FIOBJ_T_STRING,
          },
      .len = len,
      .capa = (capacity < len ? len : capacity),
      .str = str,
  };
  return ((uintptr_t)s | FIOBJECT_STRING_FLAG);
}

/**
 * Creates a static String object from a static C string. Remember
 * `fiobj_free`.
 *
 * This variation avoids allocating memory for an existing static String.
 *
 * The object still needs to be frees, but the string isn't copied and isn't
 * freed.
 *
 * NOTICE: static strings can't be written to.
 */
FIOBJ fiobj_str_static(const char *str, size_t len) {
#if !FIOBJ_DONT_COPY_SMALL_STATIC_STRINGS
  if (len < STR_INTENAL_CAPA)
    return fiobj_str_new(str, len);
#endif
  fiobj_str_s *s = fio_malloc(sizeof(*s));
  if (!s) {
    perror("ERROR: fiobj string couldn't allocate memory");
    exit(errno);
  }
  *s = (fiobj_str_s){
      .head =
          {
              .ref = 1, .type = FIOBJ_T_STRING,
          },
      .len = len,
      .capa = 0,
      .str = (char *)str,
  };
  return ((uintptr_t)s | FIOBJECT_STRING_FLAG);
}

/** Creates a String object using a printf like interface. */
__attribute__((format(printf, 1, 0))) FIOBJ fiobj_strvprintf(const char *format,
                                                             va_list argv) {
  FIOBJ str = 0;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len == 0)
    str = fiobj_str_new("", 0);
  if (len <= 0)
    return str;
  str = fiobj_str_buf(len);
  char *mem = FIOBJECT2VTBL(str)->to_str(str).data;
  vsnprintf(mem, len + 1, format, argv);
  fiobj_str_setlen(str, len);
  return str;
}
__attribute__((format(printf, 1, 2))) FIOBJ fiobj_strprintf(const char *format,
                                                            ...) {
  va_list argv;
  va_start(argv, format);
  FIOBJ str = fiobj_strvprintf(format, argv);
  va_end(argv);
  return str;
}

/**
 * Returns a thread-static temporary string. Avoid calling `fiobj_dup` or
 * `fiobj_free`.
 */
FIOBJ fiobj_str_tmp(void) {
  static __thread fiobj_str_s tmp = {
      .head =
          {
              .ref = ((~(uint32_t)0) >> 4), .type = FIOBJ_T_STRING,
          },
      .is_small = 1,
      .slen = 0,
  };
  tmp.len = 0;
  tmp.slen = 0;
  return ((uintptr_t)&tmp | FIOBJECT_STRING_FLAG);
}

/** Dumps the `filename` file's contents into a new String. If `limit == 0`,
 * than the data will be read until EOF.
 *
 * If the file can't be located, opened or read, or if `start_at` is beyond
 * the EOF position, NULL is returned.
 *
 * Remember to use `fiobj_free`.
 */
FIOBJ fiobj_str_readfile(const char *filename, intptr_t start_at,
                         intptr_t limit) {
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
  /* POSIX implementations. */
  if (filename == NULL)
    return FIOBJ_INVALID;
  struct stat f_data;
  int file = -1;
  size_t file_path_len = strlen(filename);
  if (file_path_len == 0 || file_path_len >= PATH_MAX)
    return FIOBJ_INVALID;

  char real_public_path[PATH_MAX];
  real_public_path[PATH_MAX - 1] = 0;

  if (filename[0] == '~' && getenv("HOME") && file_path_len < PATH_MAX) {
    strcpy(real_public_path, getenv("HOME"));
    memcpy(real_public_path + strlen(real_public_path), filename + 1,
           file_path_len);
    filename = real_public_path;
  }

  if (stat(filename, &f_data) || f_data.st_size <= 0)
    return FIOBJ_INVALID;

  if (start_at < 0)
    start_at = f_data.st_size + start_at;

  if (start_at < 0 || start_at >= f_data.st_size)
    return FIOBJ_INVALID;

  if (limit <= 0 || f_data.st_size < (limit + start_at))
    limit = f_data.st_size - start_at;
  FIOBJ str = fiobj_str_buf(limit + 1);
  if (!str)
    return FIOBJ_INVALID;
  file = open(filename, O_RDONLY);
  if (file < 0) {
    FIOBJECT2VTBL(str)->dealloc(str, NULL, NULL);
    return FIOBJ_INVALID;
  }
  if (pread(file, fiobj_str_mem_addr(str), limit, start_at) != (ssize_t)limit) {
    FIOBJECT2VTBL(str)->dealloc(str, NULL, NULL);
    close(file);
    return FIOBJ_INVALID;
  }
  close(file);
  fiobj_str_setlen(str, limit);
  return str;
#else
  /* TODO: consider adding non POSIX implementations. */
  return FIOBJ_INVALID;
#endif
}

/** Prevents the String object from being changed. */
void fiobj_str_freeze(FIOBJ str) {
  if (FIOBJ_TYPE_IS(str, FIOBJ_T_STRING))
    obj2str(str)->frozen = 1;
}

/** Confirms the requested capacity is available and allocates as required. */
size_t fiobj_str_capa_assert(FIOBJ str, size_t size) {

  assert(FIOBJ_TYPE_IS(str, FIOBJ_T_STRING));
  if (obj2str(str)->frozen)
    return 0;
  size += 1;
  if (obj2str(str)->is_small) {
    if (size <= STR_INTENAL_CAPA)
      return STR_INTENAL_CAPA;
    if (size >> 12)
      size = ((size >> 12) + 1) << 12;
    char *mem = fio_malloc(size);
    if (!mem) {
      perror("FATAL ERROR: Couldn't allocate larger String memory");
      exit(errno);
    }
    memcpy(mem, STR_INTENAL_STR(str), obj2str(str)->slen + 1);
    *obj2str(str) = (fiobj_str_s){
        .head =
            {
                .ref = obj2str(str)->head.ref, .type = FIOBJ_T_STRING,
            },
        .len = obj2str(str)->slen,
        .capa = size,
        .str = mem,
    };
    return obj2str(str)->capa;
  }
  if (obj2str(str)->capa >= size)
    return obj2str(str)->capa;

  /* large strings should increase memory by page size (assumes 4096 pages) */
  if (size >> 12)
    size = ((size >> 12) + 1) << 12;
  else if (size < (obj2str(str)->capa << 1))
    size = obj2str(str)->capa << 1; /* grow in steps */

  if (obj2str(str)->capa == 0) {
    /* a static string */
    char *mem = fio_malloc(size);
    if (!mem) {
      perror("FATAL ERROR: Couldn't allocate new String memory");
      exit(errno);
    }
    memcpy(mem, obj2str(str)->str, obj2str(str)->len + 1);
    obj2str(str)->str = mem;
  } else {
    /* it's better to crash than live without memory... */
    obj2str(str)->str =
        fio_realloc2(obj2str(str)->str, size, obj2str(str)->len + 1);
    if (!obj2str(str)->str) {
      perror("FATAL ERROR: Couldn't (re)allocate String memory");
      exit(errno);
    }
  }
  obj2str(str)->capa = size;
  return obj2str(str)->capa - 1;
}

/** Return's a String's capacity, if any. */
size_t fiobj_str_capa(FIOBJ str) {
  assert(FIOBJ_TYPE_IS(str, FIOBJ_T_STRING));
  if (obj2str(str)->frozen)
    return 0;
  return fiobj_str_getcapa(str) - 1;
}

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(FIOBJ str, size_t size) {
  assert(FIOBJ_TYPE_IS(str, FIOBJ_T_STRING));
  if (obj2str(str)->frozen)
    return;
  fiobj_str_capa_assert(str, size);
  fiobj_str_setlen(str, size);
  return;
}

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(FIOBJ str) {
  assert(FIOBJ_TYPE_IS(str, FIOBJ_T_STRING));
  if (obj2str(str)->frozen || obj2str(str)->is_small || obj2str(str)->capa == 0)
    return;
  obj2str(str)->capa = obj2str(str)->len + 1;
  obj2str(str)->str = fio_realloc(obj2str(str)->str, obj2str(str)->capa);
  return;
}

/** Empties a String's data. */
void fiobj_str_clear(FIOBJ str) {
  assert(FIOBJ_TYPE_IS(str, FIOBJ_T_STRING));
  if (obj2str(str)->frozen)
    return;
  fiobj_str_setlen(str, 0);
}

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write(FIOBJ dest, const char *data, size_t len) {
  assert(FIOBJ_TYPE_IS(dest, FIOBJ_T_STRING));
  if (obj2str(dest)->frozen)
    return 0;
  fiobj_str_resize(dest, fiobj_str_getlen(dest) + len);
  fio_cstr_s s = fiobj_str_get_cstr(dest);
  memcpy(s.data + s.len - len, data, len);
  return s.len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write2(FIOBJ dest, const char *format, ...) {
  assert(FIOBJ_TYPE_IS(dest, FIOBJ_T_STRING));
  if (obj2str(dest)->frozen)
    return 0;
  va_list argv;
  va_start(argv, format);
  int len = vsnprintf(NULL, 0, format, argv);
  va_end(argv);
  if (len <= 0)
    return obj2str(dest)->len;
  fiobj_str_resize(dest, fiobj_str_getlen(dest) + len);
  va_start(argv, format);
  fio_cstr_s s = fiobj_str_get_cstr(dest);
  vsnprintf(s.data + s.len - len, len + 1, format, argv);
  va_end(argv);
  // ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0; // see str_resize
  return s.len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_join(FIOBJ dest, FIOBJ obj) {
  assert(FIOBJ_TYPE_IS(dest, FIOBJ_T_STRING));
  if (obj2str(dest)->frozen)
    return 0;
  fio_cstr_s o = fiobj_obj2cstr(obj);
  if (o.len == 0)
    return obj2str(dest)->len;
  return fiobj_str_write(dest, o.data, o.len);
}

/**
 * Calculates a String's SipHash value for use as a HashMap key.
 */
uint64_t fiobj_str_hash(FIOBJ o) {
  assert(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING));
  // if (obj2str(o)->is_small) {
  //   return fio_siphash(STR_INTENAL_STR(o), STR_INTENAL_LEN(o));
  // } else
  if (obj2str(o)->hash) {
    return obj2str(o)->hash;
  }
  if (obj2str(o)->is_small) {
    obj2str(o)->hash = fio_siphash(STR_INTENAL_STR(o), STR_INTENAL_LEN(o));
  } else {
    obj2str(o)->hash = fio_siphash(obj2str(o)->str, obj2str(o)->len);
  }
  return obj2str(o)->hash;
}

/* *****************************************************************************
Tests
***************************************************************************** */

#if DEBUG
void fiobj_test_string(void) {
  fprintf(stderr, "=== Testing Strings\n");
  fprintf(stderr, "* Internal String Capacity %u with offset, %u\n",
          (unsigned int)STR_INTENAL_CAPA, (unsigned int)STR_INTENAL_OFFSET);
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
#define STR_EQ(o, str)                                                         \
  TEST_ASSERT((fiobj_str_getlen(o) == strlen(str) &&                           \
               !memcmp(fiobj_str_mem_addr(o), str, strlen(str))),              \
              "String not equal to " str)
  FIOBJ o = fiobj_str_new("Hello", 5);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING), "Small String isn't string!\n");
  TEST_ASSERT(obj2str(o)->is_small, "Hello isn't small\n");
  fiobj_str_write(o, " World", 6);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING),
              "Hello World String isn't string!\n");
  TEST_ASSERT(obj2str(o)->is_small, "Hello World isn't small\n");
  TEST_ASSERT(fiobj_obj2cstr(o).len == 11,
              "Invalid small string length (%u != 11)!\n",
              (unsigned int)fiobj_obj2cstr(o).len)
  fiobj_str_write(o, " World, you crazy longer sleep loving person :-)", 48);
  TEST_ASSERT(!obj2str(o)->is_small, "Crazier shouldn't be small\n");
  fiobj_free(o);

  o = fiobj_str_new(
      "hello my dear friend, I hope that your are well and happy.", 58);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING), "Long String isn't string!\n");
  TEST_ASSERT(!obj2str(o)->is_small,
              "Long String is small! (capa: %lu, len: %lu)\n", obj2str(o)->capa,
              obj2str(o)->len);
  TEST_ASSERT(fiobj_obj2cstr(o).len == 58,
              "Invalid long string length (%lu != 58)!\n",
              fiobj_obj2cstr(o).len)
  uint64_t hash = fiobj_str_hash(o);
  TEST_ASSERT(!obj2str(o)->frozen, "String forzen when only hashing!\n");
  fiobj_str_freeze(o);
  TEST_ASSERT(obj2str(o)->frozen, "String not forzen!\n");
  fiobj_str_write(o, " World", 6);
  TEST_ASSERT(hash == fiobj_str_hash(o),
              "String hash changed after hashing - not frozen?\n");
  TEST_ASSERT(fiobj_obj2cstr(o).len == 58,
              "String was edited after hashing - not frozen!\n (%lu): %s",
              (unsigned long)fiobj_obj2cstr(o).len, fiobj_obj2cstr(o).data);
  fiobj_free(o);

  o = fiobj_str_static("Hello", 5);
  TEST_ASSERT(obj2str(o)->is_small,
              "Small Static should be converted to dynamic.\n");
  fiobj_free(o);

  o = fiobj_str_static(
      "hello my dear friend, I hope that your are well and happy.", 58);
  fiobj_str_write(o, " World", 6);
  STR_EQ(o, "hello my dear friend, I hope that your are well and happy."
            " World");
  fiobj_free(o);

  o = fiobj_strprintf("%u", 42);
  TEST_ASSERT(fiobj_str_getlen(o) == 2, "fiobj_strprintf length error.\n");
  TEST_ASSERT(fiobj_obj2num(o), "fiobj_strprintf integer error.\n");
  TEST_ASSERT(!memcmp(fiobj_obj2cstr(o).data, "42", 2),
              "fiobj_strprintf string error.\n");
  fiobj_free(o);

  o = fiobj_str_buf(4);
  for (int i = 0; i < 16000; ++i) {
    fiobj_str_write(o, "a", 1);
  }
  TEST_ASSERT(obj2str(o)->len == 16000, "16K fiobj_str_write not 16K.\n");
  TEST_ASSERT(obj2str(o)->capa > 16001,
              "16K fiobj_str_write capa not enough.\n");
  fiobj_free(o);

  fprintf(stderr, "* passed.\n");
}
#endif
