/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#include "spnlock.inc"

#include "http1.h"
#include "http1_parser.h"
#include "http_internal.h"

#include <stddef.h>

/* *****************************************************************************
The HTTP/1.1 Protocol Object
***************************************************************************** */

typedef struct http1_s {
  http_protocol_s p;
  http1_parser_s parser;
  http_req_s request;
  uint8_t restart;    /* placed here to force padding */
  uint8_t body_is_fd; /* placed here to force padding */
  uintptr_t buf_pos;
  uintptr_t buf_len;
  uint8_t *buf;
} http1_s;

/* *****************************************************************************
Internal Helpers
***************************************************************************** */

#define parser2http(x)                                                         \
  ((http1_s *)((uintptr_t)(x) - (uintptr_t)(&((http1_s *)0)->parser)))

inline static void set_request(http1_s *p) {
  p->request = (http_req_s){
      .private.ref_count = 1,
      .private.uuid = p->p.uuid,
      .private.request_id = 1,
      .private.response_headers = fiobj_hash_new(),
      .headers = fiobj_hash_new(),
      .version = p->request.version,
      .received_at = facil_last_tick(),
      .status = 200,
  };
}

inline static void clear_request(http_req_s *r) {
  fiobj_free(r->method);
  fiobj_free(r->private.response_headers);
  fiobj_free(r->headers);
  fiobj_free(r->version);
  fiobj_free(r->query);
  fiobj_free(r->path);
  fiobj_free(r->status_str);
  fiobj_free(r->cookies);
  fiobj_free(r->body);
  fiobj_free(r->params);
}

inline static void h1_reset(http1_s *p) {
  p->buf_len = p->buf_len - p->buf_pos;
  if (p->buf_len) {
    memmove((uint8_t *)(p + 1), p->buf + p->buf_pos, p->buf_len);
  }
  p->buf_pos = 0;
  p->buf = (uint8_t *)(p + 1);
  p->body_is_fd = 0;
  p->restart = 0;
}
/* *****************************************************************************
Virtual Functions API
***************************************************************************** */

/* *****************************************************************************
Parser Callbacks
***************************************************************************** */

