/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/io-core
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define H_FACIL_IO_H

#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
General Compile Time Settings
***************************************************************************** */
#ifndef FIO_CPU_CORES_FALLBACK
/**
 * When failing to detect the available CPU cores, this is the used value.
 *
 * Note: this does not affect the FIO_MEMORY_ARENA_COUNT_FALLBACK value.
 */
#define FIO_CPU_CORES_FALLBACK 8
#endif

#ifndef FIO_CPU_CORES_LIMIT
/** Maximum number of cores to detect. */
#define FIO_CPU_CORES_LIMIT 32
#endif

#ifndef FIO_SOCKET_BUFFER_PER_WRITE
/** The buffer size on the stack, for when a call to `write` required a copy. */
#define FIO_SOCKET_BUFFER_PER_WRITE (1UL << 16)
#endif

#ifndef FIO_SOCKET_THROTTLE_LIMIT
/** Throttle the client (prevent `on_data`) at outgoing byte queue limit. */
#define FIO_SOCKET_THROTTLE_LIMIT (1UL << 20)
#endif

#ifndef FIO_IO_TIMEOUT_MAX
#define FIO_IO_TIMEOUT_MAX 600
#endif

#ifndef FIO_SHOTDOWN_TIMEOUT
/** The number of shutdown seconds after which unsent data is ignored. */
#define FIO_SHOTDOWN_TIMEOUT 5
#endif

/* *****************************************************************************
CSTL modules
***************************************************************************** */
#define FIO_LOG
#define FIO_EXTERN
#include "fio-stl.h"

#if !defined(FIO_USE_THREAD_MUTEX) && FIO_OS_POSIX
#define FIO_USE_THREAD_MUTEX 1
#endif

/* CLI extension should use the system allocator. */
#define FIO_EXTERN
#define FIO_CLI
#include "fio-stl.h"

#define FIO_EXTERN
#define FIO_MALLOC
#include "fio-stl.h"

#define FIO_EXTERN
#define FIO_ATOL
#define FIO_ATOMIC
#define FIO_BITWISE
#define FIO_GLOB_MATCH
#define FIO_LOCK
#define FIO_RAND
#define FIO_THREADS
#define FIO_TIME
#define FIO_URL
#include "fio-stl.h"

#define FIO_EXTERN
#define FIOBJ_EXTERN
#define FIOBJ_MALLOC
#define FIO_FIOBJ
#include "fio-stl.h"

#define FIO_STREAM
#define FIO_QUEUE
#define FIO_SOCK
#include "fio-stl.h"

/* Should be automatic, but why not... */
#undef FIO_EXTERN
#undef FIO_EXTERN_COMPLETE
/* *****************************************************************************
Additional Included files
***************************************************************************** */
#if FIO_OS_POSIX
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

/** The main protocol object type. See `struct fio_protocol_s`. */
typedef struct fio_protocol_s fio_protocol_s;

/** The main protocol object type. See `struct fio_protocol_s`. */
typedef struct fio_s fio_s;

/** TLS context object, if any. */
typedef struct fio_tls_s fio_tls_s;

/* *****************************************************************************
Quick Windows Patches
***************************************************************************** */
#if FIO_OS_WIN
#ifndef pid_t
#define pid_t DWORD
#endif
#ifndef getpid
#define getpid GetCurrentProcessId
#endif
#endif
