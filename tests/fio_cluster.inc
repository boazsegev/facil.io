#ifndef H_FIO_CLUSTER_TEST_H
#define H_FIO_CLUSTER_TEST_H

#include "facil.h"
#include <errno.h>

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

static void defered_test_cluster(void *a1, void *a2) {
  (void)a1;
  (void)a2;
  facil_cluster_set_handler(7, handle_cluster_test);
  if (!defer_fork_pid())
    facil_run_every(5000, -1, send_cluster_msg, NULL, NULL);
}

static void test_cluster(void) {
  print_error_code(EBADF);
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
  print_error_code(EINVAL);
  print_error_code(EIO);
  print_error_code(EPIPE);
  print_error_code(ENOSPC);
  print_error_code(ENOENT);

  defer(defered_test_cluster, NULL, NULL);
}

#endif
