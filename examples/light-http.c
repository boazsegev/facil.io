/**
In this example we will author an HTTP server with a smaller memory footprint,
using a similar (yet somewhat simplified) design to the one implemented in the
facil.io 0.5.0 version.

This simplified design can be effective when authoring a proxy / CGI type of
module, however it comes with a few inconviniences, such as a rigid HTTP header
limit and a harder to use data structure, making it a poor choice for a full
application module.

Once we srart adding header recognition and seeking, the balance begins to tip
in favor of a more complex data structure which might perform poorly on simpler
benchmarks (i.e. "Hello World") but improve real-world application performance.

For example, facil.io 0.6.x is slower than nginX for "Hello World" but can be as
fast (and sometimes faster) when serving static files.

Benchmark with keep-alive:

   ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
   wrk -c200 -d4 -t1 http://localhost:3000/

*/

/* include the core library, without any extensions */
#include "facil.h"

#define FIO_OVERRIDE_MALLOC 1
#include "fio_mem.h"

#include "fio_cli.h"
#include "fiobj.h"
#include "http.h" /* for date/time helper */
#include "http1_parser.h"

/* our header buffer size */
#define MAX_HTTP_HEADER_LENGTH 16384
#define MIN_HTTP_READFILE 4096
/* our header count */
#define MAX_HTTP_HEADER_COUNT 64
/* our HTTP POST limits */
#define MAX_HTTP_BODY_MAX 524288

/* *****************************************************************************
The Protocol Data Structure
***************************************************************************** */

typedef struct {
  protocol_s protocol; /* all protocols must use this callback structure */
  intptr_t uuid; /* this will hold the connection's uuid for `sock` functions */
  http1_parser_s parser; /* the HTTP/1.1 parser */
  char *method;          /* the HTTP method, NUL terminated */
  char *path;            /* the URI path, NUL terminated */
  char *query; /* the URI query (after the '?'), if any, NUL terminated */
  char *http_version;    /* the HTTP version, NUL terminated */
  size_t content_length; /* the body's content length, if any */
  size_t header_count; /* the header count - everything after this is garbage */
  char *headers[MAX_HTTP_HEADER_COUNT];
  char *values[MAX_HTTP_HEADER_COUNT];
  FIOBJ body; /* the HTTP body, this is where a little complexity helps */
  size_t buf_reader; /* internal: marks the read position in the buffer */
  size_t buf_writer; /* internal: marks the write position in the buffer */
  uint8_t reset; /* used internally to mark when some buffer can be deleted */
} light_http_s;

/* turns a parser pointer into a `light_http_s` pointer using it's offset */
#define parser2pr(parser)                                                      \
  ((light_http_s *)((uintptr_t)(parser) -                                      \
                    (uintptr_t)(&((light_http_s *)(0))->parser)))

void light_http_send_response(intptr_t uuid, int status, fio_cstr_s status_str,
                              size_t header_count, fio_cstr_s headers[][2],
                              fio_cstr_s body);
/* *****************************************************************************
The HTTP/1.1 Request Handler - change this to whateve you feel like.
***************************************************************************** */

int on_http_request(light_http_s *http) {
  /* handle a request for `http->path` */
  if (1) {
    /* a simple, hardcoded HTTP/1.1 response */
    static char HTTP_RESPONSE[] = "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: 13\r\n"
                                  "Connection: keep-alive\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "\r\n"
                                  "Hello Wolrld!";
    sock_write2(.uuid = http->uuid, .buffer = HTTP_RESPONSE,
                .length = sizeof(HTTP_RESPONSE) - 1,
                .dealloc = SOCK_DEALLOC_NOOP);
  } else {
    /* an allocated, dynamic, HTTP/1.1 response */
    light_http_send_response(
        http->uuid, 200, (fio_cstr_s){.len = 2, .data = "OK"}, 1,
        (fio_cstr_s[][2]){{{.len = 12, .data = "Content-Type"},
                           {.len = 10, .data = "text/plain"}}},
        (fio_cstr_s){.len = 13, .data = "Hello Wolrld!"});
  }
  return 0;
}

/* *****************************************************************************
Listening for Connections (main)
***************************************************************************** */

/* we're referencing this function, but defining it later on. */
void light_http_on_open(intptr_t uuid, void *udata);

/* our main function / starting point */
int main(int argc, char const *argv[]) {
  /* A simple CLI interface. */
  fio_cli_start(argc, argv, "Fast HTTP example for the facil.io framework.");
  fio_cli_accept_str("port p", "Port to bind to.");
  fio_cli_accept_str("workers w", "Number of workers (processes).");
  fio_cli_accept_str("threads t", "Number of threads.");
  /* Default to port 3000. */
  if (!fio_cli_get_str("p"))
    fio_cli_set_str("p", "3000");
  /* Default to single thread. */
  if (!fio_cli_get_int("t"))
    fio_cli_set_int("t", 1);
  /* try to listen on port 3000. */
  if (facil_listen(.port = fio_cli_get_str("p"), .address = NULL,
                   .on_open = light_http_on_open, .udata = NULL))
    perror("FATAL ERROR: Couldn't open listening socket"), exit(errno);
  /* run facil with 1 working thread - this blocks until we're done. */
  facil_run(.threads = fio_cli_get_int("t"), .processes = fio_cli_get_int("w"));
  /* that's it */
  return 0;
}

