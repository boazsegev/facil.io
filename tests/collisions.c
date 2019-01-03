/*
Copyright: Boaz Segev, 2018-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define FIO_INCLUDE_STR
#include <fio.h>
#include <fio_cli.h>

/* *****************************************************************************
State machine and types
***************************************************************************** */

static uint8_t print_flag = 1;

static inline int fio_str_eq_print(fio_str_s *a, fio_str_s *b) {
  /* always return 1, to avoid internal set collision mitigation. */
  if (print_flag)
    fprintf(stderr, "* Collision Detected: %s vs. %s\n", fio_str_data(a),
            fio_str_data(b));
  return 1;
}

// static inline void destroy_collision_object(fio_str_s *a) {
//   fprintf(stderr, "* Collision Detected: %s\n", fio_str_data(a));
//   fio_str_free2(a);
// }

#define FIO_SET_NAME collisions
#define FIO_SET_OBJ_TYPE fio_str_s *
#define FIO_SET_OBJ_COPY(dest, src) ((dest) = fio_str_new_copy2((src)))
#define FIO_SET_OBJ_COMPARE(a, b) fio_str_eq_print((a), (b))
#define FIO_SET_OBJ_DESTROY(a) fio_str_free2((a))
#include <fio.h>

typedef uintptr_t (*hashing_func_fn)(char *, size_t);
#define FIO_SET_NAME hash_name
#define FIO_SET_OBJ_TYPE hashing_func_fn
#include <fio.h>

#define FIO_ARY_NAME words
#define FIO_ARY_TYPE fio_str_s
#define FIO_ARY_COMPARE(a, b) fio_str_iseq(&(a), &(b))
#define FIO_ARY_COPY(dest, src)                                                \
  do {                                                                         \
    fio_str_clear(&(dest)), fio_str_concat(&(dest), &(src));                   \
  } while (0)
#define FIO_ARY_DESTROY(a) fio_str_free((&a))
#include <fio.h>

static hash_name_s hash_names = FIO_SET_INIT;
static words_s words = FIO_SET_INIT;

/* *****************************************************************************
Main
***************************************************************************** */

static void test_hash_function(hashing_func_fn h);
static void initialize_cli(int argc, char const *argv[]);
static void load_words(void);
static void initialize_hash_names(void);
static void cleanup(void);

int main(int argc, char const *argv[]) {
  // FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  initialize_cli(argc, argv);
  load_words();
  initialize_hash_names();
  if (fio_cli_get("-t")) {
    fio_str_s tmp = FIO_STR_INIT_STATIC(fio_cli_get("-t"));
    hashing_func_fn h =
        hash_name_find(&hash_names, fio_str_hash_risky(&tmp), NULL);
    if (h)
      test_hash_function(h);
    else
      FIO_LOG_ERROR("Test function %s unknown.", tmp.data);
  } else {
    FIO_SET_FOR_LOOP(&hash_names, pos) { test_hash_function(pos->obj); }
  }
  cleanup();
  return 0;
}

/* *****************************************************************************
CLI
***************************************************************************** */

static void initialize_cli(int argc, char const *argv[]) {
  fio_cli_start(
      argc, argv, 0, 0,
      "This is a Hash algorythm collision test program. It accepts the "
      "following arguments:",
      FIO_CLI_STRING(
          "-test -t test only the specified algorithm. Options include:"),
      FIO_CLI_PRINT("\t\tsiphash13"), FIO_CLI_PRINT("\t\tsiphash24"),
      FIO_CLI_PRINT("\t\tsha1"),
      FIO_CLI_PRINT("\t\trisky (fio_str_hash_risky)"),
      FIO_CLI_PRINT("\t\trisky2 (fio_str_hash_risky alternative)"),
      // FIO_CLI_PRINT("\t\txor (xor all bytes and length)"),
      FIO_CLI_STRING(
          "-dictionary -d a text file containing words separated by an "
          "EOL marker."));
  FIO_LOG_INFO("initialized CLI.");
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
    while (tmp.len && tmp.data[tmp.len - 1] != '/') {
      --tmp.len;
    }
    fio_str_resize(&filename, tmp.len);
    fio_str_write(&filename, "words.txt", 9);
  }
  fio_str_readfile(&data, fio_str_data(&filename), 0, 0);
  fio_str_info_s d = fio_str_info(&data);
  if (d.len == 0) {
    FIO_LOG_FATAL("Couldn't find / read dictionary file (or no words?)");
    FIO_LOG_FATAL("\tmissing or empty: %s", fio_str_data(&filename));
    cleanup();
    fio_str_free(&filename);
    exit(-1);
  }
  while (d.len) {
    char *eol = memchr(d.data, '\n', d.len);
    if (!eol) {
      /* push what's left */
      words_push(&words, FIO_STR_INIT_STATIC2(d.data, d.len));
      break;
    }
    if (eol == d.data || (eol == d.data + 1 && eol[-1] == '\r')) {
      /* empty line */
      ++d.data;
      --d.len;
      continue;
    }
    words_push(&words, FIO_STR_INIT_STATIC2(
                           d.data, (eol - (d.data + (eol[-1] == '\r')))));
    d.len -= (eol + 1) - d.data;
    d.data = eol + 1;
  }
  fio_free(&filename);
  fio_free(&data);
  FIO_LOG_INFO("Loaded %zu words.", words_count(&words));
}

