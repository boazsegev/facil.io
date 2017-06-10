#ifndef H_FIO_HASH_TABLE_H
/**
The facil.io hash table imeplementation tries to balance performance with memory
footprint and collision risks.

It might be a good implementation and it might suck. Test it out and descide for
yourself.

*/
#define H_FIO_HASH_TABLE_H
#include "fio_list.h"

#include <stdint.h>
#include <stdlib.h>

/* *****************************************************************************
Data Types and API
***************************************************************************** */

#define FIO_FUNC static __attribute__((unused))

typedef struct {
  fio_list_s items;
  uint64_t count;
  uint64_t bin_count;
  uint64_t mask;
  struct fio_list_s *bins;
} fio_ht_s;

/** Used to initialize an empty `fio_ht_s`. */
#define FIO_HASH_TABLE_INIT(name)                                              \
  (fio_ht_s) { .items = FIO_LIST_INIT_STATIC(name.items) }

/** Used to initialize an empty `fio_ht_s`. */
#define FIO_HASH_TABLE_STATIC(name)                                            \
  { .items = FIO_LIST_INIT_STATIC(name.items) }

typedef struct {
  fio_list_s items;
  fio_list_s siblings;
  fio_ht_s *parent;
  uint64_t hash;
} fio_ht_node_s;

#ifndef fio_node2obj
/** Takes a node pointer (list/hash/dict, etc') and returns it's container. */
#define fio_node2obj(type, member, ptr)                                        \
  ((type *)((uintptr_t)(ptr) - (uintptr_t)(&(((type *)0)->member))))
#endif

/** Takes a list pointer and returns a pointer to it's container. */
#define fio_ht_object(type, member, pht) fio_node2obj(type, member, (pht))

/** A simple SipHash2/4 function implementation that can be used for hashing. */
FIO_FUNC uint64_t fio_ht_hash(const void *data, size_t len);

/** A simple SipHash2/4 function for when a string's length is unknown. */
FIO_FUNC uint64_t fio_ht_hash_cstr(const void *data);

/**
 * Adds a new item to the hash table.
 *
 * If a hash collision occurs, the old item is replaced and returned.
 */
inline FIO_FUNC fio_ht_node_s *fio_ht_add(fio_ht_s *table, fio_ht_node_s *item,
                                          uint64_t hash_value);

/** Finds an item in the hash table. */
inline FIO_FUNC fio_ht_node_s *fio_ht_find(fio_ht_s *table,
                                           uint64_t hash_value);
/** Removes the item from the hash table, returning the removed item. */
inline FIO_FUNC fio_ht_node_s *fio_ht_remove(fio_ht_node_s *item);

/** Finds, removes and returns the matching item from the hash table. */
inline FIO_FUNC fio_ht_node_s *fio_ht_pop(fio_ht_s *table, uint64_t hash);

/**
 * Re-hashes the hash table. This is automatically when growth is required.
 *
 * `bin_count` MUST be a power of 2 or 0 (0,1,2,4,8,16,32,64,128...).
 *
 * If `bin_count`, the hash table's memory is released and it will behave like a
 * list (until an item is added).
 */
inline FIO_FUNC void fio_ht_rehash(fio_ht_s *table, uint64_t bin_count);

/** iterates through all Hash Table members. */
#define fio_ht_for_each(type, member, var, table)                              \
  fio_list_for_each(type, member.items, var, (table).items)

/**
 * Frees any internal memory used by the hash table.
 *
 * This soes **NOT** free any items in the table or the table object itself.
 */
#define fio_ht_free(table) fio_ht_rehash(table, 0)

/* *****************************************************************************
Implementations
***************************************************************************** */

/** Adds a new item to the hash table. If a hash collision occurs, the old item
 * is removed and replaced. */
