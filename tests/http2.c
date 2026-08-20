/*
Copyright: Devstroop, 2026
License: MIT

Integration test for the HTTP/2 protocol object (lib/facil/http/http2.c).

The test starts an HTTP/2 server (using `http2_new` directly) and connects a
hand-rolled HTTP/2 client that speaks the protocol on the wire (preface,
SETTINGS, HPACK encoded HEADERS, flow-control aware DATA and WINDOW_UPDATE).

The following scenarios are covered:
  - the server validates the connection preface and decodes a GET request,
    invoking the standard request handler (stream 1, "hello world").
  - a large POST body (70,000 bytes) is received under flow control
    (stream 3, echoed back by the server).
  - a large response body (200,000 bytes) is sent under flow control
    (stream 5, "/large").
*/
#include <fio.h>
#include <http.h>
#include <fiobj.h>

#include <hpack.h>
#include <http2_parser.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* http2.c protocol API (not yet exposed in a public header) */
fio_protocol_s *http2_new(uintptr_t uuid, http_settings_s *settings,
                          void *unread_data, size_t unread_length,
                          void *upgrade_settings, size_t upgrade_settings_len);

/* *****************************************************************************
Shared state
***************************************************************************** */

#define H2_PORT "19090"

static int g_fail = 0;

static void check(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "TEST FAILED: %s\n", msg);
    g_fail = 1;
    fio_stop();
  }
}

/* frame flags */
enum { H2_FLAG_END_STREAM = 0x1, H2_FLAG_END_HEADERS = 0x4,
       H2_FLAG_PADDED = 0x8, H2_FLAG_PRIORITY = 0x20 };
/* frame types */
enum { H2_FRAME_DATA = 0, H2_FRAME_HEADERS = 1, H2_FRAME_PRIORITY = 2,
       H2_FRAME_RST_STREAM = 3, H2_FRAME_SETTINGS = 4, H2_FRAME_PUSH_PROMISE = 5,
       H2_FRAME_PING = 6, H2_FRAME_GOAWAY = 7, H2_FRAME_WINDOW_UPDATE = 8,
       H2_FRAME_CONTINUATION = 9 };
