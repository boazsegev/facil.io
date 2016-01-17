#include "lib-server.h"
#include <stdio.h>
#include <string.h>

// we don't have to, but printing stuff is nice...
void on_open(server_pt server, int sockfd) {
  printf("A connection was accepted on socket #%d.\n", sockfd);
}
// server_pt is just a nicely typed pointer, which is there for type-safty.
// The Server API is used by calling `Server.api_call` (often with the pointer).
void on_close(server_pt server, int sockfd) {
  printf("Socket #%d is now disconnected.\n", sockfd);
}

// a simple echo... this is the main callback
void on_data(server_pt server, int sockfd) {
  // We'll assign a reading buffer on the stack
  char buff[1024];
  ssize_t incoming = 0;
  // Read everything, this is edge triggered, `on_data` won't be called
  // again until all the data was read.
  while ((incoming = Server.read(sockfd, buff, 1024)) > 0) {
    // since the data is stack allocated, we'll write a copy
    // otherwise, we'd avoid a copy using Server.write_move
    Server.write(server, sockfd, buff, incoming);
    if (!memcmp(buff, "bye", 3)) {
      // closes the connection automatically AFTER all the buffer was sent.
      Server.close(server, sockfd);
    }
  }
}

// running the server
int main(void) {
  // We'll create the echo protocol object. It will be the server's default
  struct Protocol protocol = {.on_open = on_open,
                              .on_close = on_close,
                              .on_data = on_data,
                              .service = "echo"};
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = &protocol, .timeout = 10, .threads = 8);
}

// easy :-)
