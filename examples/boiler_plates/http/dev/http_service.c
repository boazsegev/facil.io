#include "fio_cli_helper.h"
#include "main.h"

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_request_s *request) {
  http_response_s *response = http_response_create(request);
  if (request->settings->log_static)
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
  if (http_listen(fio_cli_get_str("port"), fio_cli_get_str("address"),
                  .on_request = on_http_request,
                  .max_body_size = fio_cli_get_int("maxbd"),
                  .public_folder = fio_cli_get_str("public"),
                  .log_static = fio_cli_get_int("log"),
                  .timeout = fio_cli_get_int("keep-alive"))) {
    perror("ERROR: facil couldn't initialize HTTP service (already running?)");
    exit(1);
  }
}
