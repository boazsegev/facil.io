/* when compiling tests this is easier... */
#ifdef TEST_WITH_LIBRARY
#include "fio.h"
#else
#include "fio.c"
#endif
#define FIO_STR_NAME             chat_str
#define FIO_REF_NAME             chat_str
#define FIO_REF_CONSTRUCTOR_ONLY 1
#include "fio-stl.h"

/* the default timeout for the chat protocol. */
#define TIMEOUT 15

/* running the program */
int main(int argc, char const *argv[]);
/* Called when a new connection arrived (used for allocation) */
FIO_SFUNC void chat_on_open(int fd, void *udata);
/* Called when a connection was attached to the protocol */
FIO_SFUNC void chat_on_attach(fio_s *io);
/* Called when a chat message is ready to be pushed to the connection */
FIO_SFUNC void chat_on_message(fio_msg_s *msg);
/* Called when data is available on the socket (io). */
FIO_SFUNC void chat_on_data(fio_s *io);
/* Called when the server is shutting down, before the io is closed. */
FIO_SFUNC void chat_on_shutdown(fio_s *io);
/* Called after the connection was closed, for object cleanup. */
FIO_SFUNC void chat_on_close(void *udata);
/* Called if no data was received in a while and TIMEOUT occurred. */
FIO_SFUNC void chat_ping(fio_s *io);

fio_protocol_s CHAT_PROTOCOL = {
    .on_attach = chat_on_attach,
    .on_data = chat_on_data,
    .on_shutdown = chat_on_shutdown,
    .on_close = chat_on_close,
    .on_timeout = chat_ping,
    .timeout = TIMEOUT,
};
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
      "This is a raw (TCP/IP) chat server example.",
      FIO_CLI_STRING("--binding -b a URL with the binding to use. i.e.: "
                     "tcp://0.0.0.0:3000 "),
      FIO_CLI_INT("--workers -w (0) The number of worker processes to spawn."),
      FIO_CLI_PRINT("Negative values are fractions of CPU core count."),
      FIO_CLI_PRINT(
          "A zero value (0) will run facil.io in single process mode."),
      FIO_CLI_INT(
          "--threads -t (0) The number of worker threads per worker process."),
      FIO_CLI_PRINT("Negative values are fractions of CPU core count."));

  /* NULL will result in the default address and port (NULL): 0.0.0.0:3000 */
  fio_listen(fio_cli_get("-b"), .on_open = chat_on_open);
  /* start the reactor */
  fio_start(.workers = fio_cli_get_i("-w"), .threads = fio_cli_get_i("-t"));
  /* free CLI memory */
  fio_cli_end();
  return 0;
}

/* *****************************************************************************
Accepting new connections
***************************************************************************** */

FIO_SFUNC void chat_on_open(int fd, void *udata) {
  /* allocate a new chat protocol object */
  chat_str_s *buf = chat_str_new();
  FIO_ASSERT_ALLOC(buf);
  /* attach the protocol to the UUID (otherwise the UUID might leak) */
  fio_attach_fd(fd, &CHAT_PROTOCOL, buf, NULL);
  /* log it all */
  FIO_LOG_INFO("(%d) chat connection open at %p", getpid(), (void *)buf);
  (void)udata; /* unused */
}

FIO_SFUNC void chat_on_attach(fio_s *io) {
  fio_subscribe(.io = io,
                .channel = {.buf = "my chat channel", .len = 15},
                .on_message = chat_on_message);
}

/* *****************************************************************************
Chat pub/sub callback
***************************************************************************** */

/* Called when a chat message is ready to be pushed to the connection */
FIO_SFUNC void chat_on_message(fio_msg_s *msg) {
  fio_write(msg->io, "Chat: ", 6);
  fio_write(msg->io, msg->message.buf, msg->message.len);
}

/* *****************************************************************************
Chat protocol callbacks
***************************************************************************** */

/* Called when data is available on the socket (uuid). */
FIO_SFUNC void chat_on_data(fio_s *io) {
  /* read the data from the socket  */
  char mem[1024];
  ssize_t r = fio_read(io, mem, 1018);
  if (r <= 0)
    return;

  /* write the data to the end of the existing buffer - support fragmentation */
  fio_str_info_s s = chat_str_write(fio_udata_get(io), mem, (size_t)r);
  char *end = s.buf;
  char *start = s.buf;

  /* consume the unified buffer line by line */
  while (s.buf + s.len > start && (end = memchr(start, '\n', s.len))) {
    ++end;
    size_t len = (size_t)(end - start);
    if (len) {
      /* send the message by transferring ownership of the allocated buffer */
      fio_publish(.channel = {.buf = "my chat channel", .len = 15},
                  .message = {.buf = start, .len = len});
    }
    /* seek immediately after the '\n' character  */
    start = end;
  }

  /* are there any fragmented lines? */
  if (s.buf + s.len <= start) {
    /* all data was consumed, keep string's buffer, just reset it's size. */
    chat_str_resize(fio_udata_get(io), 0);
  } else {
    /* move the fragmented line to the beginning of the buffer  */
    memmove(s.buf, start, (s.len + s.buf) - start);
    chat_str_resize(fio_udata_get(io), (s.len + s.buf) - start);
    /* are we under attack? more than 64Kb with no new lines markers? */
    if (chat_str_len(fio_udata_get(io)) > 65536) {
      fio_write(io, "Server: attack pattern detected! Goodbye!\n", 42);
      chat_str_destroy(fio_udata_get(io));
      fio_close(io);
    }
  }
}

/* Called when the server is shutting down, before the uuid is closed. */
FIO_SFUNC void chat_on_shutdown(fio_s *io) {
  fio_write(io, "SERVER: Server shutting down. Goodbye.\n", 40);
}

/* Called after the connection was closed, for object cleanup. */
FIO_SFUNC void chat_on_close(void *udata) {
  chat_str_free(udata);
  /* log it all */
  FIO_LOG_INFO("(%d) chat connection closed at %p", getpid(), udata);
}

/* Called if no data was received in a while and TIMEOUT occured. */
FIO_SFUNC void chat_ping(fio_s *io) {
  fio_write(io, "SERVER: Are you there?\n", 23);
}
