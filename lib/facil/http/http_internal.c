/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http_internal.h"

/** Use this function to handle HTTP requests.*/
void http_on_request_handler______internal(http_s *h,
                                           http_settings_s *settings) {
  if (settings->public_folder) {

    fio_cstr_s path = fiobj_obj2cstr(h->path);
    http_sendfile2(h, path.name, path.length);
  }
  settings->on_request(h);
}

fiobj_s *HTTP_HEADER_UPGRADE;
fiobj_s *HTTP_HEADER_CONNECTION;
fiobj_s *HTTP_HEADER_DATE;
fiobj_s *HTTP_HEADER_ETAG;
fiobj_s *HTTP_HEADER_CONTENT_LENGTH;
fiobj_s *HTTP_HEADER_CONTENT_TYPE;
fiobj_s *HTTP_HEADER_LAST_MODIFIED;
fiobj_s *HTTP_HEADER_SET_COOKIE;
fiobj_s *HTTP_HEADER_COOKIE;

void http_lib_init(void) {
  HTTP_HEADER_UPGRADE = fiobj_sym_new("upgrade", 7);
  HTTP_HEADER_CONNECTION = fiobj_sym_new("connection", 10);
  HTTP_HEADER_DATE = fiobj_sym_new("date", 4);
  HTTP_HEADER_ETAG = fiobj_sym_new("etag", 4);
  HTTP_HEADER_CONTENT_LENGTH = fiobj_sym_new("content-length", 14);
  HTTP_HEADER_CONTENT_TYPE = fiobj_sym_new("content-type", 12);
  HTTP_HEADER_LAST_MODIFIED = fiobj_sym_new("last-modified", 13);
  HTTP_HEADER_SET_COOKIE = fiobj_sym_new("set-cookie", 10);
  HTTP_HEADER_COOKIE = fiobj_sym_new("cookie", 6);
}

void http_lib_cleanup(void) {
#define HTTPLIB_RESET(x)                                                       \
  fiobj_free(x);                                                               \
  x = NULL;
  HTTPLIB_RESET(HTTP_HEADER_UPGRADE);
  HTTPLIB_RESET(HTTP_HEADER_CONNECTION);
  HTTPLIB_RESET(HTTP_HEADER_DATE);
  HTTPLIB_RESET(HTTP_HEADER_ETAG);
  HTTPLIB_RESET(HTTP_HEADER_CONTENT_LENGTH);
  HTTPLIB_RESET(HTTP_HEADER_CONTENT_TYPE);
  HTTPLIB_RESET(HTTP_HEADER_LAST_MODIFIED);
  HTTPLIB_RESET(HTTP_HEADER_SET_COOKIE);
  HTTPLIB_RESET(HTTP_HEADER_COOKIE);
#undef HTTPLIB_RESET
}
