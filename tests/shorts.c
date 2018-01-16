#include "facil.h"
#include "fio_hashmap.h"

int main(void) {
#if DEBUG
  fio_hash_test();
  fiobj_test();
  defer_test();
  sock_libtest();
#else
  fprintf(stderr, "DEBUG must e set to access tests.\n");
  exit(-1);
#endif
  return 0;
}
