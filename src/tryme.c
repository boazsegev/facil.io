/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

#include <stdio.h>
#include <stdlib.h>

#define USE_HTTP_PROTOCOL 1
#define THREAD_COUNT 1

/**************************************
HttpProtocol (lib-server) "Hello World"
*/
#include "http.h"
#include "websockets.h"

void ws_message(ws_s* ws, char* data, size_t size, int is_text) {
  if (!is_text)
    fprintf(stderr, "Error parsing message type - should be text?\n");
  else
    fprintf(stderr, "Got Websocket message: %.*s\n", (int)size, data);
  Websocket.write(ws, data, size, 1);  // echo
}

void on_request(struct HttpRequest* request) {
  if (!strcmp(request->path, "/ws")) {
    websocket_upgrade(.request = request, .on_message = ws_message);
    return;
  }
  struct HttpResponse* response = HttpResponse.new(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}

/**************************************
Lib Server "Hello World" (Http)
*/
#include "lib-server.h"
#include "lib-tls-server.h"

static void on_data(server_pt srv, int fd) {
  static char reply[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=2\r\n"
      "\r\n"
      "Hello World!";
  char buff[1024];

  if (Server.read(srv, fd, buff, 1024)) {
    Server.write(srv, fd, reply, sizeof(reply));
  }
}

/**************************************
initialization... how about timers?
*/
void print_conn(server_pt srv, int fd, void* arg) {
  printf("- Connection to FD: %d\n", fd);
}
void done_printing(server_pt srv, int fd, void* arg) {
  fprintf(stderr, "******* Total Clients: %lu\n", Server.count(srv, NULL));
}

void timer_task(server_pt srv) {
  Server.each(srv, NULL, print_conn, NULL, done_printing);
  fprintf(stderr, "Clients: %lu\n", Server.count(srv, NULL));
}
void on_init(server_pt srv) {
  // TLSServer.init_server(srv);
  Server.run_every(srv, 1000, -1, (void*)timer_task, srv);
}

/**************************************
Main function
*/

int main(int argc, char const* argv[]) {
  if (USE_HTTP_PROTOCOL) {
    start_http_server(on_request, NULL, .threads = THREAD_COUNT,
                      .on_init = on_init);
  } else {
    struct Protocol protocol = {.on_data = on_data};
    start_server(.protocol = &protocol, .timeout = 2, .on_init = on_init,
                 .threads = THREAD_COUNT);
  }
  return 0;
}
