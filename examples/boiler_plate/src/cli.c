#include <stdlib.h>
#include <string.h>

#include "cli.h"

#include "fio_cli.h"

void initialize_cli(int argc, char const *argv[]) {
  /*     ****  Command line arguments ****     */
  fio_cli_start(
      argc, argv, 0, 0, NULL,
      "-bind -b address to listen to. defaults any available.",
      "-port -p port number to listen to. defaults port 3000", FIO_CLI_TYPE_INT,
      "-workers -w number of processes to use.", FIO_CLI_TYPE_INT,
      "-threads -t number of threads per process.", FIO_CLI_TYPE_INT,
      "-log -v request verbosity (logging).", FIO_CLI_TYPE_BOOL,
      "-public -www public folder, for static file service.",
      "-keep-alive -k HTTP keep-alive timeout (0..255). default: ~5s",
      FIO_CLI_TYPE_INT, "-ping websocket ping interval (0..255). default: ~40s",
      FIO_CLI_TYPE_INT, "-max-body -maxbd HTTP upload limit. default: ~50Mb",
      FIO_CLI_TYPE_INT,
      "-max-message -maxms incoming websocket message size limit. default: "
      "~250Kb",
      FIO_CLI_TYPE_INT,
      "-redis-url -ru an optional Redis URL server address. i.e.: "
      "redis://user:password@localhost:6379/");

  /* Test and set any default options */
  if (!fio_cli_get("-p")) {
    /* Test environment as well */
    char *tmp = getenv("PORT");
    if (!tmp)
      tmp = "3000";
    /* Set default (unlike cmd line arguments, aliases are manually set) */
    fio_cli_set("-p", tmp);
    fio_cli_set("-port", tmp);
  }
  if (!fio_cli_get("-b")) {
    char *tmp = getenv("ADDRESS");
    if (tmp) {
      fio_cli_set("-b", tmp);
      fio_cli_set("-bind", tmp);
    }
  }
  if (!fio_cli_get("-public")) {
    char *tmp = getenv("HTTP_PUBLIC_FOLDER");
    if (tmp) {
      fio_cli_set("-public", tmp);
      fio_cli_set("-www", tmp);
    }
  }

  if (!fio_cli_get("-public")) {
    char *tmp = getenv("REDIS_URL");
    if (tmp) {
      fio_cli_set("-redis-url", tmp);
      fio_cli_set("-ru", tmp);
    }
  }
}

void free_cli(void) { fio_cli_end(); }
