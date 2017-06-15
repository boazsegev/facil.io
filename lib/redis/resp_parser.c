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

#include "spnlock.inc" /* for atomic operations when reference counting. */

/* *****************************************************************************
The parser object
***************************************************************************** */

typedef struct resp_parser_s {
  resp_object_s *obj;
  size_t missing;
  uint8_t *leftovers;
  size_t llen;
  unsigned has_first_object : 1;
  unsigned exclude_pubsub : 1;
  unsigned is_pubsub : 1;
  unsigned multiplex_pubsub : 1;
} resp_parser_s;

/* *****************************************************************************
Pub/Sub state and Extension Flags
***************************************************************************** */

/** Set the parsing mode to enable and prefer pub/sub semantics. */
void resp_set_pubsub(resp_parser_pt p) { p->is_pubsub = 1; }

/** Set the parsing mode to enable the experimental pub/sub duplexing. */
void resp_enable_duplex_pubsub(resp_parser_pt p) {
  p->is_pubsub = 1;
  p->multiplex_pubsub = 1;
}

/* *****************************************************************************
Object management
+
Sometimes you just need a dirty stack.
***************************************************************************** */

typedef struct {
  volatile uintptr_t ref;
  resp_object_s *link;
} resp_objhead_s;

#define OBJHEAD(obj) (((resp_objhead_s *)(obj)) - 1)

static resp_object_s *resp_alloc_obj(enum resp_type_enum type, size_t length) {
  resp_objhead_s *head = NULL;
  switch (type) {
  /* Fallthrough */
  case RESP_ERR:
  case RESP_STRING:
    head = malloc(sizeof(*head) + sizeof(resp_string_s) + length + 1);
    *head = (resp_objhead_s){.ref = 1};
    resp_string_s *s = (void *)(head + 1); /* + 1 for NULL (safety) */
    *s = (resp_string_s){.type = type, .len = length};
    return (void *)s;
    break;
  case RESP_NULL:
  case RESP_OK:
    head = malloc(sizeof(*head) + sizeof(resp_object_s));
    *head = (resp_objhead_s){.ref = 1};
    resp_object_s *ok = (void *)(head + 1);
    *ok = (resp_object_s){.type = type};
    return (void *)ok;
    break;
  case RESP_ARRAY:
  case RESP_PUBSUB:
    head = malloc(sizeof(*head) + sizeof(resp_array_s) +
                  (sizeof(resp_object_s *) * length));
    *head = (resp_objhead_s){.ref = 1};
    resp_array_s *ar = (void *)(head + 1);
    *ar = (resp_array_s){.type = type, .len = length};
    return (void *)ar;
    break;
  case RESP_NUMBER:
    head = malloc(sizeof(*head) + sizeof(resp_number_s));
    *head = (resp_objhead_s){.ref = 1};
    resp_number_s *i = (void *)(head + 1);
    *i = (resp_number_s){.type = RESP_NUMBER, .number = length};
    return (void *)i;
    break;
  }
  return NULL;
}

static void resp_dealloc_obj(resp_object_s *obj) {
  if (!spn_sub(&OBJHEAD(obj)->ref, 1))
    free(OBJHEAD(obj));
}

inline static resp_object_s *pop_obj(resp_object_s *obj) {
  resp_object_s *ret = OBJHEAD(obj)->link;
  if (ret) {
    OBJHEAD(obj)->link = OBJHEAD(ret)->link;
  } else
    OBJHEAD(obj)->link = NULL;
  return ret;
}
inline static resp_object_s *peek_obj(resp_object_s *obj) {
  return OBJHEAD(obj)->link;
}
inline static void push_obj(resp_object_s *dest, resp_object_s *obj) {
  if (!dest)
    return;
  if (obj) {
    OBJHEAD(obj)->link = OBJHEAD(dest)->link;
  }
  OBJHEAD(dest)->link = obj;
}

/* *****************************************************************************
Object API
***************************************************************************** */

/** Allocates an RESP NULL objcet. Remeber to free when done. */
resp_object_s *resp_nil2obj(void) { return resp_alloc_obj(RESP_NULL, 0); }

/** Allocates an RESP OK objcet. Remeber to free when done. */
resp_object_s *resp_OK2obj(void) { return resp_alloc_obj(RESP_OK, 0); }

/** Allocates an RESP Error objcet. Remeber to free when done. */
resp_object_s *resp_err2obj(const void *msg, size_t len) {
  resp_string_s *o = (resp_string_s *)resp_alloc_obj(RESP_STRING, len);
  memcpy(o->string, msg, len);
  o->string[len] = 0;
  return (void *)o;
}

/** Allocates an RESP Number objcet. Remeber to free when done. */
resp_object_s *resp_num2obj(uint64_t num) {
  return resp_alloc_obj(RESP_NUMBER, num);
}

/** Allocates an RESP String objcet. Remeber to free when done. */
resp_object_s *resp_str2obj(const void *str, size_t len) {
  resp_string_s *o = (resp_string_s *)resp_alloc_obj(RESP_STRING, len);
  memcpy(o->string, str, len);
  o->string[len] = 0;
  return (void *)o;
}

