// update the demo.c file to use the existing folder structure and makefile
#include "tests/http_ws.h" // includes the "http.h" header

void hpack_test_huffman(void);

int main(void) {
  hpack_test_huffman();
  // print_huff();
  return 0;
}

/*****************************
The main function
*/
// #define THREAD_COUNT 8
// int main(int argc, char const *argv[]) {
//   // spn_lock_test();
//   // http_parser_test();
//   HTTP_WEBSOCKET_TEST();
// }
