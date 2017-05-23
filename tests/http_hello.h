
#ifndef HTTP_HELLO
#include "http.h"

/*
A simple Hello World HTTP response emulation. Test with:
ab -n 1000000 -c 200 -k http://127.0.0.1:3000/
*/
static void http_hello_on_request(http_request_s *request) {
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1;timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  sock_write(request->fd, hello_message, sizeof(hello_message) - 1);
}

#define HTTP_HELLO(port, public_fldr)                                          \
  http_listen(port, NULL, .public_folder = (public_fldr),                      \
              .on_request = http_hello_on_request)
#endif
