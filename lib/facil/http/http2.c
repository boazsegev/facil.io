/*
Copyright: Devstroop, 2026
License: MIT

HTTP/2 protocol object for facil.io.

Integrates the HTTP/2 connection core (parsers/http2.h) with the facil.io
socket layer and the http_s request/response API:

  - server role validates the client connection preface and decodes HEADERS
    blocks into http_s request objects handled by the standard handlers.
  - client role sends the connection preface and decodes responses.
  - outbound DATA is buffered per stream and flushed as the peer's
    WINDOW_UPDATE / SETTINGS replenish the send windows (flow control).
*/
#include <fio.h>

#include <http.h>
#include <http_internal.h>
#include <websockets.h>

#include <http2.h>

#include <fiobj.h>

#include <assert.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

/* *****************************************************************************
Types
***************************************************************************** */

typedef struct http2pr_s http2pr_s;
typedef struct http2_request_s http2_request_s;

/* pending outbound body data (flow control) - memory or file backed */
typedef struct http2_pending_s {
  http2_request_s *req;
  union {
    uint8_t *data; /* malloc'd copy (kind == 0) */
    int fd;        /* file descriptor (kind == 1) */
  } mem;
  uintptr_t offset; /* FILE: next read offset */
  uintptr_t length; /* bytes remaining */
  uint8_t kind;     /* 0 = memory, 1 = file */
  struct http2_pending_s *next;
} http2_pending_s;

struct http2_request_s {
  http_s h;
  uint32_t stream;      /* the HTTP/2 stream id (0 = not yet assigned) */
  uint8_t headers_sent; /* the header block was emitted */
  uint8_t finished;     /* the response is complete (END_STREAM is due) */
  uint8_t closed;       /* END_STREAM was emitted */
  uint8_t push_stream;  /* server push: not a client request / response */
  http2_pending_s *pending; /* flow-control buffered body */
  http2_request_s *next;    /* active requests list */
};

struct http2pr_s {
  http_fio_protocol_s p;
  http2_connection_s conn;
  uint8_t *buf; /* input accumulation buffer */
  size_t buf_len;
  size_t buf_cap;
  uint8_t is_client;
  uint8_t stop;
  uint32_t next_promised; /* server role: next server-initiated stream id */
  http2_request_s *requests; /* active request objects */
  http2_request_s *pushes;   /* client role: received push responses */
  http2_request_s *zombies;  /* completed requests pending free (freed after
                                the current parse completes) */
};

struct http_vtable_s HTTP2_VTABLE; /* initialized later on */

static http2pr_s *conn2pr(http2_connection_s *c) {
  return (http2pr_s *)((uint8_t *)c - offsetof(http2pr_s, conn));
}

static http2pr_s *req2pr(http_s *h) {
  return (http2pr_s *)h->private_data.flag;
}

static http2_request_s *http2_find_req(http2pr_s *p, uint32_t stream) {
  for (http2_request_s *r = p->requests; r; r = r->next)
    if (r->stream == stream)
      return r;
  return NULL;
}

/* finds a received server-push response (client role) */
static http2_request_s *http2_find_push(http2pr_s *p, uint32_t stream) {
  for (http2_request_s *r = p->pushes; r; r = r->next)
    if (r->stream == stream)
      return r;
  return NULL;
}

/* *****************************************************************************
Request object management
***************************************************************************** */

static void http2_flush(http2pr_s *p, uint32_t stream);

static void http2_req_free(http2_request_s *req) {
  http2_pending_s *pend = req->pending;
  while (pend) {
    http2_pending_s *tmp = pend;
    pend = pend->next;
    if (tmp->kind == 0)
      fio_free(tmp->mem.data);
    else
      close(tmp->mem.fd);
    fio_free(tmp);
  }
  http_s_destroy(&req->h, req2pr(&req->h)->p.settings->log);
  fio_free(req);
}

/* moves a completed request to the zombie list (unlinked from the active
 * list, but freed only after the current parse returns - the handler stack
 * may still hold a pointer to it). */
static void http2_req_zombie(http2_request_s *req) {
  http2pr_s *p = req2pr(&req->h);
  http2_request_s **pnext = &p->requests;
  while (*pnext && *pnext != req)
    pnext = &(*pnext)->next;
  if (*pnext)
    *pnext = req->next;
  req->next = p->zombies;
  p->zombies = req;
}

/* frees all zombie requests (called after a parse cycle completes) */
static void http2_zombies_free(http2pr_s *p) {
  http2_request_s *z = p->zombies;
  p->zombies = NULL;
  while (z) {
    http2_request_s *tmp = z;
    z = z->next;
    http2_req_free(tmp);
  }
}

/* the response is complete - flush everything and send END_STREAM */
static void http2_req_complete(http2_request_s *req) {
  http2pr_s *p = req2pr(&req->h);
  if (req->finished || req->closed)
    return;
  req->finished = 1;
  http2_flush(p, req->stream);
}

/* appends a pending chunk to the request's pending queue */
static void http2_pending_add(http2_request_s *req, http2_pending_s *pend) {
  http2_pending_s **tail = &req->pending;
  while (*tail)
    tail = &(*tail)->next;
  *tail = pend;
}

