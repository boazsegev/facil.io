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
#if _MSC_VER
#define fork()           (-1)
#define waitpid(...)     (-1)
#define WIFEXITED(...)   (-1)
#define WEXITSTATUS(...) (-1)
#endif
