/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.

Copyright refers to the parser, not the protocol.
*/
#include "resp_parser.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* *****************************************************************************
Object management
+
Sometimes you just need a dirty stack.
***************************************************************************** */

static resp_object_s *resp_alloc_obj(enum resp_type_enum type, size_t length) {
  void **mem = NULL;
  switch (type) {
  /* Fallthrough */
  case RESP_ERR:
  case RESP_STRING:
    mem = malloc(sizeof(void *) + sizeof(resp_string_s) + length + 1);
    mem[0] = NULL;
    resp_string_s *s = (void *)(mem + 1);
    *s = (resp_string_s){.type = type, .len = length};
    return (void *)s;
    break;
  case RESP_NULL:
  case RESP_OK:
    mem = malloc(sizeof(void *) + sizeof(resp_object_s));
    mem[0] = NULL;
    resp_object_s *ok = (void *)(mem + 1);
    *ok = (resp_object_s){.type = type};
    return (void *)ok;
    break;
  case RESP_ARRAY:
    mem = malloc(sizeof(void *) + sizeof(resp_array_s) +
                 (sizeof(resp_object_s *) * length));
    mem[0] = NULL;
    resp_array_s *ar = (void *)(mem + 1);
    *ar = (resp_array_s){.type = RESP_ARRAY, .len = length};
    return (void *)ar;
    break;
  case RESP_NUMBER:
    mem = malloc(sizeof(void *) + sizeof(resp_number_s));
    mem[0] = NULL;
    resp_number_s *i = (void *)(mem + 1);
    *i = (resp_number_s){.type = RESP_NUMBER, .number = length};
    return (void *)i;
    break;
  }
  return NULL;
}
static void resp_dealloc_obj(resp_object_s *obj) {
  void **mem = (void **)obj;
  free(mem - 1);
}

inline static resp_object_s *pop_obj(resp_object_s *obj) {
  void **mem = (void **)obj;
  return mem[-1];
}
inline static void push_obj(resp_object_s *dest, resp_object_s *obj) {
  if (!dest)
    return;
  void **mem = (void **)dest;
  mem[-1] = obj;
}

/* *****************************************************************************
Parser
***************************************************************************** */

typedef struct resp_parser_s {
  resp_object_s *obj;
  size_t missing;
  uint8_t *leftovers;
  size_t llen;
} resp_parser_s;

/** create the parser */
resp_parser_pt resp_parser_new(void) {
  resp_parser_s *p = malloc(sizeof(*p));
  *p = (resp_parser_s){.obj = NULL};
  return p;
}

/** free the parser and it's resources. */
void resp_parser_destroy(resp_parser_pt p) {
  if (p->leftovers)
    free(p->leftovers);
  if (p->obj)
    resp_free_object(p->obj);
  free(p);
}

/** frees an object returned from the parser. */
void resp_free_object(resp_object_s *obj) {
  switch (obj->type) {
  /* Fallthrough */
  case RESP_ERR:
  case RESP_STRING:
  case RESP_NUMBER:
  case RESP_NULL:
  case RESP_OK:
    resp_dealloc_obj(obj);
    break;
  case RESP_ARRAY:
    ((resp_array_s *)obj)->pos = 0;
    while (((resp_array_s *)obj)->pos < ((resp_array_s *)obj)->len) {
      resp_free_object(
          ((resp_array_s *)obj)->array[((resp_array_s *)obj)->pos++]);
    }
    resp_dealloc_obj(obj);
    break;
  }
}

/**
 * Feed the parser with data.
 *
 * Returns any fully parsed object / reply (often an array, but not always) or
 * NULL (needs more data / error).
 *
 * If an error occurs, the `pos` pointer is set to -1, otherwise it's updated
 * with the amount of data consumed.
 *
 * Partial consumption is possible when multiple replys were available in the
 * buffer. Otherwise the parser will consume the whole of the buffer.
 *
 */
