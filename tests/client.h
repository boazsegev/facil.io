#ifndef CLIENT_MODE_TEST

#ifndef THREAD_COUNT
#define THREAD_COUNT 1
#endif

#include "http.h"

#include <arpa/inet.h>
#include <sys/socket.h>

void on_http_hello(http_request_s *req) {
  http_response_s *response = http_response_create(req);
  http_response_log_start(response);
  http_response_write_body(response, "Hello World!", 12);
  http_response_finish(response);
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
          udata ? (char *)udata : "");
  char buffer[128];
  sock_peer_addr_s addrinfo = sock_peer_addr(uuid);
  if (addrinfo.addrlen) {
    if (inet_ntop(
            addrinfo.addr->sa_family,
            addrinfo.addr->sa_family == AF_INET
                ? (void *)&((struct sockaddr_in *)addrinfo.addr)->sin_addr
                : (void *)&((struct sockaddr_in6 *)addrinfo.addr)->sin6_addr,
            buffer, 128))
      fprintf(stderr, "Client connected to: %s\n", buffer);
  }
  sock_write(uuid, "GET / HTTP/1.1\r\nHost: localhost:3000\r\n\r\n", 40);
  return &client;
}

void on_client_fail(void *udata) {
  fprintf(stderr, "Client FAILED to connect (%s).\n",
          udata ? (char *)udata : "");
  perror("Reason");
}

void client_attempt(void *port1, void *port2) {
  if (-1 == facil_connect(.address = "localhost", .port = port1,
                          .on_connect = on_client_connect,
                          .on_fail = on_client_fail, .udata = "should be okay"))
    fprintf(stderr, "server_connect failed\n");
  if (-1 == facil_connect(.address = "localhost", .port = port2,
                          .on_connect = on_client_connect,
                          .on_fail = on_client_fail, .udata = "should fail"))
    fprintf(stderr, "server_connect failed\n");
}

#define CLIENT_MODE_TEST()                                                     \
  {                                                                            \
    http_listen("3000", NULL, .on_request = on_http_hello);                    \
    defer(client_attempt, "3000", "3999");                                     \
    facil_run(.threads = THREAD_COUNT);                                        \
  }
#endif
