#include "fio_cli.h"
#include "main.h"

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_s *h) {
  /* set a response and send it (finnish vs. destroy). */
  http_send_body(h, "Hello World!", 12);
}

/* starts a listeninng socket for HTTP connections. */
void initialize_http_service(void) {
  /* listen for inncoming connections */
  if (http_listen(fio_cli_get_str("port"), fio_cli_get_str("address"),
                  .on_request = on_http_request,
                  .max_body_size = fio_cli_get_int("maxbd"),
                  .public_folder = fio_cli_get_str("public"),
                  .log = fio_cli_get_int("log"),
                  .timeout = fio_cli_get_int("keep-alive")) == -1) {
    /* listen failed ?*/
    perror("ERROR: facil couldn't initialize HTTP service (already running?)");
    exit(1);
  }
}
