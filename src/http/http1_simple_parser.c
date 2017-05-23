/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define _GNU_SOURCE
#include "http1_simple_parser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __has_include
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define HAVE_X86Intrin
// see:
// https://software.intel.com/en-us/node/513411
// quick reference:
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/
// pdf guide:
// https://software.intel.com/sites/default/files/a6/22/18072-347603.pdf
#endif
#endif

/* *****************************************************************************
Useful macros an helpers
*/

/*
#define a2i(a)                                                                 \
  (((a) >= '0' && a <= '9') ? ((a) - '0') : ({                                 \
    return -1;                                                                 \
    0;                                                                         \
  }))
*/

#define CHECK_END()                                                            \
  {                                                                            \
    request->udata = pos;                                                      \
    if (pos >= end) {                                                          \
      return -2;                                                               \
    }                                                                          \
  }

#define EAT_EOL()                                                              \
  {                                                                            \
    if (*pos == '\r' || *pos == 0)                                             \
      *(pos++) = 0;                                                            \
    if (*pos == '\n' || *pos == 0)                                             \
      *(pos++) = 0;                                                            \
  }
#undef EAT_EOL
#define EAT_EOL()                                                              \
  {                                                                            \
    *(pos++) = 0;                                                              \
    *(pos++) = 0;                                                              \
  }

static inline char *seek_to_char(char *start, char *end, char tok) {
  while (start < end) {
    if (*start == tok)
      return start;
    ++start;
  }
  return NULL;
}
/*
#define seek_to_char(start, end, tok) memchr(start, tok, (end) - (start))
*/

static inline char *seek_to_2eol(char *start, char *end) {
  char *ret = seek_to_char(start, end, '\n');
  return ret ? (*(ret - 1) == '\r' ? ret - 1 : ret) : NULL;
  // while (start < end) {
  //   if ((*start == '\r' && *(start + 1) == '\n') || *start == '\n')
  //     return start;
  //   ++start;
  // }
  // return NULL;
}

#define HOST "host"
#define CONTENT_TYPE "content-type"
#define CONTENT_LENGTH "content-length"
#define UPGRADE "upgrade"
#define CONNECTION "connection"

#if defined(HTTP_HEADERS_LOWERCASE) && HTTP_HEADERS_LOWERCASE == 1
/* header is lowercase */

#define to_lower(c)                                                            \
  if ((c) >= 'A' && (c) <= 'Z')                                                \
    (c) |= 32;

/* reviews the latest header and updates any required data in the request
 * structure. */
static inline ssize_t review_header_data(http_request_s *request,
                                         http_header_s *header) {
  if (header->name_len == 4 &&
      *((uint32_t *)header->name) == *((uint32_t *)HOST)) { // exact match
    request->host = (void *)header->data;
    request->host_len = header->data_len;
  } else if (header->name_len == 12 &&
             *((uint64_t *)(header->name + 3)) ==
                 *((uint64_t *)(CONTENT_TYPE + 3))) { // almost
    request->content_type = (void *)header->data;
    request->content_type_len = header->data_len;
  } else if (header->name_len == 14 &&
             *((uint64_t *)(header->name + 3)) ==
                 *((uint64_t *)(CONTENT_LENGTH + 3))) { // close match
    // tmp still holds a pointer to the value
    size_t c_len = 0;
    char *tmp = (char *)header->data;
    while (*tmp) {
      if ((*tmp) >= '0' && (*tmp) <= '9')
        c_len = (c_len * 10) + ((*tmp) - '0');
      else
        return -1;
      ++tmp;
    };
    request->content_length = c_len;
  } else if (header->name_len == 7 &&
             *((uint64_t *)header->name) ==
                 *((uint64_t *)UPGRADE)) { // matches also the NUL character
    request->upgrade = (void *)header->data;
    request->upgrade_len = header->data_len;
  } else if (header->name_len == 10 &&
             *((uint64_t *)header->name) ==
                 *((uint64_t *)CONNECTION)) { // a close enough match
    request->connection = (void *)header->data;
    request->connection_len = header->data_len;
  }
  return 0;
}

#else
/* unknown header case */
#define to_lower(c)

