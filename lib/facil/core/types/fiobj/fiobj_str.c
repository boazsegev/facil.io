/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#endif

#include "fiobj_internal.h"
#include "fiobj_str.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 2096
#endif

/* *****************************************************************************
String Type
***************************************************************************** */

typedef struct {
  struct fiobj_vtable_s *vtable;
  uint64_t capa;
  uint64_t len;
  uint8_t is_static;
  char *str;
} fiobj_str_s;

#define obj2str(o) ((fiobj_str_s *)(o))

/* *****************************************************************************
String VTable
***************************************************************************** */

static void fiobj_str_dealloc(fiobj_s *o) {
  free(obj2str(o)->str);
  fiobj_dealloc(o);
}

static int fiobj_str_is_eq(const fiobj_s *self, const fiobj_s *other) {
  if (!other || (FIOBJ_IS_STRING(other)) ||
      obj2str(self)->len != obj2str(other)->len)
    return 0;
  return self == other || obj2str(self)->str == obj2str(other)->str ||
         !memcmp(obj2str(self)->str, obj2str(other)->str, obj2str(self)->len);
}

static fio_cstr_s fio_str2str(const fiobj_s *o) {
  return (fio_cstr_s){.buffer = obj2str(o)->str, .len = obj2str(o)->len};
}
static int64_t fio_str2i(const fiobj_s *o) {
  char *s = obj2str(o)->str;
  return fio_atol(&s);
}
static double fio_str2f(const fiobj_s *o) {
  char *s = obj2str(o)->str;
  return fio_atof(&s);
}

static int fio_str2bool(const fiobj_s *o) { return obj2str(o)->len != 0; }

static struct fiobj_vtable_s FIOBJ_VTABLE_STRING = {
    .name = "String",
    .free = fiobj_str_dealloc,
    .to_i = fio_str2i,
    .to_f = fio_str2f,
    .to_str = fio_str2str,
    .is_eq = fiobj_str_is_eq,
    .is_true = fio_str2bool,
    .count = fiobj_noop_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_noop_each1,
};

const uintptr_t FIOBJ_T_STRING = (uintptr_t)(&FIOBJ_VTABLE_STRING);

static struct fiobj_vtable_s FIOBJ_VTABLE_STATIC_STRING = {
    .name = "StaticString",
    .free = fiobj_simple_dealloc,
    .to_i = fio_str2i,
    .to_f = fio_str2f,
    .to_str = fio_str2str,
    .is_eq = fiobj_str_is_eq,
    .is_true = fio_str2bool,
    .count = fiobj_noop_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_noop_each1,
};

const uintptr_t FIOBJ_T_STRING_STATIC =
    (uintptr_t)(&FIOBJ_VTABLE_STATIC_STRING);

/* *****************************************************************************
String API
***************************************************************************** */

static inline fiobj_s *fiobj_str_alloc(size_t len) {
  fiobj_s *o = fiobj_alloc(sizeof(fiobj_str_s) + len + 1);
  if (!o)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  *obj2str(o) = (fiobj_str_s){
      .vtable = &FIOBJ_VTABLE_STRING,
      .len = len,
      .capa = len + 1,
      .str = malloc(len + 1),
  };
  if (!obj2str(o)->str)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  obj2str(o)->str[len] = 0;
  return o;
}

/** Creates a String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_new(const char *str, size_t len) {
  fiobj_s *s = fiobj_str_alloc(len);
  if (str)
    memcpy(obj2str(s)->str, str, len);
  return s;
}

/** Creates a buffer String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_buf(size_t capa) {
  if (capa)
    capa = capa - 1;
  else
    capa = fiobj_memory_page_size();
  fiobj_s *s = fiobj_str_alloc(capa);
  fiobj_str_clear(s);
  return s;
}

/**
 * Creates a String object. Remember to use `fiobj_free`.
 *
 * The ownership of the memory indicated by `str` will now "move" to the object,
 * so `free` will be called for `str` by the `fiobj` library as needed.
 */
fiobj_s *fiobj_str_move(char *str, size_t len, size_t capacity) {
  fiobj_s *o = fiobj_alloc(sizeof(fiobj_str_s) + len + 1);
  if (!o)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  *obj2str(o) = (fiobj_str_s){
      .vtable = &FIOBJ_VTABLE_STRING,
      .len = len,
      .capa = (capacity < len ? len : capacity),
      .str = (char *)str,
  };
  return o;
}

/**
 * Creates a static String object from a static C string. Remember `fiobj_free`.
 *
 * This variation avoids allocating memory for an existing static String.
 *
 * The object still needs to be frees, but the string isn't copied and isn't
 * freed.
 *
 * NOTICE: static strings can't be written to.
 */
fiobj_s *fiobj_str_static(const char *str, size_t len) {
  fiobj_s *o = fiobj_alloc(sizeof(fiobj_str_s) + len + 1);
  if (!o)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  *obj2str(o) = (fiobj_str_s){
      .vtable = &FIOBJ_VTABLE_STATIC_STRING,
      .len = (len ? len : strlen(str)),
      .capa = 0,
      .str = (char *)str,
  };
  if (!obj2str(o)->str)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  return o;
}

