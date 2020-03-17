/*
Copyright: Boaz Segev, 2018-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <fio.c>
#include <fio.h>

#define FIO_RAND 1
#define FIO_LOG 1
#define FIO_RISKY_HASH 1
#include <fio-stl.h>

#define FIO_CLI 1
#include <fio-stl.h>

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#ifndef TEST_XXHASH
#define TEST_XXHASH 1
#endif

/* *****************************************************************************
State machine and types
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define FIO_STR_NAME fio_str
#include <fio-stl.h>
#endif

static uint8_t print_flag = 0;

static inline int fio_str_eq_print(fio_str_s *a, fio_str_s *b) {
  /* always return 1, to avoid internal set collision mitigation. */
  if (print_flag)
    fprintf(stderr,
            "* Collision Detected: %s vs. %s\n",
            fio_str2ptr(a),
            fio_str2ptr(b));
  return 1;
}

// static inline void destroy_collision_object(fio_str_s *a) {
//   fprintf(stderr, "* Collision Detected: %s\n", fio_str2ptr(a));
//   fio_str_free2(a);
// }

#define FIO_MAP_NAME collisions
#define FIO_MAP_TYPE fio_str_s *
#define FIO_MAP_TYPE_COPY(dest, src)                                           \
  do {                                                                         \
    (dest) = fio_str_new();                                                    \
    fio_str_init_copy2((dest), (src));                                         \
  } while (0)
#define FIO_MAP_TYPE_CMP(a, b) fio_str_eq_print((a), (b))
#define FIO_MAP_TYPE_DESTROY(a) fio_str_free((a))
#include <fio-stl.h>

typedef uintptr_t (*hashing_func_fn)(char *, size_t);
#define FIO_MAP_NAME hash_name
#define FIO_MAP_TYPE hashing_func_fn
#include <fio-stl.h>

#define FIO_ARRAY_NAME words
#define FIO_ARRAY_TYPE fio_str_s
#define FIO_ARRAY_TYPE_CMP(a, b) fio_str_is_eq(&(a), &(b))
#define FIO_ARRAY_TYPE_COPY(dest, src) fio_str_init_copy2(&(dest), &(src))
#define FIO_ARRAY_TYPE_DESTROY(a) fio_str_destroy(&(a))
#include <fio-stl.h>

static hash_name_s hash_names = FIO_MAP_INIT;
static words_s words = FIO_ARRAY_INIT;

/* *****************************************************************************
Main
***************************************************************************** */

static void test_hash_function(hashing_func_fn h);
static void initialize_cli(int argc, char const *argv[]);
static void load_words(void);
static void initialize_hash_names(void);
static void print_hash_names(void);
static char *hash_name(hashing_func_fn fn);
static void cleanup(void);
static void find_bit_collisions(hashing_func_fn fn, size_t count, uint8_t bits);
static void find_full_collisions_rot(hashing_func_fn fn);

int main(int argc, char const *argv[]) {
  // FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  initialize_cli(argc, argv);
  load_words();
  initialize_hash_names();
  if (fio_cli_get("-t")) {
    fio_str_s tmp;
    fio_str_init_const(&tmp, fio_cli_get("-t"), strlen(fio_cli_get("-t")));
    hashing_func_fn h = hash_name_get(&hash_names, fio_str_hash(&tmp, 0), NULL);
    if (h) {
      test_hash_function(h);
      if (fio_cli_get_i("-b"))
        find_bit_collisions(h, 8, fio_cli_get_i("-b"));
      if (fio_cli_get_i("-r"))
        find_full_collisions_rot(h);
      if (1) {
        fprintf(stderr, "10 sample hashes:\n");
        for (size_t i = 0; i < 10; ++i) {
          uint64_t hash = h((char *)&i, sizeof(i));
          char sbuf[80];
          sbuf[fio_ltoa(sbuf, hash, 2)] = 0;
          fprintf(stderr, "%s\n", sbuf);
        }
      }
    } else {
      FIO_LOG_ERROR("Test function %s unknown.", tmp.buf);
      fprintf(stderr, "Try any of the following:\n");
      print_hash_names();
    }
  } else {
    uint8_t skip = 0;
    FIO_MAP_EACH(&hash_names, pos) {
      /* skip collision tester */
      if (!skip) {
        ++skip;
        continue;
      }
      test_hash_function(pos->obj);
      if (skip < 2) {
        /* don't brute force the counter. It might never happen... */
        ++skip;
        continue;
      }
      if (fio_cli_get_i("-b")) {
        if (fio_cli_get_i("-b") < 0) {
          fprintf(
              stderr,
              "ERROR: brute force bits value can't be less than zero (%zu).\n",
              (size_t)fio_cli_get_i("-b"));
        } else if (fio_cli_get_i("-b") > 20) {
          fprintf(
              stderr,
              "ERROR: brute force bits value above testing limit (%zu > 20).\n",
              (size_t)fio_cli_get_i("-b"));
        } else {
          find_bit_collisions(pos->obj, 8, fio_cli_get_i("-b"));
        }
      }
    }
  }
  cleanup();
  return 0;
}

