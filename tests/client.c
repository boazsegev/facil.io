/*
This is a simple REPL client example, similar to netcat but simpler.

Data is read from STDIN (which is most of the code) and sent as is, including
the EOL (end of line) character(s).

To try it out, compile using (avoids server state printout):

    NAME=client make

Than run:

    ./tmp/client localhost 3000

*/
/* when compiling tests this is easier... */
#ifdef TEST_WITH_LIBRARY
#include "fio.h"
#else
#include "fio.c"
#endif

#define MAX_BYTES_RAPEL_PER_CYCLE 2048
#define MAX_BYTES_READ_PER_CYCLE  4096
#define PUBSUB_FILTER_NUMBER      0
/* *****************************************************************************
REPL
***************************************************************************** */

static void repl_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  ssize_t ret = 0;
  char buffer[MAX_BYTES_RAPEL_PER_CYCLE];
  ret = fio_read(uuid, buffer, MAX_BYTES_RAPEL_PER_CYCLE);
  if (ret > 0) {
    FIO_LOG_DEBUG("REPL data available, read %zd bytes", ret);
    fio_publish(.filter = PUBSUB_FILTER_NUMBER,
                .channel = {.buf = "repl, but only if filter == 0", .len = 4},
                .message = {.buf = buffer, .len = ret});
  }
  (void)protocol; /* we ignore the protocol object, we don't use it */
}

static void repl_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  FIO_LOG_DEBUG("REPL stopped");
  (void)uuid;     /* we ignore the uuid object, we don't use it */
  (void)protocol; /* we ignore the protocol object, we don't use it */
}

static fio_protocol_s repel_protocol = {
    .on_data = repl_on_data,
    .on_close = repl_on_close,
    .on_timeout = FIO_PING_ETERNAL,
};

static void repl_attach(void) {
  /* wait for new lines in REPL? uncomment to avoid waiting. */
  // system("stty raw");
  /* Attach REPL */
  fio_set_non_block(fileno(stdin));
  fio_attach_fd(fileno(stdin), &repel_protocol);
}

/* *****************************************************************************
TCP/IP / Unix Socket Client
***************************************************************************** */

static void on_data(intptr_t uuid, fio_protocol_s *protocol) {
  ssize_t ret = 0;
  char buffer[MAX_BYTES_READ_PER_CYCLE + 1];
  ret = fio_read(uuid, buffer, MAX_BYTES_READ_PER_CYCLE);
  while (ret > 0) {
    FIO_LOG_DEBUG("Recieved %zu bytes", ret);
    buffer[ret] = 0;
    fwrite(buffer, ret, 1, stdout); /* NUL bytes on binary streams are normal */
    fflush(stdout);
    ret = fio_read(uuid, buffer, MAX_BYTES_READ_PER_CYCLE);
  }

  (void)protocol; /* we ignore the protocol object, we don't use it */
}

/* Called during server shutdown */
static uint8_t on_shutdown(intptr_t uuid, fio_protocol_s *protocol) {
  FIO_LOG_INFO("Disconnecting.\n");
  /* don't print a message on protocol closure */
  protocol->on_close = NULL;
  return 0;   /* close immediately, don't wait */
  (void)uuid; /*we ignore the uuid object, we don't use it*/
}

/** Called when the connection was closed, but will not run concurrently */
static void on_close(intptr_t uuid, fio_protocol_s *protocol) {
  FIO_LOG_INFO("Remote connection lost.\n");
  fio_stop();     /* signal facil.io to stop */
  (void)protocol; /* we ignore the protocol object, we don't use it */
  (void)uuid;     /* we ignore the uuid object, we don't use it */
}

/** Timeout handling. To ignore timeouts, we constantly "touch" the socket */
static void ping(intptr_t uuid, fio_protocol_s *protocol) {
  fio_touch(uuid);
  (void)protocol; /* we ignore the protocol object, we don't use it */
}

