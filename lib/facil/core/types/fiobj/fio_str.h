#ifndef H_FIO_STRING_H
/*
Copyright: Boaz Segev, 2018
License: MIT
*/

/**
 * A Dynamic String C library for ease of use and binary strings.
 *
 * The string is a simple byte string which is compatible with binary data (NUL
 * is a valid byte).
 *
 * Example use:
 *
 *     fio_str_s str = FIO_STR_INIT;    // container on the stack.
 *     fio_str_write(&str, "hello", 5); // add / remove / read data...
 *     fio_str_free(&str)               // free the data, not the container.
 *
 * Should work with 32bit and 64bit architectures.
 */
#define H_FIO_STRING_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#ifndef FIO_ASSERT_ALLOC
/** Tests for an allocation failure. The behavior can be overridden. */
#define FIO_ASSERT_ALLOC(ptr)                                                  \
  if (!(ptr)) {                                                                \
    perror("FATAL ERROR: no memory (for string allocation)");                  \
    exit(errno);                                                               \
  }
#endif

#ifdef __cplusplus
#define register
#endif

/* *****************************************************************************
String API - Initialization and Destruction
***************************************************************************** */

/**
 * The `fio_str_s` type should be considered opaque when possible.
 *
 * The type's attributes should be accessed using the accessor functions:
 * `fio_str_len`, `fio_str_data`, `fio_str_capa`, `fio_str_state`, etc'.
 *
 * When accessing the type's
 */
typedef struct {
  uint8_t small;  /* Flag indicating the String is small and self-contained */
  uint8_t frozen; /* Flag indicating the String is frozen (don't edit) */
  uint8_t reserved[sizeof(size_t) - (sizeof(uint8_t) * 2)]; /* padding */
  size_t capa; /* Known capacity for longer Strings */
  size_t len;  /* String length for longer Strings */
  char *data;  /* Data for longer Strings */
} fio_str_s;

/**
 * This value should be used for initialization. For example:
 *
 *      // on the stack
 *      fio_str_s str = FIO_STR_INIT;
 *
 *      // or on the heap
 *      fio_str_s *str = malloc(sizeof(*str);
 *      *str = FIO_STR_INIT;
 *
 * Remember to cleanup:
 *
 *      // on the stack
 *      fio_str_free(&str);
 *
 *      // or on the heap
 *      fio_str_free(str);
 *      free(str);
 */
#define FIO_STR_INIT ((fio_str_s){.data = NULL, .small = 1})

/**
 * This macro allows the container to be initialized with existing data, as long
 * as it's memory was allocated using `malloc`.
 *
 * The `capacity` value should exclude the NUL character (if exists).
 */
#define FIO_STR_INIT_EXISTING(buffer, length, capacity)                        \
  ((fio_str_s){.data = (buffer), .len = (length), .capa = (capacity)})

/**
 * Frees the String's resources and _reinitializes the container_.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 *       separately using `free(s)`.
 */
inline FIO_FUNC void fio_str_free(fio_str_s *s);

/* *****************************************************************************
String API - String state (data pointers, length, capacity, etc')
***************************************************************************** */

typedef struct {
  size_t capa;
  size_t len;
  char *data;
} fio_str_state_s;

/** Returns the String's complete state (capacity, length and pointer). */
inline FIO_FUNC fio_str_state_s fio_str_state(const fio_str_s *s);

/** Returns the String's length in bytes. */
inline FIO_FUNC size_t fio_str_len(fio_str_s *s);

/** Returns a pointer (`char *`) to the String's content. */
inline FIO_FUNC char *fio_str_data(fio_str_s *s);

/** Returns a byte pointer (`uint8_t *`) to the String's unsigned content. */
inline FIO_FUNC uint8_t *fio_str_bytes(fio_str_s *s);

/** Returns the String's existing capacity (allocated memory). */
inline FIO_FUNC size_t fio_str_capa(fio_str_s *s);

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 *
 * Returns the updated state of the String.
 *
 * Note: When shrinking, any existing data beyond the new size may be corrupted.
 */
