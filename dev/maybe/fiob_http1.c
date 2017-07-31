/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "facil.h"
#include "fiobj.h"
#include "http.h"
#include "http1_parser.h"

#include <string.h>

/* *****************************************************************************
Header File Data
***************************************************************************** */

#ifndef DEFAULT_MAX_MIDDLEWARE_COUNT
/** Sets the default maximum number of middleware functions in each category. */
#define DEFAULT_MAX_MIDDLEWARE_COUNT 64
#endif
#ifndef DEFAULT_MAX_MESSAGE_SIZE
/** Sets the default maximum message size. */
#define DEFAULT_MAX_MESSAGE_SIZE 33554432
#endif

/** An opaque service type. */
typedef struct fioapp_s fioapp_s;

/** Manages protocol settings for the HTTP protocol */
struct fioapp_settings_s {
  /**
   * The maximum size of incoming message data, in bytes.
   *
   * This applies to both HTTP and Websocket messages but doesn't include HTTP
   * header data or cookies.
   *
   * Defaults to `DEFAULT_MAX_MESSAGE_SIZE` (33,554,432 bytes).
   *
   * It is recomended that this value is *reduced* and messaged are fragmented
   * when possible.
   */
  size_t max_message;
  /**
   * Logging flag - set to TRUE to log requests and service events.
   */
  uint8_t log;
  /** Short connection timeout, for HTTP/1.x. Defaults to ~5 seconds.*/
  uint8_t timeout_short;
  /** Persistent connection timeout, for Websockets etc'. Defaults to ~40
   * seconds.*/
  uint8_t timeout_long;
  /** Opaque user data for the optional `set_rw_hooks`. */
  void *rw_udata;
  /**
   * Allows a an implementation for the transport layer (i.e. TLS) without
   * effecting the HTTP protocol.
   */
  sock_rw_hook_s *(*set_rw_hooks)(intptr_t fduuid, void *rw_udata);
  /**
   * A cleanup callback for the `rw_udata`.
   */
  void (*on_finish_rw)(intptr_t uuid, void *rw_udata);
};

extern fiobj_s *PATH_INFO;
extern fiobj_s *PATH_ARRAY;
extern fiobj_s *QUERY;
extern fiobj_s *HEADERS;
extern fiobj_s *METHOD;
extern fiobj_s *HTTP_VERSION;
extern fiobj_s *BODY;

fioapp_s *fioapp_new(void);
int fioapp_middleware_conn(fioapp_s *app,
                           int (**func)(fiobj_s *dest, fiobj_s *source,
                                        void *udata),
                           void *udata);
int fioapp_middleware_req_http(fioapp_s *app,
                               int (**func)(fiobj_s *dest, fiobj_s *source,
                                            void *udata),
                               void *udata);
int fioapp_middleware_req(fioapp_s *app,
                          int (**func)(fiobj_s *dest, fiobj_s *source,
                                       void *udata),
                          void *udata);
void fioapp_destroy(fioapp_s *app);

/* *****************************************************************************
Constants Etc'
***************************************************************************** */
char *FIOBJ_HTTP1_Protocol_String = "facil_fiobj_http/1.1_protocol";

fiobj_s *PATH_INFO;
fiobj_s *PATH_ARRAY;
fiobj_s *QUERY;
fiobj_s *HEADERS;
fiobj_s *METHOD;
fiobj_s *HTTP_VERSION;
fiobj_s *BODY;

/* *****************************************************************************
The Service object
***************************************************************************** */
/**
 * Middleware contain a callback and a user opaque pointer.
 *
 * If the callback returns a non-zero value, no further processing is performed
 * (except for resource cleanup).
 */
struct middleware_info_s {
  int (**func)(fiobj_s *dest, fiobj_s *source, void *udata);
  void *udata;
  struct middleware_info_s *next;
};

struct fioapp_s {
  struct fioapp_settings_s settings;
  struct {
    struct middleware_info_s **http;
    struct middleware_info_s **websocket;
    struct middleware_info_s **request;
    size_t http_len;
    size_t websocket_len;
    size_t request_len;
  } middleware;
};
/* *****************************************************************************
The HTTP/1.x Protocol object
***************************************************************************** */
#define POOL_SIZE 64
typedef struct fiobj_fiobj_http1_protocol_s {
  protocol_s protocol;
  struct fiobj_fiobj_http1_protocol_s *next;
  struct fioapp_settings_s *settings;
  fiobj_s *request;
  fiobj_s *headers;
  fiobj_s *body;
  uintptr_t uuid;
  /* parsing helpers. */
  http1_parser_s parser;
  ssize_t len;
  fiobj_s *buffer;
} fiobj_http1_protocol_s;