/*
 * Since we have only one connection and a single thread, we can use a static
 * protocol object (otherwise protocol objects should be dynamically allocated).
 */
static fio_protocol_s client_protocol = {
    .on_data = on_data,
    .on_shutdown = on_shutdown,
    .on_close = on_close,
    .on_timeout = ping,
};

/* Forward REPL messages to the socket - pub/sub callback */
static void on_repl_message(fio_msg_s *msg) {
  fio_write(msg->uuid, msg->msg.buf, msg->msg.len);
  fprintf(stderr, "SENT %d bytes\n", (int)msg->msg.len);
}

static void on_connect(intptr_t uuid, void *udata) {
  fio_attach(uuid, &client_protocol);
  /* subscribe to REPL, linking the subscription with the connection's UUID  */
  fio_subscribe(.filter = PUBSUB_FILTER_NUMBER,
                .channel = {.buf = "repl, but only if filter == 0", .len = 4},
                .on_message = on_repl_message,
                .uuid = uuid);

  /* start REPL */
  // void *repl = fio_thread_new(repl_thread, (void *)uuid);
  // fio_state_callback_add(FIO_CALL_AT_EXIT, repl_thread_cleanup, repl);
  (void)udata; /* we ignore the udata pointer, we don't use it here */
}

static void on_finish(void *udata) {
  FIO_LOG_INFO("Connection closed...\n");
  fio_stop();  /* signal facil.io to stop */
  (void)udata; /* we ignore the udata object, we don't use it */
}

/* *****************************************************************************
Main
***************************************************************************** */

int main(int argc, char const *argv[]) {
  /* Setup CLI arguments */
  fio_cli_start(argc,
                argv,
                1,
                1,
                "use:\n\tclient <args> URL\n",
                FIO_CLI_BOOL("-tls use TLS to establish a secure connection."),
                FIO_CLI_STRING("-tls-alpn set the ALPN extension for TLS."),
                FIO_CLI_STRING("-trust comma separated list of PEM "
                               "files for TLS verification."),
                FIO_CLI_INT("-v -verbousity sets the verbosity level 0..5 (5 "
                            "== debug, 0 == quite)."));

  /* set logging level */
  FIO_LOG_LEVEL = FIO_LOG_LEVEL_ERROR;
  if (fio_cli_get("-v") && fio_cli_get_i("-v") >= 0)
    FIO_LOG_LEVEL = fio_cli_get_i("-v");

  /* Manage TLS */
  fio_tls_s *tls = NULL;
  if (fio_cli_get_bool("-tls")) {
    tls = fio_tls_new(NULL, NULL, NULL, NULL);
    if (fio_cli_get("-trust")) {
      const char *trust = fio_cli_get("-trust");
      size_t len = strlen(trust);
      const char *end = memchr(trust, ',', len);
      while (end) {
        /* copy partial string to attach NUL char at end of file name */
        fio_str_s tmp = FIO_STR_INIT;
        fio_str_info_s t = fio_str_write(&tmp, trust, end - trust);
        fio_tls_trust(tls, t.buf);
        fio_str_free(&tmp);
        len -= (end - trust) + 1;
        trust = end + 1;
        end = memchr(trust, ',', len);
      }
      fio_tls_trust(tls, trust);
    }
    if (fio_cli_get("-tls-alpn")) {
      fio_tls_alpn_add(tls, fio_cli_get("-tls-alpn"), NULL, NULL, NULL);
    }
  }

  /* Attach REPL */
  repl_attach();

  /* Connect */
  fio_connect_s *c = fio_connect(.url = fio_cli_unnamed(0),
                                 .on_open = on_connect,
                                 .on_finish = on_finish,
                                 .tls = tls);
  if (!c && fio_cli_get_bool("-v"))
    FIO_LOG_ERROR("Connection can't be established");
  else
    fio_start(.threads = 1);
  if (tls)
    fio_tls_free(tls);
  fio_cli_end();
}
