#define SHOOTOUT_USE_DIRECT_WRITE 1
// update the demo.c file to use the existing folder structure and makefile
#include "tests/http_ws.h" // includes the "http.h" header
// #include "tests/client.h" // includes the "http.h" header
// #include "mempool.h"                  // includes the "http.h" header
// #include "tests/srv_tasks.h" // includes the "http.h" header
// #include "tests/websocket-shootout.h" // includes the "http.h" header

// void hpack_test_huffman(void);
// void hpack_test_int_primitive(void);
// void hpack_test_string_packing(void);
// int main(void) {
//   hpack_test_string_packing();
//   hpack_test_huffman();
//   hpack_test_int_primitive();
//   mempool_test();
//   // print_huff();
//   fprintf(stderr, "Done.\n");
//   return 0;
// }

// #include "http.h"
// void on_http_hello(http_request_s *req) {
//   http_response_s response = http_response_init(req);
//   http_response_log_start(&response);
//   http_response_write_body(&response, "Hello World!", 12);
//   http_response_finish(&response);
// }

/*****************************
The main function
*/
// #undef THREAD_COUNT
#ifndef THREAD_COUNT
#define THREAD_COUNT 16
#endif

void async_test_library_speed(void);

int main(void) {
  // mempool_test();
  // spn_lock_test();
  // http_parser_test();
  HTTP_WEBSOCKET_TEST();
  // HTTP_SHOOTOUT_TEST();
  // SRV_TASKS_TEST("3000", NULL);
  // CLIENT_MODE_TEST();
  // async_test_library_speed();
}
