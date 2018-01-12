/**
This example creates a simple echo Websocket server.

To run the test, connect using a websocket client.

i.e., from the browser's console:

ws = new WebSocket("ws://localhost:3000/"); // run 1st app on port 3000.
ws.onmessage = function(e) { console.log(e.data); };
ws.onclose = function(e) { console.log("closed"); };
ws.onopen = function(e) { ws.send("Echo This!"); };

*/

#include "pubsub.h"
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
  http_upgrade2ws(.http = h, .on_message = handle_websocket_messages,
                  .on_shutdown = on_server_shutdown, .udata = NULL);
}

#include "fio_cli.h"

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
  uint32_t threads = 1;
  uint32_t workers = 1;
  uint8_t print_log = 0;

  /*     ****  Command line arguments ****     */
  fio_cli_start(
      argc, argv,
      "This is a facil.io example application.\n"
      "\nThis example demonstrates Pub/Sub using a Chat application.\n"
      "Optional Redis support is also demonstrated.\n"
      "\nThe following arguments are supported:");
  fio_cli_accept_num(
      "threads t",
      "The number of threads to use. Default uses smart selection.");
  fio_cli_accept_num(
      "workers w",
      "The number of processes to use. Default uses smart selection.");
  fio_cli_accept_num("port p", "The port number to listen to.");
  fio_cli_accept_str("public www",
                     "A public folder for serve an HTTP static file service.");
  fio_cli_accept_bool("log v", "Turns logging on.");

  if (fio_cli_get_str("p"))
    port = fio_cli_get_str("p");
  if (fio_cli_get_str("www")) {
    public_folder = fio_cli_get_str("www");
    fprintf(stderr, "* serving static files from:%s\n", public_folder);
  }
  if (fio_cli_get_str("t"))
    threads = fio_cli_get_int("t");
  if (fio_cli_get_str("w"))
    workers = fio_cli_get_int("w");
  print_log = fio_cli_get_int("v");

  if (!threads || !workers)
    threads = workers = 0;

  /*     ****  actual code ****     */
  if (http_listen(port, NULL, .on_request = on_http_request,
                  .on_upgrade = on_http_upgrade, .log = print_log,
                  .public_folder = public_folder))
    perror("Couldn't initiate Websocket service"), exit(1);
  facil_run(.threads = threads, .processes = workers);

  fio_cli_end();
}
