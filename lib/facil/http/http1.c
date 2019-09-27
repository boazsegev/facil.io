/*
Copyright: Boaz Segev, 2016-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http_internal.h"

#include "http1_parser.h"

/* *****************************************************************************
The HTTP/1.1 Protocol Object
***************************************************************************** */

#ifndef HTTP1_READ_BUFFER
/**
 * The size of a single `read` command, it sets the limit for an HTTP/1.1
 * header line.
 */
#define HTTP1_READ_BUFFER (8 * 1024) /* ~8kb */
#endif

typedef struct http1pr_s {
  http_fio_protocol_s p;
  http1_parser_s parser;
  http_internal_s request;
  uintptr_t buf_len;
  uintptr_t max_header_size;
  uintptr_t header_size;
  uint8_t close;
  uint8_t is_client;
  uint8_t stop;
  uint8_t buf[];
} http1pr_s;

#define parser2http(x) FIO_PTR_FROM_FIELD(http1pr_s, parser, (x))

inline static void h1_reset(http1pr_s *p) { p->header_size = 0; }

#define http1proto2handle(pr) (((http1pr_s *)(pr))->request)
#define handle2pr(h) ((http1pr_s *)(HTTP2PRIVATE(h)->pr))
#define internal2http1(h) ((http1pr_s *)((h)->pr))

/* cleanup an HTTP/1.1 handler object */
static inline void http1_after_finish(http_s *h_) {
  http1pr_s *p = handle2pr(h_);
  http_internal_s *h = HTTP2PRIVATE(h_);
  p->stop = p->stop & (~1UL);
  if (h != &http1proto2handle(p)) {
    http_h_destroy(h, 0);
    fio_free(h);
  } else {
    http_h_clear(h, p->p.settings->log);
  }
  if (p->close)
    fio_close(p->p.uuid);
}

/* *****************************************************************************
VTable FUNCTIONS
***************************************************************************** */

/** Should send existing headers and data */
static int http1_send_body(http_internal_s *h, void *data, uintptr_t length);
/** Should send existing headers and file */
static int http1_sendfile(http_internal_s *h, int fd, uintptr_t, uintptr_t);
/** Should send existing headers and data and prepare for streaming */
static int http1_stream(http_internal_s *h, void *data, uintptr_t length);
/** Should send existing headers or complete streaming */
static void http1_finish(http_internal_s *h);
/** Push for data. */
static int http1_push_data(http_internal_s *h, void *, uintptr_t, FIOBJ);
/** Upgrades a connection to Websockets. */
static int http1_http2websocket(http_internal_s *h, websocket_settings_s *arg);
/** Push for files. */
static int http1_push_file(http_internal_s *h, FIOBJ filename, FIOBJ mime_type);
/** Pauses the request / response handling. */
static void http1_on_pause(http_internal_s *, http_fio_protocol_s *);

/** Resumes a request / response handling. */
static void http1_on_resume(http_internal_s *, http_fio_protocol_s *);
/** hijacks the socket aaway from the protocol. */
static intptr_t http1_hijack(http_internal_s *h, fio_str_info_s *leftover);

/** Upgrades an HTTP connection to an EventSource (SSE) connection. */
static int http1_upgrade2sse(http_internal_s *h, http_sse_s *sse);
/** Writes data to an EventSource (SSE) connection. MUST free the FIOBJ. */
static int http1_sse_write(http_sse_s *sse, FIOBJ str);
/** Closes an EventSource (SSE) connection. */
static int http1_sse_close(http_sse_s *sse);

/* *****************************************************************************
Public API + VTable
***************************************************************************** */

static struct http_vtable_s HTTP1_VTABLE = {
    /** Should send existing headers and data */
    .send_body = http1_send_body,
    /** Should send existing headers and file */
    .sendfile = http1_sendfile,
    /** Should send existing headers and data and prepare for streaming */
    .stream = http1_stream,
    /** Should send existing headers or complete streaming */
    .finish = http1_finish,
    /** Push for data. */
    .push_data = http1_push_data,
    /** Upgrades a connection to Websockets. */
    .http2websocket = http1_http2websocket,
    /** Push for files. */
    .push_file = http1_push_file,
    /** Pauses the request / response handling. */
    .on_pause = http1_on_pause,

