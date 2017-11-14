/*
Copyright: Boaz Segev, 2017
License: MIT
*/

/**
This facil.io core library provides wrappers around complex and (or) dynamic
types, abstracting some complexity and making dynamic type related tasks easier.


The library offers a rudementry protection against cyclic references using the
`FIOBJ_NESTING_PROTECTION` flag (i.e., nesting an Array within itself)...
however, this isn't fully tested and the performance price is high.
*/
#include "fiobj_internal.h"
#include "fiobj_primitives.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* *****************************************************************************
Cyclic Protection helpers & API
***************************************************************************** */

static __thread fiobj_s *fiobj_cyclic_protection = NULL;
fiobj_s *fiobj_each_get_cyclic(void) { return fiobj_cyclic_protection; }

static inline fiobj_s *protected_pop_obj(fio_ls_s *queue, fio_ls_s *history) {
#if FIOBJ_NESTING_PROTECTION
  fiobj_cyclic_protection = NULL;

  fiobj_s *obj = fio_ls_pop(queue);
  if (!obj)
    return NULL;
  fiobj_s *child = OBJVTBL(obj)->unwrap(obj);
  if (!child)
    return obj;
  if (OBJVTBL(child)->count(child) == 0)
    return obj;
  fio_ls_s *pos = history->next;
  while (pos != history) {
    if (child == pos->obj) {
      fiobj_cyclic_protection = obj;
      return NULL;
    }
    pos = pos->next;
  }
  return obj;
#else
  return fio_ls_pop(queue);
  (void)history;
#endif
}

static inline void protected_push_obj(const fiobj_s *obj, fio_ls_s *history) {
#if FIOBJ_NESTING_PROTECTION
  fio_ls_push(history, OBJVTBL(obj)->unwrap(obj));
#else
  (void)obj;
  (void)history;
#endif
}

/* *****************************************************************************
Generic Object API
***************************************************************************** */

/** Returns a C string naming the objects dynamic type. */
const char *fiobj_type_name(const fiobj_s *obj) { return OBJVTBL(obj)->name; }

/**
 * Copy by reference(!) - increases an object's (and any nested object's)
 * reference count.
 *
 * Always returns the value passed along.
 *
 * Future implementations might provide `fiobj_dup2` providing a deep copy.
 *
 * We don't need this feature just yet, so I'm not working on it.
 */
fiobj_s *fiobj_dup(fiobj_s *obj) {
  OBJREF_ADD(obj);
  return obj;
}

static int fiobj_free_or_mark(fiobj_s *o, void *arg) {
  if (!o)
    return 0;
#if FIOBJ_NESTING_PROTECTION
  if (OBJ2HEAD(o)->ref == 0) /* maybe a nested returning... */
    return 0;
#elif DEBUG
  if (OBJ2HEAD(o)->ref == 0) {
    fprintf(stderr,
            "ERROR: attempting to free an object that isn't a fiobj or already "
            "freed (%p)\n",
            (void *)o);
    kill(0, SIGABRT);
  }
#endif

  if (OBJREF_REM(o))
    return 0;

  /* reference count is zero: free memory or add to queue */

  /* test for wrapped object (i.e., Hash Couplet) */
  fiobj_s *child = OBJVTBL(o)->unwrap(o);

  if (child != o) {
    if (!child || OBJREF_REM(child)) {
      OBJVTBL(o)->free(o);
      return 0;
    }
    OBJVTBL(o)->free(o);
    o = child;
  }

  if (OBJVTBL(o)->count(o)) {
    fio_ls_push(arg, o);
  } else
    OBJVTBL(o)->free(o);

  /* handle nesting / wrapping (i.e., Array, Hash, Couplets ) */
  return 0;
}

/**
 * Decreases an object's reference count, releasing memory and
 * resources.
 *
 * This function affects nested objects, meaning that when an Array or
 * a Hash object is passed along, it's children (nested objects) are
 * also freed.
 */
