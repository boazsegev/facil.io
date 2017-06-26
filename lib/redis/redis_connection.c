/*
Copyright: Boaz segev, 2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#include "spnlock.inc"

#include "fio_list.h"
#include "redis_connection.h"
#include <errno.h>
#include <math.h>
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
  unsigned authenticated : 1;
} redis_protocol_s;

static inline size_t ul2a(char *dest, size_t num) {
  uint8_t digits = 1;
  size_t tmp = num;
  while ((tmp /= 10))
    ++digits;

  if (dest) {
    dest += digits;
    *(dest--) = 0;
  }
  for (size_t i = 0; i < digits; i++) {
    num = num - (10 * (tmp = (num / 10)));
    if (dest)
      *(dest--) = '0' + num;
    num = tmp;
  }
  return digits;
}

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
  if (args.auth) {
    if (!args.auth_len)
      args.auth_len = strlen(args.auth);
    args.auth_len++;
  }
  struct redis_context_args *c = malloc(sizeof(*c) + args.auth_len);
  *c = args;
  if (args.auth) {
    c->auth_len--;
    c->auth = (char *)(c + 1);
    memcpy(c->auth, args.auth, c->auth_len);
    c->auth[c->auth_len] = 0;
  }
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
      if (r->authenticated) {
        r->authenticated--;
        if (msg->type != RESP_OK) {
          if (msg->type == RESP_ERR) {
            fprintf(stderr,
                    "ERROR: (RedisConnection) Authentication FAILED.\n"
                    "        %s\n",
                    resp_obj2str(msg)->string);
          } else {
            fprintf(stderr,
                    "ERROR: (RedisConnection) Authentication FAILED "
                    "(unexpected response %d).\n",
                    msg->type);
          }
        }
      } else if ((msg->type == RESP_STRING &&
                  resp_obj2str(msg)->len == REDIS_PING_LEN &&
                  !memcmp(resp_obj2str(msg)->string, REDIS_PING_PAYLOAD,
                          REDIS_PING_LEN)) ||
                 (msg->type == RESP_ARRAY &&
                  resp_obj2arr(msg)->array[0]->type == RESP_STRING &&
                  resp_obj2str(resp_obj2arr(msg)->array[0])->len == 4 &&
                  resp_obj2arr(msg)->array[1]->type == RESP_STRING &&
                  resp_obj2str(resp_obj2arr(msg)->array[1])->len ==
                      REDIS_PING_LEN &&
                  !strncasecmp(
                      (char *)resp_obj2str(resp_obj2arr(msg)->array[0])->string,
                      "pong", 4) &&
                  !memcmp(
                      (char *)resp_obj2str(resp_obj2arr(msg)->array[0])->string,
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
  if (r->settings->auth) {
    r->authenticated = 1;
    size_t n_len = ul2a(NULL, r->settings->auth_len);
    char *t =
        malloc(r->settings->auth_len + 20 + n_len); /* 19 is probably enough */
    memcpy(t, "*2\r\n$4\r\nAUTH\r\n$", 15);
    ul2a(t + 15, r->settings->auth_len);
    t[15 + n_len] = '\r';
    t[16 + n_len] = '\n';
    memcpy(t + 17 + n_len, r->settings->auth, r->settings->auth_len);
    t[17 + n_len + r->settings->auth_len] = '\r';
    t[18 + n_len + r->settings->auth_len] = '\n';
    t[19 + n_len + r->settings->auth_len] = 0; /* we don't need it, but nice */
    sock_write2(.uuid = uuid, .buffer = t,
                .length = (19 + n_len + r->settings->auth_len), .move = 1);

  } else
    r->authenticated = 0;
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
