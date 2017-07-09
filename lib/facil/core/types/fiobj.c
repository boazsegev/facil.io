/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "spnlock.inc"

#include "fio_hash_table.h"
#include "fiobj.h"

#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/* *****************************************************************************
Simple List - I will slowly move any external dependencies to allow independance
***************************************************************************** */

typedef struct fio_ls_s {
  struct fio_ls_s *prev;
  struct fio_ls_s *next;
  fiobj_s *obj;
} fio_ls_s;

#define FIO_LS_INIT(name)                                                      \
  { .next = &(name), .prev = &(name) }

/** Adds an object to the list's head. */
static inline __attribute__((unused)) void fio_ls_push(fio_ls_s *pos,
                                                       fiobj_s *obj) {
  /* prepare item */
  fio_ls_s *item = malloc(sizeof(*item));
  *item = (fio_ls_s){.prev = pos, .next = pos->next, .obj = obj};
  /* inject item */
  pos->next->prev = item;
  pos->next = item;
}

/** Adds an object to the list's tail. */
static inline __attribute__((unused)) void fio_ls_unshift(fio_ls_s *pos,
                                                          fiobj_s *obj) {
  pos = pos->prev;
  fio_ls_push(pos, obj);
}

/** Removes an object from the list's head. */
static inline __attribute__((unused)) fiobj_s *fio_ls_pop(fio_ls_s *list) {
  if (list->next == list)
    return NULL;
  fio_ls_s *item = list->next;
  fiobj_s *ret = item->obj;
  list->next = item->next;
  list->next->prev = list;
  free(item);
  return ret;
}

