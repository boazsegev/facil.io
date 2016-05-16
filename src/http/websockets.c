#include "websockets.h"
#include "mini-crypt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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
The API functions (declarations)
*/

/** Upgrades an existing HTTP connection to a Websocket connection. */
static void upgrade(struct WebsocketSettings settings);
/** Writes data to the websocket. */
static int ws_write(ws_s* ws, void* data, size_t size, char text);
/** Closes a websocket connection. */
static void ws_close(ws_s* ws);
/** Returns the opaque user data associated with the websocket. */
static void* get_udata(ws_s* ws);
/** Returns the the `server_pt` for the Server object that owns the connection
 */
static server_pt get_server(ws_s* ws);
/** Returns the the connection's UUID (the Server's connection ID). */
static uint64_t get_uuid(ws_s* ws);
/** Sets the opaque user data associated with the websocket. returns the old
 * value, if any. */
static void* set_udata(ws_s* ws, void* udata);
/**
Performs a task on each websocket connection that shares the same process.
*/
static int ws_each(ws_s* ws_originator,
                   void (*task)(ws_s* ws_target, void* arg),
                   void* arg,
                   void (*on_finish)(ws_s* ws_originator, void* arg));
/**
Counts the number of websocket connections.
*/
static long ws_count(ws_s* ws);
/*******************************************************************************
The API container
*/

struct Websockets_API__ Websocket = {
    .max_msg_size = 262144, /** defaults to ~250KB */
    .timeout = 40,          /** defaults to 40 seconds */
    .upgrade = upgrade,
    .write = ws_write,
    .close = ws_close,
    .each = ws_each,
    .count = ws_count,
    .get_udata = get_udata,
    .set_udata = set_udata,
    .get_server = get_server,
    .get_uuid = get_uuid,
};

/*******************************************************************************
Create/Destroy the websocket object (prototypes)
*/

static ws_s* new_websocket();
static void destroy_ws(ws_s* ws);

/*******************************************************************************
The Websocket object (protocol + parser)
*/
struct Websocket {
  struct Protocol protocol;
  /** connection data */
  server_pt srv;
  uint64_t fd;
  /** callbacks */
  void (*on_message)(ws_s* ws, char* data, size_t size, int is_text);
  void (*on_open)(ws_s* ws);
  void (*on_shutdown)(ws_s* ws);
  void (*on_close)(ws_s* ws);
  /** Opaque user data. */
  void* udata;
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
      unsigned busy : 1;
      unsigned client : 1;
    } state;
  } parser;
};

/**
The Websocket Protocol Identifying String. Used for the `each` function.
*/
char WEBSOCKET_PROTOCOL_ID_STR[] = "ws";

/*******************************************************************************
The Websocket Protocol implementation
*/
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#include <endian.h>
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

#define ws_protocol(srv, fd) ((struct Websocket*)(Server.get_protocol(srv, fd)))

static void ping(server_pt srv, uint64_t fd) {
  // struct WebsocketProtocol* ws =
  //     (struct WebsocketProtocol*)Server.get_protocol(server, sockfd);
  Server.write_urgent(srv, fd, "\x89\x00", 2);
}

static void on_close(server_pt srv, uint64_t fd) {
  destroy_ws(ws_protocol(srv, fd));
}

static void on_shutdown(server_pt srv, uint64_t fd) {
  ws_s* ws = ws_protocol(srv, fd);
  if (ws && ws->on_shutdown)
    ws->on_shutdown(ws);
}

/* later */
static void websocket_write(server_pt srv,
                            uint64_t fd,
                            void* data,
                            size_t len,
                            char text,
                            char first,
                            char last);

