/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_str.h"

#include "fiobj_internal.h"

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

static int fiobj_str_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || (FIOBJ_IS_STRING(other)) ||
      obj2str(self)->len != obj2str(other)->len)
    return 0;
  return self == other || obj2str(self)->str == obj2str(other)->str ||
         !memcmp(obj2str(self)->str, obj2str(other)->str, obj2str(self)->len);
}

static fio_cstr_s fio_str2str(fiobj_s *o) {
  return (fio_cstr_s){.buffer = obj2str(o)->str, .len = obj2str(o)->len};
}
static int64_t fio_str2i(fiobj_s *o) {
  char *s = obj2str(o)->str;
  return fio_atol(&s);
}
static double fio_str2f(fiobj_s *o) {
  char *s = obj2str(o)->str;
  return fio_atof(&s);
}

static int fio_str2bool(fiobj_s *o) { return obj2str(o)->len != 0; }

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
      .capa = len,
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