static const char H2_PREFACE[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

static void be32(uint8_t *dest, uint32_t v) {
  dest[0] = (uint8_t)(v >> 24);
  dest[1] = (uint8_t)(v >> 16);
  dest[2] = (uint8_t)(v >> 8);
  dest[3] = (uint8_t)v;
}

/* *****************************************************************************
Server
***************************************************************************** */

static http_settings_s g_settings;
static int g_requests = 0;

static void server_on_request(http_s *h) {
  ++g_requests;
  h->status = 200;
  fio_str_info_s path = fiobj_obj2cstr(h->path);
  if (path.len == 6 && !memcmp(path.data, "/hello", 6)) {
    /* server push (RFC 9113 §8.4) */
    check(http_push_data(h, "pushed!", 7,
                         fiobj_str_new("text/plain", 10)) == 0,
          "server push failed");
    http_send_body(h, "hello world", 11);
  } else if (path.len == 5 && !memcmp(path.data, "/echo", 5)) {
    fio_str_info_s body = fiobj_obj2cstr(h->body);
    http_send_body(h, (void *)(body.data ? body.data : ""), body.len);
  } else if (path.len == 6 && !memcmp(path.data, "/large", 6)) {
    static uint8_t big[200000];
    static uint8_t inited = 0;
    if (!inited) {
      for (size_t i = 0; i < sizeof(big); ++i)
        big[i] = (uint8_t)(i % 251);
      inited = 1;
    }
    http_send_body(h, big, sizeof(big));
  } else {
    http_send_body(h, "other", 5);
  }
}

static void server_on_open(intptr_t uuid, void *udata) {
  (void)udata;
  http2_new(uuid, &g_settings, NULL, 0, NULL, 0);
}

/* *****************************************************************************
Client
***************************************************************************** */

typedef struct client_s client_s;

struct client_s {
  fio_protocol_s protocol;
  intptr_t uuid;
  uint8_t *buf;
  size_t buf_len;
  size_t buf_cap;
  int64_t send_window;    /* connection send window */
  int64_t stream_window;  /* stream 3 (POST) send window */
  uint8_t body[70000];    /* stream 3 request body */
  size_t body_len;
  size_t body_sent;
  uint32_t last_stream;
  hpack_context_s hpack; /* client encoder */
  hpack_context_s hdec;  /* client decoder (responses) */
  http2_parser_s parser;
  int status1, status3, status_large;
  uint8_t resp3[70000];
  size_t resp3_len;
  uint8_t large[200000];
  size_t large_len;
  int done1, done3, done_large;
  int upgrade_mode; /* h2c upgrade test: send the HTTP/1.1 request first */
  int got_101;      /* h2c upgrade test: saw the 101 response headers */
  uint8_t hblock[8192]; /* response header block accumulation */
  size_t hblock_len;
  int hblocking;
  uint32_t push_stream;  /* server push: the promised stream id (0 = none) */
  int push_done;         /* server push: the pushed response was verified */
  int push_status;
  uint8_t push_body[64];
  size_t push_body_len;
};

static client_s g_client;

/* protocol negotiation test clients (http_listen: HTTP/2 prior knowledge and
 * the h2c upgrade, RFC 9113 §3.2-3.3) */
static client_s g_neg_prior;   /* prior knowledge: preface sent directly */
static client_s g_neg_upgrade; /* h2c upgrade: HTTP/1.1 upgrade request */

/* h2c upgrade adversarial cases (RFC 9113 §3.2) */
static int g_adv_done = 0;
static int g_adv_total = 7;

/* sends a raw frame (9 byte header + payload) */
static void client_emit(client_s *cl, uint8_t type, uint8_t flags,
                        uint32_t stream, const uint8_t *payload,
                        size_t len) {
  uint8_t *frame = (uint8_t *)fio_malloc(9 + len);
  frame[0] = (uint8_t)(len >> 16);
  frame[1] = (uint8_t)(len >> 8);
  frame[2] = (uint8_t)len;
  frame[3] = type;
  frame[4] = flags;
  be32(frame + 5, stream);
  if (len)
    memcpy(frame + 9, payload, len);
  fio_write2(cl->uuid, .data.buffer = frame, .length = 9 + len,
             .after.dealloc = fio_free);
}

static void client_emit_preface(client_s *cl) {
  fio_write2(cl->uuid, .data.buffer = (void *)H2_PREFACE,
             .length = 24, .after.dealloc = FIO_DEALLOC_NOOP);
  client_emit(cl, H2_FRAME_SETTINGS, 0, 0, NULL, 0);
}

/* sends a request HEADERS block on the given stream */
static void client_send_headers(client_s *cl, uint32_t stream,
                                const char *method, const char *path) {
  static const char *const scheme = "http";
  hpack_header_s fields[5];
  fields[0].name.data = ":method";
  fields[0].name.len = 7;
  fields[0].value.data = (char *)method;
  fields[0].value.len = strlen(method);
  fields[1].name.data = ":path";
  fields[1].name.len = 5;
  fields[1].value.data = (char *)path;
  fields[1].value.len = strlen(path);
  fields[2].name.data = ":scheme";
  fields[2].name.len = 7;
  fields[2].value.data = (char *)scheme;
  fields[2].value.len = 4;
  fields[3].name.data = ":authority";
  fields[3].name.len = 10;
  fields[3].value.data = (char *)"test.local";
  fields[3].value.len = 10;
  fields[4].name.data = "x-test";
  fields[4].name.len = 6;
  fields[4].value.data = (char *)"42";
  fields[4].value.len = 2;
  uint8_t block[2048];
  size_t used = 0;
  ssize_t blen =
      hpack_context_encode(&cl->hpack, block, sizeof(block), fields, 5, &used);
  check(blen >= 0, "hpack encode failed");
  client_emit(cl, H2_FRAME_HEADERS,
              H2_FLAG_END_HEADERS |
                  (strcmp(method, "GET") ? 0 : H2_FLAG_END_STREAM),
              stream, block, (size_t)blen);
}

static void client_flush_body(client_s *cl) {
  while (cl->body_sent < cl->body_len) {
    size_t max = cl->send_window < cl->stream_window
                     ? (size_t)cl->send_window
                     : (size_t)cl->stream_window;
    if (max > 16384)
      max = 16384; /* the server's SETTINGS_MAX_FRAME_SIZE */
    size_t remaining = cl->body_len - cl->body_sent;
    if (max > remaining)
      max = remaining;
    if (!max)
      break;
    int es = (cl->body_sent + max == cl->body_len);
    client_emit(cl, H2_FRAME_DATA, es ? H2_FLAG_END_STREAM : 0, 3,
                cl->body + cl->body_sent, max);
    cl->send_window -= (int64_t)max;
    cl->stream_window -= (int64_t)max;
    cl->body_sent += max;
  }
}

/* response header block accumulation + decoding */
static void client_decode_headers(client_s *cl, uint32_t stream) {
  hpack_header_s fields[100];
  size_t count = 0;
  int rc = hpack_context_decode(&cl->hdec, cl->hblock, cl->hblock_len, fields,
                                100, &count);
  check(rc == 0, "hpack decode failed");
  uint32_t status = 0;
  size_t content_length = 0;
  for (size_t i = 0; i < count; ++i) {
    if (fields[i].name.len == 7 && !memcmp(fields[i].name.data, ":status", 7)) {
      status = 0;
      for (size_t j = 0; j < fields[i].value.len; ++j)
        status = status * 10 + (uint32_t)(fields[i].value.data[j] - '0');
    } else if (fields[i].name.len == 14 &&
               !memcmp(fields[i].name.data, "content-length", 14)) {
      content_length = 0;
      for (size_t j = 0; j < fields[i].value.len; ++j)
        content_length =
            content_length * 10 + (uint32_t)(fields[i].value.data[j] - '0');
    }
  }
  if (stream == 1) {
    cl->status1 = (int)status;
    check(status == 200, "unexpected status for stream 1");
    check(content_length == 11, "unexpected content-length for stream 1");
    cl->done1 = 1;
  } else if (stream == 3) {
    cl->status3 = (int)status;
    check(status == 200, "unexpected status for stream 3");
  } else if (stream == 5) {
    cl->status_large = (int)status;
    check(status == 200, "unexpected status for stream 5");
    check(content_length == 200000, "unexpected content-length for /large");
  } else if (stream == cl->push_stream) {
    /* the pushed response (server push) */
    cl->push_status = (int)status;
    check(status == 200, "unexpected status for pushed response");
    check(content_length == 7, "unexpected content-length for pushed body");
  }
  cl->hblock_len = 0;
  cl->hblocking = 0;
}

static void maybe_all_done(void);

static void client_maybe_done(client_s *cl) {
  if (cl->done1 && cl->done3 && cl->done_large) {
    check(cl->resp3_len == 70000,
          "echo body length mismatch (stream 3)");
    for (size_t i = 0; i < cl->resp3_len; ++i) {
      if (cl->resp3[i] != cl->body[i]) {
        check(0, "echo body mismatch (stream 3)");
        break;
      }
    }
    check(cl->large_len == 200000, "large body length mismatch (stream 5)");
    for (size_t i = 0; i < cl->large_len; ++i) {
      if (cl->large[i] != (uint8_t)(i % 251)) {
        check(0, "large body mismatch (stream 5)");
        break;
      }
    }
    check(g_requests >= 3, "server did not receive all requests");
    fprintf(stderr, "  stream 1 status=%d\n", cl->status1);
    fprintf(stderr, "  stream 3 status=%d echo=%zu bytes\n", cl->status3,
            cl->resp3_len);
    fprintf(stderr, "  stream 5 status=%d large=%zu bytes\n", cl->status_large,
            cl->large_len);
    maybe_all_done();
  }
}

/* ends the test when every connection has completed */
static void maybe_all_done(void) {
  if (!(g_client.done1 && g_client.done3 && g_client.done_large))
    return;
  if (g_neg_prior.done1 && g_neg_upgrade.done1 && g_client.push_done &&
      g_neg_prior.push_done && g_neg_upgrade.push_done &&
      g_adv_done == g_adv_total) {
    check(g_requests >= 5, "server did not receive all requests");
    if (!g_fail)
      fprintf(stderr, "* HTTP/2 protocol tests complete.\n");
    fio_stop();
    exit(0);
  }
}

/* parser callbacks (required by http2_parser.h) */
static int http2_on_error(http2_parser_s *parser) {
  (void)parser;
  check(0, "client frame parse error");
  return 1;
}

static int http2_on_frame(http2_parser_s *parser, uint32_t length, uint8_t type,
                          uint8_t flags, uint32_t stream, uint8_t *payload) {
  client_s *cl = (client_s *)((uint8_t *)parser -
                              offsetof(client_s, parser));
  switch (type) {
  case H2_FRAME_SETTINGS:
    if (flags & 1)
      break; /* SETTINGS ACK - nothing to do */
    /* acknowledge the server SETTINGS (optional, but polite) */
    client_emit(cl, H2_FRAME_SETTINGS, 1, 0, NULL, 0);
    break;
  case H2_FRAME_WINDOW_UPDATE: {
    uint32_t inc = ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
                   ((uint32_t)payload[2] << 8) | (uint32_t)payload[3];
    if (stream == 0)
      cl->send_window += (int64_t)inc;
    else if (stream == 3)
      cl->stream_window += (int64_t)inc;
    client_flush_body(cl);
    break;
  }
  case H2_FRAME_HEADERS:
    if (flags & H2_FLAG_END_HEADERS) {
      cl->hblock_len = 0;
      cl->hblocking = 0;
      if (length) {
        memcpy(cl->hblock, payload, length);
        cl->hblock_len = length;
      }
      client_decode_headers(cl, stream);
    } else {
      cl->hblock_len = 0;
      cl->hblocking = 1;
      if (length) {
        memcpy(cl->hblock, payload, length);
        cl->hblock_len = length;
      }
    }
    break;
  case H2_FRAME_CONTINUATION:
    if (length) {
      if (cl->hblock_len + length <= sizeof(cl->hblock)) {
        memcpy(cl->hblock + cl->hblock_len, payload, length);
        cl->hblock_len += length;
      } else {
        check(0, "client header block too large");
      }
    } else {
      check(cl->hblock_len, "client empty CONTINUATION");
    }
    if (flags & H2_FLAG_END_HEADERS)
      client_decode_headers(cl, stream);
    break;
  case H2_FRAME_PUSH_PROMISE: {
    /* the promised request header block (RFC 9113 §6.6) */
    check(length >= 4, "client PUSH_PROMISE too short");
    check(flags & H2_FLAG_END_HEADERS,
          "client PUSH_PROMISE split across frames (unexpected)");
    uint32_t promised =
        ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
        ((uint32_t)payload[2] << 8) | (uint32_t)payload[3];
    check(!(promised & 1), "pushed stream id must be even");
    cl->push_stream = promised;
    hpack_header_s fields[100];
    size_t count = 0;
    int rc = hpack_context_decode(&cl->hdec, payload + 4, length - 4, fields,
                                  100, &count);
    check(rc == 0, "PUSH_PROMISE hpack decode failed");
    check(count >= 1 && fields[0].name.len == 7 &&
              !memcmp(fields[0].name.data, ":method", 7) &&
              fields[0].value.len == 3 &&
              !memcmp(fields[0].value.data, "GET", 3),
          "PUSH_PROMISE must promise a GET request");
    int saw_path = 0;
    for (size_t i = 0; i < count; ++i) {
      if (fields[i].name.len == 5 &&
          !memcmp(fields[i].name.data, ":path", 5) &&
          fields[i].value.len == 6 &&
          !memcmp(fields[i].value.data, "/hello", 6))
        saw_path = 1;
    }
    check(saw_path, "PUSH_PROMISE path mismatch");
    break;
  }
  case H2_FRAME_DATA: {
    size_t l = length;
    if (l) {
      if (stream == 3) {
        if (cl->resp3_len + l <= sizeof(cl->resp3)) {
          memcpy(cl->resp3 + cl->resp3_len, payload, l);
          cl->resp3_len += l;
        } else {
          check(0, "stream 3 body overflow");
        }
      } else if (stream == 5) {
        if (cl->large_len + l <= sizeof(cl->large)) {
          memcpy(cl->large + cl->large_len, payload, l);
          cl->large_len += l;
        } else {
          check(0, "stream 5 body overflow");
        }
      } else if (stream == cl->push_stream) {
        if (cl->push_body_len + l <= sizeof(cl->push_body)) {
          memcpy(cl->push_body + cl->push_body_len, payload, l);
          cl->push_body_len += l;
        } else {
          check(0, "pushed body overflow");
        }
      }
    }
    /* replenish the receive windows */
    if (l) {
      uint8_t wu[4];
      be32(wu, (uint32_t)l);
      client_emit(cl, H2_FRAME_WINDOW_UPDATE, 0, stream, wu, 4);
      client_emit(cl, H2_FRAME_WINDOW_UPDATE, 0, 0, wu, 4);
    }
    if (flags & H2_FLAG_END_STREAM) {
      if (stream == 3)
        cl->done3 = 1;
      else if (stream == 5)
        cl->done_large = 1;
      else if (stream == cl->push_stream) {
        check(cl->push_body_len == 7 && !memcmp(cl->push_body, "pushed!", 7),
              "pushed body mismatch");
        cl->push_done = 1;
        maybe_all_done();
      }
      client_maybe_done(cl);
    }
    break;
  }
  case H2_FRAME_RST_STREAM:
    check(0, "unexpected RST_STREAM");
    break;
  case H2_FRAME_GOAWAY:
    check(0, "unexpected GOAWAY");
    break;
  default:
    break;
  }
  return 0;
}

/* fio protocol callbacks */
static void client_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  client_s *cl = (client_s *)protocol;
  if (!cl->buf) {
    cl->buf_cap = (1 << 16);
    cl->buf = (uint8_t *)fio_malloc(cl->buf_cap);
    FIO_ASSERT_ALLOC(cl->buf);
  }
  if (cl->buf_len == cl->buf_cap) {
    cl->buf_cap <<= 1;
    cl->buf = (uint8_t *)fio_realloc(cl->buf, cl->buf_cap);
    FIO_ASSERT_ALLOC(cl->buf);
  }
  ssize_t i = fio_read(uuid, cl->buf + cl->buf_len, cl->buf_cap - cl->buf_len);
  if (i > 0)
    cl->buf_len += (size_t)i;
  if (!cl->buf_len)
    return;
  if (cl->upgrade_mode && !cl->got_101) {
    /* h2c upgrade test: skip the HTTP/1.1 101 response headers */
    for (size_t j = 0; j + 3 < cl->buf_len; ++j) {
      if (cl->buf[j] == '\r' && cl->buf[j + 1] == '\n' &&
          cl->buf[j + 2] == '\r' && cl->buf[j + 3] == '\n') {
        check(j >= 12 && !memcmp(cl->buf, "HTTP/1.1 101", 12),
              "h2c upgrade: expected a 101 response");
        cl->got_101 = 1;
        size_t consumed = j + 4;
        memmove(cl->buf, cl->buf + consumed, cl->buf_len - consumed);
        cl->buf_len -= consumed;
        break;
      }
    }
    if (!cl->got_101)
      return; /* wait for the complete 101 response */
  }
  size_t used = http2_parse(&cl->parser, cl->buf, cl->buf_len);
  if (used) {
    memmove(cl->buf, cl->buf + used, cl->buf_len - used);
    cl->buf_len -= used;
  }
}

