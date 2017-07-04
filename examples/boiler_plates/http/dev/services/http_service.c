#include "main.h"
#include "websockets.h"

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_request_s *request) {
  http_response_s *response = http_response_create(request);
  if (VERBOSE)
    http_response_log_start(response);
  if (request->upgrade && request->upgrade_len == 9) {
    response->status = 400;
    http_response_write_body(response, "TODO: writer websocket support", 30);
    http_response_finish(response);
    return;
  }
  http_response_write_body(response, "Hello World!", 12);
  http_response_finish(response);
}

/* this function can be safely ignored. */
void initialize_http_service(void) {
  if (http_listen(HTTP_PORT, HTTP_ADDRESS, .on_request = on_http_request,
                  .max_body_size = HTTP_BODY_LIMIT,
                  .public_folder = HTTP_PUBLIC_FOLDER, .log_static = VERBOSE,
                  .timeout = HTTP_TIMEOUT)) {
    perror("ERROR: facil couldn't initialize HTTP service (already running?)");
    exit(1);
  }
}