/* sends buffered body data, respecting the flow-control windows */
static void http2_flush(http2pr_s *p, uint32_t stream) {
  http2_request_s *req = p->requests;
  while (req) {
    http2_request_s *next = req->next;
    if (!stream || req->stream == stream) {
http2_pending_s *pend = req->pending;
      while (pend && !req->closed) {
        if (pend->kind == 0) {
          size_t remaining = pend->length - pend->offset;
          ssize_t sent = http2_connection_send_data(
              &p->conn, req->stream, pend->mem.data + pend->offset, remaining,
              0);
          if (sent < 0) {
            fio_close(p->p.uuid);
            return;
          }
          if (!sent)
            break; /* windows exhausted - wait for WINDOW_UPDATE */
          pend->offset += (size_t)sent;
          if (pend->offset == pend->length) {
            /* this entry was fully sent */
            http2_pending_s *tmp = pend;
            req->pending = pend->next;
            fio_free(tmp->mem.data);
            fio_free(tmp);
            pend = req->pending;
          }
          continue;
        }
        /* file backed - read the next chunk */
        uintptr_t chunk = pend->length;
        uintptr_t limit = p->conn.peer.max_frame_size;
        if (chunk > limit)
          chunk = limit;
        uint8_t *buf = (uint8_t *)fio_malloc(chunk ? chunk : 1);
        if (!buf) {
          fio_close(p->p.uuid);
          return;
        }
        ssize_t r = pread(pend->mem.fd, buf, chunk, (off_t)pend->offset);
        if (r <= 0) {
          fio_free(buf);
          fio_close(p->p.uuid);
          return;
        }
        ssize_t sent = http2_connection_send_data(&p->conn, req->stream, buf,
                                                  (size_t)r, 0);
        if (sent < 0) {
          fio_free(buf);
          fio_close(p->p.uuid);
          return;
        }
        if ((size_t)sent < (size_t)r) {
          /* partial send - continue from memory */
          pend->kind = 0;
          pend->mem.data = buf;
          pend->offset = 0;
          pend->length = (size_t)r - (size_t)sent;
          memmove(buf, buf + sent, pend->length);
          break;
        }
        fio_free(buf);
        pend->offset += (uintptr_t)r;
        pend->length -= (uintptr_t)r;
        if (!pend->length) {
          http2_pending_s *tmp = pend;
          req->pending = pend->next;
          close(tmp->mem.fd);
          fio_free(tmp);
          pend = req->pending;
        }
      }
    if (!req->pending && req->finished && !req->closed) {
      /* finalize the response with an empty END_STREAM DATA frame */
      if (http2_connection_send_data(&p->conn, req->stream, NULL, 0, 1) < 0) {
        fio_close(p->p.uuid);
        return;
      }
      req->closed = 1;
      if (!p->is_client) {
        /* the request is complete - defer its free until the current parse
         * returns and restart the scan */
        http2_req_zombie(req);
        req = p->requests;
        continue;
      }
    }
    }
    req = next;
  }
}

/* *****************************************************************************
Header block helpers
***************************************************************************** */

/* scratch buffer used while building a header block (synchronous use only) */
typedef struct {
  uint8_t *data;
  size_t len;
  size_t cap;
} http2_scratch_s;

static uint8_t *http2_scratch_alloc(http2_scratch_s *s, size_t len) {
  if (s->len + len > s->cap) {
    size_t ncap = (s->len + len) * 2;
    if (ncap < 64)
      ncap = 64;
    uint8_t *nd = (uint8_t *)fio_realloc(s->data, ncap);
    FIO_ASSERT_ALLOC(nd);
    s->data = nd;
    s->cap = ncap;
  }
  uint8_t *p = s->data + s->len;
  s->len += len;
  return p;
}

/* connection-specific header names that must not be sent over HTTP/2 */
static int http2__skip_header(const char *name, size_t len) {
  static const char *const skip[] = {"connection", "keep-alive",
                                     "transfer-encoding", "upgrade",
                                     "proxy-connection"};
  for (size_t i = 0; i < 5; ++i) {
    size_t slen = 0;
    while (skip[i][slen])
      ++slen;
    if (len != slen)
      continue;
    size_t j = 0;
    for (; j < len; ++j) {
      char a = name[j];
      char b = skip[i][j];
      if (a >= 'A' && a <= 'Z')
        a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z')
        b = (char)(b - 'A' + 'a');
      if (a != b)
        break;
    }
    if (j == len)
      return 1;
  }
  return 0;
}

typedef struct {
  http2_scratch_s *sc;
  hpack_header_s *fields;
  size_t count;
} http2_hwriter_s;

/* counts the fields in an outgoing header hash */
static int http2__hcount(FIOBJ o, void *w_) {
  size_t *count = (size_t *)w_;
  if (!o)
    return 0;
  FIOBJ key = fiobj_hash_key_in_loop();
  if (key && !FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fio_str_info_s name = fiobj_obj2cstr(key);
    if (name.data && http2__skip_header(name.data, name.len))
      return 0;
  }
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, http2__hcount, w_);
    return 0;
  }
  fio_str_info_s str = fiobj_obj2cstr(o);
  if (!str.data)
    return 0;
  ++*count;
  return 0;
}