/** Allocates an RESP Number objcet. Remeber to free when done. */
resp_object_s *resp_arr2obj(int argc, const resp_object_s *argv[]) {
  if (argc < 0)
    return NULL;
  resp_array_s *o = (resp_array_s *)resp_alloc_obj(RESP_ARRAY, argc);
  if (argv) {
    for (int i = 0; i < argc; i++) {
      o->array[i] = (resp_object_s *)argv[i];
    }
  }
  return (void *)o;
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
  case RESP_PUBSUB:
    ((resp_array_s *)obj)->pos = 0;
    while (((resp_array_s *)obj)->pos < ((resp_array_s *)obj)->len) {
      resp_free_object(
          ((resp_array_s *)obj)->array[((resp_array_s *)obj)->pos++]);
    }
    resp_dealloc_obj(obj);
    break;
  }
}

/* *****************************************************************************
Formatter (RESP => Memory Buffer)
***************************************************************************** */

int resp_format(resp_parser_pt p, uint8_t *dest, size_t *size,
                resp_object_s *obj) {
  /* use a stack to print it all...*/
  resp_object_s *head = obj;

  uint8_t multiplex_pubsub =
      (p && p->multiplex_pubsub && obj->type == RESP_ARRAY &&
       ((resp_array_s *)obj)->len &&
       ((resp_array_s *)obj)->array[0]->type == RESP_STRING &&
       ((((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
         'm') ||
        (((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
         'M') ||
        (((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
         '+')));

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

  do {
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
    case RESP_PUBSUB:
      safe_write1('*');
      safe_write_i(resp_obj2arr(obj)->len);
      safe_write_eol();
      {
        resp_array_s *a = resp_obj2arr(obj);
        a->pos = a->len;
        while (a->pos) {
          a->pos--;
          push_obj(head, a->array[a->pos]);
        }
      }
      continue;
      break;
    case RESP_STRING:
      safe_write1('$');
      safe_write_i(resp_obj2str(obj)->len);
      safe_write_eol();
      if (multiplex_pubsub) {
        multiplex_pubsub = 0;
        safe_write1('+');
      }
      safe_write2((resp_obj2str(obj)->string), (resp_obj2str(obj)->len));
      safe_write_eol();
      break;
    case RESP_NUMBER:
      safe_write1(':');
      safe_write_i(resp_obj2num(obj)->number);
      safe_write_eol();
      break;
    }
  } while ((obj = pop_obj(head)));
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
Parser (Memory Buffer => RESP)
***************************************************************************** */

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
      if (!p->has_first_object && p->multiplex_pubsub && pos[0] == '+' &&
          (pos[1] == '+' || pos[1] == 'm' || pos[1] == 'M')) {
        pos++;
        p->exclude_pubsub = 1;
      }
      p->has_first_object = 1;

      if (p->missing > (size_t)(end - pos)) {
        memcpy(as_s->string + as_s->len - p->missing, pos, (size_t)(end - pos));
        p->missing -= (size_t)(end - pos);
        pos = end;
        goto finish;
      } else {
        memcpy(as_s->string + as_s->len - p->missing, pos, p->missing);
        pos += p->missing + 2; /* eat the EOL */
        p->missing = 0;
        /* this part of the review logic happpens only here */
        /* once we finished with the object, we pop the stack. */
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
      p->has_first_object = 1;
      if (num == 2 && pos[0] == 'O' && pos[1] == 'K') {
        tmp = resp_alloc_obj(RESP_OK, 0);
      } else {
        tmp = resp_alloc_obj((pos[-1] == '+' ? RESP_STRING : RESP_ERR), num);
        memcpy(((resp_string_s *)tmp)->string, pos, num + 1); /* copy NUL */
      }
      p->missing = 0;
      break;

    case ':': /* number */
      p->has_first_object = 1;
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

    if (!p->obj)
      goto message_complete; /* tmp == result */

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

/* tmp == result */
message_complete:
  /* report how much the parser actually "ate" */
  *len = pos - buf;
  /* test for pub/sub semantics */
  if (p->is_pubsub && !p->exclude_pubsub) {
    if (tmp->type == RESP_ARRAY && resp_obj2arr(tmp)->len == 3 &&
        resp_obj2arr(tmp)->array[0]->type == RESP_STRING &&
        resp_obj2arr(tmp)->array[1]->type == RESP_STRING &&
        resp_obj2arr(tmp)->array[2]->type == RESP_STRING &&
        resp_obj2str(resp_obj2arr(tmp)->array[0])->len == 7 &&
        !memcmp("message", resp_obj2str(resp_obj2arr(tmp)->array[0])->string,
                7)) {
      /* PUB / SUB */
      tmp->type = RESP_PUBSUB;
    }
  }
  /* reset the parser, keeping it's state. */
  *p = (resp_parser_s){
      .is_pubsub = p->is_pubsub, .multiplex_pubsub = p->multiplex_pubsub,
  };
  /* return the result. */
  return tmp;

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
            (pop_obj(obj) == tmp && pop_obj(obj) == NULL) ? "ok." : "FAILED!");
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
      resp_format(NULL, buff, &len, obj);
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
