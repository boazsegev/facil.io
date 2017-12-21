/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_INTERNAL_H
#define H_HTTP_INTERNAL_H

#include "http.h"

typedef struct http_protocol_s http_protocol_s;
typedef struct http_vtable_s http_vtable_s;

struct http_vtable_s {
  int (*send_body)(http_req_s *self, void *data, uintptr_t length);
  int (*sendfile)(http_req_s *self, int fd, uintptr_t length, uintptr_t offset);
  http_req_s *(*stream)(http_req_s *self, void *data, uintptr_t length);
  void (*finish)(http_req_s *self);
  int (*http_push_data)(http_req_s *r, void *data, uintptr_t length,
                        char *mime_type, uintptr_t type_length);
  int (*http_push_file)(http_req_s *r, char *filename, size_t name_length,
                        char *mime_type, uintptr_t type_length);
};

struct http_protocol_s {
  protocol_s protocol;
  intptr_t uuid;
  http_settings_s *settings;
  http_vtable_s *vtable;
};

#endif /* H_HTTP_INTERNAL_H */
