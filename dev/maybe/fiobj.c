/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "spnlock.inc"

#include "fio_hash_table.h"
#include "fiobj.h"

#include <math.h>
#include <string.h>
#include <unistd.h>

/* *****************************************************************************
Object types
***************************************************************************** */

/* Number */
typedef struct {
  fiobj_type_en type;
  int64_t i;
} fio_num_s;

/* Float */
typedef struct {
  fiobj_type_en type;
  double f;
} fio_float_s;

/* String */
typedef struct {
  fiobj_type_en type;
  uint64_t len;
  char str[];
} fio_str_s;

/* Symbol */
typedef struct {
  fiobj_type_en type;
  uint64_t hash;
  uint64_t len;
  char str[];
} fio_sym_s;

/* IO */
typedef struct {
  fiobj_type_en type;
  intptr_t fd;
} fio_io_s;

/* File */
typedef struct {
  fiobj_type_en type;
  FILE *f;
} fio_file_s;

/* Array */
typedef struct {
  fiobj_type_en type;
  uint64_t start;
  uint64_t end;
  uint64_t capa;
  fiobj_s **arry;
} fio_ary_s;

/* Hash */
typedef struct {
  fiobj_type_en type;
  fio_ht_s h;
} fio_hash_s;

/* Hash node */
typedef struct {
  fiobj_type_en type;
  fiobj_s *name;
  fiobj_s *obj;
  fio_ht_node_s node;
} fio_couplet_s;

/* *****************************************************************************
The Object type head and management
***************************************************************************** */

typedef struct { uint64_t ref; } fiobj_head_s;

#define OBJ2HEAD(o) (((fiobj_head_s *)(o)) - 1)[0]
#define HEAD2OBJ(o) ((fiobj_s *)(((fiobj_head_s *)(o)) + 1))

/* *****************************************************************************
Object Allocation
***************************************************************************** */

