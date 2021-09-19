/* *****************************************************************************
Task Scheduling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2) {
  fio_queue_push(FIO_QUEUE_USER,
                 .fn = task,
                 .udata1 = udata1,
                 .udata2 = udata2);
}

FIO_SFUNC void fio_io_task_wrapper(void *task_, void *ignr_) {
  fio_queue_task_s *t = task_;
  fio_s *io = t->udata1;
  union {
    void (*fn)(fio_s *io, void *udata);
    void (*fn2)(void *io, void *udata);
    void *p;
  } u = {.fn2 = t->fn};
  if (fio_trylock(&io->lock))
    goto reschedule;
  u.fn(io, t->udata2);
  fio_unlock(&io->lock);
  fio_free(t);
  fio_undup(io);
  return;
reschedule:
  fio_queue_push(FIO_QUEUE_IO(io),
                 .fn = fio_io_task_wrapper,
                 .udata1 = task_,
                 .udata2 = ignr_);
}

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_s *io,
                  void (*task)(fio_s *io, void *udata),
                  void *udata) {
  union {
    void (*fn)(fio_s *io, void *udata);
    void (*fn2)(void *io, void *udata);
    void *p;
  } u;
  u.fn = task;
  fio_queue_task_s *t = fio_malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  *t = (fio_queue_task_s){.fn = u.fn2, .udata1 = fio_dup(io), .udata2 = udata};
  fio_queue_push(FIO_QUEUE_IO(io), .fn = fio_io_task_wrapper, .udata1 = t);
}

/** Schedules a timer bound task, see `fio_timer_schedule` in the CSTL. */
void fio_run_every FIO_NOOP(fio_timer_schedule_args_s args) {
  args.start_at += fio_data.tick * (!args.start_at);
  fio_timer_schedule FIO_NOOP(&fio_data.timers, args);
}
/**
 * Schedules a timer bound task, see `fio_timer_schedule` in the CSTL.
 *
 * Possible "named arguments" (fio_timer_schedule_args_s members) include:
 *
 * * The timer function. If it returns a non-zero value, the timer stops:
 *        int (*fn)(void *, void *)
 * * Opaque user data:
 *        void *udata1
 * * Opaque user data:
 *        void *udata2
 * * Called when the timer is done (finished):
 *        void (*on_finish)(void *, void *)
 * * Timer interval, in milliseconds:
 *        uint32_t every
 * * The number of times the timer should be performed. -1 == infinity:
 *        int32_t repetitions
 */
