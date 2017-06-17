#include "redis_connection.h"
#include "fio_list.h"
#include "spnlock.inc"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>

/* *****************************************************************************
Memory and Data structures - The protocol object.
***************************************************************************** */
#ifndef REDIS_POOL_SIZES /* per protocol */
#define REDIS_POOL_SIZES 256
#endif

#ifndef REDIS_READ_BUFFER /* during network data events */
#define REDIS_READ_BUFFER 2048
#endif

/* a protocol ID string */
static const char *REDIS_PROTOCOL_ID =
    "Redis Protocol for the facil.io framework";
/* Pings */
#define REDIS_PING_LEN 24
static const char REDIS_PING_STR[] = "*2\r\n"
                                     "$4\r\nPING\r\n"
                                     "$24\r\nfacil.io connection PING\r\n";
static const char REDIS_PING_PAYLOAD[] = "facil.io connection PING";

typedef struct {
  protocol_s protocol;
  struct redis_context_args *settings;
} redis_protocol_s;

/* *****************************************************************************
The Protocol Context
***************************************************************************** */
#undef redis_create_context
void *redis_create_context(struct redis_context_args args) {
  if (!args.on_message || !args.parser) {
    fprintf(stderr, "A Redix connection context requires both an `on_message` "
                    "callback and a parser.\n");
    exit(EINVAL);
  }
  struct redis_context_args *c = malloc(sizeof(*c));
  *c = args;
  return c;
}

/* *****************************************************************************
The Protocol Callbacks
***************************************************************************** */

void redis_protocol_cleanup(intptr_t uuid, void *settings) {
  struct redis_context_args *s = settings;
  if (s->on_close)
    s->on_close(uuid, s->udata);
  if (s->autodestruct)
    free(settings);
}

static void redis_on_close_client(intptr_t uuid, protocol_s *pr) {
  redis_protocol_s *r = (redis_protocol_s *)pr;
  // if (r->settings->on_close)
  //   r->settings->on_close(uuid, r->settings->udata);
  redis_protocol_cleanup(uuid, r->settings);
  free(r);
}

static void redis_on_close_server(intptr_t uuid, protocol_s *pr) {
  redis_protocol_s *r = (redis_protocol_s *)pr;
  if (r->settings->on_close)
    r->settings->on_close(uuid, r->settings->udata);
  free(r);
}

/** called when a connection's timeout was reached */
static void redis_ping(intptr_t uuid, protocol_s *pr) {
  /* We cannow write directly to the socket in case `redis_send` has scheduled
   * callbacks. */
  sock_write2(.uuid = uuid, .buffer = REDIS_PING_STR,
              .length = sizeof(REDIS_PING_STR) - 1, .move = 1,
              .dealloc = SOCK_DEALLOC_NOOP);
  (void)pr;
}

/** called when a data is available, but will not run concurrently */
static void redis_on_data_deferred(intptr_t uuid, protocol_s *pr, void *d);
static void redis_on_data(intptr_t uuid, protocol_s *pr) {
  redis_protocol_s *r = (redis_protocol_s *)pr;
  uint8_t buffer[REDIS_READ_BUFFER];
  ssize_t len, limit, pos;
  resp_object_s *msg;
  limit = len = sock_read(uuid, buffer, REDIS_READ_BUFFER);
  if (len <= 0)
    return;
  pos = 0;
  while (len) {
    msg = resp_parser_feed(r->settings->parser, buffer + pos, (size_t *)&len);
    if (!len && !msg) {
      fprintf(stderr, "ERROR: (RESP Parser) Bad input (%lu):\n%s\n", limit,
              buffer);
      /* we'll simply ignore bad input. skip a line. */
      for (; pos < limit; pos++) {
        len++;
        if (buffer[pos] == '\n') {
          pos++;
          len++;
          break; /* from `for` loop */
        }
      }
      continue; /* while loop */
    }
    if (msg) {
      if ((msg->type == RESP_STRING &&
           resp_obj2str(msg)->len == REDIS_PING_LEN &&
           !memcmp(resp_obj2str(msg)->string, REDIS_PING_PAYLOAD,
                   REDIS_PING_LEN)) ||
          (msg->type == RESP_ARRAY &&
           resp_obj2arr(msg)->array[0]->type == RESP_STRING &&
           resp_obj2str(resp_obj2arr(msg)->array[0])->len == 4 &&
           resp_obj2arr(msg)->array[1]->type == RESP_STRING &&
           resp_obj2str(resp_obj2arr(msg)->array[1])->len == REDIS_PING_LEN &&
           !strncasecmp(
               (char *)resp_obj2str(resp_obj2arr(msg)->array[0])->string,
               "pong", 4) &&
           !memcmp((char *)resp_obj2str(resp_obj2arr(msg)->array[0])->string,
                   REDIS_PING_PAYLOAD, REDIS_PING_LEN))) {
        /* an internal ping, do not forward. */
      } else
        r->settings->on_message(uuid, msg, r->settings->udata);
      resp_free_object(msg);
      msg = NULL;
    }
    if (len == limit) {
      /* fragment events, it's edge polling, so we need to read everything */
      facil_defer(.uuid = uuid, .task = redis_on_data_deferred,
                  .task_type = FIO_PR_LOCK_TASK);
      return;
    }
    pos += len;
    limit = len = limit - len;
  }
}
static void redis_on_data_deferred(intptr_t uuid, protocol_s *pr, void *d) {
  if (!pr || pr->service != REDIS_PROTOCOL_ID)
    return;
  redis_on_data(uuid, pr);
  (void)d;
}

static void redis_on_open(intptr_t uuid, protocol_s *pr, void *d) {
  redis_protocol_s *r = (void *)pr;
  facil_set_timeout(uuid, r->settings->ping);
  r->settings->on_open(uuid, r->settings->udata);
  (void)d;
}

/**
 * This function is used as a function pointer for the `facil_connect` and
 * calls (the `on_connect` callback).
 */
protocol_s *redis_create_client_protocol(intptr_t uuid, void *settings) {
  redis_protocol_s *r = malloc(sizeof(*r));
  *r = (redis_protocol_s){
      .protocol =
          {
              .service = REDIS_PROTOCOL_ID,
              .on_data = redis_on_data,
              .on_close = redis_on_close_client,
              .ping = redis_ping,
          },
      .settings = settings,
  };
  facil_set_timeout(uuid, r->settings->ping);
  if (r->settings->on_open)
    facil_defer(.task = redis_on_open, .uuid = uuid);
  return &r->protocol;
  (void)uuid;
}

/**
 * This function is used as a function pointer for the `facil_listen` calls (the
 * `on_open` callbacks).
 */
protocol_s *redis_create_server_protocol(intptr_t uuid, void *settings) {
  redis_protocol_s *r = malloc(sizeof(*r));
  *r = (redis_protocol_s){
      .protocol =
          {
              .service = REDIS_PROTOCOL_ID,
              .on_data = redis_on_data,
              .on_close = redis_on_close_server,
              .ping = redis_ping,
          },
      .settings = settings,
  };
  facil_set_timeout(uuid, r->settings->ping);
  if (r->settings->on_open)
    facil_defer(.task = redis_on_open, .uuid = uuid);
  return &r->protocol;
  (void)uuid;
}
