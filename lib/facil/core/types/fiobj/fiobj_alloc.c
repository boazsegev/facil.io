/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Object Deallocation
***************************************************************************** */

void fiobj_dealloc(fiobj_s *obj) {
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
  case FIOBJ_T_ARRAY:
    free(((fio_ary_s *)obj)->arry);
    goto common;
  case FIOBJ_T_HASH:
    /* the actual objects are handled by the deep `fiobj_each2` dealloc task */
    while (fio_ls_pop(&obj2hash(obj)->items))
      ;
    free(obj2hash(obj)->map.data);
    obj2hash(obj)->map.data = NULL;
    obj2hash(obj)->map.capa = 0;
    goto common;
  case FIOBJ_T_IO:
    close(((fio_io_s *)obj)->fd);
    goto common;
  case FIOBJ_T_STRING:
    if (obj2str(obj)->is_static == 0)
      free(((fio_str_s *)obj)->str);
    goto common;

  common:
  case FIOBJ_T_COUPLET:
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
Deep Allocation (`fiobj_dup`)
***************************************************************************** */

/* simply increrase the reference count for each object. */
static int dup_task_callback(fiobj_s *obj, void *arg) {
  if (!obj)
    return 0;
  spn_add(&OBJ2HEAD(obj).ref, 1);
  if (obj->type == FIOBJ_T_COUPLET) {
    spn_add(&OBJ2HEAD((((fio_couplet_s *)obj)->name)).ref, 1);
    spn_add(&OBJ2HEAD((((fio_couplet_s *)obj)->obj)).ref, 1);
  }
  return 0;
  (void)arg;
}

/** Increases an object's reference count. */
fiobj_s *fiobj_dup(fiobj_s *obj) {
  fiobj_each2(obj, dup_task_callback, NULL);
  return obj;
}

/* *****************************************************************************
Deep Deallocation (`fiobj_free`)
***************************************************************************** */

static int dealloc_task_callback(fiobj_s *obj, void *arg) {
  if (obj && obj->type == FIOBJ_T_COUPLET) {
    fiobj_dealloc(((fio_couplet_s *)obj)->obj);
    fiobj_dealloc(((fio_couplet_s *)obj)->name);
  }
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
  if (!obj)
    return;
  fiobj_each2(obj, dealloc_task_callback, NULL);
}
