#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fio_cli.h"
#include "http.h"

#include <stdio.h>

/*
A simple Hello World HTTP response + static file service, for benchmarking.

ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
wrk -c200 -d4 -t1 http://localhost:3000/

Or without keep-alive (`ab` complains and fails):

ab -c 200 -t 4 -n 1000000 http://127.0.0.1:3000/
wrk -c200 -d5 -t1 -H"Connection: close" http://localhost:3000/

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

static FIOBJ SERVER_HEADER;
static FIOBJ SERVER_NAME;
static FIOBJ TEXT_TYPE;

/* The HTTP request handler */
static void http_hello_on_request(http_s *h) {
  http_set_header(h, SERVER_HEADER, fiobj_dup(SERVER_NAME));
  http_set_header(h, HTTP_HEADER_CONTENT_TYPE, fiobj_dup(TEXT_TYPE));
  http_send_body(h, "Hello World!", 12);
}

/* reads command line arguments and starts up the server. */
int main(int argc, char const *argv[]) {
  uint8_t print_log = 0;

  /*     ****  Command line arguments ****     */
  fio_cli_start(argc, argv,
                "This is a facil.io example application.\n\n"
                "This example offers a simple \"Hello World\" server "
                "used for benchmarking.\n\n"
                "The following arguments are supported:\n");

  fio_cli_accept_num("port p", "the port to listen to, defaults to 3000.");
  fio_cli_accept_num("threads t", "number of threads.");
  fio_cli_accept_num("workers w", "number of processes.");
  fio_cli_accept_str("public www", "public folder for static file service.");
  fio_cli_accept_bool("log v", "verobse, logs to a file at the ./tmp folder.");

  if (fio_cli_get_int("log"))
    print_log = 1;
  if (!fio_cli_get_str("port"))
    fio_cli_set_str("port", "3000");
  const char *port = fio_cli_get_str("port");
  const uint32_t threads = fio_cli_get_int("t");
  const uint32_t workers = fio_cli_get_int("w");
  const char *public_folder = fio_cli_get_str("www");

  /*     ****  logging  ****     */

  if (public_folder) {
    fprintf(stderr, "* Serving static files from: %s\n", public_folder);
  }

  if (print_log) {
    /* log to the "benchmark.log" file, set to `if` to 0 to skip this*/
    if (1) {
      int old_stderr = dup(fileno(stderr));
      fclose(stderr);
      FILE *log = fopen("./tmp/hello_world.log", "a");
      if (!log) {
        fdopen(old_stderr, "a");
        fprintf(stdout,
                "* Failed to open logging file - logging to terminal.\n");
      } else {
        close(old_stderr);
        fprintf(stdout,
                "* All logging reports (stderr) routed to a log file at "
                "./tmp/hello_world.log\n");
        sock_open(fileno(log));
      }
    }
  }

  /*     ****  actual code ****     */

  SERVER_HEADER = fiobj_str_static("server", 6);
  SERVER_NAME = fiobj_strprintf("facil.io %u.%u.%u", FACIL_VERSION_MAJOR,
                                FACIL_VERSION_MINOR, FACIL_VERSION_PATCH);

  TEXT_TYPE = http_mimetype_find("txt", 3);

  // RedisEngine = redis_engine_create(.address = "localhost", .port = "6379");
  if (http_listen(port, NULL, .on_request = http_hello_on_request,
                  .log = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Hello World service"), exit(1);
  facil_run(.threads = threads, .processes = workers);

  fio_cli_end();
  fiobj_free(SERVER_HEADER);
  fiobj_free(SERVER_NAME);
  fiobj_free(TEXT_TYPE);
}
