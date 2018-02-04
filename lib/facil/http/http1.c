/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#include "spnlock.inc"

#include "http1.h"
#include "http1_parser.h"
#include "http_internal.h"
#include "websockets.h"

#include "fio_ary.h"
#include "fio_base64.h"
#include "fio_sha1.h"
#include "fiobj.h"

#include <assert.h>
#include <stddef.h>

/* *****************************************************************************
The HTTP/1.1 Protocol Object
***************************************************************************** */

typedef struct http1pr_s {
  http_protocol_s p;
  http1_parser_s parser;
  http_s request;
  uintptr_t buf_len;
  uintptr_t max_header_size;
  uintptr_t header_size;
  uint8_t close;
  uint8_t is_client;
  uint8_t stop;
  uint8_t buf[];
} http1pr_s;

/* *****************************************************************************
Internal Helpers
***************************************************************************** */

#define parser2http(x)                                                         \
  ((http1pr_s *)((uintptr_t)(x) - (uintptr_t)(&((http1pr_s *)0)->parser)))

inline static void h1_reset(http1pr_s *p) { p->header_size = 0; }

#define http1_pr2handle(pr) (((http1pr_s *)(pr))->request)
#define handle2pr(h) ((http1pr_s *)h->private_data.flag)

static fio_cstr_s http1pr_status2str(uintptr_t status);

/* cleanup an HTTP/1.1 handler object */
static inline void http1_after_finish(http_s *h) {
  http1pr_s *p = handle2pr(h);
  p->stop = p->stop & (~1UL);
  http_s_clear(h, p->p.settings->log);
  if (h != &p->request) {
    http_s_destroy(h, 0);
    free(h);
  }
  if (p->close)
    sock_close(p->p.uuid);
}

/* *****************************************************************************
HTTP Request / Response (Virtual) Functions
***************************************************************************** */
struct header_writer_s {
  FIOBJ dest;
  FIOBJ name;
  FIOBJ value;
};

static int write_header(FIOBJ o, void *w_) {
  struct header_writer_s *w = w_;
  if (!o)
    return 0;
  if (fiobj_hash_key_in_loop()) {
    w->name = fiobj_hash_key_in_loop();
  }
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, write_header, w);
    return 0;
  }
  fio_cstr_s name = fiobj_obj2cstr(w->name);
  fio_cstr_s str = fiobj_obj2cstr(o);
  if (!str.data)
    return 0;
  fiobj_str_write(w->dest, name.data, name.len);
  fiobj_str_write(w->dest, ":", 1);
  fiobj_str_write(w->dest, str.data, str.len);
  fiobj_str_write(w->dest, "\r\n", 2);
  return 0;
}

