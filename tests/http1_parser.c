#define HTTP1_TEST_PARSER 1
#define HTTP_ADD_CONTENT_LENGTH_HEADER_IF_MISSING 1
#define HTTP1_ALLOW_CHUNKED_IN_MIDDLE_OF_HEADER 1
#include "http1_parser.h"
int main(void) { http1_parser_test(); }
