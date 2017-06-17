/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "http.h"
#include "http1.h"
#include "http1_request.h"
#include "http1_simple_parser.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

char *HTTP1_Protocol_String = "facil_http/1.1_protocol";
/* *****************************************************************************
HTTP/1.1 data structures
*/

typedef struct http1_protocol_s {
  protocol_s protocol;
  http_settings_s *settings;
  void (*on_request)(http_request_s *request);
  struct http1_protocol_s *next;
  http1_request_s request;
  ssize_t len; /* used as a persistent socket `read` indication. */
} http1_protocol_s;

static void http1_on_data(intptr_t uuid, http1_protocol_s *protocol);

/* *****************************************************************************
HTTP/1.1 pool
*/

static struct {
  spn_lock_i lock;
  uint8_t init;
  http1_protocol_s *next;
  http1_protocol_s protocol_mem[HTTP1_POOL_SIZE];
} http1_pool = {.lock = SPN_LOCK_INIT, .init = 0};

static void http1_free(intptr_t uuid, http1_protocol_s *pr) {
  if ((uintptr_t)pr < (uintptr_t)http1_pool.protocol_mem ||
      (uintptr_t)pr >= (uintptr_t)(http1_pool.protocol_mem + HTTP1_POOL_SIZE))
    goto use_free;
  spn_lock(&http1_pool.lock);
  pr->next = http1_pool.next;
  http1_pool.next = pr;
  spn_unlock(&http1_pool.lock);
  return;
use_free:
  free(pr);
  return;
  (void)uuid;
}

static inline void http1_set_protocol_data(http1_protocol_s *pr) {
  pr->protocol = (protocol_s){
      .on_data = (void (*)(intptr_t, protocol_s *))http1_on_data,
      .on_close = (void (*)(intptr_t uuid, protocol_s *))http1_free};
  pr->request.request = (http_request_s){.fd = 0, .http_version = HTTP_V1};
  pr->request.header_pos = pr->request.buffer_pos = pr->len = 0;
}

static http1_protocol_s *http1_alloc(void) {
  http1_protocol_s *pr;
  spn_lock(&http1_pool.lock);
  if (!http1_pool.next)
    goto use_malloc;
  pr = http1_pool.next;
  http1_pool.next = pr->next;
  spn_unlock(&http1_pool.lock);
  http1_request_clear(&pr->request.request);
  pr->request.request.settings = pr->settings;
  pr->len = 0;
  return pr;
use_malloc:
  if (http1_pool.init == 0)
    goto initialize;
  spn_unlock(&http1_pool.lock);
  // fprintf(stderr, "using malloc\n");
  pr = malloc(sizeof(*pr));
  http1_set_protocol_data(pr);
  return pr;
initialize:
  http1_pool.init = 1;
  for (size_t i = 1; i < (HTTP1_POOL_SIZE - 1); i++) {
    http1_set_protocol_data(http1_pool.protocol_mem + i);
    http1_pool.protocol_mem[i].next = http1_pool.protocol_mem + (i + 1);
  }
  http1_pool.protocol_mem[HTTP1_POOL_SIZE - 1].next = NULL;
  http1_pool.next = http1_pool.protocol_mem + 1;
  spn_unlock(&http1_pool.lock);
  http1_set_protocol_data(http1_pool.protocol_mem);
  return http1_pool.protocol_mem;
}

/* *****************************************************************************
HTTP callbacks
*/
#define HTTP_BODY_CHUNK_SIZE 3072 // 4096

static void http1_on_header_found(http_request_s *request,
                                  http_header_s *header) {
  ((http1_request_s *)request)
      ->headers[((http1_request_s *)request)->header_pos] = *header;
  ((http1_request_s *)request)->header_pos += 1;
}

