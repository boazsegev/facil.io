/*
(un)copyright: Boaz segev, 2016
License: Public Domain except for any non-public-domain algorithms, which are
subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

/* *****************************************************************************
 * A simple bitmap implementation which includes:
 * * Cluster seek/set/unset.
 * * Bounds review.
 */

/** creates a bitmap object with the requested number of 64bit words. */
#define MakeBitmap(name, words64)                                              \
  struct {                                                                     \
    uint64_t limit;                                                            \
    uint64_t bitmap[words64];                                                  \
  } name = {.limit = (words64 << 6)};

/** Declares a bitmap object but doesn't initialize it. */
#define DeclareBitmap(name, words64)                                           \
  struct {                                                                     \
    uint64_t limit;                                                            \
    uint64_t bitmap[words64];                                                  \
  } name;
/** Initializes an already declared bitmap. */
#define InitializeBitmap(name, words64)                                        \
  name.limit = words64 << 6;                                                   \
  memset(name.bitmap, 0, words64 * sizeof(uint64_t));

/* *****************************************************************************
 * Log2 map for 64 bits.
 */
UNUSED_FUNC static int _bm_log2(uint64_t word) {
  switch (word) {
  case 0x1ULL:
    return 0;
  case 0x2ULL:
    return 1;
  case 0x4ULL:
    return 2;
  case 0x8ULL:
    return 3;
  case 0x10ULL:
    return 4;
  case 0x20ULL:
    return 5;
  case 0x40ULL:
    return 6;
  case 0x80ULL:
    return 7;
  case 0x100ULL:
    return 8;
  case 0x200ULL:
    return 9;
  case 0x400ULL:
    return 10;
  case 0x800ULL:
    return 11;
  case 0x1000ULL:
    return 12;
  case 0x2000ULL:
    return 13;
  case 0x4000ULL:
    return 14;
  case 0x8000ULL:
    return 15;
  case 0x10000ULL:
    return 16;
  case 0x20000ULL:
    return 17;
  case 0x40000ULL:
    return 18;
  case 0x80000ULL:
    return 19;
  case 0x100000ULL:
    return 20;
  case 0x200000ULL:
    return 21;
  case 0x400000ULL:
    return 22;
  case 0x800000ULL:
    return 23;
  case 0x1000000ULL:
    return 24;
  case 0x2000000ULL:
    return 25;
  case 0x4000000ULL:
    return 26;
  case 0x8000000ULL:
    return 27;
  case 0x10000000ULL:
    return 28;
  case 0x20000000ULL:
    return 29;
  case 0x40000000ULL:
    return 30;
  case 0x80000000ULL:
    return 31;
  case 0x100000000ULL:
    return 32;
  case 0x200000000ULL:
    return 33;
  case 0x400000000ULL:
    return 34;
  case 0x800000000ULL:
    return 35;
  case 0x1000000000ULL:
    return 36;
  case 0x2000000000ULL:
    return 37;
  case 0x4000000000ULL:
    return 38;
  case 0x8000000000ULL:
    return 39;
  case 0x10000000000ULL:
    return 40;
  case 0x20000000000ULL:
    return 41;
  case 0x40000000000ULL:
    return 42;
  case 0x80000000000ULL:
    return 43;
  case 0x100000000000ULL:
    return 44;
  case 0x200000000000ULL:
    return 45;
  case 0x400000000000ULL:
    return 46;
  case 0x800000000000ULL:
    return 47;
  case 0x1000000000000ULL:
    return 48;
  case 0x2000000000000ULL:
    return 49;
  case 0x4000000000000ULL:
    return 50;
  case 0x8000000000000ULL:
    return 51;
  case 0x10000000000000ULL:
    return 52;
  case 0x20000000000000ULL:
    return 53;
  case 0x40000000000000ULL:
    return 54;
  case 0x80000000000000ULL:
    return 55;
  case 0x100000000000000ULL:
    return 56;
  case 0x200000000000000ULL:
    return 57;
  case 0x400000000000000ULL:
    return 58;
  case 0x800000000000000ULL:
    return 59;
  case 0x1000000000000000ULL:
    return 60;
  case 0x2000000000000000ULL:
    return 61;
  case 0x4000000000000000ULL:
    return 62;
  case 0x8000000000000000ULL:
    return 63;
  }
  return 0;
}

/* *****************************************************************************
 * implementation macros, should be undefined later on.
 */

