/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/cstl
***************************************************************************** */

/** ****************************************************************************
# facil.io's C STL - Simple (type) Template Library

This file contains macros that create generic / common core types, such as:

* Linked Lists - defined by `FIO_LIST_NAME`

* Dynamic Arrays - defined by `FIO_ARRAY_NAME`

* Hash Maps / Sets - defined by `FIO_MAP_NAME`

* Binary Safe Dynamic Strings - defined by `FIO_STR_NAME` or `FIO_STR_SMALL`

* Reference counting / Type wrapper - defined by `FIO_REF_NAME` (adds atomic)

* Pointer Tagging for Types - defined by `FIO_PTR_TAG(p)`/`FIO_PTR_UNTAG(p)`

* Soft / Dynamic Types (FIOBJ) - defined by `FIO_FIOBJ`


This file also contains common helper macros / primitives, such as:

* Macro Stringifier - `FIO_MACRO2STR(macro)`

* Version Macros - i.e., `FIO_VERSION_MAJOR` / `FIO_VERSION_STRING`

* Pointer Math - i.e., `FIO_PTR_MATH_ADD` / `FIO_PTR_FROM_FIELD`

* Memory Allocation Macros - i.e., `FIO_MEM_REALLOC`

* Security Related macros - i.e., `FIO_MEM_STACK_WIPE`

* String Information Helper Type - `fio_str_info_s` / `FIO_STR_INFO_IS_EQ`

* Naming Macros - i.e., `FIO_NAME` / `FIO_NAME2` / `FIO_NAME_BL`

* OS portable Threads - defined by `FIO_THREADS`

* OS portable file helpers - defined by `FIO_FILES`

* Sleep / Thread Scheduling Macros - i.e., `FIO_THREAD_RESCHEDULE`

* Logging and Assertion (no heap allocation) - defined by `FIO_LOG`

* Atomic add/subtract/replace - defined by `FIO_ATOMIC`

* Bit-Byte Operations - defined by `FIO_BITWISE` and `FIO_BITMAP` (adds atomic)

* Data Hashing (using Risky Hash) - defined by `FIO_RISKY_HASH`

* Psedo Random Generation - defined by `FIO_RAND`

* String / Number conversion - defined by `FIO_ATOL`

* Time Helpers - defined by `FIO_TIME`

* Task / Timer Queues (Event Loop Engine) - defined by `FIO_QUEUE`

* Command Line Interface helpers - defined by `FIO_CLI`

* Socket Helpers - defined by `FIO_SOCK`

* Polling Helpers - defined by `FIO_POLL`

* Data Stream Containers - defined by `FIO_STREAM`

* Signal (pass-through) Monitors - defined by `FIO_SIGNAL`

* Custom Memory Pool / Allocation - defined by `FIO_MEMORY_NAME` / `FIO_MALLOC`,
  if `FIO_MALLOC` is used, it updates `FIO_MEM_REALLOC` etc'

* Custom JSON Parser - defined by `FIO_JSON`

However, this file does very little unless specifically requested.

To make sure this file defines a specific macro or type, it's macro should be
set.

In addition, if the `FIO_TEST_CSTL` macro is defined, the self-testing function
`fio_test_dynamic_types()` will be defined. the `fio_test_dynamic_types`
function will test the functionality of this file and, as consequence, will
define all available macros.

**Notes**:

- To make this file usable for kernel authoring, the `include` statements should
be reviewed.

- To make these functions safe for kernel authoring, the `FIO_MEM_REALLOC` and
`FIO_MEM_FREE` macros should be (re)-defined.

  These macros default to using the `realloc` and `free` functions calls. If
  `FIO_MALLOC` was defined, these macros will default to the custom memory
  allocator.

- To make the custom memory allocator safe for kernel authoring, the
  `FIO_MEM_PAGE_ALLOC`, `FIO_MEM_PAGE_REALLOC` and `FIO_MEM_PAGE_FREE` macros
  should be redefined. These macros default to using `mmap` and `munmap` (on
  linux, also `mremap`).

- The functions defined using this file default to `static` or `static
  inline`.

  To create an externally visible API, define the `FIO_EXTERN`. Define the
  `FIO_EXTERN_COMPLETE` macro to include the API's implementation as well.

- To implement a library style version guard, define the `FIO_VERSION_GUARD`
macro in a single translation unit (.c file) **before** including this STL
library for the first time.

***************************************************************************** */

/* *****************************************************************************
C++ extern start
***************************************************************************** */
/* support C++ */
#ifdef __cplusplus
extern "C" {
/* C++ keyword was deprecated */
#ifndef register
#define register
#endif
/* C keyword - unavailable in C++ */
#ifndef restrict
#define restrict
#endif
#endif

/* *****************************************************************************




                            Constants (included once)




***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H
#define H___FIO_CSTL_INCLUDE_ONCE_H

/* *****************************************************************************
Compiler detection, GCC / CLang features and OS dependent included files
***************************************************************************** */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if !defined(__GNUC__) && !defined(__clang__) && !defined(GNUC_BYPASS)
#define __attribute__(...)
#define __has_include(...) 0
#define __has_builtin(...) 0
#define GNUC_BYPASS        1
#elif !defined(__clang__) && !defined(__has_builtin)
/* E.g: GCC < 6.0 doesn't support __has_builtin */
#define __has_builtin(...) 0
#define GNUC_BYPASS        1
#endif

#ifndef __has_include
#define __has_include(...) 0
#define GNUC_BYPASS 1
#endif

#if defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5))
/* GCC < 4.5 doesn't support deprecation reason string */
#define DEPRECATED(reason) __attribute__((deprecated))
#else
#define DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FIO_ALIGN(bytes) __attribute__((aligned(bytes)))
#elif defined(__INTEL_COMPILER) || defined(_MSC_VER)
#define FIO_ALIGN(bytes) __declspec(align(bytes))
#else
#define FIO_ALIGN(bytes)
#endif

#if _MSC_VER
#define __thread __declspec(thread)
#elif !defined(__clang__) && !defined(__GNUC__)
#define __thread _Thread_value
#endif

#if defined(__clang__) || defined(__GNUC__)
/** Clobber CPU registers and prevent compiler reordering optimizations. */
#define FIO_COMPILER_GUARD __asm__ volatile("" ::: "memory")
#elif defined(_MSC_VER)
#include <intrin.h>
/** Clobber CPU registers and prevent compiler reordering optimizations. */
#define FIO_COMPILER_GUARD _ReadWriteBarrier()
#pragma message("Warning: Windows deprecated it's low-level C memory barrier.")
#else
#warning Unknown OS / compiler, some macros are poorly defined and errors might occur.
#define FIO_COMPILER_GUARD asm volatile("" ::: "memory")
#endif

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#define FIO_HAVE_UNIX_TOOLS 1
#define FIO_OS_POSIX        1
#define FIO___PRINTF_STYLE  printf
#elif defined(_WIN32) || defined(_WIN64) || defined(WIN32) ||                  \
    defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#define FIO_OS_WIN     1
#define POSIX_C_SOURCE 200809L
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#if defined(__MINGW32__)
/* Mingw supports */
#define FIO_HAVE_UNIX_TOOLS    2
#define __USE_MINGW_ANSI_STDIO 1
#define FIO___PRINTF_STYLE     __MINGW_PRINTF_FORMAT
#elif defined(__CYGWIN__)
/* TODO: cygwin support */
#define FIO_HAVE_UNIX_TOOLS    3
#define __USE_MINGW_ANSI_STDIO 1
#define FIO___PRINTF_STYLE     __MINGW_PRINTF_FORMAT
#else
#define FIO_HAVE_UNIX_TOOLS 0
typedef SSIZE_T ssize_t;
#endif /* __CYGWIN__ __MINGW32__ */
#else
#define FIO_HAVE_UNIX_TOOLS 0
#warning Unknown OS / compiler, some macros are poorly defined and errors might occur.
#endif

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

#if FIO_HAVE_UNIX_TOOLS
#include <sys/param.h>
#include <unistd.h>
#endif

#if FIO_UNALIGNED_ACCESS &&                                                    \
    (__amd64 || __amd64__ || __x86_64 || __x86_64__ || __i386 ||               \
     __aarch64__ || _M_IX86 || _M_X64 || _M_ARM64)
#define FIO_UNALIGNED_MEMORY_ACCESS_ENABLED 1
#else
#define FIO_UNALIGNED_MEMORY_ACCESS_ENABLED 0
#endif

/* memcpy selectors / overriding */
#ifndef FIO_MEMCPY
#if __has_builtin(__builtin_memcpy)
#define FIO_MEMCPY __builtin_memcpy
#else
#define FIO_MEMCPY memcpy
#endif
#endif

/* *****************************************************************************
Function Attributes
***************************************************************************** */

/** Marks a function as `static`, `inline` and possibly unused. */
#define FIO_IFUNC static inline __attribute__((unused))

/** Marks a function as `static` and possibly unused. */
#define FIO_SFUNC static __attribute__((unused))

/** Marks a function as weak */
#define FIO_WEAK __attribute__((weak))

#if _MSC_VER
#pragma section(".CRT$XCU", read)
#undef FIO_CONSTRUCTOR
#undef FIO_DESTRUCTOR
/** Marks a function as a constructor - if supported. */

#if _WIN64 /* MSVC linker uses different name mangling on 32bit systems */
#define FIO___CONSTRUCTOR_INTERNAL(fname)                                      \
  static void fname(void);                                                     \
  __pragma(comment(linker, "/include:" #fname "__")); /* and next.... */       \
  __declspec(allocate(".CRT$XCU")) void (*fname##__)(void) = fname;            \
  static void fname(void)
#else
#define FIO___CONSTRUCTOR_INTERNAL(fname)                                      \
  static void fname(void);                                                     \
  __declspec(allocate(".CRT$XCU")) void (*fname##__)(void) = fname;            \
  __pragma(comment(linker, "/include:_" #fname "__")); /* and next.... */      \
  static void fname(void)
#endif
#define FIO_CONSTRUCTOR(fname) FIO___CONSTRUCTOR_INTERNAL(fname)

#define FIO_DESTRUCTOR_INTERNAL(fname)                                         \
  static void fname(void);                                                     \
  FIO_CONSTRUCTOR(fname##__hook) { atexit(fname); }                            \
  static void fname(void)
#define FIO_DESTRUCTOR(fname) FIO_DESTRUCTOR_INTERNAL(fname)

#else
/** Marks a function as a constructor - if supported. */
#define FIO_CONSTRUCTOR(fname)                                                 \
  FIO_SFUNC __attribute__((constructor)) void fname FIO_NOOP(void)

/** Marks a function as a destructor - if supported. Consider using atexit() */
#define FIO_DESTRUCTOR(fname)                                                  \
  FIO_SFUNC                                                                    \
  __attribute__((destructor)) void fname FIO_NOOP(void)
#endif

/* *****************************************************************************
Macro Stringifier
***************************************************************************** */

#ifndef FIO_MACRO2STR
#define FIO_MACRO2STR_STEP2(macro) #macro
/** Converts a macro's content to a string literal. */
#define FIO_MACRO2STR(macro) FIO_MACRO2STR_STEP2(macro)
#endif

/* *****************************************************************************
Conditional Likelihood
***************************************************************************** */

#if defined(__clang__) || defined(__GNUC__)
#define FIO_LIKELY(cond)   __builtin_expect((cond), 1)
#define FIO_UNLIKELY(cond) __builtin_expect((cond), 0)
#else
#define FIO_LIKELY(cond)   (cond)
#define FIO_UNLIKELY(cond) (cond)
#endif
/* *****************************************************************************
Naming Macros
***************************************************************************** */

/* Used for naming functions and types */
#define FIO_NAME_FROM_MACRO_STEP2(prefix, postfix, div) prefix##div##postfix
#define FIO_NAME_FROM_MACRO_STEP1(prefix, postfix, div)                        \
  FIO_NAME_FROM_MACRO_STEP2(prefix, postfix, div)

/** Used for naming functions and variables resulting in: prefix_postfix */
#define FIO_NAME(prefix, postfix) FIO_NAME_FROM_MACRO_STEP1(prefix, postfix, _)

/** Sets naming convention for conversion functions, i.e.: foo2bar */
#define FIO_NAME2(prefix, postfix) FIO_NAME_FROM_MACRO_STEP1(prefix, postfix, 2)

/** Sets naming convention for boolean testing functions, i.e.: foo_is_true */
#define FIO_NAME_BL(prefix, postfix)                                           \
  FIO_NAME_FROM_MACRO_STEP1(prefix, postfix, _is_)

/** Used internally to name test functions. */
#define FIO_NAME_TEST(prefix, postfix)                                         \
  FIO_NAME(fio___test, FIO_NAME(prefix, postfix))

/* *****************************************************************************
Version Macros

The facil.io C STL library follows [semantic versioning](https://semver.org) and
supports macros that will help detect and validate it's version.
***************************************************************************** */

/** MAJOR version: API/ABI breaking changes. */
#define FIO_VERSION_MAJOR 0
/** MINOR version: Deprecation, or significant features added. May break ABI. */
#define FIO_VERSION_MINOR 8
/** PATCH version: Bug fixes, minor features may be added. */
#define FIO_VERSION_PATCH 0
/** BETA version: pre-version development marker. Nothing is stable. */
#define FIO_VERSION_BETA 1

#if FIO_VERSION_BETA
/** Version as a String literal (MACRO). */
#define FIO_VERSION_STRING                                                     \
  FIO_MACRO2STR(FIO_VERSION_MAJOR)                                             \
  "." FIO_MACRO2STR(FIO_VERSION_MINOR) "." FIO_MACRO2STR(                      \
      FIO_VERSION_PATCH) ".beta" FIO_MACRO2STR(FIO_VERSION_BETA)
#else
/** Version as a String literal (MACRO). */
#define FIO_VERSION_STRING                                                     \
  FIO_MACRO2STR(FIO_VERSION_MAJOR)                                             \
  "." FIO_MACRO2STR(FIO_VERSION_MINOR) "." FIO_MACRO2STR(FIO_VERSION_PATCH)
#endif

/** If implemented, returns the major version number. */
size_t fio_version_major(void);
/** If implemented, returns the minor version number. */
size_t fio_version_minor(void);
/** If implemented, returns the patch version number. */
size_t fio_version_patch(void);
/** If implemented, returns the beta version number. */
size_t fio_version_beta(void);
/** If implemented, returns the version number as a string. */
char *fio_version_string(void);

#define FIO_VERSION_VALIDATE()                                                 \
  FIO_ASSERT(fio_version_major() == FIO_VERSION_MAJOR &&                       \
                 fio_version_minor() == FIO_VERSION_MINOR &&                   \
                 fio_version_patch() == FIO_VERSION_PATCH &&                   \
                 fio_version_beta() == FIO_VERSION_BETA,                       \
             "facil.io version mismatch, not %s",                              \
             fio_version_string())

/**
 * To implement the fio_version_* functions and FIO_VERSION_VALIDATE guard, the
 * `FIO_VERSION_GUARD` must be defined (only) once per application / library.
 */
#ifdef FIO_VERSION_GUARD
size_t __attribute__((weak)) fio_version_major(void) {
  return FIO_VERSION_MAJOR;
}
size_t __attribute__((weak)) fio_version_minor(void) {
  return FIO_VERSION_MINOR;
}
size_t __attribute__((weak)) fio_version_patch(void) {
  return FIO_VERSION_PATCH;
}
size_t __attribute__((weak)) fio_version_beta(void) { return FIO_VERSION_BETA; }
char *__attribute__((weak)) fio_version_string(void) {
  return FIO_VERSION_STRING;
}
#undef FIO_VERSION_GUARD
#endif /* FIO_VERSION_GUARD */

#if !defined(FIO_NO_COOKIE)
/** If implemented, does stuff. */
void __attribute__((weak)) fio___(void) {
  volatile uint8_t tmp[] =
      "\xA8\x94\x9A\x10\x99\x92\x93\x96\x9C\x1D\x96\x9F\x10\x9C\x96\x91\xB1\x92"
      "\xB1\xB6\x10\xBB\x92\xB3\x10\x92\xBA\xB8\x94\x9F\xB1\x9A\x98\x10\x91\xB6"
      "\x10\x81\x9F\x92\xB5\x10\xA3\x9A\x9B\x9A\xB9\x1D\x05\x10\x10\x10\x10\x8C"
      "\x96\xB9\x9A\x10\x9C\x9F\x9D\x9B\x10\x92\x9D\x98\x10\xB0\xB1\x9F\xB3\xB0"
      "\x9A\xB1\x1D";
  for (size_t i = 0; tmp[i]; ++i) {
    tmp[i] = ((tmp[i] & 0x55) << 1) | ((tmp[i] & 0xaa) >> 1);
  }
  fprintf(stderr, "%s\n", tmp);
}
#endif

/* *****************************************************************************
Pointer Math
***************************************************************************** */

/** Masks a pointer's left-most bits, returning the right bits. */
#define FIO_PTR_MATH_LMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & (((uintptr_t)1 << (bits)) - 1)))

/** Masks a pointer's right-most bits, returning the left bits. */
#define FIO_PTR_MATH_RMASK(T_type, ptr, bits)                                  \
  ((T_type *)((uintptr_t)(ptr) & ((~(uintptr_t)0) << (bits))))

/** Add offset bytes to pointer, updating the pointer's type. */
#define FIO_PTR_MATH_ADD(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) + (uintptr_t)(offset)))

/** Subtract X bytes from pointer, updating the pointer's type. */
#define FIO_PTR_MATH_SUB(T_type, ptr, offset)                                  \
  ((T_type *)((uintptr_t)(ptr) - (uintptr_t)(offset)))

/** Find the root object (of a struct) from it's field. */
#define FIO_PTR_FROM_FIELD(T_type, field, ptr)                                 \
  FIO_PTR_MATH_SUB(T_type, ptr, (&((T_type *)0)->field))

/* *****************************************************************************
Security Related macros
***************************************************************************** */
#define FIO_MEM_STACK_WIPE(pages)                                              \
  do {                                                                         \
    volatile char stack_mem[(pages) << 12] = {0};                              \
    (void)stack_mem;                                                           \
  } while (0)

/* *****************************************************************************
String Information Helper Type
***************************************************************************** */

/** An information type for reporting the string's state. */
typedef struct fio_str_info_s {
  /** The string's buffer (pointer to first byte) or NULL on error. */
  char *buf;
  /** The string's length, if any. */
  size_t len;
  /** The buffer's capacity. Zero (0) indicates the buffer is read-only. */
  size_t capa;
} fio_str_info_s;

/** An information type for reporting/storing buffer data. */
typedef struct fio_buf_info_s {
  /** The string's buffer (pointer to first byte) or NULL on error. */
  char *buf;
  /** The string's length, if any. */
  size_t len;
} fio_buf_info_s;

/** Compares two `fio_str_info_s` objects for content equality. */
#define FIO_STR_INFO_IS_EQ(s1, s2)                                             \
  ((s1).len == (s2).len && (!(s1).len || (s1).buf == (s2).buf ||               \
                            !memcmp((s1).buf, (s2).buf, (s1).len)))

/* *****************************************************************************
Linked Lists Persistent Macros and Types
***************************************************************************** */

/** A linked list arch-type */
typedef struct fio_list_node_s {
  struct fio_list_node_s *next;
  struct fio_list_node_s *prev;
} fio_list_node_s;

/** A linked list node type */
#define FIO_LIST_NODE fio_list_node_s
/** A linked list head type */
#define FIO_LIST_HEAD fio_list_node_s

/** Allows initialization of FIO_LIST_HEAD objects. */
#define FIO_LIST_INIT(obj)                                                     \
  (fio_list_node_s) { .next = &(obj), .prev = &(obj) }

#ifndef FIO_LIST_EACH
/** Loops through every node in the linked list except the head. */
#define FIO_LIST_EACH(type, node_name, head, pos)                              \
  for (type *pos = FIO_PTR_FROM_FIELD(type, node_name, (head)->next),          \
            *next____p_ls_##pos =                                              \
                FIO_PTR_FROM_FIELD(type, node_name, (head)->next->next);       \
       pos != FIO_PTR_FROM_FIELD(type, node_name, (head));                     \
       (pos = next____p_ls_##pos),                                             \
            (next____p_ls_##pos =                                              \
                 FIO_PTR_FROM_FIELD(type,                                      \
                                    node_name,                                 \
                                    next____p_ls_##pos->node_name.next)))
#endif

/** UNSAFE macro for pushing a node to a list. */
#define FIO_LIST_PUSH(head, n)                                                 \
  do {                                                                         \
    (n)->prev = (head)->prev;                                                  \
    (n)->next = (head);                                                        \
    (head)->prev->next = (n);                                                  \
    (head)->prev = (n);                                                        \
  } while (0)

/** UNSAFE macro for removing a node from a list. */
#define FIO_LIST_REMOVE(n)                                                     \
  do {                                                                         \
    (n)->prev->next = (n)->next;                                               \
    (n)->next->prev = (n)->prev;                                               \
    (n)->next = (n)->prev = (n);                                               \
  } while (0)

/** UNSAFE macro for testing if a list is empty. */
#define FIO_LIST_IS_EMPTY(head) (!(head) || (head)->next == (head)->prev)

/* *****************************************************************************
Indexed Linked Lists Persistent Macros and Types

Indexed Linked Lists can be used to create a linked list that uses is always
relative to some root pointer (usually the root of an array). This:

1. Allows easy reallocation of the list without requiring pointer updates.

2. Could be used for memory optimization if the array limits are known.

The "head" index is usually validated by reserving the value of `-1` to indicate
an empty list.
***************************************************************************** */
#ifndef FIO_INDEXED_LIST_EACH

/** A 32 bit indexed linked list node type */
typedef struct fio_index32_node_s {
  uint32_t next;
  uint32_t prev;
} fio_index32_node_s;

/** A 16 bit indexed linked list node type */
typedef struct fio_index16_node_s {
  uint16_t next;
  uint16_t prev;
} fio_index16_node_s;

/** An 8 bit indexed linked list node type */
typedef struct fio_index8_node_s {
  uint8_t next;
  uint8_t prev;
} fio_index8_node_s;

/** A 32 bit indexed linked list node type */
#define FIO_INDEXED_LIST32_NODE fio_index32_node_s
#define FIO_INDEXED_LIST32_HEAD uint32_t
/** A 16 bit indexed linked list node type */
#define FIO_INDEXED_LIST16_NODE fio_index16_node_s
#define FIO_INDEXED_LIST16_HEAD uint16_t
/** An 8 bit indexed linked list node type */
#define FIO_INDEXED_LIST8_NODE fio_index8_node_s
#define FIO_INDEXED_LIST8_HEAD uint8_t

/** UNSAFE macro for pushing a node to a list. */
#define FIO_INDEXED_LIST_PUSH(root, node_name, head, i)                        \
  do {                                                                         \
    register const size_t n__ = (i);                                           \
    (root)[n__].node_name.prev = (root)[(head)].node_name.prev;                \
    (root)[n__].node_name.next = (head);                                       \
    (root)[(root)[(head)].node_name.prev].node_name.next = n__;                \
    (root)[(head)].node_name.prev = n__;                                       \
  } while (0)

/** UNSAFE macro for removing a node from a list. */
#define FIO_INDEXED_LIST_REMOVE(root, node_name, i)                            \
  do {                                                                         \
    register const size_t n__ = (i);                                           \
    (root)[(root)[n__].node_name.prev].node_name.next =                        \
        (root)[n__].node_name.next;                                            \
    (root)[(root)[n__].node_name.next].node_name.prev =                        \
        (root)[n__].node_name.prev;                                            \
    (root)[n__].node_name.next = (root)[n__].node_name.prev = n__;             \
  } while (0)

/** Loops through every index in the indexed list, assuming `head` is valid. */
#define FIO_INDEXED_LIST_EACH(root, node_name, head, pos)                      \
  for (size_t pos = (head), stopper___ils___ = 0; !stopper___ils___;           \
       stopper___ils___ = ((pos = (root)[pos].node_name.next) == (head)))
#endif

/* *****************************************************************************
Sleep / Thread Scheduling Macros
***************************************************************************** */

#ifndef FIO_THREAD_WAIT
#if FIO_OS_WIN
/**
 * Calls NtDelayExecution with the requested nano-second count.
 */
#define FIO_THREAD_WAIT(nano_sec)                                              \
  do {                                                                         \
    Sleep(((nano_sec) / 1000000) ? ((nano_sec) / 1000000) : 1);                \
  } while (0)
// https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep

#elif FIO_OS_POSIX
/**
 * Calls nonsleep with the requested nano-second count.
 */
#define FIO_THREAD_WAIT(nano_sec)                                              \
  do {                                                                         \
    const struct timespec tm = {.tv_sec = (time_t)((nano_sec) / 1000000000),   \
                                .tv_nsec = ((long)(nano_sec) % 1000000000)};   \
    nanosleep(&tm, (struct timespec *)NULL);                                   \
  } while (0)

#endif
#endif

#ifndef FIO_THREAD_RESCHEDULE
/**
 * Reschedules the thread by calling nanosleeps for a sinlge nano-second.
 *
 * In practice, the thread will probably sleep for 60ns or more.
 */
#define FIO_THREAD_RESCHEDULE() FIO_THREAD_WAIT(4)
#endif

/* *****************************************************************************
Miscellaneous helper macros
***************************************************************************** */

/* avoid printing a full / nested path when __FILE_NAME__ is available */
#ifdef __FILE_NAME__
#define FIO__FILE__ __FILE_NAME__
#else
#define FIO__FILE__ __FILE__
#endif

/** An empty macro, adding white space. Used to avoid function like macros. */
#define FIO_NOOP
/* allow logging to quitely fail unless enabled */
#define FIO_LOG_DEBUG(...)
#define FIO_LOG_DEBUG2(...)
#define FIO_LOG_INFO(...)
#define FIO_LOG_WARNING(...)
#define FIO_LOG_ERROR(...)
#define FIO_LOG_SECURITY(...)
#define FIO_LOG_FATAL(...)
#define FIO_LOG2STDERR(...)
#define FIO_LOG2STDERR2(...)
#define FIO_LOG_PRINT__(...)

#ifndef FIO_LOG_LENGTH_LIMIT
/** Defines a point at which logging truncates (limited by stack memory) */
#define FIO_LOG_LENGTH_LIMIT 1024
#endif

// clang-format off
/* Asserts a condition is true, or kills the application using SIGINT. */
#define FIO_ASSERT(cond, ...)                                                  \
  if (!(cond)) {                                                               \
    FIO_LOG_FATAL("(" FIO__FILE__ ":" FIO_MACRO2STR(__LINE__) ") " __VA_ARGS__);  \
    fprintf(stderr, "     errno(%d): %s\n", errno, strerror(errno));                                                      \
    kill(0, SIGINT);                                                           \
    exit(-1);                                                                  \
  }

#ifndef FIO_ASSERT_ALLOC
/** Tests for an allocation failure. The behavior can be overridden. */
#define FIO_ASSERT_ALLOC(ptr)  FIO_ASSERT((ptr), "memory allocation failed.")
#endif
// clang-format on

#ifdef DEBUG
/** If `DEBUG` is defined, raises SIGINT if assertion fails, otherwise NOOP. */
#define FIO_ASSERT_DEBUG(cond, ...)                                            \
  if (!(cond)) {                                                               \
    FIO_LOG_FATAL("(" FIO__FILE__                                              \
                  ":" FIO_MACRO2STR(__LINE__) ") " __VA_ARGS__);               \
    fprintf(stderr, "     errno(%d): %s\n", errno, strerror(errno));           \
    kill(0, SIGINT);                                                           \
    exit(-1);                                                                  \
  }
#else
#define FIO_ASSERT_DEBUG(...)
#endif

/* *****************************************************************************
End persistent segment (end include-once guard)
***************************************************************************** */
#endif /* H___FIO_CSTL_INCLUDE_ONCE_H */

/* *****************************************************************************




                          Common internal Macros




***************************************************************************** */

/* *****************************************************************************
Memory allocation macros
***************************************************************************** */

#ifndef FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT
/* secure by default */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT 1
#endif

#if defined(FIO_MEM_REST) || !defined(FIO_MEM_REALLOC) || !defined(FIO_MEM_FREE)

#undef FIO_MEM_REALLOC
#undef FIO_MEM_FREE
#undef FIO_MEM_REALLOC_IS_SAFE
#undef FIO_MEM_REST

/* if a global allocator was previously defined route macros to fio_malloc */
#if defined(H___FIO_MALLOC___H)
/** Reallocates memory, copying (at least) `copy_len` if necessary. */
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len)                     \
  fio_realloc2((ptr), (new_size), (copy_len))
/** Frees allocated memory. */
#define FIO_MEM_FREE(ptr, size) fio_free((ptr))
/** Set to true of internall allocator is used (memory returned set to zero). */
#define FIO_MEM_REALLOC_IS_SAFE 1

#else
/** Reallocates memory, copying (at least) `copy_len` if necessary. */
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len)                     \
  realloc((ptr), (new_size))
/** Frees allocated memory. */
#define FIO_MEM_FREE(ptr, size) free((ptr))
/** Set to true of internall allocator is used (memory returned set to zero). */
#define FIO_MEM_REALLOC_IS_SAFE 0
#endif /* H___FIO_MALLOC___H */

#endif /* defined(FIO_MEM_REALLOC) */

/* *****************************************************************************
Locking selector
***************************************************************************** */

#ifndef FIO_USE_THREAD_MUTEX
#define FIO_USE_THREAD_MUTEX 0
#endif

#ifndef FIO_USE_THREAD_MUTEX_TMP
#define FIO_USE_THREAD_MUTEX_TMP FIO_USE_THREAD_MUTEX
#endif

#if FIO_USE_THREAD_MUTEX_TMP
#define FIO_THREAD
#define FIO___LOCK_NAME          "OS mutex"
#define FIO___LOCK_TYPE          fio_thread_mutex_t
#define FIO___LOCK_INIT          ((FIO___LOCK_TYPE)FIO_THREAD_MUTEX_INIT)
#define FIO___LOCK_DESTROY(lock) fio_thread_mutex_destroy(&(lock))
#define FIO___LOCK_LOCK(lock)                                                  \
  do {                                                                         \
    if (fio_thread_mutex_lock(&(lock)))                                        \
      FIO_LOG_ERROR("Couldn't lock mutex @ %s:%d - error (%d): %s",            \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    errno,                                                     \
                    strerror(errno));                                          \
  } while (0)
#define FIO___LOCK_TRYLOCK(lock) fio_thread_mutex_trylock(&(lock))
#define FIO___LOCK_UNLOCK(lock)                                                \
  do {                                                                         \
    if (fio_thread_mutex_unlock(&(lock))) {                                    \
      FIO_LOG_ERROR("Couldn't release mutex @ %s:%d - error (%d): %s",         \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    errno,                                                     \
                    strerror(errno));                                          \
    }                                                                          \
  } while (0)

#else
#define FIO___LOCK_NAME          "facil.io spinlocks"
#define FIO___LOCK_TYPE          fio_lock_i
#define FIO___LOCK_INIT          (FIO_LOCK_INIT)
#define FIO___LOCK_DESTROY(lock) ((lock) = FIO___LOCK_INIT)
#define FIO___LOCK_LOCK(lock)    fio_lock(&(lock))
#define FIO___LOCK_TRYLOCK(lock) fio_trylock(&(lock))
#define FIO___LOCK_UNLOCK(lock)  fio_unlock(&(lock))
#endif

/* *****************************************************************************
Common macros
***************************************************************************** */
#ifndef SFUNC_ /* if we aren't in a recursive #include statement */

#ifdef FIO_EXTERN
#define SFUNC_
#define IFUNC_

#else /* !FIO_EXTERN */
#undef SFUNC
#undef IFUNC
#define SFUNC_ static __attribute__((unused))
#define IFUNC_ static inline __attribute__((unused))
#ifndef FIO_EXTERN_COMPLETE /* force implementation, emitting static data */
#define FIO_EXTERN_COMPLETE 2
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_EXTERN */

#undef SFUNC
#undef IFUNC
#define SFUNC SFUNC_
#define IFUNC IFUNC_

#ifndef FIO_PTR_TAG
/**
 * Supports embedded pointer tagging / untagging for the included types.
 *
 * Should resolve to a tagged pointer value. i.e.: ((uintptr_t)(p) | 1)
 */
#define FIO_PTR_TAG(p) (p)
#endif

#ifndef FIO_PTR_UNTAG
/**
 * Supports embedded pointer tagging / untagging for the included types.
 *
 * Should resolve to an untagged pointer value. i.e.: ((uintptr_t)(p) | ~1UL)
 */
#define FIO_PTR_UNTAG(p) (p)
#endif

/**
 * If FIO_PTR_TAG_TYPE is defined, then functions returning a type's pointer
 * will return a pointer of the specified type instead.
 */
#ifndef FIO_PTR_TAG_TYPE
#endif

/**
 * If FIO_PTR_TAG_VALIDATE is defined, tagging will be verified before executing
 * any code.
 */
#ifdef FIO_PTR_TAG_VALIDATE
#define FIO_PTR_TAG_VALID_OR_RETURN(tagged_ptr, value)                         \
  do {                                                                         \
    if (!(FIO_PTR_TAG_VALIDATE(tagged_ptr))) {                                 \
      FIO_LOG_DEBUG("pointer tag (type) mismatch in function call.");          \
      return (value);                                                          \
    }                                                                          \
  } while (0)
#define FIO_PTR_TAG_VALID_OR_RETURN_VOID(tagged_ptr)                           \
  do {                                                                         \
    if (!(FIO_PTR_TAG_VALIDATE(tagged_ptr))) {                                 \
      FIO_LOG_DEBUG("pointer tag (type) mismatch in function call.");          \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define FIO_PTR_TAG_VALID_OR_GOTO(tagged_ptr, lable)                           \
  do {                                                                         \
    if (!(FIO_PTR_TAG_VALIDATE(tagged_ptr))) {                                 \
      /* Log error since GOTO indicates cleanup or other side-effects. */      \
      FIO_LOG_ERROR("(" FIO__FILE__ ":" FIO_MACRO2STR(                         \
          __LINE__) ") pointer tag (type) mismatch in function call.");        \
      goto lable;                                                              \
    }                                                                          \
  } while (0)
#else
#define FIO_PTR_TAG_VALIDATE(tagged_ptr) 1
#define FIO_PTR_TAG_VALID_OR_RETURN(tagged_ptr, value)
#define FIO_PTR_TAG_VALID_OR_RETURN_VOID(tagged_ptr)
#define FIO_PTR_TAG_VALID_OR_GOTO(tagged_ptr, lable)                           \
  while (0) {                                                                  \
    goto lable;                                                                \
  }
#endif

#else /* SFUNC_ - internal helper types are `static` */
#undef SFUNC
#undef IFUNC
#define SFUNC FIO_SFUNC
#define IFUNC FIO_IFUNC
#endif /* SFUNC_ vs FIO_STL_KEEP__*/

/* *****************************************************************************



                          Internal Dependencies



***************************************************************************** */

/* Modules that require logging */
#if defined(FIO_MEMORY_NAME) || defined(FIO_MALLOC)
#ifndef FIO_LOG
#define FIO_LOG
#endif
#endif /* FIO_MALLOC */

/* Modules that require FIO_SOCK */
#if defined(FIO_POLL)
#define FIO_SOCK
#endif

/* Modules that require FIO_URL */
#if defined(FIO_SOCK)
#define FIO_URL
#endif

/* Modules that require Threads data */
#if (defined(FIO_QUEUE) && defined(FIO_TEST_CSTL)) ||                          \
    defined(FIO_MEMORY_NAME) || defined(FIO_MALLOC) ||                         \
    defined(FIO_USE_THREAD_MUTEX_TMP)
#define FIO_THREADS
#endif

/* Modules that require File Utils */
#if defined(FIO_STR_NAME)
#define FIO_FILES
#endif

/* Modules that require FIO_TIME */
#if defined(FIO_QUEUE) || defined(FIO_RAND)
#ifndef FIO_TIME
#define FIO_TIME
#endif
#endif /* FIO_QUEUE */

/* Modules that require randomness */
#if defined(FIO_MEMORY_NAME) || defined(FIO_MALLOC) || defined(FIO_FILES)
#ifndef FIO_RAND
#define FIO_RAND
#endif
#endif /* FIO_MALLOC */

/* Modules that require FIO_RISKY_HASH */
#if defined(FIO_RAND) || defined(FIO_STR_NAME) || defined(FIO_STR_SMALL) ||    \
    defined(FIO_CLI) || defined(FIO_MEMORY_NAME) || defined(FIO_MALLOC)
#ifndef FIO_RISKY_HASH
#define FIO_RISKY_HASH
#endif
#endif /* FIO_RISKY_HASH */

/* Modules that require FIO_BITMAP */
#if defined(FIO_JSON)
#ifndef FIO_BITMAP
#define FIO_BITMAP
#endif
#endif /* FIO_BITMAP */

/* Modules that require FIO_BITWISE (includes FIO_RISKY_HASH requirements) */
#if defined(FIO_RISKY_HASH) || defined(FIO_JSON) || defined(FIO_MAP_NAME) ||   \
    defined(FIO_UMAP_NAME) || defined(FIO_SHA1)
#ifndef FIO_BITWISE
#define FIO_BITWISE
#endif
#endif /* FIO_BITWISE */

/* Modules that require FIO_ATOMIC */
#if defined(FIO_BITMAP) || defined(FIO_REF_NAME) || defined(FIO_LOCK2) ||      \
    (defined(FIO_POLL) && !FIO_USE_THREAD_MUTEX_TMP) ||                        \
    (defined(FIO_MEMORY_NAME) || defined(FIO_MALLOC)) ||                       \
    (defined(FIO_QUEUE) && !FIO_USE_THREAD_MUTEX_TMP) || defined(FIO_JSON) ||  \
    defined(FIO_SIGNAL) || defined(FIO_BITMAP) || defined(FIO_THREADS)
#ifndef FIO_ATOMIC
#define FIO_ATOMIC
#endif
#endif /* FIO_ATOMIC */

/* Modules that require FIO_ATOL */
#if defined(FIO_STR_NAME) || defined(FIO_STR_SMALL) || defined(FIO_QUEUE) ||   \
    defined(FIO_TIME) || defined(FIO_CLI) || defined(FIO_JSON) ||              \
    defined(FIO_FILES)
#ifndef FIO_ATOL
#define FIO_ATOL
#endif

#endif /* FIO_ATOL */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_PATCHES_H
#define H___FIO_CSTL_PATCHES_H

/* *****************************************************************************


Patch for OSX version < 10.12 from https://stackoverflow.com/a/9781275/4025095


***************************************************************************** */
#if (defined(__MACH__) && !defined(CLOCK_REALTIME))
#warning fio_time functions defined using gettimeofday patch.
#include <sys/time.h>
#define CLOCK_REALTIME 0
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif
#define clock_gettime fio_clock_gettime
// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_MONOTONIC will have already been defined.
FIO_IFUNC int fio_clock_gettime(int clk_id, struct timespec *t) {
  struct timeval now;
  int rv = gettimeofday(&now, NULL);
  if (rv)
    return rv;
  t->tv_sec = now.tv_sec;
  t->tv_nsec = now.tv_usec * 1000;
  return 0;
  (void)clk_id;
}

#endif
/* *****************************************************************************




Patches for Windows




***************************************************************************** */
#if FIO_OS_WIN
#if _MSC_VER
#pragma message("warning: some functionality is enabled by patchwork.")
#else
#warning some functionality is enabled by patchwork.
#endif
#include <fcntl.h>
#include <io.h>
#include <processthreadsapi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysinfoapi.h>
#include <time.h>
#include <winsock2.h> /* struct timeval is here... why? Microsoft. */

/* *****************************************************************************
Windows initialization
***************************************************************************** */

/* Enable console colors */
FIO_CONSTRUCTOR(fio___windows_startup_housekeeping) {
  HANDLE c = GetStdHandle(STD_OUTPUT_HANDLE);
  if (c) {
    DWORD mode = 0;
    if (GetConsoleMode(c, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(c, mode);
    }
  }
  c = GetStdHandle(STD_ERROR_HANDLE);
  if (c) {
    DWORD mode = 0;
    if (GetConsoleMode(c, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(c, mode);
    }
  }
}

/* *****************************************************************************
Inlined patched and MACRO statements
***************************************************************************** */

FIO_IFUNC struct tm *gmtime_r(const time_t *timep, struct tm *result) {
  struct tm *t = gmtime(timep);
  if (t && result)
    *result = *t;
  return result;
}

#define strcasecmp    _stricmp
#define stat          _stat64
#define fstat         _fstat64
#define open          _open
#define close         _close
#define write         _write
#define read          _read
#define O_APPEND      _O_APPEND
#define O_BINARY      _O_BINARY
#define O_CREAT       _O_CREAT
#define O_CREAT       _O_CREAT
#define O_SHORT_LIVED _O_SHORT_LIVED
#define O_CREAT       _O_CREAT
#define O_TEMPORARY   _O_TEMPORARY
#define O_CREAT       _O_CREAT
#define O_EXCL        _O_EXCL
#define O_NOINHERIT   _O_NOINHERIT
#define O_RANDOM      _O_RANDOM
#define O_RDONLY      _O_RDONLY
#define O_RDWR        _O_RDWR
#define O_SEQUENTIAL  _O_SEQUENTIAL
#define O_TEXT        _O_TEXT
#define O_TRUNC       _O_TRUNC
#define O_WRONLY      _O_WRONLY
#define O_U16TEXT     _O_U16TEXT
#define O_U8TEXT      _O_U8TEXT
#define O_WTEXT       _O_WTEXT
#if defined(CLOCK_REALTIME) && defined(CLOCK_MONOTONIC) &&                     \
    CLOCK_REALTIME == CLOCK_MONOTONIC
#undef CLOCK_MONOTONIC
#undef CLOCK_REALTIME
#endif

#ifndef CLOCK_REALTIME
#ifdef CLOCK_MONOTONIC
#define CLOCK_REALTIME (CLOCK_MONOTONIC + 1)
#else
#define CLOCK_REALTIME 0
#endif
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/** patch for clock_gettime */
SFUNC int fio_clock_gettime(const uint32_t clk_type, struct timespec *tv);
/** patch for pread */
SFUNC ssize_t fio_pread(int fd, void *buf, size_t count, off_t offset);
/** patch for pwrite */
SFUNC ssize_t fio_pwrite(int fd, const void *buf, size_t count, off_t offset);
SFUNC int fio_kill(int pid, int signum);

#define kill   fio_kill
#define pread  fio_pread
#define pwrite fio_pwrite

#if !FIO_HAVE_UNIX_TOOLS
/* patch clock_gettime */
#define clock_gettime fio_clock_gettime
#define pipe(fds)     _pipe(fds, 65536, _O_BINARY)
#endif

/* *****************************************************************************
Patched functions
***************************************************************************** */
#if FIO_EXTERN_COMPLETE

/* based on:
 * https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
 */
/** patch for clock_gettime */
SFUNC int fio_clock_gettime(const uint32_t clk_type, struct timespec *tv) {
  if (!tv)
    return -1;
  static union {
    uint64_t u;
    LARGE_INTEGER li;
  } freq = {.u = 0};
  static double tick2n = 0;
  union {
    uint64_t u;
    FILETIME ft;
    LARGE_INTEGER li;
  } tu;

  switch (clk_type) {
  case CLOCK_REALTIME:
  realtime_clock:
    GetSystemTimePreciseAsFileTime(&tu.ft);
    tv->tv_sec = tu.u / 10000000;
    tv->tv_nsec = tu.u - (tv->tv_sec * 10000000);
    return 0;

#ifdef CLOCK_PROCESS_CPUTIME_ID
  case CLOCK_PROCESS_CPUTIME_ID:
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
  case CLOCK_THREAD_CPUTIME_ID:
#endif
  case CLOCK_MONOTONIC:
    if (!QueryPerformanceCounter(&tu.li))
      goto realtime_clock;
    if (!freq.u)
      QueryPerformanceFrequency(&freq.li);
    if (!freq.u) {
      tick2n = 0;
      freq.u = 1;
    } else {
      tick2n = (double)1000000000 / freq.u;
    }
    tv->tv_sec = tu.u / freq.u;
    tv->tv_nsec =
        (uint64_t)(0ULL + ((double)(tu.u - (tv->tv_sec * freq.u)) * tick2n));
    return 0;
  }
  return -1;
}

/** patch for pread */
SFUNC ssize_t fio_pread(int fd, void *buf, size_t count, off_t offset) {
  /* Credit to Jan Biedermann (GitHub: @janbiedermann) */
  ssize_t bytes_read = 0;
  HANDLE handle = (HANDLE)_get_osfhandle(fd);
  if (handle == INVALID_HANDLE_VALUE)
    goto bad_file;
  OVERLAPPED overlapped = {0};
  if (offset > 0)
    overlapped.Offset = offset;
  if (ReadFile(handle, buf, count, (u_long *)&bytes_read, &overlapped))
    return bytes_read;
  if (GetLastError() == ERROR_HANDLE_EOF)
    return bytes_read;
  errno = EIO;
  return -1;
bad_file:
  errno = EBADF;
  return -1;
}

/** patch for pwrite */
SFUNC ssize_t fio_pwrite(int fd, const void *buf, size_t count, off_t offset) {
  /* Credit to Jan Biedermann (GitHub: @janbiedermann) */
  ssize_t bytes_written = 0;
  HANDLE handle = (HANDLE)_get_osfhandle(fd);
  if (handle == INVALID_HANDLE_VALUE)
    goto bad_file;
  OVERLAPPED overlapped = {0};
  if (offset > 0)
    overlapped.Offset = offset;
  if (WriteFile(handle, buf, count, (u_long *)&bytes_written, &overlapped))
    return bytes_written;
  errno = EIO;
  return -1;
bad_file:
  errno = EBADF;
  return -1;
}

/** patch for kill */
SFUNC int fio_kill(int pid, int sig) {
  /* Credit to Jan Biedermann (GitHub: @janbiedermann) */
  HANDLE handle;
  DWORD status;
  if (sig < 0 || sig >= NSIG) {
    errno = EINVAL;
    return -1;
  }
#ifdef SIGCONT
  if (sig == SIGCONT) {
    errno = ENOSYS;
    return -1;
  }
#endif

  if (pid == -1)
    pid = 0;

  if (!pid)
    handle = GetCurrentProcess();
  else
    handle =
        OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!handle)
    goto something_went_wrong;

  switch (sig) {
#ifdef SIGKILL
  case SIGKILL:
#endif
  case SIGTERM:
  case SIGINT: /* terminate */
    if (!TerminateProcess(handle, 1))
      goto something_went_wrong;
    break;
  case 0: /* check status */
    if (!GetExitCodeProcess(handle, &status))
      goto something_went_wrong;
    if (status != STILL_ACTIVE) {
      errno = ESRCH;
      goto cleanup_after_error;
    }
    break;
  default: /* not supported? */
    errno = ENOSYS;
    goto cleanup_after_error;
  }

  if (pid) {
    CloseHandle(handle);
  }
  return 0;

something_went_wrong:

  switch (GetLastError()) {
  case ERROR_INVALID_PARAMETER:
    errno = ESRCH;
    break;
  case ERROR_ACCESS_DENIED:
    errno = EPERM;
    if (handle && GetExitCodeProcess(handle, &status) && status != STILL_ACTIVE)
      errno = ESRCH;
    break;
  default:
    errno = GetLastError();
  }
cleanup_after_error:
  if (handle && pid)
    CloseHandle(handle);
  return -1;
}

#endif /* FIO_EXTERN_COMPLETE */

/* *****************************************************************************



Patches for POSIX



***************************************************************************** */
#elif FIO_OS_POSIX /* POSIX patches */
#endif
/* *****************************************************************************
Done
***************************************************************************** */
#endif /* H___FIO_CSTL_PATCHES_H */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                  Logging





Use:

```c
FIO_LOG2STDERR("message.") // => message.
FIO_LOG_LEVEL = FIO_LOG_LEVEL_WARNING; // set dynamic logging level
FIO_LOG_INFO("message"); // => [no output, exceeds logging level]
int i = 3;
FIO_LOG_WARNING("number invalid: %d", i); // => WARNING: number invalid: 3
```

***************************************************************************** */

/**
 * Enables logging macros that avoid heap memory allocations
 */
#if !defined(H___FIO_LOG___H) && (defined(FIO_LOG) || defined(FIO_LEAK_COUNTER))
#define H___FIO_LOG___H

#if FIO_LOG_LENGTH_LIMIT > 128
#define FIO_LOG____LENGTH_ON_STACK FIO_LOG_LENGTH_LIMIT
#define FIO_LOG____LENGTH_BORDER   (FIO_LOG_LENGTH_LIMIT - 34)
#else
#define FIO_LOG____LENGTH_ON_STACK (FIO_LOG_LENGTH_LIMIT + 34)
#define FIO_LOG____LENGTH_BORDER   FIO_LOG_LENGTH_LIMIT
#endif

#undef FIO_LOG2STDERR

__attribute__((format(FIO___PRINTF_STYLE, 1, 0), weak)) void FIO_LOG2STDERR(
    const char *format,
    ...) {
  va_list argv;
  char tmp___log[FIO_LOG____LENGTH_ON_STACK + 32];
  va_start(argv, format);
  int len___log = vsnprintf(tmp___log, FIO_LOG_LENGTH_LIMIT - 2, format, argv);
  va_end(argv);
  if (len___log > 0) {
    if (len___log >= FIO_LOG_LENGTH_LIMIT - 2) {
      FIO_MEMCPY(tmp___log + FIO_LOG____LENGTH_BORDER,
                 "...\n\t\x1B[2mWARNING:\x1B[0m TRUNCATED!",
                 32);
      len___log = FIO_LOG____LENGTH_BORDER + 32;
    }
    tmp___log[len___log++] = '\n';
    tmp___log[len___log] = '0';
    fwrite(tmp___log, 1, len___log, stderr);
    return;
  }
  fwrite("\x1B[1mERROR:\x1B[0m log output error (can't write).\n",
         1,
         47,
         stderr);
}
#undef FIO_LOG____LENGTH_ON_STACK
#undef FIO_LOG____LENGTH_BORDER

// clang-format off
#undef FIO_LOG2STDERR2
#define FIO_LOG2STDERR2(...) FIO_LOG2STDERR("(" FIO__FILE__ ":" FIO_MACRO2STR(__LINE__) "): " __VA_ARGS__)
// clang-format on

/** Logging level of zero (no logging). */
#define FIO_LOG_LEVEL_NONE 0
/** Log fatal errors. */
#define FIO_LOG_LEVEL_FATAL 1
/** Log errors and fatal errors. */
#define FIO_LOG_LEVEL_ERROR 2
/** Log warnings, errors and fatal errors. */
#define FIO_LOG_LEVEL_WARNING 3
/** Log every message (info, warnings, errors and fatal errors). */
#define FIO_LOG_LEVEL_INFO 4
/** Log everything, including debug messages. */
#define FIO_LOG_LEVEL_DEBUG 5

/** The logging level */
#ifndef FIO_LOG_LEVEL_DEFAULT
#if DEBUG
#define FIO_LOG_LEVEL_DEFAULT FIO_LOG_LEVEL_DEBUG
#else
#define FIO_LOG_LEVEL_DEFAULT FIO_LOG_LEVEL_INFO
#endif
#endif
int __attribute__((weak)) FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEFAULT;

#undef FIO_LOG_PRINT__
#define FIO_LOG_PRINT__(level, ...)                                            \
  do {                                                                         \
    if (level <= FIO_LOG_LEVEL)                                                \
      FIO_LOG2STDERR(__VA_ARGS__);                                             \
  } while (0)

// clang-format off
#undef FIO_LOG_DEBUG
#define FIO_LOG_DEBUG(...)   FIO_LOG_PRINT__(FIO_LOG_LEVEL_DEBUG,"DEBUG:    (" FIO__FILE__ ":" FIO_MACRO2STR(__LINE__) ") " __VA_ARGS__)
#undef FIO_LOG_DEBUG2
#define FIO_LOG_DEBUG2(...)  FIO_LOG_PRINT__(FIO_LOG_LEVEL_DEBUG, "DEBUG:    " __VA_ARGS__)
#undef FIO_LOG_INFO
#define FIO_LOG_INFO(...)    FIO_LOG_PRINT__(FIO_LOG_LEVEL_INFO, "INFO:     " __VA_ARGS__)
#undef FIO_LOG_WARNING
#define FIO_LOG_WARNING(...) FIO_LOG_PRINT__(FIO_LOG_LEVEL_WARNING, "\x1B[2mWARNING:\x1B[0m  " __VA_ARGS__)
#undef FIO_LOG_SECURITY
#define FIO_LOG_SECURITY(...)   FIO_LOG_PRINT__(FIO_LOG_LEVEL_ERROR, "\x1B[1mSECURITY:\x1B[0m " __VA_ARGS__)
#undef FIO_LOG_ERROR
#define FIO_LOG_ERROR(...)   FIO_LOG_PRINT__(FIO_LOG_LEVEL_ERROR, "\x1B[1mERROR:\x1B[0m    " __VA_ARGS__)
#undef FIO_LOG_FATAL
#define FIO_LOG_FATAL(...)   FIO_LOG_PRINT__(FIO_LOG_LEVEL_FATAL, "\x1B[1m\x1B[7mFATAL:\x1B[0m    " __VA_ARGS__)
// clang-format on

#endif /* FIO_LOG */
#undef FIO_LOG
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#define FIO_LOCK2                   /* Development inclusion - ignore line */
#define FIO_ATOMIC                  /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            Atomic Operations




***************************************************************************** */

#if defined(FIO_ATOMIC) && !defined(H___FIO_ATOMIC___H)
#define H___FIO_ATOMIC___H 1

/* C11 Atomics are defined? */
#if defined(__ATOMIC_RELAXED)
/** An atomic load operation, returns value in pointer. */
#define fio_atomic_load(dest, p_obj)                                           \
  do {                                                                         \
    dest = __atomic_load_n((p_obj), __ATOMIC_SEQ_CST);                         \
  } while (0)

// clang-format off

/** An atomic compare and exchange operation, returns true if an exchange occured. `p_expected` MAY be overwritten with the existing value (system specific). */
#define fio_atomic_compare_exchange_p(p_obj, p_expected, p_desired) __atomic_compare_exchange((p_obj), (p_expected), (p_desired), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
/** An atomic exchange operation, returns previous value */
#define fio_atomic_exchange(p_obj, value) __atomic_exchange_n((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic addition operation, returns previous value */
#define fio_atomic_add(p_obj, value) __atomic_fetch_add((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic subtraction operation, returns previous value */
#define fio_atomic_sub(p_obj, value) __atomic_fetch_sub((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic AND (&) operation, returns previous value */
#define fio_atomic_and(p_obj, value) __atomic_fetch_and((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic XOR (^) operation, returns previous value */
#define fio_atomic_xor(p_obj, value) __atomic_fetch_xor((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic OR (|) operation, returns previous value */
#define fio_atomic_or(p_obj, value) __atomic_fetch_or((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic NOT AND ((~)&) operation, returns previous value */
#define fio_atomic_nand(p_obj, value) __atomic_fetch_nand((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic addition operation, returns new value */
#define fio_atomic_add_fetch(p_obj, value) __atomic_add_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic subtraction operation, returns new value */
#define fio_atomic_sub_fetch(p_obj, value) __atomic_sub_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic AND (&) operation, returns new value */
#define fio_atomic_and_fetch(p_obj, value) __atomic_and_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic XOR (^) operation, returns new value */
#define fio_atomic_xor_fetch(p_obj, value) __atomic_xor_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic OR (|) operation, returns new value */
#define fio_atomic_or_fetch(p_obj, value) __atomic_or_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/** An atomic NOT AND ((~)&) operation, returns new value */
#define fio_atomic_nand_fetch(p_obj, value) __atomic_nand_fetch((p_obj), (value), __ATOMIC_SEQ_CST)
/* note: __ATOMIC_SEQ_CST may be safer and __ATOMIC_ACQ_REL may be faster */

/* Select the correct compiler builtin method. */
#elif __has_builtin(__sync_add_and_fetch) || (__GNUC__ > 3)
/** An atomic load operation, returns value in pointer. */
#define fio_atomic_load(dest, p_obj)                                           \
  do {                                                                         \
    dest = *(p_obj);                                                           \
  } while (!__sync_bool_compare_and_swap((p_obj), dest, dest))


/** An atomic compare and exchange operation, returns true if an exchange occured. `p_expected` MAY be overwritten with the existing value (system specific). */
#define fio_atomic_compare_exchange_p(p_obj, p_expected, p_desired) __sync_bool_compare_and_swap((p_obj), (p_expected), *(p_desired))
/** An atomic exchange operation, ruturns previous value */
#define fio_atomic_exchange(p_obj, value) __sync_val_compare_and_swap((p_obj), *(p_obj), (value))
/** An atomic addition operation, returns new value */
#define fio_atomic_add(p_obj, value) __sync_fetch_and_add((p_obj), (value))
/** An atomic subtraction operation, returns new value */
#define fio_atomic_sub(p_obj, value) __sync_fetch_and_sub((p_obj), (value))
/** An atomic AND (&) operation, returns new value */
#define fio_atomic_and(p_obj, value) __sync_fetch_and_and((p_obj), (value))
/** An atomic XOR (^) operation, returns new value */
#define fio_atomic_xor(p_obj, value) __sync_fetch_and_xor((p_obj), (value))
/** An atomic OR (|) operation, returns new value */
#define fio_atomic_or(p_obj, value) __sync_fetch_and_or((p_obj), (value))
/** An atomic NOT AND ((~)&) operation, returns new value */
#define fio_atomic_nand(p_obj, value) __sync_fetch_and_nand((p_obj), (value))
/** An atomic addition operation, returns previous value */
#define fio_atomic_add_fetch(p_obj, value) __sync_add_and_fetch((p_obj), (value))
/** An atomic subtraction operation, returns previous value */
#define fio_atomic_sub_fetch(p_obj, value) __sync_sub_and_fetch((p_obj), (value))
/** An atomic AND (&) operation, returns previous value */
#define fio_atomic_and_fetch(p_obj, value) __sync_and_and_fetch((p_obj), (value))
/** An atomic XOR (^) operation, returns previous value */
#define fio_atomic_xor_fetch(p_obj, value) __sync_xor_and_fetch((p_obj), (value))
/** An atomic OR (|) operation, returns previous value */
#define fio_atomic_or_fetch(p_obj, value) __sync_or_and_fetch((p_obj), (value))
/** An atomic NOT AND ((~)&) operation, returns previous value */
#define fio_atomic_nand_fetch(p_obj, value) __sync_nand_and_fetch((p_obj), (value))


#elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#ifdef _MSC_VER
#pragma message ("Fallback to C11 atomic header, might be missing some features.")
#undef FIO_COMPILER_GUARD
#define FIO_COMPILER_GUARD atomic_thread_fence(memory_order_seq_cst)
#else
#warning Fallback to C11 atomic header, might be missing some features.
#endif /* _MSC_VER */
/** An atomic load operation, returns value in pointer. */
#define fio_atomic_load(dest, p_obj)  (dest = atomic_load(p_obj))

/** An atomic compare and exchange operation, returns true if an exchange occured. `p_expected` MAY be overwritten with the existing value (system specific). */
#define fio_atomic_compare_exchange_p(p_obj, p_expected, p_desired) atomic_compare_exchange_strong((p_obj), (p_expected), (p_desired))
/** An atomic exchange operation, returns previous value */
#define fio_atomic_exchange(p_obj, value) atomic_exchange((p_obj), (value))
/** An atomic addition operation, returns previous value */
#define fio_atomic_add(p_obj, value) atomic_fetch_add((p_obj), (value))
/** An atomic subtraction operation, returns previous value */
#define fio_atomic_sub(p_obj, value) atomic_fetch_sub((p_obj), (value))
/** An atomic AND (&) operation, returns previous value */
#define fio_atomic_and(p_obj, value) atomic_fetch_and((p_obj), (value))
/** An atomic XOR (^) operation, returns previous value */
#define fio_atomic_xor(p_obj, value) atomic_fetch_xor((p_obj), (value))
/** An atomic OR (|) operation, returns previous value */
#define fio_atomic_or(p_obj, value) atomic_fetch_or((p_obj), (value))
/** An atomic NOT AND ((~)&) operation, returns previous value */
#define fio_atomic_nand(p_obj, value) atomic_fetch_nand((p_obj), (value))
/** An atomic addition operation, returns new value */
#define fio_atomic_add_fetch(p_obj, value) (atomic_fetch_add((p_obj), (value)), atomic_load((p_obj)))
/** An atomic subtraction operation, returns new value */
#define fio_atomic_sub_fetch(p_obj, value) (atomic_fetch_sub((p_obj), (value)), atomic_load((p_obj)))
/** An atomic AND (&) operation, returns new value */
#define fio_atomic_and_fetch(p_obj, value) (atomic_fetch_and((p_obj), (value)), atomic_load((p_obj)))
/** An atomic XOR (^) operation, returns new value */
#define fio_atomic_xor_fetch(p_obj, value) (atomic_fetch_xor((p_obj), (value)), atomic_load((p_obj)))
/** An atomic OR (|) operation, returns new value */
#define fio_atomic_or_fetch(p_obj, value) (atomic_fetch_or((p_obj), (value)), atomic_load((p_obj)))

#elif _MSC_VER
#pragma message ("WARNING: WinAPI atomics have less features, but this is what this compiler has, so...")
#include <intrin.h>
#define FIO___ATOMICS_FN_ROUTE(fn, ptr, ...)                                   \
  ((sizeof(*ptr) == 1)                                                         \
       ? fn##8((int8_t volatile *)(ptr), __VA_ARGS__)                          \
       : (sizeof(*ptr) == 2)                                                   \
             ? fn##16((int16_t volatile *)(ptr), __VA_ARGS__)                  \
             : (sizeof(*ptr) == 4)                                             \
                   ? fn((int32_t volatile *)(ptr), __VA_ARGS__)                \
                   : fn##64((int64_t volatile *)(ptr), __VA_ARGS__))

#ifndef _WIN64
#error Atomics on Windows require 64bit OS and compiler support.
#endif

/** An atomic load operation, returns value in pointer. */
#define fio_atomic_load(dest, p_obj) (dest = *(p_obj))

/** An atomic compare and exchange operation, returns true if an exchange occured. `p_expected` MAY be overwritten with the existing value (system specific). */
#define fio_atomic_compare_exchange_p(p_obj, p_expected, p_desired) (FIO___ATOMICS_FN_ROUTE(_InterlockedCompareExchange, (p_obj),(*(p_desired)),(*(p_expected))), (*(p_obj) == *(p_desired)))
/** An atomic exchange operation, returns previous value */
#define fio_atomic_exchange(p_obj, value) FIO___ATOMICS_FN_ROUTE(_InterlockedExchange, (p_obj), (value))

/** An atomic addition operation, returns previous value */
#define fio_atomic_add(p_obj, value) FIO___ATOMICS_FN_ROUTE(_InterlockedExchangeAdd, (p_obj), (value))
/** An atomic subtraction operation, returns previous value */
#define fio_atomic_sub(p_obj, value) FIO___ATOMICS_FN_ROUTE(_InterlockedExchangeAdd, (p_obj), (0ULL - (value)))
/** An atomic AND (&) operation, returns previous value */
#define fio_atomic_and(p_obj, value) FIO___ATOMICS_FN_ROUTE(_InterlockedAnd, (p_obj), (value))
/** An atomic XOR (^) operation, returns previous value */
#define fio_atomic_xor(p_obj, value) FIO___ATOMICS_FN_ROUTE(_InterlockedXor, (p_obj), (value))
/** An atomic OR (|) operation, returns previous value */
#define fio_atomic_or(p_obj, value)  FIO___ATOMICS_FN_ROUTE(_InterlockedOr, (p_obj), (value))

/** An atomic addition operation, returns new value */
#define fio_atomic_add_fetch(p_obj, value) (fio_atomic_add((p_obj), (value)), (*(p_obj)))
/** An atomic subtraction operation, returns new value */
#define fio_atomic_sub_fetch(p_obj, value) (fio_atomic_sub((p_obj), (value)), (*(p_obj)))
/** An atomic AND (&) operation, returns new value */
#define fio_atomic_and_fetch(p_obj, value) (fio_atomic_and((p_obj), (value)), (*(p_obj)))
/** An atomic XOR (^) operation, returns new value */
#define fio_atomic_xor_fetch(p_obj, value) (fio_atomic_xor((p_obj), (value)), (*(p_obj)))
/** An atomic OR (|) operation, returns new value */
#define fio_atomic_or_fetch(p_obj, value) (fio_atomic_or((p_obj), (value)), (*(p_obj)))
#else
#error Required atomics not found (__STDC_NO_ATOMICS__) and older __sync_add_and_fetch is also missing.

#endif
// clang-format on

#define FIO_LOCK_INIT         0
#define FIO_LOCK_SUBLOCK(sub) ((uint8_t)(1U) << ((sub)&7))
typedef volatile unsigned char fio_lock_i;

/** Tries to lock a specific sublock. Returns 0 on success and 1 on failure. */
FIO_IFUNC uint8_t fio_trylock_sublock(fio_lock_i *lock, uint8_t sub) {
  FIO_COMPILER_GUARD;
  sub &= 7;
  uint8_t sub_ = 1U << sub;
  return ((fio_atomic_or(lock, sub_) & sub_) >> sub);
}

/** Busy waits for a specific sublock to become available - not recommended. */
FIO_IFUNC void fio_lock_sublock(fio_lock_i *lock, uint8_t sub) {
  while (fio_trylock_sublock(lock, sub)) {
    FIO_THREAD_RESCHEDULE();
  }
}

/** Unlocks the specific sublock, no matter which thread owns the lock. */
FIO_IFUNC void fio_unlock_sublock(fio_lock_i *lock, uint8_t sub) {
  sub = 1U << (sub & 7);
  fio_atomic_and(lock, (~(fio_lock_i)sub));
}

/**
 * Tries to lock a group of sublocks.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      if(!fio_trylock_group(&lock,
 *                            FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2))) {
 *         // act in lock
 *      }
 *
 * Returns 0 on success and non-zero on failure.
 */
FIO_IFUNC uint8_t fio_trylock_group(fio_lock_i *lock, uint8_t group) {
  if (!group)
    group = 1;
  FIO_COMPILER_GUARD;
  uint8_t state = fio_atomic_or(lock, group);
  if (!(state & group))
    return 0;
  /* release the locks we aquired, which are: ((~state) & group) */
  fio_atomic_and(lock, ~((~state) & group));
  return 1;
}

/**
 * Busy waits for a group lock to become available - not recommended.
 *
 * See `fio_trylock_group` for details.
 */
FIO_IFUNC void fio_lock_group(fio_lock_i *lock, uint8_t group) {
  while (fio_trylock_group(lock, group)) {
    FIO_THREAD_RESCHEDULE();
  }
}

/** Unlocks a sublock group, no matter which thread owns which sublock. */
FIO_IFUNC void fio_unlock_group(fio_lock_i *lock, uint8_t group) {
  if (!group)
    group = 1;
  fio_atomic_and(lock, (~group));
}

/** Tries to lock all sublocks. Returns 0 on success and 1 on failure. */
FIO_IFUNC uint8_t fio_trylock_full(fio_lock_i *lock) {
  FIO_COMPILER_GUARD;
  fio_lock_i old = fio_atomic_or(lock, ~(fio_lock_i)0);
  if (!old)
    return 0;
  fio_atomic_and(lock, old);
  return 1;
}

/** Busy waits for all sub lock to become available - not recommended. */
FIO_IFUNC void fio_lock_full(fio_lock_i *lock) {
  while (fio_trylock_full(lock)) {
    FIO_THREAD_RESCHEDULE();
  }
}

/** Unlocks all sub locks, no matter which thread owns the lock. */
FIO_IFUNC void fio_unlock_full(fio_lock_i *lock) { fio_atomic_and(lock, 0); }

/**
 * Tries to acquire the default lock (sublock 0).
 *
 * Returns 0 on success and 1 on failure.
 */
FIO_IFUNC uint8_t fio_trylock(fio_lock_i *lock) {
  return fio_trylock_sublock(lock, 0);
}

/** Busy waits for the default lock to become available - not recommended. */
FIO_IFUNC void fio_lock(fio_lock_i *lock) {
  while (fio_trylock(lock)) {
    FIO_THREAD_RESCHEDULE();
  }
}

/** Unlocks the default lock, no matter which thread owns the lock. */
FIO_IFUNC void fio_unlock(fio_lock_i *lock) { fio_unlock_sublock(lock, 0); }

/** Returns 1 if the lock is locked, 0 otherwise. */
FIO_IFUNC uint8_t FIO_NAME_BL(fio, locked)(fio_lock_i *lock) {
  return *lock & 1;
}

/** Returns 1 if the lock is locked, 0 otherwise. */
FIO_IFUNC uint8_t FIO_NAME_BL(fio, sublocked)(fio_lock_i *lock, uint8_t sub) {
  uint8_t bit = 1U << (sub & 7);
  return (((*lock) & bit) >> (sub & 7));
}

/* *****************************************************************************
Atomic operations - test
***************************************************************************** */
#if defined(FIO_TEST_CSTL)

FIO_SFUNC void FIO_NAME_TEST(stl, atomics)(void) {
  fprintf(stderr, "* Testing atomic operation macros.\n");
  struct fio___atomic_test_s {
    size_t w;
    unsigned long l;
    unsigned short s;
    unsigned char c;
  } s = {0}, r1 = {0}, r2 = {0};
  fio_lock_i lock = FIO_LOCK_INIT;

  r1.c = fio_atomic_add(&s.c, 1);
  r1.s = fio_atomic_add(&s.s, 1);
  r1.l = fio_atomic_add(&s.l, 1);
  r1.w = fio_atomic_add(&s.w, 1);
  FIO_ASSERT(r1.c == 0 && s.c == 1, "fio_atomic_add failed for c");
  FIO_ASSERT(r1.s == 0 && s.s == 1, "fio_atomic_add failed for s");
  FIO_ASSERT(r1.l == 0 && s.l == 1, "fio_atomic_add failed for l");
  FIO_ASSERT(r1.w == 0 && s.w == 1, "fio_atomic_add failed for w");
  r2.c = fio_atomic_add_fetch(&s.c, 1);
  r2.s = fio_atomic_add_fetch(&s.s, 1);
  r2.l = fio_atomic_add_fetch(&s.l, 1);
  r2.w = fio_atomic_add_fetch(&s.w, 1);
  FIO_ASSERT(r2.c == 2 && s.c == 2, "fio_atomic_add_fetch failed for c");
  FIO_ASSERT(r2.s == 2 && s.s == 2, "fio_atomic_add_fetch failed for s");
  FIO_ASSERT(r2.l == 2 && s.l == 2, "fio_atomic_add_fetch failed for l");
  FIO_ASSERT(r2.w == 2 && s.w == 2, "fio_atomic_add_fetch failed for w");
  r1.c = fio_atomic_sub(&s.c, 1);
  r1.s = fio_atomic_sub(&s.s, 1);
  r1.l = fio_atomic_sub(&s.l, 1);
  r1.w = fio_atomic_sub(&s.w, 1);
  FIO_ASSERT(r1.c == 2 && s.c == 1, "fio_atomic_sub failed for c");
  FIO_ASSERT(r1.s == 2 && s.s == 1, "fio_atomic_sub failed for s");
  FIO_ASSERT(r1.l == 2 && s.l == 1, "fio_atomic_sub failed for l");
  FIO_ASSERT(r1.w == 2 && s.w == 1, "fio_atomic_sub failed for w");
  r2.c = fio_atomic_sub_fetch(&s.c, 1);
  r2.s = fio_atomic_sub_fetch(&s.s, 1);
  r2.l = fio_atomic_sub_fetch(&s.l, 1);
  r2.w = fio_atomic_sub_fetch(&s.w, 1);
  FIO_ASSERT(r2.c == 0 && s.c == 0, "fio_atomic_sub_fetch failed for c");
  FIO_ASSERT(r2.s == 0 && s.s == 0, "fio_atomic_sub_fetch failed for s");
  FIO_ASSERT(r2.l == 0 && s.l == 0, "fio_atomic_sub_fetch failed for l");
  FIO_ASSERT(r2.w == 0 && s.w == 0, "fio_atomic_sub_fetch failed for w");
  fio_atomic_add(&s.c, 1);
  fio_atomic_add(&s.s, 1);
  fio_atomic_add(&s.l, 1);
  fio_atomic_add(&s.w, 1);
  r1.c = fio_atomic_exchange(&s.c, 99);
  r1.s = fio_atomic_exchange(&s.s, 99);
  r1.l = fio_atomic_exchange(&s.l, 99);
  r1.w = fio_atomic_exchange(&s.w, 99);
  FIO_ASSERT(r1.c == 1 && s.c == 99, "fio_atomic_exchange failed for c");
  FIO_ASSERT(r1.s == 1 && s.s == 99, "fio_atomic_exchange failed for s");
  FIO_ASSERT(r1.l == 1 && s.l == 99, "fio_atomic_exchange failed for l");
  FIO_ASSERT(r1.w == 1 && s.w == 99, "fio_atomic_exchange failed for w");
  // clang-format off
  FIO_ASSERT(!fio_atomic_compare_exchange_p(&s.c, &r1.c, &r1.c), "fio_atomic_compare_exchange_p didn't fail for c");
  FIO_ASSERT(!fio_atomic_compare_exchange_p(&s.s, &r1.s, &r1.s), "fio_atomic_compare_exchange_p didn't fail for s");
  FIO_ASSERT(!fio_atomic_compare_exchange_p(&s.l, &r1.l, &r1.l), "fio_atomic_compare_exchange_p didn't fail for l");
  FIO_ASSERT(!fio_atomic_compare_exchange_p(&s.w, &r1.w, &r1.w), "fio_atomic_compare_exchange_p didn't fail for w");
  r1.c = 1;s.c = 99; r1.s = 1;s.s = 99; r1.l = 1;s.l = 99; r1.w = 1;s.w = 99; /* ignore system spefcific behavior. */
  r1.c = fio_atomic_compare_exchange_p(&s.c,&s.c, &r1.c);
  r1.s = fio_atomic_compare_exchange_p(&s.s,&s.s, &r1.s);
  r1.l = fio_atomic_compare_exchange_p(&s.l,&s.l, &r1.l);
  r1.w = fio_atomic_compare_exchange_p(&s.w,&s.w, &r1.w);
  FIO_ASSERT(r1.c == 1 && s.c == 1, "fio_atomic_compare_exchange_p failed for c (%zu got %zu)", (size_t)s.c, (size_t)r1.c);
  FIO_ASSERT(r1.s == 1 && s.s == 1, "fio_atomic_compare_exchange_p failed for s (%zu got %zu)", (size_t)s.s, (size_t)r1.s);
  FIO_ASSERT(r1.l == 1 && s.l == 1, "fio_atomic_compare_exchange_p failed for l (%zu got %zu)", (size_t)s.l, (size_t)r1.l);
  FIO_ASSERT(r1.w == 1 && s.w == 1, "fio_atomic_compare_exchange_p failed for w (%zu got %zu)", (size_t)s.w, (size_t)r1.w);
  // clang-format on

  uint64_t val = 1;
  FIO_ASSERT(fio_atomic_and(&val, 2) == 1,
             "fio_atomic_and should return old value");
  FIO_ASSERT(val == 0, "fio_atomic_and should update value");
  FIO_ASSERT(fio_atomic_xor(&val, 1) == 0,
             "fio_atomic_xor should return old value");
  FIO_ASSERT(val == 1, "fio_atomic_xor_fetch should update value");
  FIO_ASSERT(fio_atomic_xor_fetch(&val, 1) == 0,
             "fio_atomic_xor_fetch should return new value");
  FIO_ASSERT(val == 0, "fio_atomic_xor should update value");
  FIO_ASSERT(fio_atomic_or(&val, 2) == 0,
             "fio_atomic_or should return old value");
  FIO_ASSERT(val == 2, "fio_atomic_or should update value");
  FIO_ASSERT(fio_atomic_or_fetch(&val, 1) == 3,
             "fio_atomic_or_fetch should return new value");
  FIO_ASSERT(val == 3, "fio_atomic_or_fetch should update value");
#if !_MSC_VER /* don't test missing MSVC features */
  FIO_ASSERT(fio_atomic_nand_fetch(&val, 4) == ~0ULL,
             "fio_atomic_nand_fetch should return new value");
  FIO_ASSERT(val == ~0ULL, "fio_atomic_nand_fetch should update value");
  val = 3ULL;
  FIO_ASSERT(fio_atomic_nand(&val, 4) == 3ULL,
             "fio_atomic_nand should return old value");
  FIO_ASSERT(val == ~0ULL, "fio_atomic_nand_fetch should update value");
#endif /* !_MSC_VER */
  FIO_ASSERT(!fio_is_locked(&lock),
             "lock should be initialized in unlocked state");
  FIO_ASSERT(!fio_trylock(&lock), "fio_trylock should succeed");
  FIO_ASSERT(fio_trylock(&lock), "fio_trylock should fail");
  FIO_ASSERT(fio_is_locked(&lock), "lock should be engaged");
  fio_unlock(&lock);
  FIO_ASSERT(!fio_is_locked(&lock), "lock should be released");
  fio_lock(&lock);
  FIO_ASSERT(fio_is_locked(&lock), "lock should be engaged (fio_lock)");
  for (uint8_t i = 1; i < 8; ++i) {
    FIO_ASSERT(!fio_is_sublocked(&lock, i),
               "sublock flagged, but wasn't engaged (%u - %p)",
               (unsigned int)i,
               (void *)(uintptr_t)lock);
  }
  fio_unlock(&lock);
  FIO_ASSERT(!fio_is_locked(&lock), "lock should be released");
  lock = FIO_LOCK_INIT;
  for (size_t i = 0; i < 8; ++i) {
    FIO_ASSERT(!fio_is_sublocked(&lock, i),
               "sublock should be initialized in unlocked state");
    FIO_ASSERT(!fio_trylock_sublock(&lock, i),
               "fio_trylock_sublock should succeed");
    FIO_ASSERT(fio_trylock_sublock(&lock, i), "fio_trylock should fail");
    FIO_ASSERT(fio_trylock_full(&lock), "fio_trylock_full should fail");
    FIO_ASSERT(fio_is_sublocked(&lock, i), "sub-lock %d should be engaged", i);
    {
      uint8_t g =
          fio_trylock_group(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(3));
      FIO_ASSERT((i != 1 && i != 3 && !g) || ((i == 1 || i == 3) && g),
                 "fio_trylock_group should succeed / fail");
      if (!g)
        fio_unlock_group(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(3));
    }
    for (uint8_t j = 1; j < 8; ++j) {
      FIO_ASSERT(i == j || !fio_is_sublocked(&lock, j),
                 "another sublock was flagged, though it wasn't engaged");
    }
    FIO_ASSERT(fio_is_sublocked(&lock, i), "lock should remain engaged");
    fio_unlock_sublock(&lock, i);
    FIO_ASSERT(!fio_is_sublocked(&lock, i), "sublock should be released");
    FIO_ASSERT(!fio_trylock_full(&lock), "fio_trylock_full should succeed");
    fio_unlock_full(&lock);
    FIO_ASSERT(!lock, "fio_unlock_full should unlock all");
  }
}

#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
Atomics - cleanup
***************************************************************************** */
#endif /* FIO_ATOMIC */
#undef FIO_ATOMIC

/* *****************************************************************************




                      Multi-Lock with Mutex Emulation




***************************************************************************** */
#if defined(FIO_LOCK2) && !defined(H___FIO_LOCK2___H)
#define H___FIO_LOCK2___H 1

#ifndef FIO_THREAD_T
#include <pthread.h>
#define FIO_THREAD_T pthread_t
#endif

#ifndef FIO_THREAD_ID
#define FIO_THREAD_ID() pthread_self()
#endif

#ifndef FIO_THREAD_PAUSE
#define FIO_THREAD_PAUSE(id)                                                   \
  do {                                                                         \
    sigset_t set___;                                                           \
    int got___sig;                                                             \
    sigemptyset(&set___);                                                      \
    sigaddset(&set___, SIGINT);                                                \
    sigaddset(&set___, SIGTERM);                                               \
    sigaddset(&set___, SIGCONT);                                               \
    sigwait(&set___, &got___sig);                                              \
  } while (0)
#endif

#ifndef FIO_THREAD_RESUME
#define FIO_THREAD_RESUME(id) pthread_kill((id), SIGCONT)
#endif

typedef struct fio___lock2_wait_s fio___lock2_wait_s;

/* *****************************************************************************
Public API
***************************************************************************** */

/**
 * The fio_lock2 variation is a Mutex style multi-lock.
 *
 * Thread functions and types are managed by the following macros:
 * * the `FIO_THREAD_T` macro should return a thread type, default: `pthread_t`
 * * the `FIO_THREAD_ID()` macro should return this thread's FIO_THREAD_T.
 * * the `FIO_THREAD_PAUSE(id)` macro should temporarily pause thread execution.
 * * the `FIO_THREAD_RESUME(id)` macro should resume thread execution.
 */
typedef struct {
  volatile size_t lock;
  fio___lock2_wait_s *volatile waiting;
} fio_lock2_s;

/**
 * Tries to lock a multilock.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      if(!fio_trylock2(&lock,
 *                            FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2))) {
 *         // act in lock
 *      }
 *
 * Returns 0 on success and non-zero on failure.
 */
FIO_IFUNC uint8_t fio_trylock2(fio_lock2_s *lock, size_t group);

/**
 * Locks a multilock, waiting as needed.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      fio_lock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2)));
 *
 * Doesn't return until a successful lock was acquired.
 */
SFUNC void fio_lock2(fio_lock2_s *lock, size_t group);

/**
 * Unlocks a multilock, regardless of who owns the locked group.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      fio_unlock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2));
 *
 */
SFUNC void fio_unlock2(fio_lock2_s *lock, size_t group);

/* *****************************************************************************
Implementation - Inline
***************************************************************************** */

/**
 * Tries to lock a multilock.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      if(!fio_trylock2(&lock,
 *                            FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2))) {
 *         // act in lock
 *      }
 *
 * Returns 0 on success and non-zero on failure.
 */
FIO_IFUNC uint8_t fio_trylock2(fio_lock2_s *lock, size_t group) {
  if (!group)
    group = 1;
  FIO_COMPILER_GUARD;
  size_t state = fio_atomic_or(&lock->lock, group);
  if (!(state & group))
    return 0;
  fio_atomic_and(&lock->lock, ~((~state) & group));
  return 1;
}

/* *****************************************************************************
Implementation - Extern
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE)

struct fio___lock2_wait_s {
  struct fio___lock2_wait_s *next;
  struct fio___lock2_wait_s *prev;
  FIO_THREAD_T t;
};

/**
 * Locks a multilock, waiting as needed.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      fio_lock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2)));
 *
 * Doesn't return until a successful lock was acquired.
 */
SFUNC void fio_lock2(fio_lock2_s *lock, size_t group) {
  if (!group)
    group = 1;
  FIO_COMPILER_GUARD;
  size_t state = fio_atomic_or(&lock->lock, group);
  if (!(state & group))
    return;
  /* note, we now own the part of the lock */

  /* a lock-wide (all groups) lock ID for the waitlist */
  const size_t inner_lock = (sizeof(inner_lock) >= 8)
                                ? ((size_t)1ULL << 63)
                                : (sizeof(inner_lock) >= 4)
                                      ? ((size_t)1UL << 31)
                                      : (sizeof(inner_lock) >= 2)
                                            ? ((size_t)1UL << 15)
                                            : ((size_t)1UL << 7);

  /* initialize self-waiting node memory (using stack memory) */
  fio___lock2_wait_s self_thread = {
      .next = &self_thread,
      .prev = &self_thread,
      .t = FIO_THREAD_ID(),
  };

  /* enter waitlist spinlock */
  while ((fio_atomic_or(&lock->lock, inner_lock) & inner_lock)) {
    FIO_THREAD_RESCHEDULE();
  }

  /* add self-thread to end of waitlist */
  if (lock->waiting) {
    FIO_LIST_PUSH(lock->waiting, &self_thread);
  } else {
    lock->waiting = &self_thread;
  }

  /* release waitlist spinlock and unlock any locks we may have aquired */
  fio_atomic_xor(&lock->lock, (((~state) & group) | inner_lock));

  for (;;) {
    if (!fio_trylock2(lock, group))
      break;
    /* it's possible the next thread is waiting for a different group */
    if (self_thread.next != lock->waiting) {
      FIO_THREAD_RESUME(self_thread.next->t);
    }
    if (!fio_trylock2(lock, group))
      break;
    FIO_THREAD_PAUSE(self_thread.t);
  }

  /* remove self from waiting list */
  while ((fio_atomic_or(&lock->lock, inner_lock) & inner_lock)) {
    FIO_THREAD_RESCHEDULE();
  }
  if (self_thread.next != lock->waiting) {
    FIO_THREAD_RESUME(self_thread.next->t);
  }
  FIO_LIST_REMOVE(&self_thread);
  fio_atomic_and(&lock->lock, ~inner_lock);
}

/**
 * Unlocks a multilock, regardless of who owns the locked group.
 *
 * Combine a number of sublocks using OR (`|`) and the FIO_LOCK_SUBLOCK(i)
 * macro. i.e.:
 *
 *      fio_unlock2(&lock, FIO_LOCK_SUBLOCK(1) | FIO_LOCK_SUBLOCK(2));
 *
 */
SFUNC void fio_unlock2(fio_lock2_s *lock, size_t group) {
  /* a lock-wide (all groups) lock ID for the waitlist */
  const size_t inner_lock = (sizeof(inner_lock) >= 8)
                                ? ((size_t)1ULL << 63)
                                : (sizeof(inner_lock) >= 4)
                                      ? ((size_t)1UL << 31)
                                      : (sizeof(inner_lock) >= 2)
                                            ? ((size_t)1UL << 15)
                                            : ((size_t)1UL << 7);
  fio___lock2_wait_s *waiting;
  if (!group)
    group = 1;
  /* spinlock for waitlist */
  while ((fio_atomic_or(&lock->lock, inner_lock) & inner_lock)) {
    FIO_THREAD_RESCHEDULE();
  }
  waiting = lock->waiting;
  /* unlock group & waitlist */
  fio_atomic_and(&lock->lock, ~(group | inner_lock));
  if (waiting) {
    FIO_THREAD_RESUME(waiting->t);
  }
}
#endif /* FIO_EXTERN_COMPLETE */

#endif /* FIO_LOCK2 */
#undef FIO_LOCK2
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#define FIO_BITWISE                 /* Development inclusion - ignore line */
#define FIO_BITMAP                  /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            Bit-Byte Operations



More joyful ideas at:       https://graphics.stanford.edu/~seander/bithacks.html
***************************************************************************** */

#if defined(FIO_BITWISE) && !defined(H___BITWISE___H)
#define H___BITWISE___H
/* *****************************************************************************
Swapping byte's order (`bswap` variations)
***************************************************************************** */

/** Byte swap a 16 bit integer, inlined. */
#if __has_builtin(__builtin_bswap16)
#define fio_bswap16(i) __builtin_bswap16((uint16_t)(i))
#else
FIO_IFUNC uint16_t fio_bswap16(uint16_t i) {
  return ((((i)&0xFFU) << 8) | (((i)&0xFF00U) >> 8));
}
#endif

/** Byte swap a 32 bit integer, inlined. */
#if __has_builtin(__builtin_bswap32)
#define fio_bswap32(i) __builtin_bswap32((uint32_t)(i))
#else
FIO_IFUNC uint32_t fio_bswap32(uint32_t i) {
  return ((((i)&0xFFUL) << 24) | (((i)&0xFF00UL) << 8) |
          (((i)&0xFF0000UL) >> 8) | (((i)&0xFF000000UL) >> 24));
}
#endif

/** Byte swap a 64 bit integer, inlined. */
#if __has_builtin(__builtin_bswap64)
#define fio_bswap64(i) __builtin_bswap64((uint64_t)(i))
#else
FIO_IFUNC uint64_t fio_bswap64(uint64_t i) {
  return ((((i)&0xFFULL) << 56) | (((i)&0xFF00ULL) << 40) |
          (((i)&0xFF0000ULL) << 24) | (((i)&0xFF000000ULL) << 8) |
          (((i)&0xFF00000000ULL) >> 8) | (((i)&0xFF0000000000ULL) >> 24) |
          (((i)&0xFF000000000000ULL) >> 40) |
          (((i)&0xFF00000000000000ULL) >> 56));
}
#endif

#ifdef __SIZEOF_INT128__
#if __has_builtin(__builtin_bswap128)
#define fio_bswap128(i) __builtin_bswap128((__uint128_t)(i))
#else
FIO_IFUNC __uint128_t fio_bswap128(__uint128_t i) {
  return ((__uint128_t)fio_bswap64(i) << 64) | fio_bswap64(i >> 64);
}
#endif
#endif /* __SIZEOF_INT128__ */

/* *****************************************************************************
Big Endian / Small Endian
***************************************************************************** */
#if (defined(__LITTLE_ENDIAN__) && __LITTLE_ENDIAN__) ||                       \
    (defined(__BIG_ENDIAN__) && !__BIG_ENDIAN__) ||                            \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 0
#endif
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif
#elif (defined(__BIG_ENDIAN__) && __BIG_ENDIAN__) ||                           \
    (defined(__LITTLE_ENDIAN__) && !__LITTLE_ENDIAN__) ||                      \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 0
#endif
#elif !defined(__BIG_ENDIAN__) && !defined(__BYTE_ORDER__) &&                  \
    !defined(__LITTLE_ENDIAN__)
#define FIO_LITTLE_ENDIAN_TEST 0x31323334UL
#define FIO_BIG_ENDIAN_TEST    0x34333231UL
#define FIO_ENDIAN_ORDER_TEST  ('1234')
#if ENDIAN_ORDER_TEST == LITTLE_ENDIAN_TEST
#define __BIG_ENDIAN__    0
#define __LITTLE_ENDIAN__ 1
#elif ENDIAN_ORDER_TEST == BIG_ENDIAN_TEST
#define __BIG_ENDIAN__    1
#define __LITTLE_ENDIAN__ 0
#else
#error Could not detect byte order on this system.
#endif

#endif

#if __BIG_ENDIAN__

/** Local byte order to Network byte order, 16 bit integer */
#define fio_lton16(i) (i)
/** Local byte order to Network byte order, 32 bit integer */
#define fio_lton32(i) (i)
/** Local byte order to Network byte order, 62 bit integer */
#define fio_lton64(i) (i)

/** Network byte order to Local byte order, 16 bit integer */
#define fio_ntol16(i) (i)
/** Network byte order to Local byte order, 32 bit integer */
#define fio_ntol32(i) (i)
/** Network byte order to Local byte order, 62 bit integer */
#define fio_ntol64(i) (i)

#ifdef __SIZEOF_INT128__
/** Network byte order to Local byte order, 128 bit integer */
#define fio_ntol128(i) (i)
#endif /* __SIZEOF_INT128__ */

#else /* Little Endian */

/** Local byte order to Network byte order, 16 bit integer */
#define fio_lton16(i) fio_bswap16((i))
/** Local byte order to Network byte order, 32 bit integer */
#define fio_lton32(i) fio_bswap32((i))
/** Local byte order to Network byte order, 62 bit integer */
#define fio_lton64(i) fio_bswap64((i))

/** Network byte order to Local byte order, 16 bit integer */
#define fio_ntol16(i) fio_bswap16((i))
/** Network byte order to Local byte order, 32 bit integer */
#define fio_ntol32(i) fio_bswap32((i))
/** Network byte order to Local byte order, 62 bit integer */
#define fio_ntol64(i) fio_bswap64((i))

#ifdef __SIZEOF_INT128__
/** Local byte order to Network byte order, 128 bit integer */
#define fio_lton128(i) fio_bswap128((i))
/** Network byte order to Local byte order, 128 bit integer */
#define fio_ntol128(i) fio_bswap128((i))
#endif /* __SIZEOF_INT128__ */

#endif /* __BIG_ENDIAN__ */

/* *****************************************************************************
Bit rotation
***************************************************************************** */

/** Left rotation for an unknown size element, inlined. */
#define FIO_LROT(i, bits)                                                      \
  (((i) << ((bits) & ((sizeof((i)) << 3) - 1))) |                              \
   ((i) >> ((-(bits)) & ((sizeof((i)) << 3) - 1))))

/** Right rotation for an unknown size element, inlined. */
#define FIO_RROT(i, bits)                                                      \
  (((i) >> ((bits) & ((sizeof((i)) << 3) - 1))) |                              \
   ((i) << ((-(bits)) & ((sizeof((i)) << 3) - 1))))

#if __has_builtin(__builtin_rotateleft8)
/** 8Bit left rotation, inlined. */
#define fio_lrot8(i, bits) __builtin_rotateleft8(i, bits)
#else
/** 8Bit left rotation, inlined. */
FIO_IFUNC uint8_t fio_lrot8(uint8_t i, uint8_t bits) {
  return ((i << (bits & 7UL)) | (i >> ((-(bits)) & 7UL)));
}
#endif

#if __has_builtin(__builtin_rotateleft16)
/** 16Bit left rotation, inlined. */
#define fio_lrot16(i, bits) __builtin_rotateleft16(i, bits)
#else
/** 16Bit left rotation, inlined. */
FIO_IFUNC uint16_t fio_lrot16(uint16_t i, uint8_t bits) {
  return ((i << (bits & 15UL)) | (i >> ((-(bits)) & 15UL)));
}
#endif

#if __has_builtin(__builtin_rotateleft32)
/** 32Bit left rotation, inlined. */
#define fio_lrot32(i, bits) __builtin_rotateleft32(i, bits)
#else
/** 32Bit left rotation, inlined. */
FIO_IFUNC uint32_t fio_lrot32(uint32_t i, uint8_t bits) {
  return ((i << (bits & 31UL)) | (i >> ((-(bits)) & 31UL)));
}
#endif

#if __has_builtin(__builtin_rotateleft64)
/** 64Bit left rotation, inlined. */
#define fio_lrot64(i, bits) __builtin_rotateleft64(i, bits)
#else
/** 64Bit left rotation, inlined. */
FIO_IFUNC uint64_t fio_lrot64(uint64_t i, uint8_t bits) {
  return ((i << ((bits)&63UL)) | (i >> ((-(bits)) & 63UL)));
}
#endif

#if __has_builtin(__builtin_rotatrightt8)
/** 8Bit right rotation, inlined. */
#define fio_rrot8(i, bits) __builtin_rotateright8(i, bits)
#else
/** 8Bit right rotation, inlined. */
FIO_IFUNC uint8_t fio_rrot8(uint8_t i, uint8_t bits) {
  return ((i >> (bits & 7UL)) | (i << ((-(bits)) & 7UL)));
}
#endif

#if __has_builtin(__builtin_rotateright16)
/** 16Bit right rotation, inlined. */
#define fio_rrot16(i, bits) __builtin_rotateright16(i, bits)
#else
/** 16Bit right rotation, inlined. */
FIO_IFUNC uint16_t fio_rrot16(uint16_t i, uint8_t bits) {
  return ((i >> (bits & 15UL)) | (i << ((-(bits)) & 15UL)));
}
#endif

#if __has_builtin(__builtin_rotateright32)
/** 32Bit right rotation, inlined. */
#define fio_rrot32(i, bits) __builtin_rotateright32(i, bits)
#else
/** 32Bit right rotation, inlined. */
FIO_IFUNC uint32_t fio_rrot32(uint32_t i, uint8_t bits) {
  return ((i >> (bits & 31UL)) | (i << ((-(bits)) & 31UL)));
}
#endif

#if __has_builtin(__builtin_rotateright64)
/** 64Bit right rotation, inlined. */
#define fio_rrot64(i, bits) __builtin_rotateright64(i, bits)
#else
/** 64Bit right rotation, inlined. */
FIO_IFUNC uint64_t fio_rrot64(uint64_t i, uint8_t bits) {
  return ((i >> ((bits)&63UL)) | (i << ((-(bits)) & 63UL)));
}
#endif

#ifdef __SIZEOF_INT128__
#if __has_builtin(__builtin_rotateright128) &&                                 \
    __has_builtin(__builtin_rotateleft128)
/** 128Bit left rotation, inlined. */
#define fio_lrot128(i, bits) __builtin_rotateleft128(i, bits)
/** 128Bit right rotation, inlined. */
#define fio_rrot128(i, bits) __builtin_rotateright128(i, bits)
#else
/** 128Bit left rotation, inlined. */
FIO_IFUNC __uint128_t fio_lrot128(__uint128_t i, uint8_t bits) {
  return ((i << ((bits)&127UL)) | (i >> ((-(bits)) & 127UL)));
}
/** 128Bit right rotation, inlined. */
FIO_IFUNC __uint128_t fio_rrot128(__uint128_t i, uint8_t bits) {
  return ((i >> ((bits)&127UL)) | (i << ((-(bits)) & 127UL)));
}
#endif
#endif /* __SIZEOF_INT128__ */

/* *****************************************************************************
Unaligned memory read / write operations
***************************************************************************** */

#if FIO_UNALIGNED_MEMORY_ACCESS_ENABLED
/** Converts an unaligned byte stream to a 16 bit number (local byte order). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16_local)(const void *c) {
  const uint16_t *tmp = (const uint16_t *)c; /* fio_buf2u16 */
  return *tmp;
}
/** Converts an unaligned byte stream to a 32 bit number (local byte order). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32_local)(const void *c) {
  const uint32_t *tmp = (const uint32_t *)c; /* fio_buf2u32 */
  return *tmp;
}
/** Converts an unaligned byte stream to a 64 bit number (local byte order). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64_local)(const void *c) {
  const uint64_t *tmp = (const uint64_t *)c; /* fio_buf2u64 */
  return *tmp;
}

/** Writes a local 16 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16_local)(void *buf, uint16_t i) {
  *((uint16_t *)buf) = i; /* fio_u2buf16 */
}
/** Writes a local 32 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32_local)(void *buf, uint32_t i) {
  *((uint32_t *)buf) = i; /* fio_u2buf32 */
}
/** Writes a local 64 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64_local)(void *buf, uint64_t i) {
  *((uint64_t *)buf) = i; /* fio_u2buf64 */
}

#ifdef __SIZEOF_INT128__
/** Converts an unaligned byte stream to a 128 bit number (local byte order). */
FIO_IFUNC __uint128_t FIO_NAME2(fio_buf, u128_local)(const void *c) {
  const __uint128_t *tmp = (const __uint128_t *)c; /* fio_buf2u64 */
  return *tmp;
}

/** Writes a local 128 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128_local)(void *buf, __uint128_t i) {
  *((__uint128_t *)buf) = i; /* fio_u2buf64 */
}
#endif /* __SIZEOF_INT128__ */

#else /* FIO_UNALIGNED_MEMORY_ACCESS_ENABLED */

/** Converts an unaligned byte stream to a 16 bit number (local byte order). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16_local)(const void *c) {
  uint16_t tmp; /* fio_buf2u16 */
  FIO_MEMCPY(&tmp, c, sizeof(tmp));
  return tmp;
}
/** Converts an unaligned byte stream to a 32 bit number (local byte order). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32_local)(const void *c) {
  uint32_t tmp; /* fio_buf2u32 */
  FIO_MEMCPY(&tmp, c, sizeof(tmp));
  return tmp;
}
/** Converts an unaligned byte stream to a 64 bit number (local byte order). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64_local)(const void *c) {
  uint64_t tmp; /* fio_buf2u64 */
  FIO_MEMCPY(&tmp, c, sizeof(tmp));
  return tmp;
}

/** Writes a local 16 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16_local)(void *buf, uint16_t i) {
  FIO_MEMCPY(buf, &i, sizeof(i)); /* fio_u2buf16 */
}
/** Writes a local 32 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32_local)(void *buf, uint32_t i) {
  FIO_MEMCPY(buf, &i, sizeof(i)); /* fio_u2buf32 */
}
/** Writes a local 64 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64_local)(void *buf, uint64_t i) {
  FIO_MEMCPY(buf, &i, sizeof(i)); /* fio_u2buf64 */
}

#ifdef __SIZEOF_INT128__
/** Converts an unaligned byte stream to a 128 bit number (local byte order). */
FIO_IFUNC __uint128_t FIO_NAME2(fio_buf, u128_local)(const void *c) {
  __uint128_t tmp; /* fio_buf2u1128 */
  FIO_MEMCPY(&tmp, c, sizeof(tmp));
  return tmp;
}

/** Writes a local 128 bit number to an unaligned buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128_local)(void *buf, __uint128_t i) {
  FIO_MEMCPY(buf, &i, sizeof(i)); /* fio_u2buf128 */
}
#endif /* __SIZEOF_INT128__ */

#endif /* FIO_UNALIGNED_MEMORY_ACCESS_ENABLED */

/** Converts an unaligned byte stream to a 16 bit number (reversed order). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16_bswap)(const void *c) {
  return fio_bswap16(FIO_NAME2(fio_buf, u16_local)(c)); /* fio_buf2u16 */
}
/** Converts an unaligned byte stream to a 32 bit number (reversed order). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32_bswap)(const void *c) {
  return fio_bswap32(FIO_NAME2(fio_buf, u32_local)(c)); /* fio_buf2u32 */
}
/** Converts an unaligned byte stream to a 64 bit number (reversed order). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64_bswap)(const void *c) {
  return fio_bswap64(FIO_NAME2(fio_buf, u64_local)(c)); /* fio_buf2u64 */
}

/** Writes a local 16 bit number to an unaligned buffer in reversed order. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16_bswap)(void *buf, uint16_t i) {
  FIO_NAME2(fio_u, buf16_local)(buf, fio_bswap16(i));
}
/** Writes a local 32 bit number to an unaligned buffer in reversed order. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32_bswap)(void *buf, uint32_t i) {
  FIO_NAME2(fio_u, buf32_local)(buf, fio_bswap32(i));
}
/** Writes a local 64 bit number to an unaligned buffer in reversed order. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64_bswap)(void *buf, uint64_t i) {
  FIO_NAME2(fio_u, buf64_local)(buf, fio_bswap64(i));
}

#ifdef __SIZEOF_INT128__
/** Writes a local 64 bit number to an unaligned buffer in reversed order. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128_bswap)(void *buf, __uint128_t i) {
  FIO_NAME2(fio_u, buf128_local)(buf, fio_bswap128(i));
}
#endif /* __SIZEOF_INT128__ */

/** Converts an unaligned byte stream to a 16 bit number (Big Endian). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16)(const void *c) { /* fio_buf2u16 */
  uint16_t i = FIO_NAME2(fio_buf, u16_local)(c);
  return fio_lton16(i);
}
/** Converts an unaligned byte stream to a 32 bit number (Big Endian). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32)(const void *c) { /* fio_buf2u32 */
  uint32_t i = FIO_NAME2(fio_buf, u32_local)(c);
  return fio_lton32(i);
}
/** Converts an unaligned byte stream to a 64 bit number (Big Endian). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64)(const void *c) { /* fio_buf2u64 */
  uint64_t i = FIO_NAME2(fio_buf, u64_local)(c);
  return fio_lton64(i);
}

/** Writes a local 16 bit number to an unaligned buffer in Big Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16)(void *buf, uint16_t i) {
  FIO_NAME2(fio_u, buf16_local)(buf, fio_ntol16(i));
}
/** Writes a local 32 bit number to an unaligned buffer in Big Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32)(void *buf, uint32_t i) {
  FIO_NAME2(fio_u, buf32_local)(buf, fio_ntol32(i));
}
/** Writes a local 64 bit number to an unaligned buffer in Big Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64)(void *buf, uint64_t i) {
  FIO_NAME2(fio_u, buf64_local)(buf, fio_ntol64(i));
}

#ifdef __SIZEOF_INT128__
/** Converts an unaligned byte stream to a 128 bit number (Big Endian). */
FIO_IFUNC __uint128_t FIO_NAME2(fio_buf,
                                u128)(const void *c) { /* fio_buf2u64 */
  __uint128_t i = FIO_NAME2(fio_buf, u128_local)(c);
  return fio_lton128(i);
}
/** Writes a local 128 bit number to an unaligned buffer in Big Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128)(void *buf, __uint128_t i) {
  FIO_NAME2(fio_u, buf128_local)(buf, fio_ntol128(i));
}
#endif /* __SIZEOF_INT128__ */

#if __LITTLE_ENDIAN__

/** Converts an unaligned byte stream to a 16 bit number (Little Endian). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16_little)(const void *c) {
  return FIO_NAME2(fio_buf, u16_local)(c); /* fio_buf2u16 */
}
/** Converts an unaligned byte stream to a 32 bit number (Little Endian). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32_little)(const void *c) {
  return FIO_NAME2(fio_buf, u32_local)(c); /* fio_buf2u32 */
}
/** Converts an unaligned byte stream to a 64 bit number (Little Endian). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64_little)(const void *c) {
  return FIO_NAME2(fio_buf, u64_local)(c); /* fio_buf2u64 */
}

/** Writes a local 16 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16_little)(void *buf, uint16_t i) {
  FIO_NAME2(fio_u, buf16_local)(buf, i);
}
/** Writes a local 32 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32_little)(void *buf, uint32_t i) {
  FIO_NAME2(fio_u, buf32_local)(buf, i);
}
/** Writes a local 64 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64_little)(void *buf, uint64_t i) {
  FIO_NAME2(fio_u, buf64_local)(buf, i);
}

#ifdef __SIZEOF_INT128__
/** Converts an unaligned byte stream to a 128 bit number (Little Endian). */
FIO_IFUNC __uint128_t FIO_NAME2(fio_buf, u128_little)(const void *c) {
  return FIO_NAME2(fio_buf, u128_local)(c); /* fio_buf2u64 */
}
/** Writes a local 128 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128_little)(void *buf, __uint128_t i) {
  FIO_NAME2(fio_u, buf128_local)(buf, i);
}
#endif /* __SIZEOF_INT128__ */

#else

/** Converts an unaligned byte stream to a 16 bit number (Little Endian). */
FIO_IFUNC uint16_t FIO_NAME2(fio_buf, u16_little)(const void *c) {
  return FIO_NAME2(fio_buf, u16_bswap)(c); /* fio_buf2u16 */
}
/** Converts an unaligned byte stream to a 32 bit number (Little Endian). */
FIO_IFUNC uint32_t FIO_NAME2(fio_buf, u32_little)(const void *c) {
  return FIO_NAME2(fio_buf, u32_bswap)(c); /* fio_buf2u32 */
}
/** Converts an unaligned byte stream to a 64 bit number (Little Endian). */
FIO_IFUNC uint64_t FIO_NAME2(fio_buf, u64_little)(const void *c) {
  return FIO_NAME2(fio_buf, u64_bswap)(c); /* fio_buf2u64 */
}

/** Writes a local 16 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf16_little)(void *buf, uint16_t i) {
  FIO_NAME2(fio_u, buf16_bswap)(buf, i);
}
/** Writes a local 32 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf32_little)(void *buf, uint32_t i) {
  FIO_NAME2(fio_u, buf32_bswap)(buf, i);
}
/** Writes a local 64 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf64_little)(void *buf, uint64_t i) {
  FIO_NAME2(fio_u, buf64_bswap)(buf, i);
}

#ifdef __SIZEOF_INT128__
/** Converts an unaligned byte stream to a 128 bit number (Little Endian). */
FIO_IFUNC __uint128_t FIO_NAME2(fio_buf, u128_little)(const void *c) {
  return FIO_NAME2(fio_buf, u128_bswap)(c); /* fio_buf2u64 */
}
/** Writes a local 128 bit number to an unaligned buffer in Little Endian. */
FIO_IFUNC void FIO_NAME2(fio_u, buf128_little)(void *buf, __uint128_t i) {
  FIO_NAME2(fio_u, buf128_bswap)(buf, i);
}
#endif /* __SIZEOF_INT128__ */

#endif /* __LITTLE_ENDIAN__ */

/** Convinience function for reading 1 byte (8 bit) from a buffer. */
FIO_IFUNC uint8_t FIO_NAME2(fio_buf, u8_local)(const void *c) {
  const uint8_t *tmp = (const uint8_t *)c; /* fio_buf2u16 */
  return *tmp;
}

/** Convinience function for writing 1 byte (8 bit) to a buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf8_local)(void *buf, uint8_t i) {
  *((uint8_t *)buf) = i; /* fio_u2buf16 */
}

/** Convinience function for reading 1 byte (8 bit) from a buffer. */
FIO_IFUNC uint8_t FIO_NAME2(fio_buf, u8_bswap)(const void *c) {
  const uint8_t *tmp = (const uint8_t *)c; /* fio_buf2u16 */
  return *tmp;
}

/** Convinience function for writing 1 byte (8 bit) to a buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf8_bswap)(void *buf, uint8_t i) {
  *((uint8_t *)buf) = i; /* fio_u2buf16 */
}

/** Convinience function for reading 1 byte (8 bit) from a buffer. */
FIO_IFUNC uint8_t FIO_NAME2(fio_buf, u8_little)(const void *c) {
  const uint8_t *tmp = (const uint8_t *)c; /* fio_buf2u16 */
  return *tmp;
}

/** Convinience function for writing 1 byte (8 bit) to a buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf8_little)(void *buf, uint8_t i) {
  *((uint8_t *)buf) = i; /* fio_u2buf16 */
}

/** Convinience function for reading 1 byte (8 bit) from a buffer. */
FIO_IFUNC uint8_t FIO_NAME2(fio_buf, u8)(const void *c) {
  const uint8_t *tmp = (const uint8_t *)c; /* fio_buf2u16 */
  return *tmp;
}

/** Convinience function for writing 1 byte (8 bit) to a buffer. */
FIO_IFUNC void FIO_NAME2(fio_u, buf8)(void *buf, uint8_t i) {
  *((uint8_t *)buf) = i; /* fio_u2buf16 */
}

/* *****************************************************************************
Constant-Time Selectors
***************************************************************************** */

/** Returns 1 if the expression is true (input isn't zero). */
FIO_IFUNC uintptr_t fio_ct_true(uintptr_t cond) {
  // promise that the highest bit is set if any bits are set, than shift.
  return ((cond | (0 - cond)) >> ((sizeof(cond) << 3) - 1));
}

/** Returns 1 if the expression is false (input is zero). */
FIO_IFUNC uintptr_t fio_ct_false(uintptr_t cond) {
  // fio_ct_true returns only one bit, XOR will inverse that bit.
  return fio_ct_true(cond) ^ 1;
}

/** Returns `a` if `cond` is boolean and true, returns b otherwise. */
FIO_IFUNC uintptr_t fio_ct_if_bool(uint8_t cond, uintptr_t a, uintptr_t b) {
  // b^(a^b) cancels b out. 0-1 => sets all bits.
  return (b ^ ((0 - (cond & 1)) & (a ^ b)));
}

/** Returns `a` if `cond` isn't zero (uses fio_ct_true), returns b otherwise. */
FIO_IFUNC uintptr_t fio_ct_if(uintptr_t cond, uintptr_t a, uintptr_t b) {
  // b^(a^b) cancels b out. 0-1 => sets all bits.
  return fio_ct_if_bool(fio_ct_true(cond), a, b);
}

/** Returns `a` if a >= `b`. */
FIO_IFUNC intptr_t fio_ct_max(intptr_t a_, intptr_t b_) {
  // if b - a is negative, a > b, unless both / one are negative.
  const uintptr_t a = a_, b = b_;
  return (
      intptr_t)fio_ct_if_bool(((a - b) >> ((sizeof(a) << 3) - 1)) & 1, b, a);
}

/* *****************************************************************************
SIMD emulation helpers
***************************************************************************** */

/**
 * Detects a byte where all the bits are set (255) within a 4 byte vector.
 *
 * The full byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint32_t fio_has_full_byte32(uint32_t row) {
  return ((row & UINT32_C(0x7F7F7F7F)) + UINT32_C(0x01010101)) &
         (row & UINT32_C(0x80808080));
}

/**
 * Detects a byte where no bits are set (0) within a 4 byte vector.
 *
 * The zero byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint32_t fio_has_zero_byte32(uint32_t row) {
  return fio_has_full_byte32(~row);
}

/**
 * Detects if `byte` exists within a 4 byte vector.
 *
 * The requested byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint32_t fio_has_byte32(uint32_t row, uint8_t byte) {
  return fio_has_full_byte32(~(row ^ (UINT32_C(0x01010101) * byte)));
}

/**
 * Detects a byte where all the bits are set (255) within an 8 byte vector.
 *
 * The full byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint64_t fio_has_full_byte64(uint64_t row) {
  return ((row & UINT64_C(0x7F7F7F7F7F7F7F7F)) + UINT64_C(0x0101010101010101)) &
         (row & UINT64_C(0x8080808080808080));
}

/**
 * Detects a byte where no bits are set (0) within an 8 byte vector.
 *
 * The zero byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint64_t fio_has_zero_byte64(uint64_t row) {
  return fio_has_full_byte64(~row);
}

/**
 * Detects if `byte` exists within an 8 byte vector.
 *
 * The requested byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC uint64_t fio_has_byte64(uint64_t row, uint8_t byte) {
  return fio_has_full_byte64(~(row ^ (UINT64_C(0x0101010101010101) * byte)));
}

#ifdef __SIZEOF_INT128__
/**
 * Detects a byte where all the bits are set (255) within an 16 byte vector.
 *
 * The full byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC __uint128_t fio_has_full_byte128(__uint128_t row) {
  const __uint128_t allF7 = ((__uint128_t)(0x7F7F7F7F7F7F7F7FULL) << 64) |
                            (__uint128_t)(0x7F7F7F7F7F7F7F7FULL);
  const __uint128_t all80 = ((__uint128_t)(0x8080808080808080) << 64) |
                            (__uint128_t)(0x8080808080808080);
  const __uint128_t all01 = ((__uint128_t)(0x0101010101010101) << 64) |
                            (__uint128_t)(0x0101010101010101);
  return ((row & allF7) + all01) & (row & all80);
}

/**
 * Detects a byte where no bits are set (0) within an 8 byte vector.
 *
 * The zero byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC __uint128_t fio_has_zero_byte128(__uint128_t row) {
  return fio_has_full_byte64(~row);
}

/**
 * Detects if `byte` exists within an 8 byte vector.
 *
 * The requested byte will be be set to 0x80, all other bytes will be 0x0.
 */
FIO_IFUNC __uint128_t fio_has_byte128(__uint128_t row, uint8_t byte) {
  const __uint128_t all01 = ((__uint128_t)(0x0101010101010101) << 64) |
                            (__uint128_t)(0x0101010101010101);
  return fio_has_full_byte64(~(row ^ (all01 * byte)));
}
#endif /* __SIZEOF_INT128__ */

/** Converts a `fio_has_byteX` result to a bitmap. */
FIO_IFUNC uint8_t fio_has_byte2bitmap(uint64_t result) {
  result >>= 7;             /* move result to first bit of each byte */
  result |= (result >> 7);  /* combine 2 bytes of result */
  result |= (result >> 14); /* combine 4 bytes of result */
  result |= (result >> 28); /* combine 8 bytes of result */
  return (((uint8_t)result) & 0xFF);
}

/** Isolated the least significant (lowest) bit. */
FIO_IFUNC uint64_t fio_bits_lsb(uint64_t i) { return (size_t)(i & (0 - i)); }

/** Returns the index of the most significant (highest) bit. */
FIO_IFUNC size_t fio_bits_msb_index(uint64_t i) {
  uint64_t r = 0;
  if (!i)
    goto zero;
#if defined(__has_builtin) && __has_builtin(__builtin_clzll)
  return __builtin_clzll(i);
#else
#define fio___bits_msb_index_step(x)                                           \
  if (i >= ((uint64_t)1) << x)                                                 \
    r += x, i >>= x;
  fio___bits_msb_index_step(32);
  fio___bits_msb_index_step(16);
  fio___bits_msb_index_step(8);
  fio___bits_msb_index_step(4);
  fio___bits_msb_index_step(2);
  fio___bits_msb_index_step(1);
#undef fio___bits_msb_index_step
  return r;
#endif
zero:
  r = (size_t)-1;
  return r;
}

/** Returns the index of the least significant (lowest) bit. */
FIO_IFUNC size_t fio_bits_lsb_index(uint64_t i) {
#if defined(__has_builtin) && __has_builtin(__builtin_ctzll)
  if (!i)
    return (size_t)-1;
  return __builtin_ctzll(i);
#elif 0
  return fio_bits_msb_index(fio_bits_lsb(i));
#else
  // clang-format off
  switch (fio_bits_lsb(i)) {
    case UINT64_C(0x0): return (size_t)-1;
    case UINT64_C(0x1): return 0;
    case UINT64_C(0x2): return 1;
    case UINT64_C(0x4): return 2;
    case UINT64_C(0x8): return 3;
    case UINT64_C(0x10): return 4;
    case UINT64_C(0x20): return 5;
    case UINT64_C(0x40): return 6;
    case UINT64_C(0x80): return 7;
    case UINT64_C(0x100): return 8;
    case UINT64_C(0x200): return 9;
    case UINT64_C(0x400): return 10;
    case UINT64_C(0x800): return 11;
    case UINT64_C(0x1000): return 12;
    case UINT64_C(0x2000): return 13;
    case UINT64_C(0x4000): return 14;
    case UINT64_C(0x8000): return 15;
    case UINT64_C(0x10000): return 16;
    case UINT64_C(0x20000): return 17;
    case UINT64_C(0x40000): return 18;
    case UINT64_C(0x80000): return 19;
    case UINT64_C(0x100000): return 20;
    case UINT64_C(0x200000): return 21;
    case UINT64_C(0x400000): return 22;
    case UINT64_C(0x800000): return 23;
    case UINT64_C(0x1000000): return 24;
    case UINT64_C(0x2000000): return 25;
    case UINT64_C(0x4000000): return 26;
    case UINT64_C(0x8000000): return 27;
    case UINT64_C(0x10000000): return 28;
    case UINT64_C(0x20000000): return 29;
    case UINT64_C(0x40000000): return 30;
    case UINT64_C(0x80000000): return 31;
    case UINT64_C(0x100000000): return 32;
    case UINT64_C(0x200000000): return 33;
    case UINT64_C(0x400000000): return 34;
    case UINT64_C(0x800000000): return 35;
    case UINT64_C(0x1000000000): return 36;
    case UINT64_C(0x2000000000): return 37;
    case UINT64_C(0x4000000000): return 38;
    case UINT64_C(0x8000000000): return 39;
    case UINT64_C(0x10000000000): return 40;
    case UINT64_C(0x20000000000): return 41;
    case UINT64_C(0x40000000000): return 42;
    case UINT64_C(0x80000000000): return 43;
    case UINT64_C(0x100000000000): return 44;
    case UINT64_C(0x200000000000): return 45;
    case UINT64_C(0x400000000000): return 46;
    case UINT64_C(0x800000000000): return 47;
    case UINT64_C(0x1000000000000): return 48;
    case UINT64_C(0x2000000000000): return 49;
    case UINT64_C(0x4000000000000): return 50;
    case UINT64_C(0x8000000000000): return 51;
    case UINT64_C(0x10000000000000): return 52;
    case UINT64_C(0x20000000000000): return 53;
    case UINT64_C(0x40000000000000): return 54;
    case UINT64_C(0x80000000000000): return 55;
    case UINT64_C(0x100000000000000): return 56;
    case UINT64_C(0x200000000000000): return 57;
    case UINT64_C(0x400000000000000): return 58;
    case UINT64_C(0x800000000000000): return 59;
    case UINT64_C(0x1000000000000000): return 60;
    case UINT64_C(0x2000000000000000): return 61;
    case UINT64_C(0x4000000000000000): return 62;
    case UINT64_C(0x8000000000000000): return 63;
  }
  // clang-format on
  return -1;
#endif /* __builtin vs. math vs. map */
}
/* *****************************************************************************
Byte masking (XOR) with nonce (counter mode)
***************************************************************************** */

/**
 * Masks 64 bit memory aligned data using a 64 bit mask and a counter mode
 * nonce.
 *
 * Returns the end state of the mask.
 */
FIO_IFUNC uint64_t fio___xmask2_aligned64(uint64_t buf[],
                                          size_t byte_len,
                                          uint64_t mask,
                                          uint64_t nonce) {

  register uint64_t m = mask;
  for (size_t i = 7; i < byte_len; i += 8) {
    *buf ^= m;
    m += nonce;
    ++buf;
  }
  mask = m;
  union { /* type punning */
    char *p8;
    uint64_t *p64;
  } pn, mpn;
  pn.p64 = buf;
  mpn.p64 = &mask;

  switch ((byte_len & 7)) {
  case 0:
    return mask;
  case 7:
    pn.p8[6] ^= mpn.p8[6];
  /* fallthrough */
  case 6:
    pn.p8[5] ^= mpn.p8[5];
  /* fallthrough */
  case 5:
    pn.p8[4] ^= mpn.p8[4];
  /* fallthrough */
  case 4:
    pn.p8[3] ^= mpn.p8[3];
  /* fallthrough */
  case 3:
    pn.p8[2] ^= mpn.p8[2];
  /* fallthrough */
  case 2:
    pn.p8[1] ^= mpn.p8[1];
  /* fallthrough */
  case 1:
    pn.p8[0] ^= mpn.p8[0];
    /* fallthrough */
  }
  return mask;
}

/**
 * Masks unaligned memory data using a 64 bit mask and a counter mode nonce.
 *
 * Returns the end state of the mask.
 */
FIO_IFUNC uint64_t fio___xmask2_unaligned_words(void *buf_,
                                                size_t len,
                                                uint64_t mask,
                                                const uint64_t nonce) {
  register uint8_t *buf = (uint8_t *)buf_;
  register uint64_t m = mask;
  for (size_t i = 7; i < len; i += 8) {
    uint64_t tmp;
    tmp = FIO_NAME2(fio_buf, u64_local)(buf);
    tmp ^= m;
    FIO_NAME2(fio_u, buf64_local)(buf, tmp);
    m += nonce;
    buf += 8;
  }
  mask = m;
  switch ((len & 7)) {
  case 0:
    return mask;
  case 7:
    buf[6] ^= ((uint8_t *)(&mask))[6];
  /* fallthrough */
  case 6:
    buf[5] ^= ((uint8_t *)(&mask))[5];
  /* fallthrough */
  case 5:
    buf[4] ^= ((uint8_t *)(&mask))[4];
  /* fallthrough */
  case 4:
    buf[3] ^= ((uint8_t *)(&mask))[3];
  /* fallthrough */
  case 3:
    buf[2] ^= ((uint8_t *)(&mask))[2];
  /* fallthrough */
  case 2:
    buf[1] ^= ((uint8_t *)(&mask))[1];
  /* fallthrough */
  case 1:
    buf[0] ^= ((uint8_t *)(&mask))[0];
    /* fallthrough */
  }
  return mask;
}

/**
 * Masks data using a 64 bit mask and a counter mode nonce. When the buffer's
 * memory is aligned, the function may perform significantly better.
 *
 * Returns the end state of the mask.
 */
FIO_IFUNC uint64_t fio_xmask2(char *buf,
                              size_t len,
                              uint64_t mask,
                              uint64_t nonce) {
  if (!((uintptr_t)buf & 7)) {
    union {
      char *p8;
      uint64_t *p64;
    } pn;
    pn.p8 = buf;
    return fio___xmask2_aligned64(pn.p64, len, mask, nonce);
  }
  return fio___xmask2_unaligned_words(buf, len, mask, nonce);
}

/* *****************************************************************************
Byte masking (XOR) - no nonce
***************************************************************************** */

/**
 * Masks data using a persistent 64 bit mask.
 *
 * When the buffer's memory is aligned, the function may perform significantly
 * better.
 */
FIO_IFUNC void fio_xmask(char *buf_, size_t len, uint64_t mask) {
  register uint8_t *buf = (uint8_t *)buf_;
  register uint64_t m = mask;
  for (size_t i = 7; i < len; i += 8) {
    uint64_t tmp;
    tmp = FIO_NAME2(fio_buf, u64_local)(buf);
    tmp ^= m;
    FIO_NAME2(fio_u, buf64_local)(buf, tmp);
    buf += 8;
  }
  uint64_t t = mask;
  register union { /* type punning */
    uint8_t *restrict p8;
    uint64_t *restrict p64;
  } pn;
  pn.p64 = &t;
  switch ((len & 7)) {
  case 7:
    buf[6] ^= pn.p8[6];
  /* fallthrough */
  case 6:
    buf[5] ^= pn.p8[5];
  /* fallthrough */
  case 5:
    buf[4] ^= pn.p8[4];
  /* fallthrough */
  case 4:
    buf[3] ^= pn.p8[3];
  /* fallthrough */
  case 3:
    buf[2] ^= pn.p8[2];
  /* fallthrough */
  case 2:
    buf[1] ^= pn.p8[1];
  /* fallthrough */
  case 1:
    buf[0] ^= pn.p8[0];
    /* fallthrough */
  }
}

/* *****************************************************************************
Hemming Distance and bit counting
***************************************************************************** */

#if __has_builtin(__builtin_popcountll)
/** performs a `popcount` operation to count the set bits. */
#define fio_popcount(n) __builtin_popcountll(n)
#else
FIO_IFUNC int fio_popcount(uint64_t n) {
  /* for logic, see Wikipedia: https://en.wikipedia.org/wiki/Hamming_weight */
  n = n - ((n >> 1) & 0x5555555555555555);
  n = (n & 0x3333333333333333) + ((n >> 2) & 0x3333333333333333);
  n = (n + (n >> 4)) & 0x0f0f0f0f0f0f0f0f;
  n = n + (n >> 8);
  n = n + (n >> 16);
  n = n + (n >> 32);
  return n & 0x7f;
}
#endif

#define fio_hemming_dist(n1, n2) fio_popcount(((uint64_t)(n1) ^ (uint64_t)(n2)))

/* *****************************************************************************
Bitewise helpers cleanup
***************************************************************************** */
#endif /* FIO_BITWISE */
#undef FIO_BITWISE

/* *****************************************************************************




                                Bitmap Helpers




***************************************************************************** */
#if defined(FIO_BITMAP) && !defined(H___FIO_BITMAP_H)
#define H___FIO_BITMAP_H
/* *****************************************************************************
Bitmap access / manipulation
***************************************************************************** */

/** Gets the state of a bit in a bitmap. */
FIO_IFUNC uint8_t fio_bitmap_get(void *map, size_t bit) {
  return ((((uint8_t *)(map))[(bit) >> 3] >> ((bit)&7)) & 1);
}

/** Sets the a bit in a bitmap (sets to 1). */
FIO_IFUNC void fio_bitmap_set(void *map, size_t bit) {
  fio_atomic_or((uint8_t *)(map) + ((bit) >> 3), (1UL << ((bit)&7)));
}

/** Unsets the a bit in a bitmap (sets to 0). */
FIO_IFUNC void fio_bitmap_unset(void *map, size_t bit) {
  fio_atomic_and((uint8_t *)(map) + ((bit) >> 3),
                 (uint8_t)(~(1UL << ((bit)&7))));
}

/** Flips the a bit in a bitmap (sets to 0 if 1, sets to 1 if 0). */
FIO_IFUNC void fio_bitmap_flip(void *map, size_t bit) {
  fio_atomic_xor((uint8_t *)(map) + ((bit) >> 3), (1UL << ((bit)&7)));
}

/* *****************************************************************************
Bit-Byte operations - testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL

/* used in the test, defined later */
SFUNC uint64_t fio_rand64(void);
SFUNC void fio_rand_bytes(void *target, size_t len);

FIO_SFUNC void FIO_NAME_TEST(stl, bitwise)(void) {
  fprintf(stderr, "* Testing fio_bswapX macros.\n");
  FIO_ASSERT(fio_bswap16(0x0102) == (uint16_t)0x0201, "fio_bswap16 failed");
  FIO_ASSERT(fio_bswap32(0x01020304) == (uint32_t)0x04030201,
             "fio_bswap32 failed");
  FIO_ASSERT(fio_bswap64(0x0102030405060708ULL) == 0x0807060504030201ULL,
             "fio_bswap64 failed");

  fprintf(stderr, "* Testing fio_lrotX and fio_rrotX macros.\n");
  {
    uint64_t tmp = 1;
    tmp = FIO_RROT(tmp, 1);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << ((sizeof(uint64_t) << 3) - 1)),
               "fio_rrot failed");
    tmp = FIO_LROT(tmp, 3);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << 2), "fio_lrot failed");
    tmp = 1;
    tmp = fio_rrot32(tmp, 1);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << 31), "fio_rrot32 failed");
    tmp = fio_lrot32(tmp, 3);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << 2), "fio_lrot32 failed");
    tmp = 1;
    tmp = fio_rrot64(tmp, 1);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << 63), "fio_rrot64 failed");
    tmp = fio_lrot64(tmp, 3);
    FIO_COMPILER_GUARD;
    FIO_ASSERT(tmp == ((uint64_t)1 << 2), "fio_lrot64 failed");
  }

  fprintf(stderr, "* Testing fio_buf2uX and fio_u2bufX helpers.\n");
#define FIO___BITMAP_TEST_BITS(bits)                                           \
  for (size_t i = 0; i <= (bits); ++i) {                                       \
    char tmp_buf[16];                                                          \
    int##bits##_t n = ((uint##bits##_t)1 << i);                                \
    FIO_NAME2(fio_u, buf##bits)(tmp_buf, n);                                   \
    int##bits##_t r = FIO_NAME2(fio_buf, u##bits)(tmp_buf);                    \
    FIO_ASSERT(r == n,                                                         \
               "roundtrip failed for U" #bits " at bit %zu\n\t%zu != %zu",     \
               i,                                                              \
               (size_t)n,                                                      \
               (size_t)r);                                                     \
  }
  FIO___BITMAP_TEST_BITS(8);
  FIO___BITMAP_TEST_BITS(16);
  FIO___BITMAP_TEST_BITS(32);
  FIO___BITMAP_TEST_BITS(64);
#undef FIO___BITMAP_TEST_BITS

  fprintf(stderr, "* Testing constant-time helpers.\n");
  FIO_ASSERT(fio_ct_true(0) == 0, "fio_ct_true(0) should be zero!");
  for (uintptr_t i = 1; i; i <<= 1) {
    FIO_ASSERT(fio_ct_true(i) == 1,
               "fio_ct_true(%p) should be true!",
               (void *)i);
  }
  for (uintptr_t i = 1; i + 1 != 0; i = (i << 1) | 1) {
    FIO_ASSERT(fio_ct_true(i) == 1,
               "fio_ct_true(%p) should be true!",
               (void *)i);
  }
  FIO_ASSERT(fio_ct_true(((uintptr_t)~0ULL)) == 1,
             "fio_ct_true(%p) should be true!",
             (void *)(uintptr_t)(~0ULL));

  FIO_ASSERT(fio_ct_false(0) == 1, "fio_ct_false(0) should be true!");
  for (uintptr_t i = 1; i; i <<= 1) {
    FIO_ASSERT(fio_ct_false(i) == 0,
               "fio_ct_false(%p) should be zero!",
               (void *)i);
  }
  for (uintptr_t i = 1; i + 1 != 0; i = (i << 1) | 1) {
    FIO_ASSERT(fio_ct_false(i) == 0,
               "fio_ct_false(%p) should be zero!",
               (void *)i);
  }
  FIO_ASSERT(fio_ct_false(((uintptr_t)~0ULL)) == 0,
             "fio_ct_false(%p) should be zero!",
             (void *)(uintptr_t)(~0ULL));
  FIO_ASSERT(fio_ct_true(8), "fio_ct_true should be true.");
  FIO_ASSERT(!fio_ct_true(0), "fio_ct_true should be false.");
  FIO_ASSERT(!fio_ct_false(8), "fio_ct_false should be false.");
  FIO_ASSERT(fio_ct_false(0), "fio_ct_false should be true.");
  FIO_ASSERT(fio_ct_if_bool(0, 1, 2) == 2,
             "fio_ct_if_bool selection error (false).");
  FIO_ASSERT(fio_ct_if_bool(1, 1, 2) == 1,
             "fio_ct_if_bool selection error (true).");
  FIO_ASSERT(fio_ct_if(0, 1, 2) == 2, "fio_ct_if selection error (false).");
  FIO_ASSERT(fio_ct_if(8, 1, 2) == 1, "fio_ct_if selection error (true).");
  FIO_ASSERT(fio_ct_max(1, 2) == 2, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(2, 1) == 2, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(-1, 2) == 2, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(2, -1) == 2, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(1, -2) == 1, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(-2, 1) == 1, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(-1, -2) == -1, "fio_ct_max error.");
  FIO_ASSERT(fio_ct_max(-2, -1) == -1, "fio_ct_max error.");
  {
    uint8_t bitmap[1024];
    memset(bitmap, 0, 1024);
    fprintf(stderr, "* Testing bitmap helpers.\n");
    FIO_ASSERT(!fio_bitmap_get(bitmap, 97), "fio_bitmap_get should be 0.");
    fio_bitmap_set(bitmap, 97);
    FIO_ASSERT(fio_bitmap_get(bitmap, 97) == 1,
               "fio_bitmap_get should be 1 after being set");
    FIO_ASSERT(!fio_bitmap_get(bitmap, 96),
               "other bits shouldn't be effected by set.");
    FIO_ASSERT(!fio_bitmap_get(bitmap, 98),
               "other bits shouldn't be effected by set.");
    fio_bitmap_flip(bitmap, 96);
    fio_bitmap_flip(bitmap, 97);
    FIO_ASSERT(!fio_bitmap_get(bitmap, 97),
               "fio_bitmap_get should be 0 after flip.");
    FIO_ASSERT(fio_bitmap_get(bitmap, 96) == 1,
               "other bits shouldn't be effected by flip");
    fio_bitmap_unset(bitmap, 96);
    fio_bitmap_flip(bitmap, 97);
    FIO_ASSERT(!fio_bitmap_get(bitmap, 96),
               "fio_bitmap_get should be 0 after unset.");
    FIO_ASSERT(fio_bitmap_get(bitmap, 97) == 1,
               "other bits shouldn't be effected by unset");
    fio_bitmap_unset(bitmap, 96);
  }
  {
    fprintf(stderr, "* Testing popcount and hemming distance calculation.\n");
    for (int i = 0; i < 64; ++i) {
      FIO_ASSERT(fio_popcount((uint64_t)1 << i) == 1,
                 "fio_popcount error for 1 bit");
    }
    for (int i = 0; i < 63; ++i) {
      FIO_ASSERT(fio_popcount((uint64_t)3 << i) == 2,
                 "fio_popcount error for 2 bits");
    }
    for (int i = 0; i < 62; ++i) {
      FIO_ASSERT(fio_popcount((uint64_t)7 << i) == 3,
                 "fio_popcount error for 3 bits");
    }
    for (int i = 0; i < 59; ++i) {
      FIO_ASSERT(fio_popcount((uint64_t)21 << i) == 3,
                 "fio_popcount error for 3 alternating bits");
    }
    for (int i = 0; i < 64; ++i) {
      FIO_ASSERT(fio_hemming_dist(((uint64_t)1 << i) - 1, 0) == i,
                 "fio_hemming_dist error at %d",
                 i);
    }
  }
  {
    struct test_s {
      int a;
      char force_padding;
      int b;
    } stst = {.a = 1};

    struct test_s *stst_p = FIO_PTR_FROM_FIELD(struct test_s, b, &stst.b);
    FIO_ASSERT(stst_p == &stst, "FIO_PTR_FROM_FIELD failed to retrace pointer");
  }
  {
    fprintf(stderr, "* Testing fio_xmask and fio_xmask2.\n");
    char data[128], buf[256];
    uint64_t mask;
    uint64_t counter;
    do {
      mask = fio_rand64();
      counter = fio_rand64();
    } while (!mask || !counter);
    fio_rand_bytes(data, 128);
    for (uint8_t i = 0; i < 16; ++i) {
      FIO_MEMCPY(buf + i, data, 128);
      fio_xmask(buf + i, 128, mask);
      fio_xmask(buf + i, 128, mask);
      FIO_ASSERT(!memcmp(buf + i, data, 128), "fio_xmask rountrip error");
      fio_xmask(buf + i, 128, mask);
      memmove(buf + i + 1, buf + i, 128);
      fio_xmask(buf + i + 1, 128, mask);
      FIO_ASSERT(!memcmp(buf + i + 1, data, 128),
                 "fio_xmask rountrip (with move) error");
    }
    for (uint8_t i = 0; i < 16; ++i) {
      FIO_MEMCPY(buf + i, data, 128);
      fio_xmask2(buf + i, 128, mask, counter);
      fio_xmask2(buf + i, 128, mask, counter);
      FIO_ASSERT(!memcmp(buf + i, data, 128), "fio_xmask2 CM rountrip error");
      fio_xmask2(buf + i, 128, mask, counter);
      memmove(buf + i + 1, buf + i, 128);
      fio_xmask2(buf + i + 1, 128, mask, counter);
      FIO_ASSERT(!memcmp(buf + i + 1, data, 128),
                 "fio_xmask2 CM rountrip (with move) error");
    }
  }
}
#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Bit-Byte operations - cleanup
***************************************************************************** */
#endif /* FIO_BITMAP */
#undef FIO_BITMAP
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                        Risky Hash - a fast and simple hash




***************************************************************************** */

#if defined(FIO_RISKY_HASH) && !defined(H___FIO_RISKY_HASH_H)
#define H___FIO_RISKY_HASH_H

/* *****************************************************************************
Risky Hash - API
***************************************************************************** */

/** Computes a facil.io Risky Hash (Risky v.3). */
SFUNC uint64_t fio_risky_hash(const void *buf, size_t len, uint64_t seed);

/** Adds bit of entropy to pointer values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_ptr(void *ptr);

/**
 * Masks data using a Risky Hash and a counter mode nonce.
 *
 * Used for mitigating memory access attacks when storing "secret" information
 * in memory.
 *
 * Keep the nonce information in a different memory address then the secret. For
 * example, if the secret is on the stack, store the nonce on the heap or using
 * a static variable.
 *
 * Don't use the same nonce-secret combination for other data.
 *
 * This is NOT a cryptographically secure encryption. Even if the algorithm was
 * secure, it would provide no more then a 32bit level encryption, which isn't
 * strong enough for any cryptographic use-case.
 *
 * However, this could be used to mitigate memory probing attacks. Secrets
 * stored in the memory might remain accessible after the program exists or
 * through core dump information. By storing "secret" information masked in this
 * way, it mitigates the risk of secret information being recognized or
 * deciphered.
 */
IFUNC void fio_risky_mask(char *buf, size_t len, uint64_t key, uint64_t nonce);

/* *****************************************************************************
Risky Hash - Implementation

Note: I don't remember what information I used when designing this, but Risky
Hash is probably NOT cryptographically safe (though I wanted it to be).

Here's a few resources about hashes that might explain more:
- https://komodoplatform.com/cryptographic-hash-function/
- https://en.wikipedia.org/wiki/Avalanche_effect
- http://ticki.github.io/blog/designing-a-good-non-cryptographic-hash-function/

***************************************************************************** */

/* Risky Hash primes */
#define FIO_RISKY3_PRIME0 0xCAEF89D1E9A5EB21ULL
#define FIO_RISKY3_PRIME1 0xAB137439982B86C9ULL
#define FIO_RISKY3_PRIME2 0xD9FDC73ABE9EDECDULL
#define FIO_RISKY3_PRIME3 0x3532D520F9511B13ULL
#define FIO_RISKY3_PRIME4 0x038720DDEB5A8415ULL

/** Adds bit entropy to a pointer values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_ptr(void *ptr) {
  uint64_t n = (uint64_t)(uintptr_t)ptr;
  n ^= (n + FIO_RISKY3_PRIME0) * FIO_RISKY3_PRIME1;
  n ^= fio_rrot64(n, 7);
  n ^= fio_rrot64(n, 13);
  n ^= fio_rrot64(n, 17);
  n ^= fio_rrot64(n, 31);
  return n;
}

#ifdef FIO_EXTERN_COMPLETE

/* Risky Hash initialization constants */
#define FIO_RISKY3_IV0 0x0000001000000001ULL
#define FIO_RISKY3_IV1 0x0000010000000010ULL
#define FIO_RISKY3_IV2 0x0000100000000100ULL
#define FIO_RISKY3_IV3 0x0001000000001000ULL
/* read u64 in little endian */
#define FIO_RISKY_BUF2U64 fio_buf2u64_little

/* switch to 0 if the compiler's optimizer prefers arrays... */
#if 0
/*  Computes a facil.io Risky Hash. */
SFUNC uint64_t fio_risky_hash(const void *data_, size_t len, uint64_t seed) {
  register uint64_t v0 = FIO_RISKY3_IV0;
  register uint64_t v1 = FIO_RISKY3_IV1;
  register uint64_t v2 = FIO_RISKY3_IV2;
  register uint64_t v3 = FIO_RISKY3_IV3;
  register uint64_t w0;
  register uint64_t w1;
  register uint64_t w2;
  register uint64_t w3;
  register const uint8_t *data = (const uint8_t *)data_;

#define FIO_RISKY3_ROUND64(vi, w_)                                             \
  w##vi = w_;                                                                  \
  v##vi += w##vi;                                                              \
  v##vi = fio_lrot64(v##vi, 29);                                               \
  v##vi += w##vi;                                                              \
  v##vi *= FIO_RISKY3_PRIME##vi;

#define FIO_RISKY3_ROUND256(w0, w1, w2, w3)                                    \
  FIO_RISKY3_ROUND64(0, w0);                                                   \
  FIO_RISKY3_ROUND64(1, w1);                                                   \
  FIO_RISKY3_ROUND64(2, w2);                                                   \
  FIO_RISKY3_ROUND64(3, w3);

  if (seed) {
    /* process the seed as if it was a prepended 8 Byte string. */
    v0 *= seed;
    v1 *= seed;
    v2 *= seed;
    v3 *= seed;
    v1 ^= seed;
    v2 ^= seed;
    v3 ^= seed;
  }

  for (size_t i = 31; i < len; i += 32) {
    /* vectorized 32 bytes / 256 bit access */
    FIO_RISKY3_ROUND256(FIO_RISKY_BUF2U64(data),
                        FIO_RISKY_BUF2U64(data + 8),
                        FIO_RISKY_BUF2U64(data + 16),
                        FIO_RISKY_BUF2U64(data + 24));
    data += 32;
  }
  switch (len & 24) {
  case 24:
    FIO_RISKY3_ROUND64(2, FIO_RISKY_BUF2U64(data + 16));
    /* fallthrough */
  case 16:
    FIO_RISKY3_ROUND64(1, FIO_RISKY_BUF2U64(data + 8));
    /* fallthrough */
  case 8:
    FIO_RISKY3_ROUND64(0, FIO_RISKY_BUF2U64(data + 0));
    data += len & 24;
  }

  /* add offset information to padding */
  uint64_t tmp = ((uint64_t)len & 0xFF) << 56;
  /* leftover bytes */
  switch ((len & 7)) {
  case 7:
    tmp |= ((uint64_t)data[6]) << 48; /* fallthrough */
  case 6:
    tmp |= ((uint64_t)data[5]) << 40; /* fallthrough */
  case 5:
    tmp |= ((uint64_t)data[4]) << 32; /* fallthrough */
  case 4:
    tmp |= ((uint64_t)data[3]) << 24; /* fallthrough */
  case 3:
    tmp |= ((uint64_t)data[2]) << 16; /* fallthrough */
  case 2:
    tmp |= ((uint64_t)data[1]) << 8; /* fallthrough */
  case 1:
    tmp |= ((uint64_t)data[0]);
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
  uint64_t r = (len) ^ ((uint64_t)len << 36);
  r += fio_lrot64(v0, 17) + fio_lrot64(v1, 13) + fio_lrot64(v2, 47) +
       fio_lrot64(v3, 57);
  r += v0 ^ v1;
  r ^= fio_lrot64(r, 13);
  r += v1 ^ v2;
  r ^= fio_lrot64(r, 29);
  r += v2 ^ v3;
  r += fio_lrot64(r, 33);
  r += v3 ^ v0;
  r ^= fio_lrot64(r, 51);
  r ^= (r >> 29) * FIO_RISKY3_PRIME4;
  return r;
}
#else
/*  Computes a facil.io Risky Hash. */
SFUNC uint64_t fio_risky_hash(const void *data_, size_t len, uint64_t seed) {
  FIO_ALIGN(16)
  uint64_t v[4] = {FIO_RISKY3_IV0,
                   FIO_RISKY3_IV1,
                   FIO_RISKY3_IV2,
                   FIO_RISKY3_IV3};
  FIO_ALIGN(16) uint64_t w[4];
  const uint8_t *data = (const uint8_t *)data_;

#define FIO_RISKY3_ROUND64(vi, w_)                                             \
  w[vi] = w_;                                                                  \
  v[vi] += w[vi];                                                              \
  v[vi] = fio_lrot64(v[vi], 29);                                               \
  v[vi] += w[vi];                                                              \
  v[vi] *= FIO_RISKY3_PRIME##vi;

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

  for (size_t i = 31; i < len; i += 32) {
    /* vectorized 32 bytes / 256 bit access */
    FIO_RISKY3_ROUND256(FIO_RISKY_BUF2U64(data),
                        FIO_RISKY_BUF2U64(data + 8),
                        FIO_RISKY_BUF2U64(data + 16),
                        FIO_RISKY_BUF2U64(data + 24));
    data += 32;
  }
  switch (len & 24) {
  case 24:
    FIO_RISKY3_ROUND64(2, FIO_RISKY_BUF2U64(data + 16));
    /* fallthrough */
  case 16:
    FIO_RISKY3_ROUND64(1, FIO_RISKY_BUF2U64(data + 8));
    /* fallthrough */
  case 8:
    FIO_RISKY3_ROUND64(0, FIO_RISKY_BUF2U64(data + 0));
    data += len & 24;
  }

  /* add offset information to padding */
  uint64_t tmp = ((uint64_t)len & 0xFF) << 56;
  /* leftover bytes */
  switch ((len & 7)) {
  case 7:
    tmp |= ((uint64_t)data[6]) << 48; /* fallthrough */
  case 6:
    tmp |= ((uint64_t)data[5]) << 40; /* fallthrough */
  case 5:
    tmp |= ((uint64_t)data[4]) << 32; /* fallthrough */
  case 4:
    tmp |= ((uint64_t)data[3]) << 24; /* fallthrough */
  case 3:
    tmp |= ((uint64_t)data[2]) << 16; /* fallthrough */
  case 2:
    tmp |= ((uint64_t)data[1]) << 8; /* fallthrough */
  case 1:
    tmp |= ((uint64_t)data[0]);
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
  uint64_t r = (len) ^ ((uint64_t)len << 36);
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
#endif

/**
 * Masks data using a Risky Hash and a counter mode nonce.
 */
IFUNC void fio_risky_mask(char *buf, size_t len, uint64_t key, uint64_t nonce) {
  { /* avoid zero nonce, make sure nonce is effective and odd */
    nonce |= 1;
    nonce *= 0xDB1DD478B9E93B1ULL;
    nonce ^= ((nonce << 24) | (nonce >> 40));
    nonce |= 1;
  }
  uint64_t hash = fio_risky_hash(&key, sizeof(key), nonce);
  fio_xmask2(buf, len, hash, nonce);
}
/* *****************************************************************************
Risky Hash - Cleanup
***************************************************************************** */
#undef FIO_RISKY3_ROUND64
#undef FIO_RISKY3_ROUND256
#undef FIO_RISKY_BUF2U64

#endif /* FIO_EXTERN_COMPLETE */
#endif
#undef FIO_RISKY_HASH

/* *****************************************************************************




                      Psedo-Random Generator Functions




***************************************************************************** */
#if defined(FIO_RAND) && !defined(H___FIO_RAND_H)
#define H___FIO_RAND_H
/* *****************************************************************************
Random - API
***************************************************************************** */

/** Returns 64 psedo-random bits. Probably not cryptographically safe. */
SFUNC uint64_t fio_rand64(void);

/** Writes `len` bytes of psedo-random bits to the target buffer. */
SFUNC void fio_rand_bytes(void *target, size_t len);

/** Feeds up to 1023 bytes of entropy to the random state. */
IFUNC void fio_rand_feed2seed(void *buf_, size_t len);

/** Reseeds the random engin using system state (rusage / jitter). */
IFUNC void fio_rand_reseed(void);

/* *****************************************************************************
Random - Implementation
***************************************************************************** */

#ifdef FIO_EXTERN_COMPLETE

#if FIO_OS_POSIX ||                                                            \
    (__has_include("sys/resource.h") && __has_include("sys/time.h"))
#include <sys/resource.h>
#include <sys/time.h>
#endif

static volatile uint64_t fio___rand_state[4]; /* random state */
static volatile size_t fio___rand_counter;    /* seed counter */
/* feeds random data to the algorithm through this 256 bit feed. */
static volatile uint64_t fio___rand_buffer[4] = {0x9c65875be1fce7b9ULL,
                                                 0x7cc568e838f6a40d,
                                                 0x4bb8d885a0fe47d5,
                                                 0x95561f0927ad7ecd};

IFUNC void fio_rand_feed2seed(void *buf_, size_t len) {
  len &= 1023;
  uint8_t *buf = (uint8_t *)buf_;
  uint8_t offset = (fio___rand_counter & 3);
  uint64_t tmp = 0;
  for (size_t i = 0; i < (len >> 3); ++i) {
    tmp = FIO_NAME2(fio_buf, u64_local)(buf);
    fio___rand_buffer[(offset++ & 3)] ^= tmp;
    buf += 8;
  }
  switch (len & 7) {
  case 7:
    tmp <<= 8;
    tmp |= buf[6];
    /* fallthrough */
  case 6:
    tmp <<= 8;
    tmp |= buf[5];
  /* fallthrough */
  case 5:
    tmp <<= 8;
    tmp |= buf[4];
  /* fallthrough */
  case 4:
    tmp <<= 8;
    tmp |= buf[3];
  /* fallthrough */
  case 3:
    tmp <<= 8;
    tmp |= buf[2];
  /* fallthrough */
  case 2:
    tmp <<= 8;
    tmp |= buf[1];
  /* fallthrough */
  case 1:
    tmp <<= 8;
    tmp |= buf[1];
    fio___rand_buffer[(offset & 3)] ^= tmp;
    break;
  }
}

/* used here, defined later */
FIO_IFUNC int64_t fio_time_nano();

IFUNC void fio_rand_reseed(void) {
  const size_t jitter_samples = 16 | (fio___rand_state[0] & 15);
#if defined(RUSAGE_SELF)
  {
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    fio___rand_state[0] ^=
        fio_risky_hash(&rusage, sizeof(rusage), fio___rand_state[0]);
  }
#endif
  for (size_t i = 0; i < jitter_samples; ++i) {
    uint64_t clk = (uint64_t)fio_time_nano();
    fio___rand_state[0] ^=
        fio_risky_hash(&clk, sizeof(clk), fio___rand_state[0] + i);
    clk = fio_time_nano();
    fio___rand_state[1] ^=
        fio_risky_hash(&clk,
                       sizeof(clk),
                       fio___rand_state[1] + fio___rand_counter);
  }
  fio___rand_state[2] ^=
      fio_risky_hash((void *)fio___rand_buffer,
                     sizeof(fio___rand_buffer),
                     fio___rand_counter + fio___rand_state[0]);
  fio___rand_state[3] ^= fio_risky_hash((void *)fio___rand_state,
                                        sizeof(fio___rand_state),
                                        fio___rand_state[1] + jitter_samples);
  fio___rand_buffer[0] = fio_lrot64(fio___rand_buffer[0], 31);
  fio___rand_buffer[1] = fio_lrot64(fio___rand_buffer[1], 29);
  fio___rand_buffer[2] ^= fio___rand_buffer[0];
  fio___rand_buffer[3] ^= fio___rand_buffer[1];
  fio___rand_counter += jitter_samples;
}

/* tested for randomness using code from: http://xoshiro.di.unimi.it/hwd.php */
SFUNC uint64_t fio_rand64(void) {
  /* modeled after xoroshiro128+, by David Blackman and Sebastiano Vigna */
  const uint64_t P[] = {0x37701261ED6C16C7ULL, 0x764DBBB75F3B3E0DULL};
  if (((fio___rand_counter++) & (((size_t)1 << 19) - 1)) == 0) {
    /* re-seed state every 524,288 requests / 2^19-1 attempts  */
    fio_rand_reseed();
  }
  fio___rand_state[0] +=
      (fio_lrot64(fio___rand_state[0], 33) + fio___rand_counter) * P[0];
  fio___rand_state[1] += fio_lrot64(fio___rand_state[1], 33) * P[1];
  fio___rand_state[2] +=
      (fio_lrot64(fio___rand_state[2], 33) + fio___rand_counter) * (~P[0]);
  fio___rand_state[3] += fio_lrot64(fio___rand_state[3], 33) * (~P[1]);
  return fio_lrot64(fio___rand_state[0], 31) +
         fio_lrot64(fio___rand_state[1], 29) +
         fio_lrot64(fio___rand_state[2], 27) +
         fio_lrot64(fio___rand_state[3], 30);
}

/* copies 64 bits of randomness (8 bytes) repeatedly. */
SFUNC void fio_rand_bytes(void *data_, size_t len) {
  if (!data_ || !len)
    return;
  uint8_t *data = (uint8_t *)data_;

  if (len < 8)
    goto small_random;

  if ((uintptr_t)data & 7) {
    /* align pointer to 64 bit word */
    size_t offset = 8 - ((uintptr_t)data & 7);
    fio_rand_bytes(data_, offset); /* perform small_random */
    data += offset;
    len -= offset;
  }

  /* 128 random bits at a time */
  for (size_t i = (len >> 4); i; --i) {
    uint64_t t0 = fio_rand64();
    uint64_t t1 = fio_rand64();
    FIO_NAME2(fio_u, buf64_local)(data, t0);
    FIO_NAME2(fio_u, buf64_local)(data + 8, t1);
    data += 16;
  }
  /* 64 random bits at tail */
  if ((len & 8)) {
    uint64_t t0 = fio_rand64();
    FIO_NAME2(fio_u, buf64_local)(data, t0);
  }

small_random:
  if ((len & 7)) {
    /* leftover bits */
    uint64_t tmp = fio_rand64();
    /* leftover bytes */
    switch ((len & 7)) {
    case 7:
      data[6] = (tmp >> 8) & 0xFF;
      /* fallthrough */
    case 6:
      data[5] = (tmp >> 16) & 0xFF;
      /* fallthrough */
    case 5:
      data[4] = (tmp >> 24) & 0xFF;
      /* fallthrough */
    case 4:
      data[3] = (tmp >> 32) & 0xFF;
      /* fallthrough */
    case 3:
      data[2] = (tmp >> 40) & 0xFF;
      /* fallthrough */
    case 2:
      data[1] = (tmp >> 48) & 0xFF;
      /* fallthrough */
    case 1:
      data[0] = (tmp >> 56) & 0xFF;
    }
  }
}

/* *****************************************************************************
Hashing speed test
***************************************************************************** */
#ifdef FIO_TEST_CSTL
#include <math.h>

typedef uintptr_t (*fio__hashing_func_fn)(char *, size_t);

FIO_SFUNC void fio_test_hash_function(fio__hashing_func_fn h,
                                      char *name,
                                      uint8_t size_log,
                                      uint8_t mem_alignment_ofset,
                                      uint8_t fast) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  if (size_log >= 21 || ((sizeof(uint64_t) - 1) >> size_log)) {
    FIO_LOG_ERROR("fio_test_hash_function called with a log size too big.");
    return;
  }
  mem_alignment_ofset &= 7;
  size_t const buffer_len = (1ULL << size_log);
  uint64_t cycles_start_at = (1ULL << (16 + (fast * 2)));
  if (size_log < 13)
    cycles_start_at <<= (13 - size_log);
  else if (size_log > 13)
    cycles_start_at >>= (size_log - 13);

#ifdef DEBUG
  fprintf(stderr,
          "* Testing %s speed with %zu byte blocks"
          "(DEBUG mode detected - speed may be affected).\n",
          name,
          buffer_len);
#else
  fprintf(stderr,
          "* Testing %s speed with %zu byte blocks.\n",
          name,
          buffer_len);
#endif

  uint8_t *buffer_mem = (uint8_t *)
      FIO_MEM_REALLOC(NULL, 0, (buffer_len + mem_alignment_ofset) + 64, 0);
  uint8_t *buffer = buffer_mem + mem_alignment_ofset;

  memset(buffer, 'T', buffer_len);
  /* warmup */
  uint64_t hash = 0;
  for (size_t i = 0; i < 4; i++) {
    hash += h((char *)buffer, buffer_len);
    FIO_MEMCPY(buffer, &hash, sizeof(hash));
  }
  /* loop until test runs for more than 2 seconds */
  for (uint64_t cycles = cycles_start_at;;) {
    clock_t start, end;
    start = clock();
    for (size_t i = cycles; i > 0; i--) {
      hash += h((char *)buffer, buffer_len);
      FIO_COMPILER_GUARD;
    }
    end = clock();
    FIO_MEMCPY(buffer, &hash, sizeof(hash));
    if ((end - start) >= (2 * CLOCKS_PER_SEC) ||
        cycles >= ((uint64_t)1 << 62)) {
      fprintf(stderr,
              "\t%-40s %8.2f MB/s\n",
              name,
              (double)(buffer_len * cycles) /
                  (((end - start) * (1000000.0 / CLOCKS_PER_SEC))));
      break;
    }
    cycles <<= 1;
  }
  FIO_MEM_FREE(buffer_mem, (buffer_len + mem_alignment_ofset) + 64);
}

FIO_SFUNC uintptr_t FIO_NAME_TEST(stl, risky_wrapper)(char *buf, size_t len) {
  return fio_risky_hash(buf, len, 1);
}

FIO_SFUNC uintptr_t FIO_NAME_TEST(stl, risky_mask_wrapper)(char *buf,
                                                           size_t len) {
  fio_risky_mask(buf, len, 0, 0);
  return len;
}

FIO_SFUNC uintptr_t FIO_NAME_TEST(stl, xmask_wrapper)(char *buf, size_t len) {
  fio_xmask(buf, len, fio_rand64());
  return len;
}

FIO_SFUNC void FIO_NAME_TEST(stl, risky)(void) {
  for (int i = 0; i < 8; ++i) {
    char buf[128];
    uint64_t nonce = fio_rand64();
    const char *str = "this is a short text, to test risky masking";
    char *tmp = buf + i;
    FIO_MEMCPY(tmp, str, strlen(str));
    fio_risky_mask(tmp, strlen(str), (uint64_t)(uintptr_t)tmp, nonce);
    FIO_ASSERT(memcmp(tmp, str, strlen(str)), "Risky Hash masking failed");
    size_t err = 0;
    for (size_t b = 0; b < strlen(str); ++b) {
      FIO_ASSERT(tmp[b] != str[b] || (err < 2),
                 "Risky Hash masking didn't mask buf[%zu] on offset "
                 "%d (statistical deviation?)",
                 b,
                 i);
      err += (tmp[b] == str[b]);
    }
    fio_risky_mask(tmp, strlen(str), (uint64_t)(uintptr_t)tmp, nonce);
    FIO_ASSERT(!memcmp(tmp, str, strlen(str)), "Risky Hash masking RT failed");
  }
  const uint8_t alignment_test_offset = 0;
  if (alignment_test_offset)
    fprintf(stderr,
            "The following speed tests use a memory alignment offset of %d "
            "bytes.\n",
            (int)(alignment_test_offset & 7));
#if !DEBUG
  fio_test_hash_function(FIO_NAME_TEST(stl, risky_wrapper),
                         (char *)"fio_risky_hash",
                         7,
                         alignment_test_offset,
                         3);
  fio_test_hash_function(FIO_NAME_TEST(stl, risky_wrapper),
                         (char *)"fio_risky_hash",
                         13,
                         alignment_test_offset,
                         2);
  fio_test_hash_function(FIO_NAME_TEST(stl, risky_mask_wrapper),
                         (char *)"fio_risky_mask (Risky XOR + counter)",
                         13,
                         alignment_test_offset,
                         4);
  fio_test_hash_function(FIO_NAME_TEST(stl, risky_mask_wrapper),
                         (char *)"fio_risky_mask (unaligned)",
                         13,
                         1,
                         4);
  if (0) {
    fio_test_hash_function(FIO_NAME_TEST(stl, xmask_wrapper),
                           (char *)"fio_xmask (XOR, NO counter)",
                           13,
                           alignment_test_offset,
                           4);
    fio_test_hash_function(FIO_NAME_TEST(stl, xmask_wrapper),
                           (char *)"fio_xmask (unaligned)",
                           13,
                           1,
                           4);
  }
#endif
}

FIO_SFUNC void FIO_NAME_TEST(stl, random_buffer)(uint64_t *stream,
                                                 size_t len,
                                                 const char *name,
                                                 size_t clk) {
  size_t totals[2] = {0};
  size_t freq[256] = {0};
  const size_t total_bits = (len * sizeof(*stream) * 8);
  uint64_t hemming = 0;
  /* collect data */
  for (size_t i = 1; i < len; i += 2) {
    hemming += fio_hemming_dist(stream[i], stream[i - 1]);
    for (size_t byte = 0; byte < (sizeof(*stream) << 1); ++byte) {
      uint8_t val = ((uint8_t *)(stream + (i - 1)))[byte];
      ++freq[val];
      for (int bit = 0; bit < 8; ++bit) {
        ++totals[(val >> bit) & 1];
      }
    }
  }
  hemming /= len;
  fprintf(stderr, "\n");
#if DEBUG
  fprintf(stderr,
          "\t- \x1B[1m%s\x1B[0m (%zu CPU cycles NOT OPTIMIZED):\n",
          name,
          clk);
#else
  fprintf(stderr, "\t- \x1B[1m%s\x1B[0m (%zu CPU cycles):\n", name, clk);
#endif
  fprintf(stderr,
          "\t  zeros / ones (bit frequency)\t%.05f\n",
          ((float)1.0 * totals[0]) / totals[1]);
  if (!(totals[0] < totals[1] + (total_bits / 20) &&
        totals[1] < totals[0] + (total_bits / 20)))
    FIO_LOG_ERROR("randomness isn't random?");
  fprintf(stderr,
          "\t  avarage hemming distance\t%zu (should be: 14-18)\n",
          (size_t)hemming);
  /* expect avarage hemming distance of 25% == 16 bits */
  if (!(hemming >= 14 && hemming <= 18))
    FIO_LOG_ERROR("randomness isn't random (hemming distance failed)?");
  /* test chi-square ... I think */
  if (len * sizeof(*stream) > 2560) {
    double n_r = (double)1.0 * ((len * sizeof(*stream)) / 256);
    double chi_square = 0;
    for (unsigned int i = 0; i < 256; ++i) {
      double f = freq[i] - n_r;
      chi_square += (f * f);
    }
    chi_square /= n_r;
    double chi_square_r_abs =
        (chi_square - 256 >= 0) ? chi_square - 256 : (256 - chi_square);
    fprintf(
        stderr,
        "\t  chi-sq. variation\t\t%.02lf - %s (expect <= %0.2lf)\n",
        chi_square_r_abs,
        ((chi_square_r_abs <= 2 * (sqrt(n_r)))
             ? "good"
             : ((chi_square_r_abs <= 3 * (sqrt(n_r))) ? "not amazing"
                                                      : "\x1B[1mBAD\x1B[0m")),
        2 * (sqrt(n_r)));
  }
}

FIO_SFUNC void FIO_NAME_TEST(stl, random)(void) {
  fprintf(stderr,
          "* Testing randomness "
          "- bit frequency / hemming distance / chi-square.\n");
  const size_t test_len = (TEST_REPEAT << 7);
  uint64_t *rs =
      (uint64_t *)FIO_MEM_REALLOC(NULL, 0, sizeof(*rs) * test_len, 0);
  clock_t start, end;
  FIO_ASSERT_ALLOC(rs);

  rand(); /* warmup */
  if (sizeof(int) < sizeof(uint64_t)) {
    start = clock();
    for (size_t i = 0; i < test_len; ++i) {
      rs[i] = ((uint64_t)rand() << 32) | (uint64_t)rand();
    }
    end = clock();
  } else {
    start = clock();
    for (size_t i = 0; i < test_len; ++i) {
      rs[i] = (uint64_t)rand();
    }
    end = clock();
  }
  FIO_NAME_TEST(stl, random_buffer)
  (rs, test_len, "rand (system - naive, ignoring missing bits)", end - start);

  memset(rs, 0, sizeof(*rs) * test_len);
  {
    if (RAND_MAX == ~(uint64_t)0ULL) {
      /* RAND_MAX fills all bits */
      start = clock();
      for (size_t i = 0; i < test_len; ++i) {
        rs[i] = (uint64_t)rand();
      }
      end = clock();
    } else if (RAND_MAX >= (~(uint32_t)0UL)) {
      /* RAND_MAX fill at least 32 bits per call */
      uint32_t *rs_adjusted = (uint32_t *)rs;

      start = clock();
      for (size_t i = 0; i < (test_len << 1); ++i) {
        rs_adjusted[i] = (uint32_t)rand();
      }
      end = clock();
    } else if (RAND_MAX >= (~(uint16_t)0U)) {
      /* RAND_MAX fill at least 16 bits per call */
      uint16_t *rs_adjusted = (uint16_t *)rs;

      start = clock();
      for (size_t i = 0; i < (test_len << 2); ++i) {
        rs_adjusted[i] = (uint16_t)rand();
      }
      end = clock();
    } else {
      /* assume RAND_MAX fill at least 8 bits per call */
      uint8_t *rs_adjusted = (uint8_t *)rs;

      start = clock();
      for (size_t i = 0; i < (test_len << 2); ++i) {
        rs_adjusted[i] = (uint8_t)rand();
      }
      end = clock();
    }
    /* test RAND_MAX value */
    uint8_t rand_bits = 63;
    while (rand_bits) {
      if (RAND_MAX <= (~(0ULL)) >> rand_bits)
        break;
      --rand_bits;
    }
    rand_bits = 64 - rand_bits;

    char buffer[128] = {0};
    snprintf(buffer,
             128 - 14,
             "rand (system - fixed, testing %d random bits)",
             (int)rand_bits);
    FIO_NAME_TEST(stl, random_buffer)(rs, test_len, buffer, end - start);
  }

  memset(rs, 0, sizeof(*rs) * test_len);
  fio_rand64(); /* warmup */
  start = clock();
  for (size_t i = 0; i < test_len; ++i) {
    rs[i] = fio_rand64();
  }
  end = clock();
  FIO_NAME_TEST(stl, random_buffer)(rs, test_len, "fio_rand64", end - start);
  memset(rs, 0, sizeof(*rs) * test_len);
  start = clock();
  fio_rand_bytes(rs, test_len * sizeof(*rs));
  end = clock();
  FIO_NAME_TEST(stl, random_buffer)
  (rs, test_len, "fio_rand_bytes", end - start);

  fio_rand_feed2seed(rs, sizeof(*rs) * test_len);
  FIO_MEM_FREE(rs, sizeof(*rs) * test_len);
  fprintf(stderr, "\n");
#if DEBUG
  fprintf(stderr,
          "\t- to compare CPU cycles, test randomness with optimization.\n\n");
#endif /* DEBUG */
}
#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Random - Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_RAND */
#undef FIO_RAND
/* *****************************************************************************
Copyright: Boaz Segev, 2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_BITWISE                 /* Development inclusion - ignore line */
#define FIO_SHA1                    /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                    SHA 1




***************************************************************************** */
#ifdef FIO_SHA1
/* *****************************************************************************
SHA 1
***************************************************************************** */

/** The data tyope containing the SHA1 digest (result). */
typedef union {
#ifdef __SIZEOF_INT128__
  __uint128_t align__;
#else
  uint64_t align__;
#endif
  uint32_t v[5];
  uint8_t digest[20];
} fio_sha1_s;

/**
 * A simple, non streaming, implementation of the SHA1 hashing algorithm.
 *
 * Do NOT use - SHA1 is broken... but for some reason some protocols still
 * require it's use (i.e., WebSockets), so it's here for your convenience.
 */
SFUNC fio_sha1_s fio_sha1(const void *data, uint64_t len);

/** returns the digest length of SHA1 in bytes */
FIO_IFUNC size_t fio_sha1_len(void);

/** returns the digest of a SHA1 object. */
FIO_IFUNC uint8_t *fio_sha1_digest(fio_sha1_s *s);

/* *****************************************************************************
SHA 1 Implementation - inlined static functions
***************************************************************************** */

/** returns the digest length of SHA1 in bytes */
FIO_IFUNC size_t fio_sha1_len(void) { return 20; }

/** returns the digest of a SHA1 object. */
FIO_IFUNC uint8_t *fio_sha1_digest(fio_sha1_s *s) { return s->digest; }

/* *****************************************************************************
Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

FIO_IFUNC void fio___sha1_round512(fio_sha1_s *old, /* state */
                                   uint32_t *w /* 16 words */) {

  register uint32_t v0 = old->v[0];
  register uint32_t v1 = old->v[1];
  register uint32_t v2 = old->v[2];
  register uint32_t v3 = old->v[3];
  register uint32_t v4 = old->v[4];
  register uint32_t v5;

#define FIO___SHA1_ROTATE(K, F, i)                                             \
  v5 = fio_lrot32(v0, 5) + v4 + F + (uint32_t)K + w[(i)&15];                   \
  v4 = v3;                                                                     \
  v3 = v2;                                                                     \
  v2 = fio_lrot32(v1, 30);                                                     \
  v1 = v0;                                                                     \
  v0 = v5;
#define FIO___SHA1_CALC_WORD(i)                                                \
  fio_lrot32(                                                                  \
      (w[(i + 13) & 15] ^ w[(i + 8) & 15] ^ w[(i + 2) & 15] ^ w[(i)&15]),      \
      1);

#define FIO___SHA1_ROUND4(K, F, i)                                             \
  FIO___SHA1_ROUND((K), (F), i);                                               \
  FIO___SHA1_ROUND((K), (F), i + 1);                                           \
  FIO___SHA1_ROUND((K), (F), i + 2);                                           \
  FIO___SHA1_ROUND((K), (F), i + 3);
#define FIO___SHA1_ROUND16(K, F, i)                                            \
  FIO___SHA1_ROUND4((K), (F), i);                                              \
  FIO___SHA1_ROUND4((K), (F), i + 4);                                          \
  FIO___SHA1_ROUND4((K), (F), i + 8);                                          \
  FIO___SHA1_ROUND4((K), (F), i + 12);
#define FIO___SHA1_ROUND20(K, F, i)                                            \
  FIO___SHA1_ROUND16(K, F, i);                                                 \
  FIO___SHA1_ROUND4((K), (F), i + 16);

#define FIO___SHA1_ROUND(K, F, i)                                              \
  w[i] = fio_ntol32(w[i]);                                                     \
  FIO___SHA1_ROTATE(K, F, i);

  FIO___SHA1_ROUND16(0x5A827999, ((v1 & v2) | ((~v1) & (v3))), 0);

#undef FIO___SHA1_ROUND
#define FIO___SHA1_ROUND(K, F, i)                                              \
  w[(i)&15] = FIO___SHA1_CALC_WORD(i);                                         \
  FIO___SHA1_ROTATE(K, F, i);

  FIO___SHA1_ROUND4(0x5A827999, ((v1 & v2) | ((~v1) & (v3))), 16);

  FIO___SHA1_ROUND20(0x6ED9EBA1, (v1 ^ v2 ^ v3), 20);
  FIO___SHA1_ROUND20(0x8F1BBCDC, ((v1 & (v2 | v3)) | (v2 & v3)), 40);
  FIO___SHA1_ROUND20(0xCA62C1D6, (v1 ^ v2 ^ v3), 60);

  old->v[0] += v0;
  old->v[1] += v1;
  old->v[2] += v2;
  old->v[3] += v3;
  old->v[4] += v4;

#undef FIO___SHA1_ROTATE
#undef FIO___SHA1_CALC_WORD
#undef FIO___SHA1_ROUND
#undef FIO___SHA1_ROUND4
#undef FIO___SHA1_ROUND16
#undef FIO___SHA1_ROUND20
}

/**
 * A simple, non streaming, implementation of the SHA1 hashing algorithm.
 *
 * Do NOT use - SHA1 is broken... but for some reason some protocols still
 * require it's use (i.e., WebSockets), so it's here for your convinience.
 */
SFUNC fio_sha1_s fio_sha1(const void *data, uint64_t len) {
  /* TODO: hash */

  fio_sha1_s s = (fio_sha1_s){
      .v =
          {
              0x67452301,
              0xEFCDAB89,
              0x98BADCFE,
              0x10325476,
              0xC3D2E1F0,
          },
  };

  const uint8_t *buf = (const uint8_t *)data;

  uint32_t vec[16];

  for (size_t i = 63; i < len; i += 64) {
    FIO_MEMCPY(vec, buf, 64);
    fio___sha1_round512(&s, vec);
    buf += 64;
  }
  memset(vec, 0, sizeof(vec));
  if ((len & 63)) {
    FIO_MEMCPY(vec, buf, (len & 63));
  }
  ((uint8_t *)vec)[(len & 63)] = 0x80;

  if ((len & 63) > 55) {
    fio___sha1_round512(&s, vec);
    memset(vec, 0, sizeof(vec));
  }

  fio_u2buf64((void *)(vec + 14), (len << 3));
  fio___sha1_round512(&s, vec);

  s.v[0] = fio_ntol32(s.v[0]);
  s.v[1] = fio_ntol32(s.v[1]);
  s.v[2] = fio_ntol32(s.v[2]);
  s.v[3] = fio_ntol32(s.v[3]);
  s.v[4] = fio_ntol32(s.v[4]);
  return s;
}

/* *****************************************************************************
SHA1 Testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL

FIO_SFUNC uintptr_t FIO_NAME_TEST(stl, __sha1_wrapper)(char *data, size_t len) {
  fio_sha1_s h = fio_sha1((const void *)data, (uint64_t)len);
  return *(uintptr_t *)h.digest;
}

#if HAVE_OPENSSL
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

FIO_SFUNC uintptr_t FIO_NAME_TEST(stl, __sha1_open_ssl_wrapper)(char *data,
                                                                size_t len) {
  /* test based on code from BearSSL with credit to Thomas Pornin */
  uintptr_t result[6];
  SHA_CTX o_sh1;
  SHA1_Init(&o_sh1);
  SHA1_Update(&o_sh1, data, len);
  SHA1_Final((unsigned char *)result, &o_sh1);
  return result[0];
}

#endif

FIO_SFUNC void FIO_NAME_TEST(stl, sha1)(void) {
  fprintf(stderr, "* Testing SHA1\n");
  struct {
    const char *str;
    const char *sha1;
  } data[] = {
      {
          .str = "",
          .sha1 = "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18"
                  "\x90\xaf\xd8\x07\x09",
      },
      {
          .str = "The quick brown fox jumps over the lazy dog",
          .sha1 = "\x2f\xd4\xe1\xc6\x7a\x2d\x28\xfc\xed\x84\x9e\xe1\xbb\x76\xe7"
                  "\x39\x1b\x93\xeb\x12",
      },
      {
          .str = "The quick brown fox jumps over the lazy cog",
          .sha1 = "\xde\x9f\x2c\x7f\xd2\x5e\x1b\x3a\xfa\xd3\xe8\x5a\x0b\xd1\x7d"
                  "\x9b\x10\x0d\xb4\xb3",
      },
  };
  for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
    fio_sha1_s sha1 = fio_sha1(data[i].str, strlen(data[i].str));

    FIO_ASSERT(!memcmp(sha1.digest, data[i].sha1, fio_sha1_len()),
               "SHA1 mismatch for \"%s\"",
               data[i].str);
  }
#if !DEBUG
  fio_test_hash_function(FIO_NAME_TEST(stl, __sha1_wrapper),
                         (char *)"fio_sha1",
                         5,
                         0,
                         0);
  fio_test_hash_function(FIO_NAME_TEST(stl, __sha1_wrapper),
                         (char *)"fio_sha1",
                         13,
                         0,
                         1);
#if HAVE_OPENSSL
  fio_test_hash_function(FIO_NAME_TEST(stl, __sha1_open_ssl_wrapper),
                         (char *)"OpenSSL SHA1",
                         5,
                         0,
                         0);
  fio_test_hash_function(FIO_NAME_TEST(stl, __sha1_open_ssl_wrapper),
                         (char *)"OpenSSL SHA1",
                         13,
                         0,
                         1);
#endif /* HAVE_OPENSSL */
#endif /* !DEBUG */
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_SHA1 */
#undef FIO_SHA1
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_ATOL                    /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#define FIO_TEST_CSTL               /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            String <=> Number helpers




***************************************************************************** */
#if defined(FIO_ATOL) && !defined(H___FIO_ATOL_H)
#define H___FIO_ATOL_H
#include <inttypes.h>
#include <math.h>
/* *****************************************************************************
Strings to Numbers - API
***************************************************************************** */
/**
 * A helper function that converts between String data to a signed int64_t.
 *
 * Numbers are assumed to be in base 10. Octal (`0###`), Hex (`0x##`/`x##`) and
 * binary (`0b##`/ `b##`) are recognized as well. For binary Most Significant
 * Bit must come first.
 *
 * The most significant difference between this function and `strtol` (aside of
 * API design), is the added support for binary representations.
 */
SFUNC int64_t fio_atol(char **pstr);

/** A helper function that converts between String data to a signed double. */
SFUNC double fio_atof(char **pstr);

/**
 * Maps characters to alphanumerical value, where numbers have their natural
 * values (0-9) and `A-Z` (or `a-z`) are the values 10-35.
 *
 * Out of bound values return 255.
 *
 * This allows parsing of numeral strings for up to base 36.
 */
IFUNC uint8_t fio_c2i(unsigned char c);

/**
 * Maps numeral values to alphanumerical characters, where numbers have their
 * natural values (0-9) and `A-Z` are the values 10-35.
 *
 * Accepts values up to 63. Returns zero for values over 35. Out of bound values
 * produce undefined behavior.
 *
 * This allows printing of numerals for up to base 36.
 */
IFUNC uint8_t fio_i2c(unsigned char i);

/* *****************************************************************************
Numbers to Strings - API
***************************************************************************** */

/**
 * A helper function that writes a signed int64_t to a string.
 *
 * No overflow guard is provided, make sure there's at least 68 bytes available
 * (for base 2).
 *
 * Offers special support for base 2 (binary), base 8 (octal), base 10 and base
 * 16 (hex) where prefixes are automatically added if required (i.e.,`"0x"` for
 * hex and `"0b"` for base 2, and `"0"` for octal).
 *
 * Supports any base up to base 36 (using 0-9,A-Z).
 *
 * An unsupported base will log an error and print zero.
 *
 * Returns the number of bytes actually written (excluding the NUL terminator).
 */
SFUNC size_t fio_ltoa(char *dest, int64_t num, uint8_t base);

/**
 * A helper function that converts between a double to a string.
 *
 * No overflow guard is provided, make sure there's at least 130 bytes
 * available (for base 2).
 *
 * Supports base 2, base 10 and base 16. An unsupported base will silently
 * default to base 10. Prefixes aren't added (i.e., no "0x" or "0b" at the
 * beginning of the string).
 *
 * Returns the number of bytes actually written (excluding the NUL
 * terminator).
 */
SFUNC size_t fio_ftoa(char *dest, double num, uint8_t base);
/* *****************************************************************************
Strings to Numbers - Implementation
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

typedef struct {
  uint64_t val;
  int64_t expo;
  uint8_t sign;
} fio___number_s;

/**
 * Maps characters to alphanumerical value, where numbers have their natural
 * values (0-9) and `A-Z` (or `a-z`) are the values 10-35.
 *
 * Out of bound values return 255.
 *
 * This allows parsing of numeral strings for up to base 36.
 */
IFUNC uint8_t fio_c2i(unsigned char c) {
  static const uint8_t fio___alphanumerical_map[256] = {
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   255, 255,
      255, 255, 255, 255, 255, 10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
      20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
      35,  255, 255, 255, 255, 255, 255, 10,  11,  12,  13,  14,  15,  16,  17,
      18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
      33,  34,  35,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255};
  return fio___alphanumerical_map[c];
}

/**
 * Maps numeral values to alphanumerical characters, where numbers have their
 * natural values (0-9) and `A-Z` are the values 10-35.
 *
 * Accepts values up to 63. Returns zero for values over 35. Out of bound values
 * produce undefined behavior.
 *
 * This allows printing of numerals for up to base 36.
 */
IFUNC uint8_t fio_i2c(unsigned char i) {
  static const uint8_t fio___alphanumerical_map[64] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
      'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
      'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
  return fio___alphanumerical_map[i & 63];
}

/** Reads number information in base 2. Returned expo in base 2. */
FIO_IFUNC fio___number_s fio___aton_read_b2_b2(char **pstr) {
  fio___number_s r = (fio___number_s){0};
  const uint64_t mask = ((1ULL) << ((sizeof(mask) << 3) - 1));
  while (**pstr >= '0' && **pstr <= '1' && !(r.val & mask)) {
    r.val = (r.val << 1) | (**pstr - '0');
    ++(*pstr);
  }
  while (**pstr >= '0' && **pstr <= '1') {
    ++r.expo;
    ++(*pstr);
  }
  return r;
}

FIO_IFUNC fio___number_s fio___aton_read_b2_bX(char **pstr, uint8_t base) {
  fio___number_s r = (fio___number_s){0};
  const uint64_t limit = ((~0ULL) / base) - (base - 1);
  register uint8_t tmp;
  while ((tmp = fio_c2i(**pstr)) < base && r.val <= (limit)) {
    r.val = (r.val * base) + tmp;
    ++(*pstr);
  }
  while (fio_c2i(**pstr) < base) {
    ++r.expo;
    ++(*pstr);
  }
  return r;
}

/** Reads number information for base 16 (hex). Returned expo in base 4. */
FIO_IFUNC fio___number_s fio___aton_read_b2_b16(char **pstr) {
  fio___number_s r = (fio___number_s){0};
  const uint64_t mask = ~((~(uint64_t)0ULL) >> 4);
  for (; !(r.val & mask);) {
    uint8_t tmp = fio_c2i(**pstr);
    if (tmp > 15)
      return r;
    r.val = (r.val << 4) | tmp;
    ++(*pstr);
  }
  while ((fio_c2i(**pstr)) < 16)
    ++r.expo;
  return r;
}

SFUNC int64_t fio_atol(char **pstr) {
  if (!pstr || !(*pstr))
    return 0;
  char *p = *pstr;
  unsigned char invert = 0;
  fio___number_s n = (fio___number_s){0};

  while ((int)(unsigned char)isspace((unsigned char)*p))
    ++p;
  if (*p == '-') {
    invert = 1;
    ++p;
  } else if (*p == '+') {
    ++p;
  }
  switch (*p) {
  case 'x': /* fallthrough */
  case 'X':
    goto is_hex;
  case 'b': /* fallthrough */
  case 'B':
    goto is_binary;
  case '0':
    ++p;
    switch (*p) {
    case 'x': /* fallthrough */
    case 'X':
      goto is_hex;
    case 'b': /* fallthrough */
    case 'B':
      goto is_binary;
    }
    goto is_base8;
  }

  /* is_base10: */
  *pstr = p;
  n = fio___aton_read_b2_bX(pstr, 10);

  /* sign can't be embeded */
#define CALC_N_VAL()                                                           \
  if (invert) {                                                                \
    if (n.expo || ((n.val << 1) && (n.val >> ((sizeof(n.val) << 3) - 1)))) {   \
      errno = E2BIG;                                                           \
      return (int64_t)(1ULL << ((sizeof(n.val) << 3) - 1));                    \
    }                                                                          \
    n.val = 0 - n.val;                                                         \
  } else {                                                                     \
    if (n.expo || (n.val >> ((sizeof(n.val) << 3) - 1))) {                     \
      errno = E2BIG;                                                           \
      return (int64_t)((~0ULL) >> 1);                                          \
    }                                                                          \
  }

  CALC_N_VAL();
  return n.val;

is_hex:
  ++p;
  while (*p == '0') {
    ++p;
  }
  *pstr = p;
  n = fio___aton_read_b2_b16(pstr);

  /* sign can be embeded */
#define CALC_N_VAL_EMBEDABLE()                                                 \
  if (invert) {                                                                \
    if (n.expo) {                                                              \
      errno = E2BIG;                                                           \
      return (int64_t)(1ULL << ((sizeof(n.val) << 3) - 1));                    \
    }                                                                          \
    n.val = 0 - n.val;                                                         \
  } else {                                                                     \
    if (n.expo) {                                                              \
      errno = E2BIG;                                                           \
      return (int64_t)((~0ULL) >> 1);                                          \
    }                                                                          \
  }

  CALC_N_VAL_EMBEDABLE();
  return n.val;

is_binary:
  ++p;
  while (*p == '0') {
    ++p;
  }
  *pstr = p;
  n = fio___aton_read_b2_b2(pstr);
  CALC_N_VAL_EMBEDABLE()
  return n.val;

is_base8:
  while (*p == '0') {
    ++p;
  }
  *pstr = p;
  n = fio___aton_read_b2_bX(pstr, 8);
  CALC_N_VAL();
  return n.val;
}

SFUNC double fio_atof(char **pstr) {
  if (!pstr || !(*pstr))
    return 0;
  if ((*pstr)[1] == 'b' || ((*pstr)[1] == '0' && (*pstr)[1] == 'b'))
    goto binary_raw;
  return strtod(*pstr, pstr);
binary_raw:
  /* binary representation is assumed to spell an exact double */
  (void)0;
  union {
    uint64_t i;
    double d;
  } punned = {.i = (uint64_t)fio_atol(pstr)};
  return punned.d;
}

/* *****************************************************************************
Numbers to Strings - Implementation
***************************************************************************** */

SFUNC size_t fio_ltoa(char *dest, int64_t num, uint8_t base) {
  size_t len = 0;
  char buf[48]; /* we only need up to 20 for base 10, but base 3 needs 41... */

  if (!num)
    goto zero;
  if (base > 36)
    goto base_error;

  switch (base) {
  case 1: /* fallthrough */
  case 2:
    /* Base 2 */
    {
      uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
      uint8_t i = 0;    /* counting bits */
      dest[len++] = '0';
      dest[len++] = 'b';
#if __has_builtin(__builtin_clzll)
      i = __builtin_clzll(n);
      /* make sure the Binary representation doesn't appear signed */
      if (i) {
        --i;
        /*  keep it even */
        if ((i & 1))
          --i;
        n <<= i;
      }
#else
      while ((i < 64) && (n & 0x8000000000000000ULL) == 0) {
        n <<= 1;
        i++;
      }
      /* make sure the Binary representation doesn't appear signed */
      if (i) {
        --i;
        n = n >> 1;
        /*  keep it even */
        if ((i & 1)) {
          --i;
          n = n >> 1;
        }
      }
#endif
      /* write to dest. */
      while (i < 64) {
        dest[len++] = ((n & 0x8000000000000000ULL) ? '1' : '0');
        n = n << 1;
        i++;
      }
      dest[len] = 0;
      return len;
    }
  case 8:
    /* Base 8 */
    {
      uint64_t l = 0;
      if (num < 0) {
        dest[len++] = '-';
        num = 0 - num;
      }
      dest[len++] = '0';

      while (num) {
        buf[l++] = '0' + (num & 7);
        num = num >> 3;
      }
      while (l) {
        --l;
        dest[len++] = buf[l];
      }
      dest[len] = 0;
      return len;
    }

  case 16:
    /* Base 16 */
    {
      uint64_t n = num; /* avoid bit shifting inconsistencies with signed bit */
      uint8_t i = 0;    /* counting bits */
      dest[len++] = '0';
      dest[len++] = 'x';
      while ((n & 0xFF00000000000000ULL) == 0) { // since n != 0, then i < 8
        n = n << 8;
        i++;
      }
      /* make sure the Hex representation doesn't appear misleadingly signed. */
      if (i && (n & 0x8000000000000000ULL) && (n & 0x00FFFFFFFFFFFFFFULL)) {
        dest[len++] = '0';
        dest[len++] = '0';
      }
      /* write the damn thing, high to low */
      while (i < 8) {
        uint8_t tmp = (n & 0xF000000000000000ULL) >> 60;
        uint8_t tmp2 = (n & 0x0F00000000000000ULL) >> 56;
        dest[len++] = fio_i2c(tmp);
        dest[len++] = fio_i2c(tmp2);
        i++;
        n = n << 8;
      }
      dest[len] = 0;
      return len;
    }
  case 0: /* fallthrough */
  case 10:
    /* Base 10 */
    {
      int64_t t = num / 10;
      uint64_t l = 0;
      if (num < 0) {
        num = 0 - num; /* might fail due to overflow, but fixed with tail
        (t) */
        t = (int64_t)0 - t;
        dest[len++] = '-';
      }
      while (num) {
        buf[l++] = '0' + (num - (t * 10));
        num = t;
        t = num / 10;
      }
      while (l) {
        --l;
        dest[len++] = buf[l];
      }
      dest[len] = 0;
      return len;
    }

  default:
    /* any base up to base 36 */
    {
      int64_t t = num / base;
      uint64_t l = 0;
      if (num < 0) {
        num = 0 - num; /* might fail due to overflow, but fixed with tail (t) */
        t = (int64_t)0 - t;
        dest[len++] = '-';
      }
      while (num) {
        buf[l++] = fio_i2c(num - (t * base));
        num = t;
        t = num / base;
      }
      while (l) {
        --l;
        dest[len++] = buf[l];
      }
      dest[len] = 0;
      return len;
    }
  }

base_error:
  FIO_LOG_ERROR("fio_ltoa base out of range");
zero:
  switch (base) {
  case 1:
  case 2:
    dest[len++] = '0';
    dest[len++] = 'b';
    break;
  case 8:
    dest[len++] = '0';
    break;
  case 16:
    dest[len++] = '0';
    dest[len++] = 'x';
    dest[len++] = '0';
    break;
  }
  dest[len++] = '0';
  dest[len] = 0;
  return len;
}

SFUNC size_t fio_ftoa(char *dest, double num, uint8_t base) {
  if (base == 2 || base == 16) {
    /* handle binary / Hex representation the same as an int64_t */
    /* FIXME: Hex representation should use floating-point hex instead */
    union {
      int64_t i;
      double d;
    } p;
    p.d = num;
    return fio_ltoa(dest, p.i, base);
  }
  size_t written = 0;
  uint8_t need_zero = 1;
  char *start = dest;

  if (isinf(num))
    goto is_inifinity;
  if (isnan(num))
    goto is_nan;

  written = sprintf(dest, "%g", num);
  while (*start) {
    if (*start == 'e')
      goto finish;
    if (*start == ',') // locale issues?
      *start = '.';
    if (*start == '.') {
      need_zero = 0;
    }
    start++;
  }
  if (need_zero) {
    dest[written++] = '.';
    dest[written++] = '0';
  }

finish:
  dest[written] = 0;
  return written;

is_inifinity:
  if (num < 0)
    dest[written++] = '-';
  FIO_MEMCPY(dest + written, "Infinity", 9);
  return written + 8;
is_nan:
  FIO_MEMCPY(dest, "NaN", 4);
  return 3;
}

/* *****************************************************************************
Numbers <=> Strings - Testing
***************************************************************************** */

#ifdef FIO_TEST_CSTL

#define FIO_ATOL_TEST_MAX 1048576

FIO_IFUNC int64_t FIO_NAME_TEST(stl, atol_time)(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return ((int64_t)t.tv_sec * 1000000) + (int64_t)t.tv_nsec / 1000;
}

FIO_SFUNC void FIO_NAME_TEST(stl, atol_speed)(const char *name,
                                              int64_t (*a2l)(char **),
                                              size_t (*l2a)(char *,
                                                            int64_t,
                                                            uint8_t)) {
  int64_t start;
  int64_t tw = 0;
  int64_t trt = 0;
  char buf[1024];
  struct {
    const char *str;
    const char *prefix;
    uint8_t prefix_len;
    uint8_t base;
  } * pb, b[] = {
              {.str = "Base 10", .base = 10},
              {.str = "Hex    ", .prefix = "0x", .prefix_len = 2, .base = 16},
              // {.str = "Binary ", .prefix = "0b", .prefix_len = 2, .base = 2},
              // {.str = "Oct    ", .prefix = "0", .prefix_len = 1, .base = 8},
              /* end marker */
              {.str = NULL},
          };
  fprintf(stderr, "    * %s test performance:\n", name);
  for (pb = b; pb->str; ++pb) {
    start = FIO_NAME_TEST(stl, atol_time)();
    for (int64_t i = -FIO_ATOL_TEST_MAX; i < FIO_ATOL_TEST_MAX; ++i) {
      char *bf = buf + pb->prefix_len;
      size_t len = l2a(bf, i, pb->base);
      bf[len] = 0;
      if (bf[0] == '-') {
        for (int pre_test = 0; pre_test < pb->prefix_len; ++pre_test) {
          if (bf[pre_test + 1] == pb->prefix[pre_test])
            continue;
          FIO_MEMCPY(buf, pb->prefix, pb->prefix_len);
          bf = buf;
          break;
        }
      } else {
        for (int pre_test = 0; pre_test < pb->prefix_len; ++pre_test) {
          if (bf[pre_test] == pb->prefix[pre_test])
            continue;
          FIO_MEMCPY(buf, pb->prefix, pb->prefix_len);
          bf = buf;
          break;
        }
      }
      FIO_COMPILER_GUARD; /* don't optimize this loop */
      int64_t n = a2l(&bf);
      bf = buf;
      FIO_ASSERT(n == i,
                 "roundtrip error for %s: %s != %lld (got %lld)",
                 name,
                 buf,
                 i,
                 a2l(&bf));
    }
    trt = FIO_NAME_TEST(stl, atol_time)() - start;
    start = FIO_NAME_TEST(stl, atol_time)();
    for (int64_t i = -FIO_ATOL_TEST_MAX; i < FIO_ATOL_TEST_MAX; ++i) {
      char *bf = buf + pb->prefix_len;
      size_t len = l2a(bf, i, pb->base);
      bf[len] = 0;
      if (bf[0] == '-') {
        for (int pre_test = 0; pre_test < pb->prefix_len; ++pre_test) {
          if (bf[pre_test + 1] == pb->prefix[pre_test])
            continue;
          FIO_MEMCPY(buf, pb->prefix, pb->prefix_len);
          bf = buf;
          break;
        }
      } else {
        for (int pre_test = 0; pre_test < pb->prefix_len; ++pre_test) {
          if (bf[pre_test] == pb->prefix[pre_test])
            continue;
          FIO_MEMCPY(buf, pb->prefix, pb->prefix_len);
          bf = buf;
          break;
        }
      }
      FIO_COMPILER_GUARD; /* don't optimize this loop */
    }
    tw = FIO_NAME_TEST(stl, atol_time)() - start;
    // clang-format off
    fprintf(stderr, "        - %s roundtrip   %zd us\n", pb->str, (size_t)trt);
    fprintf(stderr, "        - %s write       %zd us\n", pb->str, (size_t)tw);
    fprintf(stderr, "        - %s read (calc) %zd us\n", pb->str, (size_t)(trt - tw));
    // clang-format on
  }
}

SFUNC size_t sprintf_wrapper(char *dest, int64_t num, uint8_t base) {
  switch (base) {
  case 2: /* overflow - unsupported */
  case 8: /* overflow - unsupported */
  case 10:
    return sprintf(dest, "%" PRId64, num);
  case 16:
    if (num >= 0)
      return sprintf(dest, "0x%.16" PRIx64, num);
    return sprintf(dest, "-0x%.8" PRIx64, (0 - num));
  }
  return sprintf(dest, "%" PRId64, num);
}

SFUNC int64_t strtoll_wrapper(char **pstr) { return strtoll(*pstr, pstr, 0); }

FIO_SFUNC void FIO_NAME_TEST(stl, atol)(void) {
  fprintf(stderr, "* Testing fio_atol and fio_ltoa.\n");
  char buffer[1024];
  for (int i = 0 - FIO_ATOL_TEST_MAX; i < FIO_ATOL_TEST_MAX; ++i) {
    size_t tmp = fio_ltoa(buffer, i, 0);
    FIO_ASSERT(tmp > 0, "fio_ltoa returned length error");
    buffer[tmp++] = 0;
    char *tmp2 = buffer;
    int i2 = fio_atol(&tmp2);
    FIO_ASSERT(tmp2 > buffer, "fio_atol pointer motion error");
    FIO_ASSERT(i == i2,
               "fio_ltoa-fio_atol roundtrip error %lld != %lld",
               i,
               i2);
  }
  for (size_t bit = 0; bit < sizeof(int64_t) * 8; ++bit) {
    uint64_t i = (uint64_t)1 << bit;
    size_t tmp = fio_ltoa(buffer, (int64_t)i, 0);
    FIO_ASSERT(tmp > 0, "fio_ltoa return length error");
    buffer[tmp] = 0;
    char *tmp2 = buffer;
    int64_t i2 = fio_atol(&tmp2);
    FIO_ASSERT(tmp2 > buffer, "fio_atol pointer motion error");
    FIO_ASSERT((int64_t)i == i2,
               "fio_ltoa-fio_atol roundtrip error %lld != %lld",
               i,
               i2);
  }
  for (unsigned char i = 0; i < 36; ++i) {
    FIO_ASSERT(i == fio_c2i(fio_i2c(i)), "fio_c2i / fio_i2c roundtrip error.")
  }
  fprintf(stderr, "* Testing fio_atol samples.\n");
#define TEST_ATOL(s_, n)                                                       \
  do {                                                                         \
    char *s = (char *)s_;                                                      \
    char *p = (char *)(s);                                                     \
    int64_t r = fio_atol(&p);                                                  \
    FIO_ASSERT(r == (n),                                                       \
               "fio_atol test error! %s => %zd (not %zd)",                     \
               ((char *)(s)),                                                  \
               (size_t)r,                                                      \
               (size_t)n);                                                     \
    FIO_ASSERT((s) + strlen((s)) == p,                                         \
               "fio_atol test error! %s reading position not at end "          \
               "(!%zu == %zu)\n\t0x%p - 0x%p",                                 \
               (s),                                                            \
               (size_t)strlen((s)),                                            \
               (size_t)(p - (s)),                                              \
               (void *)p,                                                      \
               (void *)s);                                                     \
    char buf[72];                                                              \
    buf[fio_ltoa(buf, n, 2)] = 0;                                              \
    p = buf;                                                                   \
    FIO_ASSERT(fio_atol(&p) == (n),                                            \
               "fio_ltoa base 2 test error! "                                  \
               "%s != %s (%zd)",                                               \
               buf,                                                            \
               ((char *)(s)),                                                  \
               (size_t)((p = buf), fio_atol(&p)));                             \
    buf[fio_ltoa(buf, n, 8)] = 0;                                              \
    p = buf;                                                                   \
    FIO_ASSERT(fio_atol(&p) == (n),                                            \
               "fio_ltoa base 8 test error! "                                  \
               "%s != %s (%zd)",                                               \
               buf,                                                            \
               ((char *)(s)),                                                  \
               (size_t)((p = buf), fio_atol(&p)));                             \
    buf[fio_ltoa(buf, n, 10)] = 0;                                             \
    p = buf;                                                                   \
    FIO_ASSERT(fio_atol(&p) == (n),                                            \
               "fio_ltoa base 10 test error! "                                 \
               "%s != %s (%zd)",                                               \
               buf,                                                            \
               ((char *)(s)),                                                  \
               (size_t)((p = buf), fio_atol(&p)));                             \
    buf[fio_ltoa(buf, n, 16)] = 0;                                             \
    p = buf;                                                                   \
    FIO_ASSERT(fio_atol(&p) == (n),                                            \
               "fio_ltoa base 16 test error! "                                 \
               "%s != %s (%zd)",                                               \
               buf,                                                            \
               ((char *)(s)),                                                  \
               (size_t)((p = buf), fio_atol(&p)));                             \
  } while (0)

  TEST_ATOL("0x1", 1);
  TEST_ATOL("-0x1", -1);
  TEST_ATOL("-0xa", -10);                                  /* sign before hex */
  TEST_ATOL("0xe5d4c3b2a1908770", -1885667171979196560LL); /* sign within hex */
  TEST_ATOL("0b00000000000011", 3);
  TEST_ATOL("-0b00000000000011", -3);
  TEST_ATOL("0b0000000000000000000000000000000000000000000000000", 0);
  TEST_ATOL("0", 0);
  TEST_ATOL("1", 1);
  TEST_ATOL("2", 2);
  TEST_ATOL("-2", -2);
  TEST_ATOL("0000000000000000000000000000000000000000000000042", 34); /* oct */
  TEST_ATOL("9223372036854775807", 9223372036854775807LL); /* INT64_MAX */
  TEST_ATOL("9223372036854775808",
            9223372036854775807LL); /* INT64_MAX overflow protection */
  TEST_ATOL("9223372036854775999",
            9223372036854775807LL); /* INT64_MAX overflow protection */
  TEST_ATOL("9223372036854775806",
            9223372036854775806LL); /* almost INT64_MAX */
#undef TEST_ATOL

  FIO_NAME_TEST(stl, atol_speed)("fio_atol/fio_ltoa", fio_atol, fio_ltoa);
  FIO_NAME_TEST(stl, atol_speed)
  ("system strtoll/sprintf", strtoll_wrapper, sprintf_wrapper);

#ifdef FIO_ATOF_ALT
#define TEST_DOUBLE(s, d, stop)                                                \
  do {                                                                         \
    union {                                                                    \
      double d_;                                                               \
      uint64_t as_i;                                                           \
    } pn, pn2;                                                                 \
    pn2.d_ = d;                                                                \
    char *p = (char *)(s);                                                     \
    char *p2 = (char *)(s);                                                    \
    double r = fio_atof(&p);                                                   \
    double std = strtod(p2, &p2);                                              \
    (void)std;                                                                 \
    pn.d_ = r;                                                                 \
    FIO_ASSERT(*p == stop || p == p2,                                          \
               "float parsing didn't stop at correct possition! %x != %x",     \
               *p,                                                             \
               stop);                                                          \
    if ((double)d == r || r == std) {                                          \
      /** fprintf(stderr, "Okay for %s\n", s); */                              \
    } else if ((pn2.as_i + 1) == (pn.as_i) || (pn.as_i + 1) == pn2.as_i) {     \
      fprintf(stderr,                                                          \
              "* WARNING: Single bit rounding error detected: %s\n",           \
              s);                                                              \
    } else if (r == 0.0 && d != 0.0) {                                         \
      fprintf(stderr, "* WARNING: float range limit marked before: %s\n", s);  \
    } else {                                                                   \
      char f_buf[164];                                                         \
      pn.d_ = std;                                                             \
      pn2.d_ = r;                                                              \
      size_t tmp_pos = fio_ltoa(f_buf, pn.as_i, 2);                            \
      f_buf[tmp_pos] = '\n';                                                   \
      fio_ltoa(f_buf + tmp_pos + 1, pn2.as_i, 2);                              \
      FIO_ASSERT(0,                                                            \
                 "Float error bigger than a single bit rounding error. exp. "  \
                 "vs. act.:\n%.19g\n%.19g\nBinary:\n%s",                       \
                 std,                                                          \
                 r,                                                            \
                 f_buf);                                                       \
    }                                                                          \
  } while (0)

  fprintf(stderr, "* Testing fio_atof samples.\n");

  /* A few hex-float examples  */
  TEST_DOUBLE("0x10.1p0", 0x10.1p0, 0);
  TEST_DOUBLE("0x1.8p1", 0x1.8p1, 0);
  TEST_DOUBLE("0x1.8p5", 0x1.8p5, 0);
  TEST_DOUBLE("0x4.0p5", 0x4.0p5, 0);
  TEST_DOUBLE("0x1.0p50a", 0x1.0p50, 'a');
  TEST_DOUBLE("0x1.0p500", 0x1.0p500, 0);
  TEST_DOUBLE("0x1.0P-1074", 0x1.0P-1074, 0);
  TEST_DOUBLE("0x3a.0P-1074", 0x3a.0P-1074, 0);

  /* These numbers were copied from https://gist.github.com/mattn/1890186 */
  TEST_DOUBLE(".1", 0.1, 0);
  TEST_DOUBLE("  .", 0, 0);
  TEST_DOUBLE("  1.2e3", 1.2e3, 0);
  TEST_DOUBLE(" +1.2e3", 1.2e3, 0);
  TEST_DOUBLE("1.2e3", 1.2e3, 0);
  TEST_DOUBLE("+1.2e3", 1.2e3, 0);
  TEST_DOUBLE("+1.e3", 1000, 0);
  TEST_DOUBLE("-1.2e3", -1200, 0);
  TEST_DOUBLE("-1.2e3.5", -1200, '.');
  TEST_DOUBLE("-1.2e", -1.2, 0);
  TEST_DOUBLE("--1.2e3.5", 0, '-');
  TEST_DOUBLE("--1-.2e3.5", 0, '-');
  TEST_DOUBLE("-a", 0, 'a');
  TEST_DOUBLE("a", 0, 'a');
  TEST_DOUBLE(".1e", 0.1, 0);
  TEST_DOUBLE(".1e3", 100, 0);
  TEST_DOUBLE(".1e-3", 0.1e-3, 0);
  TEST_DOUBLE(".1e-", 0.1, 0);
  TEST_DOUBLE(" .e-", 0, 0);
  TEST_DOUBLE(" .e", 0, 0);
  TEST_DOUBLE(" e", 0, 0);
  TEST_DOUBLE(" e0", 0, 0);
  TEST_DOUBLE(" ee", 0, 'e');
  TEST_DOUBLE(" -e", 0, 0);
  TEST_DOUBLE(" .9", 0.9, 0);
  TEST_DOUBLE(" ..9", 0, '.');
  TEST_DOUBLE("009", 9, 0);
  TEST_DOUBLE("0.09e02", 9, 0);
  /* http://thread.gmane.org/gmane.editors.vim.devel/19268/ */
  TEST_DOUBLE("0.9999999999999999999999999999999999", 1, 0);
  TEST_DOUBLE("2.2250738585072010e-308", 2.225073858507200889e-308, 0);
  TEST_DOUBLE("2.2250738585072013e-308", 2.225073858507201383e-308, 0);
  TEST_DOUBLE("9214843084008499", 9214843084008499, 0);
  TEST_DOUBLE("30078505129381147446200", 3.007850512938114954e+22, 0);

  /* These numbers were copied from https://github.com/miloyip/rapidjson */
  TEST_DOUBLE("0.0", 0.0, 0);
  TEST_DOUBLE("-0.0", -0.0, 0);
  TEST_DOUBLE("1.0", 1.0, 0);
  TEST_DOUBLE("-1.0", -1.0, 0);
  TEST_DOUBLE("1.5", 1.5, 0);
  TEST_DOUBLE("-1.5", -1.5, 0);
  TEST_DOUBLE("3.1416", 3.1416, 0);
  TEST_DOUBLE("1E10", 1E10, 0);
  TEST_DOUBLE("1e10", 1e10, 0);
  TEST_DOUBLE("100000000000000000000000000000000000000000000000000000000000"
              "000000000000000000000",
              1E80,
              0);
  TEST_DOUBLE("1E+10", 1E+10, 0);
  TEST_DOUBLE("1E-10", 1E-10, 0);
  TEST_DOUBLE("-1E10", -1E10, 0);
  TEST_DOUBLE("-1e10", -1e10, 0);
  TEST_DOUBLE("-1E+10", -1E+10, 0);
  TEST_DOUBLE("-1E-10", -1E-10, 0);
  TEST_DOUBLE("1.234E+10", 1.234E+10, 0);
  TEST_DOUBLE("1.234E-10", 1.234E-10, 0);
  TEST_DOUBLE("1.79769e+308", 1.79769e+308, 0);
  TEST_DOUBLE("2.22507e-308", 2.22507e-308, 0);
  TEST_DOUBLE("-1.79769e+308", -1.79769e+308, 0);
  TEST_DOUBLE("-2.22507e-308", -2.22507e-308, 0);
  TEST_DOUBLE("4.9406564584124654e-324", 4.9406564584124654e-324, 0);
  TEST_DOUBLE("2.2250738585072009e-308", 2.2250738585072009e-308, 0);
  TEST_DOUBLE("2.2250738585072014e-308", 2.2250738585072014e-308, 0);
  TEST_DOUBLE("1.7976931348623157e+308", 1.7976931348623157e+308, 0);
  TEST_DOUBLE("1e-10000", 0.0, 0);
  TEST_DOUBLE("18446744073709551616", 18446744073709551616.0, 0);

  TEST_DOUBLE("-9223372036854775809", -9223372036854775809.0, 0);

  TEST_DOUBLE("0.9868011474609375", 0.9868011474609375, 0);
  TEST_DOUBLE("123e34", 123e34, 0);
  TEST_DOUBLE("45913141877270640000.0", 45913141877270640000.0, 0);
  TEST_DOUBLE("2.2250738585072011e-308", 2.2250738585072011e-308, 0);
  TEST_DOUBLE("1e-214748363", 0.0, 0);
  TEST_DOUBLE("1e-214748364", 0.0, 0);
  TEST_DOUBLE("0.017976931348623157e+310, 1", 1.7976931348623157e+308, ',');

  TEST_DOUBLE("2.2250738585072012e-308", 2.2250738585072014e-308, 0);
  TEST_DOUBLE("2.22507385850720113605740979670913197593481954635164565e-308",
              2.2250738585072014e-308,
              0);

  TEST_DOUBLE("0.999999999999999944488848768742172978818416595458984375",
              1.0,
              0);
  TEST_DOUBLE("0.999999999999999944488848768742172978818416595458984376",
              1.0,
              0);
  TEST_DOUBLE("1.00000000000000011102230246251565404236316680908203125",
              1.0,
              0);
  TEST_DOUBLE("1.00000000000000011102230246251565404236316680908203124",
              1.0,
              0);

  TEST_DOUBLE("72057594037927928.0", 72057594037927928.0, 0);
  TEST_DOUBLE("72057594037927936.0", 72057594037927936.0, 0);
  TEST_DOUBLE("72057594037927932.0", 72057594037927936.0, 0);
  TEST_DOUBLE("7205759403792793200001e-5", 72057594037927936.0, 0);

  TEST_DOUBLE("9223372036854774784.0", 9223372036854774784.0, 0);
  TEST_DOUBLE("9223372036854775808.0", 9223372036854775808.0, 0);
  TEST_DOUBLE("9223372036854775296.0", 9223372036854775808.0, 0);
  TEST_DOUBLE("922337203685477529600001e-5", 9223372036854775808.0, 0);

  TEST_DOUBLE("10141204801825834086073718800384",
              10141204801825834086073718800384.0,
              0);
  TEST_DOUBLE("10141204801825835211973625643008",
              10141204801825835211973625643008.0,
              0);
  TEST_DOUBLE("10141204801825834649023672221696",
              10141204801825835211973625643008.0,
              0);
  TEST_DOUBLE("1014120480182583464902367222169600001e-5",
              10141204801825835211973625643008.0,
              0);

  TEST_DOUBLE("5708990770823838890407843763683279797179383808",
              5708990770823838890407843763683279797179383808.0,
              0);
  TEST_DOUBLE("5708990770823839524233143877797980545530986496",
              5708990770823839524233143877797980545530986496.0,
              0);
  TEST_DOUBLE("5708990770823839207320493820740630171355185152",
              5708990770823839524233143877797980545530986496.0,
              0);
  TEST_DOUBLE("5708990770823839207320493820740630171355185152001e-3",
              5708990770823839524233143877797980545530986496.0,
              0);
#undef TEST_DOUBLE
#if !DEBUG
  {
    clock_t start, stop;
    FIO_MEMCPY(buffer, "1234567890.123", 14);
    buffer[14] = 0;
    size_t r = 0;
    start = clock();
    for (int i = 0; i < (FIO_ATOL_TEST_MAX << 3); ++i) {
      char *pos = buffer;
      r += fio_atol(&pos);
      FIO_COMPILER_GUARD;
      // FIO_ASSERT(r == exp, "fio_atol failed during speed test");
    }
    stop = clock();
    fprintf(stderr,
            "* fio_atol speed test completed in %zu cycles\n",
            stop - start);
    r = 0;

    start = clock();
    for (int i = 0; i < (FIO_ATOL_TEST_MAX << 3); ++i) {
      char *pos = buffer;
      r += strtol(pos, NULL, 10);
      FIO_COMPILER_GUARD;
      // FIO_ASSERT(r == exp, "system strtol failed during speed test");
    }
    stop = clock();
    fprintf(stderr,
            "* system atol speed test completed in %zu cycles\n",
            stop - start);
  }
#endif /* !DEBUG */
#endif /* FIO_ATOF_ALT */
}
#undef FIO_ATOL_TEST_MAX
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
Numbers <=> Strings - Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_ATOL */
#undef FIO_ATOL
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_THREADS                 /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                        Simple Portable Threads




***************************************************************************** */
#if defined(FIO_THREADS) && !defined(H___FIO_THREADS___H)
#define H___FIO_THREADS___H

/* *****************************************************************************
Module Settings

At this point, define any MACROs and customaizable settings avsailable to the
developer.
***************************************************************************** */

#if FIO_OS_POSIX
#include <pthread.h>
#include <sched.h>
typedef pthread_t fio_thread_t;
typedef pthread_mutex_t fio_thread_mutex_t;
/** Used this macro for static initialization. */
#define FIO_THREAD_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER

#elif FIO_OS_WIN
#include <synchapi.h>
typedef HANDLE fio_thread_t;
typedef HANDLE fio_thread_mutex_t;
/** Used this macro for static initialization. */
#define FIO_THREAD_MUTEX_INIT ((fio_thread_mutex_t)0)
#else
#error facil.io Simple Portable Threads require a POSIX system or Windows
#endif

#ifdef FIO_THREADS_BYO
#define FIO_IFUNC_T
#else
#define FIO_IFUNC_T FIO_IFUNC
#endif

#ifdef FIO_THREADS_MUTEX_BYO
#define FIO_IFUNC_M
#else
#define FIO_IFUNC_M FIO_IFUNC
#endif

/* *****************************************************************************
Module API
***************************************************************************** */

/** Starts a new thread, returns 0 on success and -1 on failure. */
FIO_IFUNC_T int fio_thread_create(fio_thread_t *t,
                                  void *(*fn)(void *),
                                  void *arg);

/** Waits for the thread to finish. */
FIO_IFUNC_T int fio_thread_join(fio_thread_t t);

/** Detaches the thread, so thread resources are freed automatically. */
FIO_IFUNC_T int fio_thread_detach(fio_thread_t t);

/** Ends the current running thread. */
FIO_IFUNC_T void fio_thread_exit(void);

/* Returns non-zero if both threads refer to the same thread. */
FIO_IFUNC_T int fio_thread_equal(fio_thread_t a, fio_thread_t b);

/** Returns the current thread. */
FIO_IFUNC_T fio_thread_t fio_thread_current(void);

/** Yields thread execution. */
FIO_IFUNC_T void fio_thread_yield(void);

/**
 * Initializes a simple Mutex.
 *
 * Or use the static initialization value: FIO_THREAD_MUTEX_INIT
 */
FIO_IFUNC_M int fio_thread_mutex_init(fio_thread_mutex_t *m);

/** Locks a simple Mutex, returning -1 on error. */
FIO_IFUNC_M int fio_thread_mutex_lock(fio_thread_mutex_t *m);

/** Attempts to lock a simple Mutex, returning zero on success. */
FIO_IFUNC_M int fio_thread_mutex_trylock(fio_thread_mutex_t *m);

/** Unlocks a simple Mutex, returning zero on success or -1 on error. */
FIO_IFUNC_M int fio_thread_mutex_unlock(fio_thread_mutex_t *m);

/** Destroys the simple Mutex (cleanup). */
FIO_IFUNC_M void fio_thread_mutex_destroy(fio_thread_mutex_t *m);

/* *****************************************************************************
POSIX Implementation - inlined static functions
***************************************************************************** */
#if FIO_OS_POSIX
#ifndef FIO_THREADS_BYO
// clang-format off
/** Starts a new thread, returns 0 on success and -1 on failure. */
FIO_IFUNC int fio_thread_create(fio_thread_t *t, void *(*fn)(void *), void *arg) { return pthread_create(t, NULL, fn, arg); }

FIO_IFUNC int fio_thread_join(fio_thread_t t) { return pthread_join(t, NULL); }

/** Detaches the thread, so thread resources are freed automatically. */
FIO_IFUNC int fio_thread_detach(fio_thread_t t) { return pthread_detach(t); }

/** Ends the current running thread. */
FIO_IFUNC void fio_thread_exit(void) { pthread_exit(NULL); }

/* Returns non-zero if both threads refer to the same thread. */
FIO_IFUNC int fio_thread_equal(fio_thread_t a, fio_thread_t b) { return pthread_equal(a, b); }

/** Returns the current thread. */
FIO_IFUNC fio_thread_t fio_thread_current(void) { return pthread_self(); }

/** Yields thread execution. */
FIO_IFUNC void fio_thread_yield(void) { sched_yield(); }

#endif /* FIO_THREADS_BYO */
#ifndef FIO_THREADS_MUTEX_BYO

/** Initializes a simple Mutex. */
FIO_IFUNC int fio_thread_mutex_init(fio_thread_mutex_t *m) { return pthread_mutex_init(m, NULL); }

/** Locks a simple Mutex, returning -1 on error. */
FIO_IFUNC int fio_thread_mutex_lock(fio_thread_mutex_t *m) { return pthread_mutex_lock(m); }

/** Attempts to lock a simple Mutex, returning zero on success. */
FIO_IFUNC int fio_thread_mutex_trylock(fio_thread_mutex_t *m) { return pthread_mutex_trylock(m); }

/** Unlocks a simple Mutex, returning zero on success or -1 on error. */
FIO_IFUNC int fio_thread_mutex_unlock(fio_thread_mutex_t *m) { return pthread_mutex_unlock(m); }

/** Destroys the simple Mutex (cleanup). */
FIO_IFUNC void fio_thread_mutex_destroy(fio_thread_mutex_t *m) { pthread_mutex_destroy(m); *m = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; }

#endif /* FIO_THREADS_MUTEX_BYO */
// clang-format on
/* *****************************************************************************
Windows Implementation - inlined static functions
***************************************************************************** */
#elif FIO_OS_WIN
#include <process.h>
#ifndef FIO_THREADS_BYO

// clang-format off
/** Starts a new thread, returns 0 on success and -1 on failure. */
FIO_IFUNC int fio_thread_create(fio_thread_t *t, void *(*fn)(void *), void *arg) { *t = (HANDLE)_beginthreadex(NULL, 0, (unsigned int (*)(void *))(uintptr_t)fn, arg, 0, NULL); return (!!t) - 1; }

FIO_IFUNC int fio_thread_join(fio_thread_t t) {
  int r = 0;
  if (WaitForSingleObject(t, INFINITE) == WAIT_FAILED) {
    errno = GetLastError();
    r = -1;
  } else
    CloseHandle(t);
  return r;
}

/** Detaches the thread, so thread resources are freed automatically. */
FIO_IFUNC int fio_thread_detach(fio_thread_t t) { return CloseHandle(t) - 1; }

/** Ends the current running thread. */
FIO_IFUNC void fio_thread_exit(void) { _endthread(); }

/* Returns non-zero if both threads refer to the same thread. */
FIO_IFUNC int fio_thread_equal(fio_thread_t a, fio_thread_t b) { return GetThreadId(a) == GetThreadId(b); }

/** Returns the current thread. */
FIO_IFUNC fio_thread_t fio_thread_current(void) { return GetCurrentThread(); }

/** Yields thread execution. */
FIO_IFUNC void fio_thread_yield(void) { Sleep(0); }

#endif /* FIO_THREADS_BYO */
#ifndef FIO_THREADS_MUTEX_BYO

SFUNC int fio___thread_mutex_lazy_init(fio_thread_mutex_t *m);

FIO_IFUNC int fio_thread_mutex_init(fio_thread_mutex_t *m) { return ((*m = CreateMutexW(NULL, FALSE, NULL)) != NULL) - 1; }

/** Unlocks a simple Mutex, returning zero on success or -1 on error. */
FIO_IFUNC int fio_thread_mutex_unlock(fio_thread_mutex_t *m) { return ((m && *m) ? ReleaseMutex(*m) : 0) -1; }

/** Destroys the simple Mutex (cleanup). */
FIO_IFUNC void fio_thread_mutex_destroy(fio_thread_mutex_t *m) { CloseHandle(*m); *m = FIO_THREAD_MUTEX_INIT; }

// clang-format on

/** Locks a simple Mutex, returning -1 on error. */
FIO_IFUNC int fio_thread_mutex_lock(fio_thread_mutex_t *m) {
  if (!*m && fio___thread_mutex_lazy_init(m))
    return -1;
  return (WaitForSingleObject((*m), INFINITE) == WAIT_OBJECT_0) - 1;
}

/** Attempts to lock a simple Mutex, returning zero on success. */
FIO_IFUNC int fio_thread_mutex_trylock(fio_thread_mutex_t *m) {
  if (!*m && fio___thread_mutex_lazy_init(m))
    return -1;
  return (WaitForSingleObject((*m), 0) == WAIT_OBJECT_0) - 1;
}
#endif
#endif /* FIO_THREADS_MUTEX_BYO */

/* *****************************************************************************
Module Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE
#if FIO_OS_WIN
#ifndef FIO_THREADS_MUTEX_BYO
/** Initializes a simple Mutex */
SFUNC int fio___thread_mutex_lazy_init(fio_thread_mutex_t *m) {
  static fio_lock_i lock = FIO_LOCK_INIT;
  /* lazy initialization */
  fio_lock(&lock);
  if (!*m) { /* retest, as this may chave changed... */
    *m = CreateMutexW(NULL, FALSE, NULL);
  }
  fio_unlock(&lock);
  return (!!m) - 1;
}
#endif /* FIO_THREADS_MUTEX_BYO */
#endif /* FIO_OS_WIN */
/* *****************************************************************************
Module Testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, threads)(void) {
  /*
   * TODO? test module here
   */
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_MODULE_PTR
#endif /* FIO_THREADS */
#undef FIO_THREADS
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                  URI Parsing




***************************************************************************** */
#if (defined(FIO_URL) || defined(FIO_URI)) && !defined(H___FIO_URL___H)
#define H___FIO_URL___H
/** the result returned by `fio_url_parse` */
typedef struct {
  fio_str_info_s scheme;
  fio_str_info_s user;
  fio_str_info_s password;
  fio_str_info_s host;
  fio_str_info_s port;
  fio_str_info_s path;
  fio_str_info_s query;
  fio_str_info_s target;
} fio_url_s;

/**
 * Parses the URI returning it's components and their lengths (no decoding
 * performed, doesn't accept decoded URIs).
 *
 * The returned string are NOT NUL terminated, they are merely locations within
 * the original string.
 *
 * This function attempts to accept many different formats, including any of the
 * following:
 *
 * * `/complete_path?query#target`
 *
 *   i.e.: /index.html?page=1#list
 *
 * * `host:port/complete_path?query#target`
 *
 *   i.e.:
 *      example.com
 *      example.com:8080
 *      example.com/index.html
 *      example.com:8080/index.html
 *      example.com:8080/index.html?key=val#target
 *
 * * `user:password@host:port/path?query#target`
 *
 *   i.e.: user:1234@example.com:8080/index.html
 *
 * * `username[:password]@host[:port][...]`
 *
 *   i.e.: john:1234@example.com
 *
 * * `schema://user:password@host:port/path?query#target`
 *
 *   i.e.: http://example.com/index.html?page=1#list
 *
 * Invalid formats might produce unexpected results. No error testing performed.
 */
SFUNC fio_url_s fio_url_parse(const char *url, size_t len);

/* *****************************************************************************
FIO_URL - Implementation
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE)

/**
 * Parses the URI returning it's components and their lengths (no decoding
 * performed, doesn't accept decoded URIs).
 *
 * The returned string are NOT NUL terminated, they are merely locations within
 * the original string.
 *
 * This function expects any of the following formats:
 *
 * * `/complete_path?query#target`
 *
 *   i.e.: /index.html?page=1#list
 *
 * * `host:port/complete_path?query#target`
 *
 *   i.e.:
 *      example.com/index.html
 *      example.com:8080/index.html
 *
 * * `schema://user:password@host:port/path?query#target`
 *
 *   i.e.: http://example.com/index.html?page=1#list
 *
 * Invalid formats might produce unexpected results. No error testing performed.
 */
SFUNC fio_url_s fio_url_parse(const char *url, size_t len) {
  /*
  Intention:
  [schema://][user[:]][password[@]][host.com[:/]][:port/][/path][?quary][#target]
  */
  const char *end = url + len;
  const char *pos = url;
  fio_url_s r = {.scheme = {.buf = (char *)url}};
  if (len == 0) {
    goto finish;
  }

  if (pos[0] == '/') {
    /* start at path */
    goto start_path;
  }

  while (pos < end && pos[0] != ':' && pos[0] != '/' && pos[0] != '@' &&
         pos[0] != '#' && pos[0] != '?')
    ++pos;

  if (pos == end) {
    /* was only host (path starts with '/') */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    goto finish;
  }
  switch (pos[0]) {
  case '@':
    /* username@[host] */
    r.user = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    goto start_host;
  case '/':
    /* host[/path] */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    goto start_path;
  case '?':
    /* host?[query] */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    goto start_query;
  case '#':
    /* host#[target] */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    goto start_target;
  case ':':
    if (pos + 2 <= end && pos[1] == '/' && pos[2] == '/') {
      /* scheme:// */
      r.scheme.len = pos - url;
      pos += 3;
    } else {
      /* username:[password] OR */
      /* host:[port] */
      r.user = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
      ++pos;
      goto start_password;
    }
    break;
  }

  // start_username:
  url = pos;
  while (pos < end && pos[0] != ':' && pos[0] != '/' && pos[0] != '@'
         /* && pos[0] != '#' && pos[0] != '?' */)
    ++pos;

  if (pos >= end) { /* scheme://host */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    goto finish;
  }

  switch (pos[0]) {
  case '/':
    /* scheme://host[/path] */
    r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    goto start_path;
  case '@':
    /* scheme://username@[host]... */
    r.user = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    goto start_host;
  case ':':
    /* scheme://username:[password]@[host]... OR */
    /* scheme://host:[port][/...] */
    r.user = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    break;
  }

start_password:
  url = pos;
  while (pos < end && pos[0] != '/' && pos[0] != '@')
    ++pos;

  if (pos >= end) {
    /* was host:port */
    r.port = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    r.host = r.user;
    r.user.len = 0;
    goto finish;
    ;
  }

  switch (pos[0]) {
  case '/':
    r.port = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    r.host = r.user;
    r.user.len = 0;
    goto start_path;
  case '@':
    r.password =
        (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
    ++pos;
    break;
  }

start_host:
  url = pos;
  while (pos < end && pos[0] != '/' && pos[0] != ':' && pos[0] != '#' &&
         pos[0] != '?')
    ++pos;

  r.host = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
  if (pos >= end) {
    goto finish;
  }
  switch (pos[0]) {
  case '/':
    /* scheme://[...@]host[/path] */
    goto start_path;
  case '?':
    /* scheme://[...@]host?[query] (bad)*/
    ++pos;
    goto start_query;
  case '#':
    /* scheme://[...@]host#[target] (bad)*/
    ++pos;
    goto start_target;
    // case ':':
    /* scheme://[...@]host:[port] */
  }
  ++pos;

  // start_port:
  url = pos;
  while (pos < end && pos[0] != '/' && pos[0] != '#' && pos[0] != '?')
    ++pos;

  r.port = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};

  if (pos >= end) {
    /* scheme://[...@]host:port */
    goto finish;
  }
  switch (pos[0]) {
  case '?':
    /* scheme://[...@]host:port?[query] (bad)*/
    ++pos;
    goto start_query;
  case '#':
    /* scheme://[...@]host:port#[target] (bad)*/
    ++pos;
    goto start_target;
    // case '/':
    /* scheme://[...@]host:port[/path] */
  }

start_path:
  url = pos;
  while (pos < end && pos[0] != '#' && pos[0] != '?')
    ++pos;

  r.path = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};

  if (pos >= end) {
    goto finish;
  }
  ++pos;
  if (pos[-1] == '#')
    goto start_target;

start_query:
  url = pos;
  while (pos < end && pos[0] != '#')
    ++pos;

  r.query = (fio_str_info_s){.buf = (char *)url, .len = (size_t)(pos - url)};
  ++pos;

  if (pos >= end)
    goto finish;

start_target:
  r.target = (fio_str_info_s){.buf = (char *)pos, .len = (size_t)(end - pos)};

finish:

  if (r.scheme.len == 4 && r.host.buf &&
      (((r.scheme.buf[0] | 32) == 'f' && (r.scheme.buf[1] | 32) == 'i' &&
        (r.scheme.buf[2] | 32) == 'l' && (r.scheme.buf[3] | 32) == 'e') ||
       ((r.scheme.buf[0] | 32) == 'u' && (r.scheme.buf[1] | 32) == 'n' &&
        (r.scheme.buf[2] | 32) == 'i' && (r.scheme.buf[3] | 32) == 'x'))) {
    r.path.len += (r.path.buf - (r.scheme.buf + 7));
    r.path.buf = r.scheme.buf + 7;
    r.user.len = r.password.len = r.port.len = r.host.len = 0;
  }

  /* set any empty values to NULL */
  if (!r.scheme.len)
    r.scheme.buf = NULL;
  if (!r.user.len)
    r.user.buf = NULL;
  if (!r.password.len)
    r.password.buf = NULL;
  if (!r.host.len)
    r.host.buf = NULL;
  if (!r.port.len)
    r.port.buf = NULL;
  if (!r.path.len)
    r.path.buf = NULL;
  if (!r.query.len)
    r.query.buf = NULL;
  if (!r.target.len)
    r.target.buf = NULL;

  return r;
}

/* *****************************************************************************
URL parsing - Test
***************************************************************************** */
#ifdef FIO_TEST_CSTL

/* Test for URI variations:
 *
 * * `/complete_path?query#target`
 *
 *   i.e.: /index.html?page=1#list
 *
 * * `host:port/complete_path?query#target`
 *
 *   i.e.:
 *      example.com
 *      example.com:8080
 *      example.com/index.html
 *      example.com:8080/index.html
 *      example.com:8080/index.html?key=val#target
 *
 * * `user:password@host:port/path?query#target`
 *
 *   i.e.: user:1234@example.com:8080/index.html
 *
 * * `username[:password]@host[:port][...]`
 *
 *   i.e.: john:1234@example.com
 *
 * * `schema://user:password@host:port/path?query#target`
 *
 *   i.e.: http://example.com/index.html?page=1#list
 */
FIO_SFUNC void FIO_NAME_TEST(stl, url)(void) {
  fprintf(stderr, "* Testing URL (URI) parser.\n");
  struct {
    char *url;
    size_t len;
    fio_url_s expected;
  } tests[] = {
      {
          .url = (char *)"file://go/home/",
          .len = 15,
          .expected =
              {
                  .scheme = {.buf = (char *)"file", .len = 4},
                  .path = {.buf = (char *)"go/home/", .len = 8},
              },
      },
      {
          .url = (char *)"unix:///go/home/",
          .len = 16,
          .expected =
              {
                  .scheme = {.buf = (char *)"unix", .len = 4},
                  .path = {.buf = (char *)"/go/home/", .len = 9},
              },
      },
      {
          .url = (char *)"schema://user:password@host:port/path?query#target",
          .len = 50,
          .expected =
              {
                  .scheme = {.buf = (char *)"schema", .len = 6},
                  .user = {.buf = (char *)"user", .len = 4},
                  .password = {.buf = (char *)"password", .len = 8},
                  .host = {.buf = (char *)"host", .len = 4},
                  .port = {.buf = (char *)"port", .len = 4},
                  .path = {.buf = (char *)"/path", .len = 5},
                  .query = {.buf = (char *)"query", .len = 5},
                  .target = {.buf = (char *)"target", .len = 6},
              },
      },
      {
          .url = (char *)"schema://user@host:port/path?query#target",
          .len = 41,
          .expected =
              {
                  .scheme = {.buf = (char *)"schema", .len = 6},
                  .user = {.buf = (char *)"user", .len = 4},
                  .host = {.buf = (char *)"host", .len = 4},
                  .port = {.buf = (char *)"port", .len = 4},
                  .path = {.buf = (char *)"/path", .len = 5},
                  .query = {.buf = (char *)"query", .len = 5},
                  .target = {.buf = (char *)"target", .len = 6},
              },
      },
      {
          .url = (char *)"http://localhost.com:3000/home?is=1",
          .len = 35,
          .expected =
              {
                  .scheme = {.buf = (char *)"http", .len = 4},
                  .host = {.buf = (char *)"localhost.com", .len = 13},
                  .port = {.buf = (char *)"3000", .len = 4},
                  .path = {.buf = (char *)"/home", .len = 5},
                  .query = {.buf = (char *)"is=1", .len = 4},
              },
      },
      {
          .url = (char *)"/complete_path?query#target",
          .len = 27,
          .expected =
              {
                  .path = {.buf = (char *)"/complete_path", .len = 14},
                  .query = {.buf = (char *)"query", .len = 5},
                  .target = {.buf = (char *)"target", .len = 6},
              },
      },
      {
          .url = (char *)"/index.html?page=1#list",
          .len = 23,
          .expected =
              {
                  .path = {.buf = (char *)"/index.html", .len = 11},
                  .query = {.buf = (char *)"page=1", .len = 6},
                  .target = {.buf = (char *)"list", .len = 4},
              },
      },
      {
          .url = (char *)"example.com",
          .len = 11,
          .expected =
              {
                  .host = {.buf = (char *)"example.com", .len = 11},
              },
      },

      {
          .url = (char *)"example.com:8080",
          .len = 16,
          .expected =
              {
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .port = {.buf = (char *)"8080", .len = 4},
              },
      },
      {
          .url = (char *)"example.com/index.html",
          .len = 22,
          .expected =
              {
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .path = {.buf = (char *)"/index.html", .len = 11},
              },
      },
      {
          .url = (char *)"example.com:8080/index.html",
          .len = 27,
          .expected =
              {
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .port = {.buf = (char *)"8080", .len = 4},
                  .path = {.buf = (char *)"/index.html", .len = 11},
              },
      },
      {
          .url = (char *)"example.com:8080/index.html?key=val#target",
          .len = 42,
          .expected =
              {
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .port = {.buf = (char *)"8080", .len = 4},
                  .path = {.buf = (char *)"/index.html", .len = 11},
                  .query = {.buf = (char *)"key=val", .len = 7},
                  .target = {.buf = (char *)"target", .len = 6},
              },
      },
      {
          .url = (char *)"user:1234@example.com:8080/index.html",
          .len = 37,
          .expected =
              {
                  .user = {.buf = (char *)"user", .len = 4},
                  .password = {.buf = (char *)"1234", .len = 4},
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .port = {.buf = (char *)"8080", .len = 4},
                  .path = {.buf = (char *)"/index.html", .len = 11},
              },
      },
      {
          .url = (char *)"user@example.com:8080/index.html",
          .len = 32,
          .expected =
              {
                  .user = {.buf = (char *)"user", .len = 4},
                  .host = {.buf = (char *)"example.com", .len = 11},
                  .port = {.buf = (char *)"8080", .len = 4},
                  .path = {.buf = (char *)"/index.html", .len = 11},
              },
      },
      {.url = NULL},
  };
  for (size_t i = 0; tests[i].url; ++i) {
    fio_url_s result = fio_url_parse(tests[i].url, tests[i].len);
    FIO_LOG_DEBUG2("Result for: %s"
                   "\n\t     scheme   (%zu bytes):  %.*s"
                   "\n\t     user     (%zu bytes):  %.*s"
                   "\n\t     password (%zu bytes):  %.*s"
                   "\n\t     host     (%zu bytes):  %.*s"
                   "\n\t     port     (%zu bytes):  %.*s"
                   "\n\t     path     (%zu bytes):  %.*s"
                   "\n\t     query    (%zu bytes):  %.*s"
                   "\n\t     target   (%zu bytes):  %.*s\n",
                   tests[i].url,
                   result.scheme.len,
                   (int)result.scheme.len,
                   result.scheme.buf,
                   result.user.len,
                   (int)result.user.len,
                   result.user.buf,
                   result.password.len,
                   (int)result.password.len,
                   result.password.buf,
                   result.host.len,
                   (int)result.host.len,
                   result.host.buf,
                   result.port.len,
                   (int)result.port.len,
                   result.port.buf,
                   result.path.len,
                   (int)result.path.len,
                   result.path.buf,
                   result.query.len,
                   (int)result.query.len,
                   result.query.buf,
                   result.target.len,
                   (int)result.target.len,
                   result.target.buf);
    FIO_ASSERT(
        result.scheme.len == tests[i].expected.scheme.len &&
            (!result.scheme.len || !memcmp(result.scheme.buf,
                                           tests[i].expected.scheme.buf,
                                           tests[i].expected.scheme.len)),
        "scheme result failed for:\n\ttest[%zu]: %s\n\texpected: "
        "%s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.scheme.buf,
        (int)result.scheme.len,
        result.scheme.buf);
    FIO_ASSERT(
        result.user.len == tests[i].expected.user.len &&
            (!result.user.len || !memcmp(result.user.buf,
                                         tests[i].expected.user.buf,
                                         tests[i].expected.user.len)),
        "user result failed for:\n\ttest[%zu]: %s\n\texpected: %s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.user.buf,
        (int)result.user.len,
        result.user.buf);
    FIO_ASSERT(
        result.password.len == tests[i].expected.password.len &&
            (!result.password.len || !memcmp(result.password.buf,
                                             tests[i].expected.password.buf,
                                             tests[i].expected.password.len)),
        "password result failed for:\n\ttest[%zu]: %s\n\texpected: %s\n\tgot: "
        "%.*s",
        i,
        tests[i].url,
        tests[i].expected.password.buf,
        (int)result.password.len,
        result.password.buf);
    FIO_ASSERT(
        result.host.len == tests[i].expected.host.len &&
            (!result.host.len || !memcmp(result.host.buf,
                                         tests[i].expected.host.buf,
                                         tests[i].expected.host.len)),
        "host result failed for:\n\ttest[%zu]: %s\n\texpected: %s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.host.buf,
        (int)result.host.len,
        result.host.buf);
    FIO_ASSERT(
        result.port.len == tests[i].expected.port.len &&
            (!result.port.len || !memcmp(result.port.buf,
                                         tests[i].expected.port.buf,
                                         tests[i].expected.port.len)),
        "port result failed for:\n\ttest[%zu]: %s\n\texpected: %s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.port.buf,
        (int)result.port.len,
        result.port.buf);
    FIO_ASSERT(
        result.path.len == tests[i].expected.path.len &&
            (!result.path.len || !memcmp(result.path.buf,
                                         tests[i].expected.path.buf,
                                         tests[i].expected.path.len)),
        "path result failed for:\n\ttest[%zu]: %s\n\texpected: %s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.path.buf,
        (int)result.path.len,
        result.path.buf);
    FIO_ASSERT(result.query.len == tests[i].expected.query.len &&
                   (!result.query.len || !memcmp(result.query.buf,
                                                 tests[i].expected.query.buf,
                                                 tests[i].expected.query.len)),
               "query result failed for:\n\ttest[%zu]: %s\n\texpected: "
               "%s\n\tgot: %.*s",
               i,
               tests[i].url,
               tests[i].expected.query.buf,
               (int)result.query.len,
               result.query.buf);
    FIO_ASSERT(
        result.target.len == tests[i].expected.target.len &&
            (!result.target.len || !memcmp(result.target.buf,
                                           tests[i].expected.target.buf,
                                           tests[i].expected.target.len)),
        "target result failed for:\n\ttest[%zu]: %s\n\texpected: "
        "%s\n\tgot: %.*s",
        i,
        tests[i].url,
        tests[i].expected.target.buf,
        (int)result.target.len,
        result.target.buf);
  }
}
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
FIO_URL - Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_URL || FIO_URI */
#undef FIO_URL
#undef FIO_URI
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                JSON Parsing



***************************************************************************** */
#if defined(FIO_JSON) && !defined(H___FIO_JSON_H)
#define H___FIO_JSON_H

#ifndef JSON_MAX_DEPTH
/** Maximum allowed JSON nesting level. Values above 64K might fail. */
#define JSON_MAX_DEPTH 512
#endif

/** The JSON parser type. Memory must be initialized to 0 before first uses. */
typedef struct {
  /** level of nesting. */
  uint32_t depth;
  /** expectation bit flag: 0=key, 1=colon, 2=value, 4=comma/closure . */
  uint8_t expect;
  /** nesting bit flags - dictionary bit = 0, array bit = 1. */
  uint8_t nesting[(JSON_MAX_DEPTH + 7) >> 3];
} fio_json_parser_s;

#define FIO_JSON_INIT                                                          \
  { .depth = 0 }

/**
 * The facil.io JSON parser is a non-strict parser, with support for trailing
 * commas in collections, new-lines in strings, extended escape characters and
 * octal, hex and binary numbers.
 *
 * The parser allows for streaming data and decouples the parsing process from
 * the resulting data-structure by calling static callbacks for JSON related
 * events.
 *
 * Returns the number of bytes consumed before parsing stopped (due to either
 * error or end of data). Stops as close as possible to the end of the buffer or
 * once an object parsing was completed.
 */
SFUNC size_t fio_json_parse(fio_json_parser_s *parser,
                            const char *buffer,
                            const size_t len);

/* *****************************************************************************
JSON Parsing - Implementation - Helpers and Callbacks


Note: static Callacks must be implemented in the C file that uses the parser

Note: a Helper API is provided for the parsing implementation.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/** common FIO_JSON callback function properties */
#define FIO_JSON_CB static inline __attribute__((unused))

/* *****************************************************************************
JSON Parsing - Helpers API
***************************************************************************** */

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_in_array(fio_json_parser_s *parser);

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_in_object(fio_json_parser_s *parser);

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_key(fio_json_parser_s *parser);

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_value(fio_json_parser_s *parser);

/* *****************************************************************************
JSON Parsing - Implementation - Callbacks
***************************************************************************** */

/** a NULL object was detected */
FIO_JSON_CB void fio_json_on_null(fio_json_parser_s *p);
/** a TRUE object was detected */
static inline void fio_json_on_true(fio_json_parser_s *p);
/** a FALSE object was detected */
FIO_JSON_CB void fio_json_on_false(fio_json_parser_s *p);
/** a Number was detected (long long). */
FIO_JSON_CB void fio_json_on_number(fio_json_parser_s *p, long long i);
/** a Float was detected (double). */
FIO_JSON_CB void fio_json_on_float(fio_json_parser_s *p, double f);
/** a String was detected (int / float). update `pos` to point at ending */
FIO_JSON_CB void fio_json_on_string(fio_json_parser_s *p,
                                    const void *start,
                                    size_t len);
/** a dictionary object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_object(fio_json_parser_s *p);
/** a dictionary object closure detected */
FIO_JSON_CB void fio_json_on_end_object(fio_json_parser_s *p);
/** an array object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_array(fio_json_parser_s *p);
/** an array closure was detected */
FIO_JSON_CB void fio_json_on_end_array(fio_json_parser_s *p);
/** the JSON parsing is complete */
FIO_JSON_CB void fio_json_on_json(fio_json_parser_s *p);
/** the JSON parsing encountered an error */
FIO_JSON_CB void fio_json_on_error(fio_json_parser_s *p);

/* *****************************************************************************
JSON Parsing - Implementation - Helpers and Parsing


Note: static Callacks must be implemented in the C file that uses the parser
***************************************************************************** */

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_in_array(fio_json_parser_s *p) {
  return p->depth && fio_bitmap_get(p->nesting, p->depth);
}

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_in_object(fio_json_parser_s *p) {
  return p->depth && !fio_bitmap_get(p->nesting, p->depth);
}

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_key(fio_json_parser_s *p) {
  return fio_json_parser_is_in_object(p) && !p->expect;
}

/** Tests the state of the JSON parser. Returns 1 for true and 0 for false. */
FIO_JSON_CB uint8_t fio_json_parser_is_value(fio_json_parser_s *p) {
  return !fio_json_parser_is_key(p);
}

FIO_IFUNC const char *fio___json_skip_comments(const char *buffer,
                                               const char *stop) {
  if (*buffer == '#' ||
      ((stop - buffer) > 2 && buffer[0] == '/' && buffer[1] == '/')) {
    /* EOL style comment, C style or Bash/Ruby style*/
    buffer = (const char *)memchr(buffer + 1, '\n', stop - (buffer + 1));
    return buffer;
  }
  if (((stop - buffer) > 3 && buffer[0] == '/' && buffer[1] == '*')) {
    while ((buffer = (const char *)memchr(buffer, '/', stop - buffer)) &&
           buffer && ++buffer && buffer[-2] != '*')
      ;
    return buffer;
  }
  return NULL;
}

FIO_IFUNC const char *fio___json_consume_string(fio_json_parser_s *p,
                                                const char *buffer,
                                                const char *stop) {
  const char *start = ++buffer;
  for (;;) {
    buffer = (const char *)memchr(buffer, '\"', stop - buffer);
    if (!buffer)
      return NULL;
    size_t escaped = 1;
    while (buffer[0 - escaped] == '\\')
      ++escaped;
    if (escaped & 1)
      break;
    ++buffer;
  }
  fio_json_on_string(p, start, buffer - start);
  return buffer + 1;
}

FIO_IFUNC const char *fio___json_consume_number(fio_json_parser_s *p,
                                                const char *buffer,
                                                const char *stop) {

  const char *const was = buffer;
  errno = 0; /* testo for E2BIG on number parsing */
  long long i = fio_atol((char **)&buffer);

  if (buffer < stop &&
      ((*buffer) == '.' || (*buffer | 32) == 'e' || (*buffer | 32) == 'x' ||
       (*buffer | 32) == 'p' || (*buffer | 32) == 'i' || errno)) {
    buffer = was;
    double f = fio_atof((char **)&buffer);
    fio_json_on_float(p, f);
  } else {
    fio_json_on_number(p, i);
  }
  return buffer;
}

FIO_IFUNC const char *fio___json_identify(fio_json_parser_s *p,
                                          const char *buffer,
                                          const char *stop) {
  /* Use `break` to change separator requirement status.
   * Use `continue` to keep separator requirement the same.
   */
  switch (*buffer) {
  case 0x09: /* fallthrough */
  case 0x0A: /* fallthrough */
  case 0x0D: /* fallthrough */
  case 0x20:
    /* consume whitespace */
    ++buffer;
    while (buffer + 8 < stop && (buffer[0] == 0x20 || buffer[0] == 0x09 ||
                                 buffer[0] == 0x0A || buffer[0] == 0x0D)) {
      const uint64_t w = fio_buf2u64_local(buffer);
      const uint64_t w1 = 0x0101010101010101 * 0x09; /* '\t' (tab) */
      const uint64_t w2 = 0x0101010101010101 * 0x0A; /* '\n' (new line) */
      const uint64_t w3 = 0x0101010101010101 * 0x0D; /* '\r' (CR) */
      const uint64_t w4 = 0x0101010101010101 * 0x20; /* ' '  (space) */
      uint64_t b = fio_has_zero_byte64(w1 ^ w) | fio_has_zero_byte64(w2 ^ w) |
                   fio_has_zero_byte64(w3 ^ w) | fio_has_zero_byte64(w4 ^ w);
      if (b == 0x8080808080808080ULL) {
        buffer += 8;
        continue;
      }
      while ((b & UINT64_C(0x80))) {
        b >>= 8;
        ++buffer;
      }
      break;
    }

    return buffer;
  case ',': /* comma separator */
    if (!p->depth || !(p->expect & 4))
      goto unexpected_separator;
    ++buffer;
    p->expect = (fio_bitmap_get(p->nesting, p->depth) << 1);
    return buffer;
  case ':': /* colon separator */
    if (!p->depth || !(p->expect & 1))
      goto unexpected_separator;
    ++buffer;
    p->expect = 2;
    return buffer;
    /*
     *
     * JSON Strings
     *
     */
  case '"':
    if (p->depth && (p->expect & ((uint8_t)5)))
      goto missing_separator;
    buffer = fio___json_consume_string(p, buffer, stop);
    if (!buffer)
      goto unterminated_string;
    break;
    /*
     *
     * JSON Objects
     *
     */
  case '{':
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    if (p->depth == JSON_MAX_DEPTH)
      goto too_deep;
    ++p->depth;
    fio_bitmap_unset(p->nesting, p->depth);
    fio_json_on_start_object(p);
    p->expect = 0;
    return buffer + 1;
  case '}':
    if (fio_bitmap_get(p->nesting, p->depth) || !p->depth || (p->expect & 3))
      goto object_closure_unexpected;
    fio_bitmap_unset(p->nesting, p->depth);
    p->expect = 4; /* expect comma */
    --p->depth;
    fio_json_on_end_object(p);
    return buffer + 1;
    /*
     *
     * JSON Arrays
     *
     */
  case '[':
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    if (p->depth == JSON_MAX_DEPTH)
      goto too_deep;
    ++p->depth;
    fio_json_on_start_array(p);
    fio_bitmap_set(p->nesting, p->depth);
    p->expect = 2;
    return buffer + 1;
  case ']':
    if (!fio_bitmap_get(p->nesting, p->depth) || !p->depth)
      goto array_closure_unexpected;
    fio_bitmap_unset(p->nesting, p->depth);
    p->expect = 4; /* expect comma */
    --p->depth;
    fio_json_on_end_array(p);
    return buffer + 1;
    /*
     *
     * JSON Primitives (true / false / null (NaN))
     *
     */
  case 'N': /* NaN or null? - fallthrough */
  case 'n':
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    if (buffer + 4 > stop || buffer[1] != 'u' || buffer[2] != 'l' ||
        buffer[3] != 'l') {
      if (buffer + 3 > stop || (buffer[1] | 32) != 'a' ||
          (buffer[2] | 32) != 'n')
        return NULL;
      char *nan_str = (char *)"NaN";
      fio_json_on_float(p, fio_atof(&nan_str));
      buffer += 3;
      break;
    }
    fio_json_on_null(p);
    buffer += 4;
    break;
  case 't': /* true */
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    if (buffer + 4 > stop || buffer[1] != 'r' || buffer[2] != 'u' ||
        buffer[3] != 'e')
      return NULL;
    fio_json_on_true(p);
    buffer += 4;
    break;
  case 'f': /* false */
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    if (buffer + 5 > stop || buffer[1] != 'a' || buffer[2] != 'l' ||
        buffer[3] != 's' || buffer[4] != 'e')
      return NULL;
    fio_json_on_false(p);
    buffer += 5;
    break;
    /*
     *
     * JSON Numbers (Integers / Floats)
     *
     */
  case '+': /* fallthrough */
  case '-': /* fallthrough */
  case '0': /* fallthrough */
  case '1': /* fallthrough */
  case '2': /* fallthrough */
  case '3': /* fallthrough */
  case '4': /* fallthrough */
  case '5': /* fallthrough */
  case '6': /* fallthrough */
  case '7': /* fallthrough */
  case '8': /* fallthrough */
  case '9': /* fallthrough */
  case 'x': /* fallthrough */
  case '.': /* fallthrough */
  case 'e': /* fallthrough */
  case 'E': /* fallthrough */
  case 'i': /* fallthrough */
  case 'I':
    if (p->depth && !(p->expect & 2))
      goto missing_separator;
    buffer = fio___json_consume_number(p, buffer, stop);
    if (!buffer)
      goto bad_number_format;
    break;
    /*
     *
     * Comments
     *
     */
  case '#': /* fallthrough */
  case '/': /* fallthrough */
    return fio___json_skip_comments(buffer, stop);
    /*
     *
     * Unrecognized Data Handling
     *
     */
  default:
    FIO_LOG_DEBUG("unrecognized JSON identifier at:\n%.*s",
                  ((stop - buffer > 48) ? (int)48 : ((int)(stop - buffer))),
                  buffer);
    return NULL;
  }
  /* p->expect should be either 0 (key) or 2 (value) */
  p->expect = (p->expect << 1) + ((p->expect ^ 2) >> 1);
  return buffer;

missing_separator:
  FIO_LOG_DEBUG("missing JSON separator '%c' at (%d):\n%.*s",
                (p->expect == 2 ? ':' : ','),
                p->expect,
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
unexpected_separator:
  FIO_LOG_DEBUG("unexpected JSON separator at:\n%.*s",
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
unterminated_string:
  FIO_LOG_DEBUG("unterminated JSON string at:\n%.*s",
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
bad_number_format:
  FIO_LOG_DEBUG("bad JSON numeral format at:\n%.*s",
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
array_closure_unexpected:
  FIO_LOG_DEBUG("JSON array closure unexpected at:\n%.*s",
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
object_closure_unexpected:
  FIO_LOG_DEBUG("JSON object closure unexpected at (%d):\n%.*s",
                p->expect,
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
too_deep:
  FIO_LOG_DEBUG("JSON object nesting too deep at:\n%.*s",
                p->expect,
                ((stop - buffer > 48) ? 48 : ((int)(stop - buffer))),
                buffer);
  fio_json_on_error(p);
  return NULL;
}

/**
 * Returns the number of bytes consumed. Stops as close as possible to the end
 * of the buffer or once an object parsing was completed.
 */
SFUNC size_t fio_json_parse(fio_json_parser_s *p,
                            const char *buffer,
                            const size_t len) {
  const char *start = buffer;
  const char *stop = buffer + len;
  const char *last;
  /* skip BOM, if exists */
  if (len >= 3 && buffer[0] == (char)0xEF && buffer[1] == (char)0xBB &&
      buffer[2] == (char)0xBF) {
    buffer += 3;
    if (len == 3)
      goto finish;
  }
  /* loop until the first JSON data was read */
  do {
    last = buffer;
    buffer = fio___json_identify(p, buffer, stop);
    if (!buffer)
      goto failed;
  } while (!p->expect && buffer < stop);
  /* loop until the JSON object (nesting) is closed */
  while (p->depth && buffer < stop) {
    last = buffer;
    buffer = fio___json_identify(p, buffer, stop);
    if (!buffer)
      goto failed;
  }
  if (!p->depth) {
    p->expect = 0;
    fio_json_on_json(p);
  }
finish:
  return buffer - start;
failed:
  FIO_LOG_DEBUG("JSON parsing failed after:\n%.*s",
                ((stop - last > 48) ? 48 : ((int)(stop - last))),
                last);
  return last - start;
}

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_JSON
#endif /* FIO_JSON */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_MEMORY_NAME fio         /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "005 riskyhash.h"          /* Development inclusion - ignore line */
#include "007 threads.h"            /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                      Custom Memory Allocator / Pooling




***************************************************************************** */

/* *****************************************************************************
Memory Allocation - fast setup for a global allocator
***************************************************************************** */
#if defined(FIO_MALLOC) && !defined(H___FIO_MALLOC___H)
#define FIO_MEMORY_NAME fio

#ifndef FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG
/* for a general allocator, increase system allocation size to 8Gb */
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG 23
#endif

#ifndef FIO_MEMORY_CACHE_SLOTS
/* for a general allocator, increase cache size */
#define FIO_MEMORY_CACHE_SLOTS 8
#endif

#ifndef FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG
/* set fragmentation cost at 0.5Mb blocks */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 5
#endif

#ifndef FIO_MEMORY_ENABLE_BIG_ALLOC
/* support big allocations using undivided memory chunks */
#define FIO_MEMORY_ENABLE_BIG_ALLOC 1
#endif

#undef FIO_MEM_REALLOC
/** Reallocates memory, copying (at least) `copy_len` if necessary. */
#define FIO_MEM_REALLOC(ptr, old_size, new_size, copy_len)                     \
  fio_realloc2((ptr), (new_size), (copy_len))

#undef FIO_MEM_FREE
/** Frees allocated memory. */
#define FIO_MEM_FREE(ptr, size) fio_free((ptr))

#undef FIO_MEM_REALLOC_IS_SAFE
#define FIO_MEM_REALLOC_IS_SAFE fio_realloc_is_safe()

/* prevent double decleration of FIO_MALLOC */
#define H___FIO_MALLOC___H
#endif
#undef FIO_MALLOC

/* *****************************************************************************
Memory Allocation - Setup Alignment Info
***************************************************************************** */
#ifdef FIO_MEMORY_NAME

#undef FIO_MEM_ALIGN
#undef FIO_MEM_ALIGN_NEW

#ifndef FIO_MEMORY_ALIGN_LOG
/** Allocation alignment, MUST be >= 3 and <= 10*/
#define FIO_MEMORY_ALIGN_LOG 4

#elif FIO_MEMORY_ALIGN_LOG < 3 || FIO_MEMORY_ALIGN_LOG > 10
#undef FIO_MEMORY_ALIGN_LOG
#define FIO_MEMORY_ALIGN_LOG 4
#endif

/* Helper macro, don't change this */
#undef FIO_MEMORY_ALIGN_SIZE
/**
 * The maximum allocation size, after which a direct system allocation is used.
 */
#define FIO_MEMORY_ALIGN_SIZE (1UL << FIO_MEMORY_ALIGN_LOG)

/* inform the compiler that the returned value is aligned on 16 byte marker */
#if __clang__ || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
#define FIO_MEM_ALIGN __attribute__((assume_aligned(FIO_MEMORY_ALIGN_SIZE)))
#define FIO_MEM_ALIGN_NEW                                                      \
  __attribute__((malloc, assume_aligned(FIO_MEMORY_ALIGN_SIZE)))
#else
#define FIO_MEM_ALIGN
#define FIO_MEM_ALIGN_NEW
#endif /* (__clang__ || __GNUC__)... */

/* *****************************************************************************
Memory Helpers - API
***************************************************************************** */
#ifndef H___FIO_MEM_INCLUDE_ONCE___H
/**
 * A 16 byte aligned memset (almost) naive implementation.
 *
 * Probably slower than the one included with your compiler's C library.
 *
 * Requires BOTH addresses to be 16 bit aligned memory addresses.
 */
SFUNC void fio_memset_aligned(void *restrict dest, uint64_t data, size_t bytes);

/**
 * A 16 byte aligned memcpy (almost) naive implementation.
 *
 * Probably slower than the one included with your compiler's C library.
 *
 * Requires a 16 bit aligned memory address.
 */
SFUNC void fio_memcpy_aligned(void *dest_, const void *src_, size_t bytes);

#endif /* H___FIO_MEM_INCLUDE_ONCE___H */
/* *****************************************************************************
Memory Allocation - API
***************************************************************************** */

/**
 * Allocates memory using a per-CPU core block memory pool.
 * Memory is zeroed out.
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT will be redirected to `mmap`,
 * as if `mempool_mmap` was called.
 *
 * `mempool_malloc` promises a best attempt at providing locality between
 * consecutive calls, but locality can't be guaranteed.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, malloc)(size_t size);

/**
 * same as calling `fio_malloc(size_per_unit * unit_count)`;
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT will be redirected to `mmap`,
 * as if `mempool_mmap` was called.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME,
                                       calloc)(size_t size_per_unit,
                                               size_t unit_count);

/** Frees memory that was allocated using this library. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, free)(void *ptr);

/**
 * Re-allocates memory. An attempt to avoid copying the data is made only for
 * big memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 */
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc)(void *ptr,
                                                             size_t new_size);

/**
 * Re-allocates memory. An attempt to avoid copying the data is made only for
 * big memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 *
 * This variation is slightly faster as it might copy less data.
 */
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc2)(void *ptr,
                                                              size_t new_size,
                                                              size_t copy_len);

/**
 * Allocates memory directly using `mmap`, this is preferred for objects that
 * both require almost a page of memory (or more) and expect a long lifetime.
 *
 * However, since this allocation will invoke the system call (`mmap`), it will
 * be inherently slower.
 *
 * `mempoll_free` can be used for deallocating the memory.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, mmap)(size_t size);

/**
 * When forking is called manually, call this function to reset the facil.io
 * memory allocator's locks.
 */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_after_fork)(void);

/* *****************************************************************************
Memory Allocation - configuration macros

NOTE: most configuration values should be a power of 2 or a logarithmic value.
***************************************************************************** */

/** FIO_MEMORY_DISABLE disables all custom memory allocators. */
#if defined(FIO_MEMORY_DISABLE)
#ifndef FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_MALLOC_TMP_USE_SYSTEM 1
#endif
#endif

/* Make sure the system's allocator is marked as unsafe. */
#if FIO_MALLOC_TMP_USE_SYSTEM
#undef FIO_MEMORY_INITIALIZE_ALLOCATIONS
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 0
#endif

#ifndef FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG
/**
 * The logarithmic size of a single allocation "chunk" (16 blocks).
 *
 * Limited to >=17 and <=24.
 *
 * By default 22, which is a ~2Mb allocation per system call, resulting in a
 * maximum allocation size of 131Kb.
 */
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG 21
#endif

#ifndef FIO_MEMORY_CACHE_SLOTS
/**
 * The number of system allocation "chunks" to cache even if they are not in
 * use.
 */
#define FIO_MEMORY_CACHE_SLOTS 4
#endif

#ifndef FIO_MEMORY_INITIALIZE_ALLOCATIONS
/**
 * Forces the allocator to zero out memory early and often, so allocations
 * return initialized memory (bytes are all zeros).
 *
 * This will make the realloc2 safe for use (all data not copied is zero).
 */
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS                                      \
  FIO_MEMORY_INITIALIZE_ALLOCATIONS_DEFAULT
#elif FIO_MEMORY_INITIALIZE_ALLOCATIONS
#undef FIO_MEMORY_INITIALIZE_ALLOCATIONS
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 1
#else
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 0
#endif

#ifndef FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG
/**
 * The number of blocks per system allocation.
 *
 * More blocks protect against fragmentation, but lower the maximum number that
 * can be allocated without reverting to mmap.
 *
 * Range: 0-4
 * Recommended: depends on object allocation sizes, usually 1 or 2.
 */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 2
#endif

#ifndef FIO_MEMORY_ENABLE_BIG_ALLOC
/**
 * Uses a whole system allocation to support bigger allocations.
 *
 * Could increase fragmentation costs.
 */
#define FIO_MEMORY_ENABLE_BIG_ALLOC 1
#endif

#ifndef FIO_MEMORY_ARENA_COUNT
/**
 * Memory arenas mitigate thread contention while using more memory.
 *
 * Note that at some point arenas are statistically irrelevant... except when
 * benchmarking contention in multi-core machines.
 *
 * Negative values will result in dynamic selection based on CPU core count.
 */
#define FIO_MEMORY_ARENA_COUNT -1
#endif

#ifndef FIO_MEMORY_ARENA_COUNT_FALLBACK
/*
 * Used when dynamic arena count calculations fail.
 *
 * NOTE: if FIO_MEMORY_ARENA_COUNT is negative, dynamic arena calculation is
 * performed using CPU core calculation.
 */
#define FIO_MEMORY_ARENA_COUNT_FALLBACK 8
#endif

#ifndef FIO_MEMORY_ARENA_COUNT_MAX
/*
 * Used when dynamic arena count calculations fail.
 *
 * NOTE: if FIO_MEMORY_ARENA_COUNT is negative, dynamic arena calculation is
 * performed using CPU core calculation.
 */
#define FIO_MEMORY_ARENA_COUNT_MAX 32
#endif

#ifndef FIO_MEMORY_WARMUP
#define FIO_MEMORY_WARMUP 0
#endif

#ifndef FIO_MEMORY_USE_THREAD_MUTEX
#if FIO_USE_THREAD_MUTEX_TMP
/*
 * If arena count isn't linked to the CPU count, threads might busy-spin.
 * It is better to slow wait than fast busy spin when the work in the lock is
 * longer... and system allocations are performed inside arena locks.
 */
#define FIO_MEMORY_USE_THREAD_MUTEX 1
#else
/** defaults to use a spinlock. */
#define FIO_MEMORY_USE_THREAD_MUTEX 0
#endif
#endif

#ifndef FIO_MEMORY_USE_FIO_MEMSET
/** If true, uses a facil.io custom implementation. */
#define FIO_MEMORY_USE_FIO_MEMSET 1
#endif

#ifndef FIO_MEMORY_USE_FIO_MEMCOPY
/** If true, uses a facil.io custom implementation. */
#define FIO_MEMORY_USE_FIO_MEMCOPY 0
#endif

#ifndef FIO_MEM_PAGE_SIZE_LOG
#define FIO_MEM_PAGE_SIZE_LOG 12 /* 4096 bytes per page */
#endif

#if !defined(FIO_MEM_SYS_ALLOC) || !defined(FIO_MEM_SYS_REALLOC) ||            \
    !defined(FIO_MEM_SYS_FREE)
/**
 * The following MACROS, when all of them are defined, allow the memory
 * allocator to collect memory from the system using an alternative method.
 *
 * - FIO_MEM_SYS_ALLOC(pages, alignment_log)
 *
 * - FIO_MEM_SYS_REALLOC(ptr, old_pages, new_pages, alignment_log)
 *
 * - FIO_MEM_SYS_FREE(ptr, pages) FIO_MEM_SYS_FREE_def_func((ptr), (pages))
 *
 * Note that the alignment property for the allocated memory is essential and
 * may me quite large.
 */
#undef FIO_MEM_SYS_ALLOC
#undef FIO_MEM_SYS_REALLOC
#undef FIO_MEM_SYS_FREE
#endif /* undefined FIO_MEM_SYS_ALLOC... */

/* *****************************************************************************
Memory Allocation - configuration value - results and constants
***************************************************************************** */

/* Helper macros, don't change their values */
#undef FIO_MEMORY_BLOCKS_PER_ALLOCATION
#undef FIO_MEMORY_SYS_ALLOCATION_SIZE
#undef FIO_MEMORY_BLOCK_ALLOC_LIMIT

#if FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG < 0 ||                                \
    FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG > 5
#undef FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 3
#endif

/** the number of allocation blocks per system allocation. */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION                                       \
  (1UL << FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG)

/** the total number of bytes consumed per system allocation. */
#define FIO_MEMORY_SYS_ALLOCATION_SIZE                                         \
  (1UL << FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG)

/**
 * The maximum allocation size, after which a big/system allocation is used.
 */
#define FIO_MEMORY_BLOCK_ALLOC_LIMIT                                           \
  (FIO_MEMORY_SYS_ALLOCATION_SIZE >> (FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG + 2))

#if FIO_MEMORY_ENABLE_BIG_ALLOC
/** the limit of a big allocation, if enabled */
#define FIO_MEMORY_BIG_ALLOC_LIMIT                                             \
  (FIO_MEMORY_SYS_ALLOCATION_SIZE >>                                           \
   (FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG > 3                                   \
        ? 3                                                                    \
        : FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG))
#define FIO_MEMORY_ALLOC_LIMIT FIO_MEMORY_BIG_ALLOC_LIMIT
#else
#define FIO_MEMORY_ALLOC_LIMIT FIO_MEMORY_BLOCK_ALLOC_LIMIT
#endif

/* *****************************************************************************
Memory Allocation - configuration access - UNSTABLE API!!!
***************************************************************************** */

FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_sys_alloc_size)(void) {
  return FIO_MEMORY_SYS_ALLOCATION_SIZE;
}

FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_cache_slots)(void) {
  return FIO_MEMORY_CACHE_SLOTS;
}
FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_alignment)(void) {
  return FIO_MEMORY_ALIGN_SIZE;
}
FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_alignment_log)(void) {
  return FIO_MEMORY_ALIGN_LOG;
}

FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_alloc_limit)(void) {
  return (FIO_MEMORY_BLOCK_ALLOC_LIMIT > FIO_MEMORY_ALLOC_LIMIT)
             ? FIO_MEMORY_BLOCK_ALLOC_LIMIT
             : FIO_MEMORY_ALLOC_LIMIT;
}

FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_arena_alloc_limit)(void) {
  return FIO_MEMORY_BLOCK_ALLOC_LIMIT;
}

/* will realloc2 return junk data? */
FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, realloc_is_safe)(void) {
  return FIO_MEMORY_INITIALIZE_ALLOCATIONS;
}

/* Returns the calculated block size. */
SFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_block_size)(void);

/** Prints the allocator's data structure. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_state)(void);

/** Prints the allocator's free block list. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_free_block_list)(void);

/** Prints the settings used to define the allocator. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_settings)(void);

/* *****************************************************************************
Temporarily (at least) set memory allocation macros to use this allocator
***************************************************************************** */
#undef FIO_MEM_REALLOC_
#undef FIO_MEM_FREE_
#undef FIO_MEM_REALLOC_IS_SAFE_

#ifndef FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_MEM_REALLOC_(ptr, old_size, new_size, copy_len)                    \
  FIO_NAME(FIO_MEMORY_NAME, realloc2)((ptr), (new_size), (copy_len))
#define FIO_MEM_FREE_(ptr, size) FIO_NAME(FIO_MEMORY_NAME, free)((ptr))
#define FIO_MEM_REALLOC_IS_SAFE_ FIO_NAME(FIO_MEMORY_NAME, realloc_is_safe)()

#endif /* FIO_MALLOC_TMP_USE_SYSTEM */

/* *****************************************************************************





Memory Allocation - start implementation





***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE
/* internal workings start here */

/* *****************************************************************************







Helpers and System Memory Allocation




***************************************************************************** */
#ifndef H___FIO_MEM_INCLUDE_ONCE___H
#define H___FIO_MEM_INCLUDE_ONCE___H

#define FIO_MEM_BYTES2PAGES(size)                                              \
  (((size_t)(size) + ((1UL << FIO_MEM_PAGE_SIZE_LOG) - 1)) &                   \
   ((~(size_t)0) << FIO_MEM_PAGE_SIZE_LOG))

/* *****************************************************************************
Aligned memory copying
***************************************************************************** */
#define FIO_MEMCOPY_FIO_IFUNC_ALIGNED(type, size)                              \
  FIO_IFUNC void fio___memcpy_##size##b(void *restrict dest_,                  \
                                        const void *restrict src_,             \
                                        size_t units) {                        \
    type *dest = (type *)dest_;                                                \
    type *src = (type *)src_;                                                  \
    if (src > dest || (src + units) <= dest) {                                 \
      while (units >= 16) {                                                    \
        dest[0] = src[0];                                                      \
        dest[1] = src[1];                                                      \
        dest[2] = src[2];                                                      \
        dest[3] = src[3];                                                      \
        dest[4] = src[4];                                                      \
        dest[5] = src[5];                                                      \
        dest[6] = src[6];                                                      \
        dest[7] = src[7];                                                      \
        dest[8] = src[8];                                                      \
        dest[9] = src[9];                                                      \
        dest[10] = src[10];                                                    \
        dest[11] = src[11];                                                    \
        dest[12] = src[12];                                                    \
        dest[13] = src[13];                                                    \
        dest[14] = src[14];                                                    \
        dest[15] = src[15];                                                    \
        dest += 16;                                                            \
        src += 16;                                                             \
        units -= 16;                                                           \
      }                                                                        \
      switch (units) {                                                         \
      case 15:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 14:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 13:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 12:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 11:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 10:                                                                 \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 9:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 8:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 7:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 6:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 5:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 4:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 3:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 2:                                                                  \
        *(dest++) = *(src++); /* fallthrough */                                \
      case 1:                                                                  \
        *(dest++) = *(src++);                                                  \
      }                                                                        \
    } else {                                                                   \
      dest += units;                                                           \
      src += units;                                                            \
      switch ((units & 15)) {                                                  \
      case 15:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 14:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 13:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 12:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 11:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 10:                                                                 \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 9:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 8:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 7:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 6:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 5:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 4:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 3:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 2:                                                                  \
        *(--dest) = *(--src); /* fallthrough */                                \
      case 1:                                                                  \
        *(--dest) = *(--src);                                                  \
      }                                                                        \
      while (units >= 16) {                                                    \
        dest -= 16;                                                            \
        src -= 16;                                                             \
        units -= 16;                                                           \
        dest[15] = src[15];                                                    \
        dest[14] = src[14];                                                    \
        dest[13] = src[13];                                                    \
        dest[12] = src[12];                                                    \
        dest[11] = src[11];                                                    \
        dest[10] = src[10];                                                    \
        dest[9] = src[9];                                                      \
        dest[8] = src[8];                                                      \
        dest[7] = src[7];                                                      \
        dest[6] = src[6];                                                      \
        dest[5] = src[5];                                                      \
        dest[4] = src[4];                                                      \
        dest[3] = src[3];                                                      \
        dest[2] = src[2];                                                      \
        dest[1] = src[1];                                                      \
        dest[0] = src[0];                                                      \
      }                                                                        \
    }                                                                          \
  }

FIO_MEMCOPY_FIO_IFUNC_ALIGNED(uint16_t, 2)
FIO_MEMCOPY_FIO_IFUNC_ALIGNED(uint32_t, 4)
FIO_MEMCOPY_FIO_IFUNC_ALIGNED(uint64_t, 8)

#undef FIO_MEMCOPY_FIO_IFUNC_ALIGNED

/** Copies 16 byte `units` of size_t aligned memory blocks */
SFUNC void fio_memcpy_aligned(void *dest_, const void *src_, size_t bytes) {
  if (src_ == dest_ || !bytes)
    return;
  if ((char *)src_ > (char *)dest_ || ((char *)src_ + bytes) <= (char *)dest_) {
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFF /* 64 bit size_t */
    fio___memcpy_8b(dest_, src_, bytes >> 3);
#elif SIZE_MAX == 0xFFFFFFFF /* 32 bit size_t */
    fio___memcpy_4b(dest_, src_, bytes >> 2);
#else                        /* unknown... assume 16 bit? */
    fio___memcpy_2b(dest_, src_, bytes >> 1);
    if (bytes & 1) {
      uint8_t *dest = (uint8_t *)dest_;
      uint8_t *src = (uint8_t *)src_;
      dest[bytes - 1] = src[bytes - 1];
    }
#endif                       /* SIZE_MAX */
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFF || SIZE_MAX == 0xFFFFFFFF /* 64/32 bit */
    uint8_t *dest = (uint8_t *)dest_;
    uint8_t *src = (uint8_t *)src_;
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFF /* 64 bit size_t */
    const size_t offset = bytes & ((~0ULL) << 3);
    dest += offset;
    src += offset;
    switch ((bytes & 7)) {
    case 7:
      *(dest++) = *(src++); /* fallthrough */
    case 6:
      *(dest++) = *(src++); /* fallthrough */
    case 5:
      *(dest++) = *(src++); /* fallthrough */
    case 4:
      *(dest++) = *(src++);  /* fallthrough */
#elif SIZE_MAX == 0xFFFFFFFF /* 32 bit size_t */
    const size_t offset = bytes & ((~0ULL) << 2);
    dest += offset;
    src += offset;
    switch ((bytes & 3)) {
#endif                       /* 32 bit */
    /* fallthrough */
    case 3:
      *(dest++) = *(src++); /* fallthrough */
    case 2:
      *(dest++) = *(src++); /* fallthrough */
    case 1:
      *(dest++) = *(src++); /* fallthrough */
    }
#endif /* 32 / 64 bit */
  } else {
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFF /* 64 bit */
    uint8_t *dest = (uint8_t *)dest_ + bytes;
    uint8_t *src = (uint8_t *)src_ + bytes;
    switch ((bytes & 7)) {
    case 7:
      *(--dest) = *(--src); /* fallthrough */
    case 6:
      *(--dest) = *(--src); /* fallthrough */
    case 5:
      *(--dest) = *(--src); /* fallthrough */
    case 4:
      *(--dest) = *(--src); /* fallthrough */
    case 3:
      *(--dest) = *(--src); /* fallthrough */
    case 2:
      *(--dest) = *(--src); /* fallthrough */
    case 1:
      *(--dest) = *(--src); /* fallthrough */
    }
#elif SIZE_MAX == 0xFFFFFFFF /* 32 bit size_t */
    uint8_t *dest = (uint8_t *)dest_ + bytes;
    uint8_t *src = (uint8_t *)src_ + bytes;
    switch ((bytes & 3)) {
    case 3:
      *(--dest) = *(--src); /* fallthrough */
    case 2:
      *(--dest) = *(--src); /* fallthrough */
    case 1:
      *(--dest) = *(--src); /* fallthrough */
    }
#endif                       /* 64 bit */

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFF /* 64 bit size_t */
    fio___memcpy_8b(dest_, src_, bytes >> 3);
#elif SIZE_MAX == 0xFFFFFFFF /* 32 bit size_t */
    fio___memcpy_4b(dest_, src_, bytes >> 2);
#else                        /* unknown... assume 16 bit? */
    if (bytes & 1) {
      uint8_t *dest = (uint8_t *)dest_;
      uint8_t *src = (uint8_t *)src_;
      dest[bytes - 1] = src[bytes - 1];
    }
    fio___memcpy_2b(dest_, src_, bytes >> 1);
#endif                       /* SIZE_MAX */
  }
}

/** a 16 byte aligned memset implementation. */
SFUNC void fio_memset_aligned(void *restrict dest_,
                              uint64_t data,
                              size_t bytes) {
  uint64_t *dest = (uint64_t *)dest_;
  bytes >>= 3;
  while (bytes >= 16) {
    dest[0] = data;
    dest[1] = data;
    dest[2] = data;
    dest[3] = data;
    dest[4] = data;
    dest[5] = data;
    dest[6] = data;
    dest[7] = data;
    dest[8] = data;
    dest[9] = data;
    dest[10] = data;
    dest[11] = data;
    dest[12] = data;
    dest[13] = data;
    dest[14] = data;
    dest[15] = data;
    dest += 16;
    bytes -= 16;
  }
  switch (bytes) {
  case 15:
    *(dest++) = data; /* fallthrough */
  case 14:
    *(dest++) = data; /* fallthrough */
  case 13:
    *(dest++) = data; /* fallthrough */
  case 12:
    *(dest++) = data; /* fallthrough */
  case 11:
    *(dest++) = data; /* fallthrough */
  case 10:
    *(dest++) = data; /* fallthrough */
  case 9:
    *(dest++) = data; /* fallthrough */
  case 8:
    *(dest++) = data; /* fallthrough */
  case 7:
    *(dest++) = data; /* fallthrough */
  case 6:
    *(dest++) = data; /* fallthrough */
  case 5:
    *(dest++) = data; /* fallthrough */
  case 4:
    *(dest++) = data; /* fallthrough */
  case 3:
    *(dest++) = data; /* fallthrough */
  case 2:
    *(dest++) = data; /* fallthrough */
  case 1:
    *(dest++) = data;
  }
}

/* *****************************************************************************



POSIX Allocation



***************************************************************************** */
#if FIO_OS_POSIX || __has_include("sys/mman.h")
#include <sys/mman.h>

/* Mitigates MAP_ANONYMOUS not being defined on older versions of MacOS */
#if !defined(MAP_ANONYMOUS)
#if defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#else
#define MAP_ANONYMOUS 0
#endif /* defined(MAP_ANONYMOUS) */
#endif /* FIO_MEM_SYS_ALLOC */

/* inform the compiler that the returned value is aligned on 16 byte marker */
#if __clang__ || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)
#define FIO_PAGE_ALIGN                                                         \
  __attribute__((assume_aligned((1UL << FIO_MEM_PAGE_SIZE_LOG))))
#define FIO_PAGE_ALIGN_NEW                                                     \
  __attribute__((malloc, assume_aligned((1UL << FIO_MEM_PAGE_SIZE_LOG))))
#else
#define FIO_PAGE_ALIGN
#define FIO_PAGE_ALIGN_NEW
#endif /* (__clang__ || __GNUC__)... */

/*
 * allocates memory using `mmap`, but enforces alignment.
 */
FIO_SFUNC void *FIO_MEM_SYS_ALLOC_def_func(size_t bytes,
                                           uint8_t alignment_log) {
  void *result;
  static void *next_alloc = (void *)0x01;
  const size_t alignment_mask = (1ULL << alignment_log) - 1;
  const size_t alignment_size = (1ULL << alignment_log);
  bytes = FIO_MEM_BYTES2PAGES(bytes);
  next_alloc =
      (void *)(((uintptr_t)next_alloc + alignment_mask) & alignment_mask);
/* hope for the best? */
#ifdef MAP_ALIGNED
  result = mmap(next_alloc,
                pages,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_ALIGNED(alignment_log),
                -1,
                0);
#else
  result = mmap(next_alloc,
                bytes,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0);
#endif /* MAP_ALIGNED */
  if (result == MAP_FAILED)
    return (void *)NULL;
  if (((uintptr_t)result & alignment_mask)) {
    munmap(result, bytes);
    result = mmap(NULL,
                  bytes + alignment_size,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1,
                  0);
    if (result == MAP_FAILED) {
      return (void *)NULL;
    }
    const uintptr_t offset =
        (alignment_size - ((uintptr_t)result & alignment_mask));
    if (offset) {
      munmap(result, offset);
      result = (void *)((uintptr_t)result + offset);
    }
    munmap((void *)((uintptr_t)result + bytes), alignment_size - offset);
  }
  next_alloc = (void *)((uintptr_t)result + (bytes << 2));
  return result;
}

/*
 * Re-allocates memory using `mmap`, enforcing alignment.
 */
FIO_SFUNC void *FIO_MEM_SYS_REALLOC_def_func(void *mem,
                                             size_t old_len,
                                             size_t new_len,
                                             uint8_t alignment_log) {
  old_len = FIO_MEM_BYTES2PAGES(old_len);
  new_len = FIO_MEM_BYTES2PAGES(new_len);
  if (new_len > old_len) {
    void *result;
#if defined(__linux__)
    result = mremap(mem, old_len, new_len, 0);
    if (result != MAP_FAILED)
      return result;
#endif
    result = mmap((void *)((uintptr_t)mem + old_len),
                  new_len - old_len,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1,
                  0);
    if (result == (void *)((uintptr_t)mem + old_len)) {
      result = mem;
    } else {
      /* copy and free */
      munmap(result, new_len - old_len); /* free the failed attempt */
      result =
          FIO_MEM_SYS_ALLOC_def_func(new_len,
                                     alignment_log); /* allocate new memory */
      if (!result) {
        return (void *)NULL;
      }
      fio_memcpy_aligned(result, mem, old_len); /* copy data */
      munmap(mem, old_len);                     /* free original memory */
    }
    return result;
  }
  if (old_len != new_len) /* remove dangling pages */
    munmap((void *)((uintptr_t)mem + new_len), old_len - new_len);
  return mem;
}

/* frees memory using `munmap`. */
FIO_IFUNC void FIO_MEM_SYS_FREE_def_func(void *mem, size_t bytes) {
  bytes = FIO_MEM_BYTES2PAGES(bytes);
  munmap(mem, bytes);
}

/* *****************************************************************************



Windows Allocation



***************************************************************************** */
#elif FIO_OS_WIN
#include <memoryapi.h>

FIO_IFUNC void FIO_MEM_SYS_FREE_def_func(void *mem, size_t bytes) {
  bytes = FIO_MEM_BYTES2PAGES(bytes);
  if (!VirtualFree(mem, 0, MEM_RELEASE))
    FIO_LOG_ERROR("Memory address at %p couldn't be returned to the system",
                  mem);
  (void)bytes;
}

FIO_IFUNC void *FIO_MEM_SYS_ALLOC_def_func(size_t bytes,
                                           uint8_t alignment_log) {
  // return aligned_alloc((pages << 12), (1UL << alignment_log));
  void *result;
  size_t attempts = 0;
  static void *next_alloc = (void *)0x01;
  const uintptr_t alignment_rounder = (1ULL << alignment_log) - 1;
  const uintptr_t alignment_mask = ~alignment_rounder;
  bytes = FIO_MEM_BYTES2PAGES(bytes);
  do {
    next_alloc =
        (void *)(((uintptr_t)next_alloc + alignment_rounder) & alignment_mask);
    FIO_ASSERT_DEBUG(!((uintptr_t)next_alloc & alignment_rounder),
                     "alignment allocation rounding error?");
    result =
        VirtualAlloc(next_alloc, (bytes << 2), MEM_RESERVE, PAGE_READWRITE);
    next_alloc = (void *)((uintptr_t)next_alloc + (bytes << 2));
  } while (!result && (attempts++) < 1024);
  if (result) {
    result = VirtualAlloc(result, bytes, MEM_COMMIT, PAGE_READWRITE);
    FIO_ASSERT_DEBUG(result, "couldn't commit memory after reservation?!");

  } else {
    FIO_LOG_ERROR("Couldn't allocate memory from the system, error %zu."
                  "\n\t%zu attempts with final address %p",
                  GetLastError(),
                  attempts,
                  next_alloc);
  }
  return result;
}

FIO_IFUNC void *FIO_MEM_SYS_REALLOC_def_func(void *mem,
                                             size_t old_len,
                                             size_t new_len,
                                             uint8_t alignment_log) {
  if (!new_len)
    goto free_mem;
  old_len = FIO_MEM_BYTES2PAGES(old_len);
  new_len = FIO_MEM_BYTES2PAGES(new_len);
  if (new_len > old_len) {
    /* extend allocation */
    void *tmp = VirtualAlloc((void *)((uintptr_t)mem + old_len),
                             new_len - old_len,
                             MEM_COMMIT,
                             PAGE_READWRITE);
    if (tmp)
      return mem;
    /* Alloc, Copy, Free... sorry... */
    tmp = FIO_MEM_SYS_ALLOC_def_func(new_len, alignment_log);
    if (!tmp) {
      FIO_LOG_ERROR("sysem realloc failed to allocate memory.");
      return NULL;
    }
    fio_memcpy_aligned(tmp, mem, old_len);
    FIO_MEM_SYS_FREE_def_func(mem, old_len);
    mem = tmp;
  } else if (old_len > new_len) {
    /* shrink allocation */
    if (!VirtualFree((void *)((uintptr_t)mem + new_len),
                     old_len - new_len,
                     MEM_DECOMMIT))
      FIO_LOG_ERROR("failed to decommit memory range @ %p.", mem);
  }
  return mem;
free_mem:
  FIO_MEM_SYS_FREE_def_func(mem, old_len);
  mem = NULL;
  return NULL;
}

/* *****************************************************************************


Unknown OS... Unsupported?


***************************************************************************** */
#else /* FIO_OS_POSIX / FIO_OS_WIN */

FIO_IFUNC void *FIO_MEM_SYS_ALLOC_def_func(size_t bytes,
                                           uint8_t alignment_log) {
  // return aligned_alloc((pages << 12), (1UL << alignment_log));
  exit(-1);
  (void)bytes;
  (void)alignment_log;
}

FIO_IFUNC void *FIO_MEM_SYS_REALLOC_def_func(void *mem,
                                             size_t old_len,
                                             size_t new_len,
                                             uint8_t alignment_log) {
  (void)old_len;
  (void)alignment_log;
  new_len = FIO_MEM_BYTES2PAGES(new_len);
  return realloc(mem, new_len);
}

FIO_IFUNC void FIO_MEM_SYS_FREE_def_func(void *mem, size_t bytes) {
  free(mem);
  (void)bytes;
}

#endif /* FIO_OS_POSIX / FIO_OS_WIN */
/* *****************************************************************************
Overridable system allocation macros
***************************************************************************** */
#ifndef FIO_MEM_SYS_ALLOC
#define FIO_MEM_SYS_ALLOC(pages, alignment_log)                                \
  FIO_MEM_SYS_ALLOC_def_func((pages), (alignment_log))
#define FIO_MEM_SYS_REALLOC(ptr, old_pages, new_pages, alignment_log)          \
  FIO_MEM_SYS_REALLOC_def_func((ptr), (old_pages), (new_pages), (alignment_log))
#define FIO_MEM_SYS_FREE(ptr, pages) FIO_MEM_SYS_FREE_def_func((ptr), (pages))
#endif /* FIO_MEM_SYS_ALLOC */

#endif /* H___FIO_MEM_INCLUDE_ONCE___H */

/* *****************************************************************************
FIO_MEMORY_DISABLE - use the system allocator
***************************************************************************** */
#if defined(FIO_MEMORY_DISABLE) || defined(FIO_MALLOC_TMP_USE_SYSTEM)

SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, malloc)(size_t size) {
#if FIO_MEMORY_INITIALIZE_ALLOCATIONS
  return calloc(size, 1);
#else
  return malloc(size);
#endif
}
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME,
                                       calloc)(size_t size_per_unit,
                                               size_t unit_count) {
  return calloc(size_per_unit, unit_count);
}
SFUNC void FIO_NAME(FIO_MEMORY_NAME, free)(void *ptr) { free(ptr); }
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc)(void *ptr,
                                                             size_t new_size) {
  return realloc(ptr, new_size);
}
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc2)(void *ptr,
                                                              size_t new_size,
                                                              size_t copy_len) {
  return realloc(ptr, new_size);
  (void)copy_len;
}
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, mmap)(size_t size) {
  return calloc(size, 1);
}

SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_after_fork)(void) {}
/** Prints the allocator's data structure. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_state)(void) {}
/** Prints the allocator's free block list. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_free_block_list)(void) {}
/** Prints the settings used to define the allocator. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_settings)(void) {}
SFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_block_size)(void) { return 0; }

#ifdef FIO_TEST_CSTL
SFUNC void FIO_NAME_TEST(FIO_NAME(stl, FIO_MEMORY_NAME), mem)(void) {
  fprintf(stderr, "* Custom memory allocator bypassed.\n");
}
#endif /* FIO_TEST_CSTL */

#else /* FIO_MEMORY_DISABLE */

/* *****************************************************************************





                  Memory allocation implementation starts here
                    helper function and setup are complete





***************************************************************************** */

/* *****************************************************************************
memset / memcpy selectors
***************************************************************************** */

#if FIO_MEMORY_USE_FIO_MEMSET
#define FIO___MEMSET fio_memset_aligned
#else
#define FIO___MEMSET memset
#endif /* FIO_MEMORY_USE_FIO_MEMSET */

#if FIO_MEMORY_USE_FIO_MEMCOPY
#define FIO___MEMCPY2 fio_memcpy_aligned
#else
#define FIO___MEMCPY2 FIO_MEMCPY
#endif /* FIO_MEMORY_USE_FIO_MEMCOPY */

/* *****************************************************************************
Lock type choice
***************************************************************************** */
#if FIO_MEMORY_USE_THREAD_MUTEX
#define FIO_MEMORY_LOCK_TYPE fio_thread_mutex_t
#define FIO_MEMORY_LOCK_TYPE_INIT(lock)                                        \
  ((lock) = (fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT)
#define FIO_MEMORY_TRYLOCK(lock) fio_thread_mutex_trylock(&(lock))
#define FIO_MEMORY_LOCK(lock)    fio_thread_mutex_lock(&(lock))
#define FIO_MEMORY_UNLOCK(lock)                                                \
  do {                                                                         \
    int tmp__ = fio_thread_mutex_unlock(&(lock));                              \
    if (tmp__) {                                                               \
      FIO_LOG_ERROR("Couldn't free mutex! error (%d): %s",                     \
                    tmp__,                                                     \
                    strerror(tmp__));                                          \
    }                                                                          \
  } while (0)

#define FIO_MEMORY_LOCK_NAME "pthread_mutex"
#else
#define FIO_MEMORY_LOCK_TYPE            fio_lock_i
#define FIO_MEMORY_LOCK_TYPE_INIT(lock) ((lock) = FIO_LOCK_INIT)
#define FIO_MEMORY_TRYLOCK(lock)        fio_trylock(&(lock))
#define FIO_MEMORY_LOCK(lock)           fio_lock(&(lock))
#define FIO_MEMORY_UNLOCK(lock)         fio_unlock(&(lock))
#define FIO_MEMORY_LOCK_NAME            "facil.io spinlocks"
#endif

/* *****************************************************************************
Allocator debugging helpers
***************************************************************************** */

#if defined(DEBUG) || FIO_LEAK_COUNTER
/* maximum block allocation count. */
static size_t FIO_NAME(fio___,
                       FIO_NAME(FIO_MEMORY_NAME, state_chunk_count_max));
/* current block allocation count. */
static size_t FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count));

#define FIO_MEMORY_ON_CHUNK_ALLOC(ptr)                                         \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY SYS-ALLOC - retrieved %p from system", ptr);        \
    fio_atomic_add(                                                            \
        &FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)),       \
        1);                                                                    \
    if (FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)) >       \
        FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count_max)))    \
      FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count_max)) =     \
          FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count));      \
  } while (0)
#define FIO_MEMORY_ON_CHUNK_FREE(ptr)                                          \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY SYS-DEALLOC- returned %p to system", ptr);          \
    fio_atomic_sub_fetch(                                                      \
        &FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)),       \
        1);                                                                    \
  } while (0)
#define FIO_MEMORY_ON_CHUNK_CACHE(ptr)                                         \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY CACHE-DEALLOC placed %p in cache", ptr);            \
  } while (0);
#define FIO_MEMORY_ON_CHUNK_UNCACHE(ptr)                                       \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY CACHE-ALLOC retrieved %p from cache", ptr);         \
  } while (0);
#define FIO_MEMORY_ON_CHUNK_DIRTY(ptr)                                         \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY MARK-DIRTY placed %p in dirty list", ptr);          \
  } while (0);
#define FIO_MEMORY_ON_CHUNK_UNDIRTY(ptr)                                       \
  do {                                                                         \
    FIO_LOG_DEBUG2("MEMORY UNMARK-DIRTY retrieved %p from dirty list", ptr);   \
  } while (0);
#define FIO_MEMORY_ON_BLOCK_RESET_IN_LOCK(ptr, blk)                            \
  if (0)                                                                       \
    do {                                                                       \
      FIO_LOG_DEBUG2("MEMORY chunk %p block %zu reset in lock",                \
                     ptr,                                                      \
                     (size_t)blk);                                             \
    } while (0);

#define FIO_MEMORY_ON_BIG_BLOCK_SET(ptr)                                       \
  if (1)                                                                       \
    do {                                                                       \
      FIO_LOG_DEBUG2("MEMORY chunk %p used as big-block", ptr);                \
    } while (0);

#define FIO_MEMORY_ON_BIG_BLOCK_UNSET(ptr)                                     \
  if (1)                                                                       \
    do {                                                                       \
      FIO_LOG_DEBUG2("MEMORY chunk %p no longer used as big-block", ptr);      \
    } while (0);

#define FIO_MEMORY_PRINT_STATS()                                               \
  FIO_LOG_INFO(                                                                \
      "(" FIO_MACRO2STR(FIO_NAME(                                              \
          FIO_MEMORY_NAME,                                                     \
          malloc)) "):\n          "                                            \
                   "Total memory chunks allocated before cleanup %zu\n"        \
                   "          Maximum memory blocks allocated at a single "    \
                   "time %zu",                                                 \
      FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)),          \
      FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count_max)))
#define FIO_MEMORY_PRINT_STATS_END()                                           \
  do {                                                                         \
    if (FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count))) {      \
      FIO_LOG_ERROR(                                                           \
          "(" FIO_MACRO2STR(                                                   \
              FIO_NAME(FIO_MEMORY_NAME,                                        \
                       malloc)) "):\n          "                               \
                                "Total memory chunks allocated "               \
                                "after cleanup (POSSIBLE LEAKS): %zu\n",       \
          FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)));     \
    }                                                                          \
  } while (0)
#else /* defined(DEBUG) || defined(FIO_LEAK_COUNTER) */
#define FIO_MEMORY_ON_CHUNK_ALLOC(ptr)
#define FIO_MEMORY_ON_CHUNK_FREE(ptr)
#define FIO_MEMORY_ON_CHUNK_CACHE(ptr)
#define FIO_MEMORY_ON_CHUNK_UNCACHE(ptr)
#define FIO_MEMORY_ON_CHUNK_DIRTY(ptr)
#define FIO_MEMORY_ON_CHUNK_UNDIRTY(ptr)
#define FIO_MEMORY_ON_BLOCK_RESET_IN_LOCK(ptr, blk)
#define FIO_MEMORY_ON_BIG_BLOCK_SET(ptr)
#define FIO_MEMORY_ON_BIG_BLOCK_UNSET(ptr)
#define FIO_MEMORY_PRINT_STATS()
#define FIO_MEMORY_PRINT_STATS_END()
#endif /* defined(DEBUG) || defined(FIO_LEAK_COUNTER) */

/* *****************************************************************************






Memory chunk headers and block data (in chunk header)






***************************************************************************** */

/* *****************************************************************************
Chunk and Block data / header
***************************************************************************** */

typedef struct {
  volatile int32_t ref;
  volatile int32_t pos;
} FIO_NAME(FIO_MEMORY_NAME, __mem_block_s);

typedef struct {
  /* the head of the chunk... node->next says a lot */
  uint32_t marker;
  volatile int32_t ref;
  FIO_NAME(FIO_MEMORY_NAME, __mem_block_s)
  blocks[FIO_MEMORY_BLOCKS_PER_ALLOCATION];
} FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s);

#if FIO_MEMORY_ENABLE_BIG_ALLOC
/* big-blocks consumes a chunk, sizeof header MUST be <= chunk header */
typedef struct {
  /* marker and ref MUST overlay chunk header */
  uint32_t marker;
  volatile int32_t ref;
  volatile int32_t pos;
} FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s);
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */

/* *****************************************************************************
Arena type
***************************************************************************** */
typedef struct {
  void *block;
  int32_t last_pos;
  FIO_MEMORY_LOCK_TYPE lock;
} FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s);

/* *****************************************************************************
Allocator State
***************************************************************************** */

typedef struct FIO_NAME(FIO_MEMORY_NAME, __mem_state_s)
    FIO_NAME(FIO_MEMORY_NAME, __mem_state_s);

static struct FIO_NAME(FIO_MEMORY_NAME, __mem_state_s) {
#if FIO_MEMORY_CACHE_SLOTS
  /** cache array container for available memory chunks */
  struct {
    /* chunk slot array */
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * a[FIO_MEMORY_CACHE_SLOTS];
    size_t pos;
  } cache;
#endif /* FIO_MEMORY_CACHE_SLOTS */

#if FIO_MEMORY_ENABLE_BIG_ALLOC
  /** a block for big allocations, shared (no arena) */
  FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) * big_block;
  int32_t big_last_pos;
  /** big allocation lock */
  FIO_MEMORY_LOCK_TYPE big_lock;
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */
  /** main memory state lock */
  FIO_MEMORY_LOCK_TYPE lock;
  /** free list for available blocks */
  FIO_LIST_HEAD blocks;
  /** the arena count for the allocator */
  size_t arena_count;
  FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s) arena[];
} * FIO_NAME(FIO_MEMORY_NAME, __mem_state);

/* *****************************************************************************
Arena assignment
***************************************************************************** */

/* SublimeText marker */
void fio___mem_arena_unlock___(void);
/** Unlocks the thread's arena. */
FIO_SFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_arena_unlock)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s) * a) {
  FIO_ASSERT_DEBUG(a, "unlocking a NULL arena?!");
  FIO_MEMORY_UNLOCK(a->lock);
}

/* SublimeText marker */
void fio___mem_arena_lock___(void);
/** Locks and returns the thread's arena. */
FIO_SFUNC FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s) *
    FIO_NAME(FIO_MEMORY_NAME, __mem_arena_lock)(void) {
#if FIO_MEMORY_ARENA_COUNT == 1
  FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[0].lock);
  return FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena;

#else /* FIO_MEMORY_ARENA_COUNT != 1 */

#if defined(DEBUG) && FIO_MEMORY_ARENA_COUNT > 0 && !defined(FIO_TEST_CSTL)
  static size_t warning_printed = 0;
#endif
  /** thread arena value */
  size_t thread_default_arena;
  {
    /* select the default arena selection using a thread ID. */
    union {
      void *p;
      fio_thread_t t;
    } u = {.t = fio_thread_current()};
    thread_default_arena = (size_t)fio_risky_ptr(u.p) %
                           FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
    // FIO_LOG_DEBUG("thread %p (%p) associated with arena %zu / %zu",
    //               u.p,
    //               (void *)fio_risky_ptr(u.p),
    //               thread_default_arena,
    //               (size_t)FIO_NAME(FIO_MEMORY_NAME,
    //               __mem_state)->arena_count);
  }
  for (;;) {
    /* rotate all arenas to find one that's available */
    for (size_t i = 0; i < FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
         ++i) {
      /* first attempt is the last used arena, then cycle with offset */
      size_t index = i + thread_default_arena;
      index %= FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;

      if (FIO_MEMORY_TRYLOCK(
              FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[index].lock))
        continue;
      return (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena + index);
    }
#if defined(DEBUG) && FIO_MEMORY_ARENA_COUNT > 0 && !defined(FIO_TEST_CSTL)
    if (!warning_printed)
      FIO_LOG_WARNING(FIO_MACRO2STR(
          FIO_NAME(FIO_MEMORY_NAME,
                   malloc)) " high arena contention.\n"
                            "          Consider recompiling with more arenas.");
    warning_printed = 1;
#endif /* DEBUG */
#if FIO_MEMORY_USE_THREAD_MUTEX && FIO_OS_POSIX
    /* slow wait for last arena used by the thread */
    FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)
                        ->arena[thread_default_arena]
                        .lock);
    return FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena + thread_default_arena;
#else
    // FIO_THREAD_RESCHEDULE();
#endif /* FIO_MEMORY_USE_THREAD_MUTEX */
  }
#endif /* FIO_MEMORY_ARENA_COUNT != 1 */
}

/* *****************************************************************************
Converting between chunk & block data to pointers (and back)
***************************************************************************** */

#define FIO_MEMORY_HEADER_SIZE                                                 \
  ((sizeof(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s)) +                         \
    (FIO_MEMORY_ALIGN_SIZE - 1)) &                                             \
   (~(FIO_MEMORY_ALIGN_SIZE - 1)))

#define FIO_MEMORY_BLOCK_SIZE                                                  \
  (((FIO_MEMORY_SYS_ALLOCATION_SIZE - FIO_MEMORY_HEADER_SIZE) /                \
    FIO_MEMORY_BLOCKS_PER_ALLOCATION) &                                        \
   (~(FIO_MEMORY_ALIGN_SIZE - 1)))

#define FIO_MEMORY_UNITS_PER_BLOCK                                             \
  (FIO_MEMORY_BLOCK_SIZE / FIO_MEMORY_ALIGN_SIZE)

/* SublimeText marker */
void fio___mem_chunk2ptr___(void);
/** returns a pointer within a chunk, given it's block and offset value. */
FIO_IFUNC void *FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c,
    size_t block,
    size_t offset) {
  return (void *)(((uintptr_t)(c) + FIO_MEMORY_HEADER_SIZE) +
                  (block * FIO_MEMORY_BLOCK_SIZE) +
                  (offset << FIO_MEMORY_ALIGN_LOG));
}

/* SublimeText marker */
void fio___mem_ptr2chunk___(void);
/** returns a chunk given a pointer. */
FIO_IFUNC FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *
    FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(void *p) {
  return FIO_PTR_MATH_RMASK(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s),
                            p,
                            FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG);
}

/* SublimeText marker */
void fio___mem_ptr2index___(void);
/** returns a pointer's block index within it's chunk. */
FIO_IFUNC size_t FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c,
    void *p) {
  FIO_ASSERT_DEBUG(c == FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(p),
                   "chunk-pointer offset argument error");
  size_t i =
      (size_t)FIO_PTR_MATH_LMASK(void, p, FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG);
  i -= FIO_MEMORY_HEADER_SIZE;
  i /= FIO_MEMORY_BLOCK_SIZE;
  return i;
  (void)c;
}

/* *****************************************************************************
Allocator State Initialization & Cleanup
***************************************************************************** */
#define FIO_MEMORY_STATE_SIZE(arean_count)                                     \
  FIO_MEM_BYTES2PAGES(                                                         \
      (sizeof(*FIO_NAME(FIO_MEMORY_NAME, __mem_state)) +                       \
       (sizeof(FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s)) * (arean_count))))

/* function declarations for functions called during cleanup */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_dealloc)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c);
FIO_IFUNC void *FIO_NAME(FIO_MEMORY_NAME, __mem_block_new)(void);
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)(void *ptr);
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_free)(void *ptr);
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_free)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c);

/* SublimeText marker */
void fio___mem_state_cleanup___(void);
FIO_DESTRUCTOR(FIO_NAME(FIO_MEMORY_NAME, __mem_state_cleanup)) {
  if (!FIO_NAME(FIO_MEMORY_NAME, __mem_state))
    return;

#if DEBUG
  FIO_LOG_INFO("starting facil.io memory allocator cleanup for " FIO_MACRO2STR(
      FIO_NAME(FIO_MEMORY_NAME, malloc)) ".");
#endif /* DEBUG */
  FIO_MEMORY_PRINT_STATS();
  /* free arena blocks */
  for (size_t i = 0; i < FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
       ++i) {
    if (FIO_MEMORY_TRYLOCK(
            FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].lock)) {
      FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].lock);
      FIO_LOG_ERROR(FIO_MACRO2STR(
          FIO_NAME(FIO_MEMORY_NAME,
                   malloc)) "cleanup called while some arenas are in use!");
    }
    FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].lock);
    FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)
    (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block);
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block = NULL;
    FIO_MEMORY_LOCK_TYPE_INIT(
        FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].lock);
  }

#if FIO_MEMORY_ENABLE_BIG_ALLOC
  /* cleanup big-alloc chunk */
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block) {
    if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block->ref > 1) {
      FIO_LOG_WARNING("(" FIO_MACRO2STR(FIO_NAME(
          FIO_MEMORY_NAME,
          malloc)) ") active big-block reference count error at %p\n"
                   "          Possible memory leaks for big-block allocation.");
    }
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_free)
    (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block);
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block = NULL;
    FIO_MEMORY_LOCK_TYPE_INIT(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
  }
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */

#if FIO_MEMORY_CACHE_SLOTS
  /* deallocate all chunks in the cache */
  while (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos) {
    const size_t pos = --FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos;
    FIO_MEMORY_ON_CHUNK_UNCACHE(
        FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.a[pos]);
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_dealloc)
    (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.a[pos]);
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.a[pos] = NULL;
  }
#endif /* FIO_MEMORY_CACHE_SLOTS */

  /* report any blocks in the allocation list - even if not in DEBUG mode */
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks.next !=
      &FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks) {
    struct t_s {
      FIO_LIST_NODE node;
    };
    void *last_chunk = NULL;
    FIO_LOG_WARNING("(" FIO_MACRO2STR(
        FIO_NAME(FIO_MEMORY_NAME,
                 malloc)) ") blocks left after cleanup - memory leaks?");
    FIO_LIST_EACH(struct t_s,
                  node,
                  &FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks,
                  pos) {
      if (last_chunk == (void *)FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(pos))
        continue;
      last_chunk = (void *)FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(pos);
      FIO_LOG_WARNING(
          "(" FIO_MACRO2STR(FIO_NAME(FIO_MEMORY_NAME,
                                     malloc)) ") leaked block(s) for chunk %p",
          (void *)pos,
          last_chunk);
    }
  }

  /* dealloc the state machine */
  const size_t s = FIO_MEMORY_STATE_SIZE(
      FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count);
  FIO_MEM_SYS_FREE(FIO_NAME(FIO_MEMORY_NAME, __mem_state), s);
  FIO_NAME(FIO_MEMORY_NAME, __mem_state) =
      (FIO_NAME(FIO_MEMORY_NAME, __mem_state_s *))NULL;

  FIO_MEMORY_PRINT_STATS_END();
#if DEBUG && defined(FIO_LOG_INFO)
  FIO_LOG_INFO("finished facil.io memory allocator cleanup for " FIO_MACRO2STR(
      FIO_NAME(FIO_MEMORY_NAME, malloc)) ".");
#endif /* DEBUG */
}

/* initializes (allocates) the arenas and state machine */
FIO_CONSTRUCTOR(FIO_NAME(FIO_MEMORY_NAME, __mem_state_setup)) {
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state))
    return;
  /* allocate the state machine */
  {
#if FIO_MEMORY_ARENA_COUNT > 0
    size_t const arean_count = FIO_MEMORY_ARENA_COUNT;
#else
    size_t arean_count = FIO_MEMORY_ARENA_COUNT_FALLBACK;
#ifdef _SC_NPROCESSORS_ONLN
    arean_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (arean_count == (size_t)-1UL)
      arean_count = FIO_MEMORY_ARENA_COUNT_FALLBACK;
#else
#if _MSC_VER
#pragma message(                                                               \
    "Dynamic CPU core count is unavailable - assuming FIO_MEMORY_ARENA_COUNT_FALLBACK cores.")
#else
#warning Dynamic CPU core count is unavailable - assuming FIO_MEMORY_ARENA_COUNT_FALLBACK cores.
#endif
#endif /* _SC_NPROCESSORS_ONLN */

    if (arean_count >= FIO_MEMORY_ARENA_COUNT_MAX)
      arean_count = FIO_MEMORY_ARENA_COUNT_MAX;

#endif /* FIO_MEMORY_ARENA_COUNT > 0 */

    const size_t s = FIO_MEMORY_STATE_SIZE(arean_count);
    FIO_NAME(FIO_MEMORY_NAME, __mem_state) =
        (FIO_NAME(FIO_MEMORY_NAME, __mem_state_s *))FIO_MEM_SYS_ALLOC(s, 0);
    FIO_ASSERT_ALLOC(FIO_NAME(FIO_MEMORY_NAME, __mem_state));
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count = arean_count;
  }
  FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks =
      FIO_LIST_INIT(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks);
  FIO_NAME(FIO_MEMORY_NAME, malloc_after_fork)();

#if defined(FIO_MEMORY_WARMUP) && FIO_MEMORY_WARMUP
  for (size_t i = 0; i < (size_t)FIO_MEMORY_WARMUP &&
                     i < FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
       ++i) {
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block =
        FIO_NAME(FIO_MEMORY_NAME, __mem_block_new)();
  }
#endif
#ifdef DEBUG
  FIO_NAME(FIO_MEMORY_NAME, malloc_print_settings)();
#endif /* DEBUG */
  (void)FIO_NAME(FIO_MEMORY_NAME, malloc_print_free_block_list);
  (void)FIO_NAME(FIO_MEMORY_NAME, malloc_print_state);
  (void)FIO_NAME(FIO_MEMORY_NAME, malloc_print_settings);
}

/* SublimeText marker */
void fio_after_fork___(void);
/**
 * When forking is called manually, call this function to reset the facil.io
 * memory allocator's locks.
 */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_after_fork)(void) {
  if (!FIO_NAME(FIO_MEMORY_NAME, __mem_state)) {
    FIO_NAME(FIO_MEMORY_NAME, __mem_state_setup)();
    return;
  }
  FIO_LOG_DEBUG2("MEMORY reinitializeing " FIO_MACRO2STR(
      FIO_NAME(FIO_MEMORY_NAME, malloc)) " state");
  FIO_MEMORY_LOCK_TYPE_INIT(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
#if FIO_MEMORY_ENABLE_BIG_ALLOC
  FIO_MEMORY_LOCK_TYPE_INIT(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */
  for (size_t i = 0; i < FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
       ++i) {
    FIO_MEMORY_LOCK_TYPE_INIT(
        FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].lock);
  }
}

/* *****************************************************************************
Memory Allocation - state printing (debug helper)
***************************************************************************** */

/* SublimeText marker */
void fio_malloc_print_state___(void);
/** Prints the allocator's data structure. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_state)(void) {
  fprintf(
      stderr,
      FIO_MACRO2STR(FIO_NAME(FIO_MEMORY_NAME, malloc)) " allocator state:\n");
  for (size_t i = 0; i < FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count;
       ++i) {
    fprintf(stderr,
            "\t* arena[%zu] block: %p\n",
            i,
            (void *)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block);
    if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block) {
      FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c =
          FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(
              FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block);
      size_t b = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(
          c,
          FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena[i].block);
      fprintf(stderr, "\t\tchunk-ref: %zu (%p)\n", (size_t)c->ref, (void *)c);
      fprintf(stderr,
              "\t\t- block[%zu]-ref: %zu\n"
              "\t\t- block[%zu]-pos: %zu\n",
              b,
              (size_t)c->blocks[b].ref,
              b,
              (size_t)c->blocks[b].pos);
    }
  }
#if FIO_MEMORY_ENABLE_BIG_ALLOC
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block) {
    fprintf(stderr, "\t---big allocations---\n");
    fprintf(stderr,
            "\t* big-block: %p\n"
            "\t\t ref: %zu\n"
            "\t\t pos: %zu\n",
            (void *)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block,
            (size_t)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block->ref,
            (size_t)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block->pos);
  } else {
    fprintf(stderr,
            "\t---big allocations---\n"
            "\t* big-block: NULL\n");
  }

#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */

#if FIO_MEMORY_CACHE_SLOTS
  fprintf(stderr, "\t---caches---\n");
  for (size_t i = 0; i < FIO_MEMORY_CACHE_SLOTS; ++i) {
    fprintf(stderr,
            "\t* cache[%zu] chunk: %p\n",
            i,
            (void *)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.a[i]);
  }
#endif /* FIO_MEMORY_CACHE_SLOTS */
}

void fio_malloc_print_free_block_list___(void);
/** Prints the allocator's free block list. May be used for debugging. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_free_block_list)(void) {
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks.prev ==
      &FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks)
    return;
  fprintf(stderr,
          FIO_MACRO2STR(FIO_NAME(FIO_MEMORY_NAME,
                                 malloc)) " allocator free block list:\n");
  FIO_LIST_NODE *n = FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks.prev;
  for (size_t i = 0; n != &FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks;
       ++i) {
    fprintf(stderr, "\t[%zu] %p\n", i, (void *)n);
    n = n->prev;
  }
}

/* *****************************************************************************
chunk allocation / deallocation
***************************************************************************** */

/* SublimeText marker */
void fio___mem_chunk_dealloc___(void);
/* returns memory to system */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_dealloc)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c) {
  if (!c)
    return;
  FIO_MEM_SYS_FREE(((void *)c), FIO_MEMORY_SYS_ALLOCATION_SIZE);
  FIO_MEMORY_ON_CHUNK_FREE(c);
}

FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_cache_or_dealloc)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c) {
#if FIO_MEMORY_CACHE_SLOTS
  /* place in cache...? */
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos <
      FIO_MEMORY_CACHE_SLOTS) {
    FIO_MEMORY_ON_CHUNK_CACHE(c);
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)
        ->cache.a[FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos++] = c;
    c = NULL;
  }
#endif /* FIO_MEMORY_CACHE_SLOTS */

  FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_dealloc)(c);
}

FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_free)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c) {
  /* should we free the chunk? */
  if (!c || fio_atomic_sub_fetch(&c->ref, 1)) {
    FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
    return;
  }

  /* remove all blocks from the block allocation list */
  for (size_t b = 0; b < FIO_MEMORY_BLOCKS_PER_ALLOCATION; ++b) {
    FIO_LIST_NODE *n =
        (FIO_LIST_NODE *)FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0);
    if (n->prev && n->next) {
      FIO_LIST_REMOVE(n);
      n->prev = n->next = NULL;
    }
  }
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_cache_or_dealloc)(c);
}

/* SublimeText marker */
void fio___mem_chunk_new___(void);
/* UNSAFE! returns a clean chunk (cache / allocation). */
FIO_IFUNC FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_new)(const size_t needs_lock) {
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c = NULL;
#if FIO_MEMORY_CACHE_SLOTS
  /* cache allocation */
  if (needs_lock) {
    FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  }
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos) {
    c = FIO_NAME(FIO_MEMORY_NAME, __mem_state)
            ->cache.a[--FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos];
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)
        ->cache.a[FIO_NAME(FIO_MEMORY_NAME, __mem_state)->cache.pos] = NULL;
  }
  if (needs_lock) {
    FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  }
  if (c) {
    FIO_MEMORY_ON_CHUNK_UNCACHE(c);
    *c = (FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s)){.ref = 1};
    return c;
  }
#endif /* FIO_MEMORY_CACHE_SLOTS */

  /* system allocation */
  c = (FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)FIO_MEM_SYS_ALLOC(
      FIO_MEMORY_SYS_ALLOCATION_SIZE,
      FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG);

  if (!c)
    return c;
  FIO_MEMORY_ON_CHUNK_ALLOC(c);
  c->ref = 1;
  return c;
  (void)needs_lock; /* in case it isn't used */
}

/* *****************************************************************************
block allocation / deallocation
***************************************************************************** */

FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_block__reset_memory)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c,
    size_t b) {
#if FIO_MEMORY_INITIALIZE_ALLOCATIONS
  if (c->blocks[b].pos >= (int32_t)(FIO_MEMORY_UNITS_PER_BLOCK - 4)) {
    /* zero out the whole block */
    FIO___MEMSET(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0),
                 0,
                 FIO_MEMORY_BLOCK_SIZE);
  } else {
    /* zero out only the memory that was used */
    FIO___MEMSET(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0),
                 0,
                 (((size_t)c->blocks[b].pos) << FIO_MEMORY_ALIGN_LOG));
  }
#else
  /** only reset a block's free-list header */
  FIO___MEMSET(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0),
               0,
               (((FIO_MEMORY_ALIGN_SIZE - 1) + sizeof(FIO_LIST_NODE)) &
                (~(FIO_MEMORY_ALIGN_SIZE - 1))));
#endif /*FIO_MEMORY_INITIALIZE_ALLOCATIONS*/
  c->blocks[b].pos = 0;
}

/* SublimeText marker */
void fio___mem_block_free___(void);
/** frees a block / decreases it's reference count */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)(void *p) {
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c =
      FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(p);
  size_t b = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(c, p);
  if (!c || fio_atomic_sub_fetch(&c->blocks[b].ref, 1))
    return;

  /* reset memory */
  FIO_NAME(FIO_MEMORY_NAME, __mem_block__reset_memory)(c, b);

  /* place in free list */
  FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  FIO_LIST_NODE *n =
      (FIO_LIST_NODE *)FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0);
  FIO_LIST_PUSH(&FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks, n);
  /* free chunk reference while in locked state */
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_free)(c);
}

/* SublimeText marker */
void fio___mem_block_new___(void);
/** returns a new block with a reference count of 1 */
FIO_IFUNC void *FIO_NAME(FIO_MEMORY_NAME, __mem_block_new)(void) {
  void *p = NULL;
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c = NULL;
  size_t b;

  FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);

  /* try to collect from list */
  if (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks.prev !=
      &FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks) {
    FIO_LIST_NODE *n = FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks.prev;
    FIO_LIST_REMOVE(n);
    n->next = n->prev = NULL;
    c = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)((void *)n);
    fio_atomic_add_fetch(&c->ref, 1);
    p = (void *)n;
    b = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(c, p);
    goto done;
  }

  /* allocate from cache / system (sets chunk reference to 1) */
  c = FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_new)(0);
  if (!c)
    goto done;

  /* use the first block in the chunk as the new block */
  p = FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, 0, 0);

  /* place the rest of the blocks in the block allocation list */
  for (b = 1; b < FIO_MEMORY_BLOCKS_PER_ALLOCATION; ++b) {
    FIO_LIST_NODE *n =
        (FIO_LIST_NODE *)FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0);
    FIO_LIST_PUSH(&FIO_NAME(FIO_MEMORY_NAME, __mem_state)->blocks, n);
  }
  /* set block index to zero */
  b = 0;

done:
  FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  if (!p)
    return p;
  /* update block reference and allocation position */
  c->blocks[b].ref = 1;
  c->blocks[b].pos = 0;
  return p;
}

/* *****************************************************************************
Small allocation internal API
***************************************************************************** */

/* SublimeText marker */
void fio___mem_slice_new___(void);
/** slice a block to allocate a set number of bytes. */
FIO_SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME,
                                           __mem_slice_new)(size_t bytes,
                                                            void *is_realloc) {
  int32_t last_pos = 0;
  void *p = NULL;
  bytes = (bytes + ((1UL << FIO_MEMORY_ALIGN_LOG) - 1)) >> FIO_MEMORY_ALIGN_LOG;
  FIO_NAME(FIO_MEMORY_NAME, __mem_arena_s) *a =
      FIO_NAME(FIO_MEMORY_NAME, __mem_arena_lock)();

  if (!a->block)
    a->block = FIO_NAME(FIO_MEMORY_NAME, __mem_block_new)();
  else if (is_realloc)
    last_pos = a->last_pos;
  for (;;) {
    if (!a->block)
      goto no_mem;
    void *const block = a->block;

    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *const c =
        FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(block);
    const size_t b = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(c, block);

    /* if we are the only thread holding a reference to this block... reset. */
    if (fio_atomic_add(&c->blocks[b].ref, 1) == 1 && c->blocks[b].pos) {
      FIO_NAME(FIO_MEMORY_NAME, __mem_block__reset_memory)(c, b);
      FIO_MEMORY_ON_BLOCK_RESET_IN_LOCK(c, b);
    }

    /* a lucky realloc? */
    if (last_pos && is_realloc == FIO_NAME(FIO_MEMORY_NAME,
                                           __mem_chunk2ptr)(c, b, last_pos)) {
      c->blocks[b].pos = bytes + last_pos;
      fio_atomic_sub(&c->blocks[b].ref, 1);
      FIO_NAME(FIO_MEMORY_NAME, __mem_arena_unlock)(a);
      return is_realloc;
    }

    /* enough space? allocate */
    if (c->blocks[b].pos + bytes < FIO_MEMORY_UNITS_PER_BLOCK) {
      p = FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, c->blocks[b].pos);
      a->last_pos = c->blocks[b].pos;
      c->blocks[b].pos += bytes;
      FIO_NAME(FIO_MEMORY_NAME, __mem_arena_unlock)(a);
      return p;
    }
    /* release reference added */
    if (is_realloc)
      fio_atomic_sub(&c->blocks[b].ref, 1);
    else
      FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)(a->block);

    /*
     * allocate a new block before freeing the existing block
     * this prevents the last chunk from de-allocating and reallocating
     */
    a->block = FIO_NAME(FIO_MEMORY_NAME, __mem_block_new)();
    last_pos = 0;
    /* release the reference held by the allocator */
    FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)(block);
  }

no_mem:
  FIO_NAME(FIO_MEMORY_NAME, __mem_arena_unlock)(a);
  return p;
}

/* SublimeText marker */
void fio_____mem_slice_free___(void);
/** slice a block to allocate a set number of bytes. */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_slice_free)(void *p) {
  FIO_NAME(FIO_MEMORY_NAME, __mem_block_free)(p);
}

/* *****************************************************************************
big block allocation / de-allocation
***************************************************************************** */
#if FIO_MEMORY_ENABLE_BIG_ALLOC

#define FIO_MEMORY_BIG_BLOCK_MARKER ((~(uint32_t)0) << 2)
#define FIO_MEMORY_BIG_BLOCK_HEADER_SIZE                                       \
  (((sizeof(FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s)) +                    \
     ((FIO_MEMORY_ALIGN_SIZE - 1))) &                                          \
    ((~(0UL)) << FIO_MEMORY_ALIGN_LOG)))

#define FIO_MEMORY_BIG_BLOCK_SIZE                                              \
  (FIO_MEMORY_SYS_ALLOCATION_SIZE - FIO_MEMORY_BIG_BLOCK_HEADER_SIZE)

#define FIO_MEMORY_UNITS_PER_BIG_BLOCK                                         \
  (FIO_MEMORY_BIG_BLOCK_SIZE / FIO_MEMORY_ALIGN_SIZE)

/* SublimeText marker */
void fio___mem_big_block__reset_memory___(void);
/** zeros out a big-block's memory, keeping it's reference count at 1. */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_big_block__reset_memory)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) * b) {

#if FIO_MEMORY_INITIALIZE_ALLOCATIONS
  /* zero out memory */
  if (b->pos >= (int32_t)(FIO_MEMORY_UNITS_PER_BIG_BLOCK - 10)) {
    /* zero out everything */
    FIO___MEMSET((void *)b, 0, FIO_MEMORY_SYS_ALLOCATION_SIZE);
  } else {
    /* zero out only the used part of the memory */
    FIO___MEMSET((void *)b,
                 0,
                 (((size_t)b->pos << FIO_MEMORY_ALIGN_LOG) +
                  FIO_MEMORY_BIG_BLOCK_HEADER_SIZE));
  }
#else
  /* reset chunk header, which is always bigger than big_block header*/
  FIO___MEMSET((void *)b, 0, sizeof(FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s)));
  /* zero out possible block memory (if required) */
  for (size_t i = 0; i < FIO_MEMORY_BLOCKS_PER_ALLOCATION; ++i) {
    FIO_NAME(FIO_MEMORY_NAME, __mem_block__reset_memory)
    ((FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)b, i);
  }
#endif /* FIO_MEMORY_INITIALIZE_ALLOCATIONS */
  b->ref = 1;
}

/* SublimeText marker */
void fio___mem_big_block_free___(void);
/** frees a block / decreases it's reference count */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_free)(void *p) {
  // FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s)      ;
  FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *b =
      (FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *)
          FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(p);
  /* should we free the block? */
  if (!b || fio_atomic_sub_fetch(&b->ref, 1))
    return;
  FIO_MEMORY_ON_BIG_BLOCK_UNSET(b);

  /* zero out memory */
  FIO_NAME(FIO_MEMORY_NAME, __mem_big_block__reset_memory)(b);
#if FIO_MEMORY_CACHE_SLOTS
  /* lock for chunk de-allocation review () */
  FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->lock);
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_cache_or_dealloc)
  ((FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)b);
#else
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_dealloc)
  ((FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)b);
#endif
}

/* SublimeText marker */
void fio___mem_big_block_new___(void);
/** returns a new block with a reference count of 1 */
FIO_IFUNC FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_new)(void) {
  FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *b =
      (FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *)
          FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_new)(1);
  if (!b)
    return b;
  b->marker = FIO_MEMORY_BIG_BLOCK_MARKER;
  b->ref = 1;
  b->pos = 0;
  FIO_MEMORY_ON_BIG_BLOCK_SET(b);
  return b;
}

/* *****************************************************************************
Big allocation internal API
***************************************************************************** */

/* SublimeText marker */
void fio___mem_big2ptr___(void);
/** returns a pointer within a chunk, given it's block and offset value. */
FIO_IFUNC void *FIO_NAME(FIO_MEMORY_NAME, __mem_big2ptr)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) * b,
    size_t offset) {
  return (void *)(((uintptr_t)(b) + FIO_MEMORY_BIG_BLOCK_HEADER_SIZE) +
                  (offset << FIO_MEMORY_ALIGN_LOG));
}

/* SublimeText marker */
void fio___mem_big_slice_new___(void);
FIO_SFUNC void *FIO_MEM_ALIGN_NEW
FIO_NAME(FIO_MEMORY_NAME, __mem_big_slice_new)(size_t bytes, void *is_realloc) {
  int32_t last_pos = 0;
  void *p = NULL;
  bytes = (bytes + ((1UL << FIO_MEMORY_ALIGN_LOG) - 1)) >> FIO_MEMORY_ALIGN_LOG;
  for (;;) {
    FIO_MEMORY_LOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
    if (!FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block)
      FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block =
          FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_new)();
    else if (is_realloc)
      last_pos = FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_last_pos;
    if (!FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block)
      goto done;
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_s) *b =
        FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block;

    /* are we the only thread holding a reference to this block... reset? */
    if (b->ref == 1 && b->pos) {
      FIO_NAME(FIO_MEMORY_NAME, __mem_big_block__reset_memory)(b);
      FIO_MEMORY_ON_BLOCK_RESET_IN_LOCK(b, 0);
      b->marker = FIO_MEMORY_BIG_BLOCK_MARKER;
    }

    /* a lucky realloc? */
    if (last_pos &&
        is_realloc == FIO_NAME(FIO_MEMORY_NAME, __mem_big2ptr)(b, last_pos)) {
      FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_last_pos = bytes + last_pos;
      FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
      return is_realloc;
    }

    /* enough space? */
    if (b->pos + bytes < FIO_MEMORY_UNITS_PER_BIG_BLOCK) {
      p = FIO_NAME(FIO_MEMORY_NAME, __mem_big2ptr)(b, b->pos);
      fio_atomic_add(&b->ref, 1); /* keep inside lock to enable reset */
      FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_last_pos = b->pos;
      b->pos += bytes;
      FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
      return p;
    }

    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_block = NULL;
    FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_last_pos = 0;
    FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_free)(b);
  }
done:
  FIO_MEMORY_UNLOCK(FIO_NAME(FIO_MEMORY_NAME, __mem_state)->big_lock);
  return p;
}

/* SublimeText marker */
void fio_____mem_big_slice_free___(void);
/** slice a block to allocate a set number of bytes. */
FIO_IFUNC void FIO_NAME(FIO_MEMORY_NAME, __mem_big_slice_free)(void *p) {
  FIO_NAME(FIO_MEMORY_NAME, __mem_big_block_free)(p);
}

#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */
/* *****************************************************************************
Memory Allocation - malloc(0) pointer
***************************************************************************** */

static long double FIO_NAME(
    FIO_MEMORY_NAME,
    malloc_zero)[((1UL << (FIO_MEMORY_ALIGN_LOG)) / sizeof(long double)) + 1];

#define FIO_MEMORY_MALLOC_ZERO_POINTER                                         \
  ((void *)(((uintptr_t)FIO_NAME(FIO_MEMORY_NAME, malloc_zero) +               \
             (FIO_MEMORY_ALIGN_SIZE - 1)) &                                    \
            ((~(uintptr_t)0) << FIO_MEMORY_ALIGN_LOG)))

/* *****************************************************************************
Memory Allocation - API implementation - debugging and info
***************************************************************************** */

/* SublimeText marker */
void fio_malloc_block_size___(void);
/* public API obligation */
SFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_block_size)(void) {
  return FIO_MEMORY_BLOCK_SIZE;
}

void fio_malloc_arenas___(void);
SFUNC size_t FIO_NAME(FIO_MEMORY_NAME, malloc_arenas)(void) {
  return FIO_NAME(FIO_MEMORY_NAME, __mem_state)
             ? FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count
             : 0;
}

SFUNC void FIO_NAME(FIO_MEMORY_NAME, malloc_print_settings)(void) {
  // FIO_LOG_DEBUG2(
  fprintf(
      stderr,
      "Custom memory allocator " FIO_MACRO2STR(FIO_NAME(
          FIO_MEMORY_NAME,
          malloc)) " initialized with:\n"
                   "\t* system allocation arenas:                 %zu arenas\n"
                   "\t* system allocation size:                   %zu bytes\n"
                   "\t* system allocation overhead (theoretical): %zu bytes\n"
                   "\t* system allocation overhead (actual):      %zu bytes\n"
                   "\t* cached system allocations (max):          %zu units\n"
                   "\t* memory block size:                        %zu bytes\n"
                   "\t* blocks per system allocation:             %zu blocks\n"
                   "\t* allocation units per block:               %zu units\n"
                   "\t* arena per-allocation limit:               %zu bytes\n"
                   "\t* local per-allocation limit (before mmap): %zu bytes\n"
                   "\t* malloc(0) pointer:                        %p\n"
                   "\t* always initializes memory  (zero-out):    %s\n"
                   "\t* " FIO_MEMORY_LOCK_NAME " locking system\n",
      (size_t)FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count,
      (size_t)FIO_MEMORY_SYS_ALLOCATION_SIZE,
      (size_t)FIO_MEMORY_HEADER_SIZE,
      (size_t)FIO_MEMORY_SYS_ALLOCATION_SIZE % (size_t)FIO_MEMORY_BLOCK_SIZE,
      (size_t)FIO_MEMORY_CACHE_SLOTS,
      (size_t)FIO_MEMORY_BLOCK_SIZE,
      (size_t)FIO_MEMORY_BLOCKS_PER_ALLOCATION,
      (size_t)FIO_MEMORY_UNITS_PER_BLOCK,
      (size_t)FIO_MEMORY_BLOCK_ALLOC_LIMIT,
      (size_t)FIO_MEMORY_ALLOC_LIMIT,
      FIO_MEMORY_MALLOC_ZERO_POINTER,
      (FIO_MEMORY_INITIALIZE_ALLOCATIONS ? "true" : "false"));
}

/* *****************************************************************************
Malloc implementation
***************************************************************************** */

/* SublimeText marker */
void fio___malloc__(void);
/**
 * Allocates memory using a per-CPU core block memory pool.
 * Memory is zeroed out.
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT will be redirected to `mmap`,
 * as if `mempool_mmap` was called.
 *
 * `mempool_malloc` promises a best attempt at providing locality between
 * consecutive calls, but locality can't be guaranteed.
 */
FIO_IFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME,
                                           ___malloc)(size_t size,
                                                      void *is_realloc) {
  void *p = NULL;
  if (!size)
    goto malloc_zero;
#if FIO_MEMORY_ENABLE_BIG_ALLOC
  if ((is_realloc && size > (FIO_MEMORY_BIG_BLOCK_SIZE -
                             (FIO_MEMORY_BIG_BLOCK_HEADER_SIZE << 1))) ||
      (!is_realloc && size > FIO_MEMORY_ALLOC_LIMIT))
#else
  if (!is_realloc && size > FIO_MEMORY_ALLOC_LIMIT)
#endif
  {
#ifdef DEBUG
    FIO_LOG_WARNING(
        "unintended " FIO_MACRO2STR(
            FIO_NAME(FIO_MEMORY_NAME, mmap)) " allocation (slow): %zu pages",
        FIO_MEM_BYTES2PAGES(size));
#endif
    return FIO_NAME(FIO_MEMORY_NAME, mmap)(size);
  }
  if (!FIO_NAME(FIO_MEMORY_NAME, __mem_state)) {
    FIO_NAME(FIO_MEMORY_NAME, __mem_state_setup)();
  }
#if FIO_MEMORY_ENABLE_BIG_ALLOC
  if ((is_realloc &&
       size > FIO_MEMORY_BLOCK_SIZE - (2 << FIO_MEMORY_ALIGN_LOG)) ||
      (!is_realloc && size > FIO_MEMORY_BLOCK_ALLOC_LIMIT))
    return FIO_NAME(FIO_MEMORY_NAME, __mem_big_slice_new)(size, is_realloc);
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */

  return FIO_NAME(FIO_MEMORY_NAME, __mem_slice_new)(size, is_realloc);
malloc_zero:
  p = FIO_MEMORY_MALLOC_ZERO_POINTER;
  return p;
}

/* *****************************************************************************
Memory Allocation - API implementation
***************************************************************************** */

/* SublimeText marker */
void fio_malloc__(void);
/**
 * Allocates memory using a per-CPU core block memory pool.
 * Memory is zeroed out.
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT will be redirected to `mmap`,
 * as if `mempool_mmap` was called.
 *
 * `mempool_malloc` promises a best attempt at providing locality between
 * consecutive calls, but locality can't be guaranteed.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, malloc)(size_t size) {
  return FIO_NAME(FIO_MEMORY_NAME, ___malloc)(size, NULL);
}

/* SublimeText marker */
void fio_calloc__(void);
/**
 * same as calling `fio_malloc(size_per_unit * unit_count)`;
 *
 * Allocations above FIO_MEMORY_BLOCK_ALLOC_LIMIT will be redirected to `mmap`,
 * as if `mempool_mmap` was called.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME,
                                       calloc)(size_t size_per_unit,
                                               size_t unit_count) {
#if FIO_MEMORY_INITIALIZE_ALLOCATIONS
  return FIO_NAME(FIO_MEMORY_NAME, malloc)(size_per_unit * unit_count);
#else
  void *p;
  /* round up to alignment size. */
  const size_t len =
      ((size_per_unit * unit_count) + (FIO_MEMORY_ALIGN_SIZE - 1)) &
      (~((size_t)FIO_MEMORY_ALIGN_SIZE - 1));
  p = FIO_NAME(FIO_MEMORY_NAME, malloc)(len);
  /* initialize memory only when required */
  FIO___MEMSET(p, 0, len);
  return p;
#endif /* FIO_MEMORY_INITIALIZE_ALLOCATIONS */
}

/* SublimeText marker */
void fio_free__(void);
/** Frees memory that was allocated using this library. */
SFUNC void FIO_NAME(FIO_MEMORY_NAME, free)(void *ptr) {
  if (!ptr || ptr == FIO_MEMORY_MALLOC_ZERO_POINTER)
    return;
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c =
      FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(ptr);
  if (!c) {
    FIO_LOG_ERROR(FIO_MACRO2STR(
        FIO_NAME(FIO_MEMORY_NAME,
                 free)) " attempting to free a pointer owned by a NULL chunk.");
    return;
  }

#if FIO_MEMORY_ENABLE_BIG_ALLOC
  if (c->marker == FIO_MEMORY_BIG_BLOCK_MARKER) {
    FIO_NAME(FIO_MEMORY_NAME, __mem_big_slice_free)(ptr);
    return;
  }
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */

  /* big mmap allocation? */
  if (((uintptr_t)c + FIO_MEMORY_ALIGN_SIZE) == (uintptr_t)ptr && c->marker)
    goto mmap_free;
  FIO_NAME(FIO_MEMORY_NAME, __mem_slice_free)(ptr);
  return;
mmap_free:
  /* zero out memory before returning it to the system */
  FIO___MEMSET(ptr,
               0,
               ((size_t)c->marker << FIO_MEM_PAGE_SIZE_LOG) -
                   FIO_MEMORY_ALIGN_SIZE);
  FIO_MEM_SYS_FREE(c, (size_t)c->marker << FIO_MEM_PAGE_SIZE_LOG);
  FIO_MEMORY_ON_CHUNK_FREE(c);
}

/* SublimeText marker */
void fio_realloc__(void);
/**
 * Re-allocates memory. An attempt to avoid copying the data is made only for
 * big memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 */
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc)(void *ptr,
                                                             size_t new_size) {
  return FIO_NAME(FIO_MEMORY_NAME, realloc2)(ptr, new_size, new_size);
}

/**
 * Uses system page maps for reallocation.
 */
FIO_SFUNC void *FIO_NAME(FIO_MEMORY_NAME, __mem_realloc2_big)(
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) * c,
    size_t new_size) {
  const size_t new_len = FIO_MEM_BYTES2PAGES(new_size + FIO_MEMORY_ALIGN_SIZE);
  c = (FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)FIO_MEM_SYS_REALLOC(
      c,
      c->marker,
      new_len,
      FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG);
  if (!c)
    return NULL;
  c->marker = (uint32_t)(new_len >> FIO_MEM_PAGE_SIZE_LOG);
  return (void *)((uintptr_t)c + FIO_MEMORY_ALIGN_SIZE);
}

/* SublimeText marker */
void fio_realloc2__(void);
/**
 * Re-allocates memory. An attempt to avoid copying the data is made only for
 * big memory allocations (larger than FIO_MEMORY_BLOCK_ALLOC_LIMIT).
 *
 * This variation is slightly faster as it might copy less data.
 */
SFUNC void *FIO_MEM_ALIGN FIO_NAME(FIO_MEMORY_NAME, realloc2)(void *ptr,
                                                              size_t new_size,
                                                              size_t copy_len) {
  void *mem = NULL;
  if (!new_size)
    goto act_as_free;
  if (!ptr || ptr == FIO_MEMORY_MALLOC_ZERO_POINTER)
    goto act_as_malloc;

  { /* test for big-paged malloc and limit copy_len */
    FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c =
        FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2chunk)(ptr);
    size_t b = FIO_NAME(FIO_MEMORY_NAME, __mem_ptr2index)(c, ptr);

    register size_t max_len =
        ((uintptr_t)FIO_NAME(FIO_MEMORY_NAME, __mem_chunk2ptr)(c, b, 0) +
         FIO_MEMORY_BLOCK_SIZE) -
        ((uintptr_t)ptr);
#if FIO_MEMORY_ENABLE_BIG_ALLOC
    if (c->marker == FIO_MEMORY_BIG_BLOCK_MARKER) {
      /* extend max_len to accomodate possible length */
      max_len =
          ((uintptr_t)c + FIO_MEMORY_SYS_ALLOCATION_SIZE) - ((uintptr_t)ptr);
    } else
#endif /* FIO_MEMORY_ENABLE_BIG_ALLOC */
        if ((uintptr_t)(c) + FIO_MEMORY_ALIGN_SIZE == (uintptr_t)ptr &&
            c->marker) {
      if (new_size > FIO_MEMORY_ALLOC_LIMIT)
        return (mem =
                    FIO_NAME(FIO_MEMORY_NAME, __mem_realloc2_big)(c, new_size));
      max_len = new_size; /* shrinking from mmap to allocator */
    }

    if (copy_len > max_len)
      copy_len = max_len;
    if (copy_len > new_size)
      copy_len = new_size;
  }

  mem = FIO_NAME(FIO_MEMORY_NAME, ___malloc)(new_size, ptr);
  if (!mem || mem == ptr) {
    return mem;
  }

  /* when allocated from the same block, the max length might be adjusted */
  if ((uintptr_t)mem > (uintptr_t)ptr &&
      (uintptr_t)ptr + copy_len >= (uintptr_t)mem) {
    copy_len = (uintptr_t)mem - (uintptr_t)ptr;
  }

  FIO___MEMCPY2(mem,
                ptr,
                ((copy_len + (FIO_MEMORY_ALIGN_SIZE - 1)) &
                 ((~(size_t)0) << FIO_MEMORY_ALIGN_LOG)));
  // zero out leftover bytes, if any.
  while (copy_len & (FIO_MEMORY_ALIGN_SIZE - 1)) {
    ((uint8_t *)mem)[copy_len++] = 0;
  }

  FIO_NAME(FIO_MEMORY_NAME, free)(ptr);

  return mem;

act_as_malloc:
  mem = FIO_NAME(FIO_MEMORY_NAME, malloc)(new_size);
  return mem;

act_as_free:
  FIO_NAME(FIO_MEMORY_NAME, free)(ptr);
  mem = FIO_MEMORY_MALLOC_ZERO_POINTER;
  return mem;
}

/* SublimeText marker */
void fio_mmap__(void);
/**
 * Allocates memory directly using `mmap`, this is preferred for objects that
 * both require almost a page of memory (or more) and expect a long lifetime.
 *
 * However, since this allocation will invoke the system call (`mmap`), it will
 * be inherently slower.
 *
 * `mempoll_free` can be used for deallocating the memory.
 */
SFUNC void *FIO_MEM_ALIGN_NEW FIO_NAME(FIO_MEMORY_NAME, mmap)(size_t size) {
  if (!size)
    return FIO_NAME(FIO_MEMORY_NAME, malloc)(0);
  size_t pages = FIO_MEM_BYTES2PAGES(size + FIO_MEMORY_ALIGN_SIZE);
  if (((uint64_t)pages >> 32))
    return NULL;
  FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *c =
      (FIO_NAME(FIO_MEMORY_NAME, __mem_chunk_s) *)
          FIO_MEM_SYS_ALLOC(pages, FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG);
  if (!c)
    return NULL;
  FIO_MEMORY_ON_CHUNK_ALLOC(c);
  c->marker = (uint32_t)(pages >> FIO_MEM_PAGE_SIZE_LOG);
  return (void *)((uintptr_t)c + FIO_MEMORY_ALIGN_SIZE);
}

/* *****************************************************************************
Override the system's malloc functions if required
***************************************************************************** */
#if defined(FIO_MALLOC_OVERRIDE_SYSTEM) && !defined(H___FIO_MALLOC_OVERRIDE___H)
#define H___FIO_MALLOC_OVERRIDE___H
void *malloc(size_t size) { return FIO_NAME(FIO_MEMORY_NAME, malloc)(size); }
void *calloc(size_t size, size_t count) {
  return FIO_NAME(FIO_MEMORY_NAME, calloc)(size, count);
}
void free(void *ptr) { FIO_NAME(FIO_MEMORY_NAME, free)(ptr); }
void *realloc(void *ptr, size_t new_size) {
  return FIO_NAME(FIO_MEMORY_NAME, realloc2)(ptr, new_size, new_size);
}
#endif /* FIO_MALLOC_OVERRIDE_SYSTEM */
#undef FIO_MALLOC_OVERRIDE_SYSTEM

/* *****************************************************************************





Memory Allocation - test





***************************************************************************** */
#ifdef FIO_TEST_CSTL

#ifndef H___FIO_TEST_MEMORY_HELPERS_H
#define H___FIO_TEST_MEMORY_HELPERS_H

FIO_IFUNC void fio___memset_test_aligned(void *restrict dest_,
                                         uint64_t data,
                                         size_t bytes,
                                         const char *msg) {
  uint64_t *dest = (uint64_t *)dest_;
  size_t units = bytes >> 3;
  FIO_ASSERT(*(dest) = data,
             "%s memory data was overwritten (first 8 bytes)",
             msg);
  while (units >= 16) {
    FIO_ASSERT(dest[0] == data && dest[1] == data && dest[2] == data &&
                   dest[3] == data && dest[4] == data && dest[5] == data &&
                   dest[6] == data && dest[7] == data && dest[8] == data &&
                   dest[9] == data && dest[10] == data && dest[11] == data &&
                   dest[12] == data && dest[13] == data && dest[14] == data &&
                   dest[15] == data,
               "%s memory data was overwritten",
               msg);
    dest += 16;
    units -= 16;
  }
  switch (units) {
  case 15:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 14:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 13:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 12:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 11:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 10:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 9:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 8:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 7:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 6:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 5:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 4:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 3:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 2:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten",
               msg); /* fallthrough */
  case 1:
    FIO_ASSERT(*(dest++) = data,
               "%s memory data was overwritten (last 8 bytes)",
               msg);
  }
  (void)msg; /* in case FIO_ASSERT is disabled */
}

/* main test function */
FIO_SFUNC void FIO_NAME_TEST(stl, mem_helper_speeds)(void) {
  uint64_t start, end;
  const int repetitions = 8192;

  fprintf(stderr,
          "* Speed testing memset (%d repetitions per test):\n",
          repetitions);

  for (int len_i = 11; len_i < 20; ++len_i) {
    const size_t mem_len = 1ULL << len_i;
    void *mem = malloc(mem_len);
    FIO_ASSERT_ALLOC(mem);
    uint64_t sig = (uintptr_t)mem;
    sig ^= sig >> 13;
    sig ^= sig << 17;
    sig ^= sig << 29;
    sig ^= sig << 31;

    start = fio_time_micro();
    for (int i = 0; i < repetitions; ++i) {
      fio_memset_aligned(mem, sig, mem_len);
      FIO_COMPILER_GUARD;
    }
    end = fio_time_micro();
    fio___memset_test_aligned(mem,
                              sig,
                              mem_len,
                              "fio_memset_aligned sanity test FAILED");
    fprintf(stderr,
            "\tfio_memset_aligned\t(%zu bytes):\t%zu us\n",
            mem_len,
            (size_t)(end - start));
    start = fio_time_micro();
    for (int i = 0; i < repetitions; ++i) {
      memset(mem, (int)sig, mem_len);
      FIO_COMPILER_GUARD;
    }
    end = fio_time_micro();
    fprintf(stderr,
            "\tsystem memset\t\t(%zu bytes):\t%zu us\n",
            mem_len,
            (size_t)(end - start));

    free(mem);
  }

  fprintf(stderr,
          "* Speed testing memcpy (%d repetitions per test):\n",
          repetitions);

  for (int len_i = 11; len_i < 20; ++len_i) {
    const size_t mem_len = 1ULL << len_i;
    void *mem = malloc(mem_len << 1);
    FIO_ASSERT_ALLOC(mem);
    uint64_t sig = (uintptr_t)mem;
    sig ^= sig >> 13;
    sig ^= sig << 17;
    sig ^= sig << 29;
    sig ^= sig << 31;
    fio_memset_aligned(mem, sig, mem_len);

    start = fio_time_micro();
    for (int i = 0; i < repetitions; ++i) {
      fio_memcpy_aligned((char *)mem + mem_len, mem, mem_len);
      FIO_COMPILER_GUARD;
    }
    end = fio_time_micro();

    fio___memset_test_aligned((char *)mem + mem_len,
                              sig,
                              mem_len,
                              "fio_memcpy_aligned sanity test FAILED");
    fprintf(stderr,
            "\tfio_memcpy_aligned\t(%zu bytes):\t%zu us\n",
            mem_len,
            (size_t)(end - start));
    start = fio_time_micro();
    for (int i = 0; i < repetitions; ++i) {
      memcpy((char *)mem + mem_len, mem, mem_len);
      FIO_COMPILER_GUARD;
    }
    end = fio_time_micro();
    fprintf(stderr,
            "\tsystem memcpy\t\t(%zu bytes):\t%zu us\n",
            mem_len,
            (size_t)(end - start));

    free(mem);
  }
  {
    /* test fio_memcpy_aligned as a memmove alternative. */
    uint64_t buf1[64];
    uint8_t *buf = (uint8_t *)buf1;
    fio_memset_aligned(buf1, ~(uint64_t)0, sizeof(*buf1) * 64);
    char *data =
        (char *)"This should be an uneven amount of characters, say 53";
    fio_memcpy_aligned(buf, data, strlen(data));
    FIO_ASSERT(!memcmp(buf, data, strlen(data)) && buf[strlen(data)] == 0xFF,
               "fio_memcpy_aligned should not overflow or underflow on uneven "
               "amounts of bytes.");
    fio_memcpy_aligned(buf + 8, buf, strlen(data));
    FIO_ASSERT(!memcmp(buf + 8, data, strlen(data)) &&
                   buf[strlen(data) + 8] == 0xFF,
               "fio_memcpy_aligned should not fail as memmove.");
  }
}
#endif /* H___FIO_TEST_MEMORY_HELPERS_H */

/* contention testing (multi-threaded) */
FIO_IFUNC void *FIO_NAME_TEST(FIO_NAME(FIO_MEMORY_NAME, fio),
                              mem_tsk)(void *i_) {
  uintptr_t cycles = (uintptr_t)i_;
  const size_t test_byte_count =
      FIO_MEMORY_SYS_ALLOCATION_SIZE + (FIO_MEMORY_SYS_ALLOCATION_SIZE >> 1);
  uint64_t marker;
  do {
    marker = fio_rand64();
  } while (!marker);

  const size_t limit = (test_byte_count / cycles);
  char **ary = (char **)FIO_NAME(FIO_MEMORY_NAME, calloc)(sizeof(*ary), limit);
  const uintptr_t alignment_mask = (FIO_MEMORY_ALIGN_SIZE - 1);
  FIO_ASSERT(ary, "allocation failed for test container");
  for (size_t i = 0; i < limit; ++i) {
    if (1) {
      /* add some fragmentation */
      char *tmp = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(16);
      FIO_NAME(FIO_MEMORY_NAME, free)(tmp);
      FIO_ASSERT(tmp, "small allocation failed!")
      FIO_ASSERT(!((uintptr_t)tmp & alignment_mask),
                 "allocation alignment error!");
    }
    ary[i] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
    FIO_ASSERT(ary[i], "allocation failed!");
    FIO_ASSERT(!((uintptr_t)ary[i] & alignment_mask),
               "allocation alignment error!");
    FIO_ASSERT(!FIO_MEMORY_INITIALIZE_ALLOCATIONS || !ary[i][(cycles - 1)],
               "allocated memory not zero (end): %p",
               (void *)ary[i]);
    FIO_ASSERT(!FIO_MEMORY_INITIALIZE_ALLOCATIONS || !ary[i][0],
               "allocated memory not zero (start): %p",
               (void *)ary[i]);
    fio_memset_aligned(ary[i], marker, (cycles));
  }
  for (size_t i = 0; i < limit; ++i) {
    char *tmp = (char *)FIO_NAME(FIO_MEMORY_NAME,
                                 realloc2)(ary[i], (cycles << 1), (cycles));
    FIO_ASSERT(tmp, "re-allocation failed!")
    ary[i] = tmp;
    FIO_ASSERT(!((uintptr_t)ary[i] & alignment_mask),
               "allocation alignment error!");
    FIO_ASSERT(!FIO_MEMORY_INITIALIZE_ALLOCATIONS || !ary[i][(cycles)],
               "realloc2 copy overflow!");
    fio___memset_test_aligned(ary[i], marker, (cycles), "realloc grow");
    tmp =
        (char *)FIO_NAME(FIO_MEMORY_NAME, realloc2)(ary[i], (cycles), (cycles));
    FIO_ASSERT(tmp, "re-allocation (shrinking) failed!")
    ary[i] = tmp;
    fio___memset_test_aligned(ary[i], marker, (cycles), "realloc shrink");
  }
  for (size_t i = 0; i < limit; ++i) {
    fio___memset_test_aligned(ary[i], marker, (cycles), "mem review");
    FIO_NAME(FIO_MEMORY_NAME, free)(ary[i]);
    ary[i] = NULL;
  }

  uint64_t mark;
  void *old = &mark;
  mark = fio_risky_hash(&old, sizeof(mark), 0);

  for (int repeat_cycle_test = 0; repeat_cycle_test < 4; ++repeat_cycle_test) {
    for (size_t i = 0; i < limit - 4; i += 4) {
      if (ary[i])
        fio___memset_test_aligned(ary[i], mark, 16, "mark missing at ary[0]");
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i]);
      if (ary[i + 1])
        fio___memset_test_aligned(ary[i + 1],
                                  mark,
                                  cycles,
                                  "mark missing at ary[1]");
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + 1]);
      if (ary[i + 2])
        fio___memset_test_aligned(ary[i + 2],
                                  mark,
                                  cycles,
                                  "mark missing at ary[2]");
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + 2]);
      if (ary[i + 3])
        fio___memset_test_aligned(ary[i + 3],
                                  mark,
                                  cycles,
                                  "mark missing at ary[3]");
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + 3]);

      ary[i] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      fio_memset_aligned(ary[i], mark, cycles);

      ary[i + 1] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + 1]);
      ary[i + 1] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      fio_memset_aligned(ary[i + 1], mark, cycles);

      ary[i + 2] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      fio_memset_aligned(ary[i + 2], mark, cycles);
      ary[i + 2] = (char *)FIO_NAME(FIO_MEMORY_NAME,
                                    realloc2)(ary[i + 2], cycles * 2, cycles);

      ary[i + 3] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + 3]);
      ary[i + 3] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
      fio_memset_aligned(ary[i + 3], mark, cycles);
      ary[i + 3] = (char *)FIO_NAME(FIO_MEMORY_NAME,
                                    realloc2)(ary[i + 3], cycles * 2, cycles);

      for (int b = 0; b < 4; ++b) {
        for (size_t pos = 0; pos < (cycles / sizeof(uint64_t)); ++pos) {
          FIO_ASSERT(((uint64_t *)(ary[i + b]))[pos] == mark,
                     "memory mark corrupted at test ptr %zu",
                     i + b);
        }
      }
      for (int b = 1; b < 4; ++b) {
        FIO_NAME(FIO_MEMORY_NAME, free)(ary[b]);
        ary[b] = NULL;
        FIO_NAME(FIO_MEMORY_NAME, free)(ary[i + b]);
      }
      for (int b = 1; b < 4; ++b) {
        ary[i + b] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
        if (i) {
          ary[b] = (char *)FIO_NAME(FIO_MEMORY_NAME, malloc)(cycles);
          fio_memset_aligned(ary[b], mark, cycles);
        }
        fio_memset_aligned(ary[i + b], mark, cycles);
      }

      for (int b = 0; b < 4; ++b) {
        for (size_t pos = 0; pos < (cycles / sizeof(uint64_t)); ++pos) {
          FIO_ASSERT(((uint64_t *)(ary[b]))[pos] == mark,
                     "memory mark corrupted at test ptr %zu",
                     i + b);
          FIO_ASSERT(((uint64_t *)(ary[i + b]))[pos] == mark,
                     "memory mark corrupted at test ptr %zu",
                     i + b);
        }
      }
    }
  }
  for (size_t i = 0; i < limit; ++i) {
    FIO_NAME(FIO_MEMORY_NAME, free)(ary[i]);
    ary[i] = NULL;
  }

  FIO_NAME(FIO_MEMORY_NAME, free)(ary);
  return NULL;
}

/* main test function */
FIO_SFUNC void FIO_NAME_TEST(FIO_NAME(stl, FIO_MEMORY_NAME), mem)(void) {
  fprintf(stderr,
          "* Testing core memory allocator " FIO_MACRO2STR(
              FIO_NAME(FIO_MEMORY_NAME, malloc)) ".\n");

  const uintptr_t alignment_mask = (FIO_MEMORY_ALIGN_SIZE - 1);
  fprintf(stderr,
          "* validating allocation alignment on %zu byte border.\n",
          (size_t)(FIO_MEMORY_ALIGN_SIZE));
  for (size_t i = 0; i < alignment_mask; ++i) {
    void *p = FIO_NAME(FIO_MEMORY_NAME, malloc)(i);
    FIO_ASSERT(!((uintptr_t)p & alignment_mask),
               "allocation alignment error allocating %zu bytes!",
               i);
    FIO_NAME(FIO_MEMORY_NAME, free)(p);
  }
  const size_t thread_count =
      FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count +
      (FIO_NAME(FIO_MEMORY_NAME, __mem_state)->arena_count >> 1);

  for (uintptr_t cycles = 16; cycles <= (FIO_MEMORY_ALLOC_LIMIT); cycles *= 2) {
    fprintf(stderr,
            "* Testing %zu byte allocation blocks, single threaded.\n",
            (size_t)(cycles));
    FIO_NAME_TEST(FIO_NAME(FIO_MEMORY_NAME, fio), mem_tsk)((void *)cycles);
  }

  for (uintptr_t cycles = 16; cycles <= (FIO_MEMORY_ALLOC_LIMIT); cycles *= 2) {
#if _MSC_VER
    fio_thread_t threads[(FIO_MEMORY_ARENA_COUNT_MAX + 1) * 2];
    FIO_ASSERT(((FIO_MEMORY_ARENA_COUNT_MAX + 1) * 2) >= thread_count,
               "Please use CLang or GCC to test this memory allocator");
#else
    fio_thread_t threads[thread_count];
#endif

    fprintf(stderr,
            "* Testing %zu byte allocation blocks, using %zu threads.\n",
            (size_t)(cycles),
            (thread_count + 1));
    for (size_t i = 0; i < thread_count; ++i) {
      if (fio_thread_create(
              threads + i,
              FIO_NAME_TEST(FIO_NAME(FIO_MEMORY_NAME, fio), mem_tsk),
              (void *)cycles)) {
        abort();
      }
    }
    FIO_NAME_TEST(FIO_NAME(FIO_MEMORY_NAME, fio), mem_tsk)((void *)cycles);
    for (size_t i = 0; i < thread_count; ++i) {
      fio_thread_join(threads[i]);
    }
  }
  fprintf(stderr,
          "* re-validating allocation alignment on %zu byte border.\n",
          (size_t)(FIO_MEMORY_ALIGN_SIZE));
  for (size_t i = 0; i < alignment_mask; ++i) {
    void *p = FIO_NAME(FIO_MEMORY_NAME, malloc)(i);
    FIO_ASSERT(!((uintptr_t)p & alignment_mask),
               "allocation alignment error allocating %zu bytes!",
               i);
    FIO_NAME(FIO_MEMORY_NAME, free)(p);
  }

#if DEBUG
  FIO_NAME(FIO_MEMORY_NAME, malloc_print_state)();
  FIO_NAME(FIO_MEMORY_NAME, __mem_state_cleanup)();
  FIO_ASSERT(!FIO_NAME(fio___, FIO_NAME(FIO_MEMORY_NAME, state_chunk_count)),
             "memory leaks?");
#endif /* DEBUG */
}
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
Memory pool cleanup
***************************************************************************** */
#undef FIO___MEMSET
#undef FIO___MEMCPY2
#undef FIO_MEM_ALIGN
#undef FIO_MEM_ALIGN_NEW
#undef FIO_MEMORY_MALLOC_ZERO_POINTER

#endif /* FIO_MEMORY_DISABLE */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_MEMORY_NAME */

#undef FIO_MEMORY_ON_CHUNK_ALLOC
#undef FIO_MEMORY_ON_CHUNK_FREE
#undef FIO_MEMORY_ON_CHUNK_CACHE
#undef FIO_MEMORY_ON_CHUNK_UNCACHE
#undef FIO_MEMORY_ON_CHUNK_DIRTY
#undef FIO_MEMORY_ON_CHUNK_UNDIRTY
#undef FIO_MEMORY_ON_BLOCK_RESET_IN_LOCK
#undef FIO_MEMORY_ON_BIG_BLOCK_SET
#undef FIO_MEMORY_ON_BIG_BLOCK_UNSET
#undef FIO_MEMORY_PRINT_STATS
#undef FIO_MEMORY_PRINT_STATS_END

#undef FIO_MEMORY_ARENA_COUNT
#undef FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG
#undef FIO_MEMORY_CACHE_SLOTS
#undef FIO_MEMORY_ALIGN_LOG
#undef FIO_MEMORY_INITIALIZE_ALLOCATIONS
#undef FIO_MEMORY_USE_THREAD_MUTEX
#undef FIO_MEMORY_USE_FIO_MEMSET
#undef FIO_MEMORY_USE_FIO_MEMCOPY
#undef FIO_MEMORY_BLOCK_SIZE
#undef FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG
#undef FIO_MEMORY_BLOCKS_PER_ALLOCATION
#undef FIO_MEMORY_ENABLE_BIG_ALLOC
#undef FIO_MEMORY_ARENA_COUNT_FALLBACK
#undef FIO_MEMORY_ARENA_COUNT_MAX
#undef FIO_MEMORY_WARMUP

#undef FIO_MEMORY_LOCK_NAME
#undef FIO_MEMORY_LOCK_TYPE
#undef FIO_MEMORY_LOCK_TYPE_INIT
#undef FIO_MEMORY_TRYLOCK
#undef FIO_MEMORY_LOCK
#undef FIO_MEMORY_UNLOCK

/* don't undefine FIO_MEMORY_NAME due to possible use in allocation macros */

/* *****************************************************************************
Memory management macros
***************************************************************************** */

#if !defined(FIO_MEM_REALLOC_) || !defined(FIO_MEM_FREE_)
#undef FIO_MEM_REALLOC_
#undef FIO_MEM_FREE_
#undef FIO_MEM_REALLOC_IS_SAFE_

#ifdef FIO_MALLOC_TMP_USE_SYSTEM /* force malloc */
#define FIO_MEM_REALLOC_(ptr, old_size, new_size, copy_len)                    \
  realloc((ptr), (new_size))
#define FIO_MEM_FREE_(ptr, size) free((ptr))
#define FIO_MEM_REALLOC_IS_SAFE_ 0

#else /* FIO_MALLOC_TMP_USE_SYSTEM */
#define FIO_MEM_REALLOC_         FIO_MEM_REALLOC
#define FIO_MEM_FREE_            FIO_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIO_MEM_REALLOC_IS_SAFE
#endif /* FIO_MALLOC_TMP_USE_SYSTEM */

#endif /* !defined(FIO_MEM_REALLOC_)... */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_TIME                    /* Development inclusion - ignore line */
#define FIO_ATOL                    /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                  Time Helpers




***************************************************************************** */
#if defined(FIO_TIME) && !defined(H___FIO_TIME___H)
#define H___FIO_TIME___H

/* *****************************************************************************
Collecting Monotonic / Real Time
***************************************************************************** */

/** Returns human (watch) time... this value isn't as safe for measurements. */
FIO_IFUNC struct timespec fio_time_real();

/** Returns monotonic time. */
FIO_IFUNC struct timespec fio_time_mono();

/** Returns monotonic time in nano-seconds (now in 1 billionth of a second). */
FIO_IFUNC int64_t fio_time_nano();

/** Returns monotonic time in micro-seconds (now in 1 millionth of a second). */
FIO_IFUNC int64_t fio_time_micro();

/** Returns monotonic time in milliseconds. */
FIO_IFUNC int64_t fio_time_milli();

/** Converts a `struct timespec` to milliseconds. */
FIO_IFUNC int64_t fio_time2milli(struct timespec);

/**
 * A faster (yet less localized) alternative to `gmtime_r`.
 *
 * See the libc `gmtime_r` documentation for details.
 *
 * Falls back to `gmtime_r` for dates before epoch.
 */
SFUNC struct tm fio_time2gm(time_t time);

/** Converts a `struct tm` to time in seconds (assuming UTC). */
SFUNC time_t fio_gm2time(struct tm tm);

/**
 * Writes an RFC 7231 date representation (HTTP date format) to target.
 *
 * Usually requires 29 characters, although this may vary.
 */
SFUNC size_t fio_time2rfc7231(char *target, time_t time);

/**
 * Writes an RFC 2109 date representation to target.
 *
 * Usually requires 31 characters, although this may vary.
 */
SFUNC size_t fio_time2rfc2109(char *target, time_t time);

/**
 * Writes an RFC 2822 date representation to target.
 *
 * Usually requires 28 to 29 characters, although this may vary.
 */
SFUNC size_t fio_time2rfc2822(char *target, time_t time);

/**
 * Writes a date representation to target in common log format. i.e.,
 *
 *         [DD/MMM/yyyy:hh:mm:ss +0000]
 *
 * Usually requires 29 characters (includiing square brackes and NUL).
 */
SFUNC size_t fio_time2log(char *target, time_t time);

/* *****************************************************************************
Time Inline Helpers
***************************************************************************** */

/** Returns human (watch) time... this value isn't as safe for measurements. */
FIO_IFUNC struct timespec fio_time_real() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t;
}

/** Returns monotonic time. */
FIO_IFUNC struct timespec fio_time_mono() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t;
}

/** Returns monotonic time in nano-seconds (now in 1 micro of a second). */
FIO_IFUNC int64_t fio_time_nano() {
  struct timespec t = fio_time_mono();
  return ((int64_t)t.tv_sec * 1000000000) + (int64_t)t.tv_nsec;
}

/** Returns monotonic time in micro-seconds (now in 1 millionth of a second). */
FIO_IFUNC int64_t fio_time_micro() {
  struct timespec t = fio_time_mono();
  return ((int64_t)t.tv_sec * 1000000) + (int64_t)t.tv_nsec / 1000;
}

/** Returns monotonic time in milliseconds. */
FIO_IFUNC int64_t fio_time_milli() {
  struct timespec t = fio_time_mono();
  return ((int64_t)t.tv_sec * 1000) + (int64_t)t.tv_nsec / 1000000;
}

/** Converts a `struct timespec` to milliseconds. */
FIO_IFUNC int64_t fio_time2milli(struct timespec t) {
  return ((int64_t)t.tv_sec * 1000) + (int64_t)t.tv_nsec / 1000000;
}

/* *****************************************************************************
Time Implementation
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE)

/**
 * A faster (yet less localized) alternative to `gmtime_r`.
 *
 * See the libc `gmtime_r` documentation for details.
 *
 * Falls back to `gmtime_r` for dates before epoch.
 */
SFUNC struct tm fio_time2gm(time_t timer) {
  struct tm tm;
  ssize_t a, b;
#if HAVE_TM_TM_ZONE || defined(BSD)
  tm = (struct tm){
      .tm_isdst = 0,
      .tm_zone = (char *)"UTC",
  };
#else
  tm = (struct tm){
      .tm_isdst = 0,
  };
#endif

  // convert seconds from epoch to days from epoch + extract data
  if (timer >= 0) {
    // for seconds up to weekdays, we reduce the reminder every step.
    a = (ssize_t)timer;
    b = a / 60; // b == time in minutes
    tm.tm_sec = (int)(a - (b * 60));
    a = b / 60; // b == time in hours
    tm.tm_min = (int)(b - (a * 60));
    b = a / 24; // b == time in days since epoch
    tm.tm_hour = (int)(a - (b * 24));
    // b == number of days since epoch
    // day of epoch was a thursday. Add + 4 so sunday == 0...
    tm.tm_wday = (b + 4) % 7;
  } else {
    // for seconds up to weekdays, we reduce the reminder every step.
    a = (ssize_t)timer;
    b = a / 60; // b == time in minutes
    if (b * 60 != a) {
      /* seconds passed */
      tm.tm_sec = (int)((a - (b * 60)) + 60);
      --b;
    } else {
      /* no seconds */
      tm.tm_sec = 0;
    }
    a = b / 60; // b == time in hours
    if (a * 60 != b) {
      /* minutes passed */
      tm.tm_min = (int)((b - (a * 60)) + 60);
      --a;
    } else {
      /* no minutes */
      tm.tm_min = 0;
    }
    b = a / 24; // b == time in days since epoch?
    if (b * 24 != a) {
      /* hours passed */
      tm.tm_hour = (int)((a - (b * 24)) + 24);
      --b;
    } else {
      /* no hours */
      tm.tm_hour = 0;
    }
    // day of epoch was a thursday. Add + 4 so sunday == 0...
    tm.tm_wday = ((b - 3) % 7);
    if (tm.tm_wday)
      tm.tm_wday += 7;
    /* b == days from epoch */
  }

  // at this point we can apply the algorithm described here:
  // http://howardhinnant.github.io/date_algorithms.html#civil_from_days
  // Credit to Howard Hinnant.
  {
    b += 719468L; // adjust to March 1st, 2000 (post leap of 400 year era)
    // 146,097 = days in era (400 years)
    const size_t era = (b >= 0 ? b : b - 146096) / 146097;
    const uint32_t doe = (uint32_t)(b - (era * 146097)); // day of era
    const uint16_t yoe = (uint16_t)(
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365); // year of era
    a = yoe;
    a += era * 400; // a == year number, assuming year starts on March 1st...
    const uint16_t doy = (uint16_t)(doe - (365 * yoe + yoe / 4 - yoe / 100));
    const uint16_t mp = (uint16_t)((5U * doy + 2) / 153);
    const uint16_t d = (uint16_t)(doy - (153U * mp + 2) / 5 + 1);
    const uint8_t m = (uint8_t)(mp + (mp < 10 ? 2 : -10));
    a += (m <= 1);
    tm.tm_year = (int)(a - 1900); // tm_year == years since 1900
    tm.tm_mon = m;
    tm.tm_mday = d;
    const uint8_t is_leap = (a % 4 == 0 && (a % 100 != 0 || a % 400 == 0));
    tm.tm_yday = (doy + (is_leap) + 28 + 31) % (365 + is_leap);
  }

  return tm;
}

/** Converts a `struct tm` to time in seconds (assuming UTC). */
SFUNC time_t fio_gm2time(struct tm tm) {
  int64_t time = 0;
  // we start with the algorithm described here:
  // http://howardhinnant.github.io/date_algorithms.html#days_from_civil
  // Credit to Howard Hinnant.
  {
    const int32_t y = (tm.tm_year + 1900) - (tm.tm_mon < 2);
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint16_t yoe = (y - era * 400L); // 0-399
    const uint32_t doy =
        (153L * (tm.tm_mon + (tm.tm_mon > 1 ? -2 : 10)) + 2) / 5 + tm.tm_mday -
        1;                                                       // 0-365
    const uint32_t doe = yoe * 365L + yoe / 4 - yoe / 100 + doy; // 0-146096
    time = era * 146097LL + doe - 719468LL; // time == days from epoch
  }

  /* Adjust for hour, minute and second */
  time = time * 24LL + tm.tm_hour;
  time = time * 60LL + tm.tm_min;
  time = time * 60LL + tm.tm_sec;

  if (tm.tm_isdst > 0) {
    time -= 60 * 60;
  }
#if HAVE_TM_TM_ZONE || defined(BSD)
  if (tm.tm_gmtoff) {
    time += tm.tm_gmtoff;
  }
#endif
  return (time_t)time;
}

static const char *FIO___DAY_NAMES[] =
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
// clang-format off
static const char *FIO___MONTH_NAMES[] =
    {"Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
     "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
// clang-format on
static const char *FIO___GMT_STR = "GMT";

/** Writes an RFC 7231 date representation (HTTP date format) to target. */
SFUNC size_t fio_time2rfc7231(char *target, time_t time) {
  const struct tm tm = fio_time2gm(time);
  /* note: day of month is always 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tm.tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tm.tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tm.tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  tmp = tm.tm_mday / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tm.tm_mday - (tmp * 10));
  pos += 2;
  *(pos++) = ' ';
  pos[0] = FIO___MONTH_NAMES[tm.tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tm.tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tm.tm_mon][2];
  pos[3] = ' ';
  pos += 4;
  // write year.
  pos += fio_ltoa(pos, tm.tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tm.tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tm.tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tm.tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tm.tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tm.tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tm.tm_sec - (tmp * 10));
  pos += 8;
  pos[0] = ' ';
  pos[1] = FIO___GMT_STR[0];
  pos[2] = FIO___GMT_STR[1];
  pos[3] = FIO___GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}
/** Writes an RFC 2109 date representation to target. */
SFUNC size_t fio_time2rfc2109(char *target, time_t time) {
  const struct tm tm = fio_time2gm(time);
  /* note: day of month is always 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tm.tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tm.tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tm.tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  tmp = tm.tm_mday / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tm.tm_mday - (tmp * 10));
  pos += 2;
  *(pos++) = ' ';
  pos[0] = FIO___MONTH_NAMES[tm.tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tm.tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tm.tm_mon][2];
  pos[3] = ' ';
  pos += 4;
  // write year.
  pos += fio_ltoa(pos, tm.tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tm.tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tm.tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tm.tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tm.tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tm.tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tm.tm_sec - (tmp * 10));
  pos += 8;
  *pos++ = ' ';
  *pos++ = '-';
  *pos++ = '0';
  *pos++ = '0';
  *pos++ = '0';
  *pos++ = '0';
  *pos = 0;
  return pos - target;
}

/** Writes an RFC 2822 date representation to target. */
SFUNC size_t fio_time2rfc2822(char *target, time_t time) {
  const struct tm tm = fio_time2gm(time);
  /* note: day of month is either 1 or 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tm.tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tm.tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tm.tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  if (tm.tm_mday < 10) {
    *pos = '0' + tm.tm_mday;
    ++pos;
  } else {
    tmp = tm.tm_mday / 10;
    pos[0] = '0' + tmp;
    pos[1] = '0' + (tm.tm_mday - (tmp * 10));
    pos += 2;
  }
  *(pos++) = '-';
  pos[0] = FIO___MONTH_NAMES[tm.tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tm.tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tm.tm_mon][2];
  pos += 3;
  *(pos++) = '-';
  // write year.
  pos += fio_ltoa(pos, tm.tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tm.tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tm.tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tm.tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tm.tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tm.tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tm.tm_sec - (tmp * 10));
  pos += 8;
  pos[0] = ' ';
  pos[1] = FIO___GMT_STR[0];
  pos[2] = FIO___GMT_STR[1];
  pos[3] = FIO___GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}

/**
 * Writes a date representation to target in common log format. i.e.,
 *
 *         [DD/MMM/yyyy:hh:mm:ss +0000]
 *
 * Usually requires 29 characters (includiing square brackes and NUL).
 */
SFUNC size_t fio_time2log(char *target, time_t time) {
  {
    const struct tm tm = fio_time2gm(time);
    /* note: day of month is either 1 or 2 digits */
    char *pos = target;
    uint16_t tmp;
    *(pos++) = '[';
    tmp = tm.tm_mday / 10;
    *(pos++) = '0' + tmp;
    *(pos++) = '0' + (tm.tm_mday - (tmp * 10));
    *(pos++) = '/';
    *(pos++) = FIO___MONTH_NAMES[tm.tm_mon][0];
    *(pos++) = FIO___MONTH_NAMES[tm.tm_mon][1];
    *(pos++) = FIO___MONTH_NAMES[tm.tm_mon][2];
    *(pos++) = '/';
    pos += fio_ltoa(pos, tm.tm_year + 1900, 10);
    *(pos++) = ':';
    tmp = tm.tm_hour / 10;
    *(pos++) = '0' + tmp;
    *(pos++) = '0' + (tm.tm_hour - (tmp * 10));
    *(pos++) = ':';
    tmp = tm.tm_min / 10;
    *(pos++) = '0' + tmp;
    *(pos++) = '0' + (tm.tm_min - (tmp * 10));
    *(pos++) = ':';
    tmp = tm.tm_sec / 10;
    *(pos++) = '0' + tmp;
    *(pos++) = '0' + (tm.tm_sec - (tmp * 10));
    *(pos++) = ' ';
    *(pos++) = '+';
    *(pos++) = '0';
    *(pos++) = '0';
    *(pos++) = '0';
    *(pos++) = '0';
    *(pos++) = ']';
    *(pos) = 0;
    return pos - target;
  }
}

/* *****************************************************************************
Time - test
***************************************************************************** */
#ifdef FIO_TEST_CSTL

#define FIO___GMTIME_TEST_INTERVAL ((60LL * 60 * 23) + 1027) /* 23:17:07 */
#if 1 || FIO_OS_WIN
#define FIO___GMTIME_TEST_RANGE (1001LL * 376) /* test 0.5 millenia */
#else
#define FIO___GMTIME_TEST_RANGE (3003LL * 376) /* test ~3  millenia */
#endif

FIO_SFUNC void FIO_NAME_TEST(stl, time)(void) {
  fprintf(stderr, "* Testing facil.io fio_time2gm vs gmtime_r\n");
  struct tm tm1, tm2;
  const time_t now = fio_time_real().tv_sec;
#if FIO_OS_WIN
  const time_t end = (FIO___GMTIME_TEST_RANGE * FIO___GMTIME_TEST_INTERVAL);
  time_t t = 1; /* Windows fails on some date ranges. */
#else
  const time_t end =
      now + (FIO___GMTIME_TEST_RANGE * FIO___GMTIME_TEST_INTERVAL);
  time_t t = now - (FIO___GMTIME_TEST_RANGE * FIO___GMTIME_TEST_INTERVAL);
#endif
  FIO_ASSERT(t < end, "time testing range overflowed.");
  do {
    time_t tmp = t;
    t += FIO___GMTIME_TEST_INTERVAL;
    tm2 = fio_time2gm(tmp);
    FIO_ASSERT(fio_gm2time(tm2) == tmp,
               "fio_gm2time roundtrip error (%zu != %zu)",
               (size_t)fio_gm2time(tm2),
               (size_t)tmp);
    gmtime_r(&tmp, &tm1);
    if (tm1.tm_year != tm2.tm_year || tm1.tm_mon != tm2.tm_mon ||
        tm1.tm_mday != tm2.tm_mday || tm1.tm_yday != tm2.tm_yday ||
        tm1.tm_hour != tm2.tm_hour || tm1.tm_min != tm2.tm_min ||
        tm1.tm_sec != tm2.tm_sec || tm1.tm_wday != tm2.tm_wday) {
      char buf[256];
      FIO_LOG_ERROR("system gmtime_r != fio_time2gm for %ld!\n", (long)t);
      fio_time2rfc7231(buf, tmp);
      FIO_ASSERT(0,
                 "\n"
                 "-- System:\n"
                 "\ttm_year: %d\n"
                 "\ttm_mon: %d\n"
                 "\ttm_mday: %d\n"
                 "\ttm_yday: %d\n"
                 "\ttm_hour: %d\n"
                 "\ttm_min: %d\n"
                 "\ttm_sec: %d\n"
                 "\ttm_wday: %d\n"
                 "-- facil.io:\n"
                 "\ttm_year: %d\n"
                 "\ttm_mon: %d\n"
                 "\ttm_mday: %d\n"
                 "\ttm_yday: %d\n"
                 "\ttm_hour: %d\n"
                 "\ttm_min: %d\n"
                 "\ttm_sec: %d\n"
                 "\ttm_wday: %d\n"
                 "-- As String:\n"
                 "\t%s",
                 tm1.tm_year,
                 tm1.tm_mon,
                 tm1.tm_mday,
                 tm1.tm_yday,
                 tm1.tm_hour,
                 tm1.tm_min,
                 tm1.tm_sec,
                 tm1.tm_wday,
                 tm2.tm_year,
                 tm2.tm_mon,
                 tm2.tm_mday,
                 tm2.tm_yday,
                 tm2.tm_hour,
                 tm2.tm_min,
                 tm2.tm_sec,
                 tm2.tm_wday,
                 buf);
    }
  } while (t < end);
  {
    char buf[48];
    buf[47] = 0;
    memset(buf, 'X', 47);
    fio_time2rfc7231(buf, now);
    FIO_LOG_DEBUG2("fio_time2rfc7231:   %s", buf);
    memset(buf, 'X', 47);
    fio_time2rfc2109(buf, now);
    FIO_LOG_DEBUG2("fio_time2rfc2109:   %s", buf);
    memset(buf, 'X', 47);
    fio_time2rfc2822(buf, now);
    FIO_LOG_DEBUG2("fio_time2rfc2822:   %s", buf);
    memset(buf, 'X', 47);
    fio_time2log(buf, now);
    FIO_LOG_DEBUG2("fio_time2log:       %s", buf);
  }
  {
    uint64_t start, stop;
#if DEBUG
    fprintf(stderr, "PERFOMEANCE TESTS IN DEBUG MODE ARE BIASED\n");
#endif
    fprintf(stderr, "  performance testing fio_time2gm vs gmtime_r\n");
    start = fio_time_micro();
    for (size_t i = 0; i < (1 << 17); ++i) {
      volatile struct tm tm = fio_time2gm(now);
      FIO_COMPILER_GUARD;
      (void)tm;
    }
    stop = fio_time_micro();
    fprintf(stderr,
            "\t- fio_time2gm speed test took:\t%zuus\n",
            (size_t)(stop - start));
    start = fio_time_micro();
    for (size_t i = 0; i < (1 << 17); ++i) {
      volatile struct tm tm;
      time_t tmp = now;
      gmtime_r(&tmp, (struct tm *)&tm);
      FIO_COMPILER_GUARD;
    }
    stop = fio_time_micro();
    fprintf(stderr,
            "\t- gmtime_r speed test took:  \t%zuus\n",
            (size_t)(stop - start));
    fprintf(stderr, "\n");
    struct tm tm_now = fio_time2gm(now);
    start = fio_time_micro();
    for (size_t i = 0; i < (1 << 17); ++i) {
      tm_now = fio_time2gm(now + i);
      time_t t_tmp = fio_gm2time(tm_now);
      FIO_COMPILER_GUARD;
      (void)t_tmp;
    }
    stop = fio_time_micro();
    fprintf(stderr,
            "\t- fio_gm2time speed test took:\t%zuus\n",
            (size_t)(stop - start));
    start = fio_time_micro();
    for (size_t i = 0; i < (1 << 17); ++i) {
      tm_now = fio_time2gm(now + i);
      volatile time_t t_tmp = mktime((struct tm *)&tm_now);
      FIO_COMPILER_GUARD;
      (void)t_tmp;
    }
    stop = fio_time_micro();
    fprintf(stderr,
            "\t- mktime speed test took:    \t%zuus\n",
            (size_t)(stop - start));
    fprintf(stderr, "\n");
  }
}
#undef FIO___GMTIME_TEST_INTERVAL
#undef FIO___GMTIME_TEST_RANGE
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
Time Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_TIME
#endif /* FIO_TIME */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_QUEUE                   /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#include "101 time.h"               /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                                Task / Timer Queues
                                (Event Loop Engine)




***************************************************************************** */
#if defined(FIO_QUEUE) && !defined(H___FIO_QUEUE___H)
#define H___FIO_QUEUE___H

/* *****************************************************************************
Queue Type(s)
***************************************************************************** */

/* Note: FIO_QUEUE_TASKS_PER_ALLOC can't be more than 65535 */
#ifndef FIO_QUEUE_TASKS_PER_ALLOC
#if UINTPTR_MAX <= 0xFFFFFFFF
/* fits fio_queue_s in one page on most 32 bit machines */
#define FIO_QUEUE_TASKS_PER_ALLOC 338
#else
/* fits fio_queue_s in one page on most 64 bit machines */
#define FIO_QUEUE_TASKS_PER_ALLOC 168
#endif
#endif

/** Task information */
typedef struct {
  /** The function to call */
  void (*fn)(void *, void *);
  /** User opaque data */
  void *udata1;
  /** User opaque data */
  void *udata2;
} fio_queue_task_s;

/* internal use */
typedef struct fio___task_ring_s {
  uint16_t r;   /* reader position */
  uint16_t w;   /* writer position */
  uint16_t dir; /* direction */
  struct fio___task_ring_s *next;
  fio_queue_task_s buf[FIO_QUEUE_TASKS_PER_ALLOC];
} fio___task_ring_s;

/** The queue object - should be considered opaque (or, at least, read only). */
typedef struct {
  fio___task_ring_s *r;
  fio___task_ring_s *w;
  /** the number of tasks waiting to be performed. */
  size_t count;
  FIO___LOCK_TYPE lock;
  fio___task_ring_s mem;
} fio_queue_s;

/* *****************************************************************************
Queue API
***************************************************************************** */

#if FIO_USE_THREAD_MUTEX_TMP
/** May be used to initialize global, static memory, queues. */
#define FIO_QUEUE_STATIC_INIT(queue)                                           \
  {                                                                            \
    .r = &(queue).mem, .w = &(queue).mem,                                      \
    .lock = (fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT                          \
  }
#else
/** May be used to initialize global, static memory, queues. */
#define FIO_QUEUE_STATIC_INIT(queue)                                           \
  { .r = &(queue).mem, .w = &(queue).mem, .lock = FIO_LOCK_INIT }
#endif

/** Initializes a fio_queue_s object. */
FIO_IFUNC void fio_queue_init(fio_queue_s *q);

/** Destroys a queue and re-initializes it, after freeing any used resources. */
SFUNC void fio_queue_destroy(fio_queue_s *q);

/** Creates a new queue object (allocated on the heap). */
FIO_IFUNC fio_queue_s *fio_queue_new(void);

/** Frees a queue object after calling fio_queue_destroy. */
SFUNC void fio_queue_free(fio_queue_s *q);

/** Pushes a task to the queue. Returns -1 on error. */
SFUNC int fio_queue_push(fio_queue_s *q, fio_queue_task_s task);

/**
 * Pushes a task to the queue, offering named arguments for the task.
 * Returns -1 on error.
 */
#define fio_queue_push(q, ...)                                                 \
  fio_queue_push((q), (fio_queue_task_s){__VA_ARGS__})

/** Pushes a task to the head of the queue. Returns -1 on error (no memory). */
SFUNC int fio_queue_push_urgent(fio_queue_s *q, fio_queue_task_s task);

/**
 * Pushes a task to the queue, offering named arguments for the task.
 * Returns -1 on error.
 */
#define fio_queue_push_urgent(q, ...)                                          \
  fio_queue_push_urgent((q), (fio_queue_task_s){__VA_ARGS__})

/** Pops a task from the queue (FIFO). Returns a NULL task on error. */
SFUNC fio_queue_task_s fio_queue_pop(fio_queue_s *q);

/** Performs a task from the queue. Returns -1 on error (queue empty). */
SFUNC int fio_queue_perform(fio_queue_s *q);

/** Performs all tasks in the queue. */
SFUNC void fio_queue_perform_all(fio_queue_s *q);

/** returns the number of tasks in the queue. */
FIO_IFUNC size_t fio_queue_count(fio_queue_s *q);

/* *****************************************************************************
Timer Queue Types and API
***************************************************************************** */

typedef struct fio___timer_event_s fio___timer_event_s;

typedef struct {
  fio___timer_event_s *next;
  FIO___LOCK_TYPE lock;
} fio_timer_queue_s;

#if FIO_USE_THREAD_MUTEX_TMP
#define FIO_TIMER_QUEUE_INIT                                                   \
  { .lock = ((fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT) }
#else
#define FIO_TIMER_QUEUE_INIT                                                   \
  { .lock = FIO_LOCK_INIT }
#endif

typedef struct {
  /** The timer function. If it returns a non-zero value, the timer stops. */
  int (*fn)(void *, void *);
  /** Opaque user data. */
  void *udata1;
  /** Opaque user data. */
  void *udata2;
  /** Called when the timer is done (finished). */
  void (*on_finish)(void *, void *);
  /** Timer interval, in milliseconds. */
  uint32_t every;
  /** The number of times the timer should be performed. -1 == infinity. */
  int32_t repetitions;
  /** Millisecond at which to start. If missing, filled automatically. */
  int64_t start_at;
} fio_timer_schedule_args_s;

/** Adds a time-bound event to the timer queue. */
SFUNC void fio_timer_schedule(fio_timer_queue_s *timer_queue,
                              fio_timer_schedule_args_s args);

/** A MACRO allowing named arguments to be used. See fio_timer_schedule_args_s.
 */
#define fio_timer_schedule(timer_queue, ...)                                   \
  fio_timer_schedule((timer_queue), (fio_timer_schedule_args_s){__VA_ARGS__})

/** Pushes due events from the timer queue to an event queue. */
SFUNC size_t fio_timer_push2queue(fio_queue_s *queue,
                                  fio_timer_queue_s *timer_queue,
                                  int64_t now_in_milliseconds);

/*
 * Returns the millisecond at which the next event should occur.
 *
 * If no timer is due (list is empty), returns `(uint64_t)-1`.
 *
 * NOTE: unless manually specified, millisecond timers are relative to
 * `fio_time_milli()`.
 */
FIO_IFUNC int64_t fio_timer_next_at(fio_timer_queue_s *timer_queue);

/**
 * Clears any waiting timer bound tasks.
 *
 * NOTE:
 *
 * The timer queue must NEVER be freed when there's a chance that timer tasks
 * are waiting to be performed in a `fio_queue_s`.
 *
 * This is due to the fact that the tasks may try to reschedule themselves (if
 * they repeat).
 */
SFUNC void fio_timer_destroy(fio_timer_queue_s *timer_queue);

/* *****************************************************************************
Queue Inline Helpers
***************************************************************************** */

/** Creates a new queue object (allocated on the heap). */
FIO_IFUNC fio_queue_s *fio_queue_new(void) {
  fio_queue_s *q = (fio_queue_s *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*q), 0);
  if (!q)
    return NULL;
  fio_queue_init(q);
  return q;
}

/** returns the number of tasks in the queue. */
FIO_IFUNC size_t fio_queue_count(fio_queue_s *q) { return q->count; }

/* *****************************************************************************
Timer Queue Inline Helpers
***************************************************************************** */

struct fio___timer_event_s {
  int (*fn)(void *, void *);
  void *udata1;
  void *udata2;
  void (*on_finish)(void *udata1, void *udata2);
  int64_t due;
  uint32_t every;
  int32_t repetitions;
  struct fio___timer_event_s *next;
};

/*
 * Returns the millisecond at which the next event should occur.
 *
 * If no timer is due (list is empty), returns `-1`.
 *
 * NOTE: unless manually specified, millisecond timers are relative to
 * `fio_time_milli()`.
 */
FIO_IFUNC int64_t fio_timer_next_at(fio_timer_queue_s *tq) {
  int64_t v = -1;
  if (!tq)
    goto missing_tq;
  if (!tq || !tq->next)
    return v;
  FIO___LOCK_LOCK(tq->lock);
  if (tq->next)
    v = tq->next->due;
  FIO___LOCK_UNLOCK(tq->lock);
  return v;

missing_tq:
  FIO_LOG_ERROR("`fio_timer_next_at` called with a NULL timer queue!");
  return v;
}

/* *****************************************************************************
Queue Implementation
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE)

/** Initializes a fio_queue_s object. */
FIO_IFUNC void fio_queue_init(fio_queue_s *q) {
  /* do this manually, we don't want to reset a whole page */
  q->r = &q->mem;
  q->w = &q->mem;
  q->count = 0;
  q->lock = FIO___LOCK_INIT;
  q->mem.next = NULL;
  q->mem.r = q->mem.w = q->mem.dir = 0;
}

/** Destroys a queue and re-initializes it, after freeing any used resources. */
SFUNC void fio_queue_destroy(fio_queue_s *q) {
  FIO___LOCK_LOCK(q->lock);
  while (q->r) {
    fio___task_ring_s *tmp = q->r;
    q->r = q->r->next;
    if (tmp != &q->mem)
      FIO_MEM_FREE_(tmp, sizeof(*tmp));
  }
  FIO___LOCK_UNLOCK(q->lock);
  FIO___LOCK_DESTROY(q->lock);
  fio_queue_init(q);
}

/** Frees a queue object after calling fio_queue_destroy. */
SFUNC void fio_queue_free(fio_queue_s *q) {
  fio_queue_destroy(q);
  FIO_MEM_FREE_(q, sizeof(*q));
}

FIO_IFUNC int fio___task_ring_push(fio___task_ring_s *r,
                                   fio_queue_task_s task) {
  if (r->dir && r->r == r->w)
    return -1;
  r->buf[r->w] = task;
  ++r->w;
  if (r->w == FIO_QUEUE_TASKS_PER_ALLOC) {
    r->w = 0;
    r->dir = ~r->dir;
  }
  return 0;
}

FIO_IFUNC int fio___task_ring_unpop(fio___task_ring_s *r,
                                    fio_queue_task_s task) {
  if (r->dir && r->r == r->w)
    return -1;
  if (!r->r) {
    r->r = FIO_QUEUE_TASKS_PER_ALLOC;
    r->dir = ~r->dir;
  }
  --r->r;
  r->buf[r->r] = task;
  return 0;
}

FIO_IFUNC fio_queue_task_s fio___task_ring_pop(fio___task_ring_s *r) {
  fio_queue_task_s t = {.fn = NULL};
  if (!r->dir && r->r == r->w) {
    return t;
  }
  t = r->buf[r->r];
  ++r->r;
  if (r->r == FIO_QUEUE_TASKS_PER_ALLOC) {
    r->r = 0;
    r->dir = ~r->dir;
  }
  return t;
}

int fio_queue_push___(void); /* sublime text marker */
/** Pushes a task to the queue. Returns -1 on error. */
SFUNC int fio_queue_push FIO_NOOP(fio_queue_s *q, fio_queue_task_s task) {
  if (!task.fn)
    return 0;
  FIO___LOCK_LOCK(q->lock);
  if (fio___task_ring_push(q->w, task)) {
    if (q->w != &q->mem && q->mem.next == NULL) {
      q->w->next = &q->mem;
      q->mem.w = q->mem.r = q->mem.dir = 0;
    } else {
      void *tmp = (fio___task_ring_s *)
          FIO_MEM_REALLOC_(NULL, 0, sizeof(*q->w->next), 0);
      if (!tmp)
        goto no_mem;
      q->w->next = (fio___task_ring_s *)tmp;
      if (!FIO_MEM_REALLOC_IS_SAFE_) {
        q->w->next->r = q->w->next->w = q->w->next->dir = 0;

        q->w->next->next = NULL;
      }
    }
    q->w = q->w->next;
    fio___task_ring_push(q->w, task);
  }
  ++q->count;
  FIO___LOCK_UNLOCK(q->lock);
  return 0;
no_mem:
  FIO___LOCK_UNLOCK(q->lock);
  FIO_LOG_ERROR("No memory for Queue %p to increase task ring buffer.",
                (void *)q);
  return -1;
}

int fio_queue_push_urgent___(void); /* sublimetext marker */
/** Pushes a task to the head of the queue. Returns -1 on error (no memory). */
SFUNC int fio_queue_push_urgent FIO_NOOP(fio_queue_s *q,
                                         fio_queue_task_s task) {
  if (!task.fn)
    return 0;
  FIO___LOCK_LOCK(q->lock);
  if (fio___task_ring_unpop(q->r, task)) {
    /* such a shame... but we must allocate a while task block for one task */
    fio___task_ring_s *tmp =
        (fio___task_ring_s *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*q->w->next), 0);
    if (!tmp)
      goto no_mem;
    tmp->next = q->r;
    q->r = tmp;
    tmp->w = 1;
    tmp->dir = tmp->r = 0;
    tmp->buf[0] = task;
  }
  ++q->count;
  FIO___LOCK_UNLOCK(q->lock);
  return 0;
no_mem:
  FIO___LOCK_UNLOCK(q->lock);
  FIO_LOG_ERROR("No memory for Queue %p to increase task ring buffer.",
                (void *)q);
  return -1;
}

/** Pops a task from the queue (FIFO). Returns a NULL task on error. */
SFUNC fio_queue_task_s fio_queue_pop(fio_queue_s *q) {
  fio_queue_task_s t = {.fn = NULL};
  fio___task_ring_s *to_free = NULL;
  if (!q->count)
    return t;
  FIO___LOCK_LOCK(q->lock);
  if (!q->count)
    goto finish;
  if (!(t = fio___task_ring_pop(q->r)).fn) {
    to_free = q->r;
    q->r = to_free->next;
    to_free->next = NULL;
    t = fio___task_ring_pop(q->r);
  }
  if (t.fn && !(--q->count) && q->r != &q->mem) {
    if (to_free && to_free != &q->mem) { // edge case
      FIO_MEM_FREE_(to_free, sizeof(*to_free));
    }
    to_free = q->r;
    q->r = q->w = &q->mem;
    q->mem.w = q->mem.r = q->mem.dir = 0;
  }
finish:
  FIO___LOCK_UNLOCK(q->lock);
  if (to_free && to_free != &q->mem) {
    FIO_MEM_FREE_(to_free, sizeof(*to_free));
  }
  return t;
}

/** Performs a task from the queue. Returns -1 on error (queue empty). */
SFUNC int fio_queue_perform(fio_queue_s *q) {
  fio_queue_task_s t = fio_queue_pop(q);
  if (t.fn) {
    t.fn(t.udata1, t.udata2);
    return 0;
  }
  return -1;
}

/** Performs all tasks in the queue. */
SFUNC void fio_queue_perform_all(fio_queue_s *q) {
  fio_queue_task_s t;
  while ((t = fio_queue_pop(q)).fn)
    t.fn(t.udata1, t.udata2);
}

/* *****************************************************************************
Timer Queue Implementation
***************************************************************************** */

FIO_IFUNC void fio___timer_insert(fio___timer_event_s **pos,
                                  fio___timer_event_s *e) {
  while (*pos && e->due >= (*pos)->due)
    pos = &((*pos)->next);
  e->next = *pos;
  *pos = e;
}

FIO_IFUNC fio___timer_event_s *fio___timer_pop(fio___timer_event_s **pos,
                                               int64_t due) {
  if (!*pos || (*pos)->due > due)
    return NULL;
  fio___timer_event_s *t = *pos;
  *pos = t->next;
  return t;
}

FIO_IFUNC fio___timer_event_s *fio___timer_event_new(
    fio_timer_schedule_args_s args) {
  fio___timer_event_s *t = NULL;
  t = (fio___timer_event_s *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*t), 0);
  if (!t)
    goto init_error;
  if (!args.repetitions)
    args.repetitions = 1;
  *t = (fio___timer_event_s){
      .fn = args.fn,
      .udata1 = args.udata1,
      .udata2 = args.udata2,
      .on_finish = args.on_finish,
      .due = args.start_at + args.every,
      .every = args.every,
      .repetitions = args.repetitions,
  };
  return t;
init_error:
  if (args.on_finish)
    args.on_finish(args.udata1, args.udata2);
  return NULL;
}

FIO_IFUNC void fio___timer_event_free(fio_timer_queue_s *tq,
                                      fio___timer_event_s *t) {
  if (tq && (t->repetitions < 0 || fio_atomic_sub_fetch(&t->repetitions, 1))) {
    FIO___LOCK_LOCK(tq->lock);
    fio___timer_insert(&tq->next, t);
    FIO___LOCK_UNLOCK(tq->lock);
    return;
  }
  if (t->on_finish)
    t->on_finish(t->udata1, t->udata2);
  FIO_MEM_FREE_(t, sizeof(*t));
}

SFUNC void fio___timer_perform(void *timer_, void *t_) {
  fio_timer_queue_s *tq = (fio_timer_queue_s *)timer_;
  fio___timer_event_s *t = (fio___timer_event_s *)t_;
  if (t->fn(t->udata1, t->udata2))
    tq = NULL;
  t->due += t->every;
  fio___timer_event_free(tq, t);
}

/** Pushes due events from the timer queue to an event queue. */
SFUNC size_t fio_timer_push2queue(fio_queue_s *queue,
                                  fio_timer_queue_s *timer,
                                  int64_t start_at) {
  size_t r = 0;
  if (!start_at)
    start_at = fio_time_milli();
  if (FIO___LOCK_TRYLOCK(timer->lock))
    return 0;
  fio___timer_event_s *t;
  while ((t = fio___timer_pop(&timer->next, start_at))) {
    fio_queue_push(queue,
                   .fn = fio___timer_perform,
                   .udata1 = timer,
                   .udata2 = t);
    ++r;
  }
  FIO___LOCK_UNLOCK(timer->lock);
  return r;
}

void fio_timer_schedule___(void); /* sublimetext marker */
/** Adds a time-bound event to the timer queue. */
SFUNC void fio_timer_schedule FIO_NOOP(fio_timer_queue_s *timer,
                                       fio_timer_schedule_args_s args) {
  fio___timer_event_s *t = NULL;
  if (!timer || !args.fn || !args.every)
    goto no_timer_queue;
  if (!args.start_at)
    args.start_at = fio_time_milli();
  t = fio___timer_event_new(args);
  if (!t)
    return;
  FIO___LOCK_LOCK(timer->lock);
  fio___timer_insert(&timer->next, t);
  FIO___LOCK_UNLOCK(timer->lock);
  return;
no_timer_queue:
  if (args.on_finish)
    args.on_finish(args.udata1, args.udata2);
  FIO_LOG_ERROR("fio_timer_schedule called with illegal arguments.");
}

/**
 * Clears any waiting timer bound tasks.
 *
 * NOTE:
 *
 * The timer queue must NEVER be freed when there's a chance that timer tasks
 * are waiting to be performed in a `fio_queue_s`.
 *
 * This is due to the fact that the tasks may try to reschedule themselves (if
 * they repeat).
 */
SFUNC void fio_timer_destroy(fio_timer_queue_s *tq) {
  fio___timer_event_s *next;
  FIO___LOCK_LOCK(tq->lock);
  next = tq->next;
  tq->next = NULL;
  FIO___LOCK_UNLOCK(tq->lock);
  FIO___LOCK_DESTROY(tq->lock);
  while (next) {
    fio___timer_event_s *tmp = next;

    next = next->next;
    fio___timer_event_free(NULL, tmp);
  }
}

/* *****************************************************************************
Queue - test
***************************************************************************** */
#ifdef FIO_TEST_CSTL

#ifndef FIO___QUEUE_TEST_PRINT
#define FIO___QUEUE_TEST_PRINT 0
#endif

#define FIO___QUEUE_TOTAL_COUNT (512 * 1024)

typedef struct {
  fio_queue_s *q;
  uintptr_t count;
} fio___queue_test_s;

FIO_SFUNC void fio___queue_test_sample_task(void *i_count, void *unused2) {
  (void)(unused2);
  fio_atomic_add((uintptr_t *)i_count, 1);
}

FIO_SFUNC void fio___queue_test_static_task(void *i_count1, void *i_count2) {
  static intptr_t counter = 0;
  if (!i_count1 && !i_count2) {
    counter = 0;
    return;
  }
  FIO_ASSERT((intptr_t)i_count1 == (intptr_t)counter + 1,
             "udata1 value error in task");
  FIO_ASSERT((intptr_t)i_count2 == (intptr_t)counter + 2,
             "udata2 value error in task");
  ++counter;
}

FIO_SFUNC void fio___queue_test_sched_sample_task(void *t_, void *i_count) {
  fio___queue_test_s *t = (fio___queue_test_s *)t_;
  for (size_t i = 0; i < t->count; i++) {
    FIO_ASSERT(!fio_queue_push(t->q,
                               .fn = fio___queue_test_sample_task,
                               .udata1 = i_count),
               "Couldn't push task!");
  }
}

FIO_SFUNC int fio___queue_test_timer_task(void *i_count, void *unused2) {
  fio_atomic_add((uintptr_t *)i_count, 1);
  return (unused2 ? -1 : 0);
}

FIO_SFUNC void FIO_NAME_TEST(stl, queue)(void) {
  fprintf(stderr, "* Testing facil.io task scheduling (fio_queue)\n");
  fio_queue_s *q = fio_queue_new();
  fio_queue_s q2;

  fprintf(stderr, "\t- size of queue object (fio_queue_s): %zu\n", sizeof(*q));
  fprintf(stderr,
          "\t- size of queue ring buffer (per allocation): %zu\n",
          sizeof(q->mem));
  fprintf(stderr,
          "\t- event slots per queue allocation: %zu\n",
          (size_t)FIO_QUEUE_TASKS_PER_ALLOC);

  /* test task user data integrity. */
  fio___queue_test_static_task(NULL, NULL);
  for (intptr_t i = 0; i < (FIO_QUEUE_TASKS_PER_ALLOC << 2); ++i) {
    fio_queue_push(q,
                   .fn = fio___queue_test_static_task,
                   .udata1 = (void *)(i + 1),
                   .udata2 = (void *)(i + 2));
  }
  fio_queue_perform_all(q);
  for (intptr_t i = (FIO_QUEUE_TASKS_PER_ALLOC << 2);
       i < (FIO_QUEUE_TASKS_PER_ALLOC << 3);
       ++i) {
    fio_queue_push(q,
                   .fn = fio___queue_test_static_task,
                   .udata1 = (void *)(i + 1),
                   .udata2 = (void *)(i + 2));
  }
  fio_queue_perform_all(q);
  FIO_ASSERT(!fio_queue_count(q) && fio_queue_perform(q) == -1,
             "fio_queue_perform_all didn't perform all");

  const size_t max_threads = 12; // assumption / pure conjuncture...
  uintptr_t i_count;
  clock_t start, end;
  i_count = 0;
  start = clock();
  for (size_t i = 0; i < FIO___QUEUE_TOTAL_COUNT; i++) {
    fio___queue_test_sample_task(&i_count, NULL);
  }
  end = clock();
  if (FIO___QUEUE_TEST_PRINT) {
    fprintf(
        stderr,
        "\t- Queueless (direct call) counter: %lu cycles with i_count = %lu\n",
        (unsigned long)(end - start),
        (unsigned long)i_count);
  }
  size_t i_count_should_be = i_count;
  i_count = 0;
  start = clock();
  for (size_t i = 0; i < FIO___QUEUE_TOTAL_COUNT; i++) {
    fio_queue_push(q,
                   .fn = fio___queue_test_sample_task,
                   .udata1 = (void *)&i_count);
  }
  fio_queue_perform_all(q);
  end = clock();
  if (FIO___QUEUE_TEST_PRINT) {
    fprintf(stderr,
            "\t- single task counter: %lu cycles with i_count = %lu\n",
            (unsigned long)(end - start),
            (unsigned long)i_count);
  }
  FIO_ASSERT(i_count == i_count_should_be, "ERROR: queue count invalid\n");

  if (FIO___QUEUE_TEST_PRINT) {
    fprintf(stderr, "\n");
  }

  for (size_t i = 1; i < 32 && FIO___QUEUE_TOTAL_COUNT >> i; ++i) {
    fio___queue_test_s info = {.q = q,
                               .count =
                                   (uintptr_t)(FIO___QUEUE_TOTAL_COUNT >> i)};
    const size_t tasks = 1 << i;
    i_count = 0;
    start = clock();
    for (size_t j = 0; j < tasks; ++j) {
      fio_queue_push(q,
                     fio___queue_test_sched_sample_task,
                     (void *)&info,
                     &i_count);
    }
    FIO_ASSERT(fio_queue_count(q), "tasks not counted?!");
    {
      const size_t t_count = (i % max_threads) + 1;
      union {
        void *(*t)(void *);
        void (*act)(fio_queue_s *);
      } thread_tasks;
      thread_tasks.act = fio_queue_perform_all;
      fio_thread_t *threads = (fio_thread_t *)
          FIO_MEM_REALLOC_(NULL, 0, sizeof(*threads) * t_count, 0);
      for (size_t j = 0; j < t_count; ++j) {
        if (fio_thread_create(threads + j, thread_tasks.t, q)) {
          abort();
        }
      }
      for (size_t j = 0; j < t_count; ++j) {
        fio_thread_join(threads[j]);
      }
      FIO_MEM_FREE(threads, sizeof(*threads) * t_count);
    }

    end = clock();
    if (FIO___QUEUE_TEST_PRINT) {
      fprintf(stderr,
              "- queue performed using %zu threads, %zu scheduling loops (%zu "
              "each):\n"
              "    %lu cycles with i_count = %lu\n",
              ((i % max_threads) + 1),
              tasks,
              info.count,
              (unsigned long)(end - start),
              (unsigned long)i_count);
    } else {
      fprintf(stderr, ".");
    }
    FIO_ASSERT(i_count == i_count_should_be, "ERROR: queue count invalid\n");
  }
  if (!(FIO___QUEUE_TEST_PRINT))
    fprintf(stderr, "\n");
  FIO_ASSERT(q->w == &q->mem,
             "queue library didn't release dynamic queue (should be static)");
  fio_queue_free(q);
  {
    fprintf(stderr, "* testing urgent insertion\n");
    fio_queue_init(&q2);
    for (size_t i = 0; i < (FIO_QUEUE_TASKS_PER_ALLOC * 3); ++i) {
      FIO_ASSERT(!fio_queue_push_urgent(&q2,
                                        .fn = (void (*)(void *, void *))(i + 1),
                                        .udata1 = (void *)(i + 1)),
                 "fio_queue_push_urgent failed");
    }
    FIO_ASSERT(q2.r->next && q2.r->next->next && !q2.r->next->next->next,
               "should have filled only three task blocks");
    for (size_t i = 0; i < (FIO_QUEUE_TASKS_PER_ALLOC * 3); ++i) {
      fio_queue_task_s t = fio_queue_pop(&q2);
      FIO_ASSERT(
          t.fn && (size_t)t.udata1 == (FIO_QUEUE_TASKS_PER_ALLOC * 3) - i,
          "fio_queue_push_urgent pop ordering error [%zu] %zu != %zu (%p)",
          i,
          (size_t)t.udata1,
          (FIO_QUEUE_TASKS_PER_ALLOC * 3) - i,
          (void *)(uintptr_t)t.fn);
    }
    FIO_ASSERT(fio_queue_pop(&q2).fn == NULL,
               "pop overflow after urgent tasks");
    fio_queue_destroy(&q2);
  }
  {
    fprintf(stderr,
            "* Testing facil.io timer scheduling (fio_timer_queue_s)\n");
    fprintf(stderr, "  Note: Errors SHOULD print out to the log.\n");
    fio_queue_init(&q2);
    uintptr_t tester = 0;
    fio_timer_queue_s tq = FIO_TIMER_QUEUE_INIT;

    /* test failuers */
    fio_timer_schedule(&tq,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 100,
                       .repetitions = -1);
    FIO_ASSERT(tester == 1,
               "fio_timer_schedule should have called `on_finish`");
    tester = 0;
    fio_timer_schedule(NULL,
                       .fn = fio___queue_test_timer_task,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 100,
                       .repetitions = -1);
    FIO_ASSERT(tester == 1,
               "fio_timer_schedule should have called `on_finish`");
    tester = 0;
    fio_timer_schedule(&tq,
                       .fn = fio___queue_test_timer_task,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 0,
                       .repetitions = -1);
    FIO_ASSERT(tester == 1,
               "fio_timer_schedule should have called `on_finish`");
    fprintf(stderr, "  Note: no more errors should pront for this test.\n");

    /* test endless task */
    tester = 0;
    fio_timer_schedule(&tq,
                       .fn = fio___queue_test_timer_task,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 1,
                       .repetitions = -1,
                       .start_at = fio_time_milli() - 10);
    FIO_ASSERT(tester == 0,
               "fio_timer_schedule should have scheduled the task.");
    for (size_t i = 0; i < 10; ++i) {
      uint64_t now = fio_time_milli();
      fio_timer_push2queue(&q2, &tq, now);
      fio_timer_push2queue(&q2, &tq, now);
      FIO_ASSERT(fio_queue_count(&q2), "task should have been scheduled");
      FIO_ASSERT(fio_queue_count(&q2) == 1,
                 "task should have been scheduled only once");
      fio_queue_perform(&q2);
      FIO_ASSERT(!fio_queue_count(&q2), "queue should be empty");
      FIO_ASSERT(tester == i + 1,
                 "task should have been performed (%zu).",
                 (size_t)tester);
    }

    tester = 0;
    fio_timer_destroy(&tq);
    FIO_ASSERT(tester == 1, "fio_timer_destroy should have called `on_finish`");

    /* test single-use task */
    tester = 0;
    int64_t milli_now = fio_time_milli();
    fio_timer_schedule(&tq,
                       .fn = fio___queue_test_timer_task,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 100,
                       .repetitions = 1,
                       .start_at = milli_now - 10);
    FIO_ASSERT(tester == 0,
               "fio_timer_schedule should have scheduled the task.");
    fio_timer_schedule(&tq,
                       .fn = fio___queue_test_timer_task,
                       .udata1 = &tester,
                       .on_finish = fio___queue_test_sample_task,
                       .every = 1,
                       // .repetitions = 1, // auto-value is 1
                       .start_at = milli_now - 10);
    FIO_ASSERT(tester == 0,
               "fio_timer_schedule should have scheduled the task.");
    FIO_ASSERT(fio_timer_next_at(&tq) == milli_now - 9,
               "fio_timer_next_at value error.");
    fio_timer_push2queue(&q2, &tq, milli_now);
    FIO_ASSERT(fio_queue_count(&q2) == 1,
               "task should have been scheduled (2)");
    FIO_ASSERT(fio_timer_next_at(&tq) == milli_now + 90,
               "fio_timer_next_at value error for unscheduled task.");
    fio_queue_perform(&q2);
    FIO_ASSERT(!fio_queue_count(&q2), "queue should be empty");
    FIO_ASSERT(tester == 2,
               "task should have been performed and on_finish called (%zu).",
               (size_t)tester);
    fio_timer_destroy(&tq);
    FIO_ASSERT(
        tester == 3,
        "fio_timer_destroy should have called on_finish of future task (%zu).",
        (size_t)tester);
    FIO_ASSERT(!tq.next, "timer queue should be empty.");
    fio_queue_destroy(&q2);
  }
  fprintf(stderr, "* passed.\n");
}
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
Queue/Timer Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_QUEUE
#endif /* FIO_QUEUE */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "005 riskyhash.h"          /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#include "210 map api.h"            /* Development inclusion - ignore line */
#include "211 ordered map.h"        /* Development inclusion - ignore line */
#include "211 unordered map.h"      /* Development inclusion - ignore line */
#define FIO_CLI                     /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                  CLI helpers - command line interface parsing



***************************************************************************** */
#if defined(FIO_CLI) && !defined(H___FIO_CLI___H) && !defined(FIO_STL_KEEP__)
#define H___FIO_CLI___H 1

/* *****************************************************************************
Internal Macro Implementation
***************************************************************************** */

/** Used internally. */
#define FIO_CLI_STRING__TYPE_I       0x1
#define FIO_CLI_BOOL__TYPE_I         0x2
#define FIO_CLI_INT__TYPE_I          0x3
#define FIO_CLI_PRINT__TYPE_I        0x4
#define FIO_CLI_PRINT_LINE__TYPE_I   0x5
#define FIO_CLI_PRINT_HEADER__TYPE_I 0x6

/** Indicates the CLI argument should be a String (default). */
#define FIO_CLI_STRING(line) (line), ((char *)FIO_CLI_STRING__TYPE_I)
/** Indicates the CLI argument is a Boolean value. */
#define FIO_CLI_BOOL(line) (line), ((char *)FIO_CLI_BOOL__TYPE_I)
/** Indicates the CLI argument should be an Integer (numerical). */
#define FIO_CLI_INT(line) (line), ((char *)FIO_CLI_INT__TYPE_I)
/** Indicates the CLI string should be printed as is with proper offset. */
#define FIO_CLI_PRINT(line) (line), ((char *)FIO_CLI_PRINT__TYPE_I)
/** Indicates the CLI string should be printed as is with no offset. */
#define FIO_CLI_PRINT_LINE(line) (line), ((char *)FIO_CLI_PRINT_LINE__TYPE_I)
/** Indicates the CLI string should be printed as a header. */
#define FIO_CLI_PRINT_HEADER(line)                                             \
  (line), ((char *)FIO_CLI_PRINT_HEADER__TYPE_I)

/* *****************************************************************************
CLI API
***************************************************************************** */

/**
 * This function parses the Command Line Interface (CLI), creating a temporary
 * "dictionary" that allows easy access to the CLI using their names or aliases.
 *
 * Command line arguments may be typed. If an optional type requirement is
 * provided and the provided arument fails to match the required type, execution
 * will end and an error message will be printed along with a short "help".
 *
 * The function / macro accepts the following arguments:
 * - `argc`: command line argument count.
 * - `argv`: command line argument list (array).
 * - `unnamed_min`: the required minimum of un-named arguments.
 * - `unnamed_max`: the maximum limit of un-named arguments.
 * - `description`: a C string containing the program's description.
 * - named arguments list: a list of C strings describing named arguments.
 *
 * The following optional type requirements are:
 *
 * * FIO_CLI_STRING(desc_line)       - (default) string argument.
 * * FIO_CLI_BOOL(desc_line)         - boolean argument (no value).
 * * FIO_CLI_INT(desc_line)          - integer argument.
 * * FIO_CLI_PRINT_HEADER(desc_line) - extra header for output.
 * * FIO_CLI_PRINT(desc_line)        - extra information for output.
 *
 * Argument names MUST start with the '-' character. The first word starting
 * without the '-' character will begin the description for the CLI argument.
 *
 * The arguments "-?", "-h", "-help" and "--help" are automatically handled
 * unless overridden.
 *
 * Un-named arguments shouldn't be listed in the named arguments list.
 *
 * Example use:
 *
 *    fio_cli_start(argc, argv, 0, 0, "The NAME example accepts the following:",
 *                  FIO_CLI_PRINT_HREADER("Concurrency:"),
 *                  FIO_CLI_INT("-t -thread number of threads to run."),
 *                  FIO_CLI_INT("-w -workers number of workers to run."),
 *                  FIO_CLI_PRINT_HREADER("Address Binding:"),
 *                  "-b, -address the address to bind to.",
 *                  FIO_CLI_INT("-p,-port the port to bind to."),
 *                  FIO_CLI_PRINT("\t\tset port to zero (0) for Unix s."),
 *                  FIO_CLI_PRINT_HREADER("Logging:"),
 *                  FIO_CLI_BOOL("-v -log enable logging."));
 *
 *
 * This would allow access to the named arguments:
 *
 *      fio_cli_get("-b") == fio_cli_get("-address");
 *
 *
 * Once all the data was accessed, free the parsed data dictionary using:
 *
 *      fio_cli_end();
 *
 * It should be noted, arguments will be recognized in a number of forms, i.e.:
 *
 *      app -t=1 -p3000 -a localhost
 *
 * This function is NOT thread safe.
 */
#define fio_cli_start(argc, argv, unnamed_min, unnamed_max, description, ...)  \
  fio_cli_start((argc),                                                        \
                (argv),                                                        \
                (unnamed_min),                                                 \
                (unnamed_max),                                                 \
                (description),                                                 \
                (char const *[]){__VA_ARGS__, (char const *)NULL})
/**
 * Never use the function directly, always use the MACRO, because the macro
 * attaches a NULL marker at the end of the `names` argument collection.
 */
SFUNC void fio_cli_start FIO_NOOP(int argc,
                                  char const *argv[],
                                  int unnamed_min,
                                  int unnamed_max,
                                  char const *description,
                                  char const **names);
/**
 * Clears the memory used by the CLI dictionary, removing all parsed data.
 *
 * This function is NOT thread safe.
 */
SFUNC void fio_cli_end(void);

/** Returns the argument's value as a NUL terminated C String. */
SFUNC char const *fio_cli_get(char const *name);

/** Returns the argument's value as an integer. */
SFUNC int fio_cli_get_i(char const *name);

/** This MACRO returns the argument's value as a boolean. */
#define fio_cli_get_bool(name) (fio_cli_get((name)) != NULL)

/** Returns the number of unnamed argument. */
SFUNC unsigned int fio_cli_unnamed_count(void);

/** Returns the unnamed argument using a 0 based `index`. */
SFUNC char const *fio_cli_unnamed(unsigned int index);

/**
 * Sets the argument's value as a NUL terminated C String (no copy!).
 *
 * CAREFUL: This does not automatically detect aliases or type violations! it
 * will only effect the specific name given, even if invalid. i.e.:
 *
 *     fio_cli_start(argc, argv,
 *                  "this is example accepts the following options:",
 *                  "-p -port the port to bind to", FIO_CLI_INT;
 *
 *     fio_cli_set("-p", "hello"); // fio_cli_get("-p") != fio_cli_get("-port");
 *
 * Note: this does NOT copy the C strings to memory. Memory should be kept alive
 *       until `fio_cli_end` is called.
 *
 * This function is NOT thread safe.
 */
SFUNC void fio_cli_set(char const *name, char const *value);

/**
 * This MACRO is the same as:
 *
 *     if(!fio_cli_get(name)) {
 *       fio_cli_set(name, value)
 *     }
 *
 * See fio_cli_set for notes and restrictions.
 */
#define fio_cli_set_default(name, value)                                       \
  if (!fio_cli_get((name)))                                                    \
    fio_cli_set(name, value);

/* *****************************************************************************
CLI Implementation
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/* *****************************************************************************
CLI Data Stores
***************************************************************************** */

typedef struct {
  const char *buf;
  size_t len;
} fio___cli_cstr_s;

#define FIO_RISKY_HASH
#define FIO_MAP_TYPE const char *
#define FIO_MAP_KEY  fio___cli_cstr_s
#define FIO_MAP_KEY_CMP(o1, o2)                                                \
  (o1.len == o2.len &&                                                         \
   (!o1.len || o1.buf == o2.buf ||                                             \
    (o1.buf && o2.buf && !memcmp(o1.buf, o2.buf, o1.len))))
#define FIO_UMAP_NAME fio___cli_hash
#define FIO_STL_KEEP__
#include __FILE__
#undef FIO_STL_KEEP__

static fio___cli_hash_s fio___cli_aliases = FIO_MAP_INIT;
static fio___cli_hash_s fio___cli_values = FIO_MAP_INIT;
static size_t fio___cli_unnamed_count = 0;

typedef struct {
  int unnamed_min;
  int unnamed_max;
  int pos;
  int unnamed_count;
  int argc;
  char const **argv;
  char const *description;
  char const **names;
} fio_cli_parser_data_s;

#define FIO_CLI_HASH_VAL(s)                                                    \
  fio_risky_hash((s).buf, (s).len, (uint64_t)(uintptr_t)fio_cli_start)

/* *****************************************************************************
Default parameter storage
***************************************************************************** */

static fio___cli_cstr_s fio___cli_default_values;

/** extracts the "default" marker from a string's line */
FIO_SFUNC fio___cli_cstr_s fio___cli_map_line2default(char const *line) {
  fio___cli_cstr_s n = {.buf = line};
  /* skip aliases */
  while (n.buf[n.len] == '-') {
    while (n.buf[n.len] && n.buf[n.len] != ' ' && n.buf[n.len] != ',')
      ++n.len;
    while (n.buf[n.len] && (n.buf[n.len] == ' ' || n.buf[n.len] == ',')) {
      ++n.len;
    }
    n.buf += n.len;
    n.len = 0;
  }
  /* a default is maked with (value) or ("value"), both escapable with '\\' */
  if (n.buf[0] != '(')
    goto no_default;
  ++n.buf;
  if (n.buf[0] == '"') {
    ++n.buf;
    /* seek default value end with `")` */
    while (n.buf[n.len] && !(n.buf[n.len] == '"' && n.buf[n.len + 1] == ')'))
      ++n.len;
    if ((n.buf[n.len] != '"' || n.buf[n.len + 1] != ')'))
      goto no_default;
  } else {
    /* seek default value end with `)` */
    while (n.buf[n.len] && n.buf[n.len] != ')')
      ++n.len;
    if (n.buf[n.len] != ')')
      goto no_default;
  }

  return n;
no_default:
  n.buf = NULL;
  n.len = 0;
  return n;
}

FIO_IFUNC fio___cli_cstr_s fio___cli_map_store_default(fio___cli_cstr_s d) {
  fio___cli_cstr_s val = {.buf = NULL, .len = 0};
  if (!d.len || !d.buf)
    return val;
  {
    void *tmp = FIO_MEM_REALLOC_((void *)fio___cli_default_values.buf,
                                 fio___cli_default_values.len,
                                 fio___cli_default_values.len + d.len + 1,
                                 fio___cli_default_values.len);
    if (!tmp)
      return val;
    fio___cli_default_values.buf = (const char *)tmp;
  }
  val.buf = fio___cli_default_values.buf + fio___cli_default_values.len;
  val.len = d.len;
  fio___cli_default_values.len += d.len + 1;

  ((char *)val.buf)[val.len] = 0;
  FIO_MEMCPY((char *)val.buf, d.buf, val.len);
  FIO_LOG_DEBUG("CLI stored a string: %s", val.buf);
  return val;
}

/* *****************************************************************************
CLI Parsing
***************************************************************************** */

FIO_SFUNC void fio___cli_map_line2alias(char const *line) {
  fio___cli_cstr_s n = {.buf = line};
  /* if a line contains a default value, store that value with the aliases. */
  fio___cli_cstr_s def =
      fio___cli_map_store_default(fio___cli_map_line2default(line));
  while (n.buf[0] == '-') {
    while (n.buf[n.len] && n.buf[n.len] != ' ' && n.buf[n.len] != ',') {
      ++n.len;
    }
    const char *old = NULL;
    fio___cli_hash_set(&fio___cli_aliases,
                       FIO_CLI_HASH_VAL(n),
                       n,
                       (char const *)line,
                       &old);
    if (def.buf) {
      fio___cli_hash_set(&fio___cli_values,
                         FIO_CLI_HASH_VAL(n),
                         n,
                         def.buf,
                         NULL);
    }
#ifdef FIO_LOG_ERROR
    if (old) {
      FIO_LOG_ERROR("CLI argument name conflict detected\n"
                    "         The following two directives conflict:\n"
                    "\t%s\n\t%s\n",
                    old,
                    line);
    }
#endif
    while (n.buf[n.len] && (n.buf[n.len] == ' ' || n.buf[n.len] == ',')) {
      ++n.len;
    }
    n.buf += n.len;
    n.len = 0;
  }
}

FIO_SFUNC char const *fio___cli_get_line_type(fio_cli_parser_data_s *parser,
                                              const char *line) {
  if (!line) {
    return NULL;
  }
  char const **pos = parser->names;
  while (*pos) {
    switch ((intptr_t)*pos) {
    case FIO_CLI_STRING__TYPE_I:       /* fallthrough */
    case FIO_CLI_BOOL__TYPE_I:         /* fallthrough */
    case FIO_CLI_INT__TYPE_I:          /* fallthrough */
    case FIO_CLI_PRINT__TYPE_I:        /* fallthrough */
    case FIO_CLI_PRINT_LINE__TYPE_I:   /* fallthrough */
    case FIO_CLI_PRINT_HEADER__TYPE_I: /* fallthrough */
      ++pos;
      continue;
    }
    if (line == *pos) {
      goto found;
    }
    ++pos;
  }
  return NULL;
found:
  switch ((size_t)pos[1]) {
  case FIO_CLI_STRING__TYPE_I:       /* fallthrough */
  case FIO_CLI_BOOL__TYPE_I:         /* fallthrough */
  case FIO_CLI_INT__TYPE_I:          /* fallthrough */
  case FIO_CLI_PRINT__TYPE_I:        /* fallthrough */
  case FIO_CLI_PRINT_LINE__TYPE_I:   /* fallthrough */
  case FIO_CLI_PRINT_HEADER__TYPE_I: /* fallthrough */
    return pos[1];
  }
  return NULL;
}

FIO_SFUNC void fio___cli_print_line(char const *desc, char const *name) {
  char buf[1024];
  size_t pos = 0;
  while (name[0] == '.' || name[0] == '/')
    ++name;
  while (*desc) {
    if (desc[0] == 'N' && desc[1] == 'A' && desc[2] == 'M' && desc[3] == 'E') {
      buf[pos++] = 0;
      desc += 4;
      fprintf(stderr, "%s%s", buf, name);
      pos = 0;
    } else {
      buf[pos++] = *desc;
      ++desc;
      if (pos >= 980) {
        buf[pos++] = 0;
        fwrite(buf, pos, sizeof(*buf), stderr);
        pos = 0;
      }
    }
  }
  if (pos)
    fwrite(buf, pos, sizeof(*buf), stderr);
}

FIO_SFUNC void fio___cli_set_arg(fio___cli_cstr_s arg,
                                 char const *value,
                                 char const *line,
                                 fio_cli_parser_data_s *parser) {
  char const *type = NULL;
  /* handle unnamed argument */
  if (!line || !arg.len) {
    if (!value) {
      goto print_help;
    }
    if (!strcmp(value, "-?") || !strcasecmp(value, "-h") ||
        !strcasecmp(value, "-help") || !strcasecmp(value, "--help")) {
      goto print_help;
    }
    fio___cli_cstr_s n = {.len = (size_t)++parser->unnamed_count};
    fio___cli_hash_set(&fio___cli_values, n.len, n, value, NULL);
    if (parser->unnamed_max >= 0 &&
        parser->unnamed_count > parser->unnamed_max) {
      arg.len = 0;
      goto error;
    }
    FIO_LOG_DEBUG2("(CLI) set an unnamed argument: %s", value);
    FIO_ASSERT_DEBUG(fio___cli_hash_get(&fio___cli_values, n.len, n) == value,
                     "(CLI) set argument failed!");
    return;
  }

  /* validate data types */
  type = fio___cli_get_line_type(parser, line);
  switch ((size_t)type) {
  case FIO_CLI_BOOL__TYPE_I:
    if (value && value != parser->argv[parser->pos + 1]) {
      while (*value) {
        /* support grouped boolean flags with one `-`*/
        char bf[3] = {'-', *value, 0};
        ++value;

        fio___cli_cstr_s a = {.buf = bf, .len = 2};

        const char *l =
            fio___cli_hash_get(&fio___cli_aliases, FIO_CLI_HASH_VAL(a), a);
        if (!l) {
          if (bf[1] == ',')
            continue;
          value = arg.buf + arg.len;
          goto error;
        }
        const char *t = fio___cli_get_line_type(parser, l);
        if (t != (char *)FIO_CLI_BOOL__TYPE_I) {
          value = arg.buf + arg.len;
          goto error;
        }
        fio___cli_set_arg(a, parser->argv[parser->pos + 1], l, parser);
      }
    }
    value = "1";
    break;
  case FIO_CLI_INT__TYPE_I:
    if (value) {
      char const *tmp = value;
      fio_atol((char **)&tmp);
      if (*tmp) {
        goto error;
      }
    }
    /* fallthrough */
  case FIO_CLI_STRING__TYPE_I:
    if (!value)
      goto error;
    if (!value[0])
      goto finish;
    break;
  }

  /* add values using all aliases possible */
  {
    fio___cli_cstr_s n = {.buf = line};
    while (n.buf[0] == '-') {
      while (n.buf[n.len] && n.buf[n.len] != ' ' && n.buf[n.len] != ',') {
        ++n.len;
      }
      fio___cli_hash_set(&fio___cli_values,
                         FIO_CLI_HASH_VAL(n),
                         n,
                         value,
                         NULL);
      FIO_LOG_DEBUG2("(CLI) set argument %.*s = %s", (int)n.len, n.buf, value);
      FIO_ASSERT_DEBUG(fio___cli_hash_get(&fio___cli_values,
                                          FIO_CLI_HASH_VAL(n),
                                          n) == value,
                       "(CLI) set argument failed!");
      while (n.buf[n.len] && (n.buf[n.len] == ' ' || n.buf[n.len] == ',')) {
        ++n.len;
      }
      n.buf += n.len;
      n.len = 0;
    }
  }

finish:

  /* handle additional argv progress (if value is on separate argv) */
  if (value && parser->pos < parser->argc &&
      value == parser->argv[parser->pos + 1])
    ++parser->pos;
  return;

error: /* handle errors*/
  FIO_LOG_DEBUG2("(CLI) error detected, printing help and exiting.");
  fprintf(stderr,
          "\n\r\x1B[31mError:\x1B[0m invalid argument %.*s %s %s\n\n",
          (int)arg.len,
          arg.buf,
          arg.len ? "with value" : "",
          value ? (value[0] ? value : "(empty)") : "(null)");
print_help:
  if (parser->description) {
    fprintf(stderr, "\n");
    fio___cli_print_line(parser->description, parser->argv[0]);
    fprintf(stderr, "\n");
  } else {
    const char *name_tmp = parser->argv[0];
    while (name_tmp[0] == '.' || name_tmp[0] == '/')
      ++name_tmp;
    fprintf(stderr,
            "\nAvailable command-line options for \x1B[1m%s\x1B[0m:\n",
            name_tmp);
  }
  /* print out each line's arguments */
  char const **pos = parser->names;
  while (*pos) {
    switch ((intptr_t)*pos) {
    case FIO_CLI_STRING__TYPE_I:     /* fallthrough */
    case FIO_CLI_BOOL__TYPE_I:       /* fallthrough */
    case FIO_CLI_INT__TYPE_I:        /* fallthrough */
    case FIO_CLI_PRINT__TYPE_I:      /* fallthrough */
    case FIO_CLI_PRINT_LINE__TYPE_I: /* fallthrough */
    case FIO_CLI_PRINT_HEADER__TYPE_I:
      ++pos;
      continue;
    }
    type = (char *)FIO_CLI_STRING__TYPE_I;
    switch ((intptr_t)pos[1]) {
    case FIO_CLI_PRINT__TYPE_I:
      fprintf(stderr, "          \t   ");
      fio___cli_print_line(pos[0], parser->argv[0]);
      fprintf(stderr, "\n");
      pos += 2;
      continue;
    case FIO_CLI_PRINT_LINE__TYPE_I:
      fio___cli_print_line(pos[0], parser->argv[0]);
      fprintf(stderr, "\n");
      pos += 2;
      continue;
    case FIO_CLI_PRINT_HEADER__TYPE_I:
      fprintf(stderr, "\n\x1B[4m");
      fio___cli_print_line(pos[0], parser->argv[0]);
      fprintf(stderr, "\x1B[0m\n");
      pos += 2;
      continue;

    case FIO_CLI_STRING__TYPE_I: /* fallthrough */
    case FIO_CLI_BOOL__TYPE_I:   /* fallthrough */
    case FIO_CLI_INT__TYPE_I:    /* fallthrough */
      type = pos[1];
    }
    /* print line @ pos, starting with main argument name */
    int alias_count = 0;
    int first_len = 0;
    size_t tmp = 0;
    char const *const p = *pos;
    fio___cli_cstr_s def = fio___cli_map_line2default(p);
    while (p[tmp] == '-') {
      while (p[tmp] && p[tmp] != ' ' && p[tmp] != ',') {
        if (!alias_count)
          ++first_len;
        ++tmp;
      }
      ++alias_count;
      while (p[tmp] && (p[tmp] == ' ' || p[tmp] == ',')) {
        ++tmp;
      }
    }
    if (def.len) {
      tmp = (size_t)((def.buf + def.len + 1) - p);
      tmp += (p[tmp] == ')'); /* in case of `")` */
      while (p[tmp] && (p[tmp] == ' ' || p[tmp] == ',')) {
        ++tmp;
      }
    }
    switch ((size_t)type) {
    case FIO_CLI_STRING__TYPE_I:
      fprintf(stderr,
              " \x1B[1m%-10.*s\x1B[0m\x1B[2m\t\"\" \x1B[0m%s\n",
              first_len,
              p,
              p + tmp);
      break;
    case FIO_CLI_BOOL__TYPE_I:
      fprintf(stderr, " \x1B[1m%-10.*s\x1B[0m\t   %s\n", first_len, p, p + tmp);
      break;
    case FIO_CLI_INT__TYPE_I:
      fprintf(stderr,
              " \x1B[1m%-10.*s\x1B[0m\x1B[2m\t## \x1B[0m%s\n",
              first_len,
              p,
              p + tmp);
      break;
    }
    /* print alias information */
    tmp = first_len;
    while (p[tmp] && (p[tmp] == ' ' || p[tmp] == ',')) {
      ++tmp;
    }
    while (p[tmp] == '-') {
      const size_t start = tmp;
      while (p[tmp] && p[tmp] != ' ' && p[tmp] != ',') {
        ++tmp;
      }
      int padding = first_len - (tmp - start);
      if (padding < 0)
        padding = 0;
      switch ((size_t)type) {
      case FIO_CLI_STRING__TYPE_I:
        fprintf(stderr,
                " \x1B[1m%-10.*s\x1B[0m\x1B[2m\t\"\" \x1B[0m%.*s\x1B[2msame as "
                "%.*s\x1B[0m\n",
                (int)(tmp - start),
                p + start,
                padding,
                "",
                first_len,
                p);
        break;
      case FIO_CLI_BOOL__TYPE_I:
        fprintf(stderr,
                " \x1B[1m%-10.*s\x1B[0m\t   %.*s\x1B[2msame as %.*s\x1B[0m\n",
                (int)(tmp - start),
                p + start,
                padding,
                "",
                first_len,
                p);
        break;
      case FIO_CLI_INT__TYPE_I:
        fprintf(stderr,
                " \x1B[1m%-10.*s\x1B[0m\x1B[2m\t## \x1B[0m%.*s\x1B[2msame as "
                "%.*s\x1B[0m\n",
                (int)(tmp - start),
                p + start,
                padding,
                "",
                first_len,
                p);
        break;
      }
    }
    /* print default information */
    if (def.len)
      fprintf(stderr,
              "           \t\x1B[2mdefault value: %.*s\x1B[0m\n",
              (int)def.len,
              def.buf);
    ++pos;
  }
  fprintf(stderr,
          "\nUse any of the following input formats:\n"
          "\t-arg <value>\t-arg=<value>\t-arg<value>\n"
          "\n"
          "Use \x1B[1m-h\x1B[0m , \x1B[1m-help\x1B[0m or "
          "\x1B[1m-?\x1B[0m "
          "to get this information again.\n"
          "\n");
  fio_cli_end();
  exit(0);
}

/* *****************************************************************************
CLI Initialization
***************************************************************************** */

void fio_cli_start___(void); /* sublime text marker */
SFUNC void fio_cli_start FIO_NOOP(int argc,
                                  char const *argv[],
                                  int unnamed_min,
                                  int unnamed_max,
                                  char const *description,
                                  char const **names) {
  if (unnamed_max >= 0 && unnamed_max < unnamed_min)
    unnamed_max = unnamed_min;
  fio_cli_parser_data_s parser = {
      .unnamed_min = unnamed_min,
      .unnamed_max = unnamed_max,
      .pos = 0,
      .argc = argc,
      .argv = argv,
      .description = description,
      .names = names,
  };

  if (fio___cli_hash_count(&fio___cli_values)) {
    fio_cli_end();
  }

  /* prepare aliases hash map */

  char const **line = names;
  while (*line) {
    switch ((intptr_t)*line) {
    case FIO_CLI_STRING__TYPE_I:       /* fallthrough */
    case FIO_CLI_BOOL__TYPE_I:         /* fallthrough */
    case FIO_CLI_INT__TYPE_I:          /* fallthrough */
    case FIO_CLI_PRINT__TYPE_I:        /* fallthrough */
    case FIO_CLI_PRINT_LINE__TYPE_I:   /* fallthrough */
    case FIO_CLI_PRINT_HEADER__TYPE_I: /* fallthrough */
      ++line;
      continue;
    }
    if (line[1] != (char *)FIO_CLI_PRINT__TYPE_I &&
        line[1] != (char *)FIO_CLI_PRINT_LINE__TYPE_I &&
        line[1] != (char *)FIO_CLI_PRINT_HEADER__TYPE_I)
      fio___cli_map_line2alias(*line);
    ++line;
  }

  /* parse existing arguments */

  while ((++parser.pos) < argc) {
    char const *value = NULL;
    fio___cli_cstr_s n = {.buf = argv[parser.pos],
                          .len = strlen(argv[parser.pos])};
    if (parser.pos + 1 < argc) {
      value = argv[parser.pos + 1];
    }
    const char *l = NULL;
    while (
        n.len &&
        !(l = fio___cli_hash_get(&fio___cli_aliases, FIO_CLI_HASH_VAL(n), n))) {
      --n.len;
      value = n.buf + n.len;
    }
    if (n.len && value && value[0] == '=') {
      ++value;
    }
    // fprintf(stderr, "Setting %.*s to %s\n", (int)n.len, n.buf, value);
    fio___cli_set_arg(n, value, l, &parser);
  }

  /* Cleanup and save state for API */
  fio___cli_hash_destroy(&fio___cli_aliases);
  fio___cli_unnamed_count = parser.unnamed_count;
  /* test for required unnamed arguments */
  if (parser.unnamed_count < parser.unnamed_min)
    fio___cli_set_arg((fio___cli_cstr_s){.len = 0}, NULL, NULL, &parser);
}

/* *****************************************************************************
CLI Destruction
***************************************************************************** */

SFUNC void __attribute__((destructor)) fio_cli_end(void) {
  fio___cli_hash_destroy(&fio___cli_values);
  fio___cli_hash_destroy(&fio___cli_aliases);
  fio___cli_unnamed_count = 0;
  if (fio___cli_default_values.buf) {
    FIO_MEM_FREE_((void *)fio___cli_default_values.buf,
                  fio___cli_default_values.len);
    fio___cli_default_values.buf = NULL;
    fio___cli_default_values.len = 0;
  }
}
/* *****************************************************************************
CLI Data Access API
***************************************************************************** */

/** Returns the argument's value as a NUL terminated C String. */
SFUNC char const *fio_cli_get(char const *name) {
  if (!name)
    return NULL;
  fio___cli_cstr_s n = {.buf = name, .len = strlen(name)};
  if (!fio___cli_hash_count(&fio___cli_values)) {
    return NULL;
  }
  char const *val =
      fio___cli_hash_get(&fio___cli_values, FIO_CLI_HASH_VAL(n), n);
  return val;
}

/** Returns the argument's value as an integer. */
SFUNC int fio_cli_get_i(char const *name) {
  char const *val = fio_cli_get(name);
  return fio_atol((char **)&val);
}

/** Returns the number of unrecognized argument. */
SFUNC unsigned int fio_cli_unnamed_count(void) {
  return (unsigned int)fio___cli_unnamed_count;
}

/** Returns the unrecognized argument using a 0 based `index`. */
SFUNC char const *fio_cli_unnamed(unsigned int index) {
  if (!fio___cli_hash_count(&fio___cli_values) || !fio___cli_unnamed_count) {
    return NULL;
  }
  fio___cli_cstr_s n = {.buf = NULL, .len = index + 1};
  return fio___cli_hash_get(&fio___cli_values, index + 1, n);
}

/**
 * Sets the argument's value as a NUL terminated C String (no copy!).
 *
 * Note: this does NOT copy the C strings to memory. Memory should be kept
 * alive until `fio_cli_end` is called.
 */
SFUNC void fio_cli_set(char const *name, char const *value) {
  fio___cli_cstr_s n = (fio___cli_cstr_s){.buf = name, .len = strlen(name)};
  fio___cli_hash_set(&fio___cli_values, FIO_CLI_HASH_VAL(n), n, value, NULL);
}

/* *****************************************************************************
CLI - test
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, cli)(void) {
  const char *argv[] = {
      "appname",
      "-i11",
      "-i2=2",
      "-i3",
      "3",
      "-t,u",
      "-s",
      "test",
      "unnamed",
  };
  const int argc = sizeof(argv) / sizeof(argv[0]);
  fprintf(stderr, "* Testing CLI helpers.\n");
  { /* avoid macro for C++ */
    const char *arguments[] = {
        FIO_CLI_INT("-integer1 -i1 first integer"),
        FIO_CLI_INT("-integer2 -i2 second integer"),
        FIO_CLI_INT("-integer3 -i3 third integer"),
        FIO_CLI_INT("-integer4 -i4 (4) fourth integer"),
        FIO_CLI_INT("-integer5 -i5 (\"5\") fifth integer"),
        FIO_CLI_BOOL("-boolean -t boolean"),
        FIO_CLI_BOOL("-boolean2 -u boolean"),
        FIO_CLI_BOOL("-boolean_false -f boolean"),
        FIO_CLI_STRING("-str -s a string"),
        FIO_CLI_PRINT_HEADER("Printing stuff"),
        FIO_CLI_PRINT_LINE("does nothing, but shouldn't crash either"),
        FIO_CLI_PRINT("does nothing, but shouldn't crash either"),
        NULL,
    };
    fio_cli_start FIO_NOOP(argc, argv, 0, -1, NULL, arguments);
  }
  FIO_ASSERT(fio_cli_get_i("-i2") == 2, "CLI second integer error.");
  FIO_ASSERT(fio_cli_get_i("-i3") == 3, "CLI third integer error.");
  FIO_ASSERT(fio_cli_get_i("-i4") == 4,
             "CLI fourth integer error (%s).",
             fio_cli_get("-i4"));
  FIO_ASSERT(fio_cli_get_i("-i5") == 5,
             "CLI fifth integer error (%s).",
             fio_cli_get("-i5"));
  FIO_ASSERT(fio_cli_get_i("-i1") == 1, "CLI first integer error.");
  FIO_ASSERT(fio_cli_get_i("-i2") == fio_cli_get_i("-integer2"),
             "CLI second integer error.");
  FIO_ASSERT(fio_cli_get_i("-i3") == fio_cli_get_i("-integer3"),
             "CLI third integer error.");
  FIO_ASSERT(fio_cli_get_i("-i1") == fio_cli_get_i("-integer1"),
             "CLI first integer error.");
  FIO_ASSERT(fio_cli_get_i("-t") == 1, "CLI boolean true error.");
  FIO_ASSERT(fio_cli_get_i("-u") == 1, "CLI boolean 2 true error.");
  FIO_ASSERT(fio_cli_get_i("-f") == 0, "CLI boolean false error.");
  FIO_ASSERT(!strcmp(fio_cli_get("-s"), "test"), "CLI string error.");
  FIO_ASSERT(fio_cli_unnamed_count() == 1, "CLI unnamed count error.");
  FIO_ASSERT(!strcmp(fio_cli_unnamed(0), "unnamed"), "CLI unnamed error.");
  fio_cli_set("-manual", "okay");
  FIO_ASSERT(!strcmp(fio_cli_get("-manual"), "okay"), "CLI set/get error.");
  fio_cli_end();
  FIO_ASSERT(fio_cli_get_i("-i1") == 0, "CLI cleanup error.");
}
#endif /* FIO_TEST_CSTL */

/* *****************************************************************************
CLI - cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE*/
#endif /* FIO_CLI */
#undef FIO_CLI
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_SOCK                    /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                        Basic Socket Helpers / IO Polling



Example:
********************************************************************************

#define FIO_SOCK
#define FIO_CLI
#define FIO_LOG
#include "fio-stl.h" // __FILE__

typedef struct {
  int fd;
  unsigned char is_client;
} state_s;

static void on_data_server(int fd, size_t index, void *udata) {
  (void)udata; // unused for server
  (void)index; // we don't use the array index in this example
  char buf[65536];
  FIO_MEMCPY(buf, "echo: ", 6);
  ssize_t len = 0;
  struct sockaddr_storage peer;
  socklen_t peer_addrlen = sizeof(peer);
  len = recvfrom(fd, buf + 6, (65536 - 7), 0, (struct sockaddr *)&peer,
                 &peer_addrlen);
  if (len <= 0)
    return;
  buf[len + 6] = 0;
  fprintf(stderr, "Recieved: %s", buf + 6);
  // sends all data in UDP, with TCP sending may be partial
  len =
      sendto(fd, buf, len + 6, 0, (const struct sockaddr *)&peer, peer_addrlen);
  if (len < 0)
    perror("error");
}

static void on_data_client(int fd, size_t index, void *udata) {
  state_s *state = (state_s *)udata;
  fprintf(stderr, "on_data_client %zu\n", index);
  if (!index) // stdio is index 0 in the fd list
    goto is_stdin;
  char buf[65536];
  ssize_t len = 0;
  struct sockaddr_storage peer;
  socklen_t peer_addrlen = sizeof(peer);
  len = recvfrom(fd, buf, 65535, 0, (struct sockaddr *)&peer, &peer_addrlen);
  if (len <= 0)
    return;
  buf[len] = 0;
  fprintf(stderr, "%s", buf);
  return;
is_stdin:
  len = read(fd, buf, 65535);
  if (len <= 0)
    return;
  buf[len] = 0;
  // sends all data in UDP, with TCP sending may be partial
  len = send(state->fd, buf, len, 0);
  fprintf(stderr, "Sent: %zd bytes\n", len);
  if (len < 0)
    perror("error");
  return;
  (void)udata;
}

int main(int argc, char const *argv[]) {
  // Using CLI to set address, port and client/server mode.
  fio_cli_start(
      argc, argv, 0, 0, "UDP echo server / client example.",
      FIO_CLI_PRINT_HEADER("Address Binding"),
      FIO_CLI_STRING("-address -b address to listen / connect to."),
      FIO_CLI_INT("-port -p port to listen / connect to. Defaults to 3030."),
      FIO_CLI_PRINT_HEADER("Operation Mode"),
      FIO_CLI_BOOL("-client -c Client mode."),
      FIO_CLI_BOOL("-verbose -v verbose mode (debug messages on)."));

  if (fio_cli_get_bool("-v"))
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  fio_cli_set_default("-p", "3030");

  // Using FIO_SOCK functions for setting up UDP server / client
  state_s state = {.is_client = fio_cli_get_bool("-c")};
  state.fd = fio_sock_open(
      fio_cli_get("-b"), fio_cli_get("-p"),
      FIO_SOCK_UDP | FIO_SOCK_NONBLOCK |
          (fio_cli_get_bool("-c") ? FIO_SOCK_CLIENT : FIO_SOCK_SERVER));

  if (state.fd == -1) {
    FIO_LOG_FATAL("Couldn't open socket!");
    exit(1);
  }
  FIO_LOG_DEBUG("UDP socket open on fd %d", state.fd);

  if (state.is_client) {
    int i =
        send(state.fd, "Client hello... further data will be sent using REPL\n",
             53, 0);
    fprintf(stderr, "Sent: %d bytes\n", i);
    if (i < 0)
      perror("error");
    while (fio_sock_poll(.on_data = on_data_client, .udata = (void *)&state,
                         .timeout = 1000,
                         .fds = FIO_SOCK_POLL_LIST(
                             FIO_SOCK_POLL_R(fileno(stdin)),
                             FIO_SOCK_POLL_R(state.fd))) >= 0)
      ;
  } else {
    while (fio_sock_poll(.on_data = on_data_server, .udata = (void *)&state,
                         .timeout = 1000,
                         .fds = FIO_SOCK_POLL_LIST(
                             FIO_SOCK_POLL_R(state.fd))) >= 0)
      ;
  }
  // we should cleanup, though we'll exit with Ctrl+C, so it's won't matter.
  fio_cli_end();
  fio_sock_close(state.fd);
  return 0;
  (void)argv;
}


***************************************************************************** */
#if defined(FIO_SOCK) && !defined(FIO_SOCK_POLL_LIST)

/* *****************************************************************************
OS specific patches.
***************************************************************************** */
#if FIO_OS_WIN
#if _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef FIO_SOCK_FD_ISVALID
#define FIO_SOCK_FD_ISVALID(fd) ((size_t)fd <= (size_t)0x7FFFFFFF)
#endif
/** Acts as POSIX write. Use this macro for portability with WinSock2. */
#define fio_sock_write(fd, data, len) send((fd), (data), (len), 0)
/** Acts as POSIX read. Use this macro for portability with WinSock2. */
#define fio_sock_read(fd, buf, len) recv((fd), (buf), (len), 0)
/** Acts as POSIX close. Use this macro for portability with WinSock2. */
#define fio_sock_close(fd) closesocket(fd)
/** Protects against type size overflow on Windows, where FD > MAX_INT. */
FIO_IFUNC int fio_sock_accept(int s, struct sockaddr *addr, int *addrlen) {
  int r = -1;
  SOCKET c = accept(s, addr, addrlen);
  if (c == INVALID_SOCKET)
    return r;
  if (FIO_SOCK_FD_ISVALID(c)) {
    r = (int)c;
    return r;
  }
  closesocket(c);
  errno = ERANGE;
  FIO_LOG_ERROR("Windows SOCKET value overflowed int limits (was: %zu)",
                (size_t)c);
  return r;
}
#define accept fio_sock_accept

#elif FIO_HAVE_UNIX_TOOLS
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#ifndef FIO_SOCK_FD_ISVALID
#define FIO_SOCK_FD_ISVALID(fd) ((int)fd != (int)-1)
#endif
/** Acts as POSIX write. Use this macro for portability with WinSock2. */
#define fio_sock_write(fd, data, len) write((fd), (data), (len))
/** Acts as POSIX read. Use this macro for portability with WinSock2. */
#define fio_sock_read(fd, buf, len)   read((fd), (buf), (len))
/** Acts as POSIX close. Use this macro for portability with WinSock2. */
#define fio_sock_close(fd)            close(fd)
#else
#error FIO_SOCK requires a supported OS (Windows / POSIX).
#endif

/* *****************************************************************************
IO Poll - API
***************************************************************************** */
#define FIO_SOCK_POLL_RW(fd_)                                                  \
  (struct pollfd) { .fd = fd_, .events = (POLLIN | POLLOUT) }
#define FIO_SOCK_POLL_R(fd_)                                                   \
  (struct pollfd) { .fd = fd_, .events = POLLIN }
#define FIO_SOCK_POLL_W(fd_)                                                   \
  (struct pollfd) { .fd = fd_, .events = POLLOUT }
#define FIO_SOCK_POLL_LIST(...)                                                \
  (struct pollfd[]) {                                                          \
    __VA_ARGS__, (struct pollfd) { .fd = -1 }                                  \
  }

typedef enum {
  FIO_SOCK_SERVER = 0,
  FIO_SOCK_CLIENT = 1,
  FIO_SOCK_NONBLOCK = 2,
  FIO_SOCK_TCP = 4,
  FIO_SOCK_UDP = 8,
#if FIO_OS_POSIX
  FIO_SOCK_UNIX = 16,
#endif
} fio_sock_open_flags_e;

/**
 * Creates a new socket according to the provided flags.
 *
 * The `port` string will be ignored when `FIO_SOCK_UNIX` is set.
 */
FIO_IFUNC int fio_sock_open(const char *restrict address,
                            const char *restrict port,
                            uint16_t flags);

/** Creates a new socket, according to the provided flags. */
SFUNC int fio_sock_open2(const char *url, uint16_t flags);

/**
 * Attempts to resolve an address to a valid IP6 / IP4 address pointer.
 *
 * The `sock_type` element should be a socket type, such as `SOCK_DGRAM` (UDP)
 * or `SOCK_STREAM` (TCP/IP).
 *
 * The address should be freed using `fio_sock_address_free`.
 */
FIO_IFUNC struct addrinfo *fio_sock_address_new(const char *restrict address,
                                                const char *restrict port,
                                                int sock_type);

/** Frees the pointer returned by `fio_sock_address_new`. */
FIO_IFUNC void fio_sock_address_free(struct addrinfo *a);

/** Creates a new network socket and binds it to a local address. */
SFUNC int fio_sock_open_local(struct addrinfo *addr, int nonblock);

/** Creates a new network socket and connects it to a remote address. */
SFUNC int fio_sock_open_remote(struct addrinfo *addr, int nonblock);

#if FIO_OS_POSIX
/** Creates a new Unix socket and binds it to a local address. */
SFUNC int fio_sock_open_unix(const char *address, int is_client, int nonblock);
#endif

/** Sets a file descriptor / socket to non blocking state. */
SFUNC int fio_sock_set_non_block(int fd);

/** Attempts to maximize the allowed open file limits. returns known limit */
SFUNC size_t fio_sock_maximize_limits(void);

/**
 * Returns 0 on timeout, -1 on error or the events that are valid.
 *
 * Possible events are POLLIN | POLLOUT
 */
SFUNC short fio_sock_wait_io(int fd, short events, int timeout);

/** A helper macro that waits on a single IO with no callbacks (0 = no event) */
#define FIO_SOCK_WAIT_RW(fd, timeout_)                                         \
  fio_sock_wait_io(fd, POLLIN | POLLOUT, timeout_)

/** A helper macro that waits on a single IO with no callbacks (0 = no event) */
#define FIO_SOCK_WAIT_R(fd, timeout_) fio_sock_wait_io(fd, POLLIN, timeout_)

/** A helper macro that waits on a single IO with no callbacks (0 = no event) */
#define FIO_SOCK_WAIT_W(fd, timeout_) fio_sock_wait_io(fd, POLLOUT, timeout_)

/* *****************************************************************************
Small Poll API
***************************************************************************** */

typedef struct {
  /** Called after polling but before any events are processed. */
  void (*before_events)(void *udata);
  /** Called when the fd can be written too (available outgoing buffer). */
  void (*on_ready)(int fd, size_t index, void *udata);
  /** Called when data iis available to be read from the fd. */
  void (*on_data)(int fd, size_t index, void *udata);
  /** Called on error or when the fd was closed. */
  void (*on_error)(int fd, size_t index, void *udata);
  /** Called after polling and after all events are processed. */
  void (*after_events)(void *udata);
  /** An opaque user data pointer. */
  void *udata;
  /** A pointer to the fd pollin array. */
  struct pollfd *fds;
  /**
   * the number of fds to listen to.
   *
   * If zero, and `fds` is set, it will be auto-calculated trying to find the
   * first array member where `events == 0`. Make sure to supply this end
   * marker, of the buffer may overrun!
   */
  uint32_t count;
  /** timeout for the polling system call. */
  int timeout;
} fio_sock_poll_args;

/**
 * The `fio_sock_poll` function uses the `poll` system call to poll a simple IO
 * list.
 *
 * The list must end with a `struct pollfd` with it's `events` set to zero. No
 * other member of the list should have their `events` data set to zero.
 *
 * It is recommended to use the `FIO_SOCK_POLL_LIST(...)` and
 * `FIO_SOCK_POLL_[RW](fd)` macros. i.e.:
 *
 *     int count = fio_sock_poll(.on_ready = on_ready,
 *                         .on_data = on_data,
 *                         .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(io_fd)));
 *
 * NOTE: The `poll` system call should perform reasonably well for light loads
 * (short lists). However, for complex IO needs or heavier loads, use the
 * system's native IO API, such as kqueue or epoll.
 */
FIO_IFUNC int fio_sock_poll(fio_sock_poll_args args);
#define fio_sock_poll(...) fio_sock_poll((fio_sock_poll_args){__VA_ARGS__})

/* *****************************************************************************
IO Poll - Implementation (always static / inlined)
***************************************************************************** */

FIO_SFUNC void fio___sock_poll_mock_ev(int fd, size_t index, void *udata) {
  (void)fd;
  (void)index;
  (void)udata;
}

int fio_sock_poll____(void); /* sublime text marker */
FIO_IFUNC int fio_sock_poll FIO_NOOP(fio_sock_poll_args args) {
  size_t event_count = 0;
  size_t limit = 0;
  if (!args.fds)
    goto empty_list;
  if (!args.count)
    while (args.fds[args.count].events)
      ++args.count;
  if (!args.count)
    goto empty_list;

  /* move if statement out of loop using a move callback */
  if (!args.on_ready)
    args.on_ready = fio___sock_poll_mock_ev;
  if (!args.on_data)
    args.on_data = fio___sock_poll_mock_ev;
  if (!args.on_error)
    args.on_error = fio___sock_poll_mock_ev;
#if FIO_OS_WIN
  event_count = WSAPoll(args.fds, args.count, args.timeout);
#else
  event_count = poll(args.fds, args.count, args.timeout);
#endif
  if (args.before_events)
    args.before_events(args.udata);
  if (event_count <= 0)
    goto finish;
  for (size_t i = 0; i < args.count && limit < event_count; ++i) {
    if (!args.fds[i].revents)
      continue;
    ++limit;
    if ((args.fds[i].revents & POLLOUT))
      args.on_ready(args.fds[i].fd, i, args.udata);
    if ((args.fds[i].revents & POLLIN))
      args.on_data(args.fds[i].fd, i, args.udata);
    if ((args.fds[i].revents & (POLLERR | POLLNVAL)))
      args.on_error(args.fds[i].fd, i, args.udata); /* TODO: POLLHUP ? */
  }
finish:
  if (args.after_events)
    args.after_events(args.udata);
  return event_count;
empty_list:
  if (args.timeout)
    FIO_THREAD_WAIT(args.timeout);
  if (args.before_events)
    args.before_events(args.udata);
  if (args.after_events)
    args.after_events(args.udata);
  return 0;
}

/**
 * Creates a new socket according to the provided flags.
 *
 * The `port` string will be ignored when `FIO_SOCK_UNIX` is set.
 */
FIO_IFUNC int fio_sock_open(const char *restrict address,
                            const char *restrict port,
                            uint16_t flags) {
  struct addrinfo *addr = NULL;
  int fd;
  switch ((flags & ((uint16_t)FIO_SOCK_TCP | (uint16_t)FIO_SOCK_UDP
#if FIO_OS_POSIX
                    | (uint16_t)FIO_SOCK_UNIX
#endif
                    ))) {
  case FIO_SOCK_UDP:
    addr = fio_sock_address_new(address, port, SOCK_DGRAM);
    if (!addr) {
      FIO_LOG_ERROR("(fio_sock_open) address error: %s", strerror(errno));
      return -1;
    }
    if ((flags & FIO_SOCK_CLIENT)) {
      fd = fio_sock_open_remote(addr, (flags & FIO_SOCK_NONBLOCK));
    } else {
      fd = fio_sock_open_local(addr, (flags & FIO_SOCK_NONBLOCK));
    }
    fio_sock_address_free(addr);
    return fd;
  case FIO_SOCK_TCP:
    addr = fio_sock_address_new(address, port, SOCK_STREAM);
    if (!addr) {
      FIO_LOG_ERROR("(fio_sock_open) address error: %s", strerror(errno));
      return -1;
    }
    if ((flags & FIO_SOCK_CLIENT)) {
      fd = fio_sock_open_remote(addr, (flags & FIO_SOCK_NONBLOCK));
    } else {
      fd = fio_sock_open_local(addr, (flags & FIO_SOCK_NONBLOCK));
      if (fd != -1 && listen(fd, SOMAXCONN) == -1) {
        FIO_LOG_ERROR("(fio_sock_open) failed on call to listen: %s",
                      strerror(errno));
        fio_sock_close(fd);
        fd = -1;
      }
    }
    fio_sock_address_free(addr);
    return fd;
#if FIO_OS_POSIX
  case FIO_SOCK_UNIX:
    return fio_sock_open_unix(address,
                              (flags & FIO_SOCK_CLIENT),
                              (flags & FIO_SOCK_NONBLOCK));
#endif
  }
  FIO_LOG_ERROR("(fio_sock_open) the FIO_SOCK_TCP, FIO_SOCK_UDP, and "
                "FIO_SOCK_UNIX flags are exclusive");
  return -1;
}

FIO_IFUNC struct addrinfo *fio_sock_address_new(
    const char *restrict address,
    const char *restrict port,
    int sock_type /*i.e., SOCK_DGRAM */) {
  struct addrinfo addr_hints = (struct addrinfo){0}, *a;
  int e;
  addr_hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
  addr_hints.ai_socktype = sock_type;
  addr_hints.ai_flags = AI_PASSIVE; // use my IP

  if ((e = getaddrinfo(address, (port ? port : "0"), &addr_hints, &a)) != 0) {
    FIO_LOG_ERROR("(fio_sock_address_new(\"%s\", \"%s\")) error: %s",
                  (address ? address : "NULL"),
                  (port ? port : "0"),
                  gai_strerror(e));
    return NULL;
  }
  return a;
}

FIO_IFUNC void fio_sock_address_free(struct addrinfo *a) { freeaddrinfo(a); }

/* *****************************************************************************
FIO_SOCK - Implementation
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE)

/** Creates a new socket, according to the provided flags. */
SFUNC int fio_sock_open2(const char *url, uint16_t flags) {
  char buf[2048];
  char port[64];
  char *addr = buf;
  char *pr = port;

  /* parse URL */
  fio_url_s u = fio_url_parse(url, strlen(url));
#if FIO_OS_POSIX
  if (!u.host.buf && !u.port.buf && u.path.buf) {
    /* unix socket */
    flags &= FIO_SOCK_SERVER | FIO_SOCK_CLIENT | FIO_SOCK_NONBLOCK;
    flags |= FIO_SOCK_UNIX;
    if (u.path.len >= 2048) {
      errno = EINVAL;
      FIO_LOG_ERROR("Couldn't open socket to %s - host name too long.", url);
      return -1;
    }
    FIO_MEMCPY(buf, u.path.buf, u.path.len);
    buf[u.path.len] = 0;
    pr = NULL;
  } else
#endif
  {
    if (!u.port.len)
      u.port = u.scheme;
    if (!u.port.len) {
      pr = NULL;
    } else {
      if (u.port.len >= 64) {
        errno = EINVAL;
        FIO_LOG_ERROR("Couldn't open socket to %s - port / scheme too long.",
                      url);
        return -1;
      }
      FIO_MEMCPY(port, u.port.buf, u.port.len);
      port[u.port.len] = 0;
      if (!(flags & (FIO_SOCK_TCP | FIO_SOCK_UDP))) {
        /* TODO? prefer...? TCP? */
        if (u.scheme.len == 3 && (u.scheme.buf[0] | 32) == 'u' &&
            (u.scheme.buf[1] | 32) == 'd' && (u.scheme.buf[2] | 32) == 'p')
          flags |= FIO_SOCK_UDP;
        else if (u.scheme.len == 3 && (u.scheme.buf[0] | 32) == 't' &&
                 (u.scheme.buf[1] | 32) == 'c' && (u.scheme.buf[2] | 32) == 'p')
          flags |= FIO_SOCK_TCP;
        else if ((u.scheme.len == 4 || u.scheme.len == 5) &&
                 (u.scheme.buf[0] | 32) == 'h' &&
                 (u.scheme.buf[1] | 32) == 't' &&
                 (u.scheme.buf[2] | 32) == 't' &&
                 (u.scheme.buf[3] | 32) == 'p' &&
                 (u.scheme.len == 4 ||
                  (u.scheme.len == 5 && (u.scheme.buf[4] | 32) == 's')))
          flags |= FIO_SOCK_TCP;
      }
    }
    if (u.host.len) {
      if (u.host.len >= 2048) {
        errno = EINVAL;
        FIO_LOG_ERROR("Couldn't open socket to %s - host name too long.", url);
        return -1;
      }
      FIO_MEMCPY(buf, u.host.buf, u.host.len);
      buf[u.host.len] = 0;
    } else {
      addr = NULL;
    }
  }
  return fio_sock_open(addr, pr, flags);
}

/** Sets a file descriptor / socket to non blocking state. */
SFUNC int fio_sock_set_non_block(int fd) {
/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
  /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
  int flags;
  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
#ifdef O_CLOEXEC
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_CLOEXEC);
#else
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
#elif defined(FIONBIO)
  /* Otherwise, use the old way of doing it */
#if FIO_OS_WIN
  unsigned long flags = 1;
  if (ioctlsocket(fd, FIONBIO, &flags)) {
    switch (WSAGetLastError()) {
    case WSANOTINITIALISED:
      FIO_LOG_DEBUG("Windows non-blocking ioctl failed with WSANOTINITIALISED");
      break;
    case WSAENETDOWN:
      FIO_LOG_DEBUG("Windows non-blocking ioctl failed with WSAENETDOWN");
      break;
    case WSAEINPROGRESS:
      FIO_LOG_DEBUG("Windows non-blocking ioctl failed with WSAEINPROGRESS");
      break;
    case WSAENOTSOCK:
      FIO_LOG_DEBUG("Windows non-blocking ioctl failed with WSAENOTSOCK");
      break;
    case WSAEFAULT:
      FIO_LOG_DEBUG("Windows non-blocking ioctl failed with WSAEFAULT");
      break;
    }
    return -1;
  }
  return 0;
#else
  int flags = 1;
  return ioctl(fd, FIONBIO, &flags);
#endif /* FIO_OS_WIN */
#else
#error No functions / argumnet macros for non-blocking sockets.
#endif
}

/** Creates a new network socket and binds it to a local address. */
SFUNC int fio_sock_open_local(struct addrinfo *addr, int nonblock) {
  int fd = -1;
  for (struct addrinfo *p = addr; p != NULL; p = p->ai_next) {
#if FIO_OS_WIN
    SOCKET fd_tmp;
    if ((fd_tmp = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        INVALID_SOCKET) {
      FIO_LOG_DEBUG("socket creation error %s", strerror(errno));
      continue;
    }
    if (!FIO_SOCK_FD_ISVALID(fd_tmp)) {
      FIO_LOG_DEBUG("windows socket value out of valid portable range.");
      errno = ERANGE;
    }
    fd = (int)fd_tmp;
#else
    if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      FIO_LOG_DEBUG("socket creation error %s", strerror(errno));
      continue;
    }
#endif
    {
      // avoid the "address taken"
      int optval = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));
    }
    if (bind(fd, p->ai_addr, p->ai_addrlen) == -1) {
      FIO_LOG_DEBUG("Failed attempt to bind socket (%d) to address %s",
                    fd,
                    strerror(errno));
      fio_sock_close(fd);
      fd = -1;
      continue;
    }
    if (nonblock && fio_sock_set_non_block(fd) == -1) {
      FIO_LOG_DEBUG("Couldn't set socket (%d) to non-blocking mode %s",
                    fd,
                    strerror(errno));
      fio_sock_close(fd);
      fd = -1;
      continue;
    }
    break;
  }
  if (fd == -1) {
    FIO_LOG_DEBUG("socket binding/creation error %s", strerror(errno));
  }
  return fd;
}

/** Creates a new network socket and connects it to a remote address. */
SFUNC int fio_sock_open_remote(struct addrinfo *addr, int nonblock) {
  int fd = -1;
  for (struct addrinfo *p = addr; p != NULL; p = p->ai_next) {
#if FIO_OS_WIN
    SOCKET fd_tmp;
    if ((fd_tmp = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        INVALID_SOCKET) {
      FIO_LOG_DEBUG("socket creation error %s", strerror(errno));
      continue;
    }
    if (!FIO_SOCK_FD_ISVALID(fd_tmp)) {
      FIO_LOG_DEBUG("windows socket value out of valid portable range.");
      errno = ERANGE;
    }
    fd = (int)fd_tmp;
#else
    if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      FIO_LOG_DEBUG("socket creation error %s", strerror(errno));
      continue;
    }
#endif

    if (nonblock && fio_sock_set_non_block(fd) == -1) {
      FIO_LOG_DEBUG(
          "Failed attempt to set client socket (%d) to non-blocking %s",
          fd,
          strerror(errno));
      fio_sock_close(fd);
      fd = -1;
      continue;
    }
    if (connect(fd, p->ai_addr, p->ai_addrlen) == -1 &&
#if FIO_OS_WIN
        (WSAGetLastError() != WSAEWOULDBLOCK || errno != EINPROGRESS)
#else
        errno != EINPROGRESS
#endif
    ) {
#if FIO_OS_WIN
      FIO_LOG_DEBUG(
          "Couldn't connect client socket (%d) to remote address %s (%d)",
          fd,
          strerror(errno),
          WSAGetLastError());
#else
      FIO_LOG_DEBUG("Couldn't connect client socket (%d) to remote address %s",
                    fd,
                    strerror(errno));
#endif
      fio_sock_close(fd);
      fd = -1;
      continue;
    }
    break;
  }
  if (fd == -1) {
    FIO_LOG_DEBUG("socket connection/creation error %s", strerror(errno));
  }
  return fd;
}

/** Returns 0 on timeout, -1 on error or the events that are valid. */
SFUNC short fio_sock_wait_io(int fd, short events, int timeout) {
  short r;
  struct pollfd pfd = {.fd = fd, .events = events};
#if FIO_OS_WIN
  r = (short)WSAPoll(&pfd, 1, timeout);
#else
  r = (short)poll(&pfd, 1, timeout);
#endif
  if (r == 1)
    r = events;
  return r;
}

/** Attempts to maximize the allowed open file limits. returns known limit */
SFUNC size_t fio_sock_maximize_limits(void) {
  ssize_t capa = 0;
#if FIO_OS_POSIX

#ifdef _SC_OPEN_MAX
  capa = sysconf(_SC_OPEN_MAX);
#elif defined(FOPEN_MAX)
  capa = FOPEN_MAX;
#endif
  // try to maximize limits - collect max and set to max
  struct rlimit rlim = {.rlim_max = 0};
  if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
    FIO_LOG_WARNING("`getrlimit` failed (%d): %s", errno, strerror(errno));
    return capa;
  }

  FIO_LOG_DEBUG2("existing / maximum open file limit detected: %zd / %zd",
                 (ssize_t)rlim.rlim_cur,
                 (ssize_t)rlim.rlim_max);

  rlim_t original = rlim.rlim_cur;
  rlim.rlim_cur = rlim.rlim_max;
  while (setrlimit(RLIMIT_NOFILE, &rlim) == -1 && rlim.rlim_cur > original)
    rlim.rlim_cur -= 32;

  FIO_LOG_DEBUG2("new open file limit: %zd", (ssize_t)rlim.rlim_cur);

  getrlimit(RLIMIT_NOFILE, &rlim);
  capa = rlim.rlim_cur;
#elif FIO_OS_WIN
  capa = 1ULL << 10;
  while (_setmaxstdio(capa) > 0)
    capa <<= 1;
  capa >>= 1;
  FIO_LOG_DEBUG("new open file limit: %zd", (ssize_t)capa);
#else
  FIO_LOG_ERROR("No OS detected, couldn't maximize open file limit.");
#endif
  return capa;
}

#if FIO_OS_POSIX
/** Creates a new Unix socket and binds it to a local address. */
SFUNC int fio_sock_open_unix(const char *address, int is_client, int nonblock) {
  /* Unix socket */
  struct sockaddr_un addr = {0};
  size_t addr_len = strlen(address);
  if (addr_len >= sizeof(addr.sun_path)) {
    FIO_LOG_ERROR(
        "(fio_sock_open_unix) address too long (%zu bytes > %zu bytes).",
        addr_len,
        sizeof(addr.sun_path) - 1);
    errno = ENAMETOOLONG;
    return -1;
  }
  addr.sun_family = AF_UNIX;
  FIO_MEMCPY(addr.sun_path, address, addr_len + 1); /* copy the NUL byte. */
#if defined(__APPLE__)
  addr.sun_len = addr_len;
#endif
  // get the file descriptor
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    FIO_LOG_DEBUG("couldn't open unix socket (client? == %d) %s",
                  is_client,
                  strerror(errno));
    return -1;
  }
  /* chmod for foreign connections */
  fchmod(fd, S_IRWXO | S_IRWXG | S_IRWXU);
  if (nonblock && fio_sock_set_non_block(fd) == -1) {
    FIO_LOG_DEBUG("couldn't set socket to nonblocking mode");
    fio_sock_close(fd);
    return -1;
  }
  if (is_client) {
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 &&
        errno != EINPROGRESS) {
      FIO_LOG_DEBUG("couldn't connect unix client: %s", strerror(errno));
      fio_sock_close(fd);
      return -1;
    }
  } else {
    unlink(addr.sun_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      FIO_LOG_DEBUG("couldn't bind unix socket to %s", address);
      // umask(old_umask);
      fio_sock_close(fd);
      return -1;
    }
    // umask(old_umask);
    if (listen(fd, SOMAXCONN) < 0) {
      FIO_LOG_DEBUG("couldn't start listening to unix socket at %s", address);
      fio_sock_close(fd);
      return -1;
    }
  }
  return fd;
}
#elif FIO_OS_WIN

/* UNIX Sockets?
 * https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/
 */

static WSADATA fio___sock_useless_windows_data;
FIO_CONSTRUCTOR(fio___sock_win_init) {
  static uint8_t flag = 0;
  if (!flag) {
    flag |= 1;
    if (WSAStartup(MAKEWORD(2, 2), &fio___sock_useless_windows_data)) {
      FIO_LOG_FATAL("WinSock2 unavailable.");
      exit(-1);
    }
    atexit((void (*)(void))(WSACleanup));
  }
}

// FIO_DESTRUCTOR void fio___sock_win_cleanup(void) { (); }
#endif /* FIO_OS_WIN / FIO_OS_POSIX */

/* *****************************************************************************
Socket helper testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void fio___sock_test_before_events(void *udata) {
  *(size_t *)udata = 0;
}
FIO_SFUNC void fio___sock_test_on_event(int fd, size_t index, void *udata) {
  *(size_t *)udata += 1;
  if (errno) {
    FIO_LOG_WARNING("(possibly expected) %s", strerror(errno));
    errno = 0;
  }
  (void)fd;
  (void)index;
}
FIO_SFUNC void fio___sock_test_after_events(void *udata) {
  if (*(size_t *)udata)
    *(size_t *)udata += 1;
}

FIO_SFUNC void FIO_NAME_TEST(stl, sock)(void) {
  fprintf(stderr,
          "* Testing socket helpers (FIO_SOCK) - partial tests only!\n");
#ifdef __cplusplus
  FIO_LOG_WARNING("fio_sock_poll test only runs in C - the FIO_SOCK_POLL_LIST "
                  "macro doesn't work in C++ and writing the test without it "
                  "is a headache.");
#else
  struct {
    const char *address;
    const char *port;
    const char *msg;
    uint16_t flag;
  } server_tests[] = {
    {"127.0.0.1", "9437", "TCP", FIO_SOCK_TCP},
#if FIO_OS_POSIX
#ifdef P_tmpdir
    {P_tmpdir "/tmp_unix_testing_socket_facil_io.sock",
     NULL,
     "Unix",
     FIO_SOCK_UNIX},
#else
    {"./tmp_unix_testing_socket_facil_io.sock", NULL, "Unix", FIO_SOCK_UNIX},
#endif
#endif
    /* accept doesn't work with UDP, not like this... UDP test is seperate */
    // {"127.0.0.1", "9437", "UDP", FIO_SOCK_UDP},
    {.address = NULL},
  };
  for (size_t i = 0; server_tests[i].address; ++i) {
    size_t flag = (size_t)-1;
    errno = 0;
    fprintf(stderr, "* Testing %s socket API\n", server_tests[i].msg);
    int srv = fio_sock_open(server_tests[i].address,
                            server_tests[i].port,
                            server_tests[i].flag | FIO_SOCK_SERVER);
    FIO_ASSERT(srv != -1, "server socket failed to open: %s", strerror(errno));
    flag = (size_t)-1;
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = NULL,
                  .on_error = fio___sock_test_on_event,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag);
    FIO_ASSERT(!flag, "before_events not called for missing list! (%zu)", flag);
    flag = (size_t)-1;
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = NULL,
                  .on_error = fio___sock_test_on_event,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST({.fd = -1}));
    FIO_ASSERT(!flag, "before_events not called for empty list! (%zu)", flag);
    flag = (size_t)-1;
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = NULL,
                  .on_error = fio___sock_test_on_event,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(srv)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);
    flag = (size_t)-1;
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = fio___sock_test_on_event,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(srv)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);
    flag = (size_t)-1;
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = fio___sock_test_on_event,
                  .on_data = NULL,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(srv)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);

    int cl = fio_sock_open(server_tests[i].address,
                           server_tests[i].port,
                           server_tests[i].flag | FIO_SOCK_CLIENT);
    FIO_ASSERT(FIO_SOCK_FD_ISVALID(cl),
               "client socket failed to open (%d)",
               cl);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = NULL,
                  .on_error = fio___sock_test_on_event,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = fio___sock_test_on_event,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);
    // // is it possible to write to a still-connecting socket?
    // fio_sock_poll(.before_events = fio___sock_test_before_events,
    //               .after_events = fio___sock_test_after_events,
    //               .on_ready = fio___sock_test_on_event, .on_data = NULL,
    //               .on_error = NULL, .udata = &flag,
    //               .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    // FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);
    FIO_LOG_INFO("error may print when polling server for `write`.");
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = fio___sock_test_on_event,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .timeout = 100,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(srv)));
    FIO_ASSERT(flag == 2, "Event should have occured here! (%zu)", flag);
    FIO_LOG_INFO("error may have been emitted.");

    intptr_t accepted = accept(srv, NULL, NULL);
    FIO_ASSERT(FIO_SOCK_FD_ISVALID(accepted),
               "accepted socket failed to open (%zd)",
               (ssize_t)accepted);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = fio___sock_test_on_event,
                  .on_data = NULL,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .timeout = 100,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    FIO_ASSERT(flag, "Event should have occured here! (%zu)", flag);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = fio___sock_test_on_event,
                  .on_data = NULL,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .timeout = 100,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(accepted)));
    FIO_ASSERT(flag, "Event should have occured here! (%zu)", flag);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = fio___sock_test_on_event,
                  .on_error = NULL,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    FIO_ASSERT(!flag, "No event should have occured here! (%zu)", flag);

    if (fio_sock_write(accepted, "hello", 5) > 0) {
      // wait for read
      FIO_ASSERT(fio_sock_wait_io(cl, POLLIN, 0) != -1 &&
                     (fio_sock_wait_io(cl, POLLIN, 0) | POLLIN),
                 "fio_sock_wait_io should have returned a POLLIN event.");
      fio_sock_poll(.before_events = fio___sock_test_before_events,
                    .on_ready = NULL,
                    .on_data = fio___sock_test_on_event,
                    .on_error = NULL,
                    .after_events = fio___sock_test_after_events,
                    .udata = &flag,
                    .timeout = 100,
                    .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_R(cl)));
      // test read/write
      fio_sock_poll(.before_events = fio___sock_test_before_events,
                    .on_ready = fio___sock_test_on_event,
                    .on_data = fio___sock_test_on_event,
                    .on_error = NULL,
                    .after_events = fio___sock_test_after_events,
                    .udata = &flag,
                    .timeout = 100,
                    .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
      {
        char buf[64];
        errno = 0;
        FIO_ASSERT(fio_sock_read(cl, buf, 64) > 0,
                   "Read should have read some data...\n\t"
                   "error: %s",
                   strerror(errno));
      }
      FIO_ASSERT(flag == 3, "Event should have occured here! (%zu)", flag);
    } else
      FIO_ASSERT(0,
                 "send(fd:%ld) failed! error: %s",
                 accepted,
                 strerror(errno));
    fio_sock_close(accepted);
    fio_sock_close(cl);
    fio_sock_close(srv);
    fio_sock_poll(.before_events = fio___sock_test_before_events,
                  .on_ready = NULL,
                  .on_data = NULL,
                  .on_error = fio___sock_test_on_event,
                  .after_events = fio___sock_test_after_events,
                  .udata = &flag,
                  .fds = FIO_SOCK_POLL_LIST(FIO_SOCK_POLL_RW(cl)));
    FIO_ASSERT(flag, "Event should have occured here! (%zu)", flag);
#if FIO_OS_POSIX
    if (FIO_SOCK_UNIX == server_tests[i].flag)
      unlink(server_tests[i].address);
#endif
  }
  {
    /* UDP semi test */
    fprintf(stderr, "* Testing UDP socket (abbreviated test)\n");
    int srv =
        fio_sock_open("127.0.0.1", "9437", FIO_SOCK_UDP | FIO_SOCK_SERVER);
    int n = 0; /* try for 32Mb */
    socklen_t sn = sizeof(n);
    if (-1 != getsockopt(srv, SOL_SOCKET, SO_RCVBUF, (void *)&n, &sn) &&
        sizeof(n) == sn)
      fprintf(stderr, "\t- UDP default receive buffer is %d bytes\n", n);
    n = 32 * 1024 * 1024; /* try for 32Mb */
    sn = sizeof(n);
    while (setsockopt(srv, SOL_SOCKET, SO_RCVBUF, (void *)&n, sn) == -1) {
      /* failed - repeat attempt at 0.5Mb interval */
      if (n >= (1024 * 1024)) // OS may have returned max value
        n -= 512 * 1024;
      else
        break;
    }
    if (-1 != getsockopt(srv, SOL_SOCKET, SO_RCVBUF, (void *)&n, &sn) &&
        sizeof(n) == sn)
      fprintf(stderr, "\t- UDP receive buffer could be set to %d bytes\n", n);
    FIO_ASSERT(srv != -1,
               "Couldn't open UDP server socket: %s",
               strerror(errno));
    FIO_LOG_INFO("Opening client UDP socket.");
    int cl = fio_sock_open("127.0.0.1", "9437", FIO_SOCK_UDP | FIO_SOCK_CLIENT);
    FIO_ASSERT(cl != -1,
               "Couldn't open UDP client socket: %s",
               strerror(errno));
    FIO_LOG_INFO("Starting UDP roundtrip.");
    FIO_ASSERT(fio_sock_write(cl, "hello", 5) != -1,
               "couldn't send datagram from client");
    char buf[64];
    FIO_LOG_INFO("Receiving UDP msg.");
    FIO_ASSERT(recvfrom(srv, buf, 64, 0, NULL, NULL) != -1,
               "couldn't read datagram");
    FIO_ASSERT(!memcmp(buf, "hello", 5), "transmission error");
    FIO_LOG_INFO("cleaning up UDP sockets.");
    fio_sock_close(srv);
    fio_sock_close(cl);
  }
#endif /* !__cplusplus */
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
FIO_SOCK - cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_SOCK
#endif
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_ATOMIC                  /* Development inclusion - ignore line */
#define FIO_POLL                    /* Development inclusion - ignore line */
#define FIO_POLL_DEV                /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
#ifdef FIO_POLL_DEV                 /* Development inclusion - ignore line */
#include "201 array.h"              /* Development inclusion - ignore line */
#include "210 hashmap.h"            /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */

/* *****************************************************************************




                        POSIX Portable Polling with `poll`




***************************************************************************** */
#if defined(FIO_POLL) && !defined(H___FIO_POLL___H) && !defined(FIO_STL_KEEP__)
#define H___FIO_POLL___H

#ifndef FIO_POLL_HAS_UDATA_COLLECTION
/* A unique `udata` per fd (true)? or a global `udata` (false)?*/
#define FIO_POLL_HAS_UDATA_COLLECTION 1
#endif

#ifndef FIO_POLL_FRAGMENTATION_LIMIT
/**
 * * When the polling array is fragmented by more than the set value, it will be
 * de-fragmented on the idle cycle (if no events occur).
 * */
#define FIO_POLL_FRAGMENTATION_LIMIT 63
#endif
/* *****************************************************************************
Polling API
***************************************************************************** */

/** the `fio_poll_s` type should be considered opaque. */
typedef struct fio_poll_s fio_poll_s;

typedef struct {
  /** callback for when data is availabl in the incoming buffer. */
  void (*on_data)(int fd, void *udata);
  /** callback for when the outgoing buffer allows a call to `write`. */
  void (*on_ready)(int fd, void *udata);
  /** callback for closed connections and / or connections with errors. */
  void (*on_close)(int fd, void *udata);
} fio_poll_settings_s;

#if FIO_USE_THREAD_MUTEX_TMP
#define FIO_POLL_INIT(on_data_func, on_ready_func, on_close_func)              \
  {                                                                            \
    .settings =                                                                \
        {                                                                      \
            .on_data = on_data_func,                                           \
            .on_ready = on_ready_func,                                         \
            .on_close = on_close_func,                                         \
        },                                                                     \
    .lock = (fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT                          \
  }
#else
#define FIO_POLL_INIT(on_data_func, on_ready_func, on_close_func)              \
  {                                                                            \
    .settings =                                                                \
        {                                                                      \
            .on_data = on_data_func,                                           \
            .on_ready = on_ready_func,                                         \
            .on_close = on_close_func,                                         \
        },                                                                     \
    .lock = FIO_LOCK_INIT                                                      \
  }
#endif

#ifndef FIO_REF_CONSTRUCTOR_ONLY
/** Creates a new polling object / queue. */
FIO_IFUNC fio_poll_s *fio_poll_new(fio_poll_settings_s settings);
#define fio_poll_new(...) fio_poll_new((fio_poll_settings_s){__VA_ARGS__})

/** Frees the polling object and its resources. */
FIO_IFUNC int fio_poll_free(fio_poll_s *p);
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Destroys the polling object, freeing its resources. */
FIO_IFUNC void fio_poll_destroy(fio_poll_s *p);

/**
 * Adds a file descriptor to be monitored, adds events to be monitored or
 * updates the monitored file's `udata`.
 *
 * Possible flags are: `POLLIN` and `POLLOUT`. Other flags may be set but might
 * be ignored.
 *
 * Monitoring mode is always one-shot. If an event if fired, it is removed from
 * the monitoring state.
 *
 * Returns -1 on error.
 */
SFUNC int fio_poll_monitor(fio_poll_s *p,
                           int fd,
                           void *udata,
                           unsigned short flags);

/**
 * Reviews if any of the monitored file descriptors has any events.
 *
 * `timeout` is in milliseconds.
 *
 * Returns the number of events called.
 *
 * Polling is thread safe, but has different effects on different threads.
 *
 * Adding a new file descriptor from one thread while polling in a different
 * thread will not poll that IO untill `fio_poll_review` is called again.
 */
SFUNC int fio_poll_review(fio_poll_s *p, int timeout);

/**
 * Stops monitoring the specified file descriptor, returning its udata (if any).
 */
SFUNC void *fio_poll_forget(fio_poll_s *p, int fd);

/** Closes all sockets, calling the `on_close`. */
SFUNC void fio_poll_close_all(fio_poll_s *p);

/* *****************************************************************************



                          Poll Monitoring Implementation



***************************************************************************** */

/* *****************************************************************************
Poll Monitoring Implementation - The polling type(s)
***************************************************************************** */
#define FIO_STL_KEEP__

#define FIO_RISKY_HASH
#define FIO_MAP_TYPE         int32_t
#define FIO_MAP_HASH         uint32_t
#define FIO_MAP_TYPE_INVALID (-1) /* allow monitoring of fd == 0*/
#define FIO_UMAP_NAME        fio___poll_index
#include __FILE__
#define FIO_ARRAY_TYPE           struct pollfd
#define FIO_ARRAY_NAME           fio___poll_fds
#define FIO_ARRAY_TYPE_CMP(a, b) (a.fd == b.fd)
#include __FILE__
#if FIO_POLL_HAS_UDATA_COLLECTION
#define FIO_ARRAY_TYPE void *
#define FIO_ARRAY_NAME fio___poll_udata
#include __FILE__
#endif /* FIO_POLL_HAS_UDATA_COLLECTION */

#ifdef FIO_STL_KEEP__
#undef FIO_STL_KEEP__
#endif

struct fio_poll_s {
  fio_poll_settings_s settings;
  fio___poll_index_s index;
  fio___poll_fds_s fds;
#if FIO_POLL_HAS_UDATA_COLLECTION
  fio___poll_udata_s udata;
#else
  void *udata;
#endif /* FIO_POLL_HAS_UDATA_COLLECTION */
  FIO___LOCK_TYPE lock;
  size_t forgotten;
};

/* *****************************************************************************
When avoiding the `udata` array
***************************************************************************** */
#if !FIO_POLL_HAS_UDATA_COLLECTION
FIO_IFUNC void fio___poll_udata_destroy(void **pu) { *pu = NULL; }
FIO_IFUNC void **fio___poll_udata_push(void **pu, void *udata) {
  if (udata)
    *pu = udata;
  return pu;
}
FIO_IFUNC void **fio___poll_udata_pop(void **pu, void *ignr) {
  return pu;
  (void)ignr;
}
FIO_IFUNC void **fio___poll_udata_set(void **pu,
                                      int32_t pos,
                                      void *udata,
                                      void **ignr) {
  if (udata)
    *pu = udata;
  return pu;
  (void)ignr;
  (void)pos;
}
FIO_IFUNC void **fio___poll_udata2ptr(void **pu) { return pu; }
FIO_IFUNC void *fio___poll_udata_get(void **pu, int32_t pos) {
  return *pu;
  (void)pos;
}
#endif /* FIO_POLL_HAS_UDATA_COLLECTION */
/* *****************************************************************************
Poll Monitoring Implementation - inline static functions
***************************************************************************** */

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC fio_poll_s *fio_poll_new FIO_NOOP(fio_poll_settings_s settings) {
  fio_poll_s *p = (fio_poll_s *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*p), 0);
  if (p) {
    *p = (fio_poll_s) {
      .settings = settings, .index = FIO_MAP_INIT, .fds = FIO_ARRAY_INIT,
#if FIO_POLL_HAS_UDATA_COLLECTION
      .udata = FIO_ARRAY_INIT,
#endif
      .lock = FIO___LOCK_INIT, .forgotten = 0,
    };
  }
  return p;
}
/* Frees any internal data AND the object's container! */
FIO_IFUNC int fio_poll_free(fio_poll_s *p) {
  fio_poll_destroy(p);
  FIO_MEM_FREE_(p, sizeof(*p));
  return 0;
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Destroys the polling object, freeing its resources. */
FIO_IFUNC void fio_poll_destroy(fio_poll_s *p) {
  if (p) {
    fio___poll_index_destroy(&p->index);
    fio___poll_fds_destroy(&p->fds);
    fio___poll_udata_destroy(&p->udata);
    FIO___LOCK_DESTROY(p->lock);
  }
}

/* *****************************************************************************
Poll Monitoring Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

#ifdef FIO_POLL_DEBUG
#define FIO_POLL_DEBUG_LOG FIO_LOG_DEBUG
#else
#define FIO_POLL_DEBUG_LOG(...)
#endif

/* mock event */
FIO_SFUNC void fio___poll_ev_mock(int fd, void *udata) {
  (void)fd;
  (void)udata;
}

FIO_IFUNC int fio___poll_monitor(fio_poll_s *p,
                                 int fd,
                                 void *udata,
                                 unsigned short flags) {
  if (fd == -1)
    return -1;
  int32_t pos = fio___poll_index_get(&p->index, fd, 0);
  struct pollfd *i = fio___poll_fds2ptr(&p->fds);
  if (i && pos != -1 && (int)i[pos].fd == fd)
    goto edit_existing;
  if (i && pos != -1 && (int)i[pos].fd == -1)
    goto renew_monitoring;
  /* insert new entry */
  i = fio___poll_fds_push(&p->fds,
                          (struct pollfd){.fd = fd, .events = (short)flags});
  if (!i) {
    FIO_LOG_ERROR("fio___poll_monitor failed to push fd %d", fd);
    return -1;
  }
  pos = (uint32_t)(i - fio___poll_fds2ptr(&p->fds));
  if (!fio___poll_udata_push(&p->udata, udata)) {
    FIO_LOG_ERROR("fio___poll_monitor failed to push udata for fd %d", fd);
    fio___poll_fds_pop(&p->fds, NULL);
    return -1;
  }
  fio___poll_index_set(&p->index, fd, pos, NULL);
  return 0;

edit_existing:
  /* pos is correct, we are updating a value */
  i[pos].events |= flags;
  if (udata)
    fio___poll_udata_set(&p->udata, (int32_t)pos, udata, NULL);
  return 0;

renew_monitoring:
  i[pos].fd = fd;
  i[pos].events = flags;
  i[pos].revents = 0;
  fio___poll_udata_set(&p->udata, (int32_t)pos, udata, NULL);
  return 0;
}

/**
 * Adds a file descriptor to be monitored, adds events to be monitored or
 * updates the monitored file's `udata`.
 *
 * Possible flags are: `POLLIN` and `POLLOUT`. Other flags may be set but might
 * be ignored.
 *
 * Returns -1 on error.
 */
SFUNC int fio_poll_monitor(fio_poll_s *p,
                           int fd,
                           void *udata,
                           unsigned short flags) {
  int r = -1;
  if (!p || fd == -1)
    return r;
  flags &= POLLIN | POLLOUT | POLLPRI;
#ifdef POLLRDHUP
  flags |= POLLRDHUP;
#endif
  FIO___LOCK_LOCK(p->lock);
  r = fio___poll_monitor(p, fd, udata, flags);
  FIO___LOCK_UNLOCK(p->lock);
  return r;
}

/**
 * Reviews if any of the monitored file descriptors has any events.
 *
 * Returns the number of events called.
 *
 * Polling is thread safe, but has different effects on different threads.
 *
 * Adding a new file descriptor from one thread while polling in a different
 * thread will not poll that IO until `fio_poll_review` is called again.
 */
SFUNC int fio_poll_review(fio_poll_s *p, int timeout) {
  int r = -1;
  int to_copy = 0;
  fio_poll_s cpy;
  if (!p)
    return r;

  /* move all data to a copy (thread safety) */
  FIO___LOCK_LOCK(p->lock);
  cpy = *p;
  p->index = (fio___poll_index_s)FIO_MAP_INIT;
  p->fds = (fio___poll_fds_s)FIO_ARRAY_INIT;
#if FIO_POLL_HAS_UDATA_COLLECTION
  p->udata = (fio___poll_udata_s)FIO_ARRAY_INIT;
#endif /* FIO_POLL_HAS_UDATA_COLLECTION */

  FIO___LOCK_UNLOCK(p->lock);

  /* move if conditions out of the loop */
  if (!cpy.settings.on_data)
    cpy.settings.on_data = fio___poll_ev_mock;
  if (!cpy.settings.on_ready)
    cpy.settings.on_ready = fio___poll_ev_mock;
  if (!cpy.settings.on_close)
    cpy.settings.on_close = fio___poll_ev_mock;

  /* poll the array */
  struct pollfd *const fds_ary = fio___poll_fds2ptr(&cpy.fds);
  int const len = (int)fio___poll_fds_count(&cpy.fds);
  void **const ud_ary = fio___poll_udata2ptr(&cpy.udata);
  FIO_POLL_DEBUG_LOG("fio_poll_review reviewing %zu file descriptors.", len);
#if FIO_POLL_HAS_UDATA_COLLECTION
#define FIO___POLL_UDATA_GET(index) ud_ary[(index)]
#else
#define FIO___POLL_UDATA_GET(index) ud_ary[0]
#endif
#if FIO_OS_WIN
  r = WSAPoll(fds_ary, len, timeout);
#else
  r = poll(fds_ary, len, timeout);
#endif

  /* process events */
  if (r > 0) {
    int i = 0; /* index in fds_ary array. */
    int c = 0; /* count events handled, to stop loop if no more events. */
    do {
      if ((int)fds_ary[i].fd != -1) {
        if ((fds_ary[i].revents & (POLLIN /* | POLLPRI */))) {
          cpy.settings.on_data(fds_ary[i].fd, FIO___POLL_UDATA_GET(i));
          FIO_POLL_DEBUG_LOG("fio_poll_review calling `on_data` for %d.",
                             fds_ary[i].fd);
        }
        if ((fds_ary[i].revents & POLLOUT)) {
          cpy.settings.on_ready(fds_ary[i].fd, FIO___POLL_UDATA_GET(i));
          FIO_POLL_DEBUG_LOG("fio_poll_review calling `on_ready` for %d.",
                             fds_ary[i].fd);
        }
        if ((fds_ary[i].revents & (POLLHUP | POLLERR | POLLNVAL))) {
          cpy.settings.on_close(fds_ary[i].fd, FIO___POLL_UDATA_GET(i));
          FIO_POLL_DEBUG_LOG("fio_poll_review calling `on_close` for %d.",
                             fds_ary[i].fd);
          /* handle possible re-insertion after events */
          fio_poll_forget(p, fds_ary[i].fd);
          /* never retain event monitoring after closure / error */
          fds_ary[i].events = 0;
        }
        /* did we perform any events? */
        c += !!fds_ary[i].revents;
        /* any unfired events for the fd? */
        fds_ary[i].events &= ~(fds_ary[i].revents);
#ifdef POLLRDHUP
        fds_ary[i].events &= ~POLLRDHUP;
#endif
        if (fds_ary[i].events) {
          /* unfired events await */
          fds_ary[to_copy].fd = fds_ary[i].fd;
          fds_ary[to_copy].events = fds_ary[i].events;
#ifdef POLLRDHUP
          fds_ary[to_copy].events |= POLLRDHUP;
#endif
          fds_ary[to_copy].revents = 0;
          FIO___POLL_UDATA_GET(to_copy) = FIO___POLL_UDATA_GET(i);
          ++to_copy;
          FIO_POLL_DEBUG_LOG("fio_poll_review %d still has pending events",
                             fds_ary[i].fd);
        } else {
          FIO_POLL_DEBUG_LOG("fio_poll_review no more events for %d",
                             fds_ary[i].fd);
        }
      }
      ++i;
      if (i < len && c < r)
        continue;
      if (to_copy != i) {
        /* copy remaining events in incomplete loop, if any. */
        while (i < len) {
          FIO_POLL_DEBUG_LOG("fio_poll_review %d no-events-left mark copy",
                             fds_ary[i].fd);
          FIO_ASSERT_DEBUG(!fds_ary[i].revents,
                           "Event unhandled for %d",
                           fds_ary[i].fd);
          fds_ary[to_copy].fd = fds_ary[i].fd;
          fds_ary[to_copy].events = fds_ary[i].events;
          fds_ary[to_copy].revents = 0;
          FIO___POLL_UDATA_GET(to_copy) = FIO___POLL_UDATA_GET(i);
          ++to_copy;
          ++i;
        }
      } else {
        if (to_copy != len) {
          FIO_POLL_DEBUG_LOG("fio_poll_review no events left, quick mark");
        }
        to_copy = len;
      }
      break;
    } while (1);
  } else {
    to_copy = len;
  }

  /* insert all un-fired events back to the (thread safe) queue */
  FIO___LOCK_LOCK(p->lock);
  if ((FIO_POLL_FRAGMENTATION_LIMIT > 0) && !r &&
      (p->forgotten >= (FIO_POLL_FRAGMENTATION_LIMIT))) {
    /* de-fragment list */
    FIO_POLL_DEBUG_LOG("fio_poll_review de-fragmentation cycle");
    fio_poll_s cpy2;
    cpy2 = *p;
    p->forgotten = 0;
    p->index = (fio___poll_index_s)FIO_MAP_INIT;
    p->fds = (fio___poll_fds_s)FIO_ARRAY_INIT;
#if FIO_POLL_HAS_UDATA_COLLECTION
    p->udata = (fio___poll_udata_s)FIO_ARRAY_INIT;
#endif /* FIO_POLL_HAS_UDATA_COLLECTION */

    for (size_t i = 0; i < fio___poll_fds_count(&cpy2.fds); ++i) {
      if ((int)fio___poll_fds_get(&cpy2.fds, i).fd == -1 ||
          !fio___poll_fds_get(&cpy2.fds, i).events)
        continue;
      fio___poll_monitor(p,
                         fio___poll_fds_get(&cpy2.fds, i).fd,
                         fio___poll_udata_get(&cpy2.udata, i),
                         fio___poll_fds_get(&cpy2.fds, i).events);
    }

    fio___poll_fds_destroy(&cpy2.fds);
    fio___poll_udata_destroy(&cpy2.udata);
    fio___poll_index_destroy(&cpy2.index);
    to_copy = 0;
    for (int i = 0; i < len; ++i) {
      if ((int)fds_ary[i].fd == -1 || !fds_ary[i].events)
        continue;
      ++to_copy;
      fio___poll_monitor(p,
                         fds_ary[i].fd,
                         FIO___POLL_UDATA_GET(i),
                         fds_ary[i].events);
    }
    FIO_POLL_DEBUG_LOG(
        "fio_poll_review resubmitted %zu items for pending events",
        to_copy);

  } else if (to_copy == len && !fio___poll_index_count(&p->index)) {
    /* it's possible to move the data set as is */
    FIO_POLL_DEBUG_LOG(
        "fio_poll_review overwriting %zu items for pending events",
        to_copy);
    *p = cpy;
    cpy = (fio_poll_s)FIO_POLL_INIT(NULL, NULL, NULL);
  } else {
    FIO_POLL_DEBUG_LOG("fio_poll_review copying %zu items with pending events",
                       to_copy);
    for (int i = 0; i < to_copy; ++i) {
      fio___poll_monitor(p,
                         fds_ary[i].fd,
                         FIO___POLL_UDATA_GET(i),
                         fds_ary[i].events);
    }
  }
  FIO___LOCK_UNLOCK(p->lock);

  /* cleanup memory */
  fio___poll_index_destroy(&cpy.index);
  fio___poll_fds_destroy(&cpy.fds);
  fio___poll_udata_destroy(&cpy.udata);
  return r;
#undef FIO___POLL_UDATA_GET
}

/**
 * Stops monitoring the specified file descriptor, returning its udata (if any).
 */
SFUNC void *fio_poll_forget(fio_poll_s *p, int fd) {
  void *old = NULL;
  FIO_POLL_DEBUG_LOG("fio_poll_forget called for %d", fd);
  if (!p || fd == -1 || !fio___poll_fds_count(&p->fds))
    return old;
  FIO___LOCK_LOCK(p->lock);
  int32_t pos = fio___poll_index_get(&p->index, fd, 0);
  if (pos != -1 && (int)fio___poll_fds_get(&p->fds, pos).fd == fd) {
    FIO_POLL_DEBUG_LOG("fio_poll_forget evicting %d at position [%d]",
                       fd,
                       (int)pos);
    fio___poll_index_remove(&p->index, fd, 0, NULL);
    old = fio___poll_udata_get(&p->udata, (int32_t)pos);
    fio___poll_fds2ptr(&p->fds)[(int32_t)pos].fd = -1;
    fio___poll_fds2ptr(&p->fds)[(int32_t)pos].events = 0;
    fio___poll_udata_set(&p->udata, (int32_t)pos, NULL, NULL);
    ++p->forgotten;
    while (p->forgotten && (pos = fio___poll_fds_count(&p->fds) - 1) >= 0 &&
           ((int)fio___poll_fds_get(&p->fds, pos).fd == -1 ||
            !fio___poll_fds_get(&p->fds, pos).events)) {
      fio___poll_fds_pop(&p->fds, NULL);
      fio___poll_udata_pop(&p->udata, NULL);
      --p->forgotten;
    }
  }
  FIO___LOCK_UNLOCK(p->lock);
  return old;
}

/** Closes all sockets, calling the `on_close`. */
SFUNC void fio_poll_close_all(fio_poll_s *p) {
  fio_poll_s tmp;
  FIO___LOCK_LOCK(p->lock);
  tmp = *p;
  *p = (fio_poll_s)FIO_POLL_INIT(p->settings.on_data,
                                 p->settings.on_ready,
                                 p->settings.on_close);
  p->lock = tmp.lock;
  FIO___LOCK_UNLOCK(tmp.lock);
  for (size_t i = 0; i < fio___poll_fds_count(&tmp.fds); ++i) {
    if ((int)fio___poll_fds_get(&tmp.fds, i).fd == -1)
      continue;
    close(fio___poll_fds_get(&tmp.fds, i).fd);
#if FIO_POLL_HAS_UDATA_COLLECTION
    tmp.settings.on_close(fio___poll_fds_get(&tmp.fds, i).fd,
                          fio___poll_udata2ptr(&tmp.udata)[i]);
#else
    tmp.settings.on_close(fio___poll_fds_get(&tmp.fds, i).fd,
                          fio___poll_udata2ptr(&tmp.udata)[0]);
#endif
  }
  fio___poll_index_destroy(&tmp.index);
  fio___poll_fds_destroy(&tmp.fds);
  fio___poll_udata_destroy(&tmp.udata);
}

/* *****************************************************************************
Poll Monitoring Testing?
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, poll)(void) {
  fprintf(
      stderr,
      "* testing file descriptor monitoring (poll setup / cleanup only).\n");
  fio_poll_s p = FIO_POLL_INIT(NULL, NULL, NULL);
#ifdef POLLRDHUP
  /* if defined, the event is automatically monitored, so test for it. */
  short events[4] = {
      POLLRDHUP | POLLOUT,
      POLLRDHUP | POLLIN,
      POLLRDHUP | POLLOUT | POLLIN,
      POLLRDHUP | POLLOUT | POLLIN,
  };
#else
  short events[4] = {POLLOUT, POLLIN, POLLOUT | POLLIN, POLLOUT | POLLIN};
#endif
  for (int i = 128; i--;) {
    FIO_ASSERT(!fio_poll_monitor(&p, i, (void *)(uintptr_t)i, events[(i & 3)]),
               "fio_poll_monitor failed for fd %d",
               i);
  }
  for (int i = 128; i--;) {
    if ((i & 3) == 3) {
      FIO_ASSERT(fio_poll_forget(&p, i) == (void *)(uintptr_t)i,
                 "fio_poll_forget didn't return correct udata at %d",
                 i);
    }
  }
  for (int i = 128; i--;) {
    size_t pos = fio___poll_index_get(&p.index, i, 0);
    if ((i & 3) == 3) {
      FIO_ASSERT((int)fio___poll_fds_get(&p.fds, pos).fd != i,
                 "fd wasn't removed?");
      FIO_ASSERT((int)(uintptr_t)fio___poll_udata_get(&p.udata, pos) != i,
                 "udata value wasn't removed?");
      continue;
    }
    FIO_ASSERT((int)fio___poll_fds_get(&p.fds, pos).fd == i,
               "index value [%zu] doesn't match fd (%d != %d)",
               pos,
               fio___poll_fds_get(&p.fds, pos).fd,
               i);
    FIO_ASSERT(fio___poll_fds_get(&p.fds, pos).events == events[(i & 3)],
               "events value isn't setup correctly");
    FIO_ASSERT((int)(uintptr_t)fio___poll_udata_get(&p.udata, pos) == i,
               "udata value isn't setup correctly");
  }
  fio_poll_destroy(&p);
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_POLL_HAS_UDATA_COLLECTION
#undef FIO_POLL
#endif /* FIO_POLL */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_STREAM                  /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




      A packet based data stream for storing / buffering endless data.




***************************************************************************** */
#if defined(FIO_STREAM) && !defined(H___FIO_STREAM___H)
#define H___FIO_STREAM___H

#if !FIO_HAVE_UNIX_TOOLS
#if _MSC_VER
#pragma message(                                                               \
    "POSIX is required for the fio_stream API, or issues may occure.")
#else
#warning "POSIX behavior is expected by the fio_stream API."
#endif
#endif
#include <sys/stat.h>

#ifndef FIO_STREAM_COPY_PER_PACKET
/** Break apart large memory blocks into smaller pieces. by default 96Kb */
#define FIO_STREAM_COPY_PER_PACKET 98304
#endif

/* *****************************************************************************
Stream API - types, constructor / destructor
***************************************************************************** */

typedef struct fio_stream_packet_s fio_stream_packet_s;

typedef struct {
  /* do not directly acecss! */
  fio_stream_packet_s *next;
  fio_stream_packet_s **pos;
  uint32_t consumed;
  uint32_t packets;
} fio_stream_s;

/* at this point publish (declare only) the public API */

#ifndef FIO_STREAM_INIT
/* Initialization macro. */
#define FIO_STREAM_INIT(s)                                                     \
  { .next = NULL, .pos = &(s).next }
#endif

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY

/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC fio_stream_s *fio_stream_new(void);

/* Frees any internal data AND the object's container! */
FIO_IFUNC int fio_stream_free(fio_stream_s *stream);

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Destroys the object, re-initializing its container. */
SFUNC void fio_stream_destroy(fio_stream_s *stream);

/* *****************************************************************************
Stream API - packing data into packets and adding it to the stream
***************************************************************************** */

/** Packs data into a fio_stream_packet_s container. */
SFUNC fio_stream_packet_s *fio_stream_pack_data(void *buf,
                                                size_t len,
                                                size_t offset,
                                                uint8_t copy_buffer,
                                                void (*dealloc_func)(void *));

/** Packs a file descriptor into a fio_stream_packet_s container. */
SFUNC fio_stream_packet_s *fio_stream_pack_fd(int fd,
                                              size_t len,
                                              size_t offset,
                                              uint8_t keep_open);

/** Adds a packet to the stream. This isn't thread safe.*/
SFUNC void fio_stream_add(fio_stream_s *stream, fio_stream_packet_s *packet);

/** Destroys the fio_stream_packet_s - call this ONLY if unused. */
SFUNC void fio_stream_pack_free(fio_stream_packet_s *packet);

/* *****************************************************************************
Stream API - Consuming the stream
***************************************************************************** */

/**
 * Reads data from the stream (if any), leaving it in the stream.
 *
 * `buf` MUST point to a buffer with - at least - `len` bytes. This is required
 * in case the packed data is fragmented or references a file and needs to be
 * copied to an available buffer.
 *
 * On error, or if the stream is empty, `buf` will be set to NULL and `len` will
 * be set to zero.
 *
 * Otherwise, `buf` may retain the same value or it may point directly to a
 * memory address within the stream's buffer (the original value may be lost)
 * and `len` will be updated to the largest possible value for valid data that
 * can be read from `buf`.
 *
 * Note: this isn't thread safe.
 */
SFUNC void fio_stream_read(fio_stream_s *stream, char **buf, size_t *len);

/**
 * Advances the Stream, so the first `len` bytes are marked as consumed.
 *
 * Note: this isn't thread safe.
 */
SFUNC void fio_stream_advance(fio_stream_s *stream, size_t len);

/**
 * Returns true if there's any data in the stream.
 *
 * Note: this isn't truly thread safe.
 */
FIO_IFUNC uint8_t fio_stream_any(fio_stream_s *stream);

/**
 * Returns the number of packets waiting in the stream.
 *
 * Note: this isn't truly thread safe.
 */
FIO_IFUNC uint32_t fio_stream_packets(fio_stream_s *stream);

/* *****************************************************************************








                          Stream Implementation








***************************************************************************** */

/* *****************************************************************************
Stream Implementation - inlined static functions
***************************************************************************** */

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC fio_stream_s *fio_stream_new(void) {
  fio_stream_s *s = (fio_stream_s *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*s), 0);
  if (s) {
    *s = (fio_stream_s)FIO_STREAM_INIT(s[0]);
  }
  return s;
}
/* Frees any internal data AND the object's container! */
FIO_IFUNC int fio_stream_free(fio_stream_s *s) {
  fio_stream_destroy(s);
  FIO_MEM_FREE_(s, sizeof(*s));
  return 0;
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/* Returns true if there's any data in the stream */
FIO_IFUNC uint8_t fio_stream_any(fio_stream_s *s) { return s && !!s->next; }

/* Returns the number of packets waiting in the stream */
FIO_IFUNC uint32_t fio_stream_packets(fio_stream_s *s) { return s->packets; }

/* *****************************************************************************
Stream Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

FIO_IFUNC void fio_stream_packet_free_all(fio_stream_packet_s *p);
/* Frees any internal data AND the object's container! */
SFUNC void fio_stream_destroy(fio_stream_s *s) {
  if (!s)
    return;
  fio_stream_packet_free_all(s->next);
  *s = (fio_stream_s)FIO_STREAM_INIT(s[0]);
  return;
}

/* *****************************************************************************
Stream API - packing data into packets and adding it to the stream
***************************************************************************** */

struct fio_stream_packet_s {
  fio_stream_packet_s *next;
};

typedef enum {
  FIO_PACKET_TYPE_EMBEDDED = 0,
  FIO_PACKET_TYPE_EXTERNAL = 1,
  FIO_PACKET_TYPE_FILE = 2,
  FIO_PACKET_TYPE_FILE_NO_CLOSE = 3,
} fio_stream_packet_type_e;

typedef struct fio_stream_packet_embd_s {
  uint32_t type;
  char buf[];
} fio_stream_packet_embd_s;
#define FIO_STREAM___EMBD_BIT_OFFSET 4

typedef struct fio_stream_packet_extrn_s {
  uint32_t type;
  uint32_t length;
  char *buf;
  uintptr_t offset;
  void (*dealloc)(void *buf);
} fio_stream_packet_extrn_s;

/** User-space socket buffer data */
typedef struct {
  uint32_t type;
  uint32_t length;
  int32_t offset;
  int fd;
} fio_stream_packet_fd_s;

FIO_SFUNC void fio_stream_packet_free(fio_stream_packet_s *p) {
  if (!p)
    return;
  union {
    fio_stream_packet_embd_s *em;
    fio_stream_packet_extrn_s *ext;
    fio_stream_packet_fd_s *f;
  } const u = {.em = (fio_stream_packet_embd_s *)(p + 1)};
  switch ((fio_stream_packet_type_e)(u.em->type & 3)) {
  case FIO_PACKET_TYPE_EMBEDDED:
    FIO_MEM_FREE_(
        p,
        sizeof(*p) + sizeof(*u.em) +
            (sizeof(char) * (u.em->type >> FIO_STREAM___EMBD_BIT_OFFSET)));
    break;
  case FIO_PACKET_TYPE_EXTERNAL:
    if (u.ext->dealloc)
      u.ext->dealloc(u.ext->buf);
    FIO_MEM_FREE_(p, sizeof(*p) + sizeof(*u.ext));
    break;
  case FIO_PACKET_TYPE_FILE:
    close(u.f->fd);
    /* fallthrough */
  case FIO_PACKET_TYPE_FILE_NO_CLOSE:
    FIO_MEM_FREE_(p, sizeof(*p) + sizeof(*u.f));
    break;
  }
}

FIO_IFUNC void fio_stream_packet_free_all(fio_stream_packet_s *p) {
  while (p) {
    register fio_stream_packet_s *t = p;
    p = p->next;
    fio_stream_packet_free(t);
  }
}

/** Packs data into a fio_stream_packet_s container. */
SFUNC fio_stream_packet_s *fio_stream_pack_data(void *buf,
                                                size_t len,
                                                size_t offset,
                                                uint8_t copy_buffer,
                                                void (*dealloc_func)(void *)) {
  fio_stream_packet_s *p = NULL;
  if (!len || !buf || (len & ((~(0UL)) << (32 - FIO_STREAM___EMBD_BIT_OFFSET))))
    goto error;
  if (copy_buffer || len <= 14) {
    while (len) {
      /* break apart large memory blocks into smaller pieces */
      const size_t slice =
          (len > FIO_STREAM_COPY_PER_PACKET) ? FIO_STREAM_COPY_PER_PACKET : len;
      fio_stream_packet_embd_s *em;
      fio_stream_packet_s *tmp = (fio_stream_packet_s *)FIO_MEM_REALLOC_(
          NULL,
          0,
          sizeof(*p) + sizeof(*em) + (sizeof(char) * slice),
          0);
      if (!tmp)
        goto error;
      tmp->next = p;
      em = (fio_stream_packet_embd_s *)(tmp + 1);
      em->type = (uint32_t)FIO_PACKET_TYPE_EMBEDDED |
                 (uint32_t)(slice << FIO_STREAM___EMBD_BIT_OFFSET);
      FIO_MEMCPY(em->buf, (char *)buf + offset + (len - slice), slice);
      p = tmp;
      len -= slice;
    }
    if (dealloc_func)
      dealloc_func(buf);
  } else {
    fio_stream_packet_extrn_s *ext;
    p = (fio_stream_packet_s *)
        FIO_MEM_REALLOC_(NULL, 0, sizeof(*p) + sizeof(*ext), 0);
    if (!p)
      goto error;
    p->next = NULL;
    ext = (fio_stream_packet_extrn_s *)(p + 1);
    *ext = (fio_stream_packet_extrn_s){
        .type = FIO_PACKET_TYPE_EXTERNAL,
        .length = (uint32_t)len,
        .buf = (char *)buf,
        .offset = offset,
        .dealloc = dealloc_func,
    };
  }
  return p;

error:
  if (dealloc_func)
    dealloc_func(buf);
  fio_stream_packet_free_all(p);
  return p;
}

/** Packs a file descriptor into a fio_stream_packet_s container. */
SFUNC fio_stream_packet_s *fio_stream_pack_fd(int fd,
                                              size_t len,
                                              size_t offset,
                                              uint8_t keep_open) {
  fio_stream_packet_s *p = NULL;
  fio_stream_packet_fd_s *f;
  if (fd < 0)
    goto no_file;

  if (!len) {
    /* review file total length and auto-calculate */
    struct stat st;
    if (fstat(fd, &st))
      goto error;
    if (st.st_size <= 0 || offset >= (size_t)st.st_size ||
        (uint64_t)st.st_size >= ((uint64_t)1UL << 32))
      goto error;
    len = (size_t)st.st_size - offset;
  }

  p = (fio_stream_packet_s *)
      FIO_MEM_REALLOC_(NULL, 0, sizeof(*p) + sizeof(*f), 0);
  if (!p)
    goto error;
  p->next = NULL;
  f = (fio_stream_packet_fd_s *)(p + 1);
  *f = (fio_stream_packet_fd_s){
      .type =
          (keep_open ? FIO_PACKET_TYPE_FILE : FIO_PACKET_TYPE_FILE_NO_CLOSE),
      .length = (uint32_t)len,
      .offset = (int32_t)offset,
      .fd = fd,
  };
  return p;
error:
  if (!keep_open)
    close(fd);
no_file:
  return p;
}

/** Adds a packet to the stream. This isn't thread safe.*/
SFUNC void fio_stream_add(fio_stream_s *s, fio_stream_packet_s *p) {
  fio_stream_packet_s *last = p;
  uint32_t packets = 1;
  if (!s || !p)
    goto error;
  while (last->next) {
    last = last->next;
    ++packets;
  }
  if (!s->pos)
    s->pos = &s->next;
  *s->pos = p;
  s->pos = &last->next;
  s->packets += packets;
  return;
error:
  fio_stream_pack_free(p);
}

/** Destroys the fio_stream_packet_s - call this ONLY if unused. */
SFUNC void fio_stream_pack_free(fio_stream_packet_s *p) {
  fio_stream_packet_free_all(p);
}

/* *****************************************************************************
Stream API - Consuming the stream
***************************************************************************** */

FIO_IFUNC size_t fio___stream_p2len(fio_stream_packet_s *p) {
  size_t len = 0;
  if (!p)
    return len;
  union {
    fio_stream_packet_embd_s *em;
    fio_stream_packet_extrn_s *ext;
    fio_stream_packet_fd_s *f;
  } const u = {.em = (fio_stream_packet_embd_s *)(p + 1)};

  switch ((fio_stream_packet_type_e)(u.em->type & 3)) {
  case FIO_PACKET_TYPE_EMBEDDED:
    len = u.em->type >> FIO_STREAM___EMBD_BIT_OFFSET;
    return len;
  case FIO_PACKET_TYPE_EXTERNAL:
    len = u.ext->length;
    return len;
  case FIO_PACKET_TYPE_FILE: /* fallthrough */
  case FIO_PACKET_TYPE_FILE_NO_CLOSE:
    len = u.f->length;
    return len;
  }
  return len;
}

FIO_SFUNC void fio___stream_read_internal(fio_stream_packet_s *p,
                                          char **buf,
                                          size_t *len,
                                          size_t buf_offset,
                                          size_t offset,
                                          size_t must_copy) {
  if (!p || !len[0]) {
    len[0] = 0;
    return;
  }
  union {
    fio_stream_packet_embd_s *em;
    fio_stream_packet_extrn_s *ext;
    fio_stream_packet_fd_s *f;
  } const u = {.em = (fio_stream_packet_embd_s *)(p + 1)};
  size_t written = 0;

  switch ((fio_stream_packet_type_e)(u.em->type & 3)) {
  case FIO_PACKET_TYPE_EMBEDDED:
    if (!buf[0] || !len[0] ||
        (!must_copy &&
         (!p->next ||
          (u.em->type >> FIO_STREAM___EMBD_BIT_OFFSET) >= len[0] + offset))) {
      buf[0] = u.em->buf + offset;
      len[0] = (size_t)(u.em->type >> FIO_STREAM___EMBD_BIT_OFFSET) - offset;
      return;
    }
    written = (u.em->type >> FIO_STREAM___EMBD_BIT_OFFSET) - offset;
    if (written > len[0])
      written = len[0];
    if (written) {
      FIO_MEMCPY(buf[0] + buf_offset, u.em->buf + offset, written);
      len[0] -= written;
    }
    if (len[0]) {
      fio___stream_read_internal(p->next, buf, len, written + buf_offset, 0, 1);
    }
    len[0] += written;
    return;
  case FIO_PACKET_TYPE_EXTERNAL:
    if (!buf[0] || !len[0] ||
        (!must_copy && (!p->next || u.ext->length >= len[0] + offset))) {
      buf[0] = u.ext->buf + u.ext->offset + offset;
      len[0] = (size_t)(u.ext->length) - offset;
      return;
    }
    written = u.ext->length - offset;
    if (written > len[0])
      written = len[0];
    if (written) {
      FIO_MEMCPY(buf[0] + buf_offset,
                 u.ext->buf + u.ext->offset + offset,
                 written);
      len[0] -= written;
    }
    if (len[0]) {
      fio___stream_read_internal(p->next, buf, len, written + buf_offset, 0, 1);
    }
    len[0] += written;
    return;
    break;
  case FIO_PACKET_TYPE_FILE: /* fallthrough */
  case FIO_PACKET_TYPE_FILE_NO_CLOSE:
    if (!buf[0] || !len[0]) {
      buf[0] = NULL;
      len[0] = 0;
      return;
    }
    {
      uint8_t possible_eol_surprise = 0;
      written = u.f->length - offset;
      if (written > len[0])
        written = len[0];
      if (written) {
        ssize_t act;
      retry_on_signal:
        act =
            pread(u.f->fd, buf[0] + buf_offset, written, u.f->offset + offset);
        if (act <= 0) {
          /* no more data in the file? */
          FIO_LOG_DEBUG("file read error for %d: %s", u.f->fd, strerror(errno));
          if (errno == EINTR)
            goto retry_on_signal;
          u.f->length = offset;
        } else if ((size_t)act != written) {
          /* a surprising EOF? */
          written = act;
          possible_eol_surprise = 1;
        }
        len[0] -= written;
      }
      if (!possible_eol_surprise && len[0]) {
        fio___stream_read_internal(p->next,
                                   buf,
                                   len,
                                   written + buf_offset,
                                   0,
                                   1);
      }
      len[0] += written;
    }
    return;
  }
}

/**
 * Reads data from the stream (if any), leaving it in the stream.
 *
 * `buf` MUST point to a buffer with - at least - `len` bytes. This is required
 * in case the packed data is fragmented or references a file and needs to be
 * copied to an available buffer.
 *
 * On error, or if the stream is empty, `buf` will be set to NULL and `len` will
 * be set to zero.
 *
 * Otherwise, `buf` may retain the same value or it may point directly to a
 * memory address wiithin the stream's buffer (the original value may be lost)
 * and `len` will be updated to the largest possible value for valid data that
 * can be read from `buf`.
 *
 * Note: this isn't thread safe.
 */
SFUNC void fio_stream_read(fio_stream_s *s, char **buf, size_t *len) {
  if (!s || !s->next)
    goto none;
  fio___stream_read_internal(s->next, buf, len, 0, s->consumed, 0);
  return;
none:
  *buf = NULL;
  *len = 0;
}

/**
 * Advances the Stream, so the first `len` bytes are marked as consumed.
 *
 * Note: this isn't thread safe.
 */
SFUNC void fio_stream_advance(fio_stream_s *s, size_t len) {
  if (!s || !s->next)
    return;
  len += s->consumed;
  while (len) {
    size_t p_len = fio___stream_p2len(s->next);
    if (len >= p_len) {
      fio_stream_packet_s *p = s->next;
      s->next = p->next;
      --s->packets;
      fio_stream_packet_free(p);
      len -= p_len;
      if (!s->next) {
        s->pos = &s->next;
        s->consumed = 0;
        s->packets = 0;
        return;
      }
    } else {
      s->consumed = len;
      return;
    }
  }
  s->consumed = len;
}

/* *****************************************************************************
Stream Testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL

FIO_SFUNC size_t FIO_NAME_TEST(stl, stream___noop_dealloc_count) = 0;
FIO_SFUNC void FIO_NAME_TEST(stl, stream___noop_dealloc)(void *ignr_) {
  fio_atomic_add(&FIO_NAME_TEST(stl, stream___noop_dealloc_count), 1);
  (void)ignr_;
}

FIO_SFUNC void FIO_NAME_TEST(stl, stream)(void) {
  char *const str =
      (char *)"My Hello World string should be long enough so it can be used "
              "for testing the stream functionality in the facil.io stream "
              "module. The stream moduule takes strings and failes and places "
              "them (by reference / copy) into a linked list of objects. When "
              "data is requested from the stream, the stream will either copy "
              "the data to a pre-allocated buffer or it may update the link to "
              "it points to its own internal buffer (avoiding a copy when "
              "possible).";
  fio_stream_s s = FIO_STREAM_INIT(s);
  char mem[4000];
  char *buf = mem;
  size_t len = 4000;
  size_t expect_dealloc = FIO_NAME_TEST(stl, stream___noop_dealloc_count);

  fprintf(stderr, "* Testing fio_stream for streaming buffer storage.\n");
  fio_stream_add(
      &s,
      fio_stream_pack_data(str,
                           11,
                           3,
                           1,
                           FIO_NAME_TEST(stl, stream___noop_dealloc)));
  ++expect_dealloc;
  FIO_ASSERT(fio_stream_any(&s),
             "stream is empty after `fio_stream_add` (data, copy)");
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "copying a packet should deallocate the original");
  for (int i = 0; i < 3; ++i) {
    /* test that read operrations are immutable */
    buf = mem;
    len = 4000;

    fio_stream_read(&s, &buf, &len);
    FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) ==
                   expect_dealloc,
               "reading a packet shouldn't deallocate anything");
    FIO_ASSERT(len == 11,
               "fio_stream_read didn't read all data from stream? (%zu)",
               len);
    FIO_ASSERT(!memcmp(str + 3, buf, len),
               "fio_stream_read data error? (%.*s)",
               (int)len,
               buf);
    FIO_ASSERT_DEBUG(
        buf != mem,
        "fio_stream_read should have been performed with zero-copy");
  }
  fio_stream_advance(&s, len);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "advancing an embedded packet shouldn't deallocate anything");
  FIO_ASSERT(
      !fio_stream_any(&s),
      "after advance, at this point, the stream should have been consumed.");
  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);
  FIO_ASSERT(
      !buf && !len,
      "reading from an empty stream should set buf and len to NULL and zero.");
  fio_stream_destroy(&s);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "destroying an empty stream shouldn't deallocate anything");
  FIO_ASSERT(!fio_stream_any(&s), "destroyed stream should be empty.");

  fio_stream_add(&s, fio_stream_pack_data(str, 11, 0, 1, NULL));
  fio_stream_add(
      &s,
      fio_stream_pack_data(str,
                           49,
                           11,
                           0,
                           FIO_NAME_TEST(stl, stream___noop_dealloc)));
  fio_stream_add(&s, fio_stream_pack_data(str, 20, 60, 0, NULL));

  FIO_ASSERT(fio_stream_any(&s), "stream with data shouldn't be empty.");
  FIO_ASSERT(fio_stream_packets(&s) == 3, "packet counut error.");
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "adding a stream shouldn't deallocate it.");

  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);

  FIO_ASSERT(len == 80,
             "fio_stream_read didn't read all data from stream(2)? (%zu)",
             len);
  FIO_ASSERT(!memcmp(str, buf, len),
             "fio_stream_read data error? (%.*s)",
             (int)len,
             buf);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "reading a stream shouldn't deallocate any packets.");

  buf = mem;
  len = 8;
  fio_stream_read(&s, &buf, &len);

  FIO_ASSERT(len < 80,
             "fio_stream_read didn't perform a partial read? (%zu)",
             len);
  FIO_ASSERT(!memcmp(str, buf, len),
             "fio_stream_read partial read data error? (%.*s)",
             (int)len,
             buf);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "failing to read a stream shouldn't deallocate any packets.");

  fio_stream_advance(&s, 20);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "partial advancing shouldn't deallocate any packets.");
  FIO_ASSERT(fio_stream_packets(&s) == 2, "packet counut error (2).");
  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);
  FIO_ASSERT(len == 60,
             "fio_stream_read didn't read all data from stream(3)? (%zu)",
             len);
  FIO_ASSERT(!memcmp(str + 20, buf, len),
             "fio_stream_read data error? (%.*s)",
             (int)len,
             buf);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "reading shouldn't deallocate packets the head packet.");

  fio_stream_add(&s, fio_stream_pack_fd(open(__FILE__, O_RDONLY), 20, 0, 0));
  FIO_ASSERT(fio_stream_packets(&s) == 3, "packet counut error (3).");
  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);
  FIO_ASSERT(len == 80,
             "fio_stream_read didn't read all data from stream(4)? (%zu)",
             len);
  FIO_ASSERT(!memcmp("/* *****************", buf + 60, 20),
             "fio_stream_read file read data error?\n%.*s",
             (int)len,
             buf);
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "reading more than one packet shouldn't deallocate anything.");
  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);
  FIO_ASSERT(len == 80,
             "fio_stream_read didn't (re)read all data from stream(5)? (%zu)",
             len);
  FIO_ASSERT(!memcmp("/* *****************", buf + 60, 20),
             "fio_stream_read file (re)read data error? (%.*s)",
             (int)len,
             buf);

  fio_stream_destroy(&s);
  ++expect_dealloc;

  FIO_ASSERT(!fio_stream_any(&s), "destroyed stream should be empty.");
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "destroying a stream should deallocate it's packets.");
  fio_stream_add(
      &s,
      fio_stream_pack_data(str,
                           49,
                           11,
                           0,
                           FIO_NAME_TEST(stl, stream___noop_dealloc)));
  buf = mem;
  len = 4000;
  fio_stream_read(&s, &buf, &len);
  FIO_ASSERT(len == 49,
             "fio_stream_read didn't read all data from stream? (%zu)",
             len);
  FIO_ASSERT(!memcmp(str + 11, buf, len),
             "fio_stream_read data error? (%.*s)",
             (int)len,
             buf);
  fio_stream_advance(&s, 80);
  ++expect_dealloc;
  FIO_ASSERT(FIO_NAME_TEST(stl, stream___noop_dealloc_count) == expect_dealloc,
             "partial advancing shouldn't deallocate any packets.");
  FIO_ASSERT(!fio_stream_any(&s), "stream should be empty at this point.");
  fio_stream_destroy(&s);
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_STREAM___EMBD_BIT_OFFSET
#endif /* FIO_STREAM */
#undef FIO_STREAM
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_ATOMIC                  /* Development inclusion - ignore line */
#define FIO_SIGNAL                  /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                              Signal Monitoring




***************************************************************************** */
#if defined(FIO_SIGNAL) && !defined(H___FIO_SIGNAL___H)
#define H___FIO_SIGNAL___H

#ifndef FIO_SIGNAL_MONITOR_MAX
/* The maximum number of signals the implementation will be able to monitor */
#define FIO_SIGNAL_MONITOR_MAX 24
#endif

#if !(FIO_OS_POSIX) && !(FIO_OS_WIN) /* use FIO_HAVE_UNIX_TOOLS instead? */
#error Either POSIX or Windows are required for the fio_signal API.
#endif

#include <signal.h>
/* *****************************************************************************
Signal Monitoring API
***************************************************************************** */

/**
 * Starts to monitor for the specified signal, setting an optional callback.
 */
SFUNC int fio_signal_monitor(int sig,
                             void (*callback)(int sig, void *),
                             void *udata);

/** Reviews all signals, calling any relevant callbacks. */
SFUNC int fio_signal_review(void);

/** Stops monitoring the specified signal. */
SFUNC int fio_signal_forget(int sig);

/* *****************************************************************************




                          Signal Monitoring Implementation




***************************************************************************** */

/* *****************************************************************************
Signal Monitoring Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/* *****************************************************************************
POSIX implementation
***************************************************************************** */
#ifdef FIO_OS_POSIX

static struct {
  int32_t sig;
  volatile int32_t flag;
  void (*callback)(int sig, void *);
  void *udata;
  struct sigaction old;
} fio___signal_watchers[FIO_SIGNAL_MONITOR_MAX];

FIO_SFUNC void fio___signal_catcher(int sig) {
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    if (!fio___signal_watchers[i].sig && !fio___signal_watchers[i].udata)
      return; /* initialized list is finishe */
    if (fio___signal_watchers[i].sig != sig)
      continue;
    /* mark flag */
    fio_atomic_exchange(&fio___signal_watchers[i].flag, 1);
    /* pass-through if exists */
    if (fio___signal_watchers[i].old.sa_handler != SIG_IGN &&
        fio___signal_watchers[i].old.sa_handler != SIG_DFL)
      fio___signal_watchers[i].old.sa_handler(sig);
    return;
  }
}

/**
 * Starts to monitor for the specified signal, setting an optional callback.
 */
SFUNC int fio_signal_monitor(int sig,
                             void (*callback)(int sig, void *),
                             void *udata) {
  if (!sig)
    return -1;
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    /* updating an existing monitor */
    if (fio___signal_watchers[i].sig == sig) {
      fio___signal_watchers[i].callback = callback;
      fio___signal_watchers[i].udata = udata;
      return 0;
    }
    /* slot busy */
    if (fio___signal_watchers[i].sig || fio___signal_watchers[i].callback)
      continue;
    /* place monitor in this slot */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    memset(fio___signal_watchers + i, 0, sizeof(fio___signal_watchers[i]));
    fio___signal_watchers[i].sig = sig;
    fio___signal_watchers[i].callback = callback;
    fio___signal_watchers[i].udata = udata;
    act.sa_handler = fio___signal_catcher;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(sig, &act, &fio___signal_watchers[i].old)) {
      FIO_LOG_ERROR("couldn't set signal handler: %s", strerror(errno));
      fio___signal_watchers[i].callback = NULL;
      fio___signal_watchers[i].udata = (void *)1;
      fio___signal_watchers[i].sig = 0;
      return -1;
    }
    return 0;
  }
  return -1;
}

/** Stops monitoring the specified signal. */
SFUNC int fio_signal_forget(int sig) {
  if (!sig)
    return -1;
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    if (!fio___signal_watchers[i].sig && !fio___signal_watchers[i].udata)
      return -1; /* initialized list is finishe */
    if (fio___signal_watchers[i].sig != sig)
      continue;
    fio___signal_watchers[i].callback = NULL;
    fio___signal_watchers[i].udata = (void *)1;
    fio___signal_watchers[i].sig = 0;
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    if (sigaction(sig, &fio___signal_watchers[i].old, &act)) {
      FIO_LOG_ERROR("couldn't unset signal handler: %s", strerror(errno));
      return -1;
    }
    return 0;
  }
  return -1;
}

/* *****************************************************************************
Windows Implementation
***************************************************************************** */
#elif FIO_OS_WIN

static struct {
  int32_t sig;
  volatile int32_t flag;
  void (*callback)(int sig, void *);
  void *udata;
  void (*old)(int sig);
} fio___signal_watchers[FIO_SIGNAL_MONITOR_MAX];

FIO_SFUNC void fio___signal_catcher(int sig) {
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    if (!fio___signal_watchers[i].sig && !fio___signal_watchers[i].udata)
      return; /* initialized list is finished */
    if (fio___signal_watchers[i].sig != sig)
      continue;
    /* mark flag */
    fio___signal_watchers[i].flag = 1;
    /* pass-through if exists */
    if (fio___signal_watchers[i].old &&
        (intptr_t)fio___signal_watchers[i].old != (intptr_t)SIG_IGN &&
        (intptr_t)fio___signal_watchers[i].old != (intptr_t)SIG_DFL) {
      fio___signal_watchers[i].old(sig);
      fio___signal_watchers[i].old = signal(sig, fio___signal_catcher);
    } else {
      fio___signal_watchers[i].old = signal(sig, fio___signal_catcher);
    }
    break;
  }
}

/**
 * Starts to monitor for the specified signal, setting an optional callback.
 */
SFUNC int fio_signal_monitor(int sig,
                             void (*callback)(int sig, void *),
                             void *udata) {
  if (!sig)
    return -1;
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    /* updating an existing monitor */
    if (fio___signal_watchers[i].sig == sig) {
      fio___signal_watchers[i].callback = callback;
      fio___signal_watchers[i].udata = udata;
      return 0;
    }
    /* slot busy */
    if (fio___signal_watchers[i].sig || fio___signal_watchers[i].callback)
      continue;
    /* place monitor in this slot */
    fio___signal_watchers[i].sig = sig;
    fio___signal_watchers[i].callback = callback;
    fio___signal_watchers[i].udata = udata;
    fio___signal_watchers[i].old = signal(sig, fio___signal_catcher);
    if ((intptr_t)SIG_ERR == (intptr_t)fio___signal_watchers[i].old) {
      fio___signal_watchers[i].sig = 0;
      fio___signal_watchers[i].callback = NULL;
      fio___signal_watchers[i].udata = (void *)1;
      fio___signal_watchers[i].old = NULL;
      FIO_LOG_ERROR("couldn't set signal handler: %s", strerror(errno));
      return -1;
    }
    return 0;
  }
  return -1;
}

/** Stops monitoring the specified signal. */
SFUNC int fio_signal_forget(int sig) {
  if (!sig)
    return -1;
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    if (!fio___signal_watchers[i].sig && !fio___signal_watchers[i].udata)
      return -1; /* initialized list is finished */
    if (fio___signal_watchers[i].sig != sig)
      continue;
    fio___signal_watchers[i].callback = NULL;
    fio___signal_watchers[i].udata = (void *)1;
    fio___signal_watchers[i].sig = 0;
    if (fio___signal_watchers[i].old) {
      if ((intptr_t)signal(sig, fio___signal_watchers[i].old) ==
          (intptr_t)SIG_ERR)
        goto sig_error;
    } else {
      if ((intptr_t)signal(sig, SIG_DFL) == (intptr_t)SIG_ERR)
        goto sig_error;
    }
    return 0;
  }
  return -1;
sig_error:
  FIO_LOG_ERROR("couldn't unset signal handler: %s", strerror(errno));
  return -1;
}
#endif /* POSIX vs WINDOWS */

/* *****************************************************************************
Common OS implementation
***************************************************************************** */

/** Reviews all signals, calling any relevant callbacks. */
SFUNC int fio_signal_review(void) {
  int c = 0;
  for (size_t i = 0; i < FIO_SIGNAL_MONITOR_MAX; ++i) {
    if (!fio___signal_watchers[i].sig && !fio___signal_watchers[i].udata)
      return c;
    if (fio___signal_watchers[i].flag) {
      fio___signal_watchers[i].flag = 0;
      ++c;
      if (fio___signal_watchers[i].callback)
        fio___signal_watchers[i].callback(fio___signal_watchers[i].sig,
                                          fio___signal_watchers[i].udata);
    }
  }
  return c;
}

/* *****************************************************************************
Signal Monitoring Testing?
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, signal)(void) {

#define FIO___SIGNAL_MEMBER(a)                                                 \
  { (int)a, #a }
  struct {
    int sig;
    const char *name;
  } t[] = {
    FIO___SIGNAL_MEMBER(SIGINT),
    FIO___SIGNAL_MEMBER(SIGILL),
    FIO___SIGNAL_MEMBER(SIGABRT),
    FIO___SIGNAL_MEMBER(SIGSEGV),
    FIO___SIGNAL_MEMBER(SIGTERM),
#if FIO_OS_POSIX
    FIO___SIGNAL_MEMBER(SIGQUIT),
    FIO___SIGNAL_MEMBER(SIGHUP),
    FIO___SIGNAL_MEMBER(SIGTRAP),
    FIO___SIGNAL_MEMBER(SIGBUS),
    FIO___SIGNAL_MEMBER(SIGFPE),
    FIO___SIGNAL_MEMBER(SIGUSR1),
    FIO___SIGNAL_MEMBER(SIGUSR2),
    FIO___SIGNAL_MEMBER(SIGPIPE),
    FIO___SIGNAL_MEMBER(SIGALRM),
    FIO___SIGNAL_MEMBER(SIGCHLD),
    FIO___SIGNAL_MEMBER(SIGCONT),
#endif
  };
#undef FIO___SIGNAL_MEMBER
  size_t e = 0;
  fprintf(stderr, "* testing signal monitoring (setup / cleanup only).\n");
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
    if (fio_signal_monitor(t[i].sig, NULL, NULL)) {
      FIO_LOG_ERROR("couldn't set signal monitoring for %s (%d)",
                    t[i].name,
                    t[i].sig);
      e = 1;
    }
  }
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
    if (fio_signal_forget(t[i].sig)) {
      FIO_LOG_ERROR("couldn't stop signal monitoring for %s (%d)",
                    t[i].name,
                    t[i].sig);
      e = 1;
    }
  }
  FIO_ASSERT(!e, "signal monitoring error");
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_SIGNAL_MONITOR_MAX
#endif /* FIO_SIGNAL */
#undef FIO_SIGNAL
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_ATOMIC                  /* Development inclusion - ignore line */
#define FIO_GLOB_MATCH              /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            Globe Matching




***************************************************************************** */
#if defined(FIO_GLOB_MATCH) && !defined(H___FIO_GLOB_MATCH___H)
#define H___FIO_GLOB_MATCH___H

/* *****************************************************************************
Globe Matching API
***************************************************************************** */

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
SFUNC uint8_t fio_glob_match(fio_str_info_s pattern, fio_str_info_s string);

/* *****************************************************************************




                          Globe Matching Implementation




***************************************************************************** */

/* *****************************************************************************
Globe Matching Monitoring Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/* *****************************************************************************
 * Glob Matching
 **************************************************************************** */

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
SFUNC uint8_t fio_glob_match(fio_str_info_s pat, fio_str_info_s str) {
  /* adapted and rewritten, with thankfulness, from the code at:
   * https://github.com/opnfv/kvmfornfv/blob/master/kernel/lib/glob.c
   *
   * Original version's copyright:
   * Copyright 2015 Open Platform for NFV Project, Inc. and its contributors
   * Under the MIT license.
   */

  /*
   * Backtrack to previous * on mismatch and retry starting one
   * character later in the string.  Because * matches all characters,
   * there's never a need to backtrack multiple levels.
   */
  uint8_t *back_pat = NULL, *back_str = (uint8_t *)str.buf;
  size_t back_pat_len = 0, back_str_len = str.len;

  /*
   * Loop over each token (character or class) in pat, matching
   * it against the remaining unmatched tail of str.  Return false
   * on mismatch, or true after matching the trailing nul bytes.
   */
  while (str.len && pat.len) {
    uint8_t c = *(uint8_t *)str.buf++;
    uint8_t d = *(uint8_t *)pat.buf++;
    str.len--;
    pat.len--;

    switch (d) {
    case '?': /* Wildcard: anything goes */
      break;

    case '*':       /* Any-length wildcard */
      if (!pat.len) /* Optimize trailing * case */
        return 1;
      back_pat = (uint8_t *)pat.buf;
      back_pat_len = pat.len;
      back_str = (uint8_t *)--str.buf; /* Allow zero-length match */
      back_str_len = ++str.len;
      break;

    case '[': { /* Character class */
      uint8_t match = 0, inverted = (*(uint8_t *)pat.buf == '^' ||
                                     *(uint8_t *)pat.buf == '!');
      uint8_t *cls = (uint8_t *)pat.buf + inverted;
      uint8_t a = *cls++;

      /*
       * Iterate over each span in the character class.
       * A span is either a single character a, or a
       * range a-b.  The first span may begin with ']'.
       */
      do {
        uint8_t b = a;
        if (a == '\\') { /* when escaped, next character is regular */
          b = a = *(cls++);
        } else if (cls[0] == '-' && cls[1] != ']') {
          b = cls[1];

          cls += 2;
          if (a > b) {
            uint8_t tmp = a;
            a = b;
            b = tmp;
          }
        }
        match |= (a <= c && c <= b);
      } while ((a = *cls++) != ']');

      if (match == inverted)
        goto backtrack;
      pat.len -= cls - (uint8_t *)pat.buf;
      pat.buf = (char *)cls;

    } break;
    case '\\':
      d = *(uint8_t *)pat.buf++;
      pat.len--;
    /* fallthrough */
    default: /* Literal character */
      if (c == d)
        break;
    backtrack:
      if (!back_pat)
        return 0; /* No point continuing */
      /* Try again from last *, one character later in str. */
      pat.buf = (char *)back_pat;
      str.buf = (char *)++back_str;
      str.len = --back_str_len;
      pat.len = back_pat_len;
    }
  }
  /* if the trailing pattern allows for empty data, skip it */
  while (pat.len && pat.buf[0] == '*') {
    ++pat.buf;
    --pat.len;
  }
  return !str.len && !pat.len;
}

/* *****************************************************************************
Globe Matching Monitoring Testing?
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, glob_matching)(void) {
  struct {
    char *pat;
    char *str;
    uint8_t expect;
  } t[] = {
      // clang-format off
      /* test empty string */
      {.pat = (char *)"", .str = (char *)"", .expect = 1},
      /* test exact match */
      {.pat = (char *)"a", .str = (char *)"a", .expect = 1},
      /* test empty pattern */
      {.pat = (char *)"", .str = (char *)"a", .expect = 0},
      /* test longer pattern */
      {.pat = (char *)"a", .str = (char *)"", .expect = 0},
      /* test empty string with glob pattern */
      {.pat = (char *)"*", .str = (char *)"", .expect = 1},
      /* test glob pattern */
      {.pat = (char *)"*", .str = (char *)"Whatever", .expect = 1},
      /* test glob pattern at end */
      {.pat = (char *)"W*", .str = (char *)"Whatever", .expect = 1},
      /* test glob pattern as bookends */
      {.pat = (char *)"*Whatever*", .str = (char *)"Whatever", .expect = 1},
      /* test glob pattern in the middle */
      {.pat = (char *)"W*er", .str = (char *)"Whatever", .expect = 1},
      /* test glob pattern in the middle - empty match*/
      {.pat = (char *)"W*hatever", .str = (char *)"Whatever", .expect = 1},
      /* test glob pattern in the middle  - no match */
      {.pat = (char *)"W*htever", .str = (char *)"Whatever", .expect = 0},
      /* test partial match with glob at end */
      {.pat = (char *)"h*", .str = (char *)"Whatever", .expect = 0},
      /* test partial match with glob in the middle */
      {.pat = (char *)"h*er", .str = (char *)"Whatever", .expect = 0},
      /* test glob match with "?"  */
      {.pat = (char *)"?h*er", .str = (char *)"Whatever", .expect = 1},
      /* test "?" for length restrictions */
      {.pat = (char *)"?", .str = (char *)"Whatever", .expect = 0},
      /* test ? in the middle */
      {.pat = (char *)"What?ver", .str = (char *)"Whatever", .expect = 1},
      /* test letter list */
      {.pat = (char *)"[ASW]hat?ver", .str = (char *)"Whatever", .expect = 1},
      /* test letter range */
      {.pat = (char *)"[A-Z]hat?ver", .str = (char *)"Whatever", .expect = 1},
      /* test letter range (fail) */
      {.pat = (char *)"[a-z]hat?ver", .str = (char *)"Whatever", .expect = 0},
      /* test inverted letter range */
      {.pat = (char *)"[!a-z]hat?ver", .str = (char *)"Whatever", .expect = 1},
      /* test inverted list */
      {.pat = (char *)"[!F]hat?ver", .str = (char *)"Whatever", .expect = 1},
      /* test escaped range */
      {.pat = (char *)"[!a-z\\]]hat?ver", .str = (char *)"Whatever", .expect = 1},
      /* test "?" after range (no skip) */
      {.pat = (char *)"[A-Z]?at?ver", .str = (char *)"Whatever", .expect = 1},
      /* test error after range (no skip) */
      {.pat = (char *)"[A-Z]Fat?ver", .str = (char *)"Whatever", .expect = 0},
      /* end of test marker */
      {.pat = (char *)NULL, .str = (char *)NULL, .expect = 0},
      // clang-format on
  };
  fprintf(stderr, "* testing glob matching.\n");
  for (size_t i = 0; t[i].pat; ++i) {
    fio_str_info_s p, s;
    p.buf = t[i].pat;
    p.len = strlen(t[i].pat);
    s.buf = t[i].str;
    s.len = strlen(t[i].str);
    FIO_ASSERT(t[i].expect == fio_glob_match(p, s),
               "glob matching error for:\n\t String: %s\n\t Pattern: %s",
               s.buf,
               p.buf);
  }
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_GLOB_MATCH_MONITOR_MAX
#endif /* FIO_GLOB_MATCH */
#undef FIO_GLOB_MATCH
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_FILES                   /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "005 riskyhash.h"          /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                   Common File Operations (POSIX style)




***************************************************************************** */
#if defined(FIO_FILES) && !defined(H___FIO_FILES___H)
#define H___FIO_FILES___H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* *****************************************************************************
File Helper API
***************************************************************************** */

/**
 * Opens `filename`, returning the same as values as `open` on POSIX systems.
 *
 * If `path` starts with a `"~/"` than it will be relative to the user's home
 * folder (on Windows, testing for `"~\"`).
 */
SFUNC int fio_filename_open(const char *filename, int flags);

/** Returns 1 if `path` does folds backwards (has "/../" or "//"). */
SFUNC int fio_filename_is_unsafe(const char *path);

/** Creates a temporary file, returning its file descriptor. */
SFUNC int fio_filename_tmp(void);

/**
 * Overwrites `filename` with the data in the buffer.
 *
 * If `path` starts with a `"~/"` than it will be relative to the user's home
 * folder (on Windows, testing for `"~\"`).
 *
 * Returns -1 on error or 0 on success. On error, the state of the file is
 * undefined (may be doesn't exit / nothing written / partially written).
 */
FIO_IFUNC int fio_filename_overwrite(const char *filename,
                                     const void *buf,
                                     size_t len);

/**
 * Writes data to a file, returning the number of bytes written.
 *
 * Returns -1 on error.
 *
 * Since some systems have a limit on the number of bytes that can be written at
 * a single time, this function fragments the system calls into smaller `write`
 * blocks, allowing large data to be written.
 *
 * If the file descriptor is non-blocking, test errno for EAGAIN / EWOULDBLOCK.
 */
FIO_IFUNC ssize_t fio_fd_write(int fd, const void *buf, size_t len);

/* *****************************************************************************
File Helper Inline Implementation
***************************************************************************** */

/**
 * Writes data to a file, returning the number of bytes written.
 *
 * Returns -1 on error.
 *
 * Since some systems have a limit on the number of bytes that can be written at
 * a single time, this function fragments the system calls into smaller `write`
 * blocks, allowing large data to be written.
 *
 * If the file descriptor is non-blocking, test errno for EAGAIN / EWOULDBLOCK.
 */
FIO_IFUNC ssize_t fio_fd_write(int fd, const void *buf_, size_t len) {
  ssize_t total = 0;
  const char *buf = (const char *)buf_;
  const size_t write_limit = (1ULL << 17);
  while (len > write_limit) {
    ssize_t w = write(fd, buf, write_limit);
    if (w > 0) {
      len -= w;
      buf += w;
      total += w;
      continue;
    }
    /* if (w == -1 && errno == EINTR) continue; */
    if (total == 0)
      return -1;
    return total;
  }
  while (len) {
    ssize_t w = write(fd, buf, len);
    if (w > 0) {
      len -= w;
      buf += w;
      continue;
    }
    if (total == 0)
      return -1;
    return total;
  }
  return total;
}

/**
 * Overwrites `filename` with the data in the buffer.
 *
 * If `path` starts with a `"~/"` than it will be relative to the user's home
 * folder (on Windows, testing for `"~\"`).
 */
FIO_IFUNC int fio_filename_overwrite(const char *filename,
                                     const void *buf,
                                     size_t len) {
  int fd = fio_filename_open(filename, O_RDWR | O_CREAT | O_TRUNC);
  if (fd == -1)
    return -1;
  ssize_t w = fio_fd_write(fd, buf, len);
  close(fd);
  if ((size_t)w != len)
    return -1;
  return 0;
}

/* *****************************************************************************
File Helper Implementation
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/**
 * Opens `filename`, returning the same as values as `open` on POSIX systems.
 *
 * If `path` starts with a `"~/"` than it will be relative to the user's home
 * folder (on Windows, testing for `"~\"`).
 */
SFUNC int fio_filename_open(const char *filename, int flags) {
  int fd = -1;
  /* POSIX implementations. */
  if (filename == NULL)
    return fd;
  char *path = NULL;
  size_t path_len = 0;
#if FIO_OS_WIN
  const char sep = '\\';
#else
  const char sep = '/';
#endif

  if (filename[0] == '~' && filename[1] == sep) {
    char *home = getenv("HOME");
    if (home) {
      size_t filename_len = strlen(filename);
      size_t home_len = strlen(home);
      if ((home_len + filename_len) >= (1 << 16)) {
        /* too long */
        FIO_LOG_ERROR("couldn't open file, as filename is too long %.*s...",
                      (int)16,
                      (filename_len >= 16 ? filename : home));
        return fd;
      }
      if (home[home_len - 1] == sep)
        --home_len;
      path_len = home_len + filename_len - 1;
      path =
          (char *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*path) * (path_len + 1), 0);
      if (!path)
        return fd;
      FIO_MEMCPY(path, home, home_len);
      FIO_MEMCPY(path + home_len, filename + 1, filename_len);
      path[path_len] = 0;
      filename = path;
    }
  }
  fd = open(filename, flags);
  if (path) {
    FIO_MEM_FREE_(path, path_len + 1);
  }
  return fd;
}

/** Returns 1 if `path` does folds backwards (has "/../" or "//"). */
SFUNC int fio_filename_is_unsafe(const char *path) {
#if FIO_OS_WIN
  const char sep = '\\';
#else
  const char sep = '/';
#endif
  for (;;) {
    if (!path)
      return 0;
    if (path[0] == sep && path[1] == sep)
      return 1;
    if (path[0] == sep && path[1] == '.' && path[2] == '.' && path[3] == sep)
      return 1;
    ++path;
    path = strchr(path, sep);
  }
}

/** Creates a temporary file, returning its file descriptor. */
SFUNC int fio_filename_tmp(void) {
  // create a temporary file to contain the data.
  int fd;
  char name_template[512];
  size_t len = 0;
#if FIO_OS_WIN
  const char sep = '\\';
  const char *tmp = NULL;
#else
  const char sep = '/';
  const char *tmp = NULL;
#endif

  if (!tmp)
    tmp = getenv("TMPDIR");
  if (!tmp)
    tmp = getenv("TMP");
  if (!tmp)
    tmp = getenv("TEMP");
#if defined(P_tmpdir)
  if (!tmp && sizeof(P_tmpdir) <= 464 && sizeof(P_tmpdir) > 0) {
    tmp = P_tmpdir;
  }
#endif
  if (tmp && (len = strlen(tmp))) {
    FIO_MEMCPY(name_template, tmp, len);
    if (tmp[len - 1] != sep) {
      name_template[len++] = sep;
    }
  } else {
    /* use current folder */
    name_template[len++] = '.';
    name_template[len++] = sep;
  }

  FIO_MEMCPY(name_template + len, "facil_io_tmpfile_", 17);
  len += 17;
  do {
#ifdef O_TMPFILE
    uint64_t r = fio_rand64();
    size_t delta = fio_ltoa(name_template + len, r, 32);
    name_template[delta + len] = 0;
    fd = open(name_template, O_CREAT | O_TMPFILE | O_EXCL | O_RDWR);
#else
    FIO_MEMCPY(name_template + len, "XXXXXXXXXXXX", 12);
    name_template[12 + len] = 0;
    fd = mkstemp(name_template);
#endif
  } while (fd == -1 && errno == EEXIST);
  return fd;
  (void)tmp;
}

/* *****************************************************************************
Module Testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, filename)(void) { /* TODO: test module */
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_FILES */
#undef FIO_FILES
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            Linked Lists (embeded)








Example:

```c
// initial `include` defines the `FIO_LIST_NODE` macro and type
#include "fio-stl.h"
// list element
typedef struct {
  long l;
  FIO_LIST_NODE node;
  int i;
  double d;
} my_list_s;
// create linked list helper functions
#define FIO_LIST_NAME my_list
#include "fio-stl.h"

void example(void) {
  FIO_LIST_HEAD FIO_LIST_INIT(list);
  for (int i = 0; i < 10; ++i) {
    my_list_s *n = malloc(sizeof(*n));
    n->i = i;
    my_list_push(&list, n);
  }
  int i = 0;
  while (my_list_any(&list)) {
    my_list_s *n = my_list_shift(&list);
    if (i != n->i) {
      fprintf(stderr, "list error - value mismatch\n"), exit(-1);
    }
    free(n);
    ++i;
  }
  if (i != 10) {
    fprintf(stderr, "list error - count error\n"), exit(-1);
  }
}
```

***************************************************************************** */

/* *****************************************************************************
Linked Lists (embeded) - Type
***************************************************************************** */

#if defined(FIO_LIST_NAME)

#ifndef FIO_LIST_TYPE
/** Name of the list type and function prefix, defaults to FIO_LIST_NAME_s */
#define FIO_LIST_TYPE FIO_NAME(FIO_LIST_NAME, s)
#endif

#ifndef FIO_LIST_NODE_NAME
/** List types must contain at least one node element, defaults to `node`. */
#define FIO_LIST_NODE_NAME node
#endif

#ifdef FIO_PTR_TAG_TYPE
#define FIO_LIST_TYPE_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_LIST_TYPE_PTR FIO_LIST_TYPE *
#endif

/* *****************************************************************************
Linked Lists (embeded) - API
***************************************************************************** */

/** Initialize FIO_LIST_HEAD objects - already defined. */
/* FIO_LIST_INIT(obj) */

/** Returns a non-zero value if there are any linked nodes in the list. */
IFUNC int FIO_NAME(FIO_LIST_NAME, any)(const FIO_LIST_HEAD *head);

/** Returns a non-zero value if the list is empty. */
IFUNC int FIO_NAME_BL(FIO_LIST_NAME, empty)(const FIO_LIST_HEAD *head);

/** Removes a node from the list, Returns NULL if node isn't linked. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, remove)(FIO_LIST_TYPE_PTR node);

/** Pushes an existing node to the end of the list. Returns node. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME,
                                 push)(FIO_LIST_HEAD *restrict head,
                                       FIO_LIST_TYPE_PTR restrict node);

/** Pops a node from the end of the list. Returns NULL if list is empty. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, pop)(FIO_LIST_HEAD *head);

/** Adds an existing node to the beginning of the list. Returns node. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME,
                                 unshift)(FIO_LIST_HEAD *restrict head,
                                          FIO_LIST_TYPE_PTR restrict node);

/** Removed a node from the start of the list. Returns NULL if list is empty. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, shift)(FIO_LIST_HEAD *head);

/** Returns a pointer to a list's element, from a pointer to a node. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, root)(FIO_LIST_HEAD *ptr);

/* *****************************************************************************
Linked Lists (embeded) - Implementation
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/** Returns a non-zero value if there are any linked nodes in the list. */
IFUNC int FIO_NAME(FIO_LIST_NAME, any)(const FIO_LIST_HEAD *head) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, 0);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  return head->next != head;
}

/** Returns a non-zero value if the list is empty. */
IFUNC int FIO_NAME_BL(FIO_LIST_NAME, empty)(const FIO_LIST_HEAD *head) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, 0);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  return head->next == head;
}

/** Removes a node from the list, always returning the node. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME,
                                 remove)(FIO_LIST_TYPE_PTR node_) {
  FIO_PTR_TAG_VALID_OR_RETURN(node_, (FIO_LIST_TYPE_PTR)0);
  FIO_LIST_TYPE *node = (FIO_LIST_TYPE *)(FIO_PTR_UNTAG(node_));
  if (node->FIO_LIST_NODE_NAME.next == &node->FIO_LIST_NODE_NAME)
    return NULL;
  node->FIO_LIST_NODE_NAME.prev->next = node->FIO_LIST_NODE_NAME.next;
  node->FIO_LIST_NODE_NAME.next->prev = node->FIO_LIST_NODE_NAME.prev;
  node->FIO_LIST_NODE_NAME.next = node->FIO_LIST_NODE_NAME.prev =
      &node->FIO_LIST_NODE_NAME;
  return node_;
}

/** Pushes an existing node to the end of the list. Returns node or NULL. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME,
                                 push)(FIO_LIST_HEAD *restrict head,
                                       FIO_LIST_TYPE_PTR restrict node_) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, (FIO_LIST_TYPE_PTR)NULL);
  FIO_PTR_TAG_VALID_OR_RETURN(node_, (FIO_LIST_TYPE_PTR)NULL);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  FIO_LIST_TYPE *restrict node = (FIO_LIST_TYPE *)(FIO_PTR_UNTAG(node_));
  node->FIO_LIST_NODE_NAME.prev = head->prev;
  node->FIO_LIST_NODE_NAME.next = head;
  head->prev->next = &node->FIO_LIST_NODE_NAME;
  head->prev = &node->FIO_LIST_NODE_NAME;
  return node_;
}

/** Pops a node from the end of the list. Returns NULL if list is empty. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, pop)(FIO_LIST_HEAD *head) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, (FIO_LIST_TYPE_PTR)NULL);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  return FIO_NAME(FIO_LIST_NAME, remove)(
      FIO_PTR_FROM_FIELD(FIO_LIST_TYPE, FIO_LIST_NODE_NAME, head->prev));
}

/** Adds an existing node to the beginning of the list. Returns node or NULL. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME,
                                 unshift)(FIO_LIST_HEAD *restrict head,
                                          FIO_LIST_TYPE_PTR restrict node) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, (FIO_LIST_TYPE_PTR)NULL);
  FIO_PTR_TAG_VALID_OR_RETURN(node, (FIO_LIST_TYPE_PTR)NULL);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  return FIO_NAME(FIO_LIST_NAME, push)(head->next, node);
}

/** Removed a node from the start of the list. Returns NULL if list is empty. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, shift)(FIO_LIST_HEAD *head) {
  FIO_PTR_TAG_VALID_OR_RETURN(head, (FIO_LIST_TYPE_PTR)NULL);
  head = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(head));
  return FIO_NAME(FIO_LIST_NAME, remove)(
      FIO_PTR_FROM_FIELD(FIO_LIST_TYPE, FIO_LIST_NODE_NAME, head->next));
}

/** Removed a node from the start of the list. Returns NULL if list is empty. */
IFUNC FIO_LIST_TYPE_PTR FIO_NAME(FIO_LIST_NAME, root)(FIO_LIST_HEAD *ptr) {
  FIO_PTR_TAG_VALID_OR_RETURN(ptr, (FIO_LIST_TYPE_PTR)NULL);
  ptr = (FIO_LIST_HEAD *)(FIO_PTR_UNTAG(ptr));
  return FIO_PTR_FROM_FIELD(FIO_LIST_TYPE, FIO_LIST_NODE_NAME, ptr);
}

/* *****************************************************************************
Linked Lists (embeded) - cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_LIST_NAME
#undef FIO_LIST_TYPE
#undef FIO_LIST_NODE_NAME
#undef FIO_LIST_TYPE_PTR
#endif
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_ARRAY_NAME ary          /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#define FIO_TEST_CSTL               /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                            Dynamic Arrays




Example:

```c
typedef struct {
  int i;
  float f;
} foo_s;

#define FIO_ARRAY_NAME ary
#define FIO_ARRAY_TYPE foo_s
#define FIO_ARRAY_TYPE_CMP(a,b) (a.i == b.i && a.f == b.f)
#include "fio_cstl.h"

void example(void) {
  ary_s a = FIO_ARRAY_INIT;
  foo_s *p = ary_push(&a, (foo_s){.i = 42});
  FIO_ARRAY_EACH(ary, &a, pos) { // pos will be a pointer to the element
    fprintf(stderr, "* [%zu]: %p : %d\n", (size_t)(pos - ary2ptr(&a)), pos->i);
  }
  ary_destroy(&a);
}
```

***************************************************************************** */

#ifdef FIO_ARRAY_NAME

#ifndef FIO_ARRAY_TYPE
/** The type for array elements (an array of FIO_ARRAY_TYPE) */
#define FIO_ARRAY_TYPE void *
/** An invalid value for that type (if any). */
#define FIO_ARRAY_TYPE_INVALID        NULL
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1
#else
#ifndef FIO_ARRAY_TYPE_INVALID
/** An invalid value for that type (if any). */
#define FIO_ARRAY_TYPE_INVALID        ((FIO_ARRAY_TYPE){0})
/* internal flag - do not set */
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 1
#endif
#endif

#ifndef FIO_ARRAY_TYPE_INVALID_SIMPLE
/** Is the FIO_ARRAY_TYPE_INVALID object memory is all zero? (yes = 1) */
#define FIO_ARRAY_TYPE_INVALID_SIMPLE 0
#endif

#ifndef FIO_ARRAY_TYPE_COPY
/** Handles a copy operation for an array's element. */
#define FIO_ARRAY_TYPE_COPY(dest, src) (dest) = (src)
/* internal flag - do not set */
#define FIO_ARRAY_TYPE_COPY_SIMPLE 1
#endif

#ifndef FIO_ARRAY_TYPE_DESTROY
/** Handles a destroy / free operation for an array's element. */
#define FIO_ARRAY_TYPE_DESTROY(obj)
/* internal flag - do not set */
#define FIO_ARRAY_TYPE_DESTROY_SIMPLE 1
#endif

#ifndef FIO_ARRAY_TYPE_CMP
/** Handles a comparison operation for an array's element. */
#define FIO_ARRAY_TYPE_CMP(a, b) (a) == (b)
/* internal flag - do not set */
#define FIO_ARRAY_TYPE_CMP_SIMPLE 1
#endif

#ifndef FIO_ARRAY_TYPE_CONCAT_COPY
#define FIO_ARRAY_TYPE_CONCAT_COPY        FIO_ARRAY_TYPE_COPY
#define FIO_ARRAY_TYPE_CONCAT_COPY_SIMPLE FIO_ARRAY_TYPE_COPY_SIMPLE
#endif
/**
 * The FIO_ARRAY_DESTROY_AFTER_COPY macro should be set if
 * FIO_ARRAY_TYPE_DESTROY should be called after FIO_ARRAY_TYPE_COPY when an
 * object is removed from the array after being copied to an external container
 * (an `old` pointer)
 */
#ifndef FIO_ARRAY_DESTROY_AFTER_COPY
#if !FIO_ARRAY_TYPE_DESTROY_SIMPLE && !FIO_ARRAY_TYPE_COPY_SIMPLE
#define FIO_ARRAY_DESTROY_AFTER_COPY 1
#else
#define FIO_ARRAY_DESTROY_AFTER_COPY 0
#endif
#endif

/* Extra empty slots when allocating memory. */
#ifndef FIO_ARRAY_PADDING
#define FIO_ARRAY_PADDING 4
#endif

/*
 * Uses the array structure to embed object, if there's sppace for them.
 *
 * This optimizes small arrays and specifically touplets. For `void *` type
 * arrays this allows for 2 objects to be embedded, resulting in faster access
 * due to cache locality and reduced pointer redirection.
 *
 * For large arrays, it is better to disable this feature.
 *
 * Note: alues larger than 1 add a memory allocation cost to the array
 * container, adding enough room for at least `FIO_ARRAY_ENABLE_EMBEDDED - 1`
 * items.
 */
#ifndef FIO_ARRAY_ENABLE_EMBEDDED
#define FIO_ARRAY_ENABLE_EMBEDDED 1
#endif

/* Sets memory growth to exponentially increase. Consumes more memory. */
#ifndef FIO_ARRAY_EXPONENTIAL
#define FIO_ARRAY_EXPONENTIAL 0
#endif

#undef FIO_ARRAY_SIZE2WORDS
#define FIO_ARRAY_SIZE2WORDS(size)                                             \
  ((sizeof(FIO_ARRAY_TYPE) & 1)                                                \
       ? (((size) & (~15)) + 16)                                               \
       : (sizeof(FIO_ARRAY_TYPE) & 2)                                          \
             ? (((size) & (~7)) + 8)                                           \
             : (sizeof(FIO_ARRAY_TYPE) & 4)                                    \
                   ? (((size) & (~3)) + 4)                                     \
                   : (sizeof(FIO_ARRAY_TYPE) & 8) ? (((size) & (~1)) + 2)      \
                                                  : (size))

/* *****************************************************************************
Dynamic Arrays - type
***************************************************************************** */

typedef struct {
  /* start common header */
  /** the offser to the first item. */
  uint32_t start;
  /** The offset to the first empty location the array. */
  uint32_t end;
  /* end common header */
  /** The attay's capacity only 32bits are valid */
  uintptr_t capa;
  /** a pointer to the array's memory (if not embedded) */
  FIO_ARRAY_TYPE *ary;
#if FIO_ARRAY_ENABLE_EMBEDDED > 1
  /** Do we wanted larger small-array optimizations? */
  FIO_ARRAY_TYPE
  extra_memory_for_embedded_arrays[(FIO_ARRAY_ENABLE_EMBEDDED - 1)]
#endif
} FIO_NAME(FIO_ARRAY_NAME, s);

#ifdef FIO_PTR_TAG_TYPE
#define FIO_ARRAY_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, s) *
#endif

/* *****************************************************************************
Dynamic Arrays - API
***************************************************************************** */

#ifndef FIO_ARRAY_INIT
/* Initialization macro. */
#define FIO_ARRAY_INIT                                                         \
  { 0 }
#endif

#ifndef FIO_REF_CONSTRUCTOR_ONLY

/* Allocates a new array object on the heap and initializes it's memory. */
FIO_IFUNC FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, new)(void);

/* Frees an array's internal data AND it's container! */
FIO_IFUNC void FIO_NAME(FIO_ARRAY_NAME, free)(FIO_ARRAY_PTR ary);

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/* Destroys any objects stored in the array and frees the internal state. */
SFUNC void FIO_NAME(FIO_ARRAY_NAME, destroy)(FIO_ARRAY_PTR ary);

/** Returns the number of elements in the Array. */
FIO_IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, count)(FIO_ARRAY_PTR ary);

/** Returns the current, temporary, array capacity (it's dynamic). */
FIO_IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, capa)(FIO_ARRAY_PTR ary);

/**
 * Returns 1 if the array is embedded, 0 if it has memory allocated and -1 on an
 * error.
 */
FIO_IFUNC int FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(FIO_ARRAY_PTR ary);

/**
 * Returns a pointer to the C array containing the objects.
 */
FIO_IFUNC FIO_ARRAY_TYPE *FIO_NAME2(FIO_ARRAY_NAME, ptr)(FIO_ARRAY_PTR ary);

/**
 * Reserves a minimal capacity for the array.
 *
 * If `capa` is negative, new memory will be allocated at the beginning of the
 * array rather then it's end.
 *
 * Returns the array's new capacity.
 *
 * Note: the reserved capacity includes existing data. If the requested reserved
 * capacity is equal (or less) then the existing capacity, nothing will be done.
 */
SFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, reserve)(FIO_ARRAY_PTR ary,
                                                 int32_t capa);

/**
 * Adds all the items in the `src` Array to the end of the `dest` Array.
 *
 * The `src` Array remain untouched.
 *
 * Always returns the destination array (`dest`).
 */
SFUNC FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, concat)(FIO_ARRAY_PTR dest,
                                                     FIO_ARRAY_PTR src);

/**
 * Sets `index` to the value in `data`.
 *
 * If `index` is negative, it will be counted from the end of the Array (-1 ==
 * last element).
 *
 * If `old` isn't NULL, the existing data will be copied to the location pointed
 * to by `old` before the copy in the Array is destroyed.
 *
 * Returns a pointer to the new object, or NULL on error.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, set)(FIO_ARRAY_PTR ary,
                                                    int32_t index,
                                                    FIO_ARRAY_TYPE data,
                                                    FIO_ARRAY_TYPE *old);

/**
 * Returns the value located at `index` (no copying is performed).
 *
 * If `index` is negative, it will be counted from the end of the Array (-1 ==
 * last element).
 */
FIO_IFUNC FIO_ARRAY_TYPE FIO_NAME(FIO_ARRAY_NAME, get)(FIO_ARRAY_PTR ary,
                                                       int32_t index);

/**
 * Returns the index of the object or -1 if the object wasn't found.
 *
 * If `start_at` is negative (i.e., -1), than seeking will be performed in
 * reverse, where -1 == last index (-2 == second to last, etc').
 */
SFUNC int32_t FIO_NAME(FIO_ARRAY_NAME, find)(FIO_ARRAY_PTR ary,
                                             FIO_ARRAY_TYPE data,
                                             int32_t start_at);

/**
 * Removes an object from the array, MOVING all the other objects to prevent
 * "holes" in the data.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns 0 on success and -1 on error.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, remove)(FIO_ARRAY_PTR ary,
                                           int32_t index,
                                           FIO_ARRAY_TYPE *old);

/**
 * Removes all occurrences of an object from the array (if any), MOVING all the
 * existing objects to prevent "holes" in the data.
 *
 * Returns the number of items removed.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
SFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, remove2)(FIO_ARRAY_PTR ary,
                                                 FIO_ARRAY_TYPE data);

/** Attempts to lower the array's memory consumption. */
SFUNC void FIO_NAME(FIO_ARRAY_NAME, compact)(FIO_ARRAY_PTR ary);

/**
 * Pushes an object to the end of the Array. Returns a pointer to the new object
 * or NULL on error.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, push)(FIO_ARRAY_PTR ary,
                                                     FIO_ARRAY_TYPE data);

/**
 * Removes an object from the end of the Array.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns -1 on error (Array is empty) and 0 on success.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, pop)(FIO_ARRAY_PTR ary, FIO_ARRAY_TYPE *old);

/**
 * Unshifts an object to the beginning of the Array. Returns a pointer to the
 * new object or NULL on error.
 *
 * This could be expensive, causing `memmove`.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, unshift)(FIO_ARRAY_PTR ary,
                                                        FIO_ARRAY_TYPE data);

/**
 * Removes an object from the beginning of the Array.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns -1 on error (Array is empty) and 0 on success.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, shift)(FIO_ARRAY_PTR ary,
                                          FIO_ARRAY_TYPE *old);

/** Iteration information structure passed to the callback. */
typedef struct FIO_NAME(FIO_ARRAY_NAME, each_s) {
  /** The being iterated. Once set, cannot be safely changed. */
  FIO_ARRAY_PTR const parent;
  /** The current object's index */
  uint64_t index;
  /** Always 1, but may be used to allow type detection. */
  const int64_t items_at_index;
  /** The callback / task called for each index, may be updated mid-cycle. */
  int (*task)(struct FIO_NAME(FIO_ARRAY_NAME, each_s) * info);
  /** Opaque user data. */
  void *udata;
  /** The object / value at the current index. */
  FIO_ARRAY_TYPE value;
} FIO_NAME(FIO_ARRAY_NAME, each_s);

/**
 * Iteration using a callback for each entry in the array.
 *
 * The callback task function must accept an each_s pointer, see above.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME,
                        each)(FIO_ARRAY_PTR ary,
                              int (*task)(FIO_NAME(FIO_ARRAY_NAME, each_s) *
                                          info),
                              void *udata,
                              int32_t start_at);

#ifndef FIO_ARRAY_EACH
/**
 * Iterates through the array using a `for` loop.
 *
 * Access the object with the pointer `pos`. The `pos` variable can be named
 * however you please.
 *
 * Avoid editing the array during a FOR loop, although I hope it's possible, I
 * wouldn't count on it.
 *
 * **Note**: this variant supports automatic pointer tagging / untagging.
 */
#define FIO_ARRAY_EACH(array_name, array, pos)                                 \
  for (FIO_NAME(FIO_ARRAY_NAME,                                                \
                ____type_t) *first___ = NULL,                                  \
                            *pos =                                             \
                                FIO_NAME(array_name,                           \
                                         each_next)((array), &first___, NULL); \
       pos;                                                                    \
       pos = FIO_NAME(array_name, each_next)((array), &first___, pos))
#endif

/**
 * Returns a pointer to the (next) object in the array.
 *
 * Returns a pointer to the first object if `pos == NULL` and there are objects
 * in the array.
 *
 * The first pointer is automatically set and it allows object insertions and
 * memory effecting functions to be called from within the loop.
 *
 * If the object in `pos` (or an object before it) were removed, consider
 * passing `pos-1` to the function, to avoid skipping any elements while
 * looping.
 *
 * Returns the next object if both `first` and `pos` are valid.
 *
 * Returns NULL if `pos` was the last object or no object exist.
 *
 * Returns the first object if either `first` or `pos` are invalid.
 *
 */
FIO_IFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME,
                                   each_next)(FIO_ARRAY_PTR ary,
                                              FIO_ARRAY_TYPE **first,
                                              FIO_ARRAY_TYPE *pos);

/* *****************************************************************************
Dynamic Arrays - embedded arrays
***************************************************************************** */
#if FIO_ARRAY_ENABLE_EMBEDDED
#define FIO_ARRAY_IS_EMBEDDED(a)                                               \
  (sizeof(FIO_ARRAY_TYPE) <= sizeof(void *) &&                                 \
   (((a)->start > (a)->end) || !(a)->ary))
#define FIO_ARRAY_IS_EMBEDDED_PTR(ary, ptr)                                    \
  (sizeof(FIO_ARRAY_TYPE) <= sizeof(void *) &&                                 \
   (uintptr_t)(ptr) > (uintptr_t)(ary) &&                                      \
   (uintptr_t)(ptr) < (uintptr_t)((ary) + 1))
#define FIO_ARRAY_EMBEDDED_CAPA                                                \
  (sizeof(FIO_ARRAY_TYPE) > sizeof(void *)                                     \
       ? 0                                                                     \
       : ((sizeof(FIO_NAME(FIO_ARRAY_NAME, s)) -                               \
           sizeof(FIO_NAME(FIO_ARRAY_NAME, ___embedded_s))) /                  \
          sizeof(FIO_ARRAY_TYPE)))

#else
#define FIO_ARRAY_IS_EMBEDDED(a)            0
#define FIO_ARRAY_IS_EMBEDDED_PTR(ary, ptr) 0
#define FIO_ARRAY_EMBEDDED_CAPA             0

#endif /* FIO_ARRAY_ENABLE_EMBEDDED */

typedef struct {
  /* start common header */
  /** the offser to the first item. */
  uint32_t start;
  /** The offset to the first empty location the array. */
  uint32_t end;
  /* end common header */
  FIO_ARRAY_TYPE embedded[];
} FIO_NAME(FIO_ARRAY_NAME, ___embedded_s);

#define FIO_ARRAY2EMBEDDED(a) ((FIO_NAME(FIO_ARRAY_NAME, ___embedded_s) *)(a))

/* *****************************************************************************
Inlined functions
***************************************************************************** */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new array object on the heap and initializes it's memory. */
FIO_IFUNC FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, new)(void) {
  FIO_NAME(FIO_ARRAY_NAME, s) *a =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*a), 0);
  if (!FIO_MEM_REALLOC_IS_SAFE_ && a) {
    *a = (FIO_NAME(FIO_ARRAY_NAME, s))FIO_ARRAY_INIT;
  }
  return (FIO_ARRAY_PTR)FIO_PTR_TAG(a);
}

/* Frees an array's internal data AND it's container! */
FIO_IFUNC void FIO_NAME(FIO_ARRAY_NAME, free)(FIO_ARRAY_PTR ary_) {
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(ary_);
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  FIO_NAME(FIO_ARRAY_NAME, destroy)(ary_);
  FIO_MEM_FREE_(ary, sizeof(*ary));
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Returns the number of elements in the Array. */
FIO_IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, count)(FIO_ARRAY_PTR ary_) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    return ary->end - ary->start;
  case 1:
    return ary->start;
  }
  return 0;
}

/** Returns the current, temporary, array capacity (it's dynamic). */
FIO_IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, capa)(FIO_ARRAY_PTR ary_) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    return ary->capa;
  case 1:
    return FIO_ARRAY_EMBEDDED_CAPA;
  }
  return 0;
}

/**
 * Returns a pointer to the C array containing the objects.
 */
FIO_IFUNC FIO_ARRAY_TYPE *FIO_NAME2(FIO_ARRAY_NAME, ptr)(FIO_ARRAY_PTR ary_) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    return ary->ary + ary->start;
  case 1:
    return FIO_ARRAY2EMBEDDED(ary)->embedded;
  }
  return NULL;
}

/**
 * Returns 1 if the array is embedded, 0 if it has memory allocated and -1 on an
 * error.
 */
FIO_IFUNC int FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(FIO_ARRAY_PTR ary_) {
  FIO_PTR_TAG_VALID_OR_RETURN(ary_, -1);
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  return FIO_ARRAY_IS_EMBEDDED(ary);
  (void)ary; /* if unused (never embedded) */
}

/**
 * Returns the value located at `index` (no copying is performed).
 *
 * If `index` is negative, it will be counted from the end of the Array (-1 ==
 * last element).
 */
FIO_IFUNC FIO_ARRAY_TYPE FIO_NAME(FIO_ARRAY_NAME, get)(FIO_ARRAY_PTR ary_,
                                                       int32_t index) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  FIO_ARRAY_TYPE *a;
  size_t count;
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    a = ary->ary + ary->start;
    count = ary->end - ary->start;
    break;
  case 1:
    a = FIO_ARRAY2EMBEDDED(ary)->embedded;
    count = ary->start;
    break;
  default:
    return FIO_ARRAY_TYPE_INVALID;
  }

  if (index < 0) {
    index += count;
    if (index < 0)
      return FIO_ARRAY_TYPE_INVALID;
  }
  if ((uint32_t)index >= count)
    return FIO_ARRAY_TYPE_INVALID;
  return a[index];
}

/* Returns a pointer to the (next) object in the array. */
FIO_IFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME,
                                   each_next)(FIO_ARRAY_PTR ary_,
                                              FIO_ARRAY_TYPE **first,
                                              FIO_ARRAY_TYPE *pos) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  int32_t count;
  FIO_ARRAY_TYPE *a;
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    count = ary->end - ary->start;
    a = ary->ary + ary->start;
    break;
  case 1:
    count = ary->start;
    a = FIO_ARRAY2EMBEDDED(ary)->embedded;
    break;
  default:
    return NULL;
  }
  intptr_t i;
  if (!count || !first)
    return NULL;
  if (!pos || !(*first) || (*first) > pos) {
    i = -1;
  } else {
    i = (intptr_t)(pos - (*first));
  }
  *first = a;
  ++i;
  if (i >= count)
    return NULL;
  return i + a;
}

/** Used internally for the EACH macro */
typedef FIO_ARRAY_TYPE FIO_NAME(FIO_ARRAY_NAME, ____type_t);

/* *****************************************************************************
Exported functions
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE
/* *****************************************************************************
Helper macros
***************************************************************************** */
#if FIO_ARRAY_EXPONENTIAL
#define FIO_ARRAY_ADD2CAPA(capa) (((capa) << 1) + FIO_ARRAY_PADDING)
#else
#define FIO_ARRAY_ADD2CAPA(capa) ((capa) + FIO_ARRAY_PADDING)
#endif

/* *****************************************************************************
Dynamic Arrays - internal helpers
***************************************************************************** */

#define FIO_ARRAY_POS2ABS(ary, pos)                                            \
  (pos >= 0 ? (ary->start + pos) : (ary->end - pos))

#define FIO_ARRAY_AB_CT(cond, a, b) ((b) ^ ((0 - ((cond)&1)) & ((a) ^ (b))))

/* *****************************************************************************
Dynamic Arrays - implementation
***************************************************************************** */

/* Destroys any objects stored in the array and frees the internal state. */
SFUNC void FIO_NAME(FIO_ARRAY_NAME, destroy)(FIO_ARRAY_PTR ary_) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  union {
    FIO_NAME(FIO_ARRAY_NAME, s) a;
    FIO_NAME(FIO_ARRAY_NAME, ___embedded_s) e;
  } tmp = {.a = *ary};
  *ary = (FIO_NAME(FIO_ARRAY_NAME, s))FIO_ARRAY_INIT;

  switch (
      FIO_NAME_BL(FIO_ARRAY_NAME, embedded)((FIO_ARRAY_PTR)FIO_PTR_TAG(&tmp))) {
  case 0:
#if !FIO_ARRAY_TYPE_DESTROY_SIMPLE
    for (size_t i = tmp.a.start; i < tmp.a.end; ++i) {
      FIO_ARRAY_TYPE_DESTROY(tmp.a.ary[i]);
    }
#endif
    FIO_MEM_FREE_(tmp.a.ary, tmp.a.capa * sizeof(*tmp.a.ary));
    return;
  case 1:
#if !FIO_ARRAY_TYPE_DESTROY_SIMPLE
    while (tmp.e.start--) {
      FIO_ARRAY_TYPE_DESTROY((tmp.e.embedded[tmp.e.start]));
    }
#endif
    return;
  }
  return;
}

/** Reserves a minimal capacity for the array. */
SFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, reserve)(FIO_ARRAY_PTR ary_,
                                                 int32_t capa_) {
  const uint32_t abs_capa =
      (capa_ >= 0) ? (uint32_t)capa_ : (uint32_t)(0 - capa_);
  const uint32_t capa = FIO_ARRAY_SIZE2WORDS(abs_capa);
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  FIO_ARRAY_TYPE *tmp;
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    if (abs_capa <= ary->capa)
      return ary->capa;
    /* objects don't move, use the system's realloc */
    if ((capa_ >= 0) || (capa_ < 0 && ary->start > 0)) {
      tmp = (FIO_ARRAY_TYPE *)FIO_MEM_REALLOC_(ary->ary,
                                               0,
                                               sizeof(*tmp) * capa,
                                               sizeof(*tmp) * ary->end);
      if (!tmp)
        return ary->capa;
      ary->capa = capa;
      ary->ary = tmp;
      return capa;
    } else {
      /* moving objects, starting with a fresh piece of memory */
      tmp = (FIO_ARRAY_TYPE *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*tmp) * capa, 0);
      const uint32_t count = ary->end - ary->start;
      if (!tmp)
        return ary->capa;
      if (capa_ >= 0) {
        /* copy items at begining of memory stack */
        if (count) {
          FIO_MEMCPY(tmp, ary->ary + ary->start, count * sizeof(*tmp));
        }
        FIO_MEM_FREE_(ary->ary, sizeof(*ary->ary) * ary->capa);
        *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
            .start = 0,
            .end = count,
            .capa = capa,
            .ary = tmp,
        };
        return capa;
      }
      /* copy items at ending of memory stack */
      if (count) {
        FIO_MEMCPY(tmp + (capa - count),
                   ary->ary + ary->start,
                   count * sizeof(*tmp));
      }
      FIO_MEM_FREE_(ary->ary, sizeof(*ary->ary) * ary->capa);
      *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
          .start = (capa - count),
          .end = capa,
          .capa = capa,
          .ary = tmp,
      };
    }
    return capa;
  case 1:
    if (abs_capa <= FIO_ARRAY_EMBEDDED_CAPA)
      return FIO_ARRAY_EMBEDDED_CAPA;
    tmp = (FIO_ARRAY_TYPE *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*tmp) * capa, 0);
    if (!tmp)
      return FIO_ARRAY_EMBEDDED_CAPA;
    if (capa_ >= 0) {
      /* copy items at begining of memory stack */
      if (ary->start) {
        FIO_MEMCPY(tmp,
                   FIO_ARRAY2EMBEDDED(ary)->embedded,
                   ary->start * sizeof(*tmp));
      }
      *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
          .start = 0,
          .end = ary->start,
          .capa = capa,
          .ary = tmp,
      };
      return capa;
    }
    /* copy items at ending of memory stack */
    if (ary->start) {
      FIO_MEMCPY(tmp + (capa - ary->start),
                 FIO_ARRAY2EMBEDDED(ary)->embedded,
                 ary->start * sizeof(*tmp));
    }
    *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
        .start = (capa - ary->start),
        .end = capa,
        .capa = capa,
        .ary = tmp,
    };
    return capa;
  default:
    return 0;
  }
}

/**
 * Adds all the items in the `src` Array to the end of the `dest` Array.
 *
 * The `src` Array remain untouched.
 *
 * Returns `dest` on success or NULL on error (i.e., no memory).
 */
SFUNC FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, concat)(FIO_ARRAY_PTR dest_,
                                                     FIO_ARRAY_PTR src_) {
  FIO_PTR_TAG_VALID_OR_RETURN(dest_, (FIO_ARRAY_PTR)NULL);
  FIO_PTR_TAG_VALID_OR_RETURN(src_, (FIO_ARRAY_PTR)NULL);
  FIO_NAME(FIO_ARRAY_NAME, s) *dest =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(dest_));
  FIO_NAME(FIO_ARRAY_NAME, s) *src =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(src_));
  if (!dest || !src)
    return dest_;
  const uint32_t offset = FIO_NAME(FIO_ARRAY_NAME, count)(dest_);
  const uint32_t added = FIO_NAME(FIO_ARRAY_NAME, count)(src_);
  const uint32_t total = offset + added;
  if (!added)
    return dest_;

  if (total < offset || total + offset < total)
    return NULL; /* item count overflow */

  const uint32_t capa = FIO_NAME(FIO_ARRAY_NAME, reserve)(dest_, total);

  if (!FIO_ARRAY_IS_EMBEDDED(dest) && dest->start + total > capa) {
    /* we need to move the existing items due to the offset */
    memmove(dest->ary,
            dest->ary + dest->start,
            (dest->end - dest->start) * sizeof(*dest->ary));
    dest->start = 0;
    dest->end = offset;
  }
#if FIO_ARRAY_TYPE_CONCAT_COPY_SIMPLE
  /* copy data */
  FIO_MEMCPY(FIO_NAME2(FIO_ARRAY_NAME, ptr)(dest_) + offset,
             FIO_NAME2(FIO_ARRAY_NAME, ptr)(src_),
             added);
#else
  {
    FIO_ARRAY_TYPE *const a1 = FIO_NAME2(FIO_ARRAY_NAME, ptr)(dest_);
    FIO_ARRAY_TYPE *const a2 = FIO_NAME2(FIO_ARRAY_NAME, ptr)(src_);
    for (uint32_t i = 0; i < added; ++i) {
      FIO_ARRAY_TYPE_CONCAT_COPY(a1[i + offset], a2[i]);
    }
  }
#endif /* FIO_ARRAY_TYPE_CONCAT_COPY_SIMPLE */
  /* update dest */
  if (!FIO_ARRAY_IS_EMBEDDED(dest)) {
    dest->end += added;
    return dest_;
  } else
    dest->start = total;
  return dest_;
}

/**
 * Sets `index` to the value in `data`.
 *
 * If `index` is negative, it will be counted from the end of the Array (-1 ==
 * last element).
 *
 * If `old` isn't NULL, the existing data will be copied to the location pointed
 * to by `old` before the copy in the Array is destroyed.
 *
 * Returns a pointer to the new object, or NULL on error.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, set)(FIO_ARRAY_PTR ary_,
                                                    int32_t index,
                                                    FIO_ARRAY_TYPE data,
                                                    FIO_ARRAY_TYPE *old) {
  FIO_NAME(FIO_ARRAY_NAME, s) * ary;
  FIO_ARRAY_TYPE *a;
  uint32_t count;
  uint8_t pre_existing = 1;

  FIO_PTR_TAG_VALID_OR_GOTO(ary_, invalid);

  ary = (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);

  if (index < 0) {
    index += count;
    if (index < 0)
      goto negative_expansion;
  }

  if ((uint32_t)index >= count) {
    if ((uint32_t)index == count)
      FIO_NAME(FIO_ARRAY_NAME, reserve)(ary_, FIO_ARRAY_ADD2CAPA(index));
    else
      FIO_NAME(FIO_ARRAY_NAME, reserve)(ary_, (uint32_t)index + 1);
    if (FIO_ARRAY_IS_EMBEDDED(ary))
      goto expand_embedded;
    goto expansion;
  }

  a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);

done:

  /* copy / clear object */
  if (pre_existing) {
    if (old) {
      FIO_ARRAY_TYPE_COPY(old[0], a[index]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
      FIO_ARRAY_TYPE_DESTROY(a[index]);
#endif
    } else {
      FIO_ARRAY_TYPE_DESTROY(a[index]);
    }
  } else if (old) {
    FIO_ARRAY_TYPE_COPY(old[0], FIO_ARRAY_TYPE_INVALID);
  }
  FIO_ARRAY_TYPE_COPY(a[index], FIO_ARRAY_TYPE_INVALID);
  FIO_ARRAY_TYPE_COPY(a[index], data);
  return a + index;

expansion:

  pre_existing = 0;
  a = ary->ary;
  {
    uint8_t was_moved = 0;
    /* test if we need to move objects to make room at the end */
    if (ary->start + index >= ary->capa) {
      memmove(ary->ary, ary->ary + ary->start, (count) * sizeof(*ary->ary));
      ary->start = 0;
      ary->end = index + 1;
      was_moved = 1;
    }
    /* initialize memory in between objects */
    if (was_moved || !FIO_MEM_REALLOC_IS_SAFE_ ||
        !FIO_ARRAY_TYPE_INVALID_SIMPLE) {
#if FIO_ARRAY_TYPE_INVALID_SIMPLE
      memset(a + count, 0, (index - count) * sizeof(*ary->ary));
#else
      for (size_t i = count; i <= (size_t)index; ++i) {
        FIO_ARRAY_TYPE_COPY(a[i], FIO_ARRAY_TYPE_INVALID);
      }
#endif
    }
    ary->end = index + 1;
  }
  goto done;

expand_embedded:
  pre_existing = 0;
  ary->start = index + 1;
  a = FIO_ARRAY2EMBEDDED(ary)->embedded;
  goto done;

negative_expansion:
  pre_existing = 0;
  FIO_NAME(FIO_ARRAY_NAME, reserve)(ary_, (index - count));
  index = 0 - index;

  if ((FIO_ARRAY_IS_EMBEDDED(ary)))
    goto negative_expansion_embedded;
  a = ary->ary;
  if (index > (int32_t)ary->start) {
    memmove(a + index, a + ary->start, count * sizeof(*a));
    ary->end = index + count;
    ary->start = index;
  }
  index = ary->start - index;
  if ((uint32_t)(index + 1) < ary->start) {
#if FIO_ARRAY_TYPE_INVALID_SIMPLE
    memset(a + index, 0, (ary->start - index) * (sizeof(*a)));
#else
    for (size_t i = index; i < (size_t)ary->start; ++i) {
      FIO_ARRAY_TYPE_COPY(a[i], FIO_ARRAY_TYPE_INVALID);
    }
#endif
  }
  ary->start = index;
  goto done;

negative_expansion_embedded:
  a = FIO_ARRAY2EMBEDDED(ary)->embedded;
  memmove(a + index, a, count * count * sizeof(*a));
#if FIO_ARRAY_TYPE_INVALID_SIMPLE
  memset(a, 0, index * (sizeof(a)));
#else
  for (size_t i = 0; i < (size_t)index; ++i) {
    FIO_ARRAY_TYPE_COPY(a[i], FIO_ARRAY_TYPE_INVALID);
  }
#endif
  index = 0;
  goto done;

invalid:
  FIO_ARRAY_TYPE_DESTROY(data);
  if (old) {
    FIO_ARRAY_TYPE_COPY(old[0], FIO_ARRAY_TYPE_INVALID);
  }

  return NULL;
}

/**
 * Returns the index of the object or -1 if the object wasn't found.
 *
 * If `start_at` is negative (i.e., -1), than seeking will be performed in
 * reverse, where -1 == last index (-2 == second to last, etc').
 */
SFUNC int32_t FIO_NAME(FIO_ARRAY_NAME, find)(FIO_ARRAY_PTR ary_,
                                             FIO_ARRAY_TYPE data,
                                             int32_t start_at) {
  FIO_ARRAY_TYPE *a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);
  if (!a)
    return -1;
  size_t count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);
  if (start_at >= 0) {
    /* seek forwards */
    if ((uint32_t)start_at >= count)
      start_at = count;
    while ((uint32_t)start_at < count) {
      if (FIO_ARRAY_TYPE_CMP(a[start_at], data))
        return start_at;
      ++start_at;
    }
  } else {
    /* seek backwards */
    if (start_at + (int32_t)count < 0)
      return -1;
    count += start_at;
    count += 1;
    while (count--) {
      if (FIO_ARRAY_TYPE_CMP(a[count], data))
        return count;
    }
  }
  return -1;
}

/**
 * Removes an object from the array, MOVING all the other objects to prevent
 * "holes" in the data.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns 0 on success and -1 on error.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, remove)(FIO_ARRAY_PTR ary_,
                                           int32_t index,
                                           FIO_ARRAY_TYPE *old) {
  FIO_ARRAY_TYPE *a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);
  FIO_NAME(FIO_ARRAY_NAME, s) * ary;
  ary = (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  size_t count;
  if (!a)
    goto invalid;
  count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);

  if (index < 0) {
    index += count;
    if (index < 0) {
      FIO_LOG_WARNING(
          FIO_MACRO2STR(FIO_NAME(FIO_ARRAY_NAME,
                                 remove)) " called with a negative index lower "
                                          "than the element count.");
      goto invalid;
    }
  }
  if ((uint32_t)index >= count)
    goto invalid;
  if (!index) {
    FIO_NAME(FIO_ARRAY_NAME, shift)(ary_, old);
    return 0;
  }
  if ((uint32_t)index + 1 == count) {
    FIO_NAME(FIO_ARRAY_NAME, pop)(ary_, old);
    return 0;
  }

  if (old) {
    FIO_ARRAY_TYPE_COPY(*old, a[index]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
    FIO_ARRAY_TYPE_DESTROY(a[index]);
#endif
  } else {
    FIO_ARRAY_TYPE_DESTROY(a[index]);
  }

  if ((uint32_t)(index + 1) < count) {
    memmove(a + index, a + index + 1, (count - (index + 1)) * sizeof(*a));
  }
  FIO_ARRAY_TYPE_COPY((a + (count - 1))[0], FIO_ARRAY_TYPE_INVALID);

  if (FIO_ARRAY_IS_EMBEDDED(ary))
    goto embedded;
  --ary->end;
  return 0;

embedded:
  --ary->start;
  return 0;

invalid:
  if (old) {
    FIO_ARRAY_TYPE_COPY(*old, FIO_ARRAY_TYPE_INVALID);
  }
  return -1;
}

/**
 * Removes all occurrences of an object from the array (if any), MOVING all the
 * existing objects to prevent "holes" in the data.
 *
 * Returns the number of items removed.
 */
SFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME, remove2)(FIO_ARRAY_PTR ary_,
                                                 FIO_ARRAY_TYPE data) {
  FIO_ARRAY_TYPE *a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);
  FIO_NAME(FIO_ARRAY_NAME, s) * ary;
  ary = (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  size_t count;
  if (!a)
    return 0;
  count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);

  size_t c = 0;
  size_t i = 0;
  while ((i + c) < count) {
    if (!(FIO_ARRAY_TYPE_CMP(a[i + c], data))) {
      a[i] = a[i + c];
      ++i;
      continue;
    }
    FIO_ARRAY_TYPE_DESTROY(a[i + c]);
    ++c;
  }
  if (c && FIO_MEM_REALLOC_IS_SAFE_) {
    /* keep memory zeroed out */
    memset(a + i, 0, sizeof(*a) * c);
  }
  if (!FIO_ARRAY_IS_EMBEDDED_PTR(ary, a)) {
    ary->end = ary->start + i;
    return c;
  }
  ary->start = i;
  return c;
}

/** Attempts to lower the array's memory consumption. */
SFUNC void FIO_NAME(FIO_ARRAY_NAME, compact)(FIO_ARRAY_PTR ary_) {
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(ary_);
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  size_t count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);
  FIO_ARRAY_TYPE *tmp = NULL;

  if (count <= FIO_ARRAY_EMBEDDED_CAPA)
    goto re_embed;

  tmp = (FIO_ARRAY_TYPE *)
      FIO_MEM_REALLOC_(NULL, 0, (ary->end - ary->start) * sizeof(*tmp), 0);
  if (!tmp)
    return;
  FIO_MEMCPY(tmp, ary->ary + ary->start, count * sizeof(*ary->ary));
  FIO_MEM_FREE_(ary->ary, ary->capa * sizeof(*ary->ary));
  *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
      .start = 0,
      .end = (ary->end - ary->start),
      .capa = (ary->end - ary->start),
      .ary = tmp,
  };
  return;

re_embed:
  if (!FIO_ARRAY_IS_EMBEDDED(ary)) {
    tmp = ary->ary;
    uint32_t offset = ary->start;
    size_t old_capa = ary->capa;
    *ary = (FIO_NAME(FIO_ARRAY_NAME, s)){
        .start = (uint32_t)count,
    };
    if (count) {
      FIO_MEMCPY(FIO_ARRAY2EMBEDDED(ary)->embedded,
                 tmp + offset,
                 count * sizeof(*tmp));
    }
    if (tmp) {
      FIO_MEM_FREE_(tmp, sizeof(*tmp) * old_capa);
      (void)old_capa; /* if unused */
    }
  }
  return;
}
/**
 * Pushes an object to the end of the Array. Returns NULL on error.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, push)(FIO_ARRAY_PTR ary_,
                                                     FIO_ARRAY_TYPE data) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    if (ary->end == ary->capa) {
      if (!ary->start) {
        if (FIO_NAME(FIO_ARRAY_NAME,
                     reserve)(ary_, FIO_ARRAY_ADD2CAPA(ary->capa)) == ary->end)
          goto invalid;
      } else {
        const uint32_t new_start = (ary->start >> 2);
        const uint32_t count = ary->end - ary->start;
        if (count)
          memmove(ary->ary + new_start,
                  ary->ary + ary->start,
                  count * sizeof(*ary->ary));
        ary->end = count + new_start;
        ary->start = new_start;
      }
    }
    FIO_ARRAY_TYPE_COPY(ary->ary[ary->end], data);
    return ary->ary + (ary->end++);

  case 1:
    if (ary->start == FIO_ARRAY_EMBEDDED_CAPA)
      goto needs_memory_embedded;
    FIO_ARRAY_TYPE_COPY(FIO_ARRAY2EMBEDDED(ary)->embedded[ary->start], data);
    return FIO_ARRAY2EMBEDDED(ary)->embedded + (ary->start++);
  }
invalid:
  FIO_ARRAY_TYPE_DESTROY(data);
  return NULL;

needs_memory_embedded:
  if (FIO_NAME(FIO_ARRAY_NAME,
               reserve)(ary_, FIO_ARRAY_ADD2CAPA(FIO_ARRAY_EMBEDDED_CAPA)) ==
      FIO_ARRAY_EMBEDDED_CAPA)
    goto invalid;
  FIO_ARRAY_TYPE_COPY(ary->ary[ary->end], data);
  return ary->ary + (ary->end++);
}

/**
 * Removes an object from the end of the Array.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns -1 on error (Array is empty) and 0 on success.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, pop)(FIO_ARRAY_PTR ary_,
                                        FIO_ARRAY_TYPE *old) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    if (ary->end == ary->start)
      return -1;
    --ary->end;
    if (old) {
      FIO_ARRAY_TYPE_COPY(*old, ary->ary[ary->end]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
      FIO_ARRAY_TYPE_DESTROY(ary->ary[ary->end]);
#endif
    } else {
      FIO_ARRAY_TYPE_DESTROY(ary->ary[ary->end]);
    }
    return 0;
  case 1:
    if (!ary->start)
      return -1;
    --ary->start;
    if (old) {
      FIO_ARRAY_TYPE_COPY(*old, FIO_ARRAY2EMBEDDED(ary)->embedded[ary->start]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
      FIO_ARRAY_TYPE_DESTROY(FIO_ARRAY2EMBEDDED(ary)->embedded[ary->start]);
#endif
    } else {
      FIO_ARRAY_TYPE_DESTROY(FIO_ARRAY2EMBEDDED(ary)->embedded[ary->start]);
    }
    memset(FIO_ARRAY2EMBEDDED(ary)->embedded + ary->start,
           0,
           sizeof(*ary->ary));
    return 0;
  }
  if (old)
    FIO_ARRAY_TYPE_COPY(old[0], FIO_ARRAY_TYPE_INVALID);
  return -1;
}

/**
 * Unshifts an object to the beginning of the Array. Returns -1 on error.
 *
 * This could be expensive, causing `memmove`.
 */
SFUNC FIO_ARRAY_TYPE *FIO_NAME(FIO_ARRAY_NAME, unshift)(FIO_ARRAY_PTR ary_,
                                                        FIO_ARRAY_TYPE data) {
  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));
  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    if (!ary->start) {
      if (ary->end == ary->capa) {
        FIO_NAME(FIO_ARRAY_NAME, reserve)
        (ary_, (-1 - (int32_t)FIO_ARRAY_ADD2CAPA(ary->capa)));
        if (!ary->start)
          goto invalid;
      } else {
        const uint32_t new_end = ary->capa - ((ary->capa - ary->end) >> 2);
        const uint32_t count = ary->end - ary->start;
        const uint32_t new_start = new_end - count;
        if (count)
          memmove(ary->ary + new_start,
                  ary->ary + ary->start,
                  count * sizeof(*ary->ary));
        ary->end = new_end;
        ary->start = new_start;
      }
    }
    FIO_ARRAY_TYPE_COPY(ary->ary[--ary->start], data);
    return ary->ary + ary->start;

  case 1:
    if (ary->start == FIO_ARRAY_EMBEDDED_CAPA)
      goto needs_memory_embed;
    if (ary->start)
      memmove(FIO_ARRAY2EMBEDDED(ary)->embedded + 1,
              FIO_ARRAY2EMBEDDED(ary)->embedded,
              sizeof(*ary->ary) * ary->start);
    ++ary->start;
    FIO_ARRAY_TYPE_COPY(FIO_ARRAY2EMBEDDED(ary)->embedded[0], data);
    return FIO_ARRAY2EMBEDDED(ary)->embedded;
  }
invalid:
  FIO_ARRAY_TYPE_DESTROY(data);
  return NULL;

needs_memory_embed:
  if (FIO_NAME(FIO_ARRAY_NAME, reserve)(
          ary_,
          (-1 - (int32_t)FIO_ARRAY_ADD2CAPA(FIO_ARRAY_EMBEDDED_CAPA))) ==
      FIO_ARRAY_EMBEDDED_CAPA)
    goto invalid;
  FIO_ARRAY_TYPE_COPY(ary->ary[--ary->start], data);
  return ary->ary + ary->start;
}

/**
 * Removes an object from the beginning of the Array.
 *
 * If `old` is set, the data is copied to the location pointed to by `old`
 * before the data in the array is destroyed.
 *
 * Returns -1 on error (Array is empty) and 0 on success.
 */
SFUNC int FIO_NAME(FIO_ARRAY_NAME, shift)(FIO_ARRAY_PTR ary_,
                                          FIO_ARRAY_TYPE *old) {

  FIO_NAME(FIO_ARRAY_NAME, s) *ary =
      (FIO_NAME(FIO_ARRAY_NAME, s) *)(FIO_PTR_UNTAG(ary_));

  switch (FIO_NAME_BL(FIO_ARRAY_NAME, embedded)(ary_)) {
  case 0:
    if (ary->end == ary->start)
      return -1;
    if (old) {
      FIO_ARRAY_TYPE_COPY(*old, ary->ary[ary->start]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
      FIO_ARRAY_TYPE_DESTROY(ary->ary[ary->start]);
#endif
    } else {
      FIO_ARRAY_TYPE_DESTROY(ary->ary[ary->start]);
    }
    ++ary->start;
    return 0;
  case 1:
    if (!ary->start)
      return -1;
    if (old) {
      FIO_ARRAY_TYPE_COPY(old[0], FIO_ARRAY2EMBEDDED(ary)->embedded[0]);
#if FIO_ARRAY_DESTROY_AFTER_COPY
      FIO_ARRAY_TYPE_DESTROY(FIO_ARRAY2EMBEDDED(ary)->embedded[0]);
#endif
    } else {
      FIO_ARRAY_TYPE_DESTROY(FIO_ARRAY2EMBEDDED(ary)->embedded[0]);
    }
    --ary->start;
    if (ary->start)
      memmove(FIO_ARRAY2EMBEDDED(ary)->embedded,
              FIO_ARRAY2EMBEDDED(ary)->embedded +
                  FIO_ARRAY2EMBEDDED(ary)->start,
              FIO_ARRAY2EMBEDDED(ary)->start *
                  sizeof(*FIO_ARRAY2EMBEDDED(ary)->embedded));
    memset(FIO_ARRAY2EMBEDDED(ary)->embedded + ary->start,
           0,
           sizeof(*ary->ary));
    return 0;
  }
  if (old)
    FIO_ARRAY_TYPE_COPY(old[0], FIO_ARRAY_TYPE_INVALID);
  return -1;
}

/**
 * Iteration using a callback for each entry in the array.
 *
 * The callback task function must accept an the entry data as well as an opaque
 * user pointer.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
IFUNC uint32_t FIO_NAME(FIO_ARRAY_NAME,
                        each)(FIO_ARRAY_PTR ary_,
                              int (*task)(FIO_NAME(FIO_ARRAY_NAME, each_s) *
                                          info),
                              void *udata,
                              int32_t start_at) {
  FIO_ARRAY_TYPE *a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);
  if (!a)
    return (uint32_t)-1;

  uint32_t count = FIO_NAME(FIO_ARRAY_NAME, count)(ary_);

  if (start_at < 0) {
    start_at = count - start_at;
    if (start_at < 0)
      start_at = 0;
  }

  if (!a || !task)
    return (uint32_t)-1;

  if ((uint32_t)start_at >= count)
    return count;

  FIO_NAME(FIO_ARRAY_NAME, each_s)
  e = {
      .parent = ary_,
      .index = (uint64_t)start_at,
      .items_at_index = 1,
      .task = task,
      .udata = udata,
  };

  while ((uint32_t)e.index < FIO_NAME(FIO_ARRAY_NAME, count)(ary_)) {
    a = FIO_NAME2(FIO_ARRAY_NAME, ptr)(ary_);
    e.value = a[e.index];
    int r = e.task(&e);
    ++e.index;
    if (r == -1) {
      return (uint32_t)(e.index);
    }
  }
  return e.index;
}

/* *****************************************************************************
Dynamic Arrays - test
***************************************************************************** */
#ifdef FIO_TEST_CSTL

/* make suer the functions are defined for the testing */
#ifdef FIO_REF_CONSTRUCTOR_ONLY
IFUNC FIO_ARRAY_PTR FIO_NAME(FIO_ARRAY_NAME, new)(void);
IFUNC void FIO_NAME(FIO_ARRAY_NAME, free)(FIO_ARRAY_PTR ary);
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

#define FIO_ARRAY_TEST_OBJ_SET(dest, val)                                      \
  memset(&(dest), (int)(val), sizeof(FIO_ARRAY_TYPE))
#define FIO_ARRAY_TEST_OBJ_IS(val)                                             \
  (!memcmp(&o, memset(&v, (int)(val), sizeof(v)), sizeof(FIO_ARRAY_TYPE)))

FIO_SFUNC int FIO_NAME_TEST(stl, FIO_NAME(FIO_ARRAY_NAME, test_task))(
    FIO_NAME(FIO_ARRAY_NAME, each_s) * i) {
  struct data_s {
    int i;
    int va[];
  } *d = (struct data_s *)i->udata;
  FIO_ARRAY_TYPE v;

  FIO_ARRAY_TEST_OBJ_SET(v, d->va[d->i]);
  ++d->i;
  if (d->va[d->i + 1])
    return 0;
  return -1;
}

FIO_SFUNC void FIO_NAME_TEST(stl, FIO_ARRAY_NAME)(void) {
  FIO_ARRAY_TYPE o;
  FIO_ARRAY_TYPE v;
  FIO_NAME(FIO_ARRAY_NAME, s) a_on_stack = FIO_ARRAY_INIT;
  FIO_ARRAY_PTR a_array[2];
  a_array[0] = (FIO_ARRAY_PTR)FIO_PTR_TAG((&a_on_stack));
  a_array[1] = FIO_NAME(FIO_ARRAY_NAME, new)();
  FIO_ASSERT_ALLOC(a_array[1]);
  /* perform test twice, once for an array on the stack and once for allocate */
  for (int selector = 0; selector < 2; ++selector) {
    FIO_ARRAY_PTR a = a_array[selector];
    fprintf(stderr,
            "* Testing dynamic arrays on the %s (" FIO_MACRO2STR(
                FIO_NAME(FIO_ARRAY_NAME,
                         s)) ").\n"
                             "  This type supports %zu embedded items\n",
            (selector ? "heap" : "stack"),
            FIO_ARRAY_EMBEDDED_CAPA);
    /* Test start here */

    /* test push */
    for (int i = 0; i < (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3; ++i) {
      FIO_ARRAY_TEST_OBJ_SET(o, (i + 1));
      o = *FIO_NAME(FIO_ARRAY_NAME, push)(a, o);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1), "push failed (%d)", i);
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, i);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1), "push-get cycle failed (%d)", i);
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, -1);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1),
                 "get with -1 returned wrong result (%d)",
                 i);
    }
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   FIO_ARRAY_EMBEDDED_CAPA + 3,
               "push didn't update count correctly (%d != %d)",
               FIO_NAME(FIO_ARRAY_NAME, count)(a),
               (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3);

    /* test pop */
    for (int i = (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3; i--;) {
      FIO_NAME(FIO_ARRAY_NAME, pop)(a, &o);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS((i + 1)),
                 "pop value error failed (%d)",
                 i);
    }
    FIO_ASSERT(!FIO_NAME(FIO_ARRAY_NAME, count)(a),
               "pop didn't pop all elements?");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, pop)(a, &o),
               "pop for empty array should return an error.");

    /* test compact with zero elements */
    FIO_NAME(FIO_ARRAY_NAME, compact)(a);
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) == FIO_ARRAY_EMBEDDED_CAPA,
               "compact zero elementes didn't make array embedded?");

    /* test unshift */
    for (int i = (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3; i--;) {
      FIO_ARRAY_TEST_OBJ_SET(o, (i + 1));
      o = *FIO_NAME(FIO_ARRAY_NAME, unshift)(a, o);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1), "shift failed (%d)", i);
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, 0);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1),
                 "unshift-get cycle failed (%d)",
                 i);
      int32_t negative_index = 0 - (((int)(FIO_ARRAY_EMBEDDED_CAPA) + 3) - i);
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, negative_index);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1),
                 "get with %d returned wrong result.",
                 negative_index);
    }
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   FIO_ARRAY_EMBEDDED_CAPA + 3,
               "unshift didn't update count correctly (%d != %d)",
               FIO_NAME(FIO_ARRAY_NAME, count)(a),
               (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3);

    /* test shift */
    for (int i = 0; i < (int)(FIO_ARRAY_EMBEDDED_CAPA) + 3; ++i) {
      FIO_NAME(FIO_ARRAY_NAME, shift)(a, &o);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS((i + 1)),
                 "shift value error failed (%d)",
                 i);
    }
    FIO_ASSERT(!FIO_NAME(FIO_ARRAY_NAME, count)(a),
               "shift didn't shift all elements?");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, shift)(a, &o),
               "shift for empty array should return an error.");

    /* test set from embedded? array */
    FIO_NAME(FIO_ARRAY_NAME, compact)(a);
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) == FIO_ARRAY_EMBEDDED_CAPA,
               "compact zero elementes didn't make array embedded (2)?");
    FIO_ARRAY_TEST_OBJ_SET(o, 1);
    FIO_NAME(FIO_ARRAY_NAME, push)(a, o);
    if (FIO_ARRAY_EMBEDDED_CAPA) {
      FIO_ARRAY_TEST_OBJ_SET(o, 1);
      FIO_NAME(FIO_ARRAY_NAME, set)(a, FIO_ARRAY_EMBEDDED_CAPA, o, &o);
      FIO_ASSERT(FIO_ARRAY_TYPE_CMP(o, FIO_ARRAY_TYPE_INVALID),
                 "set overflow from embedded array should reset `old`");
      FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                     FIO_ARRAY_EMBEDDED_CAPA + 1,
                 "set didn't update count correctly from embedded "
                 "array (%d != %d)",
                 FIO_NAME(FIO_ARRAY_NAME, count)(a),
                 (int)FIO_ARRAY_EMBEDDED_CAPA);
    }

    /* test set from bigger array */
    FIO_ARRAY_TEST_OBJ_SET(o, 1);
    FIO_NAME(FIO_ARRAY_NAME, set)
    (a, ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4), o, &o);
    FIO_ASSERT(FIO_ARRAY_TYPE_CMP(o, FIO_ARRAY_TYPE_INVALID),
               "set overflow should reset `old`");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) + 1,
               "set didn't update count correctly (%d != %d)",
               FIO_NAME(FIO_ARRAY_NAME, count)(a),
               (int)((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4));
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) >=
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4),
               "set capa should be above item count");
    if (FIO_ARRAY_EMBEDDED_CAPA) {
      FIO_ARRAY_TYPE_COPY(o, FIO_ARRAY_TYPE_INVALID);
      FIO_NAME(FIO_ARRAY_NAME, set)(a, FIO_ARRAY_EMBEDDED_CAPA, o, &o);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(1),
                 "set overflow lost last item while growing.");
    }
    o = FIO_NAME(FIO_ARRAY_NAME, get)(a, (FIO_ARRAY_EMBEDDED_CAPA + 1) * 2);
    FIO_ASSERT(FIO_ARRAY_TYPE_CMP(o, FIO_ARRAY_TYPE_INVALID),
               "set overflow should have memory in the middle set to invalid "
               "objetcs.");
    FIO_ARRAY_TEST_OBJ_SET(o, 2);
    FIO_NAME(FIO_ARRAY_NAME, set)(a, 0, o, &o);
    FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(1),
               "set should set `old` to previous value");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) + 1,
               "set item count error");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) >=
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) + 1,
               "set capa should be above item count");

    /* test find TODO: test with uninitialized array */
    FIO_ARRAY_TEST_OBJ_SET(o, 99);
    if (FIO_ARRAY_TYPE_CMP(o, FIO_ARRAY_TYPE_INVALID)) {
      FIO_ARRAY_TEST_OBJ_SET(o, 100);
    }
    int found = FIO_NAME(FIO_ARRAY_NAME, find)(a, o, 0);
    FIO_ASSERT(found == -1,
               "seeking for an object that doesn't exist should fail.");
    FIO_ARRAY_TEST_OBJ_SET(o, 1);
    found = FIO_NAME(FIO_ARRAY_NAME, find)(a, o, 1);
    FIO_ASSERT(found == ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4),
               "seeking for an object returned the wrong index.");
    FIO_ASSERT(found == FIO_NAME(FIO_ARRAY_NAME, find)(a, o, -1),
               "seeking for an object in reverse returned the wrong index.");
    FIO_ARRAY_TEST_OBJ_SET(o, 2);
    FIO_ASSERT(
        !FIO_NAME(FIO_ARRAY_NAME, find)(a, o, -2),
        "seeking for an object in reverse (2) returned the wrong index.");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) + 1,
               "find should have side-effects - count error");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) >=
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) + 1,
               "find should have side-effects - capa error");

    /* test remove */
    FIO_NAME(FIO_ARRAY_NAME, remove)(a, found, &o);
    FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(1), "remove didn't copy old data?");
    o = FIO_NAME(FIO_ARRAY_NAME, get)(a, 0);
    FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(2), "remove removed more?");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) ==
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4),
               "remove with didn't update count correctly (%d != %s)",
               FIO_NAME(FIO_ARRAY_NAME, count)(a),
               (int)((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4));
    o = FIO_NAME(FIO_ARRAY_NAME, get)(a, -1);

    /* test remove2 */
    FIO_ARRAY_TYPE_COPY(o, FIO_ARRAY_TYPE_INVALID);
    FIO_ASSERT((found = FIO_NAME(FIO_ARRAY_NAME, remove2)(a, o)) ==
                   ((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) - 1,
               "remove2 result error, %d != %d items.",
               found,
               (int)((FIO_ARRAY_EMBEDDED_CAPA + 1) * 4) - 1);
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) == 1,
               "remove2 didn't update count correctly (%d != 1)",
               FIO_NAME(FIO_ARRAY_NAME, count)(a));

    /* hopefuly these will end... or crash on error. */
    while (!FIO_NAME(FIO_ARRAY_NAME, pop)(a, NULL)) {
      ;
    }
    while (!FIO_NAME(FIO_ARRAY_NAME, shift)(a, NULL)) {
      ;
    }

    /* test push / unshift alternate */
    FIO_NAME(FIO_ARRAY_NAME, destroy)(a);
    for (int i = 0; i < 4096; ++i) {
      FIO_ARRAY_TEST_OBJ_SET(o, (i + 1));
      FIO_NAME(FIO_ARRAY_NAME, push)(a, o);
      FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) + 1 ==
                     ((uint32_t)(i + 1) << 1),
                 "push-unshift[%d.5] cycle count arror (%d != %d)",
                 i,
                 FIO_NAME(FIO_ARRAY_NAME, count)(a),
                 (((uint32_t)(i + 1) << 1)) - 1);
      FIO_ARRAY_TEST_OBJ_SET(o, (i + 4097));
      FIO_NAME(FIO_ARRAY_NAME, unshift)(a, o);
      FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, count)(a) == ((uint32_t)(i + 1) << 1),
                 "push-unshift[%d] cycle count arror (%d != %d)",
                 i,
                 FIO_NAME(FIO_ARRAY_NAME, count)(a),
                 ((uint32_t)(i + 1) << 1));
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, 0);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 4097),
                 "unshift-push cycle failed (%d)",
                 i);
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, -1);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS(i + 1),
                 "push-shift cycle failed (%d)",
                 i);
    }
    for (int i = 0; i < 4096; ++i) {
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, i);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS((4096 * 2) - i),
                 "item value error at index %d",
                 i);
    }
    for (int i = 0; i < 4096; ++i) {
      o = FIO_NAME(FIO_ARRAY_NAME, get)(a, i + 4096);
      FIO_ASSERT(FIO_ARRAY_TEST_OBJ_IS((1 + i)),
                 "item value error at index %d",
                 i + 4096);
    }
#if DEBUG
    for (int i = 0; i < 2; ++i) {
      FIO_LOG_DEBUG2(
          "\t- " FIO_MACRO2STR(
              FIO_NAME(FIO_ARRAY_NAME, s)) " after push/unshit cycle%s:\n"
                                           "\t\t- item count: %d items\n"
                                           "\t\t- capacity:   %d items\n"
                                           "\t\t- memory:     %d bytes\n",
          (i ? " after compact" : ""),
          FIO_NAME(FIO_ARRAY_NAME, count)(a),
          FIO_NAME(FIO_ARRAY_NAME, capa)(a),
          FIO_NAME(FIO_ARRAY_NAME, capa)(a) * sizeof(FIO_ARRAY_TYPE));
      FIO_NAME(FIO_ARRAY_NAME, compact)(a);
    }
#endif /* DEBUG */

    FIO_ARRAY_TYPE_COPY(o, FIO_ARRAY_TYPE_INVALID);
/* test set with NULL, hopefully a bug will cause a crash */
#if FIO_ARRAY_TYPE_DESTROY_SIMPLE
    for (int i = 0; i < 4096; ++i) {
      FIO_NAME(FIO_ARRAY_NAME, set)(a, i, o, NULL);
    }
#else
    /*
     * we need to clear the memory to make sure a cleanup actions don't get
     * unexpected values.
     */
    for (int i = 0; i < (4096 * 2); ++i) {
      FIO_ARRAY_TYPE_COPY((FIO_NAME2(FIO_ARRAY_NAME, ptr)(a)[i]),
                          FIO_ARRAY_TYPE_INVALID);
    }

#endif

    /* TODO: test concat */

    /* test each */
    {
      struct data_s {
        int i;
        int va[10];
      } d = {1, {1, 8, 2, 7, 3, 6, 4, 5}};
      FIO_NAME(FIO_ARRAY_NAME, destroy)(a);
      for (int i = 0; d.va[i]; ++i) {
        FIO_ARRAY_TEST_OBJ_SET(o, d.va[i]);
        FIO_NAME(FIO_ARRAY_NAME, push)(a, o);
      }

      int index = FIO_NAME(FIO_ARRAY_NAME, each)(
          a,
          FIO_NAME_TEST(stl, FIO_NAME(FIO_ARRAY_NAME, test_task)),
          (void *)&d,
          d.i);
      FIO_ASSERT(index == d.i,
                 "index rerturned from each should match next object");
      FIO_ASSERT(*(char *)&d.va[d.i],
                 "array each error (didn't stop in time?).");
      FIO_ASSERT(!(*(char *)&d.va[d.i + 1]),
                 "array each error (didn't stop in time?).");
    }
#if FIO_ARRAY_TYPE_DESTROY_SIMPLE
    {
      FIO_NAME(FIO_ARRAY_NAME, destroy)(a);
      size_t max_items = 63;
      FIO_ARRAY_TYPE tmp[64];
      for (size_t i = 0; i < max_items; ++i) {
        memset(tmp + i, i + 1, sizeof(*tmp));
      }
      for (size_t items = 0; items <= max_items; items = ((items << 1) | 1)) {
        FIO_LOG_DEBUG2("* testing the FIO_ARRAY_EACH macro with %zu items.",
                       items);
        size_t i = 0;
        for (i = 0; i < items; ++i)
          FIO_NAME(FIO_ARRAY_NAME, push)(a, tmp[i]);
        i = 0;
        FIO_ARRAY_EACH(FIO_ARRAY_NAME, a, pos) {
          FIO_ASSERT(!memcmp(tmp + i, pos, sizeof(*pos)),
                     "FIO_ARRAY_EACH pos is at wrong index %zu != %zu",
                     (size_t)(pos - FIO_NAME2(FIO_ARRAY_NAME, ptr)(a)),
                     i);
          ++i;
        }
        FIO_ASSERT(i == items,
                   "FIO_ARRAY_EACH macro count error - didn't review all "
                   "items? %zu != %zu ",
                   i,
                   items);
        FIO_NAME(FIO_ARRAY_NAME, destroy)(a);
      }
    }
#endif
    /* test destroy */
    FIO_NAME(FIO_ARRAY_NAME, destroy)(a);
    FIO_ASSERT(!FIO_NAME(FIO_ARRAY_NAME, count)(a),
               "destroy didn't clear count.");
    FIO_ASSERT(FIO_NAME(FIO_ARRAY_NAME, capa)(a) == FIO_ARRAY_EMBEDDED_CAPA,
               "destroy capa error.");
    /* Test end here */
  }
  FIO_NAME(FIO_ARRAY_NAME, free)(a_array[1]);
}
#undef FIO_ARRAY_TEST_OBJ_SET
#undef FIO_ARRAY_TEST_OBJ_IS

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Dynamic Arrays - cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_ARRAY_NAME */

#undef FIO_ARRAY_NAME
#undef FIO_ARRAY_TYPE
#undef FIO_ARRAY_ENABLE_EMBEDDED
#undef FIO_ARRAY_TYPE_INVALID
#undef FIO_ARRAY_TYPE_INVALID_SIMPLE
#undef FIO_ARRAY_TYPE_COPY
#undef FIO_ARRAY_TYPE_COPY_SIMPLE
#undef FIO_ARRAY_TYPE_CONCAT_COPY
#undef FIO_ARRAY_TYPE_CONCAT_COPY_SIMPLE
#undef FIO_ARRAY_TYPE_DESTROY
#undef FIO_ARRAY_TYPE_DESTROY_SIMPLE
#undef FIO_ARRAY_DESTROY_AFTER_COPY
#undef FIO_ARRAY_TYPE_CMP
#undef FIO_ARRAY_TYPE_CMP_SIMPLE
#undef FIO_ARRAY_PADDING
#undef FIO_ARRAY_SIZE2WORDS
#undef FIO_ARRAY_POS2ABS
#undef FIO_ARRAY_AB_CT
#undef FIO_ARRAY_PTR
#undef FIO_ARRAY_EXPONENTIAL
#undef FIO_ARRAY_ADD2CAPA
#undef FIO_ARRAY_IS_EMBEDDED
#undef FIO_ARRAY_IS_EMBEDDED_PTR
#undef FIO_ARRAY_EMBEDDED_CAPA
#undef FIO_ARRAY2EMBEDDED
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_UMAP_NAME umap          /* Development inclusion - ignore line */
#define FIO_MAP_TEST                /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                  Common Map Settings (ordered / unordered)




***************************************************************************** */
#if defined(FIO_UMAP_NAME)
#ifndef FIO_MAP_NAME
#define FIO_MAP_NAME FIO_UMAP_NAME
#endif
#ifndef FIO_MAP_ORDERED
#define FIO_MAP_ORDERED 0
#endif
#elif defined(FIO_OMAP_NAME)
#ifndef FIO_MAP_NAME
#define FIO_MAP_NAME FIO_OMAP_NAME
#endif
#ifndef FIO_MAP_ORDERED
#define FIO_MAP_ORDERED 1
#endif
#else
#ifndef FIO_MAP_ORDERED
#define FIO_MAP_ORDERED 1
#endif
#endif

#ifdef FIO_MAP_NAME
/* *****************************************************************************
The following macros are used to customize the map.
***************************************************************************** */

#ifndef FIO_MAP_TYPE
/** The type for the elements in the map */
#define FIO_MAP_TYPE void *
/** An invalid value for that type (if any). */
#define FIO_MAP_TYPE_INVALID NULL
#else
#ifndef FIO_MAP_TYPE_INVALID
/** An invalid value for that type (if any). */
#define FIO_MAP_TYPE_INVALID ((FIO_MAP_TYPE){0})
#endif /* FIO_MAP_TYPE_INVALID */
#endif /* FIO_MAP_TYPE */

#ifndef FIO_MAP_TYPE_COPY
/** Handles a copy operation for an value. */
#define FIO_MAP_TYPE_COPY(dest, src) (dest) = (src)
/* internal flag - do not set */
#define FIO_MAP_TYPE_COPY_SIMPLE 1
#endif

#ifndef FIO_MAP_TYPE_DESTROY
/** Handles a destroy / free operation for a map's value. */
#define FIO_MAP_TYPE_DESTROY(obj)
/** internal flag - set only if the object desctructor is optional */
#define FIO_MAP_TYPE_DESTROY_SIMPLE 1
#else
#ifndef FIO_MAP_TYPE_DESTROY_SIMPLE
#define FIO_MAP_TYPE_DESTROY_SIMPLE 0
#endif
#endif

#ifndef FIO_MAP_TYPE_DISCARD
/** Handles discarded value data (i.e., insert without overwrite). */
#define FIO_MAP_TYPE_DISCARD(obj)
#endif

#ifndef FIO_MAP_TYPE_CMP
/** Handles a comparison operation for a map's value. */
#define FIO_MAP_TYPE_CMP(a, b) 1
#endif

/**
 * The FIO_MAP_DESTROY_AFTER_COPY macro should be set if FIO_MAP_TYPE_DESTROY
 * should be called after FIO_MAP_TYPE_COPY when an object is removed from the
 * array after being copied to an external container (an `old` pointer)
 */
#ifndef FIO_MAP_DESTROY_AFTER_COPY
#if !FIO_MAP_TYPE_DESTROY_SIMPLE && !FIO_MAP_TYPE_COPY_SIMPLE
#define FIO_MAP_DESTROY_AFTER_COPY 1
#else
#define FIO_MAP_DESTROY_AFTER_COPY 0
#endif
#endif /* FIO_MAP_DESTROY_AFTER_COPY */

/* *****************************************************************************
Dictionary / Hash Map - a Hash Map is basically a Set of couplets
***************************************************************************** */
/* Defining a key makes a Hash Map instead of a Set */
#ifdef FIO_MAP_KEY

#ifndef FIO_MAP_KEY_INVALID
/** An invalid value for the hash map key type (if any). */
#define FIO_MAP_KEY_INVALID ((FIO_MAP_KEY){0})
#endif

#ifndef FIO_MAP_KEY_COPY
/** Handles a copy operation for a hash maps key. */
#define FIO_MAP_KEY_COPY(dest, src) (dest) = (src)
#endif

#ifndef FIO_MAP_KEY_DESTROY
/** Handles a destroy / free operation for a hash maps key. */
#define FIO_MAP_KEY_DESTROY(obj)
/** internal flag - set only if the object desctructor is optional */
#define FIO_MAP_KEY_DESTROY_SIMPLE 1
#else
#ifndef FIO_MAP_KEY_DESTROY_SIMPLE
#define FIO_MAP_KEY_DESTROY_SIMPLE 0
#endif
#endif

#ifndef FIO_MAP_KEY_DISCARD
/** Handles discarded element data (i.e., when overwriting only the value). */
#define FIO_MAP_KEY_DISCARD(obj)
#endif

#ifndef FIO_MAP_KEY_CMP
/** Handles a comparison operation for a hash maps key. */
#define FIO_MAP_KEY_CMP(a, b) 1
#endif

typedef struct {
  FIO_MAP_KEY key;
  FIO_MAP_TYPE value;
} FIO_NAME(FIO_MAP_NAME, couplet_s);

FIO_IFUNC void FIO_NAME(FIO_MAP_NAME, __couplet_copy)(
    FIO_NAME(FIO_MAP_NAME, couplet_s) * dest,
    FIO_NAME(FIO_MAP_NAME, couplet_s) * src) {
  FIO_MAP_KEY_COPY((dest->key), (src->key));
  FIO_MAP_TYPE_COPY((dest->value), (src->value));
}

FIO_IFUNC void FIO_NAME(FIO_MAP_NAME,
                        __couplet_destroy)(FIO_NAME(FIO_MAP_NAME, couplet_s) *
                                           c) {
  FIO_MAP_KEY_DESTROY(c->key);
  FIO_MAP_TYPE_DESTROY(c->value);
  (void)c; /* in case where macros do nothing */
}

/** FIO_MAP_OBJ is either a couplet (for hash maps) or the objet (for sets) */
#define FIO_MAP_OBJ FIO_NAME(FIO_MAP_NAME, couplet_s)

/** FIO_MAP_OBJ_KEY is FIO_MAP_KEY for hash maps or FIO_MAP_TYPE for sets */
#define FIO_MAP_OBJ_KEY FIO_MAP_KEY

#define FIO_MAP_OBJ_INVALID                                                    \
  ((FIO_NAME(FIO_MAP_NAME, couplet_s)){.key = FIO_MAP_KEY_INVALID,             \
                                       .value = FIO_MAP_TYPE_INVALID})

#define FIO_MAP_OBJ_COPY(dest, src)                                            \
  FIO_NAME(FIO_MAP_NAME, __couplet_copy)(&(dest), &(src))

#define FIO_MAP_OBJ_DESTROY(obj)                                               \
  FIO_NAME(FIO_MAP_NAME, __couplet_destroy)(&(obj))

#define FIO_MAP_OBJ_CMP(a, b)        FIO_MAP_KEY_CMP((a).key, (b).key)
#define FIO_MAP_OBJ_KEY_CMP(a, key_) FIO_MAP_KEY_CMP((a).key, (key_))
#define FIO_MAP_OBJ2KEY(o)           (o).key
#define FIO_MAP_OBJ2TYPE(o)          (o).value

#define FIO_MAP_OBJ_DISCARD(o)                                                 \
  do {                                                                         \
    FIO_MAP_TYPE_DISCARD(((o).value));                                         \
    FIO_MAP_KEY_DISCARD(((o).key));                                            \
  } while (0);

#if FIO_MAP_DESTROY_AFTER_COPY
#define FIO_MAP_OBJ_DESTROY_AFTER FIO_MAP_OBJ_DESTROY
#else
#define FIO_MAP_OBJ_DESTROY_AFTER(obj) FIO_MAP_KEY_DESTROY((obj).key);
#endif /* FIO_MAP_DESTROY_AFTER_COPY */

/* *****************************************************************************
Set Map
***************************************************************************** */
#else /* FIO_MAP_KEY */
#define FIO_MAP_KEY_DESTROY_SIMPLE 1
/** FIO_MAP_OBJ is either a couplet (for hash maps) or the objet (for sets) */
#define FIO_MAP_OBJ                FIO_MAP_TYPE
/** FIO_MAP_OBJ_KEY is FIO_MAP_KEY for hash maps or FIO_MAP_TYPE for sets */
#define FIO_MAP_OBJ_KEY            FIO_MAP_TYPE
#define FIO_MAP_OBJ_INVALID        FIO_MAP_TYPE_INVALID
#define FIO_MAP_OBJ_COPY           FIO_MAP_TYPE_COPY
#define FIO_MAP_OBJ_DESTROY        FIO_MAP_TYPE_DESTROY
#define FIO_MAP_OBJ_CMP            FIO_MAP_TYPE_CMP
#define FIO_MAP_OBJ_KEY_CMP        FIO_MAP_TYPE_CMP
#define FIO_MAP_OBJ2KEY(o)         (o)
#define FIO_MAP_OBJ2TYPE(o)        (o)
#define FIO_MAP_OBJ_DISCARD        FIO_MAP_TYPE_DISCARD
#define FIO_MAP_KEY_DISCARD(_ignore)
#define FIO_MAP_KEY_COPY(_ignore, _ignore2)
#if FIO_MAP_DESTROY_AFTER_COPY
#define FIO_MAP_OBJ_DESTROY_AFTER FIO_MAP_TYPE_DESTROY
#else
#define FIO_MAP_OBJ_DESTROY_AFTER(obj)
#endif /* FIO_MAP_DESTROY_AFTER_COPY */

#endif /* FIO_MAP_KEY */

/* *****************************************************************************
Misc Settings (eviction policy, load-factor attempts, etc')
***************************************************************************** */

#ifndef FIO_MAP_MAX_SEEK /* LIMITED to 255 */
#if FIO_MAP_ORDERED
/* The maximum number of bins to rotate when (partial/full) collisions occure */
#define FIO_MAP_MAX_SEEK (13U)
#else
#define FIO_MAP_MAX_SEEK (7U)
#endif
#endif

#ifndef FIO_MAP_MAX_FULL_COLLISIONS /* LIMITED to 255 */
/* The maximum number of full hash collisions that can be consumed */
#define FIO_MAP_MAX_FULL_COLLISIONS (22U)
#endif

#ifndef FIO_MAP_CUCKOO_STEPS
/* Prime numbers are better */
#define FIO_MAP_CUCKOO_STEPS (0x43F82D0BUL) /* should be a high prime */
#endif

#ifndef FIO_MAP_EVICT_LRU
/** Set the `evict` method to evict based on the Least Recently Used object. */
#define FIO_MAP_EVICT_LRU 0
#endif

#ifndef FIO_MAP_SHOULD_OVERWRITE
/** Tests if `older` should be replaced with `newer`. */
#define FIO_MAP_SHOULD_OVERWRITE(older, newer) 1
#endif

#ifndef FIO_MAP_MAX_ELEMENTS
/** The maximum number of elements allowed before removing old data (FIFO) */
#define FIO_MAP_MAX_ELEMENTS 0
#endif

#ifndef FIO_MAP_HASH
/** The type for map hash value (an X bit integer) */
#define FIO_MAP_HASH uint64_t
#endif

#undef FIO_MAP_HASH_FIXED
/** the value to be used when the hash is a reserved value. */
#define FIO_MAP_HASH_FIXED ((FIO_MAP_HASH)-2LL)

#undef FIO_MAP_HASH_FIX
/** Validates the hash value and returns the valid value. */
#define FIO_MAP_HASH_FIX(h) (!h ? FIO_MAP_HASH_FIXED : (h))

/**
 * Unordered maps don't have to cache an object's hash.
 *
 * If the hash is cheap to calculate, it could be recalculated on the fly.
 */
#if defined(FIO_MAP_HASH_FN) && !FIO_MAP_ORDERED
FIO_IFUNC FIO_MAP_HASH FIO_NAME(FIO_MAP_NAME, __get_hash)(FIO_MAP_OBJ_KEY k) {
  FIO_MAP_HASH h = FIO_MAP_HASH_FN(k);
  h = FIO_MAP_HASH_FIX(h);
  return h;
}
#define FIO_MAP_HASH_CACHED 0
#define FIO_MAP_HASH_GET_HASH(map_ptr, index)                                  \
  FIO_NAME(FIO_MAP_NAME, __get_hash)                                           \
  (FIO_MAP_OBJ2KEY((map_ptr)->map[(index)].obj))
#else
#define FIO_MAP_HASH_GET_HASH(map_ptr, index) (map_ptr)->map[(index)].hash
#define FIO_MAP_HASH_CACHED                   1
#endif

#ifndef FIO_MAP_SEEK_AS_ARRAY_LOG_LIMIT
/* Hash to Array optimization limit in log2. MUST be less then 8. */
#define FIO_MAP_SEEK_AS_ARRAY_LOG_LIMIT 3
#endif

/**
 * Normally, FIO_MAP uses 32bit internal indexing and types.
 *
 * This limits the map to approximately 2 billion items (2,147,483,648).
 * Depending on possible 32 bit hash collisions, more items may be inserted.
 *
 * If FIO_MAP_BIG is be defined, 64 bit addressing is used, increasing the
 * maximum number of items to... hmm... a lot (1 << 63).
 */
#ifdef FIO_MAP_BIG
#define FIO_MAP_SIZE_TYPE      uint64_t
#define FIO_MAP_INDEX_USED_BIT ((uint64_t)1 << 63)
#else
#define FIO_MAP_SIZE_TYPE      uint32_t
#define FIO_MAP_INDEX_USED_BIT ((uint32_t)1 << 31)
#endif /* FIO_MAP_BIG */
/* *****************************************************************************
Pointer Tagging Support
***************************************************************************** */

#ifdef FIO_PTR_TAG_TYPE
#define FIO_MAP_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_MAP_PTR FIO_NAME(FIO_MAP_NAME, s) *
#endif

/* *****************************************************************************





Map API





***************************************************************************** */

/* *****************************************************************************
Types
***************************************************************************** */

/** The type for each node in the map. */
typedef struct FIO_NAME(FIO_MAP_NAME, node_s) FIO_NAME(FIO_MAP_NAME, node_s);
/** The Map Type (container) itself. */
typedef struct FIO_NAME(FIO_MAP_NAME, s) FIO_NAME(FIO_MAP_NAME, s);

#ifndef FIO_MAP_INIT
/* Initialization macro. */
#define FIO_MAP_INIT                                                           \
  { 0 }
#endif

struct FIO_NAME(FIO_MAP_NAME, node_s) {
  /** the data being stored in the Map / key-value pair: obj.key obj.value. */
  FIO_MAP_OBJ obj;
#if FIO_MAP_HASH_CACHED
  /** a copy of the hash value. */
  FIO_MAP_HASH hash;
#endif
#if FIO_MAP_EVICT_LRU
  /** LRU evicion monitoring - do not access directly */
  struct {
    FIO_MAP_SIZE_TYPE next;
    FIO_MAP_SIZE_TYPE prev;
  } node;
#endif /* FIO_MAP_EVICT_LRU */
};

/* *****************************************************************************
Contruction API
***************************************************************************** */

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY

/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC FIO_MAP_PTR FIO_NAME(FIO_MAP_NAME, new)(void);

/* Frees any internal data AND the object's container! */
FIO_IFUNC int FIO_NAME(FIO_MAP_NAME, free)(FIO_MAP_PTR map);

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Destroys the object, reinitializing its container. */
SFUNC void FIO_NAME(FIO_MAP_NAME, destroy)(FIO_MAP_PTR map);

/* *****************************************************************************
Get / Set / Remove
***************************************************************************** */

/** Gets a value from the map, returning a temporary pointer. */
SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, get_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
                                                    FIO_MAP_OBJ_KEY key);

/** Sets a value in the map, returning a temporary pointer. */
SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, set_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                    FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                    FIO_MAP_TYPE obj,
                                                    FIO_MAP_TYPE *old,
                                                    uint8_t overwrite);

/** Gets a value from the map, if exists. */
FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME, get)(FIO_MAP_PTR map,
                                                   FIO_MAP_HASH hash,
                                                   FIO_MAP_OBJ_KEY key);

/** Sets a value in the map, overwriting existing data if any. */
FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME, set)(FIO_MAP_PTR map,
                                                   FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                   FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                   FIO_MAP_TYPE obj,
                                                   FIO_MAP_TYPE *old);

/** Removes a value from the map. */
SFUNC int FIO_NAME(FIO_MAP_NAME, remove)(FIO_MAP_PTR map,
                                         FIO_MAP_HASH hash,
                                         FIO_MAP_OBJ_KEY key,
                                         FIO_MAP_TYPE *old);

/** Sets the object only if missing. Otherwise keeps existing value. */
FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME, set_if_missing)(FIO_MAP_PTR map,
                                                              FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                              FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                              FIO_MAP_TYPE obj);

/** Removes all objects from the map. */
SFUNC void FIO_NAME(FIO_MAP_NAME, clear)(FIO_MAP_PTR map);

/**
 * If `FIO_MAP_EVICT_LRU` is defined, evicts `number_of_elements` least
 * recently accessed.
 *
 * Otherwise, eviction is somewhat random and undefined.
 */
SFUNC int FIO_NAME(FIO_MAP_NAME, evict)(FIO_MAP_PTR map,
                                        size_t number_of_elements);

/* *****************************************************************************
Object state information
***************************************************************************** */

/** Returns the maps current object count. */
FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, count)(FIO_MAP_PTR map);

/** Returns the maps current theoretical capacity. */
FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, capa)(FIO_MAP_PTR map);

/** Reservse enough space for a theoretical capacity of `capa` objects. */
SFUNC size_t FIO_NAME(FIO_MAP_NAME, reserve)(FIO_MAP_PTR map,
                                             FIO_MAP_SIZE_TYPE capa);

/** Attempts to minimize memory use. */
SFUNC void FIO_NAME(FIO_MAP_NAME, compact)(FIO_MAP_PTR map);

/** Rehashes the map. No need to call this, rehashing is automatic. */
SFUNC int FIO_NAME(FIO_MAP_NAME, rehash)(FIO_MAP_PTR map);

/* *****************************************************************************
Iteration
***************************************************************************** */

/** Takes a previous (or NULL) item's position and returns the next. */
FIO_IFUNC FIO_NAME(FIO_MAP_NAME, node_s) *
    FIO_NAME(FIO_MAP_NAME, each_next)(FIO_MAP_PTR map,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * *first,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * pos);

/** Iteration information structure passed to the callback. */
typedef struct FIO_NAME(FIO_MAP_NAME, each_s) {
  /** The being iterated. Once set, cannot be safely changed. */
  FIO_MAP_PTR const parent;
  /** The current object's index */
  uint64_t index;
  /** Either 1 (set) or 2 (map), and may be used to allow type detection. */
  const int64_t items_at_index;
  /** The callback / task called for each index, may be updated mid-cycle. */
  int (*task)(struct FIO_NAME(FIO_MAP_NAME, each_s) * info);
  /** Opaque user data. */
  void *udata;
  /** The object / value at the current index. */
  FIO_MAP_TYPE value;
#ifdef FIO_MAP_KEY
  /** The key used to access the specific value. */
  FIO_MAP_KEY key;
#endif
} FIO_NAME(FIO_MAP_NAME, each_s);

/**
 * Iteration using a callback for each element in the map.
 *
 * The callback task function must accept an each_s pointer, see above.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
SFUNC FIO_MAP_SIZE_TYPE
    FIO_NAME(FIO_MAP_NAME, each)(FIO_MAP_PTR map,
                                 int (*task)(FIO_NAME(FIO_MAP_NAME, each_s) *),
                                 void *udata,
                                 ssize_t start_at);

/* *****************************************************************************





Common Map Implementation - inlined static functions





***************************************************************************** */

FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME, get)(FIO_MAP_PTR map,
                                                   FIO_MAP_HASH hash,
                                                   FIO_MAP_OBJ_KEY key) {
  FIO_MAP_TYPE *r = FIO_NAME(FIO_MAP_NAME, get_ptr)(map, hash, key);
  if (!r)
    return FIO_MAP_TYPE_INVALID;
  return *r;
}

FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME, set)(FIO_MAP_PTR map,
                                                   FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                   FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                   FIO_MAP_TYPE obj,
                                                   FIO_MAP_TYPE *old) {
  FIO_MAP_TYPE *r = FIO_NAME(FIO_MAP_NAME, set_ptr)(map,
                                                    hash,
#ifdef FIO_MAP_KEY
                                                    key,
#endif /* FIO_MAP_KEY */
                                                    obj,
                                                    old,
                                                    1);
  if (!r)
    return FIO_MAP_TYPE_INVALID;
  return *r;
}

FIO_IFUNC FIO_MAP_TYPE FIO_NAME(FIO_MAP_NAME,
                                set_if_missing)(FIO_MAP_PTR map,
                                                FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                FIO_MAP_TYPE obj) {
  FIO_MAP_TYPE *r = FIO_NAME(FIO_MAP_NAME, set_ptr)(map,
                                                    hash,
#ifdef FIO_MAP_KEY
                                                    key,
#endif /* FIO_MAP_KEY */
                                                    obj,
                                                    NULL,
                                                    0);
  if (!r)
    return FIO_MAP_TYPE_INVALID;
  return *r;
}

/* *****************************************************************************
Iteration Macro
***************************************************************************** */
#ifndef FIO_MAP_EACH
/**
 * A macro for a `for` loop that iterates over all the Map's objects (in
 * order).
 *
 * Use this macro for small Hash Maps / Sets.
 *
 * - `map_name` is the Map's type name / function prefix, same as FIO_MAP_NAME.
 *
 * - `map_p` is a pointer to the Hash Map / Set variable.
 *
 * - `pos` is a temporary variable name to be created for iteration. This
 *    variable may SHADOW external variables, be aware.
 *
 * To access the object information, use:
 *
 * - `pos->hash` to access the hash value.
 *
 * - `pos->obj` to access the object's data.
 *
 *    For Hash Maps, use `pos->obj.key` and `pos->obj.value`.
 */
#define FIO_MAP_EACH(map_name, map_p, pos)                                     \
  for (FIO_NAME(map_name,                                                      \
                node_s) *first___ = NULL,                                      \
                        *pos = FIO_NAME(map_name,                              \
                                        each_next)(map_p, &first___, NULL);    \
       pos;                                                                    \
       pos = FIO_NAME(map_name, each_next)(map_p, &first___, pos))
#endif

/* *****************************************************************************
Common Map Settings - Finish
***************************************************************************** */
#endif
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_MAP_NAME map            /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#include "210 map api.h"            /* Development inclusion - ignore line */
#define FIO_MAP_TEST                /* Development inclusion - ignore line */
#define FIO_MAP_V2                  /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                  Unordered Map - an Unordered Hash Map / Set




***************************************************************************** */
#if defined(FIO_MAP_NAME) && FIO_MAP_ORDERED

/* *****************************************************************************





Ordered Map Types - Implementation





***************************************************************************** */

/** An Ordered Map Type */
struct FIO_NAME(FIO_MAP_NAME, s) {
  /** Internal map / memory - do not access directly */
  FIO_NAME(FIO_MAP_NAME, node_s) * map;
  /** Object count - do not access directly */
  FIO_MAP_SIZE_TYPE count;
  /** Writing position - do not access directly */
  FIO_MAP_SIZE_TYPE w;
#if FIO_MAP_EVICT_LRU
  /** LRU evicion monitoring - do not access directly */
  FIO_MAP_SIZE_TYPE last_used;
#endif /* FIO_MAP_EVICT_LRU */
  uint8_t bits;
  uint8_t under_attack;
};

/* *****************************************************************************
Ordered Map Implementation - inlined static functions
***************************************************************************** */

#ifndef FIO_MAP_CAPA
#define FIO_MAP_CAPA(bits) (((uintptr_t)1ULL << (bits)) - 1)
#endif

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC FIO_MAP_PTR FIO_NAME(FIO_MAP_NAME, new)(void) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*m), 0);
  if (!m)
    return (FIO_MAP_PTR)NULL;
  *m = (FIO_NAME(FIO_MAP_NAME, s))FIO_MAP_INIT;
  return (FIO_MAP_PTR)FIO_PTR_TAG(m);
}
/* Frees any internal data AND the object's container! */
FIO_IFUNC int FIO_NAME(FIO_MAP_NAME, free)(FIO_MAP_PTR map) {
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  FIO_NAME(FIO_MAP_NAME, destroy)(map);
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  FIO_MEM_FREE_(m, sizeof(*m));
  return 0;
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, count)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  return m->count;
}

FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, capa)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  return FIO_MAP_CAPA(m->bits);
}

FIO_IFUNC FIO_NAME(FIO_MAP_NAME, node_s) *
    FIO_NAME(FIO_MAP_NAME, each_next)(FIO_MAP_PTR map,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * *first,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * pos) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m || !first)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  if (!m->count || !m->map)
    return NULL;
  intptr_t i;
#if FIO_MAP_EVICT_LRU
  FIO_MAP_SIZE_TYPE next;
  if (!pos) {
    i = m->last_used;
    *first = m->map;
    return m->map + i;
  }
  i = pos - *first;
  *first = m->map; /* was it updated? */
  next = m->map[i].node.next;
  if (next == m->last_used)
    return NULL;
  return m->map + next;

#else  /* FIO_MAP_EVICT_LRU */
  if (!pos) {
    i = -1;
  } else {
    i = (intptr_t)(pos - *first);
  }
  ++i;
  *first = m->map;
  while ((uintptr_t)i < (uintptr_t)m->w) {
    if (m->map[i].hash)
      return m->map + i;
    ++i;
  }
  return NULL;
#endif /* FIO_MAP_EVICT_LRU */
}

/* *****************************************************************************
Ordered Map Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

#ifndef FIO_MAP_MEMORY_SIZE
#define FIO_MAP_MEMORY_SIZE(bits)                                              \
  ((sizeof(FIO_NAME(FIO_MAP_NAME, node_s)) + sizeof(FIO_MAP_SIZE_TYPE)) *      \
   FIO_MAP_CAPA(bits))
#endif

/* *****************************************************************************
Ordered Map Implementation - helper functions.
***************************************************************************** */

/** the value to be used when the hash is a reserved value. */
#define FIO_MAP_HASH_FIXED ((FIO_MAP_HASH)-2LL)

/** the value to be used when the hash is a reserved value. */
#define FIO_MAP_HASH_FIX(h) (!h ? FIO_MAP_HASH_FIXED : (h))

FIO_IFUNC FIO_MAP_SIZE_TYPE *FIO_NAME(FIO_MAP_NAME,
                                      __imap)(FIO_NAME(FIO_MAP_NAME, s) * m) {
  return (FIO_MAP_SIZE_TYPE *)(m->map + FIO_MAP_CAPA(m->bits));
}

FIO_IFUNC FIO_MAP_SIZE_TYPE FIO_NAME(FIO_MAP_NAME,
                                     __hash2imap)(FIO_MAP_HASH hash,
                                                  uint8_t bits) {
  FIO_MAP_SIZE_TYPE r = hash & ((~(FIO_MAP_SIZE_TYPE)0) << bits);
  return r ? r
           : (((~(FIO_MAP_SIZE_TYPE)0) << bits) << 1); /* must never be zero */
}

typedef struct {
  /* index in the index map */
  FIO_MAP_SIZE_TYPE i;
  /* index in the data array */
  FIO_MAP_SIZE_TYPE a;
} FIO_NAME(FIO_MAP_NAME, __pos_s);

/* locat an objects index in the index map and its array position */
FIO_SFUNC FIO_NAME(FIO_MAP_NAME, __pos_s)
    FIO_NAME(FIO_MAP_NAME, __index)(FIO_NAME(FIO_MAP_NAME, s) * m,
                                    const FIO_MAP_HASH hash,
                                    FIO_MAP_OBJ_KEY key,
                                    FIO_MAP_SIZE_TYPE set_hash) {
  FIO_NAME(FIO_MAP_NAME, __pos_s)
  i = {
      .i = (FIO_MAP_SIZE_TYPE)-1LL,
      .a = (FIO_MAP_SIZE_TYPE)-1LL,
  };
  size_t total_collisions = 0;
  if (!m->map)
    return i;
  FIO_MAP_SIZE_TYPE *const imap = FIO_NAME(FIO_MAP_NAME, __imap)(m);
  /* note: hash MUST be normalized by this point */
  const FIO_MAP_SIZE_TYPE pos_mask = FIO_MAP_CAPA(m->bits);
  const FIO_MAP_SIZE_TYPE hashed_mask =
      ((size_t)(m->bits + 1) < (size_t)(sizeof(FIO_MAP_SIZE_TYPE) * 8))
          ? ((~(FIO_MAP_SIZE_TYPE)0) << m->bits)
          : 0;
  const int max_attempts = (FIO_MAP_CAPA(m->bits)) >= FIO_MAP_MAX_SEEK
                               ? (int)FIO_MAP_MAX_SEEK
                               : (FIO_MAP_CAPA(m->bits));
  /* we perform X attempts using large cuckoo steps */
  FIO_MAP_SIZE_TYPE pos = hash;
  if (m->bits <= FIO_MAP_SEEK_AS_ARRAY_LOG_LIMIT)
    goto seek_as_array;
  for (int attempts = 0; attempts < max_attempts;
       (++attempts), (pos += FIO_MAP_CUCKOO_STEPS)) {
    const FIO_MAP_SIZE_TYPE desired_hash =
        FIO_NAME(FIO_MAP_NAME, __hash2imap)(pos, m->bits);
    /* each attempt tests a group of 5 slots with high cache locality */
    for (int byte = 0, offset = 0; byte < 5; (++byte), (offset += byte)) {
      const FIO_MAP_SIZE_TYPE index = (pos + offset) & pos_mask;
      /* the last slot is reserved for marking deleted items, not allocated. */
      if (index == pos_mask) {
        continue;
      }
      /* return if there's an available slot (no need to look further) */
      if (!imap[index]) {
        i.i = index;
        if (set_hash)
          imap[index] = desired_hash;
        return i;
      }
      /* test cache friendly partial match */
      if ((imap[index] & hashed_mask) == desired_hash || !hashed_mask) {
        /* test full hash */
        FIO_MAP_SIZE_TYPE a_index = imap[index] & pos_mask;
        if (a_index != pos_mask) {
          if (m->map[a_index].hash == hash) {
            /* test full collisions (attack) / match */
            if (m->under_attack ||
                FIO_MAP_OBJ_KEY_CMP(m->map[a_index].obj, key)) {
              i.i = index;
              i.a = a_index;
              return i;
            } else if (++total_collisions >= FIO_MAP_MAX_FULL_COLLISIONS) {
              m->under_attack = 1;
              FIO_LOG_SECURITY("Ordered map under attack?");
            }
          }
        }
      } else if (i.i == (FIO_MAP_SIZE_TYPE)-1LL &&
                 (imap[index] & pos_mask) == pos_mask) {
        /* (recycling) mark first available slot in the group */
        i.i = index;
        set_hash *= desired_hash;
      }
    }
  }

  if (set_hash && i.i != (FIO_MAP_SIZE_TYPE)-1LL)
    imap[i.i] = set_hash;

  return i;

seek_as_array:
  pos = 0;
  if (m->w < FIO_MAP_CAPA(m->bits))
    i.i = m->w;
  while (pos < m->w) {
    if (m->map[pos].hash == hash) {
      /* test full collisions (attack) / match */
      if (m->under_attack || FIO_MAP_OBJ_KEY_CMP(m->map[pos].obj, key)) {
        i.i = pos;
        i.a = pos;
        return i;
      } else if (++total_collisions >= FIO_MAP_MAX_FULL_COLLISIONS) {
        m->under_attack = 1;
        FIO_LOG_SECURITY("Ordered map under attack?");
      }
    } else if (!m->map[pos].hash && i.i > pos) {
      i.i = pos;
    }
    ++pos;
  }
  if (set_hash && i.i != (FIO_MAP_SIZE_TYPE)-1LL)
    imap[i.i] = FIO_NAME(FIO_MAP_NAME, __hash2imap)(hash, m->bits);
  return i;

  (void)key; /* if unused */
}

FIO_IFUNC int FIO_NAME(FIO_MAP_NAME, __realloc)(FIO_NAME(FIO_MAP_NAME, s) * m,
                                                size_t bits) {
  if (!m || bits > (sizeof(FIO_MAP_SIZE_TYPE) * 8))
    return -1;
  // if (bits < 3)
  //   bits = 3;
  if (bits != m->bits) {
    FIO_NAME(FIO_MAP_NAME, node_s) *tmp =
        (FIO_NAME(FIO_MAP_NAME, node_s) *)FIO_MEM_REALLOC_(
            m->map,
            FIO_MAP_MEMORY_SIZE(m->bits),
            FIO_MAP_MEMORY_SIZE(bits),
            (m->w * sizeof(*m->map)));
    if (!tmp)
      return -1;
    m->map = tmp;
    m->bits = (uint8_t)bits;
  }
  if (!FIO_MEM_REALLOC_IS_SAFE_ || bits == m->bits)
    memset(FIO_NAME(FIO_MAP_NAME, __imap)(m),
           0,
           sizeof(FIO_MAP_SIZE_TYPE) * FIO_MAP_CAPA(bits));
  /* rehash the map */
  if (m->count) {
    register FIO_MAP_SIZE_TYPE *const imap = FIO_NAME(FIO_MAP_NAME, __imap)(m);
    /* scan map for used slots to re-insert data */
    register const FIO_MAP_SIZE_TYPE end = m->w;
    if (m->w == m->count) {
      /* no holes, we can quickly run through the array and reindex */
      FIO_MAP_SIZE_TYPE i = 0;
      do {
        if (m->map[i].hash) {
          FIO_NAME(FIO_MAP_NAME, __pos_s)
          pos = FIO_NAME(
              FIO_MAP_NAME,
              __index)(m, m->map[i].hash, FIO_MAP_OBJ2KEY(m->map[i].obj), 1);
          if (pos.i == (FIO_MAP_SIZE_TYPE)-1LL)
            goto error;
          imap[pos.i] |= i;
        }
        i++;
      } while (i < end);
    } else {
      /* the array has holes -o compact the array while reindexing */
      FIO_MAP_SIZE_TYPE r = 0, w = 0;
      do {
#if FIO_MAP_EVICT_LRU
        if (w != r) {
          FIO_MAP_SIZE_TYPE head = m->map[r].node.next;
          m->map[w++] = m->map[r];
          if (m->last_used == r)
            m->last_used = w;
          FIO_INDEXED_LIST_REMOVE(m->map, node, r);
          FIO_INDEXED_LIST_PUSH(m->map, node, head, w);
        }
#else
        m->map[w++] = m->map[r];
#endif /* FIO_MAP_EVICT_LRU */
        if (m->map[r].hash) {
          FIO_NAME(FIO_MAP_NAME, __pos_s)
          pos = FIO_NAME(
              FIO_MAP_NAME,
              __index)(m, m->map[r].hash, FIO_MAP_OBJ2KEY(m->map[r].obj), 1);
          if (pos.i == (FIO_MAP_SIZE_TYPE)-1)
            goto error;
          imap[pos.i] |= r;
        }
        r++;
      } while (r < end);
      FIO_ASSERT_DEBUG(w == m->count, "rehashing logic error @ ordered map");
    }
  }
  return 0;
error:
  return -1;
}

FIO_IFUNC void FIO_NAME(FIO_MAP_NAME,
                        __destroy_all_objects)(FIO_NAME(FIO_MAP_NAME, s) * m) {
#if !FIO_MAP_TYPE_DESTROY_SIMPLE || !FIO_MAP_KEY_DESTROY_SIMPLE
  for (FIO_MAP_SIZE_TYPE i = 0; i < m->w; ++i) {
    if (!m->map[i].hash)
      continue;
    FIO_MAP_OBJ_DESTROY(m->map[i].obj);
#if DEBUG
    --m->count;
#endif
  }
  FIO_ASSERT_DEBUG(!m->count, "logic error @ ordered map clear.");
#else
  (void)m; /* no-op*/
#endif
}

/* *****************************************************************************
Unordered Map Implementation - API implementation
***************************************************************************** */

/* Frees any internal data AND the object's container! */
SFUNC void FIO_NAME(FIO_MAP_NAME, destroy)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);
  /* add destruction logic */
  FIO_NAME(FIO_MAP_NAME, __destroy_all_objects)(m);
  FIO_MEM_FREE_(m->map, FIO_MAP_MEMORY_SIZE(m->bits));
  *m = (FIO_NAME(FIO_MAP_NAME, s))FIO_MAP_INIT;
  return;
}

/* *****************************************************************************
Get / Set / Remove
***************************************************************************** */

SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, get_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
                                                    FIO_MAP_OBJ_KEY key) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
  FIO_NAME(FIO_MAP_NAME, __pos_s)
  pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key, 0);
  if (pos.a == (FIO_MAP_SIZE_TYPE)(-1) || !m->map[pos.a].hash)
    return NULL;
#if FIO_MAP_EVICT_LRU
  if (m->last_used != pos.a) {
    FIO_INDEXED_LIST_REMOVE(m->map, node, pos.a);
    FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos.a);
    m->last_used = pos.a;
  }
#endif /* FIO_MAP_EVICT_LRU */
  return &FIO_MAP_OBJ2TYPE(m->map[pos.a].obj);
}

SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, set_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                    FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                    FIO_MAP_TYPE obj,
                                                    FIO_MAP_TYPE *old,
                                                    uint8_t overwrite) {
  if (old)
    *old = FIO_MAP_TYPE_INVALID;
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
  /* make sure there's room in the value array */
  if (m->w + 1 == FIO_MAP_CAPA(m->bits))
    FIO_NAME(FIO_MAP_NAME, __realloc)(m, m->bits + (m->w == m->count));

#ifdef FIO_MAP_KEY
  FIO_NAME(FIO_MAP_NAME, __pos_s)
  pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key, 1);
#else
  FIO_NAME(FIO_MAP_NAME, __pos_s)
  pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, obj, 1);
#endif /* FIO_MAP_KEY */

  for (int i = 0; pos.i == (FIO_MAP_SIZE_TYPE)-1LL && i < 2; ++i) {
    if (FIO_NAME(FIO_MAP_NAME, __realloc)(m, m->bits + 1))
      continue;
#ifdef FIO_MAP_KEY
    pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key, 1);
#else
    pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, obj, 1);
#endif /* FIO_MAP_KEY */
  }
  if (pos.i == (FIO_MAP_SIZE_TYPE)-1LL)
    goto error;
  if (pos.a == (FIO_MAP_SIZE_TYPE)-1LL || !m->map[pos.a].hash) {
    /* new */
    if (pos.a == (FIO_MAP_SIZE_TYPE)-1LL)
      pos.a = m->w++;
#if FIO_MAP_MAX_ELEMENTS
    if (m->count >= FIO_MAP_MAX_ELEMENTS) {
      FIO_NAME(FIO_MAP_NAME, evict)(map, 1);
    }
#endif
    FIO_NAME(FIO_MAP_NAME, __imap)
    (m)[pos.i] |= pos.a;
    m->map[pos.a].hash = hash;
    FIO_MAP_TYPE_COPY(FIO_MAP_OBJ2TYPE(m->map[pos.a].obj), obj);
    FIO_MAP_KEY_COPY(FIO_MAP_OBJ2KEY(m->map[pos.a].obj), key);
#if FIO_MAP_EVICT_LRU
    if (m->count) {
      FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos.a);
    } else {
      m->map[pos.a].node.prev = m->map[pos.a].node.next = pos.a;
    }
    m->last_used = pos.a;
#endif /* FIO_MAP_EVICT_LRU */
    ++m->count;
  } else if (overwrite &&
             FIO_MAP_SHOULD_OVERWRITE(FIO_MAP_OBJ2TYPE(m->map[pos.a].obj),
                                      obj)) {
    /* overwrite existing */
    FIO_MAP_KEY_DISCARD(key);
    if (old) {
      FIO_MAP_TYPE_COPY(old[0], FIO_MAP_OBJ2TYPE(m->map[pos.a].obj));
      if (FIO_MAP_DESTROY_AFTER_COPY) {
        FIO_MAP_TYPE_DESTROY(FIO_MAP_OBJ2TYPE(m->map[pos.a].obj));
      }
    } else {
      FIO_MAP_TYPE_DESTROY(FIO_MAP_OBJ2TYPE(m->map[pos.a].obj));
    }
    FIO_MAP_TYPE_COPY(FIO_MAP_OBJ2TYPE(m->map[pos.a].obj), obj);
#if FIO_MAP_EVICT_LRU
    if (m->last_used != pos.a) {
      FIO_INDEXED_LIST_REMOVE(m->map, node, pos.a);
      FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos.a);
      m->last_used = pos.a;
    }
#endif /* FIO_MAP_EVICT_LRU */
  } else {
    FIO_MAP_TYPE_DISCARD(obj);
    FIO_MAP_KEY_DISCARD(key);
  }
  return &FIO_MAP_OBJ2TYPE(m->map[pos.a].obj);

error:
  FIO_MAP_TYPE_DISCARD(obj);
  FIO_MAP_KEY_DISCARD(key);
  return NULL;
}

SFUNC int FIO_NAME(FIO_MAP_NAME, remove)(FIO_MAP_PTR map,
                                         FIO_MAP_HASH hash,
                                         FIO_MAP_OBJ_KEY key,
                                         FIO_MAP_TYPE *old) {
  if (old)
    *old = FIO_MAP_TYPE_INVALID;
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m || !m->count)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
  FIO_NAME(FIO_MAP_NAME, __pos_s)
  pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key, 0);
  if (pos.a == (FIO_MAP_SIZE_TYPE)(-1) || pos.i == (FIO_MAP_SIZE_TYPE)(-1) ||
      !m->map[pos.a].hash)
    return -1;
  FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos.i] = ~(FIO_MAP_SIZE_TYPE)0;
  m->map[pos.a].hash = 0;
  --m->count;
  if (old) {
    FIO_MAP_TYPE_COPY(*old, FIO_MAP_OBJ2TYPE(m->map[pos.a].obj));
    FIO_MAP_OBJ_DESTROY_AFTER(m->map[pos.a].obj);
  } else {
    FIO_MAP_OBJ_DESTROY(m->map[pos.a].obj);
  }
#if FIO_MAP_EVICT_LRU
  if (pos.a == m->last_used)
    m->last_used = m->map[pos.a].node.next;
  FIO_INDEXED_LIST_REMOVE(m->map, node, pos.a);
#endif
  if (!m->count)
    m->w = 0;
  else if (pos.a + 1 == m->w) {
    --m->w;
    while (m->w && !m->map[m->w - 1].hash)
      --m->w;
  }
  return 0;
}

SFUNC void FIO_NAME(FIO_MAP_NAME, clear)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);
  FIO_NAME(FIO_MAP_NAME, __destroy_all_objects)(m);
  memset(FIO_NAME(FIO_MAP_NAME, __imap)(m), 0, FIO_MAP_CAPA(m->bits));
  m->under_attack = 0;
  m->count = m->w = 0;
#if FIO_MAP_EVICT_LRU
  m->last_used = 0;
#endif
}

SFUNC int FIO_NAME(FIO_MAP_NAME, evict)(FIO_MAP_PTR map,
                                        size_t number_of_elements) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, -1);
  if (!m->count)
    return -1;
  if (number_of_elements >= m->count) {
    FIO_NAME(FIO_MAP_NAME, clear)(map);
    return -1;
  }
#if FIO_MAP_EVICT_LRU
  /* evict by LRU */
  do {
    FIO_MAP_SIZE_TYPE n = m->map[m->last_used].node.prev;
    FIO_NAME(FIO_MAP_NAME, remove)
    (map, m->map[n].hash, FIO_MAP_OBJ2KEY(m->map[n].obj), NULL);
  } while (--number_of_elements);
#else  /* FIO_MAP_EVICT_LRU */
  /* scan map and evict FIFO. */
  for (FIO_MAP_SIZE_TYPE i = 0; number_of_elements && i < m->w; ++i) {
    /* skip empty groups (test for all bytes == 0 || 255 */
    if (m->map[i].hash) {
      FIO_NAME(FIO_MAP_NAME, remove)
      (map, m->map[i].hash, FIO_MAP_OBJ2KEY(m->map[i].obj), NULL);
      --number_of_elements; /* stop evicting? */
    }
  }
#endif /* FIO_MAP_EVICT_LRU */
  return 0;
}

/* *****************************************************************************
Object state information
***************************************************************************** */

/** Reservse enough space for a theoretical capacity of `capa` objects. */
SFUNC size_t FIO_NAME(FIO_MAP_NAME, reserve)(FIO_MAP_PTR map,
                                             FIO_MAP_SIZE_TYPE capa) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  if (FIO_MAP_CAPA(m->bits) < capa) {
    size_t bits = 3;
    while (FIO_MAP_CAPA(bits) < capa)
      ++bits;
    for (int i = 0; FIO_NAME(FIO_MAP_NAME, __realloc)(m, bits + i) && i < 2;
         ++i) {
    }
    if (m->bits < bits)
      return 0;
  }
  return FIO_MAP_CAPA(m->bits);
}

/** Attempts to minimize memory use. */
SFUNC void FIO_NAME(FIO_MAP_NAME, compact)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);
  if (!m->bits)
    return;
  if (!m->count) {
    FIO_NAME(FIO_MAP_NAME, destroy)(map);
    return;
  }
  size_t bits = m->bits;
  size_t count = 0;
  while (bits && FIO_MAP_CAPA((bits - 1)) > m->count) {
    --bits;
    ++count;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!FIO_NAME(FIO_MAP_NAME, __realloc)(m, bits + i))
      return;
  }
}

/** Rehashes the map. No need to call this, rehashing is automatic. */
SFUNC int FIO_NAME(FIO_MAP_NAME, rehash)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, -1);
  return FIO_NAME(FIO_MAP_NAME, __realloc)(m, m->bits);
}

/* *****************************************************************************
Iteration
***************************************************************************** */

/**
 * Iteration using a callback for each element in the map.
 *
 * The callback task function must accept an element variable as well as an
 * opaque user pointer.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
SFUNC FIO_MAP_SIZE_TYPE
FIO_NAME(FIO_MAP_NAME, each)(FIO_MAP_PTR map,
                             int (*task)(FIO_NAME(FIO_MAP_NAME, each_s) *),
                             void *udata,
                             ssize_t start_at) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, (FIO_MAP_SIZE_TYPE)-1);
  FIO_MAP_SIZE_TYPE count = m->count;
  if (start_at < 0) {
    start_at = count - start_at;
    if (start_at < 0)
      start_at = 0;
  }
  if ((FIO_MAP_SIZE_TYPE)start_at >= count)
    return count;
  FIO_MAP_SIZE_TYPE pos = 0;
  FIO_NAME(FIO_MAP_NAME, each_s)
  e = {
      .parent = map,
      .index = (uint64_t)start_at,
#ifdef FIO_MAP_KEY
      .items_at_index = 2,
#else
      .items_at_index = 1,
#endif
      .task = task,
      .udata = udata,
  };

  if (m->w == m->count) {
    while (e.index < m->count) {
      e.value = FIO_MAP_OBJ2TYPE(m->map[e.index].obj);
#ifdef FIO_MAP_KEY
      e.key = FIO_MAP_OBJ2KEY(m->map[e.index].obj);
#endif
      int r = e.task(&e);
      ++e.index;
      if (r == -1)
        break;
    }
    return (FIO_MAP_SIZE_TYPE)(e.index);
  }

  pos = 0;
  while (start_at && pos < m->w) {
    if (!m->map[pos++].hash) {
      continue;
    }
    --start_at;
  }

  if (start_at)
    return m->count;

  while (e.index < m->count && pos < m->w) {
    if (m->map[pos].hash) {
      e.value = FIO_MAP_OBJ2TYPE(m->map[pos].obj);
#ifdef FIO_MAP_KEY
      e.key = FIO_MAP_OBJ2KEY(m->map[pos].obj);
#endif
      int r = e.task(&e);
      ++e.index;
      if (r == -1)
        break;
    }
    ++pos;
  }
  return e.index;
}

/* *****************************************************************************
Ordered Map Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_MAP_NAME */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_UMAP_NAME map           /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#include "210 map api.h"            /* Development inclusion - ignore line */
#define FIO_MAP_TEST                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                  Unordered Map - an Unordered Hash Map / Set




***************************************************************************** */
#if defined(FIO_MAP_NAME) && !FIO_MAP_ORDERED

/* *****************************************************************************



Unordered Map Types - Implementation



TODO?
Benchmark: https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html
***************************************************************************** */

/** An Unordered Map Type */
struct FIO_NAME(FIO_MAP_NAME, s) {
  /** Internal map / memory - do not access directly */
  FIO_NAME(FIO_MAP_NAME, node_s) * map;
  /** Object count - do not access directly */
  FIO_MAP_SIZE_TYPE count;
#if FIO_MAP_EVICT_LRU
  /** LRU eviction monitoring - do not access directly */
  FIO_MAP_SIZE_TYPE last_used;
#endif /* FIO_MAP_EVICT_LRU */
  uint8_t bits;
  uint8_t under_attack;
};

/* *****************************************************************************
Unordered Map Implementation - inlined static functions
***************************************************************************** */

#ifndef FIO_MAP_CAPA
#define FIO_MAP_CAPA(bits) ((uintptr_t)1ULL << (bits))
#endif

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC FIO_MAP_PTR FIO_NAME(FIO_MAP_NAME, new)(void) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*m), 0);
  if (!m)
    return (FIO_MAP_PTR)NULL;
  *m = (FIO_NAME(FIO_MAP_NAME, s))FIO_MAP_INIT;
  return (FIO_MAP_PTR)FIO_PTR_TAG(m);
}
/* Frees any internal data AND the object's container! */
FIO_IFUNC int FIO_NAME(FIO_MAP_NAME, free)(FIO_MAP_PTR map) {
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  FIO_NAME(FIO_MAP_NAME, destroy)(map);
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  FIO_MEM_FREE_(m, sizeof(*m));
  return 0;
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Internal helper - do not access */
FIO_IFUNC uint8_t *FIO_NAME(FIO_MAP_NAME,
                            __imap)(FIO_NAME(FIO_MAP_NAME, s) * m) {
  return (uint8_t *)(m->map + FIO_MAP_CAPA(m->bits));
}

FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, count)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  return m->count;
}

FIO_IFUNC size_t FIO_NAME(FIO_MAP_NAME, capa)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  return FIO_MAP_CAPA(m->bits);
}

FIO_IFUNC FIO_NAME(FIO_MAP_NAME, node_s) *
    FIO_NAME(FIO_MAP_NAME, each_next)(FIO_MAP_PTR map,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * *first,
                                      FIO_NAME(FIO_MAP_NAME, node_s) * pos) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m || !first)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  if (!m->count || !m->map)
    return NULL;
  size_t i;
#if FIO_MAP_EVICT_LRU
  FIO_MAP_SIZE_TYPE next;
  if (!pos) {
    i = m->last_used;
    *first = m->map;
    return m->map + i;
  }
  i = pos - *first;
  *first = m->map; /* was it updated? */
  next = m->map[i].node.next;
  if (next == m->last_used)
    return NULL;
  return m->map + next;

#else  /*FIO_MAP_EVICT_LRU*/
  if (!pos || !(*first)) {
    i = -1;
  } else {
    i = pos - *first;
  }
  ++i;
  *first = m->map;
  uint8_t *imap = FIO_NAME(FIO_MAP_NAME, __imap)(m);
  while (i + 8 < FIO_MAP_CAPA(m->bits)) {
    /* test only groups with valid values (test for all bytes == 0 || 255 */
    register uint64_t row = fio_buf2u64_local(imap + i);
    row = (fio_has_full_byte64(row) | fio_has_zero_byte64(row));
    if (row != UINT64_C(0x8080808080808080)) {
      row ^= UINT64_C(0x8080808080808080);
      for (int j = 0; j < 8; ++j) {
        if ((row & UINT64_C(0x80))) {
          return m->map + i + j;
        }
        row >>= 8;
      }
      i += 8;
    }
  }
  while (i < FIO_MAP_CAPA(m->bits)) {
    if (imap[i] && imap[i] != 255)
      return m->map + i;
    ++i;
  }
  return NULL;
#endif /* FIO_MAP_EVICT_LRU */
}
/* *****************************************************************************
Unordered Map Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

#ifndef FIO_MAP_MEMORY_SIZE
#define FIO_MAP_MEMORY_SIZE(bits)                                              \
  ((sizeof(FIO_NAME(FIO_MAP_NAME, node_s)) + sizeof(uint8_t)) *                \
   FIO_MAP_CAPA(bits))
#endif

/* *****************************************************************************
Unordered Map Implementation - helper functions.
***************************************************************************** */

#ifndef FIO_MAP___IMAP_DELETED
#define FIO_MAP___IMAP_DELETED 255
#endif
#ifndef FIO_MAP___IMAP_FREE
#define FIO_MAP___IMAP_FREE 0
#endif

FIO_IFUNC FIO_MAP_SIZE_TYPE FIO_NAME(FIO_MAP_NAME,
                                     __hash2imap)(FIO_MAP_HASH hash,
                                                  uint8_t bits) {
  FIO_MAP_SIZE_TYPE r = (((hash >> bits) ^ hash) & 255);
  if (!r || r == 255)
    r ^= 1;
  return r;
}

FIO_SFUNC FIO_MAP_SIZE_TYPE FIO_NAME(FIO_MAP_NAME,
                                     __index)(FIO_NAME(FIO_MAP_NAME, s) * m,
                                              const FIO_MAP_HASH hash,
                                              FIO_MAP_OBJ_KEY key) {
  FIO_MAP_SIZE_TYPE pos = (FIO_MAP_SIZE_TYPE)-1LL;
  FIO_MAP_SIZE_TYPE free_slot = (FIO_MAP_SIZE_TYPE)-1LL;
  size_t total_collisions = 0;
  if (!m->map)
    return pos;
  const uint8_t *imap = FIO_NAME(FIO_MAP_NAME, __imap)(m);
  /* note: hash MUST be normalized by this point */
  const uint64_t simd_base =
      FIO_NAME(FIO_MAP_NAME, __hash2imap)(hash, m->bits) *
      UINT64_C(0x0101010101010101);
  const FIO_MAP_SIZE_TYPE pos_mask = FIO_MAP_CAPA(m->bits) - 1;
  const int max_attempts = (FIO_MAP_CAPA(m->bits) >> 3) >= FIO_MAP_MAX_SEEK
                               ? (int)FIO_MAP_MAX_SEEK
                               : (FIO_MAP_CAPA(m->bits) >> 3);
  if (m->bits <= FIO_MAP_SEEK_AS_ARRAY_LOG_LIMIT)
    goto seek_as_array;
  /* we perrform X attempts using large cuckoo steps */
  pos = hash;
  for (int attempts = 0; attempts < max_attempts;
       (++attempts), (pos += FIO_MAP_CUCKOO_STEPS)) {
    /* each attempt test a group of 8 slots spaced by a few bytes (comb) */
    const uint8_t offsets[] = {0, 3, 7, 12, 18, 25, 33, 41};
    const uint64_t comb =
        (uint64_t)imap[(pos + offsets[0]) & pos_mask] |
        ((uint64_t)imap[(pos + offsets[1]) & pos_mask] << (1 * 8)) |
        ((uint64_t)imap[(pos + offsets[2]) & pos_mask] << (2 * 8)) |
        ((uint64_t)imap[(pos + offsets[3]) & pos_mask] << (3 * 8)) |
        ((uint64_t)imap[(pos + offsets[4]) & pos_mask] << (4 * 8)) |
        ((uint64_t)imap[(pos + offsets[5]) & pos_mask] << (5 * 8)) |
        ((uint64_t)imap[(pos + offsets[6]) & pos_mask] << (6 * 8)) |
        ((uint64_t)imap[(pos + offsets[7]) & pos_mask] << (7 * 8));
    uint64_t simd_result = simd_base ^ comb;
    simd_result = fio_has_zero_byte64(simd_result);

    /* test for exact match in each of the bytes in the 8 byte group */
    /* note: the MSB is 1 for both (x-1) and (~x) only if x == 0. */
    if (simd_result) {
      for (int i = 0; simd_result; ++i) {
        /* test cache friendly 8bit match */
        if ((simd_result & UINT64_C(0x80))) {
          /* test full hash */
          register FIO_MAP_HASH obj_hash =
              FIO_MAP_HASH_GET_HASH(m, ((pos + offsets[i]) & pos_mask));
          if (obj_hash == hash) {
            /* test full collisions (attack) / match */
            if (m->under_attack ||
                FIO_MAP_OBJ_KEY_CMP(m->map[(pos + offsets[i]) & pos_mask].obj,
                                    key)) {
              pos = (pos + offsets[i]) & pos_mask;
              return pos;
            } else if (++total_collisions >= FIO_MAP_MAX_FULL_COLLISIONS) {
              m->under_attack = 1;
              FIO_LOG_SECURITY("Unordered map under attack?");
            }
          }
        }
        simd_result >>= 8;
      }
    }
    /* test if there's an available slot in the group */
    if (free_slot == (FIO_MAP_SIZE_TYPE)-1LL &&
        (simd_result =
             (fio_has_zero_byte64(comb) | fio_has_full_byte64(comb)))) {
      for (int i = 0; simd_result; ++i) {
        if (simd_result & UINT64_C(0x80)) {
          free_slot = (pos + offsets[i]) & pos_mask;
          break;
        }
        simd_result >>= 8;
      }
    }
    /* test if there's a free slot in the group (never used => stop seeking) */
    /* note: the MSB is 1 for both (x-1) and (~x) only if x == 0. */
    if (fio_has_zero_byte64(comb))
      break;
  }

  pos = free_slot;
  return pos;

seek_as_array:

  if (m->count < FIO_MAP_CAPA(m->bits))
    free_slot = m->count;
  pos = 0;
  while (pos < m->count) {
    switch (imap[pos]) {
    case 0:
      return pos;
    case 255:
      if (free_slot > pos)
        free_slot = pos;
      break;
    default:
      if (imap[pos] == (uint8_t)(simd_base & 0xFF)) {
        FIO_MAP_HASH obj_hash = FIO_MAP_HASH_GET_HASH(m, pos);
        if (obj_hash == hash) {
          /* test full collisions (attack) / match */
          if (m->under_attack || FIO_MAP_OBJ_KEY_CMP(m->map[pos].obj, key)) {
            return pos;
          } else if (++total_collisions >= FIO_MAP_MAX_FULL_COLLISIONS) {
            m->under_attack = 1;
            FIO_LOG_SECURITY("Unordered map under attack?");
          }
        }
      }
    }
    ++pos;
  }
  pos = free_slot;
  return pos;

  (void)key; /* if unused */
}

FIO_IFUNC int FIO_NAME(FIO_MAP_NAME, __realloc)(FIO_NAME(FIO_MAP_NAME, s) * m,
                                                size_t bits) {
  if (!m || bits >= (sizeof(FIO_MAP_SIZE_TYPE) * 8))
    return -1;
  FIO_NAME(FIO_MAP_NAME, node_s) *tmp = (FIO_NAME(FIO_MAP_NAME, node_s) *)
      FIO_MEM_REALLOC_(NULL, 0, FIO_MAP_MEMORY_SIZE(bits), 0);
  if (!tmp)
    return -1;
  if (!FIO_MEM_REALLOC_IS_SAFE_)
    memset(tmp, 0, FIO_MAP_MEMORY_SIZE(bits));
  /* rehash the map */
  FIO_NAME(FIO_MAP_NAME, s) m2;
  m2 = (FIO_NAME(FIO_MAP_NAME, s)){
      .map = tmp,
      .bits = (uint8_t)bits,
  };
  if (m->count) {
#if FIO_MAP_EVICT_LRU
    /* use eviction list to re-insert data. */
    FIO_MAP_SIZE_TYPE last = 0;
    FIO_INDEXED_LIST_EACH(m->map, node, m->last_used, i) {
      /* place old values in new hash */
      FIO_MAP_HASH obj_hash = FIO_MAP_HASH_GET_HASH(m, i);
      FIO_MAP_SIZE_TYPE pos =
          FIO_NAME(FIO_MAP_NAME,
                   __index)(&m2, obj_hash, FIO_MAP_OBJ2KEY(m->map[i].obj));
      if (pos == (FIO_MAP_SIZE_TYPE)-1)
        goto error;
      FIO_NAME(FIO_MAP_NAME, __imap)
      (&m2)[pos] = FIO_NAME(FIO_MAP_NAME, __hash2imap)(obj_hash, m2.bits);
#if FIO_MAP_HASH_CACHED
      m2.map[pos].hash = obj_hash;
#endif /* FIO_MAP_HASH_CACHED */
      m2.map[pos].obj = m->map[i].obj;
      if (m2.count) {
        FIO_INDEXED_LIST_PUSH(m2.map, node, last, pos);
      } else {
        m2.map[pos].node.prev = m2.map[pos].node.next = pos;
        m2.last_used = pos;
      }
      last = pos;
      ++m2.count;
    }
#else /* FIO_MAP_EVICT_LRU */
    /* scan map for used slots to re-insert data */
    if (FIO_MAP_CAPA(m->bits) > 8) {
      uint64_t *imap64 = (uint64_t *)FIO_NAME(FIO_MAP_NAME, __imap)(m);
      for (FIO_MAP_SIZE_TYPE i = 0;
           m2.count < m->count && i < FIO_MAP_CAPA(m->bits);
           i += 8) {
        /* skip empty groups (test for all bytes == 0) (can we test == 255?) */
        uint64_t result = (fio_has_zero_byte64(imap64[(i >> 3)]) |
                           fio_has_full_byte64(imap64[(i >> 3)]));
        if (result == UINT64_C(0x8080808080808080))
          continue;
        result ^= UINT64_C(0x8080808080808080);
        for (int j = 0; j < 8 && result; ++j) {
          const FIO_MAP_SIZE_TYPE n = i + j;
          if ((result & UINT64_C(0x80))) {
            /* place in new hash */
            FIO_MAP_HASH obj_hash = FIO_MAP_HASH_GET_HASH(m, n);
            FIO_MAP_SIZE_TYPE pos = FIO_NAME(
                FIO_MAP_NAME,
                __index)(&m2, obj_hash, FIO_MAP_OBJ2KEY(m->map[n].obj));
            if (pos == (FIO_MAP_SIZE_TYPE)-1)
              goto error;
            FIO_NAME(FIO_MAP_NAME, __imap)
            (&m2)[pos] = FIO_NAME(FIO_MAP_NAME, __hash2imap)(obj_hash, m2.bits);
            m2.map[pos] = m->map[n];
#if FIO_MAP_EVICT_LRU
            if (!m2.count) {
              m2.last_used = pos;
              m2.map[pos].node.prev = m2.map[pos].node.next = pos;
            }
            FIO_INDEXED_LIST_PUSH(m2.map, node, m2.last_used, pos);
            if (m->last_used == n)
              m2.last_used = pos;
#endif /* FIO_MAP_EVICT_LRU */
            ++m2.count;
          }
          result >>= 8;
        }
      }
    } else {
      for (FIO_MAP_SIZE_TYPE i = 0; m->count && i < FIO_MAP_CAPA(m->bits);
           ++i) {
        if (FIO_NAME(FIO_MAP_NAME, __imap)(m)[i] &&
            FIO_NAME(FIO_MAP_NAME, __imap)(m)[i] != 255) {
          FIO_MAP_HASH obj_hash = FIO_MAP_HASH_GET_HASH(m, i);
          FIO_MAP_SIZE_TYPE pos =
              FIO_NAME(FIO_MAP_NAME,
                       __index)(&m2, obj_hash, FIO_MAP_OBJ2KEY(m->map[i].obj));
          if (pos == (FIO_MAP_SIZE_TYPE)-1)
            goto error;
          FIO_NAME(FIO_MAP_NAME, __imap)
          (&m2)[pos] = FIO_NAME(FIO_MAP_NAME, __hash2imap)(obj_hash, m2.bits);
          m2.map[pos] = m->map[i];
          ++m2.count;
        }
      }
    }
#endif /* FIO_MAP_EVICT_LRU */
  }

  FIO_MEM_FREE_(m->map, FIO_MAP_MEMORY_SIZE(m->bits));
  *m = m2;
  return 0;
error:
  FIO_MEM_FREE_(tmp, FIO_MAP_MEMORY_SIZE(bits));
  return -1;
}

/* *****************************************************************************
Unordered Map Implementation - API implementation
*****************************************************************************
*/

/* Frees any internal data AND the object's container! */
SFUNC void FIO_NAME(FIO_MAP_NAME, destroy)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);
  FIO_NAME(FIO_MAP_NAME, clear)(map);
  FIO_MEM_FREE_(m->map, FIO_MAP_MEMORY_SIZE(m->bits));
  *m = (FIO_NAME(FIO_MAP_NAME, s))FIO_MAP_INIT;
  return;
}

/* *****************************************************************************
Get / Set / Remove
*****************************************************************************
*/

SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, get_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
                                                    FIO_MAP_OBJ_KEY key) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
  FIO_MAP_SIZE_TYPE pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key);
  if (pos == (FIO_MAP_SIZE_TYPE)(-1) ||
      FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos] == 255 ||
      !FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos])
    return NULL;
#if FIO_MAP_EVICT_LRU
  if (m->last_used != pos) {
    FIO_INDEXED_LIST_REMOVE(m->map, node, pos);
    FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos);
    m->last_used = pos;
  }
#endif /* FIO_MAP_EVICT_LRU */
  return &FIO_MAP_OBJ2TYPE(m->map[pos].obj);
}

SFUNC FIO_MAP_TYPE *FIO_NAME(FIO_MAP_NAME, set_ptr)(FIO_MAP_PTR map,
                                                    FIO_MAP_HASH hash,
#ifdef FIO_MAP_KEY
                                                    FIO_MAP_KEY key,
#endif /* FIO_MAP_KEY */
                                                    FIO_MAP_TYPE obj,
                                                    FIO_MAP_TYPE *old,
                                                    uint8_t overwrite) {
  if (old)
    *old = FIO_MAP_TYPE_INVALID;
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return NULL;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
#ifdef FIO_MAP_KEY
  FIO_MAP_SIZE_TYPE pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key);
#else
  FIO_MAP_SIZE_TYPE pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, obj);
#endif /* FIO_MAP_KEY */

  for (int i = 0; pos == (FIO_MAP_SIZE_TYPE)-1 && i < 2; ++i) {
    if (FIO_NAME(FIO_MAP_NAME, __realloc)(m, m->bits + 1))
      goto error;
#ifdef FIO_MAP_KEY
    pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key);
#else
    pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, obj);
#endif /* FIO_MAP_KEY */
  }
  if (pos == (FIO_MAP_SIZE_TYPE)-1)
    goto error;
  if (!FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos] ||
      FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos] == 255) {
    /* new */
    FIO_NAME(FIO_MAP_NAME, __imap)
    (m)[pos] = FIO_NAME(FIO_MAP_NAME, __hash2imap)(hash, m->bits);
#if FIO_MAP_HASH_CACHED
    m->map[pos].hash = hash;
#endif
    FIO_MAP_TYPE_COPY(FIO_MAP_OBJ2TYPE(m->map[pos].obj), obj);
    FIO_MAP_KEY_COPY(FIO_MAP_OBJ2KEY(m->map[pos].obj), key);
#if FIO_MAP_EVICT_LRU
    if (m->count) {
      FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos);
    } else {
      m->map[pos].node.prev = m->map[pos].node.next = pos;
    }
    m->last_used = pos;
#endif /* FIO_MAP_EVICT_LRU */
    ++m->count;
  } else if (overwrite &&
             FIO_MAP_SHOULD_OVERWRITE(FIO_MAP_OBJ2TYPE(m->map[pos].obj), obj)) {
    /* overwrite existing */
    FIO_MAP_KEY_DISCARD(key);
    if (old) {
      FIO_MAP_TYPE_COPY(old[0], FIO_MAP_OBJ2TYPE(m->map[pos].obj));
      if (FIO_MAP_DESTROY_AFTER_COPY) {
        FIO_MAP_TYPE_DESTROY(FIO_MAP_OBJ2TYPE(m->map[pos].obj));
      }
    } else {
      FIO_MAP_TYPE_DESTROY(FIO_MAP_OBJ2TYPE(m->map[pos].obj));
    }
    FIO_MAP_TYPE_COPY(FIO_MAP_OBJ2TYPE(m->map[pos].obj), obj);
#if FIO_MAP_EVICT_LRU
    if (m->last_used != pos) {
      FIO_INDEXED_LIST_REMOVE(m->map, node, pos);
      FIO_INDEXED_LIST_PUSH(m->map, node, m->last_used, pos);
      m->last_used = pos;
    }
#endif /* FIO_MAP_EVICT_LRU */
  } else {
    FIO_MAP_TYPE_DISCARD(obj);
    FIO_MAP_KEY_DISCARD(key);
  }
  return &FIO_MAP_OBJ2TYPE(m->map[pos].obj);

error:
  FIO_MAP_TYPE_DISCARD(obj);
  FIO_MAP_KEY_DISCARD(key);
  return NULL;
}

SFUNC int FIO_NAME(FIO_MAP_NAME, remove)(FIO_MAP_PTR map,
                                         FIO_MAP_HASH hash,
                                         FIO_MAP_OBJ_KEY key,
                                         FIO_MAP_TYPE *old) {
  if (old)
    *old = FIO_MAP_TYPE_INVALID;
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m || !m->count)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, NULL);
  hash = FIO_MAP_HASH_FIX(hash);
  FIO_MAP_SIZE_TYPE pos = FIO_NAME(FIO_MAP_NAME, __index)(m, hash, key);
  if (pos == (FIO_MAP_SIZE_TYPE)(-1) ||
      FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos] == 255 ||
      !FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos])
    return -1;
  FIO_NAME(FIO_MAP_NAME, __imap)(m)[pos] = 255;
#if FIO_MAP_HASH_CACHED
  m->map[pos].hash = 0;
#endif
  --m->count;
  if (old) {
    FIO_MAP_TYPE_COPY(*old, FIO_MAP_OBJ2TYPE(m->map[pos].obj));
    FIO_MAP_OBJ_DESTROY_AFTER(m->map[pos].obj);
  } else {
    FIO_MAP_OBJ_DESTROY(m->map[pos].obj);
  }
#if FIO_MAP_EVICT_LRU
  if (pos == m->last_used)
    m->last_used = m->map[pos].node.next;
  FIO_INDEXED_LIST_REMOVE(m->map, node, pos);
#endif
  return 0;
}

SFUNC void FIO_NAME(FIO_MAP_NAME, clear)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);

  /* scan map to clear data. */
  if (m->bits > 3) {
    uint64_t *imap64 = (uint64_t *)FIO_NAME(FIO_MAP_NAME, __imap)(m);
    for (FIO_MAP_SIZE_TYPE i = 0; m->count && i < FIO_MAP_CAPA(m->bits);
         i += 8) {
      /* skip empty groups (test for all bytes == 0 || 255 */
      register uint64_t row = imap64[i >> 3];
      row = (fio_has_full_byte64(row) | fio_has_zero_byte64(row));
      if (row == UINT64_C(0x8080808080808080)) {
        imap64[i >> 3] = 0;
        continue;
      }
      imap64[i >> 3] = 0;
      row ^= UINT64_C(0x8080808080808080);
      for (int j = 0; j < 8; ++j) {
        if ((row & UINT64_C(0x80))) {
          FIO_MAP_OBJ_DESTROY(m->map[i + j].obj);
#if FIO_MAP_HASH_CACHED
          m->map[i + j].hash = 0;
#endif
          --m->count; /* stop seeking if no more elements */
        }
        row >>= 8;
      }
    }
  } else {
    for (FIO_MAP_SIZE_TYPE i = 0; m->count && i < FIO_MAP_CAPA(m->bits); ++i) {
      if (FIO_NAME(FIO_MAP_NAME, __imap)(m)[i] &&
          FIO_NAME(FIO_MAP_NAME, __imap)(m)[i] != 255) {
        FIO_MAP_OBJ_DESTROY(m->map[i].obj);
        --m->count;
      }
    }
  }
  FIO_ASSERT_DEBUG(!m->count, "logic error @ unordered map clear.");
}

SFUNC int FIO_NAME(FIO_MAP_NAME, evict)(FIO_MAP_PTR map,
                                        size_t number_of_elements) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, -1);
  if (!m->count)
    return -1;
  if (number_of_elements >= m->count) {
    FIO_NAME(FIO_MAP_NAME, clear)(map);
    return -1;
  }
#if FIO_MAP_EVICT_LRU
  /* evict by LRU */
  do {
    FIO_MAP_SIZE_TYPE n = m->map[m->last_used].node.prev;
    FIO_INDEXED_LIST_REMOVE(m->map, node, n);
  } while (--number_of_elements);
#else /* FIO_MAP_EVICT_LRU */
  /* scan map and evict semi randomly. */
  uint64_t *imap64 = (uint64_t *)FIO_NAME(FIO_MAP_NAME, __imap)(m);
  for (FIO_MAP_SIZE_TYPE i = 0;
       number_of_elements && (i + 7) < FIO_MAP_CAPA(m->bits);
       i += 8) {
    /* skip empty groups (test for all bytes == 0 || 255 */
    register uint64_t row = imap64[i >> 3];
    row = (fio_has_full_byte64(row) | fio_has_zero_byte64(row));
    if (row == UINT64_C(0x8080808080808080))
      continue;
    row ^= UINT64_C(0x8080808080808080);
    for (int j = 0; number_of_elements && j < 8; ++j) {
      if ((row & UINT64_C(0x80))) {
        FIO_MAP_OBJ_DESTROY(m->map[i + j].obj);
#if FIO_MAP_HASH_CACHED
        m->map[i + j].hash = 0;
#endif
        FIO_NAME(FIO_MAP_NAME, __imap)(m)[i + j] = 255;
        --m->count;
        --number_of_elements; /* stop evicting? */
      }
      row >>= 8;
    }
  }

#endif /* FIO_MAP_EVICT_LRU */
  return -1;
}

/* *****************************************************************************
Object state information
*****************************************************************************
*/

/** Reservse enough space for a theoretical capacity of `capa` objects. */
SFUNC size_t FIO_NAME(FIO_MAP_NAME, reserve)(FIO_MAP_PTR map,
                                             FIO_MAP_SIZE_TYPE capa) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, 0);
  if (FIO_MAP_CAPA(m->bits) < capa) {
    size_t bits = 3;
    while (FIO_MAP_CAPA(bits) < capa)
      ++bits;
    for (int i = 0; FIO_NAME(FIO_MAP_NAME, __realloc)(m, bits + i) && i < 2;
         ++i) {
    }
    if (m->bits < bits)
      return 0;
  }
  return FIO_MAP_CAPA(m->bits);
}

/** Attempts to minimize memory use. */
SFUNC void FIO_NAME(FIO_MAP_NAME, compact)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(map);
  if (!m->bits)
    return;
  if (!m->count) {
    FIO_NAME(FIO_MAP_NAME, destroy)(map);
    return;
  }
  size_t bits = m->bits;
  size_t count = 0;
  while (bits && FIO_MAP_CAPA((bits - 1)) > m->count) {
    --bits;
    ++count;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!FIO_NAME(FIO_MAP_NAME, __realloc)(m, bits + i))
      return;
  }
}

/** Rehashes the map. No need to call this, rehashing is automatic. */
SFUNC int FIO_NAME(FIO_MAP_NAME, rehash)(FIO_MAP_PTR map) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return -1;
  FIO_PTR_TAG_VALID_OR_RETURN(map, -1);
  return FIO_NAME(FIO_MAP_NAME, __realloc)(m, m->bits);
}

/* *****************************************************************************
Iteration
***************************************************************************** */

/**
 * Iteration using a callback for each element in the map.
 *
 * The callback task function must accept an element variable as well as an
 * opaque user pointer.
 *
 * If the callback returns -1, the loop is broken. Any other value is ignored.
 *
 * Returns the relative "stop" position, i.e., the number of items processed +
 * the starting point.
 */
SFUNC FIO_MAP_SIZE_TYPE
FIO_NAME(FIO_MAP_NAME, each)(FIO_MAP_PTR map,
                             int (*task)(FIO_NAME(FIO_MAP_NAME, each_s) *),
                             void *udata,
                             ssize_t start_at) {
  FIO_NAME(FIO_MAP_NAME, s) *m =
      (FIO_NAME(FIO_MAP_NAME, s) *)FIO_PTR_UNTAG(map);
  if (!m)
    return 0;
  FIO_PTR_TAG_VALID_OR_RETURN(map, (FIO_MAP_SIZE_TYPE)-1);
  if (start_at < 0) {
    start_at = m->count - start_at;
    if (start_at < 0)
      start_at = 0;
  }
  if ((FIO_MAP_SIZE_TYPE)start_at >= m->count)
    return m->count;

  FIO_NAME(FIO_MAP_NAME, each_s)
  e = {
      .parent = map,
      .index = (uint64_t)start_at,
#ifdef FIO_MAP_KEY
      .items_at_index = 2,
#else
      .items_at_index = 1,
#endif
      .task = task,
      .udata = udata,
  };

#if FIO_MAP_EVICT_LRU
  if (start_at) {
    FIO_INDEXED_LIST_EACH(m->map, node, m->last_used, pos) {
      if (start_at) {
        --start_at;
        continue;
      }
      e.value = FIO_MAP_OBJ2TYPE(m->map[pos].obj);
#ifdef FIO_MAP_KEY
      e.key = FIO_MAP_OBJ2KEY(m->map[pos].obj);
#endif
      int r = e.task(&e);
      ++e.index;
      if (r == -1)
        goto finish;
    }
  } else {
    FIO_INDEXED_LIST_EACH(m->map, node, m->last_used, pos) {
      e.value = FIO_MAP_OBJ2TYPE(m->map[pos].obj);
#ifdef FIO_MAP_KEY
      e.key = FIO_MAP_OBJ2KEY(m->map[pos].obj);
#endif
      int r = e.task(&e);
      ++e.index;
      if (r == -1)
        goto finish;
    }
  }

#else /* FIO_MAP_EVICT_LRU */

  uint8_t *imap = FIO_NAME(FIO_MAP_NAME, __imap)(m);
  FIO_MAP_SIZE_TYPE pos = 0;
  if (start_at) {
    uint64_t *imap64 = (uint64_t *)imap;
    /* scan map to arrive at starting point. */
    for (FIO_MAP_SIZE_TYPE i = 0; start_at && i < FIO_MAP_CAPA(m->bits);
         i += 8) {
      /* skip empty groups (test for all bytes == 0 || 255 */
      register uint64_t row = imap64[i >> 3];
      row = (fio_has_full_byte64(row) | fio_has_zero_byte64(row));
      if (row == UINT64_C(0x8080808080808080))
        continue;
      row ^= UINT64_C(0x8080808080808080);
      for (int j = 0; start_at && j < 8; ++j) {
        if ((row & UINT64_C(0x80))) {
          pos = i + j;
          --start_at;
        }
        row >>= 8;
      }
    }
  }
  while (pos + 8 < FIO_MAP_CAPA(m->bits)) {
    /* test only groups with valid values (test for all bytes == 0 || 255 */
    register uint64_t row = fio_buf2u64_local(imap + pos);
    row = (fio_has_full_byte64(row) | fio_has_zero_byte64(row));
    if (row != UINT64_C(0x8080808080808080)) {
      row ^= UINT64_C(0x8080808080808080);
      for (int j = 0; j < 8; ++j) {
        if ((row & UINT64_C(0xFF))) {
          e.value = FIO_MAP_OBJ2TYPE(m->map[pos + j].obj);
#ifdef FIO_MAP_KEY
          e.key = FIO_MAP_OBJ2KEY(m->map[pos + j].obj);
#endif
          int r = e.task(&e);
          ++e.index;
          if (r == -1)
            goto finish;
        }
        row >>= 8;
      }
    }
    pos += 8;
  }
  /* scan leftover (not 8 byte aligned) byte-map */
  while (pos < FIO_MAP_CAPA(m->bits)) {
    if (imap[pos] && imap[pos] != 255) {
      e.value = FIO_MAP_OBJ2TYPE(m->map[pos].obj);
#ifdef FIO_MAP_KEY
      e.key = FIO_MAP_OBJ2KEY(m->map[pos].obj);
#endif
      int r = e.task(&e);
      ++e.index;
      if (r == -1)
        goto finish;
    }
    ++pos;
  }
#endif /* FIO_MAP_EVICT_LRU */
finish:
  return (FIO_MAP_SIZE_TYPE)e.index;
}

/* *****************************************************************************
Unordered Map Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_MAP_NAME */
/* *****************************************************************************
Map Testing
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_MAP_NAME map            /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#include "210 map api.h"            /* Development inclusion - ignore line */
#include "211 ordered map.h"        /* Development inclusion - ignore line */
#include "211 unordered map.h"      /* Development inclusion - ignore line */
#define FIO_MAP_TEST                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
#if defined(FIO_MAP_TEST) && defined(FIO_MAP_NAME)

FIO_SFUNC int FIO_NAME_TEST(stl, FIO_NAME(FIO_MAP_NAME, task))(
    FIO_NAME(FIO_MAP_NAME, each_s) * e) {
  *(size_t *)e->udata -= (size_t)e->value;
  return 0;
}
FIO_SFUNC void FIO_NAME_TEST(stl, FIO_MAP_NAME)(void) {
  /*
   * test unrodered maps here
   */
  uint64_t total = 0;
#ifdef FIO_MAP_KEY
  fprintf(stderr,
          "* testing %s map (hash-map) " FIO_MACRO2STR(FIO_MAP_NAME) "\n",
          (FIO_MAP_ORDERED ? "ordered  " : "unordered"));
#define FIO_MAP_TEST_KEY FIO_MAP_KEY
#else
  fprintf(stderr,
          "* testing %s map (set) " FIO_MACRO2STR(FIO_MAP_NAME) "\n",
          (FIO_MAP_ORDERED ? "ordered  " : "unordered"));
#define FIO_MAP_TEST_KEY FIO_MAP_TYPE
#endif
  FIO_NAME(FIO_MAP_NAME, s) m = FIO_MAP_INIT;
  const size_t MEMBERS = (1 << 18);
  for (size_t i = 1; i < MEMBERS; ++i) {
    total += i;
    FIO_MAP_TYPE old = (FIO_MAP_TYPE)i;
#ifdef FIO_MAP_KEY
    FIO_ASSERT((FIO_MAP_TYPE)i == FIO_NAME(FIO_MAP_NAME, set)(&m,
                                                              (FIO_MAP_HASH)i,
                                                              (FIO_MAP_KEY)i,
                                                              (FIO_MAP_TYPE)i,
                                                              &old),
               "insertion failed at %zu",
               i);
#else
    FIO_ASSERT((FIO_MAP_TYPE)i ==
                   FIO_NAME(FIO_MAP_NAME,
                            set)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TYPE)i, &old),
               "insertion failed at %zu",
               i);
#endif
    FIO_ASSERT(old == FIO_MAP_TYPE_INVALID,
               "old value should be set to the invalid value (%zu != %zu @%zu)",
               old,
               (size_t)FIO_MAP_TYPE_INVALID,
               i);
    FIO_ASSERT(
        FIO_NAME(FIO_MAP_NAME, get)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i) ==
            (FIO_MAP_TYPE)i,
        "set-get roundtrip error for %zu",
        i);
  }
  size_t old_capa = FIO_NAME(FIO_MAP_NAME, capa)(&m);

  for (size_t i = 1; i < MEMBERS; ++i) {
    FIO_ASSERT(
        FIO_NAME(FIO_MAP_NAME, get)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i) ==
            (FIO_MAP_TYPE)i,
        "get error for %zu",
        i);
  }
  for (size_t i = 1; i < MEMBERS; ++i) {
    FIO_MAP_TYPE old = (FIO_MAP_TYPE)i;
#ifdef FIO_MAP_KEY
    FIO_ASSERT((FIO_MAP_TYPE)i == FIO_NAME(FIO_MAP_NAME, set)(&m,
                                                              (FIO_MAP_HASH)i,
                                                              (FIO_MAP_KEY)i,
                                                              (FIO_MAP_TYPE)i,
                                                              &old),
               "overwrite failed at %zu",
               i);
#else
    FIO_ASSERT((FIO_MAP_TYPE)i ==
                   FIO_NAME(FIO_MAP_NAME,
                            set)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TYPE)i, &old),
               "overwrite failed at %zu",
               i);
#endif
    FIO_ASSERT(
        !memcmp(&old, &i, sizeof(old) > sizeof(i) ? sizeof(i) : sizeof(old)),
        "old value should be set to the replaced value");
    FIO_ASSERT(
        FIO_NAME(FIO_MAP_NAME, get)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i) ==
            (FIO_MAP_TYPE)i,
        "set-get overwrite roundtrip error for %zu",
        i);
  }
  for (size_t i = 1; i < MEMBERS; ++i) {
    FIO_ASSERT(
        FIO_NAME(FIO_MAP_NAME, get)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i) ==
            (FIO_MAP_TYPE)i,
        "get (overwrite) error for %zu",
        i);
  }
  for (size_t i = 1; i < MEMBERS; ++i) {

    FIO_ASSERT(FIO_NAME(FIO_MAP_NAME, count)(&m) == MEMBERS - 1,
               "unexpected member count");
    FIO_NAME(FIO_MAP_NAME, remove)
    (&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i, NULL);
    FIO_ASSERT(FIO_NAME(FIO_MAP_NAME, count)(&m) == MEMBERS - 2,
               "removing member didn't count removal");
#ifdef FIO_MAP_KEY
    FIO_ASSERT((FIO_MAP_TYPE)i == FIO_NAME(FIO_MAP_NAME, set)(&m,
                                                              (FIO_MAP_HASH)i,
                                                              (FIO_MAP_KEY)i,
                                                              (FIO_MAP_TYPE)i,
                                                              NULL),
               "re-insertion failed at %zu",
               i);
#else
    FIO_ASSERT((FIO_MAP_TYPE)i ==
                   FIO_NAME(FIO_MAP_NAME,
                            set)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TYPE)i, NULL),
               "re-insertion failed at %zu",
               i);
#endif

    FIO_ASSERT(FIO_NAME(FIO_MAP_NAME, get)(&m,
                                           (FIO_MAP_HASH)i,
                                           (FIO_MAP_TYPE)i) == (FIO_MAP_TYPE)i,
               "remove-set-get roundtrip error for %zu",
               i);
  }
  for (size_t i = 1; i < MEMBERS; ++i) {
    FIO_ASSERT(
        FIO_NAME(FIO_MAP_NAME, get)(&m, (FIO_MAP_HASH)i, (FIO_MAP_TEST_KEY)i) ==
            (FIO_MAP_TYPE)i,
        "get (remove/re-insert) error for %zu",
        i);
  }
  if (FIO_NAME(FIO_MAP_NAME, capa)(&m) != old_capa) {
    FIO_LOG_WARNING("capacity shouldn't change when re-inserting the same "
                    "number of items.");
  }
  {
    size_t count = 0;
    size_t tmp = total;
    FIO_MAP_EACH(FIO_MAP_NAME, &m, i) {
      ++count;
      tmp -= (size_t)(FIO_MAP_OBJ2TYPE(i->obj));
    }
    FIO_ASSERT(count + 1 == MEMBERS,
               "FIO_MAP_EACH macro error, repetitions %zu != %zu",
               count,
               MEMBERS - 1);
    FIO_ASSERT(
        !tmp,
        "FIO_MAP_EACH macro error total value %zu != 0 (%zu repetitions)",
        tmp,
        count);
    tmp = total;
    count = FIO_NAME(FIO_MAP_NAME,
                     each)(&m,
                           FIO_NAME_TEST(stl, FIO_NAME(FIO_MAP_NAME, task)),
                           (void *)&tmp,
                           0);
    FIO_ASSERT(count + 1 == MEMBERS,
               "each task error, repetitions %zu != %zu",
               count,
               MEMBERS - 1);
    FIO_ASSERT(!tmp,
               "each task error, total value %zu != 0 (%zu repetitions)",
               tmp,
               count);
  }
  FIO_NAME(FIO_MAP_NAME, destroy)(&m);
}
#undef FIO_MAP_TEST_KEY
#endif /* FIO_MAP_TEST */

/* *****************************************************************************
Map - cleanup
***************************************************************************** */
#undef FIO_MAP_PTR

#undef FIO_MAP_NAME
#undef FIO_UMAP_NAME
#undef FIO_OMAP_NAME

#undef FIO_MAP_DESTROY_AFTER_COPY

#undef FIO_MAP_HASH
#undef FIO_MAP_HASH_FIX
#undef FIO_MAP_HASH_FIXED
#undef FIO_MAP_HASH_INVALID
#undef FIO_MAP_HASH_IS_INVALID
#undef FIO_MAP_HASH_FN
#undef FIO_MAP_HASH_GET_HASH
#undef FIO_MAP_HASH_CACHED

#undef FIO_MAP_INDEX_CALC
#undef FIO_MAP_INDEX_INVALID
#undef FIO_MAP_INDEX_UNUSED
#undef FIO_MAP_INDEX_USED_BIT

#undef FIO_MAP_ORDERED

#undef FIO_MAP_KEY
#undef FIO_MAP_KEY_CMP
#undef FIO_MAP_KEY_COPY
#undef FIO_MAP_KEY_DESTROY
#undef FIO_MAP_KEY_DESTROY_SIMPLE
#undef FIO_MAP_KEY_DISCARD
#undef FIO_MAP_KEY_INVALID

#undef FIO_MAP_MAX_ELEMENTS
#undef FIO_MAP_MAX_FULL_COLLISIONS
#undef FIO_MAP_MAX_SEEK
#undef FIO_MAP_EVICT_LRU
#undef FIO_MAP_SHOULD_OVERWRITE

#undef FIO_MAP_OBJ
#undef FIO_MAP_OBJ2KEY
#undef FIO_MAP_OBJ2TYPE
#undef FIO_MAP_OBJ_CMP
#undef FIO_MAP_OBJ_COPY
#undef FIO_MAP_OBJ_DESTROY
#undef FIO_MAP_OBJ_DESTROY_AFTER
#undef FIO_MAP_OBJ_DISCARD
#undef FIO_MAP_OBJ_INVALID
#undef FIO_MAP_OBJ_KEY
#undef FIO_MAP_OBJ_KEY_CMP

#undef FIO_MAP_S
#undef FIO_MAP_SEEK_AS_ARRAY_LOG_LIMIT
#undef FIO_MAP_SIZE_TYPE
#undef FIO_MAP_TYPE
#undef FIO_MAP_TYPE_CMP
#undef FIO_MAP_TYPE_COPY
#undef FIO_MAP_TYPE_COPY_SIMPLE
#undef FIO_MAP_TYPE_DESTROY
#undef FIO_MAP_TYPE_DESTROY_SIMPLE
#undef FIO_MAP_TYPE_DISCARD
#undef FIO_MAP_TYPE_INVALID
#undef FIO_MAP_BIG
#undef FIO_MAP_HASH
#undef FIO_MAP_INDEX_USED_BIT
#undef FIO_MAP_TYPE
#undef FIO_MAP_TYPE_INVALID
#undef FIO_MAP_TYPE_COPY
#undef FIO_MAP_TYPE_COPY_SIMPLE
#undef FIO_MAP_TYPE_DESTROY
#undef FIO_MAP_TYPE_DESTROY_SIMPLE
#undef FIO_MAP_TYPE_DISCARD
#undef FIO_MAP_TYPE_CMP
#undef FIO_MAP_DESTROY_AFTER_COPY
#undef FIO_MAP_KEY
#undef FIO_MAP_KEY_INVALID
#undef FIO_MAP_KEY_COPY
#undef FIO_MAP_KEY_DESTROY
#undef FIO_MAP_KEY_DESTROY_SIMPLE
#undef FIO_MAP_KEY_DISCARD
#undef FIO_MAP_KEY_CMP
#undef FIO_MAP_OBJ
#undef FIO_MAP_OBJ_KEY
#undef FIO_MAP_OBJ_INVALID
#undef FIO_MAP_OBJ_COPY
#undef FIO_MAP_OBJ_DESTROY
#undef FIO_MAP_OBJ_CMP
#undef FIO_MAP_OBJ_KEY_CMP
#undef FIO_MAP_OBJ2KEY
#undef FIO_MAP_OBJ2TYPE
#undef FIO_MAP_OBJ_DISCARD
#undef FIO_MAP_DESTROY_AFTER_COPY
#undef FIO_MAP_OBJ_DESTROY_AFTER
#undef FIO_MAP_MAX_SEEK
#undef FIO_MAP_MAX_FULL_COLLISIONS
#undef FIO_MAP_CUCKOO_STEPS
#undef FIO_MAP_EVICT_LRU
#undef FIO_MAP_CAPA
#undef FIO_MAP_MEMORY_SIZE

#undef FIO_MAP___IMAP_FREE
#undef FIO_MAP___IMAP_DELETED
#undef FIO_MAP_TEST
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_STR_NAME fio            /* Development inclusion - ignore line */
#define FIO_ATOL                    /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                        Dynamic Strings (binary safe)




***************************************************************************** */
#ifdef FIO_STR_SMALL
#ifndef FIO_STR_NAME
#define FIO_STR_NAME FIO_STR_SMALL
#endif
#ifndef FIO_STR_OPTIMIZE4IMMUTABILITY
#define FIO_STR_OPTIMIZE4IMMUTABILITY 1
#endif
#endif /* FIO_STR_SMALL */

#if defined(FIO_STR_NAME)

#ifndef FIO_STR_OPTIMIZE_EMBEDDED
/**
 * For each unit (0 by default), adds `sizeof(char *)` bytes to the type size,
 * increasing the amount of strings that could be embedded within the type
 * without memory allocation.
 *
 * For example, when using a referrence counter wrapper on a 64bit system, it
 * would make sense to set this value to 1 - allowing the type size to fully
 * utilize a 16 byte memory allocation alignment.
 */
#define FIO_STR_OPTIMIZE_EMBEDDED 0
#endif

#ifndef FIO_STR_OPTIMIZE4IMMUTABILITY
/**
 * Optimizes the struct to minimal size that can store the string length and a
 * pointer.
 *
 * By avoiding extra (mutable related) data, such as the allocated memory's
 * capacity, strings require less memory. However, this does introduce a
 * performance penalty when editing the string data.
 */
#define FIO_STR_OPTIMIZE4IMMUTABILITY 0
#endif

#if FIO_STR_OPTIMIZE4IMMUTABILITY
/* enforce limit after which FIO_STR_OPTIMIZE4IMMUTABILITY makes no sense */
#if FIO_STR_OPTIMIZE_EMBEDDED > 1
#undef FIO_STR_OPTIMIZE_EMBEDDED
#define FIO_STR_OPTIMIZE_EMBEDDED 1
#endif
#else
/* enforce limit due to 6 bit embedded string length limit */
#if FIO_STR_OPTIMIZE_EMBEDDED > 4
#undef FIO_STR_OPTIMIZE_EMBEDDED
#define FIO_STR_OPTIMIZE_EMBEDDED 4
#endif
#endif /* FIO_STR_OPTIMIZE4IMMUTABILITY*/

/* *****************************************************************************
String API - Initialization and Destruction
***************************************************************************** */

/**
 * The `fio_str_s` type should be considered opaque.
 *
 * The type's attributes should be accessed ONLY through the accessor
 * functions: `fio_str2cstr`, `fio_str_len`, `fio_str2ptr`, `fio_str_capa`,
 * etc'.
 *
 * Note: when the `small` flag is present, the structure is ignored and used
 * as raw memory for a small String (no additional allocation). This changes
 * the String's behavior drastically and requires that the accessor functions
 * be used.
 */
typedef struct {
  /* String flags:
   *
   * bit 1: small string.
   * bit 2: frozen string.
   * bit 3: static (non allocated) string (big strings only).
   * bit 3-8: small string length (up to 64 bytes).
   */
  uint8_t special;
  uint8_t reserved[(sizeof(void *) * (1 + FIO_STR_OPTIMIZE_EMBEDDED)) -
                   (sizeof(uint8_t))]; /* padding length */
#if !FIO_STR_OPTIMIZE4IMMUTABILITY
  size_t capa; /* known capacity for longer Strings */
  size_t len;  /* String length for longer Strings */
#endif         /* FIO_STR_OPTIMIZE4IMMUTABILITY */
  char *buf;   /* pointer for longer Strings */
} FIO_NAME(FIO_STR_NAME, s);

#ifdef FIO_PTR_TAG_TYPE
#define FIO_STR_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_STR_PTR FIO_NAME(FIO_STR_NAME, s) *
#endif

#ifndef FIO_STR_INIT
/**
 * This value should be used for initialization. For example:
 *
 *      // on the stack
 *      fio_str_s str = FIO_STR_INIT;
 *
 *      // or on the heap
 *      fio_str_s *str = malloc(sizeof(*str));
 *      *str = FIO_STR_INIT;
 *
 * Remember to cleanup:
 *
 *      // on the stack
 *      fio_str_destroy(&str);
 *
 *      // or on the heap
 *      fio_str_free(str);
 *      free(str);
 */
#define FIO_STR_INIT                                                           \
  { .special = 0 }

/**
 * This macro allows the container to be initialized with existing data, as long
 * as it's memory was allocated with the same allocator (`malloc` /
 * `fio_malloc`).
 *
 * The `capacity` value should exclude the NUL character (if exists).
 *
 * NOTE: This macro isn't valid for FIO_STR_SMALL (or strings with the
 * FIO_STR_OPTIMIZE4IMMUTABILITY optimization)
 */
#define FIO_STR_INIT_EXISTING(buffer, length, capacity)                        \
  { .capa = (capacity), .len = (length), .buf = (buffer) }

/**
 * This macro allows the container to be initialized with existing static data,
 * that shouldn't be freed.
 *
 * NOTE: This macro isn't valid for FIO_STR_SMALL (or strings with the
 * FIO_STR_OPTIMIZE4IMMUTABILITY optimization)
 */
#define FIO_STR_INIT_STATIC(buffer)                                            \
  { .special = 4, .len = strlen((buffer)), .buf = (char *)(buffer) }

/**
 * This macro allows the container to be initialized with existing static data,
 * that shouldn't be freed.
 *
 * NOTE: This macro isn't valid for FIO_STR_SMALL (or strings with the
 * FIO_STR_OPTIMIZE4IMMUTABILITY optimization)
 */
#define FIO_STR_INIT_STATIC2(buffer, length)                                   \
  { .special = 4, .len = (length), .buf = (char *)(buffer) }

#endif /* FIO_STR_INIT */

#ifndef FIO_REF_CONSTRUCTOR_ONLY

/** Allocates a new String object on the heap. */
FIO_IFUNC FIO_STR_PTR FIO_NAME(FIO_STR_NAME, new)(void);

/**
 * Destroys the string and frees the container (if allocated with `new`).
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, free)(FIO_STR_PTR s);

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/**
 * Initializes the container with the provided static / constant string.
 *
 * The string will be copied to the container **only** if it will fit in the
 * container itself. Otherwise, the supplied pointer will be used as is and it
 * should remain valid until the string is destroyed.
 *
 * The final string can be safely be destroyed (using the `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_const)(FIO_STR_PTR s,
                                                            const char *str,
                                                            size_t len);

/**
 * Initializes the container with a copy of the provided dynamic string.
 *
 * The string is always copied and the final string must be destroyed (using the
 * `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_copy)(FIO_STR_PTR s,
                                                           const char *str,
                                                           size_t len);

/**
 * Initializes the container with a copy of an existing String object.
 *
 * The string is always copied and the final string must be destroyed (using the
 * `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_copy2)(FIO_STR_PTR dest,
                                                            FIO_STR_PTR src);

/**
 * Frees the String's resources and reinitializes the container.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 * separately using the appropriate `free` function.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, destroy)(FIO_STR_PTR s);

/**
 * Returns a C string with the existing data, re-initializing the String.
 *
 * Note: the String data is removed from the container, but the container
 * isn't freed.
 *
 * Returns NULL if there's no String data.
 *
 * NOTE: Returned string is ALWAYS dynamically allocated. Remember to free.
 */
FIO_IFUNC char *FIO_NAME(FIO_STR_NAME, detach)(FIO_STR_PTR s);

/* *****************************************************************************
String API - String state (data pointers, length, capacity, etc')
***************************************************************************** */

/** Returns the String's complete state (capacity, length and pointer).  */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, info)(const FIO_STR_PTR s);

/** Returns a pointer (`char *`) to the String's content. */
FIO_IFUNC char *FIO_NAME2(FIO_STR_NAME, ptr)(FIO_STR_PTR s);

/** Returns the String's length in bytes. */
FIO_IFUNC size_t FIO_NAME(FIO_STR_NAME, len)(FIO_STR_PTR s);

/** Returns the String's existing capacity (total used & available memory). */
FIO_IFUNC size_t FIO_NAME(FIO_STR_NAME, capa)(FIO_STR_PTR s);

/**
 * Prevents further manipulations to the String's content.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, freeze)(FIO_STR_PTR s);

/**
 * Returns true if the string is frozen.
 */
FIO_IFUNC uint8_t FIO_NAME_BL(FIO_STR_NAME, frozen)(FIO_STR_PTR s);

/** Returns 1 if memory was allocated and (the String must be destroyed). */
FIO_IFUNC int FIO_NAME_BL(FIO_STR_NAME, allocated)(const FIO_STR_PTR s);

/**
 * Binary comparison returns `1` if both strings are equal and `0` if not.
 */
FIO_IFUNC int FIO_NAME_BL(FIO_STR_NAME, eq)(const FIO_STR_PTR str1,
                                            const FIO_STR_PTR str2);

/**
 * Returns the string's Risky Hash value.
 *
 * Note: Hash algorithm might change without notice.
 */
FIO_IFUNC uint64_t FIO_NAME(FIO_STR_NAME, hash)(const FIO_STR_PTR s,
                                                uint64_t seed);

/* *****************************************************************************
String API - Memory management
***************************************************************************** */

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 *
 * Returns the updated state of the String.
 *
 * Note: When shrinking, any existing data beyond the new size may be
 * corrupted.
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, resize)(FIO_STR_PTR s,
                                                        size_t size);

/**
 * Performs a best attempt at minimizing memory consumption.
 *
 * Actual effects depend on the underlying memory allocator and it's
 * implementation. Not all allocators will free any memory.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, compact)(FIO_STR_PTR s);

#if !FIO_STR_OPTIMIZE4IMMUTABILITY
/**
 * Reserves (at least) `amount` of bytes for the string's data.
 *
 * The reserved count includes used data. If `amount` is less than the current
 * string length, the string will be truncated(!).
 *
 * May corrupt the string length information (if string is assumed to be
 * immutable), make sure to call `resize` with the updated information once the
 * editing is done.
 *
 * Returns the updated state of the String.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, reserve)(FIO_STR_PTR s,
                                                     size_t amount);
#define FIO_STR_RESERVE_NAME reserve
#else
/** INTERNAL - DO NOT USE! */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, __reserve)(FIO_STR_PTR s,
                                                       size_t amount);
#define FIO_STR_RESERVE_NAME __reserve
#endif
/* *****************************************************************************
String API - UTF-8 State
***************************************************************************** */

/** Returns 1 if the String is UTF-8 valid and 0 if not. */
SFUNC size_t FIO_NAME(FIO_STR_NAME, utf8_valid)(FIO_STR_PTR s);

/** Returns the String's length in UTF-8 characters. */
SFUNC size_t FIO_NAME(FIO_STR_NAME, utf8_len)(FIO_STR_PTR s);

/**
 * Takes a UTF-8 character selection information (UTF-8 position and length)
 * and updates the same variables so they reference the raw byte slice
 * information.
 *
 * If the String isn't UTF-8 valid up to the requested selection, than `pos`
 * will be updated to `-1` otherwise values are always positive.
 *
 * The returned `len` value may be shorter than the original if there wasn't
 * enough data left to accommodate the requested length. When a `len` value of
 * `0` is returned, this means that `pos` marks the end of the String.
 *
 * Returns -1 on error and 0 on success.
 */
SFUNC int FIO_NAME(FIO_STR_NAME,
                   utf8_select)(FIO_STR_PTR s, intptr_t *pos, size_t *len);

/* *****************************************************************************
String API - Content Manipulation and Review
***************************************************************************** */

/**
 * Writes data at the end of the String.
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write)(FIO_STR_PTR s,
                                                       const void *src,
                                                       size_t src_len);

/**
 * Writes a number at the end of the String using normal base 10 notation.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_i)(FIO_STR_PTR s,
                                                     int64_t num);

/**
 * Writes a number at the end of the String using Hex (base 16) notation.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_hex)(FIO_STR_PTR s,
                                                       int64_t num);
/**
 * Appends the `src` String to the end of the `dest` String.
 *
 * If `dest` is empty, the resulting Strings will be equal.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, concat)(FIO_STR_PTR dest,
                                                    FIO_STR_PTR const src);

/** Alias for fio_str_concat */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, join)(FIO_STR_PTR dest,
                                                      FIO_STR_PTR const src) {
  return FIO_NAME(FIO_STR_NAME, concat)(dest, src);
}

/**
 * Replaces the data in the String - replacing `old_len` bytes starting at
 * `start_pos`, with the data at `src` (`src_len` bytes long).
 *
 * Negative `start_pos` values are calculated backwards, `-1` == end of
 * String.
 *
 * When `old_len` is zero, the function will insert the data at `start_pos`.
 *
 * If `src_len == 0` than `src` will be ignored and the data marked for
 * replacement will be erased.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, replace)(FIO_STR_PTR s,
                                                     intptr_t start_pos,
                                                     size_t old_len,
                                                     const void *src,
                                                     size_t src_len);

/**
 * Writes to the String using a vprintf like interface.
 *
 * Data is written to the end of the String.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, vprintf)(FIO_STR_PTR s,
                                                     const char *format,
                                                     va_list argv);

/**
 * Writes to the String using a printf like interface.
 *
 * Data is written to the end of the String.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              printf)(FIO_STR_PTR s, const char *format, ...);

/**
 * Reads data from a file descriptor `fd` at offset `start_at` and pastes it's
 * contents (or a slice of it) at the end of the String. If `limit == 0`, than
 * the data will be read until EOF.
 *
 * The file should be a regular file or the operation might fail (can't be used
 * for sockets).
 *
 * The file descriptor will remain open and should be closed manually.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, readfd)(FIO_STR_PTR s,
                                                    int fd,
                                                    intptr_t start_at,
                                                    intptr_t limit);
/**
 * Opens the file `filename` and pastes it's contents (or a slice ot it) at
 * the end of the String. If `limit == 0`, than the data will be read until
 * EOF.
 *
 * If the file can't be located, opened or read, or if `start_at` is beyond
 * the EOF position, NULL is returned in the state's `data` field.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, readfile)(FIO_STR_PTR s,
                                                      const char *filename,
                                                      intptr_t start_at,
                                                      intptr_t limit);

/* *****************************************************************************
String API - C / JSON escaping
***************************************************************************** */

/**
 * Writes data at the end of the String, escaping the data using JSON semantics.
 *
 * The JSON semantic are common to many programming languages, promising a UTF-8
 * String while making it easy to read and copy the string during debugging.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_escape)(FIO_STR_PTR s,
                                                          const void *data,
                                                          size_t data_len);

/**
 * Writes an escaped data into the string after unescaping the data.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_unescape)(FIO_STR_PTR s,
                                                            const void *escaped,
                                                            size_t len);

/* *****************************************************************************
String API - Base64 support
***************************************************************************** */

/**
 * Writes data at the end of the String, encoding the data as Base64 encoded
 * data.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              write_base64enc)(FIO_STR_PTR s,
                                               const void *data,
                                               size_t data_len,
                                               uint8_t url_encoded);

/**
 * Writes decoded base64 data to the end of the String.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              write_base64dec)(FIO_STR_PTR s,
                                               const void *encoded,
                                               size_t encoded_len);

/* *****************************************************************************
String API - Testing
***************************************************************************** */
#ifdef FIO_STR_WRITE_TEST_FUNC
/**
 * Tests the fio_str functionality.
 */
SFUNC void FIO_NAME_TEST(stl, FIO_STR_NAME)(void);
#endif
/* *****************************************************************************


                             String Implementation

                           IMPLEMENTATION - INLINED


***************************************************************************** */

/* *****************************************************************************
String Macro Helpers
***************************************************************************** */

#define FIO_STR_IS_SMALL(s)  (((s)->special & 1) || !(s)->buf)
#define FIO_STR_SMALL_LEN(s) ((size_t)((s)->special >> 2))
#define FIO_STR_SMALL_LEN_SET(s, l)                                            \
  ((s)->special = (((s)->special & 2) | ((uint8_t)(l) << 2) | 1))
#define FIO_STR_SMALL_CAPA(s) ((sizeof(*(s)) - 2) & 63)
#define FIO_STR_SMALL_DATA(s) ((char *)((s)->reserved))

#define FIO_STR_BIG_DATA(s)       ((s)->buf)
#define FIO_STR_BIG_IS_DYNAMIC(s) (!((s)->special & 4))
#define FIO_STR_BIG_SET_STATIC(s) ((s)->special |= 4)
#define FIO_STR_BIG_FREE_BUF(s)   (FIO_MEM_FREE_((s)->buf, FIO_STR_BIG_CAPA((s))))

#define FIO_STR_IS_FROZEN(s) ((s)->special & 2)
#define FIO_STR_FREEZE_(s)   ((s)->special |= 2)
#define FIO_STR_THAW_(s)     ((s)->special ^= (uint8_t)2)

#if FIO_STR_OPTIMIZE4IMMUTABILITY

#define FIO_STR_BIG_LEN(s)                                                     \
  ((sizeof(void *) == 4)                                                       \
       ? (((uint32_t)(s)->reserved[0]) | ((uint32_t)(s)->reserved[1] << 8) |   \
          ((uint32_t)(s)->reserved[2] << 16))                                  \
       : (((uint64_t)(s)->reserved[0]) | ((uint64_t)(s)->reserved[1] << 8) |   \
          ((uint64_t)(s)->reserved[2] << 16) |                                 \
          ((uint64_t)(s)->reserved[3] << 24) |                                 \
          ((uint64_t)(s)->reserved[4] << 32) |                                 \
          ((uint64_t)(s)->reserved[5] << 40) |                                 \
          ((uint64_t)(s)->reserved[6] << 48)))
#define FIO_STR_BIG_LEN_SET(s, l)                                              \
  do {                                                                         \
    if (sizeof(void *) == 4) {                                                 \
      if (!((l) & ((~(uint32_t)0) << 24))) {                                   \
        (s)->reserved[0] = (l)&0xFF;                                           \
        (s)->reserved[1] = ((uint32_t)(l) >> 8) & 0xFF;                        \
        (s)->reserved[2] = ((uint32_t)(l) >> 16) & 0xFF;                       \
      } else {                                                                 \
        FIO_LOG_ERROR("facil.io small string length error - too long");        \
        (s)->reserved[0] = 0xFF;                                               \
        (s)->reserved[1] = 0xFF;                                               \
        (s)->reserved[2] = 0xFF;                                               \
      }                                                                        \
    } else {                                                                   \
      if (!((l) & ((~(uint64_t)0) << 56))) {                                   \
        (s)->reserved[0] = (l)&0xff;                                           \
        (s)->reserved[1] = ((uint64_t)(l) >> 8) & 0xFF;                        \
        (s)->reserved[2] = ((uint64_t)(l) >> 16) & 0xFF;                       \
        (s)->reserved[3] = ((uint64_t)(l) >> 24) & 0xFF;                       \
        (s)->reserved[4] = ((uint64_t)(l) >> 32) & 0xFF;                       \
        (s)->reserved[5] = ((uint64_t)(l) >> 40) & 0xFF;                       \
        (s)->reserved[6] = ((uint64_t)(l) >> 48) & 0xFF;                       \
      } else {                                                                 \
        FIO_LOG_ERROR("facil.io small string length error - too long");        \
        (s)->reserved[0] = 0xFF;                                               \
        (s)->reserved[1] = 0xFF;                                               \
        (s)->reserved[2] = 0xFF;                                               \
        (s)->reserved[3] = 0xFF;                                               \
        (s)->reserved[4] = 0xFF;                                               \
        (s)->reserved[5] = 0xFF;                                               \
        (s)->reserved[6] = 0xFF;                                               \
      }                                                                        \
    }                                                                          \
  } while (0)
#define FIO_STR_BIG_CAPA(s) FIO_STR_CAPA2WORDS(FIO_STR_BIG_LEN((s)))
#define FIO_STR_BIG_CAPA_SET(s, capa)
#else
#define FIO_STR_BIG_LEN(s)            ((s)->len)
#define FIO_STR_BIG_LEN_SET(s, l)     ((s)->len = (l))
#define FIO_STR_BIG_CAPA(s)           ((s)->capa)
#define FIO_STR_BIG_CAPA_SET(s, capa) (FIO_STR_BIG_CAPA(s) = (capa))
#endif

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
 * allocate the same amount of memory as `malloc(32)` due to alignment concerns.
 *
 * In fact, with some allocators (i.e., jemalloc), spacing increases for larger
 * allocations - meaning the allocator will round up to more than 16 bytes, as
 * noted here: http://jemalloc.net/jemalloc.3.html#size_classes
 *
 * Note that this increased spacing, doesn't occure with facil.io's allocator,
 * since it uses 16 byte alignment right up until allocations are routed
 * directly to `mmap` (due to their size, usually over 12KB).
 */
#define FIO_STR_CAPA2WORDS(num)                                                \
  ((size_t)(                                                                   \
      (size_t)(num) |                                                          \
      ((sizeof(long double) > 16) ? (sizeof(long double) - 1) : (size_t)15)))

/* *****************************************************************************
String Constructors (inline)
***************************************************************************** */
#ifndef FIO_REF_CONSTRUCTOR_ONLY

/** Allocates a new String object on the heap. */
FIO_IFUNC FIO_STR_PTR FIO_NAME(FIO_STR_NAME, new)(void) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*s), 0);
  if (!FIO_MEM_REALLOC_IS_SAFE_ && s) {
    *s = (FIO_NAME(FIO_STR_NAME, s))FIO_STR_INIT;
  }
#ifdef DEBUG
  {
    FIO_NAME(FIO_STR_NAME, s) tmp = {0};
    FIO_ASSERT(!memcmp(&tmp, s, sizeof(tmp)),
               "new " FIO_MACRO2STR(
                   FIO_NAME(FIO_STR_NAME, s)) " object not initialized!");
  }
#endif
  return (FIO_STR_PTR)FIO_PTR_TAG(s);
}

/** Destroys the string and frees the container (if allocated with `new`). */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, free)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, destroy)(s_);
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (s) {
    FIO_MEM_FREE_(s, sizeof(*s));
  }
}

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/**
 * Frees the String's resources and reinitializes the container.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 * separately using the appropriate `free` function.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, destroy)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return;
  if (!FIO_STR_IS_SMALL(s) && FIO_STR_BIG_IS_DYNAMIC(s)) {
    FIO_STR_BIG_FREE_BUF(s);
  }
  *s = (FIO_NAME(FIO_STR_NAME, s))FIO_STR_INIT;
}

/**
 * Returns a C string with the existing data, re-initializing the String.
 *
 * Note: the String data is removed from the container, but the container
 * isn't freed.
 *
 * Returns NULL if there's no String data.
 */
FIO_IFUNC char *FIO_NAME(FIO_STR_NAME, detach)(FIO_STR_PTR s_) {
  char *data = NULL;
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_) {
    return data;
  }
  if (FIO_STR_IS_SMALL(s)) {
    if (FIO_STR_SMALL_LEN(s)) { /* keep these ifs apart */
      data =
          (char *)FIO_MEM_REALLOC_(NULL,
                                   0,
                                   sizeof(*data) * (FIO_STR_SMALL_LEN(s) + 1),
                                   0);
      if (data)
        FIO_MEMCPY(data, FIO_STR_SMALL_DATA(s), (FIO_STR_SMALL_LEN(s) + 1));
    }
  } else {
    if (FIO_STR_BIG_IS_DYNAMIC(s)) {
      data = FIO_STR_BIG_DATA(s);
    } else if (FIO_STR_BIG_LEN(s)) {
      data = (char *)FIO_MEM_REALLOC_(NULL,
                                      0,
                                      sizeof(*data) * (FIO_STR_BIG_LEN(s) + 1),
                                      0);
      if (data)
        FIO_MEMCPY(data, FIO_STR_BIG_DATA(s), FIO_STR_BIG_LEN(s) + 1);
    }
  }
  *s = (FIO_NAME(FIO_STR_NAME, s)){0};
  return data;
}

/**
 * Performs a best attempt at minimizing memory consumption.
 *
 * Actual effects depend on the underlying memory allocator and it's
 * implementation. Not all allocators will free any memory.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, compact)(FIO_STR_PTR s_) {
#if FIO_STR_OPTIMIZE4IMMUTABILITY
  (void)s_;
#else
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s_ || !s || FIO_STR_IS_SMALL(s) || FIO_STR_BIG_IS_DYNAMIC(s) ||
      FIO_STR_CAPA2WORDS(FIO_NAME(FIO_STR_NAME, len)(s_)) >=
          FIO_NAME(FIO_STR_NAME, capa)(s_))
    return;
  FIO_NAME(FIO_STR_NAME, s) tmp = FIO_STR_INIT;
  fio_str_info_s i = FIO_NAME(FIO_STR_NAME, info)(s_);
  FIO_NAME(FIO_STR_NAME, init_copy)
  ((FIO_STR_PTR)FIO_PTR_TAG(&tmp), i.buf, i.len);
  FIO_NAME(FIO_STR_NAME, destroy)(s_);
  *s = tmp;
#endif
}

/* *****************************************************************************
String Initialization (inline)
***************************************************************************** */

/**
 * Initializes the container with the provided static / constant string.
 *
 * The string will be copied to the container **only** if it will fit in the
 * container itself. Otherwise, the supplied pointer will be used as is and it
 * should remain valid until the string is destroyed.
 *
 * The final string can be safely be destroyed (using the `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_const)(FIO_STR_PTR s_,
                                                            const char *str,
                                                            size_t len) {
  fio_str_info_s i = {0};
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_) {
    FIO_LOG_ERROR("Attempted to initialize a NULL String.");
    return i;
  }
  *s = (FIO_NAME(FIO_STR_NAME, s)){0};
  if (len < FIO_STR_SMALL_CAPA(s)) {
    FIO_STR_SMALL_LEN_SET(s, len);
    if (len && str)
      FIO_MEMCPY(FIO_STR_SMALL_DATA(s), str, len);
    FIO_STR_SMALL_DATA(s)[len] = 0;

    i = (fio_str_info_s){.buf = FIO_STR_SMALL_DATA(s),
                         .len = len,
                         .capa = FIO_STR_SMALL_CAPA(s)};
    return i;
  }
  FIO_STR_BIG_DATA(s) = (char *)str;
  FIO_STR_BIG_LEN_SET(s, len);
  FIO_STR_BIG_SET_STATIC(s);
  i = (fio_str_info_s){.buf = FIO_STR_BIG_DATA(s), .len = len, .capa = 0};
  return i;
}

/**
 * Initializes the container with the provided dynamic string.
 *
 * The string is always copied and the final string must be destroyed (using the
 * `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_copy)(FIO_STR_PTR s_,
                                                           const char *str,
                                                           size_t len) {
  fio_str_info_s i = {0};
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_) {
    FIO_LOG_ERROR("Attempted to initialize a NULL String.");
    return i;
  }
  *s = (FIO_NAME(FIO_STR_NAME, s)){0};
  if (len < FIO_STR_SMALL_CAPA(s)) {
    FIO_STR_SMALL_LEN_SET(s, len);
    if (len && str)
      FIO_MEMCPY(FIO_STR_SMALL_DATA(s), str, len);
    FIO_STR_SMALL_DATA(s)[len] = 0;

    i = (fio_str_info_s){.buf = FIO_STR_SMALL_DATA(s),
                         .len = len,
                         .capa = FIO_STR_SMALL_CAPA(s)};
    return i;
  }

  {
    char *buf =
        (char *)FIO_MEM_REALLOC_(NULL,
                                 0,
                                 sizeof(*buf) * (FIO_STR_CAPA2WORDS(len) + 1),
                                 0);
    if (!buf)
      return i;
    buf[len] = 0;
    i = (fio_str_info_s){.buf = buf,
                         .len = len,
                         .capa = FIO_STR_CAPA2WORDS(len)};
  }
  FIO_STR_BIG_CAPA_SET(s, i.capa);
  FIO_STR_BIG_DATA(s) = i.buf;
  FIO_STR_BIG_LEN_SET(s, len);
  if (str)
    FIO_MEMCPY(FIO_STR_BIG_DATA(s), str, len);
  return i;
}

/**
 * Initializes the container with a copy of an existing String object.
 *
 * The string is always copied and the final string must be destroyed (using the
 * `destroy` function).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, init_copy2)(FIO_STR_PTR dest,
                                                            FIO_STR_PTR src) {
  fio_str_info_s i;
  i = FIO_NAME(FIO_STR_NAME, info)(src);
  i = FIO_NAME(FIO_STR_NAME, init_copy)(dest, i.buf, i.len);
  return i;
}

/* *****************************************************************************
String Information (inline)
***************************************************************************** */

/** Returns the String's complete state (capacity, length and pointer).  */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, info)(const FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return (fio_str_info_s){0};
  if (FIO_STR_IS_SMALL(s))
    return (fio_str_info_s){
        .buf = FIO_STR_SMALL_DATA(s),
        .len = FIO_STR_SMALL_LEN(s),
        .capa = (FIO_STR_IS_FROZEN(s) ? 0 : FIO_STR_SMALL_CAPA(s)),
    };

  return (fio_str_info_s){
      .buf = FIO_STR_BIG_DATA(s),
      .len = FIO_STR_BIG_LEN(s),
      .capa = (FIO_STR_IS_FROZEN(s) ? 0 : FIO_STR_BIG_CAPA(s)),
  };
}

/** Returns a pointer (`char *`) to the String's content. */
FIO_IFUNC char *FIO_NAME2(FIO_STR_NAME, ptr)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return NULL;
  return FIO_STR_IS_SMALL(s) ? FIO_STR_SMALL_DATA(s) : FIO_STR_BIG_DATA(s);
}

/** Returns the String's length in bytes. */
FIO_IFUNC size_t FIO_NAME(FIO_STR_NAME, len)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return 0;
  return FIO_STR_IS_SMALL(s) ? FIO_STR_SMALL_LEN(s) : FIO_STR_BIG_LEN(s);
}

/** Returns the String's existing capacity (total used & available memory). */
FIO_IFUNC size_t FIO_NAME(FIO_STR_NAME, capa)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return 0;
  if (FIO_STR_IS_SMALL(s))
    return FIO_STR_SMALL_CAPA(s);
  if (FIO_STR_BIG_IS_DYNAMIC(s))
    return FIO_STR_BIG_CAPA(s);
  return 0;
}

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, resize)(FIO_STR_PTR s_,
                                                        size_t size) {
  fio_str_info_s i = {.capa = FIO_NAME(FIO_STR_NAME, capa)(s_)};
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s_ || !s || FIO_STR_IS_FROZEN(s)) {
    i = FIO_NAME(FIO_STR_NAME, info)(s_);
    return i;
  }
  /* resize may be used to reserve memory in advance while setting size  */
  if (i.capa < size) {
    i = FIO_NAME(FIO_STR_NAME, FIO_STR_RESERVE_NAME)(s_, size);
  }

  if (FIO_STR_IS_SMALL(s)) {
    FIO_STR_SMALL_DATA(s)[size] = 0;
    FIO_STR_SMALL_LEN_SET(s, size);
    i = (fio_str_info_s){
        .buf = FIO_STR_SMALL_DATA(s),
        .len = size,
        .capa = FIO_STR_SMALL_CAPA(s),
    };
  } else {
    FIO_STR_BIG_DATA(s)[size] = 0;
    FIO_STR_BIG_LEN_SET(s, size);
    i = (fio_str_info_s){
        .buf = FIO_STR_BIG_DATA(s),
        .len = size,
        .capa = i.capa,
    };
  }
  return i;
}

/**
 * Prevents further manipulations to the String's content.
 */
FIO_IFUNC void FIO_NAME(FIO_STR_NAME, freeze)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return;
  FIO_STR_FREEZE_(s);
}

/**
 * Returns true if the string is frozen.
 */
FIO_IFUNC uint8_t FIO_NAME_BL(FIO_STR_NAME, frozen)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return 1;
  return FIO_STR_IS_FROZEN(s);
}

/** Returns 1 if memory was allocated and (the String must be destroyed). */
FIO_IFUNC int FIO_NAME_BL(FIO_STR_NAME, allocated)(const FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  return (s_ && s && !FIO_STR_IS_SMALL(s) && FIO_STR_BIG_IS_DYNAMIC(s));
}

/**
 * Binary comparison returns `1` if both strings are equal and `0` if not.
 */
FIO_IFUNC int FIO_NAME_BL(FIO_STR_NAME, eq)(const FIO_STR_PTR str1_,
                                            const FIO_STR_PTR str2_) {
  if (str1_ == str2_)
    return 1;
  FIO_NAME(FIO_STR_NAME, s) *const str1 =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(str1_);
  FIO_NAME(FIO_STR_NAME, s) *const str2 =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(str2_);
  if (str1 == str2)
    return 1;
  if (!str1 || !str2)
    return 0;
  fio_str_info_s s1 = FIO_NAME(FIO_STR_NAME, info)(str1_);
  fio_str_info_s s2 = FIO_NAME(FIO_STR_NAME, info)(str2_);
  return FIO_STR_INFO_IS_EQ(s1, s2);
}

/**
 * Returns the string's Risky Hash value.
 *
 * Note: Hash algorithm might change without notice.
 */
FIO_IFUNC uint64_t FIO_NAME(FIO_STR_NAME, hash)(const FIO_STR_PTR s_,
                                                uint64_t seed) {
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return fio_risky_hash(NULL, 0, seed);
  if (FIO_STR_IS_SMALL(s))
    return fio_risky_hash((void *)FIO_STR_SMALL_DATA(s),
                          FIO_STR_SMALL_LEN(s),
                          seed);
  return fio_risky_hash((void *)FIO_STR_BIG_DATA(s), FIO_STR_BIG_LEN(s), seed);
}

/* *****************************************************************************
String API - Content Manipulation and Review (inline)
***************************************************************************** */

/**
 * Writes data at the end of the String (similar to `fio_str_insert` with the
 * argument `pos == -1`).
 */
FIO_IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write)(FIO_STR_PTR s_,
                                                       const void *src,
                                                       size_t src_len) {
  FIO_NAME(FIO_STR_NAME, s) *s = (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s_ || !s || !src_len || FIO_STR_IS_FROZEN(s))
    return FIO_NAME(FIO_STR_NAME, info)(s_);
  size_t const org_len = FIO_NAME(FIO_STR_NAME, len)(s_);
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, resize)(s_, src_len + org_len);
  if (src)
    FIO_MEMCPY(state.buf + org_len, src, src_len);
  return state;
}

/* *****************************************************************************


                             String Implementation

                               IMPLEMENTATION


***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/* *****************************************************************************
String Implementation - Memory management
***************************************************************************** */

/**
 * Reserves at least `amount` of bytes for the string's data (reserved count
 * includes used data).
 *
 * Returns the current state of the String.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              FIO_STR_RESERVE_NAME)(FIO_STR_PTR s_,
                                                    size_t amount) {
  fio_str_info_s i = {0};
  FIO_NAME(FIO_STR_NAME, s) *const s =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s_ || !s || FIO_STR_IS_FROZEN(s)) {
    i = FIO_NAME(FIO_STR_NAME, info)(s_);
    return i;
  }
  /* result is an embedded string */
  if (amount <= FIO_STR_SMALL_CAPA(s)) {
    if (!FIO_STR_IS_SMALL(s)) {
      /* shrink from allocated(?) string */
      FIO_NAME(FIO_STR_NAME, s) tmp = FIO_STR_INIT;
      FIO_NAME(FIO_STR_NAME, init_copy)
      ((FIO_STR_PTR)FIO_PTR_TAG(&tmp),
       FIO_STR_BIG_DATA(s),
       ((FIO_STR_SMALL_CAPA(s) > FIO_STR_BIG_LEN(s)) ? FIO_STR_BIG_LEN(s)
                                                     : FIO_STR_SMALL_CAPA(s)));
      if (FIO_STR_BIG_IS_DYNAMIC(s))
        FIO_STR_BIG_FREE_BUF(s);
      *s = tmp;
    }
    i = (fio_str_info_s){
        .buf = FIO_STR_SMALL_DATA(s),
        .len = FIO_STR_SMALL_LEN(s),
        .capa = FIO_STR_SMALL_CAPA(s),
    };
    return i;
  }
  /* round up to allocation boundary */
  amount = FIO_STR_CAPA2WORDS(amount);
  if (FIO_STR_IS_SMALL(s)) {
    /* from small to big */
    FIO_NAME(FIO_STR_NAME, s) tmp = FIO_STR_INIT;
    FIO_NAME(FIO_STR_NAME, init_copy)
    ((FIO_STR_PTR)FIO_PTR_TAG(&tmp), NULL, amount);
    FIO_MEMCPY(FIO_STR_BIG_DATA(&tmp),
               FIO_STR_SMALL_DATA(s),
               FIO_STR_SMALL_CAPA(s));
    FIO_STR_BIG_LEN_SET(&tmp, FIO_STR_SMALL_LEN(s));
    *s = tmp;
    i = (fio_str_info_s){
        .buf = FIO_STR_BIG_DATA(s),
        .len = FIO_STR_BIG_LEN(s),
        .capa = amount,
    };
    return i;
  } else if (FIO_STR_BIG_IS_DYNAMIC(s) && FIO_STR_BIG_CAPA(s) == amount) {
    i = (fio_str_info_s){
        .buf = FIO_STR_BIG_DATA(s),
        .len = FIO_STR_BIG_LEN(s),
        .capa = amount,
    };
  } else {
    /* from big to big - grow / shrink */
    const size_t __attribute__((unused)) old_capa = FIO_STR_BIG_CAPA(s);
    size_t data_len = FIO_STR_BIG_LEN(s);
    if (data_len > amount) {
      /* truncate */
      data_len = amount;
      FIO_STR_BIG_LEN_SET(s, data_len);
    }
    char *tmp = NULL;
    if (FIO_STR_BIG_IS_DYNAMIC(s)) {
      tmp = (char *)FIO_MEM_REALLOC_(FIO_STR_BIG_DATA(s),
                                     old_capa,
                                     (amount + 1) * sizeof(char),
                                     data_len);
      (void)old_capa; /* might not be used by macro */
    } else {
      tmp = (char *)FIO_MEM_REALLOC_(NULL, 0, (amount + 1) * sizeof(char), 0);
      if (tmp) {
        s->special = 0;
        tmp[data_len] = 0;
        if (data_len)
          FIO_MEMCPY(tmp, FIO_STR_BIG_DATA(s), data_len);
      }
    }
    if (tmp) {
      tmp[data_len] = 0;
      FIO_STR_BIG_DATA(s) = tmp;
      FIO_STR_BIG_CAPA_SET(s, amount);
    } else {
      amount = FIO_STR_BIG_CAPA(s);
    }
    i = (fio_str_info_s){
        .buf = FIO_STR_BIG_DATA(s),
        .len = data_len,
        .capa = amount,
    };
  }
  return i;
}

/* *****************************************************************************
String Implementation - UTF-8 State
***************************************************************************** */

#ifndef FIO_STR_UTF8_CODE_POINT
/**
 * Maps the first 5 bits in a byte (0b11111xxx) to a UTF-8 codepoint length.
 *
 * Codepoint length 0 == error.
 *
 * The first valid length can be any value between 1 to 4.
 *
 * A continuation byte (second, third or forth) valid length marked as 5.
 *
 * To map was populated using the following Ruby script:
 *
 *      map = []; 32.times { map << 0 }; (0..0b1111).each {|i| map[i] = 1} ;
 *      (0b10000..0b10111).each {|i| map[i] = 5} ;
 *      (0b11000..0b11011).each {|i| map[i] = 2} ;
 *      (0b11100..0b11101).each {|i| map[i] = 3} ;
 *      map[0b11110] = 4; map;
 */
static __attribute__((unused))
uint8_t fio__str_utf8_map[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                               5, 5, 5, 5, 5, 5, 5, 5, 2, 2, 2, 2, 3, 3, 4, 0};

/**
 * Advances the `ptr` by one utf-8 character, placing the value of the UTF-8
 * character into the i32 variable (which must be a signed integer with 32bits
 * or more). On error, `i32` will be equal to `-1` and `ptr` will not step
 * forwards.
 *
 * The `end` value is only used for overflow protection.
 */
#define FIO_STR_UTF8_CODE_POINT(ptr, end, i32)                                 \
  do {                                                                         \
    switch (fio__str_utf8_map[((uint8_t *)(ptr))[0] >> 3]) {                   \
    case 1:                                                                    \
      (i32) = ((uint8_t *)(ptr))[0];                                           \
      ++(ptr);                                                                 \
      break;                                                                   \
    case 2:                                                                    \
      if (((ptr) + 2 > (end)) ||                                               \
          fio__str_utf8_map[((uint8_t *)(ptr))[1] >> 3] != 5) {                \
        (i32) = -1;                                                            \
        break;                                                                 \
      }                                                                        \
      (i32) =                                                                  \
          ((((uint8_t *)(ptr))[0] & 31) << 6) | (((uint8_t *)(ptr))[1] & 63);  \
      (ptr) += 2;                                                              \
      break;                                                                   \
    case 3:                                                                    \
      if (((ptr) + 3 > (end)) ||                                               \
          fio__str_utf8_map[((uint8_t *)(ptr))[1] >> 3] != 5 ||                \
          fio__str_utf8_map[((uint8_t *)(ptr))[2] >> 3] != 5) {                \
        (i32) = -1;                                                            \
        break;                                                                 \
      }                                                                        \
      (i32) = ((((uint8_t *)(ptr))[0] & 15) << 12) |                           \
              ((((uint8_t *)(ptr))[1] & 63) << 6) |                            \
              (((uint8_t *)(ptr))[2] & 63);                                    \
      (ptr) += 3;                                                              \
      break;                                                                   \
    case 4:                                                                    \
      if (((ptr) + 4 > (end)) ||                                               \
          fio__str_utf8_map[((uint8_t *)(ptr))[1] >> 3] != 5 ||                \
          fio__str_utf8_map[((uint8_t *)(ptr))[2] >> 3] != 5 ||                \
          fio__str_utf8_map[((uint8_t *)(ptr))[3] >> 3] != 5) {                \
        (i32) = -1;                                                            \
        break;                                                                 \
      }                                                                        \
      (i32) = ((((uint8_t *)(ptr))[0] & 7) << 18) |                            \
              ((((uint8_t *)(ptr))[1] & 63) << 12) |                           \
              ((((uint8_t *)(ptr))[2] & 63) << 6) |                            \
              (((uint8_t *)(ptr))[3] & 63);                                    \
      (ptr) += 4;                                                              \
      break;                                                                   \
    default:                                                                   \
      (i32) = -1;                                                              \
      break;                                                                   \
    }                                                                          \
  } while (0);
#endif

/** Returns 1 if the String is UTF-8 valid and 0 if not. */
SFUNC size_t FIO_NAME(FIO_STR_NAME, utf8_valid)(FIO_STR_PTR s_) {
  FIO_NAME(FIO_STR_NAME, s) *s = (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || !s_)
    return 0;
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, info)(s_);
  if (!state.len)
    return 1;
  char *const end = state.buf + state.len;
  int32_t c = 0;
  do {
    FIO_STR_UTF8_CODE_POINT(state.buf, end, c);
  } while (c > 0 && state.buf < end);
  return state.buf == end && c >= 0;
}

/** Returns the String's length in UTF-8 characters. */
SFUNC size_t FIO_NAME(FIO_STR_NAME, utf8_len)(FIO_STR_PTR s_) {
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, info)(s_);
  if (!state.len)
    return 0;
  char *end = state.buf + state.len;
  size_t utf8len = 0;
  int32_t c = 0;
  do {
    ++utf8len;
    FIO_STR_UTF8_CODE_POINT(state.buf, end, c);
  } while (c > 0 && state.buf < end);
  if (state.buf != end || c == -1) {
    /* invalid */
    return 0;
  }
  return utf8len;
}

/**
 * Takes a UTF-8 character selection information (UTF-8 position and length)
 * and updates the same variables so they reference the raw byte slice
 * information.
 *
 * If the String isn't UTF-8 valid up to the requested selection, than `pos`
 * will be updated to `-1` otherwise values are always positive.
 *
 * The returned `len` value may be shorter than the original if there wasn't
 * enough data left to accommodate the requested length. When a `len` value of
 * `0` is returned, this means that `pos` marks the end of the String.
 *
 * Returns -1 on error and 0 on success.
 */
SFUNC int FIO_NAME(FIO_STR_NAME,
                   utf8_select)(FIO_STR_PTR s_, intptr_t *pos, size_t *len) {
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, info)(s_);
  int32_t c = 0;
  char *p = state.buf;
  char *const end = state.buf + state.len;
  size_t start;

  if (!state.buf)
    goto error;
  if (!state.len || *pos == -1)
    goto at_end;

  if (*pos) {
    if ((*pos) > 0) {
      start = *pos;
      while (start && p < end && c >= 0) {
        FIO_STR_UTF8_CODE_POINT(p, end, c);
        --start;
      }
      if (c == -1)
        goto error;
      if (start || p >= end)
        goto at_end;
      *pos = p - state.buf;
    } else {
      /* walk backwards */
      p = state.buf + state.len - 1;
      c = 0;
      ++*pos;
      do {
        switch (fio__str_utf8_map[((uint8_t *)p)[0] >> 3]) {
        case 5:
          ++c;
          break;
        case 4:
          if (c != 3)
            goto error;
          c = 0;
          ++(*pos);
          break;
        case 3:
          if (c != 2)
            goto error;
          c = 0;
          ++(*pos);
          break;
        case 2:
          if (c != 1)
            goto error;
          c = 0;
          ++(*pos);
          break;
        case 1:
          if (c)
            goto error;
          ++(*pos);
          break;
        default:
          goto error;
        }
        --p;
      } while (p > state.buf && *pos);
      if (c)
        goto error;
      ++p; /* There's always an extra back-step */
      *pos = (p - state.buf);
    }
  }

  /* find end */
  start = *len;
  while (start && p < end && c >= 0) {
    FIO_STR_UTF8_CODE_POINT(p, end, c);
    --start;
  }
  if (c == -1 || p > end)
    goto error;
  *len = p - (state.buf + (*pos));
  return 0;

at_end:
  *pos = state.len;
  *len = 0;
  return 0;
error:
  *pos = -1;
  *len = 0;
  return -1;
}

/* *****************************************************************************
String Implementation - Content Manipulation and Review
***************************************************************************** */

/**
 * Writes a number at the end of the String using normal base 10 notation.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_i)(FIO_STR_PTR s_,
                                                     int64_t num) {
  /* because fio_ltoa uses an internal buffer, we "save" a `memcpy` loop and
   * minimize memory allocations by re-implementing the same logic in a
   * dedicated fasion. */
  FIO_NAME(FIO_STR_NAME, s) *s = (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  if (!s || FIO_STR_IS_FROZEN(s))
    return FIO_NAME(FIO_STR_NAME, info)(s_);
  fio_str_info_s i;
  if (!num)
    goto write_zero;
  {
    char buf[22];
    uint64_t l = 0;
    uint8_t neg = 0;
    int64_t t = num / 10;
    if (num < 0) {
      num = 0 - num; /* might fail due to overflow, but fixed with tail (t) */
      t = (int64_t)0 - t;
      neg = 1;
    }
    while (num) {
      buf[l++] = '0' + (num - (t * 10));
      num = t;
      t = num / 10;
    }
    if (neg) {
      buf[l++] = '-';
    }
    i = FIO_NAME(FIO_STR_NAME, resize)(s_, FIO_NAME(FIO_STR_NAME, len)(s_) + l);

    while (l) {
      --l;
      i.buf[i.len - (l + 1)] = buf[l];
    }
  }
  return i;
write_zero:
  i = FIO_NAME(FIO_STR_NAME, resize)(s_, FIO_NAME(FIO_STR_NAME, len)(s_) + 1);
  i.buf[i.len - 1] = '0';
  return i;
}

/**
 * Writes a number at the end of the String using Hex (base 16) notation.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_hex)(FIO_STR_PTR s,
                                                       int64_t num) {
  /* using base 16 with fio_ltoa, buffer might minimize memory allocation. */
  char buf[(sizeof(int64_t) * 2) + 8];
  size_t written = fio_ltoa(buf, num, 16);
  return FIO_NAME(FIO_STR_NAME, write)(s, buf, written);
}

/**
 * Appends the `src` String to the end of the `dest` String.
 *
 * If `dest` is empty, the resulting Strings will be equal.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, concat)(FIO_STR_PTR dest_,
                                                    FIO_STR_PTR const src_) {
  FIO_NAME(FIO_STR_NAME, s) *dest =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(dest_);
  FIO_NAME(FIO_STR_NAME, s) *src =
      (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(src_);
  if (!dest || !src || !dest_ || !src_ || FIO_STR_IS_FROZEN(dest)) {
    return FIO_NAME(FIO_STR_NAME, info)(dest_);
  }
  fio_str_info_s src_state = FIO_NAME(FIO_STR_NAME, info)(src_);
  if (!src_state.len)
    return FIO_NAME(FIO_STR_NAME, info)(dest_);
  const size_t old_len = FIO_NAME(FIO_STR_NAME, len)(dest_);
  fio_str_info_s state =
      FIO_NAME(FIO_STR_NAME, resize)(dest_, src_state.len + old_len);
  FIO_MEMCPY(state.buf + old_len, src_state.buf, src_state.len);
  return state;
}

/**
 * Replaces the data in the String - replacing `old_len` bytes starting at
 * `start_pos`, with the data at `src` (`src_len` bytes long).
 *
 * Negative `start_pos` values are calculated backwards, `-1` == end of
 * String.
 *
 * When `old_len` is zero, the function will insert the data at `start_pos`.
 *
 * If `src_len == 0` than `src` will be ignored and the data marked for
 * replacement will be erased.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, replace)(FIO_STR_PTR s_,
                                                     intptr_t start_pos,
                                                     size_t old_len,
                                                     const void *src,
                                                     size_t src_len) {
  FIO_NAME(FIO_STR_NAME, s) *s = (FIO_NAME(FIO_STR_NAME, s) *)FIO_PTR_UNTAG(s_);
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, info)(s_);
  if (!s_ || !s || FIO_STR_IS_FROZEN(s) || (!old_len && !src_len) ||
      (!src && src_len))
    return state;

  if (start_pos < 0) {
    /* backwards position indexing */
    start_pos += FIO_NAME(FIO_STR_NAME, len)(s_) + 1;
    if (start_pos < 0)
      start_pos = 0;
  }

  if (start_pos + old_len >= state.len) {
    /* old_len overflows the end of the String */
    if (FIO_STR_IS_SMALL(s)) {
      FIO_STR_SMALL_LEN_SET(s, start_pos);
    } else {
      FIO_STR_BIG_LEN_SET(s, start_pos);
    }
    return FIO_NAME(FIO_STR_NAME, write)(s_, src, src_len);
  }

  /* data replacement is now always in the middle (or start) of the String */
  const size_t new_size = state.len + (src_len - old_len);

  if (old_len != src_len) {
    /* there's an offset requiring an adjustment */
    if (old_len < src_len) {
      /* make room for new data */
      const size_t offset = src_len - old_len;
      state = FIO_NAME(FIO_STR_NAME, resize)(s_, state.len + offset);
    }
    memmove(state.buf + start_pos + src_len,
            state.buf + start_pos + old_len,
            (state.len - start_pos) - old_len);
  }
  if (src_len) {
    FIO_MEMCPY(state.buf + start_pos, src, src_len);
  }

  return FIO_NAME(FIO_STR_NAME, resize)(s_, new_size);
}

/**
 * Writes to the String using a vprintf like interface.
 *
 * Data is written to the end of the String.
 */
SFUNC fio_str_info_s __attribute__((format(FIO___PRINTF_STYLE, 2, 0)))
FIO_NAME(FIO_STR_NAME,
         vprintf)(FIO_STR_PTR s_, const char *format, va_list argv) {
  va_list argv_cpy;
  va_copy(argv_cpy, argv);
  int len = vsnprintf(NULL, 0, format, argv_cpy);
  va_end(argv_cpy);
  if (len <= 0)
    return FIO_NAME(FIO_STR_NAME, info)(s_);
  fio_str_info_s state =
      FIO_NAME(FIO_STR_NAME, resize)(s_, len + FIO_NAME(FIO_STR_NAME, len)(s_));
  if (state.capa >= (size_t)len)
    vsnprintf(state.buf + (state.len - len), len + 1, format, argv);
  return state;
}

/**
 * Writes to the String using a printf like interface.
 *
 * Data is written to the end of the String.
 */
SFUNC fio_str_info_s __attribute__((format(FIO___PRINTF_STYLE, 2, 3)))
FIO_NAME(FIO_STR_NAME, printf)(FIO_STR_PTR s_, const char *format, ...) {
  va_list argv;
  va_start(argv, format);
  fio_str_info_s state = FIO_NAME(FIO_STR_NAME, vprintf)(s_, format, argv);
  va_end(argv);
  return state;
}

/* *****************************************************************************
String API - C / JSON escaping
***************************************************************************** */

/* constant time (non-branching) if statement used in a loop as a helper */
#define FIO_STR_WRITE_ESCAPED_CT_OR(cond, a, b)                                \
  ((b) ^                                                                       \
   ((0 - ((((cond) | (0 - (cond))) >> ((sizeof((cond)) << 3) - 1)) & 1)) &     \
    ((a) ^ (b))))

/**
 * Writes data at the end of the String, escaping the data using JSON semantics.
 *
 * The JSON semantic are common to many programming languages, promising a UTF-8
 * String while making it easy to read and copy the string during debugging.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_escape)(FIO_STR_PTR s,
                                                          const void *src_,
                                                          size_t len) {
  const uint8_t *src = (const uint8_t *)src_;
  size_t extra_len = 0;
  size_t at = 0;
  uint8_t set_at = 1;

  /* collect escaping requiremnents */
  for (size_t i = 0; i < len; ++i) {
    /* skip valid ascii */
    if ((src[i] > 34 && src[i] < 127 && src[i] != '\\') || src[i] == '!' ||
        src[i] == ' ')
      continue;
    /* skip valid UTF-8 */
    switch (fio__str_utf8_map[src[i] >> 3]) {
    case 4:
      if (fio__str_utf8_map[src[i + 3] >> 3] != 5) {
        break; /* from switch */
      }
    /* fallthrough */
    case 3:
      if (fio__str_utf8_map[src[i + 2] >> 3] != 5) {
        break; /* from switch */
      }
    /* fallthrough */
    case 2:
      if (fio__str_utf8_map[src[i + 1] >> 3] != 5) {
        break; /* from switch */
      }
      i += fio__str_utf8_map[src[i] >> 3] - 1;
      continue;
    }
    /* store first instance of character that needs escaping */
    at = FIO_STR_WRITE_ESCAPED_CT_OR(set_at, i, at);
    set_at = 0;

    /* count extra bytes */
    switch (src[i]) {
    case '\b': /* fallthrough */
    case '\f': /* fallthrough */
    case '\n': /* fallthrough */
    case '\r': /* fallthrough */
    case '\t': /* fallthrough */
    case '"':  /* fallthrough */
    case '\\': /* fallthrough */
    case '/':  /* fallthrough */
      ++extra_len;
      break;
    default:
      /* escaping all control charactes and non-UTF-8 characters */
      extra_len += 5;
    }
  }
  /* reserve space and copy any valid "head" */
  fio_str_info_s dest;
  {
    const size_t org_len = FIO_NAME(FIO_STR_NAME, len)(s);
#if !FIO_STR_OPTIMIZE4IMMUTABILITY
    /* often, after `write_escape` come quotes */
    FIO_NAME(FIO_STR_NAME, reserve)(s, org_len + extra_len + len + 4);
#endif
    dest = FIO_NAME(FIO_STR_NAME, resize)(s, org_len + extra_len + len);
    dest.len = org_len;
  }
  dest.buf += dest.len;
  /* is escaping required? - simple memcpy if we don't need to escape */
  if (set_at) {
    FIO_MEMCPY(dest.buf, src, len);
    dest.buf -= dest.len;
    dest.len += len;
    return dest;
  }
  /* simple memcpy until first char that needs escaping */
  if (at >= 8) {
    FIO_MEMCPY(dest.buf, src, at);
  } else {
    at = 0;
  }
  /* start escaping */
  for (size_t i = at; i < len; ++i) {
    /* skip valid ascii */
    if ((src[i] > 34 && src[i] < 127 && src[i] != '\\') || src[i] == '!' ||
        src[i] == ' ') {
      dest.buf[at++] = src[i];
      continue;
    }
    /* skip valid UTF-8 */
    switch (fio__str_utf8_map[src[i] >> 3]) {
    case 4:
      if (fio__str_utf8_map[src[i + 3] >> 3] != 5) {
        break; /* from switch */
      }
    /* fallthrough */
    case 3:
      if (fio__str_utf8_map[src[i + 2] >> 3] != 5) {
        break; /* from switch */
      }
    /* fallthrough */
    case 2:
      if (fio__str_utf8_map[src[i + 1] >> 3] != 5) {
        break; /* from switch */
      }
      switch (fio__str_utf8_map[src[i] >> 3]) {
      case 4:
        dest.buf[at++] = src[i++]; /* fallthrough */
      case 3:
        dest.buf[at++] = src[i++]; /* fallthrough */
      case 2:
        dest.buf[at++] = src[i++];
        dest.buf[at++] = src[i];
      }
      continue;
    }

    /* write escape sequence */
    dest.buf[at++] = '\\';
    switch (src[i]) {
    case '\b':
      dest.buf[at++] = 'b';
      break;
    case '\f':
      dest.buf[at++] = 'f';
      break;
    case '\n':
      dest.buf[at++] = 'n';
      break;
    case '\r':
      dest.buf[at++] = 'r';
      break;
    case '\t':
      dest.buf[at++] = 't';
      break;
    case '"':
      dest.buf[at++] = '"';
      break;
    case '\\':
      dest.buf[at++] = '\\';
      break;
    case '/':
      dest.buf[at++] = '/';
      break;
    default:
      /* escaping all control charactes and non-UTF-8 characters */
      if (src[i] < 127) {
        dest.buf[at++] = 'u';
        dest.buf[at++] = '0';
        dest.buf[at++] = '0';
        dest.buf[at++] = fio_i2c(src[i] >> 4);
        dest.buf[at++] = fio_i2c(src[i] & 15);
      } else {
        /* non UTF-8 data... encode as...? */
        dest.buf[at++] = 'x';
        dest.buf[at++] = fio_i2c(src[i] >> 4);
        dest.buf[at++] = fio_i2c(src[i] & 15);
      }
    }
  }
  return FIO_NAME(FIO_STR_NAME, resize)(s, dest.len + at);
}

/**
 * Writes an escaped data into the string after unescaping the data.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, write_unescape)(FIO_STR_PTR s,
                                                            const void *src_,
                                                            size_t len) {
  fio_str_info_s dest;
  {
    const size_t org_len = FIO_NAME(FIO_STR_NAME, len)(s);
    dest = FIO_NAME(FIO_STR_NAME, resize)(s, org_len + len);
    dest.len = org_len;
  }
  size_t at = 0;
  const uint8_t *src = (const uint8_t *)src_;
  const uint8_t *end = src + len;
  dest.buf += dest.len;
  while (src < end) {
#if 1 /* A/B performance at a later stage */
    if (*src != '\\') {
      const uint8_t *escape_pos = (const uint8_t *)memchr(src, '\\', end - src);
      if (!escape_pos)
        escape_pos = end;
      const size_t valid_len = escape_pos - src;
      if (valid_len) {
        memmove(dest.buf + at, src, valid_len);
        at += valid_len;
        src = escape_pos;
      }
    }
#else
#if __x86_64__ || __aarch64__
    /* levarege unaligned memory access to test and copy 8 bytes at a time */
    while (src + 8 <= end) {
      const uint64_t wanted1 = 0x0101010101010101ULL * '\\';
      const uint64_t eq1 =
          ~((*((uint64_t *)src)) ^ wanted1); /* 0 == eq. inverted, all bits 1 */
      const uint64_t t0 = (eq1 & 0x7f7f7f7f7f7f7f7fllu) + 0x0101010101010101llu;
      const uint64_t t1 = (eq1 & 0x8080808080808080llu);
      if ((t0 & t1)) {
        break; /* from 8 byte seeking algorithm */
      }
      *(uint64_t *)(dest.buf + at) = *(uint64_t *)src;
      src += 8;
      at += 8;
    }
#endif
    while (src < end && *src != '\\') {
      dest.buf[at++] = *(src++);
    }
#endif
    if (end - src == 1) {
      dest.buf[at++] = *(src++);
    }
    if (src >= end)
      break;
    /* escaped data - src[0] == '\\' */
    ++src;
    switch (src[0]) {
    case 'b':
      dest.buf[at++] = '\b';
      ++src;
      break; /* from switch */
    case 'f':
      dest.buf[at++] = '\f';
      ++src;
      break; /* from switch */
    case 'n':
      dest.buf[at++] = '\n';
      ++src;
      break; /* from switch */
    case 'r':
      dest.buf[at++] = '\r';
      ++src;
      break; /* from switch */
    case 't':
      dest.buf[at++] = '\t';
      ++src;
      break; /* from switch */
    case 'u': {
      /* test UTF-8 notation */
      if (fio_c2i(src[1]) < 16 && fio_c2i(src[2]) < 16 &&
          fio_c2i(src[3]) < 16 && fio_c2i(src[4]) < 16) {
        uint32_t u = (((fio_c2i(src[1]) << 4) | fio_c2i(src[2])) << 8) |
                     ((fio_c2i(src[3]) << 4) | fio_c2i(src[4]));
        if (((fio_c2i(src[1]) << 4) | fio_c2i(src[2])) == 0xD8U &&
            src[5] == '\\' && src[6] == 'u' && fio_c2i(src[7]) < 16 &&
            fio_c2i(src[8]) < 16 && fio_c2i(src[9]) < 16 &&
            fio_c2i(src[10]) < 16) {
          /* surrogate-pair */
          u = (u & 0x03FF) << 10;
          u |= (((((fio_c2i(src[7]) << 4) | fio_c2i(src[8])) << 8) |
                 ((fio_c2i(src[9]) << 4) | fio_c2i(src[10]))) &
                0x03FF);
          u += 0x10000;
          src += 6;
        }
        if (u <= 127) {
          dest.buf[at++] = u;
        } else if (u <= 2047) {
          dest.buf[at++] = 192 | (u >> 6);
          dest.buf[at++] = 128 | (u & 63);
        } else if (u <= 65535) {
          dest.buf[at++] = 224 | (u >> 12);
          dest.buf[at++] = 128 | ((u >> 6) & 63);
          dest.buf[at++] = 128 | (u & 63);
        } else {
          dest.buf[at++] = 240 | ((u >> 18) & 7);
          dest.buf[at++] = 128 | ((u >> 12) & 63);
          dest.buf[at++] = 128 | ((u >> 6) & 63);
          dest.buf[at++] = 128 | (u & 63);
        }
        src += 5;
        break; /* from switch */
      } else
        goto invalid_escape;
    }
    case 'x': { /* test for hex notation */
      if (fio_c2i(src[1]) < 16 && fio_c2i(src[2]) < 16) {
        dest.buf[at++] = (fio_c2i(src[1]) << 4) | fio_c2i(src[2]);
        src += 3;
        break; /* from switch */
      } else
        goto invalid_escape;
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': { /* test for octal notation */
      if (src[0] >= '0' && src[0] <= '7' && src[1] >= '0' && src[1] <= '7') {
        dest.buf[at++] = ((src[0] - '0') << 3) | (src[1] - '0');
        src += 2;
        break; /* from switch */
      } else
        goto invalid_escape;
    }
    case '"':
    case '\\':
    case '/':
    /* fallthrough */
    default:
    invalid_escape:
      dest.buf[at++] = *(src++);
    }
  }
  return FIO_NAME(FIO_STR_NAME, resize)(s, dest.len + at);
}

#undef FIO_STR_WRITE_ESCAPED_CT_OR

/* *****************************************************************************
String - Base64 support
***************************************************************************** */

/**
 * Writes data at the end of the String, encoding the data as Base64 encoded
 * data.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              write_base64enc)(FIO_STR_PTR s_,
                                               const void *data,
                                               size_t len,
                                               uint8_t url_encoded) {
  if (!s_ || !FIO_PTR_UNTAG(s_) || !len ||
      FIO_NAME_BL(FIO_STR_NAME, frozen)(s_))
    return FIO_NAME(FIO_STR_NAME, info)(s_);

  /* the base64 encoding array */
  const char *encoding;
  if (url_encoded == 0) {
    encoding =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
  } else {
    encoding =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";
  }

  /* base64 length and padding information */
  int groups = len / 3;
  const int mod = len - (groups * 3);
  const int target_size = (groups + (mod != 0)) * 4;
  const uint32_t org_len = FIO_NAME(FIO_STR_NAME, len)(s_);
  fio_str_info_s i = FIO_NAME(FIO_STR_NAME, resize)(s_, org_len + target_size);
  char *writer = i.buf + org_len;
  const unsigned char *reader = (const unsigned char *)data;

  /* write encoded data */
  while (groups) {
    --groups;
    const unsigned char tmp1 = *(reader++);
    const unsigned char tmp2 = *(reader++);
    const unsigned char tmp3 = *(reader++);

    *(writer++) = encoding[(tmp1 >> 2) & 63];
    *(writer++) = encoding[(((tmp1 & 3) << 4) | ((tmp2 >> 4) & 15))];
    *(writer++) = encoding[((tmp2 & 15) << 2) | ((tmp3 >> 6) & 3)];
    *(writer++) = encoding[tmp3 & 63];
  }

  /* write padding / ending */
  switch (mod) {
  case 2: {
    const unsigned char tmp1 = *(reader++);
    const unsigned char tmp2 = *(reader++);

    *(writer++) = encoding[(tmp1 >> 2) & 63];
    *(writer++) = encoding[((tmp1 & 3) << 4) | ((tmp2 >> 4) & 15)];
    *(writer++) = encoding[((tmp2 & 15) << 2)];
    *(writer++) = '=';
  } break;
  case 1: {
    const unsigned char tmp1 = *(reader++);

    *(writer++) = encoding[(tmp1 >> 2) & 63];
    *(writer++) = encoding[(tmp1 & 3) << 4];
    *(writer++) = '=';
    *(writer++) = '=';
  } break;
  }
  return i;
}

/**
 * Writes decoded base64 data to the end of the String.
 */
IFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME,
                              write_base64dec)(FIO_STR_PTR s_,
                                               const void *encoded_,
                                               size_t len) {
  /*
  Base64 decoding array. Generation script (Ruby):

a = []; a[255] = 0
s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=".bytes;
s.length.times {|i| a[s[i]] = (i << 1) | 1 };
s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,".bytes;
s.length.times {|i| a[s[i]] = (i << 1) | 1 };
s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_".bytes;
s.length.times {|i| a[s[i]] = (i << 1) | 1 }; a.map!{ |i| i.to_i }; a

  */
  const uint8_t base64_decodes[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   125, 127,
      125, 0,   127, 105, 107, 109, 111, 113, 115, 117, 119, 121, 123, 0,   0,
      0,   129, 0,   0,   0,   1,   3,   5,   7,   9,   11,  13,  15,  17,  19,
      21,  23,  25,  27,  29,  31,  33,  35,  37,  39,  41,  43,  45,  47,  49,
      51,  0,   0,   0,   0,   127, 0,   53,  55,  57,  59,  61,  63,  65,  67,
      69,  71,  73,  75,  77,  79,  81,  83,  85,  87,  89,  91,  93,  95,  97,
      99,  101, 103, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0};
#define FIO_BASE64_BITVAL(x) ((base64_decodes[(x)] >> 1) & 63)

  if (!s_ || !FIO_PTR_UNTAG(s_) || !len || !encoded_ ||
      FIO_NAME_BL(FIO_STR_NAME, frozen)(s_))
    return FIO_NAME(FIO_STR_NAME, info)(s_);

  const uint8_t *encoded = (const uint8_t *)encoded_;

  /* skip unknown data at end */
  while (len && !base64_decodes[encoded[len - 1]]) {
    len--;
  }

  /* reserve memory space */
  const uint32_t org_len = FIO_NAME(FIO_STR_NAME, len)(s_);
  fio_str_info_s i =
      FIO_NAME(FIO_STR_NAME, resize)(s_, org_len + ((len >> 2) * 3) + 3);
  i.buf += org_len;

  /* decoded and count actual length */
  int32_t written = 0;
  uint8_t tmp1, tmp2, tmp3, tmp4;
  while (len >= 4) {
    if (isspace((*encoded))) {
      while (len && isspace((*encoded))) {
        len--;
        encoded++;
      }
      continue;
    }
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    tmp3 = *(encoded++);
    tmp4 = *(encoded++);
    if (!base64_decodes[tmp1] || !base64_decodes[tmp2] ||
        !base64_decodes[tmp3] || !base64_decodes[tmp4]) {
      return (fio_str_info_s){.buf = NULL};
    }
    *(i.buf++) =
        (FIO_BASE64_BITVAL(tmp1) << 2) | (FIO_BASE64_BITVAL(tmp2) >> 4);
    *(i.buf++) =
        (FIO_BASE64_BITVAL(tmp2) << 4) | (FIO_BASE64_BITVAL(tmp3) >> 2);
    *(i.buf++) = (FIO_BASE64_BITVAL(tmp3) << 6) | (FIO_BASE64_BITVAL(tmp4));
    /* make sure we don't loop forever */
    len -= 4;
    /* count written bytes */
    written += 3;
  }
  /* skip white spaces */
  while (len && isspace((*encoded))) {
    len--;
    encoded++;
  }
  /* decode "tail" - if any (mis-encoded, shouldn't happen) */
  tmp1 = 0;
  tmp2 = 0;
  tmp3 = 0;
  tmp4 = 0;
  switch (len) {
  case 1:
    tmp1 = *(encoded++);
    if (!base64_decodes[tmp1]) {
      return (fio_str_info_s){.buf = NULL};
    }
    *(i.buf++) = FIO_BASE64_BITVAL(tmp1);
    written += 1;
    break;
  case 2:
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    if (!base64_decodes[tmp1] || !base64_decodes[tmp2]) {
      return (fio_str_info_s){.buf = NULL};
    }
    *(i.buf++) =
        (FIO_BASE64_BITVAL(tmp1) << 2) | (FIO_BASE64_BITVAL(tmp2) >> 6);
    *(i.buf++) = (FIO_BASE64_BITVAL(tmp2) << 4);
    written += 2;
    break;
  case 3:
    tmp1 = *(encoded++);
    tmp2 = *(encoded++);
    tmp3 = *(encoded++);
    if (!base64_decodes[tmp1] || !base64_decodes[tmp2] ||
        !base64_decodes[tmp3]) {
      return (fio_str_info_s){.buf = NULL};
    }
    *(i.buf++) =
        (FIO_BASE64_BITVAL(tmp1) << 2) | (FIO_BASE64_BITVAL(tmp2) >> 6);
    *(i.buf++) =
        (FIO_BASE64_BITVAL(tmp2) << 4) | (FIO_BASE64_BITVAL(tmp3) >> 2);
    *(i.buf++) = FIO_BASE64_BITVAL(tmp3) << 6;
    written += 3;
    break;
  }
#undef FIO_BASE64_BITVAL

  if (encoded[-1] == '=') {
    i.buf--;
    written--;
    if (encoded[-2] == '=') {
      i.buf--;
      written--;
    }
    if (written < 0)
      written = 0;
  }

  // if (FIO_STR_IS_SMALL(s_))
  //   FIO_STR_SMALL_LEN_SET(s_, (org_len + written));
  // else
  //   FIO_STR_BIG_LEN_SET(s_, (org_len + written));

  return FIO_NAME(FIO_STR_NAME, resize)(s_, org_len + written);
}

/* *****************************************************************************
String - read file
***************************************************************************** */

/**
 * Reads data from a file descriptor `fd` at offset `start_at` and pastes it's
 * contents (or a slice of it) at the end of the String. If `limit == 0`, than
 * the data will be read until EOF.
 *
 * The file should be a regular file or the operation might fail (can't be used
 * for sockets).
 *
 * The file descriptor will remain open and should be closed manually.
 *
 * Currently implemented only on POSIX systems.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, readfd)(FIO_STR_PTR s_,
                                                    int fd,
                                                    intptr_t start_at,
                                                    intptr_t limit) {
  struct stat f_data;
  fio_str_info_s state = {.buf = NULL};
  if (fd == -1 || fstat(fd, &f_data) == -1) {
    return state;
  }

  if (f_data.st_size <= 0 || start_at >= f_data.st_size) {
    state = FIO_NAME(FIO_STR_NAME, info)(s_);
    return state;
  }

  if (limit <= 0 || f_data.st_size < (limit + start_at)) {
    limit = f_data.st_size - start_at;
  }

  const size_t org_len = FIO_NAME(FIO_STR_NAME, len)(s_);
  size_t write_pos = org_len;
  state = FIO_NAME(FIO_STR_NAME, resize)(s_, org_len + limit);
  if (state.capa < (org_len + limit) || !state.buf) {
    return state;
  }

  while (limit) {
    /* copy up to 128Mb at a time... why? because pread might fail */
    const size_t to_read =
        (limit & (((size_t)1 << 27) - 1)) | ((!!(limit >> 27)) << 27);
    if (pread(fd, state.buf + write_pos, to_read, start_at) !=
        (ssize_t)to_read) {
      goto error;
    }
    limit -= to_read;
    write_pos += to_read;
    start_at += to_read;
  }
  return state;

error:
  FIO_NAME(FIO_STR_NAME, resize)(s_, org_len);
  state.buf = NULL;
  state.len = state.capa = 0;
  return state;
}

/**
 * Opens the file `filename` and pastes it's contents (or a slice ot it) at
 * the end of the String. If `limit == 0`, than the data will be read until
 * EOF.
 *
 * If the file can't be located, opened or read, or if `start_at` is beyond
 * the EOF position, NULL is returned in the state's `data` field.
 *
 * Currently implemented only on POSIX systems.
 */
SFUNC fio_str_info_s FIO_NAME(FIO_STR_NAME, readfile)(FIO_STR_PTR s_,
                                                      const char *filename,
                                                      intptr_t start_at,
                                                      intptr_t limit) {
  fio_str_info_s state = {.buf = NULL};
  /* POSIX implementations. */
  int fd = fio_filename_open(filename, O_RDONLY);
  if (fd == -1)
    return state;
  state = FIO_NAME(FIO_STR_NAME, readfd)(s_, fd, start_at, limit);
  close(fd);
  return state;
}

/* *****************************************************************************


                                    String Test


***************************************************************************** */
#ifdef FIO_STR_WRITE_TEST_FUNC

/**
 * Tests the fio_str functionality.
 */
SFUNC void FIO_NAME_TEST(stl, FIO_STR_NAME)(void) {
  FIO_NAME(FIO_STR_NAME, s) str = {0}; /* test zeroed out memory */
#define FIO__STR_SMALL_CAPA FIO_STR_SMALL_CAPA(&str)
  fprintf(
      stderr,
      "* Testing core string features for " FIO_MACRO2STR(FIO_STR_NAME) ".\n");
  fprintf(stderr,
          "* String container size (without wrapper): %zu\n",
          sizeof(FIO_NAME(FIO_STR_NAME, s)));
  fprintf(stderr,
          "* Self-contained capacity (FIO_STR_SMALL_CAPA): %zu\n",
          FIO__STR_SMALL_CAPA);
  FIO_ASSERT(!FIO_NAME_BL(FIO_STR_NAME, frozen)(&str), "new string is frozen");
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) == FIO__STR_SMALL_CAPA,
             "small string capacity returned %zu",
             FIO_NAME(FIO_STR_NAME, capa)(&str));
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) == 0,
             "small string length reporting error!");
  FIO_ASSERT(
      FIO_NAME2(FIO_STR_NAME, ptr)(&str) == ((char *)(&str) + 1),
      "small string pointer reporting error (%zd offset)!",
      (ssize_t)(((char *)(&str) + 1) - FIO_NAME2(FIO_STR_NAME, ptr)(&str)));
  FIO_NAME(FIO_STR_NAME, write)(&str, "World", 4);
  FIO_ASSERT(FIO_STR_IS_SMALL(&str),
             "small string writing error - not small on small write!");
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) == FIO__STR_SMALL_CAPA,
             "Small string capacity reporting error after write!");
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) == 4,
             "small string length reporting error after write!");
  FIO_ASSERT(FIO_NAME2(FIO_STR_NAME, ptr)(&str) == (char *)&str + 1,
             "small string pointer reporting error after write!");
  FIO_ASSERT(!FIO_NAME2(FIO_STR_NAME, ptr)(&str)[4] &&
                 strlen(FIO_NAME2(FIO_STR_NAME, ptr)(&str)) == 4,
             "small string NUL missing after write (%zu)!",
             strlen(FIO_NAME2(FIO_STR_NAME, ptr)(&str)));
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Worl"),
             "small string write error (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));
  FIO_ASSERT(FIO_NAME2(FIO_STR_NAME, ptr)(&str) ==
                 FIO_NAME(FIO_STR_NAME, info)(&str).buf,
             "small string `data` != `info.buf` (%p != %p)",
             (void *)FIO_NAME2(FIO_STR_NAME, ptr)(&str),
             (void *)FIO_NAME(FIO_STR_NAME, info)(&str).buf);

  FIO_NAME(FIO_STR_NAME, FIO_STR_RESERVE_NAME)
  (&str, sizeof(FIO_NAME(FIO_STR_NAME, s)));
  FIO_ASSERT(!FIO_STR_IS_SMALL(&str),
             "Long String reporting as small after capacity update!");
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) >=
                 sizeof(FIO_NAME(FIO_STR_NAME, s)) - 1,
             "Long String capacity update error (%zu != %zu)!",
             FIO_NAME(FIO_STR_NAME, capa)(&str),
             FIO_STR_SMALL_CAPA(&str));

  FIO_ASSERT(FIO_NAME2(FIO_STR_NAME, ptr)(&str) ==
                 FIO_NAME(FIO_STR_NAME, info)(&str).buf,
             "Long String `ptr` !>= "
             "`cstr(s).buf` (%p != %p)",
             (void *)FIO_NAME2(FIO_STR_NAME, ptr)(&str),
             (void *)FIO_NAME(FIO_STR_NAME, info)(&str).buf);

#if FIO_STR_OPTIMIZE4IMMUTABILITY
  /* immutable string length is updated after `reserve` to reflect new capa */
  FIO_NAME(FIO_STR_NAME, resize)(&str, 4);
#endif
  FIO_ASSERT(
      FIO_NAME(FIO_STR_NAME, len)(&str) == 4,
      "Long String length changed during conversion from small string (%zu)!",
      FIO_NAME(FIO_STR_NAME, len)(&str));
  FIO_ASSERT(FIO_NAME2(FIO_STR_NAME, ptr)(&str) == str.buf,
             "Long String pointer reporting error after capacity update!");
  FIO_ASSERT(strlen(FIO_NAME2(FIO_STR_NAME, ptr)(&str)) == 4,
             "Long String NUL missing after capacity update (%zu)!",
             strlen(FIO_NAME2(FIO_STR_NAME, ptr)(&str)));
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Worl"),
             "Long String value changed after capacity update (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_NAME(FIO_STR_NAME, write)(&str, "d!", 2);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "World!"),
             "Long String `write` error (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_NAME(FIO_STR_NAME, replace)(&str, 0, 0, "Hello ", 6);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello World!"),
             "Long String `insert` error (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_NAME(FIO_STR_NAME, resize)(&str, 6);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello "),
             "Long String `resize` clipping error (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_NAME(FIO_STR_NAME, replace)(&str, 6, 0, "My World!", 9);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello My World!"),
             "Long String `replace` error when testing overflow (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_NAME(FIO_STR_NAME, FIO_STR_RESERVE_NAME)
  (&str, FIO_NAME(FIO_STR_NAME, len)(&str)); /* may truncate */

  FIO_NAME(FIO_STR_NAME, replace)(&str, -10, 2, "Big", 3);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello Big World!"),
             "Long String `replace` error when testing splicing (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) ==
                     FIO_STR_CAPA2WORDS(strlen("Hello Big World!")) ||
                 !FIO_NAME_BL(FIO_STR_NAME, allocated)(&str),
             "Long String `replace` capacity update error "
             "(%zu >=? %zu)!",
             FIO_NAME(FIO_STR_NAME, capa)(&str),
             FIO_STR_CAPA2WORDS(strlen("Hello Big World!")));

  if (FIO_NAME(FIO_STR_NAME, len)(&str) < (sizeof(str) - 2)) {
    FIO_NAME(FIO_STR_NAME, compact)(&str);
    FIO_ASSERT(FIO_STR_IS_SMALL(&str),
               "Compacting didn't change String to small!");
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) == strlen("Hello Big World!"),
               "Compacting altered String length! (%zu != %zu)!",
               FIO_NAME(FIO_STR_NAME, len)(&str),
               strlen("Hello Big World!"));
    FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello Big World!"),
               "Compact data error (%s)!",
               FIO_NAME2(FIO_STR_NAME, ptr)(&str));
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) == sizeof(str) - 2,
               "Compacted String capacity reporting error!");
  } else {
    fprintf(stderr, "* skipped `compact` test (irrelevent for type).\n");
  }

  {
    FIO_NAME(FIO_STR_NAME, freeze)(&str);
    FIO_ASSERT(FIO_NAME_BL(FIO_STR_NAME, frozen)(&str),
               "Frozen String not flagged as frozen.");
    fio_str_info_s old_state = FIO_NAME(FIO_STR_NAME, info)(&str);
    FIO_NAME(FIO_STR_NAME, write)(&str, "more data to be written here", 28);
    FIO_NAME(FIO_STR_NAME, replace)
    (&str, 2, 1, "more data to be written here", 28);
    fio_str_info_s new_state = FIO_NAME(FIO_STR_NAME, info)(&str);
    FIO_ASSERT(old_state.len == new_state.len, "Frozen String length changed!");
    FIO_ASSERT(old_state.buf == new_state.buf,
               "Frozen String pointer changed!");
    FIO_ASSERT(
        old_state.capa == new_state.capa,
        "Frozen String capacity changed (allowed, but shouldn't happen)!");
    FIO_STR_THAW_(&str);
  }
  FIO_NAME(FIO_STR_NAME, printf)(&str, " %u", 42);
  FIO_ASSERT(!strcmp(FIO_NAME2(FIO_STR_NAME, ptr)(&str), "Hello Big World! 42"),
             "`printf` data error (%s)!",
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));

  {
    FIO_NAME(FIO_STR_NAME, s) str2 = FIO_STR_INIT;
    FIO_NAME(FIO_STR_NAME, concat)(&str2, &str);
    FIO_ASSERT(FIO_NAME_BL(FIO_STR_NAME, eq)(&str, &str2),
               "`concat` error, strings not equal (%s != %s)!",
               FIO_NAME2(FIO_STR_NAME, ptr)(&str),
               FIO_NAME2(FIO_STR_NAME, ptr)(&str2));
    FIO_NAME(FIO_STR_NAME, write)(&str2, ":extra data", 11);
    FIO_ASSERT(!FIO_NAME_BL(FIO_STR_NAME, eq)(&str, &str2),
               "`write` error after copy, strings equal "
               "((%zu)%s == (%zu)%s)!",
               FIO_NAME(FIO_STR_NAME, len)(&str),
               FIO_NAME2(FIO_STR_NAME, ptr)(&str),
               FIO_NAME(FIO_STR_NAME, len)(&str2),
               FIO_NAME2(FIO_STR_NAME, ptr)(&str2));

    FIO_NAME(FIO_STR_NAME, destroy)(&str2);
  }

  FIO_NAME(FIO_STR_NAME, destroy)(&str);

  FIO_NAME(FIO_STR_NAME, write_i)(&str, -42);
  FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) == 3 &&
                 !memcmp("-42", FIO_NAME2(FIO_STR_NAME, ptr)(&str), 3),
             "write_i output error ((%zu) %s != -42)",
             FIO_NAME(FIO_STR_NAME, len)(&str),
             FIO_NAME2(FIO_STR_NAME, ptr)(&str));
  FIO_NAME(FIO_STR_NAME, destroy)(&str);

  {
    fprintf(stderr, "* Testing string `readfile`.\n");
    FIO_NAME(FIO_STR_NAME, s) *s = FIO_NAME(FIO_STR_NAME, new)();
    FIO_ASSERT(FIO_PTR_UNTAG(s),
               "error, string not allocated (%p)!",
               (void *)s);
    fio_str_info_s state = FIO_NAME(FIO_STR_NAME, readfile)(s, __FILE__, 0, 0);

    FIO_ASSERT(state.len && state.buf,
               "error, no data was read for file %s!",
               __FILE__);

    FIO_ASSERT(!memcmp(state.buf,
                       "/* "
                       "******************************************************"
                       "***********************",
                       80),
               "content error, header mismatch!\n %s",
               state.buf);
    fprintf(stderr, "* Testing UTF-8 validation and length.\n");
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_valid)(s),
               "`utf8_valid` error, code in this file "
               "should be valid!");
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_len)(s) &&
                   (FIO_NAME(FIO_STR_NAME, utf8_len)(s) <=
                    FIO_NAME(FIO_STR_NAME, len)(s)) &&
                   (FIO_NAME(FIO_STR_NAME, utf8_len)(s) >=
                    (FIO_NAME(FIO_STR_NAME, len)(s)) >> 1),
               "`utf8_len` error, invalid value (%zu / %zu!",
               FIO_NAME(FIO_STR_NAME, utf8_len)(s),
               FIO_NAME(FIO_STR_NAME, len)(s));

    if (1) {
      /* String content == whole file (this file) */
      intptr_t pos = -11;
      size_t len = 20;
      fprintf(stderr, "* Testing UTF-8 positioning.\n");

      FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_select)(s, &pos, &len) == 0,
                 "`select` returned error for negative "
                 "pos! (%zd, %zu)",
                 (ssize_t)pos,
                 len);
      FIO_ASSERT(pos ==
                     (intptr_t)state.len - 10, /* no UTF-8 bytes in this file */
                 "`utf8_select` error, negative position "
                 "invalid! (%zd)",
                 (ssize_t)pos);
      FIO_ASSERT(len == 10,
                 "`utf8_select` error, trancated length "
                 "invalid! (%zd)",
                 (ssize_t)len);
      pos = 10;
      len = 20;
      FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_select)(s, &pos, &len) == 0,
                 "`utf8_select` returned error! (%zd, %zu)",
                 (ssize_t)pos,
                 len);
      FIO_ASSERT(pos == 10,
                 "`utf8_select` error, position invalid! (%zd)",
                 (ssize_t)pos);
      FIO_ASSERT(len == 20,
                 "`utf8_select` error, length invalid! (%zd)",
                 (ssize_t)len);
    }
    FIO_NAME(FIO_STR_NAME, free)(s);
  }
  FIO_NAME(FIO_STR_NAME, destroy)(&str);
  if (1) {
    /* Testing UTF-8 */
    const char *utf8_sample = /* three hearts, small-big-small*/
        "\xf0\x9f\x92\x95\xe2\x9d\xa4\xef\xb8\x8f\xf0\x9f\x92\x95";
    FIO_NAME(FIO_STR_NAME, write)(&str, utf8_sample, strlen(utf8_sample));
    intptr_t pos = -2;
    size_t len = 2;
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_select)(&str, &pos, &len) == 0,
               "`utf8_select` returned error for negative pos on "
               "UTF-8 data! (%zd, %zu)",
               (ssize_t)pos,
               len);
    FIO_ASSERT(pos == (intptr_t)FIO_NAME(FIO_STR_NAME, len)(&str) -
                          4, /* 4 byte emoji */
               "`utf8_select` error, negative position invalid on "
               "UTF-8 data! (%zd)",
               (ssize_t)pos);
    FIO_ASSERT(len == 4, /* last utf-8 char is 4 byte long */
               "`utf8_select` error, trancated length invalid on "
               "UTF-8 data! (%zd)",
               (ssize_t)len);
    pos = 1;
    len = 20;
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_select)(&str, &pos, &len) == 0,
               "`utf8_select` returned error on UTF-8 data! "
               "(%zd, %zu)",
               (ssize_t)pos,
               len);
    FIO_ASSERT(pos == 4,
               "`utf8_select` error, position invalid on "
               "UTF-8 data! (%zd)",
               (ssize_t)pos);
    FIO_ASSERT(len == 10,
               "`utf8_select` error, length invalid on "
               "UTF-8 data! (%zd)",
               (ssize_t)len);
    pos = 1;
    len = 3;
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, utf8_select)(&str, &pos, &len) == 0,
               "`utf8_select` returned error on UTF-8 data "
               "(2)! (%zd, %zu)",
               (ssize_t)pos,
               len);
    FIO_ASSERT(len ==
                   10, /* 3 UTF-8 chars: 4 byte + 4 byte + 2 byte codes == 10 */
               "`utf8_select` error, length invalid on UTF-8 data! "
               "(%zd)",
               (ssize_t)len);
  }
  FIO_NAME(FIO_STR_NAME, destroy)(&str);
  if (1) {
    /* Testing Static initialization and writing */
#if FIO_STR_OPTIMIZE4IMMUTABILITY
    FIO_NAME(FIO_STR_NAME, init_const)(&str, "Welcome", 7);
#else
    str = (FIO_NAME(FIO_STR_NAME, s))FIO_STR_INIT_STATIC("Welcome");
#endif
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) == 0 ||
                   FIO_STR_IS_SMALL(&str),
               "Static string capacity non-zero.");
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) > 0,
               "Static string length should be automatically calculated.");
    FIO_ASSERT(!FIO_NAME_BL(FIO_STR_NAME, allocated)(&str),
               "Static strings shouldn't be dynamic.");
    FIO_NAME(FIO_STR_NAME, destroy)(&str);

#if FIO_STR_OPTIMIZE4IMMUTABILITY
    FIO_NAME(FIO_STR_NAME, init_const)
    (&str,
     "Welcome to a very long static string that should not fit within a "
     "containing struct... hopefuly",
     95);
#else
    str = (FIO_NAME(FIO_STR_NAME, s))FIO_STR_INIT_STATIC(
        "Welcome to a very long static string that should not fit within a "
        "containing struct... hopefuly");
#endif
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, capa)(&str) == 0 ||
                   FIO_STR_IS_SMALL(&str),
               "Static string capacity non-zero.");
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) > 0,
               "Static string length should be automatically calculated.");
    FIO_ASSERT(!FIO_NAME_BL(FIO_STR_NAME, allocated)(&str),
               "Static strings shouldn't be dynamic.");
    FIO_NAME(FIO_STR_NAME, destroy)(&str);

#if FIO_STR_OPTIMIZE4IMMUTABILITY
    FIO_NAME(FIO_STR_NAME, init_const)(&str, "Welcome", 7);
#else
    str = (FIO_NAME(FIO_STR_NAME, s))FIO_STR_INIT_STATIC("Welcome");
#endif
    fio_str_info_s state = FIO_NAME(FIO_STR_NAME, write)(&str, " Home", 5);
    FIO_ASSERT(state.capa > 0, "Static string not converted to non-static.");
    FIO_ASSERT(FIO_NAME_BL(FIO_STR_NAME, allocated)(&str) ||
                   FIO_STR_IS_SMALL(&str),
               "String should be dynamic after `write`.");

    char *cstr = FIO_NAME(FIO_STR_NAME, detach)(&str);
    FIO_ASSERT(cstr, "`detach` returned NULL");
    FIO_ASSERT(!memcmp(cstr, "Welcome Home\0", 13),
               "`detach` string error: %s",
               cstr);
    FIO_MEM_FREE(cstr, state.capa);
    FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&str) == 0,
               "`detach` data wasn't cleared.");
    FIO_NAME(FIO_STR_NAME, destroy)
    (&str); /* does nothing, but what the heck... */
  }
  {
    fprintf(stderr, "* Testing Base64 encoding / decoding.\n");
    FIO_NAME(FIO_STR_NAME, destroy)(&str); /* does nothing, but why not... */

    FIO_NAME(FIO_STR_NAME, s) b64message = FIO_STR_INIT;
    fio_str_info_s b64i = FIO_NAME(
        FIO_STR_NAME,
        write)(&b64message, "Hello World, this is the voice of peace:)", 41);
    for (int i = 0; i < 256; ++i) {
      uint8_t c = i;
      b64i = FIO_NAME(FIO_STR_NAME, write)(&b64message, &c, 1);
      FIO_ASSERT(FIO_NAME(FIO_STR_NAME, len)(&b64message) == (size_t)(42 + i),
                 "Base64 message length error (%zu != %zu)",
                 FIO_NAME(FIO_STR_NAME, len)(&b64message),
                 (size_t)(42 + i));
      FIO_ASSERT(FIO_NAME2(FIO_STR_NAME, ptr)(&b64message)[41 + i] == (char)c,
                 "Base64 message data error");
    }
    fio_str_info_s encoded =
        FIO_NAME(FIO_STR_NAME, write_base64enc)(&str, b64i.buf, b64i.len, 1);
    /* prevent encoded data from being deallocated during unencoding */
    encoded = FIO_NAME(FIO_STR_NAME, FIO_STR_RESERVE_NAME)(
        &str,
        encoded.len + ((encoded.len >> 2) * 3) + 8);
    fio_str_info_s decoded;
    {
      FIO_NAME(FIO_STR_NAME, s) tmps;
      FIO_NAME(FIO_STR_NAME, init_copy2)(&tmps, &str);
      decoded = FIO_NAME(FIO_STR_NAME,
                         write_base64dec)(&str,
                                          FIO_NAME2(FIO_STR_NAME, ptr)(&tmps),
                                          FIO_NAME(FIO_STR_NAME, len)(&tmps));
      FIO_NAME(FIO_STR_NAME, destroy)(&tmps);
      encoded.buf = decoded.buf;
    }
    FIO_ASSERT(encoded.len, "Base64 encoding failed");
    FIO_ASSERT(decoded.len > encoded.len,
               "Base64 decoding failed:\n%s",
               encoded.buf);
    FIO_ASSERT(b64i.len == decoded.len - encoded.len,
               "Base 64 roundtrip length error, %zu != %zu (%zu - %zu):\n %s",
               b64i.len,
               decoded.len - encoded.len,
               decoded.len,
               encoded.len,
               decoded.buf);

    FIO_ASSERT(!memcmp(b64i.buf, decoded.buf + encoded.len, b64i.len),
               "Base 64 roundtrip failed:\n %s",
               decoded.buf);
    FIO_NAME(FIO_STR_NAME, destroy)(&b64message);
    FIO_NAME(FIO_STR_NAME, destroy)(&str);
  }
  {
    fprintf(stderr, "* Testing JSON style character escaping / unescaping.\n");
    FIO_NAME(FIO_STR_NAME, s) unescaped = FIO_STR_INIT;
    fio_str_info_s ue;
    const char *utf8_sample = /* three hearts, small-big-small*/
        "\xf0\x9f\x92\x95\xe2\x9d\xa4\xef\xb8\x8f\xf0\x9f\x92\x95";
    FIO_NAME(FIO_STR_NAME, write)(&unescaped, utf8_sample, strlen(utf8_sample));
    for (int i = 0; i < 256; ++i) {
      uint8_t c = i;
      ue = FIO_NAME(FIO_STR_NAME, write)(&unescaped, &c, 1);
    }
    fio_str_info_s encoded =
        FIO_NAME(FIO_STR_NAME, write_escape)(&str, ue.buf, ue.len);
    // fprintf(stderr, "* %s\n", encoded.buf);
    fio_str_info_s decoded;
    {
      FIO_NAME(FIO_STR_NAME, s) tmps;
      FIO_NAME(FIO_STR_NAME, init_copy2)(&tmps, &str);
      decoded = FIO_NAME(FIO_STR_NAME,
                         write_unescape)(&str,
                                         FIO_NAME2(FIO_STR_NAME, ptr)(&tmps),
                                         FIO_NAME(FIO_STR_NAME, len)(&tmps));
      FIO_NAME(FIO_STR_NAME, destroy)(&tmps);
      encoded.buf = decoded.buf;
    }
    FIO_ASSERT(!memcmp(encoded.buf, utf8_sample, strlen(utf8_sample)),
               "valid UTF-8 data shouldn't be escaped:\n%.*s\n%s",
               (int)encoded.len,
               encoded.buf,
               decoded.buf);
    FIO_ASSERT(encoded.len, "JSON encoding failed");
    FIO_ASSERT(decoded.len > encoded.len,
               "JSON decoding failed:\n%s",
               encoded.buf);
    FIO_ASSERT(ue.len == decoded.len - encoded.len,
               "JSON roundtrip length error, %zu != %zu (%zu - %zu):\n %s",
               ue.len,
               decoded.len - encoded.len,
               decoded.len,
               encoded.len,
               decoded.buf);

    FIO_ASSERT(!memcmp(ue.buf, decoded.buf + encoded.len, ue.len),
               "JSON roundtrip failed:\n %s",
               decoded.buf);
    FIO_NAME(FIO_STR_NAME, destroy)(&unescaped);
    FIO_NAME(FIO_STR_NAME, destroy)(&str);
  }
}
#undef FIO__STR_SMALL_CAPA
#undef FIO_STR_WRITE_TEST_FUNC
#endif /* FIO_STR_WRITE_TEST_FUNC */

/* *****************************************************************************
String Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */

#undef FIO_STR_SMALL
#undef FIO_STR_SMALL_CAPA
#undef FIO_STR_SMALL_DATA
#undef FIO_STR_SMALL_LEN
#undef FIO_STR_SMALL_LEN_SET

#undef FIO_STR_BIG_CAPA
#undef FIO_STR_BIG_CAPA_SET
#undef FIO_STR_BIG_DATA
#undef FIO_STR_BIG_FREE_BUF
#undef FIO_STR_BIG_IS_DYNAMIC
#undef FIO_STR_BIG_LEN
#undef FIO_STR_BIG_LEN_SET
#undef FIO_STR_BIG_SET_STATIC

#undef FIO_STR_CAPA2WORDS
#undef FIO_STR_FREEZE_

#undef FIO_STR_IS_FROZEN
#undef FIO_STR_IS_SMALL
#undef FIO_STR_NAME

#undef FIO_STR_OPTIMIZE4IMMUTABILITY
#undef FIO_STR_OPTIMIZE_EMBEDDED
#undef FIO_STR_PTR
#undef FIO_STR_THAW_
#undef FIO_STR_WRITE_ESCAPED_CT_OR
#undef FIO_STR_RESERVE_NAME

#endif /* FIO_STR_NAME */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_REF_NAME long_ref       /* Development inclusion - ignore line */
#define FIO_REF_TYPE long           /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                      Reference Counting / Wrapper
                   (must be placed after all type macros)



***************************************************************************** */

#ifdef FIO_REF_NAME

#ifndef fio_atomic_add
#error FIO_REF_NAME requires enabling the FIO_ATOMIC extension.
#endif

#ifndef FIO_REF_TYPE
#define FIO_REF_TYPE FIO_NAME(FIO_REF_NAME, s)
#endif

#ifndef FIO_REF_INIT
#define FIO_REF_INIT(obj)                                                      \
  do {                                                                         \
    (obj) = (FIO_REF_TYPE){0};                                                 \
  } while (0)
#endif

#ifndef FIO_REF_DESTROY
#define FIO_REF_DESTROY(obj)
#endif

#ifndef FIO_REF_METADATA_INIT
#ifdef FIO_REF_METADATA
#define FIO_REF_METADATA_INIT(meta)                                            \
  do {                                                                         \
    (meta) = (FIO_REF_METADATA){0};                                            \
  } while (0)
#else
#define FIO_REF_METADATA_INIT(meta)
#endif
#endif

#ifndef FIO_REF_METADATA_DESTROY
#define FIO_REF_METADATA_DESTROY(meta)
#endif

/**
 * FIO_REF_CONSTRUCTOR_ONLY allows the reference counter constructor (TYPE_new)
 * to be the only constructor function.
 *
 * When set, the reference counting functions will use `X_new` and `X_free`.
 * Otherwise (assuming `X_new` and `X_free` are already defined), the reference
 * counter will define `X_new2` and `X_free2` instead.
 */
#ifdef FIO_REF_CONSTRUCTOR_ONLY
#define FIO_REF_CONSTRUCTOR new
#define FIO_REF_DESTRUCTOR  free
#define FIO_REF_DUPNAME     dup
#else
#define FIO_REF_CONSTRUCTOR new2
#define FIO_REF_DESTRUCTOR  free2
#define FIO_REF_DUPNAME     dup2
#endif

typedef struct {
  volatile size_t ref;
#ifdef FIO_REF_METADATA
  FIO_REF_METADATA metadata;
#endif
} FIO_NAME(FIO_REF_NAME, _wrapper_s);

#ifdef FIO_PTR_TAG_TYPE
#define FIO_REF_TYPE_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_REF_TYPE_PTR FIO_REF_TYPE *
#endif

/* *****************************************************************************
Reference Counter (Wrapper) API
***************************************************************************** */

/** Allocates a reference counted object. */
#ifdef FIO_REF_FLEX_TYPE
IFUNC FIO_REF_TYPE_PTR FIO_NAME(FIO_REF_NAME,
                                FIO_REF_CONSTRUCTOR)(size_t members);
#else
IFUNC FIO_REF_TYPE_PTR FIO_NAME(FIO_REF_NAME, FIO_REF_CONSTRUCTOR)(void);
#endif /* FIO_REF_FLEX_TYPE */

/** Increases the reference count. */
FIO_IFUNC FIO_REF_TYPE_PTR FIO_NAME(FIO_REF_NAME,
                                    FIO_REF_DUPNAME)(FIO_REF_TYPE_PTR wrapped);

/** Frees a reference counted object (or decreases the reference count). */
IFUNC void FIO_NAME(FIO_REF_NAME, FIO_REF_DESTRUCTOR)(FIO_REF_TYPE_PTR wrapped);

#ifdef FIO_REF_METADATA
/** Returns a pointer to the object's metadata, if defined. */
IFUNC FIO_REF_METADATA *FIO_NAME(FIO_REF_NAME,
                                 metadata)(FIO_REF_TYPE_PTR wrapped);
#endif

/* *****************************************************************************
Inline Implementation
***************************************************************************** */
/** Increases the reference count. */
FIO_IFUNC FIO_REF_TYPE_PTR
FIO_NAME(FIO_REF_NAME, FIO_REF_DUPNAME)(FIO_REF_TYPE_PTR wrapped_) {
  FIO_REF_TYPE *wrapped = (FIO_REF_TYPE *)(FIO_PTR_UNTAG(wrapped_));
  FIO_NAME(FIO_REF_NAME, _wrapper_s) *o =
      ((FIO_NAME(FIO_REF_NAME, _wrapper_s) *)wrapped) - 1;
  if (!o)
    return wrapped_;
  fio_atomic_add(&o->ref, 1);
  return wrapped_;
}

/* *****************************************************************************
Reference Counter (Wrapper) Implementation
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

#if defined(DEBUG) || defined(FIO_LEAK_COUNTER)
static size_t FIO_NAME(FIO_REF_NAME, ___leak_tester);
#define FIO_REF_ON_ALLOC()                                                     \
  fio_atomic_add(&FIO_NAME(FIO_REF_NAME, ___leak_tester), 1)
#define FIO_REF_ON_FREE()                                                      \
  fio_atomic_sub(&FIO_NAME(FIO_REF_NAME, ___leak_tester), 1)
static void __attribute__((destructor))
FIO_NAME(FIO_REF_NAME, ___leak_test)(void) {
  if (FIO_NAME(FIO_REF_NAME, ___leak_tester)) {
    FIO_LOG_ERROR(
        "(" FIO_MACRO2STR(FIO_REF_NAME) "):\n          "
                                        "%zd memory leak(s) detected for "
                                        "type: " FIO_MACRO2STR(FIO_REF_TYPE),
        FIO_NAME(FIO_REF_NAME, ___leak_tester));
  }
}
#else
#define FIO_REF_ON_ALLOC()
#define FIO_REF_ON_FREE()
#endif /* defined(DEBUG) || defined(FIO_LEAK_COUNTER) */

/** Allocates a reference counted object. */
#ifdef FIO_REF_FLEX_TYPE
IFUNC FIO_REF_TYPE_PTR FIO_NAME(FIO_REF_NAME,
                                FIO_REF_CONSTRUCTOR)(size_t members) {
  FIO_NAME(FIO_REF_NAME, _wrapper_s) *o =
      (FIO_NAME(FIO_REF_NAME, _wrapper_s) *)FIO_MEM_REALLOC_(
          NULL,
          0,
          sizeof(*o) + sizeof(FIO_REF_TYPE) +
              (sizeof(FIO_REF_FLEX_TYPE) * members),
          0);
#else
IFUNC FIO_REF_TYPE_PTR FIO_NAME(FIO_REF_NAME, FIO_REF_CONSTRUCTOR)(void) {
  FIO_NAME(FIO_REF_NAME, _wrapper_s) *o = (FIO_NAME(FIO_REF_NAME, _wrapper_s) *)
      FIO_MEM_REALLOC_(NULL, 0, sizeof(*o) + sizeof(FIO_REF_TYPE), 0);
#endif /* FIO_REF_FLEX_TYPE */
  if (!o)
    return (FIO_REF_TYPE_PTR)(o);
  FIO_REF_ON_ALLOC();
  o->ref = 1;
  FIO_REF_METADATA_INIT((o->metadata));
  FIO_REF_TYPE *ret = (FIO_REF_TYPE *)(o + 1);
  FIO_REF_INIT((ret[0]));
  return (FIO_REF_TYPE_PTR)(FIO_PTR_TAG(ret));
}

/** Frees a reference counted object (or decreases the reference count). */
IFUNC void FIO_NAME(FIO_REF_NAME,
                    FIO_REF_DESTRUCTOR)(FIO_REF_TYPE_PTR wrapped_) {
  FIO_REF_TYPE *wrapped = (FIO_REF_TYPE *)(FIO_PTR_UNTAG(wrapped_));
  if (!wrapped || !wrapped_)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(wrapped_);
  FIO_NAME(FIO_REF_NAME, _wrapper_s) *o =
      ((FIO_NAME(FIO_REF_NAME, _wrapper_s) *)wrapped) - 1;
  if (!o)
    return;
  if (fio_atomic_sub_fetch(&o->ref, 1))
    return;
  FIO_REF_DESTROY((wrapped[0]));
  FIO_REF_METADATA_DESTROY((o->metadata));
  FIO_MEM_FREE_(o, sizeof(*o) + sizeof(FIO_REF_TYPE));
  FIO_REF_ON_FREE();
}

#ifdef FIO_REF_METADATA
/** Returns a pointer to the object's metadata, if defined. */
IFUNC FIO_REF_METADATA *FIO_NAME(FIO_REF_NAME,
                                 metadata)(FIO_REF_TYPE_PTR wrapped_) {
  FIO_REF_TYPE *wrapped = (FIO_REF_TYPE *)(FIO_PTR_UNTAG(wrapped_));
  FIO_NAME(FIO_REF_NAME, _wrapper_s) *o =
      ((FIO_NAME(FIO_REF_NAME, _wrapper_s) *)wrapped) - 1;
  return &o->metadata;
}
#endif

/* *****************************************************************************
Reference Counter (Wrapper) Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_REF_NAME
#undef FIO_REF_FLEX_TYPE
#undef FIO_REF_TYPE
#undef FIO_REF_INIT
#undef FIO_REF_DESTROY
#undef FIO_REF_METADATA
#undef FIO_REF_METADATA_INIT
#undef FIO_REF_METADATA_DESTROY
#undef FIO_REF_TYPE_PTR
#undef FIO_REF_CONSTRUCTOR_ONLY
#undef FIO_REF_CONSTRUCTOR
#undef FIO_REF_DUPNAME
#undef FIO_REF_DESTRUCTOR
#endif
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#define FIO_MODULE_NAME module      /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "100 mem.h"                /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************




                  A Template for New Types / Modules




***************************************************************************** */
#ifdef FIO_MODULE_NAME

/* *****************************************************************************
Module Settings

At this point, define any MACROs and customaizable settings avsailable to the
developer.
***************************************************************************** */

/* *****************************************************************************
Pointer Tagging Support
***************************************************************************** */

#ifdef FIO_PTR_TAG_TYPE
#define FIO_MODULE_PTR FIO_PTR_TAG_TYPE
#else
#define FIO_MODULE_PTR FIO_NAME(FIO_MODULE_NAME, s) *
#endif

/* *****************************************************************************
Module API
***************************************************************************** */

typedef struct {
  /* module's type(s) if any */
  void *data;
} FIO_NAME(FIO_MODULE_NAME, s);

/* at this point publish (declare only) the public API */

#ifndef FIO_MODULE_INIT
/* Initialization macro. */
#define FIO_MODULE_INIT                                                        \
  { 0 }
#endif

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY

/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC FIO_MODULE_PTR FIO_NAME(FIO_MODULE_NAME, new)(void);

/* Frees any internal data AND the object's container! */
FIO_IFUNC int FIO_NAME(FIO_MODULE_NAME, free)(FIO_MODULE_PTR obj);

#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/** Destroys the object, reinitializing its container. */
SFUNC void FIO_NAME(FIO_MODULE_NAME, destroy)(FIO_MODULE_PTR obj);

/* *****************************************************************************
Module Implementation - inlined static functions
***************************************************************************** */
/*
REMEMBER:
========

All memory allocations should use:
* FIO_MEM_REALLOC_(ptr, old_size, new_size, copy_len)
* FIO_MEM_FREE_(ptr, size) fio_free((ptr))

*/

/* do we have a constructor? */
#ifndef FIO_REF_CONSTRUCTOR_ONLY
/* Allocates a new object on the heap and initializes it's memory. */
FIO_IFUNC FIO_MODULE_PTR FIO_NAME(FIO_MODULE_NAME, new)(void) {
  FIO_NAME(FIO_MODULE_NAME, s) *o =
      (FIO_NAME(FIO_MODULE_NAME, s) *)FIO_MEM_REALLOC_(NULL, 0, sizeof(*o), 0);
  if (!o)
    return (FIO_MODULE_PTR)NULL;
  *o = (FIO_NAME(FIO_MODULE_NAME, s))FIO_MODULE_INIT;
  return (FIO_MODULE_PTR)FIO_PTR_TAG(o);
}
/* Frees any internal data AND the object's container! */
FIO_IFUNC int FIO_NAME(FIO_MODULE_NAME, free)(FIO_MODULE_PTR obj) {
  FIO_PTR_TAG_VALID_OR_RETURN(obj, 0);
  FIO_NAME(FIO_MODULE_NAME, destroy)(obj);
  FIO_NAME(FIO_MODULE_NAME, s) *o =
      (FIO_NAME(FIO_MODULE_NAME, s) *)FIO_PTR_UNTAG(obj);
  FIO_MEM_FREE_(o, sizeof(*o));
  return 0;
}
#endif /* FIO_REF_CONSTRUCTOR_ONLY */

/* *****************************************************************************
Module Implementation - possibly externed functions.
***************************************************************************** */
#ifdef FIO_EXTERN_COMPLETE

/*
REMEMBER:
========

All memory allocations should use:
* FIO_MEM_REALLOC_(ptr, old_size, new_size, copy_len)
* FIO_MEM_FREE_(ptr, size) fio_free((ptr))

*/

/* Frees any internal data AND the object's container! */
SFUNC void FIO_NAME(FIO_MODULE_NAME, destroy)(FIO_MODULE_PTR obj) {
  FIO_NAME(FIO_MODULE_NAME, s) *o =
      (FIO_NAME(FIO_MODULE_NAME, s) *)FIO_PTR_UNTAG(obj);
  if (!o)
    return;
  FIO_PTR_TAG_VALID_OR_RETURN_VOID(obj);
  /* add destruction logic */

  *o = (FIO_NAME(FIO_MODULE_NAME, s))FIO_MODULE_INIT;
  return;
}

/* *****************************************************************************
Module Testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC void FIO_NAME_TEST(stl, FIO_MODULE_NAME)(void) {
  /*
   * test module here
   */
}

#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
Module Cleanup
***************************************************************************** */

#endif /* FIO_EXTERN_COMPLETE */
#undef FIO_MODULE_PTR
#endif /* FIO_MODULE_NAME */
#undef FIO_MODULE_NAME
/* *****************************************************************************




                            Common Cleanup




***************************************************************************** */

/* *****************************************************************************
Common cleanup
***************************************************************************** */
#ifndef FIO_STL_KEEP__

#undef FIO_EXTERN
#undef SFUNC
#undef IFUNC
#undef SFUNC_
#undef IFUNC_
#undef FIO_PTR_TAG
#undef FIO_PTR_UNTAG
#undef FIO_PTR_TAG_TYPE
#undef FIO_PTR_TAG_VALIDATE
#undef FIO_PTR_TAG_VALID_OR_RETURN
#undef FIO_PTR_TAG_VALID_OR_RETURN_VOID
#undef FIO_PTR_TAG_VALID_OR_GOTO

#undef FIO_MALLOC_TMP_USE_SYSTEM
#undef FIO_MEM_REALLOC_
#undef FIO_MEM_FREE_
#undef FIO_MEM_REALLOC_IS_SAFE_
#undef FIO_MEMORY_NAME /* postponed due to possible use in macros */

#undef FIO___LOCK_TYPE
#undef FIO___LOCK_INIT
#undef FIO___LOCK_LOCK
#undef FIO___LOCK_LOCK_TRY
#undef FIO___LOCK_UNLOCK
#undef FIO_USE_THREAD_MUTEX_TMP

/* undefine FIO_EXTERN_COMPLETE only if it was defined locally */
#if defined(FIO_EXTERN_COMPLETE) && FIO_EXTERN_COMPLETE &&                     \
    FIO_EXTERN_COMPLETE == 2
#undef FIO_EXTERN_COMPLETE
#endif

#else

#undef SFUNC
#undef IFUNC
#define SFUNC SFUNC_
#define IFUNC IFUNC_

#endif /* !FIO_STL_KEEP__ */
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#include "003 atomics.h"            /* Development inclusion - ignore line */
#include "004 bitwise.h"            /* Development inclusion - ignore line */
#include "005 riskyhash.h"          /* Development inclusion - ignore line */
#include "006 atol.h"               /* Development inclusion - ignore line */
#include "051 json.h"               /* Development inclusion - ignore line */
#include "201 array.h"              /* Development inclusion - ignore line */
#include "210 hashmap.h"            /* Development inclusion - ignore line */
#include "220 string.h"             /* Development inclusion - ignore line */
#include "299 reference counter.h"  /* Development inclusion - ignore line */
#include "700 cleanup.h"            /* Development inclusion - ignore line */
#define FIO_FIOBJ                   /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************








                          FIOBJ - soft (dynamic) types



FIOBJ - dynamic types

These are dynamic types that use pointer tagging for fast type identification.

Pointer tagging on 64 bit systems allows for 3 bits at the lower bits. On most
32 bit systems this is also true due to allocator alignment. When in doubt, use
the provided custom allocator.

To keep the 64bit memory address alignment on 32bit systems, a 32bit metadata
integer is added when a virtual function table is missing. This doesn't effect
memory consumption on 64 bit systems and uses 4 bytes on 32 bit systems.

Note: this code is placed at the end of the STL file, since it leverages most of
the SLT features and could be affected by their inclusion.
***************************************************************************** */
#if defined(FIO_FIOBJ) && !defined(H___FIOBJ___H)
#define H___FIOBJ___H

/* *****************************************************************************
FIOBJ compilation settings (type names and JSON nesting limits).

Type Naming Macros for FIOBJ types. By default, results in:
- fiobj_true()
- fiobj_false()
- fiobj_null()
- fiobj_num_new() ... (etc')
- fiobj_float_new() ... (etc')
- fiobj_str_new() ... (etc')
- fiobj_array_new() ... (etc')
- fiobj_hash_new() ... (etc')
***************************************************************************** */

#define FIOBJ___NAME_TRUE   true
#define FIOBJ___NAME_FALSE  false
#define FIOBJ___NAME_NULL   null
#define FIOBJ___NAME_NUMBER num
#define FIOBJ___NAME_FLOAT  float
#define FIOBJ___NAME_STRING str
#define FIOBJ___NAME_ARRAY  array
#define FIOBJ___NAME_HASH   hash

#ifndef FIOBJ_MAX_NESTING
/**
 * Sets the limit on nesting level transversal by recursive functions.
 *
 * This effects JSON output / input and the `fiobj_each2` function since they
 * are recursive.
 *
 * HOWEVER: this value will NOT effect the recursive `fiobj_free` which could
 * (potentially) expload the stack if given melformed input such as cyclic data
 * structures.
 *
 * Values should be less than 32K.
 */
#define FIOBJ_MAX_NESTING 512
#endif

/* make sure roundtrips work */
#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH FIOBJ_MAX_NESTING
#endif

#ifndef FIOBJ_JSON_APPEND
#define FIOBJ_JSON_APPEND 1
#endif
/* *****************************************************************************
General Requirements / Macros
***************************************************************************** */

#define FIO_ATOL   1
#define FIO_ATOMIC 1
#include __FILE__

#ifdef FIOBJ_EXTERN
#define FIOBJ_FUNC
#define FIOBJ_IFUNC
#define FIOBJ_EXTERN_OBJ     extern
#define FIOBJ_EXTERN_OBJ_IMP __attribute__((weak))

#else /* FIO_EXTERN */
#define FIOBJ_FUNC           static __attribute__((unused))
#define FIOBJ_IFUNC          static inline __attribute__((unused))
#define FIOBJ_EXTERN_OBJ     static __attribute__((unused))
#define FIOBJ_EXTERN_OBJ_IMP static __attribute__((unused))
#ifndef FIOBJ_EXTERN_COMPLETE /* force implementation, emitting static data */
#define FIOBJ_EXTERN_COMPLETE 2
#endif /* FIOBJ_EXTERN_COMPLETE */

#endif /* FIO_EXTERN */

#ifdef FIO_LOG_PRINT__
#define FIOBJ_LOG_PRINT__(...) FIO_LOG_PRINT__(__VA_ARGS__)
#else
#define FIOBJ_LOG_PRINT__(...)
#endif

#ifdef __cplusplus /* C++ doesn't allow declarations for static variables */
#undef FIOBJ_EXTERN_OBJ
#undef FIOBJ_EXTERN_OBJ_IMP
#define FIOBJ_EXTERN_OBJ     extern "C"
#define FIOBJ_EXTERN_OBJ_IMP extern "C" __attribute__((weak))
#endif

/* *****************************************************************************
Dedicated memory allocator for FIOBJ types? (recommended for locality)
***************************************************************************** */
#ifdef FIOBJ_MALLOC
#define FIO_MEMORY_NAME fiobj_mem
#ifndef FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG
/* 4Mb per system call */
#define FIO_MEMORY_SYS_ALLOCATION_SIZE_LOG 22
#endif
#ifndef FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG
/* fight fragmentation */
#define FIO_MEMORY_BLOCKS_PER_ALLOCATION_LOG 4
#endif
#ifndef FIO_MEMORY_ALIGN_LOG
/* align on 8 bytes, it's enough */
#define FIO_MEMORY_ALIGN_LOG 3
#endif
#ifndef FIO_MEMORY_CACHE_SLOTS
/* cache up to 64Mb */
#define FIO_MEMORY_CACHE_SLOTS 16
#endif
#ifndef FIO_MEMORY_ENABLE_BIG_ALLOC
/* for big arrays / maps */
#define FIO_MEMORY_ENABLE_BIG_ALLOC 1
#endif
#ifndef FIO_MEMORY_ARENA_COUNT
/* CPU core arena count */
#define FIO_MEMORY_ARENA_COUNT -1
#endif
#if FIO_OS_POSIX && !defined(FIO_MEMORY_USE_THREAD_MUTEX)
/* yes, well... POSIX Mutexes are decent on the machines I tested. */
#define FIO_MEMORY_USE_THREAD_MUTEX 1
#endif
/* make sure functions are exported if requested */
#ifdef FIOBJ_EXTERN
#define FIO_EXTERN
#if defined(FIOBJ_EXTERN_COMPLETE) && !defined(FIO_EXTERN_COMPLETE)
#define FIO_EXTERN_COMPLETE 2
#endif
#endif
#include __FILE__

#define FIOBJ_MEM_REALLOC(ptr, old_size, new_size, copy_len)                   \
  FIO_NAME(fiobj_mem, realloc2)((ptr), (new_size), (copy_len))
#define FIOBJ_MEM_FREE(ptr, size) FIO_NAME(fiobj_mem, free)((ptr))
#define FIOBJ_MEM_REALLOC_IS_SAFE 0

#else

FIO_IFUNC void *FIO_NAME(fiobj_mem, realloc2)(void *ptr,
                                              size_t new_size,
                                              size_t copy_len) {
  return FIO_MEM_REALLOC(ptr, new_size, new_size, copy_len);
  (void)copy_len; /* might be unused */
}
FIO_IFUNC void FIO_NAME(fiobj_mem, free)(void *ptr) { FIO_MEM_FREE(ptr, -1); }

#define FIOBJ_MEM_REALLOC         FIO_MEM_REALLOC
#define FIOBJ_MEM_FREE            FIO_MEM_FREE
#define FIOBJ_MEM_REALLOC_IS_SAFE FIO_MEM_REALLOC_IS_SAFE

#endif /* FIOBJ_MALLOC */
/* *****************************************************************************
Debugging / Leak Detection
***************************************************************************** */
#if defined(TEST) || defined(DEBUG) || defined(FIO_LEAK_COUNTER)
#define FIOBJ_MARK_MEMORY 1
#endif

#if FIOBJ_MARK_MEMORY
size_t __attribute__((weak)) FIOBJ_MARK_MEMORY_ALLOC_COUNTER;
size_t __attribute__((weak)) FIOBJ_MARK_MEMORY_FREE_COUNTER;
#define FIOBJ_MARK_MEMORY_ALLOC()                                              \
  fio_atomic_add(&FIOBJ_MARK_MEMORY_ALLOC_COUNTER, 1)
#define FIOBJ_MARK_MEMORY_FREE()                                               \
  fio_atomic_add(&FIOBJ_MARK_MEMORY_FREE_COUNTER, 1)
#define FIOBJ_MARK_MEMORY_PRINT()                                              \
  FIOBJ_LOG_PRINT__(                                                           \
      ((FIOBJ_MARK_MEMORY_ALLOC_COUNTER == FIOBJ_MARK_MEMORY_FREE_COUNTER)     \
           ? 4 /* FIO_LOG_LEVEL_INFO */                                        \
           : 3 /* FIO_LOG_LEVEL_WARNING */),                                   \
      ((FIOBJ_MARK_MEMORY_ALLOC_COUNTER == FIOBJ_MARK_MEMORY_FREE_COUNTER)     \
           ? "INFO: total FIOBJ allocations: %zu (%zu/%zu)"                    \
           : "WARNING: LEAKED! FIOBJ allocations: %zu (%zu/%zu)"),             \
      FIOBJ_MARK_MEMORY_ALLOC_COUNTER - FIOBJ_MARK_MEMORY_FREE_COUNTER,        \
      FIOBJ_MARK_MEMORY_FREE_COUNTER,                                          \
      FIOBJ_MARK_MEMORY_ALLOC_COUNTER)
#define FIOBJ_MARK_MEMORY_ENABLED 1

#else

#define FIOBJ_MARK_MEMORY_ALLOC_COUNTER 0 /* when testing unmarked FIOBJ */
#define FIOBJ_MARK_MEMORY_FREE_COUNTER  0 /* when testing unmarked FIOBJ */
#define FIOBJ_MARK_MEMORY_ALLOC()
#define FIOBJ_MARK_MEMORY_FREE()
#define FIOBJ_MARK_MEMORY_PRINT()
#define FIOBJ_MARK_MEMORY_ENABLED 0
#endif

/* *****************************************************************************
The FIOBJ Type
***************************************************************************** */

/** Use the FIOBJ type for dynamic types. */
typedef struct FIOBJ_s {
  struct FIOBJ_s *compiler_validation_type;
} * FIOBJ;

/** FIOBJ type enum for common / primitive types. */
typedef enum {
  FIOBJ_T_NUMBER = 0x01, /* 0b001 3 bits taken for small numbers */
  FIOBJ_T_PRIMITIVE = 2, /* 0b010 a lonely second bit signifies a primitive */
  FIOBJ_T_STRING = 3,    /* 0b011 */
  FIOBJ_T_ARRAY = 4,     /* 0b100 */
  FIOBJ_T_HASH = 5,      /* 0b101 */
  FIOBJ_T_FLOAT = 6,     /* 0b110 */
  FIOBJ_T_OTHER = 7,     /* 0b111 dynamic type - test content */
} fiobj_class_en;

#define FIOBJ_T_NULL  2  /* 0b010 a lonely second bit signifies a primitive */
#define FIOBJ_T_TRUE  18 /* 0b010 010 - primitive value */
#define FIOBJ_T_FALSE 34 /* 0b100 010 - primitive value */

/** Use the macros to avoid future API changes. */
#define FIOBJ_TYPE(o) fiobj_type(o)
/** Use the macros to avoid future API changes. */
#define FIOBJ_TYPE_IS(o, type) (fiobj_type(o) == type)
/** Identifies an invalid type identifier (returned from FIOBJ_TYPE(o) */
#define FIOBJ_T_INVALID 0
/** Identifies an invalid object */
#define FIOBJ_INVALID 0
/** Tests if the object is (probably) a valid FIOBJ */
#define FIOBJ_IS_INVALID(o)       (((uintptr_t)(o)&7UL) == 0)
#define FIOBJ_TYPE_CLASS(o)       ((fiobj_class_en)(((uintptr_t)(o)) & 7UL))
#define FIOBJ_PTR_TAG(o, klass)   ((uintptr_t)((uintptr_t)(o) | (klass)))
#define FIOBJ_PTR_UNTAG(o)        ((uintptr_t)((uintptr_t)(o) & (~7ULL)))
#define FIOBJ_PTR_TAG_VALIDATE(o) ((uintptr_t)((uintptr_t)(o) & (7ULL)))
/** Returns an objects type. This isn't limited to known types. */
FIO_IFUNC size_t fiobj_type(FIOBJ o);

/* *****************************************************************************
FIOBJ Memory Management
***************************************************************************** */

/** Increases an object's reference count (or copies) and returns it. */
FIO_IFUNC FIOBJ fiobj_dup(FIOBJ o);

/** Decreases an object's reference count or frees it. */
FIO_IFUNC void fiobj_free(FIOBJ o);

/* *****************************************************************************
FIOBJ Data / Info
***************************************************************************** */

/** Compares two objects. */
FIO_IFUNC unsigned char FIO_NAME_BL(fiobj, eq)(FIOBJ a, FIOBJ b);

/** Returns a temporary String representation for any FIOBJ object. */
FIO_IFUNC fio_str_info_s FIO_NAME2(fiobj, cstr)(FIOBJ o);

/** Returns an integer representation for any FIOBJ object. */
FIO_IFUNC intptr_t FIO_NAME2(fiobj, i)(FIOBJ o);

/** Returns a float (double) representation for any FIOBJ object. */
FIO_IFUNC double FIO_NAME2(fiobj, f)(FIOBJ o);

/* *****************************************************************************
FIOBJ Containers (iteration)
***************************************************************************** */

/** Iteration information structure passed to the callback. */
typedef struct fiobj_each_s {
  /** The being iterated. Once set, cannot be safely changed. */
  FIOBJ const parent;
  /** The index to start at / the current object's index */
  uint64_t index;
  /** Always 1, but may be used to allow type detection. */
  const int64_t items_at_index;
  /** The callback / task called for each index, may be updated mid-cycle. */
  int (*task)(struct fiobj_each_s *info);
  /** The argument passed along to the task. */
  void *udata;
  /**
   * The objects at the current index.
   *
   * For Hash Maps, `obj[0]` is the value and `obj[1]` is the key.
   * */
  FIOBJ obj[];
} fiobj_each_s;

/**
 * Performs a task for each element held by the FIOBJ object.
 *
 * If `task` returns -1, the `each` loop will break (stop).
 *
 * Returns the "stop" position - the number of elements processed + `start_at`.
 */
FIO_SFUNC uint32_t fiobj_each1(FIOBJ o,
                               int (*task)(fiobj_each_s *info),
                               void *udata,
                               int32_t start_at);

/**
 * Performs a task for the object itself and each element held by the FIOBJ
 * object or any of it's elements (a deep task).
 *
 * The order of performance is by order of appearance, as if all nesting levels
 * were flattened.
 *
 * If `task` returns -1, the `each` loop will break (stop).
 *
 * Returns the number of elements processed.
 */
FIOBJ_FUNC uint32_t fiobj_each2(FIOBJ o,
                                int (*task)(fiobj_each_s *info),
                                void *udata);

/* *****************************************************************************
FIOBJ Primitives (NULL, True, False)
***************************************************************************** */

/** Returns the `true` primitive. */
FIO_IFUNC FIOBJ FIO_NAME(fiobj, FIOBJ___NAME_TRUE)(void) {
  return (FIOBJ)(FIOBJ_T_TRUE);
}

/** Returns the `false` primitive. */
FIO_IFUNC FIOBJ FIO_NAME(fiobj, FIOBJ___NAME_FALSE)(void) {
  return (FIOBJ)(FIOBJ_T_FALSE);
}

/** Returns the `nil` / `null` primitive. */
FIO_IFUNC FIOBJ FIO_NAME(fiobj, FIOBJ___NAME_NULL)(void) {
  return (FIOBJ)(FIOBJ_T_NULL);
}

/* *****************************************************************************
FIOBJ Type - Extensibility (FIOBJ_T_OTHER)
***************************************************************************** */

/** FIOBJ types can be extended using virtual function tables. */
typedef struct {
  /**
   * MUST return a unique number to identify object type.
   *
   * Numbers (type IDs) under 100 are reserved. Numbers under 40 are illegal.
   */
  size_t type_id;
  /** Test for equality between two objects with the same `type_id` */
  unsigned char (*is_eq)(FIOBJ restrict a, FIOBJ restrict b);
  /** Converts an object to a String */
  fio_str_info_s (*to_s)(FIOBJ o);
  /** Converts an object to an integer */
  intptr_t (*to_i)(FIOBJ o);
  /** Converts an object to a double */
  double (*to_f)(FIOBJ o);
  /** Returns the number of exposed elements held by the object, if any. */
  uint32_t (*count)(FIOBJ o);
  /** Iterates the exposed elements held by the object. See `fiobj_each1`. */
  uint32_t (*each1)(FIOBJ o,
                    int (*task)(fiobj_each_s *e),
                    void *udata,
                    int32_t start_at);
  /**
   * Decreases the reference count and/or frees the object, calling `free2` for
   * any nested objects.
   */
  void (*free2)(FIOBJ o);
} FIOBJ_class_vtable_s;

FIOBJ_EXTERN_OBJ const FIOBJ_class_vtable_s FIOBJ___OBJECT_CLASS_VTBL;

#define FIO_REF_CONSTRUCTOR_ONLY 1
#define FIO_REF_NAME             fiobj_object
#define FIO_REF_TYPE             void *
#define FIO_REF_METADATA         const FIOBJ_class_vtable_s *
#define FIO_REF_METADATA_INIT(m)                                               \
  do {                                                                         \
    m = &FIOBJ___OBJECT_CLASS_VTBL;                                            \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_PTR_TAG(p)           FIOBJ_PTR_TAG(p, FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p)         FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE         FIOBJ
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
/* make sure functions are exported if requested */
#ifdef FIOBJ_EXTERN
#define FIO_EXTERN
#if defined(FIOBJ_EXTERN_COMPLETE) && !defined(FIO_EXTERN_COMPLETE)
#define FIO_EXTERN_COMPLETE 2
#endif
#endif
#include __FILE__

/* *****************************************************************************
FIOBJ Integers
***************************************************************************** */

/** Creates a new Number object. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(intptr_t i);

/** Reads the number from a FIOBJ Number. */
FIO_IFUNC intptr_t FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(FIOBJ i);

/** Reads the number from a FIOBJ Number, fitting it in a double. */
FIO_IFUNC double FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), f)(FIOBJ i);

/** Returns a String representation of the number (in base 10). */
FIOBJ_FUNC fio_str_info_s FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER),
                                    cstr)(FIOBJ i);

/** Frees a FIOBJ number. */
FIO_IFUNC void FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), free)(FIOBJ i);

FIOBJ_EXTERN_OBJ const FIOBJ_class_vtable_s FIOBJ___NUMBER_CLASS_VTBL;

/* *****************************************************************************
FIOBJ Floats
***************************************************************************** */

/** Creates a new Float (double) object. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), new)(double i);

/** Reads the number from a FIOBJ Float rounding it to an integer. */
FIO_IFUNC intptr_t FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), i)(FIOBJ i);

/** Reads the value from a FIOBJ Float, as a double. */
FIO_IFUNC double FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(FIOBJ i);

/** Returns a String representation of the float. */
FIOBJ_FUNC fio_str_info_s FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT),
                                    cstr)(FIOBJ i);

/** Frees a FIOBJ Float. */
FIO_IFUNC void FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), free)(FIOBJ i);

FIOBJ_EXTERN_OBJ const FIOBJ_class_vtable_s FIOBJ___FLOAT_CLASS_VTBL;

/* *****************************************************************************
FIOBJ Strings
***************************************************************************** */

#define FIO_STR_NAME              FIO_NAME(fiobj, FIOBJ___NAME_STRING)
#define FIO_STR_OPTIMIZE_EMBEDDED 1
#define FIO_REF_NAME              FIO_NAME(fiobj, FIOBJ___NAME_STRING)
#define FIO_REF_CONSTRUCTOR_ONLY  1
#define FIO_REF_DESTROY(s)                                                     \
  do {                                                                         \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), destroy)((FIOBJ)&s);        \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_REF_INIT(s_)                                                       \
  do {                                                                         \
    s_ = (FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s))FIO_STR_INIT;      \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)

#if SIZE_T_MAX == 0xFFFFFFFF /* for 32bit system pointer alignment */
#define FIO_REF_METADATA uint32_t
#endif
#define FIO_PTR_TAG(p)           FIOBJ_PTR_TAG(p, FIOBJ_T_STRING)
#define FIO_PTR_UNTAG(p)         FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE         FIOBJ
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
/* make sure functions are exported if requested */
#ifdef FIOBJ_EXTERN
#define FIO_EXTERN
#if defined(FIOBJ_EXTERN_COMPLETE) && !defined(FIO_EXTERN_COMPLETE)
#define FIO_EXTERN_COMPLETE 2
#endif
#endif
#include __FILE__

/* Creates a new FIOBJ string object, copying the data to the new string. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                         new_cstr)(const char *ptr, size_t len) {
  FIOBJ s = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  FIO_ASSERT_ALLOC(FIOBJ_PTR_UNTAG(s));
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(s, ptr, len);
  return s;
}

/* Creates a new FIOBJ string object with (at least) the requested capacity. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                         new_buf)(size_t capa) {
  FIOBJ s = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  FIO_ASSERT_ALLOC(FIOBJ_PTR_UNTAG(s));
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), reserve)(s, capa);
  return s;
}

/* Creates a new FIOBJ string object, copying the origin (`fiobj2cstr`). */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                         new_copy)(FIOBJ original) {
  FIOBJ s = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  FIO_ASSERT_ALLOC(FIOBJ_PTR_UNTAG(s));
  fio_str_info_s i = FIO_NAME2(fiobj, cstr)(original);
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(s, i.buf, i.len);
  return s;
}

/** Returns information about the string. Same as fiobj_str_info(). */
FIO_IFUNC fio_str_info_s FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                                   cstr)(FIOBJ s) {
  return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), info)(s);
}

/**
 * Creates a temporary FIOBJ String object on the stack.
 *
 * String data might be allocated dynamically.
 */
#define FIOBJ_STR_TEMP_VAR(str_name)                                           \
  struct {                                                                     \
    uint64_t i1;                                                               \
    uint64_t i2;                                                               \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;                       \
  } FIO_NAME(str_name, __auto_mem_tmp) = {0x7f7f7f7f7f7f7f7fULL,               \
                                          0x7f7f7f7f7f7f7f7fULL,               \
                                          FIO_STR_INIT};                       \
  FIOBJ str_name =                                                             \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) |         \
              FIOBJ_T_STRING);

/**
 * Creates a temporary FIOBJ String object on the stack, initialized with a
 * static string.
 *
 * String data might be allocated dynamically.
 */
#define FIOBJ_STR_TEMP_VAR_STATIC(str_name, buf_, len_)                        \
  struct {                                                                     \
    uint64_t i1;                                                               \
    uint64_t i2;                                                               \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;                       \
  } FIO_NAME(str_name,                                                         \
             __auto_mem_tmp) = {0x7f7f7f7f7f7f7f7fULL,                         \
                                0x7f7f7f7f7f7f7f7fULL,                         \
                                FIO_STR_INIT_STATIC2((buf_), (len_))};         \
  FIOBJ str_name =                                                             \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) |         \
              FIOBJ_T_STRING);

/**
 * Creates a temporary FIOBJ String object on the stack, initialized with a
 * static string.
 *
 * String data might be allocated dynamically.
 */
#define FIOBJ_STR_TEMP_VAR_EXISTING(str_name, buf_, len_, capa_)               \
  struct {                                                                     \
    uint64_t i1;                                                               \
    uint64_t i2;                                                               \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), s) s;                       \
  } FIO_NAME(str_name, __auto_mem_tmp) = {                                     \
      0x7f7f7f7f7f7f7f7fULL,                                                   \
      0x7f7f7f7f7f7f7f7fULL,                                                   \
      FIO_STR_INIT_EXISTING((buf_), (len_), (capa_))};                         \
  FIOBJ str_name =                                                             \
      (FIOBJ)(((uintptr_t) & (FIO_NAME(str_name, __auto_mem_tmp).s)) |         \
              FIOBJ_T_STRING);

/** Resets a temporary FIOBJ String, freeing and any resources allocated. */
#define FIOBJ_STR_TEMP_DESTROY(str_name)                                       \
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), destroy)(str_name);

/* *****************************************************************************
FIOBJ Arrays
***************************************************************************** */

#define FIO_ARRAY_NAME           FIO_NAME(fiobj, FIOBJ___NAME_ARRAY)
#define FIO_REF_NAME             FIO_NAME(fiobj, FIOBJ___NAME_ARRAY)
#define FIO_REF_CONSTRUCTOR_ONLY 1
#define FIO_REF_DESTROY(a)                                                     \
  do {                                                                         \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), destroy)((FIOBJ)&a);         \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_REF_INIT(a)                                                        \
  do {                                                                         \
    a = (FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), s))FIO_ARRAY_INIT;      \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#if SIZE_T_MAX == 0xFFFFFFFF /* for 32bit system pointer alignment */
#define FIO_REF_METADATA uint32_t
#endif
#define FIO_ARRAY_TYPE            FIOBJ
#define FIO_ARRAY_TYPE_CMP(a, b)  FIO_NAME_BL(fiobj, eq)((a), (b))
#define FIO_ARRAY_TYPE_DESTROY(o) fiobj_free(o)
#define FIO_ARRAY_TYPE_CONCAT_COPY(dest, obj)                                  \
  do {                                                                         \
    dest = fiobj_dup(obj);                                                     \
  } while (0)
#define FIO_PTR_TAG(p)           FIOBJ_PTR_TAG(p, FIOBJ_T_ARRAY)
#define FIO_PTR_UNTAG(p)         FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE         FIOBJ
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
/* make sure functions are exported if requested */
#ifdef FIOBJ_EXTERN
#define FIO_EXTERN
#if defined(FIOBJ_EXTERN_COMPLETE) && !defined(FIO_EXTERN_COMPLETE)
#define FIO_EXTERN_COMPLETE 2
#endif
#endif
#include __FILE__

/* *****************************************************************************
FIOBJ Hash Maps
***************************************************************************** */

#define FIO_OMAP_NAME            FIO_NAME(fiobj, FIOBJ___NAME_HASH)
#define FIO_REF_NAME             FIO_NAME(fiobj, FIOBJ___NAME_HASH)
#define FIO_REF_CONSTRUCTOR_ONLY 1
#define FIO_REF_DESTROY(a)                                                     \
  do {                                                                         \
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), destroy)((FIOBJ)&a);          \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_REF_INIT(a)                                                        \
  do {                                                                         \
    a = (FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), s))FIO_MAP_INIT;         \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#if SIZE_T_MAX == 0xFFFFFFFF /* for 32bit system pointer alignment */
#define FIO_REF_METADATA uint32_t
#endif
#define FIO_MAP_TYPE              FIOBJ
#define FIO_MAP_TYPE_DESTROY(o)   fiobj_free(o)
#define FIO_MAP_TYPE_DISCARD(o)   fiobj_free(o)
#define FIO_MAP_KEY               FIOBJ
#define FIO_MAP_KEY_CMP(a, b)     FIO_NAME_BL(fiobj, eq)((a), (b))
#define FIO_MAP_KEY_COPY(dest, o) (dest = fiobj_dup(o))
#define FIO_MAP_KEY_DESTROY(o)    fiobj_free(o)
#define FIO_PTR_TAG(p)            FIOBJ_PTR_TAG(p, FIOBJ_T_HASH)
#define FIO_PTR_UNTAG(p)          FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE          FIOBJ
#define FIO_MEM_REALLOC_          FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_             FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_  FIOBJ_MEM_REALLOC_IS_SAFE
/* make sure functions are exported if requested */
#ifdef FIOBJ_EXTERN
#define FIO_EXTERN
#if defined(FIOBJ_EXTERN_COMPLETE) && !defined(FIO_EXTERN_COMPLETE)
#define FIO_EXTERN_COMPLETE 2
#endif
#endif
#include __FILE__

/** Calculates an object's hash value for a specific hash map object. */
FIO_IFUNC uint64_t FIO_NAME2(fiobj, hash)(FIOBJ target_hash, FIOBJ object_key);

/** Inserts a value to a hash map, with a default hash value calculation. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         set2)(FIOBJ hash, FIOBJ key, FIOBJ value);

/**
 * Inserts a value to a hash map, with a default hash value calculation.
 *
 * If the key already exists in the Hash Map, the value will be freed instead.
 */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         set_if_missing2)(FIOBJ hash, FIOBJ key, FIOBJ value);

/** Finds a value in a hash map, with a default hash value calculation. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(FIOBJ hash,
                                                                   FIOBJ key);

/** Removes a value from a hash map, with a default hash value calculation. */
FIO_IFUNC int FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                       remove2)(FIOBJ hash, FIOBJ key, FIOBJ *old);

/**
 * Sets a value in a hash map, allocating the key String and automatically
 * calculating the hash value.
 */
FIO_IFUNC
FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
               set3)(FIOBJ hash, const char *key, size_t len, FIOBJ value);

/**
 * Finds a value in the hash map, using a temporary String and automatically
 * calculating the hash value.
 */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         get3)(FIOBJ hash, const char *buf, size_t len);

/**
 * Removes a value in a hash map, using a temporary String and automatically
 * calculating the hash value.
 */
FIO_IFUNC int FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                       remove3)(FIOBJ hash,
                                const char *buf,
                                size_t len,
                                FIOBJ *old);

/* *****************************************************************************
FIOBJ JSON support
***************************************************************************** */

/**
 * Returns a JSON valid FIOBJ String, representing the object.
 *
 * If `dest` is an existing String, the formatted JSON data will be appended to
 * the existing string.
 */
FIO_IFUNC FIOBJ FIO_NAME2(fiobj, json)(FIOBJ dest, FIOBJ o, uint8_t beautify);

/**
 * Updates a Hash using JSON data.
 *
 * Parsing errors and non-dictionary object JSON data are silently ignored,
 * attempting to update the Hash as much as possible before any errors
 * encountered.
 *
 * Conflicting Hash data is overwritten (preferring the new over the old).
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
FIOBJ_FUNC size_t FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                           update_json)(FIOBJ hash, fio_str_info_s str);

/** Helper function, calls `fiobj_hash_update_json` with string information */
FIO_IFUNC size_t FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                          update_json2)(FIOBJ hash, char *ptr, size_t len);

/**
 * Parses a C string for JSON data. If `consumed` is not NULL, the `size_t`
 * variable will contain the number of bytes consumed before the parser stopped
 * (due to either error or end of a valid JSON data segment).
 *
 * Returns a FIOBJ object matching the JSON valid C string `str`.
 *
 * If the parsing failed (no complete valid JSON data) `FIOBJ_INVALID` is
 * returned.
 */
FIOBJ_FUNC FIOBJ fiobj_json_parse(fio_str_info_s str, size_t *consumed);

/** Helper macro, calls `fiobj_json_parse` with string information */
#define fiobj_json_parse2(data_, len_, consumed)                               \
  fiobj_json_parse((fio_str_info_s){.buf = data_, .len = len_}, consumed)

/**
 * Uses JavaScript style notation to find data in an object structure.
 *
 * For example, "[0].name" will return the "name" property of the first object
 * in an array object.
 *
 * Returns a temporary reference to the object or FIOBJ_INVALID on an error.
 *
 * Use `fiobj_dup` to collect an actual reference to the returned object.
 */
FIOBJ_FUNC FIOBJ fiobj_json_find(FIOBJ object, fio_str_info_s notation);
/**
 * Uses JavaScript style notation to find data in an object structure.
 *
 * For example, "[0].name" will return the "name" property of the first object
 * in an array object.
 *
 * Returns a temporary reference to the object or FIOBJ_INVALID on an error.
 *
 * Use `fiobj_dup` to collect an actual reference to the returned object.
 */
#define fiobj_json_find2(object, str, length)                                  \
  fiobj_json_find(object, (fio_str_info_s){.buf = str, .len = length})
/* *****************************************************************************







FIOBJ - Implementation - Inline / Macro like fucntions







***************************************************************************** */

/* *****************************************************************************
The FIOBJ Type
***************************************************************************** */

/** Returns an objects type. This isn't limited to known types. */
FIO_IFUNC size_t fiobj_type(FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE:
    switch ((uintptr_t)(o)) {
    case FIOBJ_T_NULL:
      return FIOBJ_T_NULL;
    case FIOBJ_T_TRUE:
      return FIOBJ_T_TRUE;
    case FIOBJ_T_FALSE:
      return FIOBJ_T_FALSE;
    };
    return FIOBJ_T_INVALID;
  case FIOBJ_T_NUMBER:
    return FIOBJ_T_NUMBER;
  case FIOBJ_T_FLOAT:
    return FIOBJ_T_FLOAT;
  case FIOBJ_T_STRING:
    return FIOBJ_T_STRING;
  case FIOBJ_T_ARRAY:
    return FIOBJ_T_ARRAY;
  case FIOBJ_T_HASH:
    return FIOBJ_T_HASH;
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(o))->type_id;
  }
  if (!o)
    return FIOBJ_T_NULL;
  return FIOBJ_T_INVALID;
}

/* *****************************************************************************
FIOBJ Memory Management
***************************************************************************** */

/** Increases an object's reference count (or copies) and returns it. */
FIO_IFUNC FIOBJ fiobj_dup(FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE: /* fallthrough */
  case FIOBJ_T_NUMBER:    /* fallthrough */
  case FIOBJ_T_FLOAT:     /* fallthrough */
    return o;
  case FIOBJ_T_STRING: /* fallthrough */
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), dup)(o);
    break;
  case FIOBJ_T_ARRAY: /* fallthrough */
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), dup)(o);
    break;
  case FIOBJ_T_HASH: /* fallthrough */
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), dup)(o);
    break;
  case FIOBJ_T_OTHER: /* fallthrough */
    fiobj_object_dup(o);
  }
  return o;
}

/** Decreases an object's reference count or frees it. */
FIO_IFUNC void fiobj_free(FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE: /* fallthrough */
  case FIOBJ_T_NUMBER:    /* fallthrough */
  case FIOBJ_T_FLOAT:
    return;
  case FIOBJ_T_STRING:
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), free)(o);
    return;
  case FIOBJ_T_ARRAY:
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), free)(o);
    return;
  case FIOBJ_T_HASH:
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), free)(o);
    return;
  case FIOBJ_T_OTHER:
    (*fiobj_object_metadata(o))->free2(o);
    return;
  }
}

/* *****************************************************************************
FIOBJ Data / Info
***************************************************************************** */

/** Internal: compares two nestable objects. */
FIOBJ_FUNC unsigned char fiobj___test_eq_nested(FIOBJ restrict a,
                                                FIOBJ restrict b);

/** Compares two objects. */
FIO_IFUNC unsigned char FIO_NAME_BL(fiobj, eq)(FIOBJ a, FIOBJ b) {
  if (a == b)
    return 1;
  if (FIOBJ_TYPE_CLASS(a) != FIOBJ_TYPE_CLASS(b))
    return 0;
  switch (FIOBJ_TYPE_CLASS(a)) {
  case FIOBJ_T_PRIMITIVE:
  case FIOBJ_T_NUMBER: /* fallthrough */
  case FIOBJ_T_FLOAT:  /* fallthrough */
    return a == b;
  case FIOBJ_T_STRING:
    return FIO_NAME_BL(FIO_NAME(fiobj, FIOBJ___NAME_STRING), eq)(a, b);
  case FIOBJ_T_ARRAY:
    return fiobj___test_eq_nested(a, b);
  case FIOBJ_T_HASH:
    return fiobj___test_eq_nested(a, b);
  case FIOBJ_T_OTHER:
    if ((*fiobj_object_metadata(a))->count(a) ||
        (*fiobj_object_metadata(b))->count(b)) {
      if ((*fiobj_object_metadata(a))->count(a) !=
          (*fiobj_object_metadata(b))->count(b))
        return 0;
      return fiobj___test_eq_nested(a, b);
    }
    return (*fiobj_object_metadata(a))->type_id ==
               (*fiobj_object_metadata(b))->type_id &&
           (*fiobj_object_metadata(a))->is_eq(a, b);
  }
  return 0;
}

#define FIOBJ2CSTR_BUFFER_LIMIT 4096
__thread char __attribute__((weak))
fiobj___2cstr___buffer__perthread[FIOBJ2CSTR_BUFFER_LIMIT];

/** Returns a temporary String representation for any FIOBJ object. */
FIO_IFUNC fio_str_info_s FIO_NAME2(fiobj, cstr)(FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE:
    switch ((uintptr_t)(o)) {
    case FIOBJ_T_NULL:
      return (fio_str_info_s){.buf = (char *)"null", .len = 4};
    case FIOBJ_T_TRUE:
      return (fio_str_info_s){.buf = (char *)"true", .len = 4};
    case FIOBJ_T_FALSE:
      return (fio_str_info_s){.buf = (char *)"false", .len = 5};
    };
    return (fio_str_info_s){.buf = (char *)""};
  case FIOBJ_T_NUMBER:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), cstr)(o);
  case FIOBJ_T_FLOAT:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), cstr)(o);
  case FIOBJ_T_STRING:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), info)(o);
  case FIOBJ_T_ARRAY: /* fallthrough */
  case FIOBJ_T_HASH: {
    FIOBJ j = FIO_NAME2(fiobj, json)(FIOBJ_INVALID, o, 0);
    if (!j || FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(j) >=
                  FIOBJ2CSTR_BUFFER_LIMIT) {
      fiobj_free(j);
      return (fio_str_info_s){.buf = (FIOBJ_TYPE_CLASS(o) == FIOBJ_T_ARRAY
                                          ? (char *)"[...]"
                                          : (char *)"{...}"),
                              .len = 5};
    }
    fio_str_info_s i = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), info)(j);
    FIO_MEMCPY(fiobj___2cstr___buffer__perthread, i.buf, i.len + 1);
    fiobj_free(j);
    i.buf = fiobj___2cstr___buffer__perthread;
    return i;
  }
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(o))->to_s(o);
  }
  if (!o)
    return (fio_str_info_s){.buf = (char *)"null", .len = 4};
  return (fio_str_info_s){.buf = (char *)""};
}

/** Returns an integer representation for any FIOBJ object. */
FIO_IFUNC intptr_t FIO_NAME2(fiobj, i)(FIOBJ o) {
  fio_str_info_s tmp;
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE:
    switch ((uintptr_t)(o)) {
    case FIOBJ_T_NULL:
      return 0;
    case FIOBJ_T_TRUE:
      return 1;
    case FIOBJ_T_FALSE:
      return 0;
    };
    return -1;
  case FIOBJ_T_NUMBER:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(o);
  case FIOBJ_T_FLOAT:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), i)(o);
  case FIOBJ_T_STRING:
    tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), info)(o);
    if (!tmp.len)
      return 0;
    return fio_atol(&tmp.buf);
  case FIOBJ_T_ARRAY:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
  case FIOBJ_T_HASH:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), count)(o);
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(o))->to_i(o);
  }
  if (!o)
    return 0;
  return -1;
}

/** Returns a float (double) representation for any FIOBJ object. */
FIO_IFUNC double FIO_NAME2(fiobj, f)(FIOBJ o) {
  fio_str_info_s tmp;
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE:
    switch ((uintptr_t)(o)) {
    case FIOBJ_T_FALSE: /* fallthrough */
    case FIOBJ_T_NULL:
      return 0.0;
    case FIOBJ_T_TRUE:
      return 1.0;
    };
    return -1.0;
  case FIOBJ_T_NUMBER:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), f)(o);
  case FIOBJ_T_FLOAT:
    return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(o);
  case FIOBJ_T_STRING:
    tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), info)(o);
    if (!tmp.len)
      return 0;
    return (double)fio_atof(&tmp.buf);
  case FIOBJ_T_ARRAY:
    return (double)FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
  case FIOBJ_T_HASH:
    return (double)FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), count)(o);
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(o))->to_f(o);
  }
  if (!o)
    return 0.0;
  return -1.0;
}

/* *****************************************************************************
FIOBJ Integers
***************************************************************************** */

#define FIO_REF_NAME     fiobj___bignum
#define FIO_REF_TYPE     intptr_t
#define FIO_REF_METADATA const FIOBJ_class_vtable_s *
#define FIO_REF_METADATA_INIT(m)                                               \
  do {                                                                         \
    m = &FIOBJ___NUMBER_CLASS_VTBL;                                            \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_PTR_TAG(p)           FIOBJ_PTR_TAG(p, FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p)         FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE         FIOBJ
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
#include __FILE__

#define FIO_NUMBER_ENCODE(i) (((uintptr_t)(i) << 3) | FIOBJ_T_NUMBER)
#define FIO_NUMBER_REVESE(i)                                                   \
  ((intptr_t)(((uintptr_t)(i) >> 3) |                                          \
              ((((uintptr_t)(i) >> ((sizeof(uintptr_t) * 8) - 1)) *            \
                ((uintptr_t)3 << ((sizeof(uintptr_t) * 8) - 3))))))

/** Creates a new Number object. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER),
                         new)(intptr_t i) {
  FIOBJ o = (FIOBJ)FIO_NUMBER_ENCODE(i);
  if (FIO_NUMBER_REVESE(o) == i)
    return o;
  o = fiobj___bignum_new2();

  FIO_PTR_MATH_RMASK(intptr_t, o, 3)[0] = i;
  return o;
}

/** Reads the number from a FIOBJ number. */
FIO_IFUNC intptr_t FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(FIOBJ i) {
  if (FIOBJ_TYPE_CLASS(i) == FIOBJ_T_NUMBER)
    return FIO_NUMBER_REVESE(i);
  return FIO_PTR_MATH_RMASK(intptr_t, i, 3)[0];
}

/** Reads the number from a FIOBJ number, fitting it in a double. */
FIO_IFUNC double FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), f)(FIOBJ i) {
  return (double)FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(i);
}

/** Frees a FIOBJ number. */
FIO_IFUNC void FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), free)(FIOBJ i) {
  if (FIOBJ_TYPE_CLASS(i) == FIOBJ_T_NUMBER)
    return;
  fiobj___bignum_free2(i);
  return;
}
#undef FIO_NUMBER_ENCODE
#undef FIO_NUMBER_REVESE

/* *****************************************************************************
FIOBJ Floats
***************************************************************************** */

#define FIO_REF_NAME     fiobj___bigfloat
#define FIO_REF_TYPE     double
#define FIO_REF_METADATA const FIOBJ_class_vtable_s *
#define FIO_REF_METADATA_INIT(m)                                               \
  do {                                                                         \
    m = &FIOBJ___FLOAT_CLASS_VTBL;                                             \
    FIOBJ_MARK_MEMORY_ALLOC();                                                 \
  } while (0)
#define FIO_REF_METADATA_DESTROY(m)                                            \
  do {                                                                         \
    FIOBJ_MARK_MEMORY_FREE();                                                  \
  } while (0)
#define FIO_PTR_TAG(p)           FIOBJ_PTR_TAG(p, FIOBJ_T_OTHER)
#define FIO_PTR_UNTAG(p)         FIOBJ_PTR_UNTAG(p)
#define FIO_PTR_TAG_TYPE         FIOBJ
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
#include __FILE__

/** Creates a new Float object. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), new)(double i) {
  FIOBJ ui;
  if (sizeof(double) <= sizeof(FIOBJ)) {
    union {
      double d;
      uintptr_t i;
    } punned;
    punned.i = 0; /* dead code, but leave it, just in case */
    punned.d = i;
    if ((punned.i & 7) == 0) {
      return (FIOBJ)(punned.i | FIOBJ_T_FLOAT);
    }
  }
  ui = fiobj___bigfloat_new2();

  FIO_PTR_MATH_RMASK(double, ui, 3)[0] = i;
  return ui;
}

/** Reads the integer part from a FIOBJ Float. */
FIO_IFUNC intptr_t FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), i)(FIOBJ i) {
  return (intptr_t)floor(FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(i));
}

/** Reads the number from a FIOBJ number, fitting it in a double. */
FIO_IFUNC double FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(FIOBJ i) {
  if (sizeof(double) <= sizeof(FIOBJ) && FIOBJ_TYPE_CLASS(i) == FIOBJ_T_FLOAT) {
    union {
      double d;
      uint64_t i;
    } punned;
    punned.d = 0; /* dead code, but leave it, just in case */
    punned.i = (uint64_t)(uintptr_t)i;
    punned.i = ((uint64_t)(uintptr_t)i & (~(uintptr_t)7ULL));
    return punned.d;
  }
  return FIO_PTR_MATH_RMASK(double, i, 3)[0];
}

/** Frees a FIOBJ number. */
FIO_IFUNC void FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), free)(FIOBJ i) {
  if (FIOBJ_TYPE_CLASS(i) == FIOBJ_T_FLOAT)
    return;
  fiobj___bigfloat_free2(i);
  return;
}

/* *****************************************************************************
FIOBJ Basic Iteration
***************************************************************************** */

/**
 * Performs a task for each element held by the FIOBJ object.
 *
 * If `task` returns -1, the `each` loop will break (stop).
 *
 * Returns the "stop" position - the number of elements processed + `start_at`.
 */
FIO_SFUNC uint32_t fiobj_each1(FIOBJ o,
                               int (*task)(fiobj_each_s *e),
                               void *udata,
                               int32_t start_at) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE: /* fallthrough */
  case FIOBJ_T_NUMBER:    /* fallthrough */
  case FIOBJ_T_STRING:    /* fallthrough */
  case FIOBJ_T_FLOAT:
    return 0;
  case FIOBJ_T_ARRAY:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), each)(
        o,
        (int (*)(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), each_s *)))task,
        udata,
        start_at);
  case FIOBJ_T_HASH:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), each)(
        o,
        (int (*)(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), each_s *)))task,
        udata,
        start_at);
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(o))->each1(o, task, udata, start_at);
  }
  return 0;
}

/* *****************************************************************************
FIOBJ Hash Maps
***************************************************************************** */

/** Calculates an object's hash value for a specific hash map object. */
FIO_IFUNC uint64_t FIO_NAME2(fiobj, hash)(FIOBJ target_hash, FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE:
    return fio_risky_hash(&o,
                          sizeof(o),
                          (uint64_t)(uintptr_t)target_hash + (uintptr_t)o);
  case FIOBJ_T_NUMBER: {
    uintptr_t tmp = FIO_NAME2(fiobj, i)(o);
    return fio_risky_hash(&tmp, sizeof(tmp), (uint64_t)(uintptr_t)target_hash);
  }
  case FIOBJ_T_FLOAT: {
    double tmp = FIO_NAME2(fiobj, f)(o);
    return fio_risky_hash(&tmp, sizeof(tmp), (uint64_t)(uintptr_t)target_hash);
  }
  case FIOBJ_T_STRING: /* fallthrough */
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                    hash)(o, (uint64_t)(uintptr_t)target_hash);
  case FIOBJ_T_ARRAY: {
    uint64_t h = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
    h += fio_risky_hash(&h,
                        sizeof(h),
                        (uint64_t)(uintptr_t)target_hash + FIOBJ_T_ARRAY);
    {
      FIOBJ *a = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), ptr)(o);
      const size_t count =
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
      if (a) {
        for (size_t i = 0; i < count; ++i) {
          h += FIO_NAME2(fiobj, hash)(target_hash + FIOBJ_T_ARRAY + i, a[i]);
        }
      }
    }
    return h;
  }
  case FIOBJ_T_HASH: {
    uint64_t h = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), count)(o);
    size_t c = 0;
    h += fio_risky_hash(&h,
                        sizeof(h),
                        (uint64_t)(uintptr_t)target_hash + FIOBJ_T_HASH);
    FIO_MAP_EACH(FIO_NAME(fiobj, FIOBJ___NAME_HASH), o, pos) {
      h += FIO_NAME2(fiobj, hash)(target_hash + FIOBJ_T_HASH + (c++),
                                  pos->obj.key);
      h += FIO_NAME2(fiobj, hash)(target_hash + FIOBJ_T_HASH + (c++),
                                  pos->obj.value);
    }
    return h;
  }
  case FIOBJ_T_OTHER: {
    /* TODO: can we avoid "stringifying" the object? */
    fio_str_info_s tmp = (*fiobj_object_metadata(o))->to_s(o);
    return fio_risky_hash(tmp.buf, tmp.len, (uint64_t)(uintptr_t)target_hash);
  }
  }
  return 0;
}

/** Inserts a value to a hash map, with a default hash value calculation. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         set2)(FIOBJ hash, FIOBJ key, FIOBJ value) {
  return FIO_NAME(
      FIO_NAME(fiobj, FIOBJ___NAME_HASH),
      set)(hash, FIO_NAME2(fiobj, hash)(hash, key), key, value, NULL);
}

/**
 * Inserts a value to a hash map, with a default hash value calculation.
 *
 * If the key already exists in the Hash Map, the value will be freed instead.
 */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         set_if_missing2)(FIOBJ hash, FIOBJ key, FIOBJ value) {
  return FIO_NAME(
      FIO_NAME(fiobj, FIOBJ___NAME_HASH),
      set_if_missing)(hash, FIO_NAME2(fiobj, hash)(hash, key), key, value);
}

/** Finds a value in a hash map, automatically calculating the hash value. */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(FIOBJ hash,
                                                                   FIOBJ key) {
  if (FIOBJ_TYPE_CLASS(hash) != FIOBJ_T_HASH)
    return FIOBJ_INVALID;
  return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                  get)(hash, FIO_NAME2(fiobj, hash)(hash, key), key);
}

/** Removes a value from a hash map, with a default hash value calculation. */
FIO_IFUNC int FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                       remove2)(FIOBJ hash, FIOBJ key, FIOBJ *old) {
  return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                  remove)(hash, FIO_NAME2(fiobj, hash)(hash, key), key, old);
}

/**
 * Sets a String value in a hash map, allocating the String and automatically
 * calculating the hash value.
 */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         set3)(FIOBJ hash,
                               const char *key,
                               size_t len,
                               FIOBJ value) {
  FIOBJ tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(tmp, (char *)key, len);
  FIOBJ v = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                     set)(hash,
                          fio_risky_hash(key, len, (uint64_t)(uintptr_t)hash),
                          tmp,
                          value,
                          NULL);
  fiobj_free(tmp);
  return v;
}

/**
 * Finds a String value in a hash map, using a temporary String and
 * automatically calculating the hash value.
 */
FIO_IFUNC FIOBJ FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                         get3)(FIOBJ hash, const char *buf, size_t len) {
  if (FIOBJ_TYPE_CLASS(hash) != FIOBJ_T_HASH)
    return FIOBJ_INVALID;
  FIOBJ_STR_TEMP_VAR_STATIC(tmp, buf, len);
  FIOBJ v = FIO_NAME(
      FIO_NAME(fiobj, FIOBJ___NAME_HASH),
      get)(hash, fio_risky_hash(buf, len, (uint64_t)(uintptr_t)hash), tmp);
  return v;
}

/**
 * Removes a String value in a hash map, using a temporary String and
 * automatically calculating the hash value.
 */
FIO_IFUNC int FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                       remove3)(FIOBJ hash,
                                const char *buf,
                                size_t len,
                                FIOBJ *old) {
  FIOBJ_STR_TEMP_VAR_STATIC(tmp, buf, len);
  int r = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                   remove)(hash,
                           fio_risky_hash(buf, len, (uint64_t)(uintptr_t)hash),
                           tmp,
                           old);
  FIOBJ_STR_TEMP_DESTROY(tmp);
  return r;
}

/** Updates a hash using information from another Hash. */
FIO_IFUNC void FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), update)(FIOBJ dest,
                                                                    FIOBJ src) {
  if (FIOBJ_TYPE_CLASS(dest) != FIOBJ_T_HASH ||
      FIOBJ_TYPE_CLASS(src) != FIOBJ_T_HASH)
    return;
  FIO_MAP_EACH(FIO_NAME(fiobj, FIOBJ___NAME_HASH), src, i) {
    if (i->obj.key == FIOBJ_INVALID ||
        FIOBJ_TYPE_CLASS(i->obj.key) == FIOBJ_T_NULL) {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), remove2)
      (dest, i->obj.key, NULL);
      continue;
    }
    register FIOBJ tmp;
    switch (FIOBJ_TYPE_CLASS(i->obj.value)) {
    case FIOBJ_T_ARRAY:
      /* TODO? decide if we should merge elements or overwrite...? */
      tmp =
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(dest, i->obj.key);
      if (FIOBJ_TYPE_CLASS(tmp) == FIOBJ_T_ARRAY) {
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), concat)
        (tmp, i->obj.value);
        continue;
      }
      break;
    case FIOBJ_T_HASH:
      tmp =
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(dest, i->obj.key);
      if (FIOBJ_TYPE_CLASS(tmp) == FIOBJ_T_HASH)
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), update)
      (dest, i->obj.value);
      else break;
      continue;
    case FIOBJ_T_NUMBER:    /* fallthrough */
    case FIOBJ_T_PRIMITIVE: /* fallthrough */
    case FIOBJ_T_STRING:    /* fallthrough */
    case FIOBJ_T_FLOAT:     /* fallthrough */
    case FIOBJ_T_OTHER:
      break;
    }
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set2)
    (dest, i->obj.key, fiobj_dup(i->obj.value));
  }
}

/* *****************************************************************************
FIOBJ JSON support (inline functions)
***************************************************************************** */

typedef struct {
  FIOBJ json;
  size_t level;
  uint8_t beautify;
} fiobj___json_format_internal__s;

/* internal helper function for recursive JSON formatting. */
FIOBJ_FUNC void fiobj___json_format_internal__(
    fiobj___json_format_internal__s *,
    FIOBJ);

/** Helper function, calls `fiobj_hash_update_json` with string information */
FIO_IFUNC size_t FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                          update_json2)(FIOBJ hash, char *ptr, size_t len) {
  return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                  update_json)(hash, (fio_str_info_s){.buf = ptr, .len = len});
}

/**
 * Returns a JSON valid FIOBJ String, representing the object.
 *
 * If `dest` is an existing String, the formatted JSON data will be appended to
 * the existing string.
 */
FIO_IFUNC FIOBJ FIO_NAME2(fiobj, json)(FIOBJ dest, FIOBJ o, uint8_t beautify) {
  fiobj___json_format_internal__s args =
      (fiobj___json_format_internal__s){.json = dest, .beautify = beautify};
  if (FIOBJ_TYPE_CLASS(dest) != FIOBJ_T_STRING)
    args.json = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  fiobj___json_format_internal__(&args, o);
  return args.json;
}

/* *****************************************************************************
FIOBJ - Implementation
***************************************************************************** */
#ifdef FIOBJ_EXTERN_COMPLETE

/* *****************************************************************************
FIOBJ Basic Object vtable
***************************************************************************** */

FIOBJ_EXTERN_OBJ_IMP const FIOBJ_class_vtable_s FIOBJ___OBJECT_CLASS_VTBL = {
    .type_id = 99, /* type IDs below 100 are reserved. */
};

/* *****************************************************************************
FIOBJ Complex Iteration
***************************************************************************** */
typedef struct {
  FIOBJ obj;
  size_t pos;
} fiobj____stack_element_s;

#define FIO_ARRAY_NAME fiobj____active_stack
#define FIO_ARRAY_TYPE fiobj____stack_element_s
#define FIO_ARRAY_COPY(dest, src)                                              \
  do {                                                                         \
    (dest).obj = fiobj_dup((src).obj);                                         \
    (dest).pos = (src).pos;                                                    \
  } while (0)
#define FIO_ARRAY_TYPE_CMP(a, b) (a).obj == (b).obj
#define FIO_ARRAY_DESTROY(o)     fiobj_free(o)
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
#include __FILE__
#define FIO_ARRAY_TYPE_CMP(a, b) (a).obj == (b).obj
#define FIO_ARRAY_NAME           fiobj____stack
#define FIO_ARRAY_TYPE           fiobj____stack_element_s
#define FIO_MEM_REALLOC_         FIOBJ_MEM_REALLOC
#define FIO_MEM_FREE_            FIOBJ_MEM_FREE
#define FIO_MEM_REALLOC_IS_SAFE_ FIOBJ_MEM_REALLOC_IS_SAFE
#include __FILE__

typedef struct {
  int (*task)(fiobj_each_s *info);
  void *arg;
  FIOBJ next;
  size_t count;
  fiobj____stack_s stack;
  uint32_t end;
  uint8_t stop;
} fiobj_____each2_data_s;

FIO_SFUNC uint32_t fiobj____each2_element_count(FIOBJ o) {
  switch (FIOBJ_TYPE_CLASS(o)) {
  case FIOBJ_T_PRIMITIVE: /* fallthrough */
  case FIOBJ_T_NUMBER:    /* fallthrough */
  case FIOBJ_T_STRING:    /* fallthrough */
  case FIOBJ_T_FLOAT:
    return 0;
  case FIOBJ_T_ARRAY:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
  case FIOBJ_T_HASH:
    return FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), count)(o);
  case FIOBJ_T_OTHER: /* fallthrough */
    return (*fiobj_object_metadata(o))->count(o);
  }
  return 0;
}
FIO_SFUNC int fiobj____each2_wrapper_task(fiobj_each_s *e) {
  fiobj_____each2_data_s *d = (fiobj_____each2_data_s *)e->udata;
  e->task = d->task;
  e->udata = d->arg;
  d->stop = (d->task(e) == -1);
  d->task = e->task;
  d->arg = e->udata;
  e->task = fiobj____each2_wrapper_task;
  e->udata = d;
  ++d->count;
  if (d->stop)
    return -1;
  uint32_t c = fiobj____each2_element_count(e->obj[0]);
  if (c) {
    d->next = e->obj[0];
    d->end = c;
    return -1;
  }
  return 0;
}

/**
 * Performs a task for the object itself and each element held by the FIOBJ
 * object or any of it's elements (a deep task).
 *
 * The order of performance is by order of appearance, as if all nesting levels
 * were flattened.
 *
 * If `task` returns -1, the `each` loop will break (stop).
 *
 * Returns the number of elements processed.
 */
FIOBJ_FUNC uint32_t fiobj_each2(FIOBJ o,
                                int (*task)(fiobj_each_s *),
                                void *udata) {
  /* TODO - move to recursion with nesting limiter? */
  fiobj_____each2_data_s d = {
      .task = task,
      .arg = udata,
      .next = FIOBJ_INVALID,
      .stack = FIO_ARRAY_INIT,
  };
  struct FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), each_s) e_tmp = {

      .parent = FIOBJ_INVALID,
      .task = (int (*)(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY),
                                each_s) *))fiobj____each2_wrapper_task,
      .udata = &d,
      .items_at_index = 1,
      .value = o,
  };
  fiobj____stack_element_s i = {.obj = o, .pos = 0};
  uint32_t end = fiobj____each2_element_count(o);
  fiobj____each2_wrapper_task((fiobj_each_s *)&e_tmp);
  while (!d.stop && i.obj && i.pos < end) {
    i.pos = fiobj_each1(i.obj, fiobj____each2_wrapper_task, &d, i.pos);
    if (d.next != FIOBJ_INVALID) {
      if (fiobj____stack_count(&d.stack) + 1 > FIOBJ_MAX_NESTING) {
        FIO_LOG_ERROR("FIOBJ nesting level too deep (%u)."
                      "`fiobj_each2` stopping loop early.",
                      (unsigned int)fiobj____stack_count(&d.stack));
        d.stop = 1;
        continue;
      }
      fiobj____stack_push(&d.stack, i);
      i.pos = 0;
      i.obj = d.next;
      d.next = FIOBJ_INVALID;
      end = d.end;
    } else {
      /* re-collect end position to acommodate for changes */
      end = fiobj____each2_element_count(i.obj);
    }
    while (i.pos >= end && fiobj____stack_count(&d.stack)) {
      fiobj____stack_pop(&d.stack, &i);
      end = fiobj____each2_element_count(i.obj);
    }
  };
  fiobj____stack_destroy(&d.stack);
  return d.count;
}

/* *****************************************************************************
FIOBJ Hash / Array / Other (enumerable) Equality test.
***************************************************************************** */

FIO_SFUNC __thread size_t fiobj___test_eq_nested_level = 0;
/** Internal: compares two nestable objects. */
FIOBJ_FUNC unsigned char fiobj___test_eq_nested(FIOBJ restrict a,
                                                FIOBJ restrict b) {
  if (a == b)
    return 1;
  if (FIOBJ_TYPE_CLASS(a) != FIOBJ_TYPE_CLASS(b))
    return 0;
  if (fiobj____each2_element_count(a) != fiobj____each2_element_count(b))
    return 0;
  if (!fiobj____each2_element_count(a))
    return 1;
  if (fiobj___test_eq_nested_level >= FIOBJ_MAX_NESTING)
    return 0;
  ++fiobj___test_eq_nested_level;

  switch (FIOBJ_TYPE_CLASS(a)) {
  case FIOBJ_T_PRIMITIVE:
  case FIOBJ_T_NUMBER: /* fallthrough */
  case FIOBJ_T_FLOAT:  /* fallthrough */
  case FIOBJ_T_STRING: /* fallthrough */
    /* should never happen... this function is for enumerable objects */
    return a == b;
  case FIOBJ_T_ARRAY:
    /* test each array member with matching index */
    {
      const size_t count = fiobj____each2_element_count(a);
      for (size_t i = 0; i < count; ++i) {
        if (!FIO_NAME_BL(fiobj, eq)(
                FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), get)(a, i),
                FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), get)(b, i)))
          goto unequal;
      }
    }
    goto equal;
  case FIOBJ_T_HASH:
    FIO_MAP_EACH(FIO_NAME(fiobj, FIOBJ___NAME_HASH), a, pos) {
      FIOBJ val = fiobj_hash_get2(b, pos->obj.key);
      if (!FIO_NAME_BL(fiobj, eq)(val, pos->obj.value))
        goto equal;
    }
    goto equal;
  case FIOBJ_T_OTHER:
    return (*fiobj_object_metadata(a))->is_eq(a, b);
  }
equal:
  --fiobj___test_eq_nested_level;
  return 1;
unequal:
  --fiobj___test_eq_nested_level;
  return 0;
}

/* *****************************************************************************
FIOBJ general helpers
***************************************************************************** */
FIO_SFUNC __thread char fiobj___tmp_buffer[256];

FIO_SFUNC uint32_t fiobj___count_noop(FIOBJ o) {
  return 0;
  (void)o;
}

/* *****************************************************************************
FIOBJ Integers (bigger numbers)
***************************************************************************** */

FIO_IFUNC unsigned char FIO_NAME_BL(fiobj___num, eq)(FIOBJ restrict a,
                                                     FIOBJ restrict b) {
  return FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(a) ==
         FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(b);
}

FIOBJ_FUNC fio_str_info_s FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER),
                                    cstr)(FIOBJ i) {
  size_t len = fio_ltoa(fiobj___tmp_buffer,
                        FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i)(i),
                        10);
  fiobj___tmp_buffer[len] = 0;
  return (fio_str_info_s){.buf = fiobj___tmp_buffer, .len = len};
}

FIOBJ_EXTERN_OBJ_IMP const FIOBJ_class_vtable_s FIOBJ___NUMBER_CLASS_VTBL = {
    /**
     * MUST return a unique number to identify object type.
     *
     * Numbers (IDs) under 100 are reserved.
     */
    .type_id = FIOBJ_T_NUMBER,
    /** Test for equality between two objects with the same `type_id` */
    .is_eq = FIO_NAME_BL(fiobj___num, eq),
    /** Converts an object to a String */
    .to_s = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), cstr),
    /** Converts and object to an integer */
    .to_i = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), i),
    /** Converts and object to a float */
    .to_f = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), f),
    /** Returns the number of exposed elements held by the object, if any. */
    .count = fiobj___count_noop,
    /** Iterates the exposed elements held by the object. See `fiobj_each1`. */
    .each1 = NULL,
    /** Deallocates the element (but NOT any of it's exposed elements). */
    .free2 = fiobj___bignum_free2,
};

/* *****************************************************************************
FIOBJ Floats (bigger / smaller doubles)
***************************************************************************** */

FIO_SFUNC unsigned char FIO_NAME_BL(fiobj___float, eq)(FIOBJ restrict a,
                                                       FIOBJ restrict b) {
  unsigned char r = 0;
  union {
    uint64_t u;
    double f;
  } da, db;
  da.f = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(a);
  db.f = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(b);
  /* regular equality? */
  r |= da.f == db.f;
  /* test for small rounding errors (4 bit difference) on normalize floats */
  r |= !((da.u ^ db.u) & UINT64_C(0xFFFFFFFFFFFFFFF0)) &&
       (da.u & UINT64_C(0x7FF0000000000000));
  /* test for small ULP: */
  r |= (((da.u > db.u) ? da.u - db.u : db.u - da.u) < 2);
  /* test for +-0 */
  r |= !((da.u | db.u) & UINT64_C(0x7FFFFFFFFFFFFFFF));
  return r;
}

FIOBJ_FUNC fio_str_info_s FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT),
                                    cstr)(FIOBJ i) {
  size_t len = fio_ftoa(fiobj___tmp_buffer,
                        FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f)(i),
                        10);
  fiobj___tmp_buffer[len] = 0;
  return (fio_str_info_s){.buf = fiobj___tmp_buffer, .len = len};
}

FIOBJ_EXTERN_OBJ_IMP const FIOBJ_class_vtable_s FIOBJ___FLOAT_CLASS_VTBL = {
    /**
     * MUST return a unique number to identify object type.
     *
     * Numbers (IDs) under 100 are reserved.
     */
    .type_id = FIOBJ_T_FLOAT,
    /** Test for equality between two objects with the same `type_id` */
    .is_eq = FIO_NAME_BL(fiobj___float, eq),
    /** Converts an object to a String */
    .to_s = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), cstr),
    /** Converts and object to an integer */
    .to_i = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), i),
    /** Converts and object to a float */
    .to_f = FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), f),
    /** Returns the number of exposed elements held by the object, if any. */
    .count = fiobj___count_noop,
    /** Iterates the exposed elements held by the object. See `fiobj_each1`. */
    .each1 = NULL,
    /** Deallocates the element (but NOT any of it's exposed elements). */
    .free2 = fiobj___bigfloat_free2,
};

/* *****************************************************************************
FIOBJ JSON support - output
***************************************************************************** */

FIO_IFUNC void fiobj___json_format_internal_beauty_pad(FIOBJ json,
                                                       size_t level) {
  size_t pos = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(json);
  fio_str_info_s tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                                resize)(json, (level << 1) + pos + 2);
  tmp.buf[pos++] = '\r';
  tmp.buf[pos++] = '\n';
  for (size_t i = 0; i < level; ++i) {
    tmp.buf[pos++] = ' ';
    tmp.buf[pos++] = ' ';
  }
}

FIOBJ_FUNC void fiobj___json_format_internal__(
    fiobj___json_format_internal__s *args,
    FIOBJ o) {
  switch (FIOBJ_TYPE(o)) {
  case FIOBJ_T_TRUE:   /* fallthrough */
  case FIOBJ_T_FALSE:  /* fallthrough */
  case FIOBJ_T_NULL:   /* fallthrough */
  case FIOBJ_T_NUMBER: /* fallthrough */
  case FIOBJ_T_FLOAT:  /* fallthrough */
  {
    fio_str_info_s info = FIO_NAME2(fiobj, cstr)(o);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
    (args->json, info.buf, info.len);
    return;
  }
  case FIOBJ_T_STRING: /* fallthrough */
  default: {
    fio_str_info_s info = FIO_NAME2(fiobj, cstr)(o);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "\"", 1);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_escape)
    (args->json, info.buf, info.len);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "\"", 1);
    return;
  }
  case FIOBJ_T_ARRAY:
    if (args->level == FIOBJ_MAX_NESTING) {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
      (args->json, "[ ]", 3);
      return;
    }
    {
      ++args->level;
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "[", 1);
      const uint32_t len =
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o);
      for (size_t i = 0; i < len; ++i) {
        if (args->beautify) {
          fiobj___json_format_internal_beauty_pad(args->json, args->level);
        }
        FIOBJ child = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), get)(o, i);
        fiobj___json_format_internal__(args, child);
        if (i + 1 < len)
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
        (args->json, ",", 1);
      }
      --args->level;
      if (args->beautify) {
        fiobj___json_format_internal_beauty_pad(args->json, args->level);
      }
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "]", 1);
    }
    return;
  case FIOBJ_T_HASH:
    if (args->level == FIOBJ_MAX_NESTING) {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
      (args->json, "{ }", 3);
      return;
    }
    {
      ++args->level;
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "{", 1);
      size_t i = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), count)(o);
      if (i) {
        FIO_MAP_EACH(FIO_NAME(fiobj, FIOBJ___NAME_HASH), o, couplet) {
          if (args->beautify) {
            fiobj___json_format_internal_beauty_pad(args->json, args->level);
          }
          fio_str_info_s info = FIO_NAME2(fiobj, cstr)(couplet->obj.key);
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
          (args->json, "\"", 1);
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_escape)
          (args->json, info.buf, info.len);
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
          (args->json, "\":", 2);
          fiobj___json_format_internal__(args, couplet->obj.value);
          if (--i)
            FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)
          (args->json, ",", 1);
        }
      }
      --args->level;
      if (args->beautify) {
        fiobj___json_format_internal_beauty_pad(args->json, args->level);
      }
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(args->json, "}", 1);
    }
    return;
  }
}

/* *****************************************************************************
FIOBJ JSON parsing
***************************************************************************** */

#define FIO_JSON
#include __FILE__

/* FIOBJ JSON parser */
typedef struct {
  fio_json_parser_s p;
  FIOBJ key;
  FIOBJ top;
  FIOBJ target;
  FIOBJ stack[JSON_MAX_DEPTH + 1];
  uint8_t so; /* stack offset */
} fiobj_json_parser_s;

static inline void fiobj_json_add2parser(fiobj_json_parser_s *p, FIOBJ o) {
  if (p->top) {
    if (FIOBJ_TYPE_CLASS(p->top) == FIOBJ_T_HASH) {
      if (p->key) {
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set2)(p->top, p->key, o);
        fiobj_free(p->key);
        p->key = FIOBJ_INVALID;
      } else {
        p->key = o;
      }
    } else {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(p->top, o);
    }
  } else {
    p->top = o;
  }
}

/** a NULL object was detected */
static inline void fio_json_on_null(fio_json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p,
                        FIO_NAME(fiobj, FIOBJ___NAME_NULL)());
}
/** a TRUE object was detected */
static inline void fio_json_on_true(fio_json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p,
                        FIO_NAME(fiobj, FIOBJ___NAME_TRUE)());
}
/** a FALSE object was detected */
static inline void fio_json_on_false(fio_json_parser_s *p) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p,
                        FIO_NAME(fiobj, FIOBJ___NAME_FALSE)());
}
/** a Numberl was detected (long long). */
static inline void fio_json_on_number(fio_json_parser_s *p, long long i) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p,
                        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(i));
}
/** a Float was detected (double). */
static inline void fio_json_on_float(fio_json_parser_s *p, double f) {
  fiobj_json_add2parser((fiobj_json_parser_s *)p,
                        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), new)(f));
}
/** a String was detected (int / float). update `pos` to point at ending */
static inline void fio_json_on_string(fio_json_parser_s *p,
                                      const void *start,
                                      size_t len) {
  FIOBJ str = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_unescape)
  (str, start, len);
  fiobj_json_add2parser((fiobj_json_parser_s *)p, str);
}
/** a dictionary object was detected */
static inline int fio_json_on_start_object(fio_json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  if (pr->target) {
    /* push NULL, don't free the objects */
    pr->stack[pr->so++] = FIOBJ_INVALID;
    pr->top = pr->target;
    pr->target = FIOBJ_INVALID;
  } else {
    FIOBJ hash;
#if FIOBJ_JSON_APPEND
    hash = FIOBJ_INVALID;
    if (pr->key && FIOBJ_TYPE_CLASS(pr->top) == FIOBJ_T_HASH) {
      hash =
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(pr->top, pr->key);
    }
    if (FIOBJ_TYPE_CLASS(hash) != FIOBJ_T_HASH) {
      hash = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), new)();
      fiobj_json_add2parser(pr, hash);
    } else {
      fiobj_free(pr->key);
      pr->key = FIOBJ_INVALID;
    }
#else
    hash = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), new)();
    fiobj_json_add2parser(pr, hash);
#endif
    pr->stack[pr->so++] = pr->top;
    pr->top = hash;
  }
  return 0;
}
/** a dictionary object closure detected */
static inline void fio_json_on_end_object(fio_json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  if (pr->key) {
    FIO_LOG_WARNING("(JSON parsing) malformed JSON, "
                    "ignoring dangling Hash key.");
    fiobj_free(pr->key);
    pr->key = FIOBJ_INVALID;
  }
  pr->top = FIOBJ_INVALID;
  if (pr->so)
    pr->top = pr->stack[--pr->so];
}
/** an array object was detected */
static int fio_json_on_start_array(fio_json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  FIOBJ ary = FIOBJ_INVALID;
  if (pr->target != FIOBJ_INVALID) {
    if (FIOBJ_TYPE_CLASS(pr->target) != FIOBJ_T_ARRAY)
      return -1;
    ary = pr->target;
    pr->target = FIOBJ_INVALID;
  }
#if FIOBJ_JSON_APPEND
  if (pr->key && FIOBJ_TYPE_CLASS(pr->top) == FIOBJ_T_HASH) {
    ary = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(pr->top, pr->key);
  }
  if (FIOBJ_TYPE_CLASS(ary) != FIOBJ_T_ARRAY) {
    ary = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    fiobj_json_add2parser(pr, ary);
  } else {
    fiobj_free(pr->key);
    pr->key = FIOBJ_INVALID;
  }
#else
  FIOBJ ary = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
  fiobj_json_add2parser(pr, ary);
#endif

  pr->stack[pr->so++] = pr->top;
  pr->top = ary;
  return 0;
}
/** an array closure was detected */
static inline void fio_json_on_end_array(fio_json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  pr->top = FIOBJ_INVALID;
  if (pr->so)
    pr->top = pr->stack[--pr->so];
}
/** the JSON parsing is complete */
static void fio_json_on_json(fio_json_parser_s *p) {
  (void)p; /* nothing special... right? */
}
/** the JSON parsing is complete */
static inline void fio_json_on_error(fio_json_parser_s *p) {
  fiobj_json_parser_s *pr = (fiobj_json_parser_s *)p;
  fiobj_free(pr->stack[0]);
  fiobj_free(pr->key);
  *pr = (fiobj_json_parser_s){.top = FIOBJ_INVALID};
  FIO_LOG_DEBUG("JSON on_error callback called.");
}

/**
 * Updates a Hash using JSON data.
 *
 * Parsing errors and non-dictionary object JSON data are silently ignored,
 * attempting to update the Hash as much as possible before any errors
 * encountered.
 *
 * Conflicting Hash data is overwritten (preferring the new over the old).
 *
 * Returns the number of bytes consumed. On Error, 0 is returned and no data is
 * consumed.
 */
FIOBJ_FUNC size_t FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH),
                           update_json)(FIOBJ hash, fio_str_info_s str) {
  if (hash == FIOBJ_INVALID)
    return 0;
  fiobj_json_parser_s p = {.top = FIOBJ_INVALID, .target = hash};
  size_t consumed = fio_json_parse(&p.p, str.buf, str.len);
  fiobj_free(p.key);
  if (p.top != hash)
    fiobj_free(p.top);
  return consumed;
}

/** Returns a JSON valid FIOBJ String, representing the object. */
FIOBJ_FUNC FIOBJ fiobj_json_parse(fio_str_info_s str, size_t *consumed_p) {
  fiobj_json_parser_s p = {.top = FIOBJ_INVALID};
  register const size_t consumed = fio_json_parse(&p.p, str.buf, str.len);
  if (consumed_p) {
    *consumed_p = consumed;
  }
  if (!consumed || p.p.depth) {
    if (p.top) {
      FIO_LOG_DEBUG("WARNING - JSON failed secondary validation, no on_error");
    }
#ifdef DEBUG
    FIOBJ s = FIO_NAME2(fiobj, json)(FIOBJ_INVALID, p.top, 0);
    FIO_LOG_DEBUG("JSON data being deleted:\n%s",
                  FIO_NAME2(fiobj, cstr)(s).buf);
    fiobj_free(s);
#endif
    fiobj_free(p.stack[0]);
    p.top = FIOBJ_INVALID;
  }
  fiobj_free(p.key);
  return p.top;
}

/** Uses JSON (JavaScript) notation to find data in an object structure. Returns
 * a temporary object. */
FIOBJ_FUNC FIOBJ fiobj_json_find(FIOBJ o, fio_str_info_s n) {
  for (;;) {
  top:
    if (!n.len)
      return o;
    switch (FIOBJ_TYPE_CLASS(o)) {
    case FIOBJ_T_ARRAY: {
      if (n.len <= 2 || n.buf[0] != '[' || n.buf[1] < '0' || n.buf[1] > '9')
        return FIOBJ_INVALID;
      size_t i = 0;
      ++n.buf;
      --n.len;
      while (n.len && fio_c2i(n.buf[0]) < 10) {
        i = (i * 10) + fio_c2i(n.buf[0]);
        ++n.buf;
        --n.len;
      }
      if (!n.len || n.buf[0] != ']')
        return FIOBJ_INVALID;
      o = fiobj_array_get(o, i);
      ++n.buf;
      --n.len;
      if (n.len) {
        if (n.buf[0] == '.') {
          ++n.buf;
          --n.len;
        } else if (n.buf[0] != '[') {
          return FIOBJ_INVALID;
        }
        continue;
      }
      return o;
    }
    case FIOBJ_T_HASH: {
      FIOBJ tmp = fiobj_hash_get3(o, n.buf, n.len);
      if (tmp != FIOBJ_INVALID)
        return tmp;
      char *end = n.buf + n.len - 1;
      while (end > n.buf) {
        while (end > n.buf && end[0] != '.' && end[0] != '[')
          --end;
        if (end == n.buf)
          return FIOBJ_INVALID;
        const size_t t_len = end - n.buf;
        tmp = fiobj_hash_get3(o, n.buf, t_len);
        if (tmp != FIOBJ_INVALID) {
          o = tmp;
          n.len -= t_len + (end[0] == '.');
          n.buf = end + (end[0] == '.');
          goto top;
        }
        --end;
      }
    } /* fallthrough */
    default:
      return FIOBJ_INVALID;
    }
  }
}

/* *****************************************************************************
FIOBJ and JSON testing
***************************************************************************** */
#ifdef FIO_TEST_CSTL
FIO_SFUNC int FIO_NAME_TEST(stl, fiobj_task)(fiobj_each_s *e) {
  static size_t index = 0;
  if (!e) {
    index = 0;
    return -1;
  }
  int *expect = (int *)e->udata;
  if (expect[index] == -1) {
    FIO_ASSERT(FIOBJ_TYPE(e->obj[0]) == FIOBJ_T_ARRAY,
               "each2 ordering issue [%zu] (array).",
               index);
    FIO_ASSERT(e->items_at_index == 1,
               "each2 items_at_index value error issue [%zu] (array).",
               index);
  } else {
    FIO_ASSERT(FIO_NAME2(fiobj, i)(e->obj[0]) == expect[index],
               "each2 ordering issue [%zu] (number) %ld != %d",
               index,
               FIO_NAME2(fiobj, i)(e->obj[0]),
               expect[index]);
  }
  ++index;
  return 0;
}

FIO_SFUNC void FIO_NAME_TEST(stl, fiobj)(void) {
  FIOBJ o = FIOBJ_INVALID;
  if (!FIOBJ_MARK_MEMORY_ENABLED) {
    FIO_LOG_WARNING("FIOBJ defined without allocation counter. "
                    "Tests might not be complete.");
  }
  /* primitives - (in)sanity */
  {
    fprintf(stderr, "* Testing FIOBJ primitives.\n");
    FIO_ASSERT(FIOBJ_TYPE(o) == FIOBJ_T_NULL,
               "invalid FIOBJ type should be FIOBJ_T_NULL.");
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(o, FIO_NAME(fiobj, FIOBJ___NAME_NULL)()),
               "invalid FIOBJ is NOT a fiobj_null().");
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_TRUE)(),
                                       FIO_NAME(fiobj, FIOBJ___NAME_NULL)()),
               "fiobj_true() is NOT fiobj_null().");
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_FALSE)(),
                                       FIO_NAME(fiobj, FIOBJ___NAME_NULL)()),
               "fiobj_false() is NOT fiobj_null().");
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_FALSE)(),
                                       FIO_NAME(fiobj, FIOBJ___NAME_TRUE)()),
               "fiobj_false() is NOT fiobj_true().");
    FIO_ASSERT(FIOBJ_TYPE(FIO_NAME(fiobj, FIOBJ___NAME_NULL)()) == FIOBJ_T_NULL,
               "fiobj_null() type should be FIOBJ_T_NULL.");
    FIO_ASSERT(FIOBJ_TYPE(FIO_NAME(fiobj, FIOBJ___NAME_TRUE)()) == FIOBJ_T_TRUE,
               "fiobj_true() type should be FIOBJ_T_TRUE.");
    FIO_ASSERT(FIOBJ_TYPE(FIO_NAME(fiobj, FIOBJ___NAME_FALSE)()) ==
                   FIOBJ_T_FALSE,
               "fiobj_false() type should be FIOBJ_T_FALSE.");
    FIO_ASSERT(FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_NULL)(),
                                      FIO_NAME(fiobj, FIOBJ___NAME_NULL)()),
               "fiobj_null() should be equal to self.");
    FIO_ASSERT(FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_TRUE)(),
                                      FIO_NAME(fiobj, FIOBJ___NAME_TRUE)()),
               "fiobj_true() should be equal to self.");
    FIO_ASSERT(FIO_NAME_BL(fiobj, eq)(FIO_NAME(fiobj, FIOBJ___NAME_FALSE)(),
                                      FIO_NAME(fiobj, FIOBJ___NAME_FALSE)()),
               "fiobj_false() should be equal to self.");
  }
  {
    fprintf(stderr, "* Testing FIOBJ integers.\n");
    uint8_t allocation_flags = 0;
    for (uint8_t bit = 0; bit < (sizeof(intptr_t) * 8); ++bit) {
      uintptr_t i = (uintptr_t)1 << bit;
      o = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)((intptr_t)i);
      FIO_ASSERT(FIO_NAME2(fiobj, i)(o) == (intptr_t)i,
                 "Number not reversible at bit %d (%zd != %zd)!",
                 (int)bit,
                 (ssize_t)FIO_NAME2(fiobj, i)(o),
                 (ssize_t)i);
      allocation_flags |= (FIOBJ_TYPE_CLASS(o) == FIOBJ_T_NUMBER) ? 1 : 2;
      fiobj_free(o);
    }
    FIO_ASSERT(allocation_flags == 3,
               "no bits are allocated / no allocations optimized away (%d)",
               (int)allocation_flags);
  }
  {
    fprintf(stderr, "* Testing FIOBJ floats.\n");
    uint8_t allocation_flags = 0;
    for (uint8_t bit = 0; bit < (sizeof(double) * 8); ++bit) {
      union {
        double d;
        uint64_t i;
      } punned;
      punned.i = (uint64_t)1 << bit;
      o = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), new)(punned.d);
      FIO_ASSERT(FIO_NAME2(fiobj, f)(o) == punned.d,
                 "Float not reversible at bit %d (%lf != %lf)!",
                 (int)bit,
                 FIO_NAME2(fiobj, f)(o),
                 punned.d);
      allocation_flags |= (FIOBJ_TYPE_CLASS(o) == FIOBJ_T_FLOAT) ? 1 : 2;
      fiobj_free(o);
    }
    FIO_ASSERT(allocation_flags == 3,
               "no bits are allocated / no allocations optimized away (%d)",
               (int)allocation_flags);
  }
  {
    fprintf(stderr, "* Testing FIOBJ each2.\n");
    FIOBJ a = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    o = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(o, a);
    for (int i = 1; i < 10; ++i) // 1, 2, 3 ... 10
    {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(i));
      if (i % 3 == 0) {
        a = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(o, a);
      }
    }
    int expectation[] =
        {-1 /* array */, -1, 1, 2, 3, -1, 4, 5, 6, -1, 7, 8, 9, -1};
    size_t c =
        fiobj_each2(o, FIO_NAME_TEST(stl, fiobj_task), (void *)expectation);
    FIO_ASSERT(c == FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), count)(o) +
                        9 + 1,
               "each2 repetition count error");
    fiobj_free(o);
    FIO_NAME_TEST(stl, fiobj_task)(NULL);
  }
  {
    fprintf(stderr, "* Testing FIOBJ JSON handling.\n");
    char json[] =
        "                    "
        "\n# comment 1"
        "\n// comment 2"
        "\n/* comment 3 */"
        "{\"true\":true,\"false\":false,\"null\":null,\"array\":[1,2,3,4.2,"
        "\"five\"],"
        "\"string\":\"hello\\tjson\\bworld!\\r\\n\",\"hash\":{\"true\":true,"
        "\"false\":false},\"array2\":[1,2,3,4.2,\"five\",{\"hash\":true},[{"
        "\"hash\":{\"true\":true}}]]}";
    o = fiobj_json_parse2(json, strlen(json), NULL);
    FIO_ASSERT(o, "JSON parsing failed - no data returned.");
    FIO_ASSERT(fiobj_json_find2(o, (char *)"array2[6][0].hash.true", 22) ==
                   fiobj_true(),
               "fiobj_json_find2 failed");
    FIOBJ j = FIO_NAME2(fiobj, json)(FIOBJ_INVALID, o, 0);
#ifdef DEBUG
    fprintf(stderr, "JSON: %s\n", FIO_NAME2(fiobj, cstr)(j).buf);
#endif
    FIO_ASSERT(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(j) ==
                   strlen(json + 61),
               "JSON roundtrip failed (length error).");
    FIO_ASSERT(!memcmp(json + 61,
                       FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(j),
                       strlen(json + 61)),
               "JSON roundtrip failed (data error).");
    fiobj_free(o);
    fiobj_free(j);
    o = FIOBJ_INVALID;
  }
  {
    fprintf(stderr, "* Testing FIOBJ array equality test (fiobj_is_eq).\n");
    FIOBJ a1 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIOBJ a2 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIOBJ n1 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIOBJ n2 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a1, fiobj_null());
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a2, fiobj_null());
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(n1, fiobj_true());
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(n2, fiobj_true());
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a1, n1);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a2, n2);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
    (a1, FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new_cstr)("test", 4));
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
    (a2, FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new_cstr)("test", 4));
    FIO_ASSERT(FIO_NAME_BL(fiobj, eq)(a1, a2), "equal arrays aren't equal?");
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(n1, fiobj_null());
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(n2, fiobj_false());
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(a1, a2), "unequal arrays are equal?");
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), remove)(n1, -1, NULL);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), remove)(n2, -1, NULL);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), remove)(a1, 0, NULL);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), remove)(a2, -1, NULL);
    FIO_ASSERT(!FIO_NAME_BL(fiobj, eq)(a1, a2), "unequal arrays are equal?");
    fiobj_free(a1);
    fiobj_free(a2);
  }
  {
    fprintf(stderr, "* Testing FIOBJ array ownership.\n");
    FIOBJ a = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    for (int i = 1; i <= TEST_REPEAT; ++i) {
      FIOBJ tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                           new_cstr)("number: ", 8);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_i)(tmp, i);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a, tmp);
    }
    FIOBJ shifted = FIOBJ_INVALID;
    FIOBJ popped = FIOBJ_INVALID;
    FIOBJ removed = FIOBJ_INVALID;
    FIOBJ set = FIOBJ_INVALID;
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), shift)(a, &shifted);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), pop)(a, &popped);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), set)
    (a, 1, FIO_NAME(fiobj, FIOBJ___NAME_TRUE)(), &set);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), remove)(a, 2, &removed);
    fiobj_free(a);
    if (1) {
      FIO_ASSERT(
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(popped) ==
                  strlen("number: " FIO_MACRO2STR(TEST_REPEAT)) &&
              !memcmp(
                  "number: " FIO_MACRO2STR(TEST_REPEAT),
                  FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(popped),
                  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(popped)),
          "Object popped from Array lost it's value %s",
          FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(popped));
      FIO_ASSERT(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(shifted) ==
                         9 &&
                     !memcmp("number: 1",
                             FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                                       ptr)(shifted),
                             9),
                 "Object shifted from Array lost it's value %s",
                 FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(shifted));
      FIO_ASSERT(
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(set) == 9 &&
              !memcmp("number: 3",
                      FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(set),
                      9),
          "Object retrieved from Array using fiobj_array_set() lost it's "
          "value %s",
          FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(set));
      FIO_ASSERT(
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(removed) == 9 &&
              !memcmp(
                  "number: 4",
                  FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(removed),
                  9),
          "Object retrieved from Array using fiobj_array_set() lost it's "
          "value %s",
          FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(removed));
    }
    fiobj_free(shifted);
    fiobj_free(popped);
    fiobj_free(set);
    fiobj_free(removed);
  }
  {
    fprintf(stderr, "* Testing FIOBJ array ownership after concat.\n");
    FIOBJ a1, a2;
    a1 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    a2 = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    for (int i = 0; i < TEST_REPEAT; ++i) {
      FIOBJ str = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_i)(str, i);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a1, str);
    }
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), concat)(a2, a1);
    fiobj_free(a1);
    for (int i = 0; i < TEST_REPEAT; ++i) {
      FIOBJ_STR_TEMP_VAR(tmp);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_i)(tmp, i);
      FIO_ASSERT(
          FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(
              FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), get)(a2, i)) ==
              FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(tmp),
          "string length zeroed out - string freed?");
      FIO_ASSERT(
          !memcmp(
              FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(tmp),
              FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(
                  FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), get)(a2, i)),
              FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(tmp)),
          "string data error - string freed?");
      FIOBJ_STR_TEMP_DESTROY(tmp);
    }
    fiobj_free(a2);
  }
  {
    fprintf(stderr, "* Testing FIOBJ hash ownership.\n");
    o = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), new)();
    for (int i = 1; i <= TEST_REPEAT; ++i) {
      FIOBJ tmp = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING),
                           new_cstr)("number: ", 8);
      FIOBJ k = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(i);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_i)(tmp, i);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set2)(o, k, tmp);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set_if_missing2)
      (o, k, fiobj_dup(tmp));
      fiobj_free(k);
    }

    FIOBJ set = FIOBJ_INVALID;
    FIOBJ removed = FIOBJ_INVALID;
    FIOBJ k = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(1);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), remove2)(o, k, &removed);
    fiobj_free(k);
    k = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)(2);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set)
    (o, fiobj2hash(o, k), k, FIO_NAME(fiobj, FIOBJ___NAME_TRUE)(), &set);
    fiobj_free(k);
    FIO_ASSERT(set, "fiobj_hash_set2 didn't copy information to old pointer?");
    FIO_ASSERT(removed,
               "fiobj_hash_remove2 didn't copy information to old pointer?");
    // fiobj_hash_set(o, uintptr_t hash, FIOBJ key, FIOBJ value, FIOBJ *old)
    FIO_ASSERT(
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(removed) ==
                strlen("number: 1") &&
            !memcmp(
                "number: 1",
                FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(removed),
                FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(removed)),
        "Object removed from Hash lost it's value %s",
        FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(removed));
    FIO_ASSERT(
        FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(set) ==
                strlen("number: 2") &&
            !memcmp("number: 2",
                    FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(set),
                    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(set)),
        "Object removed from Hash lost it's value %s",
        FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(set));

    fiobj_free(removed);
    fiobj_free(set);
    fiobj_free(o);
  }

#if FIOBJ_MARK_MEMORY
  {
    fprintf(stderr, "* Testing FIOBJ for memory leaks.\n");
    FIOBJ a = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), new)();
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), reserve)(a, 64);
    for (uint8_t bit = 0; bit < (sizeof(intptr_t) * 8); ++bit) {
      uintptr_t i = (uintptr_t)1 << bit;
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_NUMBER), new)((intptr_t)i));
    }
    FIOBJ h = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), new)();
    FIOBJ key = FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), new)();
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(key, "array", 5);
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), set2)(h, key, a);
    FIO_ASSERT(FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_HASH), get2)(h, key) == a,
               "FIOBJ Hash retrival failed");
    FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)(a, key);
    if (0) {
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(fiobj, FIOBJ___NAME_NULL)());
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(fiobj, FIOBJ___NAME_TRUE)());
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(fiobj, FIOBJ___NAME_FALSE)());
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_ARRAY), push)
      (a, FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_FLOAT), new)(0.42));

      FIOBJ json = FIO_NAME2(fiobj, json)(FIOBJ_INVALID, h, 0);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write)(json, "\n", 1);
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), reserve)
      (json,
       FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(json)
           << 1); /* prevent memory realloc */
      FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), write_escape)
      (json,
       FIO_NAME2(FIO_NAME(fiobj, FIOBJ___NAME_STRING), ptr)(json),
       FIO_NAME(FIO_NAME(fiobj, FIOBJ___NAME_STRING), len)(json) - 1);
      fprintf(stderr, "%s\n", FIO_NAME2(fiobj, cstr)(json).buf);
      fiobj_free(json);
    }
    fiobj_free(h);

    FIO_ASSERT(FIOBJ_MARK_MEMORY_ALLOC_COUNTER ==
                   FIOBJ_MARK_MEMORY_FREE_COUNTER,
               "FIOBJ leak detected (freed %zu/%zu)",
               FIOBJ_MARK_MEMORY_FREE_COUNTER,
               FIOBJ_MARK_MEMORY_ALLOC_COUNTER);
  }
#endif
  fprintf(stderr, "* Passed.\n");
}
#endif /* FIO_TEST_CSTL */
/* *****************************************************************************
FIOBJ cleanup
***************************************************************************** */

#endif /* FIOBJ_EXTERN_COMPLETE */
#undef FIOBJ_FUNC
#undef FIOBJ_IFUNC
#undef FIOBJ_EXTERN
#undef FIOBJ_EXTERN_COMPLETE
#undef FIOBJ_EXTERN_OBJ
#undef FIOBJ_EXTERN_OBJ_IMP
#endif /* FIO_FIOBJ */
#undef FIO_FIOBJ
/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */
#ifndef H___FIO_CSTL_INCLUDE_ONCE_H /* Development inclusion - ignore line */
#include "000 header.h"             /* Development inclusion - ignore line */
#endif                              /* Development inclusion - ignore line */
/* *****************************************************************************



                                Testing



***************************************************************************** */

#if !defined(FIO_FIO_TEST_CSTL_ONLY_ONCE) && (defined(FIO_TEST_CSTL))
#define FIO_FIO_TEST_CSTL_ONLY_ONCE 1

#ifdef FIO_EXTERN_TEST
void fio_test_dynamic_types(void);
#else
FIO_SFUNC void fio_test_dynamic_types(void);
#endif
#if !defined(FIO_EXTERN_TEST) || defined(FIO_EXTERN_COMPLETE)

/* Common testing values / Macros */
#define TEST_REPEAT 4096

/* Make sure logging and memory leak counters are set. */
#define FIO_LOG
#ifndef FIO_LEAK_COUNTER
#define FIO_LEAK_COUNTER 1
#endif
#ifndef FIO_FIOBJ
#define FIO_FIOBJ
#endif
#ifndef FIOBJ_MALLOC
#define FIOBJ_MALLOC /* define to test with custom allocator */
#endif
#define FIO_TIME
#include __FILE__

/* Add non-type options to minimize `#include` instructions */
#define FIO_ATOL
#define FIO_ATOMIC
#define FIO_BITMAP
#define FIO_BITWISE
#define FIO_CLI
#define FIO_GLOB_MATCH
#define FIO_POLL
#define FIO_QUEUE
#define FIO_RAND
#define FIO_RISKY_HASH
#define FIO_SHA1
#define FIO_SIGNAL
#define FIO_SOCK
#define FIO_STREAM
#define FIO_TIME
#define FIO_URL
#define FIO_THREADS

// #define FIO_LOCK2 /* a signal based blocking lock is WIP */

#include __FILE__

FIO_SFUNC uintptr_t fio___dynamic_types_test_tag(uintptr_t i) { return i | 1; }
FIO_SFUNC uintptr_t fio___dynamic_types_test_untag(uintptr_t i) {
  return i & (~((uintptr_t)1UL));
}

/* *****************************************************************************
Dynamically Produced Test Types
***************************************************************************** */

static int ary____test_was_destroyed = 0;
#define FIO_ARRAY_NAME    ary____test
#define FIO_ARRAY_TYPE    int
#define FIO_REF_NAME      ary____test
#define FIO_REF_INIT(obj) obj = (ary____test_s)FIO_ARRAY_INIT
#define FIO_REF_DESTROY(obj)                                                   \
  do {                                                                         \
    ary____test_destroy(&obj);                                                 \
    ary____test_was_destroyed = 1;                                             \
  } while (0)
#define FIO_PTR_TAG(p)   fio___dynamic_types_test_tag(((uintptr_t)p))
#define FIO_PTR_UNTAG(p) fio___dynamic_types_test_untag(((uintptr_t)p))
#include __FILE__

#define FIO_ARRAY_NAME                 ary2____test
#define FIO_ARRAY_TYPE                 uint8_t
#define FIO_ARRAY_TYPE_INVALID         0xFF
#define FIO_ARRAY_TYPE_COPY(dest, src) (dest) = (src)
#define FIO_ARRAY_TYPE_DESTROY(obj)    (obj = FIO_ARRAY_TYPE_INVALID)
#define FIO_ARRAY_TYPE_CMP(a, b)       (a) == (b)
#define FIO_PTR_TAG(p)                 fio___dynamic_types_test_tag(((uintptr_t)p))
#define FIO_PTR_UNTAG(p)               fio___dynamic_types_test_untag(((uintptr_t)p))
#include __FILE__

/* test all defaults */
#define FIO_ARRAY_NAME ary3____test
#include __FILE__

#define FIO_UMAP_NAME     __umap_test__size_t
#define FIO_MAP_TYPE      size_t
#define FIO_MAP_EVICT_LRU 0
#define FIO_MAP_TEST
#include __FILE__
#define FIO_UMAP_NAME     __umap_test__size_lru
#define FIO_MAP_TYPE      size_t
#define FIO_MAP_KEY       size_t
#define FIO_MAP_EVICT_LRU 1
#define FIO_MAP_TEST
#include __FILE__
#define FIO_OMAP_NAME     __omap_test__size_t
#define FIO_MAP_TYPE      size_t
#define FIO_MAP_EVICT_LRU 0
#define FIO_MAP_TEST
#include __FILE__
#define FIO_OMAP_NAME     __omap_test__size_lru
#define FIO_MAP_TYPE      size_t
#define FIO_MAP_KEY       size_t
#define FIO_MAP_EVICT_LRU 1
#define FIO_MAP_TEST
#include __FILE__

#define FIO_STR_NAME fio_big_str
#define FIO_STR_WRITE_TEST_FUNC
#include __FILE__

#define FIO_STR_SMALL fio_small_str
#define FIO_STR_WRITE_TEST_FUNC
#include __FILE__

#define FIO_MEMORY_NAME                   fio_mem_test_safe
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 1
#undef FIO_MEMORY_USE_THREAD_MUTEX
#define FIO_MEMORY_USE_THREAD_MUTEX 0
#define FIO_MEMORY_ARENA_COUNT      4
#include __FILE__

#define FIO_MEMORY_NAME                   fio_mem_test_unsafe
#define FIO_MEMORY_INITIALIZE_ALLOCATIONS 0
#undef FIO_MEMORY_USE_THREAD_MUTEX
#define FIO_MEMORY_USE_THREAD_MUTEX 0
#define FIO_MEMORY_ARENA_COUNT      4
#include __FILE__

#define FIO_FIOBJ
#include __FILE__

/* *****************************************************************************
Linked List - Test
***************************************************************************** */

typedef struct {
  int data;
  FIO_LIST_NODE node;
} ls____test_s;

#define FIO_LIST_NAME    ls____test
#define FIO_PTR_TAG(p)   fio___dynamic_types_test_tag(((uintptr_t)p))
#define FIO_PTR_UNTAG(p) fio___dynamic_types_test_untag(((uintptr_t)p))

#include __FILE__

FIO_SFUNC void fio___dynamic_types_test___linked_list_test(void) {
  fprintf(stderr, "* Testing linked lists.\n");
  FIO_LIST_HEAD ls = FIO_LIST_INIT(ls);
  for (int i = 0; i < TEST_REPEAT; ++i) {
    ls____test_s *node = ls____test_push(
        &ls,
        (ls____test_s *)FIO_MEM_REALLOC(NULL, 0, sizeof(*node), 0));
    node->data = i;
  }
  int tester = 0;
  FIO_LIST_EACH(ls____test_s, node, &ls, pos) {
    FIO_ASSERT(pos->data == tester++,
               "Linked list ordering error for push or each");
    FIO_ASSERT(ls____test_root(&pos->node) == pos,
               "Linked List root offset error");
  }
  FIO_ASSERT(tester == TEST_REPEAT,
             "linked list EACH didn't loop through all the list");
  while (ls____test_any(&ls)) {
    ls____test_s *node = ls____test_pop(&ls);
    node = (ls____test_s *)fio___dynamic_types_test_untag((uintptr_t)(node));
    FIO_ASSERT(node, "Linked list pop or any failed");
    FIO_ASSERT(node->data == --tester, "Linked list ordering error for pop");
    FIO_MEM_FREE(node, sizeof(*node));
  }
  tester = TEST_REPEAT;
  for (int i = 0; i < TEST_REPEAT; ++i) {
    ls____test_s *node = ls____test_unshift(
        &ls,
        (ls____test_s *)FIO_MEM_REALLOC(NULL, 0, sizeof(*node), 0));
    node->data = i;
  }
  FIO_LIST_EACH(ls____test_s, node, &ls, pos) {
    FIO_ASSERT(pos->data == --tester,
               "Linked list ordering error for unshift or each");
  }
  FIO_ASSERT(tester == 0,
             "linked list EACH didn't loop through all the list after unshift");
  tester = TEST_REPEAT;
  while (ls____test_any(&ls)) {
    ls____test_s *node = ls____test_shift(&ls);
    node = (ls____test_s *)fio___dynamic_types_test_untag((uintptr_t)(node));
    FIO_ASSERT(node, "Linked list pop or any failed");
    FIO_ASSERT(node->data == --tester, "Linked list ordering error for shift");
    FIO_MEM_FREE(node, sizeof(*node));
  }
  FIO_ASSERT(FIO_NAME_BL(ls____test, empty)(&ls),
             "Linked list empty should have been true");
  for (int i = 0; i < TEST_REPEAT; ++i) {
    ls____test_s *node = ls____test_push(
        &ls,
        (ls____test_s *)FIO_MEM_REALLOC(NULL, 0, sizeof(*node), 0));
    node->data = i;
  }
  FIO_LIST_EACH(ls____test_s, node, &ls, pos) {
    ls____test_remove(pos);
    pos = (ls____test_s *)fio___dynamic_types_test_untag((uintptr_t)(pos));
    FIO_MEM_FREE(pos, sizeof(*pos));
  }
  FIO_ASSERT(FIO_NAME_BL(ls____test, empty)(&ls),
             "Linked list empty should have been true");
}

FIO_SFUNC void fio___dynamic_types_test___index_list_test(void) {
  fprintf(stderr, "* Testing indexed lists.\n");
  struct {
    size_t i;
    struct {
      uint16_t next;
      uint16_t prev;
    } node;
  } data[16];
  size_t count;
  const size_t len = 16;
  for (size_t i = 0; i < len; ++i) {
    data[i].i = i;
    if (!i)
      data[i].node.prev = data[i].node.next = i;
    else
      FIO_INDEXED_LIST_PUSH(data, node, 0, i);
  }
  count = 0;
  FIO_INDEXED_LIST_EACH(data, node, 0, i) {
    FIO_ASSERT(data[i].i == count,
               "indexed list order issue? %zu != %zu",
               data[i].i != i);
    ++count;
  }
  FIO_ASSERT(count == 16, "indexed list each failed? (%zu != %zu)", count, len);
  count = 0;
  while (data[0].node.next != 0 && count < 32) {
    ++count;
    uint16_t n = data[0].node.prev;
    FIO_INDEXED_LIST_REMOVE(data, node, n);
  }
  FIO_ASSERT(count == 15,
             "indexed list remove failed? (%zu != %zu)",
             count,
             len);
  for (size_t i = 0; i < len; ++i) {
    data[i].i = i;
    if (!i)
      data[i].node.prev = data[i].node.next = i;
    else {
      FIO_INDEXED_LIST_PUSH(data, node, 0, i);
      FIO_INDEXED_LIST_REMOVE(data, node, i);
      FIO_INDEXED_LIST_PUSH(data, node, 0, i);
    }
  }
  count = 0;
  FIO_INDEXED_LIST_EACH(data, node, 0, i) {
    FIO_ASSERT(data[i].i == count,
               "indexed list order issue (push-pop-push? %zu != %zu",
               data[i].i != count);
    ++count;
  }
}

/* *****************************************************************************
Hash Map / Set - test
***************************************************************************** */

/* a simple set of numbers */
#define FIO_MAP_NAME           set_____test
#define FIO_MAP_TYPE           size_t
#define FIO_MAP_TYPE_CMP(a, b) ((a) == (b))
#define FIO_PTR_TAG(p)         fio___dynamic_types_test_tag(((uintptr_t)p))
#define FIO_PTR_UNTAG(p)       fio___dynamic_types_test_untag(((uintptr_t)p))
#include __FILE__

/* a simple set of numbers */
#define FIO_MAP_NAME           set2_____test
#define FIO_MAP_TYPE           size_t
#define FIO_MAP_TYPE_CMP(a, b) 1
#define FIO_PTR_TAG(p)         fio___dynamic_types_test_tag(((uintptr_t)p))
#define FIO_PTR_UNTAG(p)       fio___dynamic_types_test_untag(((uintptr_t)p))
#include __FILE__

FIO_SFUNC size_t map_____test_key_copy_counter = 0;
FIO_SFUNC void map_____test_key_copy(char **dest, char *src) {
  *dest =
      (char *)FIO_MEM_REALLOC(NULL, 0, (strlen(src) + 1) * sizeof(*dest), 0);
  FIO_ASSERT(*dest, "no memory to allocate key in map_test")
  strcpy(*dest, src);
  ++map_____test_key_copy_counter;
}
FIO_SFUNC void map_____test_key_destroy(char **dest) {
  FIO_MEM_FREE(*dest, strlen(*dest) + 1);
  *dest = NULL;
  --map_____test_key_copy_counter;
}

/* keys are strings, values are numbers */
#define FIO_MAP_KEY            char *
#define FIO_MAP_KEY_CMP(a, b)  (strcmp((a), (b)) == 0)
#define FIO_MAP_KEY_COPY(a, b) map_____test_key_copy(&(a), (b))
#define FIO_MAP_KEY_DESTROY(a) map_____test_key_destroy(&(a))
#define FIO_MAP_TYPE           size_t
#define FIO_MAP_NAME           map_____test
#include __FILE__

#define HASHOFi(i) i /* fio_risky_hash(&(i), sizeof((i)), 0) */
#define HASHOFs(s) fio_risky_hash(s, strlen((s)), 0)

FIO_SFUNC int set_____test_each_task(set_____test_each_s *e) {
  uintptr_t *i_p = (uintptr_t *)e->udata;
  FIO_ASSERT(e->items_at_index == 1, "set_each items_at_index is not 1!");
  FIO_ASSERT(e->value == ++(*i_p), "set_each started at a bad offset!");

  return 0;
}

FIO_SFUNC void fio___dynamic_types_test___map_test(void) {
  {
    set_____test_s m = FIO_MAP_INIT;
    fprintf(stderr, "* Testing dynamic hash / set maps.\n");

    fprintf(stderr, "* Testing set (hash map where value == key).\n");
    FIO_ASSERT(set_____test_count(&m) == 0,
               "freshly initialized map should have no objects");
    FIO_ASSERT(set_____test_capa(&m) == 0,
               "freshly initialized map should have no capacity");
    FIO_ASSERT(set_____test_reserve(&m, (TEST_REPEAT >> 1)) >=
                   (TEST_REPEAT >> 1),
               "reserve should increase capacity.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      set_____test_set_if_missing(&m, HASHOFi(i), i + 1);
    }
    {
      uintptr_t pos_test = (TEST_REPEAT >> 1);
      size_t count =
          set_____test_each(&m, set_____test_each_task, &pos_test, pos_test);
      FIO_ASSERT(count == set_____test_count(&m),
                 "set_each task returned the wrong counter.");
      FIO_ASSERT(count == pos_test, "set_each position testing error");
    }

    FIO_ASSERT(set_____test_count(&m) == TEST_REPEAT,
               "After inserting %zu items to set, got %zu items",
               (size_t)TEST_REPEAT,
               (size_t)set_____test_count(&m));
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 1) == i + 1,
                 "item retrival error in set (%zu != %zu).",
                 set_____test_get(&m, HASHOFi(i), i + 1),
                 i + 1);
    }
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 2) == 0,
                 "item retrival error in set - object comparisson error?");
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      set_____test_set_if_missing(&m, HASHOFi(i), i + 1);
    }
    {
      size_t i = 0;
      FIO_MAP_EACH(set_____test, &m, pos) {
        FIO_ASSERT(pos->obj == pos->hash + 1 || !i,
                   "FIO_MAP_EACH loop out of order?")
        ++i;
      }
      FIO_ASSERT(i == set_____test_count(&m), "FIO_MAP_EACH loop incomplete?")
    }
    FIO_ASSERT(set_____test_count(&m) == TEST_REPEAT,
               "Inserting existing object should keep existing object.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 1) == i + 1,
                 "item retrival error in set - insert failed to update?");
      FIO_ASSERT(set_____test_get_ptr(&m, HASHOFi(i), i + 1) &&
                     set_____test_get_ptr(&m, HASHOFi(i), i + 1)[0] == i + 1,
                 "pointer retrival error in set.");
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      size_t old = 5;
      set_____test_set(&m, HASHOFi(i), i + 2, &old);
      FIO_ASSERT(old == 0,
                 "old pointer not initialized with old (or missing) data");
    }

    FIO_ASSERT(set_____test_count(&m) == (TEST_REPEAT * 2),
               "full hash collision shoudn't break map until attack limit.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 2) == i + 2,
                 "item retrival error in set - overwrite failed to update?");
    }
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 1) == i + 1,
                 "item retrival error in set - collision resolution error?");
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      size_t old = 5;
      set_____test_remove(&m, HASHOFi(i), i + 1, &old);
      FIO_ASSERT(old == i + 1,
                 "removed item not initialized with old (or missing) data");
    }
    FIO_ASSERT(set_____test_count(&m) == TEST_REPEAT,
               "removal should update object count.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 1) == 0,
                 "removed items should be unavailable");
    }
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set_____test_get(&m, HASHOFi(i), i + 2) == i + 2,
                 "previous items should be accessible after removal");
    }
    set_____test_destroy(&m);
  }
  {
    set2_____test_s m = FIO_MAP_INIT;
    fprintf(stderr, "* Testing set map without value comparison.\n");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      set2_____test_set_if_missing(&m, HASHOFi(i), i + 1);
    }

    FIO_ASSERT(set2_____test_count(&m) == TEST_REPEAT,
               "After inserting %zu items to set, got %zu items",
               (size_t)TEST_REPEAT,
               (size_t)set2_____test_count(&m));
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set2_____test_get(&m, HASHOFi(i), 0) == i + 1,
                 "item retrival error in set (%zu != %zu).",
                 set2_____test_get(&m, HASHOFi(i), 0),
                 i + 1);
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      set2_____test_set_if_missing(&m, HASHOFi(i), i + 2);
    }
    FIO_ASSERT(set2_____test_count(&m) == TEST_REPEAT,
               "Inserting existing object should keep existing object.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set2_____test_get(&m, HASHOFi(i), 0) == i + 1,
                 "item retrival error in set - insert failed to update?");
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      size_t old = 5;
      set2_____test_set(&m, HASHOFi(i), i + 2, &old);
      FIO_ASSERT(old == i + 1,
                 "old pointer not initialized with old (or missing) data");
    }

    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set2_____test_get(&m, HASHOFi(i), 0) == i + 2,
                 "item retrival error in set - overwrite failed to update?");
    }
    {
      /* test partial removal */
      for (size_t i = 1; i < TEST_REPEAT; i += 2) {
        size_t old = 5;
        set2_____test_remove(&m, HASHOFi(i), 0, &old);
        FIO_ASSERT(old == i + 2,
                   "removed item not initialized with old (or missing) data "
                   "(%zu != %zu)",
                   old,
                   i + 2);
      }
      for (size_t i = 1; i < TEST_REPEAT; i += 2) {
        FIO_ASSERT(set2_____test_get(&m, HASHOFi(i), 0) == 0,
                   "previous items should NOT be accessible after removal");
        set2_____test_set_if_missing(&m, HASHOFi(i), i + 2);
      }
    }
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      size_t old = 5;
      set2_____test_remove(&m, HASHOFi(i), 0, &old);
      FIO_ASSERT(old == i + 2,
                 "removed item not initialized with old (or missing) data "
                 "(%zu != %zu)",
                 old,
                 i + 2);
    }
    FIO_ASSERT(set2_____test_count(&m) == 0,
               "removal should update object count.");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      FIO_ASSERT(set2_____test_get(&m, HASHOFi(i), 0) == 0,
                 "previous items should NOT be accessible after removal");
    }
    set2_____test_destroy(&m);
  }

  {
    map_____test_s *m = map_____test_new();
    fprintf(stderr, "* Testing hash map.\n");
    FIO_ASSERT(map_____test_count(m) == 0,
               "freshly initialized map should have no objects");
    FIO_ASSERT(map_____test_capa(m) == 0,
               "freshly initialized map should have no capacity");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      char buffer[64];
      int l = snprintf(buffer, 63, "%zu", i);
      buffer[l] = 0;
      map_____test_set(m, HASHOFs(buffer), buffer, i + 1, NULL);
    }
    FIO_ASSERT(map_____test_key_copy_counter == TEST_REPEAT,
               "key copying error - was the key copied?");
    FIO_ASSERT(map_____test_count(m) == TEST_REPEAT,
               "After inserting %zu items to map, got %zu items",
               (size_t)TEST_REPEAT,
               (size_t)map_____test_count(m));
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      char buffer[64];
      int l = snprintf(buffer + 1, 61, "%zu", i);
      buffer[l + 1] = 0;
      FIO_ASSERT(map_____test_get(m, HASHOFs(buffer + 1), buffer + 1) == i + 1,
                 "item retrival error in map.");
      FIO_ASSERT(map_____test_get_ptr(m, HASHOFs(buffer + 1), buffer + 1) &&
                     map_____test_get_ptr(m,
                                          HASHOFs(buffer + 1),
                                          buffer + 1)[0] == i + 1,
                 "pointer retrival error in map.");
    }
    map_____test_free(m);
    FIO_ASSERT(map_____test_key_copy_counter == 0,
               "key destruction error - was the key freed?");
  }
  {
    set_____test_s s = FIO_MAP_INIT;
    map_____test_s m = FIO_MAP_INIT;
    fprintf(stderr, "* Testing attack resistance (SHOULD print warnings).\n");
    for (size_t i = 0; i < TEST_REPEAT; ++i) {
      char buf[64];
      fio_ltoa(buf, i, 16);
      set_____test_set(&s, 1, i + 1, NULL);
      map_____test_set(&m, 1, buf, i + 1, NULL);
    }
    FIO_ASSERT(set_____test_count(&s) != TEST_REPEAT,
               "full collision protection failed (set)?");
    FIO_ASSERT(map_____test_count(&m) != TEST_REPEAT,
               "full collision protection failed (map)?");
    FIO_ASSERT(set_____test_count(&s) != 1,
               "full collision test failed to push elements (set)?");
    FIO_ASSERT(map_____test_count(&m) != 1,
               "full collision test failed to push elements (map)?");
    set_____test_destroy(&s);
    map_____test_destroy(&m);
  }
}

#undef HASHOFi
#undef HASHOFs

/* *****************************************************************************
Environment printout
***************************************************************************** */

#define FIO_PRINT_SIZE_OF(T)                                                   \
  fprintf(stderr, "\t%-17s%zu Bytes\n", #T, sizeof(T))

FIO_SFUNC void FIO_NAME_TEST(stl, type_sizes)(void) {
  switch (sizeof(void *)) {
  case 2:
    fprintf(stderr, "* 16bit words size (unexpected, unknown effects).\n");
    break;
  case 4:
    fprintf(stderr, "* 32bit words size (some features might be slower).\n");
    break;
  case 8:
    fprintf(stderr, "* 64bit words size okay.\n");
    break;
  case 16:
    fprintf(stderr, "* 128bit words size... wow!\n");
    break;
  default:
    fprintf(stderr, "* Unknown words size %zubit!\n", sizeof(void *) << 3);
    break;
  }
  fprintf(stderr, "* Using the following type sizes:\n");
  FIO_PRINT_SIZE_OF(char);
  FIO_PRINT_SIZE_OF(short);
  FIO_PRINT_SIZE_OF(int);
  FIO_PRINT_SIZE_OF(float);
  FIO_PRINT_SIZE_OF(long);
  FIO_PRINT_SIZE_OF(double);
  FIO_PRINT_SIZE_OF(size_t);
  FIO_PRINT_SIZE_OF(void *);
  FIO_PRINT_SIZE_OF(uintmax_t);
  FIO_PRINT_SIZE_OF(long double);
#ifdef __SIZEOF_INT128__
  FIO_PRINT_SIZE_OF(__uint128_t);
#endif
#if FIO_OS_POSIX || defined(_SC_PAGESIZE)
  long page = sysconf(_SC_PAGESIZE);
  if (page > 0) {
    fprintf(stderr, "\t%-17s%ld bytes.\n", "Page", page);
    if (page != (1UL << FIO_MEM_PAGE_SIZE_LOG))
      FIO_LOG_WARNING("unexpected page size != 4096\n          "
                      "facil.io could be recompiled with:\n          "
                      "`CFLAGS=\"-DFIO_MEM_PAGE_SIZE_LOG=%.0lf\"`",
                      log2(page));
  }
#endif /* FIO_OS_POSIX */
}
#undef FIO_PRINT_SIZE_OF

/* *****************************************************************************
Locking - Speed Test
***************************************************************************** */
#define FIO___LOCK2_TEST_TASK    (1LU << 25)
#define FIO___LOCK2_TEST_THREADS 32U
#define FIO___LOCK2_TEST_REPEAT  1

FIO_SFUNC void fio___lock_speedtest_task_inner(void *s) {
  size_t *r = (size_t *)s;
  static size_t i;
  for (i = 0; i < FIO___LOCK2_TEST_TASK; ++i) {
    FIO_COMPILER_GUARD;
    ++r[0];
  }
}

static void *fio___lock_mytask_lock(void *s) {
  static fio_lock_i lock = FIO_LOCK_INIT;
  fio_lock(&lock);
  if (s)
    fio___lock_speedtest_task_inner(s);
  fio_unlock(&lock);
  return NULL;
}

#ifdef H___FIO_LOCK2___H
static void *fio___lock_mytask_lock2(void *s) {
  static fio_lock2_s lock = {FIO_LOCK_INIT};
  fio_lock2(&lock, 1);
  if (s)
    fio___lock_speedtest_task_inner(s);
  fio_unlock2(&lock, 1);
  return NULL;
}
#endif

static void *fio___lock_mytask_mutex(void *s) {
  static fio_thread_mutex_t mutex = FIO_THREAD_MUTEX_INIT;
  fio_thread_mutex_lock(&mutex);
  if (s)
    fio___lock_speedtest_task_inner(s);
  fio_thread_mutex_unlock(&mutex);
  return NULL;
}

FIO_SFUNC void FIO_NAME_TEST(stl, lock_speed)(void) {
  uint64_t start, end;
  fio_thread_t threads[FIO___LOCK2_TEST_THREADS];

  struct {
    size_t type_size;
    const char *type_name;
    const char *name;
    void *(*task)(void *);
  } test_funcs[] = {
      {
          .type_size = sizeof(fio_lock_i),
          .type_name = "fio_lock_i",
          .name = "fio_lock      (spinlock)",
          .task = fio___lock_mytask_lock,
      },
#ifdef H___FIO_LOCK2___H
      {
          .type_size = sizeof(fio_lock2_s),
          .type_name = "fio_lock2_s",
          .name = "fio_lock2 (pause/resume)",
          .task = fio___lock_mytask_lock2,
      },
#endif
      {
          .type_size = sizeof(fio_thread_mutex_t),
          .type_name = "fio_thread_mutex_t",
          .name = "OS threads (pthread_mutex / Windows handle)",
          .task = fio___lock_mytask_mutex,
      },
      {
          .name = NULL,
          .task = NULL,
      },
  };
  fprintf(stderr, "* Speed testing The following types:\n");
  for (size_t fn = 0; test_funcs[fn].name; ++fn) {
    fprintf(stderr,
            "\t%s\t(%zu bytes)\n",
            test_funcs[fn].type_name,
            test_funcs[fn].type_size);
  }
#ifndef H___FIO_LOCK2___H
  FIO_LOG_WARNING("Won't test `fio_lock2` functions (needs `FIO_LOCK2`).");
#endif

  start = fio_time_micro();
  for (size_t i = 0; i < FIO___LOCK2_TEST_TASK; ++i) {
    FIO_COMPILER_GUARD;
  }
  end = fio_time_micro();
  fprintf(stderr,
          "\n* Speed testing locking schemes - no contention, short work (%zu "
          "mms):\n"
          "\t\t(%zu itterations)\n",
          (size_t)(end - start),
          (size_t)FIO___LOCK2_TEST_TASK);

  for (int test_repeat = 0; test_repeat < FIO___LOCK2_TEST_REPEAT;
       ++test_repeat) {
    if (FIO___LOCK2_TEST_REPEAT > 1)
      fprintf(stderr,
              "%s (%d)\n",
              (test_repeat ? "Round" : "Warmup"),
              test_repeat);
    for (size_t fn = 0; test_funcs[fn].name; ++fn) {
      test_funcs[fn].task(NULL); /* warmup */
      start = fio_time_micro();
      for (size_t i = 0; i < FIO___LOCK2_TEST_TASK; ++i) {
        FIO_COMPILER_GUARD;
        test_funcs[fn].task(NULL);
      }
      end = fio_time_micro();
      fprintf(stderr,
              "\t%s: %zu mms\n",
              test_funcs[fn].name,
              (size_t)(end - start));
    }
  }

  fprintf(stderr,
          "\n* Speed testing locking schemes - no contention, long work ");
  start = fio_time_micro();
  for (size_t i = 0; i < FIO___LOCK2_TEST_THREADS; ++i) {
    size_t result = 0;
    FIO_COMPILER_GUARD;
    fio___lock_speedtest_task_inner(&result);
  }
  end = fio_time_micro();
  fprintf(stderr, " %zu mms\n", (size_t)(end - start));
  clock_t long_work = end - start;
  fprintf(stderr, "(%zu mms):\n", (size_t)long_work);
  for (int test_repeat = 0; test_repeat < FIO___LOCK2_TEST_REPEAT;
       ++test_repeat) {
    if (FIO___LOCK2_TEST_REPEAT > 1)
      fprintf(stderr,
              "%s (%d)\n",
              (test_repeat ? "Round" : "Warmup"),
              test_repeat);
    for (size_t fn = 0; test_funcs[fn].name; ++fn) {
      size_t result = 0;
      test_funcs[fn].task((void *)&result); /* warmup */
      result = 0;
      start = fio_time_micro();
      for (size_t i = 0; i < FIO___LOCK2_TEST_THREADS; ++i) {
        FIO_COMPILER_GUARD;
        test_funcs[fn].task(&result);
      }
      end = fio_time_micro();
      fprintf(stderr,
              "\t%s: %zu mms (%zu mms)\n",
              test_funcs[fn].name,
              (size_t)(end - start),
              (size_t)(end - (start + long_work)));
      FIO_ASSERT(result == (FIO___LOCK2_TEST_TASK * FIO___LOCK2_TEST_THREADS),
                 "%s final result error.",
                 test_funcs[fn].name);
    }
  }

  fprintf(stderr,
          "\n* Speed testing locking schemes - %zu threads, long work (%zu "
          "mms):\n",
          (size_t)FIO___LOCK2_TEST_THREADS,
          (size_t)long_work);
  for (int test_repeat = 0; test_repeat < FIO___LOCK2_TEST_REPEAT;
       ++test_repeat) {
    if (FIO___LOCK2_TEST_REPEAT > 1)
      fprintf(stderr,
              "%s (%d)\n",
              (test_repeat ? "Round" : "Warmup"),
              test_repeat);
    for (size_t fn = 0; test_funcs[fn].name; ++fn) {
      size_t result = 0;
      test_funcs[fn].task((void *)&result); /* warmup */
      result = 0;
      start = fio_time_micro();
      for (size_t i = 0; i < FIO___LOCK2_TEST_THREADS; ++i) {
        fio_thread_create(threads + i, test_funcs[fn].task, &result);
      }
      for (size_t i = 0; i < FIO___LOCK2_TEST_THREADS; ++i) {
        fio_thread_join(threads[i]);
      }
      end = fio_time_micro();
      fprintf(stderr,
              "\t%s: %zu mms (%zu mms)\n",
              test_funcs[fn].name,
              (size_t)(end - start),
              (size_t)(end - (start + long_work)));
      FIO_ASSERT(result == (FIO___LOCK2_TEST_TASK * FIO___LOCK2_TEST_THREADS),
                 "%s final result error.",
                 test_funcs[fn].name);
    }
  }
}

/* *****************************************************************************
Testing function
***************************************************************************** */

FIO_SFUNC void fio____test_dynamic_types__stack_poisoner(void) {
#define FIO___STACK_POISON_LENGTH (1ULL << 16)
  uint8_t buf[FIO___STACK_POISON_LENGTH];
  FIO_COMPILER_GUARD;
  memset(buf, (int)(~0U), FIO___STACK_POISON_LENGTH);
  FIO_COMPILER_GUARD;
  fio_trylock(buf);
#undef FIO___STACK_POISON_LENGTH
}

void fio_test_dynamic_types(void) {
  char *filename = (char *)FIO__FILE__;
  while (filename[0] == '.' && filename[1] == '/')
    filename += 2;
  fio____test_dynamic_types__stack_poisoner();
  fprintf(stderr, "===============\n");
  fprintf(stderr, "Testing Dynamic Types (%s)\n", filename);
  fprintf(
      stderr,
      "facil.io core: version \x1B[1m" FIO_VERSION_STRING "\x1B[0m\n"
      "The facil.io library was originally coded by \x1B[1mBoaz Segev\x1B[0m.\n"
      "Please give credit where credit is due.\n"
      "\x1B[1mYour support is only fair\x1B[0m - give value for value.\n"
      "(code contributions / donations)\n\n");
  fprintf(stderr, "===============\n");
  FIO_LOG_DEBUG("example FIO_LOG_DEBUG message.");
  FIO_LOG_DEBUG2("example FIO_LOG_DEBUG2 message.");
  FIO_LOG_INFO("example FIO_LOG_INFO message.");
  FIO_LOG_WARNING("example FIO_LOG_WARNING message.");
  FIO_LOG_SECURITY("example FIO_LOG_SECURITY message.");
  FIO_LOG_ERROR("example FIO_LOG_ERROR message.");
  FIO_LOG_FATAL("example FIO_LOG_FATAL message.");
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, type_sizes)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, random)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, atomics)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, bitwise)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, atol)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, url)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, glob_matching)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, sha1)();
  fprintf(stderr, "===============\n");
  fio___dynamic_types_test___linked_list_test();
  fprintf(stderr, "===============\n");
  fio___dynamic_types_test___index_list_test();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, ary____test)();
  FIO_NAME_TEST(stl, ary2____test)();
  FIO_NAME_TEST(stl, ary3____test)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, __umap_test__size_t)();
  FIO_NAME_TEST(stl, __umap_test__size_lru)();
  FIO_NAME_TEST(stl, __omap_test__size_t)();
  FIO_NAME_TEST(stl, __omap_test__size_lru)();
  fprintf(stderr, "===============\n");
  fio___dynamic_types_test___map_test();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, fio_big_str)();
  FIO_NAME_TEST(stl, fio_small_str)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, time)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, queue)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, cli)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, stream)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, signal)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, poll)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, mem_helper_speeds)();
  fprintf(stderr, "===============\n");
  /* test memory allocator that initializes memory to zero */
  FIO_NAME_TEST(FIO_NAME(stl, fio_mem_test_safe), mem)();
  fprintf(stderr, "===============\n");
  /* test memory allocator that allows junk data in allocations */
  FIO_NAME_TEST(FIO_NAME(stl, fio_mem_test_unsafe), mem)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, sock)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, fiobj)();
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, risky)();
#if !DEBUG
  fprintf(stderr, "===============\n");
  FIO_NAME_TEST(stl, lock_speed)();
#endif
  fprintf(stderr, "===============\n");
  {
    char timebuf[64];
    fio_time2rfc7231(timebuf, fio_time_real().tv_sec);
    fprintf(stderr,
            "On %s\n"
            "Testing \x1B[1mPASSED\x1B[0m "
            "for facil.io core version: "
            "\x1B[1m" FIO_VERSION_STRING "\x1B[0m"
            "\n",
            timebuf);
  }
  fprintf(stderr,
          "\nThe facil.io library was originally coded by \x1B[1mBoaz "
          "Segev\x1B[0m.\n"
          "Please give credit where credit is due.\n"
          "\x1B[1mYour support is only fair\x1B[0m - give value for value.\n"
          "(code contributions / donations)\n\n");
#if !defined(FIO_NO_COOKIE)
  fio___();
#endif
}

/* *****************************************************************************
Testing cleanup
***************************************************************************** */

#undef FIO_TEST_CSTL
#undef TEST_REPEAT

#endif /* FIO_EXTERN_COMPLETE */
#endif
/* *****************************************************************************








***************************************************************************** */

/* *****************************************************************************
C++ extern end
***************************************************************************** */
/* support C++ */
#ifdef __cplusplus
}
#endif
