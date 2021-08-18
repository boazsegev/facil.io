/* *****************************************************************************
Task Scheduling
***************************************************************************** */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_s *io, void (*task)(fio_s *io, void *udata), void *udata);