static void client_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  client_s *cl = (client_s *)protocol;
  hpack_context_destroy(&cl->hpack);
  hpack_context_destroy(&cl->hdec);
  fio_free(cl->buf);
  cl->buf = NULL;
}

static void client_on_connect(intptr_t uuid, void *udata) {
  (void)udata;
  client_s *cl = &g_client;
  cl->protocol = (fio_protocol_s){
      .on_data = client_on_data,
      .on_close = client_on_close,
  };
  cl->send_window = 65535;
  cl->stream_window = 65535;
  cl->body_len = sizeof(cl->body);
  for (size_t i = 0; i < cl->body_len; ++i)
    cl->body[i] = (uint8_t)(i % 251);
  hpack_context_init(&cl->hpack, 4096);
  hpack_context_init(&cl->hdec, 4096);
  fio_attach(uuid, &cl->protocol);
  cl->uuid = uuid;
  client_emit_preface(cl);
  client_send_headers(cl, 1, "GET", "/hello");
  client_send_headers(cl, 3, "POST", "/echo");
  client_flush_body(cl);
  client_send_headers(cl, 5, "GET", "/large");
}

static void client_on_fail(intptr_t uuid, void *udata) {
  (void)uuid;
  (void)udata;
  check(0, "client connection failed");
}

