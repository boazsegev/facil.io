#include "tests/mustache.c.h"
#include <fio.h>
#include <fiobj.h>

int main(void) {
  fio_test();
  mustache_test();
  fiobj_test();
}