void fiobj_free(fiobj_s *o) {
#if DEBUG
  if (!o)
    return;
  if (OBJ2HEAD(o)->ref == 0) {
    fprintf(stderr,
            "ERROR: attempting to free an object that isn't a fiobj or already "
            "freed (%p)\n",
            (void *)o);
    kill(0, SIGABRT);
  }
#endif
  if (!o || OBJREF_REM(o))
    return;

  /* handle wrapping */
  {
    fiobj_s *child = OBJVTBL(o)->unwrap(o);
    if (child != o) {
      OBJVTBL(o)->free(o);
      if (OBJREF_REM(child))
        return;
      o = child;
    }
  }
  if (OBJVTBL(o)->count(o) == 0) {
    OBJVTBL(o)->free(o);
    return;
  }
  /* nested free */
  fio_ls_s queue = FIO_LS_INIT(queue);
  fio_ls_s history = FIO_LS_INIT(history);
  while (o) {
    /* the queue always contains valid enumerable objects that are unwrapped. */
    OBJVTBL(o)->each1(o, 0, fiobj_free_or_mark, &queue);
    fio_ls_push(&history, o);
    o = protected_pop_obj(&queue, &history);
  }
  /* clean up and free enumerables */
  while ((o = fio_ls_pop(&history)))
    OBJVTBL(o)->free(o);
  return;
}

/**
 * Attempts to return the object's current reference count.
 *
 * This is mostly for testing rather than normal library operations.
 */
uintptr_t fiobj_reference_count(const fiobj_s *o) { return OBJ2HEAD(o)->ref; }

/**
 * Tests if an object evaluates as TRUE.
 *
 * This is object type specific. For example, empty strings might evaluate as
 * FALSE, even though they aren't a boolean type.
 */
int fiobj_is_true(const fiobj_s *o) { return (o && OBJVTBL(o)->is_true(o)); }

/**
 * Returns an Object's numerical value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * Hashes and Arrays return their object count.
 *
 * IO and File objects return their underlying file descriptor.
 *
 * A type error results in 0.
 */
int64_t fiobj_obj2num(const fiobj_s *o) { return o ? OBJVTBL(o)->to_i(o) : 0; }

/**
 * Returns a Float's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * Hashes and Arrays return their object count.
 *
 * IO and File objects return their underlying file descriptor.
 *
 * A type error results in 0.
 */
double fiobj_obj2float(const fiobj_s *o) { return o ? OBJVTBL(o)->to_f(o) : 0; }

/**
 * Returns a C String (NUL terminated) using the `fio_cstr_s` data type.
 *
 * The Sting in binary safe and might contain NUL bytes in the middle as well as
 * a terminating NUL.
 *
 * If a Symbol, a Number or a Float are passed to the function, they
 * will be parsed as a *temporary*, thread-safe, String.
 *
 * Numbers will be represented in base 10 numerical data.
 *
 * A type error results in NULL (i.e. object isn't a String).
 */
fio_cstr_s fiobj_obj2cstr(const fiobj_s *o) {
  return o ? OBJVTBL(o)->to_str(o) : fiobj_noop_str(NULL);
}

/**
 * Single layer iteration using a callback for each nested fio object.
 *
 * Accepts any `fiobj_s *` type but only collections (Arrays and Hashes) are
 * processed. The container itself (the Array or the Hash) is **not** processed
 * (unlike `fiobj_each2`).
 *
 * The callback task function must accept an object and an opaque user pointer.
 *
 * Hash objects pass along a `FIOBJ_T_COUPLET` object, containing
 * references for both the key (Symbol) and the object (any object).
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the "stop" position, i.e., the number of items processed + the
 * starting point.
 */
size_t fiobj_each1(fiobj_s *o, size_t start_at,
                   int (*task)(fiobj_s *obj, void *arg), void *arg) {
  return o ? OBJVTBL(o)->each1(o, start_at, task, arg) : 0;
}

/* *****************************************************************************
Nested concern (each2, is_eq)
***************************************************************************** */

static int each2_add_to_queue(fiobj_s *obj, void *arg) {
  fio_ls_s *const queue = arg;
  fio_ls_unshift(queue, obj);
  return 0;
}

