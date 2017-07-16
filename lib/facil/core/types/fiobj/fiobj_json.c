/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"
// #include "fio2resp.h"

/* *****************************************************************************
JSON API
***************************************************************************** */

/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
size_t fiobj_json2obj(fiobj_s **pobj, const void *data, size_t len);
/* Formats an object into a JSON string. Remember to `fiobj_free`. */
fiobj_s *fiobj_obj2json(fiobj_s *);

/* *****************************************************************************
JSON UTF-8 safe string formatting
***************************************************************************** */

static char hex_notation[] = {'0', '1', '2', '3', '4', '5', '6', '7',
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

/* invalid byte length is ignored */
static inline int utf8_clen(const uint8_t *str) {
  if (str[0] <= 127)
    return 1;
  if ((str[0] & 224) == 192 && (str[1] & 192) == 128)
    return 2;
  if ((str[0] & 240) == 224 && (str[1] & 192) == 128 && (str[2] & 192) == 128)
    return 3;
  if ((str[0] & 248) == 240 && (str[1] & 192) == 128 && (str[2] & 192) == 128 &&
      (str[3] & 192) == 128)
    return 4;
  return 0; /* invalid UTF-8 */
}

/* converts a uint16_t to UTF-8 and returns the number of bytes written */
static inline int utf8_from_u16(uint8_t *dest, uint16_t u) {
  if (u <= 127) {
    *dest = u;
    return 1;
  } else if (u <= 2047) {
    *(dest++) = 192 | (u >> 6);
    *(dest++) = 128 | (u & 63);
    return 2;
  }
  *(dest++) = 192 | (u >> 12);
  *(dest++) = 128 | ((u >> 6) & 63);
  *(dest++) = 128 | (u & 63);
  return 3;
}

/** Writes a JSON friendly version of the src String, requires the */
static void write_safe_str(fiobj_s *dest, fiobj_s *str) {
  fio_cstr_s s = fiobj_obj2cstr(str);
  const uint8_t *src = (const uint8_t *)s.data;
  size_t len = s.len;
  uint64_t end = obj2str(dest)->len;
  while (len) {
    /* make sure we have enough space for the largest UTF-8 char + NUL */
    if (obj2str(dest)->capa <= end + 7)
      fiobj_str_resize(dest, (((obj2str(dest)->capa >> 12) + 1) << 12) - 1);
    int tmp = utf8_clen(src);
    if (tmp == 1) {
      switch (src[0]) {
      case '\b':
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 'b';
        break; /* from switch */
      case '\f':
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 'f';
        break; /* from switch */
      case '\n':
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 'n';
        break; /* from switch */
      case '\r':
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 'r';
        break; /* from switch */
      case '\t':
        obj2str(dest)->str[end++] = '\\';
        obj2str(dest)->str[end++] = 't';
        break; /* from switch */
      case '"':
      case '\\':
      case '/':
        obj2str(dest)->str[end++] = '\\';
      /* fallthrough */
      default:
        if (src[0] <= 31) {
          /*TODO: MUST escape all control values less than 32 */
          obj2str(dest)->str[end++] = '\\';
          obj2str(dest)->str[end++] = 'u';
          obj2str(dest)->str[end++] = '0';
          obj2str(dest)->str[end++] = '0';
          obj2str(dest)->str[end++] = hex_notation[src[0] >> 4];
          obj2str(dest)->str[end++] = hex_notation[src[0] & 15];
        } else
          obj2str(dest)->str[end++] = src[0];
        break; /* from switch */
      }
      src++;
      len--;
    } else if (tmp == 2) {
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      len -= 2;
    } else if (tmp == 3) {
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      len -= 3;
    } else if (tmp == 4) {
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      obj2str(dest)->str[end++] = *(src++);
      len -= 4;
    } else { /* (tmp == 0) */
      // fprintf(stderr,
      //         "WARN: invalid UTF-8 code in JSON string val=%u, pos
      //         %llu:\n%s\n", *src, end - obj2str(dest)->len, s.data);
      obj2str(dest)->str[end++] = src[0];
      src++;
      len--;
      // int t = utf8_from_u16((uint8_t *)(obj2str(dest)->str + end), *(src++));
      // src += t;
      // len -= t;
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
};

static int fiobj_str_new_json_task(fiobj_s *obj, void *d_) {
  struct fiobj_str_new_json_data_s *data = d_;
  if (data->count && fiobj_obj2num(data->count))
    fiobj_num_set(data->count, fiobj_obj2num(data->count) - 1);
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
    fiobj_str_write(data->buffer, "\"", 1);
    write_safe_str(data->buffer, obj);
    fiobj_str_write(data->buffer, "\"", 1);
    break;
  }
  case FIOBJ_T_COUPLET: {
    fiobj_str_write(data->buffer, "\"", 1);
    write_safe_str(data->buffer, fiobj_couplet2key(obj));
    fiobj_str_write(data->buffer, "\":", 2);
    obj = fiobj_couplet2obj(obj);
    goto re_rooted;
    break;
  }
  case FIOBJ_T_NUMBER: {
    fio_cstr_s s = fiobj_obj2cstr(obj);
    fiobj_str_write(data->buffer, s.data, s.len);
    break;
  }
  case FIOBJ_T_FLOAT:
    fiobj_str_write2(data->buffer, "%e", fiobj_obj2float(obj));
    break;
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
fiobj_s *fiobj_obj2json(fiobj_s *obj) {
  /* Using a whole page size could optimize future allocations (no copy) */
  struct fiobj_str_new_json_data_s data = {
      .parent = fiobj_ary_new(),
      .waiting = fiobj_ary_new(),
      .buffer = fiobj_str_buf(4096),
      .count = NULL,
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
JSON UTF-8 safe string deconstruction
***************************************************************************** */

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
    int tmp = utf8_clen(reader);
    if (tmp == 1) {
      if (reader[0] == '\\') {
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
            uint16_t t =
                (((is_hex[reader[2]] - 1) << 4) | (is_hex[reader[3]] - 1)
                                                      << 8) |
                (((is_hex[reader[4]] - 1) << 4) | (is_hex[reader[5]] - 1));
            writer += utf8_from_u16(writer, t);
            reader += 6;
            break; /* from switch */
          } else
            goto invalid_escape;
        }
        case 'x': { /* test for hex notation */
          if (is_hex[reader[2]] && is_hex[reader[3]]) {
            *(writer++) =
                ((is_hex[reader[2]] - 1) << 4) | (is_hex[reader[3]] - 1);
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
          break; /* from switch */
        }
        continue;
      }
      *(writer++) = (*reader++);
    } else if (tmp == 2) {
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
    } else if (tmp == 3) {
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
    } else if (tmp == 4) {
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
      *(writer++) = (*reader++);
    } else { /* (tmp == 0) : we ignore UTF-8 invalidity*/
      *(writer++) = (*reader++);
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
JSON parsing
***************************************************************************** */

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
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
/**
 * Parses JSON, setting `pobj` to point to the new Object.
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
size_t fiobj_json2obj(fiobj_s **pobj, const void *data, size_t len) {
  fio_ls_s nesting = FIO_LS_INIT(nesting);
  const uint8_t *start;
  fiobj_s *obj;
  const uint8_t *end = (uint8_t *)data;
  const uint8_t *stop = end + len;
  while (1) {
    /* set objcet data end point to the starting endpoint */
    obj = NULL;
    start = end;
    /* skip any white space / seperators */
    while (start < stop && JSON_SEPERATOR[end[0]] == 1)
      end++;
    if (end >= stop)
      goto finish;
    start = end;

    /* test object type. tests are ordered by precedence, if one fails, the
     * other is performed. */
    if (end[0] == '{') {
      /* start an object (hash) */
      fio_ls_push(&nesting, fiobj_hash_new());
      end++;
      continue;
    }
    if (end[0] == '[') {
      /* start an array */
      fio_ls_push(&nesting, fiobj_ary_new());
      end++;
      continue;
    }
    if (end[0] == '}') {
      /* end an object (hash) */
      end++;
      obj = fio_ls_pop(&nesting);
      if (obj->type != FIOBJ_T_HASH)
        goto error;
      goto has_obj;
    }
    if (end[0] == ']') {
      /* end an array */
      end++;
      obj = fio_ls_pop(&nesting);
      if (obj->type != FIOBJ_T_ARRAY)
        goto error;
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
        while (end < stop && end[0] != '\n')
          end++;
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
      while (end < stop && end[0] != '\n')
        end++;
      continue;
    }
    if (end[0] == '-' || (end[0] >= 0 && end[0] <= '9')) {
      /* test for a number OR float */
      uint8_t invert = end[0] == '-';
      uint8_t decimal = 0;
      if (invert)
        end++;
      else
        end = start;
      while (end[0] >= '0' && end[0] <= '9')
        end++;
      if (end[0] == '.') {
        end++;
        decimal = 1;
        while (end[0] >= '0' && end[0] <= '9')
          end++;
      }
      if (end + decimal + invert == start)
        goto not_number;
      if (*end == 'e' || *end == 'E') {
        decimal = 1;
        end++;
        if (*end == '-' || *end == '+')
          end++;
        while (end[0] >= '0' && end[0] <= '9')
          end++;
        if (end[0] == '.') {
          end++;
          while (end[0] >= '0' && end[0] <= '9')
            end++;
        }
      }
      if (end - start <= 32 &&
          (JSON_SEPERATOR[*end] || *end == ']' || *end == '}')) {
        /* it's a number */
        if (decimal) {
          obj = fiobj_float_new(fio_atof((char *)start));
        } else {
          obj = fiobj_num_new(fio_atol((char *)start));
        }
        goto has_obj;
      }
    }
  not_number:
    end = start;
    /* assume object is a string (require qoutes) */
    if (start[0] != '"')
      goto error;
    start++;
    end++;
    while (end < stop && end[0] != '"') {
      end += 1 + (end[0] == '\\');
    }
    if (end >= stop)
      goto error;
    if (nesting.next->obj && nesting.next->obj->type == FIOBJ_T_HASH) {
      obj = fiobj_sym_new((char *)start, end - start);
    } else {
      obj = fiobj_str_new((char *)start, end - start);
    }
    safestr2local(obj);
    end++;

  has_obj:
    if (nesting.next == &nesting)
      goto finish_with_obj;
    if (nesting.next->obj->type == FIOBJ_T_ARRAY) {
      fiobj_ary_push(nesting.next->obj, obj);
      continue;
    } else if (nesting.next->obj->type == FIOBJ_T_SYMBOL) {
      fiobj_s *sym = fio_ls_pop(&nesting);
      fiobj_hash_set(nesting.next->obj, sym, obj);
      continue;
    } else if (nesting.next->obj->type == FIOBJ_T_HASH) {
      if (obj->type == FIOBJ_T_SYMBOL) {
        fio_ls_push(&nesting, obj);
        continue;
      }
      fio_cstr_s s = fiobj_obj2cstr(obj);
      fio_ls_push(&nesting, fiobj_sym_new(s.buffer, s.len));
      fiobj_free(obj);
      continue;
    } else
      goto error;
  }

finish:
  if (!obj)
    obj = fio_ls_pop(&nesting);
finish_with_obj:
  if (obj && nesting.next == &nesting) {
    *pobj = obj;
    return end - (uint8_t *)data;
  }
error:
  if (obj)
    fiobj_free(obj);
  while ((obj = fio_ls_pop(&nesting)))
    fiobj_free(obj);
  return 0;
}
