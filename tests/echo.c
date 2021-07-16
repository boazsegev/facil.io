/* when compiling tests this is easier... */
#include "../lib/fio.h"
#ifndef TEST_WITH_LIBRARY
#include "../lib/fio.c"
#endif

/* the default timeout for the echo protocol. */
#define TIMEOUT 15

/* running the program */
int main(int argc, char const *argv[]);
/* Called when a new connection arrived (used for allocation) */
FIO_SFUNC void echo_on_open(fio_uuid_s *uuid, void *udata);
/* Called when data is available on the socket (uuid). */
FIO_SFUNC void echo_on_data(fio_uuid_s *uuid, void *udata);
/* Called when the server is shutting down, before the uuid is closed. */
FIO_SFUNC uint8_t echo_on_shutdown(fio_uuid_s *uuid, void *udata);
/* Called after the connection was closed, for object cleanup. */
FIO_SFUNC void echo_on_close(void *udata);
/* Called if no data was received in a while and TIMEOUT occured. */
FIO_SFUNC void echo_ping(fio_uuid_s *uuid, void *udata);

static fio_protocol_s ECHO_PROTOCOL = {
    .on_data = echo_on_data,
    .on_shutdown = echo_on_shutdown,
    .on_close = echo_on_close,
    .on_timeout = echo_ping,
    .timeout = 10,
};

#define FIO_STR_NAME echo_str
#include "fio-stl.h"

/* *****************************************************************************
Main
***************************************************************************** */

int main(int argc, char const *argv[]) {
  /* setup CLI options */
  fio_cli_start(
      argc,
      argv,
      0,
      0,
      "This is an echo server example.",
      FIO_CLI_STRING("--binding -b a URL with the binding to use. i.e.: "
                     "tcp://0.0.0.0:3000 "),
      FIO_CLI_INT("--workers -w (0) The number of worker processes to spawn."),
      FIO_CLI_PRINT("Negative values are fractions of CPU core count."),
      FIO_CLI_PRINT(
          "A zero value (0) will run facil.io in single process mode."),
      FIO_CLI_INT(
          "--threads -t (1) The number of worker threads per worker process."),
      FIO_CLI_PRINT("Negative values are fractions of CPU core count."),
      FIO_CLI_BOOL("--verbose -V Turns on debug level messages."));
  if (fio_cli_get_bool("-V"))
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;

  /* NULL will result in the default address and port (NULL): 0.0.0.0:3000 */
  fio_listen(fio_cli_get("-b"), .on_open = echo_on_open);
  /* start the reactor */
  fio_start(.workers = fio_cli_get_i("-w"), .threads = fio_cli_get_i("-t"));
  /* free CLI memory */
  fio_cli_end();
  return 0;
}

/* *****************************************************************************
Accepting new connections
***************************************************************************** */

FIO_SFUNC void echo_on_open(fio_uuid_s *uuid, void *udata) {
  /* attach the protocol to the UUID, or it might be closed automatically. */
  fio_attach(uuid, &ECHO_PROTOCOL);
  fio_udata_set(uuid, echo_str_new());
  /* log it all */
  FIO_LOG_INFO("echo connection open at %p", (void *)uuid);
  (void)udata; /* unused */
}

/* *****************************************************************************
Echo protocol callbacks
***************************************************************************** */

/* Called when data is available on the socket (uuid). */
FIO_SFUNC void echo_on_data(fio_uuid_s *uuid, void *udata) {
  /* read the data from the socket  */
  char mem[1024];
  ssize_t r = fio_read(uuid, mem, 1018);
  if (r <= 0)
    return;

  echo_str_s *str = udata;
  /* write the data to the end of the existing buffer - support fragmentation */
  fio_str_info_s s = echo_str_write(str, mem, (size_t)r);
  char *end = s.buf;
  char *start = s.buf;

  /* consume the unified buffer line by line */
  while (s.buf + s.len > start && (end = memchr(start, '\n', s.len))) {
    size_t len = (size_t)(end - start);
    if (len) {
      FIO_LOG_DEBUG("echoing: %.*s", (int)(len), start);
      /* consume the trailing '\n' */
      ++len;
      /* compose the message on a locally allocated buffer */
      char *tmp = fio_malloc(len + 6);
      memcpy(tmp, "ECHO: ", 6);
      memcpy(tmp + 6, start, len);
      /* send the message by transferring ownership of the allocated buffer */
      fio_write2(uuid, .data.buf = tmp, .len = len + 6, .dealloc = fio_free);
    }
    /* seek immediately after the '\n' character  */
    start = end + 1;
  }

  /* are there any fragmented lines? */
  if (s.buf + s.len <= start) {
    /* all data was consumed, keep string's buffer, just reset it's size. */
    echo_str_resize(str, 0);
  } else {
    /* move the fragmented line to the beginning of the buffer  */
    memmove(s.buf, start, (s.len + s.buf) - start);
    echo_str_resize(str, (s.len + s.buf) - start);
    /* are we under attack? more than 64Kb with no new lines markers? */
    if (echo_str_len(str) > 65536) {
      fio_write(uuid, "Server: attack pattern detected! Goodbye!\n", 42);
      echo_str_destroy(str);
      fio_close(uuid);
    }
  }
}

/* Called when the server is shutting down, before the uuid is closed. */
FIO_SFUNC uint8_t echo_on_shutdown(fio_uuid_s *uuid, void *udata) {
  fio_write(uuid, "SERVER: Server shutting down. Goodbye.\n", 39);
  (void)udata; /* unused */
  return 0;
}

/* Called after the connection was closed, for object cleanup. */
FIO_SFUNC void echo_on_close(void *udata) {
  echo_str_destroy(udata);
  FIO_LOG_INFO("echo connection closed.");
}

/* Called if no data was received in a while and TIMEOUT occurred. */
FIO_SFUNC void echo_ping(fio_uuid_s *uuid, void *udata) {
  fio_write(uuid, "SERVER: Are you there?\n", 23);
  (void)protocol; /* unused */
}