/**
 * Deep iteration using a callback for each fio object, including the parent.
 *
 *
 * Notice that when passing collections to the function, the collection itself
 * is sent to the callback followed by it's children (if any). This is true also
 * for nested collections (a nested Hash will be sent first, followed by the
 * nested Hash's children and then followed by the rest of it's siblings.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each2(fiobj_s *obj, int (*task)(fiobj_s *obj, void *arg),
                 void *arg) {
  if (!obj)
    goto single;
  size_t count = OBJVTBL(obj)->count(obj);
  if (!count)
    goto single;

  fio_ls_s queue = FIO_LS_INIT(queue), history = FIO_LS_INIT(history);
  while (obj || queue.next != &queue) {
    int i = task(obj, arg);
    if (i == -1)
      goto finish;
    if (obj && OBJVTBL(obj)->count(obj)) {
      protected_push_obj(obj, &history);
      OBJVTBL(obj)->each1(obj, 0, each2_add_to_queue, queue.next);
    }
    obj = protected_pop_obj(&queue, &history);
  }
finish:
  while (fio_ls_pop(&history))
    ;
  while (fio_ls_pop(&queue))
    ;
  return;
single:
  task(obj, arg);
  return;
}

/**
 * Deeply compare two objects. No hashing is involved.
 *
 * KNOWN ISSUES:
 *
 * * Cyclic nesting might cause this function to hang (much like `fiobj_each2`).
 *
 * * `FIOBJ_NESTING_PROTECTION` might be ignored when testing nested objects.
 *
 * * Hash order might be ignored when comapring Hashes, which means that equal
 *   Hases might behave differently during iteration.
 *
 */
int fiobj_iseq(const fiobj_s *self, const fiobj_s *other) {
  if (self == other)
    return 1;
  if (!self)
    return other->type == FIOBJ_T_NULL;
  if (!other)
    return self->type == FIOBJ_T_NULL;

  if (!OBJVTBL(self)->is_eq(self, other))
    return 0;
  if (!OBJVTBL(self)->count(self))
    return 1;

  uint8_t eq = 0;
  fio_ls_s self_queue = FIO_LS_INIT(self_queue);
  fio_ls_s self_history = FIO_LS_INIT(self_history);
  fio_ls_s other_queue = FIO_LS_INIT(other_queue);
  fio_ls_s other_history = FIO_LS_INIT(other_history);

  while (self) {
    protected_push_obj(self, &self_history);
    protected_push_obj(other, &other_history);
    OBJVTBL(self)->each1((fiobj_s *)self, 0, each2_add_to_queue,
                         self_queue.next);
    OBJVTBL(other)->each1((fiobj_s *)other, 0, each2_add_to_queue,
                          other_queue.next);
    while (self_queue.next != &self_queue || self) {
      self = protected_pop_obj(&self_queue, &self_history);
      other = protected_pop_obj(&other_queue, &other_history);
      if (self == other)
        continue;
      if (!self && other->type != FIOBJ_T_NULL)
        goto finish;
      if (!other && self->type != FIOBJ_T_NULL)
        goto finish;
      if (OBJVTBL(self)->count(self))
        break;
      if (!OBJVTBL(self)->is_eq(self, other))
        goto finish;
    }
    if (self && !OBJVTBL(self)->is_eq(self, other))
      goto finish;
  }
  eq = 1;

finish:
  while (fio_ls_pop(&self_history))
    ;
  while (fio_ls_pop(&self_queue))
    ;
  while (fio_ls_pop(&other_history))
    ;
  while (fio_ls_pop(&other_queue))
    ;
  return eq;
}