/* *****************************************************************************
Protocol negotiation tests (http_listen: HTTP/2 prior knowledge and the
h2c upgrade, RFC 9113 §3.2-3.3)
***************************************************************************** */

#define NEG_PORT "19091"

static void neg_client_init(client_s *cl) {
  cl->protocol = (fio_protocol_s){
      .on_data = client_on_data,
      .on_close = client_on_close,
  };
  cl->send_window = 65535;
  cl->stream_window = 65535;
  hpack_context_init(&cl->hpack, 4096);
  hpack_context_init(&cl->hdec, 4096);
}

static void neg_on_connect_prior(intptr_t uuid, void *udata) {
  (void)udata;
  client_s *cl = &g_neg_prior;
  neg_client_init(cl);
  fio_attach(uuid, &cl->protocol);
  cl->uuid = uuid;
  client_emit_preface(cl);
  client_send_headers(cl, 1, "GET", "/hello");
}

static void neg_on_connect_upgrade(intptr_t uuid, void *udata) {
  static const char neg_upgrade_req[] =
      "GET /hello HTTP/1.1\r\n"
      "Host: test.local\r\n"
      "Connection: Upgrade, HTTP2-Settings\r\n"
      "Upgrade: h2c\r\n"
      "HTTP2-Settings: AAEAAABk\r\n"
      "\r\n";
  (void)udata;
  client_s *cl = &g_neg_upgrade;
  neg_client_init(cl);
  cl->upgrade_mode = 1;
  fio_attach(uuid, &cl->protocol);
  cl->uuid = uuid;
  fio_write2(uuid, .data.buffer = (void *)neg_upgrade_req,
             .length = sizeof(neg_upgrade_req) - 1,
             .after.dealloc = FIO_DEALLOC_NOOP);
  client_emit_preface(cl);
  client_send_headers(cl, 1, "GET", "/hello");
}

