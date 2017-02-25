/* -*- mode: c -*- */
#ifndef HTTP_WEBSOCKET_TEST
#include "http.h"

/*
A simple Hello World HTTP response emulation. Test with:
ab -n 1000000 -c 200 -k http://127.0.0.1:3000/
*/
static void http1_hello_on_request(http_request_s *request) {
  static char hello_message[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n"
                                "Connection: keep-alive\r\n"
                                "Keep-Alive: 1;timeout=5\r\n"
                                "\r\n"
                                "Hello World!";
  sock_write(request->metadata.fd, hello_message, sizeof(hello_message) - 1);
}

#include "websockets.h" // includes the "http.h" header

#include "bscrypt.h"
#include <stdio.h>
#include <stdlib.h>

/*****************************
A Websocket echo implementation
*/

static void ws_open(ws_s *ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", (void *)ws);
}

static void ws_echo(ws_s *ws, char *data, size_t size, uint8_t is_text) {
  // echos the data to the current websocket
  websocket_write(ws, data, size, is_text);
  if (memcmp(data, "bomb me", 7) == 0) {
    char *msg = malloc(1024 * 1024);
    for (char *pos = msg; pos < msg + (1024 * 1024 - 1); pos += 8) {
      memcpy(pos, "bomb(!) ", 8);
    }
    websocket_write(ws, msg, 1024 * 1024, is_text);
    free(msg);
  }
}

static void ws_shutdown(ws_s *ws) {
  websocket_write(ws, "Shutting Down", 13, 1);
}

static void ws_close(ws_s *ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", (void *)ws);
}

/*****************************
A Websocket Broadcast implementation
*/

/* websocket broadcast data */
struct ws_data {
  size_t size;
  char data[];
};
/* free the websocket broadcast data */
static void free_wsdata(ws_s *ws, void *arg) {
  free(arg);
  (void)(ws);
}
/* the broadcast "task" performed by `Websocket.each` */
static void ws_get_broadcast(ws_s *ws, void *arg) {
  struct ws_data *data = arg;
  websocket_write(ws, data->data, data->size, 1); // echo
}
/* The websocket broadcast server's `on_message` callback */

static void ws_broadcast(ws_s *ws, char *data, size_t size, uint8_t is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data *msg = malloc(sizeof(*msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  websocket_each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  websocket_write(ws, data, size, is_text);
}

/*****************************
The HTTP implementation
*/

static void on_request(http_request_s *request) {
  // to log we will start a response.
  http_response_s response = http_response_init(request);
  // http_response_log_start(&response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade && !strcmp(request->path, "/broadcast")) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown, .response = &response);

    return;
  }
  // other upgrade requests will have the following properties:
  if (request->upgrade) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .max_msg_size = 2097152, .on_open = ws_open,
                      .on_close = ws_close, .timeout = 10,
                      .on_shutdown = ws_shutdown, .response = &response);
    return;
  }
  // file dumping
  if (!strcmp(request->path, "/dump.jpg")) {
    fdump_s *data = bscrypt_fdump("./public_www/bo.jpg", 0);
    if (data == NULL) {
      fprintf(stderr, "Couldn't read file\n");
      http_response_write_body(&response, "Sorry, error!", 13);
      http_response_finish(&response);
      return;
    }
    http_response_write_body(&response, data->data, data->length);
    http_response_finish(&response);
    free(data);
    return;
  }
  if (!strcmp(request->path, "/dump.mov")) {
    fdump_s *data = bscrypt_fdump("./public_www/rolex.mov", 0);
    if (data == NULL) {
      fprintf(stderr, "Couldn't read file\n");
      http_response_write_body(&response, "Sorry, error!", 13);
      http_response_finish(&response);
      return;
    }
    http_response_write_body(&response, data->data, data->length);
    http_response_finish(&response);
    free(data);
    return;
  }
  // HTTP response
  http_response_write_body(&response, "Hello World!", 12);
  http_response_finish(&response);
}

/*****************************
Print to screen protocol
*/
struct prnt2scrn_protocol_s {
  protocol_s protocol;
  intptr_t uuid;
};
static void on_data(intptr_t uuid, protocol_s *protocol) {
  (void)(protocol);
  uint8_t buffer[1024];
  ssize_t len;
  while ((len = sock_read(uuid, buffer, 1024)) > 0) {
    if (len > 0)
      fprintf(stderr, "%.*s\n", (int)len, buffer);
  }
  fprintf(stderr, "returning from on_data\n");
  // sock_write(uuid, "HTTP/1.1 100 Continue\r\n\r\n", 25);
}
static void on_close(protocol_s *protocol) {
  fprintf(stderr, "Connection closed %p\n",
          (void *)(((struct prnt2scrn_protocol_s *)protocol)->uuid));
  free(protocol);
}

static protocol_s *on_open(intptr_t uuid, void *udata) {
  (void)(udata);
  struct prnt2scrn_protocol_s *prt = malloc(sizeof *prt);
  *prt = (struct prnt2scrn_protocol_s){
      .protocol.on_data = on_data, .protocol.on_close = on_close, .uuid = uuid};
  fprintf(stderr, "New connection %p\n", (void *)uuid);
  server_set_timeout(uuid, 10);
  return (void *)prt;
}

/*****************************
non-http-dump
*/

static void htpdmp_on_data(intptr_t uuid, protocol_s *protocol) {
  (void)(protocol);
  uint8_t buffer[1024];
  ssize_t len;
  while ((len = sock_read(uuid, buffer, 1024)) > 0) {
    sock_write(uuid, "HTTP/1.1 200 "
                     "OK\r\nContent-Length:11\r\nConnection:keep-"
                     "alive\r\n\r\nHello Dump!",
               72);
  }
  // sock_write(uuid, "HTTP/1.1 100 Continue\r\n\r\n", 25);
}

static protocol_s *htpdmp_on_open(intptr_t uuid, void *udata) {
  (void)(udata);
  protocol_s *prt = malloc(sizeof *prt);
  *prt = (protocol_s){.on_data = htpdmp_on_data,
                      .on_close = (void (*)(protocol_s *))free};
  server_set_timeout(uuid, 10);
  return (void *)prt;
}

/*****************************
Environment details
*/

#if defined(__linux__) // My Linux machine has a slow file system issue.
static const char *public_folder = NULL; // "./public_www";
#else
static const char *public_folder = "./public_www";
#endif

#ifndef THREAD_COUNT
#define THREAD_COUNT 8
#endif

/*****************************
The main function
*/

#define HTTP_WEBSOCKET_TEST()                                                  \
  server_listen(.port = "5000", .on_open = htpdmp_on_open);                    \
  if (http1_listen("3000", NULL, .on_request = on_request,                     \
                   .public_folder = public_folder, .log_static = 1))           \
    perror("Couldn't initiate HTTP service"), exit(1);                         \
  server_run(.threads = THREAD_COUNT);
#endif