static inline ssize_t review_header_data(http_request_s *request,
                                         http_header_s *header) {
  if (header->name_len == 4 &&
      strncasecmp((char *)header->name, HOST, 4) == 0) {
    request->host = (void *)header->data;
    request->host_len = header->data_len;
  } else if (header->name_len == 12 &&
             strncasecmp((char *)header->name, CONTENT_TYPE, 12) == 0) {
    request->content_type = (void *)header->data;
    request->content_type_len = header->data_len;
  } else if (header->name_len == 14 &&
             strncasecmp((char *)header->name, CONTENT_LENGTH, 14) == 0) {
    // tmp still holds a pointer to the value
    size_t c_len = 0;
    char *tmp = (char *)header->data;
    while (*tmp) {
      if ((*tmp) >= '0' && (*tmp) <= '9')
        c_len = (c_len * 10) + ((*tmp) - '0');
      else
        return -1;
      ++tmp;
    };
    request->content_length = c_len;
  } else if (header->name_len == 7 &&
             strncasecmp((char *)header->name, UPGRADE, 7) == 0) {
    request->upgrade = (void *)header->data;
    request->upgrade_len = header->data_len;
  } else if (header->name_len == 10 &&
             strncasecmp((char *)header->name, CONNECTION, 10) == 0) {
    request->connection = (void *)header->data;
    request->connection_len = header->data_len;
  }
  return 0;
}
#endif

static void no_on_header_found(http_request_s *request, http_header_s *header) {
  (void)request;
  (void)header;
}
/* *****************************************************************************
The (public) parsing
*/

/**
Parses HTTP request headers. This allows review of the expected content
length
before accepting any content (server resource management).

Returns the number of bytes consumed before the full request was accepted.

Returns 0 if the headers were parsed and waiting on body parsing to complete.
Returns -1 on fatal error (i.e. protocol error).
Returns -2 when the request parsing didn't complete.

Incomplete request parsing updates the content in the buffer. The same
buffer
and the same `http_request_s` should be returned to the parsed on the "next
round", only the `len` argument is expected to grow.
*/
ssize_t http1_parse_request_headers(
    void *buffer, size_t len, http_request_s *request,
    void (*on_header_found)(http_request_s *request, http_header_s *header)) {
  if (request == NULL || buffer == NULL)
    return -1;
  if (!on_header_found)
    on_header_found = no_on_header_found;
  if (request->body_str || request->body_file > 0)
    return 0;
  if (len == 0)
    return -2;
  http_header_s header;
  char *pos = (char *)buffer;
  char *end = (char *)buffer + len;
  char *next, *tmp;
  // collect method and restart parser if already collected
  if (request->method == NULL) {
    // eat empty spaces
    while ((*pos == '\n' || *pos == '\r') && pos < end)
      ++pos;
    CHECK_END();
    request->method = (char *)pos;
    next = seek_to_char(pos, end, ' ');
    if (next == NULL) {
      request->method = NULL;
      return -2;
    }
    request->method_len = (uintptr_t)next - (uintptr_t)pos;
    pos = next;
    *(pos++) = 0;
    CHECK_END();
  } else {
    /* use the `udata` pointer to store current position in the buffer */
    pos = request->udata;
    CHECK_END();
  }
  // collect path
  if (request->path == NULL) {
    next = seek_to_char(pos, end, ' ');
    if (next == NULL)
      return -2;
    request->path = (char *)pos;
    request->path_len = next - pos;
    tmp = seek_to_char(pos, next, '?');
    if (tmp) {
      request->path_len = tmp - pos;
      *(tmp++) = 0;
      request->query = (char *)tmp;
      request->query_len = next - tmp;
    }
    pos = next;
    *(pos++) = 0;
    CHECK_END();
  }
  // collect version
  if (request->version == NULL) {
    next = seek_to_2eol(pos, end);
    if (next == NULL)
      return -2;
    request->version = (char *)pos;
    request->version_len = (uintptr_t)next - (uintptr_t)pos;
    pos = next;
    EAT_EOL();
    CHECK_END();
  }

  // collect headers
  while (pos < end && *pos != 0) { /* NUL as term */
    if (request->headers_count >= HTTP1_MAX_HEADER_COUNT)
      return -1;
    next = seek_to_2eol(pos, end);
    if (next == NULL)
      return -2;
    if (next == pos) /*headers finished */
      break;
#if defined(HTTP_HEADERS_LOWERCASE) && HTTP_HEADERS_LOWERCASE == 1
    tmp = pos;
    while (tmp < next && *tmp != ':') {
      to_lower(*tmp);
      ++tmp;
    }
    if (tmp == next)
      return -1;
#else
    tmp = seek_to_char(pos, next, ':');
    if (!tmp)
      return -1;
#endif
    header.name = (void *)pos;
    header.name_len = (uintptr_t)(tmp - pos);
    *(tmp++) = 0;
    if (*tmp == ' ')
      *(tmp++) = 0;
    header.data = (void *)tmp;
    header.data_len = (uintptr_t)(next - tmp);
    // eat EOL before content-length processing.
    pos = next;
    EAT_EOL();
    // print debug info
    // fprintf(stderr, "Got header %s (%u): %s (%u)\n", header.name,
    //         header.name_len, header.data, header.data_len);
    // check special headers and assign value.
    review_header_data(request, &header);
    request->headers_count += 1;
    on_header_found(request, &header);
    // advance header position
    CHECK_END();
  }
  // Did we break because there's no more data or end of headers?
  CHECK_END();
  // "eat" the end of headers line and finish processing.
  EAT_EOL();
  if (request->content_length &&
      (end - pos) >= (ssize_t)request->content_length) {
    request->body_str = (void *)pos;
    // fprintf(stderr,
    //         "assigning body to string. content-length %lu, buffer left: "
    //         "%lu/%lu\n(%lu) %p:%.*s\n",
    //         request->content_length, end - pos, len, request->content_length,
    //         request->body_str, (int)request->content_length,
    //         request->body_str);
    return (ssize_t)(pos - (char *)buffer) + request->content_length;
  }
  // we're done.
  return (ssize_t)(pos - (char *)buffer);
}

