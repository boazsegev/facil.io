#ifndef H_FIO_ARRAY_H
/*
Copyright: Boaz Segev, 2017-2018
License: MIT
*/

/**
 * A Dynamic Array for general use (void * pointers).
 *
 * The file was written to be compatible with C++ as well as C, hence some
 * pointer casting.
 *
 * Use:

fio_ary_s ary;                   // a container can be placed on the stack.
fio_ary_new(&ary);               // initialize the container
fio_ary_push(&ary, (void*)1 );   // add / remove / read data...
fio_ary_free(&ary)               // free any resources, not the container.


 */
#define H_FIO_ARRAY_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#ifdef __cplusplus
#define register
#endif

typedef struct fio_ary_s {
  size_t start;
  size_t end;
  size_t capa;
  void **arry;
} fio_ary_s;

/** this value can be used for lazy initialization. */
#define FIO_ARY_INIT ((fio_ary_s){.arry = NULL})

/* *****************************************************************************
Array API
***************************************************************************** */

/** Initializes the array and allocates memory for it's internal data. */
FIO_FUNC inline void fio_ary_new(fio_ary_s *ary, size_t capa);

/** Frees the array's internal data. */
FIO_FUNC inline void fio_ary_free(fio_ary_s *ary);

/** Returns the number of elements in the Array. */
FIO_FUNC inline size_t fio_ary_count(fio_ary_s *ary);

/** Returns the current, temporary, array capacity (it's dynamic). */
FIO_FUNC inline size_t fio_ary_capa(fio_ary_s *ary);

/**
 * Returns the object placed in the Array, if any. Returns NULL if no data or if
 * the index is out of bounds.
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
FIO_FUNC inline void *fio_ary_index(fio_ary_s *ary, intptr_t pos);

/** alias for `fiobj_ary_index` */
#define fio_ary_entry(a, p) fiobj_ary_index((a), (p))

/**
 * Sets an object at the requested position.
 *
 * Returns the old value, if any.
 *
 * If an error occurs, the same data passed to the function is returned.
 */
FIO_FUNC inline void *fio_ary_set(fio_ary_s *ary, void *data, intptr_t pos);

/**
 * Pushes an object to the end of the Array. Returns -1 on error.
 */
FIO_FUNC inline int fio_ary_push(fio_ary_s *ary, void *data);

/** Pops an object from the end of the Array. */
FIO_FUNC inline void *fio_ary_pop(fio_ary_s *ary);

/**
 * Unshifts an object to the beginning of the Array. Returns -1 on error.
 *
 * This could be expensive, causing `memmove`.
 */
FIO_FUNC inline int fio_ary_unshift(fio_ary_s *ary, void *data);

/** Shifts an object from the beginning of the Array. */
FIO_FUNC inline void *fio_ary_shift(fio_ary_s *ary);

/**
 * Iteration using a callback for each entry in the array.
 *
 * The callback task function must accept an the entry data as well as an opaque
 * user pointer.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
FIO_FUNC inline size_t fio_ary_each(fio_ary_s *ary, size_t start_at,
                                    int (*task)(void *pt, void *arg),
                                    void *arg);
/**
 * Removes any NULL *pointers* from an Array, keeping all other data in the
 * array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
FIO_FUNC inline void fio_ary_compact(fio_ary_s *ary);

/**
 * Iterates through the list using a `for` loop.
 *
 * Access the data with `pos.obj` and it's index with `pos.i`. the `pos`
 * variable can be named however you please.
 */
#define FIO_ARY_FOR(ary, pos)                                                  \
  if ((ary)->arry)                                                             \
    for (struct fio_ary_pos_for_loop_s pos = {0, (ary)->arry[(ary)->start]};   \
         (pos.i + (ary)->start) < (ary)->end &&                                \
         ((pos.obj = (ary)->arry[pos.i + (ary)->start]), 1);                   \
         (++pos.i))
struct fio_ary_pos_for_loop_s {
  unsigned long i;
  void *obj;
};

/* *****************************************************************************
Array creation API
***************************************************************************** */