static FIOBJ headers2str(http_s *h) {
  if (!h->method && !!h->status_str)
    return FIOBJ_INVALID;

  static uintptr_t connection_hash;
  if (!connection_hash)
    connection_hash = fio_siphash("connection", 10);

  struct header_writer_s w;
  w.dest = fiobj_str_buf(0);
  http1pr_s *p = handle2pr(h);

  if (p->is_client == 0) {
    fio_cstr_s t = http1pr_status2str(h->status);
    fiobj_str_write(w.dest, t.data, t.length);
    FIOBJ tmp = fiobj_hash_get2(h->private_data.out_headers, connection_hash);
    if (tmp) {
      fio_cstr_s t = fiobj_obj2cstr(tmp);
      if (t.data[0] == 'c' || t.data[0] == 'C')
        p->close = 1;
    } else {
      tmp = fiobj_hash_get2(h->headers, connection_hash);
      if (tmp) {
        fio_cstr_s t = fiobj_obj2cstr(tmp);
        if (!t.data || !t.len || t.data[0] == 'k' || t.data[0] == 'K')
          fiobj_str_write(w.dest, "connection:keep-alive\r\n", 23);
        else {
          fiobj_str_write(w.dest, "connection:close\r\n", 18);
          p->close = 1;
        }
      } else {
        fio_cstr_s t = fiobj_obj2cstr(h->version);
        if (t.len > 7 && t.data && t.data[5] == '1' && t.data[6] == '.' &&
            t.data[7] == '1')
          fiobj_str_write(w.dest, "connection:keep-alive\r\n", 23);
        else {
          fiobj_str_write(w.dest, "connection:close\r\n", 18);
          p->close = 1;
        }
      }
    }
  } else {
    if (h->method) {
      fiobj_str_join(w.dest, h->method);
      fiobj_str_write(w.dest, " ", 1);
    } else {
      fiobj_str_write(w.dest, "GET ", 4);
    }
    fiobj_str_join(w.dest, h->path);
    if (h->query) {
      fiobj_str_write(w.dest, "?", 1);
      fiobj_str_join(w.dest, h->query);
    }
    fiobj_str_write(w.dest, " HTTP/1.1\r\n", 11);
    /* make sure we have a host header? */
    static uint64_t host_hash;
    if (!host_hash)
      host_hash = fio_siphash("host", 4);
    FIOBJ tmp;
    if (!fiobj_hash_get2(h->private_data.out_headers, host_hash) &&
        (tmp = fiobj_hash_get2(h->headers, host_hash))) {
      fiobj_str_write(w.dest, "host:", 5);
      fiobj_str_join(w.dest, tmp);
      fiobj_str_write(w.dest, "\r\n", 2);
    }
    if (!fiobj_hash_get2(h->private_data.out_headers, connection_hash))
      fiobj_str_write(w.dest, "connection:keep-alive\r\n", 23);
  }

  fiobj_each1(h->private_data.out_headers, 0, write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  return w.dest;
}

/** Should send existing headers and data */
static int http1_send_body(http_s *h, void *data, uintptr_t length) {

  FIOBJ packet = headers2str(h);
  if (!packet) {
    http1_after_finish(h);
    return -1;
  }
  fiobj_str_write(packet, data, length);
  fiobj_send_free((handle2pr(h)->p.uuid), packet);
  http1_after_finish(h);
  return 0;
}
/** Should send existing headers and file */
static int http1_sendfile(http_s *h, int fd, uintptr_t length,
                          uintptr_t offset) {
  FIOBJ packet = headers2str(h);
  if (!packet) {
    close(fd);
    http1_after_finish(h);
    return -1;
  }
  if (length < HTTP1_READ_BUFFER) {
    /* optimize away small files */
    fio_cstr_s s = fiobj_obj2cstr(packet);
    fiobj_str_capa_assert(packet, s.len + length);
    s = fiobj_obj2cstr(packet);
    intptr_t i = pread(fd, s.data + s.len, length, offset);
    if (i < 0) {
      close(fd);
      fiobj_send_free((handle2pr(h)->p.uuid), packet);
      sock_close((handle2pr(h)->p.uuid));
      return -1;
    }
    close(fd);
    fiobj_str_resize(packet, s.len + i);
    fiobj_send_free((handle2pr(h)->p.uuid), packet);
    http1_after_finish(h);
    return 0;
  }
  fiobj_send_free((handle2pr(h)->p.uuid), packet);
  sock_sendfile((handle2pr(h)->p.uuid), fd, offset, length);
  http1_after_finish(h);
  return 0;
}

/** Should send existing headers or complete streaming */
static void htt1p_finish(http_s *h) {
  FIOBJ packet = headers2str(h);
  if (packet)
    fiobj_send_free((handle2pr(h)->p.uuid), packet);
  else {
    // fprintf(stderr, "WARNING: invalid call to `htt1p_finish`\n");
  }
  http1_after_finish(h);
}
/** Push for data - unsupported. */
static int http1_push_data(http_s *h, void *data, uintptr_t length,
                           FIOBJ mime_type) {
  return -1;
  (void)h;
  (void)data;
  (void)length;
  (void)mime_type;
}
/** Push for files - unsupported. */
static int http1_push_file(http_s *h, FIOBJ filename, FIOBJ mime_type) {
  return -1;
  (void)h;
  (void)filename;
  (void)mime_type;
}

/**
 * Called befor a pause task,
 */
void http1_on_pause(http_s *h, http_protocol_s *pr) {
  ((http1pr_s *)pr)->stop = 1;
  facil_quite(pr->uuid);
  (void)h;
}

/**
 * called after the resume task had completed.
 */
void http1_on_resume(http_s *h, http_protocol_s *pr) {
  if (!((http1pr_s *)pr)->stop) {
    facil_force_event(pr->uuid, FIO_EVENT_ON_DATA);
  }
  (void)h;
}

intptr_t http1_hijack(http_s *h, fio_cstr_s *leftover) {
  if (leftover) {
    intptr_t len =
        handle2pr(h)->buf_len -
        (intptr_t)(handle2pr(h)->parser.state.next - handle2pr(h)->buf);
    if (len) {
      *leftover =
          (fio_cstr_s){.len = len, .bytes = handle2pr(h)->parser.state.next};
    } else {
      *leftover = (fio_cstr_s){.len = 0, .data = NULL};
    }
  }

  handle2pr(h)->stop = 3;
  intptr_t uuid = handle2pr(h)->p.uuid;
  facil_attach(uuid, NULL);
  return uuid;
}

/* *****************************************************************************
Websockets Upgrading
***************************************************************************** */

static void http1_websocket_client_on_upgrade(http_s *h, char *proto,
                                              size_t len) {
  http1pr_s *p = handle2pr(h);
  websocket_settings_s *args = h->udata;
  args->http = h;
  const intptr_t uuid = handle2pr(args->http)->p.uuid;
  http_settings_s *set = handle2pr(args->http)->p.settings;
  set->udata = NULL;
  http_finish(args->http);
  p->stop = 1;
  websocket_attach(uuid, set, args, p->parser.state.next,
                   p->buf_len - (intptr_t)(p->parser.state.next - p->buf));
  free(args);
  (void)proto;
  (void)len;
}
static void http1_websocket_client_on_failed(http_s *h) {
  websocket_settings_s *s = h->udata;
  if (s->on_close)
    s->on_close(0, s->udata);
  free(h->udata);
  h->udata = http_settings(h)->udata = NULL;
}
static void http1_websocket_client_on_hangup(http_settings_s *settings) {
  websocket_settings_s *s = settings->udata;
  if (s) {
    if (s->on_close)
      s->on_close(0, settings->udata);
    free(settings->udata);
    settings->udata = NULL;
  }
}

static int http1_http2websocket_server(websocket_settings_s *args) {
  // A static data used for all websocket connections.
  static char ws_key_accpt_str[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  static uintptr_t sec_version = 0;
  static uintptr_t sec_key = 0;
  if (!sec_version)
    sec_version = fio_siphash("sec-websocket-version", 21);
  if (!sec_key)
    sec_key = fio_siphash("sec-websocket-key", 17);

  FIOBJ tmp = fiobj_hash_get2(args->http->headers, sec_version);
  if (!tmp)
    goto bad_request;
  fio_cstr_s stmp = fiobj_obj2cstr(tmp);
  if (stmp.length != 2 || stmp.data[0] != '1' || stmp.data[1] != '3')
    goto bad_request;

  tmp = fiobj_hash_get2(args->http->headers, sec_key);
  if (!tmp)
    goto bad_request;
  stmp = fiobj_obj2cstr(tmp);

  sha1_s sha1 = fio_sha1_init();
  fio_sha1_write(&sha1, stmp.data, stmp.len);
  fio_sha1_write(&sha1, ws_key_accpt_str, sizeof(ws_key_accpt_str) - 1);
  tmp = fiobj_str_buf(32);
  stmp = fiobj_obj2cstr(tmp);
  fiobj_str_resize(tmp,
                   fio_base64_encode(stmp.data, fio_sha1_result(&sha1), 20));
  http_set_header(args->http, HTTP_HEADER_CONNECTION,
                  fiobj_dup(HTTP_HVALUE_WS_UPGRADE));
  http_set_header(args->http, HTTP_HEADER_UPGRADE,
                  fiobj_dup(HTTP_HVALUE_WEBSOCKET));
  http_set_header(args->http, HTTP_HEADER_WS_SEC_KEY, tmp);
  args->http->status = 101;
  http1pr_s *pr = handle2pr(args->http);
  const intptr_t uuid = handle2pr(args->http)->p.uuid;
  http_settings_s *set = handle2pr(args->http)->p.settings;
  http_finish(args->http);
  pr->stop = 1;
  websocket_attach(uuid, set, args, pr->parser.state.next,
                   pr->buf_len - (intptr_t)(pr->parser.state.next - pr->buf));
  return 0;
bad_request:
  http_send_error(args->http, 400);
  if (args->on_close)
    args->on_close(0, args->udata);
  return -1;
}

static int http1_http2websocket_client(websocket_settings_s *args) {
  http1pr_s *p = handle2pr(args->http);
  /* We're done with the HTTP stage, so we call the `on_finish` */
  if (p->p.settings->on_finish)
    p->p.settings->on_finish(p->p.settings);
  /* Copy the Websocket setting arguments to the HTTP settings `udata` */
  p->p.settings->udata = malloc(sizeof(*args));
  ((websocket_settings_s *)(p->p.settings->udata))[0] = *args;
  /* Set callbacks */
  p->p.settings->on_finish = http1_websocket_client_on_hangup;   /* unknown */
  p->p.settings->on_upgrade = http1_websocket_client_on_upgrade; /* sucess */
  p->p.settings->on_response = http1_websocket_client_on_failed; /* failed */
  p->p.settings->on_request = http1_websocket_client_on_failed;  /* failed */
  /* Set headers */
  http_set_header(args->http, HTTP_HEADER_CONNECTION,
                  fiobj_dup(HTTP_HVALUE_WS_UPGRADE));
  http_set_header(args->http, HTTP_HEADER_UPGRADE,
                  fiobj_dup(HTTP_HVALUE_WEBSOCKET));
  http_set_header(args->http, HTTP_HVALUE_WS_SEC_VERSION,
                  fiobj_dup(HTTP_HVALUE_WS_VERSION));

  /* create nonce */
  uint64_t key[2]; /* 16 bytes */
  key[0] = (uintptr_t)args->http ^ (uint64_t)facil_last_tick().tv_sec;
  key[1] = (uintptr_t)args->udata ^ (uint64_t)facil_last_tick().tv_nsec;
  FIOBJ encoded = fiobj_str_buf(26); /* we need 24 really. */
  fio_cstr_s tmp = fiobj_obj2cstr(encoded);
  tmp.len = fio_base64_encode(tmp.data, (char *)key, 16);
  fiobj_str_resize(encoded, tmp.len);
  http_set_header(args->http, HTTP_HEADER_WS_SEC_CLIENT_KEY, encoded);
  http_finish(args->http);
  return 0;
  /* *we don't set the Origin header since we're not a browser... should we? */
}

static int http1_http2websocket(websocket_settings_s *args) {
  assert(args->http);
  http1pr_s *p = handle2pr(args->http);

  if (p->is_client == 0) {
    return http1_http2websocket_server(args);
  }
  return http1_http2websocket_client(args);
}

/* *****************************************************************************
Virtual Table Decleration
***************************************************************************** */

struct http_vtable_s HTTP1_VTABLE = {
    .http_send_body = http1_send_body,
    .http_sendfile = http1_sendfile,
    .http_finish = htt1p_finish,
    .http_push_data = http1_push_data,
    .http_push_file = http1_push_file,
    .http_on_pause = http1_on_pause,
    .http_on_resume = http1_on_resume,
    .http_hijack = http1_hijack,
    .http2websocket = http1_http2websocket,
};

void *http1_vtable(void) { return (void *)&HTTP1_VTABLE; }

/* *****************************************************************************
Parser Callbacks
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  http1pr_s *p = parser2http(parser);
  http_on_request_handler______internal(&http1_pr2handle(p), p->p.settings);
  if (p->request.method && !p->stop)
    http_finish(&p->request);
  h1_reset(p);
  return 0;
}
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  http1pr_s *p = parser2http(parser);
  http_on_response_handler______internal(&http1_pr2handle(p), p->p.settings);
  if (p->request.status_str && !p->stop)
    http_finish(&p->request);
  h1_reset(p);
  return 0;
}
/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser, char *method,
                           size_t method_len) {
  http1_pr2handle(parser2http(parser)).method =
      fiobj_str_new(method, method_len);
  parser2http(parser)->header_size += method_len;
  return 0;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser, size_t status,
                           char *status_str, size_t len) {
  http1_pr2handle(parser2http(parser)).status_str =
      fiobj_str_new(status_str, len);
  http1_pr2handle(parser2http(parser)).status = status;
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t len) {
  http1_pr2handle(parser2http(parser)).path = fiobj_str_new(path, len);
  parser2http(parser)->header_size += len;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query, size_t len) {
  http1_pr2handle(parser2http(parser)).query = fiobj_str_new(query, len);
  parser2http(parser)->header_size += len;
  return 0;
}
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_http_version(http1_parser_s *parser, char *version,
                                 size_t len) {
  http1_pr2handle(parser2http(parser)).version = fiobj_str_new(version, len);
  parser2http(parser)->header_size += len;
  return 0;
}
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                           char *data, size_t data_len) {
  FIOBJ sym;
  FIOBJ obj;
  if (!http1_pr2handle(parser2http(parser)).headers) {
    fprintf(stderr,
            "ERROR: (http1 parse ordering error) missing HashMap for header "
            "%s: %s\n",
            name, data);
    {
      http_send_error2(500, parser2http(parser)->p.uuid,
                       parser2http(parser)->p.settings);
      return -1;
    }
  }
  parser2http(parser)->header_size += name_len + data_len;
  if (parser2http(parser)->header_size >=
      parser2http(parser)->max_header_size) {
    http_send_error(&http1_pr2handle(parser2http(parser)), 413);
    return -1;
  }
  sym = fiobj_str_new(name, name_len);
  obj = fiobj_str_new(data, data_len);
  set_header_add(http1_pr2handle(parser2http(parser)).headers, sym, obj);
  fiobj_free(sym);
  return 0;
}
/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser, char *data,
                               size_t data_len) {
  if (parser->state.content_length >
          (ssize_t)parser2http(parser)->p.settings->max_body_size ||
      parser->state.read >
          (ssize_t)parser2http(parser)->p.settings->max_body_size) {
    http_send_error(&http1_pr2handle(parser2http(parser)), 413);
    return -1; /* test every time, in case of chunked data */
  }
  if (!parser->state.read) {
    if (parser->state.content_length > 0 &&
        parser->state.content_length <= HTTP1_READ_BUFFER) {
      http1_pr2handle(parser2http(parser)).body = fiobj_data_newstr();
    } else {
      http1_pr2handle(parser2http(parser)).body = fiobj_data_newtmpfile();
    }
  }
  fiobj_data_write(http1_pr2handle(parser2http(parser)).body, data, data_len);
  return 0;
}

