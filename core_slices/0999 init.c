/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0999 init.c
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#include "0020 reactor.c" /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************







Library initialization / cleanup







***************************************************************************** */

/* *****************************************************************************
Destroy the State Machine
***************************************************************************** */

static void __attribute__((destructor)) fio___data_free(void) {
  fio_state_callback_force(FIO_CALL_AT_EXIT);
  fio_state_callback_clear_all();
  fio_poll_close();
  fio_timer_destroy(&fio___timer_tasks);
  fio_queue_destroy(&fio___task_queue);
  free(fio_data);
}

/* *****************************************************************************
Initialize the State Machine
***************************************************************************** */

static void __attribute__((constructor)) fio___data_new(void) {
  ssize_t capa = 0;
  struct rlimit rlim = {.rlim_max = 0};
  {
#ifdef _SC_OPEN_MAX
    capa = sysconf(_SC_OPEN_MAX);
#elif defined(FOPEN_MAX)
    capa = FOPEN_MAX;
#endif
    // try to maximize limits - collect max and set to max
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
      FIO_LOG_WARNING("`getrlimit` failed in facil.io core initialization.");
      perror("\terrno:");
    } else {
      rlim_t original = rlim.rlim_cur;
      rlim.rlim_cur = rlim.rlim_max;
      if (rlim.rlim_cur > FIO_MAX_SOCK_CAPACITY) {
        rlim.rlim_cur = rlim.rlim_max = FIO_MAX_SOCK_CAPACITY;
      }
      while (setrlimit(RLIMIT_NOFILE, &rlim) == -1 && rlim.rlim_cur > original)
        --rlim.rlim_cur;
      getrlimit(RLIMIT_NOFILE, &rlim);
      capa = rlim.rlim_cur;
      if (capa > 1024) /* leave a slice of room */
        capa -= 16;
    }
  }
  fio_data = calloc(1, sizeof(*fio_data) + (sizeof(fio_data->info[0]) * capa));
  FIO_ASSERT_ALLOC(fio_data);
  fio_data->capa = capa;
  fio_data->pid = fio_data->parent = getpid();
  fio_data->is_master = 1;
  fio_data->lock = FIO_LOCK_INIT;
  fio_mark_time();
  fio_poll_init();
  FIO_LOG_PRINT__(
      FIO_LOG_LEVEL_DEBUG,
      "facil.io " FIO_VERSION_STRING " (%s) initialization:\n"
      "\t* Maximum open files %zu out of %zu\n"
      "\t* Allocating %zu bytes for state handling.\n"
      "\t* %zu bytes per connection + %zu bytes for state handling.",
      fio_engine(),
      capa,
      (size_t)rlim.rlim_max,
      sizeof(*fio_data) + (sizeof(fio_data->info[0]) * capa),
      sizeof(fio_data->info[0]),
      sizeof(*fio_data));
  fio_pubsub_init();
  fio_state_callback_force(FIO_CALL_ON_INITIALIZE);
  fio_state_callback_clear(FIO_CALL_ON_INITIALIZE);
}
