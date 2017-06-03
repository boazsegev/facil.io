#include "tests/http_ws.h"
// #include "tests/client.h"
// #include "tests/http_bench.inc"
// #include "tests/http_stress.inc"
// #include "tests/shootout.h"

#include "facil.h"
#include "http.h"

#include <errno.h>
#include <sys/socket.h>

#define print_error_code(code)                                                 \
  do {                                                                         \
    errno = (code);                                                            \
    fprintf(stderr, #code " (%d) ", code);                                     \
    perror("");                                                                \
  } while (0);

int main() {
  // print_error_code(EWOULDBLOCK);
  // print_error_code(EAGAIN);
  // print_error_code(ECONNABORTED);
  // print_error_code(ECONNRESET);
  // print_error_code(EFAULT);
  // print_error_code(EINTR);
  // print_error_code(EMFILE);
  // print_error_code(ENOMEM);
  // print_error_code(ENOTSOCK);
  // print_error_code(EOPNOTSUPP);

  // fprintf(stderr, "sock len size: %lu\n", sizeof(socklen_t));
  listen2http_ws("3000", "./public_www");
  // listen2stress("3030", "./public_www");
  // listen2shootout("3000", 1);
  // listen2bench("3000", "./public_www");
  // defer(client_attempt, "3000", "3999");
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 1, .processes = 8);
}