static spn_lock_i lock;
static fiobj_http1_protocol_s mem_pool[POOL_SIZE];
static fiobj_http1_protocol_s *pool;

/* *****************************************************************************
Alloc, Dealloc and Initialization
***************************************************************************** */

static void destroy_data(void) {
  fiobj_free(PATH_INFO);
  fiobj_free(PATH_ARRAY);
  fiobj_free(QUERY);
  fiobj_free(HEADERS);
  fiobj_free(METHOD);
  fiobj_free(HTTP_VERSION);
  fiobj_free(BODY);
}

static inline fiobj_http1_protocol_s *initialize_data(void) {
  if (BODY)
    return NULL;
  /* Initialize symbols */
  PATH_INFO = fiobj_sym_new("PATH_INFO", 9);
  PATH_ARRAY = fiobj_sym_new("PATH_ARRAY", 9);
  QUERY = fiobj_sym_new("QUERY", 5);
  HEADERS = fiobj_sym_new("HEADERS", 7);
  METHOD = fiobj_sym_new("METHOD", 6);
  HTTP_VERSION = fiobj_sym_new("HTTP_VERSION", 12);
  BODY = fiobj_sym_new("BODY", 4);
  atexit(destroy_data);
  /* Initialize memory pool */
  for (fiobj_http1_protocol_s *i = mem_pool + 1; i < mem_pool + POOL_SIZE - 1;
       i++) {
    i->next = i + 1;
  }
  mem_pool[POOL_SIZE - 1].next = NULL;
  pool = mem_pool + 1;
  return mem_pool;
}

static inline void protocol_free(fiobj_http1_protocol_s *pr) {
  /* free objects except headers and body (they're nested in the request) */
  if (pr->buffer)
    fiobj_free(pr->buffer);
  if (pr->request)
    fiobj_free(pr->request);
  /* free the container or replace it in the pool */
  if (pr >= mem_pool && pr < mem_pool + POOL_SIZE) {
    spn_lock(&lock);
    pr->next = pool;
    pool = pr->next;
    spn_unlock(&lock);
  } else {
    free(pr);
  }
}

static inline fiobj_http1_protocol_s *protocol_alloc(void *settings) {
  fiobj_http1_protocol_s *pr = NULL;
  spn_lock(&lock);
  if (pool) {
    pr = pool;
    pool = pool->next;
  } else if (!BODY) {
    pr = initialize_data();
  }
  spn_unlock(&lock);
  if (!pr)
    pr = malloc(sizeof(*pr));
  *pr = (fiobj_http1_protocol_s){.settings = settings, .parser = {.udata = pr}};
  return pr;
}

/* *****************************************************************************
Parser callbacks
***************************************************************************** */

// /** REQUIRED: the parser object that manages the parser's state. */
// http1_parser_s *parser;
// /** REQUIRED: the data to be parsed. */
// void *buffer;
// /** REQUIRED: the length of the data to be parsed. */
// size_t length;

/** called when a request was received. */
static int on_request(http1_parser_s *parser) {
  fiobj_http1_protocol_s *pr = parser->udata;
}
/** called when a response was received. */
static int on_response(http1_parser_s *parser) {
  return -1;
  (void)parser;
}
/** called when a request method is parsed. */
static int on_method(http1_parser_s *parser, char *method, size_t method_len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  if (pr->request)
    fiobj_free(pr->request);
  pr->request = fiobj_hash_new();
  fiobj_hash_set(pr->request, METHOD, fiobj_str_new(method, method_len));
  return 0;
}
/** called when a response status is parsed. the status_str is the string
 * without the prefixed numerical status indicator.*/
static int on_status(http1_parser_s *parser, size_t status, char *status_str,
                     size_t len) {
  return -1;
  (void)parser;
  (void)len;
  (void)status;
  (void)status_str;
}
/** called when a request path (excluding query) is parsed. */
static int on_path(http1_parser_s *parser, char *path, size_t path_len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  fiobj_hash_set(pr->request, PATH_INFO, fiobj_str_new(path, path_len));
  return 0;
}
/** called when a request path (excluding query) is parsed. */
static int on_query(http1_parser_s *parser, char *query, size_t query_len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  fiobj_hash_set(pr->request, QUERY, fiobj_str_new(query, query_len));
  return 0;
}
/** called when a the HTTP/1.x version is parsed. */
static int on_http_version(http1_parser_s *parser, char *version, size_t len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  fiobj_hash_set(pr->request, HTTP_VERSION, fiobj_str_new(version, len));
  return 0;
}
/** called when a header is parsed. */
static int on_header(http1_parser_s *parser, char *name, size_t name_len,
                     char *data, size_t data_len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  fiobj_s *tmp = fiobj_sym_new(name, name_len);
  fiobj_hash_set(pr->headers, tmp, fiobj_str_new(data, data_len));
  fiobj_free(tmp);
  return 0;
}

