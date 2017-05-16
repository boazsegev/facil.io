#include "defer.h"
#include "evio.h"
#include "sock.h"
void defer_test(void);

// a global server socket
intptr_t srvfd = -1;
// a global running flag
int run = 1;

// create the callback. This callback will be global and hardcoded,
// so there are no runtime function pointer resolutions.
void evio_on_data(void *arg) {
  intptr_t uuid = (intptr_t)arg;
  if (uuid == srvfd) {
    intptr_t new_client;
    // accept connections.
    while ((new_client = sock_accept(srvfd)) > 0) {
      // fprintf(stderr, "Accepted new connetion %lu\n", new_client);
      if (evio_add(sock_uuid2fd(new_client), (void *)new_client) == -1)
        perror("evio error");
    }
  } else {
    // fprintf(stderr, "Handling incoming data on %lu.\n", uuid);
    // handle data
    char data[1024];
    ssize_t len;
    while ((len = sock_read(uuid, data, 1024)) > 0) {
      // fprintf(stderr, "received %ld of data\n", len);
      if (sock_write(uuid,
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: 12\r\n"
                     "Connection: keep-alive\r\n"
                     "Keep-Alive: 1;timeout=5\r\n"
                     "\r\n"
                     "Hello World!",
                     100) == -1)
        perror("sock_write error");
    }
  }
}

void evio_on_close(void *arg) {
  // fprintf(stderr, "closed the connection for %lu\n", (uintptr_t)arg);
  sock_force_close((uintptr_t)arg);
}

void evio_on_ready(void *arg) { sock_flush((intptr_t)arg); }

void stop_signal(int sig) {
  run = 0;
  signal(sig, SIG_DFL);
}
int main() {
  sock_max_capacity();
  // sock_libtest();
  srvfd = sock_listen(NULL, "3000");
  signal(SIGINT, stop_signal);
  signal(SIGTERM, stop_signal);
  evio_create();
  evio_add(sock_uuid2fd(srvfd), (void *)srvfd);
  fprintf(stderr, "Starting event loop\n");
  while (run && evio_review() >= 0)
    defer_perform();
  fprintf(stderr, "\nGoodbye.\n");
}
