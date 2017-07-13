/**
This example emulates the websocket shootout testing requirements, except that
the JSON will not be fully parsed.

See the Websocket-Shootout repository at GitHub:
https://github.com/hashrocket/websocket-shootout

Using the benchmarking tool, try the following benchmarks (binary and text):

websocket-bench broadcast ws://127.0.0.1:3000/ --concurrent 10 \
--sample-size 100 --server-type binary --step-size 1000 --limit-percentile 95 \
--limit-rtt 250ms --initial-clients 1000

websocket-bench broadcast ws://127.0.0.1:3000/ --concurrent 10 \
--sample-size 100 --step-size 1000 --limit-percentile 95 \
--limit-rtt 250ms --initial-clients 1000

*/

#include "pubsub.h"
#include "websockets.h"

#include "redis_engine.h"

#include <string.h>

static void on_open_shootout_websocket(ws_s *ws) {
  websocket_subscribe(ws, .channel.name = "text", .force_text = 1);
  websocket_subscribe(ws, .channel.name = "binary", .force_binary = 1);
}

static void handle_websocket_messages(ws_s *ws, char *data, size_t size,
                                      uint8_t is_text) {
  (void)(ws);
  (void)(is_text);
  (void)(size);
  if (data[0] == 'b') {
    pubsub_publish(.channel.name = "binary", .msg.data = data, .msg.len = size);
    // fwrite(".", 1, 1, stderr);
    data[0] = 'r';
    websocket_write(ws, data, size, 0);
  } else if (data[9] == 'b') {
    // fwrite(".", 1, 1, stderr);
    pubsub_publish(.channel.name = "text", .msg.data = data, .msg.len = size);
    /* send result */
    size = size + (25 - 19);
    void *buff = malloc(size);
    memcpy(buff, "{\"type\":\"broadcastResult\"", 25);
    memcpy((void *)(((uintptr_t)buff) + 25), data + 19, size - 25);
    websocket_write(ws, buff, size, 1);
    free(buff);
  } else {
    /* perform echo */
    websocket_write(ws, data, size, is_text);
  }
}

static void answer_http_request(http_request_s *request) {
  // to log we will start a response.
  http_response_s *response = http_response_create(request);
  // than we wil instruct logging to start.
  if (request->settings->log_static)
    http_response_log_start(response);
  // Set the server header.
  http_response_write_header(response, .name = "Server", .name_len = 6,
                             .value = "facil.example", .value_len = 13);

  // upgrade requests to broadcast will have the `upgrade` header
  if (request->upgrade) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_open = on_open_shootout_websocket,
                      .on_message = handle_websocket_messages);

    return;
  }
  // set the Content-Type header.
  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);
  // write some body to the response.
  http_response_write_body(response, "This is a Websocket-Shootout example!",
                           37);
  // send and free the response
  http_response_finish(response);
}

#include "fio_cli_helper.h"
/*
Read available command line details using "-?".
*/
int main(int argc, char const *argv[]) {
  const char *port = "3000";
  const char *public_folder = NULL;
  uint32_t threads = 0;
  uint32_t workers = 0;
  uint8_t print_log = 0;

  /*     ****  Command line arguments ****     */
  fio_cli_start(argc, argv,
                "This is a facil.io example application.\n"
                "\nThis example conforms to the "
                "Websocket Shootout requirements at:\n"
                "https://github.com/hashrocket/websocket-shootout\n"
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

  /*     ****  actual code ****     */
  // RedisEngine = redis_engine_create(.address = "localhost", .port = "6379");
  if (http_listen(port, NULL, .on_request = answer_http_request,
                  .log_static = print_log, .public_folder = public_folder))
    perror("Couldn't initiate Websocket Shootout service"), exit(1);
  facil_run(.threads = threads, .processes = workers);
}