#define _bs_bitmap_def_vars_or_return(bitmap_ptr, ret)                         \
  uint64_t *lim = (uint64_t *)(bitmap_ptr);                                    \
  if (offset >= *lim)                                                          \
    return ret;                                                                \
  uint64_t *bits = lim + 1;

/* *****************************************************************************
 * Single bit manipulation.
 */

/**
 * Checks to see if the specified bit (at `offset`) is set.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * Returns 1 if the bit is set and 0 if it isn't. Returns -1 if request was out
 * of bounds.
 */
UNUSED_FUNC static int bitmap_is_bit_set(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  return ((bits[offset >> 6] >> (offset & 63)) & 1UL);
}

/**
 * Sets the bit at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * Out of bounds requests fail silently.
 */
UNUSED_FUNC static void bitmap_set_bit(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, ;);
  bits[offset >> 6] |= 1UL << (offset & 63);
}
/**
 * Unsets (clears) the bit at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * Out of bounds requests fail silently.
 */
UNUSED_FUNC static void bitmap_unset_bit(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, ;);
  bits[offset >> 6] &= ~(1UL << (offset & 63));
}
/**
 * Returns the offset of the first set bit starting at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * If no bits are set (or the offset is out of bounds), the function returns -1.
 *
 */
UNUSED_FUNC static ssize_t bitmap_seek_set(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  uint64_t word;
  uint64_t stop_at = (*lim >> 6) - 1;
  word = bits[offset >> 6] & ~((1UL << (offset & 63)) - 1);
  offset >>= 6;
  while (word == 0 && offset < stop_at) {
    offset += 1;
    word = bits[offset];
  }
  if (word == 0)
    return -1;
  return (offset << 6) | (_bm_log2(word & -word));
}

/**
 * Returns the offset of the first unset bit starting at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * If no bits are set (or the offset is out of bounds), the function returns -1.
 */
UNUSED_FUNC static ssize_t bitmap_seek_unset(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  uint64_t word;
  uint64_t stop_at = (*lim >> 6) - 1;
  // word = ~(bits[offset >> 6] & ~((1UL << (offset & 63)) - 1));
  word = (~bits[offset >> 6]) & ~((1UL << (offset & 63)) - 1);
  offset >>= 6;
  while (word == 0 && offset < stop_at) {
    offset += 1;
    word = ~bits[offset];
  }
  if (word == 0)
    return -1;
  return (offset << 6) | (_bm_log2(word & -word));

  // _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  // uint64_t word;
  // if (offset & 63) {
  //   word = ~bits[offset >> 6];
  //   word >>= (offset & 63);
  //   if (word) {
  //     return offset + _bm_log2(word & -word);
  //   }
  //   offset = (offset - (offset & 63)) + 64;
  // }
  // while (offset < *lim) {
  //   if ((word = ~bits[offset >> 6]) == 0) {
  //     offset += 64;
  //     continue;
  //   }
  //   return offset + _bm_log2(word & -word);
  // }
  // return -1;
}

/**
 * Returns the offset of the first set bit starting at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * If no bits are set (or the offset is out of bounds), the function returns -1.
 *
 */
