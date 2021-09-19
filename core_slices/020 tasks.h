/* *****************************************************************************
Task Scheduling
***************************************************************************** */

/** Schedules a task for delayed execution. */
void fio_defer(void (*task)(void *, void *), void *udata1, void *udata2);

/** Schedules an IO task for delayed execution. */
void fio_defer_io(fio_s *io, void (*task)(fio_s *io, void *udata), void *udata);

/** Schedules a timer bound task, see `fio_timer_schedule` in the CSTL. */
void fio_run_every(fio_timer_schedule_args_s args);
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
#define fio_run_every(...)                                                     \
  fio_run_every((fio_timer_schedule_args_s){__VA_ARGS__})
