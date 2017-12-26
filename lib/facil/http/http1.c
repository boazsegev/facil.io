/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#include "spnlock.inc"

#include "http1.h"
#include "http1_parser.h"
#include "http_internal.h"

#include "fio_ary.h"
#include "fiobj.h"

#include <assert.h>
#include <stddef.h>

/* *****************************************************************************
The HTTP/1.1 Protocol Object
***************************************************************************** */

typedef struct http1_s {
  http_protocol_s p;
  http1_parser_s parser;
  http_s request;
  uint8_t restart;    /* placed here to force padding */
  uint8_t close;      /* placed here to force padding */
  uint8_t body_is_fd; /* placed here to force padding */
  uintptr_t buf_pos;
  uintptr_t buf_len;
  uint8_t *buf;
  intptr_t id_next;
  intptr_t id_counter;
  fio_ary_s queue;
} http1_s;

/* *****************************************************************************
Internal Helpers
***************************************************************************** */

#define parser2http(x)                                                         \
  ((http1_s *)((uintptr_t)(x) - (uintptr_t)(&((http1_s *)0)->parser)))

inline static void h1_reset(http1_s *p) {
  p->buf_len = p->buf_len - p->buf_pos;
  if (p->buf_len) {
    memmove((uint8_t *)(p + 1), p->buf + p->buf_pos, p->buf_len);
  }
  p->buf = (uint8_t *)(p + 1);
  p->buf_pos = 0;
  p->body_is_fd = 0;
  p->restart = 0;
}

static fio_cstr_s http1_status2str(uintptr_t status);

/* *****************************************************************************
Virtual Functions API
***************************************************************************** */
struct header_writer_s {
  fiobj_s *dest;
  fiobj_s *name;
  fiobj_s *value;
};

