#include "http-response.h"

#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

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
int response_printf(struct HttpResponse*, const char* format, ...);
/**
Sends the headers (if they weren't previously sent).

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int send_response(struct HttpResponse*);
/* used by `send_response` and others... */
static char* prep_headers(struct HttpResponse*);
static int send_headers(struct HttpResponse*, char*);

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
Sends the complete file referenced by the `file_path` string.

This function requires that the headers weren't previously sent and that the
file exists.

On failure, the function will return -1. On success, the function returns 0.
*/
static int sendfile_path(struct HttpResponse* response, char* file_path);
/**
Closes the connection.
*/
static void close_response(struct HttpResponse*);

/******************************************************************************
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
    .sendfile = sendfile_req,
    .sendfile2 = sendfile_path,
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
    response_pool = ObjectPool.create_dynamic(malloc_response, free, 4);
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
  if (response->metadata.should_close)
    close_response(response);
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
  response->metadata.connection_written = 0;
  response->metadata.connection_len_written = 0;
  response->metadata.date_written = 0;
  response->metadata.headers_pos = response->header_buffer + HTTP_HEADER_START;
  response->metadata.fd_uuid = request->sockfd;
  response->metadata.server = request->server;
  response->last_modified = response->date =
      Server.reactor(response->metadata.server)->last_tick;
  response->metadata.should_close =
      (request && request->connection &&
       (strcasecmp(request->connection, "close") == 0));
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
           (response->metadata.headers_pos - response->header_buffer) >=
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
          (response->metadata.headers_pos - response->header_buffer) + 96 >
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

static char* prep_headers(struct HttpResponse* response) {
  if (response->metadata.headers_sent)
    return NULL;
  char* status = HttpStatus.to_s(response->status);
  if (!status)
    return NULL;
  // write the content length header, unless forced not to (<0)
  if (response->metadata.connection_len_written == 0 &&
      !(response->content_length < 0)) {
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
    // last-modified header
    gmtime_r(&response->last_modified, &t);
    // response->metadata.headers_pos +=
    //     strftime(response->metadata.headers_pos, 100,
    //              "Last-Modified: %a, %d %b %Y %H:%M:%S GMT\r\n", &t);
    memcpy(response->metadata.headers_pos, "Last-Modified: ", 15);
    response->metadata.headers_pos += 15;
    response->metadata.headers_pos += strftime(
        response->metadata.headers_pos, 100, "%a, %d %b %Y %H:%M:%S", &t);
    memcpy(response->metadata.headers_pos, " GMT\r\n", 6);
    response->metadata.headers_pos += 6;
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
  sprintf(response->header_buffer + start, "HTTP/1.1 %d %s\r", response->status,
          status);
  // we need to seperate the writing because sprintf prints a NULL terminator.
  response->header_buffer[HTTP_HEADER_START - 1] = '\n';
  return response->header_buffer + start;
}
static int send_headers(struct HttpResponse* response, char* start) {
  if (start == NULL)
    return -1;
  // mark headers as sent
  response->metadata.headers_sent = 1;
  // write data to network
  return Server.write(response->metadata.server, response->metadata.fd_uuid,
                      start, response->metadata.headers_pos - start);
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
  char* start = prep_headers(response);
  if (start != NULL) {  // we haven't sent the headers yet
    if ((response->metadata.headers_pos - start + length) <
        (HTTP_HEAD_MAX_SIZE + SMALL_RESPONSE_LIMIT)) {
      // we can fit the data inside the response buffer.
      memcpy(response->metadata.headers_pos, body, length);
      response->metadata.headers_pos += length;
      return send_headers(response, start);
    } else {
      // we need to send the body seperately.
      send_headers(response, start);
    }
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
  send_response(response);
  return Server.write_move(response->metadata.server,
                           response->metadata.fd_uuid, (void*)body, length);
}
/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file and close it using
`close` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
static int sendfile_req(struct HttpResponse* response,
                        FILE* pf,
                        size_t length) {
  if (!response->content_length)
    response->content_length = length;
  send_response(response);
  return Server.sendfile(response->metadata.server, response->metadata.fd_uuid,
                         pf);
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

  FILE* pf;
  struct stat f_data;
  if (stat(file_path, &f_data)) {
    return -1;
  }
  pf = fopen(file_path, "rb");
  if (!pf) {
    return -1;
  }
  response->content_length = f_data.st_size;
  response->last_modified = f_data.st_mtime;
  send_response(response);
  if (Server.sendfile(response->metadata.server, response->metadata.fd_uuid,
                      pf)) {
    fclose(pf);
    return -1;
  }
  return 0;
}
/**
Closes the connection.
*/
static void close_response(struct HttpResponse* response) {
  return Server.close(response->metadata.server, response->metadata.fd_uuid);
}
