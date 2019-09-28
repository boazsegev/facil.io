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
#define HTTP1_READ_BUFFER HTTP_MAX_HEADER_LENGTH /* ~8kb */
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
  uint8_t log;
  uint8_t stop;
  uint8_t buf[];
} http1pr_s;

#define parser2http(x) FIO_PTR_FROM_FIELD(http1pr_s, parser, (x))

/** initializes the protocol object */
inline static void http1_init_protocol(http1pr_s *, uintptr_t,
                                       http_settings_s *);
inline static void h1_reset(http1pr_s *p) { p->header_size = 0; }

#define http1proto2handle(pr) (((http1pr_s *)(pr))->request)
#define internal2http1(h) ((http1pr_s *)((h)->pr))

/* cleanup an HTTP/1.1 handler object */
static void http1_after_finish(http_internal_s *h) {
  http1pr_s *p = internal2http1(h);
  p->stop = p->stop & (~1UL);
  if (h != &http1proto2handle(p)) {
    http_h_destroy(h, 0);
    fio_free(h);
  } else {
    http_h_clear(h, h->headers_out && p->log);
  }
  if (p->close)
    fio_close(p->p.uuid);
}

/* *****************************************************************************
Protocol Callbacks
***************************************************************************** */

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
  if (unread_data && unread_length > HTTP1_READ_BUFFER)
    return NULL;
  http1pr_s *p = fio_malloc(sizeof(*p) + HTTP1_READ_BUFFER);
  // FIO_LOG_DEBUG("Allocated HTTP/1.1 protocol at. %p for %p", (void *)p,
  //               (void *)uuid);
  FIO_ASSERT_ALLOC(p);
  http1_init_protocol(p, uuid, settings);
  if (unread_data) {
    memcpy(p->buf, unread_data, unread_length);
    p->buf_len = unread_length;
  }
  fio_attach(uuid, &p->p.protocol);
  if (unread_data) {
    fio_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  return &p->p.protocol;
}

/** Destroys the HTTP1 protocol object. */
void http1_destroy(fio_protocol_s *pr) {
  http1pr_s *p = (http1pr_s *)pr;
  http1proto2handle(p).public.status = 0;
  http_h_destroy(&http1proto2handle(p), 0);
  fio_free(p);
  // FIO_LOG_DEBUG("Deallocated HTTP/1.1 protocol at. %p", (void *)p);
}

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
  if (!FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH)) {
    return FIOBJ_INVALID;
  }

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
static int http1_send_body(http_internal_s *h, void *data, uintptr_t length) {
  FIOBJ headers = http1_format_headers(h);
  if (headers &&
      (fiobj_str_len(headers) + 1024) <= FIO_MEMORY_BLOCK_ALLOC_LIMIT) {
    const size_t max2unite =
        FIO_MEMORY_BLOCK_ALLOC_LIMIT - fiobj_str_len(headers);
    if (length <= max2unite) {
      fiobj_str_write(headers, data, length);
      length = 0;
    } else {
      fiobj_str_write(headers, data, max2unite);
      length -= max2unite;
      data = (void *)((uintptr_t)data + max2unite);
    }
  }
  fiobj_send_free(internal2http1(h)->p.uuid, headers);
  if (length) {
    fio_write(internal2http1(h)->p.uuid, data, length);
  }
  http1_after_finish(h);
  return 0;
}
/** Should send existing headers and file */
static int http1_sendfile(http_internal_s *h, int fd, uintptr_t offset,
                          uintptr_t length) {
  FIOBJ headers = http1_format_headers(h);
  if (!headers)
    goto headers_error;
  fiobj_send_free(internal2http1(h)->p.uuid, headers);
  if (length)
    fio_sendfile(internal2http1(h)->p.uuid, fd, offset, length);
  http1_after_finish(h);
  return 0;
headers_error:
  if (fd >= 0)
    close(fd);
  return -1;
}
/** Should send existing headers and data and prepare for streaming */
static int http1_stream(http_internal_s *h, void *data, uintptr_t length) {
  if (!h->headers_out)
    return -1;
  uint8_t should_chunk = 1;
  FIOBJ out = FIOBJ_INVALID;

  if (FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH)) {
    if (fiobj_hash_get2(h->headers_out, HTTP_HEADER_CONTENT_LENGTH) ||
        ((out = fiobj_hash_get2(h->headers_out, HTTP_HEADER_CONTENT_TYPE)) &&
         fiobj_is_eq(out, HTTP_HVALUE_SSE_MIME))) {
      /* Streaming without chuncked encoding */
      should_chunk = 0;
    } else {
      /* add chunking related headers... */
      FIOBJ tmp =
          fiobj_hash_get2(h->headers_out, HTTP_HEADER_TRANSFER_ENCODING);
      if (!tmp || !fiobj_is_eq(tmp, HTTP_HVALUE_CHUNKED_ENCODING))
        set_header_add(h->headers_out, HTTP_HEADER_TRANSFER_ENCODING,
                       fiobj_dup(HTTP_HVALUE_CHUNKED_ENCODING));
    }
  } else if (h->headers_out == fiobj_false()) {
    should_chunk = 0;
  }
  out = http1_format_headers(h);
  if (out) {
    fiobj_free(h->headers_out);
    h->headers_out = should_chunk ? fiobj_true() : fiobj_false();
    if (!length)
      goto finish;
  } else {
    if (!length)
      return -1;
    out = fiobj_str_new_buf(length + 8);
  }
  if (should_chunk) {
    /* add chuncked encoding header */
    char buffer[32];
    size_t hex_len = 0;
    if (length) {
      /* write length in Hex, no leading zeros or "0x" */
      char *dest = buffer;
      uint64_t n = length;
      uint8_t i = 0;
      while (i < 16 && (n & 0xF000000000000000) == 0) {
        n = n << 4;
        i++;
      }
      /* write the damn thing, high to low */
      while (i < 16) {
        uint8_t tmp = (n & 0xF000000000000000) >> 60;
        dest[hex_len++] = ((tmp <= 9) ? ('0' + tmp) : (('A' - 10) + tmp));
        i++;
        n = n << 4;
      }
    } else
      goto finish;
    fiobj_str_reserve(out, fiobj_str_len(out) + length + hex_len + 2);
    fiobj_str_write(out, buffer, hex_len);
    fiobj_str_write(out, "\r\n", 2);
    fiobj_str_write(out, data, length);
    fiobj_str_write(out, "\r\n", 2);
  } else {
    fiobj_str_write(out, data, length);
  }
