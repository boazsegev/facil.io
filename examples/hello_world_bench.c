#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
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
  const char *port = "3000";
  const char *public_folder = NULL;
  uint32_t threads = 0;
  uint32_t workers = 0;
  uint8_t print_log = 1;

  /*     ****  Command line arguments ****     */

  for (int i = 1; i < argc; i++) {
    int offset = 0;
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'q': /* logging */
        print_log = 0;
        break;
      case 't': /* threads */
        if (!argv[i][2])
          i++;
        else
          offset = 2;
        threads = atoi(argv[i] + offset);
        break;
      case 'w': /* processes */
        if (!argv[i][2])
          i++;
        else
          offset = 2;
        workers = atoi(argv[i] + offset);
        break;
      case 'p': /* port */
        if (!argv[i][2])
          i++;
        else
          offset = 2;
        port = argv[i] + offset;
        break;
      }
    } else if (i == argc - 1)
      public_folder = argv[i];
  }
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
    perror("Couldn't initiate Websocket Shootout service"), exit(1);
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