/* *****************************************************************************
Cleanup
***************************************************************************** */

static void cleanup(void) {
  print_flag = 0;
  hash_name_free(&hash_names);
  words_free(&words);
}

/* *****************************************************************************
Hash functions
***************************************************************************** */

static uintptr_t siphash13(char *data, size_t len) {
  return fio_siphash13(data, len);
}

static uintptr_t siphash24(char *data, size_t len) {
  return fio_siphash24(data, len);
}
static uintptr_t sha1(char *data, size_t len) {
  fio_sha1_s s = fio_sha1_init();
  fio_sha1_write(&s, data, len);
  return ((uintptr_t *)fio_sha1_result(&s))[0];
}
static uintptr_t counter(char *data, size_t len) {
  static uintptr_t counter = 0;
  const size_t len_8 = len & (((size_t)-1) << 3);

  for (size_t i = 0; i < len_8; i += 8) {
    uint64_t t = fio_str2u64(data);
    __asm__ volatile("" ::: "memory");
    (void)t;
    data += 8;
  }

  uintptr_t tmp;
  tmp = 0;
  /* assumes sizeof(uintptr_t) <= 8 */
  switch ((len & 7)) {
  case 7: /* overflow */
    ((char *)(&tmp))[6] = data[6];
  case 6: /* overflow */
    ((char *)(&tmp))[5] = data[5];
  case 5: /* overflow */
    ((char *)(&tmp))[4] = data[4];
  case 4: /* overflow */
    ((char *)(&tmp))[3] = data[3];
  case 3: /* overflow */
    ((char *)(&tmp))[2] = data[2];
  case 2: /* overflow */
    ((char *)(&tmp))[1] = data[1];
  case 1: /* overflow */
    ((char *)(&tmp))[0] = data[0];
  }
  __asm__ volatile("" ::: "memory");
  return ++counter;
}

static uintptr_t risky(char *data, size_t len) {
  fio_str_s t = FIO_STR_INIT_STATIC2(data, len);
  return fio_str_hash_risky(&t);
}

