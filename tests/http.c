/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
This is a simple HTTP "Hello World" / echo server example using `poll`.

Benchmark with keep-alive:

    ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
    wrk -c200 -d4 -t1 http://localhost:3000/

Note: This is a **TOY** example, no security whatsoever!!!
***************************************************************************** */

/* when compiling tests this is easier... */
#ifdef TEST_WITH_LIBRARY
#include "fio.h"
#else
#include "fio.c"
#endif

/* response string helper */
#define FIO_STR_NAME str
#include "fio-stl.h"

#define HTTP_CLIENT_BUFFER 32768
#define HTTP_MAX_HEADERS   16

/* an echo response is always dynamic (allocated on the heap). */
#define HTTP_RESPONSE_ECHO 1
/* if not echoing, the response is "Hello World" - but is it allocated? */
#define HTTP_RESPONSE_DYNAMIC 1

/* *****************************************************************************
Callbacks and object used by main()
***************************************************************************** */

/** Called to accept new connections */
FIO_SFUNC void on_open(int fd, void *udata);

/* *****************************************************************************
Starting the program - main()
***************************************************************************** */

int main(int argc, char const *argv[]) {
  /* initialize the CLI options */
  fio_cli_start(
      argc,
      argv,
      0, /* require 1 unnamed argument - the address to connect to */
      1,
      "A simple HTTP \"hello world\" example, listening on the "
      "speciified URL. i.e.\n"
      "\tNAME <url>\n\n"
      "Unix socket examples:\n"
      "\tNAME unix://./my.sock\n"
      "\tNAME /full/path/to/my.sock\n"
      "\nTCP/IP socket examples:\n"
      "\tNAME tcp://localhost:3000/\n"
      "\tNAME localhost://3000\n",
      FIO_CLI_STRING(
          "--bind -b (0.0.0.0:3000) The address to bind to, in URL format."),
      FIO_CLI_INT("--threads -t (1) The number of threads."),
      FIO_CLI_INT("--workers -w (1) The number of worker processes."),
      FIO_CLI_BOOL("--verbose -V -d print out debugging messages."),
      FIO_CLI_PRINT_LINE(
          "NOTE: requests are limited to 32Kb and 16 headers each."));

  /* review CLI for logging */
  if (fio_cli_get_bool("-V")) {
    FIO_LOG_INFO("Switching debug output on.");
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  }
  /* review CLI connection address (in URL format) */
  const char *url = fio_cli_get("-b");
  size_t url_len = url ? strlen(url) : 0;
  FIO_ASSERT(url_len < 1024, "URL address too long");
  FIO_ASSERT(fio_listen(url, .on_open = on_open) != -1,
             "Can't open listening socket %s",
             strerror(errno));

  /* select IO objects to be monitored */
  fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));
  /* cleanup */
  fio_cli_end();
  return 0;
}

/* *****************************************************************************
IO "Objects"and helpers
***************************************************************************** */
#include "http1_parser.h"

typedef struct {
  http1_parser_s parser;
  fio_uuid_s *uuid; /* valid only within an IO callback */
  struct {
    char *name;
    char *value;
    int32_t name_len;
    int32_t value_len;
  } headers[HTTP_MAX_HEADERS];
  char *method;
  char *path;
  char *body;
  int method_len;
  int path_len;
  int headers_len;
  int body_len;
  int buf_pos;
  int buf_consumed;
  char buf[]; /* header and data buffer */
} client_s;

#define FIO_REF_NAME client
#define FIO_REF_CONSTRUCTOR_ONLY
#define FIO_REF_FLEX_TYPE char
#include "fio-stl.h"

static void http_send_response(client_s *c,
                               int status,
                               fio_str_info_s status_str,
                               size_t header_count,
                               fio_str_info_s headers[][2],
                               fio_str_info_s body);

/** Called there's incoming data (from STDIN / the client socket. */
FIO_SFUNC void on_data(fio_uuid_s *uuid, void *udata);

fio_protocol_s HTTP_PROTOCOL_1 = {
    .on_data = on_data,
    .on_close = (void (*)(void *))(uintptr_t)client_free,
    .timeout = 5,
};

/* *****************************************************************************
IO callback(s)
***************************************************************************** */

/** Called to accept new connections */
FIO_SFUNC void on_open(int fd, void *udata) {
  client_s *c = client_new(HTTP_CLIENT_BUFFER);
  FIO_ASSERT_ALLOC(c);
  fio_attach_fd(fd, &HTTP_PROTOCOL_1, c, NULL);
  FIO_LOG_DEBUG2("Accepted a new HTTP connection (at %d): %p", fd, (void *)c);
  (void)udata;
}

/** Called there's incoming data (from STDIN / the client socket. */
FIO_SFUNC void on_data(fio_uuid_s *uuid, void *udata) {
  client_s *c = udata;
  c->uuid = uuid;
  ssize_t r =
      fio_read(uuid, c->buf + c->buf_pos, HTTP_CLIENT_BUFFER - c->buf_pos);
  if (r > 0) {
    c->buf_pos += r;
    c->buf[c->buf_pos] = 0;
    while ((r = http1_parse(&c->parser,
                            c->buf + c->buf_consumed,
                            (size_t)(c->buf_pos - c->buf_consumed)))) {
      c->buf_consumed += r;
      if (!http1_complete(&c->parser))
        break;
      if (c->buf_consumed == c->buf_pos)
        c->buf_pos = c->buf_consumed = 0;
      else {
        c->buf_pos = c->buf_pos - c->buf_consumed;
        memmove(c->buf, c->buf + c->buf_consumed, c->buf_pos);
        c->buf_consumed = 0;
      }
    }
  }
  return;
}

