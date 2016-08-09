#include "http1.h"
#include "http1_simple_parser.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "errno.h"
#include <sys/mman.h>
#include <stdatomic.h>

/* *****************************************************************************
HTTP/1.1 data structure
*/

typedef struct {
  protocol_s protocol;
  http_settings_s* settings;
  char buffer[HTTP1_MAX_HEADER_SIZE];
  size_t buffer_pos;
  void (*on_request)(http_request_s* request);
  http_request_s
      request; /* MUST be last, as it has memory extensions for the headers*/
} http1_protocol_s; /* ~ 8416 bytes for (8 * 1024) buffer size and 64 headers */

#define HTTP1_PROTOCOL_SIZE \
  (sizeof(http1_protocol_s) + HTTP_REQUEST_SIZE(HTTP1_MAX_HEADER_COUNT))

char* HTTP1 = "http1";

/* *****************************************************************************
HTTP/1.1 callbacks
*/
static void http1_on_data(intptr_t uuid, http1_protocol_s* protocol);

/* *****************************************************************************
HTTP/1.1 protocol pooling
*/

#define HTTP1_POOL_MEMORY_SIZE (HTTP1_PROTOCOL_SIZE * HTTP1_POOL_SIZE)

struct {
  void* memory;
  http1_protocol_s* pool;
  atomic_flag lock;
} http1_pool = {0};

inline static http1_protocol_s* pool_pop() {
  http1_protocol_s* prot;
  while (atomic_flag_test_and_set(&http1_pool.lock))
    sched_yield();
  prot = http1_pool.pool;
  if (prot)
    http1_pool.pool = prot->request.metadata.next;
  atomic_flag_clear(&http1_pool.lock);
  return prot;
}

inline static void pool_push(http1_protocol_s* protocol) {
  if (protocol == NULL)
    return;
  while (atomic_flag_test_and_set(&http1_pool.lock))
    sched_yield();
  protocol->request.metadata.next = http1_pool.pool;
  http1_pool.pool = protocol;
  atomic_flag_clear(&http1_pool.lock);
}

static void http1_cleanup(void) {
  // free memory
  if (http1_pool.memory) {
    munmap(http1_pool.memory, HTTP1_POOL_MEMORY_SIZE);
  }
}

static void http1_init(void) {
  pthread_mutex_t inner_lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&inner_lock);
  if (http1_pool.memory == NULL) {
    // Allocate the memory
    http1_pool.memory =
        mmap(NULL, HTTP1_POOL_MEMORY_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (http1_pool.memory == NULL)
      return;
    // setup `atexit` cleanup rutine
    atexit(http1_cleanup);

    // initialize pool
    void* pos = http1_pool.memory;
    while (pos <
           http1_pool.memory + (HTTP1_POOL_MEMORY_SIZE - HTTP1_PROTOCOL_SIZE)) {
      pool_push(pos);
      pos += HTTP1_PROTOCOL_SIZE;
    }
  }
  pthread_mutex_unlock(&inner_lock);
}

/** initializes the library if it wasn't already initialized. */
#define validate_mem()             \
  {                                \
    if (http1_pool.memory == NULL) \
      http1_init();                \
  }

inline static void http1_free(http1_protocol_s* http) {
  http_request_clear(&http->request);
  validate_mem();
  if (((void*)http) >= http1_pool.memory &&
      ((void*)http) <= (http1_pool.memory + HTTP1_POOL_MEMORY_SIZE)) {
    pool_push(http);
  } else
    free(http);
}