/* *****************************************************************************
CLI
***************************************************************************** */

static void initialize_cli(int argc, char const *argv[]) {
  fio_cli_start(
      argc,
      argv,
      0,
      0,
      "This is a Hash algorythm collision test program. It accepts the "
      "following arguments:",
      FIO_CLI_STRING(
          "-test -t test only the specified algorithm. Options include:"),
#ifdef H_FACIL_IO_H
      FIO_CLI_PRINT("\t\tsiphash13"),
      FIO_CLI_PRINT("\t\tsiphash24"),
      FIO_CLI_PRINT("\t\tsha1"),
#endif
      FIO_CLI_PRINT("\t\trisky (fio_str_hash_risky)"),
      FIO_CLI_PRINT("\t\trisky2 (fio_str_hash_risky alternative)"),
      // FIO_CLI_PRINT("\t\txor (xor all bytes and length)"),
      FIO_CLI_STRING(
          "-dictionary -d a text file containing words separated by an "
          "EOL marker."),
      FIO_CLI_INT("-bits -b number of collision bits in a brute force attack."),
      FIO_CLI_BOOL(
          "-rotate -r tests against rotation attack on a 512 bit vector."),
      FIO_CLI_BOOL("-v make output more verbouse (debug mode)"));
  if (fio_cli_get_bool("-v"))
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  FIO_LOG_DEBUG("initialized CLI.");
}

/* *****************************************************************************
Dictionary management
***************************************************************************** */

static void load_words(void) {
  fio_str_s filename = FIO_STR_INIT;
  fio_str_s data = FIO_STR_INIT;

  if (fio_cli_get("-d")) {
    fio_str_write(&filename, fio_cli_get("-d"), strlen(fio_cli_get("-d")));
  } else {
    fio_str_info_s tmp = fio_str_write(&filename, __FILE__, strlen(__FILE__));
    while (tmp.len && tmp.buf[tmp.len - 1] != '/') {
      --tmp.len;
    }
    fio_str_resize(&filename, tmp.len);
    fio_str_write(&filename, "words.txt", 9);
  }
  fio_str_readfile(&data, fio_str2ptr(&filename), 0, 0);
  fio_str_info_s d = fio_str_info(&data);
  if (d.len == 0) {
    FIO_LOG_FATAL("Couldn't find / read dictionary file (or no words?)");
    FIO_LOG_FATAL("\tmissing or empty: %s", fio_str2ptr(&filename));
    cleanup();
    fio_str_destroy(&filename);
    exit(-1);
  }
  /* assume an avarage of 8 letters per word */
  words_reserve(&words, d.len >> 3);
  while (d.len) {
    char *eol = memchr(d.buf, '\n', d.len);
    fio_str_s wrd = FIO_STR_INIT;
    if (!eol) {
      /* push what's left */
      fio_str_init_const(&wrd, d.buf, d.len);
      words_push(&words, wrd);
      break;
    }
    if (eol == d.buf || (eol == d.buf + 1 && eol[-1] == '\r')) {
      /* empty line */
      ++d.buf;
      --d.len;
      continue;
    }
    fio_str_init_const(&wrd, d.buf, (eol - (d.buf + (eol[-1] == '\r'))));
    words_push(&words, wrd);
    d.len -= (eol + 1) - d.buf;
    d.buf = eol + 1;
  }
  words_compact(&words);
  fio_str_destroy(&filename);
  fio_str_destroy(&data);
  FIO_LOG_INFO("Loaded %zu words.", words_count(&words));
}

/* *****************************************************************************
Cleanup
***************************************************************************** */

static void cleanup(void) {
  print_flag = 0;
  hash_name_destroy(&hash_names);
  words_destroy(&words);
}

/* *****************************************************************************
Hash functions
***************************************************************************** */

#ifdef H_FACIL_IO_H

static uintptr_t siphash13(char *data, size_t len) {
  return fio_siphash13(data, len, 0, 0);
}

static uintptr_t siphash24(char *data, size_t len) {
  return fio_siphash24(data, len, 0, 0);
}
static uintptr_t sha1(char *data, size_t len) {
  fio_sha1_s s = fio_sha1_init();
  fio_sha1_write(&s, data, len);
  return ((uintptr_t *)fio_sha1_result(&s))[0];
}

#endif /* H_FACIL_IO_H */

