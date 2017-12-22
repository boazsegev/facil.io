/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_INTERNAL_H
#define H_HTTP_INTERNAL_H

#include "http.h"

#include <errno.h>
typedef struct http_protocol_s http_protocol_s;
typedef struct http_vtable_s http_vtable_s;

struct http_vtable_s {
  /** Should send existing headers and data */
  int (*http_send_body)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers and file */
  int (*http_sendfile)(http_s *h, int fd, uintptr_t length, uintptr_t offset);
  /** Should send existing headers and data and prepare for streaming */
  int (*http_stream)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers or complete streaming */
  void (*http_finish)(http_s *h);
  /** Push for data. */
  int (*http_push_data)(http_s *h, void *data, uintptr_t length,
                        char *mime_type, uintptr_t type_length);
  /** Push for files. */
  int (*http_push_file)(http_s *h, char *filename, size_t name_length,
                        char *mime_type, uintptr_t type_length);
  /** Defer request handling for later... careful (memory concern apply). */
  int (*http_defer)(http_s *h, void (*task)(http_s *r));
};

struct http_protocol_s {
  protocol_s protocol;
  intptr_t uuid;
  http_settings_s *settings;
  http_vtable_s *vtable;
};

static inline void http_s_init(http_s *h, http_protocol_s *owner) {
  *h = (http_s){
      .private.owner = (protocol_s *)owner,
      .private.request_id = 1,
      .private.out_headers = fiobj_hash_new(),
      .headers = fiobj_hash_new(),
      .version = h->version,
      .received_at = facil_last_tick(),
      .status = 200,
  };
}

static inline void http_s_cleanup(http_s *h) {
  fiobj_free(h->method); /* union for   fiobj_free(r->status_str); */
  fiobj_free(h->private.out_headers);
  fiobj_free(h->headers);
  fiobj_free(h->version);
  fiobj_free(h->query);
  fiobj_free(h->path);
  fiobj_free(h->cookies);
  fiobj_free(h->body);
  fiobj_free(h->params);
  *h = (http_s){{0}};
}

/** Use this function to handle HTTP requests.*/
void http_on_request_handler______internal(http_s *h,
                                           http_settings_s *settings);

#define HTTP_ASSERT(x, m)                                                      \
  if (!x)                                                                      \
    perror("FATAL ERROR: (http)" m), exit(errno);

#endif /* H_HTTP_INTERNAL_H */