/** Removes an object from the list's tail. */
static inline __attribute__((unused)) fiobj_s *fio_ls_shift(fio_ls_s *list) {
  if (list->prev == list)
    return NULL;
  fio_ls_s *item = list->prev;
  fiobj_s *ret = item->obj;
  list->prev = item->prev;
  list->prev->next = list;
  free(item);
  return ret;
}

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
  uint64_t capa;
  uint64_t len;
  char *str;
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
  case FIOBJ_T_COUPLET: {
    head = malloc(sizeof(*head) + sizeof(fio_couplet_s));
    head->ref = 1;
    *(fio_couplet_s *)(HEAD2OBJ(head)) = (fio_couplet_s){.type = type};
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
    head = malloc(sizeof(*head) + sizeof(fio_str_s));
    head->ref = 1;
    *((fio_str_s *)HEAD2OBJ(head)) = (fio_str_s){
        .type = type, .len = len, .capa = len, .str = malloc(len),
    };
    if (buffer)
      memcpy(((fio_str_s *)HEAD2OBJ(head))->str, buffer, len);
    ((fio_str_s *)HEAD2OBJ(head))->str[len] = 0;
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_SYMBOL: {
    head = malloc(sizeof(*head) + sizeof(fio_sym_s) + len + 1);
    head->ref = 1;
    if (buffer) {
      *((fio_sym_s *)HEAD2OBJ(head)) = (fio_sym_s){
          .type = type, .len = len, .hash = fio_ht_hash(buffer, len),
      };
      memcpy(((fio_sym_s *)HEAD2OBJ(head))->str, buffer, len);
      ((fio_sym_s *)HEAD2OBJ(head))->str[len] = 0;
    } else
      *((fio_sym_s *)HEAD2OBJ(head)) = (fio_sym_s){
          .type = type, .len = len, .hash = 0,
      };
    return HEAD2OBJ(head);
    break;
  }
  case FIOBJ_T_ARRAY: {
    head = malloc(sizeof(*head) + sizeof(fio_ary_s));
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

/* *****************************************************************************
Object Deallocation
***************************************************************************** */

static void fiobj_dealloc(fiobj_s *obj) {
  if (obj == NULL)
    return;
  if (OBJ2HEAD(obj).ref == 0) {
    fprintf(stderr,
            "ERROR: attempting to free an object that isn't a fiobj or already "
            "freed (%p)\n",
            (void *)obj);
    kill(0, SIGABRT);
  }
  if (spn_sub(&OBJ2HEAD(obj).ref, 1))
    return;
  switch (obj->type) {
  case FIOBJ_T_COUPLET:
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
  case FIOBJ_T_STRING:
    free(((fio_str_s *)obj)->str);
    goto common;

  common:
  case FIOBJ_T_NULL:
  case FIOBJ_T_TRUE:
  case FIOBJ_T_FALSE:
  case FIOBJ_T_NUMBER:
  case FIOBJ_T_FLOAT:
  case FIOBJ_T_SYMBOL:
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
  fiobj_each2(obj, dup_task_callback, NULL);
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
    fiobj_each2(obj, dealloc_task_callback, NULL);
}

/* *****************************************************************************
Object Iteration (`each2`)
***************************************************************************** */

/**
 * Deep itteration using a callback for each fio object, including the parent.
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each2(fiobj_s *obj, int (*task)(fiobj_s *obj, void *arg),
                 void *arg) {

  fio_ls_s list = FIO_LS_INIT(list);
  fio_ls_s processed = FIO_LS_INIT(processed);
  fio_ls_s *pos;

  if (!task)
    return;
  if (obj && (obj->type == FIOBJ_T_ARRAY || obj->type == FIOBJ_T_HASH ||
              obj->type == FIOBJ_T_COUPLET))
    goto nested;
  /* no nested objects, fast and easy. */
  task(obj, arg);
  return;

nested:
  fio_ls_push(&list, obj);
  /* as long as `list` contains items... */
  do {
    /* remove from the begining of the list, add at it's end. */
    obj = fio_ls_shift(&list);
    if (!obj)
      goto perform_task;

    /* test for cyclic nesting before reading the object (possibly freed) */
    pos = processed.next;
    while (pos != &processed) {
      if (pos->obj == obj) {
        obj = NULL;
        goto perform_task;
      }
      pos = pos->next;
    }

    fiobj_s *tmp = obj;
    if (obj->type == FIOBJ_T_COUPLET)
      tmp = fiobj_couplet2obj(obj);

    /* if it's a simple object, fast forward */
    if (tmp->type != FIOBJ_T_ARRAY && tmp->type != FIOBJ_T_HASH)
      goto perform_task;

    /* add object to cyclic protection */
    fio_ls_push(&processed, obj);
    if (obj->type == FIOBJ_T_COUPLET)
      fio_ls_push(&processed, tmp);

    /* add all children to the queue */
    if (tmp->type == FIOBJ_T_ARRAY) {
      size_t i = fiobj_ary_count(tmp);
      while (i) {
        i--;
        fio_ls_unshift(&list, fiobj_ary_entry(tmp, i));
      }
    } else { /* must be Hash */
      fio_ls_s *at = &list;
      fio_hash_s *h = (fio_hash_s *)tmp;
      fio_couplet_s *i;
      fio_ht_for_each(fio_couplet_s, node, i, h->h) {
        fio_ls_unshift(at, (fiobj_s *)i);
        at = at->prev;
      }
    }

  perform_task:
    if (task(obj, arg) == -1)
      goto finish;
  } while (list.next != &list);

finish:
  while (fio_ls_pop(&list))
    ;
  while (fio_ls_pop(&processed))
    ;
}

/* *****************************************************************************
Object Comparison (`fiobj_iseq`)
***************************************************************************** */

static int fiobj_iseq_check(fiobj_s *obj1, fiobj_s *obj2) {
  if (obj1->type != obj2->type)
    return 0;
  switch (obj1->type) {
  case FIOBJ_T_NULL:
  case FIOBJ_T_TRUE:
  case FIOBJ_T_FALSE:
    if (obj1->type == obj2->type)
      return 1;
    break;
  case FIOBJ_T_COUPLET:
    return fiobj_iseq_check(((fio_couplet_s *)obj1)->name,
                            ((fio_couplet_s *)obj2)->name) &&
           fiobj_iseq_check(((fio_couplet_s *)obj1)->obj,
                            ((fio_couplet_s *)obj2)->obj);
    break;
  case FIOBJ_T_ARRAY:
    if (fiobj_ary_count(obj1) == fiobj_ary_count(obj2))
      return 1;
    break;
  case FIOBJ_T_HASH:
    if (fiobj_hash_count(obj1) == fiobj_hash_count(obj2))
      return 1;
    break;
  case FIOBJ_T_IO:
    if (((fio_io_s *)obj1)->fd == ((fio_io_s *)obj2)->fd)
      return 1;
    break;
  case FIOBJ_T_FILE:
    if (((fio_file_s *)obj1)->f == ((fio_file_s *)obj2)->f)
      return 1;
    break;
  case FIOBJ_T_NUMBER:
    if (((fio_num_s *)obj1)->i == ((fio_num_s *)obj2)->i)
      return 1;
    break;
  case FIOBJ_T_FLOAT:
    if (((fio_float_s *)obj1)->f == ((fio_float_s *)obj2)->f)
      return 1;
    break;
  case FIOBJ_T_SYMBOL:
    if (((fio_sym_s *)obj1)->hash == ((fio_sym_s *)obj2)->hash)
      return 1;
    break;
  case FIOBJ_T_STRING:
    if (((fio_str_s *)obj1)->len == ((fio_str_s *)obj2)->len &&
        !memcmp(((fio_str_s *)obj1)->str, ((fio_str_s *)obj2)->str,
                ((fio_str_s *)obj1)->len))
      return 1;
    break;
  }
  return 0;
}

/**
 * Deeply compare two objects. No hashing is involved.
 */
int fiobj_iseq(fiobj_s *obj1, fiobj_s *obj2) {

  fio_ls_s pos1 = FIO_LS_INIT(pos1);
  fio_ls_s pos2 = FIO_LS_INIT(pos2);
  fio_ls_s history = FIO_LS_INIT(history);
  fio_ls_push(&pos1, obj1);
  fio_ls_push(&pos2, obj2);
  int ret;

  while (pos1.next != &pos1 && pos2.next != &pos2) {
  restart_cmp_loop:
    obj1 = fio_ls_pop(&pos1);
    obj2 = fio_ls_pop(&pos2);

#ifndef DEBUG /* don't optimize when testing */
    if (obj1 == obj2)
      continue;
#else
    if (!obj1 && !obj2)
      continue;
#endif

    if (!obj1 || !obj2)
      goto not_equal;

    if (!fiobj_iseq_check(obj1, obj2))
      goto not_equal;

    /* test for nested objects */
    if (obj1->type == FIOBJ_T_COUPLET || obj1->type == FIOBJ_T_ARRAY ||
        obj1->type == FIOBJ_T_HASH) {
      fio_ls_s *p = history.next;
      while (p != &history) {
        if (p->obj == obj1)
          goto restart_cmp_loop;
        p = p->next;
      }
    }

    if (obj1->type == FIOBJ_T_COUPLET) {
      if (((fio_couplet_s *)obj1)->obj->type == FIOBJ_T_HASH ||
          ((fio_couplet_s *)obj1)->obj->type == FIOBJ_T_COUPLET)
        fio_ls_push(&history, obj1);
      obj1 = ((fio_couplet_s *)obj1)->obj;
      obj2 = ((fio_couplet_s *)obj2)->obj;
    }

    if (obj1->type == FIOBJ_T_ARRAY) {
      fio_ls_push(&history, obj1);
      size_t i = fiobj_ary_count(obj1);
      while (i) {
        i--;
        fio_ls_unshift(&pos1, fiobj_ary_entry(obj1, i));
      }
      i = fiobj_ary_count(obj2);
      while (i) {
        i--;
        fio_ls_unshift(&pos2, fiobj_ary_entry(obj2, i));
      }
    } else if (obj1->type == FIOBJ_T_HASH) {
      fio_ls_push(&history, obj1);
      fio_ls_s *at = &pos1;
      fio_couplet_s *i;
      fio_ht_for_each(fio_couplet_s, node, i, (((fio_hash_s *)obj1)->h)) {
        fio_ls_unshift(at, (fiobj_s *)i);
        at = at->prev;
      }
      at = &pos2;
      fio_ht_for_each(fio_couplet_s, node, i, (((fio_hash_s *)obj2)->h)) {
        fio_ls_unshift(at, (fiobj_s *)i);
        at = at->prev;
      }
    }
  }

finish:

  ret = (pos1.next == &pos1 && pos2.next == &pos2);

  while (fio_ls_pop(&pos1))
    ;
  while (fio_ls_pop(&pos2))
    ;
  while (fio_ls_pop(&history))
    ;
  return ret;
not_equal:
  fio_ls_push(&pos1, (fiobj_s *)&pos1);
  goto finish;
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
 * Numbers are assumed to be in base 10. `0x##` (or `x##`) and `0b##` (or
 * `b##`) are recognized as base 16 and base 2 (binary MSB first)
 * respectively.
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
  if (!obj)
    return 0;
  if (obj->type == FIOBJ_T_NUMBER)
    return ((fio_num_s *)obj)->i;
  if (obj->type == FIOBJ_T_FLOAT)
    return (int64_t)floorl(((fio_float_s *)obj)->f);
  if (obj->type == FIOBJ_T_STRING)
    return fio_atol(((fio_str_s *)obj)->str);
  if (obj->type == FIOBJ_T_SYMBOL)
    return fio_atol(((fio_sym_s *)obj)->str);
  if (obj->type == FIOBJ_T_ARRAY)
    return fiobj_ary_count(obj);
  if (obj->type == FIOBJ_T_HASH)
    return fiobj_hash_count(obj);
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
  if (!obj)
    return 0;
  if (obj->type == FIOBJ_T_FLOAT)
    return ((fio_float_s *)obj)->f;
  if (obj->type == FIOBJ_T_NUMBER)
    return (double)((fio_num_s *)obj)->i;
  if (obj->type == FIOBJ_T_STRING)
    return fio_atof(((fio_str_s *)obj)->str);
  if (obj->type == FIOBJ_T_SYMBOL)
    return fio_atof(((fio_sym_s *)obj)->str);
  if (obj->type == FIOBJ_T_ARRAY)
    return (double)fiobj_ary_count(obj);
  if (obj->type == FIOBJ_T_HASH)
    return (double)fiobj_hash_count(obj);
  return 0;
}

/* *****************************************************************************
String API
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

/** Creates a String object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_strvprintf(const char *restrict format, va_list argv) {
  fiobj_s *str = NULL;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len == 0)
    str = fiobj_alloc(FIOBJ_T_STRING, 0, (void *)"");
  if (len <= 0)
    return str;
  str = fiobj_alloc(FIOBJ_T_STRING, len, NULL); /* adds 1 to len, for NUL */
  vsnprintf(((fio_str_s *)(str))->str, len + 1, format, argv);
  return str;
}
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_strprintf(const char *restrict format, ...) {
  va_list argv;
  va_start(argv, format);
  fiobj_s *str = fiobj_strvprintf(format, argv);
  va_end(argv);
  return str;
}

/** Creates a buffer String object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_str_buf(size_t capa) {
  fiobj_s *str = fiobj_alloc(FIOBJ_T_STRING, capa, NULL);
  fiobj_str_clear(str);
  return str;
}

/** Resizes a String object, allocating more memory if required. */
void fiobj_str_resize(fiobj_s *str, size_t size) {
  if (str->type != FIOBJ_T_STRING)
    return;
  if (((fio_str_s *)str)->capa >= size + 1) {
    ((fio_str_s *)str)->len = size;
    ((fio_str_s *)str)->str[size] = 0;
    return;
  }
  /* it's better to crash than live without memory... */
  ((fio_str_s *)str)->str = realloc(((fio_str_s *)str)->str, size + 1);
  ((fio_str_s *)str)->capa = size + 1;
  return;
}

/** Return's a String's capacity, if any. */
size_t fiobj_str_capa(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return 0;
  return ((fio_str_s *)str)->capa;
}

/** Deallocates any unnecessary memory (if supported by OS). */
void fiobj_str_minimize(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  ((fio_str_s *)str)->capa = ((fio_str_s *)str)->len + 1;
  ((fio_str_s *)str)->str =
      realloc(((fio_str_s *)str)->str, ((fio_str_s *)str)->capa);
  return;
}

/** Empties a String's data. */
void fiobj_str_clear(fiobj_s *str) {
  if (str->type != FIOBJ_T_STRING)
    return;
  ((fio_str_s *)str)->str[0] = 0;
  ((fio_str_s *)str)->len = 0;
}

/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write(fiobj_s *dest, const char *data, size_t len) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  fiobj_str_resize(dest, ((fio_str_s *)dest)->len + len);
  memcpy(((fio_str_s *)dest)->str + ((fio_str_s *)dest)->len - len, data, len);
  ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0;
  return ((fio_str_s *)dest)->len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_write2(fiobj_s *dest, const char *format, ...) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  va_list argv;
  va_start(argv, format);
  int len = vsnprintf(NULL, 0, format, argv);
  va_end(argv);
  if (len <= 0)
    return ((fio_str_s *)dest)->len;
  fiobj_str_resize(dest, ((fio_str_s *)dest)->len + len);
  va_start(argv, format);
  vsnprintf(((fio_str_s *)(dest))->str + ((fio_str_s *)dest)->len - len,
            len + 1, format, argv);
  va_end(argv);
  ((fio_str_s *)dest)->str[((fio_str_s *)dest)->len] = 0;
  return ((fio_str_s *)dest)->len;
}
/**
 * Writes data at the end of the string, resizing the string as required.
 * Returns the new length of the String
 */
