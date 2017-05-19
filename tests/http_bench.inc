#ifndef HTTP_HELLO
#include "http.h"

/*
A simple Hello World HTTP response + static file service, for benchmarks.
ab -n 1000000 -c 200 -k http://127.0.0.1:3000/
*/
static void http1_hello_on_request(http_request_s *request) {
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1;timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  sock_write(request->metadata.fd, hello_message, sizeof(hello_message) - 1);
}

int main(void) {
  http1_listen("3000", NULL, .public_folder = "./public_www",
               .on_request = http1_hello_on_request);
  return 0;
}
#endif