static uintptr_t counter(char *data, size_t len) {
  static uintptr_t counter = 0;
  uint64_t v[4];
  // volatile uint64_t optimization_stopper = 0;

  for (size_t i = len >> 5; i; --i) {
    /* vectorized 32 bytes / 256 bit access */
    v[0] = fio_buf2u64_little(data);
    v[1] = fio_buf2u64_little(data + 8);
    v[2] = fio_buf2u64_little(data + 16);
    v[3] = fio_buf2u64_little(data + 24);
    // __builtin_memcpy(v, data, sizeof(uint64_t) * 4);
    // optimization_stopper ^= v[0] ^ v[1] ^ v[2] ^ v[3];
    data += 32;
    __asm__ volatile("" ::: "memory");
  }
  uint64_t tmp;
  /* 64 bit words  */
  switch (len & 24) {
  case 24:
    v[2] = fio_buf2u64_little(data + 16);
  case 16: /* overflow */
    v[1] = fio_buf2u64_little(data + 8);
  case 8: /* overflow */
    v[0] = fio_buf2u64_little(data);
    data += len & 24;
  }
  __asm__ volatile("" ::: "memory");
  tmp = 0;
  /* leftover bytes */
  switch ((len & 7)) {
  case 7:
    tmp |= ((uint64_t)data[6]) << 8; /* fallthrough */
  case 6:
    tmp |= ((uint64_t)data[5]) << 16; /* fallthrough */
  case 5:
    tmp |= ((uint64_t)data[4]) << 24; /* fallthrough */
  case 4:
    tmp |= ((uint64_t)data[3]) << 32; /* fallthrough */
  case 3:
    tmp |= ((uint64_t)data[2]) << 40; /* fallthrough */
  case 2:
    tmp |= ((uint64_t)data[1]) << 48; /* fallthrough */
  case 1:
    tmp |= ((uint64_t)data[0]) << 56;
  }
  switch ((len & 24)) { /* information about position */
  case 24:              /* offset in 32 byte segment */
    v[3] = tmp;
    break;
  case 16: /* offset in 32 byte segment */
    v[2] = tmp;
    break;
  case 8: /* offset in 32 byte segment */
    v[1] = tmp;
    break;
  case 0: /* offset in 32 byte segment */
    v[0] = tmp;
    break;
  }
  __asm__ volatile("" ::: "memory");
  return ++counter;
}

#define FIO_RISKY3_PRIME0 0xCAEF89D1E9A5EB21ULL
#define FIO_RISKY3_PRIME1 0xAB137439982B86C9ULL
#define FIO_RISKY3_PRIME2 0xD9FDC73ABE9EDECDULL
#define FIO_RISKY3_PRIME3 0x3532D520F9511B13ULL
#define FIO_RISKY3_PRIME4 0x038720DDEB5A8415ULL
#define FIO_RISKY3_IV0 0x0000001000000001ULL
#define FIO_RISKY3_IV1 0x0000010000000010ULL
#define FIO_RISKY3_IV2 0x0000100000000100ULL
#define FIO_RISKY3_IV3 0x0001000000001000ULL

static uint64_t fall(const void *data_, size_t len, uint64_t seed) {
  const uint64_t primes[] = {
      FIO_RISKY3_PRIME0,
      FIO_RISKY3_PRIME1,
      FIO_RISKY3_PRIME2,
      FIO_RISKY3_PRIME3,
  };
  register uint64_t v[] = {
      FIO_RISKY3_IV0,
      FIO_RISKY3_IV1,
      FIO_RISKY3_IV2,
      FIO_RISKY3_IV3,
  };
  const uint8_t *data = (const uint8_t *)data_;

#define FIO_RISKY3_ROUND64(vi, w)                                              \
  v[vi] += w;                                                                  \
  v[vi] = fio_lrot64(v[vi], 29);                                               \
  v[vi] += w;                                                                  \
  v[vi] *= primes[vi];

#define FIO_RISKY3_ROUND256(w0, w1, w2, w3)                                    \
  FIO_RISKY3_ROUND64(0, w0);                                                   \
  FIO_RISKY3_ROUND64(1, w1);                                                   \
  FIO_RISKY3_ROUND64(2, w2);                                                   \
  FIO_RISKY3_ROUND64(3, w3);

  if (seed) {
    /* process the seed as if it was a prepended 8 Byte string. */
    v[0] *= seed;
    v[1] *= seed;
    v[2] *= seed;
    v[3] *= seed;
    v[1] ^= seed;
    v[2] ^= seed;
    v[3] ^= seed;
  }

  for (size_t i = len >> 5; i; --i) {
    /* vectorized 32 bytes / 256 bit access */
    FIO_RISKY3_ROUND256(fio_buf2u64_little(data),
                        fio_buf2u64_little(data + 8),
                        fio_buf2u64_little(data + 16),
                        fio_buf2u64_little(data + 24));
    data += 32;
  }
  switch (len & 24) {
  case 24:
    FIO_RISKY3_ROUND64(2, fio_buf2u64_little(data + 16));
    /* fallthrough */
  case 16:
    FIO_RISKY3_ROUND64(1, fio_buf2u64_little(data + 8));
    /* fallthrough */
  case 8:
    FIO_RISKY3_ROUND64(0, fio_buf2u64_little(data + 0));
    data += len & 24;
  }

  uint64_t tmp = (len & 0xFF); /* add offset intformation to padding */
  /* leftover bytes */
  switch ((len & 7)) {
  case 7:
    tmp |= ((uint64_t)data[6]) << 8; /* fallthrough */
  case 6:
    tmp |= ((uint64_t)data[5]) << 16; /* fallthrough */
  case 5:
    tmp |= ((uint64_t)data[4]) << 24; /* fallthrough */
  case 4:
    tmp |= ((uint64_t)data[3]) << 32; /* fallthrough */
  case 3:
    tmp |= ((uint64_t)data[2]) << 40; /* fallthrough */
  case 2:
    tmp |= ((uint64_t)data[1]) << 48; /* fallthrough */
  case 1:
    tmp |= ((uint64_t)data[0]) << 56;
    /* the last (now padded) byte's position */
    switch ((len & 24)) {
    case 24: /* offset 24 in 32 byte segment */
      FIO_RISKY3_ROUND64(3, tmp);
      break;
    case 16: /* offset 16 in 32 byte segment */
      FIO_RISKY3_ROUND64(2, tmp);
      break;
    case 8: /* offset 8 in 32 byte segment */
      FIO_RISKY3_ROUND64(1, tmp);
      break;
    case 0: /* offset 0 in 32 byte segment */
      FIO_RISKY3_ROUND64(0, tmp);
      break;
    }
  }

  /* irreversible avalanche... I think */
  uint64_t r = (len)*0x0000001000000001ULL;
  r += fio_lrot64(v[0], 17) + fio_lrot64(v[1], 13) + fio_lrot64(v[2], 47) +
       fio_lrot64(v[3], 57);
  r += v[0] ^ v[1];
  r ^= fio_lrot64(r, 13);
  r += v[1] ^ v[2];
  r ^= fio_lrot64(r, 29);
  r += v[2] ^ v[3];
  r += fio_lrot64(r, 33);
  r += v[3] ^ v[0];
  r ^= fio_lrot64(r, 51);
  r ^= (r >> 29) * FIO_RISKY3_PRIME4;
  return r;
}