inline FIO_FUNC fio_ht_node_s *fio_ht_add(fio_ht_s *table, fio_ht_node_s *item,
                                          uint64_t hash) {
  fio_ht_node_s *old = fio_ht_find(table, hash);
  if (old == item)
    return NULL;
  /* initialize item */
  item->parent = table;
  item->hash = hash;
  if (old)
    goto found_old;
  fio_list_add(table->items.prev, &item->items);
  table->count++;
  if (table->count >= ((table->bin_count >> 2) * 3)) {
    fio_ht_rehash(table, (table->bin_count ? (table->bin_count << 1) : 32));
    return NULL;
  }
  fio_list_add(table->bins[item->hash & table->mask].prev, &item->siblings);
  return NULL;
found_old:
  /* replace inplace. */
  *item = *old;
  old->parent = NULL;
  old->items = FIO_LIST_INIT(old->items);
  old->siblings = FIO_LIST_INIT(old->siblings);
  item->items.prev->next = &item->items;
  item->items.next->prev = &item->items;
  item->siblings.prev->next = &item->siblings;
  item->siblings.next->prev = &item->siblings;
  return old;
}

/** Finds an item in the hash table. */
inline FIO_FUNC fio_ht_node_s *fio_ht_find(fio_ht_s *table,
                                           uint64_t hash_value) {
  fio_ht_node_s *item;
  if (!table || !table->bins)
    return NULL;
  fio_list_for_each(fio_ht_node_s, siblings, item,
                    table->bins[hash_value & (table->mask)]) {
    if (hash_value == item->hash) {
      return item;
    }
  }
  return NULL;
}

/** Removes an item to the hash table. */
inline FIO_FUNC fio_ht_node_s *fio_ht_remove(fio_ht_node_s *item) {
  if (!item || !item->parent)
    return item;
  fio_list_remove(&item->siblings);
  fio_list_remove(&item->items);
  item->parent->count--;
  /* ** memory shrinkage? not really... ** */
  /*
  if (item->parent->bin_count > 1024 &&
      (item->parent->bin_count >> 3) >= item->parent->count)
    fio_ht_rehash(item->parent, item->parent->bin_count >> 2);
  */
  item->parent = NULL;
  return item;
}

/** Finds, removes and returns the matching item from the hash table. */
inline FIO_FUNC fio_ht_node_s *fio_ht_pop(fio_ht_s *table, uint64_t hash) {
  fio_ht_node_s *item = fio_ht_find(table, hash);
  if (!item)
    return NULL;
  fio_ht_remove(item);
  return item;
}

/** Re-hashes the hash table. This is usually automatically called. */
inline FIO_FUNC void fio_ht_rehash(fio_ht_s *table, uint64_t bin_count) {
  if (!table)
    return;
  if (!bin_count) {
    if (table->bins) {
      free(table->bins);
      table->bins = NULL;
    }
    return;
  }
  void *mem = realloc(table->bins, bin_count * sizeof(*table->bins));
  if (!mem)
    return;
  table->bin_count = bin_count;
  table->bins = mem;
  table->mask = bin_count - 1;
  while (bin_count) {
    bin_count--;
    table->bins[bin_count] = FIO_LIST_INIT(table->bins[bin_count]);
  }
  fio_ht_node_s *item;
  fio_list_for_each(fio_ht_node_s, items, item, table->items) {
    fio_list_add(table->bins[item->hash & table->mask].prev, &item->siblings);
  }
}