/* *****************************************************************************
Number and Float Helpers
***************************************************************************** */
static const char hex_notation[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10. `0x##` (or `x##`) and `0b##` (or
 * `b##`) are recognized as base 16 and base 2 (binary MSB first)
 * respectively.
 */
int64_t fio_atol(char **pstr) {
  char *str = *pstr;
  uint64_t result = 0;
  uint8_t invert = 0;
  while (str[0] == '0')
    str++;
  while (str[0] == '-') {
    invert ^= 1;
    str++;
  }
  while (str[0] == '0')
    str++;
  if (str[0] == 'b' || str[0] == 'B') {
    /* base 2 */
    str++;
    while (str[0] == '0' || str[0] == '1') {
      result = (result << 1) | (str[0] == '1');
      str++;
    }
  } else if (str[0] == 'x' || str[0] == 'X') {
    /* base 16 */
    uint8_t tmp;
    str++;
    while (1) {
      if (str[0] >= '0' && str[0] <= '9')
        tmp = str[0] - '0';
      else if (str[0] >= 'A' && str[0] <= 'F')
        tmp = str[0] - ('A' - 10);
      else if (str[0] >= 'a' && str[0] <= 'f')
        tmp = str[0] - ('a' - 10);
      else
        goto finish;
      result = (result << 4) | tmp;
      str++;
    }
  } else {
    /* base 10 */
    const char *end = str;
    while (end[0] >= '0' && end[0] <= '9' && (uintptr_t)(end - str) < 22)
      end++;
    if ((uintptr_t)(end - str) > 21) /* too large for a number */
      return 0;

    while (str < end) {
      result = (result * 10) + (str[0] - '0');
      str++;
    }
  }
finish:
  if (invert)
    result = 0 - result;
  *pstr = str;
  return (int64_t)result;
}

/** A helper function that convers between String data to a signed double. */
double fio_atof(char **pstr) { return strtold(*pstr, pstr); }

/* *****************************************************************************
String Helpers
***************************************************************************** */

/**
 * A helper function that convers between a signed int64_t to a string.
 *
 * No overflow guard is provided, make sure there's at least 66 bytes
 * available (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL
 * terminator).
 */
size_t fio_ltoa(char *dest, int64_t num, uint8_t base) {
  if (!num) {
    *(dest++) = '0';
    *(dest++) = 0;
    return 1;
  }

  size_t len = 0;

  if (base == 2) {
    uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
    uint8_t i = 0;    /* counting bits */

    while ((i < 64) && (n & 0x8000000000000000) == 0) {
      n = n << 1;
      i++;
    }
    /* make sure the Binary representation doesn't appear signed. */
    if (i) {
      dest[len++] = '0';
    }
    /* write to dest. */
    while (i < 64) {
      dest[len++] = ((n & 0x8000000000000000) ? '1' : '0');
      n = n << 1;
      i++;
    }
    dest[len] = 0;
    return len;

  } else if (base == 16) {
    uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
    uint8_t i = 0;    /* counting bytes */
    uint8_t tmp = 0;
    while (i < 8 && (n & 0xFF00000000000000) == 0) {
      n = n << 8;
      i++;
    }
    /* make sure the Hex representation doesn't appear signed. */
    if (i && (n & 0x8000000000000000)) {
      dest[len++] = '0';
      dest[len++] = '0';
    }
    /* write the damn thing */
    while (i < 8) {
      tmp = (n & 0xF000000000000000) >> 60;
      dest[len++] = hex_notation[tmp];
      tmp = (n & 0x0F00000000000000) >> 56;
      dest[len++] = hex_notation[tmp];
      i++;
      n = n << 8;
    }
    dest[len] = 0;
    return len;
  }

  /* fallback to base 10 */
  uint64_t rem = 0;
  uint64_t factor = 1;
  if (num < 0) {
    dest[len++] = '-';
    num = 0 - num;
  }

  while (num / factor)
    factor *= 10;

  while (factor > 1) {
    factor = factor / 10;
    rem = (rem * 10);
    dest[len++] = '0' + ((num / factor) - rem);
    rem += ((num / factor) - rem);
  }
  dest[len] = 0;
  return len;
}

/**
 * A helper function that convers between a double to a string.
 *
 * No overflow guard is provided, make sure there's at least 130 bytes
 * available (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL
 * terminator).
 */
size_t fio_ftoa(char *dest, double num, uint8_t base) {
  if (base == 2 || base == 16) {
    /* handle the binary / Hex representation the same as if it were an
     * int64_t
     */
    int64_t *i = (void *)&num;
    return fio_ltoa(dest, *i, base);
  }

  size_t written = sprintf(dest, "%g", num);
  uint8_t need_zero = 1;
  char *start = dest;
  while (*start) {
    if (*start == ',') // locale issues?
      *start = '.';
    if (*start == '.' || *start == 'e') {
      need_zero = 0;
      break;
    }
    start++;
  }
  if (need_zero) {
    dest[written++] = '.';
    dest[written++] = '0';
  }
  return written;
}
