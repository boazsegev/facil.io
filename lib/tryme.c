/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

// #include <stdio.h>
//
// int main(int argc, char const* argv[]) {
//   printf("Hi!\n");
//   return 0;
// }

#include "lib-server.h"
#include "http-protocol.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
// #include <signal.h>
// #include <errno.h>

void (*org_on_request)(struct HttpRequest* req);
void on_request(struct HttpRequest* req) {
  // // sleep(1);
  // // busy wait for up to 1 seconds
  // time_t start_time, now_time;
  // time(&start_time);
  // time(&now_time);
  // while (now_time == start_time) {
  //   time(&now_time);
  // }
  // org_on_request(req);
  static char hello[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout = 1\r\n"
      "\r\n"
      "Hello World\r\n";
  Server.write(req->server, req->sockfd, hello, sizeof(hello));
}

// running the server
int main(void) {
  // We'll create the echo protocol object.
  // We'll use the server's default `on_request` callback (echo).
  struct HttpProtocol protocol = HttpProtocol();
  org_on_request = protocol.on_request;
  protocol.on_request = on_request;
  protocol.public_folder = "/Users/2Be/Documents/Scratch";
  // struct Protocol protocol = {.on_data = on_data, .on_close = on_close};
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = (struct Protocol*)(&protocol), .timeout = 1,
               .threads = 8);
}
