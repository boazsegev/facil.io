/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define FIO_HT_ACTS_AS_LIST_LIMIT 0

#include "fio_file_cache.h"
#include "fio_hash_table.h"
#include "spnlock.inc"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* *****************************************************************************
Data Structures / State
***************************************************************************** */

/* The global cache lock. */
static spn_lock_i fio_cfd_GIL = SPN_LOCK_INIT;

/* The cache hash-table. */
static fio_ht_s fio_cfd_store = FIO_HASH_TABLE_STATIC(fio_cfd_store);

/* The cache node data. */
typedef struct fio_cfd_s {
  /* The order of the first two items is a promis made by `fio_file_cache.h` */
  struct {
    struct stat stat;
    int fd;
  };
  volatile uint64_t ref;
  fio_ht_node_s node;
} fio_cfd_s;

/* *****************************************************************************
Helpers
***************************************************************************** */

static inline uint64_t atomic_bump(volatile uint64_t *i) {
  return __sync_add_and_fetch(i, 1ULL);
}
static inline uint64_t atomic_cut(volatile uint64_t *i) {
  return __sync_sub_and_fetch(i, 1ULL);
}

static inline uint64_t fio_cfd_simple_hash(const char *file_name,
                                           size_t length) {
  uint64_t hash = 0x736f6d6570736575ULL;
  while (length >= 8) {
    hash ^= *(uint64_t *)file_name;
    length -= 8;
    file_name += 8;
  }
  hash = (hash >> 13) | (hash << (64 - 13));
  while (length) {
    hash ^= *(uint8_t *)file_name;
    hash = (hash >> 5) | (hash << (64 - 5));
    length--;
    file_name++;
  }
  return hash;
}

/* *****************************************************************************
API
***************************************************************************** */

/**
 * Opens a file (if it's not in the cache) and returns an `fio_cfd_pt` object.
 *
 * If the file can't be found, `NULL` is returned.
 */
fio_cfd_pt fio_cfd_open(const char *file_name, size_t length) {
  fio_cfd_pt cached = NULL;
  if (!file_name || !length)
    return NULL;
  uint64_t hash = fio_ht_hash(file_name, length);
  spn_lock(&fio_cfd_GIL);
  cached = (void *)fio_ht_find(&fio_cfd_store, hash);
  if (cached) {
    cached = fio_ht_object(fio_cfd_s, node, cached);
    fio_list_switch(fio_cfd_store.items.prev, &cached->node.items);
    atomic_bump(&cached->ref);
    spn_unlock(&fio_cfd_GIL);
    return cached;
  }
  /* UNLOCK: when performing system calls, threads are more likely to rotate. */
  // spn_unlock(&fio_cfd_GIL);

  cached = malloc(sizeof(*cached));
  if (!cached)
    return NULL;
  if (stat(file_name, &cached->stat) ||
      (!S_ISREG(cached->stat.st_mode) && !S_ISLNK(cached->stat.st_mode)) ||
      (cached->fd = open(file_name, O_RDONLY)) == -1) {
    spn_unlock(&fio_cfd_GIL);
    free(cached);
    return NULL;
  }

  cached->ref = 2;

  fprintf(stderr, "* New file cached.\n");

  /* RELOCK: we're back in the lock. */
  // spn_lock(&fio_cfd_GIL);

  fio_ht_add(&fio_cfd_store, &cached->node, hash);

  if (fio_cfd_store.count <= FIO_FILE_CACHE_LIMIT ||
      fio_cfd_store.items.next == &fio_cfd_store.items) {
    spn_unlock(&fio_cfd_GIL);
    return cached;
  }

  fio_ht_node_s *tmp =
      fio_list_object(fio_ht_node_s, items, fio_cfd_store.items.next);
  fio_ht_remove(tmp);

  spn_unlock(&fio_cfd_GIL);
  fio_cfd_close(fio_ht_object(fio_cfd_s, node, tmp));
  return cached;
}

/** Handles file closure. The file may or may not be actually closed. */
void fio_cfd_close(fio_cfd_pt fio_cfd) {
  if (atomic_cut(&fio_cfd->ref))
    return;
  close(fio_cfd->fd);
  free(fio_cfd);
}

/** Asuuming the `int` pointed to by `pfd` is part of a `fio_cfd_s` ... */
void fio_cfd_close_pfd(int *pfd) {
  fio_cfd_close(
      (fio_cfd_pt)((uintptr_t)(pfd) - (uintptr_t)(&(((fio_cfd_pt)0)->fd))));
}

/** Clears the open file cache. Files may remain open if in use. */
void fio_cfd_clear(void) {
  fio_cfd_pt cfd;
  fio_ht_for_each(fio_cfd_s, node, cfd, fio_cfd_store) {
    fio_ht_remove(&cfd->node);
  }
  fio_ht_free(&fio_cfd_store);
}
