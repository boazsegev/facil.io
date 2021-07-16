/* *****************************************************************************
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_uuid_s *uuid,
                  void (*task)(fio_uuid_s *uuid, void *udata),
                  void *udata);