size_t fiobj_str_join(fiobj_s *dest, fiobj_s *obj) {
  if (dest->type != FIOBJ_T_STRING)
    return 0;
  fio_cstr_s o = fiobj_obj2cstr(obj);
  if (o.len == 0)
    return ((fio_str_s *)dest)->len;
  return fiobj_str_write(dest, o.data, o.len);
}

/**
 * Returns a C String (NUL terminated) using the `fio_cstr_s` data type.
 *
 * The Sting in binary safe and might contain NUL bytes in the middle as well
 * as a terminating NUL.
 *
 * If a Symbol, a Number or a Float are passed to the function, they
 * will be parsed as a *temporary*, thread-safe, String.
 *
 * Numbers will be represented in base 10 numerical data.
 *
 * A type error results in NULL (i.e. object isn't a String).
 */
static __thread char num_buffer[128];
fio_cstr_s fiobj_obj2cstr(fiobj_s *obj) {
  if (!obj)
    return (fio_cstr_s){.buffer = NULL, .len = 0};

  if (obj->type == FIOBJ_T_STRING) {
    return (fio_cstr_s){
        .buffer = ((fio_str_s *)obj)->str, .len = ((fio_str_s *)obj)->len,
    };
  } else if (obj->type == FIOBJ_T_SYMBOL) {
    return (fio_cstr_s){
        .buffer = ((fio_sym_s *)obj)->str, .len = ((fio_sym_s *)obj)->len,
    };
  } else if (obj->type == FIOBJ_T_NULL) {
    /* unlike NULL (not fiobj), this returns an empty string. */
    return (fio_cstr_s){.buffer = "", .len = 0};
  } else if (obj->type == FIOBJ_T_COUPLET) {
    return fiobj_obj2cstr(((fio_couplet_s *)obj)->obj);
  } else if (obj->type == FIOBJ_T_NUMBER) {
    return (fio_cstr_s){
        .buffer = num_buffer,
        .len = fio_ltoa(num_buffer, ((fio_num_s *)obj)->i, 10),
    };
  } else if (obj->type == FIOBJ_T_FLOAT) {
    return (fio_cstr_s){
        .buffer = num_buffer,
        .len = fio_ftoa(num_buffer, ((fio_float_s *)obj)->f, 10),
    };
  }
  return (fio_cstr_s){.buffer = NULL, .len = 0};
}

