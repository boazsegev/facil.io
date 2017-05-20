#include "facil.h"
#include "http.h"

#include "tests/http_bench.inc"
int main() {
  listen2bench();
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 1, .processes = 1);
}