/* *****************************************************************************
The HTTP/1.1 Parsing Callbacks - we need to implememnt everything for the parser
***************************************************************************** */

/** called when a request was received. */
int light_http1_on_request(http1_parser_s *parser) {
  int ret = on_http_request(parser2pr(parser));
  fiobj_free(parser2pr(parser)->body);
  parser2pr(parser)->body = FIOBJ_INVALID;
  parser2pr(parser)->reset = 1;
  return ret;
}

/** called when a response was received, this is for HTTP clients (error). */
int light_http1_on_response(http1_parser_s *parser) {
  return -1;
  (void)parser;
}

/** called when a request method is parsed. */
int light_http1_on_method(http1_parser_s *parser, char *method,
                          size_t method_len) {
  parser2pr(parser)->method = method;
  return 0;
  (void)method_len;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
int light_http1_on_status(http1_parser_s *parser, size_t status,
                          char *status_str, size_t len) {
  return -1;
  (void)parser;
  (void)status;
  (void)status_str;
  (void)len;
}
/** called when a request path (excluding query) is parsed. */
int light_http1_on_path(http1_parser_s *parser, char *path, size_t path_len) {
  parser2pr(parser)->path = path;
  return 0;
  (void)path_len;
}
/** called when a request path (excluding query) is parsed. */
int light_http1_on_query(http1_parser_s *parser, char *query,
                         size_t query_len) {
  parser2pr(parser)->query = query;
  return 0;
  (void)query_len;
}
/** called when a the HTTP/1.x version is parsed. */
int light_http1_on_http_version(http1_parser_s *parser, char *version,
                                size_t len) {
  parser2pr(parser)->http_version = version;
  return 0;
  (void)len;
}
/** called when a header is parsed. */
int light_http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                          char *data, size_t data_len) {
  if (parser2pr(parser)->header_count >= MAX_HTTP_HEADER_COUNT)
    return -1;
  parser2pr(parser)->headers[parser2pr(parser)->header_count] = name;
  parser2pr(parser)->values[parser2pr(parser)->header_count] = data;
  ++parser2pr(parser)->header_count;
  return 0;
  (void)name_len;
  (void)data_len;
}

/** called when a body chunk is parsed. */
int light_http1_on_body_chunk(http1_parser_s *parser, char *data,
                              size_t data_len) {
  if (parser->state.content_length >= MAX_HTTP_BODY_MAX)
    return -1;
  if (!parser2pr(parser)->body)
    parser2pr(parser)->body = fiobj_data_newtmpfile();
  fiobj_data_write(parser2pr(parser)->body, data, data_len);
  if (fiobj_obj2num(parser2pr(parser)->body) >= MAX_HTTP_BODY_MAX)
    return -1;
  return 0;
}

/** called when a protocol error occured. */
int light_http1_on_error(http1_parser_s *parser) {
  /* close the connection */
  sock_close(parser2pr(parser)->uuid);
  return 0;
}
/* *****************************************************************************
The Protocol Callbacks
***************************************************************************** */

/* facil.io callbacks we want to handle */
void light_http_on_open(intptr_t uuid, void *udata);
void light_http_on_data(intptr_t uuid, protocol_s *pr);
void light_http_on_close(intptr_t uuid, protocol_s *pr);

/* this will be called when a connection is opened. */
void light_http_on_open(intptr_t uuid, void *udata) {
  /*
   * we should allocate a protocol object for this connection.
   *
   * since protocol objects are stateful (the parsing, internal locks, etc'), we
   * need a different protocol object per connection.
   *
   * NOTE: the extra length in the memory will be the R/W buffer.
   */
  light_http_s *p =
      malloc(sizeof(*p) + MAX_HTTP_HEADER_LENGTH + MIN_HTTP_READFILE);
  *p = (light_http_s){
      .protocol.service = "Fast HTTP",        /* optional protocol identifier */
      .protocol.on_data = light_http_on_data, /* setting the data callback */
      .protocol.on_close = light_http_on_close, /* setting the close callback */
      .uuid = uuid,
  };
  /* timeouts are important. timeouts are in seconds. */
  facil_set_timeout(uuid, 5);
  /*
   * this is a very IMPORTANT function call,
   * it attaches the protocol to the socket.
   */
  facil_attach(uuid, &p->protocol);
  /* the `udata` wasn't used, but it's good for dynamic settings and such */
  (void)udata;
}

