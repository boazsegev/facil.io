/**
In this example we will author a fast HTTP server, using a slightly simpler
design than the facil.io 0.5.x design.

The speed (and design) comes at a cost - the HTTP request is harder to parse and
harder to handle as we get into the application stage, which means that it could
get slower for a full application vs. when authoring a proxy.

Benchmark with keep-alive:

   ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
   wrk -c200 -d4 -t12 http://localhost:3000/

As mentioned, the high speeds have their disadvantages.

For example, HTTP versions and Connection directives are ignored, so clients
that don't support "keep-alive" will have to wait for the connection to timeout:

   ab -c 200 -t 4 -n 1000000 http://127.0.0.1:3000/

Once we srart adding header recognnition and seeking, the balance begins to tip
in favor of more complex data structures, that will inhibit "hello world"
performance but improve real-world application performance.

For example, facil.io 0.6.x is slower than nginX for "Hello World" but can be as
fast (and sometimes faster) when serving static files.
*/

/* include the core library, without any extensions */
#include "facil.h"

#include "fiobj.h"
#include "http1_parser.h"

/* a simple HTTP/1.1 response */
static char HTTP_RESPONSE[] = "HTTP/1.1 200 OK\r\n"
                              "Content-Length: 12\r\n"
                              "Connection: keep-alive\r\n"
                              "Content-Type: text/plain\r\n"
                              "\r\n"
                              "Hello World!";

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
  fiobj_s *body; /* the HTTP body, this is where a little complexity helps */
  size_t buf_reader; /* internal: marks the read position in the buffer */
  size_t buf_writer; /* internal: marks the write position in the buffer */
  uint8_t reset; /* used internally to mark when some buffer can be deleted */
} fast_http_s;

/* turns a parser pointer into a `fast_http_s` pointer using it's offset */
#define parser2pr(parser)                                                      \
  ((fast_http_s *)((uintptr_t)(parser) -                                       \
                   (uintptr_t)(&((fast_http_s *)(0))->parser)))

/* *****************************************************************************
The HTTP/1.1 Request Handler - change this to whateve you feel like.
***************************************************************************** */

int on_http_request(fast_http_s *http) {
  /* handle a request for `http->path` */
  sock_write2(.uuid = http->uuid, .buffer = HTTP_RESPONSE,
              .length = sizeof(HTTP_RESPONSE) - 1,
              .dealloc = SOCK_DEALLOC_NOOP);
  return 0;
}

/* *****************************************************************************
Listening for Connections (main)
***************************************************************************** */

/* we're referencing this function, but defining it later on. */
void fast_http_on_open(intptr_t uuid, void *udata);

/* our main function / starting point */
int main(void) {
  /* try to listen on port 3000. */
  if (facil_listen(.port = "3000", .address = NULL,
                   .on_open = fast_http_on_open, .udata = NULL))
    perror("FATAL ERROR: Couldn't open listening socket"), exit(errno);
  /* run facil with 1 working thread - this blocks until we're done. */
  facil_run(.threads = 1);
  /* that's it */
  return 0;
}

/* *****************************************************************************
The HTTP/1.1 Parsing Callbacks - we need to implememnt everything for the parser
***************************************************************************** */

/** called when a request was received. */
int fast_http1_on_request(http1_parser_s *parser) {
  int ret = on_http_request(parser2pr(parser));
  fiobj_free(parser2pr(parser)->body);
  parser2pr(parser)->body = NULL;
  parser2pr(parser)->reset = 1;
  return ret;
}

/** called when a response was received, this is for HTTP clients (error). */
int fast_http1_on_response(http1_parser_s *parser) {
  return -1;
  (void)parser;
}

/** called when a request method is parsed. */
int fast_http1_on_method(http1_parser_s *parser, char *method,
                         size_t method_len) {
  parser2pr(parser)->method = method;
  return 0;
  (void)method_len;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
int fast_http1_on_status(http1_parser_s *parser, size_t status,
                         char *status_str, size_t len) {
  return -1;
  (void)parser;
  (void)status;
  (void)status_str;
  (void)len;
}
/** called when a request path (excluding query) is parsed. */
int fast_http1_on_path(http1_parser_s *parser, char *path, size_t path_len) {
  parser2pr(parser)->path = path;
  return 0;
  (void)path_len;
}
/** called when a request path (excluding query) is parsed. */
int fast_http1_on_query(http1_parser_s *parser, char *query, size_t query_len) {
  parser2pr(parser)->query = query;
  return 0;
  (void)query_len;
}
/** called when a the HTTP/1.x version is parsed. */
int fast_http1_on_http_version(http1_parser_s *parser, char *version,
                               size_t len) {
  parser2pr(parser)->http_version = version;
  return 0;
  (void)len;
}
/** called when a header is parsed. */
int fast_http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
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
int fast_http1_on_body_chunk(http1_parser_s *parser, char *data,
                             size_t data_len) {
  if (parser->state.content_length >= MAX_HTTP_BODY_MAX)
    return -1;
  if (!parser2pr(parser)->body)
    parser2pr(parser)->body = fiobj_io_newtmpfile();
  fiobj_io_write(parser2pr(parser)->body, data, data_len);
  if (fiobj_obj2num(parser2pr(parser)->body) >= MAX_HTTP_BODY_MAX)
    return -1;
  return 0;
}

/** called when a protocol error occured. */
int fast_http1_on_error(http1_parser_s *parser) {
  /* close the connection */
  sock_close(parser2pr(parser)->uuid);
  return 0;
}
/* *****************************************************************************
The Protocol Callbacks
***************************************************************************** */

/* facil.io callbacks we want to handle */
void fast_http_on_open(intptr_t uuid, void *udata);
void fast_http_on_data(intptr_t uuid, protocol_s *pr);
void fast_http_on_close(intptr_t uuid, protocol_s *pr);

/* this will be called when a connection is opened. */
void fast_http_on_open(intptr_t uuid, void *udata) {
  /*
   * we should allocate a protocol object for this connection.
   *
   * since protocol objects are stateful (the parsing, internal locks, etc'), we
   * need a different protocol object per connection.
   *
   * NOTE: the extra length in the memory will be the R/W buffer.
   */
  fast_http_s *p =
      malloc(sizeof(*p) + MAX_HTTP_HEADER_LENGTH + MIN_HTTP_READFILE);
  *p = (fast_http_s){
      .protocol.service = "Fast HTTP",       /* optional protocol identifier */
      .protocol.on_data = fast_http_on_data, /* setting the data callback */
      .protocol.on_close = fast_http_on_close, /* setting the close callback */
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
void fast_http_on_data(intptr_t uuid, protocol_s *pr) {
  /* We will read some / all of the data */
  fast_http_s *h = (fast_http_s *)pr;
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
                           .on_request = fast_http1_on_request,
                           .on_response = fast_http1_on_response,
                           .on_method = fast_http1_on_method,
                           .on_status = fast_http1_on_status,
                           .on_path = fast_http1_on_path,
                           .on_query = fast_http1_on_query,
                           .on_http_version = fast_http1_on_http_version,
                           .on_header = fast_http1_on_header,
                           .on_body_chunk = fast_http1_on_body_chunk,
                           .on_error = fast_http1_on_error);
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
void fast_http_on_close(intptr_t uuid, protocol_s *pr) {
  /* in case we lost connection midway */
  fiobj_free(((fast_http_s *)pr)->body);
  ((fast_http_s *)pr)->body = NULL;
  /* free our protocol data and resources */
  free(pr);
  (void)uuid;
}