static void on_data(server_pt server, uint64_t sockfd) {
  struct Websocket* ws = ws_protocol(server, sockfd);
  if (ws == NULL || ws->protocol.service != WEBSOCKET_PROTOCOL_ID_STR)
    return;
  ws->parser.state.busy = 1;
  ssize_t len = 0;
  while ((len = Server.read(server, sockfd, ws->parser.tmp_buffer, 1024)) > 0) {
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
      } else {
        // TODO enforce masking? we probably should, for security reasons...
        // fprintf(stderr, "WARNING Websockets: got unmasked message!\n");
        // Server.close(server, sockfd);
        //   ws->parser.state.busy = 0;
        // return;
      }
      // Copy the data to the Ruby buffer - only if it's a user message
      // RubyCaller.call_c(copy_data_to_buffer_in_gvl, ws);
      if (ws->parser.head.op_code == 1 || ws->parser.head.op_code == 2 ||
          (!ws->parser.head.op_code &&
           (ws->parser.head2.op_code == 1 || ws->parser.head2.op_code == 2))) {
        // check message size limit
        if (Websocket.max_msg_size < ws->length + ws->parser.data_len) {
          // close connection!
          fprintf(stderr, "ERR Websocket: Payload too big, review limits.\n");
          Server.close(server, sockfd);
          ws->parser.state.busy = 0;
          return;
        }
        // review and resize the buffer's capacity - it can only grow.
        if (ws->length + ws->parser.length - ws->parser.received >
            ws->buffer.size) {
          ws->buffer = resize_ws_buffer(ws, ws->buffer);
          if (!ws->buffer.data) {
            // no memory.
            ws_close(ws);
            ws->parser.state.busy = 0;
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
        ws_close(ws);
        if (ws->parser.head2.op_code == ws->parser.head.op_code)
          goto reset_parser;
      } else if (ws->parser.head.op_code == 9) {
        /* ping */
        // write Pong - including ping data...
        websocket_write(server, sockfd, ws->parser.tmp_buffer + ws->parser.pos,
                        ws->parser.data_len, 10, 1, 1);
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
  ws->parser.state.busy = 0;
}

/*******************************************************************************
Create/Destroy the websocket object
*/

static ws_s* new_websocket() {
  // allocate the protocol object (TODO: (maybe) pooling)
  ws_s* ws = calloc(sizeof(*ws), 1);

  // setup the protocol & protocol callbacks
  ws->protocol.ping = ping;
  ws->protocol.service = WEBSOCKET_PROTOCOL_ID_STR;
  ws->protocol.on_data = on_data;
  ws->protocol.on_close = on_close;
  ws->protocol.on_shutdown = on_shutdown;
  // return the object
  return ws;
}
static void destroy_ws(ws_s* ws) {
  if (ws && ws->protocol.service == WEBSOCKET_PROTOCOL_ID_STR) {
    if (ws->parser.state.busy) {
      // in case the `on_data` callback is active, we delay...
      Server.run_async(ws->srv, (void (*)(void*))destroy_ws, ws);
      return;
    }
    if (ws->on_close)
      ws->on_close(ws);
    free_ws_buffer(ws, ws->buffer);
    free(ws);
  }
}

/*******************************************************************************
Writing to the Websocket
*/

#define WS_MAX_FRAME_SIZE 65532  // should be less then `unsigned short`

static void websocket_write(server_pt srv,
                            uint64_t fd,
                            void* data,
                            size_t len,
                            char text, /* TODO: add client masking */
                            char first,
                            char last) {
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
              .masked = 0};
    char buff[len + 2];
    memcpy(buff, &head, 2);
    memcpy(buff + 2, data, len);
    Server.write(srv, fd, buff, len + 2);
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
      void* buff = malloc(len + 4);
      memcpy(buff, &head, 4);
      memcpy(buff + 4, data, len);
      Server.write_move(srv, fd, buff, len + 4);
    } else {
      /* head MUST be 4 bytes */
      char buff[len + 4];
      memcpy(buff, &head, 4);
      memcpy(buff + 4, data, len);
      Server.write(srv, fd, buff, len + 4);
    }
  } else {
    /* frame fragmentation is better for large data then large frames */
    while (len > WS_MAX_FRAME_SIZE) {
      websocket_write(srv, fd, data, WS_MAX_FRAME_SIZE, text, first, 0);
      data += WS_MAX_FRAME_SIZE;
      first = 0;
    }
    websocket_write(srv, fd, data, len, text, first, 1);
  }
  return;
}

/*******************************************************************************
API implementation
*/
static void ws_close(ws_s* ws) {
  Server.write(ws->srv, ws->fd, "\x88\x00", 2);
  Server.close(ws->srv, ws->fd);
  return;
}

static int ws_write(ws_s* ws, void* data, size_t size, char text) {
  if ((void*)Server.get_protocol(ws->srv, ws->fd) == ws) {
    websocket_write(ws->srv, ws->fd, data, size, text, 1, 1);
    return 0;
  }
  return -1;
}

/** Returns the opaque user data associated with the websocket. */
static void* get_udata(ws_s* ws) {
  return ws->udata;
}

/** Returns the the `server_pt` for the Server object that owns the connection
 */
static server_pt get_server(ws_s* ws) {
  return ws->srv;
}

/** Returns the the connection's UUID (the Server's connection ID). */
static uint64_t get_uuid(ws_s* ws) {
  return ws->fd;
}

/** Sets the opaque user data associated with the websocket. returns the old
 * value, if any. */
static void* set_udata(ws_s* ws, void* udata) {
  void* old = ws->udata;
  ws->udata = udata;
  return old;
}