finish:
  fiobj_send_free(internal2http1(h)->p.uuid, out);
  return 0;
}
/** Should send existing headers or complete streaming */
static void http1_finish(http_internal_s *h) {
  FIOBJ headers = http1_format_headers(h);
  if (headers)
    fiobj_send_free(internal2http1(h)->p.uuid, headers);
  else if (h->headers_out == fiobj_true()) /* finish chuncked data */
    fio_write2(internal2http1(h)->p.uuid, .data.buf = "0\r\n\r\n", .len = 5,
               .after.dealloc = FIO_DEALLOC_NOOP);
  http1_after_finish(h);
}

/* *****************************************************************************
WebSockets
***************************************************************************** */
/** Upgrades a connection to Websockets. */
static int http1_http2websocket(http_internal_s *h, websocket_settings_s *arg) {
  return -1;
  (void)h;
  (void)arg;
}

/* *****************************************************************************
Push
***************************************************************************** */

/** Push for files. */
static int http1_push_file(http_internal_s *h, FIOBJ filename,
                           FIOBJ mime_type) {
  return -1;
  (void)h;
  fiobj_free(filename);
  fiobj_free(mime_type);
}

/** Push for data. */
static int http1_push_data(http_internal_s *h, void *data, uintptr_t length,
                           FIOBJ mime_type) {
  return -1;
  (void)h;
  (void)data;
  (void)length;
  fiobj_free(mime_type);
}

/* *****************************************************************************
Pause, Renew and Hijack
***************************************************************************** */

/** Pauses the request / response handling. */
static void http1_on_pause(http_internal_s *h, http_fio_protocol_s *pr) {
  ((http1pr_s *)pr)->stop = 1;
  fio_suspend(pr->uuid);
  (void)h;
}

/** Resumes a request / response handling. */
static void http1_on_resume(http_internal_s *h, http_fio_protocol_s *pr) {
  if (!((http1pr_s *)pr)->stop) {
    fio_force_event(pr->uuid, FIO_EVENT_ON_DATA);
  }
  (void)h;
}

/** hijacks the socket aaway from the protocol. */
static intptr_t http1_hijack(http_internal_s *h, fio_str_info_s *leftover) {
  if (leftover) {
    intptr_t len = internal2http1(h)->buf_len -
                   (intptr_t)(internal2http1(h)->parser.state.next -
                              internal2http1(h)->buf);
    if (len) {
      *leftover = (fio_str_info_s){
          .len = len, .buf = (char *)internal2http1(h)->parser.state.next};
    } else {
      *leftover = (fio_str_info_s){.len = 0, .buf = NULL};
    }
  }

  internal2http1(h)->stop = 3;
  intptr_t uuid = internal2http1(h)->p.uuid;
  fio_attach(uuid, NULL);
  return uuid;
}

/* *****************************************************************************
SSE
***************************************************************************** */

