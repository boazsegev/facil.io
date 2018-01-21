/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/
#include "fiobj_json.h"

#include "fio_ary.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* *****************************************************************************
JSON API
***************************************************************************** */
/* maximum allowed depth values max out at 32 */
#if !defined(JSON_MAX_DEPTH) || JSON_MAX_DEPTH > 32
#undef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH 32
#endif
/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
size_t fiobj_json2obj(FIOBJ *pobj, const void *data, size_t len);
/* Formats an object into a JSON string. Remember to `fiobj_free`. */
FIOBJ fiobj_obj2json(FIOBJ, uint8_t);
/* *****************************************************************************
JSON Parser Type, Callacks && API (FIOBJ specifics are later on)
***************************************************************************** */

/** The JSON parser type. Memory must be initialized to 0 before first uses. */
typedef struct {
  /** in dictionary flag. */
  uint32_t dict;
  /** level of nesting. */
  uint8_t depth;
  /** in dictionary waiting for key. */
  uint8_t key;
} json_parser_s;

/** a NULL object was detected */
static void on_null(json_parser_s *p);
/** a TRUE object was detected */
static void on_true(json_parser_s *p);
/** a FALSE object was detected */
static void on_false(json_parser_s *p);
/** a Numberl was detected (long long). */
static void on_number(json_parser_s *p, long long i);
/** a Float was detected (double). */
static void on_float(json_parser_s *p, double f);
/** a String was detected (int / float). update `pos` to point at ending */
static void on_string(json_parser_s *p, void *start, size_t length);
/** a dictionary object was detected */
static void on_start_object(json_parser_s *p);
/** a dictionary object closure detected */
static void on_end_object(json_parser_s *p);
/** an array object was detected */
static void on_start_array(json_parser_s *p);
/** an array closure was detected */
static void on_end_array(json_parser_s *p);
/** the JSON parsing is complete */
static void on_json(json_parser_s *p);
/** the JSON parsing is complete */
static void on_error(json_parser_s *p);

/**
 * Stream parsing of JSON data using a persistent parser.
 *
 * Returns the number of bytes consumed (0 being a valid value).
 *
 * Unconsumed data should be resent to the parser once more data is available.
 */
static size_t __attribute__((unused))
fio_json_parse(json_parser_s *parser, const char *buffer, size_t length);

static size_t __attribute__((unused))
fio_json_unescape_str(void *dest, const char *source, size_t length);

/* *****************************************************************************
JSON maps (arrays used to map data to simplify `if` statements)
***************************************************************************** */