resp_object_s *resp_parser_feed(resp_parser_pt p, uint8_t *buf, size_t *len) {
#define as_s ((resp_string_s *)p->obj)
#define as_n ((resp_number_s *)p->obj)
#define as_a ((resp_array_s *)p->obj)
  resp_object_s *tmp;
  uint8_t *pos = buf;
  uint8_t *eol = pos;
  uint8_t *end = pos + (*len);
  int64_t num;
  uint8_t flag;

  while (pos < end) {
    /* Eat up misaligned newline markers. */
    if (pos[0] == '\r' || pos[0] == '\n') {
      pos++;
      continue;
    }

    if (p->missing && p->obj->type == RESP_STRING) {
      if (p->missing > *len) {
        memcpy(as_s->string + as_s->len - p->missing, pos, *len);
        pos = end;
        p->missing -= *len;
        goto finish;
      } else {
        memcpy(as_s->string + as_s->len - p->missing, pos, p->missing);
        pos += p->missing + 2; /* eat the EOL */
        p->missing = 0;
        /* this part of the review logic happpens only here */
        tmp = p->obj;
        p->obj = pop_obj(p->obj);
        goto review;
      }
    }

    /* seek non-String object. */
    while (eol < end - 1) {
      if (eol[0] == '\r' && eol[1] == '\n')
        goto found_eol;
      eol++;
    }

    /* no EOL, we can't parse. */
    tmp = realloc(p->leftovers, p->llen + end - pos);
    if (!tmp)
      goto error;
    p->leftovers = (void *)tmp;
    memcpy(p->leftovers + p->llen, pos, end - pos);
    if (p->llen >= 131072) /* IMHO: simple objects are smaller than 128Kib. */
      goto error;
    goto finish;

  found_eol:
    *eol = 0;
    num = eol - pos - 1;
    if (p->leftovers) {
      tmp = realloc(p->leftovers, p->llen + num + 1);
      if (!tmp)
        goto error;
      p->leftovers = (void *)tmp;
      memcpy(p->leftovers + p->llen, pos, num + 1); /* copy the NUL byte. */
      pos = p->leftovers;
    }

    switch (*pos++) {
    case '*': /* Array */
      num = 0;
      if (*pos == '-') {
        goto error;
      }
      while (*pos) {
        if (*pos < '0' || *pos > '9')
          goto error;
        num = (num * 10) + (*pos - '0');
        pos++;
      }
      tmp = resp_alloc_obj(RESP_ARRAY, num);
      p->missing = num;
      break;

    case '$': /* String */
      if (*pos == '-') {
        if (pos[1] == '1') {
          /* NULL Object */
          tmp = resp_alloc_obj(RESP_NULL, -1);
          p->missing = 0;
        } else
          goto error;
      } else {
        num = 0;
        while (*pos) {
          if (*pos < '0' || *pos > '9')
            goto error;
          num = (num * 10) + (*pos - '0');
          pos++;
        }
        tmp = resp_alloc_obj(RESP_STRING, num);
        p->missing = num;
      }
      break;

    case '+': /* Simple String */
    case '-': /* Error String */
      if (num == 2 && pos[0] == 'O' && pos[1] == 'K') {
        tmp = resp_alloc_obj(RESP_OK, 0);
      } else {
        tmp = resp_alloc_obj((pos[-1] == '+' ? RESP_STRING : RESP_ERR), num);
        memcpy(((resp_string_s *)tmp)->string, pos, num + 1); /* copy NUL */
      }
      p->missing = 0;
      break;

    case ':': /* number */
      num = 0;
      flag = 0;
      if (*pos == '-') {
        flag = 1;
        pos++;
      }
      while (*pos) {
        if (*pos < '0' || *pos > '9')
          goto error;
        num = (num * 10) + (*pos - '0');
        pos++;
      }
      if (flag)
        num = num * -1;
      tmp = resp_alloc_obj(RESP_NUMBER, num);
      p->missing = 0;
      break;
    default:
      fprintf(stderr, "ERROR RESP Parser: input prefix unknown\n");
      goto error;
    }

    if (p->leftovers) {
      free(p->leftovers);
      p->leftovers = NULL;
      p->llen = 0;
    }
    pos = eol + 2;

    if (p->missing) {
      push_obj(tmp, p->obj);
      p->obj = tmp;
      if (pos >= end)
        goto finish;
      else
        continue;
    }

  /* tmp == recently completed item ; p->obj == awaiting completion */

  review:

    if (!p->obj) {
      *len = pos - buf;
      *p = (resp_parser_s){.obj = NULL};
      return tmp;
    }

    if (p->obj->type != RESP_ARRAY) {
      fprintf(stderr, "ERROR RESP Parser: possible bug in the parser."
                      "What was the imput?\n");
      goto error; /* no nested items unless they're in an array, right? */
    }
    as_a->array[as_a->pos++] = tmp;
    p->missing = as_a->len - as_a->pos;

    if (p->missing) {
      if (pos >= end)
        goto finish;
      else
        continue;
    }

    tmp = p->obj;
    p->obj = pop_obj(p->obj);
    goto review;
  }
#undef as_s
#undef as_n
#undef as_a

finish:
  return NULL;

error:
  while ((tmp = p->obj)) {
    p->obj = pop_obj(tmp);
    resp_dealloc_obj(tmp);
  }
  if (p->leftovers) {
    free(p->leftovers);
    *p = (resp_parser_s){.llen = 0};
  }
  *len = 0;
  return NULL;
}

