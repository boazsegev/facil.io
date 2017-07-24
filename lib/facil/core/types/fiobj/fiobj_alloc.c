/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

/* *****************************************************************************
This file handles deallocation concerns.

Since Hash and Array objects will need to deallocate their children, the logic
regareding object type recorgnition and deallocation was centrelized here.
***************************************************************************** */
#include "fiobj_types.h"

/* *****************************************************************************
Object Deallocation
***************************************************************************** */

void fiobj_dealloc(fiobj_s *obj) {
  if (!obj)
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
  OBJ2HEAD(obj).vtable->free(obj);
}

/* *****************************************************************************
Deep Allocation (`fiobj_dup`)
***************************************************************************** */

/* simply increrase the reference count for each object. */
static int dup_task_callback(fiobj_s *obj, void *arg) {
  if (!obj)
    return 0;
  spn_add(&OBJ2HEAD(obj).ref, 1);
  // if (obj->type == FIOBJ_T_COUPLET)
  //   spn_add(&OBJ2HEAD(obj2couplet(obj)->obj).ref, 1);
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
  // if (!obj)
  //   return 0;
  // if (obj->type == FIOBJ_T_COUPLET)
  //   fiobj_dealloc(obj2couplet(obj)->obj);
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
