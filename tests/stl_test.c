#ifndef FIO_TEST_CSTL
#define FIO_TEST_CSTL
#endif

#include "fio-stl.h"

int main(int argc, char const *argv[]) {
  fio_test_dynamic_types();
#if !defined(FIO_NO_COOKIE)
  fio___();
#endif
  (void)argc;
  (void)argv;
  return 0;
}
