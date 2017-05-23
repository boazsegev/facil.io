#ifndef HTTP_HELLO
#define HAS_UNIX_FEATURES
#include "http.h"
#include "strings.h"

#include "misc.h"

/*
A simple Hello World + static file service + heavier memory response for memory
stree testing.

I use the following Ruby script over night:

def cycle
  puts `wrk -c4000 -d5 -t12 http://localhost:3000/`
  sleep(3)
  puts `wrk -c4000 -d5 -t12 http://localhost:3000/bo.jpg`
  sleep(3)
  puts `wrk -c4000 -d5 -t12 http://localhost:3000/stress`
   true
end
sleep(10) while cycle

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
static void http_stress_request(http_request_s *request) {
  http_response_s *rs = http_response_create(request);
  if (!rs) {
    perror("ERROR: WTF?! No Memory? ");
    return;
  }
  // http_response_log_start(rs);
  if (request->path_len == 7 && !strncasecmp(request->path, "/stress", 7)) {
    fdump_s *data = bscrypt_fdump("public_www/bo.jpg", 0);
    if (!data) {
      perror("ERROR: couldn't open file");
      rs->status = 500;
      http_response_write_body(rs, "Internal error, please see log.", 12);
      goto done;
    }
    http_response_write_body(rs, data->data, data->length);
    free(data);
  } else
    http_response_write_body(rs, "Hello World!", 12);
done:
  http_response_finish(rs);
}

void listen2stress(const char *port, const char *public_folder) {
  http_listen(port, NULL, .public_folder = public_folder,
              .on_request = http_stress_request, .log_static = 0);
}
#endif