inline FIO_FUNC fio_str_state_s fio_str_resize(fio_str_s *s, size_t size);

/**
 * Clears the string (retaining the existing capacity).
 */
#define fio_str_clear(s) fio_str_resize((s), 0)

/* *****************************************************************************
String API - Content Manipulation and Review
***************************************************************************** */

/**
 * Writes data at the end of the String (similar to `fio_str_insert` with the
 * argument `pos == -1`).
 */
inline FIO_FUNC fio_str_state_s fio_str_write(fio_str_s *s, const void *src,
                                              size_t src_len);

/**
 * Appens the `src` String to the end of the `dest` String.
 */
inline FIO_FUNC fio_str_state_s fio_str_concat(fio_str_s *dest,
                                               fio_str_s const *src);

/**
 * Replaces the data in the String - replacing `old_len` bytes starting at
 * `start_pos`, with the data at `src` (`src_len` bytes long).
 *
 * Negative `start_pos` values are calculated backwards, `-1` == end of String.
 *
 * When `old_len` is zero, the function will insert the data at `start_pos`.
 *
 * If `src_len == 0` than `src` will be ignored and the data marked for
 * replacement will be erased.
 */
inline FIO_FUNC fio_str_state_s fio_str_replace(fio_str_s *s,
                                                intptr_t start_pos,
                                                size_t old_len, const void *src,
                                                size_t src_len);

/**
 * Writes to the String using a vprintf like interface.
 *
 * Data is written to the end of the String.
 */
FIO_FUNC fio_str_state_s fio_str_vprintf(fio_str_s *s, const char *format,
                                         va_list argv);

/**
 * Writes to the String using a printf like interface.
 *
 * Data is written to the end of the String.
 */
FIO_FUNC fio_str_state_s fio_str_printf(fio_str_s *s, const char *format, ...);

/**
 * Prevents further manipulations to the String's content.
 */
inline FIO_FUNC void fio_str_freeze(fio_str_s *s);

/**
 * Binary comparison returns `1` if both strings are equal and `0` if not.
 */
inline FIO_FUNC int fio_str_iseq(const fio_str_s *str1, const fio_str_s *str2);

/* *****************************************************************************
String API - Memory management
***************************************************************************** */

/** Performs a best attempt at minimizing memory consumption. */
inline FIO_FUNC void fio_str_compact(fio_str_s *s);

/**
 * Requires the String to have at least `needed` capacity. Returns the current
 * state of the String.
 */
FIO_FUNC fio_str_state_s fio_str_capa_assert(fio_str_s *s, size_t needed);

/* *****************************************************************************


                              IMPLEMENTATION


***************************************************************************** */

/* *****************************************************************************
Implementation - String state (data pointers, length, capacity, etc')
***************************************************************************** */

/* the capacity when the string is stored in the container itself */
#define FIO_STR_SMALL_CAPA                                                     \
  (sizeof(fio_str_s) - (size_t)(&((fio_str_s *)0)->reserved))

typedef struct {
  uint8_t small;
  uint8_t frozen;
  char data[1];
} fio_str__small_s;

/** Returns the String's state (capacity, length and pointer). */
inline FIO_FUNC fio_str_state_s fio_str_state(const fio_str_s *s) {
  if (!s)
    return (fio_str_state_s){.capa = 0};
  return (s->small || !s->data)
             ? (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                                 .len = (size_t)(s->small >> 1),
                                 .data = ((fio_str__small_s *)s)->data}
             : (fio_str_state_s){
                   .capa = s->capa, .len = s->len, .data = s->data};
}

/**
 * Frees the String's resources and reinitializes the container.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 * separately using `free(s)`.
 */
inline FIO_FUNC void fio_str_free(fio_str_s *s) {
  if (!s->small)
    free(s->data);
  *s = FIO_STR_INIT;
}

/** Returns the String's length in bytes. */
inline FIO_FUNC size_t fio_str_len(fio_str_s *s) {
  return (s->small || !s->data) ? (s->small >> 1) : s->len;
}