/*
Marks as object seperators any of the following:

* White Space: [0x09, 0x0A, 0x0D, 0x20]
* Comma ("," / 0x2C)
* NOT Colon (":" / 0x3A)
* == [0x09, 0x0A, 0x0D, 0x20, 0x2C]
The rest belong to objects,
*/
static const uint8_t JSON_SEPERATOR[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*
Marks a numeral valid char (it's a permisive list):
['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'e', 'E', '+', '-', 'x', 'b',
'.']
*/
static const uint8_t JSON_NUMERAL[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static const uint8_t is_hex[] = {
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  1,  2,  3,  4, 5, 6, 7, 8, 9, 10, 0,  0,
    0,  0,  0,  0, 0, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 11, 12, 13,
    14, 15, 16, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0};

/* *****************************************************************************
JSON String Helper - Seeking to the end of a string
***************************************************************************** */

/**
 * finds the first occurance of either '"' or '\\'.
 */
static inline int seek2marker(uint8_t **buffer,
                              register const uint8_t *const limit) {
  if (**buffer == '"' || **buffer == '\\')
    return 1;

#if !__x86_64__ && !__aarch64__
  /* too short for this mess */
  if ((uintptr_t)limit <= 8 + ((uintptr_t)*buffer & (~(uintptr_t)7)))
    goto finish;

  /* align memory */
  {
    const uint8_t *alignment =
        (uint8_t *)(((uintptr_t)(*buffer) & (~(uintptr_t)7)) + 8);
    if (limit >= alignment) {
      while (*buffer < alignment) {
        if (**buffer == '"' || **buffer == '\\')
          return 1;
        *buffer += 1;
      }
    }
  }
  const uint8_t *limit64 = (uint8_t *)((uintptr_t)limit & (~(uintptr_t)7));
#else
  const uint8_t *limit64 = (uint8_t *)limit - 7;
#endif
  uint64_t wanted1 = 0x0101010101010101ULL * '"';
  uint64_t wanted2 = 0x0101010101010101ULL * '\\';
  for (; *buffer < limit64; *buffer += 8) {
    const uint64_t eq1 = ~((*((uint64_t *)*buffer)) ^ wanted1);
    const uint64_t t1 =
        ((eq1 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
        (eq1 & 0x8080808080808080ULL);
    const uint64_t eq2 = ~((*((uint64_t *)*buffer)) ^ wanted2);
    const uint64_t t2 =
        ((eq2 & 0x7f7f7f7f7f7f7f7fULL) + 0x0101010101010101ULL) &
        (eq2 & 0x8080808080808080ULL);
    if ((t1 | t2)) {
      break;
    }
  }
#if !__x86_64__ && !__aarch64__
finish:
#endif
  while (*buffer < limit) {
    if (**buffer == '"' || **buffer == '\\')
      return 1;
    (*buffer)++;
  }
  return 0;
}

static inline int seek2eos(uint8_t **buffer,
                           register const uint8_t *const limit) {
  while (*buffer < limit) {
    if (seek2marker(buffer, limit) && **buffer == '"')
      return 1;
    (*buffer) += 2; /* consume both the escape '\\' and the escape code. */
  }
  return 0;
}

/* *****************************************************************************
JSON Consumption (astract parsing)
***************************************************************************** */

/**
 * Returns the number of bytes consumed. Stops as close as possible to the end
 * of the buffer or once an object parsing was completed.
 */
static size_t __attribute__((unused))
fio_json_parse(json_parser_s *parser, const char *buffer, size_t length) {
  if (!length || !buffer)
    return 0;
  uint8_t *pos = (uint8_t *)buffer;
  const uint8_t *limit = pos + length;
  do {
    while (pos < limit && JSON_SEPERATOR[*pos])
      ++pos;
    if (pos == limit)
      goto stop;
    switch (*pos) {
    case '"': {
      uint8_t *tmp = pos + 1;
      if (seek2eos(&tmp, limit) == 0)
        goto stop;
      if (parser->key) {
        uint8_t *key = tmp + 1;
        while (key < limit && JSON_SEPERATOR[*key])
          ++key;
        if (key >= limit)
          goto stop;
        if (*key != ':')
          goto error;
        ++pos;
        on_string(parser, pos, (uintptr_t)(tmp - pos));
        pos = key + 1;
        parser->key = 0;
        continue /* skip tests */;
      } else {
        ++pos;
        on_string(parser, pos, (uintptr_t)(tmp - pos));
        pos = tmp + 1;
      }
      break;
    }
    case '{':
      if (parser->key) {
#if DEBUG
        fprintf(stderr, "ERROR: JSON key can't be a Hash.\n");
#endif
        goto error;
      }
      ++parser->depth;
      if (parser->depth >= JSON_MAX_DEPTH)
        goto error;
      parser->dict = (parser->dict << 1) | 1;
      ++pos;
      on_start_object(parser);
      break;
    case '}':
      if ((parser->dict & 1) == 0) {
#if DEBUG
        fprintf(stderr, "ERROR: JSON dictionary closure error.\n");
#endif
        goto error;
      }
      if (!parser->key) {
#if DEBUG
        fprintf(stderr, "ERROR: JSON dictionary closure missing key value.\n");
        goto error;
#endif
        on_null(parser); /* append NULL and recuperate from error. */
      }
      --parser->depth;
      ++pos;
      parser->dict = (parser->dict >> 1);
      on_end_object(parser);
      break;
    case '[':
      if (parser->key) {
#if DEBUG
        fprintf(stderr, "ERROR: JSON key can't be an array.\n");
#endif
        goto error;
      }
      ++parser->depth;
      if (parser->depth >= JSON_MAX_DEPTH)
        goto error;
      ++pos;
      parser->dict = (parser->dict << 1);
      on_start_array(parser);
      break;
    case ']':
      if ((parser->dict & 1))
        goto error;
      --parser->depth;
      ++pos;
      parser->dict = (parser->dict >> 1);
      on_end_array(parser);
      break;
    case 't':
      if (pos + 3 >= limit)
        goto stop;
      if (pos[1] == 'r' && pos[2] == 'u' && pos[3] == 'e')
        on_true(parser);
      else
        goto error;
      pos += 4;
      break;
    case 'N': /* overflow */
    case 'n':
      if (pos + 2 <= limit && (pos[1] | 32) == 'a' && (pos[2] | 32) == 'n')
        goto numeral;
      if (pos + 3 >= limit)
        goto stop;
      if (pos[1] == 'u' && pos[2] == 'l' && pos[3] == 'l')
        on_null(parser);
      else
        goto error;
      pos += 4;
      break;
    case 'f':
      if (pos + 4 >= limit)
        goto stop;
      if (pos + 4 < limit && pos[1] == 'a' && pos[2] == 'l' && pos[3] == 's' &&
          pos[4] == 'e')
        on_false(parser);
      else
        goto error;
      pos += 5;
      break;
    case '-': /* overflow */
    case '0': /* overflow */
    case '1': /* overflow */
    case '2': /* overflow */
    case '3': /* overflow */
    case '4': /* overflow */
    case '5': /* overflow */
    case '6': /* overflow */
    case '7': /* overflow */
    case '8': /* overflow */
    case '9': /* overflow */
    case '.': /* overflow */
    case 'e': /* overflow */
    case 'E': /* overflow */
    case 'x': /* overflow */
    case 'i': /* overflow */
    case 'I': /* overflow */
    numeral : {
      uint8_t *tmp = NULL;
      long long i = strtoll((char *)pos, (char **)&tmp, 0);
      if (tmp > limit)
        goto stop;
      if (!tmp || JSON_NUMERAL[*tmp]) {
        double f = strtod((char *)pos, (char **)&tmp);
        if (tmp > limit)
          goto stop;
        if (!tmp || JSON_NUMERAL[*tmp])
          goto error;
        on_float(parser, f);
        pos = tmp;
      } else {
        on_number(parser, i);
        pos = tmp;
      }
      break;
    }
    case '#': /* Ruby style comment */
    {
      uint8_t *tmp = memchr(pos, '\n', (uintptr_t)(limit - pos));
      if (!tmp)
        goto stop;
      pos = tmp + 1;
      continue /* skip tests */;
    }
    case '/': /* C style / Javascript style comment */
      if (pos[1] == '*') {
        if (pos + 4 > limit)
          goto stop;
        uint8_t *tmp = pos + 3; /* avoid this: /*/
        do {
          tmp = memchr(tmp, '/', (uintptr_t)(limit - tmp));
        } while (tmp && tmp[-1] != '*');
        if (!tmp)
          goto stop;
        pos = tmp + 1;
      } else if (pos[1] == '/') {
        uint8_t *tmp = memchr(pos, '\n', (uintptr_t)(limit - pos));
        if (!tmp)
          goto stop;
        pos = tmp + 1;
      } else
        goto error;
      continue /* skip tests */;
    default:
      goto error;
    }
    if (parser->depth == 0) {
      on_json(parser);
      goto stop;
    }
    parser->key = (parser->dict & 1);
  } while (pos < limit);
stop:
  return (size_t)((uintptr_t)pos - (uintptr_t)buffer);
error:
  on_error(parser);
  return 0;
}

/* *****************************************************************************
JSON Unescape String
***************************************************************************** */

#ifdef __cplusplus
#define REGISTER
#else
#define REGISTER register
#endif

/* converts a uint32_t to UTF-8 and returns the number of bytes written */
static inline int utf8_from_u32(uint8_t *dest, uint32_t u) {
  if (u <= 127) {
    *dest = u;
    return 1;
  } else if (u <= 2047) {
    *(dest++) = 192 | (u >> 6);
    *(dest++) = 128 | (u & 63);
    return 2;
  } else if (u <= 65535) {
    *(dest++) = 224 | (u >> 12);
    *(dest++) = 128 | ((u >> 6) & 63);
    *(dest++) = 128 | (u & 63);
    return 3;
  }
  *(dest++) = 240 | ((u >> 18) & 7);
  *(dest++) = 128 | ((u >> 12) & 63);
  *(dest++) = 128 | ((u >> 6) & 63);
  *(dest++) = 128 | (u & 63);
  return 4;
}

static void __attribute__((unused))
fio_json_unescape_str_internal(uint8_t **dest, const uint8_t **src) {
  ++(*src);
  switch (**src) {
  case 'b':
    **dest = '\b';
    ++(*src);
    ++(*dest);
    return; /* from switch */
  case 'f':
    **dest = '\f';
    ++(*src);
    ++(*dest);
    return; /* from switch */
  case 'n':
    **dest = '\n';
    ++(*src);
    ++(*dest);
    return; /* from switch */
  case 'r':
    **dest = '\r';
    ++(*src);
    ++(*dest);
    return; /* from switch */
  case 't':
    **dest = '\t';
    ++(*src);
    ++(*dest);
    return;   /* from switch */
  case 'u': { /* test for octal notation */
    if (is_hex[(*src)[1]] && is_hex[(*src)[2]] && is_hex[(*src)[3]] &&
        is_hex[(*src)[4]]) {
      uint32_t t =
          ((((is_hex[(*src)[1]] - 1) << 4) | (is_hex[(*src)[2]] - 1)) << 8) |
          (((is_hex[(*src)[3]] - 1) << 4) | (is_hex[(*src)[4]] - 1));
      if ((*src)[5] == '\\' && (*src)[6] == 'u' && is_hex[(*src)[7]] &&
          is_hex[(*src)[8]] && is_hex[(*src)[9]] && is_hex[(*src)[10]]) {
        /* Serrogate Pair */
        t = (t & 0x03FF) << 10;
        t |= ((((((is_hex[(*src)[7]] - 1) << 4) | (is_hex[(*src)[8]] - 1))
                << 8) |
               (((is_hex[(*src)[9]] - 1) << 4) | (is_hex[(*src)[10]] - 1))) &
              0x03FF);
        t += 0x10000;
        (*src) += 6;
      }
      *dest += utf8_from_u32(*dest, t);
      *src += 5;
      return;
    } else
      goto invalid_escape;
  }
  case 'x': { /* test for hex notation */
    if (is_hex[(*src)[1]] && is_hex[(*src)[2]]) {
      **dest = ((is_hex[(*src)[1]] - 1) << 4) | (is_hex[(*src)[2]] - 1);
      ++(*dest);
      (*src) += 3;
      return;
    } else
      goto invalid_escape;
  }
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7': { /* test for octal notation */
    if ((*src)[1] >= '0' && (*src)[1] <= '7') {
      **dest = (((*src)[0] - '0') << 3) | ((*src)[1] - '0');
      ++(*dest);
      (*src) += 2;
      break; /* from switch */
    } else
      goto invalid_escape;
  }
  case '"':
  case '\\':
  case '/':
  /* fallthrough */
  default:
  invalid_escape:
    **dest = **src;
    ++(*src);
    ++(*dest);
  }
}

static size_t __attribute__((unused))
fio_json_unescape_str(void *dest, const char *source, size_t length) {
  const uint8_t *reader = (uint8_t *)source;
  const uint8_t *stop = reader + length;
  uint8_t *writer = (uint8_t *)dest;
  /* copy in chuncks unless we hit an escape marker */
  while (reader < stop) {
#if !__x86_64__ && !__aarch64__
    /* we can't leverage unaligned memory access, so we read the buffer twice */
    uint8_t *tmp = memchr(reader, '\\', (size_t)(stop - reader));
    if (!tmp) {
      memcpy(writer, reader, (size_t)(stop - reader));
      writer += (size_t)(stop - reader);
      goto finish;
    }
    memcpy(writer, reader, (size_t)(tmp - reader));
    writer += (size_t)(tmp - reader);
    reader = tmp;
#else
    const uint8_t *limit64 = (uint8_t *)stop - 7;
    uint64_t wanted1 = 0x0101010101010101ULL * '\\';
    while (reader < limit64) {
      const uint64_t eq1 = ~((*((uint64_t *)reader)) ^ wanted1);
      const uint64_t t0 = (eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
      const uint64_t t1 = (eq1 & 0x8080808080808080llu);
      if ((t0 & t1)) {
        break;
      }
      *((uint64_t *)writer) = *((uint64_t *)reader);
      reader += 8;
      writer += 8;
    }
    while (reader < stop) {
      if (*reader == '\\')
        break;
      *writer = *reader;
      ++reader;
      ++writer;
    }
    if (reader >= stop)
      goto finish;
#endif
    fio_json_unescape_str_internal(&writer, &reader);
  }
finish:
  return (size_t)((uintptr_t)writer - (uintptr_t)dest);
}

#undef REGISTER

/* *****************************************************************************
FIOBJ Parser
***************************************************************************** */

#include "fio_ary.h"
typedef struct {
  json_parser_s p;
  FIOBJ key;
  FIOBJ top;
  fio_ary_s stack;
  uint8_t is_hash;
} fiobj_json_parser_s;

/* *****************************************************************************
FIOBJ Callacks
***************************************************************************** */

static inline void fiobj_json_add2parser(fiobj_json_parser_s *p, FIOBJ o) {
  if (p->top) {
    if (p->is_hash) {
      if (p->key) {
        fiobj_hash_set(p->top, p->key, o);
        fiobj_free(p->key);
        p->key = FIOBJ_INVALID;
      } else {
        p->key = o;
      }
    } else {
      fiobj_ary_push(p->top, o);
    }
  } else {
    p->top = o;
  }
}

/** a NULL object was detected */
static void on_null(json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p, fiobj_null());
}
/** a TRUE object was detected */
static void on_true(json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p, fiobj_true());
}
/** a FALSE object was detected */
static void on_false(json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p, fiobj_false());
}
/** a Numberl was detected (long long). */
static void on_number(json_parser_s *p, long long i) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p, fiobj_num_new(i));
}
/** a Float was detected (double). */
static void on_float(json_parser_s *p, double f) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p, fiobj_float_new(f));
}
/** a String was detected (int / float). update `pos` to point at ending */
static void on_string(json_parser_s *p, void *start, size_t length) {
  FIOBJ str = fiobj_str_buf(length);
  fiobj_str_resize(
      str, fio_json_unescape_str(fiobj_obj2cstr(str).data, start, length));
  fiobj_json_add2parser((fiobj_json_parser_s *)p, str);
}
/** a dictionary object was detected */
static void on_start_object(json_parser_s *p) {
  FIOBJ hash = fiobj_hash_new();
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  fiobj_json_add2parser(pr, hash);
  fio_ary_push(&pr->stack, (void *)pr->top);
  pr->top = hash;
  pr->is_hash = 1;
}
/** a dictionary object closure detected */
static void on_end_object(json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  if (pr->key) {
    fprintf(stderr, "WARNING: (JSON parsing) malformed JSON, "
                    "ignoring dangling Hash key.\n");
    fiobj_free(pr->key);
    pr->key = FIOBJ_INVALID;
  }
  pr->top = (FIOBJ)fio_ary_pop(&pr->stack);
  pr->is_hash = FIOBJ_TYPE_IS(pr->top, FIOBJ_T_HASH);
}
/** an array object was detected */
static void on_start_array(json_parser_s *p) {
  FIOBJ ary = fiobj_ary_new2(4);
  fiobj_json_add2parser((fiobj_json_parser_s *)p, ary);
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  fio_ary_push(&pr->stack, (void *)pr->top);
  pr->top = ary;
  pr->is_hash = 0;
}
/** an array closure was detected */
static void on_end_array(json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  pr->top = (FIOBJ)fio_ary_pop(&pr->stack);
  pr->is_hash = FIOBJ_TYPE_IS(pr->top, FIOBJ_T_HASH);
}
/** the JSON parsing is complete */
static void on_json(json_parser_s *p) {
  // fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  // FIO_ARY_FOR(&pr->stack, pos) { fiobj_free((FIOBJ)pos.obj); }
  // fio_ary_free(&pr->stack);
  (void)p; /* nothing special... right? */
}
/** the JSON parsing is complete */
static void on_error(json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
#if DEBUG
  fprintf(stderr, "ERROR: JSON on error called.\n");
#endif
  fiobj_free((FIOBJ)fio_ary_index(&pr->stack, 0));
  fiobj_free(pr->key);
  fio_ary_free(&pr->stack);
  pr->stack = FIO_ARY_INIT;
  *pr = (fiobj_json_parser_s){.top = FIOBJ_INVALID};
}