UNUSED_FUNC static ssize_t bitmap_total_set(void *bitmap_ptr, size_t offset) {
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  uint64_t word;
  ssize_t count = 0;

#define _btmp_is_bit_set(n, bit) (((n) & (1UL << bit)) >> bit)

#define _btmp_popcount64(n)                                                    \
  (_btmp_is_bit_set((n), 0) + _btmp_is_bit_set((n), 1) +                       \
   _btmp_is_bit_set((n), 2) + _btmp_is_bit_set((n), 3) +                       \
   _btmp_is_bit_set((n), 4) + _btmp_is_bit_set((n), 5) +                       \
   _btmp_is_bit_set((n), 6) + _btmp_is_bit_set((n), 7) +                       \
   _btmp_is_bit_set((n), 8) + _btmp_is_bit_set((n), 9) +                       \
   _btmp_is_bit_set((n), 10) + _btmp_is_bit_set((n), 11) +                     \
   _btmp_is_bit_set((n), 12) + _btmp_is_bit_set((n), 13) +                     \
   _btmp_is_bit_set((n), 14) + _btmp_is_bit_set((n), 15) +                     \
   _btmp_is_bit_set((n), 16) + _btmp_is_bit_set((n), 17) +                     \
   _btmp_is_bit_set((n), 18) + _btmp_is_bit_set((n), 19) +                     \
   _btmp_is_bit_set((n), 20) + _btmp_is_bit_set((n), 21) +                     \
   _btmp_is_bit_set((n), 22) + _btmp_is_bit_set((n), 23) +                     \
   _btmp_is_bit_set((n), 24) + _btmp_is_bit_set((n), 25) +                     \
   _btmp_is_bit_set((n), 26) + _btmp_is_bit_set((n), 27) +                     \
   _btmp_is_bit_set((n), 28) + _btmp_is_bit_set((n), 29) +                     \
   _btmp_is_bit_set((n), 30) + _btmp_is_bit_set((n), 31) +                     \
   _btmp_is_bit_set((n), 32) + _btmp_is_bit_set((n), 33) +                     \
   _btmp_is_bit_set((n), 34) + _btmp_is_bit_set((n), 35) +                     \
   _btmp_is_bit_set((n), 36) + _btmp_is_bit_set((n), 37) +                     \
   _btmp_is_bit_set((n), 38) + _btmp_is_bit_set((n), 39) +                     \
   _btmp_is_bit_set((n), 40) + _btmp_is_bit_set((n), 41) +                     \
   _btmp_is_bit_set((n), 42) + _btmp_is_bit_set((n), 43) +                     \
   _btmp_is_bit_set((n), 44) + _btmp_is_bit_set((n), 45) +                     \
   _btmp_is_bit_set((n), 46) + _btmp_is_bit_set((n), 47) +                     \
   _btmp_is_bit_set((n), 48) + _btmp_is_bit_set((n), 49) +                     \
   _btmp_is_bit_set((n), 50) + _btmp_is_bit_set((n), 51) +                     \
   _btmp_is_bit_set((n), 52) + _btmp_is_bit_set((n), 53) +                     \
   _btmp_is_bit_set((n), 54) + _btmp_is_bit_set((n), 55) +                     \
   _btmp_is_bit_set((n), 56) + _btmp_is_bit_set((n), 57) +                     \
   _btmp_is_bit_set((n), 58) + _btmp_is_bit_set((n), 59) +                     \
   _btmp_is_bit_set((n), 60) + _btmp_is_bit_set((n), 61) +                     \
   _btmp_is_bit_set((n), 62) + _btmp_is_bit_set((n), 63))

  if (offset & 63) {
    word = bits[offset >> 6];
    word >>= (offset & 63);
    count += _btmp_popcount64(word);
    offset = (offset - (offset & 63)) + 64;
  }
  offset >>= 6;
  size_t end = *lim >> 6;
  while (offset < end) {
    count += _btmp_popcount64(bits[offset]);
    offset += 1;
  }
  return count;

#undef _btmp_is_bit_set
#undef _btmp_popcount64
}

/* *****************************************************************************
 * Cluster bit manipulation.
 */

/**
 * Sets `length` bits at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * Out of bounds requests fail silently.
 */
UNUSED_FUNC static void bitmap_set_cluster(void *bitmap_ptr, size_t offset,
                                        size_t length) {
  if (length == 0)
    return;
  if (length == 1) {
    bitmap_set_bit(bitmap_ptr, offset);
    return;
  }

  _bs_bitmap_def_vars_or_return(bitmap_ptr, ;);
  if (length + offset > *lim)
    length = *lim - offset;

  if (length + (offset & 63) < 64) {
    bits[offset >> 6] |= ((1UL << length) - 1) << (offset & 63);
    return;
  }
  // fprintf(stderr, "offset %lu, length %lu\n", offset, length);
  if (offset & 63) {
    length -= (64 - (offset & 63));
    bits[offset >> 6] |= (~0UL) << (offset & 63);
    offset += 64;
  }
  offset >>= 6;
  while (length > 63) {
    bits[offset] = ~0UL;
    length -= 64;
    offset++;
  }
  if (length) {
    bits[offset] |= ((1UL << length) - 1);
  }
}
/**
 * Unsets (clears) `length` bits at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * Out of bounds requests fail silently.
 */
UNUSED_FUNC static inline void bitmap_unset_cluster(void *bitmap_ptr,
                                                 size_t offset, size_t length) {
  if (length <= 1) {
    bitmap_unset_bit(bitmap_ptr, offset);
    return;
  }
  _bs_bitmap_def_vars_or_return(bitmap_ptr, ;);
  if (length + offset > *lim)
    length = *lim - offset;
  if (length + (offset & 63) < 64) {
    bits[offset >> 6] &= ~(((1UL << length) - 1) << (offset & 63));
    return;
  }
  if (offset & 63) {
    length -= (64 - (offset & 63));
    bits[offset >> 6] &= (1UL << (offset & 63)) - 1;
    offset += 64;
  }
  offset >>= 6;
  while (length > 63) {
    bits[offset] = ~0UL;
    length -= 64;
    offset++;
  }
  if (length) {
    bits[offset] &= ~((1UL << length) - 1);
  }
}

