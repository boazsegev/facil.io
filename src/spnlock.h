/* *****************************************************************************
A Simple busy lock implementation ... (spnlock.h)

Based on a lot of internet reading as well as comperative work (i.e the Linux
karnel's code and the more readable Apple's kernel code)

Written by Boaz Segev at 2016. Donated to the public domain for all to enjoy.
*/
#ifndef _SPN_LOCK_H
#define _SPN_LOCK_H

/* allow of the unused flag */
#ifndef __unused
#define __unused __attribute__((unused))
#endif

#include <stdlib.h>
#include <stdint.h>

/*********
 * manage the way threads "wait" for the lock to release
 */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
/* nanosleep seems to be the most effective and efficient reschedule */
#include <time.h>
#define reschedule_thread()                                                    \
  {                                                                            \
    static const struct timespec tm = {.tv_nsec = 1};                          \
    nanosleep(&tm, NULL);                                                      \
  }

#else /* no effective rescheduling, just spin... */
#define reschedule_thread()

/* these are SUPER slow when comapred with nanosleep or CPU cycling */
// #if defined(__SSE2__) || defined(__SSE2)
// #define reschedule_thread() __asm__("pause" :::)
//
// #elif defined(__has_include) && __has_include(<pthread.h>)
// #include "pthread.h"
// #define reschedule_thread() sched_yield()
// #endif

#endif
/* end `reschedule_thread` block*/

/*********
 * The spin lock core functions (spn_trylock, spn_unlock, is_spn_locked)
 */

/* prefer C11 standard implementation where available (trust the system) */
#if defined(__has_include)
#if __has_include(<stdatomic.h>)
#define SPN_TMP_HAS_ATOMICS 1
#include <stdatomic.h>
typedef atomic_bool spn_lock_i;
#define SPN_LOCK_INIT ATOMIC_VAR_INIT(0)
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  return atomic_exchange(lock, 1);
}
/** Releases a lock. */
__unused static inline void spn_unlock(spn_lock_i *lock) {
  atomic_store(lock, 0);
  __asm__ volatile("" ::: "memory");
}
/** returns a lock's state (non 0 == Busy). */
__unused static inline int spn_is_locked(spn_lock_i *lock) {
  return atomic_load(lock);
}
#endif
#endif

/* Chack if stdatomic was available */
#ifdef SPN_TMP_HAS_ATOMICS
#undef SPN_TMP_HAS_ATOMICS

#else
/* Test for compiler builtins */

/* use clang builtins if available - trust the compiler */
#if defined(__clang__)
#if defined(__has_builtin) && __has_builtin(__sync_swap)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i *lock) {
  return __sync_swap(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif
/* use gcc builtins if available - trust the compiler */
#elif defined(__GNUC__) &&                                                     \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i *lock) {
  return __sync_fetch_and_or(lock, 1);
}
#define SPN_TMP_HAS_BUILTIN 1
#endif

/* Check if compiler builtins were available, if not, try assembly*/
#if SPN_TMP_HAS_BUILTIN
#undef SPN_TMP_HAS_BUILTIN

/* use Intel's asm if on Intel - trust Intel's documentation */
#elif defined(__amd64__) || defined(__x86_64__) || defined(__x86__) ||         \
    defined(__i386__) || defined(__ia64__) || defined(_M_IA64) ||              \
    defined(__itanium__) || defined(__i386__)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns 1 if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i *lock) {
  spn_lock_i tmp;
  __asm__ volatile("xchgb %0,%1" : "=r"(tmp), "=m"(*lock) : "0"(1) : "memory");
  return tmp;
}

/* use SPARC's asm if on SPARC - trust the design */
#elif defined(__sparc__) || defined(__sparc)
/* define the type */
typedef volatile uint8_t spn_lock_i;
/** returns TRUE (non-zero) if the lock was busy (TRUE == FAIL). */
__unused static inline int spn_trylock(spn_lock_i *lock) {
  spn_lock_i tmp;
  __asm__ volatile("ldstub    [%1], %0" : "=r"(tmp) : "r"(lock) : "memory");
  return tmp; /* return 0xFF if the lock was busy, 0 if free */
}