/** called when a protocol error occured. */
static int http1_on_error(http1_parser_s *parser) {
  sock_close(parser2http(parser)->p.uuid);
  return -1;
}

/* *****************************************************************************
Connection Callbacks
*****************************************************************************
*/

/**
 * A string to identify the protocol's service (i.e. "http").
 *
 * The string should be a global constant, only a pointer comparison will be
 * used (not `strcmp`).
 */
static const char *HTTP1_SERVICE_STR = "http1_protocol_facil_io";

static inline void http1_consume_data(intptr_t uuid, http1pr_s *p) {
  ssize_t i = 0;
  size_t org_len = p->buf_len;
  int pipeline_limit = 8;
  do {
    i = http1_fio_parser(.parser = &p->parser,
                         .buffer = p->buf + (org_len - p->buf_len),
                         .length = p->buf_len, .on_request = http1_on_request,
                         .on_response = http1_on_response,
                         .on_method = http1_on_method,
                         .on_status = http1_on_status, .on_path = http1_on_path,
                         .on_query = http1_on_query,
                         .on_http_version = http1_on_http_version,
                         .on_header = http1_on_header,
                         .on_body_chunk = http1_on_body_chunk,
                         .on_error = http1_on_error);
    p->buf_len -= i;
    --pipeline_limit;
  } while (i && p->buf_len && pipeline_limit && !p->stop);

  if (p->buf_len && org_len != p->buf_len) {
    memmove(p->buf, p->buf + (org_len - p->buf_len), p->buf_len);
  }

  if (p->buf_len == HTTP1_READ_BUFFER) {
    /* no room to read... parser not consuming data */
    if (p->request.method)
      http_send_error(&p->request, 413);
    else {
      p->request.method = fiobj_str_tmp();
      http_send_error(&p->request, 413);
    }
  }

  if (!pipeline_limit) {
    facil_force_event(uuid, FIO_EVENT_ON_DATA);
  }
}

