#include "http-status.h"

/* ////////////////////////////////////////////////////////////
This is a status code map that takes common codes and returns a string.
//////////////////////////////////////////////////////////// */

struct Pair {
  int i_status;
  char* s_status;
};

static struct Pair List[] = {{200, "OK"},
                             {301, "Moved Permanently"},
                             {302, "Found"},
                             {100, "Continue"},
                             {101, "Switching Protocols"},
                             {403, "Forbidden"},
                             {404, "Not Found"},
                             {400, "Bad Request"},
                             {500, "Internal Server Error"},
                             {501, "Not Implemented"},
                             {502, "Bad Gateway"},
                             {503, "Service Unavailable"},
                             {102, "Processing"},
                             {201, "Created"},
                             {202, "Accepted"},
                             {203, "Non-Authoritative Information"},
                             {204, "No Content"},
                             {205, "Reset Content"},
                             {206, "Partial Content"},
                             {207, "Multi-Status"},
                             {208, "Already Reported"},
                             {226, "IM Used"},
                             {300, "Multiple Choices"},
                             {303, "See Other"},
                             {304, "Not Modified"},
                             {305, "Use Proxy"},
                             {306, "(Unused)	"},
                             {307, "Temporary Redirect"},
                             {308, "Permanent Redirect"},
                             {401, "Unauthorized"},
                             {402, "Payment Required"},
                             {405, "Method Not Allowed"},
                             {406, "Not Acceptable"},
                             {407, "Proxy Authentication Required"},
                             {408, "Request Timeout"},
                             {409, "Conflict"},
                             {410, "Gone"},
                             {411, "Length Required"},
                             {412, "Precondition Failed"},
                             {413, "Payload Too Large"},
                             {414, "URI Too Long"},
                             {415, "Unsupported Media Type"},
                             {416, "Range Not Satisfiable"},
                             {417, "Expectation Failed"},
                             {421, "Misdirected Request"},
                             {422, "Unprocessable Entity"},
                             {423, "Locked"},
                             {424, "Failed Dependency"},
                             {425, "Unassigned"},
                             {426, "Upgrade Required"},
                             {427, "Unassigned"},
                             {428, "Precondition Required"},
                             {429, "Too Many Requests"},
                             {430, "Unassigned"},
                             {431, "Request Header Fields Too Large"},
                             {504, "Gateway Timeout"},
                             {505, "HTTP Version Not Supported"},
                             {506, "Variant Also Negotiates"},
                             {507, "Insufficient Storage"},
                             {508, "Loop Detected"},
                             {509, "Unassigned"},
                             {510, "Not Extended"},
                             {511, "Network Authentication Required"},
                             {0, 0}};

static char* to_s(int status) {
  int pos = 0;
  while (List[pos].i_status) {
    if (List[pos].i_status == status)
      return List[pos].s_status;
    pos++;
  }
  return 0;
}
struct HttpStatus_API____ HttpStatus = {.to_s = to_s};
