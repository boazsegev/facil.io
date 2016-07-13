
#ifndef ECHO_PROTOCOL
#include "libserver.h"

static void echo_on_data(intptr_t uuid, protocol_s* prt) {
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    sock_write(uuid, buffer, len + 6);
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

static void echo_ping(intptr_t uuid, protocol_s* prt) {
  sock_write(uuid, "Server: Are you there?\n", 23);
}

static void echo_on_shutdown(intptr_t uuid, protocol_s* prt) {
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}

__unused static inline protocol_s* echo_on_open(intptr_t uuid, void* _) {
  static protocol_s echo_proto = {.on_data = echo_on_data,
                                  .on_shutdown = echo_on_shutdown,
                                  .ping = echo_ping};
  sock_write(uuid, "Echo Service: Welcome\n", 22);
  server_set_timeout(uuid, 10);
  return &echo_proto;
}

#define ECHO_PROTOCOL echo_on_open
#endif
