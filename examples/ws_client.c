/*
This is an example Websocket client.

Data is read from STDIN (which is most of the code) and sent to the Websocket.

Each line of text enterd through STDIN is sent to the Websocket, excluding the
EOL (end of line) character(s).

To try it out, after compiling (assuming `tmp/demo` is the compiled file):

    $ tmp/demo echo.websocket.org

*/
#include "http.h"
#include "pubsub.h"

#include <signal.h>

/* *****************************************************************************
Globals
***************************************************************************** */

FIOBJ input_channel = FIOBJ_INVALID;

/* *****************************************************************************
REPL
***************************************************************************** */

FIOBJ repl_buffer;
/** we read data from stdin and test for EOL markers. */
void repl_on_data(intptr_t uuid, protocol_s *protocol) {
  const size_t capa = fiobj_str_capa(repl_buffer);
  fio_cstr_s buffer = fiobj_obj2cstr(repl_buffer);
  if (buffer.len == capa) {
    fprintf(stderr, "FATAL ERROR: input too long\n");
    kill(0, SIGINT);
    return;
  }
  ssize_t n = sock_read(uuid, buffer.data + buffer.len, capa - buffer.len);
  if (n <= 0)
    return;
  buffer.len += n;
  {
    /* eliminate backspace? */
    char *p;
    while ((p = memchr(buffer.data, '\b', buffer.len))) {
      if (buffer.len - (p - buffer.data)) {
        memmove(p, p + 1, buffer.len - (p - buffer.data));
      }
      --buffer.len;
    }
  }
  fiobj_str_resize(repl_buffer, buffer.len);
  char *eol = memchr(buffer.data, '\n', buffer.len);
  if (eol == NULL)
    return;
  if (eol == buffer.data) {
    memmove(buffer.data, buffer.data + 1, buffer.len - 1);
    fiobj_str_resize(repl_buffer, buffer.len - 1);
    return;
  } else if (eol == buffer.data + 1 && buffer.data[0] == '\r') {
    memmove(buffer.data, buffer.data + 2, buffer.len - 2);
    fiobj_str_resize(repl_buffer, buffer.len - 2);
    return;
  }
  if (eol[-1] == '\r')
    --eol;
  size_t message_length = eol - buffer.data;
  FIOBJ message = fiobj_str_new(buffer.data, message_length);
  pubsub_publish(.channel = input_channel, .message = message);
  fprintf(stderr, "* Sent %zu bytes\n", fiobj_obj2cstr(message).len);
  fiobj_free(message);
  if (eol[0] == '\r') {
    ++message_length;
    ++eol;
  }
  ++message_length;
  memmove(buffer.data, eol + 1, buffer.len - message_length);
  fiobj_str_resize(repl_buffer, buffer.len - message_length);
  sock_flush_all();
  (void)protocol;
}
/** called when a connection's timeout was reached */
void repl_ping(intptr_t uuid, protocol_s *protocol) {
  sock_touch(uuid);
  (void)protocol;
}

static protocol_s REPL = {
    .on_data = repl_on_data, .ping = repl_ping,
};

/* *****************************************************************************
Websocket Client
***************************************************************************** */

static void on_websocket_message(ws_s *ws, char *data, size_t size,
                                 uint8_t is_text) {
  fprintf(stderr, "%.*s\n", (int)size, data);
  (void)(ws);
  (void)(is_text);
}

static void on_open(ws_s *ws) {
  fprintf(stderr, "Opened Websocket Connection\n");
  websocket_subscribe(ws, .channel = input_channel);
}

static void on_shutdown(ws_s *ws) {
  websocket_write(ws, "Going Away...", 13, 1);
  fprintf(stderr, "Client app closing...\n");
}

static void on_close(intptr_t uuid, void *udata) {
  if (!uuid)
    fprintf(stderr, "Connection failed...\n");
  else
    fprintf(stderr, "We're done...\n");
  kill(0, SIGINT);
  (void)(uuid);
  (void)(udata);
}

/* *****************************************************************************
Main
***************************************************************************** */

int main(int argc, char const *argv[]) {
  repl_buffer = fiobj_str_buf(4096);
  FIOBJ address = fiobj_str_new("ws://", 5);
  input_channel = fiobj_str_new("input_channel", 13);
  if (argc >= 2) {
    fiobj_str_write(address, argv[1], strlen(argv[1]));
    if (argc >= 3) {
      fiobj_str_write(address, ":", 1);
      fiobj_str_write(address, argv[2], strlen(argv[2]));
    }
  } else {
    fiobj_str_write(address, "localhost:3000/", 15);
  }
  if (websocket_connect(fiobj_obj2cstr(address).data, .on_close = on_close,
                        .on_open = on_open, .on_message = on_websocket_message,
                        .on_shutdown = on_shutdown, ) < 0) {
    fprintf(stderr, "Failed to connect to Websocket server\n");
    exit(-1);
  }
  int stdio_fd = dup(fileno(stdin));
  sock_set_non_block(stdio_fd);
  facil_attach(sock_open(stdio_fd), &REPL);
  facil_run(.threads = 1, .processes = 1);
  fiobj_free(address);
  fiobj_free(input_channel);
  fiobj_free(repl_buffer);
}