/** called when a body chunk is parsed. */
static int on_body_chunk(http1_parser_s *parser, char *data, size_t data_len) {
  fiobj_http1_protocol_s *pr = parser->udata;
  if (parser->state.read == 0) {
    if (parser->state.content_length > 0) {
      if ((size_t)parser->state.content_length > pr->settings->max_message)
        return -1;
      pr->body = fiobj_str_buf(parser->state.content_length);
    } else {
      pr->body = fiobj_str_buf(4096);
    }
    fiobj_hash_set(pr->request, BODY, pr->body);
  }
  fiobj_str_write(pr->body, data, data_len);
  if (fiobj_obj2cstr(pr->body).len > pr->settings->max_message)
    return -1;
  return 0;
}

/** called when a protocol error occured. */
static int on_error(http1_parser_s *parser) {
  fiobj_http1_protocol_s *pr = parser->udata;
  /* TODO: return HTTP response "Entity too big" */

  /* free resources */
  fiobj_free(pr->request);
  pr->request = pr->headers = pr->body = NULL;
  return 0;
}

/* *****************************************************************************
Protocol callbacks
***************************************************************************** */
#define HTTP1_READ_BUFFER_SIZE 8096
/** called when a data is available, but will not run concurrently */
static void fio_http1_on_data(intptr_t uuid, protocol_s *pr_) {
  fiobj_http1_protocol_s *pr = (fiobj_http1_protocol_s *)pr_;
  char buffer[HTTP1_READ_BUFFER_SIZE];
  char *pos = buffer;
  size_t unread = 0;
  uint8_t reschedule = 1;
  if (pr->buffer) {
    /* leftover data from last "run" */
    fio_cstr_s b = fiobj_obj2cstr(pr->buffer);
    memcpy(buffer, b.buffer, b.len);
    unread += b.len;
    fiobj_free(pr->buffer);
    pr->buffer = NULL;
  }
  {
    /* read data from socket, if any */
    const ssize_t tmp =
        sock_read(uuid, buffer, HTTP1_READ_BUFFER_SIZE - unread);
    if (tmp <= 0)
      goto finish;
    unread += (size_t)tmp;
  }
  {
    /* consume data by parser */
    const size_t consumed =
        http1_fio_parser(.buffer = buffer, .length = unread,
                         .parser = &pr->parser, .on_request = on_request,
                         .on_response = on_response, .on_method = on_method,
                         .on_status = on_status, .on_path = on_path,
                         .on_query = on_query,
                         .on_http_version = on_http_version,
                         .on_header = on_header, .on_body_chunk = on_body_chunk,
                         .on_error = on_error, );
    unread -= consumed;
    pos += consumed;
  }
  /* readched leftover buffer limit? */
  if (unread >= HTTP1_READ_BUFFER_SIZE)
    goto error;
  /* schedule more data reading */
  facil_force_event(uuid, FIO_EVENT_ON_DATA);

finish:
  if (unread) {
    /* store leftover data */
    pr->buffer = fiobj_str_new(pos, unread);
  }
  return;

error:
  on_error(&pr->parser);
}

/** called when the connection was closed, but will not run concurrently */
static void fio_http1_on_close(intptr_t uuid, protocol_s *pr_) {
  fiobj_http1_protocol_s *pr = (fiobj_http1_protocol_s *)pr_;
  protocol_free(pr);
  return;
  (void)uuid;
}

/** called when the socket is ready to be written to. */
// static void fio_http1_on_ready(intptr_t uuid, protocol_s *pr_);

/* *****************************************************************************
Listening callback
***************************************************************************** */

static protocol_s *fio_http1_on_open(intptr_t fduuid, void *udata) {
  fiobj_http1_protocol_s *pr = protocol_alloc(udata);
  facil_set_timeout(fduuid, pr->settings->timeout_short);
  pr->uuid = fduuid;
  return &pr->protocol;
}
