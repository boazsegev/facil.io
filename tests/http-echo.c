#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fio_mem.h"
#include "http.h"

static void on_request(http_s *h) {
  http_parse_body(h);
  http_parse_query(h);
  http_parse_cookies(h, 1);
  FIOBJ echo = http_req2str(h);
  if (!echo) {
    http_send_error(h, 500);
    return;
  }
  fiobj_str_write(echo, "\n\nKnown Params:\n", 16);
  fiobj_obj2json2(echo, h->params, 1);
  fiobj_str_write(echo, "\n\nKnown Cookies:\n", 17);
  fiobj_obj2json2(echo, h->cookies, 1);
  char value[40];
  size_t val_len = http_time2str(value, facil_last_tick().tv_sec);
  http_set_cookie(h, .name = "Late visit", .name_len = 10, .value = value,
                  .value_len = val_len);
  fio_cstr_s body = fiobj_obj2cstr(echo);
  http_send_body(h, body.data, body.len);
  fiobj_free(echo);
}

int main(void) {
  http_listen("3000", NULL, .on_request = on_request);
  facil_run(.threads = 2);
}
