/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

#include "fiobj_numbers.h"
#include "fiobject.h"

#define FIO_OVERRIDE_MALLOC 1
#include "fiobj_mem.h"

#include <assert.h>
#include <errno.h>
#include <math.h>

/* *****************************************************************************
Numbers Type
***************************************************************************** */

typedef struct {
  fiobj_object_header_s head;
  intptr_t i;
} fiobj_num_s;

typedef struct {
  fiobj_object_header_s head;
  double f;
} fiobj_float_s;

#define obj2num(o) ((fiobj_num_s *)FIOBJ2PTR(o))
#define obj2float(o) ((fiobj_float_s *)FIOBJ2PTR(o))

/* *****************************************************************************
Numbers VTable
***************************************************************************** */

static __thread char num_buffer[512];

static intptr_t fio_i2i(const FIOBJ o) { return obj2num(o)->i; }
static intptr_t fio_f2i(const FIOBJ o) {
  return (intptr_t)floorl(obj2float(o)->f);
}
static double fio_i2f(const FIOBJ o) { return (double)obj2num(o)->i; }
static double fio_f2f(const FIOBJ o) { return obj2float(o)->f; }

static size_t fio_itrue(const FIOBJ o) { return (obj2num(o)->i != 0); }
static size_t fio_ftrue(const FIOBJ o) { return (obj2float(o)->f != 0); }

static fio_str_info_s fio_i2str(const FIOBJ o) {
  return (fio_str_info_s){
      .data = num_buffer,
      .len = fio_ltoa(num_buffer, obj2num(o)->i, 10),
  };
}
static fio_str_info_s fio_f2str(const FIOBJ o) {
  if (isnan(obj2float(o)->f))
    return (fio_str_info_s){.data = "NaN", .len = 3};
  else if (isinf(obj2float(o)->f)) {
    if (obj2float(o)->f > 0)
      return (fio_str_info_s){.data = "Infinity", .len = 8};
    else
      return (fio_str_info_s){.data = "-Infinity", .len = 9};
  }
  return (fio_str_info_s){
      .data = num_buffer,
      .len = fio_ftoa(num_buffer, obj2float(o)->f, 10),
  };
}

static size_t fiobj_i_is_eq(const FIOBJ self, const FIOBJ other) {
  return obj2num(self)->i == obj2num(other)->i;
}
static size_t fiobj_f_is_eq(const FIOBJ self, const FIOBJ other) {
  return obj2float(self)->f == obj2float(other)->f;
}

void fiobject___simple_dealloc(FIOBJ o, void (*task)(FIOBJ, void *), void *arg);
uintptr_t fiobject___noop_count(FIOBJ o);

const fiobj_object_vtable_s FIOBJECT_VTABLE_NUMBER = {
    .class_name = "Number",
    .to_i = fio_i2i,
    .to_f = fio_i2f,
    .to_str = fio_i2str,
    .is_true = fio_itrue,
    .is_eq = fiobj_i_is_eq,
    .count = fiobject___noop_count,
    .dealloc = fiobject___simple_dealloc,
};

const fiobj_object_vtable_s FIOBJECT_VTABLE_FLOAT = {
    .class_name = "Float",
    .to_i = fio_f2i,
    .to_f = fio_f2f,
    .is_true = fio_ftrue,
    .to_str = fio_f2str,
    .is_eq = fiobj_f_is_eq,
    .count = fiobject___noop_count,
    .dealloc = fiobject___simple_dealloc,
};

/* *****************************************************************************
Number API
***************************************************************************** */

/** Creates a Number object. Remember to use `fiobj_free`. */
FIOBJ fiobj_num_new_bignum(intptr_t num) {
  fiobj_num_s *o = malloc(sizeof(*o));
  if (!o) {
    perror("ERROR: fiobj number couldn't allocate memory");
    exit(errno);
  }
  *o = (fiobj_num_s){
      .head =
          {
              .type = FIOBJ_T_NUMBER,
              .ref = 1,
          },
      .i = num,
  };
  return (FIOBJ)o;
}

/** Mutates a Big Number object's value. Effects every object's reference! */
// void fiobj_num_set(FIOBJ target, intptr_t num) {
//   assert(FIOBJ_TYPE_IS(target, FIOBJ_T_NUMBER) &&
//   FIOBJ_IS_ALLOCATED(target)); obj2num(target)->i = num;
// }

/** Creates a temporary Number object. This ignores `fiobj_free`. */
FIOBJ fiobj_num_tmp(intptr_t num) {
  static __thread fiobj_num_s ret;
  ret = (fiobj_num_s){
      .head = {.type = FIOBJ_T_NUMBER, .ref = ((~(uint32_t)0) >> 4)},
      .i = num,
  };
  return (FIOBJ)&ret;
}

/* *****************************************************************************
Float API
***************************************************************************** */