/* writes an outgoing header hash into the field array (lowercasing names) */
static int http2__hwrite(FIOBJ o, void *w_) {
  http2_hwriter_s *w = (http2_hwriter_s *)w_;
  if (!o)
    return 0;
  FIOBJ key = fiobj_hash_key_in_loop();
  if (key && !FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fio_str_info_s name = fiobj_obj2cstr(key);
    if (name.data && http2__skip_header(name.data, name.len))
      return 0;
    if (name.data && name.len && name.data[0] == ':')
      return 0;
  }
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, http2__hwrite, w_);
    return 0;
  }
  fio_str_info_s str = fiobj_obj2cstr(o);
  FIOBJ key2 = fiobj_hash_key_in_loop();
  if (!str.data || !key2)
    return 0;
  fio_str_info_s name = fiobj_obj2cstr(key2);
  if (!name.data || !name.len)
    return 0;
  if (name.data[0] == ':')
    return 0;
  /* lowercase the name into the scratch buffer */
  uint8_t *lname = http2_scratch_alloc(w->sc, name.len);
  for (size_t i = 0; i < name.len; ++i)
    lname[i] = (uint8_t)((name.data[i] >= 'A' && name.data[i] <= 'Z')
                             ? name.data[i] - 'A' + 'a'
                             : name.data[i]);
  w->fields[w->count].name.data = (char *)lname;
  w->fields[w->count].name.len = name.len;
  w->fields[w->count].value.data = str.data;
  w->fields[w->count].value.len = str.len;
  ++w->count;
  return 0;
}

/* builds the response header fields (:status first). Returns 0 on success. */
static int http2__response_fields(http_s *h, http2_scratch_s *sc,
                                  hpack_header_s **fields_, size_t *count_) {
  size_t count = 1;
  fiobj_each1(h->private_data.out_headers, 0, http2__hcount, &count);
  hpack_header_s *fields =
      (hpack_header_s *)fio_malloc(count * sizeof(*fields));
  if (!fields)
    return -1;
  /* the :status pseudo header */
  uint8_t *status = http2_scratch_alloc(sc, 8);
  size_t status_len = 0;
  uintptr_t s = h->status;
  if (s > 999)
    s = 500;
  do {
    status[status_len++] = (uint8_t)('0' + s % 10);
    s /= 10;
  } while (s);
  /* reverse the digits */
  for (size_t i = 0; i < status_len / 2; ++i) {
    uint8_t tmp = status[i];
    status[i] = status[status_len - i - 1];
    status[status_len - i - 1] = tmp;
  }
  fields[0].name.data = ":status";
  fields[0].name.len = 7;
  fields[0].value.data = (char *)status;
  fields[0].value.len = status_len;
  http2_hwriter_s w = {.sc = sc, .fields = fields, .count = 1};
  fiobj_each1(h->private_data.out_headers, 0, http2__hwrite, &w);
  *fields_ = fields;
  *count_ = w.count;
  return 0;
}

/* builds the client request header fields (:method, :path, :scheme,
 * :authority first). Returns 0 on success. */
static int http2__request_fields(http_s *h, http2_scratch_s *sc,
                                 hpack_header_s **fields_, size_t *count_) {
  http2pr_s *p = req2pr(h);
  fio_str_info_s method = fiobj_obj2cstr(h->method);
  fio_str_info_s path = fiobj_obj2cstr(h->path);
  fio_str_info_s query = {.data = NULL, .len = 0};
  if (h->query != FIOBJ_INVALID)
    query = fiobj_obj2cstr(h->query);
  if (!method.data)
    method = (fio_str_info_s){.data = "GET", .len = 3};
  if (!path.data)
    return -1;
  /* :authority from the host header */
  static uint64_t host_hash;
  if (!host_hash)
    host_hash = fiobj_hash_string("host", 4);
  fio_str_info_s authority = {.data = NULL, .len = 0};
  FIOBJ host = fiobj_hash_get2(h->private_data.out_headers, host_hash);
  if (!host)
    host = fiobj_hash_get2(h->headers, host_hash);
  if (host)
    authority = fiobj_obj2cstr(host);
  size_t count = 3 + (authority.data ? 1 : 0);
  fiobj_each1(h->private_data.out_headers, 0, http2__hcount, &count);
  hpack_header_s *fields =
      (hpack_header_s *)fio_malloc(count * sizeof(*fields));
  if (!fields)
    return -1;
  /* :path = path + ? + query */
  uint8_t *pathbuf = http2_scratch_alloc(sc, path.len + (query.data ? query.len + 1 : 0));
  memcpy(pathbuf, path.data, path.len);
  size_t path_len = path.len;
  if (query.data) {
    pathbuf[path_len++] = '?';
    memcpy(pathbuf + path_len, query.data, query.len);
    path_len += query.len;
  }
  size_t i = 0;
  fields[i].name.data = ":method";
  fields[i].name.len = 7;
  fields[i].value.data = method.data;
  fields[i].value.len = method.len;
  ++i;
  fields[i].name.data = ":path";
  fields[i].name.len = 5;
  fields[i].value.data = (char *)pathbuf;
  fields[i].value.len = path_len;
  ++i;
  fields[i].name.data = ":scheme";
  fields[i].name.len = 7;
  fields[i].value.data = p->p.settings->tls ? "https" : "http";
  fields[i].value.len = p->p.settings->tls ? 5 : 4;
  ++i;
  if (authority.data) {
    fields[i].name.data = ":authority";
    fields[i].name.len = 10;
    fields[i].value.data = authority.data;
    fields[i].value.len = authority.len;
    ++i;
  }
  http2_hwriter_s w = {.sc = sc, .fields = fields, .count = i};
  fiobj_each1(h->private_data.out_headers, 0, http2__hwrite, &w);
  *fields_ = fields;
  *count_ = w.count;
  return 0;
}

