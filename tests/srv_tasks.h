
#ifndef SRV_TASKS_TEST_H
#include "http.h"

/*
Test with:
ab -n 1000 -c 200 -k http://127.0.0.1:3000/
*/

static void srv_task_test_task_fin(intptr_t fd, void *arg) {
  http_response_s *response = arg;
  fprintf(stderr, "Finished task after %lu writes\n",
          128UL - response->request->method_len);
  http_response_finish(response);
  (void)(fd);
}

static void srv_task_test_task(intptr_t fd, protocol_s *http, void *arg) {
  http_response_s *response = arg;
  if (response->request->method_len == 0) {
    srv_task_test_task_fin(fd, arg);
    return;
  }
  http_response_write_body(response, "<p>Hello World!</p>\n", 20);
  response->request->method_len--;
  facil_defer(fd, srv_task_test_task, response, srv_task_test_task_fin);
  (void)(fd);
  (void)(http);
}

static void srv_task_test_on_request(http_request_s *request) {
  http_response_s *response = http_response_create(http_request_dup(request));
  response->request_dupped = 1;
  request = response->request;
  response->content_length = -1;
  response->should_close = 1;
  response->request->method_len = 128;
  facil_defer(request->fd, srv_task_test_task, response,
              srv_task_test_task_fin);
}

#define SRV_TASKS_TEST(port, public_fldr)                                      \
  http_listen((port), NULL, .public_folder = (public_fldr),                    \
              .on_request = srv_task_test_on_request)
#endif
