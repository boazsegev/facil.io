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

#define print_error_code(code)                                                 \
  do {                                                                         \
    errno = (code);                                                            \
    fprintf(stderr, #code " (%d) ", code);                                     \
    perror("");                                                                \
  } while (0);

static void handle_cluster_test(void *msg, uint32_t len) {
  fprintf(stderr, "%.*s\n", (int)len, msg);
}

static void send_cluster_msg(void *a1) {
  (void)a1;
  fprintf(stderr, "* Sending a cluster message.\n");
  facil_cluster_send(7, "Cluster is alive.", 18);
}

static void test_cluster(void *a1, void *a2) {
  (void)a1;
  (void)a2;
  facil_cluster_set_handler(7, handle_cluster_test);
  if (!defer_fork_pid())
    facil_run_every(5000, -1, send_cluster_msg, NULL, NULL);
}
int main() {
  // print_error_code(EBADF);
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
  // print_error_code(EINVAL);
  // print_error_code(EIO);
  // print_error_code(EPIPE);
  // print_error_code(ENOSPC);
  // print_error_code(ENOENT);

  // defer(test_cluster, NULL, NULL);

  // fprintf(stderr, "sock len size: %lu\n", sizeof(socklen_t));
  // listen2http_ws("3000", "./public_www");
  // listen2stress("3030", "./public_www");
  // listen2shootout("3000", 1);
  listen2shootout_pubsub("3000", 1);
  // listen2bench("3000", "./public_www");
  // defer(client_attempt, "3000", "3999");
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 4, .processes = 4);
}