/** Returns a pointer (`char *`) to the String's content. */
inline FIO_FUNC char *fio_str_data(fio_str_s *s) {
  return (s->small || !s->data) ? (((fio_str__small_s *)s)->data) : s->data;
}

/** Returns a byte pointer (`uint8_t *`) to the String's unsigned content. */
inline FIO_FUNC uint8_t *fio_str_bytes(fio_str_s *s) {
  return (uint8_t *)fio_str_data(s);
}

/** Returns the String's existing capacity (allocated memory). */
inline FIO_FUNC size_t fio_str_capa(fio_str_s *s) {
  return (s->small || !s->data) ? (FIO_STR_SMALL_CAPA - 1) : s->capa;
}

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 *
 * Returns the updated state of the String.
 *
 * Note: When shrinking, any existing data beyond the new size may be corrupted.
 */
inline FIO_FUNC fio_str_state_s fio_str_resize(fio_str_s *s, size_t size) {
  if (!s || s->frozen) {
    return fio_str_state(s);
  }
  fio_str_capa_assert(s, size);
  if (s->small || !s->data) {
    s->small = (uint8_t)(((size << 1) | 1) & 0xFF);
    ((fio_str__small_s *)s)->data[size] = 0;
    return (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                             .len = size,
                             .data = ((fio_str__small_s *)s)->data};
  }
  s->len = size;
  s->data[size] = 0;
  return (fio_str_state_s){.capa = s->capa, .len = size, .data = s->data};
}

/* *****************************************************************************
Implementation - Memory management
***************************************************************************** */

/**
 * Rounds up allocated capacity to the closest 2 words byte boundary (leaving 1
 * byte space for the NUL byte).
 *
 * This shouldn't effect actual allocation size and should only minimize the
 * effects of the memory allocator's alignment rounding scheme.
 *
 * To clarify:
 *
 * Memory allocators are required to allocate memory on the minimal alignment
 * required by the largest type (`long double`), which usually results in memory
 * allocations using this alignment as a minimal spacing.
 *
 * For example, on 64 bit architectures, it's likely that `malloc(18)` will
 * allocate the same amount of memory as `malloc(32)` due to alignment.
 *
 * In fact, on some allocators (i.e., jemalloc), spacing increases for larger
 * allocations - meaning the allocator will round up to more than 16 bytes, as
 * noted here: http://jemalloc.net/jemalloc.3.html#size_classes
 *
 * Note that this increased spacing, doesn't occure with facil.io's `fio_mem.h`
 * allocator, since it uses 16 byte alignment right up until allocations are
 * routed directly to `mmap` (due to their size, usually over 12KB).
 */
#define ROUND_UP_CAPA_2WORDS(num)                                              \
  (((num + 1) & (sizeof(long double) - 1))                                     \
       ? ((num + 1) | (sizeof(long double) - 1))                               \
       : (num))
/**
 * Requires the String to have at least `needed` capacity. Returns the current
 * state of the String.
 */
FIO_FUNC fio_str_state_s fio_str_capa_assert(fio_str_s *s, size_t needed) {
  if (!s)
    return (fio_str_state_s){.capa = 0};
  char *tmp;
  if (s->small || !s->data) {
    goto is_small;
  }
  if (needed > s->capa) {
    needed = ROUND_UP_CAPA_2WORDS(needed);
    tmp = (char *)realloc(s->data, needed + 1);
    FIO_ASSERT_ALLOC(tmp);
    s->capa = needed;
    s->data = tmp;
    s->data[needed] = 0;
  }
  return (fio_str_state_s){.capa = s->capa, .len = s->len, .data = s->data};

is_small:
  /* small string (string data is within the container) */
  if (needed < FIO_STR_SMALL_CAPA) {
    return (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                             .len = (size_t)(s->small >> 1),
                             .data = ((fio_str__small_s *)s)->data};
  }
  needed = ROUND_UP_CAPA_2WORDS(needed);
  tmp = (char *)malloc(needed + 1);
  FIO_ASSERT_ALLOC(tmp);
  const size_t existing_len = (size_t)((s->small >> 1) & 0xFF);
  if (existing_len) {
    memcpy(tmp, ((fio_str__small_s *)s)->data, existing_len + 1);
  } else {
    tmp[0] = 0;
  }
  *s = (fio_str_s){
      .small = 0,
      .capa = needed,
      .len = existing_len,
      .data = tmp,
  };
  return (fio_str_state_s){
      .capa = needed, .len = existing_len, .data = s->data};
}

