/*
Copyright: Boaz Segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.

Copyright refers to the parser, not the protocol.
*/

/* for atomic operations when reference counting: `spn_add` and `spn_sub` */
#include "spnlock.inc"

#include "resp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* *****************************************************************************
Error Reporting
***************************************************************************** */
#ifdef DEBUG
#define REPORT(...) fprintf(stderr, __VA_ARGS__);
#else
#define REPORT(...)
#endif
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
  unsigned multiplex_pubsub : 1;
} resp_parser_s;

/* *****************************************************************************
Pub/Sub state and Extension Flags
***************************************************************************** */

/** Set the parsing mode to enable the experimental pub/sub duplexing. */
void resp_enable_duplex_pubsub(resp_parser_pt p) { p->multiplex_pubsub = 1; }

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
    /* + 1 for NULL (safety) */
    head = malloc(sizeof(*head) + sizeof(resp_string_s) + length + 1);
    *head = (resp_objhead_s){.ref = 1};
    resp_string_s *s = (void *)(head + 1);
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

static int resp_dealloc_obj(resp_parser_pt p, resp_object_s *obj, void *arg) {
  if (obj && !spn_sub(&OBJHEAD(obj)->ref, 1))
    free(OBJHEAD(obj));
  (void)arg;
  (void)p;
  return 0;
}

inline static resp_object_s *pop_obj(resp_object_s *obj) {
  resp_object_s *ret = OBJHEAD(obj)->link;
  if (ret) {
    OBJHEAD(obj)->link = OBJHEAD(ret)->link;
    OBJHEAD(ret)->link = NULL;
  } else
    OBJHEAD(obj)->link = NULL;
  return ret;
}

inline static void push_obj(resp_object_s *dest, resp_object_s *obj) {
  if (!dest)
    return;
  if (obj) {
    OBJHEAD(obj)->link = OBJHEAD(dest)->link;
  }
  OBJHEAD(dest)->link = obj;
}

static int mock_obj_tks(resp_parser_pt p, resp_object_s *obj, void *arg) {
  return 0;
  (void)arg;
  (void)p;
  (void)obj;
}

