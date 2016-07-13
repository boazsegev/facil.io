#include "libserver.h"
#include "websockets.h"
#include "minicrypt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

/*******************************************************************************
Buffer management - update to change the way the buffer is handled.
*/
struct buffer_s {
  void* data;
  size_t size;
};

#pragma weak create_ws_buffer
/** returns a buffer_s struct, with a buffer (at least) `size` long. */
struct buffer_s create_ws_buffer(ws_s* owner);

#pragma weak resize_ws_buffer
/** returns a buffer_s struct, with a buffer (at least) `size` long. */
struct buffer_s resize_ws_buffer(ws_s* owner, struct buffer_s);

#pragma weak free_ws_buffer
/** releases an existing buffer. */
void free_ws_buffer(ws_s* owner, struct buffer_s);

/** Sets the initial buffer size. (16Kb)*/
#define WS_INITIAL_BUFFER_SIZE 16384

/*******************************************************************************
Buffer management - simple implementation...
Since Websocket connections have a long life expectancy, optimizing this part of
the code probably wouldn't offer a high performance boost.
*/

// buffer increments by 4,096 Bytes (4Kb)
#define round_up_buffer_size(size) (((size) >> 12) + 1) << 12

struct buffer_s create_ws_buffer(ws_s* owner) {
  struct buffer_s buff;
  buff.size = round_up_buffer_size(WS_INITIAL_BUFFER_SIZE);
  buff.data = malloc(buff.size);
  return buff;
}

struct buffer_s resize_ws_buffer(ws_s* owner, struct buffer_s buff) {
  buff.size = round_up_buffer_size(buff.size);
  void* tmp = realloc(buff.data, buff.size);
  if (!tmp) {
    free_ws_buffer(owner, buff);
    buff.size = 0;
  }
  buff.data = tmp;
  return buff;
}
void free_ws_buffer(ws_s* owner, struct buffer_s buff) {
  if (buff.data)
    free(buff.data);
}

#undef round_up_buffer_size

/*******************************************************************************
Create/Destroy the websocket object (prototypes)
*/

static ws_s* new_websocket();
static void destroy_ws(ws_s* ws);

/*******************************************************************************
The Websocket object (protocol + parser)
*/
struct Websocket {
  /** The Websocket protocol */
  protocol_s protocol;
  /** connection data */
  intptr_t fd;
  /** callbacks */
  void (*on_message)(ws_s* ws, char* data, size_t size, uint8_t is_text);
  void (*on_open)(ws_s* ws);
  void (*on_shutdown)(ws_s* ws);
  void (*on_close)(ws_s* ws);
  /** Opaque user data. */
  void* udata;
  /** The maximum websocket message size */
  size_t max_msg_size;
  /** message buffer. */
  struct buffer_s buffer;
  /** message length (how much of the buffer actually used). */
  size_t length;
  /** parser. */
  struct {
    union {
      unsigned len1 : 16;
      unsigned long len2 : 64;
      char bytes[8];
    } psize;
    size_t length;
    size_t received;
    int pos;
    int data_len;
    char mask[4];
    char tmp_buffer[1024];
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
    } head, head2;
    struct {
      unsigned size : 7;
      unsigned masked : 1;
    } sdata;
    struct {
      unsigned has_mask : 1;
      unsigned at_mask : 2;
      unsigned has_len : 1;
      unsigned at_len : 3;
      unsigned client : 1;
    } state;
  } parser;
};

/**
The Websocket Protocol Identifying String. Used for the `each` function.
*/
char* WEBSOCKET_ID_STR = "websockets";

/*******************************************************************************
The Websocket Protocol implementation
*/

#define ws_protocol(fd) ((ws_s*)(server_get_protocol(fd)))

static void ws_ping(intptr_t fd, protocol_s* _ws) {
  sock_packet_s* packet;
  while ((packet = sock_checkout_packet()) == NULL)
    sock_flush_all();
  *packet = (sock_packet_s){
      .buffer = "\x89\x00", .length = 2, .metadata.urgent = 1,
  };
  sock_send_packet(fd, packet);
}

static void on_close(protocol_s* _ws) {
  destroy_ws((ws_s*)_ws);
}

static void on_shutdown(intptr_t fd, protocol_s* _ws) {
  if (_ws && ((ws_s*)_ws)->on_shutdown)
    ((ws_s*)_ws)->on_shutdown((ws_s*)_ws);
}

/* later */
static void websocket_write_impl(intptr_t fd,
                                 void* data,
                                 size_t len,
                                 char text,
                                 char first,
                                 char last,
                                 char client);