/* sends the request header block (client role, stream allocation) */
static int http2__send_request_headers(http2_request_s *req, int end_stream) {
  http2pr_s *p = req2pr(&req->h);
  if (!req->stream) {
    req->stream = p->conn.state.last_local_stream + 2;
    if (!(req->stream & 1) ||
        req->stream <= p->conn.state.last_local_stream)
      return -1;
  }
  http2_scratch_s sc = {0};
  hpack_header_s *fields = NULL;
  size_t count = 0;
  int r = -1;
  if (http2__request_fields(&req->h, &sc, &fields, &count) == 0)
    r = http2_connection_send_headers(&p->conn, req->stream, fields, count,
                                      end_stream);
  fio_free(fields);
  fio_free(sc.data);
  if (r)
    return -1;
  req->headers_sent = 1;
  return 0;
}

/* sends the response header block (server role) */
static int http2__send_response_headers(http2_request_s *req, int end_stream) {
  http2pr_s *p = req2pr(&req->h);
  http2_scratch_s sc = {0};
  hpack_header_s *fields = NULL;
  size_t count = 0;
  int r = -1;
  if (http2__response_fields(&req->h, &sc, &fields, &count) == 0) {
    r = http2_connection_send_headers(&p->conn, req->stream, fields, count,
                                      end_stream);
  }
  fio_free(fields);
  fio_free(sc.data);
  if (r)
    return -1;
  req->headers_sent = 1;
  return 0;
}

/* *****************************************************************************
Required HTTP/2 callbacks
***************************************************************************** */

static int http2_connection_emit(http2_connection_s *c, uint32_t length,
                                 uint8_t type, uint8_t flags, uint32_t stream,
                                 const uint8_t *payload) {
  http2pr_s *p = conn2pr(c);
  if (type == 0xff) {
    /* raw connection preface */
    if (fio_write2(p->p.uuid, .data.buffer = (void *)payload, .length = length,
                   .after.dealloc = FIO_DEALLOC_NOOP))
      return -1;
    return 0;
  }
  uint8_t *frame = (uint8_t *)fio_malloc(9 + length);
  if (!frame)
    return -1;
  frame[0] = (uint8_t)(length >> 16);
  frame[1] = (uint8_t)(length >> 8);
  frame[2] = (uint8_t)length;
  frame[3] = type;
  frame[4] = flags;
  http2__be32(frame + 5, stream);
  if (length)
    memcpy(frame + 9, payload, length);
  if (fio_write2(p->p.uuid, .data.buffer = frame, .length = 9 + length,
                 .after.dealloc = fio_free)) {
    fio_free(frame);
    return -1;
  }
  return 0;
}

/* delivers a complete request / response to the standard handlers */
static void http2_request_ready(http2pr_s *p, http2_request_s *req) {
  if (req->push_stream) {
    /* a pushed response completed (client role) - drop it */
    http2_request_s **pp = &p->pushes;
    while (*pp && *pp != req)
      pp = &(*pp)->next;
    if (*pp)
      *pp = req->next;
    http_s_destroy(&req->h, 0);
    fio_free(req);
    return;
  }
  if (p->is_client) {
    http_on_response_handler______internal(&req->h, p->p.settings);
    if (!p->stop && req->h.method && !req->closed) {
      /* the app set up a new request - send it now */
      req->stream = 0;
      req->headers_sent = 0;
      req->finished = 0;
      req->closed = 0;
      http_finish(&req->h);
    } else if (!req->closed) {
      /* reset the template for the next request */
      req->stream = 0;
      req->headers_sent = 0;
      req->finished = 0;
      req->closed = 0;
    }
    return;
  }
  /* NOTE: the handler may complete (and free) the request object, so the
   * relevant state is captured before the handler is called. After the
   * handler, the request is only freed after the current parse returns, so
   * it is safe to verify it is still active before auto-finishing. */
  uint8_t has_method = req->h.method != FIOBJ_INVALID;
  uint32_t stream = req->stream;
  http_on_request_handler______internal(&req->h, p->p.settings);
  if (has_method && !p->stop && http2_find_req(p, stream) == req)
    http_finish(&req->h);
}

