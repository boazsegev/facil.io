
#ifndef NOHTTP_HELLO
#include "facil.h"

static void nohttp_on_data(intptr_t uuid, protocol_s *prt) {
  // A simple Hello World HTTP response emulation
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1;timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  char buffer[1024];

  if (sock_read(uuid, buffer, 1024) > 0) {
    // write(sock_uuid2fd(uuid), hello_message, sizeof(hello_message) - 1);
    sock_write(uuid, hello_message, sizeof(hello_message) - 1);
  }
  (void)prt;
}

static protocol_s *nohttp_on_open(intptr_t uuid, void *ig) {
  static protocol_s nohttp_proto = {
      .on_data = nohttp_on_data,
  };
  facil_set_timeout(uuid, 4);
  return &nohttp_proto;
  (void)ig;
}

void listen2mockhttp(const char *port) {
  if (facil_listen(.on_open = nohttp_on_open, .port = port))
    perror("Couldn't initiate Mock HTTP service"), exit(1);
}

#endif
