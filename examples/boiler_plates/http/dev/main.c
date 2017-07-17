#include "main.h"

int main(int argc, char const *argv[]) {
  /* accept command line arguments and setup default values. */
  initialize_cli(argc, argv);

  /* initialize HTTP service */
  initialize_http_service();

  /* start facil */
  facil_run(.threads = fio_cli_get_int("t"), .processes = fio_cli_get_int("w"));

  free_cli();
  return 0;
}
