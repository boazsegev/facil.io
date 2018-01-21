/*
Copyright: Boaz segev, 2016-2018
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fio_random.h"

#include <errno.h>
#include <stdio.h>

#ifndef __has_include
#define __has_include(x) 0
#endif

/* include intrinsics if supported */
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define HAVE_X86Intrin
/*
see: https://software.intel.com/en-us/node/513411
and: https://software.intel.com/sites/landingpage/IntrinsicsGuide/
*/
#endif /* __has_include(<x86intrin.h>) */

/* check for unix support */
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) ||           \
    (__has_include(<unistd.h>) && __has_include(<pthread.h>))
#ifndef HAS_UNIX_FEATURES
#define HAS_UNIX_FEATURES
#endif
#endif

// clang-format off
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#   if defined(__has_include)
#     if __has_include(<endian.h>)
#      include <endian.h>
#     elif __has_include(<sys/endian.h>)
#      include <sys/endian.h>
#     endif
#   endif
#   if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
                __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __BIG_ENDIAN__
#   endif
#endif
// clang-format on

#if defined(USE_ALT_RANDOM) || !defined(HAS_UNIX_FEATURES)
#include "fio_sha2.h"
#include <time.h>
#include <string.h>

static inline void fio_random_data(sha2_s *sha2) {
#ifdef RUSAGE_SELF
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  fio_sha2_write(sha2, &rusage, sizeof(rusage));
#elif defined CLOCKS_PER_SEC
  size_t clk = (size_t)clock();
  fio_sha2_write(&sha2, &clk, sizeof(clk));
  time_t the_time;
  time(&the_time);
  fio_sha2_write(sha2, &the_time, sizeof(the_time));
  fio_sha2_write(sha2, &fio_rand64, sizeof(void *));
  fio_sha2_write(sha2, &fio_rand_bytes, sizeof(void *));
  {
    char junk_data[64];
    fio_sha2_write(sha2, junk_data, 64);
  }
#else
#error Random alternative failed to find access to the CPU clock state.
#endif
  fio_sha2_write(sha2, sha2, sizeof(void *));
}

uint32_t fio_rand32(void) {
  sha2_s sha2 = fio_sha2_init(SHA_512);
  fio_random_data(&sha2);
  fio_sha2_result(&sha2);
  return *((uint32_t *)sha2.buffer);
}

uint64_t fio_rand64(void) {
  sha2_s sha2 = fio_sha2_init(SHA_512);
  fio_random_data(&sha2);
  fio_sha2_result(&sha2);
  return *((uint64_t *)sha2.buffer);
}

void fio_rand_bytes(void *target, size_t length) {
  sha2_s sha2 = fio_sha2_init(SHA_512);
  fio_random_data(&sha2);
  fio_sha2_result(&sha2);

  while (length >= 64) {
    memcpy(target, sha2.digest.str, 64);
    length -= 64;
    target = (void *)((uintptr_t)target + 64);
    fio_random_data(&sha2);
    fio_sha2_result(&sha2);
  }
  if (length >= 32) {
    memcpy(target, sha2.digest.str, 32);
    length -= 32;
    target = (void *)((uintptr_t)target + 32);
    fio_random_data(&sha2);
    fio_sha2_result(&sha2);
  }
  if (length >= 16) {
    memcpy(target, sha2.digest.str, 16);
    length -= 16;
    target = (void *)((uintptr_t)target + 16);
    fio_random_data(&sha2);
    fio_sha2_result(&sha2);
  }
  if (length >= 8) {
    memcpy(target, sha2.digest.str, 8);
    length -= 8;
    target = (void *)((uintptr_t)target + 8);
    fio_random_data(&sha2);
    fio_sha2_result(&sha2);
  }
  while (length) {
    *((uint8_t *)target) = sha2.digest.str[length];
    target = (void *)((uintptr_t)target + 1);
    --length;
  }
}

#else
/* ***************************************************************************
Unix Random Engine (use built in machine)
*/
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

/* ***************************************************************************
Machine specific changes
*/
// #ifdef __linux__
// #undef bswap16
// #undef bswap32
// #undef bswap64
// #include <machine/bswap.h>
// #endif
#ifdef HAVE_X86Intrin
// #undef bswap16
/*
#undef bswap32
#define bswap32(i) \
  { __asm__("bswap %k0" : "+r"(i) :); }
*/
#undef bswap64
#define bswap64(i)                                                             \
  { __asm__("bswapq %0" : "+r"(i) :); }

// shadow sched_yield as _mm_pause for spinwait
#define sched_yield() _mm_pause()
#endif

/* ***************************************************************************
Random fd
***************************************************************************** */

/* rand generator management */
static int fio_rand_fd_ = -1;
static void close_rand_fd(void) {
  if (fio_rand_fd_ >= 0)
    close(fio_rand_fd_);
  fio_rand_fd_ = -1;
}
static void init_rand_fd(void) {
  if (fio_rand_fd_ < 0) {
    while ((fio_rand_fd_ = open("/dev/urandom", O_RDONLY)) == -1) {
      if (errno == ENXIO) {
        perror("FATAL ERROR: caanot initiate random generator");
        exit(-1);
      }
      sched_yield();
    }
  }
  atexit(close_rand_fd);
}

/* ***************************************************************************
Random API ... (why is this not a system call?)
***************************************************************************** */

/* rand function template */
#define MAKE_RAND_FUNC(type, func_name)                                        \
  type func_name(void) {                                                       \
    if (fio_rand_fd_ < 0)                                                      \
      init_rand_fd();                                                          \
    type ret;                                                                  \
    while (read(fio_rand_fd_, &ret, sizeof(type)) < 0)                         \
      sched_yield();                                                           \
    return ret;                                                                \
  }
/* rand functions */
MAKE_RAND_FUNC(uint32_t, fio_rand32)
MAKE_RAND_FUNC(uint64_t, fio_rand64)
/* clear template */
#undef MAKE_RAND_FUNC

void fio_rand_bytes(void *target, size_t length) {
  if (fio_rand_fd_ < 0)
    init_rand_fd();
  while (read(fio_rand_fd_, target, length) < 0)
    sched_yield();
}
#endif /* Unix Random */

/*******************************************************************************
Random Testing
***************************************************************************** */
#if DEBUG
void fio_random_test(void) {
  uint64_t buffer[8];
  clock_t start, end;
  fio_rand64();
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    buffer[i & 7] = fio_rand64();
  }
  end = clock();
  fprintf(stderr,
          "+ Random generator available\n+ created 100K X 64bits "
          "Random %lu CPU clock count\n",
          end - start);
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    fio_rand_bytes(buffer, 64);
  }
  end = clock();
  fprintf(stderr,
          "+ created 100K X 512bits "
          "Random %lu CPU clock count\n",
          end - start);
}
#endif