/* *****************************************************************************
JSON formatting
***************************************************************************** */

/** Writes a JSON friendly version of the src String */
static void write_safe_str(FIOBJ dest, const FIOBJ str) {
  fio_cstr_s s = fiobj_obj2cstr(str);
  fio_cstr_s t = fiobj_obj2cstr(dest);
  t.data[t.len] = '"';
  t.len++;
  fiobj_str_resize(dest, t.len);
  t = fiobj_obj2cstr(dest);
  const uint8_t *restrict src = (const uint8_t *)s.data;
  size_t len = s.len;
  uint64_t end = t.len;
  /* make sure we have some room */
  size_t added = 0;
  size_t capa = fiobj_str_capa(dest);
  if (capa <= end + s.len + 64) {
    if (0) {
      capa = (((capa >> 12) + 1) << 12) - 1;
      capa = fiobj_str_capa_assert(dest, capa);
    } else {
      capa = fiobj_str_capa_assert(dest, (end + s.len + 64));
    }
    fio_cstr_s tmp = fiobj_obj2cstr(dest);
    t = tmp;
  }
  while (len) {
    char *restrict writer = (char *)t.data;
    while (len &&
           (src[0] > 32 && src[0] != '"' && src[0] != '\\' && src[0] != '/')) {
      len--;
      writer[end++] = *(src++);
    }
    if (!len)
      break;
    switch (src[0]) {
    case '\b':
      writer[end++] = '\\';
      writer[end++] = 'b';
      added++;
      break; /* from switch */
    case '\f':
      writer[end++] = '\\';
      writer[end++] = 'f';
      added++;
      break; /* from switch */
    case '\n':
      writer[end++] = '\\';
      writer[end++] = 'n';
      added++;
      break; /* from switch */
    case '\r':
      writer[end++] = '\\';
      writer[end++] = 'r';
      added++;
      break; /* from switch */
    case '\t':
      writer[end++] = '\\';
      writer[end++] = 't';
      added++;
      break; /* from switch */
    case '"':
    case '\\':
    case '/':
      writer[end++] = '\\';
      writer[end++] = src[0];
      added++;
      break; /* from switch */
    default:
      if (src[0] <= 31) {
        /* MUST escape all control values less than 32 */
        writer[end++] = '\\';
        writer[end++] = 'u';
        writer[end++] = '0';
        writer[end++] = '0';
        writer[end++] = hex_chars[src[0] >> 4];
        writer[end++] = hex_chars[src[0] & 15];
        added += 4;
      } else
        writer[end++] = src[0];
      break; /* from switch */
    }
    src++;
    len--;
    if (added >= 48 && capa <= end + len + 64) {
      if (0) {
        capa = (((capa >> 12) + 1) << 12) - 1;
        capa = fiobj_str_capa_assert(dest, capa);
      } else {
        capa = fiobj_str_capa_assert(dest, (end + len + 64));
      }
      t = fiobj_obj2cstr(dest);
      added = 0;
    }
  }
  t.data[end++] = '"';
  fiobj_str_resize(dest, end);
}

