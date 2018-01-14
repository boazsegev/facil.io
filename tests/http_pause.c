#include "http.h"

static void on_response_complete(http_s *h) {
  http_send_body(h, "counter to 1", 12);
}

static void on_timer(void *h) { http_resume(h, on_response_complete, NULL); }

static void on_paused(void *h) { facil_run_every(1000, 1, on_timer, h, NULL); }

static void on_request(http_s *h) { http_pause(h, on_paused); }

int main(void) {
  http_listen("3000", NULL, .on_request = on_request);
  facil_run(.threads = 1);
  return 0;
}