/** Performs a best attempt at minimizing memory consumption. */
inline FIO_FUNC void fio_str_compact(fio_str_s *s) {
  if (!s || (s->small || !s->data))
    return;
  char *tmp;
  if (s->len < FIO_STR_SMALL_CAPA)
    goto shrink2small;
  tmp = realloc(s->data, s->len + 1);
  FIO_ASSERT_ALLOC(tmp);
  s->data = tmp;
  s->capa = s->len;
  return;

shrink2small:
  /* move the string into the container */
  tmp = s->data;
  size_t len = s->len;
  *s = (fio_str_s){.small = (uint8_t)(((len << 1) | 1) & 0xFF),
                   .frozen = s->frozen};
  if (len) {
    memcpy(((fio_str__small_s *)s)->data, tmp, len + 1);
  }
  free(tmp);
}

/* *****************************************************************************
Implementation - Content Manipulation and Review
***************************************************************************** */

/**
 * Writes data at the end of the String (similar to `fio_str_insert` with the
 * argument `pos == -1`).
 */
inline FIO_FUNC fio_str_state_s fio_str_write(fio_str_s *s, const void *src,
                                              size_t src_len) {
  if (!s || !src_len || !src || s->frozen)
    return fio_str_state(s);
  fio_str_state_s state = fio_str_resize(s, src_len + fio_str_len(s));
  memcpy(state.data + (state.len - src_len), src, src_len);
  return state;
}

/**
 * Appens the `src` String to the end of the `dest` String.
 */
inline FIO_FUNC fio_str_state_s fio_str_concat(fio_str_s *dest,
                                               fio_str_s const *src) {
  if (!dest || !src || dest->frozen)
    return fio_str_state(dest);
  fio_str_state_s src_state = fio_str_state(src);
  if (!src_state.len)
    return fio_str_state(dest);
  fio_str_state_s state =
      fio_str_resize(dest, src_state.len + fio_str_len(dest));
  memcpy(state.data + state.len - src_state.len, src_state.data, src_state.len);
  return state;
}

/**
 * Replaces the data in the String - replacing `old_len` bytes starting at
 * `start_pos`, with the data at `src` (`src_len` bytes long).
 *
 * Negative `start_pos` values are calculated backwards, `-1` == end of String.
 *
 * When `old_len` is zero, the function will insert the data at `start_pos`.
 *
 * If `src_len == 0` than `src` will be ignored and the data marked for
 * replacement will be erased.
 */
inline FIO_FUNC fio_str_state_s fio_str_replace(fio_str_s *s,
                                                intptr_t start_pos,
                                                size_t old_len, const void *src,
                                                size_t src_len) {
  fio_str_state_s state = fio_str_state(s);
  if (!s || s->frozen || (!old_len && !src_len))
    return state;

  if (start_pos < 0) {
    /* backwards position indexing */
    start_pos += s->len + 1;
    if (start_pos < 0)
      start_pos = 0;
  }

  if (start_pos + old_len >= state.len) {
    /* old_len overflows the end of the String */
    if (s->small || !s->data) {
      s->small = 1 | ((size_t)((start_pos << 1) & 0xFF));
    } else {
      s->len = start_pos;
    }
    return fio_str_write(s, src, src_len);
  }

  /* data replacement is now always in the middle (or start) of the String */
  const size_t new_size = state.len + (src_len - old_len);

  if (old_len != src_len) {
    /* there's an offset requiring an adjustment */
    if (old_len < src_len) {
      /* make room for new data */
      const size_t offset = src_len - old_len;
      state = fio_str_resize(s, state.len + offset);
    }
    memmove(state.data + start_pos + src_len, state.data + start_pos + old_len,
            (state.len - start_pos) - old_len);
  }
  if (src_len) {
    memcpy(state.data + start_pos, src, src_len);
  }

  return fio_str_resize(s, new_size);
}