typedef struct {
  FIOBJ dest;
  FIOBJ parent;
  fio_ary_s *stack;
  uintptr_t count;
  uint8_t pretty;
} obj2json_data_s;

static int fiobj_fiobj_obj2json_task(FIOBJ o, void *data_) {
  obj2json_data_s *data = data_;
  uint8_t add_seperator = 1;
  if (fiobj_hash_key_in_loop()) {
    write_safe_str(data->dest, fiobj_hash_key_in_loop());
    fiobj_str_write(data->dest, ":", 1);
  }
  switch (FIOBJ_TYPE(o)) {
  case FIOBJ_T_NUMBER:
  case FIOBJ_T_NULL:
  case FIOBJ_T_TRUE:
  case FIOBJ_T_FALSE:
  case FIOBJ_T_FLOAT:
    fiobj_str_join(data->dest, o);
    --data->count;
    break;

  case FIOBJ_T_DATA:
  case FIOBJ_T_UNKNOWN:
  case FIOBJ_T_STRING:
    write_safe_str(data->dest, o);
    --data->count;
    break;

  case FIOBJ_T_ARRAY:
    --data->count;
    fio_ary_push(data->stack, (void *)data->parent);
    fio_ary_push(data->stack, (void *)data->count);
    data->parent = o;
    data->count = fiobj_ary_count(o);
    fiobj_str_write(data->dest, "[", 1);
    add_seperator = 0;
    break;

  case FIOBJ_T_HASH:
    --data->count;
    fio_ary_push(data->stack, (void *)data->parent);
    fio_ary_push(data->stack, (void *)data->count);
    data->parent = o;
    data->count = fiobj_hash_count(o);
    fiobj_str_write(data->dest, "{", 1);
    add_seperator = 0;
    break;
  }
  if (data->pretty) {
    fiobj_str_capa_assert(data->dest, fiobj_obj2cstr(data->dest).len +
                                          (fio_ary_count(data->stack) * 5));
    while (!data->count && data->parent) {
      if (FIOBJ_TYPE_IS(data->parent, FIOBJ_T_HASH)) {
        fiobj_str_write(data->dest, "}", 1);
      } else {
        fiobj_str_write(data->dest, "]", 1);
      }
      add_seperator = 1;
      data->count = (uintptr_t)fio_ary_pop(data->stack);
      data->parent = (FIOBJ)fio_ary_pop(data->stack);
    }

    if (add_seperator && data->parent) {
      fiobj_str_write(data->dest, ",\n", 2);
      uintptr_t indent = fio_ary_count(data->stack) - 1;
      fiobj_str_capa_assert(data->dest,
                            fiobj_obj2cstr(data->dest).len + (indent * 2));
      fio_cstr_s buf = fiobj_obj2cstr(data->dest);
      while (indent--) {
        buf.bytes[buf.len++] = ' ';
        buf.bytes[buf.len++] = ' ';
      }
      fiobj_str_resize(data->dest, buf.len);
    }
  } else {
    fiobj_str_capa_assert(data->dest, fiobj_obj2cstr(data->dest).len +
                                          (fio_ary_count(data->stack) << 1));
    while (!data->count && data->parent) {
      if (FIOBJ_TYPE_IS(data->parent, FIOBJ_T_HASH)) {
        fiobj_str_write(data->dest, "}", 1);
      } else {
        fiobj_str_write(data->dest, "]", 1);
      }
      add_seperator = 1;
      data->count = (uintptr_t)fio_ary_pop(data->stack);
      data->parent = (FIOBJ)fio_ary_pop(data->stack);
    }

    if (add_seperator && data->parent) {
      fiobj_str_write(data->dest, ",", 1);
    }
  }

  return 0;
}