static void neg_on_fail(intptr_t uuid, void *udata) {
  (void)uuid;
  (void)udata;
  check(0, "negotiation client connection failed");
}

/* *****************************************************************************
h2c upgrade adversarial cases (RFC 9113 §3.2): the HTTP/1.1 upgrade request is
exercised with fragmentation, a body, multiple Upgrade tokens, malformed or
duplicate HTTP2-Settings, invalid settings values and leftover bytes.
***************************************************************************** */

typedef struct adv_case_s adv_case_s;
struct adv_case_s {
  fio_protocol_s protocol;
  intptr_t uuid;
  uint8_t buf[4096];
  size_t buf_len;
  const char *part1; /* first write (fragmenting the HTTP/1.1 request) */
  size_t part1_len;
  const char *part2; /* second write: rest of the request (+ preface/frames) */
  size_t part2_len;
  int part2_alloc;  /* part2 was malloc'd - free it on close */
  int expect_status; /* expected HTTP/1.1 response status */
  int id;            /* case id for diagnostics */
  int done;
};

static adv_case_s g_adv_frag, g_adv_body, g_adv_tokens, g_adv_bad64,
    g_adv_badpush, g_adv_badwin, g_adv_dup;


static void adv_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  adv_case_s *c = (adv_case_s *)protocol;
  ssize_t i = fio_read(uuid, c->buf + c->buf_len, sizeof(c->buf) - c->buf_len);
  if (i > 0) {
    c->buf_len += (size_t)i;
  }
  if (!c->buf_len || c->done)
    return;
  for (size_t j = 0; j + 11 < c->buf_len; ++j) {
    if (!memcmp(c->buf + j, "HTTP/1.1 ", 9)) {
      int status = 0;
      for (int k = 0; k < 3; ++k)
        status = status * 10 + c->buf[j + 9 + k] - '0';
      check(status == c->expect_status, "h2c adversarial: unexpected status");
      if (status != c->expect_status)
        fprintf(stderr, "  (case %d expected %d got %d, buf_len=%zu)\n",
                c->id, c->expect_status, status, c->buf_len);
      c->done = 1;
      ++g_adv_done;
      maybe_all_done();
      return;
    }
  }
}