    /** Resumes a request / response handling. */
    .on_resume = http1_on_resume,
    /** hijacks the socket aaway from the protocol. */
    .hijack = http1_hijack,

    /** Upgrades an HTTP connection to an EventSource (SSE) connection. */
    .upgrade2sse = http1_upgrade2sse,
    /** Writes data to an EventSource (SSE) connection. MUST free the FIOBJ. */
    .sse_write = http1_sse_write,
    /** Closes an EventSource (SSE) connection. */
    .sse_close = http1_sse_close,
};

/** returns the HTTP/1.1 protocol's VTable. */
void *http1_vtable(void) { return &HTTP1_VTABLE; }

/**
 * Creates an HTTP1 protocol object and handles any unread data in the buffer
 * (if any).
 */
fio_protocol_s *http1_new(uintptr_t uuid, http_settings_s *settings,
                          void *unread_data, size_t unread_length) {
  return NULL;
  (void)uuid;
  (void)settings;
  (void)unread_data;
  (void)unread_length;
}

/** Destroys the HTTP1 protocol object. */
void http1_destroy(fio_protocol_s *);

/* *****************************************************************************









Internal Implementation









***************************************************************************** */

/* *****************************************************************************
Format the HTTP request / response to be sent
***************************************************************************** */

struct header_writer_s {
  FIOBJ dest;
};

void http1_set_connection_headers(http_internal_s *h) {
  FIOBJ tmp = FIOBJ_INVALID;

  /* manage Keep-Alive / Closure headers and state */
  tmp = fiobj_hash_get2(h->headers_out, HTTP_HEADER_CONNECTION);
  if (tmp && FIOBJ_TYPE_IS(tmp, FIOBJ_T_STRING)) {
    fio_str_info_s t = fiobj_str2cstr(tmp);
    if (t.buf[0] == 'c' || t.buf[0] == 'C')
      goto mark_to_close;
    return;
  }
  tmp = fiobj_hash_get2(h->public.headers, HTTP_HEADER_CONNECTION);
  if (tmp && FIOBJ_TYPE_IS(tmp, FIOBJ_T_STRING)) {
    fio_str_info_s t = fiobj_str2cstr(tmp);
    if (t.buf[0] == 'c' || t.buf[0] == 'C')
      goto add_close_header;
  } else if (h->public.version &&
             FIOBJ_TYPE_IS(h->public.version, FIOBJ_T_STRING)) {
    fio_str_info_s t = fiobj_str2cstr(h->public.version);
    if (t.len < 8 || t.buf[7] == '0')
      goto add_close_header;
  }
  set_header_overwite(h->headers_out, HTTP_HEADER_CONNECTION,
                      fiobj_dup(HTTP_HVALUE_KEEP_ALIVE));
  return;
add_close_header:
  set_header_overwite(h->headers_out, HTTP_HEADER_CONNECTION,
                      fiobj_dup(HTTP_HVALUE_CLOSE));
mark_to_close:
  internal2http1(h)->close = 1;
}

static int http___write_header(FIOBJ o, void *w_) {
  struct header_writer_s *w = w_;
  FIOBJ header_name = fiobj_hash_each_get_key();
  if (!o || header_name == FIOBJ_INVALID)
    return 0;

  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, http___write_header, w);
    return 0;
  }
  fio_str_info_s name = fiobj2cstr(header_name);
  fio_str_info_s str = fiobj2cstr(o);
  if (!str.buf)
    return 0;
  fiobj_str_write(w->dest, name.buf, name.len);
  fiobj_str_write(w->dest, ":", 1);
  fiobj_str_write(w->dest, str.buf, str.len);
  fiobj_str_write(w->dest, "\r\n", 2);
  return 0;
}

/**
 * Returns a String object representing the unparsed HTTP request/response
 * headers (HTTP version is capped at HTTP/1.1).
 */
