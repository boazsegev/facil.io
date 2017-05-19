#include "http1_response.h"
#include "spnlock.inc"

/* *****************************************************************************
Response object & Initialization
***************************************************************************** */

typedef struct {
  http_response_s response;
  size_t buffer_start;
  size_t buffer_end;
  char buffer[HTTP1_MAX_HEADER_SIZE];
} http1_response_s;

static struct {
  spn_lock_i lock;
  uint8_t init;
  http1_response_s *next;
  http1_response_s pool_mem[HTTP1_POOL_SIZE];
} http1_response_pool = {.lock = SPN_LOCK_INIT, .init = 0};

static inline void http1_response_clear(http1_response_s *rs,
                                        http_request_s *request) {
  rs->response = (http_response_s){
      .http_version = HTTP_V1, .request = request, .fd = request->fd,
  };
  rs->buffer_end = rs->buffer_start = 12;
}
/** Creates / allocates a protocol version's response object. */
http_response_s *http1_response_create(http_request_s *request) {
  http1_response_s *rs;
  spn_lock(&http1_response_pool.lock);
  if (!http1_response_pool.next)
    goto use_malloc;
  rs = http1_response_pool.next;
  http1_response_pool.next = (void *)rs->response.request;
  spn_unlock(&http1_response_pool.lock);
  http1_response_clear(rs, request);
  return (http_response_s *)rs;
use_malloc:
  if (http1_response_pool.init == 0)
    goto initialize;
  spn_unlock(&http1_response_pool.lock);
  rs = malloc(sizeof(*rs));
  http1_response_clear(rs, request);
  return (http_response_s *)rs;
initialize:
  for (size_t i = 1; i < (HTTP1_POOL_SIZE - 1); i++) {
    http1_response_pool.pool_mem[i].response.request =
        (void *)(http1_response_pool.pool_mem + (i + 1));
  }
  http1_response_pool.next = http1_response_pool.pool_mem + 1;
  spn_unlock(&http1_response_pool.lock);
  http1_response_clear(http1_response_pool.pool_mem, request);
  return (http_response_s *)http1_response_pool.pool_mem;
}

/** Destroys the response object. No data is sent.*/
void http1_response_destroy(http_response_s *rs) {
  if (rs->request_dupped)
    http_request_destroy(rs->request);
  if ((uintptr_t)rs < (uintptr_t)http1_response_pool.pool_mem ||
      (uintptr_t)rs >=
          (uintptr_t)(http1_response_pool.pool_mem + HTTP1_POOL_SIZE))
    goto use_free;
  spn_lock(&http1_response_pool.lock);
  rs->request = (void *)http1_response_pool.next;
  http1_response_pool.next = (void *)rs;
  return;
use_free:
  free(rs);
  return;
}
/** Sends the data and destroys the response object.*/
void http1_response_finish(http_response_s *);

/* *****************************************************************************
Writing data to the response object
***************************************************************************** */

/**
Writes a header to the response. This function writes only the requested
number of bytes from the header name and the requested number of bytes from
the header value. It can be used even when the header name and value don't
contain NULL terminating bytes by passing the `.name_len` or `.value_len` data
in the `http_headers_s` structure.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int http1_response_write_header_fn(http_response_s *, http_header_s header);

/**
Set / Delete a cookie using this helper function.

To set a cookie, use (in this example, a session cookie):

    http_response_set_cookie(response,
            .name = "my_cookie",
            .value = "data");

To delete a cookie, use:

    http_response_set_cookie(response,
            .name = "my_cookie",
            .value = NULL);

This function writes a cookie header to the response. Only the requested
number of bytes from the cookie value and name are written (if none are
provided, a terminating NULL byte is assumed).

Both the name and the value of the cookie are checked for validity (legal
characters), but other properties aren't reviewed (domain/path) - please make
sure to use only valid data, as HTTP imposes restrictions on these things.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int http1_response_set_cookie(http_response_s *, http_cookie_s);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http1_response_write_body(http_response_s *, const char *body,
                              size_t length);

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The server's outgoing buffer will take ownership of the file and close it
using `fclose` once the data was sent.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http1_response_sendfile(http_response_s *, int source_fd, off_t offset,
                            size_t length);
