#ifndef CLIENT_MODE_TEST

#ifndef THREAD_COUNT
#define THREAD_COUNT 1
#endif

#include "http.h"

void on_http_hello(http_request_s *req) {
  http_response_s response = http_response_init(req);
  http_response_log_start(&response);
  http_response_write_body(&response, "Hello World!", 12);
  http_response_finish(&response);
}

void on_client_data(intptr_t uuid, protocol_s *protocol) {
  (void)protocol;
  char buffer[1024];
  int len = 0;
  while ((len = sock_read(uuid, buffer, 1023)) > 0) {
    buffer[len] = 0;
    fprintf(stderr, "Client Data:\n%s\n", buffer);
  }
}
void on_client_close(protocol_s *protocol) {
  (void)protocol;
  fprintf(stderr, "Client disconnect\n");
}

protocol_s *on_client_connect(intptr_t uuid, void *udata) {
  static protocol_s client = {
      .on_data = on_client_data, .on_close = on_client_close,
  };
  fprintf(stderr, "Client connected (%s), sending request\n",
          udata ? udata : "");
  sock_write(uuid, "GET / HTTP/1.1\r\nHost: localhost:3000\r\n\r\n", 40);
  return &client;
}

void on_client_fail(void *udata) {
  fprintf(stderr, "Client FAILED to connect (%s).\n", udata ? udata : "");
}

void on_CLIENT_MODE_init(void) {
  if (-1 == server_connect(.address = "localhost", .port = "3000",
                           .on_connect = on_client_connect,
                           .on_fail = on_client_fail,
                           .udata = "should be okay"))
    fprintf(stderr, "server_connect failed\n");
  if (-1 == server_connect(.address = "localhost", .port = "3030",
                           .on_connect = on_client_connect,
                           .on_fail = on_client_fail, .udata = "should fail"))
    fprintf(stderr, "server_connect failed\n");
}

#define CLIENT_MODE_TEST()                                                     \
  {                                                                            \
    http1_listen("3000", NULL, .on_request = on_http_hello);                   \
    server_run(.on_init = on_CLIENT_MODE_init, .threads = THREAD_COUNT);       \
  }
#endif
