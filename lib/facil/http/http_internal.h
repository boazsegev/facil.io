/*
Copyright: Boaz Segev, 2016-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_INTERNAL_H
#define H_HTTP_INTERNAL_H

#include <http.h>

#include <arpa/inet.h>
#include <errno.h>

/* *****************************************************************************
Types
***************************************************************************** */

typedef struct http_fio_protocol_s http_fio_protocol_s;
typedef struct http_vtable_s http_vtable_s;

struct http_fio_protocol_s {
  fio_protocol_s protocol;   /* facil.io protocol */
  intptr_t uuid;             /* socket uuid */
  http_settings_s *settings; /* pointer to HTTP settings */
};

#define http2protocol(h) ((http_fio_protocol_s *)h->private_data.flag)

/* *****************************************************************************
Constants that shouldn't be accessed by the users (`fiobj_dup` required).
***************************************************************************** */

extern FIOBJ HTTP_HEADER_ACCEPT_RANGES;
extern FIOBJ HTTP_HEADER_WS_SEC_CLIENT_KEY;
extern FIOBJ HTTP_HEADER_WS_SEC_KEY;
extern FIOBJ HTTP_HVALUE_BYTES;
extern FIOBJ HTTP_HVALUE_CLOSE;
extern FIOBJ HTTP_HVALUE_CONTENT_TYPE_DEFAULT;
extern FIOBJ HTTP_HVALUE_GZIP;
extern FIOBJ HTTP_HVALUE_KEEP_ALIVE;
extern FIOBJ HTTP_HVALUE_MAX_AGE;
extern FIOBJ HTTP_HVALUE_NO_CACHE;
extern FIOBJ HTTP_HVALUE_SSE_MIME;
extern FIOBJ HTTP_HVALUE_WEBSOCKET;
extern FIOBJ HTTP_HVALUE_WS_SEC_VERSION;
extern FIOBJ HTTP_HVALUE_WS_UPGRADE;
extern FIOBJ HTTP_HVALUE_WS_VERSION;

/* *****************************************************************************
HTTP request/response object management
***************************************************************************** */

typedef struct {
  http_vtable_s *vtbl;
  http_fio_protocol_s *pr;
  FIOBJ headers_out;
  size_t bytes_sent;
  http_s public;
} http_internal_s;

/** tests public handle validity */
#define HTTP2PRIVATE(h_) FIO_PTR_FROM_FIELD(http_internal_s, public, h_)
#define HTTP2PUBLIC(h_) (&h_->public)
#define HTTP_H_INIT(vtbl_, owner_)                                             \
  {                                                                            \
    .vtbl = (vtbl_), .pr = (owner_), .headers_out = fiobj_hash_new(),          \
    .public = {                                                                \
        .status = 200,                                                         \
        .received_at = fio_last_tick(),                                        \
    },                                                                         \
  }

#define HTTP_S_INVALID(h)                                                      \
  (!(h) || HTTP2PRIVATE(h)->headers_out == FIOBJ_INVALID)

static inline void http_h_destroy(http_internal_s *h, uint8_t log) {
  if (log && h->public.status && !h->public.status_str) {
    http_write_log(HTTP2PUBLIC(h));
  }
  fiobj_free(h->public.method);
  fiobj_free(h->public.status_str);
  fiobj_free(h->headers_out);
  fiobj_free(h->public.headers);
  fiobj_free(h->public.version);
  fiobj_free(h->public.query);
  fiobj_free(h->public.path);
  fiobj_free(h->public.cookies);
  fiobj_free(h->public.body);
  fiobj_free(h->public.params);

  *h = (http_internal_s){
      .vtbl = h->vtbl,
      .pr = h->pr,
  };
}

static inline void http_h_clear(http_internal_s *h, uint8_t log) {
  http_h_destroy(h, log);
  *h = (http_internal_s)HTTP_H_INIT(h->vtbl, h->pr);
}

/* *****************************************************************************
Virtual Function Table for HTTP Protocols
***************************************************************************** */

struct http_vtable_s {
  /** Should send existing headers and data */
  int (*const send_body)(http_internal_s *h, void *data, uintptr_t length);
  /** Should send existing headers and file */
  int (*const sendfile)(http_internal_s *h, int fd, uintptr_t length,
                        uintptr_t offset);
  /** Should send existing headers and data and prepare for streaming */
  int (*const stream)(http_internal_s *h, void *data, uintptr_t length);
  /** Should send existing headers or complete streaming */
  void (*const finish)(http_internal_s *h);
  /** Push for data. */
  int (*const push_data)(http_internal_s *h, void *data, uintptr_t length,
                         FIOBJ mime_type);
  /** Upgrades a connection to Websockets. */
  int (*const http2websocket)(http_internal_s *h, websocket_settings_s *arg);
  /** Push for files. */
  int (*const push_file)(http_internal_s *h, FIOBJ filename, FIOBJ mime_type);
  /** Pauses the request / response handling. */
  void (*on_pause)(http_internal_s *, http_fio_protocol_s *);