static void http2_on_headers(http2_connection_s *c, uint32_t stream,
                             const hpack_header_s *fields, size_t count,
                             int trailers, int end_stream) {
  http2pr_s *p = conn2pr(c);
  http2_request_s *req = http2_find_req(p, stream);
  if (!req)
    req = http2_find_push(p, stream);
  int is_trailers = trailers;
  if (p->is_client) {
    /* the client response object is the request template */
    is_trailers = is_trailers || (req && req->h.status_str);
  } else if (req) {
    is_trailers = 1;
  }
  if (is_trailers) {
    if (!req)
      return;
    for (size_t i = 0; i < count; ++i) {
      FIOBJ name = fiobj_str_new(fields[i].name.data, fields[i].name.len);
      FIOBJ value = fiobj_str_new(fields[i].value.data, fields[i].value.len);
      set_header_add(req->h.headers, name, value);
      fiobj_free(name);
      fiobj_free(value);
    }
    if (end_stream)
      http2_request_ready(p, req);
    return;
  }
  if (p->is_client) {
    if (req && !req->push_stream)
      http_s_clear(&req->h, p->p.settings->log);
  } else {
    req = (http2_request_s *)fio_malloc(sizeof(*req));
    FIO_ASSERT_ALLOC(req);
    *req = (http2_request_s){.stream = stream};
    http_s_new(&req->h, &p->p, &HTTP2_VTABLE);
    req->next = p->requests;
    p->requests = req;
  }
  FIOBJ authority = FIOBJ_INVALID;
  uint8_t has_method = 0, has_path = 0, has_scheme = 0;
  for (size_t i = 0; i < count; ++i) {
    fio_str_info_s name = fields[i].name;
    fio_str_info_s value = fields[i].value;
    if (name.len && name.data[0] == ':') {
      if (name.len == 7 && !memcmp(name.data, ":method", 7)) {
        if (!p->is_client) {
          req->h.method = fiobj_str_new(value.data, value.len);
          has_method = 1;
        }
      } else if (name.len == 5 && !memcmp(name.data, ":path", 5)) {
        if (!p->is_client) {
          const char *q =
              value.data ? memchr(value.data, '?', value.len) : NULL;
          if (q) {
            req->h.path = fiobj_str_new(value.data, (size_t)(q - value.data));
            req->h.query =
                fiobj_str_new(q + 1, value.len - (size_t)(q - value.data) - 1);
          } else {
            req->h.path = fiobj_str_new(value.data, value.len);
          }
          has_path = 1;
        }
      } else if (name.len == 7 && !memcmp(name.data, ":scheme", 7)) {
        if (!p->is_client) {
          req->h.version = fiobj_str_new(value.data, value.len);
          has_scheme = 1;
        }
      } else if (name.len == 10 && !memcmp(name.data, ":authority", 10)) {
        if (!p->is_client)
          authority = fiobj_str_new(value.data, value.len);
      } else if (name.len == 7 && !memcmp(name.data, ":status", 7)) {
        if (p->is_client) {
          uintptr_t status = 0;
          for (size_t j = 0; j < value.len && value.data[j] >= '0' &&
                             value.data[j] <= '9';
               ++j)
            status = status * 10 + (uintptr_t)(value.data[j] - '0');
          req->h.status = status;
          req->h.status_str = fiobj_str_new(value.data, value.len);
        }
      }
      continue;
    }
    if (name.len == 0 || !value.data)
      continue;
    FIOBJ k = fiobj_str_new(name.data, name.len);
    FIOBJ v = fiobj_str_new(value.data, value.len);
    set_header_add(req->h.headers, k, v);
    fiobj_free(k);
    fiobj_free(v);
  }
  if (authority != FIOBJ_INVALID) {
    set_header_add(req->h.headers, HTTP_HEADER_HOST, authority);
    fiobj_free(authority);
  }
  if (!p->is_client && !(has_method && has_path && has_scheme)) {
    /* RFC 9113 §8.3.1 - missing mandatory pseudo headers */
    http2_connection_abort(c, HTTP2_ERROR_PROTOCOL);
    http2_req_zombie(req);
    return;
  }
  if (end_stream)
    http2_request_ready(p, req);
}

static void http2_on_data(http2_connection_s *c, uint32_t stream,
                          uint8_t *data, uint32_t length,
                          uint32_t payload_len, int end_stream) {
  http2pr_s *p = conn2pr(c);
  http2_request_s *req = http2_find_req(p, stream);
  if (!req)
    req = http2_find_push(p, stream);
  if (!req)
    return;
  if (length) {
    if (!req->h.body)
      req->h.body = fiobj_data_newstr();
    fiobj_data_write(req->h.body, data, length);
    /* replenish the consumed flow-control windows - the full payload size
     * (padding included) was charged to the windows (RFC 9113 §6.9) */
    http2_connection_window_update(c, stream, payload_len);
    http2_connection_window_update(c, 0, payload_len);
  }
  if (end_stream)
    http2_request_ready(p, req);
}

static void http2_on_push_promise(http2_connection_s *c, uint32_t stream,
                                  uint32_t promised_stream,
                                  const hpack_header_s *fields, size_t count) {
  (void)stream;
  http2pr_s *p = conn2pr(c);
  if (!p->is_client) {
    /* servers must not receive PUSH_PROMISE (RFC 9113 §6.6) */
    http2_connection_abort(c, HTTP2_ERROR_PROTOCOL);
    return;
  }
  /* record the promised stream so its response can be routed */
  http2_request_s *req = http2_find_push(p, promised_stream);
  if (!req) {
    req = (http2_request_s *)fio_malloc(sizeof(*req));
    FIO_ASSERT_ALLOC(req);
    *req = (http2_request_s){.stream = promised_stream, .push_stream = 1};
    http_s_new(&req->h, &p->p, &HTTP2_VTABLE);
    req->next = p->pushes;
    p->pushes = req;
  }
  /* the promised request's pseudo headers */
  for (size_t i = 0; i < count; ++i) {
    fio_str_info_s name = fields[i].name;
    fio_str_info_s value = fields[i].value;
    if (name.len == 5 && !memcmp(name.data, ":path", 5))
      req->h.path = fiobj_str_new(value.data, value.len);
    else if (name.len == 7 && !memcmp(name.data, ":method", 7))
      req->h.method = fiobj_str_new(value.data, value.len);
  }
}