/**
Parses HTTP request body content (if any).

Returns the number of bytes consumed before the body consumption was complete.

Returns -1 on fatal error (i.e. protocol error).
Returns -2 when the request parsing didn't complete.

Incomplete body parsing doesn't effect the buffer received. It is expected that
the next "round" will contain fresh data in the `buffer` argument.
*/
ssize_t http1_parse_request_body(void *buffer, size_t len,
                                 http_request_s *request) {
  if (request == NULL)
    return -1;
  // is body parsing needed?
  if (request->content_length == 0 || request->body_str)
    return request->content_length;

  if (!request->body_file) {
// create a temporary file to contain the data.
#ifdef P_tmpdir
#if defined(__linux__) /* linux doesn't end with a divider */
    char template[] = P_tmpdir "/http_request_body_XXXXXXXX";
#else
    char template[] = P_tmpdir "http_request_body_XXXXXXXX";
#endif
#else
    char template[] = "/tmp/http_request_body_XXXXXXXX";
#endif
    request->body_file = mkstemp(template);
    if (request->body_file == -1)
      return -1;
    // use the `next` field to store parser state.
    uintptr_t *tmp = (uintptr_t *)(&request->udata);
    *tmp = 0;
  }
  // make sure we have anything to read. This might be an initializing call.
  if (len == 0)
    return ((uintptr_t)(request->udata)) >= request->content_length ? 0 : (-2);
  // Calculate how much of the buffer should be read.
  ssize_t to_read =
      ((request->content_length - ((uintptr_t)request->udata)) < len)
          ? (request->content_length - ((uintptr_t)request->udata))
          : len;
  // write the data to the temporary file.
  if (write(request->body_file, buffer, to_read) < to_read)
    return -1;
  // update the `next` field data with the received content length
  uintptr_t *tmp = (uintptr_t *)(&request->udata);
  *tmp += to_read; // request->metadata.next += to_read;

  // check the state and return.
  if (((uintptr_t)request->udata) >= request->content_length) {
    lseek(request->body_file, 0, SEEK_SET);
    return to_read;
  }
  return -2;
}

#if defined(DEBUG) && DEBUG == 1

// #include "http1_request.h"

#include <time.h>