static void adv_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  (void)uuid;
  adv_case_s *c = (adv_case_s *)protocol;
  if (c->part2_alloc) {
    fio_free((void *)c->part2);
    c->part2 = NULL;
  }
}

static void adv_on_connect(intptr_t uuid, void *udata) {
  adv_case_s *c = (adv_case_s *)udata;
  c->protocol = (fio_protocol_s){
      .on_data = adv_on_data,
      .on_close = adv_on_close,
  };
  fio_attach(uuid, &c->protocol);
  c->uuid = uuid;
  if (c->part1_len)
    fio_write2(uuid, .data.buffer = (void *)c->part1,
               .length = c->part1_len, .after.dealloc = FIO_DEALLOC_NOOP);
  fio_write2(uuid, .data.buffer = (void *)c->part2,
             .length = c->part2_len,
             .after.dealloc = c->part2_alloc ? fio_free : FIO_DEALLOC_NOOP);
}

/* builds the tail of the fragmented upgrade request: the second half of the
 * HTTP2-Settings header line, the request terminator, the connection preface
 * and a complete HEADERS frame - all in a single write (leftover bytes). */
static uint8_t *adv_build_frag_tail(size_t *len) {
  static const char tail[] = "AABk\r\n\r\n";
  hpack_context_s enc;
  hpack_context_init(&enc, 4096);
  static const hpack_header_s fields[] = {
      {.name = {.data = ":method", .len = 7},
       .value = {.data = "GET", .len = 3}},
      {.name = {.data = ":path", .len = 5},
       .value = {.data = "/hello", .len = 6}},
      {.name = {.data = ":scheme", .len = 7},
       .value = {.data = "http", .len = 4}},
      {.name = {.data = ":authority", .len = 10},
       .value = {.data = "test.local", .len = 10}},
      {.name = {.data = "x-test", .len = 6}, .value = {.data = "42", .len = 2}},
  };
  uint8_t block[2048];
  size_t used = 0;
  ssize_t blen = hpack_context_encode(&enc, block, sizeof(block), fields, 5,
                                      &used);
  hpack_context_destroy(&enc);
  check(blen >= 0, "adv frag: hpack encode failed");
  size_t tlen =
      sizeof(tail) - 1 + 24 + 9 + (size_t)blen;
  uint8_t *out = (uint8_t *)fio_malloc(tlen);
  FIO_ASSERT_ALLOC(out);
  size_t pos = 0;
  memcpy(out + pos, tail, sizeof(tail) - 1);
  pos += sizeof(tail) - 1;
  memcpy(out + pos, H2_PREFACE, 24);
  pos += 24;
  out[pos + 0] = (uint8_t)(blen >> 16);
  out[pos + 1] = (uint8_t)(blen >> 8);
  out[pos + 2] = (uint8_t)blen;
  out[pos + 3] = H2_FRAME_HEADERS;
  out[pos + 4] = H2_FLAG_END_HEADERS | H2_FLAG_END_STREAM;
  be32(out + pos + 5, 1);
  memcpy(out + pos + 9, block, (size_t)blen);
  pos += 9 + (size_t)blen;
  *len = pos;
  return out;
}