/** Writes to the String using a vprintf like interface. */
FIO_FUNC __attribute__((format(printf, 2, 0))) fio_str_state_s
fio_str_vprintf(fio_str_s *s, const char *format, va_list argv) {
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len <= 0)
    return fio_str_state(s);
  fio_str_state_s state = fio_str_resize(s, len + fio_str_len(s));
  vsnprintf(state.data + (state.len - len), len + 1, format, argv);
  return state;
}

/** Writes to the String using a printf like interface. */
FIO_FUNC __attribute__((format(printf, 2, 3))) fio_str_state_s
fio_str_printf(fio_str_s *s, const char *format, ...) {
  va_list argv;
  va_start(argv, format);
  fio_str_state_s state = fio_str_vprintf(s, format, argv);
  va_end(argv);
  return state;
}

/**
 * Prevents further manipulations to the String's content.
 */
inline FIO_FUNC void fio_str_freeze(fio_str_s *s) {
  if (!s)
    return;
  s->frozen = 1;
}

/**
 * Binary comparison returns `1` if both strings are equal and `0` if not.
 */
inline FIO_FUNC int fio_str_iseq(const fio_str_s *str1, const fio_str_s *str2) {
  if (str1 == str2)
    return 1;
  if (!str1 || !str2)
    return 0;
  fio_str_state_s s1 = fio_str_state(str1);
  fio_str_state_s s2 = fio_str_state(str2);
  return (s1.len == s2.len && !memcmp(s1.data, s2.data, s1.len));
}

/* *****************************************************************************
Testing
***************************************************************************** */