/** Upgrades an HTTP connection to an EventSource (SSE) connection. */
static int http1_upgrade2sse(http_internal_s *h, http_sse_s *sse) {
  if (!FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH))
    return -1;
  set_header_overwite(h->headers_out, HTTP_HEADER_CONTENT_ENCODING,
                      fiobj_dup(HTTP_HVALUE_SSE_MIME));
  if (http1_stream(h, NULL, 0) == -1)
    goto upgrade_failed;

  (void)sse; /* FIXME */

  if (sse->on_open)
    sse->on_open(sse);
  return 0;
upgrade_failed:
  if (sse->on_close)
    sse->on_close(sse);
  return -1;
}
/** Writes data to an EventSource (SSE) connection. MUST free the FIOBJ. */
static int http1_sse_write(http_sse_s *sse, FIOBJ str) {
  fiobj_free(str);
  (void)sse; /* FIXME */
  return -1;
}
/** Closes an EventSource (SSE) connection. */
static int http1_sse_close(http_sse_s *sse) {
  (void)sse; /* FIXME */
  return -1;
}

/* *****************************************************************************
HTTP/1.1 Protocol Callbacks
***************************************************************************** */

static inline void http1_consume_data(intptr_t uuid, http1pr_s *p) {
  if (fio_pending(uuid) > 4) {
    goto throttle;
  }
  ssize_t i = 0;
  size_t org_len = p->buf_len;
  int pipeline_limit = 8;
  if (!p->buf_len)
    return;
  do {
    i = http1_parse(&p->parser, p->buf + (org_len - p->buf_len), p->buf_len);
    p->buf_len -= i;
    --pipeline_limit;
  } while (i && p->buf_len && pipeline_limit && !p->stop);

  if (p->buf_len && org_len != p->buf_len) {
    memmove(p->buf, p->buf + (org_len - p->buf_len), p->buf_len);
  }

  if (p->buf_len == HTTP1_READ_BUFFER) {
    /* no room to read... parser not consuming data */
    if (p->request.public.method)
      http_send_error(&p->request.public, 413);
    else {
      FIOBJ_STR_TEMP_VAR(tmp_method);
      fiobj_str_write(tmp_method, "GET", 3);
      p->request.public.method = tmp_method;
      http_send_error(&p->request.public, 413);
    }
  }

  if (!pipeline_limit) {
    fio_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  return;

throttle:
  /* throttle busy clients (slowloris) */
  p->stop |= 4;
  fio_suspend(uuid);
  FIO_LOG_DEBUG("(HTTP/1,1) throttling client at %.*s",
                (int)fio_peer_addr(uuid).len, fio_peer_addr(uuid).buf);
}

/** Called when a data is available, but will not run concurrently */
static void http1_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  http1pr_s *p = (http1pr_s *)protocol;
  if (p->stop) {
    fio_suspend(uuid);
    return;
  }
  ssize_t i = 0;
  if (HTTP1_READ_BUFFER - p->buf_len)
    i = fio_read(uuid, p->buf + p->buf_len, HTTP1_READ_BUFFER - p->buf_len);
  if (i > 0) {
    p->buf_len += i;
  }
  http1_consume_data(uuid, p);
}

/** called when a data is available for the first time */
static void http1_on_data_first_time(intptr_t uuid, fio_protocol_s *protocol) {
  http1pr_s *p = (http1pr_s *)protocol;
  ssize_t i;

  i = fio_read(uuid, p->buf + p->buf_len, HTTP1_READ_BUFFER - p->buf_len);

  if (i <= 0)
    return;
  p->buf_len += i;

  /* ensure future reads skip this first time HTTP/2.0 test */
  p->p.protocol.on_data = http1_on_data;
  if (i >= 24 && !memcmp(p->buf, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24)) {
    FIO_LOG_WARNING("client claimed unsupported HTTP/2 prior knowledge.");
    fio_close(uuid);
    return;
  }

  /* Finish handling the same way as the normal `on_data` */
  http1_consume_data(uuid, p);
}

/** called once all pending `fio_write` calls are finished. */
static void http1_on_ready(intptr_t uuid, fio_protocol_s *protocol) {
  /* resume slow clients from suspension */
  http1pr_s *p = (http1pr_s *)protocol;
  if (p->stop & 4) {
    p->stop ^= 4; /* flip back the bit, so it's zero */
    fio_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  (void)protocol;
}

/** Called when the connection was closed, but will not run concurrently */
static void http1_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  http1_destroy(protocol);
  (void)uuid;
}

/** initializes the protocol object */
inline static void http1_init_protocol(http1pr_s *pr, uintptr_t uuid,
                                       http_settings_s *s) {
  *pr = (http1pr_s){
      .p =
          {
              .protocol =
                  {
                      .on_data = http1_on_data_first_time,
                      .on_ready = http1_on_ready,
                      .on_close = http1_on_close,
                  },
              .uuid = uuid,
              .settings = s,
          },
      .parser = HTTP1_PARSER_INIT,
      .request = HTTP_H_INIT(&HTTP1_VTABLE, &pr->p),
      .max_header_size = s->max_header_size,
      .is_client = s->is_client,
      .log = s->log,
  };
}

