#include "http-response.h"

#include <string.h>
#include <time.h>
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
#define HTTP_HEADER_START 61

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
  response->metadata.headers_length = 0;
  response->metadata.fd = request->sockfd;
  response->metadata.server = request->server;
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
      (header_len + value_len + 4 + response->metadata.headers_length >=
       HTTP_HEAD_MAX_SIZE))
    return -1;
  // review special headers
  if (!strcasecmp("Date", header)) {
    response->metadata.date_written = 1;
  } else if (!strcasecmp("Connection", header)) {
    response->metadata.connection_written = 1;
  }
  // write the header name to the buffer
  while (header_len--)
    response->header_buffer[HTTP_HEADER_START +
                            response->metadata.headers_length++] = *(header++);
  // write the header to value seperator (`: `)
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      ':';
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      ' ';
  // write the header's value to the buffer
  while (value_len--)
    response->header_buffer[HTTP_HEADER_START +
                            response->metadata.headers_length++] = *(value++);
  // write the header seperator (`\r\n`)
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      '\r';
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      '\n';
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
  if (response->content_length && response->content_length <= 2147483648 &&
      response->metadata.headers_length <= (HTTP_HEAD_MAX_SIZE - 48)) {
    response->metadata.headers_length +=
        sprintf(response->header_buffer + HTTP_HEADER_START +
                    response->metadata.headers_length,
                "Content-Length: %lu\r\n", response->content_length);
  }
  // write the date, if missing
  if (!response->metadata.date_written &&
      response->metadata.headers_length <= (HTTP_HEAD_MAX_SIZE - 48)) {
    struct tm t;
    gmtime_r(&Server.reactor(response->metadata.server)->last_tick, &t);
    response->metadata.headers_length +=
        strftime(response->header_buffer + HTTP_HEADER_START +
                     response->metadata.headers_length,
                 HTTP_HEAD_MAX_SIZE - response->metadata.headers_length,
                 "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", &t);
  }
  // write the keep-alive (connection) header, if missing
  if (!response->metadata.connection_written &&
      response->metadata.headers_length <= (HTTP_HEAD_MAX_SIZE - 47)) {
    response->metadata.headers_length +=
        sprintf(response->header_buffer + HTTP_HEADER_START +
                    response->metadata.headers_length,
                "Connection: keep-alive\r\n"
                "Keep-Alive: timeout=2\r\n");
  }

  // write the headers completion marker (empty line - `\r\n`)
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      '\r';
  response
      ->header_buffer[HTTP_HEADER_START + response->metadata.headers_length++] =
      '\n';

  // write the status string is "HTTP/1.1 xxx <...>\r\n" length == 15 +
  // strlen(status)
  int start = HTTP_HEADER_START - (15 + strlen(status));
  if (start < 0) {
    fprintf(stderr, "Status string pre-buffer mis-callculated. fix library\n");
    return -1;
  }
  sprintf(response->header_buffer + start, "HTTP/1.1 %d %s\r", response->status,
          status);
  // we need to seperate the writing because sprintf prints a NULL terminator.
  response->header_buffer[HTTP_HEADER_START - 1] = '\n';
  // mark headers as sent
  response->metadata.headers_sent = 1;
  // write data to network
  return Server.write(
      response->metadata.server, response->metadata.fd,
      response->header_buffer + start,
      response->metadata.headers_length + HTTP_HEADER_START - start);
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
