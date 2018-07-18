/**
This example creates a simple echo Websocket server.

To run the test, connect using a websocket client.

i.e., from the browser's console:

ws = new WebSocket("ws://localhost:3000/"); // run 1st app on port 3000.
ws.onmessage = function(e) { console.log(e.data); };
ws.onclose = function(e) { console.log("closed"); };
ws.onopen = function(e) { ws.send("Echo This!"); };

*/

#include "fio_cli.h"
#include "websockets.h"
#include <string.h>

/* *****************************************************************************
Websocket callbacks
***************************************************************************** */

/* Copy the nickname and the data to format a nicer message. */
static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  websocket_write(ws, data, size, is_text);
  (void)(ws);
  (void)(is_text);
}
/* Copy the nickname and the data to format a nicer message. */
static void on_server_shutdown(ws_s *ws) {
  websocket_write(ws, "Server is going away", 20, 1);
}

/* *****************************************************************************
HTTP Handling (+ Upgrading to Websocket)
***************************************************************************** */

static void on_http_request(http_s *h) {
  http_set_header2(h, (fio_cstr_s){.name = "Server", .len = 6},
                   (fio_cstr_s){.value = "facil.example", .len = 13});
  http_set_header(h, HTTP_HEADER_CONTENT_TYPE, http_mimetype_find("txt", 3));
  /* this both sends and frees the request / response. */
  http_send_body(h, "This is a Websocket echo example.", 33);
}

static void on_http_upgrade(http_s *h, char *target, size_t len) {
  if (target[1] != 'e' && len != 9) {
    http_send_error(h, 400);
    return;
  }
  http_upgrade2ws(h, .on_message = handle_websocket_messages,
                  .on_shutdown = on_server_shutdown, .udata = NULL);
}

/* *****************************************************************************
Command Line Arguments
***************************************************************************** */

/* *****************************************************************************
Main function
***************************************************************************** */

/*
Read available command line details using "-?".
-p <port>            : defaults port 3000.
-t <threads>         : defaults to 1 (use 0 for automatic CPU core test/set).
-w <processes>       : defaults to 1 (use 0 for automatic CPU core test/set).
-v                   : sets verbosity (HTTP logging) on.
-r <address> <port>  : a spece delimited couplet for the Redis address and port.
*/
int main(int argc, char const *argv[]) {
  const char *port = "3000";
  const char *public_folder = NULL;
  uint32_t threads = 0;
  uint32_t workers = 0;
  uint8_t print_log = 0;

  /*     ****  Command line arguments ****     */
  fio_cli_start(
      argc, argv, 0,
      "This is a facil.io example application.\n"
      "\nThis example demonstrates a simple WebSocket echo server.\n"
      "\nThe following arguments are supported:",
      "-threads -t The number of threads to use. Default uses smart selection.",
      FIO_CLI_TYPE_INT,
      "-workers -w The number of processes to use. Default uses smart "
      "selection.",
      FIO_CLI_TYPE_INT, "-port -p The port number to listen to.",
      FIO_CLI_TYPE_INT,
      "-public -www A public folder for serve an HTTP static file service.",
      "-log -v Turns logging on.", FIO_CLI_TYPE_BOOL);
  if (fio_cli_get("-p"))
    port = fio_cli_get("-p");
  if (fio_cli_get("-www")) {
    public_folder = fio_cli_get("-www");
    fprintf(stderr, "* serving static files from:%s\n", public_folder);
  }
  if (fio_cli_get("-t"))
    threads = fio_cli_get_i("-t");
  if (fio_cli_get("-w"))
    workers = fio_cli_get_i("-w");
  print_log = fio_cli_get_bool("-v");
  fio_cli_end();

  if (!threads && !workers) {
    threads = workers = 1;
  }

  /*     ****  actual code ****     */
  if (http_listen(port, NULL, .on_request = on_http_request,
                  .on_upgrade = on_http_upgrade, .log = print_log,
                  .public_folder = public_folder) == -1) {
    perror("Couldn't initiate Websocket service");
    exit(1);
  }
  facil_run(.threads = threads, .processes = workers);
}

/* *****************************************************************************
Main function
***************************************************************************** */
