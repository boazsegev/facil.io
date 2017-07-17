#include <stdlib.h>
#include <string.h>

#include "cli.h"

#include "fio_cli_helper.h"

static char tmp_url_parsing[1024];

void initialize_cli(int argc, char const *argv[]) {
  /*     ****  Command line arguments ****     */
  fio_cli_start(argc, argv, NULL);
  /* setup allowed argumennts */
  fio_cli_accept_str("bind b", "address to listen to. defaults any available.");
  fio_cli_accept_num("port p", "port number to listen to. defaults port 3000");
  fio_cli_accept_num("workers w", "number of processes to use.");
  fio_cli_accept_num("threads t", "number of threads per process.");
  fio_cli_accept_bool("log v", "request verbosity (logging).");
  fio_cli_accept_str("public www", "public folder, for static file service.");
  fio_cli_accept_num("keep-alive k", "HTTP keep-alive timeout (0..255). "
                                     "default: ~5s");
  fio_cli_accept_num("ping", "websocket ping interval (0..255). "
                             "default: ~40s");
  fio_cli_accept_num("max-body maxbd", "HTTP upload limit. default: ~50Mb");
  fio_cli_accept_num("max-message maxms",
                     "incoming websocket message size limit. "
                     "default: ~250Kb");
  fio_cli_accept_str("redis-address ra", "an optional Redis server's address.");
  fio_cli_accept_str("redis-port rp",
                     "an optional Redis server's port. default: 6379");
  fio_cli_accept_str("redis-password rpw", "Redis password, if any.");

  /* Test and set any default options */
  if (!fio_cli_get_str("p")) {
    /* Test environment as well */
    char *tmp = getenv("PORT");
    if (!tmp)
      tmp = "3000";
    /* Set default */
    fio_cli_set_str("p", tmp);
  }
  if (!fio_cli_get_str("b")) {
    char *tmp = getenv("ADDRESS");
    if (tmp)
      fio_cli_set_str("b", tmp);
  }
  if (!fio_cli_get_str("public")) {
    char *tmp = getenv("HTTP_PUBLIC_FOLDER");
    if (tmp)
      fio_cli_set_str("public", tmp);
  }

  /*     ****  Environmental defaults ****     */

  /* parse the Redis URL to manage redis pub/sub */
  if (getenv("REDIS_URL")) {
    /* We assume a format such as redis://user:password@example.com:6379/ */
    size_t l = strlen(getenv("REDIS_URL"));
    char *redis_address = NULL;
    char *redis_password = NULL;
    char *redis_port = NULL;
    if (l < 1024) {
      strcpy(tmp_url_parsing, getenv("REDIS_URL"));
      int flag = 0;
      for (size_t i = 0; i < l; i++) {
        if (tmp_url_parsing[i] == ':' && tmp_url_parsing[i + 1] == '/' &&
            tmp_url_parsing[i + 2] == '/') {
          redis_address = tmp_url_parsing + i + 2;
          i += 2;
        } else if (tmp_url_parsing[i] == ':') {
          tmp_url_parsing[i] = 0;
          if (redis_address)
            redis_port = tmp_url_parsing + i + 1;
        } else if (tmp_url_parsing[i] == '@' && flag == 0) {
          /* there was a password involved */
          flag = 1;
          redis_password = redis_port;
          redis_address = NULL;
        } else if (tmp_url_parsing[i] == '/')
          flag = 1;
      }
    }
    if (!fio_cli_get_str("ra"))
      fio_cli_set_str("ra", redis_address);
    if (!fio_cli_get_str("rp"))
      fio_cli_set_str("rp", redis_port);
    if (!fio_cli_get_str("rpw"))
      fio_cli_set_str("rpw", redis_password);
  }
}

void free_cli(void) { fio_cli_end(); }