/** Upgrades an existing HTTP connection to a Websocket connection. */
static void upgrade(struct WebsocketSettings settings) {
  // A static data used for all websocket connections.
  static char ws_key_accpt_str[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  // make sure we have a response object.
  struct HttpResponse* response = settings.response;
  int free_response = 0;
  if (!response) {
    response = HttpResponse.create(settings.request);
    free_response = 1;
  }
  if (!response)
    return;  // nothing we can do...

  // allocate the protocol object (TODO: (maybe) pooling)
  ws_s* ws = new_websocket();
  if (!ws)
    goto refuse;

  // setup the socket-server data
  ws->fd = settings.request->sockfd;
  ws->srv = settings.request->server;
  // Setup ws callbacks
  ws->on_close = settings.on_close;
  ws->on_open = settings.on_open;
  ws->on_message = settings.on_message;
  ws->on_shutdown = settings.on_shutdown;
  // setup any user data
  ws->udata = settings.udata;

  char* recv_str;
  // upgrade taking place, make sure the upgrade headers are valid for the
  // response.
  response->status = 101;
  HttpResponse.write_header(response, "Connection", 10, "upgrade", 7);
  HttpResponse.write_header(response, "Upgrade", 7, "websocket", 9);
  HttpResponse.write_header(response, "sec-websocket-version", 21, "13", 2);
  // websocket extentions (none)
  // the accept Base64 Hash - we need to compute this one and set it
  if (!HttpRequest.find(settings.request, "SEC-WEBSOCKET-KEY"))
    goto refuse;
  // the client's unique string
  recv_str = HttpRequest.value(settings.request);
  if (!recv_str)
    goto refuse;
  ;
  // use the SHA1 methods provided to concat the client string and hash
  sha1_s sha1;
  MiniCrypt.sha1_init(&sha1);
  MiniCrypt.sha1_write(&sha1, recv_str, strlen(recv_str));
  MiniCrypt.sha1_write(&sha1, ws_key_accpt_str, sizeof(ws_key_accpt_str) - 1);
  // base encode the data
  char websockets_key[32];
  int len =
      MiniCrypt.base64_encode(websockets_key, MiniCrypt.sha1_result(&sha1), 20);
  // set the string's length and encoding
  HttpResponse.write_header(response, "Sec-WebSocket-Accept", 20,
                            websockets_key, len);

  goto cleanup;
refuse:
  // set the negative response
  HttpResponse.reset(response, settings.request);
  response->status = 400;
cleanup:
  HttpResponse.send(response);
  if (response->status == 101 &&
      Server.set_protocol(settings.request->server, settings.request->sockfd,
                          (void*)ws) == 0) {
    // we have an active websocket connection - prep the connection buffer
    ws->buffer = create_ws_buffer(ws);
    // update the timeout
    Server.set_timeout(settings.request->server, settings.request->sockfd,
                       Websocket.timeout);
    if (settings.on_open)
      settings.on_open(ws);
  } else {
    destroy_ws(ws);
  }
  if (free_response)
    HttpResponse.destroy(response);
  return;
}

/*******************************************************************************
Each
*/

/** A task container. */
struct WSTask {
  void (*task)(ws_s*, void*);
  void (*on_finish)(ws_s*, void*);
  void* arg;
};
/** Performs a task on each websocket connection that shares the same process */
static void perform_ws_task(server_pt srv, uint64_t fd, void* _arg) {
  struct WSTask* tsk = _arg;
  struct Protocol* ws = Server.get_protocol(srv, fd);
  if (ws && ws->service == WEBSOCKET_PROTOCOL_ID_STR)
    tsk->task((ws_s*)(ws), tsk->arg);
}
/** clears away a wesbocket task. */
static void finish_ws_task(server_pt srv, uint64_t fd, void* _arg) {
  struct WSTask* tsk = _arg;
  if (tsk->on_finish)
    tsk->on_finish(ws_protocol(srv, fd), tsk->arg);
  free(tsk);
}

/**
Performs a task on each websocket connection that shares the same process.
*/
static int ws_each(ws_s* ws_originator,
                   void (*task)(ws_s* ws_target, void* arg),
                   void* arg,
                   void (*on_finish)(ws_s* ws_originator, void* arg)) {
  struct WSTask* tsk = malloc(sizeof(*tsk));
  tsk->arg = arg;
  tsk->on_finish = on_finish;
  tsk->task = task;
  return Server.each(ws_originator->srv, ws_originator->fd,
                     WEBSOCKET_PROTOCOL_ID_STR, perform_ws_task, tsk,
                     finish_ws_task);
}
/**
Counts the number of websocket connections.
*/
static long ws_count(ws_s* ws) {
  return Server.count(ws->srv, WEBSOCKET_PROTOCOL_ID_STR);
}