FIOBJ http1_format_headers(http_internal_s *h) {
  // http_internal_s *h = HTTP2PRIVATE(h_);
  // if (!HTTP_S_INVALID(h_))
  //   return FIOBJ_INVALID;

  struct header_writer_s w;
  w.dest = fiobj_str_new();
  http1_set_connection_headers(h);
  if (!internal2http1(h)->is_client) {
    /* server mode - response */
    fio_str_info_s status_str = http_status2str(h->public.status);
    fiobj_str_join(w.dest, h->public.version);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_write_i(w.dest, h->public.status);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_write(w.dest, status_str.buf, status_str.len);
    fiobj_str_write(w.dest, "\r\n", 2);
  } else {
    /* client mode - request */
    fiobj_str_join(w.dest, h->public.method);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, h->public.path);
    if (h->public.query) {
      fiobj_str_write(w.dest, "?", 1);
      fiobj_str_join(w.dest, h->public.query);
    }
    {
      fio_str_info_s t = fiobj2cstr(h->public.version);
      if (t.len < 6 || t.buf[5] != '1')
        fiobj_str_write(w.dest, " HTTP/1.1\r\n", 10);
      else {
        fiobj_str_write(w.dest, " ", 1);
        fiobj_str_write(w.dest, t.buf, t.len);
        fiobj_str_write(w.dest, "\r\n", 2);
      }
    }
  }

  fiobj_each1(h->headers_out, 0, http___write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  return w.dest;
}

/* *****************************************************************************
Send / Stream data
***************************************************************************** */

/** Should send existing headers and data */
static int http1_send_body(http_internal_s *h, void *data, uintptr_t length);
/** Should send existing headers and file */
static int http1_sendfile(http_internal_s *h, int fd, uintptr_t length,
                          uintptr_t offset);
/** Should send existing headers and data and prepare for streaming */
static int http1_stream(http_internal_s *h, void *data, uintptr_t length);
/** Should send existing headers or complete streaming */
static void http1_finish(http_internal_s *h);

/* *****************************************************************************
WebSockets
***************************************************************************** */
/** Upgrades a connection to Websockets. */
static int http1_http2websocket(http_internal_s *h, websocket_settings_s *arg);

/* *****************************************************************************
Push
***************************************************************************** */

/** Push for files. */
static int http1_push_file(http_internal_s *h, FIOBJ filename, FIOBJ mime_type);

/** Push for data. */
static int http1_push_data(http_internal_s *h, void *data, uintptr_t length,
                           FIOBJ mime_type);

/* *****************************************************************************
Pause, Renew and Hijack
***************************************************************************** */

/** Pauses the request / response handling. */
static void http1_on_pause(http_internal_s *, http_fio_protocol_s *);

/** Resumes a request / response handling. */
static void http1_on_resume(http_internal_s *, http_fio_protocol_s *);
/** hijacks the socket aaway from the protocol. */
static intptr_t http1_hijack(http_internal_s *h, fio_str_info_s *leftover);

/* *****************************************************************************
SSE
***************************************************************************** */

/** Upgrades an HTTP connection to an EventSource (SSE) connection. */
static int http1_upgrade2sse(http_internal_s *h, http_sse_s *sse);
/** Writes data to an EventSource (SSE) connection. MUST free the FIOBJ. */
static int http1_sse_write(http_sse_s *sse, FIOBJ str);
/** Closes an EventSource (SSE) connection. */
static int http1_sse_close(http_sse_s *sse);

/* *****************************************************************************
HTTP/1.1 Parser Callbacks
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser);
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser);
/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser, char *method,
                           size_t method_len);
/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser, size_t status,
                           char *status_str, size_t len);
/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t path_len);
/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query,
                          size_t query_len);
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_version(http1_parser_s *parser, char *version, size_t len);
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                           char *data, size_t data_len);
/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser, char *data,
                               size_t data_len);
/** called when a protocol error occurred. */
static int http1_on_error(http1_parser_s *parser);
