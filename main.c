#include "facil.h"
#include "http.h"

// #include "tests/http_bench.inc"
#include "tests/http_stress.inc"
#include "tests/shootout.h"

int main() {
  listen2stress("3000", "./public_www");
  listen2shootout("3030", 1);
  /* Run the server and hang until a stop signal is received. */
  facil_run(.threads = 8, .processes = 1);
}