static void on_data(intptr_t sockfd, protocol_s* _ws) {
#define ws ((ws_s*)_ws)
  if (ws == NULL || ws->protocol.service != WEBSOCKET_ID_STR)
    return;
  ssize_t len = 0;
  while ((len = sock_read(sockfd, ws->parser.tmp_buffer, 1024)) > 0) {
    ws->parser.data_len = 0;
    ws->parser.pos = 0;
    while (ws->parser.pos < len) {
      // collect the frame's head
      if (!(*(char*)(&ws->parser.head))) {
        *((char*)(&(ws->parser.head))) = ws->parser.tmp_buffer[ws->parser.pos];
        // save a copy if it's the first head in a fragmented message
        if (!(*(char*)(&ws->parser.head2))) {
          ws->parser.head2 = ws->parser.head;
        }
        // advance
        ws->parser.pos++;
        // go back to the `while` head, to review if there's more data
        continue;
      }

      // save the mask and size information
      if (!(*(char*)(&ws->parser.sdata))) {
        *((char*)(&(ws->parser.sdata))) = ws->parser.tmp_buffer[ws->parser.pos];
        // set length
        ws->parser.state.at_len = ws->parser.sdata.size == 127
                                      ? 7
                                      : ws->parser.sdata.size == 126 ? 1 : 0;
        ws->parser.pos++;
        continue;
      }

      // check that if we need to collect the length data
      if (ws->parser.sdata.size >= 126 && !(ws->parser.state.has_len)) {
      // avoiding a loop so we don't mixup the meaning of "continue" and
      // "break"
      collect_len:
////////// NOTICE: Network Byte Order requires us to translate the data
#ifdef __BIG_ENDIAN__
        if ((ws->parser.state.at_len == 1 && ws->parser.sdata.size == 126) ||
            (ws->parser.state.at_len == 7 && ws->parser.sdata.size == 127)) {
          ws->parser.psize.bytes[ws->parser.state.at_len] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          ws->parser.state.has_len = 1;
          ws->parser.length = (ws->parser.sdata.size == 126)
                                  ? ws->parser.psize.len1
                                  : ws->parser.psize.len2;
        } else {
          ws->parser.psize.bytes[ws->parser.state.at_len++] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          if (ws->parser.pos < len)
            goto collect_len;
        }
#else
        if (ws->parser.state.at_len == 0) {
          ws->parser.psize.bytes[ws->parser.state.at_len] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          ws->parser.state.has_len = 1;
          ws->parser.length = (ws->parser.sdata.size == 126)
                                  ? ws->parser.psize.len1
                                  : ws->parser.psize.len2;
        } else {
          ws->parser.psize.bytes[ws->parser.state.at_len--] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          if (ws->parser.pos < len)
            goto collect_len;
        }
#endif
        continue;
      } else if (!ws->parser.length && ws->parser.sdata.size < 126)
        // we should have the length data in the head
        ws->parser.length = ws->parser.sdata.size;

      // check that the data is masked and that we didn't colleced the mask yet
      if (ws->parser.sdata.masked && !(ws->parser.state.has_mask)) {
      // avoiding a loop so we don't mixup the meaning of "continue" and "break"
      collect_mask:
        if (ws->parser.state.at_mask == 3) {
          ws->parser.mask[ws->parser.state.at_mask] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          ws->parser.state.has_mask = 1;
          ws->parser.state.at_mask = 0;
        } else {
          ws->parser.mask[ws->parser.state.at_mask++] =
              ws->parser.tmp_buffer[ws->parser.pos++];
          if (ws->parser.pos < len)
            goto collect_mask;
          else
            continue;
        }
        // since it's possible that there's no more data (0 length frame),
        // we don't use `continue` (check while loop) and we process what we
        // have.
      }

      // Now that we know everything about the frame, let's collect the data

      // How much data in the buffer is part of the frame?
      ws->parser.data_len = len - ws->parser.pos;
      if (ws->parser.data_len + ws->parser.received > ws->parser.length)
        ws->parser.data_len = ws->parser.length - ws->parser.received;

      // a note about unmasking: since ws->parser.state.at_mask is only 2 bits,
      // it will wrap around (i.e. 3++ == 0), so no modulus is required :-)
      // unmask:
      if (ws->parser.sdata.masked) {
        for (int i = 0; i < ws->parser.data_len; i++) {
          ws->parser.tmp_buffer[i + ws->parser.pos] ^=
              ws->parser.mask[ws->parser.state.at_mask++];
        }
      } else if (ws->parser.state.client == 0) {
        // enforce masking unless acting as client, also for security reasons...
        fprintf(stderr, "ERROR Websockets: unmasked frame, disconnecting.\n");
        sock_close(sockfd);
        return;
      }
      // Copy the data to the Ruby buffer - only if it's a user message
      // RubyCaller.call_c(copy_data_to_buffer_in_gvl, ws);
      if (ws->parser.head.op_code == 1 || ws->parser.head.op_code == 2 ||
          (!ws->parser.head.op_code &&
           (ws->parser.head2.op_code == 1 || ws->parser.head2.op_code == 2))) {
        // check message size limit
        if (ws->max_msg_size < ws->length + ws->parser.data_len) {
          // close connection!
          fprintf(stderr, "ERROR Websocket: Payload too big, review limits.\n");
          sock_close(sockfd);
          return;
        }
        // review and resize the buffer's capacity - it can only grow.
        if (ws->length + ws->parser.length - ws->parser.received >
            ws->buffer.size) {
          ws->buffer = resize_ws_buffer(ws, ws->buffer);
          if (!ws->buffer.data) {
            // no memory.
            websocket_close(ws);
            return;
          }
        }
        // copy here
        memcpy(ws->buffer.data + ws->length,
               ws->parser.tmp_buffer + ws->parser.pos, ws->parser.data_len);
        ws->length += ws->parser.data_len;
      }
      // set the frame's data received so far (copied or not)
      // we couldn't do it soonet, because we needed the value to compute the
      // Ruby buffer capacity (within the GVL resize function).
      ws->parser.received += ws->parser.data_len;

      // check that we have collected the whole of the frame.
      if (ws->parser.length > ws->parser.received) {
        ws->parser.pos += ws->parser.data_len;
        continue;
      }

      // we have the whole frame, time to process the data.
      // pings, pongs and other non-Ruby handled messages.
      if (ws->parser.head.op_code == 0 || ws->parser.head.op_code == 1 ||
          ws->parser.head.op_code == 2) {
        /* a user data frame */
        if (ws->parser.head.fin) {
          /* This was the last frame */
          if (ws->parser.head2.op_code == 1) {
            /* text data */
          } else if (ws->parser.head2.op_code == 2) {
            /* binary data */
          } else  // not a recognized frame, don't act
            goto reset_parser;
          // call the on_message callback

          if (ws->on_message)
            ws->on_message(ws, ws->buffer.data, ws->length,
                           ws->parser.head2.op_code == 1);
          goto reset_parser;
        }
      } else if (ws->parser.head.op_code == 8) {
        /* close */
        websocket_close(ws);
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code == 9) {
        /* ping */
        // write Pong - including ping data...
        websocket_write_impl(sockfd, ws->parser.tmp_buffer + ws->parser.pos,
                             ws->parser.data_len, 10, 1, 1,
                             ws->parser.state.client);
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code == 10) {
        /* pong */
        // do nothing... almost
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code > 2 && ws->parser.head.op_code < 8) {
        /* future control frames. ignore. */
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else {
        /* WTF? */
        if (ws->parser.head.fin)
          goto reset_parser;
      }
      // not done, but move the pos marker along
      ws->parser.pos += ws->parser.data_len;
      continue;

    reset_parser:
      // move the pos marker along - in case we have more then one frame in the
      // buffer
      ws->parser.pos += ws->parser.data_len;
      // clear the parser
      *((char*)(&(ws->parser.state))) = *((char*)(&(ws->parser.sdata))) =
          *((char*)(&(ws->parser.head2))) = *((char*)(&(ws->parser.head))) = 0;
      // // // the above should be the same as... but it isn't
      // *((uint32_t*)(&(ws->parser.head))) = 0;
      // set the union size to 0
      ws->length = ws->parser.received = ws->parser.length =
          ws->parser.psize.len2 = ws->parser.data_len = 0;
    }
  }
#undef ws
}

/*******************************************************************************
Create/Destroy the websocket object
*/

static ws_s* new_websocket() {
  // allocate the protocol object (TODO: (maybe) pooling)
  ws_s* ws = calloc(sizeof(*ws), 1);

  // setup the protocol & protocol callbacks
  ws->protocol.ping = ws_ping;
  ws->protocol.service = WEBSOCKET_ID_STR;
  ws->protocol.on_data = on_data;
  ws->protocol.on_close = on_close;
  ws->protocol.on_shutdown = on_shutdown;
  // return the object
  return ws;
}
static void destroy_ws(ws_s* ws) {
  if (ws->on_close)
    ws->on_close(ws);
  free_ws_buffer(ws, ws->buffer);
  free(ws);
}

/*******************************************************************************
Writing to the Websocket
*/

#define WS_MAX_FRAME_SIZE 65532  // should be less then `unsigned short`

static void websocket_write_impl(intptr_t fd,
                                 void* data,
                                 size_t len,
                                 char text, /* TODO: add client masking */
                                 char first,
                                 char last,
                                 char client) {
  if (len < 126) {
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
      unsigned size : 7;
      unsigned masked : 1;
    } head = {.op_code = (first ? (!text ? 2 : text) : 0),
              .fin = last,
              .size = len,
              .masked = client};
    char buff[len + (client ? 6 : 2)];
    memcpy(buff, &head, 2);
    memcpy(buff + (client ? 6 : 2), data, len);
    sock_write(fd, buff, len + 2);
  } else if (len <= WS_MAX_FRAME_SIZE) {
    /* head is 4 bytes */
    struct {
      unsigned op_code : 4;
      unsigned rsv3 : 1;
      unsigned rsv2 : 1;
      unsigned rsv1 : 1;
      unsigned fin : 1;
      unsigned size : 7;
      unsigned masked : 1;
      unsigned length : 16;
    } head = {.op_code = (first ? (text ? 1 : 2) : 0),
              .fin = last,
              .size = 126,
              .masked = 0,
              .length = htons(len)};
    if (len >> 15) {  // if len is larger then 32,767 Bytes.
      /* head MUST be 4 bytes */
      void* buff = malloc(len + (client ? 8 : 4));
      memcpy(buff, &head, 4);
      memcpy(buff + (client ? 8 : 4), data, len);
      sock_write2(.fduuid = fd, .buffer = buff, .length = len + 4, .move = 1);
    } else {
      /* head MUST be 4 bytes */
      char buff[len + (client ? 8 : 4)];
      memcpy(buff, &head, 4);
      memcpy(buff + (client ? 8 : 4), data, len);
      sock_write(fd, buff, len + 4);
    }
  } else {
    /* frame fragmentation is better for large data then large frames */
    while (len > WS_MAX_FRAME_SIZE) {
      websocket_write_impl(fd, data, WS_MAX_FRAME_SIZE, text, first, 0, client);
      data += WS_MAX_FRAME_SIZE;
      first = 0;
    }
    websocket_write_impl(fd, data, len, text, first, 1, client);
  }
  return;
}

