/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"

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
