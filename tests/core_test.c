#ifndef FIO_TEST_CSTL
#define FIO_TEST_CSTL
#endif

#ifndef TEST
#define TEST 1
#endif

#include "fio.c"
#include "fio.h"

int main(int argc, char const *argv[]) {
  fio_test();
  (void)argc;
  (void)argv;
  return 0;
}
