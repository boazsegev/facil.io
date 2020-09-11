/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0800 threads.h
***************************************************************************** */
#ifndef FIO_VERSION_MAJOR /* Development inclusion - ignore line */
#include "0011 base.c"    /* Development inclusion - ignore line */
#endif                    /* Development inclusion - ignore line */
/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
***************************************************************************** */

/* only call within child process! */
void fio_on_fork(void) {
  if (fio_data) {
    fio_data->lock = FIO_LOCK_INIT;
    fio_data->is_master = 0;
    fio_data->is_worker = 1;
    fio_data->pid = getpid();
  }
  fio_unlock(&fio_fork_lock);
  fio_defer_on_fork();
  fio_malloc_after_fork();
  fio_state_callback_on_fork();
  fio_poll_init();

  /* don't pass open connections belonging to the parent onto the child. */
  if (fio_data) {
    const size_t limit = fio_data->capa;
    for (size_t i = 0; i < limit; ++i) {
      fd_data(i).lock = FIO_LOCK_INIT;
      fd_data(i).lock = FIO_LOCK_INIT;
      if (fd_data(i).protocol) {
        fd_data(i).protocol->rsv = 0;
      }
      if (fd_data(i).protocol || fd_data(i).open) {
        fio_force_close(fd2uuid(i));
      }
    }
  }
}

/**
OVERRIDE THIS to replace the default `fork` implementation.

Behaves like the system's `fork`.... bt calls the correct state callbacks.
*/
int fio_fork(void) {
  int child;
  fio_state_callback_force(FIO_CALL_BEFORE_FORK);
  child = fork();
  switch (child + 1) {
  case 0:
    /* error code -1 */
    return child;
  case 1:
    /* child = 0, this is the child process */
    fio_on_fork();
    fio_state_callback_force(FIO_CALL_AFTER_FORK);
    fio_state_callback_force(FIO_CALL_IN_CHILD);
    fio_defer_perform();
    return child;
  }
  /* child == number, this is the master process */
  fio_state_callback_force(FIO_CALL_AFTER_FORK);
  fio_state_callback_force(FIO_CALL_IN_MASTER);
  fio_defer_perform();
  return child;
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer to a function and a single argument that should be executed
 * within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
void *fio_thread_new(void *(*thread_func)(void *), void *arg) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    pthread_t t;
    if (!pthread_create(&t, NULL, thread_func, arg))
      return (void *)t;
    return NULL;
  }
  pthread_t *t = malloc(sizeof(*t));
  FIO_ASSERT_ALLOC(t);
  if (!pthread_create(t, NULL, thread_func, arg))
    return t;
  free(t);
  t = NULL;
  return t;
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
void fio_thread_free(void *p_thr) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    pthread_detach(((pthread_t)p_thr));
    return;
  }
  pthread_detach(((pthread_t *)p_thr)[0]);
  free(p_thr);
}

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
int fio_thread_join(void *p_thr) {
  if (sizeof(pthread_t) == sizeof(void *)) {
    return pthread_join(((pthread_t)p_thr), NULL);
  }
  int r = pthread_join(((pthread_t *)p_thr)[0], NULL);
  free(p_thr);
  return r;
}