static void http1_on_data(intptr_t uuid, http1_protocol_s *pr);
static void http1_on_data_def(intptr_t uuid, protocol_s *pr, void *ignr) {
  sock_touch(uuid);
  http1_on_data(uuid, (http1_protocol_s *)pr);
  (void)ignr;
}
/* parse and call callback */
static void http1_on_data(intptr_t uuid, http1_protocol_s *pr) {
  ssize_t result;
  char buff[HTTP_BODY_CHUNK_SIZE];
  char *buffer;
  http1_request_s *request = &pr->request;
  for (;;) {
    // handle requests with no file data
    if (request->request.body_file <= 0) {
      // request headers parsing
      if (pr->len == 0) {
        buffer = request->buffer;
        // make sure headers don't overflow
        pr->len = sock_read(uuid, buffer + request->buffer_pos,
                            HTTP1_MAX_HEADER_SIZE - request->buffer_pos);
        // update buffer read position.
        request->buffer_pos += pr->len;
        // if (len > 0) {
        //   fprintf(stderr, "\n----\nRead from socket, %lu bytes, total
        //   %lu:\n",
        //           len, request->buffer_pos);
        //   for (size_t i = 0; i < request->buffer_pos; i++) {
        //     fprintf(stderr, "%c", buffer[i] ? buffer[i] : '-');
        //   }
        //   fprintf(stderr, "\n");
        // }
      }
      if (pr->len <= 0)
        goto finished_reading;

      // parse headers
      result =
          http1_parse_request_headers(buffer, request->buffer_pos,
                                      &request->request, http1_on_header_found);
      // review result
      if (result >= 0) { // headers comeplete
        // are we done?
        if (request->request.content_length == 0 || request->request.body_str) {
          goto handle_request;
        }
        if (request->request.content_length > pr->settings->max_body_size) {
          goto body_to_big;
        }
        // initialize or submit body data
        result = http1_parse_request_body(buffer + result, pr->len - result,
                                          (http_request_s *)request);
        if (result >= 0) {
          request->buffer_pos += result;
          goto handle_request;
        } else if (result == -1) // parser error
          goto parser_error;
        goto parse_body;
      } else if (result == -1) // parser error
        goto parser_error;
      // assume incomplete (result == -2), even if wrong, we're right.
      pr->len = 0;
      continue;
    }
    if (request->request.body_file > 0) {
    // fprintf(stderr, "Body File\n");
    parse_body:
      buffer = buff;
      // request body parsing
      pr->len = sock_read(uuid, buffer, HTTP_BODY_CHUNK_SIZE);
      if (pr->len <= 0)
        goto finished_reading;
      result = http1_parse_request_body(buffer, pr->len, &request->request);
      if (result >= 0) {
        goto handle_request;
      } else if (result == -1) // parser error
        goto parser_error;
      if (pr->len < HTTP_BODY_CHUNK_SIZE) // pause parser for more data
        goto finished_reading;
      goto parse_body;
    }
    continue;
  handle_request:
    // review required headers / data
    if (request->request.host == NULL)
      goto bad_request;
    http_settings_s *settings = pr->settings;
    request->request.settings = settings;
    // call request callback
    if (pr && settings &&
        (request->request.upgrade || settings->public_folder == NULL ||
         http_response_sendfile2(
             NULL, &request->request, settings->public_folder,
             settings->public_folder_length, request->request.path,
             request->request.path_len, settings->log_static))) {
      pr->on_request(&request->request);
      // fprintf(stderr, "Called on_request\n");
    }
    // rotate buffer for HTTP pipelining
    if ((ssize_t)request->buffer_pos <= result) {
      pr->len = 0;
      // fprintf(stderr, "\n----\nAll data consumed.\n");
    } else {
      memmove(request->buffer, buffer + result, request->buffer_pos - result);
      pr->len = request->buffer_pos - result;
      // fprintf(stderr, "\n----\ndata after move, %lu long:\n%.*s\n", len,
      //         (int)len, request->buffer);
    }
    // fprintf(stderr, "data in buffer, %lu long:\n%.*s\n", len, (int)len,
    //         request->buffer);
    // clear request state
    http1_request_clear(&request->request);
    request->buffer_pos = pr->len;
    // make sure to use the correct buffer.
    buffer = request->buffer;
    if (pr->len) {
      /* prevent this connection from hogging the thread by pipelining endless
       * requests.
       */
      facil_defer(.task = http1_on_data_def, .task_type = FIO_PR_LOCK_TASK,
                  .uuid = uuid);
      return;
    }
  }
  // no routes lead here.
  fprintf(stderr,
          "I am lost on a deserted island, no code can reach me here :-)\n");
  goto finished_reading; // How did we get here?
parser_error:
  if (request->request.headers_count >= HTTP1_MAX_HEADER_COUNT)
    goto too_big;
bad_request:
  /* handle generally bad requests */
  {
    http_response_s *response = http_response_create(&request->request);
    response->status = 400;
    if (pr->settings->public_folder &&
        !http_response_sendfile2(response, &request->request,
                                 pr->settings->public_folder,
                                 pr->settings->public_folder_length, "400.html",
                                 8, pr->settings->log_static))
      http_response_write_body(response, "Bad Request", 11);
    http_response_finish(response);
    sock_close(uuid);
    request->buffer_pos = 0;
    goto finished_reading;
  }
too_big:
  /* handle oversized headers */
  {
    http_response_s *response = http_response_create(&request->request);
    response->status = 431;
    if (pr->settings->public_folder &&
        !http_response_sendfile2(response, &request->request,
                                 pr->settings->public_folder,
                                 pr->settings->public_folder_length, "431.html",
                                 8, pr->settings->log_static))
      http_response_write_body(response, "Request Header Fields Too Large", 31);
    http_response_finish(response);
    sock_close(uuid);
    request->buffer_pos = 0;
    goto finished_reading;
  body_to_big:
    /* handle oversized body */
    {
      http_response_s *response = http_response_create(&request->request);
      response->status = 413;
      if (pr->settings->public_folder &&
          !http_response_sendfile2(response, &request->request,
                                   pr->settings->public_folder,
                                   pr->settings->public_folder_length,
                                   "413.html", 8, pr->settings->log_static))
        http_response_write_body(response, "Payload Too Large", 17);
      http_response_finish(response);
      sock_close(uuid);
      request->buffer_pos = 0;
      goto finished_reading;
    }
  }
finished_reading:
  pr->len = 0;
}

