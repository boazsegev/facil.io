/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#define THREAD_COUNT 1
#define PROCESS_COUNT 1

#include "websockets.h"
#include "mini-crypt.h"

/*****************************
The Websocket echo implementation
*/

void ws_open(ws_s* ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", ws);
}

void ws_echo(ws_s* ws, char* data, size_t size, int is_text) {
  fprintf(stderr, "Got %lu bytes (text: %d): %s\n", size, is_text, data);
  // echos the data to the current websocket
  Websocket.write(ws, data, size, is_text);
}

void ws_shutdown(ws_s* ws) {
  Websocket.write(ws, "Shutting Down", 13, 1);
}

void ws_close(ws_s* ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", ws);
}

/*****************************
The Websocket Broadcast implementation
*/

/* websocket broadcast data */
struct ws_data {
  size_t size;
  char data[];
};
/* free the websocket broadcast data */
void free_wsdata(ws_s* ws, void* arg) {
  free(arg);
}
/* the broadcast "task" performed by `Websocket.each` */
void ws_get_broadcast(ws_s* ws, void* _data) {
  struct ws_data* data = _data;
  Websocket.write(ws, data->data, data->size, 1);  // echo
}
/* The websocket broadcast server's `on_message` callback */

void ws_broadcast(ws_s* ws, char* data, size_t size, int is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data* msg = malloc(sizeof(*msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  Websocket.each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  Websocket.write(ws, data, size, 1);
}

/*****************************
The HTTP implementation
*/

void on_request(struct HttpRequest* request) {
  // fprintf(stderr, "\n\n*** Request for %s\n", request->path);
  if (!strcmp(request->path, "/echo")) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);
    return;
  } else if (!strcmp(request->path, "/broadcast")) {
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown);

    return;
  } else if (!strcmp(request->path, "/dump")) {
    // test big data write
    fdump_s* body = MiniCrypt.fdump("~/Documents/Scratch/bo.jpg", 0);
    if (!body) {
      struct HttpResponse* response = HttpResponse.create(request);
      response->status = 500;
      HttpResponse.write_body(response, "File dump failed!", 17);
      HttpResponse.destroy(response);
      return;
    }
    struct HttpResponse* response = HttpResponse.create(request);
    HttpResponse.write_body(response, body->data, body->length);
    HttpResponse.destroy(response);
    free(body);
    return;
  }
  struct HttpResponse* response = HttpResponse.create(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}

// void on_request_f(struct HttpRequest* request) {
//   static char reply[] =
//       "HTTP/1.1 200 OK\r\n"
//       "Content-Length: 12\r\n"
//       "Connection: keep-alive\r\n"
//       "Keep-Alive: timeout=2\r\n"
//       "\r\n"
//       "Hello World!";
//   Server.write(request->server, request->sockfd, reply, sizeof(reply));
// }

void on_init(server_pt srv) {
  fprintf(
      stderr,
      "\nDemo HTTP server up and running.\n"
      "Serving static files from: %s\n",
      ((struct HttpProtocol*)Server.settings(srv)->protocol)->public_folder);
}

/*****************************
The main function
*/

int main(int argc, char const* argv[]) {
  start_http_server(on_request, "~/Documents/Scratch", .threads = THREAD_COUNT,
                    .processes = PROCESS_COUNT, .on_init = on_init);
  return 0;
}