/** called when a data is available, but will not run concurrently */
static void http1_on_data(intptr_t uuid, protocol_s *protocol) {
  http1pr_s *p = (http1pr_s *)protocol;
  if (p->stop) {
    facil_quite(uuid);
    return;
  }
  ssize_t i = 0;
  if (HTTP1_READ_BUFFER - p->buf_len)
    i = sock_read(uuid, p->buf + p->buf_len, HTTP1_READ_BUFFER - p->buf_len);
  if (i > 0) {
    p->buf_len += i;
  }
  http1_consume_data(uuid, p);
}

/** called when the connection was closed, but will not run concurrently */
static void http1_on_close(intptr_t uuid, protocol_s *protocol) {
  http1_destroy(protocol);
  (void)uuid;
}

/** called when a data is available for the first time */
static void http1_on_data_first_time(intptr_t uuid, protocol_s *protocol) {
  http1pr_s *p = (http1pr_s *)protocol;
  ssize_t i;

  i = sock_read(uuid, p->buf + p->buf_len, HTTP1_READ_BUFFER - p->buf_len);

  if (i <= 0)
    return;
  p->buf_len += i;

  /* ensure future reads skip this first time HTTP/2.0 test */
  p->p.protocol.on_data = http1_on_data;
  if (i >= 24 && !memcmp(p->buf, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24)) {
    fprintf(stderr,
            "ERROR: unsupported HTTP/2 attempeted using prior knowledge.\n");
    sock_close(uuid);
    return;
  }

  /* Finish handling the same way as the normal `on_data` */
  http1_consume_data(uuid, p);
}

