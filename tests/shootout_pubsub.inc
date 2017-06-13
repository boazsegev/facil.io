#ifndef WEBSOCKET_SHOOTOUT_PUBSUB_H
/**
This test suite emulates the websocket shoot testing requirements, except that
the JSON will not be parsed.

Here are a few possible test commands:

{"type":"broadcast",

websocket-bench broadcast ws://127.0.0.1:3000/ --concurrent 10 \
--sample-size 100 --server-type binary --step-size 1000 --limit-percentile 95 \
--limit-rtt 250ms --initial-clients 1000

websocket-bench broadcast ws://127.0.0.1:3000/ --concurrent 10 \
--sample-size 100 --step-size 1000 --limit-percentile 95 \
--limit-rtt 250ms --initial-clients 1000

ab -n 1000000 -c 2000 -k http://127.0.0.1:3000/

wrk -c400 -d5 -t12 http://localhost:3000/

I also run it for a while using the following Ruby script:

sleep 10 while `websocket-bench broadcast ws://127.0.0.1:3000/ --concurrent 10 \
--sample-size 100 --server-type binary --step-size 1000 --limit-percentile 95 \
--limit-rtt 250ms --initial-clients 1000`.tap {|s| puts s; puts "zzz..."}

*/
#define WEBSOCKET_SHOOTOUT_PUBSUB_H

#include "pubsub.h"     // includes the "http.h" header
#include "websockets.h" // includes the "http.h" header

#include <errno.h>
#include <string.h>

static void ws_shootout_pubsub_on_open(ws_s *ws) {
  websocket_subscribe(ws, .channel.name = "text", .force_text = 1);
  websocket_subscribe(ws, .channel.name = "binary", .force_binary = 1);
}

static void ws_shootout_pubsub(ws_s *ws, char *data, size_t size,
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

/*
A simple Hello World HTTP response emulation. Test with:
ab -n 1000000 -c 200 -k http://127.0.0.1:3000/
*/
static void http_websocket_shotout_pubsub(http_request_s *request) {
  // to log we will start a response.
  http_response_s *response = http_response_create(request);
  http_response_write_header(response, .name = "Server", .name_len = 6,
                             .value = "facil.io/0.4.0", .value_len = 14);

  // http_response_log_start(&response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_open = ws_shootout_pubsub_on_open,
                      .on_message = ws_shootout_pubsub);

    return;
  }
  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);

  http_response_write_body(response,
                           "This is a Websocket-Shootout application!", 41);
  http_response_finish(response);
}

static void listen2shootout_pubsub(const char *port, char is_logging) {
  if (http_listen(port, NULL, .on_request = http_websocket_shotout_pubsub,
                  .log_static = is_logging))
    perror("Couldn't initiate Websocket Shootout service"), exit(1);
}

#endif
