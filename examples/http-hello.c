/**
In this a Hello World example using the bundled HTTP / WebSockets extension.

Compile using:

    NAME=http make

Or test the `poll` engine's performance by compiling with `poll`:

    FIO_POLL=1 NAME=http make

Run with:

    ./tmp/http -t 1

Benchmark with keep-alive:

    ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
    wrk -c200 -d4 -t1 http://localhost:3000/

Benchmark with higher load:

    ab -c 4400 -t 4 -n 1000000 -k http://127.0.0.1:3000/
    wrk -c4400 -d4 -t1 http://localhost:3000/
*/

/* include the core library, without any extensions */
#include <fio.h>

#include <fio_cli.h>
#include <http.h>

/* defined later */
void initialize_cli(int argc, char const *argv[]);

/* *****************************************************************************
HTTP request / response handling
***************************************************************************** */

static void on_http_request(http_s *h) {
  /* set a response and send it (finnish vs. destroy). */
  http_send_body(h, "Hello World!", 12);
}

/* *****************************************************************************
The main function
***************************************************************************** */

int main(int argc, char const *argv[]) {
  initialize_cli(argc, argv);
  /* listen for inncoming connections */
  if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
                  .on_request = on_http_request,
                  .max_body_size = fio_cli_get_i("-maxbd"),
                  .public_folder = fio_cli_get("-public"),
                  .log = fio_cli_get_bool("-log"),
                  .timeout = fio_cli_get_i("-keep-alive")) == -1) {
    /* listen failed ?*/
    perror(
        "ERROR: facil.io couldn't initialize HTTP service (already running?)");
    exit(1);
  }
  fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));
  return 0;
}

/* *****************************************************************************
CLI helpers
***************************************************************************** */
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