#if TEST_XXHASH
#include "xxhash.h"
static uintptr_t xxhash_test(char *data, size_t len) {
  return XXH64(data, len, 0);
}
#endif

/**
Working version.
*/
inline FIO_FUNC uint64_t fio_risky_hash_v3(const void *data,
                                           size_t len,
                                           uint64_t salt);

inline FIO_FUNC uintptr_t risky2(char *data, size_t len) {
  return fio_risky_hash_v3(data, len, 0);
}

inline FIO_FUNC uintptr_t fall_wrapper(char *data, size_t len) {
  return fall(data, len, (uint64_t)fall);
}

inline FIO_FUNC uintptr_t risky(char *data, size_t len) {
  return fio_risky_hash(data, len, 0);
}

inline FIO_FUNC uintptr_t collision_promise(char *data, size_t len) {
  return ((0x55555555ULL << 31) << 1) | 0x55555555ULL;
  (void)data;
  (void)len;
}

/* *****************************************************************************
Hash setup and testing...
***************************************************************************** */

struct hash_fn_names_s {
  char *name;
  hashing_func_fn fn;
} hash_fn_list[] = {
    {"collider", collision_promise},
    {"counter (no hash, RAM access test)", counter},
    {"fall", fall_wrapper},
#ifdef H_FACIL_IO_H
    {"siphash13", siphash13},
    {"siphash24", siphash24},
    {"sha1", sha1},
#endif /* H_FACIL_IO_H */
#if TEST_XXHASH
    {"xxhash", xxhash_test},
#endif
    {"risky", risky},
    {"risky2", risky2},
    {NULL, NULL},
};

static void initialize_hash_names(void) {
  for (size_t i = 0; hash_fn_list[i].name; ++i) {
    fio_str_s tmp;
    fio_str_init_const(
        &tmp, hash_fn_list[i].name, strlen(hash_fn_list[i].name));
    hash_name_set(&hash_names, fio_str_hash(&tmp, 0), hash_fn_list[i].fn, NULL);
    FIO_LOG_DEBUG("Registered %s hashing function.\n\t\t(%zu registered)",
                  hash_fn_list[i].name,
                  hash_name_count(&hash_names));
  }
}

static char *hash_name(hashing_func_fn fn) {
  for (size_t i = 0; hash_fn_list[i].name; ++i) {
    if (hash_fn_list[i].fn == fn)
      return hash_fn_list[i].name;
  }
  return NULL;
}

static void print_hash_names(void) {
  for (size_t i = 0; hash_fn_list[i].name; ++i) {
    fprintf(stderr, "* %s\n", hash_fn_list[i].name);
  }
}

static void test_hash_function_speed(hashing_func_fn h, char *name) {
  FIO_LOG_DEBUG("Speed testing for %s", name);
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uint8_t buffer[8192];
  memset(buffer, 'T', sizeof(buffer));
  /* warmup */
  uint64_t hash = 0;
  for (size_t i = 0; i < 4; i++) {
    hash += h((char *)buffer, sizeof(buffer));
    memcpy(buffer, &hash, sizeof(hash));
  }
  /* loop until test runs for more than 2 seconds */
  for (uint64_t cycles = (8192 << 4);;) {
    clock_t start, end;
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      hash += h((char *)buffer, sizeof(buffer));
      __asm__ volatile("" ::: "memory");
    }
    end = clock();
    memcpy(buffer, &hash, sizeof(hash));
    if ((end - start) >= (2 * CLOCKS_PER_SEC) ||
        cycles >= ((uint64_t)1 << 62)) {
      fprintf(stderr,
              "%-20s %8.2f MB/s\n",
              name,
              (double)(sizeof(buffer) * cycles) /
                  (((end - start) * (1000000.0 / CLOCKS_PER_SEC))));
      break;
    }
    cycles <<= 1;
  }
}