/*******************************************************************************
The API implementation
*/

/** The upgrade */
#undef websocket_upgrade
ssize_t websocket_upgrade(websocket_settings_s settings) {
  // A static data used for all websocket connections.
  static char ws_key_accpt_str[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  // a temporary response object, in case none is provided.
  http_response_s tmp_response;
  // require either a request or a response.
  if (((uintptr_t)settings.request | (uintptr_t)settings.response) ==
      (uintptr_t)NULL)
    return -1;
  if (settings.max_msg_size == 0)
    settings.max_msg_size = 262144; /** defaults to ~250KB */
  if (settings.timeout == 0)
    settings.timeout = 40; /* defaults to 40 seconds */
  // make sure we have a response object.
  http_response_s* response = settings.response;
  if (response == NULL) {
    /* initialize a default upgrade response */
    tmp_response = http_response_init(settings.request);
    response = &tmp_response;
  } else
    settings.request = response->metadata.request;
  // allocate the protocol object (TODO: (maybe) pooling)
  ws_s* ws = new_websocket();
  if (!ws)
    goto refuse;

  // setup the socket-server data
  ws->fd = response->metadata.request->metadata.fd;
  // Setup ws callbacks
  ws->on_close = settings.on_close;
  ws->on_open = settings.on_open;
  ws->on_message = settings.on_message;
  ws->on_shutdown = settings.on_shutdown;

  // setup any user data
  ws->udata = settings.udata;
  // buffer limits
  ws->max_msg_size = settings.max_msg_size;
  const char* recv_str;

  recv_str =
      http_request_find_header(settings.request, "sec-websocket-version", 21);
  if (recv_str == NULL || recv_str[0] != '1' || recv_str[1] != '3')
    goto refuse;

  recv_str =
      http_request_find_header(settings.request, "sec-websocket-key", 17);
  if (recv_str == NULL)
    goto refuse;

  // websocket extentions (none)

  // the accept Base64 Hash - we need to compute this one and set it
  // the client's unique string
  // use the SHA1 methods provided to concat the client string and hash
  sha1_s sha1;
  minicrypt_sha1_init(&sha1);
  minicrypt_sha1_write(&sha1, recv_str, strlen(recv_str));
  minicrypt_sha1_write(&sha1, ws_key_accpt_str, sizeof(ws_key_accpt_str) - 1);
  // base encode the data
  char websockets_key[32];
  int len =
      minicrypt_base64_encode(websockets_key, minicrypt_sha1_result(&sha1), 20);

  // websocket extentions (none)

  // upgrade taking place, make sure the upgrade headers are valid for the
  // response.
  response->status = 101;
  http_response_write_header(response, .name = "Connection", .name_length = 10,
                             .value = "Upgrade", .value_length = 7);
  http_response_write_header(response, .name = "Upgrade", .name_length = 7,
                             .value = "websocket", .value_length = 9);
  http_response_write_header(response, .name = "sec-websocket-version",
                             .name_length = 21, .value = "13",
                             .value_length = 2);
  // set the string's length and encoding
  http_response_write_header(response, .name = "Sec-WebSocket-Accept",
                             .name_length = 20, .value = websockets_key,
                             .value_length = len);
  // inform about 0 extension support
  recv_str = http_request_find_header(settings.request,
                                      "sec-websocket-extensions", 24);
  // if (recv_str != NULL)
  //   http_response_write_header(response, .name = "Sec-Websocket-Extensions",
  //                              .name_length = 24);

  goto cleanup;
refuse:
  // set the negative response
  response->status = 400;
cleanup:
  http_response_finish(response);
  if (response->status == 101) {
    // update the protocol object, cleanning up the old one
    protocol_s* old = server_get_protocol(ws->fd);
    if (old->on_close)
      old->on_close(old);
    server_set_protocol(ws->fd, (void*)ws);
    // we have an active websocket connection - prep the connection buffer
    ws->buffer = create_ws_buffer(ws);
    // update the timeout
    server_set_timeout(ws->fd, settings.timeout);
    if (settings.on_open)
      settings.on_open(ws);
    return 0;
  }
  destroy_ws(ws);
  return -1;
}
#define websocket_upgrade(...) \
  websocket_upgrade((websocket_settings_s){__VA_ARGS__})

/** Returns the opaque user data associated with the websocket. */
void* websocket_get_udata(ws_s* ws) {
  return ws->udata;
}
/** Returns the the process specific connection's UUID (see `libsock`). */
intptr_t websocket_get_fduuid(ws_s* ws) {
  return ws->fd;
}
/** Sets the opaque user data associated with the websocket.
 * Returns the old value, if any. */
void* websocket_set_udata(ws_s* ws, void* udata) {
  void* old = ws->udata;
  ws->udata = udata;
  return old;
}
/** Writes data to the websocket. Returns -1 on failure (0 on success). */
int websocket_write(ws_s* ws, void* data, size_t size, uint8_t is_text) {
  if (sock_isvalid(ws->fd)) {
    websocket_write_impl(ws->fd, data, size, is_text, 1, 1,
                         ws->parser.state.client);
    return 0;
  }
  return -1;
}
/** Closes a websocket connection. */
void websocket_close(ws_s* ws) {
  sock_packet_s* packet;
  while ((packet = sock_checkout_packet()) == NULL)
    sock_flush_all();
  *packet = (sock_packet_s){
      .buffer = "\x88\x00", .length = 2,
  };
  sock_send_packet(ws->fd, packet);
  sock_close(ws->fd);
  return;
}

/**
Counts the number of websocket connections.
*/
size_t websocket_count(ws_s* ws) {
  return server_count(WEBSOCKET_ID_STR);
}

/*******************************************************************************
Each Implementation
*/

/** A task container. */
struct WSTask {
  void (*task)(ws_s*, void*);
  void (*on_finish)(ws_s*, void*);
  void* arg;
};
/** Performs a task on each websocket connection that shares the same process */
static void perform_ws_task(intptr_t fd, protocol_s* _ws, void* _arg) {
  struct WSTask* tsk = _arg;
  tsk->task((ws_s*)(_ws), tsk->arg);
}
/** clears away a wesbocket task. */
static void finish_ws_task(intptr_t fd, protocol_s* _ws, void* _arg) {
  struct WSTask* tsk = _arg;
  if (tsk->on_finish)
    tsk->on_finish((ws_s*)(_ws), tsk->arg);
  free(tsk);
}

/**
Performs a task on each websocket connection that shares the same process
(except the originating `ws_s` connection which is allowed to be NULL).
 */
void websocket_each(ws_s* ws_originator,
                    void (*task)(ws_s* ws_target, void* arg),
                    void* arg,
                    void (*on_finish)(ws_s* ws_originator, void* arg)) {
  struct WSTask* tsk = malloc(sizeof(*tsk));
  tsk->arg = arg;
  tsk->on_finish = on_finish;
  tsk->task = task;
  server_each((ws_originator ? ws_originator->fd : -1), WEBSOCKET_ID_STR,
              perform_ws_task, tsk, finish_ws_task);
}
