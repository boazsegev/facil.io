/*
copyright: Boaz Segev, 2016-2019
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <http.h>             /* includes websockets.h */
#include <websocket_parser.h> /* parser API and callback stubs */

/** Sets the initial buffer size. (4Kb)*/
#define WS_INITIAL_BUFFER_SIZE 4096UL

/* *****************************************************************************
The Websockets Protocol Callbacks - Definitions
***************************************************************************** */

/** Called when a data is available, but will not run concurrently */
static void websocket_on_data(intptr_t uuid, fio_protocol_s *protocol);
/** Called when a data is available, but will not run concurrently (first use)
 */
static void websocket_on_data_first(intptr_t uuid, fio_protocol_s *protocol);
/** called once all pending `fio_write` calls are finished. */
static void websocket_on_ready(intptr_t uuid, fio_protocol_s *protocol);
/**  Called when the server is shutting down. */
static uint8_t websocket_on_shutdown(intptr_t uuid, fio_protocol_s *protocol);
/** Called when the connection was closed, but will not run concurrently */
static void websocket_on_close(intptr_t uuid, fio_protocol_s *protocol);
/** called when a connection's timeout was reached */
static void websocket_ping(intptr_t uuid, fio_protocol_s *protocol);

/* *****************************************************************************
The Websockets Protocol
***************************************************************************** */

struct ws_s {
  fio_protocol_s pr;
  void (*on_message)(ws_s *ws, fio_str_info_s msg, uint8_t is_text);
  void (*on_open)(ws_s *ws);
  void (*on_ready)(ws_s *ws);
  void (*on_shutdown)(ws_s *ws);
  void (*on_close)(intptr_t uuid, void *udata);
  void *udata;
  intptr_t uuid;
  size_t max_msg_size;
  struct {
    char *buf;
    uint64_t len;
    uint64_t capa;
  } buf, msg;
  uint8_t is_client;
};

FIO_IFUNC ws_s *
ws___new(intptr_t uuid, websocket_settings_s args, http_settings_s *hset) {
  ws_s *ws = malloc(sizeof(*ws));
  *ws = (ws_s){
      .pr =
          {
              .on_data = websocket_on_data_first,
              .on_ready = (args.on_ready ? websocket_on_ready : NULL),
              .on_shutdown = (args.on_shutdown ? websocket_on_shutdown : NULL),
              .on_close = websocket_on_close,
              .ping = websocket_ping,
          },
      .on_message = args.on_message,
      .on_open = args.on_open,
      .on_ready = args.on_ready,
      .on_shutdown = args.on_shutdown,
      .on_close = args.on_close,
      .udata = args.udata,
      .uuid = uuid,
      .max_msg_size =
          (hset->ws_max_msg_size ? hset->ws_max_msg_size : (size_t)(-1)),
      .buf =
          {
              .capa = WS_INITIAL_BUFFER_SIZE,
          },
      .is_client = ((hset && hset->is_client) ? 1 : 0),
  };
  fio_timeout_set(uuid, hset->ws_timeout);
  FIO_ASSERT_ALLOC(ws);
  if (WS_INITIAL_BUFFER_SIZE) {
    ws->buf.buf = malloc(WS_INITIAL_BUFFER_SIZE);
    FIO_ASSERT_ALLOC(ws->buf.buf);
  }
  return ws;
}

static void ws___free(ws_s *ws) {
  free(ws->buf.buf);
  fio_free(ws->msg.buf);
  free(ws);
}

/* *****************************************************************************
The Websockets Protocol Callbacks - Implementation
***************************************************************************** */

/** called once all pending `fio_write` calls are finished. */
static void websocket_on_ready(intptr_t uuid, fio_protocol_s *protocol) {
  ws_s *ws = (ws_s *)protocol;
  ws->on_ready(ws);
  (void)uuid;
}

/**  Called when the server is shutting down. */
static uint8_t websocket_on_shutdown(intptr_t uuid, fio_protocol_s *protocol) {
  ws_s *ws = (ws_s *)protocol;
  ws->on_shutdown(ws);
  return 0;
  (void)uuid;
}

/** Called when the connection was closed, but will not run concurrently */
static void websocket_on_close(intptr_t uuid, fio_protocol_s *protocol) {
  ws_s *ws = (ws_s *)protocol;
  if (ws->on_close)
    ws->on_close(uuid, ws->udata);
  ws___free(ws);
}

/** called when a connection's timeout was reached */
static void websocket_ping(intptr_t uuid, fio_protocol_s *protocol) {
  /* TODO: test PING */
  ws_s *ws = (ws_s *)protocol;
  char *ping_msg = fio_malloc(32);
  size_t len;
  if (ws->is_client)
    len = websocket_server_wrap(ping_msg, "facil.io PING", 14, 0x9, 1, 1, 0);
  else
    len = websocket_client_wrap(ping_msg, "facil.io PING", 14, 0x9, 1, 1, 0);
  fio_write2(uuid, .data.buf = ping_msg, .len = len, .after.dealloc = fio_free);
  (void)uuid;
}

static void websocket_on_data_process_data(ws_s *ws) {
  /* TODO: complete data processing logic */
  if (!ws->buf.len)
    return;
  uint64_t r = websocket_consume(ws->buf.buf, ws->buf.len, ws, ws->is_client);
  if (r && r != ws->buf.len) {
    memmove(ws->buf.buf, ws->buf.buf + ws->buf.len - r, r);
  }
  ws->buf.len = r;
}

