/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_internal.h"

/* *****************************************************************************
Symbol Type
***************************************************************************** */

typedef struct {
  struct fiobj_vtable_s *vtable;
  uintptr_t hash;
  uint64_t len;
  char str[];
} fiobj_sym_s;

#define obj2sym(o) ((fiobj_sym_s *)(o))

/* *****************************************************************************
Symbol VTable
***************************************************************************** */

static int fiobj_sym_is_eq(const fiobj_s *self, const fiobj_s *other) {
  if (other->type != self->type)
    return 0;
  return obj2sym(self)->hash == obj2sym(other)->hash;
}

static fio_cstr_s fio_sym2str(const fiobj_s *o) {
  return (fio_cstr_s){.buffer = obj2sym(o)->str, .len = obj2sym(o)->len};
}
static int64_t fio_sym2i(const fiobj_s *o) {
  char *s = obj2sym(o)->str;
  return fio_atol(&s);
}
static double fio_sym2f(const fiobj_s *o) {
  char *s = obj2sym(o)->str;
  return fio_atof(&s);
}

static struct fiobj_vtable_s FIOBJ_VTABLE_SYMBOL = {
    .name = "Symbol",
    .free = fiobj_simple_dealloc,
    .to_i = fio_sym2i,
    .to_f = fio_sym2f,
    .to_str = fio_sym2str,
    .is_eq = fiobj_sym_is_eq,
    .is_true = fiobj_noop_true,
    .count = fiobj_noop_count,
    .unwrap = fiobj_noop_unwrap,
    .each1 = fiobj_noop_each1,
};

const uintptr_t FIOBJ_T_SYMBOL = (uintptr_t)(&FIOBJ_VTABLE_SYMBOL);

/* *****************************************************************************
Symbol API
***************************************************************************** */

static inline fiobj_s *fiobj_sym_alloc(size_t len) {
  fiobj_s *o = fiobj_alloc(sizeof(fiobj_sym_s) + len + 1);
  if (!o)
    perror("ERROR: fiobj symbol couldn't allocate memory"), exit(errno);
  *obj2sym(o) = (fiobj_sym_s){
      .vtable = &FIOBJ_VTABLE_SYMBOL, .len = len,
  };
  return o;
}

/** Creates a Symbol object. Use `fiobj_free`. */
fiobj_s *fiobj_sym_new(const char *str, size_t len) {
  fiobj_s *s = fiobj_sym_alloc(len);
  if (str)
    memcpy(obj2sym(s)->str, str, len);
  obj2sym(s)->str[len] = 0;
  obj2sym(s)->hash = (uintptr_t)fiobj_sym_hash(str, len);
  return s;
}

/** Finalizes a pre-allocated Symbol buffer to set it's final length and
 * calculate it's final hashing value. */
fiobj_s *fiobj_sym_reinitialize(fiobj_s *s, const size_t len) {
  if (obj2sym(s)->len < len)
    fprintf(stderr,
            "FATAL ERROR: facil.io Symbol object reinitialization error.\n"),
        exit(-1);
  obj2sym(s)->len = len;
  obj2sym(s)->str[len] = 0;
  obj2sym(s)->hash = (uintptr_t)fiobj_sym_hash(obj2sym(s)->str, len);
  return s;
}

/** Creates a Symbol object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_symvprintf(const char *format, va_list argv) {
  fiobj_s *sym = NULL;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len <= 0) {
    sym = fiobj_sym_alloc(0);
    obj2sym(sym)->hash = fiobj_sym_hash(NULL, 0);
    return sym;
  }
  sym = fiobj_sym_alloc(len); /* adds 1 to len, for NUL */
  vsnprintf(obj2sym(sym)->str, len + 1, format, argv);
  obj2sym(sym)->str[len] = 0; /* enforce NUL */
  obj2sym(sym)->hash = (uintptr_t)fiobj_sym_hash(obj2sym(sym)->str, len);
  return sym;
}
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_symprintf(const char *format, ...) {
  va_list argv;
  va_start(argv, format);
  fiobj_s *sym = fiobj_symvprintf(format, argv);
  va_end(argv);
  return sym;
}

/**
 * Returns a symbol's identifier.
 *
 * The unique identifier is calculated using SipHash and is equal for all Symbol
 * objects that were created using the same data.
 */
uintptr_t fiobj_sym_id(fiobj_s *sym) {
  if (sym->type != FIOBJ_T_SYMBOL)
    return 0;
  return obj2sym(sym)->hash;
}
