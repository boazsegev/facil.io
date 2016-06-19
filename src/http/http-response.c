#include "http-response.h"

#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "libsock.h"
#include "mini-crypt.h"

/******************************************************************************
Function declerations.
*/

/**
Destroys the response object pool.
*/
static void destroy_pool(void);
/**
Creates the response object pool (unless it already exists).
*/
static void create_pool(void);
/**
Creates a new response object or recycles a response object from the response
pool.

returns NULL on failuer, or a pointer to a valid response object.
*/
static struct HttpResponse* new_response(struct HttpRequest*);
/**
Destroys the response object or places it in the response pool for recycling.
*/
static void destroy_response(struct HttpResponse*);
/**
Clears the HttpResponse object, linking it with an HttpRequest object (which
will be used to set the server's pointer and socket fd).
*/
static void reset(struct HttpResponse*, struct HttpRequest*);
/** Gets a response status, as a string */
static char* status_str(struct HttpResponse*);
/**
Writes a header to the response.

This is equivelent to writing:

     HttpResponse.write_header(* response, header, value, strlen(value));

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_cstr(struct HttpResponse*,
                             const char* header,
                             const char* value);
/**
Writes a header to the response. This function writes only the requested
number of bytes from the header value and should be used when the header value
doesn't contain a NULL terminating byte.

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_data1(struct HttpResponse*,
                              const char* header,
                              const char* value,
                              size_t value_len);
/**
Writes a header to the response. This function writes only the requested
number of bytes from the header value and should be used when the header value
doesn't contain a NULL terminating byte.

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_data2(struct HttpResponse*,
                              const char* header,
                              size_t header_len,
                              const char* value,
                              size_t value_len);

/**
Set / Delete a cookie using this helper function.

To set a cookie, use (in this example, a session cookie):

    HttpResponse.set_cookie(response, (struct HttpCookie){
            .name = "my_cookie",
            .value = "data" });

To delete a cookie, use:

    HttpResponse.set_cookie(response, (struct HttpCookie){
            .name = "my_cookie",
            .value = NULL });

This function writes a cookie header to the response. Only the requested
number of bytes from the cookie value and name are written (if none are
provided, a terminating NULL byte is assumed).

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
static int set_cookie(struct HttpResponse*, struct HttpCookie);
/**
Prints a string directly to the header's buffer, appending the header
seperator (the new line marker '\r\n' should NOT be printed to the headers
buffer).

Limited error check is performed.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
static int response_printf(struct HttpResponse*, const char* format, ...);
/**
Sends the headers (if they weren't previously sent).

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int send_response(struct HttpResponse*);
/* used by `send_response` and others... */
static sock_packet_s* prep_headers(struct HttpResponse* response);
static int send_headers(struct HttpResponse*, sock_packet_s*);
static size_t write_date_data(char* target, struct tm* tmbuf);
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int write_body(struct HttpResponse*, const char* body, size_t length);
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the body and free it's
memory using `free` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int write_body_move(struct HttpResponse*,
                           const char* body,
                           size_t length);
/**
Sends the complete file referenced by the `file_path` string.

This function requires that the headers weren't previously sent and that the
file exists.

On failure, the function will return -1. On success, the function returns 0.
*/
static int sendfile_path(struct HttpResponse* response, char* file_path);
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file descriptor and
close it using `close` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int req_sendfile(struct HttpResponse* response,
                        int source_fd,
                        off_t offset,
                        size_t length);
/**
Closes the connection.
*/
static void close_response(struct HttpResponse*);

/** ****************************************************************************
The API gateway
*/
struct HttpResponseClass HttpResponse = {
    .destroy_pool = destroy_pool,
    .init_pool = create_pool,
    .create = new_response,
    .destroy = destroy_response,
    .pool_limit = 64,
    .reset = reset,
    .status_str = status_str,
    .write_header = write_header_data2,
    .write_header1 = write_header_data1,
    .write_header2 = write_header_cstr,
    .set_cookie = set_cookie,
    .printf = response_printf,
    .send = send_response,
    .write_body = write_body,
    .write_body_move = write_body_move,
    .sendfile = req_sendfile,
    .sendfile2 = sendfile_path,
    .close = close_response,
};