  /** Resumes a request / response handling. */
  void (*on_resume)(http_internal_s *, http_fio_protocol_s *);
  /** hijacks the socket aaway from the protocol. */
  intptr_t (*hijack)(http_internal_s *h, fio_str_info_s *leftover);

  /** Upgrades an HTTP connection to an EventSource (SSE) connection. */
  int (*upgrade2sse)(http_internal_s *h, http_sse_s *sse);
  /** Writes data to an EventSource (SSE) connection. MUST free the FIOBJ. */
  int (*sse_write)(http_sse_s *sse, FIOBJ str);
  /** Closes an EventSource (SSE) connection. */
  int (*sse_close)(http_sse_s *sse);
};

/* *****************************************************************************
Request / Response Handlers
***************************************************************************** */

/** Use this function to handle HTTP requests.*/
void http_on_request_handler______internal(http_s *h,
                                           http_settings_s *settings);

void http_on_response_handler______internal(http_s *h,
                                            http_settings_s *settings);
int http_send_error2(size_t error, intptr_t uuid, http_settings_s *settings);

/* *****************************************************************************
EventSource Support (SSE)
***************************************************************************** */

typedef struct http_sse_internal_s {
  http_sse_s sse;        /* the user SSE settings */
  intptr_t uuid;         /* the socket's uuid */
  http_vtable_s *vtable; /* the protocol's vtable */
  uintptr_t id;          /* the SSE identifier */
  size_t ref;            /* reference count */
} http_sse_internal_s;

static inline void http_sse_init(http_sse_internal_s *sse, intptr_t uuid,
                                 http_vtable_s *vtbl, http_sse_s *args) {
  *sse = (http_sse_internal_s){
      .sse = *args,
      .uuid = uuid,
      .vtable = vtbl,
      .ref = 1,
  };
}

static inline void http_sse_try_free(http_sse_internal_s *sse) {
  if (fio_atomic_sub(&sse->ref, 1))
    return;
  fio_free(sse);
}

static inline void http_sse_destroy(http_sse_internal_s *sse) {
  if (sse->sse.on_close)
    sse->sse.on_close(&sse->sse);
  sse->uuid = -1;
  http_sse_try_free(sse);
}

/* *****************************************************************************
Helpers
***************************************************************************** */

/** sets an outgoing header only if it doesn't exist */
static inline void set_header_if_missing(FIOBJ hash, FIOBJ name, FIOBJ value) {
  FIOBJ old = FIOBJ_INVALID;
  const uintptr_t name_hash = fiobj2hash(hash, name);
  /* look up hash only once (if object doesn't exist) */
  fiobj_hash_set(hash, name_hash, name, value, &old);
  if (!old)
    return;
  fiobj_hash_set(hash, name_hash, name, old, NULL);
}

/** sets an outgoing header, collecting duplicates in an Array (i.e. cookies) */
static inline void set_header_add(FIOBJ hash, FIOBJ name, FIOBJ value) {
  FIOBJ old = FIOBJ_INVALID;
  const uintptr_t name_hash = fiobj2hash(hash, name);
  if (value == FIOBJ_INVALID)
    goto remove_val;
  fiobj_hash_set(hash, name_hash, name, value, &old);
  if (!old)
    return;
  if (!FIOBJ_TYPE_IS(old, FIOBJ_T_ARRAY)) {
    FIOBJ tmp = fiobj_array_new();
    fiobj_array_push(tmp, old);
    old = tmp;
  }
  if (FIOBJ_TYPE_IS(value, FIOBJ_T_ARRAY)) {
    fiobj_array_concat(old, value);
    /* frees `value` */
    fiobj_hash_set(hash, name_hash, name, old, NULL);
    return;
  }
  /* value will be owned by both hash and array */
  fiobj_array_push(old, value);
  /* don't free `value` (leave in array) */
  fiobj_hash_set(hash, name_hash, name, old, &value);
  return;
remove_val:
  fiobj_hash_remove(hash, name_hash, name, NULL);
}

#endif /* H_HTTP_INTERNAL_H */
