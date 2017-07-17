#include "fio_cli_helper.h"
#include "main.h"

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_request_s *request) {
  /* carete a response object */
  http_response_s *response = http_response_create(request);
  /* log dynamic requests using the same settings for static file requests. */
  if (request->settings->log_static)
    http_response_log_start(response);
  /* hanndle Websocket upgrade requests. for now, they are simply refused. */
  if (request->upgrade && request->upgrade_len == 9) {
    response->status = 400;
    http_response_write_body(response, "TODO: write websocket support", 30);
    http_response_finish(response);
    return;
  }
  /* set a response and send it (finnish vs. destroy). */
  http_response_write_body(response, "Hello World!", 12);
  http_response_finish(response);
}

/* starts a listeninng socket for HTTP connections. */
void initialize_http_service(void) {
  /* listen for inncoming connections */
  if (http_listen(fio_cli_get_str("port"), fio_cli_get_str("address"),
                  .on_request = on_http_request,
                  .max_body_size = fio_cli_get_int("maxbd"),
                  .public_folder = fio_cli_get_str("public"),
                  .log_static = fio_cli_get_int("log"),
                  .timeout = fio_cli_get_int("keep-alive"))) {
    /* listen failed ?*/
    perror("ERROR: facil couldn't initialize HTTP service (already running?)");
    exit(1);
  }
}