static void test_hash_function(hashing_func_fn h) {
  size_t best_count = 0, best_capa = 1024;
#define test_for_best()                                                        \
  if (collisions_capa(&c) > 1024 &&                                            \
      (collisions_count(&c) * (double)1 / collisions_capa(&c)) >               \
          (best_count * (double)1 / best_capa)) {                              \
    best_count = collisions_count(&c);                                         \
    best_capa = collisions_capa(&c);                                           \
  }
  char *name = NULL;
  for (size_t i = 0; hash_fn_list[i].name; ++i) {
    if (hash_fn_list[i].fn == h) {
      name = hash_fn_list[i].name;
      break;
    }
  }
  if (!name)
    name = "unknown";
  fprintf(stderr, "======= %s\n", name);
  /* Sanity test */
  {
    uint64_t s1 = h("", 0);
    uint64_t s2 = h("\0", 1);
    uint64_t s3 = h("\0\0", 2);
    // fprintf(stderr, "Zero hash 0: %p\n", (void *)s1);
    // fprintf(stderr, "Zero hash 1: %p\n", (void *)s2);
    // fprintf(stderr, "Zero hash 2: %p\n", (void *)s3);
    FIO_ASSERT(s1 != s2 && s2 != s3 && s1 != s3, "Zero padding test failed!");
  }
  /* Speed test */
  test_hash_function_speed(h, name);
  /* Collision test */
  collisions_s c = FIO_MAP_INIT;
  size_t count = 0;
  FIO_ARRAY_EACH(&words, w) {
    fio_str_info_s i = fio_str_info(w);
    // fprintf(stderr, "%s\n", i.buf);
    if (i.len > 20)
      printf("\33[2K [%zu] %.*s...\r", ++count, (int)20, i.buf);
    else
      printf("\33[2K [%zu] %s\r", ++count, i.buf);
    collisions_set(&c, h(i.buf, i.len), w, NULL);
    test_for_best();
  }
  printf("\33[2K\r\n");
  fprintf(stderr,
          "* Total collisions detected for %s: %zu\n",
          name,
          (size_t)words_count(&words) - collisions_count(&c));
  fprintf(stderr,
          "* Final set utilization ratio (over 1024) %zu/%zu\n",
          (size_t)collisions_count(&c),
          (size_t)collisions_capa(&c));
  fprintf(
      stderr, "* Best set utilization ratio  %zu/%zu\n", best_count, best_capa);
  collisions_destroy(&c);
}

static void find_bit_collisions(hashing_func_fn fn,
                                size_t collision_count,
                                uint8_t bit_count) {
  words_s c = FIO_ARRAY_INIT;
  const uint64_t mask = (1ULL << bit_count) - 1;
  time_t start = clock();
  words_reserve(&c, collision_count * 2);
  while (words_count(&c) < collision_count) {
    uint64_t rnd = fio_rand64();
    if (!(fn((char *)&rnd, 8) & mask)) {
      fio_str_s tmp;
      fio_str_init_const(&tmp, (const char *)&rnd, sizeof(rnd));
      words_push(&c, tmp);
    }
  }
  time_t end = clock();
  char *name = hash_name(fn);
  if (!name)
    name = "unknown";
  fprintf(stderr,
          "* It took %zu cycles to find %zu (%u bit) collisions for %s (brute "
          "fource):\n",
          end - start,
          (size_t)words_count(&c),
          bit_count,
          name);
  FIO_ARRAY_EACH(&c, pos) {
    uint64_t tmp = fio_buf2u64(fio_str2ptr(pos));
    char ctmp[128];
    memcpy(ctmp, "* ", 2);
    size_t wp = fio_ltoa(ctmp + 2, tmp, 16) + 2;
    memcpy(ctmp + wp, " => ", 4);
    wp += 4;
    wp += fio_ltoa(ctmp + wp, fn(fio_str2ptr(pos), 8), 16);
    memcpy(ctmp + wp, "\n\0", 2);
    fprintf(stderr, "%s", ctmp);
  }
  words_destroy(&c);
}

static void find_full_collisions_rot(hashing_func_fn fn) {
  uint64_t collision_target[] = {1UL, 0, 0, 0, 1, 0, 0, 0};
  uint64_t h1 = fn((char *)collision_target, sizeof(collision_target));
  uint8_t pos[64] = {0};
  for (int i = 1; i < 64; ++i) {
    collision_target[4] = fio_lrot64(collision_target[4], 1);
    collision_target[5] = fio_lrot64(collision_target[5], 1);
    collision_target[6] = fio_lrot64(collision_target[6], 1);
    collision_target[7] = fio_lrot64(collision_target[7], 1);
    uint64_t h2 = fn((char *)collision_target, sizeof(collision_target));
    if (h1 == h2)
      pos[0] = pos[i] = 1;
  }
  char *name = hash_name(fn);
  if (!name)
    name = "unknown";
  if (!pos[0])
    fprintf(stderr,
            "* No collision found for %s when using 512 bit vector with 2nd "
            "half shift.\n",
            name);
  else
    for (int i = 1; i < 64; ++i) {
      if (pos[i])
        fprintf(stderr,
                "* Found full collision for %s using 512 bit vector with 2nd "
                "half shift or %d:\n",
                name,
                i);
    }
}