/** Called when a data is available, but will not run concurrently */
static void websocket_on_data(intptr_t uuid, fio_protocol_s *protocol) {
  ws_s *ws = (ws_s *)protocol;
  if ((ws->buf.len << 1) >= ws->buf.capa) {
    /* increase reading buffer, so we have room to read packet data */
    if (ws->buf.capa != WS_INITIAL_BUFFER_SIZE)
      ws->buf.capa += 32;
    else if (!WS_INITIAL_BUFFER_SIZE)
      ws->buf.capa = (1 << 11);
    ws->buf.capa <<= 1;
    ws->buf.capa -= 32;
    if (ws->max_msg_size << 1 > ws->buf.capa) {
      fio_close(uuid);
      return;
    }
    void *tmp = realloc(ws->buf.buf, ws->buf.capa);
    FIO_ASSERT_ALLOC(tmp);
    ws->buf.buf = tmp;
  }
  ssize_t r =
      fio_read(uuid, ws->buf.buf + ws->buf.len, ws->buf.capa - ws->buf.len);
  if (r > 0)
    ws->buf.len += r;
  websocket_on_data_process_data(ws);
}

/** Called when a data is available, but will not run concurrently */
static void websocket_on_data_first(intptr_t uuid, fio_protocol_s *protocol) {
  if (!fio_is_valid(uuid))
    return;
  ws_s *ws = (ws_s *)protocol;
  if (ws->on_open)
    ws->on_open(ws);
  if (ws->on_message) {
    ws->pr.on_data = websocket_on_data;
    websocket_on_data_process_data(ws);
  }
  (void)uuid;
}

/* *****************************************************************************
Parser Callbacks
***************************************************************************** */

static void websocket_on_protocol_ping(void *udata, void *msg, uint64_t len);
static void websocket_on_protocol_pong(void *udata, void *msg, uint64_t len);
static void websocket_on_protocol_close(void *udata);
static void websocket_on_protocol_error(void *udata);
static void websocket_on_unwrapped(void *udata,
                                   void *msg,
                                   uint64_t len,
                                   char first,
                                   char last,
                                   char text,
                                   unsigned char rsv);

/* *****************************************************************************







Public API







***************************************************************************** */

/** used internally: attaches the Websocket protocol to the socket. */
void websocket_attach(intptr_t uuid,
                      http_settings_s *http_settings,
                      websocket_settings_s args,
                      void *data_in_buffer,
                      size_t length);

/* *****************************************************************************
Websocket information
***************************************************************************** */

/** Returns the opaque user data associated with the websocket. */
void *websocket_udata_get(ws_s *ws) { return ws->udata; }

/**
 * Sets the opaque user data associated with the websocket.
 *
 * Returns the old value, if any.
 */
void *websocket_udata_set(ws_s *ws, void *udata) {
  void *old = ws->udata;
  ws->udata = udata;
  return old;
}

/**
 * Returns the underlying socket UUID.
 *
 * This is only relevant for collecting the protocol object from outside of
 * websocket events, as the socket shouldn't be written to.
 */
intptr_t websocket_uuid(ws_s *ws) { return ws->uuid; }

/**
 * Returns 1 if the WebSocket connection is in Client mode (connected to a
 * remote server) and 0 if the connection is in Server mode (a connection
 * established using facil.io's HTTP server).
 */
uint8_t websocket_is_client(ws_s *ws) { return ws->is_client; }

/* *****************************************************************************
Websocket Connection Management (write / close)
***************************************************************************** */

/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s *ws, fio_str_info_s msg, uint8_t is_text);

/** Closes a websocket connection. */
void websocket_close(ws_s *ws);

/* *****************************************************************************
Websocket Pub/Sub
=================

API for websocket pub/sub that can be used to publish messages across process
boundries.

Supports pub/sub engines (see {fio.h}) that can connect to a backend service
such as Redis.

The default pub/sub engine (if `NULL` or unspecified) will publish the messages
to the process cluster (all the processes in `fio_start`).
***************************************************************************** */

/** Optimize generic broadcasts, for use in websocket_optimize4broadcasts. */
#define WEBSOCKET_OPTIMIZE_PUBSUB (-32)

/**
 * Enables (or disables) broadcast optimizations.
 *
 * This is performed automatically by the `websocket_subscribe` function.
 * However, this function is provided for enabling the pub/sub metadata based
 * optimizations for external connections / subscriptions.
 *
 * This function allows enablement (or disablement) of these optimizations:
 *
 * * WEBSOCKET_OPTIMIZE_PUBSUB - optimize all direct transmission messages,
 *                               best attempt to detect Text vs. Binary data.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_TEXT - optimize direct pub/sub text messages.
 * * WEBSOCKET_OPTIMIZE_PUBSUB_BINARY - optimize direct pub/sub binary messages.
 *
 * Note: to disable an optimization it should be disabled the same amount of
 * times it was enabled - multiple optimization enablements for the same type
 * are merged, but reference counted (disabled when reference is zero).
 *
 * Note2: The pub/sub metadata type ID will match the optimnization type
 * requested (i.e., `WEBSOCKET_OPTIMIZE_PUBSUB`) and the optimized data is a
 * FIOBJ String containing a pre-encoded WebSocket packet ready to be sent.
 * i.e.:
 *
 *     FIOBJ pre_wrapped = (FIOBJ)fio_message_metadata(msg,
 *                               WEBSOCKET_OPTIMIZE_PUBSUB);
 *     fiobj_send_free((intptr_t)msg->udata1, fiobj_dup(pre_wrapped));
 */
void websocket_optimize4broadcasts(int enable);
