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

/* add the fio_str_s helpers */
#define FIO_INCLUDE_STR
#include "fio.h"

/* *****************************************************************************
REPL
***************************************************************************** */

#define MAX_BYTES_READ_PER_CYCLE 16

void *repl_thread(void *uuid_) {
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
      fprintf(stderr, "REPL read error, disconnecting.\n");
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
  if (fio_cli_get_bool("-v"))
    printf("stopping REPL\n");
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
    printf("%s", buffer); /* NUL bytes on binary streams are normal */
    ret = fio_read(uuid, buffer, MAX_BYTES_READ_PER_CYCLE);
  }

  (void)protocol; /* we ignore the protocol object, we don't use it */
}

/* Called during server shutdown */
static uint8_t on_shutdown(intptr_t uuid, fio_protocol_s *protocol) {
  printf("Disconnecting.\n");
  /* don't print a message on protocol closure */
  protocol->on_close = NULL;
  return 0;   /* close immediately, don't wait */
  (void)uuid; /*we ignore the uuid object, we don't use it*/
}

/** Called when the connection was closed, but will not run concurrently */
static void on_close(intptr_t uuid, fio_protocol_s *protocol) {
  printf("Remote connection lost.\n");
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
  fio_attach(uuid, &client_protocol);
  /* start REPL */
  fio_thread_free(fio_thread_new(repl_thread, (void *)uuid));
  (void)udata; /* we ignore the udata pointer, we don't use it here */
}

static void on_fail(intptr_t uuid, void *udata) {
  if (fio_cli_get_bool("-v"))
    printf("Connection failed to %s\n", (char *)udata);
  kill(0, SIGINT); /* signal facil.io to stop */
  (void)uuid;      /* we ignore the uuid object, we don't use it */
}

/* *****************************************************************************
Main
***************************************************************************** */

int main(int argc, char const *argv[]) {
  /* Setup CLI arguments */
  fio_cli_start(argc, argv, 1, "use:\n\tclient <args> hostname port",
                "-v -verbous mode.", FIO_CLI_TYPE_BOOL);
  if (fio_cli_unknown_count() == 0 || fio_cli_unknown_count() > 2) {
    printf("Argument error. For help type:   client -?\n");
    exit(0);
  }
  if (fio_cli_get_bool("-v")) {
    if (fio_cli_unknown_count() == 1) {
      printf("Attempting to connect to Unix socket at: %s\n",
             fio_cli_unknown(0));
    } else {
      printf("Attempting to connect to TCP/IP socket at: %s:%s\n",
             fio_cli_unknown(0), fio_cli_unknown(1));
    }
  }
  intptr_t uuid =
      fio_connect(.address = fio_cli_unknown(0), .port = fio_cli_unknown(1),
                  .on_connect = on_connect, .on_fail = on_fail,
                  .udata = (void *)fio_cli_unknown(0));
  if (uuid == -1 && fio_cli_get_bool("-v"))
    perror("Connection can't be established");
  else
    fio_start(.threads = 1);
  fio_cli_end();
}
