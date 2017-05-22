#include "facil.h"
#include "http.h"
// #include "tests/http_bench.inc"
#include "tests/http_stress.inc"
#include "tests/shootout.h"

#include <errno.h>

#define print_error_code(code)                                                 \
  do {                                                                         \
    errno = (code);                                                            \
    fprintf(stderr, #code " (%d) ", code);                                     \
    perror("");                                                                \
  } while (0);

int main() {
  print_error_code(EWOULDBLOCK);
  print_error_code(EAGAIN);
  print_error_code(ECONNABORTED);
  print_error_code(ECONNRESET);
  print_error_code(EFAULT);
  print_error_code(EINTR);
  print_error_code(EMFILE);
  print_error_code(ENOMEM);
  print_error_code(ENOTSOCK);
  print_error_code(EOPNOTSUPP);

  // listen2stress("3000", "./public_www");
  listen2shootout("3000", 1);
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 16, .processes = 1);
}
