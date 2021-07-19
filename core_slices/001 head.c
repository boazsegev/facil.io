/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
External STL features published
***************************************************************************** */
#define FIO_EXTERN_COMPLETE   1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>
/* *****************************************************************************
Quick Patches
***************************************************************************** */
#if FIO_OS_WIN
#ifndef fork
#define fork() (-1)
#endif
#ifndef waitpid
#define waitpid(...) (-1)
#endif
#ifndef WIFEXITED
#define WIFEXITED(...) (-1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(...) (-1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(...) (-1)
#endif
#ifndef pipe
#define pipe(pfd) _pipe(pfd, 0, _O_BINARY)
#endif
#ifndef dup
#define dup _dup
#endif
#ifndef dup2
#define dup2 _dup2
#endif
#endif
