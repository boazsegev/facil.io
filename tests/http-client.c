#include "http.h"
#include <signal.h>

static void on_response(http_s *h) {
  if (h->status == 0) {
    /* a connection was established, but nothing has happened so far. */
    h->path = fiobj_str_new("/", 1);
    http_finish(h);
    return;
  }
  FIOBJ str = http_req2str(h);
  fprintf(stderr, "Got the following response:\n%s\n",
          fiobj_obj2cstr(str).data);
  fiobj_free(str);
  kill(0, SIGINT);
}

int main(int __attribute__((unused)) argc,
         char __attribute__((unused)) const *argv[]) {
  /* code */
  http_connect("http://www.google.com/", .on_response = on_response);
  // http_connect("http://localhost:3000/", .on_response = on_response);
  facil_run(.threads = 1);
  return 0;
}
