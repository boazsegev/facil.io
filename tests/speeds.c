#include "tests/mustache.c.h"

#include "facil.h"
#include "fio_ary.h"
#include "fio_base64.h"
#include "fio_hashmap.h"
#include "fio_llist.h"
#include "fio_mem.h"
#include "fio_random.h"
#include "fio_set.h"
#include "fio_sha1.h"
#include "fio_sha2.h"
#include "fio_siphash.h"
#include "fio_str.h"
#include "http.h"

int main(void) {
#if DEBUG
  fio_base64_test();
  fio_sha1_test();
  fio_sha2_test();
  fio_siphash_test();
  fio_random_test();
#else
  fprintf(stderr, "DEBUG must be set to access tests.\n");
  exit(-1);
#endif
  return 0;
}
