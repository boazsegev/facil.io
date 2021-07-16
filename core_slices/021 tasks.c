/* *****************************************************************************
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2) {
  fio_queue_push(&tasks_user, .fn = task, .udata1 = udata1, .udata2 = udata2);
}

FIO_SFUNC void fio_io_task_wrapper(void *task_, void *ignr_) {
  fio_queue_task_s *t = task_;
  fio_uuid_s *uuid = t->udata1;
  union {
    void (*fn)(fio_uuid_s *uuid, void *udata);
    void (*fn2)(void *uuid, void *udata);
    void *p;
  } u = {.fn2 = t->fn};
  if (fio_trylock(&uuid->lock))
    goto reschedule;
  u.fn(uuid, t->udata2);
  fio_unlock(&uuid->lock);
  fio_free(t);
  fio_uuid_free(uuid);
  return;
reschedule:
  fio_queue_push(&tasks_user,
                 .fn = fio_io_task_wrapper,
                 .udata1 = task_,
                 .udata2 = ignr_);
}

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_uuid_s *uuid,
                  void (*task)(fio_uuid_s *uuid, void *udata),
                  void *udata) {
  union {
    void (*fn)(fio_uuid_s *uuid, void *udata);
    void (*fn2)(void *uuid, void *udata);
    void *p;
  } u;
  u.fn = task;
  fio_queue_task_s *t = fio_malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  *t = (fio_queue_task_s){.fn = u.fn2,
                          .udata1 = fio_uuid_dup(uuid),
                          .udata2 = udata};
  fio_queue_push(&tasks_user,
                 .fn = fio_io_task_wrapper,
                 .udata1 = uuid,
                 .udata2 = udata);
}