#else
/* I don't know how to provide green thread safety on PowerPC or ARM */
#error "Couldn't implement a spinlock for this system / compiler"
#endif /* types and atomic exchange */
/** Initialization value in `free` state. */
#define SPN_LOCK_INIT 0

/** Releases a lock. */
__unused static inline void spn_unlock(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  *lock = 0;
}
/** returns a lock's state (non 0 == Busy). */
__unused static inline int spn_is_locked(spn_lock_i *lock) {
  __asm__ volatile("" ::: "memory");
  return *lock;
}

#endif /* has atomics */
#include <stdio.h>
/** Busy waits for the lock. */
__unused static inline void spn_lock(spn_lock_i *lock) {
  while (spn_trylock(lock)) {
    reschedule_thread();
  }
}

/* *****************************************************************************
spnlock.h finished
*/
#endif

#if DEBUG == 1 && !defined(_SPN_LOCK_TEST_REPEAT_COUNT)

#define _SPN_LOCK_TEST_REPEAT_COUNT 10000UL
#define _SPN_LOCK_TEST_THREAD_COUNT 10000UL
#include <pthread.h>
#include <stdio.h>

__unused static void *test_spn_lock_work(void *arg) {
  static spn_lock_i lck = SPN_LOCK_INIT;
  uint64_t *ip = arg;
  for (size_t i = 0; i < _SPN_LOCK_TEST_REPEAT_COUNT; i++) {
    spn_lock(&lck);
    uint64_t j = *ip;
    j++;
    __asm__ volatile("" ::: "memory", "cc");
    *ip = j;
    spn_unlock(&lck);
  }
  return NULL;
}

__unused static void *test_spn_lock_lockless_work(void *arg) {
  uint64_t *ip = arg;
  for (size_t i = 0; i < _SPN_LOCK_TEST_REPEAT_COUNT; i++) {
    uint64_t j = *ip;
    j++;
    __asm__ volatile("" ::: "memory", "cc");
    *ip = j;
  }
  return NULL;
}

__unused static void spn_lock_test(void) {
  time_t start, end;
  unsigned long num = 0;
  pthread_t *threads = malloc(_SPN_LOCK_TEST_THREAD_COUNT * sizeof(*threads));
  void *tmp;
  start = clock();
  for (size_t i = 0; i < _SPN_LOCK_TEST_THREAD_COUNT; i++) {
    pthread_create(threads + i, NULL, test_spn_lock_lockless_work, &num);
  }
  for (size_t i = 0; i < _SPN_LOCK_TEST_THREAD_COUNT; i++) {
    pthread_join(threads[i], &tmp);
  }
  end = clock();
  fprintf(stderr, "Lockless Num = %lu with %lu CPU cycles.\n", num,
          end - start);

  num = 0;

  start = clock();
  for (size_t i = 0; i < _SPN_LOCK_TEST_THREAD_COUNT; i++) {
    if (pthread_create(threads + i, NULL, test_spn_lock_work, &num))
      fprintf(stderr,
              "Failed to create thread number %lu... test will fail to run as "
              "expected.\n",
              i);
    ;
  }
  for (size_t i = 0; i < _SPN_LOCK_TEST_THREAD_COUNT; i++) {
    pthread_join(threads[i], &tmp);
  }
  end = clock();
  free(threads);
  fprintf(stderr, "Locked Num = %lu with %lu CPU cycles.\n", num, end - start);
  fprintf(stderr, "spn_lock test %s\n",
          num == _SPN_LOCK_TEST_THREAD_COUNT * _SPN_LOCK_TEST_REPEAT_COUNT
              ? "passed."
              : "FAILED!");
}
#endif /* Test */
