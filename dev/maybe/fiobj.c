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
  long double f;
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

typedef struct {
  uint64_t ref;
  fiobj_s *next;
  fiobj_s *prev;
} fiobj_head_s;

#define OBJ2HEAD(o) (((fiobj_head_s *)(o)) - 1)[0]
#define HEAD2OBJ(o) ((fiobj_s *)(((fiobj_head_s *)(o)) + 1))

#define LISTINIT(o)                                                            \
  do {                                                                         \
    OBJ2HEAD(o).next = o;                                                      \
    OBJ2HEAD(o).prev = o;                                                      \
  } while (0);

inline static void objlist_push(fiobj_s *dest, fiobj_s *obj) {
  if (!obj || !dest)
    return;
  OBJ2HEAD(obj).prev = OBJ2HEAD(dest).prev;
  OBJ2HEAD(obj).next = dest;
  OBJ2HEAD(dest).prev = obj;
  OBJ2HEAD(OBJ2HEAD(obj).prev).next = obj;
}
inline static fiobj_s *objlist_shift(fiobj_s *from) {
  if (OBJ2HEAD(from).next == from)
    return NULL;
  fiobj_s *ret = OBJ2HEAD(from).next;
  OBJ2HEAD(OBJ2HEAD(ret).next).prev = OBJ2HEAD(ret).prev;
  OBJ2HEAD(OBJ2HEAD(ret).prev).next = OBJ2HEAD(ret).next;
  return ret;
}

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
    ((fio_float_s *)HEAD2OBJ(head))->f = ((long double *)buffer)[0];
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
                    .arry = malloc(sizeof(fiobj_s *) * 8),
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
  if (spn_sub(&OBJ2HEAD(obj).ref, 1))
    return;
  switch (obj->type) {
  case FIOBJ_T_HASH_COUPLET:
    fiobj_dealloc(((fio_couplet_s *)obj)->name);
    fiobj_dealloc(((fio_couplet_s *)obj)->obj);
    fio_ht_remove(&((fio_couplet_s *)obj)->node);
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
  spn_add(&OBJ2HEAD(obj).ref, 1);
  fiobj_dealloc(obj);
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
void fiobj_free(fiobj_s *obj) { fiobj_each(obj, dealloc_task_callback, NULL); }

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
  if (!task || !obj)
    return;
  fiobj_head_s head[2]; /* memory allocation */

  fiobj_s *list = HEAD2OBJ(&head);
  fiobj_s *processed = HEAD2OBJ((head + 1));
  LISTINIT(list);
  LISTINIT(processed);

  fiobj_s *tmp = obj;
  while (tmp) {
    switch (tmp->type) {
    case FIOBJ_T_ARRAY: {
      /* test against cyclic nesting */
      fiobj_s *pos = OBJ2HEAD(processed).next;
      while (pos != processed) {
        if (pos == tmp)
          goto skip;
        pos = OBJ2HEAD(pos).next;
      }
      fio_ary_s *a = (fio_ary_s *)tmp;

      for (size_t i = a->start; i < a->end; i++) {
        if (a->arry[i])
          objlist_push(list, a->arry[i]);
      }
      objlist_push(processed, tmp);
      break;
    }
    case FIOBJ_T_HASH: {
      /* test against cyclic nesting */
      fiobj_s *pos = OBJ2HEAD(processed).next;
      while (pos != processed) {
        if (pos == tmp)
          goto skip;
        pos = OBJ2HEAD(pos).next;
      }
      fio_hash_s *h = (fio_hash_s *)tmp;
      fio_couplet_s *i;
      fio_ht_for_each(fio_couplet_s, node, i, h->h) {
        if (i)
          objlist_push(list, (fiobj_s *)i);
      }
      objlist_push(processed, tmp);
      break;
    }
    default:
      break;
    }
    task(tmp, arg);
  skip:
    tmp = objlist_shift(list);
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
        tmp = str[0] - 'A';
      else if (str[0] >= 'a' && str[0] <= 'f')
        tmp = str[0] - 'a';
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
    end--;
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
long double fio_atof(const char *str) { return strtold(str, NULL); }

/** Creates a Number object. Remember to use `fiobj_free`. */
fiobj_s *fiobj_num_new(int64_t num) {
  return fiobj_alloc(FIOBJ_T_NUMBER, 0, &num);
}

/** Creates a Float object. Remember to use `fiobj_free`.  */
fiobj_s *fiobj_float_new(long double num) {
  return fiobj_alloc(FIOBJ_T_FLOAT, 0, &num);
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
long double fiobj_obj2float(fiobj_s *obj) {
  if (obj->type == FIOBJ_T_NUMBER)
    return (long double)((fio_num_s *)obj)->i;
  if (obj->type == FIOBJ_T_FLOAT)
    return ((fio_float_s *)obj)->f;
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
 * A helper function that convers between a signed int64_t to a String.
 *
 * No overflow guard is provided, make sure there's at least 68 bytes available
 * (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
 */
size_t fio_ltoa(char *dest, int64_t num, uint8_t base);

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
        .len = fio_ftoa(num_buffer, ((fio_num_s *)obj)->i, 10),
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
  ;
}

/** Returns 1 if both Symbols are equal and 0 if not. */
int fiobj_sym_iseql(fiobj_s *sym1, fiobj_s *sym2) {
  return (((fio_sym_s *)sym1)->hash == ((fio_sym_s *)sym2)->hash);
}

/* *****************************************************************************
IO API
***************************************************************************** */

/** Wrapps a file descriptor in an IO object. Use `fiobj_free` to close. */
fiobj_s *fio_io_wrap(intptr_t fd);

/**
 * Return an IO's fd.
 *
 * A type error results in -1.
 */
intptr_t fiobj_io_fd(fiobj_s *obj);

/* *****************************************************************************
File API
***************************************************************************** */

/** Wrapps a `FILe` pointer in a File object. Use `fiobj_free` to close. */
fiobj_s *fio_file_wrap(FILE *fd);

/**
 * Returns a temporary `FILE` pointer.
 *
 * A type error results in NULL.
 */
FILE *fiobj_file(fiobj_s *obj);

/* *****************************************************************************
Array API
***************************************************************************** */

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void);

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary);

/**
 * Pushes an object to the end of the Array.
 *
 * The Array now owns the object. Use `fiobj_dup` to push a copy if
 * required.
 */
void fiobj_ary_push(fiobj_s *ary, fiobj_s *obj);

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary);

/**
 * Unshifts an object to the begining of the Array. This could be
 * expensive.
 *
 * The Array now owns the object. Use `fiobj_dup` to push a copy if
 * required.
 */
void fiobj_ary_unshift(fiobj_s *ary, fiobj_s *obj);

/** Shifts an object from the beginning of the Array. */
fiobj_s *fiobj_ary_shift(fiobj_s *ary);

/**
 * Returns a temporary object owned by the Array.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
fiobj_s *fiobj_ary_entry(fiobj_s *ary, int64_t pos);

/**
 * Sets an object at the requested position.
 *
 * If the position overflows the current array size, all intermediate
 * positions will be set to NULL and the Array will grow in size.
 *
 * The old object (if any) occupying the same space will be freed.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 *
 * Returns -1 on error (i.e., object not an Array).
 */
int fiobj_ary_set(fiobj_s *ary, fiobj_s *obj, int64_t pos);

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
fiobj_s *fiobj_hash_sym(fiobj_s *obj);

/**
 * If object is a Hash couplet (occurs in `fiobj_each`), returns the object
 * (the value) from the key-value pair.
 *
 * Otherwise returns NULL.
 */
fiobj_s *fiobj_hash_obj(fiobj_s *obj);
