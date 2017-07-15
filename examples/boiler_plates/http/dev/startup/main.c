/*
Edit this file to add any command line argument handling and core setup concerns
(i.e. replacing the default pub/sub engine with a Redis engine).
*/

/* declared here, implemented in setup.h */
void setup_network_services(void);

/* declared here, implemented in setup.h */
#include "main.h"
#include "fio_cli_helper.h"
#include "redis_engine.h"
#include <string.h>

const int ARGC;
char const **ARGV;
int VERBOSE = 0;
const char *HTTP_PUBLIC_FOLDER;
const char *HTTP_PORT;
const char *HTTP_ADDRESS;
unsigned long HTTP_BODY_LIMIT;
unsigned char HTTP_TIMEOUT;
unsigned long WEBSOCKET_MSG_LIMIT;
unsigned char WEBSOCKET_PING_INTERVAL;

static char tmp_url_parsing[1024];
/*
clang-format off

Available command line flags:
-p <port>            : defaults port 3000.
-t <threads>         : number of threads per process, defaults to 0 (automatic CPU core test/set).
-w <processes>       : number of processes, defaults to 0 (automatic CPU core test/set).
-v                   : request verbosity (logging).
-r <address> <port>  : a spece delimited couplet for the Redis address and port for pub/sub.
-k <0..255>          : HTTP keep-alive timeout. default (0) reverts to ~5 seconds.
-ping <0..255>       : websocket ping interval. default (0) reverts to ~40 seconds.
-public <folder>     : public folder, for static file service. default (NULL) disables static file service.
-maxbd <Mb>          : HTTP upload limit. default to ~50Mb.
-maxms <Kb>          : incoming websocket message size limit. default to ~250Kb.
-?                   : print command line options.

clang-format on
*/
int main(int argc, char const *argv[]) {
  {
    /* setup global access to ARGC and ARGV */
    int *tmp = (int *)&ARGC;
    *tmp = argc;
    ARGV = argv;
  }

  const char *redis_address = NULL;
  const char *redis_port = NULL;
  const char *redis_password = NULL;
  uint32_t threads = 0;
  uint32_t workers = 0;

  /*     ****  Environmental defaults ****     */

  if (getenv("PORT"))
    HTTP_PORT = getenv("PORT");
  if (getenv("ADDRESS"))
    HTTP_ADDRESS = getenv("ADDRESS");
  if (getenv("HTTP_PUBLIC_FOLDER"))
    HTTP_PUBLIC_FOLDER = getenv("HTTP_PUBLIC_FOLDER");

  /* parse the Redis URL to manage redis pub/sub */
  if (getenv("REDIS_URL")) {
    /* We assume a format such as redis://user:password@example.com:6379/ */
    size_t l = strlen(getenv("REDIS_URL"));
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
  }

  /*     ****  Command line arguments ****     */

  fio_cli_start(argc, argv, NULL);
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

  if (fio_cli_get_str("p"))
    HTTP_PORT = fio_cli_get_str("p");
  if (fio_cli_get_str("www")) {
    HTTP_PUBLIC_FOLDER = fio_cli_get_str("www");
  }
  if (fio_cli_get_str("t"))
    threads = fio_cli_get_int("t");
  if (fio_cli_get_str("w"))
    workers = fio_cli_get_int("w");
  VERBOSE = fio_cli_get_int("v");
  if (fio_cli_get_str("redis"))
    redis_address = fio_cli_get_str("redis");
  if (fio_cli_get_str("redis-port"))
    redis_port = fio_cli_get_str("redis-port");
  if (fio_cli_get_str("redis-port"))
    redis_password = fio_cli_get_str("redis-password");
  HTTP_TIMEOUT = fio_cli_get_int("keep-alive");
  WEBSOCKET_PING_INTERVAL = fio_cli_get_int("ping");
  if (fio_cli_get_int("maxbd"))
    HTTP_BODY_LIMIT = fio_cli_get_int("maxbd");
  if (fio_cli_get_int("maxms"))
    WEBSOCKET_MSG_LIMIT = fio_cli_get_int("maxms");

  if (!HTTP_PORT)
    HTTP_PORT = "3000";

  /*     ****  actual code ****     */

  /* set Redis pub/sub if requested */
  if (redis_address) {
    PUBSUB_DEFAULT_ENGINE =
        redis_engine_create(.address = redis_address, .port = redis_port,
                            .auth = redis_password, .ping_interval = 40);
    if (!PUBSUB_DEFAULT_ENGINE) {
      perror("\nERROR: couldn't initialize Redis engine.\n");
      exit(-2);
    }
  }

  /* initialize network services */
  setup_network_services();

  /* start facil */
  facil_run(.threads = threads, .processes = workers);

  /* clean up Redis, if exists */
  if (PUBSUB_DEFAULT_ENGINE != PUBSUB_CLUSTER_ENGINE) {
    redis_engine_destroy(PUBSUB_DEFAULT_ENGINE);
    PUBSUB_DEFAULT_ENGINE = PUBSUB_CLUSTER_ENGINE;
  }
  return 0;
}
