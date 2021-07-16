/* *****************************************************************************
Testing
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */
#ifdef TEST
FIO_SFUNC void fio_test___task(void *u1, void *u2) {

  FIO_THREAD_WAIT(100000);
  fio_stop();
  (void)u1;
  (void)u2;
}
void fio_test(void) {
  fprintf(stderr, "Testing facil.io IO-Core framework modules.\n");
  FIO_NAME_TEST(io, state)();
  fio_defer(fio_test___task, NULL, NULL);
  fio_start(.threads = -1, .workers = 0);
}
#endif
