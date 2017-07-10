#include "fiobj_types.h"
// #include "fio2resp.h"

/* *****************************************************************************
JSON API TODO: implement parser + finish formatter
***************************************************************************** */

/* Parses JSON, returning a new Object. Remember to `fiobj_free`. */
fiobj_s *fiobj_json2obj(const char *data, size_t len);
/* Formats an object into a JSON string. Remember to `fiobj_free`. */
fiobj_s *fiobj_str_new_json(fiobj_s *);

/* *****************************************************************************
JSON UTF-8 safe string formatting
***************************************************************************** */

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
static inline int utf8_clen(const char *str) {
  if (str[0] <= 127)
    return 1;
  if ((str[0] & 192) == 192 && (str[1] & 192) == 192)
    return 2;
  if ((str[0] & 224) == 224 && (str[1] & 192) == 192 && (str[2] & 129) == 192)
    return 3;
  if ((str[0] & 240) == 240 && (str[1] & 192) == 192 && (str[2] & 129) == 192 &&
      (str[3] & 192) == 192)
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
  const char *src = s.data;
  size_t len = s.len;
  uint64_t end = obj2str(dest)->len;
  while (len) {
    /* make sure we have enough space for the largest UTF-8 char + NUL */
    if (obj2str(dest)->capa <= end + 5)
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
      int t = utf8_from_u16((uint8_t *)(obj2str(dest)->str + end), *(src++));
      src += t;
      len -= t;
    }
  }
  obj2str(dest)->len = end;
  obj2str(dest)->str[obj2str(dest)->len] = 0;
}

/* *****************************************************************************
JSON UTF-8 safe string deconstruction
***************************************************************************** */

/* Converts a JSON friendly String to a binary String.
 *
 * Also deals with the non-standard oct ("\77") and hex ("\xFF") notations
 */
static void safestr2local(fiobj_s *str) {
  uint8_t *end = (uint8_t *)obj2str(str)->str + obj2str(str)->len;
  uint8_t *reader = (uint8_t *)obj2str(str)->str;
  uint8_t *writer = (uint8_t *)obj2str(str)->str;
  while (reader < end) {
    int tmp = utf8_clen((char *)reader);
    if (tmp == 1) {
      if (reader[0] == '\\') {
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
  obj2str(str)->len = (uintptr_t)writer - (uintptr_t)obj2str(str)->str;
  obj2str(str)->str[obj2str(str)->len] = 0;
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
    fiobj_str_write(data->buffer, "NULL", 4);
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
  case FIOBJ_T_FILE:
  case FIOBJ_T_IO:
  case FIOBJ_T_NULL:
    fiobj_str_write(data->buffer, "NULL", 4);
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
fiobj_s *fiobj_str_new_json(fiobj_s *obj) {
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