/** Performs a task on each object. Protects from loop-backs. */
size_t resp_obj_each(resp_parser_pt p, resp_object_s *obj,
                     int (*task)(resp_parser_pt p, resp_object_s *obj,
                                 void *arg),
                     void *arg) {

  if (!obj)
    return 0;
  if (!task)
    task = mock_obj_tks;
  /* make sure the object isn't "dirty"*/
  OBJHEAD(obj)->link = NULL;
  /* a searchable store. */
  resp_objhead_s performed[3] = {{.link = NULL}}; /* a memory lump. */
  resp_object_s *past = (resp_object_s *)(performed + 1);
  resp_object_s *next = (resp_object_s *)(performed + 2);
  OBJHEAD(past)->link = NULL;
  OBJHEAD(next)->link = NULL; /* a straight line */
  push_obj(next, obj);
  resp_object_s *err = NULL; /* might host an error object... if we need one. */
  resp_object_s *tmp;
  size_t count = 0;
  obj = pop_obj(next);
  while (obj) {
    count++;
    if (obj->type == RESP_ARRAY || obj->type == RESP_PUBSUB)
      goto is_array;

  perform:
    /* in case the task is to free `obj`, "pop" the next object first */
    tmp = obj;
    obj = pop_obj(next);
    if (task(p, tmp, arg) == -1)
      break;
    continue;

  is_array:
    /* test for loop-back. */
    tmp = OBJHEAD(past)->link;
    while (tmp) {
      if (obj == tmp)
        goto skip;
      tmp = OBJHEAD(tmp)->link;
    }
    /* add current object to the loop back stack */
    push_obj(past, obj);

    /* Add all array items to the stack. */
    resp_obj2arr(obj)->pos = resp_obj2arr(obj)->len;
    while (resp_obj2arr(obj)->pos != 0) {
      resp_obj2arr(obj)->pos--;
      if (!resp_obj2arr(obj)
               ->array[resp_obj2arr(obj)->pos]) /* fill any holes...? */
        resp_obj2arr(obj)->array[resp_obj2arr(obj)->pos] = resp_nil2obj();
      push_obj(next, resp_obj2arr(obj)->array[resp_obj2arr(obj)->pos]);
    }
    goto perform;

  skip:
    if (task != resp_dealloc_obj) {
      if (!err)
        err =
            resp_err2obj("ERR: infinit loopback detected. Can't process.", 46);
      obj = err;
    } else
      obj = resp_err2obj("ERR: infinit loopback detected. Can't process.", 46);
    goto perform;
  }

  /* cleaup */
  if (task != resp_dealloc_obj) {
    if (err)
      resp_dealloc_obj(NULL, err, NULL);
    while ((tmp = pop_obj(past)))
      ;
  }
  return count;
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

/** Allocates an RESP Array objcet. Remeber to free when done. */
resp_object_s *resp_arr2obj(int argc, resp_object_s *argv[]) {
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
  resp_obj_each(NULL, obj, resp_dealloc_obj, NULL);
}

static int resp_dup_obj_task(resp_parser_pt p, resp_object_s *obj, void *arg) {
  spn_add(&OBJHEAD(obj)->ref, 1);
  return 0;
  (void)p;
  (void)arg;
}

/** Duplicates an object by increasing it's reference count. */
resp_object_s *resp_dup_object(resp_object_s *obj) {
  resp_obj_each(NULL, obj, resp_dup_obj_task, NULL);
  return obj;
}

/* *****************************************************************************
Formatter (RESP => Memory Buffer)
***************************************************************************** */
struct resp_format_s {
  uint8_t *dest;
  size_t *size;
  size_t limit;
  uintptr_t multiplex_pubsub;
  int err;
};

static int resp_format_task(resp_parser_pt p, resp_object_s *obj, void *s_) {
  struct resp_format_s *s = s_;

#define safe_write_eol()                                                       \
  if ((*s->size += 2) <= s->limit) {                                           \
    *s->dest++ = '\r';                                                         \
    *s->dest++ = '\n';                                                         \
  }
#define safe_write1(data)                                                      \
  if (++(*s->size) <= s->limit) {                                              \
    *s->dest++ = (data);                                                       \
  }
#define safe_write2(data, len)                                                 \
  do {                                                                         \
    *s->size += (len);                                                         \
    if (*s->size <= s->limit) {                                                \
      memcpy(s->dest, (data), (len));                                          \
      s->dest += (len);                                                        \
    }                                                                          \
  } while (0)
#define safe_write_i(i)                                                        \
  do {                                                                         \
    int64_t t2 = (i);                                                          \
    if ((t2) < 0) {                                                            \
      safe_write1('-');                                                        \
      t2 = (t2 * -1);                                                          \
    }                                                                          \
    size_t len = t2 ? (((size_t)log10((double)(t2))) + 1) : 1;                 \
    *s->size += (len);                                                         \
    if (*s->size <= s->limit) {                                                \
      int64_t t1 = len;                                                        \
      int64_t t3 = t2 / 10;                                                    \
      while (t1--) {                                                           \
        s->dest[t1] = '0' + (t2 - (t3 * 10));                                  \
        t2 = t3;                                                               \
        t3 = t3 / 10;                                                          \
      }                                                                        \
      s->dest += len;                                                          \
    }                                                                          \
  } while (0)

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
    break;
  case RESP_ARRAY:
  case RESP_PUBSUB:
    safe_write1('*');
    safe_write_i(resp_obj2arr(obj)->len);
    safe_write_eol();
    break;
  case RESP_STRING:
    safe_write1('$');
    safe_write_i(s->multiplex_pubsub ? (resp_obj2str(obj)->len + 1)
                                     : resp_obj2str(obj)->len);
    safe_write_eol();
    if (s->multiplex_pubsub) {
      s->multiplex_pubsub = 0;
      safe_write1('+');
    }
    safe_write2((resp_obj2str(obj)->string), (resp_obj2str(obj)->len));
    safe_write_eol();
    break;
  case RESP_NUMBER:
    safe_write1(':');
    safe_write_i((resp_obj2num(obj)->number));
    safe_write_eol();
    break;
  }
  return 0;
  (void)p;
}
#undef safe_write_eol
#undef safe_write1
#undef safe_write2
#undef safe_write_i

