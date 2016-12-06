// update the demo.c file to use the existing folder structure and makefile
#include "tests/http_ws.h" // includes the "http.h" header

/*****************************
The main function
*/
#define THREAD_COUNT 8
int main(int argc, char const *argv[]) {
  // spn_lock_test();
  // http_parser_test();
  // server_listen(.port = "4000", .on_open = on_open);
  HTTP_WEBSOCKET_TEST();
}
