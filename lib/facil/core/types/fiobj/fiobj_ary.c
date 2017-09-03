/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "fiobj_types.h"

/* *****************************************************************************
Array memory management
***************************************************************************** */

/* This funcation manages the Array's memory. */
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
  if (!obj2ary(ary)->arry)
    perror("ERROR: fiobj array couldn't be reallocated"), exit(errno);

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
VTable
***************************************************************************** */

static void fiobj_ary_dealloc(fiobj_s *a) {
  free(obj2ary(a)->arry);
  free(&OBJ2HEAD(a));
}

static size_t fiobj_ary_each1(fiobj_s *o, size_t start_at,
                              int (*task)(fiobj_s *obj, void *arg), void *arg) {
  const uint64_t start_pos = obj2ary(o)->start;
  start_at += start_pos;
  while (start_at < obj2ary(o)->end &&
         task(obj2ary(o)->arry[start_at++], arg) != -1)
    ;
  return start_at - start_pos;
}

static int fiobj_ary_is_eq(fiobj_s *self, fiobj_s *other) {
  return (other && other->type == FIOBJ_T_ARRAY &&
          obj2ary(self)->end - obj2ary(self)->start ==
              obj2ary(other)->end - obj2ary(other)->start);
}

/** Returns the number of elements in the Array. */
static size_t fiobj_ary_count_items(fiobj_s *ary) {
  return (obj2ary(ary)->end - obj2ary(ary)->start);
}

static struct fiobj_vtable_s FIOBJ_VTABLE_ARRAY = {
    .free = fiobj_ary_dealloc,
    .to_i = fiobj_noop_i,
    .to_f = fiobj_noop_f,
    .to_str = fiobj_noop_str,
    .is_eq = fiobj_ary_is_eq,
    .count = fiobj_ary_count_items,
    .each1 = fiobj_ary_each1,
};
/* *****************************************************************************
Allocation
***************************************************************************** */

static fiobj_s *fiobj_ary_alloc(size_t capa, size_t start_at) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_ary_s));
  if (!head)
    perror("ERROR: fiobj array couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_ARRAY,
  };
  *((fio_ary_s *)HEAD2OBJ(head)) =
      (fio_ary_s){.start = start_at,
                  .end = start_at,
                  .capa = capa,
                  .arry = malloc(sizeof(fiobj_s *) * capa),
                  .type = FIOBJ_T_ARRAY};
  return HEAD2OBJ(head);
}

/** Creates a mutable empty Array object. Use `fiobj_free` when done. */
fiobj_s *fiobj_ary_new(void) { return fiobj_ary_alloc(32, 8); }
/** Creates a mutable empty Array object with the requested capacity. */
fiobj_s *fiobj_ary_new2(size_t capa) { return fiobj_ary_alloc(capa, 0); }

/** Returns the number of elements in the Array. */
size_t fiobj_ary_count(fiobj_s *ary) {
  if (!ary)
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
  /* position is relative to `start`*/
  if (pos >= 0) {
    pos = pos + obj2ary(ary)->start;
    if ((uint64_t)pos >= obj2ary(ary)->end)
      return NULL;
    return obj2ary(ary)->arry[pos];
  }
  /* position is relative to `end`*/
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
    /* function takes ownership of memory even if an error occurs. */
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
    /* function takes ownership of memory even if an error occurs. */
    fiobj_free(obj);
    return;
  }
  if (obj2ary(ary)->capa <= obj2ary(ary)->end)
    fiobj_ary_getmem(ary, 1);
  obj2ary(ary)->arry[(obj2ary(ary)->end)++] = obj;
}

/** Pops an object from the end of the Array. */
fiobj_s *fiobj_ary_pop(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY ||
      obj2ary(ary)->start == obj2ary(ary)->end)
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
    /* function takes ownership of memory even if an error occurs. */
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
Array compacting (untested)
***************************************************************************** */

/**
 * Removes any NULL *pointers* from an Array, keeping all Objects (including
 * explicit NULL objects) in the array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
void fiobj_ary_compact(fiobj_s *ary) {
  if (!ary || ary->type != FIOBJ_T_ARRAY)
    return;
  fiobj_s **reader = obj2ary(ary)->arry + obj2ary(ary)->start;
  fiobj_s **writer = obj2ary(ary)->arry + obj2ary(ary)->start;
  while (reader < (obj2ary(ary)->arry + obj2ary(ary)->end)) {
    if (*reader == NULL) {
      reader++;
      continue;
    }
    *(writer++) = *(reader++);
  }
  obj2ary(ary)->end = (uint64_t)(writer - obj2ary(ary)->arry);
}