#if DEBUG
#include <stdio.h>
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }
/**
 * Removes any FIO_ARY_TYPE_INVALID  *pointers* from an Array, keeping all other
 * data in the array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
FIO_FUNC inline void fio_str_test(void) {
  fprintf(stderr, "=== Testing Core String features (fio_str.h)\n");
  fprintf(stderr, "* String container size: %zu\n", sizeof(fio_str_s));
  fprintf(stderr,
          "* Self-Contained String Capacity (FIO_STR_SMALL_CAPA): %zu\n",
          FIO_STR_SMALL_CAPA);
  fio_str_s str = FIO_STR_INIT;
  TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
              "Small String capacity reporting error!");
  TEST_ASSERT(fio_str_len(&str) == 0, "Small String length reporting error!");
  TEST_ASSERT(fio_str_data(&str) ==
                  (char *)((uintptr_t)(&str + 1) - FIO_STR_SMALL_CAPA),
              "Small String pointer reporting error!");
  fio_str_write(&str, "World", 4);
  TEST_ASSERT(str.small,
              "Small String writing error - not small on small write!");
  TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
              "Small String capacity reporting error after write!");
  TEST_ASSERT(fio_str_len(&str) == 4,
              "Small String length reporting error after write!");
  TEST_ASSERT(fio_str_data(&str) ==
                  (char *)((uintptr_t)(&str + 1) - FIO_STR_SMALL_CAPA),
              "Small String pointer reporting error after write!");
  TEST_ASSERT(strlen(fio_str_data(&str)) == 4,
              "Small String NUL missing after write (%zu)!",
              strlen(fio_str_data(&str)));
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Worl"),
              "Small String write error (%s)!", fio_str_data(&str));

  fio_str_capa_assert(&str, sizeof(fio_str_s) - 1);
  TEST_ASSERT(!str.small,
              "Long String reporting as small after capacity update!");
  TEST_ASSERT(fio_str_capa(&str) == sizeof(fio_str_s) - 1,
              "Long String capacity update error (%zu != %zu)!",
              fio_str_capa(&str), sizeof(fio_str_s));
  TEST_ASSERT(
      fio_str_len(&str) == 4,
      "Long String length changed during conversion from small string (%zu)!",
      fio_str_len(&str));
  TEST_ASSERT(fio_str_data(&str) == str.data,
              "Long String pointer reporting error after capacity update!");
  TEST_ASSERT(strlen(fio_str_data(&str)) == 4,
              "Long String NUL missing after capacity update (%zu)!",
              strlen(fio_str_data(&str)));
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Worl"),
              "Long String value changed after capacity update (%s)!",
              fio_str_data(&str));

  fio_str_write(&str, "d!", 2);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "World!"),
              "Long String `write` error (%s)!", fio_str_data(&str));

  fio_str_replace(&str, 0, 0, "Hello ", 6);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello World!"),
              "Long String `insert` error (%s)!", fio_str_data(&str));

  fio_str_resize(&str, 6);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello "),
              "Long String `resize` clipping error (%s)!", fio_str_data(&str));

  fio_str_replace(&str, 6, 0, "My World!", 9);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello My World!"),
              "Long String `replace` error when testing overflow (%s)!",
              fio_str_data(&str));

  str.capa = str.len;
  fio_str_replace(&str, -10, 2, "Big", 3);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello Big World!"),
              "Long String `replace` error when testing splicing (%s)!",
              fio_str_data(&str));

  TEST_ASSERT(
      fio_str_capa(&str) == ROUND_UP_CAPA_2WORDS(strlen("Hello Big World!")),
      "Long String `fio_str_replace` capacity update error (%zu != %zu)!",
      fio_str_capa(&str), ROUND_UP_CAPA_2WORDS(strlen("Hello Big World!")));

  if (str.len < FIO_STR_SMALL_CAPA) {
    fio_str_compact(&str);
    TEST_ASSERT(str.small, "Compacting didn't change String to small!");
    TEST_ASSERT(fio_str_len(&str) == strlen("Hello Big World!"),
                "Compacting altered String length! (%zu != %zu)!",
                fio_str_len(&str), strlen("Hello Big World!"));
    TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello Big World!"),
                "Compact data error (%s)!", fio_str_data(&str));
    TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
                "Compacted String capacity reporting error!");
  } else {
    fprintf(stderr, "* skipped `compact` test!\n");
  }

  {
    fio_str_freeze(&str);
    fio_str_state_s old_state = fio_str_state(&str);
    fio_str_write(&str, "more data to be written here", 28);
    fio_str_replace(&str, 2, 1, "more data to be written here", 28);
    fio_str_state_s new_state = fio_str_state(&str);
    TEST_ASSERT(old_state.len == new_state.len,
                "Frozen String length changed!");
    TEST_ASSERT(old_state.data == new_state.data,
                "Frozen String pointer changed!");
    TEST_ASSERT(
        old_state.capa == new_state.capa,
        "Frozen String capacity changed (allowed, but shouldn't happen)!");
    str.frozen = 0;
  }
  fio_str_printf(&str, " %u", 42);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello Big World! 42"),
              "`fio_str_printf` data error (%s)!", fio_str_data(&str));

  {
    fio_str_s str2 = FIO_STR_INIT;
    fio_str_concat(&str2, &str);
    TEST_ASSERT(fio_str_iseq(&str, &str2),
                "`fio_str_concat` error, strings not equal (%s != %s)!",
                fio_str_data(&str), fio_str_data(&str2));
    fio_str_write(&str2, ":extra data", 11);
    TEST_ASSERT(
        !fio_str_iseq(&str, &str2),
        "`fio_str_write` error after copy, strings equal ((%zu)%s == (%zu)%s)!",
        fio_str_len(&str), fio_str_data(&str), fio_str_len(&str2),
        fio_str_data(&str2));

    fio_str_free(&str2);
  }

  fio_str_free(&str);
  fprintf(stderr, "* passed.\n");
}
#undef TEST_ASSERT
#endif

/* *****************************************************************************
Done
***************************************************************************** */

#ifdef __cplusplus
#undef register
#endif

#undef FIO_FUNC
#undef FIO_ASSERT_ALLOC
#undef ROUND_UP_CAPA_2WORDS

#endif