/* *****************************************************************************
Finding a mod64 inverse
See: https://lemire.me/blog/2017/09/18/computing-the-inverse-of-odd-integers/
***************************************************************************** */

/* will return `inv` if `inv` is inverse of `n` */
FIO_FUNC uint64_t inverse64_test(uint64_t n, uint64_t inv) {
  uint64_t result = inv * (2 - (n * inv));
  return result;
}

FIO_FUNC uint64_t inverse64(uint64_t x) {
  uint64_t y = (3 * x) ^ 2;
  y = inverse64_test(x, y);
  y = inverse64_test(x, y);
  y = inverse64_test(x, y);
  y = inverse64_test(x, y);
  if (FIO_LOG_LEVEL >= FIO_LOG_LEVEL_DEBUG) {
    char buff[64];
    fio_str_s t = FIO_STR_INIT;
    fio_str_write(&t, "\n\t\tinverse for:\t", 16);
    fio_str_write(&t, buff, fio_ltoa(buff, x, 16));
    fio_str_write(&t, "\n\t\tis:\t\t\t", 8);
    fio_str_write(&t, buff, fio_ltoa(buff, y, 16));
    fio_str_write(&t, "\n\t\tsanity inverse test: 1==", 27);
    fio_str_write_i(&t, x * y);
    FIO_LOG_DEBUG("%s", fio_str2ptr(&t));
  }

  return y;
}

/* *****************************************************************************
Hash experimentation workspace
***************************************************************************** */
#define SFUNC FIO_SFUNC
#define IFUNC FIO_IFUNC

/** Converts an unaligned network ordered byte stream to a 64 bit number. */
#define FIO_RISKY_STR2U64(c)                                                   \
  ((uint64_t)((((uint64_t)((uint8_t *)(c))[0]) << 56) |                        \
              (((uint64_t)((uint8_t *)(c))[1]) << 48) |                        \
              (((uint64_t)((uint8_t *)(c))[2]) << 40) |                        \
              (((uint64_t)((uint8_t *)(c))[3]) << 32) |                        \
              (((uint64_t)((uint8_t *)(c))[4]) << 24) |                        \
              (((uint64_t)((uint8_t *)(c))[5]) << 16) |                        \
              (((uint64_t)((uint8_t *)(c))[6]) << 8) | (((uint8_t *)(c))[7])))

/** 64Bit left rotation, inlined. */
#define FIO_RISKY_LROT64(i, bits)                                              \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> (64 - (bits))))

/* Risky Hash primes */
#define RISKY_PRIME_0 0xFBBA3FA15B22113B
#define RISKY_PRIME_1 0xAB137439982B86C9

/* Risky Hash consumption round, accepts a state word s and an input word w */
#define FIO_RISKY_CONSUME(v, w)                                                \
  (v) += (w);                                                                  \
  (v) = FIO_RISKY_LROT64((v), 33);                                             \
  (v) += (w);                                                                  \
  (v) *= RISKY_PRIME_0;

