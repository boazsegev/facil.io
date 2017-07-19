/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Generic Object API
***************************************************************************** */

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
  if (obj->type == FIOBJ_T_STRING) {
    char *s = ((fio_str_s *)obj)->str;
    return fio_atol(&s);
  }
  if (obj->type == FIOBJ_T_TRUE)
    return 1;
  if (obj->type == FIOBJ_T_SYMBOL) {
    char *s = ((fio_sym_s *)obj)->str;
    return fio_atol(&s);
  }
  if (obj->type == FIOBJ_T_ARRAY)
    return fiobj_ary_count(obj);
  if (obj->type == FIOBJ_T_HASH)
    return fiobj_hash_count(obj);
  if (obj->type == FIOBJ_T_IO)
    return (int64_t)((fio_io_s *)obj)->fd;
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
  if (obj->type == FIOBJ_T_STRING) {
    char *s = ((fio_str_s *)obj)->str;
    return fio_atof(&s);
  }
  if (obj->type == FIOBJ_T_TRUE)
    return 1;
  if (obj->type == FIOBJ_T_SYMBOL) {
    char *s = ((fio_str_s *)obj)->str;
    return fio_atof(&s);
  }
  if (obj->type == FIOBJ_T_ARRAY)
    return (double)fiobj_ary_count(obj);
  if (obj->type == FIOBJ_T_HASH)
    return (double)fiobj_hash_count(obj);
  if (obj->type == FIOBJ_T_IO)
    return (double)((fio_io_s *)obj)->fd;
  return 0;
}

/**
 * Returns a C String (NUL terminated) using the `fio_cstr_s` data type.
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
Object Iteration (`fiobj_each2`)
***************************************************************************** */

static uint8_t already_processed(fiobj_s *nested, fiobj_s *obj) {
#if FIOBJ_NESTING_PROTECTION
  size_t end = obj2ary(nested)->end;
  for (size_t i = obj2ary(nested)->start; i < end; i++) {
    if (obj2ary(nested)->arry[i] == obj)
      return 1;
  }
#endif
  OBJREF_ADD(obj);
  fiobj_ary_push(nested, obj);
  return 0;
}

/**
 * Deep itteration using a callback for each fio object, including the parent.
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 */
void fiobj_each2(fiobj_s *obj, int (*task)(fiobj_s *obj, void *arg),
                 void *arg) {
  /* optimize simple items */
  if (!obj || (obj->type != FIOBJ_T_ARRAY && obj->type != FIOBJ_T_HASH)) {
    task(obj, arg);
    return;
  }
  /* Prepare for layerd iteration */
  fiobj_s *child = NULL;
  uintptr_t count = 0;
  fiobj_s *nested = fiobj_ary_new2(64);
  fiobj_s *state = fiobj_ary_new2(128);
  OBJREF_ADD(obj);
  fiobj_ary_push(nested, obj);
  if (task(obj, arg) == -1)
    goto finish;
rebase:
  switch (obj->type) {
  case FIOBJ_T_ARRAY: {
    size_t i;
    size_t end = obj2ary(obj)->end;
    /* resume ? */
    if (count) {
      i = count;
      // count = 0;
    } else {
      i = obj2ary(obj)->start;
    }
    /* iteration over elements */
    for (; i < end; i++) {
      if (obj2ary(obj)->arry[i] &&
          (obj2ary(obj)->arry[i]->type == FIOBJ_T_ARRAY ||
           obj2ary(obj)->arry[i]->type == FIOBJ_T_HASH)) {
        if (already_processed(nested, obj2ary(obj)->arry[i])) {
          if (task(NULL, arg) == -1) {
            goto finish;
          }
          continue;
        }
        if (task(obj2ary(obj)->arry[i], arg) == -1) {
          goto finish;
        }
        count = i + 1;
        child = obj2ary(obj)->arry[i];
        goto dig_deeper;
      }
      if (task(obj2ary(obj)->arry[i], arg) == -1) {
        goto finish;
      }
    }
    break;
  }
  case FIOBJ_T_HASH: {
    fio_ls_s *pos;
    /* resume ? */
    if (count) {
      pos = ((fio_ls_s *)count);
      count = 0;
    } else {
      pos = obj2hash(obj)->items.next;
    }
    /* itteration over elements */
    while (pos != &obj2hash(obj)->items) {
      // if (pos->obj == NULL) {
      //   /* an item was removed?! */
      //   pos = pos->next;
      //   continue;
      // }
      child = obj2couplet(pos->obj)->obj;
      if (child &&
          (child->type == FIOBJ_T_ARRAY || child->type == FIOBJ_T_HASH)) {
        if (already_processed(nested, child)) {
          if (task(NULL, arg) == -1) {
            goto finish;
          }
          pos = pos->next;
          continue;
        }
        if (task(pos->obj, arg) == -1) {
          goto finish;
        }
        count = (uintptr_t)pos->next;
        goto dig_deeper;
      }
      if (task(pos->obj, arg) == -1) {
        goto finish;
      }
      pos = pos->next;
    }
    break;
  }
  default:
    break;
  }

  /* any more nested layers left to handle? */
  if (fiobj_ary_count(state)) {
    count = (uintptr_t)fiobj_ary_pop(state);
    obj = fiobj_ary_pop(state);
    goto rebase;
  }

finish:
  while ((obj = fiobj_ary_pop(nested)))
    fiobj_dealloc(obj);
  fiobj_dealloc(nested);
  fiobj_dealloc(state);
  return;

dig_deeper:
  fiobj_ary_push(state, obj);
  fiobj_ary_push(state, (void *)count);
  count = 0;
  obj = child;
  goto rebase;
}

/* *****************************************************************************
Object Comparison (`fiobj_iseq`)
***************************************************************************** */

static int fiobj_iseq_check(fiobj_s *obj1, fiobj_s *obj2) {
  if (obj1 == obj2)
    return 1;
  if (!obj1 || !obj2)
    return 0;
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
  if (obj1 == obj2)
    return 1;
  if (!obj1 || !obj2)
    return 0;
  if (obj1->type != obj2->type)
    return 0;
  if (obj1->type != FIOBJ_T_ARRAY && obj1->type != FIOBJ_T_HASH &&
      obj1->type != FIOBJ_T_COUPLET)
    return fiobj_iseq_check(obj1, obj2);

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
      /* walk in reverse */
      fio_ls_s *hash_pos = obj2hash(obj1)->items.prev;
      while (hash_pos != &obj2hash(obj1)->items) {
        fio_ls_unshift(&pos1, hash_pos->obj);
        hash_pos = hash_pos->prev;
      }
      hash_pos = obj2hash(obj2)->items.prev;
      while (hash_pos != &obj2hash(obj2)->items) {
        fio_ls_unshift(&pos2, hash_pos->obj);
        hash_pos = hash_pos->prev;
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
