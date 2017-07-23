/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "http.h"
#include "http1_parser.h"
#include "http1_request.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

char *HTTP11_Protocol_String = "facil_http/1.1_protocol";
/* *****************************************************************************
HTTP/1.1 data structures
***************************************************************************** */

typedef struct http1_protocol_s {
  protocol_s protocol;
  http_settings_s *settings;
  http1_parser_s parser;
  void (*on_request)(http_request_s *request);
  struct http1_protocol_s *next;
  http1_request_s request;
  size_t len;     /* used as a persistent socket `read` indication. */
  size_t refresh; /* a flag indicating a request callback was called. */
} http1_protocol_s;

static void http1_on_data(intptr_t uuid, http1_protocol_s *protocol);

/* *****************************************************************************
HTTP/1.1 pool
***************************************************************************** */

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
  pr->refresh = pr->request.header_pos = pr->request.buffer_pos = pr->len = 0;
  pr->parser = (http1_parser_s){.udata = pr};
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
HTTP/1.1 error responses
***************************************************************************** */

static void err_bad_request(http1_protocol_s *pr) {
  http_response_s *response = http_response_create(&pr->request.request);
  if (pr->settings->log_static)
    http_response_log_start(response);
  response->status = 400;
  if (!pr->settings->public_folder ||
      http_response_sendfile2(response, &pr->request.request,
                              pr->settings->public_folder,
                              pr->settings->public_folder_length, "400.html", 8,
                              pr->settings->log_static)) {
    response->should_close = 1;
    http_response_write_body(response, "Bad Request", 11);
    http_response_finish(response);
  }
  sock_close(pr->request.request.fd);
  pr->request.buffer_pos = 0;
}

static void err_too_big(http1_protocol_s *pr) {
  http_response_s *response = http_response_create(&pr->request.request);
  if (pr->settings->log_static)
    http_response_log_start(response);
  response->status = 431;
  if (!pr->settings->public_folder ||
      http_response_sendfile2(response, &pr->request.request,
                              pr->settings->public_folder,
                              pr->settings->public_folder_length, "431.html", 8,
                              pr->settings->log_static)) {
    http_response_write_body(response, "Request Header Fields Too Large", 31);
    http_response_finish(response);
  }
}

/* *****************************************************************************
HTTP/1.1 parsre callbacks
***************************************************************************** */

#define HTTP_BODY_CHUNK_SIZE 4096

/** called when a request was received. */
static int http1_on_request(http1_parser_s *parser) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  if (pr->request.request.host == NULL) {
    goto bad_request;
  }
  pr->request.request.content_length = parser->state.content_length;
  http_settings_s *settings = pr->settings;
  pr->request.request.settings = settings;
  // make sure udata to NULL, making it available for the user
  pr->request.request.udata = NULL;
  // static file service or call request callback
  if (pr->request.request.upgrade || settings->public_folder == NULL ||
      http_response_sendfile2(
          NULL, &pr->request.request, settings->public_folder,
          settings->public_folder_length, pr->request.request.path,
          pr->request.request.path_len, settings->log_static)) {
    pr->on_request(&pr->request.request);
  }
  pr->refresh = 1;
  return 0;
bad_request:
  /* handle generally bad requests */
  err_bad_request(pr);
  return -1;
}
/** called when a response was received. */
static int http1_on_response(http1_parser_s *parser) {
  return -1;
  (void)parser;
}
/** called when a request method is parsed. */
static int http1_on_method(http1_parser_s *parser, char *method,
                           size_t method_len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  pr->request.request.method = method;
  pr->request.request.method_len = method_len;
  return 0;
}

/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int http1_on_status(http1_parser_s *parser, size_t status,
                           char *status_str, size_t len) {
  return -1;
  (void)parser;
  (void)status;
  (void)status_str;
  (void)len;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_path(http1_parser_s *parser, char *path, size_t path_len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  pr->request.request.path = path;
  pr->request.request.path_len = path_len;
  return 0;
}

/** called when a request path (excluding query) is parsed. */
static int http1_on_query(http1_parser_s *parser, char *query,
                          size_t query_len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  pr->request.request.query = query;
  pr->request.request.query_len = query_len;
  return 0;
}

/** called when a the HTTP/1.x version is parsed. */
static int http1_on_http_version(http1_parser_s *parser, char *version,
                                 size_t len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  pr->request.request.version = version;
  pr->request.request.version_len = len;
  return 0;
}

/** called when a header is parsed. */
static int http1_on_header(http1_parser_s *parser, char *name, size_t name_len,
                           char *data, size_t data_len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr || pr->request.header_pos >= HTTP1_MAX_HEADER_COUNT - 1)
    goto too_big;
  if (parser->state.read)
    goto too_big; /* refuse trailer header data, it isn't in buffer */

/** test for special headers that should be easily accessible **/
#if HTTP_HEADERS_LOWERCASE
  if (name_len == 4 && *((uint32_t *)name) == *((uint32_t *)"host")) {
    pr->request.request.host = data;
    pr->request.request.host_len = data_len;
  } else if (name_len == 12 && *((uint32_t *)name) == *((uint32_t *)"cont") &&
             *((uint64_t *)(name + 4)) == *((uint64_t *)"ent-type")) {
    pr->request.request.content_type = data;
    pr->request.request.content_type_len = data_len;
  } else if (name_len == 7 && *((uint64_t *)name) == *((uint64_t *)"upgrade")) {
    pr->request.request.upgrade = data;
    pr->request.request.upgrade_len = data_len;
  } else if (name_len == 10 && *((uint32_t *)name) == *((uint32_t *)"conn") &&
             *((uint64_t *)(name + 2)) == *((uint64_t *)"nnection")) {
    pr->request.request.connection = data;
    pr->request.request.connection_len = data_len;
  }
#else
  if (name_len == 4 && HEADER_NAME_IS_EQ(name, "host", name_len)) {
    pr->request.request.host = data;
    pr->request.request.host_len = data_len;
  } else if (name_len == 12 &&
             HEADER_NAME_IS_EQ(name, "content-type", name_len)) {
    pr->request.request.content_type = data;
    pr->request.request.content_type_len = data_len;
  } else if (name_len == 7 && HEADER_NAME_IS_EQ(name, "upgrade", name_len)) {
    pr->request.request.upgrade = data;
    pr->request.request.upgrade_len = data_len;
  } else if (name_len == 10 &&
             HEADER_NAME_IS_EQ(name, "connection", name_len)) {
    pr->request.request.connection = data;
    pr->request.request.connection_len = data_len;
  }
#endif
  pr->request.headers[pr->request.header_pos].name = name;
  pr->request.headers[pr->request.header_pos].name_len = name_len;
  pr->request.headers[pr->request.header_pos].data = data;
  pr->request.headers[pr->request.header_pos].data_len = data_len;
  pr->request.header_pos++;
  pr->request.request.headers_count++;
  return 0;
too_big:
  /* handle oversized headers */
  err_too_big(pr);
  return -1;
}

