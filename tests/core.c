#ifndef TEST
#define TEST 1
#endif
/* when compiling tests this is easier... */
#ifdef TEST_WITH_LIBRARY
#include "fio.h"
#else
#include "fio.c"
#endif

int main(int argc, char const *argv[]) {
  fio_test();
  (void)argc;
  (void)argv;
  return 0;
}
