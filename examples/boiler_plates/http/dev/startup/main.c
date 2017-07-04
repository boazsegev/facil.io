/*
Edit this file to add any command line argument handling and core setup concerns
(i.e. replacing the default pub/sub engine with a Redis engine).
*/

/* declared here, implemented in setup.h */
void setup_network_services(void);

/* declared here, implemented in setup.h */
#include "main.h"
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

  if (argc == 2 && argv[1][0] == '-' &&
      (argv[1][1] == '?' || argv[1][1] == 'h') && argv[1][2] == 0) {
    fprintf(
        stderr,
        "Available command line options\n"
        "   -p <port>            : "
        "defaults port 3000.\n"
        "   -t <threads>         : "
        "number of threads per process."
        "defaults to 0 (automatic CPU core test/set).\n"
        "   -w <processes>       : "
        "number of processes. defaults to 0 (automatic CPU core test/set).\n"
        "   -v                   : "
        "request verbosity (logging)."
        "   -r <address> <port>  : "
        "a spece delimited couplet for the Redis address and port for "
        "pub/sub.\n"
        "   -k <0..255>          : "
        "HTTP keep-alive timeout. default (0) reverts to ~5 seconds."
        "   -ping <0..255>       : "
        "websocket ping interval. default (0) reverts to ~40 seconds.\n"
        "   -public <folder>     : "
        "public folder, for static file service."
        "default (NULL) disables static file service.\n"
        "   -maxbd <Mb>          : "
        "HTTP upload limit. default to ~50Mb.\n"
        "   -maxms <Kb>          : "
        "incoming websocket message size limit. default to ~250Kb.\n"
        "   -?                   : "
        "print command line options.\n"
        "\n");
    return 0;
  }

  for (int i = 1; i < ARGC; i++) {
    int offset = 0;
    if (ARGV[i][0] == '-') {
      switch (ARGV[i][1]) {

      case 'v': /* logging */
        if (ARGV[i][2])
          goto unknown_option;
        VERBOSE = 1;
        break;

      case 't': /* threads */
        if (!ARGV[i][2])
          i++;
        else
          offset = 2;
        if ((*(ARGV[i] + offset) >= '9' || *(ARGV[i] + offset) <= '0'))
          goto unknown_option;
        threads = atoi(ARGV[i] + offset);
        break;

      case 'w': /* processes */
        if (!ARGV[i][2])
          i++;
        else
          offset = 2;
        workers = atoi(ARGV[i] + offset);
        break;

      case 'k': /* keep-alive timeout */
        if (!ARGV[i][2])
          i++;
        else
          offset = 2;
        if ((*(ARGV[i] + offset) >= '9' || *(ARGV[i] + offset) <= '0'))
          goto unknown_option;
        HTTP_TIMEOUT = atoi(ARGV[i] + offset);
        break;

      case 'p': /* port (p), ping or public */
        if (!ARGV[i][2] || (ARGV[i][2] <= '9' && ARGV[i][2] >= '0')) {
          /* port */
          offset = 2;
          if (!ARGV[i][2]) {
            i++;
            offset = 0;
          }
          HTTP_PORT = ARGV[i] + offset;
          if ((*HTTP_PORT >= '9' || *HTTP_PORT <= '0'))
            goto unknown_option;

        } else if (ARGV[i][2] && ARGV[i][2] == 'i' && ARGV[i][3] &&
                   ARGV[i][3] == 'n' && ARGV[i][4] && ARGV[i][4] == 'g') {
          /* ping */
          offset = 5;
          if (!ARGV[i][5]) {
            i++;
            offset = 0;
          }

          WEBSOCKET_PING_INTERVAL = atoi(ARGV[i] + offset);

        } else if (ARGV[i][2] && ARGV[i][2] == 'u' && ARGV[i][3] &&
                   ARGV[i][3] == 'b' && ARGV[i][4] && ARGV[i][4] == 'l' &&
                   ARGV[i][5] && ARGV[i][5] == 'i' && ARGV[i][6] &&
                   ARGV[i][6] == 'c') {
          /* public */
          offset = 7;
          if (!ARGV[i][7]) {
            i++;
            offset = 0;
          }
          HTTP_PUBLIC_FOLDER = ARGV[i] + offset;

        } else
          goto unknown_option;
        break;

      case 'm': /* maxbd or maxms */
        offset = 6;
        if (ARGV[i][2] && ARGV[i][2] == 'a' && ARGV[i][3] &&
            ARGV[i][3] == 'x' && ARGV[i][4] && ARGV[i][4] == 'b' &&
            ARGV[i][5] && ARGV[i][5] == 'd' &&
            (!ARGV[i][6] || (ARGV[i][6] >= '0' && ARGV[i][6] <= '9'))) {
          /* maxbd */
          if (!ARGV[i][offset]) {
            i++;
            offset = 0;
          }
          HTTP_BODY_LIMIT = atoi(ARGV[i] + offset) * (1024 * 1024);

        } else if (ARGV[i][2] && ARGV[i][2] == 'a' && ARGV[i][3] &&
                   ARGV[i][3] == 'x' && ARGV[i][4] && ARGV[i][4] == 'm' &&
                   ARGV[i][5] && ARGV[i][5] == 's' &&
                   (!ARGV[i][6] || (ARGV[i][6] >= '0' && ARGV[i][6] <= '9'))) {
          /* maxms */
          if (!ARGV[i][offset]) {
            i++;
            offset = 0;
          }
          WEBSOCKET_MSG_LIMIT = atoi(ARGV[i] + offset) * 1024;

        } else
          goto unknown_option;
        break;

      case 'r': /* resid */
        if (!ARGV[i][2])
          i++;
        else
          offset = 2;
        redis_address = ARGV[i] + offset;
        offset = 0;
        while (ARGV[i + 1][offset]) {
          if (ARGV[i + 1][offset] < '0' || ARGV[i + 1][offset] > '9') {
            fprintf(stderr, "\nERROR: invalid redis port %s\n", ARGV[i + 1]);
            exit(-1);
          }
        }
        redis_port = ARGV[i + 1];
        break;
      }
    } else {
    unknown_option:
      fprintf(stderr, "ERROR: unknown option: %s\n", ARGV[i]);
      return -1;
    }
  }

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