static void http2_on_goaway(http2_connection_s *c, uint32_t last_stream,
                            uint32_t error) {
  (void)c;
  (void)last_stream;
  (void)error;
  /* the connection is draining - close once all requests are complete */
}

static void http2_on_settings(http2_connection_s *c) {
  (void)c;
}

static void http2_on_window_update(http2_connection_s *c, uint32_t stream,
                                   uint32_t increment) {
  (void)increment;
  http2pr_s *p = conn2pr(c);
  http2_flush(p, stream);
}

/* *****************************************************************************
HTTP vtable
***************************************************************************** */

static int http2_send_body(http_s *h, void *data, uintptr_t length) {
  http2_request_s *req = (http2_request_s *)h;
  http2pr_s *p = req2pr(h);
  if (req->closed || req->finished)
    return -1;
  if (!req->headers_sent) {
    int r;
    if (p->is_client && !req->stream)
      r = http2__send_request_headers(req, 0);
    else
      r = http2__send_response_headers(req, 0);
    if (r) {
      fio_close(p->p.uuid);
      return -1;
    }
  }
  if (length) {
    uint8_t *copy = (uint8_t *)fio_malloc(length);
    if (!copy)
      return -1;
    memcpy(copy, data, length);
    http2_pending_s *pend = (http2_pending_s *)fio_malloc(sizeof(*pend));
    if (!pend) {
      fio_free(copy);
      return -1;
    }
    *pend = (http2_pending_s){.req = req, .mem.data = copy, .length = length,
                              .kind = 0};
    http2_pending_add(req, pend);
  }
  http2_req_complete(req);
  return 0;
}

static int http2_sendfile(http_s *h, int fd, uintptr_t length,
                          uintptr_t offset) {
  http2_request_s *req = (http2_request_s *)h;
  http2pr_s *p = req2pr(h);
  if (req->closed || req->finished) {
    close(fd);
    return -1;
  }
  if (!req->headers_sent) {
    int r;
    if (p->is_client && !req->stream)
      r = http2__send_request_headers(req, 0);
    else
      r = http2__send_response_headers(req, 0);
    if (r) {
      close(fd);
      fio_close(p->p.uuid);
      return -1;
    }
  }
  if (length) {
    http2_pending_s *pend = (http2_pending_s *)fio_malloc(sizeof(*pend));
    if (!pend) {
      close(fd);
      return -1;
    }
    *pend = (http2_pending_s){.req = req, .mem.fd = fd, .offset = offset,
                              .length = length, .kind = 1};
    http2_pending_add(req, pend);
  } else {
    close(fd);
  }
  http2_req_complete(req);
  return 0;
}

static void http2_finish(http_s *h) {
  http2_request_s *req = (http2_request_s *)h;
  http2pr_s *p = req2pr(h);
  if (!req || !p || req->closed || req->finished)
    return;
  if (!req->headers_sent) {
    int r;
    if (p->is_client)
      r = http2__send_request_headers(req, 1);
    else
      r = http2__send_response_headers(req, 1);
    if (r) {
      fio_close(p->p.uuid);
      return;
    }
    req->closed = 1;
    if (!p->is_client)
      http2_req_zombie(req);
    return;
  }
  http2_req_complete(req);
}

static intptr_t http2_hijack(http_s *h, fio_str_info_s *leftover) {
  (void)h;
  (void)leftover;
  /* HTTP/2 connections cannot be hijacked (multiplexed streams) */
  return -1;
}

static int http2websocket(http_s *h, websocket_settings_s *arg) {
  (void)h;
  (void)arg;
  /* HTTP/2 websockets (RFC 8441) are not implemented yet (issue #5) */
  return -1;
}

/* sends a PUSH_PROMISE (RFC 9113 §6.6) for a resource served under the source
 * request's URL and creates the promised request object. Returns the promised
 * stream id, or 0 on failure. */