static fiobj_s *fiobj_alloc(fiobj_type_en type, uint64_t len, void *buffer) {
  fiobj_head_s *head;
  switch (type) {
  case FIOBJ_T_NULL:
  case FIOBJ_T_TRUE:
  case FIOBJ_T_FALSE: {
    head = malloc(sizeof(*head) + sizeof(fiobj_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_HASH_COUPLET: {
    head = malloc(sizeof(*head) + sizeof(fio_couplet_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    *(fio_couplet_s *)(HEAD2OBJ(head)) = (fio_couplet_s){.name = NULL};
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_NUMBER: {
    head = malloc(sizeof(*head) + sizeof(fio_num_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_num_s *)HEAD2OBJ(head))->i = ((int64_t *)buffer)[0];
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_FLOAT: {
    head = malloc(sizeof(*head) + sizeof(fio_float_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_float_s *)HEAD2OBJ(head))->f = ((double *)buffer)[0];
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_IO: {
    head = malloc(sizeof(*head) + sizeof(fio_io_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_io_s *)HEAD2OBJ(head))->fd = ((intptr_t *)buffer)[0];
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_FILE: {
    head = malloc(sizeof(*head) + sizeof(fio_file_s));
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_file_s *)HEAD2OBJ(head))->f = buffer;
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_STRING: {
    head = malloc(sizeof(*head) + sizeof(fio_str_s) + len + 1);
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_str_s *)HEAD2OBJ(head))->len = len;
    if (buffer)
      memcpy(((fio_str_s *)HEAD2OBJ(head))->str, buffer, len);
    ((fio_str_s *)HEAD2OBJ(head))->str[len] = 0;
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_SYM: {
    head = malloc(sizeof(*head) + sizeof(fio_sym_s) + len + 1);
    head->ref = 1;
    HEAD2OBJ(head)->type = type;
    ((fio_sym_s *)HEAD2OBJ(head))->len = len;
    if (buffer)
      memcpy(((fio_sym_s *)HEAD2OBJ(head))->str, buffer, len);
    ((fio_sym_s *)HEAD2OBJ(head))->str[len] = 0;
    ((fio_sym_s *)HEAD2OBJ(head))->hash = fio_ht_hash(buffer, len);
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_ARRAY: {
    head = malloc(sizeof(*head) + sizeof(fio_ary_s) + len + 1);
    head->ref = 1;
    *((fio_ary_s *)HEAD2OBJ(head)) =
        (fio_ary_s){.start = 8,
                    .end = 8,
                    .capa = 32,
                    .arry = malloc(sizeof(fiobj_s *) * 32),
                    .type = type};
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_HASH: {
    head = malloc(sizeof(*head) + sizeof(fio_hash_s));
    head->ref = 1;
    *((fio_hash_s *)HEAD2OBJ(head)) = (fio_hash_s){
        .h = FIO_HASH_TABLE_STATIC(((fio_hash_s *)HEAD2OBJ(head))->h),
        .type = type};
    return HEAD2OBJ(head);
    break;
  }
  }
  return NULL;
}

static void fiobj_dealloc(fiobj_s *obj) {
  if (!obj || spn_sub(&OBJ2HEAD(obj).ref, 1))
    return;
  switch (obj->type) {
  case FIOBJ_T_HASH_COUPLET:
    /* notice that the containing Hash might have already been freed */
    fiobj_dealloc(((fio_couplet_s *)obj)->name);
    fiobj_dealloc(((fio_couplet_s *)obj)->obj);
    goto common;
  case FIOBJ_T_ARRAY:
    free(((fio_ary_s *)obj)->arry);
    goto common;
  case FIOBJ_T_HASH:
    fio_ht_free(&((fio_hash_s *)obj)->h);
    goto common;
  case FIOBJ_T_IO:
    close(((fio_io_s *)obj)->fd);
    goto common;
  case FIOBJ_T_FILE:
    fclose(((fio_file_s *)obj)->f);
    goto common;
  /* fallthrough */
  common:
  case FIOBJ_T_NULL:
  case FIOBJ_T_TRUE:
  case FIOBJ_T_FALSE:
  case FIOBJ_T_NUMBER:
  case FIOBJ_T_FLOAT:
  case FIOBJ_T_STRING:
  case FIOBJ_T_SYM:
    free(&OBJ2HEAD(obj));
  }
}

/* *****************************************************************************
Generic Object API
***************************************************************************** */

/* simply increrase the reference count for each object. */
static int dup_task_callback(fiobj_s *obj, void *arg) {
  if (!obj)
    return 0;
  spn_add(&OBJ2HEAD(obj).ref, 1);
  return 0;
  (void)arg;
}

/** Increases an object's reference count. */
fiobj_s *fiobj_dup(fiobj_s *obj) {
  fiobj_each(obj, dup_task_callback, NULL);
  return obj;
}

static int dealloc_task_callback(fiobj_s *obj, void *arg) {
  fiobj_dealloc(obj);
  return 0;
  (void)arg;
}
/**
 * Decreases an object's reference count, releasing memory and
 * resources.
 *
 * This function affects nested objects, meaning that when an Array or
 * a Hashe object is passed along, it's children (nested objects) are
 * also freed.
 */
void fiobj_free(fiobj_s *obj) {
  if (obj)
    fiobj_each(obj, dealloc_task_callback, NULL);
}

/**
 * Performes a task for each fio object.
 *
 * Collections (Arrays, Hashes) are deeply probed while being protected from
 * cyclic references. Simpler objects are simply passed along.
 *
 * The callback task function should accept the object, it's name and an opaque
 * user pointer that is simply passed along.
 *
 * The callback's `name` parameter is only set for Hash pairs, indicating the
 * source of the object is a Hash. Arrays and other objects will pass along a
 * NULL pointer for the `name` argument.
 *
 * Notice that when passing collections to the function, both the collection
 * itself and it's nested objects will be passed to the callback task function.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each(fiobj_s *obj, int (*task)(fiobj_s *obj, void *arg), void *arg) {
  if (!task)
    return;
  if (!obj) {
    task(NULL, arg);
    return;
  }

  fiobj_s *list = NULL;
  fiobj_s *processed = NULL;

  if (obj->type == FIOBJ_T_HASH || obj->type == FIOBJ_T_ARRAY) {
    list = fiobj_ary_new();
    processed = fiobj_ary_new();
  }

  fiobj_s *tmp = obj;
  do {

    switch (tmp->type) {

    case FIOBJ_T_ARRAY: {
      /* test against cyclic nesting */
      for (size_t i = 0; i < fiobj_ary_count(processed); i++) {
        if (fiobj_ary_entry(processed, i) == tmp) {
          tmp = NULL;
          goto skip;
        }
      }
      fiobj_ary_push(processed, tmp);
      /* add all objects to the queue */
      fio_ary_s *a = (fio_ary_s *)tmp;
      for (size_t i = a->start; i < a->end; i++) {
        fiobj_ary_push(list, a->arry[i]);
      }
      break;
    }
    case FIOBJ_T_HASH: {
      /* test against cyclic nesting */
      for (size_t i = 0; i < fiobj_ary_count(processed); i++) {
        if (fiobj_ary_entry(processed, i) == tmp) {
          tmp = NULL;
          goto skip;
        }
      }
      fiobj_ary_push(processed, tmp);
      /* add all objects to the queue */
      fio_hash_s *h = (fio_hash_s *)tmp;
      fio_couplet_s *i;
      fio_ht_for_each(fio_couplet_s, node, i, h->h) {
        fiobj_ary_push(list, (fiobj_s *)i);
      }
      break;
    }
    default:
      break;
    }
  skip:
    if (task(tmp, arg) == -1)
      break;
    if (list)
      tmp = fiobj_ary_shift(list);
  } while (tmp && list);

  if (processed) {
    fiobj_dealloc(processed);
    fiobj_dealloc(list);
  }
}

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
Number and Float API
***************************************************************************** */

/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10. `0x##` (or `x##`) and `0b##` (or `b##`)
 * are recognized as base 16 and base 2 (binary MSB first) respectively.
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

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num) {
  return fiobj_alloc(FIOBJ_T_NUMBER, 0, &num);
}

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(double num) {
  return fiobj_alloc(FIOBJ_T_FLOAT, 0, &num);
}

/** Mutates a Number object's value. Effects every object's reference! */
void fiobj_num_set(fiobj_s *target, int64_t num) {
  ((fio_num_s *)target)[0].i = num;
}

/** Mutates a Float object's value. Effects every object's reference!  */
void fiobj_float_set(fiobj_s *target, double num) {
  ((fio_float_s *)target)[0].f = num;
}

/**
 * Returns a Number's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * A type error results in 0.
 */
int64_t fiobj_obj2num(fiobj_s *obj) {
  if (obj->type == FIOBJ_T_NUMBER)
    return ((fio_num_s *)obj)->i;
  if (obj->type == FIOBJ_T_FLOAT)
    return (int64_t)floorl(((fio_float_s *)obj)->f);
  if (obj->type == FIOBJ_T_STRING)
    return fio_atol(((fio_str_s *)obj)->str);
  if (obj->type == FIOBJ_T_SYM)
    return fio_atol(((fio_sym_s *)obj)->str);
  return 0;
}

/**
 * Returns a Float's value.
 *
 * If a String or Symbol are passed to the function, they will be
 * parsed assuming base 10 numerical data.
 *
 * A type error results in 0.
 */
double fiobj_obj2float(fiobj_s *obj) {
  if (obj->type == FIOBJ_T_FLOAT)
    return ((fio_float_s *)obj)->f;
  if (obj->type == FIOBJ_T_NUMBER)
    return (double)((fio_num_s *)obj)->i;
  if (obj->type == FIOBJ_T_STRING)
    return fio_atof(((fio_str_s *)obj)->str);
  if (obj->type == FIOBJ_T_SYM)
    return fio_atof(((fio_sym_s *)obj)->str);
  return 0;
}

/* *****************************************************************************
String API
***************************************************************************** */

/**
 * A helper function that convers between a signed int64_t to a string.
 *
 * No overflow guard is provided, make sure there's at least 66 bytes available
 * (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
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
      dest[len++] = ((tmp < 10) ? ('0' + tmp) : (('A' - 10) + tmp));
      tmp = (n & 0x0F00000000000000) >> 56;
      dest[len++] = ((tmp < 10) ? ('0' + tmp) : (('A' - 10) + tmp));
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
 * No overflow guard is provided, make sure there's at least 130 bytes available
 * (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
 */
size_t fio_ftoa(char *dest, double num, uint8_t base) {
  if (base == 2 || base == 16) {
    /* handle the binary / Hex representation the same as if it were an int64_t
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

/** Creates a String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_new(const char *str, size_t len) {
  return fiobj_alloc(FIOBJ_T_STRING, len, (void *)str);
}

/**
 * Returns a C String (NUL terminated) using the `fio_string_s` data type.
 *
 * The Sting in binary safe and might contain NUL bytes in the middle as well as
 * a terminating NUL.
 *
 * If a Symbol, a Number or a Float are passed to the function, they
 * will be parsed as a *temporary*, thread-safe, String.
 *
 * Numbers will be represented in base 10 numerical data.
 *
 * A type error results in NULL (i.e. object isn't a String).
 */
static __thread char num_buffer[128];
fio_string_s fiobj_obj2cstr(fiobj_s *obj) {
  if (obj->type == FIOBJ_T_STRING) {
    return (fio_string_s){
        .buffer = ((fio_str_s *)obj)->str, .len = ((fio_str_s *)obj)->len,
    };
  } else if (obj->type == FIOBJ_T_SYM) {
    return (fio_string_s){
        .buffer = ((fio_sym_s *)obj)->str, .len = ((fio_sym_s *)obj)->len,
    };
  } else if (obj->type == FIOBJ_T_NUMBER) {
    return (fio_string_s){
        .buffer = num_buffer,
        .len = fio_ltoa(num_buffer, ((fio_num_s *)obj)->i, 10),
    };
  } else if (obj->type == FIOBJ_T_FLOAT) {
    return (fio_string_s){
        .buffer = num_buffer,
        .len = fio_ftoa(num_buffer, ((fio_float_s *)obj)->f, 10),
    };
  }
  return (fio_string_s){.buffer = NULL, .len = 0};
}

/* *****************************************************************************
Symbol API
***************************************************************************** */

/** Creates a Symbol object. Use `fiobj_free`. */
fiobj_s *fiobj_sym_new(const char *str, size_t len) {
  return fiobj_alloc(FIOBJ_T_SYM, len, (void *)str);
}

/** Returns 1 if both Symbols are equal and 0 if not. */
int fiobj_sym_iseql(fiobj_s *sym1, fiobj_s *sym2) {
  if (sym1->type != FIOBJ_T_SYM || sym2->type != FIOBJ_T_SYM)
    return 0;
  return (((fio_sym_s *)sym1)->hash == ((fio_sym_s *)sym2)->hash);
}

/* *****************************************************************************
IO API
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
File API
***************************************************************************** */

/** Wrapps a `FILe` pointer in a File object. Use `fiobj_free` to close. */
fiobj_s *fio_file_wrap(FILE *file) {
  return fiobj_alloc(FIOBJ_T_FILE, 0, (void *)file);
}

/**
 * Returns a temporary `FILE` pointer.
 *
 * A type error results in NULL.
 */
FILE *fiobj_file(fiobj_s *obj) {
  if (obj->type != FIOBJ_T_FILE)
    return NULL;
  return ((fio_file_s *)obj)->f;
}

/* *****************************************************************************
Array API
***************************************************************************** */
#define obj2ary(ary) ((fio_ary_s *)(ary))

static void fiobj_ary_getmem(fiobj_s *ary, int64_t needed) {
  /* we have enough memory, but we need to re-organize it. */
  if (needed == -1) {
    if (obj2ary(ary)->end < obj2ary(ary)->capa) {

      /* since allocation can be cheaper than memmove (depending on size),
       * we'll just shove everything to the end...
       */
      uint64_t len = obj2ary(ary)->end - obj2ary(ary)->start;
      memmove(obj2ary(ary)->arry + (obj2ary(ary)->capa - len),
              obj2ary(ary)->arry + obj2ary(ary)->start,
              len * sizeof(*obj2ary(ary)->arry));
      obj2ary(ary)->start = obj2ary(ary)->capa - len;
      obj2ary(ary)->end = obj2ary(ary)->capa;

      return;
    }
    /* add some breathing room for future `unshift`s */
    needed =
        0 - ((obj2ary(ary)->capa <= 1024) ? (obj2ary(ary)->capa >> 1) : 1024);
  }

  /* FIFO would probably benefit from memmove over bloating allocations. */
  if (needed == 1 && obj2ary(ary)->start > (obj2ary(ary)->capa >> 1)) {
    uint64_t len = obj2ary(ary)->end - obj2ary(ary)->start;
    memmove(obj2ary(ary)->arry + 2, obj2ary(ary)->arry + obj2ary(ary)->start,
            len * sizeof(*obj2ary(ary)->arry));
    obj2ary(ary)->start = 2;
    obj2ary(ary)->end = len + 2;

    return;
  }

  // fprintf(stderr,
  //         "ARRAY MEMORY REVIEW (%p) "
  //         "with capa %llu, (%llu..%llu):\n",
  //         (void *)obj2ary(ary)->arry, obj2ary(ary)->capa,
  //         obj2ary(ary)->start, obj2ary(ary)->end);

  // for (size_t i = obj2ary(ary)->start; i < obj2ary(ary)->end; i++) {
  //   fprintf(stderr, "(%lu) %p\n", i, (void *)obj2ary(ary)->arry[i]);
  // }

  /* alocate using exponential growth, up to single page size. */
  uint64_t updated_capa = obj2ary(ary)->capa;
  uint64_t minimum =
      obj2ary(ary)->capa + ((needed < 0) ? (0 - needed) : needed);
  while (updated_capa <= minimum)
    updated_capa =
        (updated_capa <= 4096) ? (updated_capa << 1) : (updated_capa + 4096);

  /* we assume memory allocation works. it's better to crash than to continue
   * living without memory... besides, malloc is optimistic these days.       */
  obj2ary(ary)->arry =
      realloc(obj2ary(ary)->arry, updated_capa * sizeof(*obj2ary(ary)->arry));
  obj2ary(ary)->capa = updated_capa;

  if (needed >= 0) /* we're done, realloc grows the top of the address space*/
    return;

  /* change to a positive number that represents space from begining */
  needed = 0 - needed;

  uint64_t len = obj2ary(ary)->end - obj2ary(ary)->start;

  memmove(obj2ary(ary)->arry + needed, obj2ary(ary)->arry + obj2ary(ary)->start,
          len * sizeof(*obj2ary(ary)->arry));
  obj2ary(ary)->end = needed + len;
  obj2ary(ary)->start = needed;
}

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void) { return fiobj_alloc(FIOBJ_T_ARRAY, 0, NULL); }

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary) {
  if (ary->type != FIOBJ_T_ARRAY)
    return 0;
  return (obj2ary(ary)->end - obj2ary(ary)->start);
}

/**
 * Pushes an object to the end of the Array.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj) {
  if (ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }
  if (obj2ary(ary)->capa <= obj2ary(ary)->end)
    fiobj_ary_getmem(ary, 1);
  obj2ary(ary)->arry[(obj2ary(ary)->end)++] = obj;
}

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary) {
  if (ary->type != FIOBJ_T_ARRAY)
    return NULL;
  if (obj2ary(ary)->start == obj2ary(ary)->end)
    return NULL;
  fiobj_s *ret = obj2ary(ary)->arry[--(obj2ary(ary)->end)];
  return ret;
}

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 */
void fiobj_ary_unshift(fiobj_s *ary, fiobj_s *obj) {
  if (ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }
  if (obj2ary(ary)->start == 0)
    fiobj_ary_getmem(ary, -1);
  obj2ary(ary)->arry[--(obj2ary(ary)->start)] = obj;
}

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary) {
  if (ary->type != FIOBJ_T_ARRAY)
    return NULL;
  if (obj2ary(ary)->start == obj2ary(ary)->end)
    return NULL;
  fiobj_s *ret = obj2ary(ary)->arry[(obj2ary(ary)->start)++];
  return ret;
}

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
fiobj_s *fiobj_ary_entry(fiobj_s *ary, int64_t pos) {
  if (ary->type != FIOBJ_T_ARRAY)
    return NULL;
  if (pos >= 0) {
    pos = pos + obj2ary(ary)->start;
    if ((uint64_t)pos >= obj2ary(ary)->end)
      return NULL;
    return obj2ary(ary)->arry[pos];
  }
  pos = pos + obj2ary(ary)->end;
  if (pos < 0)
    return NULL;
  if ((uint64_t)pos < obj2ary(ary)->start)
    return NULL;
  return obj2ary(ary)->arry[pos];
}

/**
 * Sets an object at the requested position.
 */
void fiobj_ary_set(fiobj_s *ary, fiobj_s *obj, int64_t pos) {
  if (ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }
  /* test for memory and request memory if missing. */
  if (pos >= 0) {
    if ((uint64_t)pos >= (obj2ary(ary)->capa + obj2ary(ary)->start))
      fiobj_ary_getmem(ary, obj2ary(ary)->capa - pos);
  } else if (pos + (int64_t)obj2ary(ary)->end < 0)
    fiobj_ary_getmem(ary, pos + obj2ary(ary)->end);

  if (pos >= 0) {
    /* position relative to start */
    pos = pos + obj2ary(ary)->start;
    /* check for an existing object */
    if ((uint64_t)pos < obj2ary(ary)->end && obj2ary(ary)->arry[pos])
      fiobj_free(obj2ary(ary)->arry[pos]);
    /* initialize empty spaces */
    while ((uint64_t)pos >= obj2ary(ary)->end)
      obj2ary(ary)->arry[(obj2ary(ary)->end)++] = NULL;

  } else {
    /* position relative to end */
    pos = pos + (int64_t)obj2ary(ary)->end;
    /* initialize empty spaces */
    while (obj2ary(ary)->start >= (uint64_t)pos)
      obj2ary(ary)->arry[(obj2ary(ary)->start)--] = NULL;
  }

  obj2ary(ary)->arry[pos] = obj;
}

/* *****************************************************************************
Hash API
***************************************************************************** */

/**
 * Creates a mutable empty Hash object. Use `fiobj_free` when done.
 *
 * Notice that these Hash objects are designed for smaller collections and
 * retain order of object insertion.
 */
fiobj_s *fiobj_hash_new(void);

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving** the
 * ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj);

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash_remove(fiobj_s *hash, fiobj_s *sym);

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the associated
 * object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(fiobj_s *hash, fiobj_s *sym);

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL if
 * none.
 */
fiobj_s *fiobj_hash_get(fiobj_s *hash, fiobj_s *sym);

/**
 * If object is a Hash couplet (occurs in `fiobj_each`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2sym(fiobj_s *obj);

/**
 * If object is a Hash couplet (occurs in `fiobj_each`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(fiobj_s *obj);

/* *****************************************************************************
A touch of testing
***************************************************************************** */
#ifdef DEBUG

static int fiobj_test_array_task(fiobj_s *obj, void *arg) {
  static size_t count = 0;
  if (obj->type == FIOBJ_T_ARRAY) {
    count = fiobj_ary_count(obj);
    fprintf(stderr, "* Array data: [");
    return 0;
  }
  if (--count) {
    fprintf(stderr, "%s, ", fiobj_obj2cstr(obj).data);
  } else
    fprintf(stderr, "%s] (should be 127..0)\n", fiobj_obj2cstr(obj).data);
  return 0;
  (void)arg;
}

void fiobj_test(void) {
  fiobj_s *obj;
  size_t i;
  fprintf(stderr, "Starting fiobj basic testing:\n");

  obj = fiobj_null();
  if (obj->type != FIOBJ_T_NULL)
    fprintf(stderr, "* FAILED null object test.\n");
  fiobj_free(obj);

  obj = fiobj_false();
  if (obj->type != FIOBJ_T_FALSE)
    fprintf(stderr, "* FAILED false object test.\n");
  fiobj_free(obj);

  obj = fiobj_true();
  if (obj->type != FIOBJ_T_TRUE)
    fprintf(stderr, "* FAILED true object test.\n");
  fiobj_free(obj);

  obj = fiobj_num_new(255);
  if (obj->type != FIOBJ_T_NUMBER || fiobj_obj2num(obj) != 255)
    fprintf(stderr, "* FAILED 255 object test i == %llu with type %d.\n",
            fiobj_obj2num(obj), obj->type);
  if (strcmp(fiobj_obj2cstr(obj).data, "255"))
    fprintf(stderr, "* FAILED base 10 fiobj_obj2cstr test with %s.\n",
            fiobj_obj2cstr(obj).data);
  i = fio_ltoa(num_buffer, fiobj_obj2num(obj), 16);
  if (strcmp(num_buffer, "00FF"))
    fprintf(stderr, "* FAILED base 16 fiobj_obj2cstr test with (%lu): %s.\n", i,
            num_buffer);
  i = fio_ltoa(num_buffer, fiobj_obj2num(obj), 2);
  if (strcmp(num_buffer, "011111111"))
    fprintf(stderr, "* FAILED base 2 fiobj_obj2cstr test with (%lu): %s.\n", i,
            num_buffer);
  fiobj_free(obj);

  obj = fiobj_float_new(77.777);
  if (obj->type != FIOBJ_T_FLOAT || fiobj_obj2num(obj) != 77 ||
      fiobj_obj2float(obj) != 77.777)
    fprintf(stderr, "* FAILED 77.777 object test.\n");
  if (strcmp(fiobj_obj2cstr(obj).data, "77.777"))
    fprintf(stderr, "* FAILED float2str test with %s.\n",
            fiobj_obj2cstr(obj).data);
  fiobj_free(obj);

  obj = fiobj_str_new("0x7F", 4);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 127)
    fprintf(stderr, "* FAILED 0x7F object test.\n");
  fiobj_free(obj);

  obj = fiobj_str_new("0b01111111", 10);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 127)
    fprintf(stderr, "* FAILED 0b01111111 object test.\n");
  fiobj_free(obj);

  obj = fiobj_str_new("232.79", 6);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 232)
    fprintf(stderr, "* FAILED 232 object test. %llu\n", fiobj_obj2num(obj));
  if (fiobj_obj2float(obj) != 232.79)
    fprintf(stderr, "* FAILED fiobj_obj2float test with %f.\n",
            fiobj_obj2float(obj));
  fiobj_free(obj);

  /* test array */
  obj = fiobj_ary_new();
  if (obj->type == FIOBJ_T_ARRAY) {
    for (size_t i = 0; i < 128; i++) {
      fiobj_ary_unshift(obj, fiobj_num_new(i));
      if (fiobj_ary_count(obj) != i + 1)
        fprintf(stderr, "* FAILED Array count. %lu/%llu != %lu\n",
                fiobj_ary_count(obj), obj2ary(obj)->capa, i + 1);
    }
    fiobj_each(obj, fiobj_test_array_task, NULL);
    fiobj_free(obj);
  } else {
    fprintf(stderr, "* FAILED to initialize Array test!\n");
    fiobj_free(obj);
  }
  /* test cyclic protection */
  {
    fprintf(stderr, "* testing cyclic protection. \n");
    fiobj_s *a1 = fiobj_ary_new();
    fiobj_s *a2 = fiobj_ary_new();
    for (size_t i = 0; i < 128; i++) {
      fiobj_ary_push(a1, fiobj_num_new(i));
      fiobj_ary_unshift(a2, fiobj_num_new(i));
    }
    fiobj_ary_push(a1, a2);
    fiobj_ary_unshift(a2, a1);
    obj = fiobj_dup(fiobj_ary_entry(a2, -32));
    if (!obj || obj->type != FIOBJ_T_NUMBER)
      fprintf(stderr, "* FAILED unexpected object %p with type %d\n",
              (void *)obj, obj ? obj->type : 0);
    if (OBJ2HEAD(obj).ref != 2)
      fprintf(stderr, "* FAILED object reference counting test (%llu)\n",
              OBJ2HEAD(obj).ref);
    fiobj_free(a1); /* should free both, but I can't really test that... */
    if (OBJ2HEAD(obj).ref != 1)
      fprintf(stderr, "* FAILED to free cyclic nested array members (%llu)\n",
              OBJ2HEAD(obj).ref);
    fiobj_free(obj);
  }
}
#endif
