/* -*- mode: c -*- */
#ifndef H_HTTP_WEBSOCKET_TEST_H
#define H_HTTP_WEBSOCKET_TEST_H

#include "bscrypt.h"
#include "websockets.h"
#include <stdio.h>
#include <stdlib.h>

/*****************************
A Websocket echo implementation

Test in browser with:

ws = new WebSocket("ws://localhost:3000"); ws.onmessage = function(e)
{console.log("Got message " + e.data.length + " bytes long"); for(var i = 0; i <
e.data.length ; i += 4) {if (e.data.slice(i, i+4) != "text") {console.log(
"incoming message corrupted? ", e.data.slice(i, i+4) ); return;};}; };
ws.onclose = function(e) {console.log("closed")}; ws.onopen = function(e)
{ws.send("hi");};

str = "text"; function size_test() { if(ws.readyState != 1) return;
if(str.length > 4194304) {console.log("str reached 4MiB!"); str = "test";
return;}; ws.send(str); str = str + str; window.setTimeout(size_test, 150); };
size_test();

*/

static void ws_open(ws_s *ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", (void *)ws);
}

static void ws_echo(ws_s *ws, char *data, size_t size, uint8_t is_text) {
  // echos the data to the current websocket
  websocket_write(ws, data, size, is_text);
  if (memcmp(data, "bomb me", 7) == 0) {
    char *msg = malloc(1024 * 1024);
    for (char *pos = msg; pos < msg + (1024 * 1024 - 1); pos += 8) {
      memcpy(pos, "bomb(!) ", 8);
    }
    websocket_write(ws, msg, 1024 * 1024, is_text);
    free(msg);
  }
}

static void ws_shutdown(ws_s *ws) {
  websocket_write(ws, "Shutting Down", 13, 1);
}

static void ws_close(ws_s *ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", (void *)ws);
}

/*****************************
A Websocket Broadcast implementation
*/

static void ws_broadcast(ws_s *ws, char *data, size_t size, uint8_t is_text) {
  websocket_write_each(.data = data, .length = size, .is_text = is_text);
  (void)ws;
}

/*****************************
The HTTP implementation
*/

static void on_request(http_request_s *request) {
  // to log we will start a response.
  http_response_s *response = http_response_create(request);
  // http_response_log_start(response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade) {
    if (!strcmp(request->path, "/broadcast")) {
      // Websocket upgrade will use our existing response (never leak
      // responses).
      websocket_upgrade(.request = request, .on_message = ws_broadcast,
                        .on_open = ws_open, .on_close = ws_close,
                        .on_shutdown = ws_shutdown, .response = response);

      return;
    }
    // other upgrade requests will have the following properties:
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .max_msg_size = 2097152, .on_open = ws_open,
                      .on_close = ws_close, .timeout = 10,
                      .on_shutdown = ws_shutdown, .response = response);
    return;
  }
  // file dumping
  if (!strcmp(request->path, "/dump.jpg")) {
    fdump_s *data = bscrypt_fdump("./public_www/bo.jpg", 0);
    if (data == NULL) {
      fprintf(stderr, "Couldn't read file\n");
      http_response_write_body(response, "Sorry, error!", 13);
      http_response_finish(response);
      return;
    }
    http_response_write_body(response, data->data, data->length);
    http_response_finish(response);
    free(data);
    return;
  }
  if (!strcmp(request->path, "/dump.mov")) {
    fdump_s *data = bscrypt_fdump("./public_www/rolex.mov", 0);
    if (data == NULL) {
      fprintf(stderr, "Couldn't read file\n");
      http_response_write_body(response, "Sorry, error!", 13);
      http_response_finish(response);
      return;
    }
    http_response_write_body(response, data->data, data->length);
    http_response_finish(response);
    free(data);
    return;
  }
  // HTTP response
  http_response_write_body(response, "Hello World!", 12);
  http_response_finish(response);
}

// /*****************************
// Print to screen protocol
// */
// struct prnt2scrn_protocol_s {
//   protocol_s protocol;
//   intptr_t uuid;
// };
// static void on_data(intptr_t uuid, protocol_s *protocol) {
//   (void)(protocol);
//   uint8_t buffer[1024];
//   ssize_t len;
//   while ((len = sock_read(uuid, buffer, 1024)) > 0) {
//     if (len > 0)
//       fprintf(stderr, "%.*s\n", (int)len, buffer);
//   }
//   fprintf(stderr, "returning from on_data\n");
//   // sock_write(uuid, "HTTP/1.1 100 Continue\r\n\r\n", 25);
// }
// static void on_close(protocol_s *protocol) {
//   fprintf(stderr, "Connection closed %p\n",
//           (void *)(((struct prnt2scrn_protocol_s *)protocol)->uuid));
//   free(protocol);
// }
//
// static protocol_s *on_open(intptr_t uuid, void *udata) {
//   (void)(udata);
//   struct prnt2scrn_protocol_s *prt = malloc(sizeof *prt);
//   *prt = (struct prnt2scrn_protocol_s){
//       .protocol.on_data = on_data, .protocol.on_close = on_close, .uuid =
//       uuid};
//   fprintf(stderr, "New connection %p\n", (void *)uuid);
//   server_set_timeout(uuid, 10);
//   return (void *)prt;
// }

/*****************************
The main function
*/

void listen2http_ws(const char *port, const char *public_folder) {
  if (http_listen(port, NULL, .on_request = on_request,
                  .public_folder = public_folder, .log_static = 1))
    perror("Couldn't initiate HTTP service"), exit(1);
}
#endif