/**
 * Formats a RESP object back into a string.
 */
int resp_format(uint8_t *dest, size_t *size, resp_object_s *obj) {
  push_obj(obj, NULL);
  size_t limit = *size;
  *size = 0;
#define safe_write_eol()                                                       \
  if ((*size += 2) <= limit) {                                                 \
    *dest++ = '\r';                                                            \
    *dest++ = '\n';                                                            \
  }
#define safe_write1(data)                                                      \
  if (++(*size) <= limit) {                                                    \
    *dest++ = (data);                                                          \
  }
#define safe_write2(data, len)                                                 \
  do {                                                                         \
    *size += (len);                                                            \
    if (*size <= limit) {                                                      \
      memcpy(dest, (data), (len));                                             \
      dest += (len);                                                           \
    }                                                                          \
  } while (0)
#define safe_write_i(i)                                                        \
  do {                                                                         \
    size_t t2 = (i);                                                           \
    if (i < 0) {                                                               \
      safe_write1('-');                                                        \
      t2 = ((i) * -1);                                                         \
    }                                                                          \
    size_t len = (size_t)log10((double)(t2)) + 1;                              \
    *size += (len);                                                            \
    if (*size <= limit) {                                                      \
      size_t t1 = len;                                                         \
      size_t t3 = t2 / 10;                                                     \
      while (t1--) {                                                           \
        dest[t1] = '0' + (t2 - (t3 * 10));                                     \
        t2 = t3;                                                               \
        t3 = t3 / 10;                                                          \
      }                                                                        \
      dest += len;                                                             \
    }                                                                          \
  } while (0)

  while (obj) {
    switch (obj->type) {
    case RESP_ERR:
      safe_write1('-');
      safe_write2((resp_obj2str(obj)->string), (resp_obj2str(obj)->len));
      safe_write_eol();
      break;
    case RESP_NULL:
      safe_write2("$-1\r\n", (resp_obj2str(obj)->len));
      break;
    case RESP_OK:
      safe_write2("+OK\r\n", 5);
    case RESP_ARRAY:
      safe_write1('*');
      safe_write_i(resp_obj2arr(obj)->len);
      safe_write_eol();
      {
        resp_array_s *a = resp_obj2arr(obj);
        a->pos = a->len;
        obj = NULL;
        while (a->pos) {
          a->pos--;
          push_obj(a->array[a->pos], obj);
          obj = a->array[a->pos];
        }
      }
      continue;
      break;
    case RESP_STRING:
      safe_write1('$');
      safe_write_i(resp_obj2str(obj)->len);
      safe_write_eol();
      safe_write2((resp_obj2str(obj)->string), (resp_obj2str(obj)->len));
      safe_write_eol();
      break;
    case RESP_NUMBER:
      safe_write1(':');
      safe_write_i(resp_obj2num(obj)->number);
      safe_write_eol();
      break;
    }
    obj = pop_obj(obj);
  }
  if (*size < limit) {
    *dest = '0';
    return 0;
  }
  if (*size == limit)
    return 0;
  return -1;
#undef safe_write_eol
#undef safe_write1
#undef safe_write2
#undef safe_write_i
}

