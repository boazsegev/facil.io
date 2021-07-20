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
#define fork() ((pid_t)(-1))
#endif
#ifndef waitpid
#define waitpid(...) ((pid_t)(-1))
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
#endif