/* *****************************************************************************
Symbol API
***************************************************************************** */

/** Creates a Symbol object. Use `fiobj_free`. */
fiobj_s *fiobj_sym_new(const char *str, size_t len) {
  return fiobj_alloc(FIOBJ_T_SYMBOL, len, (void *)str);
}

/** Creates a Symbol object using a printf like interface. */
__attribute__((format(printf, 1, 0))) fiobj_s *
fiobj_symvprintf(const char *restrict format, va_list argv) {
  fiobj_s *sym = NULL;
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len == 0)
    sym = fiobj_alloc(FIOBJ_T_SYMBOL, 0, (void *)"");
  if (len <= 0)
    return sym;
  sym = fiobj_alloc(FIOBJ_T_SYMBOL, len, NULL); /* adds 1 to len, for NUL */
  vsnprintf(((fio_sym_s *)(sym))->str, len + 1, format, argv);
  ((fio_sym_s *)(sym))->hash = fio_ht_hash(((fio_sym_s *)(sym))->str, len);
  return sym;
}
__attribute__((format(printf, 1, 2))) fiobj_s *
fiobj_symprintf(const char *restrict format, ...) {
  va_list argv;
  va_start(argv, format);
  fiobj_s *sym = fiobj_symvprintf(format, argv);
  va_end(argv);
  return sym;
}