static int write_header(fiobj_s *o, void *w_) {
  struct header_writer_s *w = w_;
  if (!o)
    return 0;
  if (o->type == FIOBJ_T_COUPLET) {
    w->name = fiobj_couplet2key(o);
    o = fiobj_couplet2obj(o);
    if (!o)
      return 0;
  }
  if (o->type == FIOBJ_T_ARRAY) {
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

static fiobj_s *headers2str(http_s *h) {
  if (!h->headers)
    return NULL;

  static uintptr_t connection_key;
  if (!connection_key)
    connection_key = fiobj_sym_hash("connection", 10);

  struct header_writer_s w;
  w.dest = fiobj_str_buf(4096);

  fio_cstr_s t = http1_status2str(h->status);
  fiobj_str_write(w.dest, t.data, t.length);
  fiobj_s *tmp = fiobj_hash_get3(h->private_data.out_headers, connection_key);
  if (tmp) {
    t = fiobj_obj2cstr(tmp);
    if (t.data[0] == 'c' || t.data[0] == 'C')
      ((http1_s *)h->private_data.owner)->close = 1;
  } else {
    tmp = fiobj_hash_get3(h->headers, connection_key);
    if (tmp) {
      t = fiobj_obj2cstr(tmp);
      if (!t.data || !t.len || t.data[0] == 'k' || t.data[0] == 'K')
        fiobj_str_write(w.dest, "connection:keep-alive\r\n", 23);
      else {
        fiobj_str_write(w.dest, "connection:close\r\n", 18);
        ((http1_s *)h->private_data.owner)->close = 1;
      }
    } else {
      t = fiobj_obj2cstr(h->version);
      if (t.data && t.data[5] == '1' && t.data[6] == '.')
        fiobj_str_write(w.dest, "connection:keep-alive\r\n", 23);
      else {
        fiobj_str_write(w.dest, "connection:close\r\n", 18);
        ((http1_s *)h->private_data.owner)->close = 1;
      }
    }
  }

  fiobj_each1(h->private_data.out_headers, 0, write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  return w.dest;
}

/** Should send existing headers and data */
static int http1_send_body(http_s *h, void *data, uintptr_t length) {

  fiobj_s *packet = headers2str(h);
  if (!packet)
    return -1;
  fiobj_str_write(packet, data, length);
  fiobj_send((((http_protocol_s *)h->private_data.owner)->uuid), packet);
  if (((http1_s *)h->private_data.owner)->close)
    sock_close(((http1_s *)h->private_data.owner)->p.uuid);
  /* streaming? */
  if (h != &((http1_s *)h->private_data.owner)->request)
    free(h);
  return 0;
}
/** Should send existing headers and file */
static int http1_sendfile(http_s *h, int fd, uintptr_t length,
                          uintptr_t offset) {
  fiobj_s *packet = headers2str(h);
  if (!packet) {
    return -1;
  }
  fiobj_send((((http_protocol_s *)h->private_data.owner)->uuid), packet);
  sock_sendfile((((http_protocol_s *)h->private_data.owner)->uuid), fd, offset,
                length);
  if (((http1_s *)h->private_data.owner)->close)
    sock_close(((http1_s *)h->private_data.owner)->p.uuid);
  /* streaming? */
  if (h != &((http1_s *)h->private_data.owner)->request)
    free(h);
  return 0;
}
/** Should send existing headers and data and prepare for streaming */
static int http1_stream(http_s *h, void *data, uintptr_t length) {
  return -1; /*  TODO: tmp unsupported */
  (void)h;
  (void)data;
  (void)length;
}
/** Should send existing headers or complete streaming */
static void htt1p_finish(http_s *h) {
  fiobj_s *packet = headers2str(h);
  if (packet)
    fiobj_send((((http_protocol_s *)h->private_data.owner)->uuid), packet);
  http1_s *p = (http1_s *)h->private_data.owner;
  if (p->close)
    sock_close(((http1_s *)h->private_data.owner)->p.uuid);
  http_s_cleanup(h);
  /* streaming? */
  if (h != &p->request)
    free(h);
}
/** Push for data - unsupported. */
static int http1_push_data(http_s *h, void *data, uintptr_t length,
                           fiobj_s *mime_type) {
  return -1;
  (void)h;
  (void)data;
  (void)length;
  (void)mime_type;
}
/** Push for files - unsupported. */
static int http1_push_file(http_s *h, fiobj_s *filename, fiobj_s *mime_type) {
  return -1;
  (void)h;
  (void)filename;
  (void)mime_type;
}
typedef struct http_func_s {
  void (*task)(http_s *);
  void (*fallback)(http_s *);
} http_func_s;

/** used by defer. */
static void http1_defer_task(intptr_t uuid, protocol_s *p_, void *arg) {
  http_s *h = arg;
  http_func_s *func = ((http_func_s *)(h + 1));
  func->task(h);
  (void)p_;
  (void)uuid;
}

/** used by defer. */
static void http1_defer_fallback(intptr_t uuid, void *arg) {
  http_s *h = arg;
  http_func_s *func = ((http_func_s *)(h + 1));
  h->private_data.owner = NULL;
  if (func->fallback)
    func->fallback(h);
  http_s_cleanup(h);
  free(h);
  (void)uuid;
}

/** Defer request handling for later... careful (memory concern apply). */
static int http1_defer(http_s *h, void (*task)(http_s *h),
                       void (*fallback)(http_s *h)) {
  assert(task && h);
  // if (h == &((http1_s *)h->private_data.owner)->request) {
  //   http_s *tmp = malloc(sizeof(*tmp) + (sizeof(void *) << 1));
  //   HTTP_ASSERT(tmp, "couldn't allocate memory");
  //   *tmp = (http_s){.cookies = h->cookies,
  //                   .version = fiobj_str_copy(h->version),
  //                   .path = fiobj_str_copy(h->path),
  //                   .query = fiobj_str_copy(h->query),
  //                   .private_data = h->private_data,
  //                   .body = fiobj_dup(h->private_data.out_headers),
  //                   .params = fiobj_dup(h->params),
  //                   .status = h->status,
  //                   .method = fiobj_str_copy(h->method),
  //                   .received_at = h->received_at,
  //                   .udata = h->udata};
  //   fiobj_dup(h->private_data.out_headers);
  //   fiobj_io_assert_dynamic(h->body);
  //   http_s_cleanup(h);
  //   h = tmp;
  //   http_func_s *tasks = (http_func_s *)(h + 1);
  //   tasks->task = task;
  //   tasks->fallback = fallback;
  //   facil_defer(.uuid = ((http1_s *)h->private_data.owner)->p.uuid,
  //               .task_type = FIO_PR_LOCK_TASK, .task = http1_defer_task,
  //               .arg = h, .fallback = http1_defer_fallback);
  // }
  return -1; /*  TODO: tmp unsupported */
  (void)h;
  (void)task;
  (void)fallback;
}

struct http_vtable_s HTTP1_VTABLE = {
    .http_send_body = http1_send_body,
    .http_sendfile = http1_sendfile,
    .http_stream = http1_stream,
    .http_finish = htt1p_finish,
    .http_push_data = http1_push_data,
    .http_push_file = http1_push_file,
    .http_defer = http1_defer,
};

/* *****************************************************************************
Parser Callbacks
***************************************************************************** */

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  http1_s *p = parser2http(parser);
  p->request.private_data.request_id = p->id_counter;
  p->id_counter += 1;
  http_on_request_handler______internal(&p->request, p->p.settings);
  http_s_cleanup(&p->request);
  p->restart = 1;
  return 0;
}
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  http1_s *p = parser2http(parser);
  p->request.private_data.request_id = p->id_counter;
  p->id_counter += 1;
  p->p.settings->on_request(&p->request);
  http_s_cleanup(&p->request);
  p->restart = 1;
  return 0;
}
/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser, char *method,
                           size_t method_len) {
  http_s_init(&parser2http(parser)->request, &parser2http(parser)->p);
  parser2http(parser)->request.method = fiobj_str_static(method, method_len);
  return 0;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser, size_t status,
                           char *status_str, size_t len) {
  parser2http(parser)->request.status_str = fiobj_str_static(status_str, len);
  parser2http(parser)->request.status = status;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t len) {
  parser2http(parser)->request.path = fiobj_str_static(path, len);
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query, size_t len) {
  parser2http(parser)->request.query = fiobj_str_static(query, len);
  return 0;
}
/** called when a the HTTP/1.x version is parsed. */
static int http1_on_http_version(http1_parser_s *parser, char *version,
                                 size_t len) {
  if (!parser2http(parser)->request.headers)
    http_s_init(&parser2http(parser)->request, &parser2http(parser)->p);
  parser2http(parser)->request.version = fiobj_str_static(version, len);
  return 0;
}
/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                           char *data, size_t data_len) {
  fiobj_s *sym;
  fiobj_s *obj;
  if (!parser2http(parser)->request.headers) {
    fprintf(stderr,
            "ERROR: (http1 parse ordering error) missing HashMap for header "
            "%s: %s\n",
            name, data);
    return -1;
  }
  if ((uintptr_t)parser2http(parser)->buf ==
      (uintptr_t)(parser2http(parser) + 1)) {
    sym = fiobj_str_static(name, name_len);
    obj = fiobj_str_static(data, data_len);
  } else {
    sym = fiobj_sym_new(name, name_len);
    obj = fiobj_str_new(data, data_len);
    h1_reset(parser2http(parser));
  }
  set_header_add(parser2http(parser)->request.headers, sym, obj);
  fiobj_free(sym);
  return 0;
}
/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser, char *data,
                               size_t data_len) {
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
static int http1_on_error(http1_parser_s *parser) {
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
static void http1_on_data(intptr_t uuid, protocol_s *protocol) {
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
                         .on_request = http1_on_request,
                         .on_response = http1_on_response,
                         .on_method = http1_on_method,
                         .on_status = http1_on_status, .on_path = http1_on_path,
                         .on_query = http1_on_query,
                         .on_http_version = http1_on_http_version,
                         .on_header = http1_on_header,
                         .on_body_chunk = http1_on_body_chunk,
                         .on_error = http1_on_error);
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
static void http1_on_close(intptr_t uuid, protocol_s *protocol) {
  http1_destroy(protocol);
  (void)uuid;
}

/** called when a data is available for the first time */
static void http1_on_data_first_time(intptr_t uuid, protocol_s *protocol) {
  http1_s *p = (http1_s *)protocol;
  ssize_t i;

  i = sock_read(uuid, p->buf + p->buf_pos, HTTP1_MAX_HEADER_SIZE - p->buf_pos);

  if (i <= 0)
    return;
  if (i >= 24 && !memcmp(p->buf, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24)) {
    fprintf(stderr,
            "ERROR: unsupported HTTP/2 attempeted using prior knowledge.\n");
    sock_close(uuid);
    return;
  }
  p->p.protocol.on_data = http1_on_data;
  p->buf_len += i;
  http1_on_data(uuid, protocol);
}

/* *****************************************************************************
Public API
***************************************************************************** */

/** Creates an HTTP1 protocol object and handles any unread data in the buffer
 * (if any). */
protocol_s *http1_new(uintptr_t uuid, http_settings_s *settings,
                      void *unread_data, size_t unread_length) {
  if (unread_data && unread_length > HTTP1_MAX_HEADER_SIZE)
    return NULL;
  http1_s *p = malloc(sizeof(*p) + HTTP1_MAX_HEADER_SIZE);
  *p = (http1_s){
      .p.protocol =
          {
              .service = HTTP1_SERVICE_STR,
              .on_data = http1_on_data_first_time,
              .on_close = http1_on_close,
          },
      .p.uuid = uuid,
      .p.settings = settings,
      .p.vtable = &HTTP1_VTABLE,
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
void http1_destroy(protocol_s *pr) {
  http1_s *p = (http1_s *)pr;
  p->request.status = 0;
  http_s_cleanup(&p->request);
  if (p->queue.arry) {
    http_s *o;
    while ((o = fio_ary_pop(&p->queue))) {
      http_s_cleanup(o);
      free(o);
    }
    fio_ary_free(&p->queue);
  }
  free(p);
}

/* *****************************************************************************
Protocol Data
***************************************************************************** */

// clang-format off
#define HTTP_SET_STATUS_STR(status, str) [status-100] = { .buffer = ("HTTP/1.1 " #status " " str "\r\n"), .length = (sizeof("HTTP/1.1 " #status " " str "\r\n") - 1) }
// #undef HTTP1_SET_STATUS_STR
// clang-format on

static fio_cstr_s http1_status2str(uintptr_t status) {
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