static void adv_register_cases(void) {
  g_adv_frag.part1 = "GET /hello HTTP/1.1\r\n"
                     "Host: test.local\r\n"
                     "Connection: Upgrade, HTTP2-Settings\r\n"
                     "Upgrade: h2c\r\n"
                     "HTTP2-Settings: AAEA";
  g_adv_frag.part1_len = strlen(g_adv_frag.part1);
  g_adv_frag.part2 = (const char *)adv_build_frag_tail(&g_adv_frag.part2_len);
  g_adv_frag.part2_alloc = 1;
  g_adv_frag.expect_status = 101;
  g_adv_frag.id = 1;

  /* an upgrade request with a body must not be upgraded (RFC 9113 §3.2) */
  g_adv_body.part1 = "POST /c2 HTTP/1.1\r\n"
                     "Host: test.local\r\n"
                     "Connection: Upgrade, HTTP2-Settings\r\n"
                     "Upgrade: h2c\r\n"
                     "HTTP2-Settings: AAEAAABk\r\n"
                     "Content-Length: 5\r\n"
                     "\r\n"
                     "hello";
  g_adv_body.part1_len = strlen(g_adv_body.part1);
  g_adv_body.expect_status = 400;
  g_adv_body.id = 2;

  /* a multi-token Upgrade header must not match a plain h2c upgrade */
  g_adv_tokens.part1 = "GET /c3 HTTP/1.1\r\n"
                       "Host: test.local\r\n"
                       "Connection: Upgrade, HTTP2-Settings\r\n"
                       "Upgrade: h2c, h2\r\n"
                       "HTTP2-Settings: AAEAAABk\r\n"
                       "\r\n";
  g_adv_tokens.part1_len = strlen(g_adv_tokens.part1);
  g_adv_tokens.expect_status = 400;
  g_adv_tokens.id = 3;

  /* malformed Base64URL in HTTP2-Settings must be a 400 */
  g_adv_bad64.part1 = "GET /hello HTTP/1.1\r\n"
                      "Host: test.local\r\n"
                      "Connection: Upgrade, HTTP2-Settings\r\n"
                      "Upgrade: h2c\r\n"
                      "HTTP2-Settings: !!!\r\n"
                      "\r\n";
  g_adv_bad64.part1_len = strlen(g_adv_bad64.part1);
  g_adv_bad64.expect_status = 400;
  g_adv_bad64.id = 4;

  /* invalid ENABLE_PUSH=2 in the settings payload must be a 400 */
  g_adv_badpush.part1 = "GET /hello HTTP/1.1\r\n"
                        "Host: test.local\r\n"
                        "Connection: Upgrade, HTTP2-Settings\r\n"
                        "Upgrade: h2c\r\n"
                        "HTTP2-Settings: AAIAAAACAAEAAAAA\r\n"
                        "\r\n";
  g_adv_badpush.part1_len = strlen(g_adv_badpush.part1);
  g_adv_badpush.expect_status = 400;
  g_adv_badpush.id = 5;

  /* an out-of-range INITIAL_WINDOW_SIZE in the settings must be a 400 */
  g_adv_badwin.part1 = "GET /hello HTTP/1.1\r\n"
                       "Host: test.local\r\n"
                       "Connection: Upgrade, HTTP2-Settings\r\n"
                       "Upgrade: h2c\r\n"
                       "HTTP2-Settings: AASAAAAA\r\n"
                       "\r\n";
  g_adv_badwin.part1_len = strlen(g_adv_badwin.part1);
  g_adv_badwin.expect_status = 400;
  g_adv_badwin.id = 6;

  /* duplicate HTTP2-Settings (both invalid) must not upgrade - a 400 */
  g_adv_dup.part1 = "GET /hello HTTP/1.1\r\n"
                    "Host: test.local\r\n"
                    "Connection: Upgrade, HTTP2-Settings\r\n"
                    "Upgrade: h2c\r\n"
                    "HTTP2-Settings: !!!!\r\n"
                    "HTTP2-Settings: !!!!\r\n"
                    "\r\n";
  g_adv_dup.part1_len = strlen(g_adv_dup.part1);
  g_adv_dup.expect_status = 400;
  g_adv_dup.id = 7;
}

