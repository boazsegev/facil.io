/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Array memory management
***************************************************************************** */

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

/* *****************************************************************************
Array API
***************************************************************************** */

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void) { return fiobj_alloc(FIOBJ_T_ARRAY, 0, NULL); }

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
    return 0;
  return (obj2ary(ary)->end - obj2ary(ary)->start);
}

/* *****************************************************************************
Array direct entry access API
***************************************************************************** */

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
    while (obj2ary(ary)->start > (uint64_t)pos)
      obj2ary(ary)->arry[--(obj2ary(ary)->start)] = NULL;
  }

  /* check for an existing object and set new objects */
  if (obj2ary(ary)->arry[pos])
    fiobj_free(obj2ary(ary)->arry[pos]);
  obj2ary(ary)->arry[pos] = obj;
}

/* *****************************************************************************
Array push / shift API
***************************************************************************** */

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

/* *****************************************************************************
Array flattenning
***************************************************************************** */

static int fiobj_ary_flatten_task(fiobj_s *obj, void *a_) {
  if (obj == a_) {
    obj2ary(a_)->start = 0;
    obj2ary(a_)->end = 0;
    return 0;
  }
  if (obj->type == FIOBJ_T_HASH || obj->type == FIOBJ_T_ARRAY) {
    fiobj_dealloc(obj);
    return 0;
  }
  if (obj->type == FIOBJ_T_COUPLET) {
    fiobj_ary_push(a_, obj2couplet(obj)->name);
    fiobj_ary_push(a_, obj2couplet(obj)->obj);
    fiobj_dealloc(obj);
    return 0;
  }
  fiobj_ary_push(a_, obj);
  return 0;
}

/**
 * Flattens an Array, making it single dimentional.
 *
 * Other Arrays are simply unnested inplace.
 *
 * Hashes are treated as a multi-dimentional Array:
 * `[[key,value],[key,value],..]`.
 */
void fiobj_ary_flatten(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
    return;
  fiobj_each2(ary, fiobj_ary_flatten_task, ary);
}