/** Returns 1 if both Symbols are equal and 0 if not. */
int fiobj_sym_iseql(fiobj_s *sym1, fiobj_s *sym2) {
  if (sym1->type != FIOBJ_T_SYMBOL || sym2->type != FIOBJ_T_SYMBOL)
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

  /* FIFO support optimizes smaller FIFO ranges over bloating allocations. */
  if (needed == 1 && obj2ary(ary)->start >= (obj2ary(ary)->capa >> 1)) {
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
  //
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
   * living without memory... besides, malloc is optimistic these days. */
  obj2ary(ary)->arry =
      realloc(obj2ary(ary)->arry, updated_capa * sizeof(*obj2ary(ary)->arry));
  obj2ary(ary)->capa = updated_capa;

  if (needed >= 0) /* we're done, realloc grows the top of the address space*/
    return;

  /* move everything to the max, since  memmove could get expensive  */
  uint64_t len = obj2ary(ary)->end - obj2ary(ary)->start;
  needed = obj2ary(ary)->capa - len;
  memmove(obj2ary(ary)->arry + needed, obj2ary(ary)->arry + obj2ary(ary)->start,
          len * sizeof(*obj2ary(ary)->arry));
  obj2ary(ary)->end = needed + len;
  obj2ary(ary)->start = needed;
}

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void) { return fiobj_alloc(FIOBJ_T_ARRAY, 0, NULL); }

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
    return 0;
  return (obj2ary(ary)->end - obj2ary(ary)->start);
}