static uint32_t http2__push_promise(http2pr_s *p, http2_request_s *src) {
  http2_scratch_s sc = {0};
  hpack_header_s *fields = NULL;
  size_t count = 0;
  uint32_t promised = 0;
  do {
    uint32_t stream = src->stream;
    fio_str_info_s path = fiobj_obj2cstr(src->h.path);
    if (!path.data)
      break;
    fio_str_info_s query = {.data = NULL, .len = 0};
    if (src->h.query != FIOBJ_INVALID)
      query = fiobj_obj2cstr(src->h.query);
    fio_str_info_s authority = {.data = NULL, .len = 0};
    static uint64_t host_hash;
    if (!host_hash)
      host_hash = fiobj_hash_string("host", 4);
    FIOBJ host = fiobj_hash_get2(src->h.headers, host_hash);
    if (host)
      authority = fiobj_obj2cstr(host);
    count = 3 + (authority.data ? 1 : 0);
    fields = (hpack_header_s *)fio_malloc(count * sizeof(*fields));
    if (!fields)
      break;
    uint8_t *pathbuf =
        http2_scratch_alloc(&sc,
                            path.len + (query.data ? query.len + 1 : 0));
    memcpy(pathbuf, path.data, path.len);
    size_t path_len = path.len;
    if (query.data) {
      pathbuf[path_len++] = '?';
      memcpy(pathbuf + path_len, query.data, query.len);
      path_len += query.len;
    }
    size_t i = 0;
    fields[i].name.data = ":method";
    fields[i].name.len = 7;
    fields[i].value.data = "GET";
    fields[i].value.len = 3;
    ++i;
    fields[i].name.data = ":path";
    fields[i].name.len = 5;
    fields[i].value.data = (char *)pathbuf;
    fields[i].value.len = path_len;
    ++i;
    fields[i].name.data = ":scheme";
    fields[i].name.len = 7;
    fields[i].value.data = p->p.settings->tls ? "https" : "http";
    fields[i].value.len = p->p.settings->tls ? 5 : 4;
    ++i;
    if (authority.data) {
      fields[i].name.data = ":authority";
      fields[i].name.len = 10;
      fields[i].value.data = authority.data;
      fields[i].value.len = authority.len;
      ++i;
    }
    promised = p->next_promised;
    p->next_promised += 2;
    if (http2_connection_send_push_promise(&p->conn, stream, promised, fields,
                                           i)) {
      promised = 0;
      break;
    }
  } while (0);
  fio_free(fields);
  fio_free(sc.data);
  return promised;
}

/* creates the request object for a pushed resource */
static http2_request_s *http2__push_new(http2pr_s *p, uint32_t stream,
                                        http2_request_s *src) {
  http2_request_s *req = (http2_request_s *)fio_malloc(sizeof(*req));
  FIO_ASSERT_ALLOC(req);
  *req = (http2_request_s){.stream = stream, .push_stream = 1};
  http_s_new(&req->h, &p->p, &HTTP2_VTABLE);
  req->h.method = fiobj_str_new("GET", 3);
  req->h.path = fiobj_dup(src->h.path);
  if (src->h.query)
    req->h.query = fiobj_dup(src->h.query);
  req->next = p->requests;
  p->requests = req;
  return req;
}

static int http2_push_data(http_s *h, void *data, uintptr_t length,
                           FIOBJ mime_type) {
  http2pr_s *p = req2pr(h);
  http2_request_s *src = (http2_request_s *)h;
  if (p->is_client || src->push_stream || src->finished || src->closed ||
      p->conn.state.state != 0)
    return -1;
  uint32_t promised = http2__push_promise(p, src);
  if (!promised)
    return -1;
  http2_request_s *req = http2__push_new(p, promised, src);
  if (mime_type)
    http_set_header(&req->h, HTTP_HEADER_CONTENT_TYPE, fiobj_dup(mime_type));
  http_set_header(&req->h, HTTP_HEADER_CONTENT_LENGTH,
                  fiobj_num_new((int64_t)length));
  return http2_send_body(&req->h, data, length);
}

static int http2_push_file(http_s *h, FIOBJ filename, FIOBJ mime_type) {
  fio_str_info_s fn = fiobj_obj2cstr(filename);
  if (!fn.data)
    return -1;
  int fd = open(fn.data, O_RDONLY);
  if (fd < 0)
    return -1;
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    return -1;
  }
  http2pr_s *p = req2pr(h);
  http2_request_s *src = (http2_request_s *)h;
  if (p->is_client || src->push_stream || src->finished || src->closed ||
      p->conn.state.state != 0) {
    close(fd);
    return -1;
  }
  uint32_t promised = http2__push_promise(p, src);
  if (!promised) {
    close(fd);
    return -1;
  }
  http2_request_s *req = http2__push_new(p, promised, src);
  if (mime_type)
    http_set_header(&req->h, HTTP_HEADER_CONTENT_TYPE, fiobj_dup(mime_type));
  http_set_header(&req->h, HTTP_HEADER_CONTENT_LENGTH,
                  fiobj_num_new((int64_t)st.st_size));
  return http2_sendfile(&req->h, fd, (uintptr_t)st.st_size, 0);
}

static int http2_upgrade2sse(http_s *h, http_sse_s *sse) {
  (void)h;
  (void)sse;
  /* SSE over HTTP/2 is not implemented yet (issue #5) */
  return -1;
}

static int http2_sse_write(http_sse_s *sse, FIOBJ str) {
  (void)sse;
  (void)str;
  return -1;
}

static int http2_sse_close(http_sse_s *sse) {
  (void)sse;
  return -1;
}

static void http2_on_pause(http_s *h, http_fio_protocol_s *pr) {
  http2pr_s *p = (http2pr_s *)pr;
  p->stop = 1;
  fio_suspend(pr->uuid);
  (void)h;
}

static void http2_on_resume(http_s *h, http_fio_protocol_s *pr) {
  http2pr_s *p = (http2pr_s *)pr;
  if (!p->stop)
    fio_force_event(pr->uuid, FIO_EVENT_ON_DATA);
  (void)h;
}

