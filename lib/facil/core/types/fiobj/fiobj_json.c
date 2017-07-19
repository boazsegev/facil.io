/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"
// #include "fio2resp.h"
#include <ctype.h>

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
size_t fiobj_json2obj(fiobj_s **pobj, const void *data, size_t len);
/* Formats an object into a JSON string. Remember to `fiobj_free`. */
fiobj_s *fiobj_obj2json(fiobj_s *, uint8_t);

/* *****************************************************************************
JSON UTF-8 safe string formatting
***************************************************************************** */

static char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static uint8_t is_hex[] = {
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

/** Writes a JSON friendly version of the src String, requires the */
static void write_safe_str(fiobj_s *dest, fiobj_s *str) {
  fio_cstr_s s = fiobj_obj2cstr(str);
  const uint8_t *src = (const uint8_t *)s.data;
  size_t len = s.len;
  uint64_t end = obj2str(dest)->len;
  /* make sure we have some room */
  size_t added = 0;
  if (obj2str(dest)->capa <= end + s.len + 64)
    fiobj_str_capa_assert(dest, (((obj2str(dest)->capa >> 12) + 1) << 12) - 1);
  while (len) {
    while (len &&
           (src[0] > 32 && src[0] != '"' && src[0] != '\\' && src[0] != '/')) {
      len--;
      obj2str(dest)->str[end++] = *(src++);
    }
    if (!len)
      break;
    switch (src[0]) {
    case '\b':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = 'b';
      added++;
      break; /* from switch */
    case '\f':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = 'f';
      added++;
      break; /* from switch */
    case '\n':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = 'n';
      added++;
      break; /* from switch */
    case '\r':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = 'r';
      added++;
      break; /* from switch */
    case '\t':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = 't';
      added++;
      break; /* from switch */
    case '"':
    case '\\':
    case '/':
      obj2str(dest)->str[end++] = '\\';
      obj2str(dest)->str[end++] = src[0];
      added++;
      break; /* from switch */
    default:
      if (src[0] <= 31) {
        /* MUST escape all control values less than 32 */
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 'u';
        obj2str(dest)->str[end++] = '0';
        obj2str(dest)->str[end++] = '0';
        obj2str(dest)->str[end++] = hex_chars[src[0] >> 4];
        obj2str(dest)->str[end++] = hex_chars[src[0] & 15];
        added += 4;
      } else
        obj2str(dest)->str[end++] = src[0];
      break; /* from switch */
    }
    src++;
    len--;
    if (added >= 48 && obj2str(dest)->capa <= end + len + 64) {
      fiobj_str_capa_assert(dest,
                            (((obj2str(dest)->capa >> 12) + 1) << 12) - 1);
      added = 0;
    }
  }
  obj2str(dest)->len = end;
  obj2str(dest)->str[end] = 0;
}

/* *****************************************************************************
JSON formatting
***************************************************************************** */

/* this is used to persist data in `fiobj_each2` */
struct fiobj_str_new_json_data_s {
  fiobj_s *parent;  /* stores item types */
  fiobj_s *waiting; /* stores item counts and types */
  fiobj_s *buffer;  /* we'll write the JSON here */
  fiobj_s *count;   /* used to persist item counts for arrays / hashes */
  uint8_t pretty;   /* make it beautiful */
};

static int fiobj_str_new_json_task(fiobj_s *obj, void *d_) {
  struct fiobj_str_new_json_data_s *data = d_;
  if (data->count && fiobj_obj2num(data->count))
    fiobj_num_set(data->count, fiobj_obj2num(data->count) - 1);
  /* headroom */
  fiobj_str_capa_assert(
      data->buffer,
      ((((obj2str(data->buffer)->len + 63) >> 12) + 1) << 12) - 1);
pretty_re_rooted:
  /* pretty? */
  if (data->pretty) {
    fiobj_str_write(data->buffer, "\n", 1);
    for (size_t i = 0; i < fiobj_ary_count(data->parent); i++) {
      fiobj_str_write(data->buffer, "  ", 2);
    }
  }
re_rooted:
  if (!obj) {
    fiobj_str_write(data->buffer, "null", 4);
    goto review_nesting;
  }
  switch (obj->type) {
  case FIOBJ_T_HASH:
    fiobj_str_write(data->buffer, "{", 1);
    fiobj_ary_push(data->parent, obj);
    fiobj_ary_push(data->waiting, data->count);
    data->count = fiobj_num_new(fiobj_hash_count(obj));
    break;
  case FIOBJ_T_ARRAY:
    fiobj_str_write(data->buffer, "[", 1);
    /* push current state to stacks and update state */
    fiobj_ary_push(data->parent, obj);
    fiobj_ary_push(data->waiting, data->count);
    data->count = fiobj_num_new(fiobj_ary_count(obj));
    break;
  case FIOBJ_T_SYMBOL:
  case FIOBJ_T_STRING: {
    fiobj_str_capa_assert(
        data->buffer,
        ((((obj2str(data->buffer)->len + 63 + obj2str(obj)->len) >> 12) + 1)
         << 12) -
            1);
    fiobj_str_write(data->buffer, "\"", 1);
    write_safe_str(data->buffer, obj);
    fiobj_str_write(data->buffer, "\"", 1);
    break;
  }
  case FIOBJ_T_COUPLET: {
    fiobj_str_capa_assert(data->buffer,
                          ((((obj2str(data->buffer)->len + 31 +
                              obj2sym(fiobj_couplet2key(obj))->len) >>
                             12) +
                            1)
                           << 12) -
                              1);
    fiobj_str_write(data->buffer, "\"", 1);
    write_safe_str(data->buffer, fiobj_couplet2key(obj));
    fiobj_str_write(data->buffer, "\":", 2);
    obj = fiobj_couplet2obj(obj);
    if (data->pretty &&
        (obj->type == FIOBJ_T_ARRAY || obj->type == FIOBJ_T_HASH))
      goto pretty_re_rooted;
    goto re_rooted;
    break;
  }
  case FIOBJ_T_NUMBER:
    obj2str(data->buffer)->len +=
        fio_ltoa(obj2str(data->buffer)->str + obj2str(data->buffer)->len,
                 obj2num(obj)->i, 10);
    break;
  case FIOBJ_T_FLOAT:
    if (isnan(obj2float(obj)->f))
      fiobj_str_write(data->buffer, "\"NaN\"", 5);
    else if (isinf(obj2float(obj)->f)) {
      if (obj2float(obj)->f > 0)
        fiobj_str_write(data->buffer, "\"Infinity\"", 10);
      else
        fiobj_str_write(data->buffer, "\"-Infinity\"", 11);
    } else {
      char *start = obj2str(data->buffer)->str + obj2str(data->buffer)->len;
      fiobj_str_write2(data->buffer, "%g", obj2float(obj)->f);
      uint8_t need_zero = 1;
      while (*start) {
        if (*start == ',') // locale issues?
          *start = '.';
        if (*start == '.' || *start == 'e') {
          need_zero = 0;
          break;
        }
        start++;
      }
      if (need_zero)
        fiobj_str_write(data->buffer, ".0", 2);
    }
    break;
  // case FIOBJ_T_FLOAT:
  //   obj2str(data->buffer)->len +=
  //       fio_ftoa(obj2str(data->buffer)->str + obj2str(data->buffer)->len,
  //                obj2float(obj)->f, 10);
  //   break;
  case FIOBJ_T_TRUE:
    fiobj_str_write(data->buffer, "true", 4);
    break;
  case FIOBJ_T_FALSE:
    fiobj_str_write(data->buffer, "false", 5);
    break;
  case FIOBJ_T_IO:
  case FIOBJ_T_NULL:
    fiobj_str_write(data->buffer, "null", 4);
    break;
  }

review_nesting:
  /* print clousure to String */
  while (!fiobj_obj2num(data->count)) {
    fiobj_s *tmp = fiobj_ary_pop(data->parent);
    if (!tmp)
      break;
    fiobj_free(data->count);
    data->count = fiobj_ary_pop(data->waiting);
    if (data->pretty) {
      fiobj_str_write(data->buffer, "\n", 1);
      for (size_t i = 0; i < fiobj_ary_count(data->parent); i++) {
        fiobj_str_write(data->buffer, "  ", 2);
      }
    }
    if (tmp->type == FIOBJ_T_ARRAY)
      fiobj_str_write(data->buffer, "]", 1);
    else
      fiobj_str_write(data->buffer, "}", 1);
  }
  /* print object divisions to String */
  if (data->count && fiobj_obj2num(data->count) &&
      (!obj || (obj->type != FIOBJ_T_ARRAY && obj->type != FIOBJ_T_HASH)))
    fiobj_str_write(data->buffer, ",", 1);
  return 0;
}

/* Formats an object into a JSON string. Remember to `fiobj_free`. */
fiobj_s *fiobj_obj2json(fiobj_s *obj, uint8_t pretty) {
  /* Using a whole page size could optimize future allocations (no copy) */
  struct fiobj_str_new_json_data_s data = {
      .parent = fiobj_ary_new(),
      .waiting = fiobj_ary_new(),
      .buffer = fiobj_str_buf(4096),
      .count = NULL,
      .pretty = pretty,
  };
  fiobj_each2(obj, fiobj_str_new_json_task, &data);

  while (fiobj_ary_pop(data.parent))
    ; /* we didn't duplicate the objects, so we must remove them from array */
  fiobj_free(data.parent);
  fiobj_free(data.waiting);
  fiobj_str_minimize(data.buffer);
  return data.buffer;
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
inline static uint8_t move_to_eol(const uint8_t **pos, const uint8_t *limit) {
  /* single char lookup using library is best when target is far... */
  if (*pos >= limit)
    return 0;
  if (**pos == '\n')
    return 1;
  void *tmp = memchr(*pos, '\n', limit - (*pos));
  if (tmp) {
    *pos = tmp;
    return 1;
  }
  *pos = limit;
  return 0;
}

inline static int move_to_quote(const uint8_t **pos, const uint8_t *limit) {
  if (**pos == '\\')
    return 1;
  if (**pos == '\"')
    return 0;
  uint64_t wanted1 = 0x0101010101010101ULL * '"';
  uint64_t wanted2 = 0x0101010101010101ULL * '\\';
  uint64_t *lpos = (uint64_t *)*pos;
  uint64_t *llimit = ((uint64_t *)limit) - 1;

  for (; lpos < llimit; ++lpos) {
    const uint64_t eq1 = ~((*lpos) ^ wanted1);
    const uint64_t t0 = (eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
    const uint64_t t1 = (eq1 & 0x8080808080808080llu);
    const uint64_t eq2 = ~((*lpos) ^ wanted2);
    const uint64_t t2 = (eq2 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
    const uint64_t t3 = (eq2 & 0x8080808080808080llu);
    if ((t0 & t1) || (t2 & t3))
      break;
  }
  *pos = (uint8_t *)lpos;

  while (*pos < limit) {
    if (**pos == '\"') {
      return 0;
    }
    if (**pos == '\\') {
      return 1;
    }
    (*pos)++;
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
static void safestr2local(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING && str->type != FIOBJ_T_SYMBOL) {
    fprintf(stderr,
            "CRITICAL ERROR: unexpected function call `safestr2local`\n");
    exit(-1);
  }
  fio_cstr_s s = fiobj_obj2cstr(str);
  uint8_t had_changed = 0;
  uint8_t *end = (uint8_t *)s.bytes + s.len;
  uint8_t *reader = (uint8_t *)s.bytes;
  uint8_t *writer = (uint8_t *)s.bytes;
  while (reader < end) {
    while (reader < end && reader[0] != '\\') {
      *(writer++) = *(reader++);
    }
    if (reader[0] != '\\')
      break;

    had_changed = 1;
    switch (reader[1]) {
    case 'b':
      *(writer++) = '\b';
      reader += 2;
      break; /* from switch */
    case 'f':
      *(writer++) = '\f';
      reader += 2;
      break; /* from switch */
    case 'n':
      *(writer++) = '\n';
      reader += 2;
      break; /* from switch */
    case 'r':
      *(writer++) = '\r';
      reader += 2;
      break; /* from switch */
    case 't':
      *(writer++) = '\t';
      reader += 2;
      break;    /* from switch */
    case 'u': { /* test for octal notation */
      if (is_hex[reader[2]] && is_hex[reader[3]] && is_hex[reader[4]] &&
          is_hex[reader[5]]) {
        uint32_t t =
            ((((is_hex[reader[2]] - 1) << 4) | (is_hex[reader[3]] - 1)) << 8) |
            (((is_hex[reader[4]] - 1) << 4) | (is_hex[reader[5]] - 1));
        if (reader[6] == '\\' && reader[7] == 'u' && is_hex[reader[8]] &&
            is_hex[reader[9]] && is_hex[reader[10]] && is_hex[reader[11]]) {
          /* Serrogate Pair */
          t = (t & 0x03FF) << 10;
          t |= ((((((is_hex[reader[8]] - 1) << 4) | (is_hex[reader[9]] - 1))
                  << 8) |
                 (((is_hex[reader[10]] - 1) << 4) | (is_hex[reader[11]] - 1))) &
                0x03FF);
          t += 0x10000;
          /* Wikipedia way: */
          // t = 0x10000 + ((t - 0xD800) * 0x400) +
          //     ((((((is_hex[reader[8]] - 1) << 4) | (is_hex[reader[9]] - 1))
          //        << 8) |
          //       (((is_hex[reader[10]] - 1) << 4) | (is_hex[reader[11]] -
          //       1)))
          //       -
          //      0xDC00);
          reader += 6;
        }
        writer += utf8_from_u32(writer, t);
        reader += 6;
        break; /* from switch */
      } else
        goto invalid_escape;
    }
    case 'x': { /* test for hex notation */
      if (is_hex[reader[2]] && is_hex[reader[3]]) {
        *(writer++) = ((is_hex[reader[2]] - 1) << 4) | (is_hex[reader[3]] - 1);
        reader += 4;
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
      if (reader[1] >= '0' && reader[1] <= '7' && reader[2] >= '0' &&
          reader[2] <= '7') {
        *(writer++) = ((reader[1] - '0') << 3) | (reader[2] - '0');
        reader += 3;
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
      *(writer++) = reader[1];
      reader += 2;
    }
  }
  if (str->type == FIOBJ_T_STRING) {
    obj2str(str)->len = (uintptr_t)writer - (uintptr_t)obj2str(str)->str;
    obj2str(str)->str[obj2str(str)->len] = 0;
  } else {
    obj2sym(str)->len = (uintptr_t)writer - (uintptr_t)obj2sym(str)->str;
    obj2sym(str)->str[obj2sym(str)->len] = 0;
    if (had_changed)
      obj2sym(str)->hash = fiobj_sym_hash(obj2sym(str)->str, obj2sym(str)->len);
  }
}

/* *****************************************************************************
JSON => Obj
*****************************************************************************
*/

/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data
 * is consumed.
 */
size_t fiobj_json2obj(fiobj_s **pobj, const void *data, size_t len) {
  if (!data) {
    *pobj = NULL;
    return 0;
  }
  fiobj_s *nesting = fiobj_ary_new2(JSON_MAX_DEPTH + 2);
  const uint8_t *start;
  fiobj_s *obj;
  const uint8_t *end = (uint8_t *)data;
  const uint8_t *stop = end + len;
  uint8_t depth = 0;
  while (1) {
    /* skip any white space / seperators */
    move_to_start(&end, stop);
    /* set objcet data end point to the starting endpoint */
    obj = NULL;
    start = end;
    if (end >= stop) {
      goto finish;
    }

    /* test object type. tests are ordered by precedence, if one fails, the
     * other is performed. */
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
      if (obj->type != FIOBJ_T_HASH) {
        goto error;
      }
      goto has_obj;
    }
    if (end[0] == ']') {
      /* end an array */
      end++;
      depth--;
      obj = fiobj_ary_pop(nesting);
      if (obj->type != FIOBJ_T_ARRAY) {
        goto error;
      }
      goto has_obj;
    }
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
    if (end[0] == '/') {
      /* could be a Javascript comment */
      if (end[1] == '/') {
        end += 2;
        move_to_eol(&end, stop);
        continue;
      }
      if (end[1] == '*') {
        end += 2;
        while (end < stop && !(end[0] == '*' && end[1] == '/'))
          end++;
        if (end < stop && end[0] == '*')
          end += 2;
        continue;
      }
    }
    if (end[0] == '#') {
      /* could be a Ruby style comment */
      move_to_eol(&end, stop);
      continue;
    }
    if (start[0] == '"') {
      /* object is a string (require qoutes) */
      start++;
      end++;
      uint8_t dirty = 0;
      while (move_to_quote(&end, stop)) {
        end += 2;
        dirty = 1;
      }
      if (end >= stop) {
        goto error;
      }
      if (fiobj_ary_entry(nesting, -1) &&
          fiobj_ary_entry(nesting, -1)->type == FIOBJ_T_HASH) {
        obj = fiobj_sym_new((char *)start, end - start);
      } else {
        obj = fiobj_str_new((char *)start, end - start);
      }
      if (dirty)
        safestr2local(obj);
      end++;

      goto has_obj;
    }
    if (end[0] == '-' || (end[0] >= '0' && end[0] <= '9')) {
      /* test for a number OR float */
      int64_t num = fio_atol((char **)&end);
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
      // uint8_t decimal = 0;
      // while (end < stop && JSON_SEPERATOR[*end] == 0 && *end != ']' &&
      //        *end != '}') {
      //   if (*end == '.' || *end == 'e' || *end == 'E')
      //     decimal = 1;
      //   end++;
      // }
      // /* test against forbidden leading zeros... but allow hex and binary
      // */ if (end - start > 1 && start[0] == '0' &&
      //     !(start[1] == '.' || start[1] == 'x' || start[1] == 'b')) {
      //   goto error;
      // }
      // /* it's a number */
      // if (decimal) {
      //   obj = fiobj_float_new(fio_atof((char *)start));
      // } else {
      //   obj = fiobj_num_new(fio_atol((char *)start));
      // }
      // goto has_obj;
    }
    if (end[0] == 'N' && end[1] == 'a' && end[2] == 'N') {
      obj = fiobj_float_new(nan(""));
      goto has_obj;
    }
    if (end[0] == '-' && end[1] == 'I' && end[2] == 'n' && end[3] == 'f') {
      obj = fiobj_float_new(nan(""));
      copysign(obj2float(obj)->f, (double)-1);
      goto has_obj;
    }
    if (end[0] == 'I' && end[1] == 'n' && end[2] == 'f') {
      move_to_end(&end, stop);
      obj = fiobj_float_new(INFINITY);
      goto has_obj;
    }
    if (end[0] == '-' && end[1] == 'I' && end[2] == 'n' && end[3] == 'f') {
      move_to_end(&end, stop);
      obj = fiobj_float_new(INFINITY);
      copysign(obj2float(obj)->f, (double)-1);
      goto has_obj;
    }
    goto error;

  has_obj:

    if (fiobj_ary_count(nesting) == 0)
      goto finish_with_obj;
    if (fiobj_ary_entry(nesting, -1)->type == FIOBJ_T_ARRAY) {
      fiobj_ary_push(fiobj_ary_entry(nesting, -1), obj);
      continue;
    }
    if (fiobj_ary_entry(nesting, -1)->type == FIOBJ_T_HASH) {
      fio_cstr_s s = fiobj_obj2cstr(obj);
      fiobj_ary_push(nesting, fiobj_sym_new(s.buffer, s.len));
      fiobj_free(obj);
      continue;
    }
    fiobj_s *sym = fiobj_ary_pop(nesting);
    if (fiobj_ary_entry(nesting, -1)->type != FIOBJ_T_HASH)
      goto error;
    fiobj_hash_set(fiobj_ary_entry(nesting, -1), sym, obj);
    fiobj_free(sym);
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
  *pobj = NULL;
  // fprintf(stderr, "ERROR starting at %.*s, ending at %.*s, with %s\n", 3,
  // start,
  //         3, end, start);
  return 0;
}
