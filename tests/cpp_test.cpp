#ifndef FIO_TEST_CSTL
#define FIO_TEST_CSTL
#endif

#include "fio-stl.h"

#include "fio.h"
#include "fiobj.h"
#include "http.h"

int main(int argc, char const *argv[]) {
  fio_test_dynamic_types();
  (void)argc;
  (void)argv;
  return 0;
}