/* *****************************************************************************
Public API
***************************************************************************** */

/** Creates an HTTP1 protocol object and handles any unread data in the buffer
 * (if any). */
protocol_s *http1_new(uintptr_t uuid, http_settings_s *settings,
                      void *unread_data, size_t unread_length) {
  if (unread_data && unread_length > HTTP1_READ_BUFFER)
    return NULL;
  http1pr_s *p = malloc(sizeof(*p) + HTTP1_READ_BUFFER);
  HTTP_ASSERT(p, "HTTP/1.1 protocol allocation failed");
  *p = (http1pr_s){
      .p.protocol =
          {
              .service = HTTP1_SERVICE_STR,
              .on_data = http1_on_data_first_time,
              .on_close = http1_on_close,
          },
      .p.uuid = uuid,
      .p.settings = settings,
      .max_header_size = settings->max_header_size,
      .is_client = settings->is_client,
  };
  http_s_new(&p->request, &p->p, &HTTP1_VTABLE);
  facil_attach(uuid, &p->p.protocol);
  if (unread_data && unread_length <= HTTP1_READ_BUFFER) {
    memcpy(p->buf, unread_data, unread_length);
    p->buf_len = unread_length;
    facil_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  return &p->p.protocol;
}

/** Manually destroys the HTTP1 protocol object. */
void http1_destroy(protocol_s *pr) {
  http1pr_s *p = (http1pr_s *)pr;
  http1_pr2handle(p).status = 0;
  http_s_destroy(&http1_pr2handle(p), 0);
  free(p);
}

/* *****************************************************************************
Protocol Data
***************************************************************************** */

// clang-format off
#define HTTP_SET_STATUS_STR(status, str) [status-100] = { .buffer = ("HTTP/1.1 " #status " " str "\r\n"), .length = (sizeof("HTTP/1.1 " #status " " str "\r\n") - 1) }
// #undef HTTP_SET_STATUS_STR
// clang-format on

static fio_cstr_s http1pr_status2str(uintptr_t status) {
  static fio_cstr_s status2str[] = {
      HTTP_SET_STATUS_STR(100, "Continue"),
      HTTP_SET_STATUS_STR(101, "Switching Protocols"),
      HTTP_SET_STATUS_STR(102, "Processing"),
      HTTP_SET_STATUS_STR(200, "OK"),
      HTTP_SET_STATUS_STR(201, "Created"),
      HTTP_SET_STATUS_STR(202, "Accepted"),
      HTTP_SET_STATUS_STR(203, "Non-Authoritative Information"),
      HTTP_SET_STATUS_STR(204, "No Content"),
      HTTP_SET_STATUS_STR(205, "Reset Content"),
      HTTP_SET_STATUS_STR(206, "Partial Content"),
      HTTP_SET_STATUS_STR(207, "Multi-Status"),
      HTTP_SET_STATUS_STR(208, "Already Reported"),
      HTTP_SET_STATUS_STR(226, "IM Used"),
      HTTP_SET_STATUS_STR(300, "Multiple Choices"),
      HTTP_SET_STATUS_STR(301, "Moved Permanently"),
      HTTP_SET_STATUS_STR(302, "Found"),
      HTTP_SET_STATUS_STR(303, "See Other"),
      HTTP_SET_STATUS_STR(304, "Not Modified"),
      HTTP_SET_STATUS_STR(305, "Use Proxy"),
      HTTP_SET_STATUS_STR(306, "(Unused), "),
      HTTP_SET_STATUS_STR(307, "Temporary Redirect"),
      HTTP_SET_STATUS_STR(308, "Permanent Redirect"),
      HTTP_SET_STATUS_STR(400, "Bad Request"),
      HTTP_SET_STATUS_STR(403, "Forbidden"),
      HTTP_SET_STATUS_STR(404, "Not Found"),
      HTTP_SET_STATUS_STR(401, "Unauthorized"),
      HTTP_SET_STATUS_STR(402, "Payment Required"),
      HTTP_SET_STATUS_STR(405, "Method Not Allowed"),
      HTTP_SET_STATUS_STR(406, "Not Acceptable"),
      HTTP_SET_STATUS_STR(407, "Proxy Authentication Required"),
      HTTP_SET_STATUS_STR(408, "Request Timeout"),
      HTTP_SET_STATUS_STR(409, "Conflict"),
      HTTP_SET_STATUS_STR(410, "Gone"),
      HTTP_SET_STATUS_STR(411, "Length Required"),
      HTTP_SET_STATUS_STR(412, "Precondition Failed"),
      HTTP_SET_STATUS_STR(413, "Payload Too Large"),
      HTTP_SET_STATUS_STR(414, "URI Too Long"),
      HTTP_SET_STATUS_STR(415, "Unsupported Media Type"),
      HTTP_SET_STATUS_STR(416, "Range Not Satisfiable"),
      HTTP_SET_STATUS_STR(417, "Expectation Failed"),
      HTTP_SET_STATUS_STR(421, "Misdirected Request"),
      HTTP_SET_STATUS_STR(422, "Unprocessable Entity"),
      HTTP_SET_STATUS_STR(423, "Locked"),
      HTTP_SET_STATUS_STR(424, "Failed Dependency"),
      HTTP_SET_STATUS_STR(425, "Unassigned"),
      HTTP_SET_STATUS_STR(426, "Upgrade Required"),
      HTTP_SET_STATUS_STR(427, "Unassigned"),
      HTTP_SET_STATUS_STR(428, "Precondition Required"),
      HTTP_SET_STATUS_STR(429, "Too Many Requests"),
      HTTP_SET_STATUS_STR(430, "Unassigned"),
      HTTP_SET_STATUS_STR(431, "Request Header Fields Too Large"),
      HTTP_SET_STATUS_STR(500, "Internal Server Error"),
      HTTP_SET_STATUS_STR(501, "Not Implemented"),
      HTTP_SET_STATUS_STR(502, "Bad Gateway"),
      HTTP_SET_STATUS_STR(503, "Service Unavailable"),
      HTTP_SET_STATUS_STR(504, "Gateway Timeout"),
      HTTP_SET_STATUS_STR(505, "HTTP Version Not Supported"),
      HTTP_SET_STATUS_STR(506, "Variant Also Negotiates"),
      HTTP_SET_STATUS_STR(507, "Insufficient Storage"),
      HTTP_SET_STATUS_STR(508, "Loop Detected"),
      HTTP_SET_STATUS_STR(509, "Unassigned"),
      HTTP_SET_STATUS_STR(510, "Not Extended"),
      HTTP_SET_STATUS_STR(511, "Network Authentication Required"),
  };
  fio_cstr_s ret = (fio_cstr_s){.length = 0, .buffer = NULL};
  if (status >= 100 && status < sizeof(status2str) / sizeof(status2str[0]))
    ret = status2str[status - 100];
  if (!ret.buffer)
    ret = status2str[400];
  return ret;
}
#undef HTTP_SET_STATUS_STR