protocol_s* http1_alloc(intptr_t fd, http_settings_s* settings) {
  validate_mem();
  // HTTP/1.1 should send a busy response
  // if there aren't enough available file descriptors.
  if (sock_max_capacity() - sock_uuid2fd(fd) <= HTTP_BUSY_UNLESS_HAS_FDS)
    goto is_busy;
  // get an http object from the pool
  http1_protocol_s* http = pool_pop();
  // of malloc one
  if (http == NULL)
    http = malloc(HTTP1_PROTOCOL_SIZE);
  // review allocation
  if (http == NULL)
    return NULL;

  // we shouldn't update the `http` protocol as a struct, as this will waste
  // time as the whole buffer will be zeroed out when there is no need.

  // setup parsing state
  http->buffer_pos = 0;
  // setup protocol callbacks
  http->protocol = (protocol_s){
      .service = HTTP1,
      .on_data = (void (*)(intptr_t, protocol_s*))http1_on_data,
      .on_close = (void (*)(protocol_s*))http1_free,
  };
  // setup request data
  http->request = (http_request_s){
      .metadata.max_headers = HTTP1_MAX_HEADER_COUNT,
      .metadata.fd = fd,
      .metadata.owner = http,
  };
  // update settings
  http->settings = settings;
  http->on_request = settings->on_request;
  // set the timeout
  server_set_timeout(fd, settings->timeout);
  return (protocol_s*)http;

is_busy:
  if (settings->public_folder && settings->public_folder_length) {
    size_t p_len = settings->public_folder_length;
    struct stat file_data = {};
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
    sock_packet_s* packet;
    while ((packet = sock_checkout_packet()) == NULL)
      sched_yield();
    memcpy(packet->buffer,
           "HTTP/1.1 503 Service Unavailable\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "Content-Length: ",
           94);
    p_len = 94 + http_ul2a(packet->buffer + 94, file_data.st_size);
    memcpy(packet->buffer + p_len, "\r\n\r\n", 4);
    p_len += 4;
    if (BUFFER_PACKET_SIZE - p_len > file_data.st_size) {
      read(file, packet->buffer + p_len, file_data.st_size);
      close(file);
      packet->length = p_len + file_data.st_size;
      sock_send_packet(fd, packet);
    } else {
      packet->length = p_len;
      sock_send_packet(fd, packet);
      sock_sendfile(fd, file, 0, file_data.st_size);
    }
    return NULL;
  }
busy_no_file:
  sock_write(fd,
             "HTTP/1.1 503 Service Unavailable\r\nContent-Length: "
             "13\r\n\r\nServer Busy.",
             68);
  return NULL;
}

/* *****************************************************************************
HTTP/1.1 protocol bare-bones implementation
*/

#define HTTP_BODY_CHUNK_SIZE 3072  // 4096