/* *****************************************************************************
FIOBJ API
***************************************************************************** */

/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
size_t fiobj_json2obj(FIOBJ *pobj, const void *data, size_t len) {
  fiobj_json_parser_s p = {.top = FIOBJ_INVALID};
  size_t consumed = fio_json_parse(&p.p, data, len);
  if (!consumed || p.p.depth) {
    fiobj_free((FIOBJ)fio_ary_index(&p.stack, 0));
    p.top = FIOBJ_INVALID;
  }
  fio_ary_free(&p.stack);
  fiobj_free(p.key);
  *pobj = p.top;
  return consumed;
}

/**
 * Formats an object into a JSON string, appending the JSON string to an
 * existing String. Remember to `fiobj_free`.
 */
FIOBJ fiobj_obj2json2(FIOBJ dest, FIOBJ o, uint8_t pretty) {
  assert(dest && FIOBJ_TYPE_IS(dest, FIOBJ_T_STRING));
  fio_ary_s stack;
  obj2json_data_s data = {
      .dest = dest, .stack = &stack, .pretty = pretty, .count = 1,
  };
  if (!o || !FIOBJ_IS_ALLOCATED(o) || !FIOBJECT2VTBL(o)->each) {
    fiobj_fiobj_obj2json_task(o, &data);
    return dest;
  }
  fio_ary_new(&stack, 0);
  fiobj_each2(o, fiobj_fiobj_obj2json_task, &data);
  fio_ary_free(&stack);
  return dest;
}

