/*
(un)copyright: Boaz segev, 2016
license: MIT except for any non-public-domain algorithms, which are subject to
their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "random.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

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
#define bswap64(i) \
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
#define MAKE_RAND_FUNC(type, func_name)             \
  type func_name(void) {                            \
    if (_rand_fd_ < 0)                              \
      init_rand_fd();                               \
    type ret;                                       \
    while (read(_rand_fd_, &ret, sizeof(type)) < 0) \
      sched_yield();                                \
    return ret;                                     \
  }
/* rand functions */
MAKE_RAND_FUNC(uint32_t, bscrypt_rand32);
MAKE_RAND_FUNC(uint64_t, bscrypt_rand64);
MAKE_RAND_FUNC(bits128_u, bscrypt_rand128);
MAKE_RAND_FUNC(bits256_u, bscrypt_rand256);
/* clear template */
#undef MAKE_RAND_FUNC

void bscrypt_rand_bytes(void* target, size_t length) {
  if (_rand_fd_ < 0)
    init_rand_fd();
  while (read(_rand_fd_, target, length) < 0)
    sched_yield();
}

/*******************************************************************************
Random
*/
#if defined(BSCRYPT_TEST) && BSCRYPT_TEST == 1
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
