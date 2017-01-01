/*
Copyright: Boaz segev, 2016-2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "random.h"

#if defined(USE_ALT_RANDOM) || !defined(HAS_UNIX_FEATURES)
#include "sha2.h"
#include <time.h>
uint32_t bscrypt_rand32(void) {
  bits256_u pseudo = bscrypt_rand256();
  return pseudo.ints[3];
}

uint64_t bscrypt_rand64(void) {
  bits256_u pseudo = bscrypt_rand256();
  return pseudo.words[3];
}

bits128_u bscrypt_rand128(void) {
  bits256_u pseudo = bscrypt_rand256();
  bits128_u ret;
  ret.words[0] = pseudo.words[0];
  ret.words[1] = pseudo.words[1];
  return ret;
}

bits256_u bscrypt_rand256(void) {
  clock_t cpu_state = clock();
  time_t the_time;
  time(&the_time);
  bits256_u pseudo;
  sha2_s sha2 = bscrypt_sha2_init(SHA_256);
  bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
  bscrypt_sha2_write(&sha2, &the_time, sizeof(the_time));
  bscrypt_sha2_write(&sha2, &cpu_state - 2, 64); /* whatever's on the stack */
  bscrypt_sha2_result(&sha2);
  pseudo.words[0] = sha2.digest.i64[0];
  pseudo.words[1] = sha2.digest.i64[1];
  pseudo.words[2] = sha2.digest.i64[2];
  pseudo.words[3] = sha2.digest.i64[3];
  return pseudo;
}

void bscrypt_rand_bytes(void *target, size_t length) {
  clock_t cpu_state = clock();
  time_t the_time;
  time(&the_time);
  sha2_s sha2 = bscrypt_sha2_init(SHA_512);
  bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
  bscrypt_sha2_write(&sha2, &the_time, sizeof(the_time));
  bscrypt_sha2_write(&sha2, &cpu_state - 2, 64); /* whatever's on the stack */
  bscrypt_sha2_result(&sha2);
  while (length > 64) {
    memcpy(target, sha2.digest.str, 64);
    length -= 64;
    target += 64;
    bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
    bscrypt_sha2_result(&sha2);
  }
  if (length > 32) {
    memcpy(target, sha2.digest.str, 32);
    length -= 32;
    target += 32;
    bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
    bscrypt_sha2_result(&sha2);
  }
  if (length > 16) {
    memcpy(target, sha2.digest.str, 16);
    length -= 16;
    target += 16;
    bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
    bscrypt_sha2_result(&sha2);
  }
  if (length > 8) {
    memcpy(target, sha2.digest.str, 8);
    length -= 8;
    target += 8;
    bscrypt_sha2_write(&sha2, &cpu_state, sizeof(cpu_state));
    bscrypt_sha2_result(&sha2);
  }
  while (length) {
    *((uint8_t *)target) = sha2.digest.str[length];
    ++target;
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
Random ... (why is this not a system call?)
*/

/* rand generator management */
static int _rand_fd_ = -1;
static void close_rand_fd(void) {
  if (_rand_fd_ >= 0)
    close(_rand_fd_);
  _rand_fd_ = -1;
}
static void init_rand_fd(void) {
  if (_rand_fd_ < 0) {
    while ((_rand_fd_ = open("/dev/urandom", O_RDONLY)) == -1) {
      if (errno == ENXIO)
        perror("bscrypt fatal error, caanot initiate random generator"),
            exit(-1);
      sched_yield();
    }
  }
  atexit(close_rand_fd);
}
/* rand function template */
#define MAKE_RAND_FUNC(type, func_name)                                        \
  type func_name(void) {                                                       \
    if (_rand_fd_ < 0)                                                         \
      init_rand_fd();                                                          \
    type ret;                                                                  \
    while (read(_rand_fd_, &ret, sizeof(type)) < 0)                            \
      sched_yield();                                                           \
    return ret;                                                                \
  }
/* rand functions */
MAKE_RAND_FUNC(uint32_t, bscrypt_rand32);
MAKE_RAND_FUNC(uint64_t, bscrypt_rand64);
MAKE_RAND_FUNC(bits128_u, bscrypt_rand128);
MAKE_RAND_FUNC(bits256_u, bscrypt_rand256);
/* clear template */
#undef MAKE_RAND_FUNC

void bscrypt_rand_bytes(void *target, size_t length) {
  if (_rand_fd_ < 0)
    init_rand_fd();
  while (read(_rand_fd_, target, length) < 0)
    sched_yield();
}
#endif /* Unix Random */

/*******************************************************************************
Random
*/
#if defined(DEBUG) && DEBUG == 1
void bscrypt_test_random(void) {
  clock_t start, end;
  bscrypt_rand256();
  start = clock();
  for (size_t i = 0; i < 100000; i++) {
    bscrypt_rand256();
  }
  end = clock();
  fprintf(stderr,
          "+ bscrypt Random generator available\n+ bscrypt 100K X 256bit "
          "Random %lu CPU clock count\n",
          end - start);
}
#endif
