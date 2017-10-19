/**
This example creates a simple chat application that uses Redis to sync pub/sub
across machines.

To test this application, you will need a Redis server (defaults to address
"localhost" and port "6379").

To run the test, run the application twice, on two different ports. Clients on
each port will share their pub/sub events with clients from the other port - fun
times :-)

i.e.:


Use a javascript consol to connect to the websockets... maybe using the
following javascript code:

ws1 = new WebSocket("ws://localhost:3000/Mitchel"); // run 1st app on port 3000.
ws1.onmessage = function(e) { console.log(e.data); };
ws1.onclose = function(e) { console.log("closed"); };
ws1.onopen = function(e) { ws1.send("Yo!"); };

ws2 = new WebSocket("ws://localhost:3030/Johana"); // run 2nd app on port 3030.
ws2.onmessage = function(e) { console.log(e.data); };
ws2.onclose = function(e) { console.log("closed"); };
ws2.onopen = function(e) { ws2.send("Brut."); };

Remember that published messages will now be printed to the console both by
Mitchel and Johana, which means messages will be delivered twice unless using
two different browser windows.
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
  fprintf(stderr, "Got data: %s\n", data);
  websocket_write(ws, data, size, is_text);
  (void)(ws);
  (void)(is_text);
}

/* *****************************************************************************
HTTP Handling (Upgrading to Websocket)
***************************************************************************** */

static void answer_http_request(http_request_s *request) {
  http_response_s *response = http_response_create(request);
  /* We'll match the dynamic logging settings with the static logging ones. */
  if (request->settings->log_static)
    http_response_log_start(response);

  http_response_write_header(response, .name = "Server", .name_len = 6,
                             .value = "facil.example", .value_len = 13);

  /* the upgrade header value has a quick access pointer. */
  if (request->upgrade) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_message = handle_websocket_messages, .udata = NULL);
    return;
  }
  /*     ****  Normal HTTP request, no Websockets ****     */

  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);
  http_response_write_body(response, "This is a Websocket echo example.", 33);
  /* this both sends and frees the response. */
  http_response_finish(response);
}

#include "fio_cli_helper.h"

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
    printf("* serving static files from:%s", public_folder);
  }
  if (fio_cli_get_str("t"))
    threads = fio_cli_get_int("t");
  if (fio_cli_get_str("w"))
    workers = fio_cli_get_int("w");
  print_log = fio_cli_get_int("v");

  if (!threads || !workers)
    threads = workers = 0;

  /*     ****  actual code ****     */
  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .log_static = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Websocket service"), exit(1);
  facil_run(.threads = threads, .processes = workers);
}
