/*
This is a simple REPL client example, similar to netcat but simpler.

Data is read from STDIN (which is most of the code) and sent as is, including
the EOL (end of line) character(s).

To try it out, compile using (avoids server state printout):

    FIO_PRINT=0 NAME=client make

Than run:

    ./tmp/client localhost 3000

*/
#include "fio.h"
#include "fio_cli.h"
#include "fio_tls.h"

/* add the fio_str_s helpers */
#define FIO_INCLUDE_STR
#include "fio.h"

/* *****************************************************************************
REPL
***************************************************************************** */

#define MAX_BYTES_READ_PER_CYCLE 16

void *repl_thread(void *uuid_) {
  fprintf(stderr, "Connected, start typing.\n");
  /* collect the uuid variable */
  intptr_t uuid = (intptr_t)uuid_;
  /* We'll use a dynamic string for the buffer cache */
  fio_str_s buffer = FIO_STR_INIT;
  fio_str_info_s info = fio_str_info(&buffer);
  while (fio_is_valid(uuid)) {
    char *marker = NULL;
    /* require at least 256 free characters */
    if (info.capa - info.len < MAX_BYTES_READ_PER_CYCLE) {
      info = fio_str_capa_assert(&buffer, info.len + MAX_BYTES_READ_PER_CYCLE);
    }
    /* read from STDIN */
    int act =
        read(fileno(stdin), info.data + info.len, MAX_BYTES_READ_PER_CYCLE);
    if (act <= 0) {
      FIO_LOG_ERROR("REPL read error, disconnecting.\n");
      fio_close(uuid);
      break;
    }
    /* update the buffer data */
    info = fio_str_resize(&buffer, info.len + act);
  seek_eol:
    marker = memchr(info.data, '\n', info.len);
    if (marker) {
      /* send data to uuid */
      ++marker;
      fio_write(uuid, info.data, marker - info.data);
      /* test for leftover data and handle it */
      if (info.data + info.len == marker) {
        info = fio_str_resize(&buffer, 0);
      } else {
        memmove(info.data, marker, (info.data + info.len) - marker);
        info = fio_str_resize(&buffer, (info.data + info.len) - marker);
        goto seek_eol;
      }
    }
  }
  fio_str_free(&buffer);
  FIO_LOG_INFO("stopping REPL\n");
  return NULL;
}

/* *****************************************************************************
TCP/IP / Unix Socket Client
***************************************************************************** */

static void on_data(intptr_t uuid, fio_protocol_s *protocol) {
  ssize_t ret = 0;
  char buffer[MAX_BYTES_READ_PER_CYCLE + 1];
  ret = fio_read(uuid, buffer, MAX_BYTES_READ_PER_CYCLE);
  while (ret > 0) {
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
  kill(0, SIGINT); /* signal facil.io to stop */
  (void)protocol;  /* we ignore the protocol object, we don't use it */
  (void)uuid;      /* we ignore the uuid object, we don't use it */
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
    .ping = ping,
};

static void on_connect(intptr_t uuid, void *udata) {
  if (udata) // TLS support, udata is the TLS context.
    fio_tls_connect(uuid, udata, NULL);

  fio_attach(uuid, &client_protocol);
  /* start REPL */
  fio_thread_free(fio_thread_new(repl_thread, (void *)uuid));
  (void)udata; /* we ignore the udata pointer, we don't use it here */
}

static void on_fail(intptr_t uuid, void *udata) {
  FIO_LOG_ERROR("Connection failed\n");
  kill(0, SIGINT); /* signal facil.io to stop */
  (void)uuid;      /* we ignore the uuid object, we don't use it */
  (void)udata;     /* we ignore the udata object, we don't use it */
}

/* *****************************************************************************
Main
***************************************************************************** */

int main(int argc, char const *argv[]) {
  /* Setup CLI arguments */
  fio_cli_start(argc, argv, 1, 2, "use:\n\tclient <args> hostname port\n",
                FIO_CLI_BOOL("-tls use TLS to establish a secure connection."),
                FIO_CLI_STRING("-tls-alpn set the ALPN extension for TLS."),
                FIO_CLI_STRING("-trust comma separated list of PEM "
                               "certification files for TLS verification."),
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
        fio_tls_trust(tls, t.data);
        fio_str_free(&tmp);
        len -= (end - trust) + 1;
        trust = end + 1;
        end = memchr(trust, ',', len);
      }
      fio_tls_trust(tls, trust);
    }
    if (fio_cli_get("-tls-alpn")) {
      fio_tls_proto_add(tls, fio_cli_get("-tls-alpn"), NULL);
    }
  }

  /* Log connection attempt */
  if (fio_cli_unnamed_count() == 1 || fio_cli_unnamed(1)[0] == 0 ||
      (fio_cli_unnamed(1)[0] == '0' || fio_cli_unnamed(1)[1] == 0)) {
    FIO_LOG_INFO("Attempting to connect to Unix socket at: %s\n",
                 fio_cli_unnamed(0));
  } else {
    FIO_LOG_INFO("Attempting to connect to TCP/IP socket at: %s:%s\n",
                 fio_cli_unnamed(0), fio_cli_unnamed(1));
  }

  intptr_t uuid =
      fio_connect(.address = fio_cli_unnamed(0), .port = fio_cli_unnamed(1),
                  .on_connect = on_connect, .on_fail = on_fail, .udata = tls);
  if (uuid == -1 && fio_cli_get_bool("-v"))
    FIO_LOG_ERROR("Connection can't be established");
  else
    fio_start(.threads = 1);
  fio_tls_destroy(tls);
  fio_cli_end();
}
