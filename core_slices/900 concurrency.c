/* *****************************************************************************






                      Functions that can be overridden






***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
*****************************************************************************
*/
/**
 * OVERRIDE THIS to replace the default `fork` implementation.
 *
Behaves like the system's `fork`.... but calls the correct state callbacks.
 *
 * NOTE: When overriding, remember to call the proper state callbacks and call
 * fio_on_fork in the child process before calling any state callback or other
 * functions.
 */
static int fio_fork_overridable(void) { return fork(); }

/**
 * Accepts a pointer to a function and a single argument that should be
 * executed within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
static int fio_thread_start_overridable(void *p_thr,
                                        void *(*thread_func)(void *),
                                        void *arg) {
  return fio_thread_create(p_thr, thread_func, arg);
}

/**
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
static int fio_thread_join_overridable(void *p_thr) {
  int r = fio_thread_join(*(fio_thread_t *)p_thr);
  memset(p_thr, 0, FIO_FUNCTIONS.size_of_thread_t);
  return r;
}
