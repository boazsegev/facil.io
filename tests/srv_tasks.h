
#ifndef SRV_TASKS_TEST_H
#include "http.h"

/*
Test with:
ab -n 1000 -c 200 -k http://127.0.0.1:3000/
*/

static void srv_task_test_task_fin(intptr_t fd, void *arg) {
  http_response_s *response = arg;
  http_response_finish(response);
  fprintf(stderr, "Finished task after %lu writes\n",
          128UL - response->metadata.request->method_len);
  free(response->metadata.request);
  free(response);
  (void)(fd);
}

static void srv_task_test_task(intptr_t fd, protocol_s *http, void *arg) {
  http_response_s *response = arg;
  if (response->metadata.request->method_len == 0) {
    srv_task_test_task_fin(fd, arg);
    return;
  }
  http_response_write_body(response, "<p>Hello World!</p>\n", 20);
  response->metadata.request->method_len--;
  server_task(fd, srv_task_test_task, response, srv_task_test_task_fin);
  (void)(fd);
  (void)(http);
}

static void srv_task_test_on_request(http_request_s *request) {
  http_response_s *response = malloc(sizeof(*response));
  http_request_s *new_req = malloc(sizeof(*new_req));
  *new_req = *request;
  *response = http_response_init(new_req);
  response->content_length = -1;
  response->metadata.should_close = 1;
  response->metadata.request->method_len = 128;
  server_task(request->metadata.fd, srv_task_test_task, response,
              srv_task_test_task_fin);
}

#define SRV_TASKS_TEST(port, public_fldr)                                      \
  http1_listen((port), NULL, .public_folder = (public_fldr),                   \
               .on_request = srv_task_test_on_request)
#endif
