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

    // run 1st client app on port 3000.
    ws = new WebSocket("ws://localhost:3000/Mitchel");
    ws.onmessage = function(e) { console.log(e.data); };
    ws.onclose = function(e) { console.log("closed"); };
    ws.onopen = function(e) { e.target.send("Yo!"); };
    // run 2nd client app on port 3030.
    ws = new WebSocket("ws://localhost:3030/Johana");
    ws.onmessage = function(e) { console.log(e.data); };
    ws.onclose = function(e) { console.log("closed"); };
    ws.onopen = function(e) { e.target.send("Brut."); };

Remember that published messages will now be printed to the console both by
Mitchel and Johana, which means messages will be delivered twice unless using
two different browser windows.
*/

#include "fio_cli.h"
#include "pubsub.h"
#include "redis_engine.h"
#include "websockets.h"

#include <string.h>

/* *****************************************************************************
Websocket Pub/Sub
***************************************************************************** */

/* Pub/Sub channels can persists safely in memory. */
static FIOBJ CHAT_CHANNEL;

/* We'll subscribe to the channel's chat channel when a new connection opens */
static void on_open_websocket(ws_s *ws) {
  /* we use a FIOBJ String for the client's "nickname" */
  FIOBJ n = (FIOBJ)websocket_udata(ws);
  fprintf(stderr, "(%d) %s connected to the chat service.\n", getpid(),
          fiobj_obj2cstr(n).data);
  websocket_subscribe(ws, .channel = CHAT_CHANNEL, .force_text = 1);
}

/* Free the nickname, if any. */
static void on_close_websocket(intptr_t uuid, void *udata) {
  fiobj_free((FIOBJ)udata);
  (void)uuid;
}

/* Copy the nickname and the data to format a nicer message. */
static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  /* we use a FIOBJ String for the client's "nickname" */
  FIOBJ n = (FIOBJ)websocket_udata(ws);
  /* We'll copy the nickname and the data to a new message buffer */
  FIOBJ msg = fiobj_str_buf(fiobj_obj2cstr(n).len + 2 + size);
  fiobj_str_join(msg, n);
  fiobj_str_write(msg, ": ", 2);
  fiobj_str_write(msg, data, size);
  /* Publish to the chat channel */
  if (pubsub_publish(.channel = CHAT_CHANNEL, .message = msg))
    fprintf(stderr, "Failed to publish\n");
  /* free any temporary objects */
  fiobj_free(msg);
  /* we didn't use these for this `on_message` callback implementation */
  (void)(ws);
  (void)(is_text);
}

/* *****************************************************************************
HTTP Handling (Upgrading to Websocket)
***************************************************************************** */

/* handles normal HTTP requests. */
static void answer_http_request(http_s *h) {
  http_set_header2(h, (fio_cstr_s){.name = "Server", .len = 6},
                   (fio_cstr_s){.value = "facil.example", .len = 13});
  http_set_header(h, HTTP_HEADER_CONTENT_TYPE, http_mimetype_find("txt", 3));
  /* this both sends the response and frees the http handler. */
  http_send_body(h, "This is a Websocket chatroom example using Redis.", 49);
}

/* handles HTTP upgrade requests. */
static void answer_http_upgrade(http_s *h, char *pr, size_t len) {
  /* make sure the upgrade request is for Websockets */
  if (pr[1] != 'e' && len != 9) {
    http_send_error(h, 400);
    return;
  }
  /* Assign a nickname */
  FIOBJ nickname = FIOBJ_INVALID;
  fio_cstr_s path = fiobj_obj2cstr(h->path);
  if (path.len > 1) {
    nickname = fiobj_str_new(path.data + 1, path.len - 1);
  } else {
    nickname = fiobj_str_new("guest", 5);
  }
  /* attempt Websocket upgrade */
  if (http_upgrade2ws(.http = h, .on_open = on_open_websocket,
                      .on_close = on_close_websocket,
                      .on_message = handle_websocket_messages,
                      .udata = (void *)nickname)) {
    /* if the upgrade failed, free the user's nickname. */
    fiobj_free(nickname);
  }
}

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
  CHAT_CHANNEL = fiobj_str_new("chat", 4);

  /*     ****  Command line arguments ****     */
  fio_cli_start(
      argc, argv,
      "This is a facil.io example application.\n"
      "\nThis example demonstrates Pub/Sub using a Chat application.\n"
      "Optional Redis support is also demonstrated.\n"
      "\nThe following arguments are supported:");
  fio_cli_accept_num("threads t",
                     "The number of threads to use. System dependent default.");
  fio_cli_accept_num(
      "workers w", "The number of processes to use. System dependent default.");
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
    fprintf(stderr, "* serving static files from:%s\n", public_folder);
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
    fprintf(stderr, "* Connecting to Redis for Pub/Sub.\n");
    PUBSUB_DEFAULT_ENGINE =
        redis_engine_create(.address = redis_address, .port = redis_port,
                            .ping_interval = 40);
    if (!PUBSUB_DEFAULT_ENGINE) {
      perror("\nERROR: couldn't initialize Redis engine.\n");
      exit(-2);
    }
    printf("* Redis engine initialized.\n");
  } else {
    printf("* Redis engine details missing, "
           "using native-local pub/sub engine.\n");
  }

  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .on_upgrade = answer_http_upgrade, .log = print_log,
                  .public_folder = public_folder))
    perror("Couldn't initiate Websocket service"), exit(1);
  facil_run(.threads = threads, .processes = workers);

  if (PUBSUB_DEFAULT_ENGINE != PUBSUB_CLUSTER_ENGINE) {
    fprintf(stderr, "* Destroying Redis engine.\n");
    redis_engine_destroy(PUBSUB_DEFAULT_ENGINE);
    PUBSUB_DEFAULT_ENGINE = (pubsub_engine_s *)PUBSUB_CLUSTER_ENGINE;
  }
  fiobj_free(CHAT_CHANNEL);
  fio_cli_end();
}
