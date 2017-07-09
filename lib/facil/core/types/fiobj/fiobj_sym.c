/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Symbol API
***************************************************************************** */

/** Creates a Symbol object. Use `fiobj_free`. */
fiobj_s *fiobj_sym_new(const char *str, size_t len) {
  return fiobj_alloc(FIOBJ_T_SYMBOL, len, (void *)str);
}

/** Creates a Symbol object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_symvprintf(const char *restrict format, va_list argv) {
  fiobj_s *sym = NULL;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len == 0)
    sym = fiobj_alloc(FIOBJ_T_SYMBOL, 0, (void *)"");
  if (len <= 0)
    return sym;
  sym = fiobj_alloc(FIOBJ_T_SYMBOL, len, NULL); /* adds 1 to len, for NUL */
  vsnprintf(((fio_sym_s *)(sym))->str, len + 1, format, argv);
  ((fio_sym_s *)(sym))->hash = fio_ht_hash(((fio_sym_s *)(sym))->str, len);
  return sym;
}
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_symprintf(const char *restrict format, ...) {
  va_list argv;
  va_start(argv, format);
  fiobj_s *sym = fiobj_symvprintf(format, argv);
  va_end(argv);
  return sym;
}

/** Returns 1 if both Symbols are equal and 0 if not. */
int fiobj_sym_iseql(fiobj_s *sym1, fiobj_s *sym2) {
  if (sym1->type != FIOBJ_T_SYMBOL || sym2->type != FIOBJ_T_SYMBOL)
    return 0;
  return (((fio_sym_s *)sym1)->hash == ((fio_sym_s *)sym2)->hash);
}
