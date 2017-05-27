#ifndef WEBSOCKET_SHOOTOUT_H
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
#define WEBSOCKET_SHOOTOUT_H

#ifndef SHOOTOUT_USE_DIRECT_WRITE
#define SHOOTOUT_USE_DIRECT_WRITE 1
#endif

#include "websockets.h" // includes the "http.h" header

#include <string.h>

static void free_ws_msg_b(ws_s *origin, void *msg) {
  if (origin) {
    struct {
      size_t len;
      char data[];
    } *buff = msg;
    buff->data[0] = 'r';
    websocket_write(origin, buff->data, buff->len, 0);
  }
  free(msg);
}

static void free_ws_msg(ws_s *origin, void *msg) {
  (void)(origin);
  free(msg);
}
static void broadcast_shootout_msg(ws_s *ws, void *msg) {
  (void)(ws);
  struct {
    size_t len;
    char data[];
  } *buff = msg;
  websocket_write(ws, buff->data, buff->len, 1);
}

static void broadcast_shootout_msg_bin(ws_s *ws, void *msg) {
  (void)(ws);
  struct {
    size_t len;
    char data[];
  } *buff = msg;
  websocket_write(ws, buff->data, buff->len, 0);
}

static uint8_t ws_so_filter_callback(ws_s *ws, void *arg) {
  if (arg || !ws || ((protocol_s *)ws)->service != WEBSOCKET_ID_STR)
    perror("WTF?! No Websocket during callback!"), exit(1);
  return 1;
}
static void ws_so_finished_callback(ws_s *ws, void *arg) {
  (void)(arg);
  (void)(ws);
  fwrite(".", 1, 1, stderr);
}

static void ws_shootout(ws_s *ws, char *data, size_t size, uint8_t is_text) {
  (void)(ws);
  (void)(is_text);
  (void)(size);
  if (data[0] == 'b') {
    if (SHOOTOUT_USE_DIRECT_WRITE) {
      websocket_write_each(.data = data, .length = size, .is_text = is_text,
                           .on_finished = ws_so_finished_callback,
                           .filter = ws_so_filter_callback);
      data[0] = 'r';
      websocket_write(ws, data, size, 0);
    } else {
      struct {
        size_t len;
        char data[];
      } *buff = malloc(size + sizeof(*buff));
      buff->len = size;
      memcpy(buff->data, data, size);
      /* perform broadcast (all except this websocket) */
      websocket_each(.origin = ws, .task = broadcast_shootout_msg_bin,
                     .arg = buff, .on_finish = free_ws_msg_b);
      /* perform echo */
      websocket_write(ws, data, size, 0);
    }
  } else if (data[9] == 'b') {
    if (SHOOTOUT_USE_DIRECT_WRITE) {
      websocket_write_each(.data = data, .length = size, .is_text = is_text);
    } else {
      struct {
        size_t len;
        char data[];
      } *buff = malloc(size + sizeof(*buff));
      buff->len = size;
      memcpy(buff->data, data, size);
      /* perform broadcast (all except this websocket) */
      websocket_each(.origin = ws, .task = broadcast_shootout_msg, .arg = buff,
                     .on_finish = free_ws_msg);
      /* perform echo */
      websocket_write(ws, data, size, 1);
    }
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
static void http1_websocket_shotout(http_request_s *request) {
  // to log we will start a response.
  http_response_s *response = http_response_create(request);
  http_response_write_header(response, .name = "Server", .name_len = 6,
                             .value = "facil.io/0.4.0", .value_len = 14);

  // http_response_log_start(&response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .response = response,
                      .on_message = ws_shootout);

    return;
  }
  http_response_write_header(response, .name = "Content-Type", .name_len = 12,
                             .value = "text/plain", .value_len = 10);

  http_response_write_body(response,
                           "This is a Websocket-Shootout application!", 41);
  http_response_finish(response);
}

static void listen2shootout(const char *port, char is_logging) {
  if (http_listen(port, NULL, .on_request = http1_websocket_shotout,
                  .log_static = is_logging))
    perror("Couldn't initiate Websocket Shootout service"), exit(1);
}

#endif