/** Creates a copy from an existing String. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_copy(fiobj_s *src) {
  fio_cstr_s s = fiobj_obj2cstr(src);
  return fiobj_str_new(s.data, s.len);
}

/** Creates a String object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_strvprintf(const char *format, va_list argv) {
  fiobj_s *str = NULL;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len == 0)
    str = fiobj_str_new("", 0);
  if (len <= 0)
    return str;
  str = fiobj_str_new(NULL, len);
  vsnprintf(obj2str(str)->str, len + 1, format, argv);
  return str;
}
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_strprintf(const char *format, ...) {
  va_list argv;
  va_start(argv, format);
  fiobj_s *str = fiobj_strvprintf(format, argv);
  va_end(argv);
  return str;
}

/** Dumps the `filename` file's contents into a new String. If `limit == 0`,
 * than the data will be read until EOF.
 *
 * If the file can't be located, opened or read, or if `start_at` is beyond the
 * EOF position, NULL is returned.
 *
 * Remember to use `fiobj_free`.
 */
fiobj_s *fiobj_str_readfile(const char *filename, size_t start_at,
                            size_t limit) {
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
  /* POSIX implementations. */
  if (filename == NULL)
    return NULL;
  struct stat f_data;
  int file = -1;
  size_t file_path_len = strlen(filename);
  if (file_path_len == 0 || file_path_len > PATH_MAX)
    return NULL;

  char real_public_path[PATH_MAX + 1];
  real_public_path[PATH_MAX] = 0;

  if (filename[0] == '~' && getenv("HOME") && file_path_len <= PATH_MAX) {
    strcpy(real_public_path, getenv("HOME"));
    memcpy(real_public_path + strlen(real_public_path), filename + 1,
           file_path_len);
    filename = real_public_path;
  }

  if (stat(filename, &f_data) || f_data.st_size < 0)
    return NULL;

  if (limit <= 0 || (size_t)f_data.st_size < limit + start_at)
    limit = f_data.st_size - start_at;
  fiobj_s *str = fiobj_str_buf(limit + 1);
  if (!str)
    return NULL;
  file = open(filename, O_RDONLY);
  if (file < 0) {
    fiobj_str_dealloc(str);
    return NULL;
  }

  if (pread(file, obj2str(str)->str, limit, start_at) != (ssize_t)limit) {
    fiobj_str_dealloc(str);
    close(file);
    return NULL;
  }
  close(file);
  obj2str(str)->len = limit;
  obj2str(str)->str[limit] = 0;
  return str;
#else
  /* TODO: consider adding non POSIX implementations. */
  return NULL;
#endif
}

/** Confirms the requested capacity is available and allocates as required. */
size_t fiobj_str_capa_assert(fiobj_s *str, size_t size) {
  if (str->type != FIOBJ_T_STRING || obj2str(str)->capa == 0 ||
      obj2str(str)->capa >= size + 1)
    return obj2str(str)->capa;
  /* it's better to crash than live without memory... */
  obj2str(str)->str = realloc(obj2str(str)->str, size + 1);
  obj2str(str)->capa = size + 1;
  obj2str(str)->str[size] = 0;
  return obj2str(str)->capa;
}

/** Return's a String's capacity, if any. */
size_t fiobj_str_capa(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return 0;
  return obj2str(str)->capa;
}

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(fiobj_s *str, size_t size) {
  if (str->type != FIOBJ_T_STRING)
    return;
  fiobj_str_capa_assert(str, size);
  obj2str(str)->len = size;
  obj2str(str)->str[size] = 0;
  return;
}

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  obj2str(str)->capa = obj2str(str)->len + 1;
  obj2str(str)->str = realloc(obj2str(str)->str, obj2str(str)->capa);
  return;
}

/** Empties a String's data. */
void fiobj_str_clear(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  obj2str(str)->str[0] = 0;
  obj2str(str)->len = 0;
}

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write(fiobj_s *dest, const char *data, size_t len) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  fiobj_str_resize(dest, obj2str(dest)->len + len);
  if (len < 8) {
    size_t pos = obj2str(dest)->len;
    while (len) {
      len--;
      pos--;
      obj2str(dest)->str[pos] = data[len];
    }
  } else {
    memcpy(obj2str(dest)->str + obj2str(dest)->len - len, data, len);
  }
  // ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0; // see str_resize
  return obj2str(dest)->len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write2(fiobj_s *dest, const char *format, ...) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  va_list argv;
  va_start(argv, format);
  int len = vsnprintf(NULL, 0, format, argv);
  va_end(argv);
  if (len <= 0)
    return obj2str(dest)->len;
  fiobj_str_resize(dest, obj2str(dest)->len + len);
  va_start(argv, format);
  vsnprintf(obj2str(dest)->str + obj2str(dest)->len - len, len + 1, format,
            argv);
  va_end(argv);
  // ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0; // see str_resize
  return obj2str(dest)->len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_join(fiobj_s *dest, fiobj_s *obj) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  fio_cstr_s o = fiobj_obj2cstr(obj);
  if (o.len == 0)
    return obj2str(dest)->len;
  return fiobj_str_write(dest, o.data, o.len);
}
