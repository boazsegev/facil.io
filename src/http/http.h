/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_COLLECTED_H
#define HTTP_COLLECTED_H

/** \file http.h
This header file simply collects all the http libraries required to start an
HTTP server and defines simple marcros to start an HTTP server using lib-server.

The file also introduces the `start_http_server` macro.
*/
#include "lib-server.h"
#include "http-response.h"
#include "http-protocol.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define HTTP_DEFAULT_TIMEOUT 5

/**
This macro allows an easy start for an HTTP server, using the lib-server library
and the HTTP library helpers.

The macro accepts variable arguments. The first two (required) arguments are the
on_request callback for the HTTP protocol and an optional (can be NULL) public
folder for serving static files. The rest of the optional (variable) arguments
are any valid `struct ServerSettings` fields.

i.e. use:

    void on_request(struct HttpRequest request) {
      ; // ...
    }

    int main()
    {
      char * public_folder = NULL
      start_http_server(on_request, public_folder, .threads = 16);
    }

To get a more detailed startup and setup customization, add an initialization
callback. i.e.:

    void on_request(struct HttpRequest request) {
      ; // ...
    }
    void on_startup(server_pt srv) {
      struct HttpProtocol * http =
                        (struct HttpProtocol*)Server.settings(srv)->protocol;
      http->maximum_body_size = 100; // 100 Mb POST data limit.
      ; // ...
    }

    int main()
    {
      char * public_folder = NULL
      start_http_server(on_request,
                        public_folder,
                        .threads = 16,
                        .on_init = on_startup);
    }
*/
#define start_http_server(on_request_callback, http_public_folder, ...)     \
  do {                                                                      \
    struct HttpProtocol* protocol = HttpProtocol.create();                  \
    if ((NULL != (void*)on_request_callback))                               \
      protocol->on_request = (on_request_callback);                         \
    char real_public_path[PATH_MAX];                                        \
    if ((http_public_folder) && ((char*)http_public_folder)[0] == '~' &&    \
        getenv("HOME") && strlen((http_public_folder)) < PATH_MAX) {        \
      strcpy(real_public_path, getenv("HOME"));                             \
      strcpy(real_public_path + strlen(real_public_path),                   \
             ((char*)http_public_folder) + 1);                              \
      protocol->public_folder = real_public_path;                           \
    } else if ((http_public_folder))                                        \
      protocol->public_folder =                                             \
          realpath((http_public_folder), real_public_path);                 \
    protocol->public_folder_length = strlen(protocol->public_folder);       \
    HttpRequest.destroy(HttpRequest.create());                              \
    HttpResponse.init_pool();                                               \
    Server.listen((struct ServerSettings){                                  \
        .timeout = HTTP_DEFAULT_TIMEOUT,                                    \
        .busy_msg = "HTTP/1.1 503 Service Unavailable\r\n\r\nServer Busy.", \
        __VA_ARGS__,                                                        \
        .protocol = (struct Protocol*)(protocol)});                         \
    HttpResponse.destroy_pool();                                            \
    HttpProtocol.destroy(protocol);                                         \
  } while (0);

#ifdef SSL_VERIFY_PEER

#define start_https_server(on_request_callback, http_public_folder, ...)    \
  do {                                                                      \
    struct HttpProtocol* protocol = HttpProtocol.create();                  \
    if ((NULL != (void*)on_request_callback))                               \
      protocol->on_request = (on_request_callback);                         \
    char real_public_path[PATH_MAX];                                        \
    if ((http_public_folder) && ((char*)http_public_folder)[0] == '~' &&    \
        getenv("HOME") && strlen((http_public_folder)) < PATH_MAX) {        \
      strcpy(real_public_path, getenv("HOME"));                             \
      strcpy(real_public_path + strlen(real_public_path),                   \
             ((char*)http_public_folder) + 1);                              \
      protocol->public_folder = real_public_path;                           \
    } else if ((http_public_folder))                                        \
      protocol->public_folder =                                             \
          realpath((http_public_folder), real_public_path);                 \
    protocol->public_folder_length = strlen(protocol->public_folder);       \
    HttpRequest.destroy(HttpRequest.create());                              \
    HttpResponse.init_pool();                                               \
    struct ServerSettings settings = {                                      \
        .timeout = HTTP_DEFAULT_TIMEOUT,                                    \
        .busy_msg = "HTTP/1.1 503 Service Unavailable\r\n\r\nServer Busy.", \
        __VA_ARGS__,                                                        \
        .protocol = (struct Protocol*)(protocol)};                          \
  };                                                                        \
  TLSServer.update_settings(&settings);                                     \
  Server.listen(settings);                                                  \
  HttpResponse.destroy_pool();                                              \
  HttpProtocol.destroy(protocol);                                           \
  }                                                                         \
  while (0)                                                                 \
    ;

#endif /* SSL_VERIFY_PEER */

#endif /* HTTP_COLLECTED_H */