/**
 * Pushes an object to the end of the Array.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj) {
  if (!ary || ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }
  if (obj2ary(ary)->capa <= obj2ary(ary)->end)
    fiobj_ary_getmem(ary, 1);
  obj2ary(ary)->arry[(obj2ary(ary)->end)++] = obj;
}

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
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
  if (!ary || ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }
  if (obj2ary(ary)->start == 0)
    fiobj_ary_getmem(ary, -1);
  obj2ary(ary)->arry[--(obj2ary(ary)->start)] = obj;
}

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
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
  if (!ary || ary->type != FIOBJ_T_ARRAY)
    return NULL;

  if (pos >= 0) {
    pos = pos + obj2ary(ary)->start;
    if ((uint64_t)pos >= obj2ary(ary)->end)
      return NULL;
    return obj2ary(ary)->arry[pos];
  }

  pos = (int64_t)obj2ary(ary)->end + pos;
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
  if (!ary || ary->type != FIOBJ_T_ARRAY) {
    fiobj_free(obj);
    return;
  }

  /* test for memory and request memory if missing, promises valid bounds. */
  if (pos >= 0) {
    if ((uint64_t)pos + obj2ary(ary)->start >= obj2ary(ary)->capa)
      fiobj_ary_getmem(ary, (((uint64_t)pos + obj2ary(ary)->start) -
                             (obj2ary(ary)->capa - 1)));
  } else if (pos + (int64_t)obj2ary(ary)->end < 0)
    fiobj_ary_getmem(ary, pos + obj2ary(ary)->end);

  if (pos >= 0) {
    /* position relative to start */
    pos = pos + obj2ary(ary)->start;
    /* initialize empty spaces, if any, setting new boundries */
    while ((uint64_t)pos >= obj2ary(ary)->end)
      obj2ary(ary)->arry[(obj2ary(ary)->end)++] = NULL;
  } else {
    /* position relative to end */
    pos = pos + (int64_t)obj2ary(ary)->end;
    /* initialize empty spaces, if any, setting new boundries */
    while (obj2ary(ary)->start >= (uint64_t)pos)
      obj2ary(ary)->arry[--(obj2ary(ary)->start)] = NULL;
  }

  /* check for an existing object and set new objects */
  if (obj2ary(ary)->arry[pos])
    fiobj_free(obj2ary(ary)->arry[pos]);
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
fiobj_s *fiobj_hash_new(void) { return fiobj_alloc(FIOBJ_T_HASH, 0, NULL); }

/** Returns the number of elements in the Hash. */
size_t fiobj_hash_count(fiobj_s *hash) {
  if (!hash || hash->type != FIOBJ_T_HASH)
    return 0;
  return ((fio_hash_s *)hash)->h.count;
}

/**
 * Sets a key-value pair in the Hash, duplicating the Symbol and **moving**
 * the ownership of the object to the Hash.
 *
 * Returns -1 on error.
 */
int fiobj_hash_set(fiobj_s *hash, fiobj_s *sym, fiobj_s *obj) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    fiobj_dealloc((fiobj_s *)obj);
    return -1;
  }
  fiobj_s *coup = fiobj_alloc(FIOBJ_T_COUPLET, 0, NULL);
  ((fio_couplet_s *)coup)->name = fiobj_dup(sym);
  ((fio_couplet_s *)coup)->obj = obj;
  fio_ht_add(&((fio_hash_s *)hash)->h, &((fio_couplet_s *)coup)->node,
             ((fio_sym_s *)sym)->hash);
  return 0;
}

/**
 * Removes a key-value pair from the Hash, if it exists, returning the old
 * object (instead of freeing it).
 */
fiobj_s *fiobj_hash_remove(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL)
    return NULL;
  fio_couplet_s *coup =
      (void *)fio_ht_pop(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup)
    return NULL;
  coup = fio_node2obj(fio_couplet_s, node, coup);
  fiobj_s *obj = fiobj_dup(coup->obj);
  fiobj_dealloc((fiobj_s *)coup);
  return obj;
}

/**
 * Deletes a key-value pair from the Hash, if it exists, freeing the
 * associated object.
 *
 * Returns -1 on type error or if the object never existed.
 */
int fiobj_hash_delete(fiobj_s *hash, fiobj_s *sym) {
  fiobj_s *obj = fiobj_hash_remove(hash, sym);
  if (!obj)
    return -1;
  fiobj_free(obj);
  return 0;
}

/**
 * Returns a temporary handle to the object associated with the Symbol, NULL
 * if none.
 */
fiobj_s *fiobj_hash_get(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return NULL;
  }
  fio_couplet_s *coup =
      (void *)fio_ht_find(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup) {
    return NULL;
  }
  coup = fio_node2obj(fio_couplet_s, node, coup);
  return coup->obj;
}

/**
 * Returns 1 if the key (Symbol) exists in the Hash, even if value is NULL.
 */
int fiobj_hash_haskey(fiobj_s *hash, fiobj_s *sym) {
  if (hash->type != FIOBJ_T_HASH || sym->type != FIOBJ_T_SYMBOL) {
    return 0;
  }
  fio_couplet_s *coup =
      (void *)fio_ht_find(&((fio_hash_s *)hash)->h, ((fio_sym_s *)sym)->hash);
  if (!coup) {
    return 0;
  }
  return 1;
}

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the key
 * (Symbol) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2key(fiobj_s *obj) {
  if (!obj || obj->type != FIOBJ_T_COUPLET)
    return NULL;
  return ((fio_couplet_s *)obj)->name;
}