/** Creates a Float object. Remember to use `fiobj_free`.  */
FIOBJ fiobj_float_new(double num) {
  fiobj_float_s *o = malloc(sizeof(*o));
  if (!o) {
    perror("ERROR: fiobj float couldn't allocate memory");
    exit(errno);
  }
  *o = (fiobj_float_s){
      .head =
          {
              .type = FIOBJ_T_FLOAT,
              .ref = 1,
          },
      .f = num,
  };
  return (FIOBJ)o;
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(FIOBJ obj, double num) {
  assert(FIOBJ_TYPE_IS(obj, FIOBJ_T_FLOAT));
  obj2float(obj)->f = num;
}

/** Creates a temporary Number object. This ignores `fiobj_free`. */
FIOBJ fiobj_float_tmp(double num) {
  static __thread fiobj_float_s ret;
  ret = (fiobj_float_s){
      .head =
          {
              .type = FIOBJ_T_FLOAT,
              .ref = ((~(uint32_t)0) >> 4),
          },
      .f = num,
  };
  return (FIOBJ)&ret;
}

/* *****************************************************************************
Strings to Numbers
***************************************************************************** */

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10. Octal (`0###`), Hex (`0x##`/`x##`) and
 * binary (`0b##`/ `b##`) are recognized as well. For binary Most Significant
 * Bit must come first.
 *
 * The most significant differance between this function and `strtol` (aside of
 * API design), is the added support for binary representations.
 */
#pragma weak fio_atol
int64_t __attribute__((weak)) fio_atol(char **pstr) {
  /* No binary representation in strtol */
  char *str = *pstr;
  uint64_t result = 0;
  uint8_t invert = 0;
  while (str[0] == '-') {
    invert ^= 1;
    ++str;
  }
  if (str[0] == 'B' || str[0] == 'b' ||
      (str[0] == '0' && (str[1] == 'b' || str[1] == 'B'))) {
    /* base 2 */
    if (str[0] == '0')
      str++;
    str++;
    while (str[0] == '0' || str[0] == '1') {
      result = (result << 1) | (str[0] == '1');
      str++;
    }
  } else if (str[0] == 'x' || str[0] == 'X' ||
             (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
    /* base 16 */
    uint8_t tmp;
    if (str[0] == '0')
      str++;
    str++;
    for (;;) {
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
  } else if (str[0] == '0') {
    ++str;
    /* base 8 */
    const char *end = str;
    while (end[0] >= '0' && end[0] <= '7' && (uintptr_t)(end - str) < 22)
      end++;
    if ((uintptr_t)(end - str) > 21) /* TODO: fix too large for a number */
      return 0;

    while (str < end) {
      result = (result * 8) + (str[0] - '0');
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

/** A helper function that converts between String data to a signed double. */
#pragma weak fio_atof
double __attribute__((weak)) fio_atof(char **pstr) {
  return strtold(*pstr, pstr);
}

/* *****************************************************************************
Numbers to Strings
***************************************************************************** */

/**
 * A helper function that writes a signed int64_t to a string.
 *
 * No overflow guard is provided, make sure there's at least 68 bytes
 * available (for base 2).
 *
 * Offers special support for base 2 (binary), base 8 (octal), base 10 and base
 * 16 (hex). An unsupported base will silently default to base 10. Prefixes
 * are automatically added (i.e., "0x" for hex and "0b" for base 2).
 *
 * Returns the number of bytes actually written (excluding the NUL
 * terminator).
 */
#pragma weak fio_ltoa
size_t __attribute__((weak)) fio_ltoa(char *dest, int64_t num, uint8_t base) {
  const char notation[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  size_t len = 0;
  char buf[48]; /* we only need up to 20 for base 10, but base 3 needs 41... */

  if (!num)
    goto zero;

  switch (base) {
  case 1:
  case 2:
    /* Base 2 */
    {
      uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
      uint8_t i = 0;    /* counting bits */
      dest[len++] = '0';
      dest[len++] = 'b';

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
    }
  case 8:
    /* Base 8 */
    {
      uint64_t l = 0;
      if (num < 0) {
        dest[len++] = '-';
        num = 0 - num;
      }
      dest[len++] = '0';

      while (num) {
        buf[l++] = '0' + (num & 7);
        num = num >> 3;
      }
      while (l) {
        --l;
        dest[len++] = buf[l];
      }
      dest[len] = 0;
      return len;
    }

  case 16:
    /* Base 16 */
    {
      uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
      uint8_t i = 0;    /* counting bits */
      dest[len++] = '0';
      dest[len++] = 'x';
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
        uint8_t tmp = (n & 0xF000000000000000) >> 60;
        dest[len++] = notation[tmp];
        tmp = (n & 0x0F00000000000000) >> 56;
        dest[len++] = notation[tmp];
        i++;
        n = n << 8;
      }
      dest[len] = 0;
      return len;
    }
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 9:
    /* rare bases */
    if (num < 0) {
      dest[len++] = '-';
      num = 0 - num;
    }
    uint64_t l = 0;
    while (num) {
      uint64_t t = num / base;
      buf[l++] = '0' + (num - (t * base));
      num = t;
    }
    while (l) {
      --l;
      dest[len++] = buf[l];
    }
    dest[len] = 0;
    return len;

  default:
    break;
  }
  /* Base 10, the default base */

  if (num < 0) {
    dest[len++] = '-';
    num = 0 - num;
  }
  uint64_t l = 0;
  while (num) {
    uint64_t t = num / 10;
    buf[l++] = '0' + (num - (t * 10));
    num = t;
  }
  while (l) {
    --l;
    dest[len++] = buf[l];
  }
  dest[len] = 0;
  return len;

zero:
  switch (base) {
  case 1:
  case 2:
    dest[len++] = '0';
    dest[len++] = 'b';
  case 16:
    dest[len++] = '0';
    dest[len++] = 'x';
    dest[len++] = '0';
  }
  dest[len++] = '0';
  dest[len] = 0;
  return len;
}

/**
 * A helper function that converts between a double to a string.
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
#pragma weak fio_ftoa
size_t __attribute__((weak)) fio_ftoa(char *dest, double num, uint8_t base) {
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

/* *****************************************************************************
Numbers to Strings - Buffered
***************************************************************************** */

static __thread char num_buffer[512];

fio_str_info_s fio_ltocstr(long i) {
  return (fio_str_info_s){.data = num_buffer,
                          .len = fio_ltoa(num_buffer, i, 10)};
}
fio_str_info_s fio_ftocstr(double f) {
  return (fio_str_info_s){.data = num_buffer,
                          .len = fio_ftoa(num_buffer, f, 10)};
}

/* *****************************************************************************
Tests
***************************************************************************** */

#if DEBUG
void fiobj_test_numbers(void) {
#define NUMTEST_ASSERT(cond, ...)                                              \
  if (!(cond)) {                                                               \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  FIOBJ i = fiobj_num_new(8);
  fprintf(stderr, "=== Testing Numbers\n");
  fprintf(stderr, "* FIOBJ_NUMBER_SIGN_MASK == %p\n",
          (void *)FIOBJ_NUMBER_SIGN_MASK);
  fprintf(stderr, "* FIOBJ_NUMBER_SIGN_BIT == %p\n",
          (void *)FIOBJ_NUMBER_SIGN_BIT);
  fprintf(stderr, "* FIOBJ_NUMBER_SIGN_EXCLUDE_BIT == %p\n",
          (void *)FIOBJ_NUMBER_SIGN_EXCLUDE_BIT);
  NUMTEST_ASSERT(FIOBJ_TYPE_IS(i, FIOBJ_T_NUMBER),
                 "* FIOBJ_TYPE_IS failed to return true.");
  NUMTEST_ASSERT((FIOBJ_TYPE(i) == FIOBJ_T_NUMBER),
                 "* FIOBJ_TYPE failed to return type.");
  NUMTEST_ASSERT(!FIOBJ_TYPE_IS(i, FIOBJ_T_NULL),
                 "* FIOBJ_TYPE_IS failed to return false.");
  NUMTEST_ASSERT((i & FIOBJECT_NUMBER_FLAG),
                 "* Number 8 was dynamically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2num(i) == 8), "* Number 8 was not returned! %p\n",
                 (void *)i);
  fiobj_free(i);
  i = fiobj_num_new(-1);
  NUMTEST_ASSERT((i & FIOBJECT_NUMBER_FLAG),
                 "* Number -1 was dynamically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2num(i) == -1), "* Number -1 was not returned! %p\n",
                 (void *)i);
  fiobj_free(i);
  i = fiobj_num_new(INTPTR_MAX);
  NUMTEST_ASSERT((i & FIOBJECT_NUMBER_FLAG) == 0,
                 "* INTPTR_MAX was statically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2num(i) == INTPTR_MAX),
                 "* INTPTR_MAX was not returned! %p\n", (void *)i);
  NUMTEST_ASSERT(
      FIOBJ_TYPE_IS(i, FIOBJ_T_NUMBER),
      "* FIOBJ_TYPE_IS failed to return true for dynamic allocation.");
  NUMTEST_ASSERT((FIOBJ_TYPE(i) == FIOBJ_T_NUMBER),
                 "* FIOBJ_TYPE failed to return type for dynamic allocation.");
  fiobj_free(i);
  i = fiobj_num_new(INTPTR_MIN);
  NUMTEST_ASSERT((i & FIOBJECT_NUMBER_FLAG) == 0,
                 "* INTPTR_MIN was statically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2num(i) == INTPTR_MIN),
                 "* INTPTR_MIN was not returned! %p\n", (void *)i);
  fiobj_free(i);
  fprintf(stderr, "* passed.\n");
  fprintf(stderr, "=== Testing Floats\n");
  i = fiobj_float_new(1.0);
  NUMTEST_ASSERT(((i & FIOBJECT_NUMBER_FLAG) == 0),
                 "* float 1 was statically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2float(i) == 1.0),
                 "* Float 1.0 was not returned! %p\n", (void *)i);
  fiobj_free(i);
  i = fiobj_float_new(-1.0);
  NUMTEST_ASSERT((i & FIOBJECT_NUMBER_FLAG) == 0,
                 "* Float -1 was statically allocated?! %p\n", (void *)i);
  NUMTEST_ASSERT((fiobj_obj2float(i) == -1.0),
                 "* Float -1 was not returned! %p\n", (void *)i);
  fiobj_free(i);
  fprintf(stderr, "* passed.\n");
}
#endif