/* *****************************************************************************
HTTP/1.1 Parser Callbacks
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  http1pr_s *p = parser2http(parser);
  http_on_request_handler______internal(&http1proto2handle(p).public,
                                        p->p.settings);
  if (p->request.public.method && !p->stop)
    http_finish(&p->request.public);
  h1_reset(p);
  return fio_is_closed(p->p.uuid);
}

/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  http1pr_s *p = parser2http(parser);
  http_on_response_handler______internal(&http1proto2handle(p).public,
                                         p->p.settings);
  if (p->request.public.status_str && !p->stop)
    http_finish(&p->request.public);
  h1_reset(p);
  return fio_is_closed(p->p.uuid);
}

/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser, char *method, size_t len) {
  http1proto2handle(parser2http(parser)).public.method =
      fiobj_str_new_cstr(method, len);
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser, size_t status,
                           char *status_str, size_t len) {
  http1proto2handle(parser2http(parser)).public.status_str =
      fiobj_str_new_cstr(status_str, len);
  http1proto2handle(parser2http(parser)).public.status = status;
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t len) {
  http1proto2handle(parser2http(parser)).public.path =
      fiobj_str_new_cstr(path, len);
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query, size_t len) {
  http1proto2handle(parser2http(parser)).public.query =
      fiobj_str_new_cstr(query, len);
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a the HTTP/1.x version is parsed. */
static int http1_on_version(http1_parser_s *parser, char *version, size_t len) {
/* start counting - occurs on the first line of both requests and responses */
#if FIO_HTTP_EXACT_LOGGING
  clock_gettime(CLOCK_REALTIME,
                &http1proto2handle(parser2http(parser)).public.received_at);
#else
  http1proto2handle(parser2http(parser)).public.received_at = fio_last_tick();
#endif
  /* initialize headers hash as well as version string */
  http1proto2handle(parser2http(parser)).public.version =
      fiobj_str_new_cstr(version, len);
  parser2http(parser)->header_size += len;
  fiobj_free(http1proto2handle(parser2http(parser)).public.headers);
  http1proto2handle(parser2http(parser)).public.headers = fiobj_hash_new();
  return 0;
}

/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                           char *data, size_t data_len) {
  FIOBJ sym = FIOBJ_INVALID;
  FIOBJ obj = FIOBJ_INVALID;
  if (!FIOBJ_TYPE_IS(http1proto2handle(parser2http(parser)).public.headers,
                     FIOBJ_T_HASH)) {
    FIO_LOG_ERROR("(http1 parse ordering error) missing HashMap for header "
                  "%s: %s",
                  name, data);
    http_send_error2(500, parser2http(parser)->p.uuid,
                     parser2http(parser)->p.settings);
    return -1;
  }
  parser2http(parser)->header_size += name_len + data_len;
  if (parser2http(parser)->header_size >=
          parser2http(parser)->max_header_size ||
      fiobj_hash_count(http1proto2handle(parser2http(parser)).public.headers) >
          HTTP_MAX_HEADER_COUNT) {
    if (parser2http(parser)->log) {
      FIO_LOG_WARNING("(HTTP) security alert - header flood detected.");
    }
    http_send_error(&http1proto2handle(parser2http(parser)).public, 413);
    return -1;
  }
  sym = fiobj_str_new_cstr(name, name_len);
  obj = fiobj_str_new_cstr(data, data_len);
  set_header_add(http1proto2handle(parser2http(parser)).public.headers, sym,
                 obj);
  fiobj_free(sym);
  return 0;
}

/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser, char *data,
                               size_t data_len) {
  if (!parser->state.read) {
    if (parser->state.content_length > 0) {
      if (parser->state.content_length >
          (ssize_t)parser2http(parser)->p.settings->max_body_size)
        goto too_big;
      http1proto2handle(parser2http(parser)).public.body =
          fiobj_io_new2(parser->state.content_length);
    } else {
      http1proto2handle(parser2http(parser)).public.body = fiobj_io_new();
    }
  } else if (parser->state.read >
             (ssize_t)parser2http(parser)->p.settings->max_body_size)
    goto too_big; /* tested in case combined chuncked data is too long */
  fiobj_io_write(http1proto2handle(parser2http(parser)).public.body, data,
                 data_len);
  return 0;

too_big:
  http_send_error(&http1proto2handle(parser2http(parser)).public, 413);
  return -1;
}

/** called when a protocol error occurred. */
static int http1_on_error(http1_parser_s *parser) {
  if (parser2http(parser)->close)
    return -1;
  FIO_LOG_DEBUG("HTTP parser error.");
  fio_close(parser2http(parser)->p.uuid);
  return -1;
}