/**
Alternative to gmtime
*/

// static char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
// static char * Months = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
// "Aug", "Sep", "Oct", "Nov", "Dec"};
/******************************************************************************
The response object pool.
*/

#ifndef RESPONSE_POOL_SIZE
#define RESPONSE_POOL_SIZE 32
#endif

struct HttpResponsePool {
  struct HttpResponse response;
  struct HttpResponsePool* next;
};
// The global packet container pool
static struct {
  struct HttpResponsePool* _Atomic pool;
  struct HttpResponsePool* initialized;
} ContainerPool = {NULL};

static atomic_flag pool_initialized;

static void create_response_pool(void) {
  while (atomic_flag_test_and_set(&pool_initialized)) {
  }
  if (ContainerPool.initialized == NULL) {
    ContainerPool.initialized =
        calloc(RESPONSE_POOL_SIZE, sizeof(struct HttpResponsePool));
    for (size_t i = 0; i < RESPONSE_POOL_SIZE - 1; i++) {
      ContainerPool.initialized[i].next = &ContainerPool.initialized[i + 1];
    }
    ContainerPool.pool = ContainerPool.initialized;
  }
  atomic_flag_clear(&pool_initialized);
}

/******************************************************************************
API implementation.
*/

/**
The padding for the status line (62 + 2 for the extra \r\n after the headers).
*/
#define HTTP_HEADER_START 80

/**
Destroys the response object pool.
*/
static void destroy_pool(void) {
  void* m = ContainerPool.initialized;
  ContainerPool.initialized = NULL;
  atomic_store(&ContainerPool.pool, NULL);
  if (m)
    free(m);
}
/**
Creates the response object pool (unless it already exists).
*/
static void create_pool(void) {
  create_response_pool();
}

/**
Creates a new response object or recycles a response object from the response
pool.

returns NULL on failuer, or a pointer to a valid response object.
*/
static struct HttpResponse* new_response(struct HttpRequest* request) {
  if (ContainerPool.initialized == NULL)
    create_response_pool();
  struct HttpResponsePool* res = atomic_load(&ContainerPool.pool);
  while (res) {
    if (atomic_compare_exchange_weak(&ContainerPool.pool, &res, res->next))
      break;
  }
  if (!res)
    res = calloc(sizeof(struct HttpResponse), 1);
  if (!res)
    return NULL;

