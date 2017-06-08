// #include "tests/http_ws.h"
// #include "tests/client.h"
// #include "tests/http_bench.inc"
// #include "tests/http_stress.inc"
#include "tests/shootout_pubsub.inc"
// #include "tests/shootout.h"

#include "facil.h"
#include "http.h"

#include <errno.h>
#include <sys/socket.h>

int main() {
  // defer(test_cluster, NULL, NULL);

  // fprintf(stderr, "sock len size: %lu\n", sizeof(socklen_t));
  // listen2http_ws("3000", "./public_www");
  // listen2stress("3030", "./public_www");
  // listen2shootout("3000", 1);
  listen2shootout_pubsub("3000", 1);
  // listen2bench("3000", "./public_www");
  // defer(client_attempt, "3000", "3999");
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 4, .processes = 1);
}