/* *****************************************************************************
Tests
***************************************************************************** */
#ifdef DEBUG
#include <inttypes.h>
#include <stdint.h>
void resp_test(void) {
  uint8_t b_null[] = "$-1\r\n";
  uint8_t b_ok[] = "+OK\r\n";
  uint8_t b_num[] = ":13\r\n";
  uint8_t b_neg_num[] = ":-13\r\n";
  uint8_t b_err[] = "-ERR: or not :-)\r\n";
  uint8_t b_str[] = "$19\r\nthis is a string :)\r\n";
  uint8_t b_longer[] = "*6\r\n"
                       ":1\r\n"
                       ":2\r\n"
                       ":3\r\n"
                       ":4\r\n"
                       ":-5794\r\n"
                       "$6\r\n"
                       "foobar\r\n";
  resp_parser_pt parser = resp_parser_new();
  size_t len;

  resp_object_s *obj;
  {
    obj = resp_alloc_obj(RESP_OK, 0);
    resp_object_s *tmp = resp_alloc_obj(RESP_NULL, 0);
    push_obj(obj, tmp);
    fprintf(stderr, "* Push/Pop test: %s\n",
            (pop_obj(obj) == tmp) ? "ok." : "FAILED!");
    resp_dealloc_obj(obj);
    resp_dealloc_obj(tmp);
  }

  len = sizeof(b_null) - 1;
  obj = resp_parser_feed(parser, b_null, &len);
  if (obj && obj->type == RESP_NULL) {
    fprintf(stderr, "* NULL recognized, pos == %lu / %lu\n", len,
            sizeof(b_null) - 1);
    resp_free_object(obj);
  } else
    fprintf(stderr, "* NULL FAILED\n");

  len = sizeof(b_ok) - 1;
  obj = resp_parser_feed(parser, b_ok, &len);
  if (obj && obj->type == RESP_OK) {
    fprintf(stderr, "* OK recognized\n");
    resp_free_object(obj);
  } else
    fprintf(stderr, "* OK FAILED\n");

  len = sizeof(b_err) - 1;
  obj = resp_parser_feed(parser, b_err, &len);
  if (obj && obj->type == RESP_ERR) {
    fprintf(stderr, "* ERR / Simple String recognized: %s\n",
            resp_obj2str(obj)->string);
    resp_free_object(obj);
  } else
    fprintf(stderr, "* ERR / Simple String FAILED (type %d)\n",
            obj ? obj->type : -1);

  len = sizeof(b_num) - 1;
  obj = resp_parser_feed(parser, b_num, &len);
  if (obj && obj->type == RESP_NUMBER) {
    fprintf(stderr, "* Number recognized: %lld\n", resp_obj2num(obj)->number);
    resp_free_object(obj);
  } else
    fprintf(stderr, "* Number FAILED\n");

  len = sizeof(b_neg_num) - 1;
  obj = resp_parser_feed(parser, b_neg_num, &len);
  if (obj && obj->type == RESP_NUMBER) {
    fprintf(stderr, "* Negative Number recognized: %lld\n",
            resp_obj2num(obj)->number);
    resp_free_object(obj);
  } else
    fprintf(stderr, "* Negative Number FAILED\n");

  len = sizeof(b_str) - 1;
  obj = resp_parser_feed(parser, b_str, &len);
  if (obj && obj->type == RESP_STRING) {
    fprintf(stderr, "* String recognized: %s\n", resp_obj2str(obj)->string);
    resp_free_object(obj);
  } else
    fprintf(stderr, "* String FAILED\n");

  len = sizeof(b_longer) - 1;
  obj = resp_parser_feed(parser, b_longer, &len);
  if (obj && obj->type == RESP_ARRAY) {
    fprintf(stderr, "* Array head recognized: (%lu)\n", resp_obj2arr(obj)->len);
    resp_object_s *tmp;
    for (size_t i = 0; i < resp_obj2arr(obj)->len; i++) {
      tmp = resp_obj2arr(obj)->array[i];
      if (resp_obj2arr(tmp)) {
        fprintf(stderr, "Item %lu is an .... array?!\n", i);
      }
      if (resp_obj2str(tmp)) {
        fprintf(stderr, "Item %lu is a String %s\n", i,
                resp_obj2str(tmp)->string);
      }
      if (resp_obj2num(tmp)) {
        fprintf(stderr, "Item %lu is a Number %" PRIi64 "\n", i,
                resp_obj2num(tmp)->number);
      }
    }
    {
      uint8_t buff[48] = {0};
      size_t len = 47;
      resp_format(buff, &len, obj);
      fprintf(stderr,
              "* In RESP format, it should take %lu bytes like so:\n%s\n", len,
              buff);
    }
    resp_free_object(obj);
  } else {
    fprintf(stderr, "* Array FAILED (type == %d)\n", obj ? obj->type : -1);
  }
  resp_parser_destroy(parser);
  return;
}

#endif