/** called when a request was received. */
static int on_request(http1_parser_s *parser) {
  http1_s *p = parser2http(parser);
  p->request.private.request_id &= ~((uintptr_t)2);
  p->p.settings->on_request(&p->request);
  clear_request(&p->request);
  p->restart = 1;
  return 0;
}
/** called when a response was received. */
static int on_response(http1_parser_s *parser) {
  return -1;
  (void)parser;
}
/** called when a request method is parsed. */
static int on_method(http1_parser_s *parser, char *method, size_t method_len) {
  set_request(parser2http(parser));
  parser2http(parser)->request.method = fiobj_str_static(method, method_len);
  return 0;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int on_status(http1_parser_s *parser, size_t status, char *status_str,
                     size_t len) {
  set_request(parser2http(parser));
  parser2http(parser)->request.status_str = fiobj_str_static(status_str, len);
  parser2http(parser)->request.status = status;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int on_path(http1_parser_s *parser, char *path, size_t len) {
  parser2http(parser)->request.path = fiobj_str_static(path, len);
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int on_query(http1_parser_s *parser, char *query, size_t len) {
  parser2http(parser)->request.query = fiobj_str_static(query, len);
  return 0;
}
/** called when a the HTTP/1.x version is parsed. */
static int on_http_version(http1_parser_s *parser, char *version, size_t len) {
  parser2http(parser)->request.version = fiobj_str_static(version, len);
  return 0;
}
/** called when a header is parsed. */
static int on_header(http1_parser_s *parser, char *name, size_t name_len,
                     char *data, size_t data_len) {
  fiobj_s *sym;
  fiobj_s *obj;
  if ((uintptr_t)parser2http(parser)->buf ==
      (uintptr_t)(parser2http(parser) + 1)) {
    sym = fiobj_str_static(name, name_len);
    obj = fiobj_str_static(data, data_len);
  } else {
    sym = fiobj_sym_new(name, name_len);
    obj = fiobj_str_new(data, data_len);
    h1_reset(parser2http(parser));
  }
  fiobj_s *old =
      fiobj_hash_replace(parser2http(parser)->request.headers, sym, obj);
  if (!old) {
    fiobj_free(sym);
    return 0;
  }
  if (old->type == FIOBJ_T_ARRAY) {
    fiobj_ary_push(old, obj);
    fiobj_hash_replace(parser2http(parser)->request.headers, sym, old);
  } else {
    fiobj_s *a = fiobj_ary_new();
    fiobj_ary_push(a, old);
    fiobj_ary_push(a, obj);
    fiobj_hash_replace(parser2http(parser)->request.headers, sym, a);
  }
  fiobj_free(sym);
  return 0;
}
/** called when a body chunk is parsed. */
static int on_body_chunk(http1_parser_s *parser, char *data, size_t data_len) {
  if (parser->state.content_length >
          (ssize_t)parser2http(parser)->p.settings->max_body_size ||
      parser->state.read >
          (ssize_t)parser2http(parser)->p.settings->max_body_size)
    return -1; /* test every time, in case of chunked data */
  if (!parser->state.read) {
    if (parser->state.content_length > 0 &&
        parser->state.content_length + parser2http(parser)->buf_pos <=
            HTTP1_MAX_HEADER_SIZE) {
      parser2http(parser)->request.body =
          fiobj_io_newstr2(data, data_len, NULL);
    } else {
      parser2http(parser)->body_is_fd = 1;
      parser2http(parser)->request.body = fiobj_io_newtmpfile();
      fiobj_io_write(parser2http(parser)->request.body, data, data_len);
    }
    return 0;
  }
  if (parser2http(parser)->body_is_fd)
    fiobj_io_write(parser2http(parser)->request.body, data, data_len);
  return 0;
}

/** called when a protocol error occured. */
static int on_error(http1_parser_s *parser) {
  sock_close(parser2http(parser)->p.uuid);
  return -1;
}

/* *****************************************************************************
Connection Callbacks
***************************************************************************** */

/**
 * A string to identify the protocol's service (i.e. "http").
 *
 * The string should be a global constant, only a pointer comparison will be
 * used (not `strcmp`).
 */
static const char *HTTP1_SERVICE_STR = "http1_protocol_facil_io";

static __thread uint8_t h1_static_buffer[HTTP1_MAX_HEADER_SIZE];

/** called when a data is available, but will not run concurrently */
static void on_data(intptr_t uuid, protocol_s *protocol) {
  http1_s *p = (http1_s *)protocol;
  ssize_t i;

  if (p->body_is_fd) {
    p->buf = h1_static_buffer;
    p->buf_pos = 0;
  }
  i = sock_read(uuid, p->buf + p->buf_pos, HTTP1_MAX_HEADER_SIZE - p->buf_pos);

  if (i > 0)
    p->buf_len += i;
  if (p->buf_len - p->buf_pos)
    p->buf_pos +=
        http1_fio_parser(.parser = &p->parser, .buffer = p->buf + p->buf_pos,
                         .length = (p->buf_len - p->buf_pos),
                         .on_request = on_request, .on_response = on_response,
                         .on_method = on_method, .on_status = on_status,
                         .on_path = on_path, .on_query = on_query,
                         .on_http_version = on_http_version,
                         .on_header = on_header, .on_body_chunk = on_body_chunk,
                         .on_error = on_error);
  else
    return;
  if (p->restart) {
    h1_reset(p);
  } else if (i <= 0)
    return;
  facil_force_event(uuid, FIO_EVENT_ON_DATA);
  return;
}
/** called when the connection was closed, but will not run concurrently */
static void on_close(intptr_t uuid, protocol_s *protocol) {
  http1_destroy(protocol);
  (void)uuid;
}

/* *****************************************************************************
Public API
***************************************************************************** */

/** Creates an HTTP1 protocol object and handles any unread data in the buffer
 * (if any). */
protocol_s *http1_new(uintptr_t uuid, http_settings_s *settings,
                      void *unread_data, size_t unread_length) {
  http1_s *p = malloc(sizeof(*p) + HTTP1_MAX_HEADER_SIZE);
  *p = (http1_s){
      .p.protocol =
          {
              .service = HTTP1_SERVICE_STR,
              .on_data = on_data,
              .on_close = on_close,
          },
      .p.uuid = uuid,
      .p.settings = settings,
      .p.vtable = NULL,
      .buf = (uint8_t *)(p + 1),
  };
  if (unread_data && unread_length <= HTTP1_MAX_HEADER_SIZE) {
    memcpy(p->buf, unread_data, unread_length);
    p->buf_len = unread_length;
    facil_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  return &p->p.protocol;
}

/** Manually destroys the HTTP1 protocol object. */
void http1_destroy(protocol_s *p) { free(p); }
