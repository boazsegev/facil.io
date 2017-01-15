// update the demo.c file to use the existing folder structure and makefile
#include "tests/websocket-shootout.h" // includes the "http.h" header
// #include "tests/http_ws.h" // includes the "http.h" header
//
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

/*****************************
The main function
*/
#define THREAD_COUNT 8
int main(void) {

  // spn_lock_test();
  // http_parser_test();
  // HTTP_WEBSOCKET_TEST();
  HTTP_SHOOTOUT_TEST();
}