/**
 * Returns the offset of the first unset `length` number of bits (a cluster of
 * unset bits is an empty cluster) starting at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * If no bits are set (or the offset is out of bounds), the function returns -1.
 *
 */
UNUSED_FUNC static inline ssize_t
bitmap_seek_empty_cluster(void *bitmap_ptr, size_t offset, size_t length) {
  if (length <= 1)
    return bitmap_seek_unset(bitmap_ptr, offset);
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  (void)bits; /* noop  to ignore unused variable */
  ssize_t first = offset;
  ssize_t last;
  while (1) {
    first = bitmap_seek_unset(bitmap_ptr, first);
    if (first < 0)
      return -1;
    last = bitmap_seek_set(bitmap_ptr, first);
    if (last < 0)
      last = *lim - 1;
    if (last - first >= length)
      return first;
    first = last + 1;
  }
}

/**
 * Returns the offset of the first set `length` number of bits (a cluster of set
 * bits) starting at `offset`.
 *
 * Offsets start at 0 (0 is the first bit).
 *
 * If no bits are set (or the offset is out of bounds), the function returns -1.
 *
 */
UNUSED_FUNC static inline ssize_t
bitmap_seek_set_cluster(void *bitmap_ptr, size_t offset, size_t length) {
  if (length == 1)
    return bitmap_seek_set(bitmap_ptr, offset);
  _bs_bitmap_def_vars_or_return(bitmap_ptr, -1);
  (void)bits; /* no-op to ignore unused variable */
  ssize_t first = offset;
  ssize_t last;
  while (1) {
    first = bitmap_seek_set(bitmap_ptr, first);
    if (first < 0)
      return -1;
    last = bitmap_seek_unset(bitmap_ptr, first);
    if (last >= 0) {
      if (last - first >= length)
        return first;
      first = last + 1;
      continue;
    } else {
      if (*lim - first - 1 >= length)
        return first;
      return -1;
    }
  }
}

/* *****************************************************************************
 * Clear implementation macros.
 */

#undef _bs_bitmap_def_vars_or_return

/* *****************************************************************************
 * Testing.
 */

