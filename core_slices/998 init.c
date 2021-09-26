/* *****************************************************************************
Starting the IO reactor and reviewing it's state
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Post fork cleanup
***************************************************************************** */
static void fio___after_fork(void) {
  fio_malloc_after_fork();
  fio___after_fork___core();
  fio___after_fork___io();
}
/* *****************************************************************************
State data initialization
***************************************************************************** */

FIO_DESTRUCTOR(fio_cleanup_at_exit) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  env_safe_destroy(&fio_data.env);
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_close_wakeup_pipes();
  if (fio_data.io_wake_io) {
    fio_free2(fio_data.io_wake_io);
    fio_data.io_wake_io = NULL;
  }
  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_monitor_destroy();
  fio_cli_end();
  fio_invalidate_all();
  for (int i = 0; i < FIO_CALL_NEVER; ++i)
    fio_state_callback_clear((callback_type_e)i);
  for (int i = 0; i < CHANNEL_TYPE_NONE; ++i)
    channel_store_destroy(postoffice.channels + i);

  while (!fio_queue_perform(FIO_QUEUE_SYSTEM) ||
         !fio_queue_perform(FIO_QUEUE_USER))
    ;
  fio_queue_destroy(fio_tasks);
  fio_queue_destroy(fio_tasks + 1);
}

FIO_CONSTRUCTOR(fio_data_state_init) {
  FIO_LOG_DEBUG2("initializing facio.io IO state.");
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.master = fio_data.pid = getpid();
  fio_data.tick = fio_time2milli(fio_time_real());
  fio_data.protocols = FIO_LIST_INIT(fio_data.protocols);
  fio_data.timers = (fio_timer_queue_s)FIO_TIMER_QUEUE_INIT;
  fio_tasks[0] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_tasks[0]);
  fio_tasks[1] = (fio_queue_s)FIO_QUEUE_STATIC_INIT(fio_tasks[1]);
  fio_monitor_init();
  fio_protocol_validate(&FIO_PROTOCOL_HIJACK);
  fio_validity_map_reserve(&fio_data.valid, 300);
  FIO_PROTOCOL_HIJACK.on_timeout = FIO_PING_ETERNAL;
  FIO_PROTOCOL_HIJACK.reserved.protocols =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.protocols);
  FIO_PROTOCOL_HIJACK.reserved.ios =
      FIO_LIST_INIT(FIO_PROTOCOL_HIJACK.reserved.ios);
  FIO_PROTOCOL_HIJACK.reserved.flags |= 1;
#if FIO_VALIDATE_IO_MUTEX
  fio_data.valid_lock = (fio_thread_mutex_t)FIO_THREAD_MUTEX_INIT;
#endif
  postoffice_initialize();
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
}
