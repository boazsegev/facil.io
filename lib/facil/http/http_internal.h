/*
Copyright: Boaz Segev, 2016-2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_INTERNAL_H
#define H_HTTP_INTERNAL_H

#include "http.h"

#include "fiobj4sock.h"

#include <arpa/inet.h>
#include <errno.h>

/* *****************************************************************************
Types
***************************************************************************** */

typedef struct http_protocol_s http_protocol_s;
typedef struct http_vtable_s http_vtable_s;

struct http_vtable_s {
  /** Should send existing headers and data */
  int (*const http_send_body)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers and file */
  int (*const http_sendfile)(http_s *h, int fd, uintptr_t length,
                             uintptr_t offset);
  /** Should send existing headers and data and prepare for streaming */
  int (*const http_stream)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers or complete streaming */
  void (*const http_finish)(http_s *h);
  /** Push for data. */
  int (*const http_push_data)(http_s *h, void *data, uintptr_t length,
                              FIOBJ mime_type);
  /** Upgrades a connection to Websockets. */
  int (*const http2websocket)(websocket_settings_s *arg);
  /** Push for files. */
  int (*const http_push_file)(http_s *h, FIOBJ filename, FIOBJ mime_type);
  /** Pauses the request / response handling. */
  void (*http_on_pause)(http_s *, http_protocol_s *);

  /** Resumes a request / response handling. */
  void (*http_on_resume)(http_s *, http_protocol_s *);
  /** hijacks the socket aaway from the protocol. */
  intptr_t (*http_hijack)(http_s *h, fio_cstr_s *leftover);
};

struct http_protocol_s {
  protocol_s protocol;
  intptr_t uuid;
  http_settings_s *settings;
};

#define http2protocol(h) ((http_protocol_s *)h->private_data.flag)

/* *****************************************************************************
Constants that shouldn't be accessed by the users (`fiobj_dup` required).
***************************************************************************** */

extern FIOBJ HTTP_HEADER_ACCEPT_RANGES;
extern FIOBJ HTTP_HEADER_WS_SEC_KEY;
extern FIOBJ HTTP_HEADER_WS_SEC_CLIENT_KEY;
extern FIOBJ HTTP_HVALUE_BYTES;
extern FIOBJ HTTP_HVALUE_CLOSE;
extern FIOBJ HTTP_HVALUE_GZIP;
extern FIOBJ HTTP_HVALUE_KEEP_ALIVE;
extern FIOBJ HTTP_HVALUE_MAX_AGE;
extern FIOBJ HTTP_HVALUE_WEBSOCKET;
extern FIOBJ HTTP_HVALUE_WS_SEC_VERSION;
extern FIOBJ HTTP_HVALUE_WS_UPGRADE;
extern FIOBJ HTTP_HVALUE_WS_VERSION;

/* *****************************************************************************
HTTP request/response object management
***************************************************************************** */

static inline void http_s_new(http_s *h, http_protocol_s *owner,
                              http_vtable_s *vtbl) {
  *h = (http_s){
      .private_data =
          {
              .vtbl = vtbl,
              .flag = (uintptr_t)owner,
              .out_headers = fiobj_hash_new(),
          },
      .headers = fiobj_hash_new(),
      .received_at = facil_last_tick(),
      .status = 200,
  };
}

static inline void http_s_clear(http_s *h, uint8_t log) {
  if (log && h->status && !h->status_str)
    http_write_log(h);
  fiobj_free(h->method);
  fiobj_free(h->status_str);
  fiobj_hash_clear(h->private_data.out_headers);
  fiobj_hash_clear(h->headers);
  fiobj_free(h->version);
  fiobj_free(h->query);
  fiobj_free(h->path);
  fiobj_free(h->cookies);
  fiobj_free(h->body);
  fiobj_free(h->params);
  *h = (http_s){
      .private_data =
          {
              .vtbl = h->private_data.vtbl,
              .flag = h->private_data.flag,
              .out_headers = h->private_data.out_headers,
          },
      .headers = h->headers,
      .received_at = facil_last_tick(),
      .status = 200,
  };
}

static inline void http_s_destroy(http_s *h, uint8_t log) {
  if (log && h->status && !h->status_str)
    http_write_log(h);
  fiobj_free(h->method);
  fiobj_free(h->status_str);
  fiobj_free(h->private_data.out_headers);
  fiobj_free(h->headers);
  fiobj_free(h->version);
  fiobj_free(h->query);
  fiobj_free(h->path);
  fiobj_free(h->cookies);
  fiobj_free(h->body);
  fiobj_free(h->params);

  *h = (http_s){.private_data.vtbl = h->private_data.vtbl};
}

/** Use this function to handle HTTP requests.*/
void http_on_request_handler______internal(http_s *h,
                                           http_settings_s *settings);

void http_on_response_handler______internal(http_s *h,
                                            http_settings_s *settings);
int http_send_error2(size_t error, intptr_t uuid, http_settings_s *settings);

/* *****************************************************************************
Helpers
***************************************************************************** */

#define HTTP_ASSERT(x, m)                                                      \
  if (!x)                                                                      \
    perror("FATAL ERROR: (http)" m), exit(errno);

/** sets an outgoing header only if it doesn't exist */
static inline void set_header_if_missing(FIOBJ hash, FIOBJ name, FIOBJ value) {
  FIOBJ old = fiobj_hash_replace(hash, name, value);
  if (!old)
    return;
  fiobj_hash_replace(hash, name, old);
  fiobj_free(value);
}

/** sets an outgoing header, collecting duplicates in an Array (i.e. cookies)
 */
static inline void set_header_add(FIOBJ hash, FIOBJ name, FIOBJ value) {
  FIOBJ old = fiobj_hash_replace(hash, name, value);
  if (!old)
    return;
  if (!value) {
    fiobj_free(old);
    return;
  }
  if (!FIOBJ_TYPE_IS(old, FIOBJ_T_ARRAY)) {
    FIOBJ tmp = fiobj_ary_new();
    fiobj_ary_push(tmp, old);
    old = tmp;
  }
  fiobj_ary_push(old, value);
  fiobj_hash_replace(hash, name, old);
}

#endif /* H_HTTP_INTERNAL_H */