/* Reads and consumes a buffer (uint8_t ptr) */
#define FIO_RISKY_READ_AND_CONSUME(v0, v1, v2, v3, data, len)                  \
  do {                                                                         \
    /* consume 256 bit blocks */                                               \
    for (size_t i = len >> 5; i; --i) {                                        \
      FIO_RISKY_CONSUME(v0, FIO_RISKY_STR2U64(data));                          \
      FIO_RISKY_CONSUME(v1, FIO_RISKY_STR2U64(data + 8));                      \
      FIO_RISKY_CONSUME(v2, FIO_RISKY_STR2U64(data + 16));                     \
      FIO_RISKY_CONSUME(v3, FIO_RISKY_STR2U64(data + 24));                     \
      data += 32;                                                              \
    }                                                                          \
    /* Consume any remaining 64 bit words. */                                  \
    switch (len & 24) {                                                        \
    case 24:                                                                   \
      FIO_RISKY_CONSUME(v2, FIO_RISKY_STR2U64(data + 16));                     \
      /* fallthrough */                                                        \
    case 16:                                                                   \
      FIO_RISKY_CONSUME(v1, FIO_RISKY_STR2U64(data + 8));                      \
      /* fallthrough */                                                        \
    case 8:                                                                    \
      FIO_RISKY_CONSUME(v0, FIO_RISKY_STR2U64(data));                          \
      data += len & 24;                                                        \
    }                                                                          \
    uint64_t tmp = 0;                                                          \
    /* consume leftover bytes, if any */                                       \
    switch ((len & 7)) {                                                       \
    case 7:                                                                    \
      tmp |= ((uint64_t)data[6]) << 8;                                         \
      /* fallthrough */                                                        \
    case 6:                                                                    \
      tmp |= ((uint64_t)data[5]) << 16;                                        \
      /* fallthrough */                                                        \
    case 5:                                                                    \
      tmp |= ((uint64_t)data[4]) << 24;                                        \
      /* fallthrough */                                                        \
    case 4:                                                                    \
      tmp |= ((uint64_t)data[3]) << 32;                                        \
      /* fallthrough */                                                        \
    case 3:                                                                    \
      tmp |= ((uint64_t)data[2]) << 40;                                        \
      /* fallthrough */                                                        \
    case 2:                                                                    \
      tmp |= ((uint64_t)data[1]) << 48;                                        \
      /* fallthrough */                                                        \
    case 1:                                                                    \
      tmp |= ((uint64_t)data[0]) << 56;                                        \
      tmp |= (len & 255);                                                      \
      /* ((len >> 3) & 3) is a 0...3 value indicating consumption vector */    \
      switch ((len >> 3) & 3) {                                                \
      case 3:                                                                  \
        FIO_RISKY_CONSUME(v3, tmp);                                            \
        break;                                                                 \
      case 2:                                                                  \
        FIO_RISKY_CONSUME(v2, tmp);                                            \
        break;                                                                 \
      case 1:                                                                  \
        FIO_RISKY_CONSUME(v1, tmp);                                            \
        break;                                                                 \
      case 0:                                                                  \
        FIO_RISKY_CONSUME(v0, tmp);                                            \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
  } while (0);

#define FIO_RISKY_FINALIZE64(result, v0, v1, v2, v3, len)                      \
  do {                                                                         \
    result = FIO_RISKY_LROT64(v0, 17) + FIO_RISKY_LROT64(v1, 13) +             \
             FIO_RISKY_LROT64(v2, 47) + FIO_RISKY_LROT64(v3, 57);              \
                                                                               \
    len ^= (len << 33);                                                        \
    result += len;                                                             \
                                                                               \
    /* irreversible avalanche... I think */                                    \
    result += v0 * RISKY_PRIME_1;                                              \
    result ^= FIO_RISKY_LROT64(result, 13);                                    \
    result += v1 * RISKY_PRIME_1;                                              \
    result ^= FIO_RISKY_LROT64(result, 29);                                    \
    result += v2 * RISKY_PRIME_1;                                              \
    result ^= FIO_RISKY_LROT64(result, 33);                                    \
    result += v3 * RISKY_PRIME_1;                                              \
    result ^= FIO_RISKY_LROT64(result, 51);                                    \
    result ^= (result >> 29) * RISKY_PRIME_0;                                  \
  } while (0)

/*  Computes a facil.io Risky Hash. */
SFUNC uint64_t fio_risky_hash_v2(const void *data_, size_t len, uint64_t seed) {
  /* reading position */
  const uint8_t *data = (uint8_t *)data_;

  /* The consumption vectors initialized state */
  register uint64_t v0 = seed ^ RISKY_PRIME_1;
  register uint64_t v1 = ~seed + RISKY_PRIME_1;
  register uint64_t v2 =
      FIO_RISKY_LROT64(seed, 17) ^ ((~RISKY_PRIME_1) + RISKY_PRIME_0);
  register uint64_t v3 = FIO_RISKY_LROT64(seed, 33) + (~RISKY_PRIME_1);
  FIO_RISKY_READ_AND_CONSUME(v0, v1, v2, v3, data, len);
  /* merge and mix */
  uint64_t result;
  FIO_RISKY_FINALIZE64(result, v0, v1, v2, v3, len);
  return result;
}

#undef FIO_RISKY_FINALIZE64

#define FIO_RISKY_FINALIZE64(result, v0, v1, v2, v3, len)                      \
  do {                                                                         \
    result = FIO_RISKY_LROT64(v0, 17) + FIO_RISKY_LROT64(v1, 13) +             \
             FIO_RISKY_LROT64(v2, 47) + FIO_RISKY_LROT64(v3, 57);              \
                                                                               \
    len ^= (len << 33);                                                        \
    result += len;                                                             \
                                                                               \
    /* irreversible avalanche... I think */                                    \
    result += (v0 * RISKY_PRIME_1) ^ FIO_RISKY_LROT64(result, 13);             \
    result += (v1 * RISKY_PRIME_1) ^ FIO_RISKY_LROT64(result, 29);             \
    result += (v2 * RISKY_PRIME_1) ^ FIO_RISKY_LROT64(result, 33);             \
    result += (v3 * RISKY_PRIME_1) ^ FIO_RISKY_LROT64(result, 51);             \
    result ^= (result >> 29) * RISKY_PRIME_0;                                  \
  } while (0)

/*  Computes a facil.io Risky Hash. */
SFUNC uint64_t fio_risky_hash_v3(const void *data_, size_t len, uint64_t seed) {
  /* reading position */
  const uint8_t *data = (uint8_t *)data_;

  /* The consumption vectors initialized state */
  register uint64_t v0 = seed ^ RISKY_PRIME_1;
  register uint64_t v1 = ~seed + RISKY_PRIME_1;
  register uint64_t v2 =
      FIO_RISKY_LROT64(seed, 17) ^ ((~RISKY_PRIME_1) + RISKY_PRIME_0);
  register uint64_t v3 = FIO_RISKY_LROT64(seed, 33) + (~RISKY_PRIME_1);
  FIO_RISKY_READ_AND_CONSUME(v0, v1, v2, v3, data, len);
  /* merge and mix */
  uint64_t result;
  FIO_RISKY_FINALIZE64(result, v0, v1, v2, v3, len);
  return result;
}

#undef FIO_RISKY_STR2U64
#undef FIO_RISKY_LROT64
#undef FIO_RISKY_CONSUME
#undef FIO_RISKY_READ_AND_CONSUME
#undef FIO_RISKY_FINALIZE64
#undef FIO_RISKY_PRIME_0
#undef FIO_RISKY_PRIME_1

#if TEST_XXHASH
#include "xxhash.c"
#endif

#if 0
inline static uint64_t folder(const void *data_, size_t len, uint64_t seed) {
  const uint8_t *buf = (const uint8_t *)data_;
#define FIO_RISKY_ROUND0(w0, w1)
#define FIO_RISKY_ROUND1(w0, w1)
  if (seed) {
    FIO_RISKY_ROUND0(seed, seed);
    FIO_RISKY_ROUND1(seed, seed);
  }
  for (size_t i = len >> 5; i; --i) {
    FIO_RISKY_ROUND0(fio_buf2u64(buf + 0), fio_buf2u64(buf + 8));
    FIO_RISKY_ROUND1(fio_buf2u64(buf + 16), fio_buf2u64(buf + 24));
    buf += 32;
  }
  {
    uint64_t w0 = 0, w1 = 0, w2 = 0, w3 = (len & 255);
    switch (len & 31) {
    // clang-format off
    case 31: w3 |= (uint64_t)buf[30] << 8          ; /* fallthrough */
    case 30: w3 |= (uint64_t)buf[29] << 16         ; /* fallthrough */
    case 29: w3 |= (uint64_t)buf[28] << 24         ; /* fallthrough */
    case 28: w3 |= (uint64_t)buf[27] << 32         ; /* fallthrough */
    case 27: w3 |= (uint64_t)buf[26] << 40         ; /* fallthrough */
    case 26: w3 |= (uint64_t)buf[25] << 48         ; /* fallthrough */
    case 25: w3 |= (uint64_t)buf[24] << 56         ; /* fallthrough */
    case 24: w2 |= (uint64_t)buf[23]               ; /* fallthrough */
    case 23: w2 |= (uint64_t)buf[22] << 8          ; /* fallthrough */
    case 22: w2 |= (uint64_t)buf[21] << 16         ; /* fallthrough */
    case 21: w2 |= (uint64_t)buf[20] << 24         ; /* fallthrough */
    case 20: w2 |= (uint64_t)buf[19] << 32         ; /* fallthrough */
    case 19: w2 |= (uint64_t)buf[18] << 40         ; /* fallthrough */
    case 18: w2 |= (uint64_t)buf[17] << 48         ; /* fallthrough */
    case 17: w2 |= (uint64_t)buf[16] << 56         ; /* fallthrough */
    case 16: w1 |= (uint64_t)buf[15]               ; /* fallthrough */
    case 15: w1 |= (uint64_t)buf[14] << 8          ; /* fallthrough */
    case 14: w1 |= (uint64_t)buf[13] << 16         ; /* fallthrough */
    case 13: w1 |= (uint64_t)buf[12] << 24         ; /* fallthrough */
    case 12: w1 |= (uint64_t)buf[11] << 32         ; /* fallthrough */
    case 11: w1 |= (uint64_t)buf[10] << 40         ; /* fallthrough */
    case 10: w1 |= (uint64_t)buf[9] << 48          ; /* fallthrough */
    case 9:  w1 |= (uint64_t)buf[8] << 56          ; /* fallthrough */
    case 8:  w0 |= (uint64_t)buf[7]                ; /* fallthrough */
    case 7:  w0 |= (uint64_t)buf[6] << 8           ; /* fallthrough */
    case 6:  w0 |= (uint64_t)buf[5] << 16          ; /* fallthrough */
    case 5:  w0 |= (uint64_t)buf[4] << 24          ; /* fallthrough */
    case 4:  w0 |= (uint64_t)buf[3] << 32          ; /* fallthrough */
    case 3:  w0 |= (uint64_t)buf[2] << 40          ; /* fallthrough */
    case 2:  w0 |= (uint64_t)buf[1] << 48          ; /* fallthrough */
    case 1:  w0 |= (uint64_t)buf[0] << 56          ; /* fallthrough */
      // clang-format on
      FIO_RISKY_ROUND0(w0, w1);
      FIO_RISKY_ROUND1(w2, w3);
      break;
      // case 0: nothing.
    }
  }
}
#endif
