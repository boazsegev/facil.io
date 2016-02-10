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

// running the server
int main(void) {
  // We'll create the echo protocol object.
  // We'll use the server's default `on_request` callback (echo).
  struct HttpProtocol protocol = HttpProtocol();
  // struct Protocol protocol = {.on_data = on_data, .on_close = on_close};
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = (struct Protocol*)(&protocol), .timeout = 1,
               .threads = 4);
}
