
#include "bscrypt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline int seek1(uint8_t **buffer, uint8_t *limit, const uint8_t c) {
  if (limit - *buffer == 0)
    return 0;
  void *tmp = memchr(*buffer, c, limit - (*buffer));
  if (tmp) {
    *buffer = tmp;
    return 1;
  }
  *buffer = limit;
  return 0;
}

static inline int seek2(uint8_t **buffer, uint8_t *limit, const uint8_t c) {
  while (*buffer < limit) {
    if (**buffer == c)
      return 1;
    (*buffer)++;
  }
  return 0;
}

static inline int seek3(uint8_t **buffer, uint8_t *limit, const uint8_t c) {
  if (**buffer == c)
    return 1;
  // while (*buffer < limit && ((uintptr_t)(*buffer) & 15)) {
  //   if (**buffer == c)
  //     return 1;
  //   (*buffer)++;
  // }
  // if (*buffer > limit)
  //   return 0;

  uint64_t wanted1 = 0x0101010101010101ULL * c;
  // uint64_t wanted2 = 0x0101010101010101ULL * '\n';
  uint64_t *lpos = (uint64_t *)*buffer;
  uint64_t *llimit = ((uint64_t *)limit) - 1;

  for (; lpos < llimit; lpos++) {
    const uint64_t eq1 = ~((*lpos) ^ wanted1);
    // const uint64_t eq2 = ~((*lpos) ^ wanted2);
    const uint64_t t0 = (eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
    const uint64_t t1 = (eq1 & 0x8080808080808080llu);
    // const uint64_t t2 = (eq2 & 0x7f7f7f7f7f7f7f7fllu) +
    // 0x0101010101010101llu; const uint64_t t3 = (eq2 & 0x8080808080808080llu);
    if ((t0 & t1) /* || (t2 & t3) */) {
      break;
    }
  }

  *buffer = (uint8_t *)lpos;
  while (*buffer < limit) {
    if (**buffer == c /* || **buffer == '\n' */)
      return 1;
    (*buffer)++;
  }
  return 0;
}

static inline int seek4(uint8_t **buffer, uint8_t *limit, const uint8_t c) {
#ifndef __SIZEOF_INT128__
  return 0;
#else
  if (**buffer == c)
    return 1;
  // while (*buffer < limit && ((uintptr_t)(*buffer) & 15)) {
  //   if (**buffer == c)
  //     return 1;
  //   (*buffer)++;
  // }
  // if (*buffer > limit)
  //   return 0;

  const __uint128_t just_1_bit = ((((__uint128_t)0x0101010101010101ULL) << 64) |
                                  (__uint128_t)0x0101010101010101ULL);
  const __uint128_t is_7bit_set =
      ((((__uint128_t)0x7f7f7f7f7f7f7f7fULL) << 64) |
       (__uint128_t)0x7f7f7f7f7f7f7f7fULL);
  const __uint128_t is_1bit_set =
      ((((__uint128_t)0x8080808080808080ULL) << 64) |
       (__uint128_t)0x8080808080808080ULL);

  __uint128_t wanted1 = just_1_bit * c;
  __uint128_t *lpos = (__uint128_t *)*buffer;
  __uint128_t *llimit = ((__uint128_t *)limit) - 1;

  for (; lpos < llimit; lpos++) {
    const __uint128_t eq1 = ~((*lpos) ^ wanted1);
    const __uint128_t t0 = (eq1 & is_7bit_set) + just_1_bit;
    const __uint128_t t1 = (eq1 & is_1bit_set);
    if ((t0 & t1)) {
      break;
    }
  }

  *buffer = (uint8_t *)lpos;
  while (*buffer < limit) {
    if (**buffer == c)
      return 1;
    (*buffer)++;
  }
  return 0;
#endif
}

#define RUNS 8
void test_seek(int argc, char const **argv) {
  clock_t start, end;
  uint8_t *pos;
  uint8_t *stop;
  size_t count;

  struct {
    int (*func)(uint8_t **buffer, uint8_t *limit, const uint8_t c);
    const char *name;
  } seek_funcs[] = {
      {.func = seek1, .name = "seek1"}, {.func = seek2, .name = "seek2"},
      {.func = seek3, .name = "seek3"}, {.func = seek4, .name = "seek4"},
      {.func = NULL, .name = NULL},
  };
  size_t func_pos = 0;

  if (argc < 3 || argv[1][0] != '-' || argv[1][1] != 'f') {
    fprintf(stderr, "\nUse `-f` to load file data\n");
    exit(-1);
  }
  fdump_s *data = bscrypt_fdump(argv[2], 0);
  if (!data) {
    fprintf(stderr, "ERROR: Couldn't open file %s\n", argv[2]);
    exit(-1);
  }

  while (seek_funcs[func_pos].func) {
    fprintf(stderr, "Testing %s:\n  (", seek_funcs[func_pos].name);
    size_t avrg = 0;
    for (size_t i = 0; i < RUNS; i++) {
      if (i)
        fprintf(stderr, " + ");
      pos = (uint8_t *)data->data;
      stop = (uint8_t *)data->data + data->length;
      count = 0;
      start = clock();
      while (pos < stop && seek_funcs[func_pos].func(&pos, stop, argv[3][0])) {
        pos++;
        count++;
      }
      end = clock();
      avrg += end - start;
      fprintf(stderr, "%lu", end - start);
    }
    fprintf(stderr, ")/%d\n === finding %lu items in %lu bytes took %lfs\n\n",
            RUNS, count, data->length, (avrg / RUNS) / (1.0 * CLOCKS_PER_SEC));
    func_pos++;
  }

  free(data);
}
