#include "facil.h"
#include "fio_base64.h"
#include "fio_hashmap.h"
#include "fio_mem.h"
#include "fio_random.h"
#include "fio_sha1.h"
#include "fio_sha2.h"
#include "http.h"

int main(void) {
#if DEBUG
#if HAVE_OPENSSL && !NODEBUG
  fprintf(stderr, "\n=== WARNING: performance tests should be ignored,\n"
                  "===          facil.io's optimized code "
                  "is faster\n");
#endif
  fio_base64_test();
  fio_sha1_test();
  fio_sha2_test();
  fio_random_test();
#if HAVE_OPENSSL && !NODEBUG
  fprintf(stderr, "=== WARNING: performance tests should be ignored,\n"
                  "===          facil.io's optimized code "
                  "is faster\n\n");
#endif
  fio_malloc_test();
  fio_hash_test();
  fiobj_test();
  defer_test();
  sock_libtest();
  http_tests();
#else
  fprintf(stderr, "DEBUG must be set to access tests.\n");
  exit(-1);
#endif
  return 0;
}
