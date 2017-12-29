#include "facil.h" // the core library header

// Performed whenever there's pending incoming data on the socket
static void perform_echo(intptr_t uuid, protocol_s *prt) {
  (void)prt;
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    sock_write(uuid, buffer, len + 6);
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid); // closes after `write` had completed.
      return;
    }
  }
}
// performed whenever "timeout" is reached.
static void echo_ping(intptr_t uuid, protocol_s *prt) {
  (void)prt;
  sock_write(uuid, "Server: Are you there?\n", 23);
}
// performed during server shutdown, before closing the socket.
static void echo_on_shutdown(intptr_t uuid, protocol_s *prt) {
  (void)prt;
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}
// performed after the socket was closed and the currently running task had
// completed.
static void destroy_echo_protocol(intptr_t old_uuid, protocol_s *echo_proto) {
  if (echo_proto) // always error check, even if it isn't needed.
    free(echo_proto);
  fprintf(stderr, "Freed Echo protocol at %p\n", (void *)echo_proto);
  (void)old_uuid;
}
// performed whenever a new connection is accepted.
static void create_echo_protocol(intptr_t uuid, void *arg) {
  // create a protocol object
  protocol_s *echo_proto = malloc(sizeof(*echo_proto));
  // set the callbacks
  *echo_proto = (protocol_s){
      .service = "echo",
      .on_data = perform_echo,
      .on_shutdown = echo_on_shutdown,
      .ping = echo_ping,
      .on_close = destroy_echo_protocol,
  };
  // write data to the socket and set timeout
  sock_write(uuid, "Echo Service: Welcome. Say \"bye\" to disconnect.\n", 48);
  facil_set_timeout(uuid, 10);
  facil_attach(uuid, echo_proto);
  // print log
  fprintf(stderr, "New Echo connection %p for socket UUID %p\n",
          (void *)echo_proto, (void *)uuid);
  // return the protocol object to attach it to the socket.
  (void)arg; // we don't use this
}
// creates and runs the server
int main(void) {
  // listens on port 3000 for echo services.
  facil_listen(.port = "3000", .on_open = create_echo_protocol);
  // starts and runs the server
  facil_run(.threads = 10);
  return 0;
}
