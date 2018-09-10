/* *****************************************************************************
This is a simple echo server example.

To try it out, compile using (avoids server state printout):

    FIO_PRINT=0 NAME=char make

Than run:

    ./tmp/char

To connect to this server run telnet, netcat or the client example, using:

    telnet localhost 3000
Or:
    nc localhost 3000

This example uses the core facil.io library (fio.h) and the Command Line
Interface library (fio_cli.h).
***************************************************************************** */

#include <fio.h>
#include <fio_cli.h>

/* *****************************************************************************
Chat connection callbacks
***************************************************************************** */

typedef struct {
  fio_protocol_s pr;
  subscription_s *s;
} chat_protocol_s;

// A callback to be called whenever data is available on the socket
static void chat_on_data(intptr_t uuid, fio_protocol_s *prt) {
  // echo buffer
  char buffer[1024] = {'C', 'h', 'a', 't', ':', ' '};
  ssize_t len;
  // Read to the buffer, starting after the "Chat: "
  while ((len = fio_read(uuid, buffer + 6, 1018)) > 0) {
    fprintf(stderr, "Broadcasting: %.*s", (int)len, buffer + 6);
    fio_publish(.message = {.data = buffer, .len = (len + 6)},
                .channel = {.data = "chat", .len = 4});
  }
  (void)prt; // we can ignore the unused argument
}

// A callback called whenever a timeout is reach
static void chat_ping(intptr_t uuid, fio_protocol_s *prt) {
  fio_write(uuid, "Server: Are you there?\n", 23);
  (void)prt; // we can ignore the unused argument
}

// A callback called if the server is shutting down...
// ... while the connection is still open
static uint8_t chat_on_shutdown(intptr_t uuid, fio_protocol_s *prt) {
  fio_write(uuid, "Chat server shutting down\nGoodbye.\n", 35);
  return 0;
  (void)prt; // we can ignore the unused argument
}

static void chat_on_close(intptr_t uuid, fio_protocol_s *proto) {
  fprintf(stderr, "Connection %p closed.\n", (void *)proto);
  chat_protocol_s *p = (chat_protocol_s *)proto;
  fio_unsubscribe(p->s);
  fio_free(proto);
  (void)uuid;
}

/* *****************************************************************************
The main chat pub/sub callback
***************************************************************************** */

static void chat_message(fio_msg_s *msg) {
  fio_write((intptr_t)msg->udata1, msg->msg.data, msg->msg.len);
}
/* *****************************************************************************
The main chat protocol creation callback
***************************************************************************** */

// A callback called for new connections
static void chat_on_open(intptr_t uuid, void *udata) {
  // Protocol objects MUST be dynamically allocated when multi-threading.
  chat_protocol_s *proto = fio_malloc(sizeof(*proto));
  fio_write2(uuid, .data.buffer = "Chat Service: Welcome\n", .length = 22,
             .after.dealloc = FIO_DEALLOC_NOOP);
  subscription_s *s =
      fio_subscribe(.on_message = chat_message, .udata1 = (void *)uuid,
                    .channel = {.data = "chat", .len = 4});
  *proto = (chat_protocol_s){
      .pr = {.on_data = chat_on_data,
             .on_shutdown = chat_on_shutdown,
             .on_close = chat_on_close,
             .ping = chat_ping},
      .s = s,
  };
  fprintf(stderr, "* (%d) new Connection %p received from %s\n", getpid(),
          (void *)proto, fio_peer_addr(uuid).data);
  fio_attach(uuid, &proto->pr);
  fio_set_timeout(uuid, 10);
  (void)udata; // ignore this
}

/* *****************************************************************************
The main function (listens to the `echo` connections and handles CLI)
***************************************************************************** */

// The main function starts listening to echo connections
int main(int argc, char const *argv[]) {
  /* Setup CLI arguments */
  fio_cli_start(argc, argv, 0, "this example accepts the following options:",
                "-t -thread number of threads to run.", FIO_CLI_TYPE_INT,
                "-w -workers number of workers to run.", FIO_CLI_TYPE_INT,
                "-b, -address the address to bind to.",
                "-p,-port the port to bind to.", FIO_CLI_TYPE_INT,
                "-v -log enable logging.", FIO_CLI_TYPE_BOOL);

  /* Setup default values */
  fio_cli_set_default("-p", "3000");
  fio_cli_set_default("-t", "1");
  fio_cli_set_default("-w", "1");

  /* Listen for connections */
  if (fio_listen(.port = fio_cli_get("-p"), .on_open = chat_on_open) == -1) {
    perror("No listening socket available on port 3000");
    exit(-1);
  }
  /* Run the server and hang until a stop signal is received */
  fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));
}