/**
 * If object is a Hash couplet (occurs in `fiobj_each2`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_couplet2obj(fiobj_s *obj) {
  if (!obj || obj->type != FIOBJ_T_COUPLET)
    return obj;
  return ((fio_couplet_s *)obj)->obj;
}

/* *****************************************************************************
A touch of testing
***************************************************************************** */
#ifdef DEBUG

// #include "fio2resp.h"

static int fiobj_test_array_task(fiobj_s *obj, void *arg) {
  static __thread uintptr_t count = 0;
  static __thread const char *stop = ".";
  static __thread fio_ls_s nested = {NULL, NULL, NULL};
  if (!nested.next)
    nested = (fio_ls_s)FIO_LS_INIT(nested);

  if (obj && obj->type == FIOBJ_T_ARRAY) {
    fio_ls_push(&nested, (fiobj_s *)stop);
    if (!count) {
      fio_ls_push(&nested, (fiobj_s *)count);
      fprintf(stderr, "\n* Array data: [");
    } else {
      fio_ls_push(&nested, (fiobj_s *)(--count));
      fprintf(stderr, "[ ");
    }
    stop = " ]";
    count = fiobj_ary_count(obj);
    return 0;
  } else if (obj && obj->type == FIOBJ_T_HASH) {
    fio_ls_push(&nested, (fiobj_s *)stop);
    fio_ls_push(&nested, (fiobj_s *)count);
    if (!count)
      fprintf(stderr, "\n* Hash data: {");
    else {
      fprintf(stderr, "{ ");
    }
    stop = " }";
    count = fiobj_hash_count(obj);
    return 0;
  }
  if (obj && obj->type == FIOBJ_T_COUPLET) {
    fprintf(stderr, "%s:", fiobj_obj2cstr(fiobj_couplet2key(obj)).data);
    fiobj_s *tmp = fiobj_couplet2obj(obj);
    if (tmp->type == FIOBJ_T_HASH) {
      obj = fiobj_couplet2key(obj);
      count += fiobj_hash_count(tmp);
    } else if (tmp->type == FIOBJ_T_ARRAY) {
      obj = fiobj_couplet2key(obj);
      count += fiobj_ary_count(tmp);
    }
  }
  if (--count) {
    fprintf(stderr, "%s, ", fiobj_obj2cstr(obj).data);
  } else {
    count = (uintptr_t)fio_ls_pop(&nested);
    if (count) {
      fprintf(stderr, "%s %s, ", fiobj_obj2cstr(obj).data, stop);
      stop = (char *)fio_ls_pop(&nested);
    } else {
      fprintf(stderr, "%s %s ", fiobj_obj2cstr(obj).data, stop);
      stop = (char *)fio_ls_pop(&nested);
      if (stop[0] != '.')
        fprintf(stderr, "%s", stop);
      else
        fprintf(stderr, "\n (should be 127..0)\n");
    }
  }

  return 0;
  (void)arg;
}

/* test were written for OSX (fprintf types) with clang (%s for NULL is okay)
 */
