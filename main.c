#include "facil.h"
#include "http.h"

// A callback to be called whenever data is available on the socket
static void echo_on_data(intptr_t uuid, protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  // echo buffer
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  // Read to the buffer, starting after the "Echo: "
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    // Write back the message
    sock_write(uuid, buffer, len + 6);
    sock_write(uuid, buffer, len + 6);
    // Handle goodbye
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

// A callback called whenever a timeout is reach
static void echo_ping(intptr_t uuid, protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  sock_write(uuid, "Server: Are you there?\n", 23);
}

// A callback called if the server is shutting down...
// ... while the connection is still open
static void echo_on_shutdown(intptr_t uuid, protocol_s *prt) {
  (void)prt; // we can ignore the unused argument
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}

// A callback called for new connections
static protocol_s *echo_on_open(intptr_t uuid, void *udata) {
  (void)udata; // ignore this
  // Protocol objects MUST always be dynamically allocated.
  protocol_s *echo_proto = malloc(sizeof(*echo_proto));
  *echo_proto = (protocol_s){
      .service = "echo",
      .on_data = echo_on_data,
      .on_shutdown = echo_on_shutdown,
      .on_close = (void (*)(protocol_s *))free, // simply free when done
      .ping = echo_ping};

  sock_write(uuid, "Echo Service: Welcome\n", 22);
  facil_set_timeout(uuid, 5);
  return echo_proto;
}

//////////////////////

static void http1_hello_on_request(http_request_s *request) {
  // fprintf(stderr, "CALLED\n");
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1; timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  sock_write(request->fd, hello_message, sizeof(hello_message) - 1);
}
int main() {
  if (facil_listen(.port = "8888", .on_open = echo_on_open))
    perror("No listening socket available on port 8888"), exit(-1);
  // .public_folder = ".",
  http_listen("3000", NULL, .on_request = http1_hello_on_request);
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 1, .processes = 1);
}