/**
Working version.
*/
inline FIO_FUNC uintptr_t risky2(char *data, size_t len) {
  /* primes make sure unique value multiplication produces unique results */
  /* selected from https://asecuritysite.com/encryption/random3?val=64 */
  const uint64_t primes[] = {
      5948729071956823223ULL,
      871375739782306879UL,
      13969002540889903909ULL,
      3533ULL,
  };
  const uint64_t comb = 0xA0A0A0A0A0A0A0A0ULL;
  struct risky_state_s {
    uint64_t mem[4];
    uint64_t v[4];
    uint64_t result;
  } s = {{0}, {primes[0] + primes[1], primes[1], 0, primes[0]}, 0};
/* A single data-mangling round, n is the data in big-endian 64 bit */
/* with enough bits set (n ^ primes[0]), n will avalanch using overflow */
#define risky_round()                                                          \
  do {                                                                         \
    s.v[0] += ((s.mem[0] ^ comb) * primes[0]);                                 \
    s.v[1] += ((s.mem[1] ^ comb) * primes[0]);                                 \
    s.v[2] += ((s.mem[2] ^ comb) * primes[0]);                                 \
    s.v[3] += ((s.mem[3] ^ comb) * primes[0]);                                 \
                                                                               \
    s.v[0] = primes[1] * fio_lrot64(s.v[0], 33);                               \
    s.v[1] = primes[1] * fio_lrot64(s.v[1], 33);                               \
    s.v[2] = primes[1] * fio_lrot64(s.v[2], 33);                               \
    s.v[3] = primes[1] * fio_lrot64(s.v[3], 33);                               \
  } while (0)

  /* loop over 256 bit "blocks" */
  const size_t len_256 = len & (((size_t)-1) << 5);
  for (size_t i = 0; i < len_256; i += 32) {
    s.mem[0] = fio_str2u64(data);
    s.mem[1] = fio_str2u64(data + 8);
    s.mem[2] = fio_str2u64(data + 16);
    s.mem[3] = fio_str2u64(data + 24);
    data += 32;
    /* perform round for block */
    risky_round();
  }

  /* copy last 256 bit block */
  s.mem[0] = 0;
  s.mem[1] = 0;
  s.mem[2] = 0;
  s.mem[3] = 0;
  uint64_t *tmp = s.mem;
  len -= len_256;
  while (len >= 8) {
    *tmp = fio_str2u64(data);
    ++tmp;
    data += 8;
    len -= 8;
  }
  switch (len) {
  case 7: /* overflow */
    ((char *)(tmp))[6] = data[6];
  case 6: /* overflow */
    ((char *)(tmp))[5] = data[5];
  case 5: /* overflow */
    ((char *)(tmp))[4] = data[4];
  case 4: /* overflow */
    ((char *)(tmp))[3] = data[3];
  case 3: /* overflow */
    ((char *)(tmp))[2] = data[2];
  case 2: /* overflow */
    ((char *)(tmp))[1] = data[1];
  case 1: /* overflow */
    ((char *)(tmp))[0] = data[0];
  }

  /* perform round for block */
  risky_round();

  s.result = (fio_lrot64(s.v[0], 61) + fio_lrot64(s.v[1], 59) +
              fio_lrot64(s.v[2], 53) + fio_lrot64(s.v[3], 47)) ^
             (len * primes[3]);

  /* perform a last round with against the result... */
  s.mem[0] = s.result;
  s.mem[1] = s.result;
  s.mem[2] = s.result;
  s.mem[3] = s.result;
  risky_round();
  s.result = ((s.result ^ s.v[0]) * primes[3]) + primes[2];
  s.result = ((s.result ^ s.v[1]) * primes[3]) + primes[2];
  s.result = ((s.result ^ s.v[2]) * primes[3]) + primes[2];
  s.result = ((s.result ^ s.v[3]) * primes[3]) + primes[2];

  s.result ^= (s.result >> 33) * primes[0];
  s.result ^= (s.result >> 29) * primes[1];
  s.result ^= (s.result >> 37) * primes[2];

  /* finalize data, merging all vectors and adding length */
  return s.result;

#undef risky_round
}