  reset((struct HttpResponse*)res, request);
  return (struct HttpResponse*)res;
}
/**
Destroys the response object or places it in the response pool for recycling.
*/
static void destroy_response(struct HttpResponse* response) {
  if (response->metadata.classUUID != new_response)
    return;
  if (response->metadata.should_close)
    close_response(response);
  if (response->metadata.packet) {
    sock_free_packet(response->metadata.packet);
    response->metadata.packet = NULL;
  }

  if (ContainerPool.initialized == NULL ||
      ((struct HttpResponsePool*)response) < ContainerPool.initialized ||
      ((struct HttpResponsePool*)response) >
          (ContainerPool.initialized +
           (sizeof(struct HttpResponsePool) * RESPONSE_POOL_SIZE))) {
    free(response);
  } else {
    ((struct HttpResponsePool*)response)->next =
        atomic_load(&ContainerPool.pool);
    for (;;) {
      if (atomic_compare_exchange_weak(
              &ContainerPool.pool, &((struct HttpResponsePool*)response)->next,
              ((struct HttpResponsePool*)response)))
        break;
    }
  }
}
/**
Clears the HttpResponse object, linking it with an HttpRequest object (which
will be used to set the server's pointer and socket fd).
*/
static void reset(struct HttpResponse* response, struct HttpRequest* request) {
  time_t date = Server.reactor(request->server)->last_tick;
  *response = (struct HttpResponse){
      .metadata.classUUID = new_response,
      .status = 200,
      .metadata.fd_uuid = request->sockfd,
      .metadata.server = request->server,
      .last_modified = date,
      .date = date,
      .metadata.should_close =
          (request && request->connection &&
           (strcasecmp(request->connection, "close") == 0)),
      .metadata.packet = response->metadata.packet};
  while (response->metadata.packet == NULL) {
    response->metadata.packet = sock_checkout_packet();
    if (response->metadata.packet == NULL) {
      // fprintf(stderr, "==== Response cycle\n");
      sock_flush_all();
      sched_yield();
    }
  }
  response->metadata.headers_pos =
      response->metadata.packet->buffer + HTTP_HEADER_START;

  // response->metadata.should_close = (request && request->connection &&
  //                                    ((request->connection[0] | 32) == 'c')
  //                                    &&
  //                                    ((request->connection[1] | 32) == 'l'));
  // response->metadata.should_close = (request && request->connection &&
  //                                    ((request->connection[0] | 32) == 'c'));

  // check the first four bytes for "clos"(e), little endian and network endian
  // response->metadata.should_close =
  //     (request && request->connection &&
  //      (((*((uint32_t*)request->connection) | 538976288UL) == 1936682083UL)
  //      ||
  //       ((*((uint32_t*)request->connection) | 538976288UL) ==
  //       1668050803UL)));
}
/** Gets a response status, as a string */
static char* status_str(struct HttpResponse* response) {
  return HttpStatus.to_s(response->status);
}
/**
Writes a header to the response.

This is equivelent to writing:

     HttpResponse.write_header(* response, header, value, strlen(value));

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_cstr(struct HttpResponse* response,
                             const char* header,
                             const char* value) {
  return write_header_data2(response, header, strlen(header), value,
                            strlen(value));
}
/**
Writes a header to the response. This function writes only the requested
number of bytes from the header value and should be used when the header value
doesn't contain a NULL terminating byte.

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_data1(struct HttpResponse* response,
                              const char* header,
                              const char* value,
                              size_t value_len) {
  return write_header_data2(response, header, strlen(header), value, value_len);
}
/**
Writes a header to the response. This function writes only the requested
number of bytes from the header value and should be used when the header value
doesn't contain a NULL terminating byte.

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header_data2(struct HttpResponse* response,
                              const char* header,
                              size_t header_len,
                              const char* value,
                              size_t value_len) {
  if (response->metadata.headers_sent ||
      (header_len + value_len + 4 +
           (response->metadata.headers_pos -
            (char*)response->metadata.packet->buffer) >=
       HTTP_HEAD_MAX_SIZE))
    return -1;
  // review special headers
  if (!strcasecmp("Date", header) || !strcasecmp("Last-Modified", header)) {
    response->metadata.date_written = 1;
  } else if (!strcasecmp("Connection", header)) {
    response->metadata.connection_written = 1;
  } else if (!strcasecmp("Content-Length", header)) {
    response->metadata.connection_len_written = 1;
  }
  // write the header name to the buffer
  memcpy(response->metadata.headers_pos, header, header_len);
  response->metadata.headers_pos += header_len;
  // write the header to value seperator (`: `)
  *(response->metadata.headers_pos++) = ':';
  *(response->metadata.headers_pos++) = ' ';

  // write the header's value to the buffer
  memcpy(response->metadata.headers_pos, value, value_len);
  response->metadata.headers_pos += value_len;
  // write the header seperator (`\r\n`)
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';

  return 0;
}
/**
Set / Delete a cookie using this helper function.
*/
static int set_cookie(struct HttpResponse* response, struct HttpCookie cookie) {
  if (!cookie.name)
    return -1;  // must have a cookie name.
  // check cookie name's length
  if (!cookie.name_len)
    cookie.name_len = strlen(cookie.name);
  // check cookie name
  for (size_t i = 0; i < cookie.name_len; i++) {
    if (cookie.name[i] < '!' || cookie.name[i] > '~' || cookie.name[i] == '=' ||
        cookie.name[i] == ' ' || cookie.name[i] == ',' ||
        cookie.name[i] == ';') {
      fprintf(stderr,
              "Invalid cookie name - cannot set a cookie's name to: %.*s\n",
              (int)cookie.name_len, cookie.name);
      return -1;
    }
  }

  if (cookie.value) {  // review the value.
    // check cookie value's length
    if (!cookie.value_len)
      cookie.value_len = strlen(cookie.value);
    // check cookie value
    for (size_t i = 0; i < cookie.value_len; i++) {
      if (cookie.value[i] < '!' || cookie.value[i] > '~' ||
          cookie.value[i] == ' ' || cookie.value[i] == ',' ||
          cookie.value[i] == ';') {
        fprintf(stderr, "Invalid cookie value - cannot set a cookie to: %.*s\n",
                (int)cookie.value_len, cookie.value);
        return -1;
      }
    }
  } else {
    cookie.value_len = 0;
    cookie.max_age = -1;
  }
  if (cookie.path) {
  } else
    cookie.path_len = 0;
  if (cookie.domain) {
  } else
    cookie.domain_len = 0;
  if (cookie.name_len + cookie.value_len + cookie.path_len + cookie.domain_len +
          (response->metadata.headers_pos -
           (char*)response->metadata.packet->buffer) +
          96 >
      HTTP_HEAD_MAX_SIZE)
    return -1;  // headers too big (added 96 for keywords, header names etc').
  // write the header's name to the buffer
  memcpy(response->metadata.headers_pos, "Set-Cookie: ", 12);
  response->metadata.headers_pos += 12;
  // write the cookie name to the buffer
  memcpy(response->metadata.headers_pos, cookie.name, cookie.name_len);
  response->metadata.headers_pos += cookie.name_len;
  // seperate name from value
  *(response->metadata.headers_pos++) = '=';
  if (cookie.value) {
    // write the cookie value to the buffer
    memcpy(response->metadata.headers_pos, cookie.value, cookie.value_len);
    response->metadata.headers_pos += cookie.value_len;
  }
  // complete value data
  *(response->metadata.headers_pos++) = ';';
  if (cookie.max_age) {
    response->metadata.headers_pos +=
        sprintf(response->metadata.headers_pos, "Max-Age=%d;", cookie.max_age);
  }
  if (cookie.domain) {
    memcpy(response->metadata.headers_pos, "domain=", 7);
    response->metadata.headers_pos += 7;
    memcpy(response->metadata.headers_pos, cookie.domain, cookie.domain_len);
    response->metadata.headers_pos += cookie.domain_len;
    *(response->metadata.headers_pos++) = ';';
  }
  if (cookie.path) {
    memcpy(response->metadata.headers_pos, "path=", 5);
    response->metadata.headers_pos += 5;
    memcpy(response->metadata.headers_pos, cookie.path, cookie.path_len);
    response->metadata.headers_pos += cookie.path_len;
    *(response->metadata.headers_pos++) = ';';
  }
  if (cookie.http_only) {
    memcpy(response->metadata.headers_pos, "HttpOnly;", 9);
    response->metadata.headers_pos += 9;
  }
  if (cookie.secure) {
    memcpy(response->metadata.headers_pos, "secure;", 7);
    response->metadata.headers_pos += 7;
  }
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';
  return 0;
}
/**
Prints a string directly to the header's buffer, appending the header
seperator (the new line marker '\r\n' should NOT be printed to the headers
buffer).

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
static int response_printf(struct HttpResponse* response,
                           const char* format,
                           ...) {
  if (response->metadata.headers_sent)
    return -1;
  size_t max_len =
      HTTP_HEAD_MAX_SIZE - (response->metadata.headers_pos -
                            (char*)response->metadata.packet->buffer);
  va_list args;
  va_start(args, format);
  int written =
      vsnprintf(response->metadata.headers_pos, max_len, format, args);
  va_end(args);
  if (written > max_len)
    return -1;
  // update the writing position
  response->metadata.headers_pos += written;
  // write the header seperator (`\r\n`)
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';
  return 0;
}

/**
Sends the headers (if they weren't previously sent).

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int send_response(struct HttpResponse* response) {
  void* start = prep_headers(response);
  return send_headers(response, start);
  // // debug
  // for (int i = start; i < response->metadata.headers_length +
  // HTTP_HEADER_START;
  //      i++) {
  //   fprintf(stderr, "* %d: %x (%.6s)\n", i, response->header_buffer[i],
  //           response->header_buffer + i);
  // }
};
static char* DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char* MONTH_NAMES[] = {"Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
                              "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
static const char* GMT_STR = "GMT";
static size_t write_date_data(char* target, struct tm* tmbuf) {
  char* pos = target;
  uint16_t tmp;
  *(uint64_t*)pos = *((uint64_t*)DAY_NAMES[tmbuf->tm_wday]);
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  if (tmbuf->tm_mday < 10) {
    *pos = '0' + tmbuf->tm_mday;
    ++pos;
  } else {
    tmp = tmbuf->tm_mday / 10;
    pos[0] = '0' + tmp;
    pos[1] = '0' + (tmbuf->tm_mday - (tmp * 10));
    pos += 2;
  }
  *(pos++) = ' ';
  *(uint64_t*)pos = *((uint64_t*)MONTH_NAMES[tmbuf->tm_mon]);
  pos += 4;
  // assums years with less then 10K.
  pos[3] = '0' + (tmbuf->tm_year % 10);
  tmp = (tmbuf->tm_year + 1900) / 10;
  pos[2] = '0' + (tmp % 10);
  tmp = tmp / 10;
  pos[1] = '0' + (tmp % 10);
  tmp = tmp / 10;
  pos[0] = '0' + (tmp % 10);
  pos[4] = ' ';
  pos += 5;
  tmp = tmbuf->tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tmbuf->tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tmbuf->tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tmbuf->tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tmbuf->tm_sec - (tmp * 10));
  pos += 8;
  pos[0] = ' ';
  *((uint64_t*)(pos + 1)) = *((uint64_t*)GMT_STR);
  pos += 4;
  return pos - target;
}

static sock_packet_s* prep_headers(struct HttpResponse* response) {
  sock_packet_s* headers;
  if (response->metadata.headers_sent ||
      (headers = response->metadata.packet) == NULL)
    return NULL;
  char* status = HttpStatus.to_s(response->status);
  if (!status)
    return NULL;
  // write the content length header, unless forced not to (<0)
  if (response->metadata.connection_len_written == 0 &&
      !(response->content_length < 0) && response->status >= 200 &&
      response->status != 204 && response->status != 304) {
    memcpy(response->metadata.headers_pos, "Content-Length: ", 16);
    response->metadata.headers_pos += 16;
    response->metadata.headers_pos += sprintf(response->metadata.headers_pos,
                                              "%lu", response->content_length);
    // write the header seperator (`\r\n`)
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
  }
  // write the date, if missing
  if (!response->metadata.date_written) {
    if (response->date < response->last_modified)
      response->date = response->last_modified;
    struct tm t;
    // date header
    MiniCrypt.gmtime(&response->date, &t);
    memcpy(response->metadata.headers_pos, "Date: ", 6);
    response->metadata.headers_pos += 6;
    response->metadata.headers_pos +=
        write_date_data(response->metadata.headers_pos, &t);
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
    // last-modified header
    MiniCrypt.gmtime(&response->last_modified, &t);
    memcpy(response->metadata.headers_pos, "Last-Modified: ", 15);
    response->metadata.headers_pos += 15;
    response->metadata.headers_pos +=
        write_date_data(response->metadata.headers_pos, &t);
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
  }
  // write the keep-alive (connection) header, if missing
  if (!response->metadata.connection_written) {
    if (response->metadata.should_close) {
      memcpy(response->metadata.headers_pos, "Connection: close\r\n", 19);
      response->metadata.headers_pos += 19;
    } else {
      memcpy(response->metadata.headers_pos,
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=2\r\n",
             47);
      response->metadata.headers_pos += 47;
    }
  }

  // write the headers completion marker (empty line - `\r\n`)
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';

  // write the status string is "HTTP/1.1 xxx <...>\r\n" length == 15 +
  // strlen(status)
  int start = HTTP_HEADER_START - (15 + strlen(status));
  sprintf((char*)headers->buffer + start, "HTTP/1.1 %d %s\r", response->status,
          status);
  // we need to seperate the writing because sprintf prints a NULL terminator.
  ((char*)headers->buffer)[HTTP_HEADER_START - 1] = '\n';
  headers->buffer = (char*)headers->buffer + start;
  headers->length = response->metadata.headers_pos - (char*)headers->buffer;
  return headers;
}
static int send_headers(struct HttpResponse* response, sock_packet_s* packet) {
  if (packet == NULL)
    return -1;
  // mark headers as sent
  response->metadata.headers_sent = 1;
  response->metadata.packet = NULL;
  // write data to network
  return Server.send_packet(response->metadata.server,
                            response->metadata.fd_uuid, packet);
};

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int write_body(struct HttpResponse* response,
                      const char* body,
                      size_t length) {
  if (!response->content_length)
    response->content_length = length;
  sock_packet_s* headers = prep_headers(response);
  if (headers != NULL) {  // we haven't sent the headers yet
    ssize_t i_read =
        ((BUFFER_PACKET_SIZE - HTTP_HEADER_START) - headers->length);
    if (i_read > 1024) {
      // we can fit at least some of the data inside the response buffer.
      if (i_read > length) {
        i_read = length;
        // we can fit the data inside the response buffer.
        memcpy(response->metadata.headers_pos, body, i_read);
        response->metadata.headers_pos += i_read;
        headers->length += i_read;
        return send_headers(response, headers);
      }
      memcpy(response->metadata.headers_pos, body, i_read);
      response->metadata.headers_pos += i_read;
      headers->length += i_read;
      length -= i_read;
      body += i_read;
    }
    // we need to send the (rest of the) body seperately.
    if (send_headers(response, headers))
      return -1;
  }
  return Server.write(response->metadata.server, response->metadata.fd_uuid,
                      (void*)body, length);
}
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the body and free it's
memory using `free` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int write_body_move(struct HttpResponse* response,
                           const char* body,
                           size_t length) {
  if (!response->content_length)
    response->content_length = length;
  sock_packet_s* headers = prep_headers(response);
  if (headers != NULL) {  // we haven't sent the headers yet
    if ((headers->length + length) < (BUFFER_PACKET_SIZE - HTTP_HEADER_START)) {
      // we can fit the data inside the response buffer.
      memcpy(response->metadata.headers_pos, body, length);
      response->metadata.headers_pos += length;
      headers->length += length;
      free((void*)body);
      return send_headers(response, headers);
    } else {
      // we need to send the body seperately.
      send_headers(response, headers);
    }
  }
  return Server.write_move(response->metadata.server,
                           response->metadata.fd_uuid, (void*)body, length);
}
/**
Sends the complete file referenced by the `file_path` string.

This function requires that the headers weren't previously sent and that the
file exists.

On failure, the function will return -1. On success, the function returns 0.
*/
static int sendfile_path(struct HttpResponse* response, char* file_path) {
  if (response->metadata.headers_sent) {
    return -1;
  }

  int f_fd;
  struct stat f_data;
  if (stat(file_path, &f_data)) {
    return -1;
  }
  f_fd = open(file_path, O_RDONLY);
  if (f_fd == -1) {
    return -1;
  }
  response->last_modified = f_data.st_mtime;
  return req_sendfile(response, f_fd, f_data.st_size, 0);
}
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file descriptor and
close it using `close` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int req_sendfile(struct HttpResponse* response,
                        int source_fd,
                        off_t offset,
                        size_t length) {
  if (!response->content_length)
    response->content_length = length;

  sock_packet_s* headers = prep_headers(response);

  if (headers != NULL) {  // we haven't sent the headers yet
    if (headers->length < (BUFFER_PACKET_SIZE - HTTP_HEADER_START)) {
      // we can fit at least some of the data inside the response buffer.
      ssize_t i_read = pread(
          source_fd, response->metadata.headers_pos,
          ((BUFFER_PACKET_SIZE - HTTP_HEADER_START) - headers->length), offset);
      if (i_read > 0) {
        if (i_read >= length) {
          headers->length += length;
          close(source_fd);
          return send_headers(response, headers);
        } else {
          headers->length += i_read;
          length -= i_read;
          offset += i_read;
        }
      }
    }
    // we need to send the rest seperately.
    if (send_headers(response, headers)) {
      close(source_fd);
      return -1;
    }
  }
  return Server.sendfile(response->metadata.server, response->metadata.fd_uuid,
                         source_fd, offset, length);
}

/**
Closes the connection.
*/
static void close_response(struct HttpResponse* response) {
  return Server.close(response->metadata.server, response->metadata.fd_uuid);
}
