/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

#include <stdio.h>
#include <stdlib.h>

#define USE_HTTP_PROTOCOL 1
#define THREAD_COUNT 16

/**************************************
HttpProtocol (lib-server) "Hello World"
*/
#include "http.h"
void on_request(struct HttpRequest* request) {
  static char reply[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=2\r\n"
      "\r\n"
      "He11o World!";
  Server.write(request->server, request->sockfd, reply, sizeof(reply));
}

/**************************************
Lib Server "Hello World" (Http)
*/
#include "lib-server.h"

void on_data(server_pt srv, int fd) {
  static char reply[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=2\r\n"
      "\r\n"
      "Hello World!";
  char buff[1024];

  if (Server.read(srv, fd, buff, 1024)) {
    Server.write(srv, fd, reply, sizeof(reply));
  }
}

int main(int argc, char const* argv[]) {
  if (USE_HTTP_PROTOCOL) {
    start_http_server(on_request, NULL, .threads = THREAD_COUNT);
  } else {
    struct Protocol protocol = {.on_data = on_data};
    start_server(.protocol = &protocol, .timeout = 2, .threads = THREAD_COUNT);
  }
  return 0;
}