FIO_FUNC inline void fio_ary_new(fio_ary_s *ary, size_t capa) {
  if (!capa)
    capa = 32;
  *ary = (fio_ary_s){.arry = (void **)malloc(capa * sizeof(*ary->arry)),
                     .capa = capa};
  if (!ary->arry) {
    perror("ERROR: facil.io dynamic array couldn't be allocated");
    exit(errno);
  }
}
FIO_FUNC inline void fio_ary_free(fio_ary_s *ary) {
  if (ary)
    free(ary->arry);
}

/* *****************************************************************************
Array memory management
***************************************************************************** */

/* This funcation manages the Array's memory. */
FIO_FUNC void fio_ary_getmem(fio_ary_s *ary, intptr_t needed) {
  /* we have enough memory, but we need to re-organize it. */
  if (needed == -1) {
    if (ary->end < ary->capa) {
      /* since allocation can be cheaper than memmove (depending on size),
       * we'll just shove everything to the end...
       */
      size_t len = ary->end - ary->start;
      memmove(ary->arry + ary->capa - len, ary->arry + ary->start,
              len * sizeof(*ary->arry));
      ary->start = ary->capa - len;
      ary->end = ary->capa;
      return;
    }
    /* add some breathing room for future `unshift`s */
    needed = 0 - ((ary->capa < 1024) ? (ary->capa >> 1) : 1024);

  } else if (needed == 1 && ary->start >= (ary->capa >> 1)) {
    /* FIFO support optimizes smaller FIFO ranges over bloating allocations. */
    size_t len = ary->end - ary->start;
    memmove(ary->arry + 2, ary->arry + ary->start, len * sizeof(*ary->arry));
    ary->start = 2;
    ary->end = len + 2;
    return;
  }

  /* alocate using exponential growth, up to single page size. */
  size_t updated_capa = ary->capa;
  size_t minimum = ary->capa + ((needed < 0) ? (0 - needed) : needed);
  while (updated_capa <= minimum)
    updated_capa =
        (updated_capa <= 4096) ? (updated_capa << 1) : (updated_capa + 4096);

  /* we assume memory allocation works. it's better to crash than to continue
   * living without memory... besides, malloc is optimistic these days. */
  ary->arry = (void **)realloc(ary->arry, updated_capa * sizeof(*ary->arry));
  ary->capa = updated_capa;
  if (!ary->arry) {
    perror("ERROR: facil.io dynamic array couldn't be reallocated");
    exit(errno);
  }

  if (needed >= 0) /* we're done, realloc grows the top of the address space*/
    return;

  /* move everything to the max, since  memmove could get expensive  */
  size_t len = ary->end - ary->start;
  memmove((ary->arry + ary->capa) - len, ary->arry + ary->start,
          len * sizeof(*ary->arry));
  ary->end = ary->capa;
  ary->start = ary->capa - len;
}
/** Creates a mutable empty Array object with the requested capacity. */
FIO_FUNC inline void fiobj_ary_init(fio_ary_s *ary) {
  *ary = (fio_ary_s){.arry = NULL};
}

/* *****************************************************************************
Array direct entry access API
***************************************************************************** */

/** Returns the number of elements in the Array. */
FIO_FUNC inline size_t fio_ary_count(fio_ary_s *ary) {
  return ary->end - ary->start;
}

/** Returns the current, temporary, array capacity (it's dynamic). */
FIO_FUNC inline size_t fio_ary_capa(fio_ary_s *ary) { return ary->capa; }

/**
 * Returns a temporary object owned by the Array.
 *
 * Wrap this function call within `fiobj_dup` to get a persistent handle. i.e.:
 *
 *     fiobj_dup(fiobj_ary_index(array, 0));
 *
 * Negative values are retrived from the end of the array. i.e., `-1`
 * is the last item.
 */
FIO_FUNC inline void *fio_ary_index(fio_ary_s *ary, intptr_t pos) {
  if (!ary || !ary->arry)
    return NULL;
  /* position is relative to `start`*/
  if (pos >= 0) {
    pos = pos + ary->start;
    if ((size_t)pos >= ary->end)
      return NULL;
    return ary->arry[pos];
  }
  /* position is relative to `end`*/
  pos = (intptr_t)ary->end + pos;
  if (pos < 0 || (size_t)pos < ary->start)
    return NULL;
  return ary->arry[pos];
}

/** alias for `fiobj_ary_index` */
#define fio_ary_entry(a, p) fiobj_ary_index((a), (p))