/* Formats an object into a JSON string. Remember to `fiobj_free`. */
FIOBJ fiobj_obj2json(FIOBJ obj, uint8_t pretty) {
  return fiobj_obj2json2(fiobj_str_buf(0), obj, pretty);
}

/* *****************************************************************************
Test
***************************************************************************** */

#if DEBUG
void fiobj_test_json(void) {
  fprintf(stderr, "=== Testing JSON parser (simple test)\n");
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "Testing failed.\n");                                      \
    exit(-1);                                                                  \
  }
  char json_str[] = "{\"array\":[1,2,3,\"boom\"],\"my\":{\"secret\":42},"
                    "\"true\":true,\"false\":false,\"null\":null,\"float\":-2."
                    "2,\"string\":\"I \\\"wrote\\\" this.\"}";
  char json_str2[] =
      "[\n    \"JSON Test Pattern pass1\",\n    {\"object with 1 "
      "member\":[\"array with 1 element\"]},\n    {},\n    [],\n    -42,\n    "
      "true,\n    false,\n    null,\n    {\n        \"integer\": 1234567890,\n "
      "       \"real\": -9876.543210,\n        \"e\": 0.123456789e-12,\n       "
      " \"E\": 1.234567890E+34,\n        \"\":  23456789012E66,\n        "
      "\"zero\": 0,\n        \"one\": 1,\n        \"space\": \" \",\n        "
      "\"quote\": \"\\\"\",\n        \"backslash\": \"\\\\\",\n        "
      "\"controls\": \"\\b\\f\\n\\r\\t\",\n        \"slash\": \"/ & \\/\",\n   "
      "     \"alpha\": \"abcdefghijklmnopqrstuvwyz\",\n        \"ALPHA\": "
      "\"ABCDEFGHIJKLMNOPQRSTUVWYZ\",\n        \"digit\": \"0123456789\",\n    "
      "    \"0123456789\": \"digit\",\n        \"special\": "
      "\"`1~!@#$%^&*()_+-={':[,]}|;.</>?\",\n        \"hex\": "
      "\"\\u0123\\u4567\\u89AB\\uCDEF\\uabcd\\uef4A\",\n        \"true\": "
      "true,\n        \"false\": false,\n        \"null\": null,\n        "
      "\"array\":[  ],\n        \"object\":{  },\n        \"address\": \"50 "
      "St. James Street\",\n        \"url\": \"http://www.JSON.org/\",\n       "
      " \"comment\": \"// /* <!-- --\",\n        \"# -- --> */\": \" \",\n     "
      "   \" s p a c e d \" :[1,2 , 3\n\n,\n\n4 , 5        ,          6        "
      "   ,7        ],\"compact\":[1,2,3,4,5,6,7],\n        \"jsontext\": "
      "\"{\\\"object with 1 member\\\":[\\\"array with 1 element\\\"]}\",\n    "
      "    \"quotes\": \"&#34; \\u0022 %22 0x22 034 &#x22;\",\n        "
      "\"\\/"
      "\\\\\\\"\\uCAFE\\uBABE\\uAB98\\uFCDE\\ubcda\\uef4A\\b\\f\\n\\r\\t`1~!@#$"
      "%^&*()_+-=[]{}|;:',./<>?\"\n: \"A key can be any string\"\n    },\n    "
      "0.5 "
      ",98.6\n,\n99.44\n,\n\n1066,\n1e1,\n0.1e1,\n1e-1,\n1e00,2e+00,2e-00\n,"
      "\"rosebud\"]";

  FIOBJ o = 0;
  TEST_ASSERT(fiobj_json2obj(&o, "1", 2) == 1,
              "JSON number parsing failed to run!\n");
  TEST_ASSERT(o, "JSON (single) object missing!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_NUMBER),
              "JSON (single) not a number!\n");
  TEST_ASSERT(fiobj_obj2num(o) == 1, "JSON (single) not == 1!\n");
  fiobj_free(o);

  TEST_ASSERT(fiobj_json2obj(&o, "2.0", 5) == 3,
              "JSON float parsing failed to run!\n");
  TEST_ASSERT(o, "JSON (float) object missing!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_FLOAT), "JSON (float) not a float!\n");
  TEST_ASSERT(fiobj_obj2float(o) == 2, "JSON (float) not == 2!\n");
  fiobj_free(o);

  TEST_ASSERT(fiobj_json2obj(&o, json_str, sizeof(json_str)) ==
                  (sizeof(json_str) - 1),
              "JSON parsing failed to run!\n");
  TEST_ASSERT(o, "JSON object missing!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_HASH),
              "JSON root not a dictionary (not a hash)!\n");
  FIOBJ tmp = fiobj_hash_get2(o, fio_siphash("array", 5));
  TEST_ASSERT(FIOBJ_TYPE_IS(tmp, FIOBJ_T_ARRAY),
              "JSON 'array' not an Array!\n");
  TEST_ASSERT(fiobj_obj2num(fiobj_ary_index(tmp, 0)) == 1,
              "JSON 'array' index 0 error!\n");
  TEST_ASSERT(fiobj_obj2num(fiobj_ary_index(tmp, 1)) == 2,
              "JSON 'array' index 1 error!\n");
  TEST_ASSERT(fiobj_obj2num(fiobj_ary_index(tmp, 2)) == 3,
              "JSON 'array' index 2 error!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(fiobj_ary_index(tmp, 3), FIOBJ_T_STRING),
              "JSON 'array' index 3 type error!\n");
  TEST_ASSERT(!memcmp("boom", fiobj_obj2cstr(fiobj_ary_index(tmp, 3)).data, 4),
              "JSON 'array' index 3 error!\n");
  tmp = fiobj_hash_get2(o, fio_siphash("my", 2));
  TEST_ASSERT(FIOBJ_TYPE_IS(tmp, FIOBJ_T_HASH),
              "JSON 'my:secret' not a Hash!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(fiobj_hash_get2(tmp, fio_siphash("secret", 6)),
                            FIOBJ_T_NUMBER),
              "JSON 'my:secret' doesn't hold a number!\n");
  TEST_ASSERT(fiobj_obj2num(fiobj_hash_get2(tmp, fio_siphash("secret", 6))) ==
                  42,
              "JSON 'my:secret' not 42!\n");
  TEST_ASSERT(fiobj_hash_get2(o, fio_siphash("true", 4)) == fiobj_true(),
              "JSON 'true' not true!\n");
  TEST_ASSERT(fiobj_hash_get2(o, fio_siphash("false", 5)) == fiobj_false(),
              "JSON 'false' not false!\n");
  TEST_ASSERT(fiobj_hash_get2(o, fio_siphash("null", 4)) == fiobj_null(),
              "JSON 'null' not null!\n");
  tmp = fiobj_hash_get2(o, fio_siphash("float", 5));
  TEST_ASSERT(FIOBJ_TYPE_IS(tmp, FIOBJ_T_FLOAT), "JSON 'float' not a float!\n");
  tmp = fiobj_hash_get2(o, fio_siphash("string", 6));
  TEST_ASSERT(FIOBJ_TYPE_IS(tmp, FIOBJ_T_STRING),
              "JSON 'string' not a string!\n");
  TEST_ASSERT(!strcmp(fiobj_obj2cstr(tmp).data, "I \"wrote\" this."),
              "JSON 'string' incorrect!\n");
  fprintf(stderr, "* passed.\n");
  fprintf(stderr, "=== Testing JSON formatting (simple test)\n");
  tmp = fiobj_obj2json(o, 0);
  fprintf(stderr, "* data (%p):\n%.*s\n", fiobj_obj2cstr(tmp).buffer,
          (int)fiobj_obj2cstr(tmp).len, fiobj_obj2cstr(tmp).data);
  if (!strcmp(fiobj_obj2cstr(tmp).data, json_str))
    fprintf(stderr, "* Stringify == Original.\n");
  fiobj_free(o);
  fiobj_free(tmp);
  fprintf(stderr, "* passed.\n");

  fprintf(stderr, "=== Testing JSON parsing (UTF-8 and special cases)\n");
  fiobj_json2obj(&o, "[\"\\uD834\\uDD1E\"]", 16);
  TEST_ASSERT(o, "JSON G clef String failed to parse!\n");
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY),
              "JSON G clef container has an incorrect type! (%s)\n",
              fiobj_type_name(o));
  tmp = o;
  o = fiobj_ary_pop(o);
  fiobj_free(tmp);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING),
              "JSON G clef String incorrect type! %p => %s\n", (void *)o,
              fiobj_type_name(o));
  TEST_ASSERT((!strcmp(fiobj_obj2cstr(o).data, "\xF0\x9D\x84\x9E")),
              "JSON G clef String incorrect %s !\n", fiobj_obj2cstr(o).data);
  fiobj_free(o);

  fiobj_json2obj(&o, "\"\\uD834\\uDD1E\"", 14);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING),
              "JSON direct G clef String incorrect type! %p => %s\n", (void *)o,
              fiobj_type_name(o));
  TEST_ASSERT((!strcmp(fiobj_obj2cstr(o).data, "\xF0\x9D\x84\x9E")),
              "JSON direct G clef String incorrect %s !\n",
              fiobj_obj2cstr(o).data);
  fiobj_free(o);

  fiobj_json2obj(&o, "\"Hello\\u0000World\"", 19);
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_STRING),
              "JSON NUL containing String incorrect type! %p => %s\n",
              (void *)o, fiobj_type_name(o));
  TEST_ASSERT(
      (!memcmp(fiobj_obj2cstr(o).data, "Hello\0World", fiobj_obj2cstr(o).len)),
      "JSON NUL containing String incorrect! (%u): %s . %s\n",
      (int)fiobj_obj2cstr(o).len, fiobj_obj2cstr(o).data,
      fiobj_obj2cstr(o).data + 3);
  fiobj_free(o);
  size_t consumed = fiobj_json2obj(&o, json_str2, sizeof(json_str2));
  TEST_ASSERT(
      consumed == (sizeof(json_str2) - 1),
      "JSON messy string failed to parse (consumed %lu instead of %lu\n",
      (unsigned long)consumed, (unsigned long)(sizeof(json_str2) - 1));
  TEST_ASSERT(FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY),
              "JSON messy string object error\n");
  tmp = fiobj_obj2json(o, 1);
  TEST_ASSERT(FIOBJ_TYPE_IS(tmp, FIOBJ_T_STRING),
              "JSON messy string isn't a string\n");
  fprintf(stderr, "Messy JSON:\n%s\n", fiobj_obj2cstr(tmp).data);
  fiobj_free(o);
  fiobj_free(tmp);
  fprintf(stderr, "* passed.\n");
}

#endif