struct http_vtable_s HTTP2_VTABLE = {
    .http_send_body = http2_send_body,
    .http_sendfile = http2_sendfile,
    .http_stream = NULL,
    .http_finish = http2_finish,
    .http_push_data = http2_push_data,
    .http2websocket = http2websocket,
    .http_push_file = http2_push_file,
    .http_on_pause = http2_on_pause,
    .http_on_resume = http2_on_resume,
    .http_hijack = http2_hijack,
    .http_upgrade2sse = http2_upgrade2sse,
    .http_sse_write = http2_sse_write,
    .http_sse_close = http2_sse_close,
};

/* *****************************************************************************
Socket protocol
***************************************************************************** */

static void http2_destroy(fio_protocol_s *pr) {
  http2pr_s *p = (http2pr_s *)pr;
  while (p->requests)
    http2_req_zombie(p->requests);
  while (p->pushes) {
    http2_request_s *tmp = p->pushes;
    p->pushes = tmp->next;
    http2_req_free(tmp);
  }
  http2_zombies_free(p);
  http2_connection_destroy(&p->conn);
  fio_free(p->buf);
  fio_free(p);
}

static void http2_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  http2_destroy(protocol);
}

static void http2_on_ready(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  (void)protocol;
}

static void http2_fio_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  http2pr_s *p = (http2pr_s *)protocol;
  if (p->stop) {
    fio_suspend(uuid);
    return;
  }
  if (!p->buf) {
    p->buf_cap = (1 << 16);
    p->buf = (uint8_t *)fio_malloc(p->buf_cap);
    FIO_ASSERT_ALLOC(p->buf);
  }
  if (p->buf_len == p->buf_cap) {
    p->buf_cap <<= 1;
    p->buf = (uint8_t *)fio_realloc(p->buf, p->buf_cap);
    FIO_ASSERT_ALLOC(p->buf);
  }
  ssize_t i = fio_read(uuid, p->buf + p->buf_len, p->buf_cap - p->buf_len);
  if (i > 0)
    p->buf_len += (size_t)i;
  if (!p->buf_len)
    return;
  size_t used = http2_connection_parse(&p->conn, p->buf, p->buf_len);
  if (used) {
    memmove(p->buf, p->buf + used, p->buf_len - used);
    p->buf_len -= used;
  }
  http2_zombies_free(p);
  if (p->conn.state.state == 2 ||
      (p->conn.state.state == 1 && !p->requests)) {
    fio_close(uuid);
  }
}

/* *****************************************************************************
Public API
***************************************************************************** */

void *http2_vtable(void) { return (void *)&HTTP2_VTABLE; }

fio_protocol_s *http2_new(uintptr_t uuid, http_settings_s *settings,
                          void *unread_data, size_t unread_length,
                          void *upgrade_settings, size_t upgrade_settings_len) {
  http2pr_s *p = (http2pr_s *)fio_malloc(sizeof(*p));
  FIO_ASSERT_ALLOC(p);
  *p = (http2pr_s){
      .p.protocol =
          {
              .on_data = http2_fio_on_data,
              .on_ready = http2_on_ready,
              .on_close = http2_on_close,
          },
      .p.uuid = uuid,
      .p.settings = settings,
      .is_client = settings->is_client,
      .next_promised = 2,
  };
  http2_connection_init(&p->conn,
                        settings->is_client ? HTTP2_CONNECTION_CLIENT
                                            : HTTP2_CONNECTION_SERVER,
                        4096);
  if (upgrade_settings && upgrade_settings_len) {
    /* h2c upgrade (RFC 9113 §3.2): apply the client's HTTP2-Settings payload
     * as if a SETTINGS frame was received (no ACK is sent, §6.5.3). */
    if (upgrade_settings_len % 6) {
      http2_connection_destroy(&p->conn);
      fio_free(p);
      return NULL;
    }
    int r2 = http2__apply_settings(&p->conn, (const uint8_t *)upgrade_settings,
                                   (uint32_t)upgrade_settings_len);
    if (r2) {
      http2_connection_destroy(&p->conn);
      fio_free(p);
      return NULL;
    }
    http2_on_settings(&p->conn);
  }
  if (p->is_client) {
    http2_connection_send_preface(&p->conn);
    /* the client request template */
    http2_request_s *req =
        (http2_request_s *)fio_malloc(sizeof(*req));
    FIO_ASSERT_ALLOC(req);
    *req = (http2_request_s){.stream = 0};
    http_s_new(&req->h, &p->p, &HTTP2_VTABLE);
    req->next = p->requests;
    p->requests = req;
  }
  fio_attach(uuid, &p->p.protocol);
  if (unread_data && unread_length) {
    /* unread data was passed to the connection on creation */
    if (!p->buf) {
      p->buf_cap = unread_length + (1 << 16);
      p->buf = (uint8_t *)fio_malloc(p->buf_cap);
      FIO_ASSERT_ALLOC(p->buf);
    }
    if (p->buf_cap - p->buf_len < unread_length) {
      p->buf_cap += unread_length + (1 << 16);
      p->buf = (uint8_t *)fio_realloc(p->buf, p->buf_cap);
      FIO_ASSERT_ALLOC(p->buf);
    }
    memcpy(p->buf + p->buf_len, unread_data, unread_length);
    p->buf_len += unread_length;
    fio_force_event(uuid, FIO_EVENT_ON_DATA);
  }
  return &p->p.protocol;
}