/* this will be called when the connection has incoming data. */
void light_http_on_data(intptr_t uuid, protocol_s *pr) {
  /* We will read some / all of the data */
  light_http_s *h = (light_http_s *)pr;
  ssize_t tmp =
      sock_read(uuid, (char *)(h + 1) + h->buf_writer,
                (MAX_HTTP_HEADER_LENGTH + MIN_HTTP_READFILE) - h->buf_writer);
  if (tmp <= 0) {
    /* reading failed, we're done. */
    return;
  }
  h->buf_writer += tmp;
  /* feed the parser until it's done consuminng data. */
  do {
    tmp = http1_fio_parser(.parser = &h->parser,
                           .buffer = (char *)(h + 1) + h->buf_reader,
                           .length = h->buf_writer - h->buf_reader,
                           .on_request = light_http1_on_request,
                           .on_response = light_http1_on_response,
                           .on_method = light_http1_on_method,
                           .on_status = light_http1_on_status,
                           .on_path = light_http1_on_path,
                           .on_query = light_http1_on_query,
                           .on_http_version = light_http1_on_http_version,
                           .on_header = light_http1_on_header,
                           .on_body_chunk = light_http1_on_body_chunk,
                           .on_error = light_http1_on_error);
    if (h->body) {
      /* when reading to a body, the data is copied */
      /* keep the reading position at buf_reader. */
      h->buf_writer -= tmp;
      if (h->buf_writer != h->buf_reader) {
        /* some data wasn't processed, move it to the writer's position*/
        memmove((char *)(h + 1) + h->buf_reader,
                (char *)(h + 1) + h->buf_reader + tmp,
                h->buf_writer - h->buf_reader);
      }
    } else {
      /* since we didn't copy the data, we need to move the reader forward */
      h->buf_reader += tmp;
      if (h->reset) {
        h->header_count = 0;
        /* a request just finished, move the reader back to 0... */
        /* and test for HTTP pipelinig. */
        h->buf_writer -= h->buf_reader;
        if (h->buf_writer) {
          memmove((char *)(h + 1), (char *)(h + 1) + h->buf_reader,
                  h->buf_writer);
        }
        h->buf_reader = 0;
      }
    }
  } while ((size_t)tmp);
}

/* this will be called when the connection is closed. */
void light_http_on_close(intptr_t uuid, protocol_s *pr) {
  /* in case we lost connection midway */
  fiobj_free(((light_http_s *)pr)->body);
  ((light_http_s *)pr)->body = FIOBJ_INVALID;
  /* free our protocol data and resources */
  free(pr);
  (void)uuid;
}

/* *****************************************************************************
Fast HTTP response handling
***************************************************************************** */

void light_http_send_response(intptr_t uuid, int status, fio_cstr_s status_str,
                              size_t header_count, fio_cstr_s headers[][2],
                              fio_cstr_s body) {
  static size_t date_len = 0;
  char tmp[40];
  if (!date_len) {
    date_len = http_time2str(tmp, facil_last_tick().tv_sec);
  }

  size_t content_length_len = fio_ltoa(tmp, body.len, 10);
  size_t total_len = 9 + 4 + 15 + content_length_len + 2 + status_str.len + 2 +
                     date_len + 7 + 2 + body.len;
  for (size_t i = 0; i < header_count; ++i) {
    total_len += headers[i][0].len + 1 + headers[i][1].len + 2;
  }
  if (status < 100 || status > 999)
    status = 500;
  char *response = malloc(total_len);
  memcpy(response, "HTTP/1.1 ", 9);
  fio_ltoa(response + 9, status, 10);
  char *pos = response + 9 + 3;
  *pos++ = ' ';
  memcpy(pos, status_str.data, status_str.len);
  pos += status_str.len;
  *pos++ = '\r';
  *pos++ = '\n';
  memcpy(pos, "Date:", 5);
  pos += 5;
  pos += http_time2str(pos, facil_last_tick().tv_sec);
  *pos++ = '\r';
  *pos++ = '\n';
  memcpy(pos, "Content-Length:", 15);
  pos += 15;
  memcpy(pos, tmp, content_length_len);
  pos += content_length_len;
  *pos++ = '\r';
  *pos++ = '\n';

  for (size_t i = 0; i < header_count; ++i) {
    memcpy(pos, headers[i][0].data, headers[i][0].len);
    pos += headers[i][0].len;
    *pos++ = ':';
    memcpy(pos, headers[i][1].data, headers[i][1].len);
    pos += headers[i][1].len;
    *pos++ = '\r';
    *pos++ = '\n';
  }
  *pos++ = '\r';
  *pos++ = '\n';
  if (body.data)
    memcpy(pos, body.data, body.len);
  sock_write2(.uuid = uuid, .buffer = response, .length = total_len,
              .dealloc = free);
}
