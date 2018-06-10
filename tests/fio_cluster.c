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

static void handle_cluster_test(facil_msg_s *m) {
  if (m->filter == 7) {
    fprintf(stderr, "(%d) %s: %s\n", m->filter, fiobj_obj2cstr(m->channel).data,
            fiobj_obj2cstr(m->msg).data);
  } else {
    fprintf(stderr, "ERROR: (cluster) filter mismatch (%d)!\n", m->filter);
  }
}

static void send_cluster_msg(void *a1) {
  (void)a1;
  fprintf(stderr, "* Sending a cluster message.\n");
  FIOBJ ch = fiobj_str_new("Cluster Test", 12);
  FIOBJ msg = fiobj_str_new("okay", 4);
  facil_publish(.filter = 7, .channel = ch, .message = msg);
  facil_publish(.filter = 6, .channel = ch, .message = msg);
  fiobj_free(ch);
  fiobj_free(msg);
}

static void defered_test_cluster(void *a1, void *a2) {
  (void)a1;
  (void)a2;
  if (facil_parent_pid() == getpid()) {
    facil_run_every(5000, -1, send_cluster_msg, NULL, NULL);
  }
}

static void perform_callback(void *cb_str) {
  fprintf(stderr, "(%d) %s\n", getpid(), (char *)cb_str);
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

int main(void) {
  facil_core_callback_add(FIO_CALL_BEFORE_FORK, perform_callback,
                          "Before fork");
  facil_core_callback_add(FIO_CALL_AFTER_FORK, perform_callback, "After fork ");
  facil_core_callback_add(FIO_CALL_IN_CHILD, perform_callback,
                          "Just the child");
  facil_core_callback_add(FIO_CALL_ON_START, perform_callback, "Starting up ");
  facil_core_callback_add(FIO_CALL_ON_SHUTDOWN, perform_callback,
                          "Shutting down");
  facil_core_callback_add(FIO_CALL_ON_FINISH, perform_callback, "Done.");
  facil_core_callback_add(FIO_CALL_ON_IDLE, perform_callback, "idling.");

  facil_subscribe(.filter = 7, .on_message = handle_cluster_test);

  test_cluster();
  facil_run(.threads = 2, .workers = 4);

  return 0;
}

#endif