int resp_format(resp_parser_pt p, uint8_t *dest, size_t *size,
                resp_object_s *obj) {
  struct resp_format_s arg = {
      .dest = dest,
      .size = size,
      .limit = *size,
      .multiplex_pubsub =
          (p && p->multiplex_pubsub && obj->type == RESP_ARRAY &&
           ((resp_array_s *)obj)->len &&
           ((resp_array_s *)obj)->array[0]->type == RESP_STRING &&
           ((((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
             'm') ||
            (((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
             'M') ||
            (((resp_string_s *)((resp_array_s *)obj)->array[0])->string[0] ==
             '+')))};
  *size = 0;
  resp_obj_each(p, obj, resp_format_task, &arg);
  if (*arg.size < arg.limit) {
    *arg.dest = 0;
    return 0;
  }
  if (*arg.size == arg.limit)
    return 0;
  return -1;
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

static inline void reset_parser(resp_parser_pt p) {
  resp_object_s *tmp;
  while ((tmp = p->obj)) {
    p->obj = OBJHEAD(p->obj)->link;
    resp_free_object(tmp);
  }
  if (p->leftovers)
    free(p->leftovers);
  if (p->obj)
    resp_free_object(p->obj);
  *p = (resp_parser_s){.obj = NULL, .multiplex_pubsub = p->multiplex_pubsub};
}

/** free the parser and it's resources. */
void resp_parser_destroy(resp_parser_pt p) {
  reset_parser(p);
  free(p);
}

void resp_parser_clear(resp_parser_pt p) {
  reset_parser(p);
  *p = (resp_parser_s){.obj = NULL};
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
  resp_object_s *tmp;
  uint8_t *pos = buf;
  uint8_t *eol = NULL;
  uint8_t *end = pos + (*len);
  int64_t num;
  uint8_t flag;
restart:

  // while (pos < end && (*pos == '\r' || *pos == '\n'))
  //   pos++; /* consume empty EOL markers. */

  if (p->missing && p->obj->type == RESP_STRING) {
    /* test for pub/sub duplexing extension */
    if (!p->has_first_object && p->multiplex_pubsub && pos[0] == '+' &&
        (pos[1] == '+' || pos[1] == 'm' || pos[1] == 'M')) {
      pos++;
      p->exclude_pubsub = 1;
      p->missing--;
    }
    p->has_first_object = 1;
    /* just fill the string with missing bytes */
    if (p->missing > (size_t)(end - pos)) {
      memcpy(resp_obj2str(p->obj)->string + resp_obj2str(p->obj)->len -
                 p->missing,
             pos, (size_t)(end - pos));
      p->missing -= (size_t)(end - pos);
      pos = end;
      goto finish;
    } else {
      memcpy(resp_obj2str(p->obj)->string + resp_obj2str(p->obj)->len -
                 p->missing,
             pos, p->missing);
      resp_obj2str(p->obj)->string[resp_obj2str(p->obj)->len] = 0; /* set NUL */
      pos += p->missing + 2; /* eat the EOL */
      /* set state to equal a freash, complete object */
      p->missing = 0;
      tmp = p->obj;
      p->obj = OBJHEAD(p->obj)->link;
      goto review;
    }
  }

  /* seek EOL */
  eol = pos;
  while (eol < end - 1) {
    if (eol[0] == '\r' && eol[1] == '\n')
      goto found_eol;
    eol++;
  }

  /* no EOL, we can't parse. */
  if (p->llen + (end - pos) >=
      131072) { /* IMHO: simple objects are smaller than 128Kib. */
    REPORT("ERROR: (RESP parser) single line object too long. "
           "128Kib limit for simple strings, numbers exc'.\n");
    eol = NULL;
    goto error;
  }
  {
    void *tmp = realloc(p->leftovers, p->llen + (end - pos));
    if (!tmp) {
      fprintf(stderr, "ERROR: (RESP parser) Couldn't allocate memory.\n");
      goto error;
    }
    p->leftovers = (void *)tmp;
    memcpy(p->leftovers + p->llen, pos, end - pos);
    p->llen += (end - pos);
  }
  goto finish;

found_eol:

  *eol = 0; /* mark with NUL */

  num = eol - pos - 1;
  /* route `pos` to p->leftovers, if data was fragmented. */
  if (p->leftovers) {
    void *tmp = realloc(p->leftovers, p->llen + num + 1);
    if (!tmp) {
      REPORT("ERROR: (RESP parser) Couldn't allocate memory.\n");
      goto error;
    }
    p->leftovers = (void *)tmp;
    memcpy(p->leftovers + p->llen, pos, num + 1); /* copy the NUL byte. */
    p->llen += (end - pos);
    pos = p->leftovers;
  }

  /* ** let's actually parse something ** put new objects in `tmp` ** */
  switch (*pos++) {
  case '*': /* Array */
    num = 0;
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
      } else {
        REPORT("ERROR: (RESP parser) Bulk String input error.\n");
        goto error;
      }
    } else {
      num = 0;
      while (*pos) {
        if (*pos < '0' || *pos > '9') {
          REPORT("ERROR: (RESP parser) Bulk String length error.\n");
          goto error;
        }
        num = (num * 10) + (*pos - '0');
        pos++;
      }
      tmp = resp_alloc_obj(RESP_STRING, num);
      p->missing = num;
      if (!num) {
        *eol = '\r';
        eol += 2;
      }
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
      if (*pos < '0' || *pos > '9') {
        REPORT("ERROR: (RESP parser) input error.\n");
        goto error;
      }
      num = (num * 10) + (*pos - '0');
      pos++;
    }
    if (flag)
      num = num * -1;
    tmp = resp_alloc_obj(RESP_NUMBER, num);
    p->missing = 0;
    break;
  default:
    REPORT("ERROR: (RESP Parser) input prefix unknown\n");
    goto error;
  }
  /* replace parsing marker */
  pos = eol + 2;
  /* un-effect the EOL */
  *eol = '\r';
  eol = NULL;

  /* clear the buffer used to handle fragmented transmissions. */
  if (p->leftovers) {
    free(p->leftovers);
    p->leftovers = NULL;
    p->llen = 0;
  }

review:
  /* handle object rotation and nesting */
  if (p->missing) {
    /* tmp missing data: link and step into new object (nesting objects) */
    OBJHEAD(tmp)->link = p->obj;
    p->obj = tmp;
    tmp = NULL;
    goto restart;
  } else if (p->obj) {
    if (p->obj->type != RESP_ARRAY) {
      /* Nesting of objects can only be performed by RESP_ARRAY objects. */
      fprintf(stderr, "ERROR: (RESP Parser) internal error - "
                      "objects can only be nested within arrays.\n");
      goto error;
    }
    resp_obj2arr(p->obj)->array[resp_obj2arr(p->obj)->pos++] = tmp;
    p->missing = resp_obj2arr(p->obj)->len - resp_obj2arr(p->obj)->pos;
    if (p->missing)
      goto restart; /* collect more objects. */
    /* un-nest */
    tmp = p->obj;
    p->obj = OBJHEAD(p->obj)->link;
    goto review;
  }

  /* tmp now holds the top-most object, it's missing no data, we're done */

  /* test for pub/sub semantics */
  if (!p->exclude_pubsub) {
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
  reset_parser(p);
  /* report how much the parser actually "ate" */
  *len = pos - buf;
  /* return the result. */
  return tmp;

error:
  reset_parser(p);
  *len = 0;
  if (eol)
    *eol = '\r';

finish:
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
  uint8_t b_longer[] = "*8\r\n"
                       ":1\r\n"
                       ":2\r\n"
                       ":3\r\n"
                       ":4\r\n"
                       ":-5794\r\n"
                       "$6\r\n"
                       "foobar\r\n"
                       "$6\r\n"
                       "barfoo\r\n"
                       ":4\r\n";
  resp_parser_pt parser = resp_parser_new();
  size_t len;

  resp_object_s *obj;

  fprintf(stderr, "* OBJHEAD test %s\n",
          (OBJHEAD(((resp_objhead_s *)NULL) + 1) == NULL) ? "passed"
                                                          : "FAILED");

  {
    obj = resp_OK2obj();
    resp_object_s *tmp = resp_nil2obj();
    push_obj(obj, tmp);
    fprintf(stderr, "* Push/Pop test: %s\n",
            (pop_obj(obj) == tmp && pop_obj(obj) == NULL) ? "ok." : "FAILED!");
    resp_free_object(obj);
    resp_free_object(tmp);
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
            obj ? (int)obj->type : -1);

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

  {
    uint8_t empty_str[] = "$0\r\n\r\n";
    len = sizeof(empty_str) - 1;
    obj = resp_parser_feed(parser, empty_str, &len);
    if (obj && obj->type == RESP_STRING) {
      fprintf(stderr, "* Empty String recognized: %s\n",
              resp_obj2str(obj)->string);
      resp_free_object(obj);
    } else
      fprintf(stderr, "* Empty String FAILED\n");
  }
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
      fprintf(stderr, "found %lu objects\n",
              resp_obj_each(NULL, obj, NULL, NULL));
      uint8_t buff[128] = {0};
      size_t len = 127;
      resp_format(NULL, buff, &len, obj);
      fprintf(stderr,
              "* In RESP format, it should take %lu bytes like so:\n%s\n", len,
              buff);
    }
    resp_free_object(obj);
  } else {
    fprintf(stderr, "* Array FAILED (type == %d)\n", obj ? (int)obj->type : -1);
  }
  {
    // uint8_t buff[128] = {0};
    // size_t len = 128;
    // resp_object_s *arr1 = resp_arr2obj(1, NULL);
    // resp_object_s *arr2 = resp_arr2obj(1, NULL);
    // resp_obj2arr(arr1)->array[0] = resp_dup_object(arr2);
    // resp_obj2arr(arr2)->array[0] = resp_dup_object(arr1);
    // resp_format(NULL, buff, &len, arr1);
    // fprintf(stderr, "* Loopback test:\n%s\n", buff);
    // resp_free_object(arr1);
    // resp_free_object(arr2);
  }

  resp_parser_destroy(parser);
  return;
}

#endif