/** called when a body chunk is parsed. */
static int http1_on_body_chunk(http1_parser_s *parser, char *data,
                               size_t data_len) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  if (parser->state.content_length > (ssize_t)pr->settings->max_body_size ||
      parser->state.read > (ssize_t)pr->settings->max_body_size)
    return -1; /* test every time, in case of chunked data */
  if (!parser->state.read) {
    if (parser->state.content_length > 0 &&
        (pr->request.buffer_pos + parser->state.content_length <
         HTTP1_MAX_HEADER_SIZE)) {
      pr->request.request.body_str = data;
    } else {
// create a temporary file to contain the data.
#ifdef P_tmpdir
#if defined(__linux__) /* linux doesn't end with a divider */
      char template[] = P_tmpdir "/http_request_body_XXXXXXXX";
#else
      char template[] = P_tmpdir "http_request_body_XXXXXXXX";
#endif
#else
      char template[] = "/tmp/http_request_body_XXXXXXXX";
#endif
      pr->request.request.body_file = mkstemp(template);
      if (pr->request.request.body_file == -1)
        return -1;
    }
  }
  if (pr->request.request.body_file) {
    if (write(pr->request.request.body_file, data, data_len) !=
        (ssize_t)data_len)
      return -1;
  } else {
    /* nothing to do... the parser and `on_data` are doing all the work */
  }
  return 0;
}

/** called when a protocol error occured. */
static int http1_on_error(http1_parser_s *parser) {
  http1_protocol_s *pr = parser->udata;
  if (!pr)
    return -1;
  sock_close(pr->request.request.fd);
  http1_request_clear(&pr->request.request);
  return 0;
}

/* *****************************************************************************
HTTP/1.1 protocol callbacks
***************************************************************************** */

/* parse and call callback */
static void http1_on_data(intptr_t uuid, http1_protocol_s *pr) {
  size_t consumed;
  char buff[HTTP_BODY_CHUNK_SIZE];
  http1_request_s *request = &pr->request;
  char *buffer = request->buffer;
  ssize_t tmp = 0;
  // handle requests with no file data
  if (request->request.body_file <= 0) {
    // read into the request buffer.
    tmp = sock_read(uuid, request->buffer + request->buffer_pos,
                    HTTP1_MAX_HEADER_SIZE - request->buffer_pos);
    if (tmp > 0) {
      request->buffer_pos += tmp;
      pr->len += tmp;
    } else
      tmp = 0;
    buffer = request->buffer + request->buffer_pos - pr->len;
  } else {
    if (pr->len) {
      /* protocol error, we shouldn't have letfovers during file processing */
      err_bad_request(pr);
      http1_on_error(&pr->parser);
      return;
    }
    buffer = buff;
    tmp = sock_read(uuid, buffer, HTTP_BODY_CHUNK_SIZE);
    if (tmp > 0) {
      request->buffer_pos += tmp;
      pr->len += tmp;
    } else
      tmp = 0;
  }

  if (pr->len == 0)
    return;

  // parse HTTP data
  consumed =
      http1_fio_parser(.parser = &pr->parser, .buffer = buffer,
                       .length = pr->len, .on_request = http1_on_request,
                       .on_response = http1_on_response,
                       .on_method = http1_on_method,
                       .on_status = http1_on_status, .on_path = http1_on_path,
                       .on_query = http1_on_query,
                       .on_http_version = http1_on_http_version,
                       .on_header = http1_on_header,
                       .on_body_chunk = http1_on_body_chunk,
                       .on_error = http1_on_error);

  // handle leftovers, if any
  if (pr->refresh) {
    pr->refresh = 0;
    if (pr->len > consumed) {
      memmove(request->buffer, buffer + consumed, pr->len - consumed);
      pr->len = pr->len - consumed;
      http1_request_clear(&request->request);
      request->buffer_pos = pr->len;
      facil_force_event(uuid, FIO_EVENT_ON_DATA);
      return;
    }
    http1_request_clear(&request->request);
    pr->len = 0;
  } else {
    pr->len = pr->len - consumed;
  }
  if (tmp)
    facil_force_event(uuid, FIO_EVENT_ON_DATA);
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
             "HTTP/1.1 503 Service Unavailable\r\n"
             "Content-Length: 13\r\n\r\nServer Busy.",
             68);
  sock_close(fd);
  return NULL;
}
