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
#include "redis_engine.h"
#include "websockets.h"

#include <string.h>

/* *****************************************************************************
Nicknames
***************************************************************************** */

struct nickname {
  size_t len;
  char nick[];
};

/* This initalization requires GNU gcc / clang ...
 * ... it's a default name for unimaginative visitors.
 */
static struct nickname MISSING_NICKNAME = {.len = 8, .nick = "unknown"};

/* *****************************************************************************
Websocket callbacks
***************************************************************************** */

/* We'll subscribe to the channel's chat channel when a new connection opens */
static void on_open_websocket(ws_s *ws) {
  websocket_subscribe(ws, .channel.name = "chat", .force_text = 1);
}

/* Free the nickname, if any. */
static void on_close_websocket(ws_s *ws) {
  if (websocket_udata(ws))
    free(websocket_udata(ws));
}

/* Copy the nickname and the data to format a nicer message. */
static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  struct nickname *n = websocket_udata(ws);
  if (!n)
    n = &MISSING_NICKNAME;
  char *msg = malloc(size + n->len + 2);
  memcpy(msg, n->nick, n->len);
  msg[n->len] = ':';
  msg[n->len + 1] = ' ';
  memcpy(msg + n->len + 2, data, size);
  pubsub_publish(.channel = {.name = "chat", .len = 4},
                 .msg = {.data = msg, .len = (size + n->len + 2)});
  free(msg);
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
    struct nickname *n = NULL;
    if (request->path_len > 1) {
      n = malloc(request->path_len + sizeof(*n));
      n->len = request->path_len - 1;
      memcpy(n->nick, request->path + 1, request->path_len - 1);
    }
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_open = on_open_websocket,
                      .on_close = on_close_websocket,
                      .on_message = handle_websocket_messages, .udata = n);

    return;
  }
  /*     ****  Normal HTTP request, no Websockets ****     */

  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);
  http_response_write_body(
      response, "This is a Websocket chatroom example using Redis.", 49);
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
  const char *redis_address = NULL;
  const char *redis_port = "6379";
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
  fio_cli_accept_str("redis r", "An optional Redis server's address.");
  fio_cli_accept_str("redis-port rp",
                     "An optional Redis server's port. Defaults to 6379.");

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
  redis_address = fio_cli_get_str("redis");
  if (fio_cli_get_str("redis-port"))
    redis_port = fio_cli_get_str("redis-port");

  if (!threads || !workers)
    threads = workers = 0;

  /*     ****  actual code ****     */
  if (redis_address) {
    PUBSUB_DEFAULT_ENGINE =
        redis_engine_create(.address = redis_address, .port = redis_port,
                            .ping_interval = 40);
    if (!PUBSUB_DEFAULT_ENGINE) {
      perror("\nERROR: couldn't initialize Redis engine.\n");
      exit(-2);
    }
    printf("* Redis engine initialized.\n");
  } else {
    printf(
        "* Redis engine details missing, using native-local pub/sub engine.\n");
  }

  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .log_static = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Websocket service"), exit(1);
  facil_run(.threads = threads, .processes = workers);

  if (PUBSUB_DEFAULT_ENGINE != PUBSUB_CLUSTER_ENGINE) {
    redis_engine_destroy(PUBSUB_DEFAULT_ENGINE);
    PUBSUB_DEFAULT_ENGINE = PUBSUB_CLUSTER_ENGINE;
  }
}
