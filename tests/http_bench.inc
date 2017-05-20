#ifndef HTTP_HELLO
#include "http.h"
#include "sock.h"

/*
A simple Hello World HTTP response + static file service, for benchmarks.

ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
wrk -c200 -d4 -t12 http://localhost:3000/
*/

/*
// The following removes the outgoing stack from "hello world" benchmarks:

static void http1_hello_on_request(http_request_s *request) {
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1;timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  sock_write(request->metadata.fd, hello_message, sizeof(hello_message) - 1);
}
*/

static void http1_hello_on_request(http_request_s *request) {
  http_response_s *rs = http_response_create(request);
  if (!rs) {
    perror("ERROR: WTF?! No Memory? ");
    return;
  }
  http_response_log_start(rs);
  http_response_write_body(rs, "Hello World!", 12);
  http_response_finish(rs);
}

void listen2bench(void) {
  fclose(stderr);
  FILE *log = fopen("./tmp/benchmark.log", "a");
  if (!log) {
    fprintf(stdout, "* stderr closed and couldn't be opened.\n");
  } else {
    fprintf(stdout, "* All logging reports (stderr) routed to a log file at "
                    "./tmp/benchmark.log\n");
    sock_open(fileno(log));
  }

  http_listen("3000", NULL, .public_folder = "./public_www",
              .on_request = http1_hello_on_request, .log_static = 1);
}
#endif