/* parse and call callback */
static void http1_on_data(intptr_t uuid, http1_protocol_s* protocol) {
  ssize_t len = 0;
  ssize_t result;
  char buff[HTTP_BODY_CHUNK_SIZE];
  char* buffer;
  http_request_s* request = &protocol->request;
  for (;;) {
    // handle requests with no file data
    if (request->body_file <= 0) {
      // request headers parsing
      if (len == 0) {
        buffer = protocol->buffer;
        // make sure headers don't overflow
        len = sock_read(uuid, buffer + protocol->buffer_pos,
                        HTTP1_MAX_HEADER_SIZE - protocol->buffer_pos);
        // update buffer read position.
        protocol->buffer_pos += len;
      }
      if (len <= 0) {
        return;
      }
      // parse headers
      result =
          http1_parse_request_headers(buffer, protocol->buffer_pos, request);
      // review result
      if (result >= 0) {  // headers comeplete
        // mark buffer position, for HTTP pipelining
        protocol->buffer_pos = result;
        // are we done?
        if (request->content_length == 0 || request->body_str) {
          goto handle_request;
        }
        if (request->content_length > protocol->settings->max_body_size) {
          goto body_to_big;
        }
        // initialize or submit body data
        result =
            http1_parse_request_body(buffer + result, len - result, request);
        if (result >= 0) {
          protocol->buffer_pos += result;
          goto handle_request;
        } else if (result == -1)  // parser error
          goto parser_error;
        goto parse_body;
      } else if (result == -1)  // parser error
        goto parser_error;
      // assume incomplete (result == -2), even if wrong, we're right.
      len = 0;
      continue;
    }
    if (request->body_file > 0) {
    parse_body:
      buffer = buff;
      // request body parsing
      len = sock_read(uuid, buffer, HTTP_BODY_CHUNK_SIZE);
      if (len <= 0)
        return;
      result = http1_parse_request_body(buffer, len, request);
      if (result >= 0) {
        // set buffer pos for piplining support
        protocol->buffer_pos = result;
        goto handle_request;
      } else if (result == -1)  // parser error
        goto parser_error;
      if (len < HTTP_BODY_CHUNK_SIZE)  // pause parser for more data
        return;
      goto parse_body;
    }
    continue;
  handle_request:
    // review required headers / data
    if (request->host == NULL)
      goto bad_request;
    http_settings_s* settings = protocol->settings;
    // call request callback
    if (protocol && settings &&
        (protocol->settings->public_folder == NULL ||
         http_response_sendfile2(NULL, request, settings->public_folder,
                                 settings->public_folder_length, request->path,
                                 request->path_len, settings->log_static))) {
      protocol->on_request(request);
    }
    // clear request state
    http_request_clear(request);
    // rotate buffer for HTTP pipelining
    if (result >= len) {
      len = 0;
    } else {
      memmove(protocol->buffer, buffer + protocol->buffer_pos, len - result);
      len -= result;
    }
    // restart buffer position
    protocol->buffer_pos = 0;
    buffer = protocol->buffer;
  }
  // no routes lead here.
  fprintf(stderr,
          "I am lost on a deserted island, no code can reach me here :-)\n");
  return;  // How did we get here?
parser_error:
  if (request->headers_count == request->metadata.max_headers)
    goto too_big;
bad_request:
  /* handle generally bad requests */
  {
    http_response_s response = http_response_init(request);
    response.status = 400;
    http_response_write_body(&response, "Bad Request", 11);
    http_response_finish(&response);
    sock_close(uuid);
    protocol->buffer_pos = 0;
    return;
  }
too_big:
  /* handle oversized headers */
  {
    http_response_s response = http_response_init(request);
    response.status = 431;
    http_response_write_body(&response, "Request Header Fields Too Large", 31);
    http_response_finish(&response);
    sock_close(uuid);
    protocol->buffer_pos = 0;
    return;
  body_to_big:
    /* handle oversized body */
    {
      http_response_s response = http_response_init(request);
      response.status = 413;
      http_response_write_body(&response, "Payload Too Large", 17);
      http_response_finish(&response);
      sock_close(uuid);
      protocol->buffer_pos = 0;
      return;
    }
  }
}

/* *****************************************************************************
HTTP/1.1 listenning API implementation
*/

#undef http1_listen

static void http1_on_init(http_settings_s* settings) {
  if (settings->timeout == 0)
    settings->timeout = 5;
  if (settings->max_body_size == 0)
    settings->max_body_size = HTTP_DEFAULT_BODY_LIMIT;
  if (settings->public_folder) {
    settings->public_folder_length = strlen(settings->public_folder);
    if (settings->public_folder[0] == '~' &&
        settings->public_folder[1] == '/' && getenv("HOME")) {
      char* home = getenv("HOME");
      size_t home_len = strlen(home);
      char* tmp = malloc(settings->public_folder_length + home_len + 1);
      memcpy(tmp, home, home_len);
      if (home[home_len - 1] == '/')
        --home_len;
      memcpy(tmp + home_len, settings->public_folder + 1,
             settings->public_folder_length);  // copy also the NULL
      settings->public_folder = tmp;
      settings->private_metaflags |= 1;
      settings->public_folder_length = strlen(settings->public_folder);
    }
  }
}
static void http1_on_finish(http_settings_s* settings) {
  if (settings->private_metaflags & 1)
    free((void*)settings->public_folder);
  if (settings->private_metaflags & 2)
    free(settings);
}

int http1_listen(const char* port,
                 const char* address,
                 http_settings_s settings) {
  if (settings.on_request == NULL) {
    fprintf(
        stderr,
        "ERROR: http1_listen requires the .on_request parameter to be set\n");
    exit(11);
  }
  http_settings_s* settings_copy = malloc(sizeof(*settings_copy));
  *settings_copy = settings;
  settings_copy->private_metaflags = 2;
  return server_listen(.port = port, .address = address,
                       .on_start = (void*)http1_on_init,
                       .on_finish = (void*)http1_on_finish,
                       .on_open = (void*)http1_alloc, .udata = settings_copy);
}
