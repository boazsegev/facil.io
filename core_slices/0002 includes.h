/* *****************************************************************************
0001 macros.h
***************************************************************************** */

/* *****************************************************************************
Compilation Macros
***************************************************************************** */

#ifndef FIO_MAX_SOCK_CAPACITY
/**
 * The maximum number of connections per worker process.
 */
#define FIO_MAX_SOCK_CAPACITY 262144
#endif

#ifndef FIO_CPU_CORES_LIMIT
/**
 * If facil.io detects more CPU cores than the number of cores stated in the
 * FIO_CPU_CORES_LIMIT, it will assume an error and cap the number of cores
 * detected to the assigned limit.
 *
 * This is only relevant to automated values, when running facil.io with zero
 * threads and processes, which invokes a large matrix of workers and threads
 * (see {facil_run})
 *
 * The default auto-detection cap is set at 8 cores. The number is arbitrary
 * (historically the number 7 was used after testing `malloc` race conditions on
 * a MacBook Pro).
 *
 * This does NOT effect manually set (non-zero) worker/thread values.
 */
#define FIO_CPU_CORES_LIMIT 64
#endif

#ifndef FIO_PUBSUB_SUPPORT
/**
 * If true (1), compiles the facil.io pub/sub API.
 */
#define FIO_PUBSUB_SUPPORT 1
#endif

#ifndef FIO_TLS_PRINT_SECRET
/* If true, the master key secret SHOULD be printed using FIO_LOG_DEBUG */
#define FIO_TLS_PRINT_SECRET 0
#endif

#ifndef FIO_WEAK_TLS
/* If true, the weak-function TLS implementation will always be compiled. */
#define FIO_WEAK_TLS 0
#endif

#ifndef FIO_TLS_IGNORE_MISSING_ERROR
/* If true, a no-op TLS implementation will be enabled (for debugging). */
#define FIO_TLS_IGNORE_MISSING_ERROR 0
#endif

#ifndef FIO_TLS_TIMEOUT
/* The default timeout for TLS connections (protocol assignment deferred) */
#define FIO_TLS_TIMEOUT 4
#endif

/* *****************************************************************************
Import STL
***************************************************************************** */
#ifndef FIO_LOG_LENGTH_LIMIT
/**
 * Since logging uses stack memory rather than dynamic allocation, it's memory
 * usage must be limited to avoid exploding the stack. The following sets the
 * memory used for a logging event.
 */
#define FIO_LOG_LENGTH_LIMIT 2048
#endif

/* Backwards support for version 0.7.x memory allocator behavior */
#ifdef FIO_OVERRIDE_MALLOC
#warning FIO_OVERRIDE_MALLOC is deprecated, use FIO_MALLOC_OVERRIDE_SYSTEM
#define FIO_MALLOC_OVERRIDE_SYSTEM
#elif defined(FIO_FORCE_MALLOC)
#warning FIO_FORCE_MALLOC is deprecated, use FIO_MEMORY_DISABLE
#define FIO_MEMORY_DISABLE
#endif

/* let it run once without side-effects, to prevent self-inclusion CLI errors */
#define FIO_LOG
#define FIO_EXTERN
#include "fio-stl.h"

#define FIO_RISKY_HASH 1
#include "fio-stl.h"

/* Enable CLI extension before enabling the custom memory allocator. */
#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_EXTERN
#define FIO_CLI
#include "fio-stl.h"

/* Enable custom memory allocator. */
#define FIO_EXTERN
#define FIO_MALLOC
#include "fio-stl.h"

/* Enable required extensions and FIOBJ types. */
#define FIO_EXTERN
#define FIO_ATOMIC
#define FIO_LOCK
#define FIO_BITWISE
#define FIO_ATOL
#define FIO_NTOL
#define FIO_RAND
#define FIO_TIME
#define FIO_GLOB_MATCH
#define FIO_URL
#include "fio-stl.h"
#define FIOBJ_EXTERN
#define FIOBJ_MALLOC
#define FIO_FIOBJ
#include "fio-stl.h"

#include <limits.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#include <netinet/in.h>
#include <sys/socket.h>
#endif

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
#endif
