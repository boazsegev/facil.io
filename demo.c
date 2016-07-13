// update the tryme.c file to use the existing folder structure and makefile
#include "http.h"

void on_request(http_request_s* request) {
  http_response_s response = http_response_init(request);
  http_response_write_header(&response, .name = "X-Data", .value = "my data");
  http_response_set_cookie(&response, .name = "my_cookie", .value = "data");
  http_response_write_body(&response, "Hello World!\r\n", 14);
  http_response_finish(&response);
}

int main() {
  char* public_folder = NULL;
  // listen on port 3000, any available network binding (0.0.0.0)
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder);
  // start the server
  server_run(.threads = 16);
}