void fiobj_test(void) {
  fiobj_s *obj;
  size_t i;
  fprintf(stderr, "\n===\nStarting fiobj basic testing:\n");

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
    fprintf(stderr, "* testing Array. \n");
    for (size_t i = 0; i < 128; i++) {
      fiobj_ary_unshift(obj, fiobj_num_new(i));
      if (fiobj_ary_count(obj) != i + 1)
        fprintf(stderr, "* FAILED Array count. %lu/%llu != %lu\n",
                fiobj_ary_count(obj), obj2ary(obj)->capa, i + 1);
    }
    fiobj_each2(obj, fiobj_test_array_task, NULL);
    fiobj_free(obj);
  } else {
    fprintf(stderr, "* FAILED to initialize Array test!\n");
    fiobj_free(obj);
  }

  /* test hash */
  obj = fiobj_hash_new();
  if (obj->type == FIOBJ_T_HASH) {
    fprintf(stderr, "* testing Hash. \n");
    fiobj_s *syms = fiobj_ary_new();
    fiobj_s *a = fiobj_ary_new();
    fiobj_s *tmp;
    fiobj_ary_push(a, fiobj_str_new("String in a nested Array", 24));
    tmp = fiobj_sym_new("array", 5);
    fiobj_hash_set(obj, tmp, a);
    fiobj_free(tmp);

    for (size_t i = 0; i < 128; i++) {
      tmp = fiobj_num_new(i);
      fio_cstr_s s = fiobj_obj2cstr(tmp);
      fiobj_ary_set(syms, fiobj_sym_new(s.buffer, s.len), i);
      fiobj_hash_set(obj, fiobj_ary_entry(syms, i), tmp);
      if (fiobj_hash_count(obj) != i + 2)
        fprintf(stderr, "* FAILED Hash count. %lu != %lu\n",
                fiobj_hash_count(obj), i + 2);
    }

    if (OBJ2HEAD(fiobj_ary_entry(syms, 2)).ref < 2)
      fprintf(stderr, "* FAILED Hash Symbol duplication.\n");

    for (size_t i = 0; i < 128; i++) {
      if ((size_t)fiobj_obj2num(
              fiobj_hash_get(obj, fiobj_ary_entry(syms, i))) != i)
        fprintf(stderr,
                "* FAILED to retrive data from hash for Symbol %s (%p): %lld "
                "(%p) type %d\n",
                fiobj_obj2cstr(fiobj_ary_entry(syms, i)).data,
                (void *)((fio_sym_s *)fiobj_ary_entry(syms, i))->hash,
                fiobj_obj2num(fiobj_hash_get(obj, fiobj_ary_entry(syms, i))),
                (void *)fiobj_hash_get(obj, fiobj_ary_entry(syms, i)),
                fiobj_hash_get(obj, fiobj_ary_entry(syms, i))
                    ? fiobj_hash_get(obj, fiobj_ary_entry(syms, i))->type
                    : 0);
    }

    fiobj_each2(obj, fiobj_test_array_task, NULL);

    // char tmp_buffer[4096];
    // size_t tmp_size = 4096;
    // if (resp_fioformat((uint8_t *)tmp_buffer, &tmp_size, obj))
    //   fprintf(stderr, "* notice, RESP formatting required more space
    //   (%lu).\n",
    //           tmp_size);
    // fprintf(stderr, "* RESP formatting:\n%.*s", (int)tmp_size, tmp_buffer);

    fiobj_free(obj);
    if (OBJ2HEAD(fiobj_ary_entry(syms, 2)).ref != 1)
      fprintf(stderr, "* FAILED Hash Symbol deallocation.\n");
    fiobj_free(syms);
  } else {
    fprintf(stderr, "* FAILED to initialize Hash test!\n");
    if (obj)
      fiobj_free(obj);
  }
  obj = NULL;

  /* test cyclic protection */
  {
    fprintf(stderr, "* testing cyclic protection. \n");
    fiobj_s *a1 = fiobj_ary_new();
    fiobj_s *a2 = fiobj_ary_new();
    for (size_t i = 0; i < 129; i++) {
      obj = fiobj_num_new(1024 + i);
      fiobj_ary_push(a1, fiobj_num_new(i));
      fiobj_ary_unshift(a2, fiobj_num_new(i));
      fiobj_ary_push(a1, fiobj_dup(obj));
      fiobj_ary_unshift(a2, obj);
    }
    fiobj_ary_push(a1, a2); /* the intentionally offending code */
    fiobj_ary_push(a2, a1);
    fprintf(stderr,
            "* Printing cyclic array references with "
            "a1, pos %llu  == a2 and a2, pos %llu == a1\n",
            obj2ary(a1)->end, obj2ary(a2)->end);
    fiobj_each2(a1, fiobj_test_array_task, NULL);

    obj = fiobj_dup(fiobj_ary_entry(a2, -3));
    if (!obj || obj->type != FIOBJ_T_NUMBER)
      fprintf(stderr, "* FAILED unexpected object %p with type %d\n",
              (void *)obj, obj ? obj->type : 0);
    if (OBJ2HEAD(obj).ref < 2)
      fprintf(stderr, "* FAILED object reference counting test (%llu)\n",
              OBJ2HEAD(obj).ref);
    fiobj_free(a1); /* frees both... */
    // fiobj_free(a2);
    if (OBJ2HEAD(obj).ref != 1)
      fprintf(stderr,
              "* FAILED to free cyclic nested "
              "array members (%llu)\n ",
              OBJ2HEAD(obj).ref);
    fiobj_free(obj);
  }

  /* test deep nesting */
  {
    fprintf(stderr, "* testing deep array nesting. \n");
    fiobj_s *top = fiobj_ary_new();
    fiobj_s *pos = top;
    for (size_t i = 0; i < 128; i++) {
      for (size_t j = 0; j < 128; j++) {
        fiobj_ary_push(pos, fiobj_num_new(j));
      }
      fiobj_ary_push(pos, fiobj_ary_new());
      pos = fiobj_ary_entry(pos, -1);
      if (!pos || pos->type != FIOBJ_T_ARRAY) {
        fprintf(stderr, "* FAILED Couldn't retrive position -1 (%d)\n",
                pos ? pos->type : 0);
        break;
      }
    }
    fiobj_free(top); /* frees both... */
  }
}
#endif