/* *****************************************************************************
main
***************************************************************************** */

/* watchdog - fail the test if it hangs */
static void *watchdog(void *arg) {
  (void)arg;
  sleep(30);
  fprintf(stderr,
          "TEST FAILED: watchdog timeout (done1=%d p1=%d p2=%d pd=%d/%d/%d adv=%d/%d req=%d)\n",
          g_client.done1, g_neg_prior.done1, g_neg_upgrade.done1,
          g_client.push_done, g_neg_prior.push_done, g_neg_upgrade.push_done,
          g_adv_done, g_adv_total, g_requests);
  exit(1);
  return NULL;
}

int main(void) {
  pthread_t wd;
  pthread_create(&wd, NULL, watchdog, NULL);
  g_settings = (http_settings_s){
      .on_request = server_on_request,
      .log = 0,
  };
  g_client = (client_s){0};
  g_neg_prior = (client_s){0};
  g_neg_upgrade = (client_s){0};
  adv_register_cases();
  if (fio_listen(.port = H2_PORT, .on_open = server_on_open) == -1) {
    fprintf(stderr, "TEST FAILED: could not listen on port %s\n", H2_PORT);
    return 1;
  }
  /* the public http_listen API: HTTP/1.1 + HTTP/2 negotiation */
  if (http_listen(NEG_PORT, NULL, .on_request = server_on_request,
                  .log = 0) == -1) {
    fprintf(stderr, "TEST FAILED: could not listen on port %s\n", NEG_PORT);
    return 1;
  }
  if (fio_connect(.port = H2_PORT, .address = "127.0.0.1",
                  .on_connect = client_on_connect,
                  .on_fail = client_on_fail) == -1) {
    fprintf(stderr, "TEST FAILED: could not connect\n");
    return 1;
  }
  if (fio_connect(.port = NEG_PORT, .address = "127.0.0.1",
                  .on_connect = neg_on_connect_prior,
                  .on_fail = neg_on_fail) == -1) {
    fprintf(stderr, "TEST FAILED: could not connect\n");
    return 1;
  }
if (fio_connect(.port = NEG_PORT, .address = "127.0.0.1",
                  .on_connect = neg_on_connect_upgrade,
                  .on_fail = neg_on_fail) == -1) {
    fprintf(stderr, "TEST FAILED: could not connect\n");
    return 1;
  }
  adv_case_s *adv_cases[] = {&g_adv_frag, &g_adv_body, &g_adv_tokens,
                             &g_adv_bad64, &g_adv_badpush, &g_adv_badwin,
                             &g_adv_dup};
  for (int ai = 0; ai < g_adv_total; ++ai) {
    if (fio_connect(.port = NEG_PORT, .address = "127.0.0.1",
                    .on_connect = adv_on_connect, .on_fail = neg_on_fail,
                    .udata = adv_cases[ai]) == -1) {
      fprintf(stderr, "TEST FAILED: could not connect\n");
      return 1;
    }
  }
  fio_start(.threads = 2, .workers = 0);
  return g_fail;
}
