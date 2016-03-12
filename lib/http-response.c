#include "http-response.h"

#include <string.h>
#include <time.h>
#include <stdarg.h>

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
static int write_header(struct HttpResponse*,
                        const char* header,
                        const char* value,
                        size_t value_len);

/**
Prints a string directly to the header's buffer, appending the header
seperator (the new line marker '\r\n' should NOT be printed to the headers
buffer).

Limited error check is performed.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int response_printf(struct HttpResponse*, const char* format, ...);
/**
Sends the headers (if they weren't previously sent).

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int send_response(struct HttpResponse*);
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
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the body and free it's
memory using `free` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int sendfile_req(struct HttpResponse*, FILE* pf, size_t length);
/**
Closes the connection.
*/
static void close_response(struct HttpResponse*);

/******************************************************************************
The API gateway
*/
struct ___HttpResponse_class___ HttpResponse = {
    .destroy_pool = destroy_pool,
    .create_pool = create_pool,
    .new = new_response,
    .destroy = destroy_response,
    .pool_limit = 64,
    .reset = reset,
    .status_str = status_str,
    .write_header_cstr = write_header_cstr,
    .write_header = write_header,
    .printf = response_printf,
    .send = send_response,
    .write_body = write_body,
    .write_body_move = write_body_move,
    .sendfile = sendfile_req,
    .close = close_response,
};

/******************************************************************************
The response object pool.
*/

static object_pool response_pool;

void* malloc_response() {
  return malloc(sizeof(struct HttpResponse));
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
  ObjectPool.destroy(response_pool);
  response_pool = NULL;
}
/**
Creates the response object pool (unless it already exists).
*/
static void create_pool(void) {
  if (!response_pool)
    response_pool = ObjectPool.new_dynamic(malloc_response, free, 4);
}

/**
Creates a new response object or recycles a response object from the response
pool.

returns NULL on failuer, or a pointer to a valid response object.
*/
static struct HttpResponse* new_response(struct HttpRequest* request) {
  struct HttpResponse* response =
      response_pool ? ObjectPool.pop(response_pool) : malloc_response();
  if (!response)
    return NULL;
  response->metadata.classUUID = new_response;
  reset(response, request);
  return response;
}
/**
Destroys the response object or places it in the response pool for recycling.
*/
static void destroy_response(struct HttpResponse* response) {
  if (response->metadata.classUUID != new_response)
    return;
  if (!response_pool ||
      (ObjectPool.count(response_pool) >= HttpResponse.pool_limit))
    free(response);
  else
    ObjectPool.push(response_pool, response);
}
/**
Clears the HttpResponse object, linking it with an HttpRequest object (which
will be used to set the server's pointer and socket fd).
*/
static void reset(struct HttpResponse* response, struct HttpRequest* request) {
  response->content_length = 0;
  response->status = 200;
  response->metadata.headers_sent = 0;
  response->metadata.headers_pos = response->header_buffer + HTTP_HEADER_START;
  response->metadata.fd = request->sockfd;
  response->metadata.server = request->server;
  response->date = Server.reactor(response->metadata.server)->last_tick;
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
  return write_header(response, header, value, strlen(value));
}
/**
Writes a header to the response. This function writes only the requested
number of bytes from the header value and should be used when the header value
doesn't contain a NULL terminating byte.

If the headers were already sent, new headers cannot be sent and the function
will return -1. On success, the function returns 0.
*/
static int write_header(struct HttpResponse* response,
                        const char* header,
                        const char* value,
                        size_t value_len) {
  // check for space in the buffer
  size_t header_len = strlen(header);
  if (response->metadata.headers_sent ||
      (header_len + value_len + 4 +
           (response->metadata.headers_pos - response->header_buffer) >=
       HTTP_HEAD_MAX_SIZE))
    return -1;
  // review special headers
  if (!strcasecmp("Date", header)) {
    response->metadata.date_written = 1;
  } else if (!strcasecmp("Connection", header)) {
    response->metadata.connection_written = 1;
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
Prints a string directly to the header's buffer, appending the header
seperator (the new line marker '\r\n' should NOT be printed to the headers
buffer).

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int response_printf(struct HttpResponse* response, const char* format, ...) {
  if (response->metadata.headers_sent)
    return -1;
  size_t max_len = HTTP_HEAD_MAX_SIZE -
                   (response->metadata.headers_pos - response->header_buffer);
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
  if (response->metadata.headers_sent)
    return -1;
  char* status = HttpStatus.to_s(response->status);
  if (!status)
    return -1;

  // write the content length header, if relevant
  if (response->content_length) {
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
    struct tm t;
    gmtime_r(&response->date, &t);
    // response->metadata.headers_pos +=
    //     strftime(response->metadata.headers_pos, 100,
    //              "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", &t);
    memcpy(response->metadata.headers_pos, "Date: ", 6);
    response->metadata.headers_pos += 6;
    response->metadata.headers_pos += strftime(
        response->metadata.headers_pos, 100, "%a, %d %b %Y %H:%M:%S", &t);
    memcpy(response->metadata.headers_pos, " GMT\r\n", 6);
    response->metadata.headers_pos += 6;
  }
  // write the keep-alive (connection) header, if missing
  if (!response->metadata.connection_written) {
    memcpy(response->metadata.headers_pos,
           "Connection: keep-alive\r\n"
           "Keep-Alive: timeout=2\r\n",
           47);
    response->metadata.headers_pos += 47;
  }

  // write the headers completion marker (empty line - `\r\n`)
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';

  // write the status string is "HTTP/1.1 xxx <...>\r\n" length == 15 +
  // strlen(status)
  int start = HTTP_HEADER_START - (15 + strlen(status));
  sprintf(response->header_buffer + start, "HTTP/1.1 %d %s\r", response->status,
          status);
  // we need to seperate the writing because sprintf prints a NULL terminator.
  response->header_buffer[HTTP_HEADER_START - 1] = '\n';
  // mark headers as sent
  response->metadata.headers_sent = 1;
  // // debug
  // for (int i = start; i < response->metadata.headers_length +
  // HTTP_HEADER_START;
  //      i++) {
  //   fprintf(stderr, "* %d: %x (%.6s)\n", i, response->header_buffer[i],
  //           response->header_buffer + i);
  // }
  // write data to network
  return Server.write(
      response->metadata.server, response->metadata.fd,
      response->header_buffer + start,
      (response->metadata.headers_pos - response->header_buffer) - start);
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
  send_response(response);
  return Server.write(response->metadata.server, response->metadata.fd,
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
  send_response(response);
  return Server.write_move(response->metadata.server, response->metadata.fd,
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
static int sendfile_req(struct HttpResponse* response,
                        FILE* pf,
                        size_t length) {
  if (!response->content_length)
    response->content_length = length;
  send_response(response);
  return Server.sendfile(response->metadata.server, response->metadata.fd, pf);
}
/**
Closes the connection.
*/
static void close_response(struct HttpResponse* response) {
  return Server.close(response->metadata.server, response->metadata.fd);
}
