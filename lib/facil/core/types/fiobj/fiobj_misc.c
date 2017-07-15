/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"

/* *****************************************************************************
NULL, TRUE, FALSE API
***************************************************************************** */

/** Retruns a NULL object. */
fiobj_s *fiobj_null(void) { return fiobj_alloc(FIOBJ_T_NULL, 0, NULL); }

/** Retruns a TRUE object. */
fiobj_s *fiobj_true(void) { return fiobj_alloc(FIOBJ_T_TRUE, 0, NULL); }

/** Retruns a FALSE object. */
fiobj_s *fiobj_false(void) { return fiobj_alloc(FIOBJ_T_FALSE, 0, NULL); }

/* *****************************************************************************
IO API      TODO: move to a different file when it grows.
***************************************************************************** */

/** Wrapps a file descriptor in an IO object. Use `fiobj_free` to close. */
fiobj_s *fio_io_wrap(intptr_t fd) { return fiobj_alloc(FIOBJ_T_IO, 0, &fd); }

/**
 * Return an IO's fd.
 *
 * A type error results in -1.
 */
intptr_t fiobj_io_fd(fiobj_s *obj) {
  if (obj->type != FIOBJ_T_IO)
    return -1;
  return ((fio_io_s *)obj)->fd;
}

/* *****************************************************************************
Number and Float Helpers
***************************************************************************** */
static char hex_notation[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10. `0x##` (or `x##`) and `0b##` (or
 * `b##`) are recognized as base 16 and base 2 (binary MSB first)
 * respectively.
 */
int64_t fio_atol(const char *str) {
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
  return (int64_t)result;
}

/** A helper function that convers between String data to a signed double. */
double fio_atof(const char *str) { return strtold(str, NULL); }

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

  int64_t i = (int64_t)trunc(num); /* grab the int data and handle first */
  size_t len = fio_ltoa(dest, i, 10);
  num = num - i;
  num = copysign(num, 1.0);
  if (num) {
    /* write decimal data */
    dest[len++] = '.';
    uint8_t limit = 7; /* post decimal point limit */
    while (limit-- && num) {
      num = num * 10;
      uint8_t tmp = (int64_t)trunc(num);
      dest[len++] = tmp + '0';
      num -= tmp;
    }
    /* remove excess zeros */
    while (dest[len - 1] == '0')
      len--;
  }
  dest[len] = 0;
  return len;
}