/* *****************************************************************************
HTTP/1.1 callback(s)
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
#ifdef HTTP_RESPONSE_ECHO
  http_send_response(c,
                     200,
                     (fio_str_info_s){"OK", 2},
                     0,
                     NULL,
                     (fio_str_info_s){c->method, strlen(c->method)});

#elif HTTP_RESPONSE_DYNAMIC
  http_send_response(c,
                     200,
                     (fio_str_info_s){"OK", 2},
                     0,
                     NULL,
                     (fio_str_info_s){"Hello World!", 12});
#else
  char *response =
      "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!\n";
  fio_write2(c->fd,
             .data.buf = response,
             .len = strlen(response),
             .dealloc = FIO_DEALLOC_NOOP);
  (void)http_send_response; /* unused in this branch */
#endif

  /* reset client request data */
  c->method = NULL;
  c->path = NULL;
  c->body = NULL;
  c->method_len = 0;
  c->path_len = 0;
  c->headers_len = 0;
  c->body_len = 0;
  return 0;
}

/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  (void)parser;
  FIO_LOG_ERROR("response receieved instead of a request. Silently ignored.");
  return -1;
}
/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser,
                           char *method,
                           size_t method_len) {
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
  c->method = method;
  c->method_len = method_len;
  return 0;
}
/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser,
                           size_t status,
                           char *status_str,
                           size_t len) {
  return -1;
  (void)parser;
  (void)status;
  (void)status_str;
  (void)len;
}
/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t path_len) {
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
  c->path = path;
  c->path_len = path_len;
  return 0;
}
/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser,
                          char *query,
                          size_t query_len) {
  return 0;
  (void)parser;
  (void)query;
  (void)query_len;
}
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_version(http1_parser_s *parser, char *version, size_t len) {
  return 0;
  (void)parser;
  (void)version;
  (void)len;
}
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser,
                           char *name,
                           size_t name_len,
                           char *value,
                           size_t value_len) {
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
  if (c->headers_len >= HTTP_MAX_HEADERS)
    return -1;
  c->headers[c->headers_len].name = name;
  c->headers[c->headers_len].name_len = name_len;
  c->headers[c->headers_len].value = value;
  c->headers[c->headers_len].value_len = value_len;
  ++c->headers_len;
  return 0;
}
/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser,
                               char *data,
                               size_t data_len) {
  if (parser->state.content_length >= HTTP_CLIENT_BUFFER)
    return -1;
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
  if (!c->body)
    c->body = data;
  c->body_len += data_len;
  return 0;
}
/** called when a protocol error occurred. */
static int http1_on_error(http1_parser_s *parser) {
  client_s *c = FIO_PTR_FROM_FIELD(client_s, parser, parser);
  fio_close(c->uuid);
  return -1;
}

/* *****************************************************************************
HTTP/1.1 response authorship helper
***************************************************************************** */

static char http_date_buf[128];
static size_t http_date_len;
static time_t http_date_tm;

static void http_send_response(client_s *c,
                               int status,
                               fio_str_info_s status_str,
                               size_t header_count,
                               fio_str_info_s headers[][2],
                               fio_str_info_s body) {
  struct timespec tm = fio_time_real();
  if (http_date_tm != tm.tv_sec) {
    http_date_len = fio_time2rfc7231(http_date_buf, tm.tv_sec);
    http_date_tm = tm.tv_sec;
  }

  size_t total_len = 9 + 4 + 15 + 20 /* max content length */ + 2 +
                     status_str.len + 2 + http_date_len + 5 + 7 + 2 + body.len;
  for (size_t i = 0; i < header_count; ++i) {
    total_len += headers[i][0].len + 1 + headers[i][1].len + 2;
  }
  if (status < 100 || status > 999)
    status = 500;
  str_s *response = str_new();
  str_reserve(response, total_len);
  str_write(response, "HTTP/1.1 ", 9);
  str_write_i(response, status);
  str_write(response, " ", 1);
  str_write(response, status_str.buf, status_str.len);
  str_write(response, "\r\nContent-Length:", 17);
  str_write_i(response, body.len);
  str_write(response, "\r\nDate: ", 8);
  str_write(response, http_date_buf, http_date_len);
  str_write(response, "\r\n", 2);

  for (size_t i = 0; i < header_count; ++i) {
    str_write(response, headers[i][0].buf, headers[i][0].len);
    str_write(response, ":", 1);
    str_write(response, headers[i][1].buf, headers[i][1].len);
    str_write(response, "\r\n", 2);
  }
  fio_str_info_s final = str_write(response, "\r\n", 2);
  if (body.len && body.buf)
    final = str_write(response, body.buf, body.len);
  fio_write2(c->uuid,
             .buf = response,
             .len = final.len,
             .offset = (((uintptr_t) final.buf - (uintptr_t)response)),
             .dealloc = (void (*)(void *))str_free);
  FIO_LOG_DEBUG2("Sending response %d, %zu bytes", status, final.len);
}