/* *****************************************************************************
The Hash Function
***************************************************************************** */

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) &&                 \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/* the algorithm was designed as little endian... so, byte swap 64 bit. */
#define sip_local64(i)                                                         \
  (((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |                            \
      (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |                 \
      (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |         \
      (((i)&0xFF000000000000ULL) >> 40) | (((i)&0xFF00000000000000ULL) >> 56)
#else
/* no need */
#define sip_local64(i) (i)
#endif

/* 64Bit left rotation, inlined. */
#define lrot64(i, bits)                                                        \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))

FIO_FUNC uint64_t fio_ht_hash(const void *data, size_t len) {
  /* initialize the 4 words */
  uint64_t v0 = (0x0706050403020100ULL ^ 0x736f6d6570736575ULL);
  uint64_t v1 = (0x0f0e0d0c0b0a0908ULL ^ 0x646f72616e646f6dULL);
  uint64_t v2 = (0x0706050403020100ULL ^ 0x6c7967656e657261ULL);
  uint64_t v3 = (0x0f0e0d0c0b0a0908ULL ^ 0x7465646279746573ULL);
  const uint64_t *w64 = data;
  uint8_t len_mod = len & 255;
  union {
    uint64_t i;
    uint8_t str[8];
  } word;

#define hash_map_SipRound                                                      \
  do {                                                                         \
    v2 += v3;                                                                  \
    v3 = lrot64(v3, 16) ^ v2;                                                  \
    v0 += v1;                                                                  \
    v1 = lrot64(v1, 13) ^ v0;                                                  \
    v0 = lrot64(v0, 32);                                                       \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = lrot64(v1, 17) ^ v2;                                                  \
    v3 = lrot64(v3, 21) ^ v0;                                                  \
    v2 = lrot64(v2, 32);                                                       \
  } while (0);

  while (len >= 8) {
    word.i = sip_local64(*w64);
    v3 ^= word.i;
    /* Sip Rounds */
    hash_map_SipRound;
    hash_map_SipRound;
    v0 ^= word.i;
    w64 += 1;
    len -= 8;
  }
  word.i = 0;
  uint8_t *pos = word.str;
  uint8_t *w8 = (void *)w64;
  switch (len) { /* fallthrough is intentional */
  case 7:
    pos[6] = w8[6];
  case 6:
    pos[5] = w8[5];
  case 5:
    pos[4] = w8[4];
  case 4:
    pos[3] = w8[3];
  case 3:
    pos[2] = w8[2];
  case 2:
    pos[1] = w8[1];
  case 1:
    pos[0] = w8[0];
  }
  word.str[7] = len_mod;

  /* last round */
  v3 ^= word.i;
  hash_map_SipRound;
  hash_map_SipRound;
  v0 ^= word.i;
  /* Finalization */
  v2 ^= 0xff;
  /* d iterations of SipRound */
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  /* XOR it all together */
  v0 ^= v1 ^ v2 ^ v3;
#undef hash_map_SipRound
  return v0;
}

FIO_FUNC uint64_t fio_ht_hash_cstr(const void *data) {
  /* initialize the 4 words */
  uint64_t v0 = (0x0706050403020100ULL ^ 0x736f6d6570736575ULL);
  uint64_t v1 = (0x0f0e0d0c0b0a0908ULL ^ 0x646f72616e646f6dULL);
  uint64_t v2 = (0x0706050403020100ULL ^ 0x6c7967656e657261ULL);
  uint64_t v3 = (0x0f0e0d0c0b0a0908ULL ^ 0x7465646279746573ULL);
  const uint64_t *w64 = data;
  uint8_t len = 0;
  union {
    uint64_t i;
    uint8_t str[8];
  } word;

#define hash_map_SipRound                                                      \
  do {                                                                         \
    v2 += v3;                                                                  \
    v3 = lrot64(v3, 16) ^ v2;                                                  \
    v0 += v1;                                                                  \
    v1 = lrot64(v1, 13) ^ v0;                                                  \
    v0 = lrot64(v0, 32);                                                       \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = lrot64(v1, 17) ^ v2;                                                  \
    v3 = lrot64(v3, 21) ^ v0;                                                  \
    v2 = lrot64(v2, 32);                                                       \
  } while (0);

  while ((*w64 & 0xFFULL) && (*w64 & 0xFF00ULL) && (*w64 & 0xFF0000ULL) &&
         (*w64 & 0xFF000000ULL) && (*w64 & 0xFF00000000ULL) &&
         (*w64 & 0xFF0000000000ULL) && (*w64 & 0xFF000000000000ULL) &&
         (*w64 & 0xFF00000000000000ULL)) {
    word.i = sip_local64(*w64);
    v3 ^= word.i;
    /* Sip Rounds */
    hash_map_SipRound;
    hash_map_SipRound;
    v0 ^= word.i;
    w64 += 1;
    len += 8;
  }
  word.i = 0;
  uint8_t *pos = word.str;
  uint8_t *w8 = (void *)w64;
  while (*w8) {
    *(pos++) = *(w8++);
    len++;
  }
  word.str[7] = len & 255;

  /* last round */
  v3 ^= word.i;
  hash_map_SipRound;
  hash_map_SipRound;
  v0 ^= word.i;
  /* Finalization */
  v2 ^= 0xff;
  /* d iterations of SipRound */
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  hash_map_SipRound;
  /* XOR it all together */
  v0 ^= v1 ^ v2 ^ v3;
#undef hash_map_SipRound
  return v0;
}

#undef sip_local64
#undef lrot64
#undef FIO_FUNC
#endif