/* *****************************************************************************
HTTP listening helpers
*/

/**
Allocates memory for an upgradable HTTP/1.1 protocol.

The protocol self destructs when the `on_close` callback is called.
*/
protocol_s *http1_on_open(intptr_t fd, http_settings_s *settings) {
  if (sock_uuid2fd(fd) >= (sock_max_capacity() - HTTP_BUSY_UNLESS_HAS_FDS))
    goto is_busy;
  http1_protocol_s *pr = http1_alloc();
  pr->request.request.fd = fd;
  pr->settings = settings;
  pr->on_request = settings->on_request;
  facil_set_timeout(fd, pr->settings->timeout);
  return (protocol_s *)pr;

is_busy:
  if (settings->public_folder && settings->public_folder_length) {
    size_t p_len = settings->public_folder_length;
    struct stat file_data = {.st_mode = 0};
    char fname[p_len + 8 + 1];
    memcpy(fname, settings->public_folder, p_len);
    if (settings->public_folder[p_len - 1] == '/' ||
        settings->public_folder[p_len - 1] == '\\')
      p_len--;
    memcpy(fname + p_len, "/503.html", 9);
    p_len += 9;
    if (stat(fname, &file_data))
      goto busy_no_file;
    // check that we have a file and not something else
    if (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode))
      goto busy_no_file;
    int file = open(fname, O_RDONLY);
    if (file == -1)
      goto busy_no_file;
    sock_buffer_s *buffer;
    buffer = sock_buffer_checkout();
    memcpy(buffer->buf,
           "HTTP/1.1 503 Service Unavailable\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "Content-Length: ",
           94);
    p_len = 94 + http_ul2a((char *)buffer->buf + 94, file_data.st_size);
    memcpy(buffer->buf + p_len, "\r\n\r\n", 4);
    p_len += 4;
    if ((off_t)(BUFFER_PACKET_SIZE - p_len) > file_data.st_size) {
      if (read(file, buffer->buf + p_len, file_data.st_size) < 0) {
        close(file);
        sock_buffer_free(buffer);
        goto busy_no_file;
      }
      close(file);
      buffer->len = p_len + file_data.st_size;
      sock_buffer_send(fd, buffer);
    } else {
      buffer->len = p_len;
      sock_buffer_send(fd, buffer);
      sock_sendfile(fd, file, 0, file_data.st_size);
      sock_close(fd);
    }
    return NULL;
  }

busy_no_file:
  sock_write(fd,
             "HTTP/1.1 503 Service Unavailable\r\nContent-Length: "
             "13\r\n\r\nServer Busy.",
             68);
  sock_close(fd);
  return NULL;
}
