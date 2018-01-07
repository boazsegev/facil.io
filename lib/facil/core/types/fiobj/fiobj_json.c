/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/
#include "fiobj_json.h"

#include "fio_ary.h"

// #include "fio2resp.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* *****************************************************************************
JSON API
***************************************************************************** */
#define JSON_MAX_DEPTH 24
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
JSON UTF-8 safe string formatting
***************************************************************************** */

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

// /* invalid byte length is ignored */
// static inline int utf8_clen(const uint8_t *str) {
//   if (str[0] <= 127)
//     return 1;
//   if ((str[0] & 224) == 192 && (str[1] & 192) == 128)
//     return 2;
//   if ((str[0] & 240) == 224 && (str[1] & 192) == 128 && (str[2] & 192) ==
//   128)
//     return 3;
//   if ((str[0] & 248) == 240 && (str[1] & 192) == 128 && (str[2] & 192) == 128
//   &&
//       (str[3] & 192) == 128)
//     return 4;
//   return 0; /* invalid UTF-8 */
// }

/* converts a uint16_t to UTF-8 and returns the number of bytes written */
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

/* *****************************************************************************
JSON formatting
***************************************************************************** */

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

  case FIOBJ_T_IO:
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
JSON parsing
*****************************************************************************
*/

/*
Marks as object seperators any of the following:

* White Space: [0x09, 0x0A, 0x0D, 0x20]
* Comma ("," / 0x2C)
* Colon (":" / 0x3A)
The rest belong to objects,
*/
static const uint8_t JSON_SEPERATOR[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

inline static void move_to_end(const uint8_t **pos, const uint8_t *limit) {
  while (*pos < limit && JSON_SEPERATOR[**pos] == 0)
    (*pos)++;
}
inline static void move_to_start(const uint8_t **pos, const uint8_t *limit) {
  while (*pos < limit && JSON_SEPERATOR[**pos])
    (*pos)++;
}
inline static uint8_t move_to_ch(const uint8_t **pos, const uint8_t *limit,
                                 uint8_t ch) {
  /* single char lookup using library is best when target is far... */
  if (*pos >= limit)
    return 0;
  if (**pos == ch)
    return 1;
  void *tmp = memchr(*pos, ch, limit - (*pos));
  if (tmp) {
    *pos = tmp;
    return 1;
  }
  *pos = limit;
  return 0;
}
inline static uint8_t move_to_eol(const uint8_t **pos, const uint8_t *limit) {
  return move_to_ch(pos, limit, '\n');
}

/**
 * This seems to be faster on some systems, especially for smaller distances.
 *
 * On newer systems, `memchr` should be faster.
 */
static inline int move_to_quote(const uint8_t **buffer,
                                register const uint8_t *limit) {
  if (**buffer == '"')
    return 0;
  if (**buffer == '\\')
    return 1;

#if !defined(__x86_64__)
  /* too short for this mess */
  if ((uintptr_t)limit <= 16 + ((uintptr_t)*buffer & (~(uintptr_t)7)))
    goto finish;

  /* align memory */
  {
    const uint8_t *alignment =
        (uint8_t *)(((uintptr_t)(*buffer) & (~(uintptr_t)7)) + 8);
    if (limit >= alignment) {
      while (*buffer < alignment) {
        if (**buffer == '"')
          return 0;
        if (**buffer == '\\')
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
        ((eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu) &
        (eq1 & 0x8080808080808080llu);
    const uint64_t eq2 = ~((*((uint64_t *)*buffer)) ^ wanted2);
    const uint64_t t2 =
        ((eq2 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu) &
        (eq2 & 0x8080808080808080llu);
    if (t1 || t2) {
      break;
    }
  }
#if !defined(__x86_64__)
finish:
#endif
  while (*buffer < limit) {
    if (**buffer == '"')
      return 0;
    if (**buffer == '\\')
      return 1;
    (*buffer)++;
  }

  return 0;
}

/* *****************************************************************************
JSON UTF-8 safe string deconstruction
*****************************************************************************
*/

/* Converts a JSON friendly String to a binary String.
 *
 * Also deals with the non-standard oct ("\77") and hex ("\xFF") notations
 */
static FIOBJ safestr2local(FIOBJ str, const uint8_t **pos, const uint8_t *end) {
  fio_cstr_s s = fiobj_obj2cstr(str);
  uintptr_t capa = fiobj_str_capa_assert(str, s.len + 8);
  s = fiobj_obj2cstr(str);
  // uint8_t had_changed = 0;
  while (*pos < end) {
    while (*pos < end && **pos != '\\' && **pos != '"') {
      s.bytes[s.len++] = **pos;
      ++(*pos);
      if (s.len >= capa) {
        capa = fiobj_str_capa_assert(str, capa << 1);
        fiobj_str_resize(str, s.len);
        s = fiobj_obj2cstr(str);
      }
    }
    if (*pos == end || **pos != '\\')
      break;
    if (s.len + 6 >= capa) {
      capa = fiobj_str_capa_assert(str, capa << 1);
      fiobj_str_resize(str, s.len);
      s = fiobj_obj2cstr(str);
    }

    // had_changed = 1;
    switch ((*pos)[1]) {
    case 'b':
      s.bytes[s.len++] = '\b';
      (*pos) += 2;
      break; /* from switch */
    case 'f':
      s.bytes[s.len++] = '\f';
      (*pos) += 2;
      break; /* from switch */
    case 'n':
      s.bytes[s.len++] = '\n';
      (*pos) += 2;
      break; /* from switch */
    case 'r':
      s.bytes[s.len++] = '\r';
      (*pos) += 2;
      break; /* from switch */
    case 't':
      s.bytes[s.len++] = '\t';
      (*pos) += 2;
      break;    /* from switch */
    case 'u': { /* test for octal notation */
      if (is_hex[(*pos)[2]] && is_hex[(*pos)[3]] && is_hex[(*pos)[4]] &&
          is_hex[(*pos)[5]]) {
        uint32_t t =
            ((((is_hex[(*pos)[2]] - 1) << 4) | (is_hex[(*pos)[3]] - 1)) << 8) |
            (((is_hex[(*pos)[4]] - 1) << 4) | (is_hex[(*pos)[5]] - 1));
        if ((*pos)[6] == '\\' && (*pos)[7] == 'u' && is_hex[(*pos)[8]] &&
            is_hex[(*pos)[9]] && is_hex[(*pos)[10]] && is_hex[(*pos)[11]]) {
          /* Serrogate Pair */
          t = (t & 0x03FF) << 10;
          t |= ((((((is_hex[(*pos)[8]] - 1) << 4) | (is_hex[(*pos)[9]] - 1))
                  << 8) |
                 (((is_hex[(*pos)[10]] - 1) << 4) | (is_hex[(*pos)[11]] - 1))) &
                0x03FF);
          t += 0x10000;
          (*pos) += 6;
        }
        s.len += utf8_from_u32(s.bytes + s.len, t);
        (*pos) += 6;
        break; /* from switch */
      } else
        goto invalid_escape;
    }
    case 'x': { /* test for hex notation */
      if (is_hex[(*pos)[2]] && is_hex[(*pos)[3]]) {
        s.bytes[s.len++] =
            ((is_hex[(*pos)[2]] - 1) << 4) | (is_hex[(*pos)[3]] - 1);
        (*pos) += 4;
        break; /* from switch */
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
      if ((*pos)[1] >= '0' && (*pos)[1] <= '7' && (*pos)[2] >= '0' &&
          (*pos)[2] <= '7') {
        s.bytes[s.len++] = (((*pos)[1] - '0') << 3) | ((*pos)[2] - '0');
        (*pos) += 3;
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
      s.bytes[s.len++] = (*pos)[1];
      (*pos) += 2;
    }
  }
  fiobj_str_resize(str, s.len);
  // if(had_changed)
  return str;
}

/* *****************************************************************************
JSON => Obj
***************************************************************************** */

/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data
 * is consumed.
 */
size_t fiobj_json2obj(FIOBJ *pobj, const void *data, size_t len) {
  if (!data || !len) {
    *pobj = 0;
    return 0;
  }
  FIOBJ nesting = fiobj_ary_new2(JSON_MAX_DEPTH + 2);
  int64_t num;
  const uint8_t *start;
  FIOBJ obj;
  const uint8_t *end = (uint8_t *)data;
  const uint8_t *stop = end + len;
  uint8_t depth = 0;
  while (1) {
    /* skip any white space / seperators */
    move_to_start(&end, stop);
    /* set objcet data end point to the starting endpoint */
    obj = 0;
    start = end;
    if (end >= stop) {
      goto finish;
    }

    /* test object type. tests are ordered by precedence, if one fails, the
     * other is performed. */
    if (start[0] == '"') {
      /* object is a string (require qoutes) */
      ++start;
      ++end;
      if (move_to_quote(&end, stop)) {
        if (end >= stop) {
          goto error;
        }
        obj = fiobj_str_new((char *)start, end - start);
        obj = safestr2local(obj, &end, stop);
        if (end >= stop || *end != '"')
          goto error;
        ++end;
        goto has_obj;
      }
      if (end >= stop) {
        goto error;
      }
      obj = fiobj_str_new((char *)start, end - start);
      end++;
      goto has_obj;
    }
    if (end[0] >= 'a' && end[0] <= 'z') {
      if (end + 3 < stop && end[0] == 't' && end[1] == 'r' && end[2] == 'u' &&
          end[3] == 'e') {
        /* true */
        end += 4;
        obj = fiobj_true();
        goto has_obj;
      }
      if (end + 4 < stop && end[0] == 'f' && end[1] == 'a' && end[2] == 'l' &&
          end[3] == 's' && end[4] == 'e') {
        /* false */
        end += 5;
        obj = fiobj_false();
        goto has_obj;
      }
      if (end + 3 < stop && end[0] == 'n' && end[1] == 'u' && end[2] == 'l' &&
          end[3] == 'l') {
        /* null */
        end += 4;
        obj = fiobj_null();
        goto has_obj;
      }
      goto error;
    }
    if (end[0] == '{') {
      /* start an object (hash) */
      fiobj_ary_push(nesting, fiobj_hash_new());
      end++;
      depth++;
      if (depth >= JSON_MAX_DEPTH) {
        goto error;
      }
      continue;
    }
    if (end[0] == '[') {
      /* start an array */
      fiobj_ary_push(nesting, fiobj_ary_new2(4));
      end++;
      depth++;
      if (depth >= JSON_MAX_DEPTH) {
        goto error;
      }
      continue;
    }
    if (end[0] == '}') {
      /* end an object (hash) */
      end++;
      depth--;
      obj = fiobj_ary_pop(nesting);
      if (!FIOBJ_TYPE_IS(obj, FIOBJ_T_HASH)) {
        goto error;
      }
      goto has_obj;
    }
    if (end[0] == ']') {
      /* end an array */
      end++;
      depth--;
      obj = fiobj_ary_pop(nesting);
      if (!FIOBJ_TYPE_IS(obj, FIOBJ_T_ARRAY)) {
        goto error;
      }
      goto has_obj;
    }

    if (end[0] == '-') {
      if (end[0] == '-' && end[1] == 'N' && end[2] == 'a' && end[3] == 'N') {
        move_to_end(&end, stop);
        double fl = nan("");
        fl = copysign(fl, (double)-1);
        obj = fiobj_float_new(fl);
        goto has_obj;
      }
      if (end[0] == '-' && end[1] == 'I' && end[2] == 'n' && end[3] == 'f') {
        move_to_end(&end, stop);
        double fl = INFINITY;
        fl = copysign(fl, (double)-1);
        obj = fiobj_float_new(fl);
        goto has_obj;
      }
      goto test_for_number;
    }
    if (end[0] >= '0' && end[0] <= '9') {
    test_for_number: /* test for a number OR float */
      num = fio_atol((char **)&end);
      if (end == start || *end == '.' || *end == 'e' || *end == 'E') {
        end = start;
        double fnum = fio_atof((char **)&end);
        if (end == start)
          goto error;
        obj = fiobj_float_new(fnum);
        goto has_obj;
      }
      obj = fiobj_num_new(num);
      goto has_obj;
    }

    if (end[0] == 'N' && end[1] == 'a' && end[2] == 'N') {
      move_to_end(&end, stop);
      obj = fiobj_float_new(nan(""));
      goto has_obj;
    }
    if (end[0] == 'I' && end[1] == 'n' && end[2] == 'f') {
      move_to_end(&end, stop);
      obj = fiobj_float_new(INFINITY);
      goto has_obj;
    }
    if (end[0] == '/') {
      /* could be a Javascript comment */
      if (end[1] == '/') {
        end += 2;
        move_to_eol(&end, stop);
        continue;
      }
      if (end[1] == '*') {
        end += 2;
        do {
          move_to_ch(&end, stop, '*');
          if (end == stop) {
            goto error;
          }
          if (end[1] == '/')
            break;
          ++end;
        } while (1);
        continue;
      }
    }
    if (end[0] == '#') {
      /* could be a Ruby style comment */
      move_to_eol(&end, stop);
      continue;
    }
    goto error;

  has_obj:

    if (fiobj_ary_count(nesting) == 0)
      goto finish_with_obj;
    if (FIOBJ_TYPE_IS(fiobj_ary_index(nesting, -1), FIOBJ_T_ARRAY)) {
      fiobj_ary_push(fiobj_ary_index(nesting, -1), obj);
      continue;
    } else if (FIOBJ_TYPE_IS(fiobj_ary_index(nesting, -1), FIOBJ_T_HASH)) {
      fiobj_ary_push(nesting, obj);
      continue;
    }
    FIOBJ key = fiobj_ary_pop(nesting);
    if (!FIOBJ_TYPE_IS(fiobj_ary_index(nesting, -1), FIOBJ_T_HASH)) {
      fiobj_free(key);
      goto error;
    }
    fiobj_hash_set(fiobj_ary_index(nesting, -1), key, obj);
    fiobj_free(key);
    continue;
  }

finish:
  if (!obj)
    obj = fiobj_ary_pop(nesting);
finish_with_obj:
  if (obj && fiobj_ary_count(nesting) == 0) {
    *pobj = obj;
    fiobj_free(nesting);
    return end - (uint8_t *)data;
  }
error:
  if (obj)
    fiobj_free(obj);
  while ((obj = fiobj_ary_pop(nesting)))
    fiobj_free(obj);
  fiobj_free(nesting);
  *pobj = 0;
  // fprintf(stderr, "ERROR starting at %.*s, ending at %.*s, with %s\n", 3,
  // start,
  //         3, end, start);
  return 0;
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
}

#endif