/**
This version was okay. There were some collisions when testing with SMHasher,
but not horribly risky... still, far from ideal.
*/
inline FIO_FUNC uintptr_t risky_okay(char *data, size_t len) {
  /* primes make sure unique value multiplication produces unique results */
  /* selected from https://asecuritysite.com/encryption/random3?val=64 */
  const uint64_t primes[] = {
      5948729071956823223ULL,
      871375739782306879UL,
      28859ULL,
  };
  struct risky_state_s {
    uint64_t mem[4];
    uint64_t v[4];
    uint64_t result;
  } s = {{0}, {0xA0A0A0A0A0A0A0A0ULL, 0x0505050505050505ULL}, 0};
/* A single data-mangling round, n is the data in big-endian 64 bit */
/* with enough bits set (n ^ primes[0]), n will avalanch using overflow */
#define risky_round()                                                          \
  do {                                                                         \
    s.v[0] += (s.mem[0] * primes[1]);                                          \
    s.v[1] += (s.mem[1] * primes[1]);                                          \
    s.v[2] += (s.mem[2] * primes[1]);                                          \
    s.v[3] += (s.mem[3] * primes[1]);                                          \
                                                                               \
    s.v[0] = primes[2] * fio_lrot64(s.v[0], 29);                               \
    s.v[1] = primes[2] * fio_lrot64(s.v[1], 29);                               \
    s.v[2] = primes[2] * fio_lrot64(s.v[2], 29);                               \
    s.v[3] = primes[2] * fio_lrot64(s.v[3], 29);                               \
  } while (0)

  /* loop over 256 bit "blocks" */
  const size_t len_256 = len & (((size_t)-1) << 5);
  for (size_t i = 0; i < len_256; i += 32) {
    s.mem[0] = fio_str2u64(data);
    s.mem[1] = fio_str2u64(data + 8);
    s.mem[2] = fio_str2u64(data + 16);
    s.mem[3] = fio_str2u64(data + 24);
    data += 32;
    /* perform round for block */
    risky_round();
  }

  /* copy last 256 bit block */
  s.mem[0] = 0;
  s.mem[1] = 0;
  s.mem[2] = 0;
  s.mem[3] = 0;
  uint64_t *tmp = s.mem;
  len -= len_256;
  while (len >= 8) {
    *tmp = fio_str2u64(data);
    ++tmp;
    data += 8;
    len -= 8;
  }
  switch (len) {
  case 7: /* overflow */
    ((char *)(tmp))[6] = data[6];
  case 6: /* overflow */
    ((char *)(tmp))[5] = data[5];
  case 5: /* overflow */
    ((char *)(tmp))[4] = data[4];
  case 4: /* overflow */
    ((char *)(tmp))[3] = data[3];
  case 3: /* overflow */
    ((char *)(tmp))[2] = data[2];
  case 2: /* overflow */
    ((char *)(tmp))[1] = data[1];
  case 1: /* overflow */
    ((char *)(tmp))[0] = data[0];
  }

  /* perform round for block */
  risky_round();

  s.result = fio_lrot64(s.v[0], 1) + fio_lrot64(s.v[1], 7) +
             fio_lrot64(s.v[2], 11) + fio_lrot64(s.v[3], 13) + len;

  s.result ^= (s.result >> 33) * primes[0];
  s.result ^= (s.result >> 29) * primes[1];
  s.result ^= (s.result >> 37) * primes[2];

  /* finalize data, merging all vectors and adding length */
  return s.result;

#undef risky_round
}
/* *****************************************************************************
Hash setup and testing...
***************************************************************************** */

struct hash_fn_names_s {
  char *name;
  hashing_func_fn fn;
} hash_fn_list[] = {
    {"counter (no hash, optimal uniqueness, but ordered)", counter},
    {"siphash13", siphash13},
    {"siphash24", siphash24},
    {"sha1", sha1},
    {"risky", risky},
    {"risky2", risky2},
    {"risky_ok", risky_okay},
    // {"xor", xorhash},
    {NULL, NULL},
};

static void initialize_hash_names(void) {
  for (size_t i = 0; hash_fn_list[i].name; ++i) {
    hash_name_insert(&hash_names,
                     risky(hash_fn_list[i].name, strlen(hash_fn_list[i].name)),
                     hash_fn_list[i].fn);
    FIO_LOG_DEBUG("Registered %s hashing function", hash_fn_list[i].name);
  }
}

static void test_hash_function_speed(hashing_func_fn h, char *name) {
  FIO_LOG_INFO("Speed testing for %s", name);
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
      fprintf(stderr, "%-20s %8.2f MB/s\n", name,
              (double)(sizeof(buffer) * cycles) /
                  (((end - start) * (1000000.0 / CLOCKS_PER_SEC))));
      break;
    }
    cycles <<= 2;
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
  /* Speed test */
  test_hash_function_speed(h, name);
  /* Collision test */
  collisions_s c = FIO_SET_INIT;
  size_t count = 0;
  FIO_ARY_FOR(&words, w) {
    fio_str_info_s i = fio_str_info(w);
    // fprintf(stderr, "%s\n", i.data);
    printf("\33[2K [%zu] %s\r", ++count, i.data);
    collisions_overwrite(&c, h(i.data, i.len), w, NULL);
    test_for_best();
  }
  printf("\33[2K\r\n");
  fprintf(stderr, "* Total collisions detected for %s: %zu\n", name,
          words_count(&words) - collisions_count(&c));
  fprintf(stderr, "* Final set utilization ratio (over 1024) %zu/%zu\n",
          collisions_count(&c), collisions_capa(&c));
  fprintf(stderr, "* Best set utilization ratio  %zu/%zu\n", best_count,
          best_capa);
  collisions_free(&c);
}