#if defined(DEBUG) && DEBUG == 1
#include <time.h>
UNUSED_FUNC static void bitmap_test(void) {
  clock_t start, end;
  MakeBitmap(bitmap, 512);
  if (bitmap.bitmap[0] || bitmap.bitmap[1] || bitmap.bitmap[2] ||
      bitmap.bitmap[3] || bitmap.bitmap[4] || bitmap.bitmap[5])
    fprintf(stderr, "* bitmap initialization failed!!!\n");
  if (bitmap_total_set(&bitmap, 0))
    fprintf(stderr, "* bitmap popcount / initialization failed!!!\n");
  bitmap_set_bit(&bitmap, 3);
  if (bitmap.bitmap[0] == (1UL << 3))
    fprintf(stderr, "* bitmap bitmap_set_bit no overflow passed.\n");
  else
    fprintf(stderr, "* bitmap bitmap_set_bit no overflow FAILED.\n");
  bitmap_set_bit(&bitmap, 66);
  if (bitmap.bitmap[1] == (1UL << 2))
    fprintf(stderr, "* bitmap bitmap_set_bit with overflow passed.\n");
  else
    fprintf(stderr, "* bitmap bitmap_set_bit with overflow FAILED.\n");
  fprintf(stderr, "* bitmap bitmap_seek_set %s (result: %ld).\n",
          bitmap_seek_set(&bitmap, 5) == 66 ? "passed" : "FAILED",
          bitmap_seek_set(&bitmap, 5));
  bitmap.bitmap[0] = 0;
  bitmap_set_cluster(&bitmap, 3, 3);
  if (bitmap.bitmap[0] == (((1UL << 3) - 1) << 3))
    fprintf(stderr, "* bitmap bitmap_set_cluster no overflow passed.\n");
  else
    fprintf(stderr, "* bitmap bitmap_set_cluster no overflow FAILED"
                    "(%llu not expected result).\n",
            bitmap.bitmap[0]);
  bitmap.bitmap[0] = 0;
  bitmap.bitmap[1] = 0;
  bitmap.bitmap[2] = 0;
  bitmap_set_cluster(&bitmap, 123, 32);
  if (bitmap_seek_set(&bitmap, 0) == 123 &&
      bitmap_seek_unset(&bitmap, 127) == 155)
    fprintf(stderr, "* bitmap bitmap_set_cluster with overflow passed.\n");
  else
    fprintf(stderr,
            "* bitmap bitmap_set_cluster with overflow FAILED (%lu...%lu not "
            "expected result).\n",
            bitmap_seek_set(&bitmap, 0), bitmap_seek_unset(&bitmap, 127));

  // run through the bitmap, set everything
  ssize_t pos = 0;
  start = clock();
  while (pos >= 0) {
    pos = bitmap_seek_unset(&bitmap, pos);
    if (pos < 0)
      break;
    bitmap_set_bit(&bitmap, pos);
  }
  end = clock();
  if (bitmap_seek_unset(&bitmap, 0) == -1 &&
      bitmap_total_set(&bitmap, 0) == 32768 &&
      (bitmap.bitmap[4] & bitmap.bitmap[5] & bitmap.bitmap[7]) == ~0)
    fprintf(stderr,
            "* bitmap seek and set single bit X 32768 bits passed CPU cycles "
            "%lu.\n",
            end - start);
  else
    fprintf(stderr,
            "* bitmap seek and set single FAILED (unset bit at %ld popcount "
            "%ld).  CPU cycles %lu\n",
            bitmap_seek_unset(&bitmap, 0), bitmap_total_set(&bitmap, 0),
            end - start);

  // run through the bitmap, unset everything
  pos = 0;
  start = clock();
  while (pos >= 0) {
    pos = bitmap_seek_set(&bitmap, pos);
    if (pos < 0)
      break;
    bitmap_unset_bit(&bitmap, pos);
  }
  end = clock();
  if (bitmap_seek_set(&bitmap, 0) == -1 && bitmap_total_set(&bitmap, 0) == 0 &&
      (bitmap.bitmap[4] | bitmap.bitmap[5] | bitmap.bitmap[7]) == 0)
    fprintf(stderr, "* bitmap seek and unset single bit X 32768 bits "
                    "passed CPU cycles %lu.\n",
            end - start);
  else
    fprintf(stderr,
            "* bitmap seek and unset single FAILED (unset bit at %ld popcount "
            "%ld). CPU cycles %lu.\n",
            bitmap_seek_unset(&bitmap, 0), bitmap_total_set(&bitmap, 0),
            end - start);
  // run through the bitmap, set almost everything, cluster
  pos = 0;
  start = clock();
  while (pos >= 0) {
    pos = bitmap_seek_empty_cluster(&bitmap, pos, 7);
    if (pos < 0)
      break;
    bitmap_set_cluster(&bitmap, pos, 7);
  }
  end = clock();
  if (bitmap_seek_unset(&bitmap, 0) == 32767 &&
      bitmap_total_set(&bitmap, 0) == 32767 &&
      (bitmap.bitmap[4] & bitmap.bitmap[5] & bitmap.bitmap[6]) == ~0 &&
      bitmap.bitmap[7] == (~0 >> 1))
    fprintf(stderr, "* bitmap seek and set 7 bit cluster X 32768 bits "
                    "passed. CPU cycles %lu\n",
            end - start);
  else
    fprintf(stderr,
            "* bitmap seek and set 7 bit cluster X 32768 bits FAILED (unset "
            "bit at %ld popcount "
            "%ld).\n",
            bitmap_seek_unset(&bitmap, 0), bitmap_total_set(&bitmap, 0));
  // run through the bitmap, unset almost everything, cluster
  pos = 0;
  start = clock();
  while (pos >= 0) {
    pos = bitmap_seek_set_cluster(&bitmap, pos, 7);
    if (pos < 0)
      break;
    bitmap_unset_cluster(&bitmap, pos, 7);
  }
  end = clock();
  if (bitmap_seek_set(&bitmap, 0) == -1 && bitmap_total_set(&bitmap, 0) == 0 &&
      (bitmap.bitmap[4] | bitmap.bitmap[5] | bitmap.bitmap[7]) == 0)
    fprintf(stderr, "* bitmap seek and unset 7 bit cluster "
                    "X 32768 passed. CPU cycles %lu\n",
            end - start);
  else
    fprintf(stderr,
            "* bitmap seek and unset cluster FAILED (unset bit at %ld popcount "
            "%ld).\n",
            bitmap_seek_unset(&bitmap, 0), bitmap_total_set(&bitmap, 0));
}
#endif