/**
 * Sets an object at the requested position.
 *
 * Returns the old value, if any.
 *
 * If an error occurs, the same data passed to the function is returned.
 */
FIO_FUNC inline void *fio_ary_set(fio_ary_s *ary, void *data, intptr_t pos) {
  if (!ary->arry) {
    fio_ary_new(ary, 0);
  }
  void *old = NULL;
  /* test for memory and request memory if missing, promises valid bounds. */
  if (pos >= 0) {
    if ((size_t)pos + ary->start >= ary->capa)
      fio_ary_getmem(ary, (((size_t)pos + ary->start) - (ary->capa - 1)));
  } else if (pos + (intptr_t)ary->end < 0)
    fio_ary_getmem(ary, pos + ary->end);

  if (pos >= 0) {
    /* position relative to start */
    pos = pos + ary->start;
    /* initialize empty spaces, if any, setting new boundries */
    while ((size_t)pos >= ary->end)
      ary->arry[(ary->end)++] = NULL;
  } else {
    /* position relative to end */
    pos = pos + (intptr_t)ary->end;
    /* initialize empty spaces, if any, setting new boundries */
    while (ary->start > (size_t)pos)
      ary->arry[--(ary->start)] = NULL;
  }

  /* check for an existing object and set new objects */
  if (ary->arry[pos])
    old = ary->arry[pos];
  ary->arry[pos] = data;
  return old;
}

/* *****************************************************************************
Array push / shift API
***************************************************************************** */

/**
 * Pushes an object to the end of the Array. Returns -1 on error.
 */
FIO_FUNC inline int fio_ary_push(fio_ary_s *ary, void *data) {
  if (!ary->arry)
    fio_ary_new(ary, 0);
  else if (ary->capa <= ary->end)
    fio_ary_getmem(ary, 1);
  ary->arry[ary->end] = data;
  ary->end += 1;
  return 0;
}

/** Pops an object from the end of the Array. */
FIO_FUNC inline void *fio_ary_pop(fio_ary_s *ary) {
  if (!ary || ary->start == ary->end)
    return NULL;
  ary->end -= 1;
  return ary->arry[ary->end];
}

/**
 * Unshifts an object to the begining of the Array. Returns -1 on error.
 *
 * This could be expensive, causing `memmove`.
 */
FIO_FUNC inline int fio_ary_unshift(fio_ary_s *ary, void *data) {
  if (!ary->arry)
    fio_ary_new(ary, 0);
  else if (!ary->start)
    fio_ary_getmem(ary, -1);
  ary->start -= 1;
  ary->arry[ary->start] = data;
  return 0;
}

/** Shifts an object from the beginning of the Array. */
FIO_FUNC inline void *fio_ary_shift(fio_ary_s *ary) {
  if (!ary || ary->start == ary->end)
    return NULL;
#ifdef __cplusplus
  const size_t pos = ary->start;
#else
  register const size_t pos = ary->start;
#endif
  ary->start += 1;
  return ary->arry[pos];
}

/**
 * Single layer iteration using a callback for each entry in the array.
 *
 * The callback task function must accept an the entry data as well as an opaque
 * user pointer.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
FIO_FUNC inline size_t fio_ary_each(fio_ary_s *ary, size_t start_at,
                                    int (*task)(void *data, void *arg),
                                    void *arg) {
  if (!ary || ary->start == ary->end)
    return 0;
  const size_t start_pos = ary->start;
  start_at += start_pos;
  while (start_at < ary->end && task(ary->arry[start_at++], arg) != -1)
    ;
  return start_at - start_pos;
}

/* *****************************************************************************
Array compacting (untested)
***************************************************************************** */

/**
 * Removes any NULL *pointers* from an Array, keeping all other data in the
 * array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
FIO_FUNC inline void fio_ary_compact(fio_ary_s *ary) {
  if (!ary || ary->start == ary->end)
    return;
  register void **pos = ary->arry + ary->start;
  register void **reader = ary->arry + ary->start;
  register void **stop = ary->arry + ary->end;
  while (reader < stop) {
    if (*reader) {
      *pos = *reader;
      pos += 1;
    }
    reader += 1;
  }
  ary->end = (size_t)(pos - ary->arry);
}

#ifdef __cplusplus
#undef register
#endif

#undef FIO_FUNC

#endif
