#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fio_cli_helper.h"
#include "http.h"
#include "sock.h"

#include <stdio.h>

static void http_hello_on_request(http_request_s *request);

/*
A simple Hello World HTTP response + static file service, for benchmarking.

ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
wrk -c200 -d4 -t12 http://localhost:3000/

Or without keep-alive (`ab` complains and fails):

ab -c 200 -t 4 -n 1000000 -r http://127.0.0.1:3000/
wrk -c200 -d5 -t12 -H"Connection: close" http://localhost:3000/

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

/*
Available command line flags:
-p <port>          : defaults port 3000.
-t <threads>       : defaults to the number of CPU cores (or 1).
-w <processes>     : defaults to the number of CPU cores (or 1).
-q                 : sets verbosity (HTTP logging) off (on by default).
*/
int main(int argc, char const *argv[]) {
  uint8_t print_log = 1;

  /*     ****  Command line arguments ****     */
  fio_cli_start(argc, argv,
                "This is a facil.io example application.\n"
                "\nThis example offers a simple \"Hello World\" server "
                "used for benchmarking.\n"
                "\nThe following arguments are supported:\n");

  fio_cli_accept_num("port p", "the port to listen to, defaults to 3000.");
  fio_cli_accept_num("threads t", "number of threads.");
  fio_cli_accept_num("workers w", "number of processes.");
  fio_cli_accept_str("public www", "public folder for static file service.");
  fio_cli_accept_bool("log q", "disable logging, by default logs to a file.");

  if (fio_cli_get_int("q"))
    print_log = 0;
  if (!fio_cli_get_str("port"))
    fio_cli_set_str("port", "3000");
  const char *port = fio_cli_get_str("port");
  const uint32_t threads = fio_cli_get_int("t");
  const uint32_t workers = fio_cli_get_int("w");
  const char *public_folder = fio_cli_get_str("www");
  fio_cli_end();

  /*     ****  logging ****     */

  if (print_log) {
    /* log to the "benchmark.log" file, set to `if` to 0 to skip this*/
    if (1) {
      fclose(stderr);
      FILE *log = fopen("./tmp/benchmark.log", "a");
      if (!log) {
        fprintf(stdout, "* stderr closed and couldn't be opened.\n");
      } else {
        fprintf(stdout,
                "* All logging reports (stderr) routed to a log file at "
                "./tmp/benchmark.log\n");
        sock_open(fileno(log));
      }
    }
  }

  /*     ****  actual code ****     */

  // RedisEngine = redis_engine_create(.address = "localhost", .port = "6379");
  if (http_listen(port, NULL, .on_request = http_hello_on_request,
                  .log_static = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Hello World service"), exit(1);
  facil_run(.threads = threads, .processes = workers);
}

static void http_hello_on_request(http_request_s *request) {
  http_response_s *rs = http_response_create(request);
  if (!rs) {
    perror("ERROR: WTF?! No Memory? ");
    return;
  }

  /* locate the logging settings passed to `http_listen`*/
  if (request->settings->log_static)
    http_response_log_start(rs);

  http_response_write_body(rs, "Hello World!", 12);
  http_response_finish(rs);
}