void http1_parser_test(void) {
  char request_text[] = "GET /?a=b HTTP/1.1\r\n"
                        "Host: local\r\n"
                        "Upgrade: websocket\r\n"
                        "Content-Length: 12\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "Hello World!\r\n";
  size_t request_length = sizeof(request_text) - 1;
  http_request_s *request = http_request_create(HTTP_V1);
  ssize_t ret =
      http1_parse_request_headers(request_text, request_length, request, NULL);
  if (ret == -1) {
    fprintf(stderr, "* Parser FAILED -1.\n");
  } else if (ret == -2) {
    fprintf(stderr, "* Parser FAILED -2.\n");
  } else {
#define pok(true_str, false_str, result, expected)                             \
  (((result) == (expected)) ? fprintf(stderr, true_str)                        \
                            : fprintf(stderr, false_str))
    pok("* Correct Return\n", "* WRONG Return\n", ret,
        sizeof(request_text) - 3);
    pok("* Correct Method\n", "* WRONG Method\n",
        strcmp(request->method, "GET"), 0);
    pok("* Correct Method length\n", "* WRONG Method length",
        request->method_len, 3);
    pok("* Correct path\n", "* WRONG path", strcmp(request->path, "/"), 0);
    pok("* Correct path length\n", "* WRONG path length", request->path_len, 1);
    pok("* Correct query\n", "* WRONG query", strcmp(request->query, "a=b"), 0);
    pok("* Correct query length\n", "* WRONG query length", request->query_len,
        3);
    pok("* Correct host\n", "* WRONG host\n", strcmp(request->host, "local"),
        0);
    pok("* Correct Method length\n", "* WRONG Method length\n",
        request->host_len, 5);
    pok("* Correct header count\n", "* WRONG header count\n",
        request->headers_count, 4);
    pok("* Correct content length\n", "* WRONG content length\n",
        request->content_length, 12);
    pok("* Correct body\n", "* WRONG body\n",
        memcmp(request->body_str, "Hello World!", request->content_length), 0);
    fprintf(stderr, "%.*s\n", (int)request->content_length, request->body_str);
#undef pok
  }
  http_request_clear(request);
  clock_t start, end;
  start = clock();
  for (size_t i = 0; i < 6000000; i++) {
    char request_text2[] = "GET /?a=b HTTP/1.1\r\n"
                           "Host: local\r\n"
                           "Upgrade: websocket\r\n"
                           "Content-Length: 12\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "Hello World!\r\n";
    if (http1_parse_request_headers(request_text2, request_length, request,
                                    NULL) <= 0 ||
        (request->upgrade_len != 9)) {
      fprintf(stderr, "* HTTP/1.1 simple_parser unknown error\n");
      return;
    }
    http_request_clear(request);
  }
  end = clock();
  fprintf(stderr, "7M requests in %lu cycles (%lf ms)\n", end - start,
          (double)(end - start) / (CLOCKS_PER_SEC / 1000));
  char request_text2[] = "GET /?a=b HTTP/1.1\r\n"
                         "Host: local\r\n"
                         "Upgrade: websocket\r\n"
                         "Content-Length: 12\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "Hello World!\r\n";
  fprintf(stderr, "start\n");
  if (http1_parse_request_headers(request_text2, 7, request, NULL) != -2)
    fprintf(stderr, "Fragmented Parsing FAILED\n");
  fprintf(stderr, "step\n");
  if (http1_parse_request_headers(request_text2, 27, request, NULL) != -2)
    fprintf(stderr, "Fragmented Parsing FAILED\n");
  fprintf(stderr, "step\n");
  if (http1_parse_request_headers(request_text2, 38, request, NULL) != -2)
    fprintf(stderr, "Fragmented Parsing FAILED\n");
  fprintf(stderr, "step\n");
  if ((ret = http1_parse_request_headers(request_text2, 98, request, NULL)) !=
      94)
    fprintf(stderr, "Fragmented Parsing (some body) FAILED\n");
  fprintf(stderr, "read: %lu\n", ret);
  if ((ret += http1_parse_request_body(request_text2 + ret,
                                       request_length - ret, request)) < 98)
    fprintf(stderr, "Body parsing FAILED\n");
  fprintf(stderr, "step\n");
  if (request->body_file <= 0)
    fprintf(stderr, "Body file FAILED\n");
  fprintf(stderr, "step\n");
  ret = read(request->body_file, request_text, request->content_length);
  if (ret < 0)
    perror("Couldn't read temporary file");
  fprintf(stderr, "Body:\n%.*s\n", (int)request->content_length, request_text);

  http_request_destroy(request);
}

#endif
