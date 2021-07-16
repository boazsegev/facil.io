/* *****************************************************************************
Copyright: Boaz Segev, 2019-2021
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
Quick Patches
***************************************************************************** */
#if _MSC_VER
#define pipe(pfd) _pipe(pfd, 0, _O_BINARY)
#define pid_t     HANDLE
#define getpid    GetCurrentProcessId
#endif
/* *****************************************************************************
External STL features published
***************************************************************************** */
#define FIO_EXTERN_COMPLETE   1
#define FIOBJ_EXTERN_COMPLETE 1
#define FIO_VERSION_GUARD
#include <fio.h>
