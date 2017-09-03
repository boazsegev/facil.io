/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
String VTable
***************************************************************************** */

static void fiobj_str_dealloc(fiobj_s *o) {
  free(obj2str(o)->str);
  free(&OBJ2HEAD(o));
}

static void fiobj_str_dealloc_static(fiobj_s *o) { free(&OBJ2HEAD(o)); }

static int fiobj_str_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != self->type ||
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

static struct fiobj_vtable_s FIOBJ_VTABLE_STRING = {
    .free = fiobj_str_dealloc,
    .to_i = fio_str2i,
    .to_f = fio_str2f,
    .to_str = fio_str2str,
    .is_eq = fiobj_str_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

static struct fiobj_vtable_s FIOBJ_VTABLE_STATIC_STRING = {
    .free = fiobj_str_dealloc_static,
    .to_i = fio_str2i,
    .to_f = fio_str2f,
    .to_str = fio_str2str,
    .is_eq = fiobj_str_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

/* *****************************************************************************
String API
***************************************************************************** */

static inline fiobj_s *fiobj_str_alloc(size_t len) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_str_s));
  if (!head)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_STRING,
  };
  *obj2str(HEAD2OBJ(head)) = (fio_str_s){
      .type = FIOBJ_T_STRING, .len = len, .capa = len, .str = malloc(len + 1),
  };
  if (!obj2str(HEAD2OBJ(head))->str)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  obj2str(HEAD2OBJ(head))->str[len] = 0;
  return HEAD2OBJ(head);
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
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_str_s));
  if (!head)
    perror("ERROR: fiobj string couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_STATIC_STRING,
  };
  *obj2str(HEAD2OBJ(head)) = (fio_str_s){
      .type = FIOBJ_T_STRING,
      .len = (len ? len : strlen(str)),
      .capa = 0,
      .str = (char *)str,
      .is_static = 1,
  };
  return HEAD2OBJ(head);
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
  vsnprintf(((fio_str_s *)(str))->str, len + 1, format, argv);
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
void fiobj_str_capa_assert(fiobj_s *str, size_t size) {
  if (str->type != FIOBJ_T_STRING || ((fio_str_s *)str)->capa == 0 ||
      ((fio_str_s *)str)->capa >= size + 1)
    return;
  /* it's better to crash than live without memory... */
  ((fio_str_s *)str)->str = realloc(((fio_str_s *)str)->str, size + 1);
  ((fio_str_s *)str)->capa = size + 1;
  ((fio_str_s *)str)->str[size] = 0;
  return;
}

/** Return's a String's capacity, if any. */
size_t fiobj_str_capa(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return 0;
  return ((fio_str_s *)str)->capa;
}

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(fiobj_s *str, size_t size) {
  if (str->type != FIOBJ_T_STRING)
    return;
  fiobj_str_capa_assert(str, size);
  ((fio_str_s *)str)->len = size;
  ((fio_str_s *)str)->str[size] = 0;
  return;
}

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  ((fio_str_s *)str)->capa = ((fio_str_s *)str)->len + 1;
  ((fio_str_s *)str)->str =
      realloc(((fio_str_s *)str)->str, ((fio_str_s *)str)->capa);
  return;
}

/** Empties a String's data. */
void fiobj_str_clear(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  ((fio_str_s *)str)->str[0] = 0;
  ((fio_str_s *)str)->len = 0;
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
    memcpy(((fio_str_s *)dest)->str + ((fio_str_s *)dest)->len - len, data,
           len);
  }
  // ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0; // see str_resize
  return ((fio_str_s *)dest)->len;
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
    return ((fio_str_s *)dest)->len;
  fiobj_str_resize(dest, ((fio_str_s *)dest)->len + len);
  va_start(argv, format);
  vsnprintf(((fio_str_s *)(dest))->str + ((fio_str_s *)dest)->len - len,
            len + 1, format, argv);
  va_end(argv);
  // ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0; // see str_resize
  return ((fio_str_s *)dest)->len;
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
