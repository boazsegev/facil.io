
#ifndef BROADCAST_PROTOCOL
#include "libserver.h"
#include <string.h>

typedef struct {
  size_t length;
  char str[];
} broadcast_message_s;

void broadcast_send(intptr_t uuid, protocol_s* prt, void* _msg) {
  broadcast_message_s* msg = _msg;
  sock_write(uuid, msg->str, msg->length);
}

void broadcast_send_clear(intptr_t uuid, protocol_s* prt, void* _msg) {
  fprintf(stderr, "* Completed a broadcast.\n");
  free(_msg);
}

static void broadcast_on_data(intptr_t uuid, protocol_s* prt) {
  char buffer[1024] = {'S', 'e', 'n', 't', ':', ' '};
  ssize_t len;
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    sock_write(uuid, buffer, len + 6);
    {
      broadcast_message_s* msg = malloc(len + 3 + sizeof(broadcast_message_s));
      msg->length = len + 2;
      msg->str[0] = '>';
      msg->str[1] = ' ';
      memcpy(msg->str + 2, buffer + 6, len);
      // msg->str[msg->length] = '\n';
      server_each(uuid, prt->service, broadcast_send, msg,
                  broadcast_send_clear);
    }
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

static void broadcast_ping(intptr_t uuid, protocol_s* prt) {
  sock_write(uuid, "Server: Are you there?\n", 23);
}

static void broadcast_on_shutdown(intptr_t uuid, protocol_s* prt) {
  sock_write(uuid, "Broadcast server shutting down\nGoodbye.\n", 50);
}

 static inline protocol_s* broadcast_on_open(intptr_t uuid,
                                                     void* udata) {
  static protocol_s broadcast_proto = {.service = "broadcast",
                                       .on_data = broadcast_on_data,
                                       .on_shutdown = broadcast_on_shutdown,
                                       .ping = broadcast_ping};
  sock_write(uuid, "Broadcast Service: Welcome\n", 27);
  server_set_timeout(uuid, 10);
  return &broadcast_proto;
}

#define BROADCAST_PROTOCOL broadcast_on_open
#endif
