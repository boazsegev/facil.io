#include "facil.h"
void defer_test(void);

#include <errno.h>

/* A callback to be called whenever data is available on the socket*/
static void echo_on_data(intptr_t uuid,  /* The socket */
                         protocol_s *prt /* pointer to the protocol */
                         ) {
  (void)prt; /* ignore unused argument */
  /* echo buffer */
  char buffer[1024] = {'E', 'c', 'h', 'o', ':', ' '};
  ssize_t len;
  /* Read to the buffer, starting after the "Echo: " */
  while ((len = sock_read(uuid, buffer + 6, 1018)) > 0) {
    /* Write back the message */
    sock_write(uuid, buffer, len + 6);
    /* Handle goodbye */
    if ((buffer[6] | 32) == 'b' && (buffer[7] | 32) == 'y' &&
        (buffer[8] | 32) == 'e') {
      sock_write(uuid, "Goodbye.\n", 9);
      sock_close(uuid);
      return;
    }
  }
}

/* A callback called whenever a timeout is reach (more later) */
static void echo_ping(intptr_t uuid, protocol_s *prt) {
  (void)prt; /* ignore unused argument */
  /* Read/Write is handled by `libsock` directly. */
  sock_write(uuid, "Server: Are you there?\n", 23);
}

/* A callback called if the server is shutting down...
... while the connection is open */
static void echo_on_shutdown(intptr_t uuid, protocol_s *prt) {
  (void)prt; /* ignore unused argument */
  sock_write(uuid, "Echo server shutting down\nGoodbye.\n", 35);
}

/* A callback called for new connections */
static protocol_s *echo_on_open(intptr_t uuid, void *udata) {
  (void)udata; /*ignore this */
  /* Protocol objects MUST always be dynamically allocated. */
  protocol_s *echo_proto = malloc(sizeof(*echo_proto));
  *echo_proto = (protocol_s){
      .service = "echo",
      .on_data = echo_on_data,
      .on_shutdown = echo_on_shutdown,
      .on_close = (void (*)(protocol_s *))free, /* simply free when done */
      .ping = echo_ping};

  sock_write(uuid, "Echo Service: Welcome\n", 22);
  facil_set_timeout(uuid, 5);
  return echo_proto;
}

static void on_time(void *arg) { fprintf(stderr, "%s\n", arg); }
static void on_stop(void *arg) { fprintf(stderr, "Timer stopped (%s)\n", arg); }

int main() {
  /* Setup a listening socket */
  if (facil_listen(.port = "8888", .on_open = echo_on_open))
    perror("No listening socket available on port 8888"), exit(-1);
  /* Run a timer. */
  facil_run_every(2000, 4, on_time, "* Timer...", on_stop);
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 4, .processes = 1);
}
