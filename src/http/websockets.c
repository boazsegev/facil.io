#include "websockets.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*******************************************************************************
Buffer management - update to change the way the buffer is handled.
*/
/** returns the buffer's address, should be (at least) `size` long. */
void* create_buffer(size_t size);
/** returns the new buffer's address, should be (at least) `size` long. */
void* resize_buffer(void* old_buffer, size_t size);
/** releases an existing buffer. */
void free_buffer(void* buffer);

/*******************************************************************************
The API functions (declarations)
*/

/** Upgrades an existing HTTP connection to a Websocket connection. */
static void upgrade(struct WebsocketSettings settings);
/** Writes data to the websocket. */
static int ws_write(ws_s* ws, void* data, size_t size, unsigned char text);
/** Closes a websocket connection. */
static void ws_close(ws_s* ws);
/** Returns the opaque user data associated with the websocket. */
static void* get_udata(ws_s* ws);
/** Sets the opaque user data associated with the websocket. returns the old
 * value, if any. */
static void* set_udata(ws_s* ws, void* udata);
/**
Performs a task on each websocket connection that shares the same process.
*/
ssize_t ws_each(ws_s* ws_originator,
                void (*task)(ws_s* ws_target, void* arg),
                void* arg,
                void (*on_finish)(ws_s* ws_originator, void* arg));

/*******************************************************************************
The API container
*/

struct Websockets_API__ Websockets = {
    .max_msg_size = 262144, /** defaults to ~250KB */
    .timeout = 40,          /** defaults to 40 seconds */
    .upgrade = upgrade,
    .write = ws_write,
    .close = ws_close,
    .each = ws_each,
    .get_udata = get_udata,
    .set_udata = set_udata,
};

/*******************************************************************************
The Websocket object (protocol + parser)
*/
struct Websocket {
  struct Protocol protocol;
  /** connection data */
  server_pt srv;
  int fd;
  /** callbacks */
  void (*on_message)(ws_s* ws, char* data);
  void (*on_open)(ws_s* ws);
  void (*on_shutdown)(ws_s* ws);
  void (*on_close)(ws_s* ws);
  /** Opaque user data. */
  void* udata;
  /** message buffer. */
  void* buffer;
  size_t buffer_size;
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
      unsigned rsv : 1;
    } state;
  } parser;
};

/*******************************************************************************
The Websocket Protocol implementation
*/
#define ws_protocol(srv, fd) ((struct Websocket*)(Server.get_protocol(srv, fd)))

void ping(server_pt srv, int fd) {
  // struct WebsocketProtocol* ws =
  //     (struct WebsocketProtocol*)Server.get_protocol(server, sockfd);
  Server.write_urgent(srv, fd, "\x89\x00", 2);
}

/*******************************************************************************
Writing to the Websocket
*/

#define WS_MAX_FRAME_SIZE 65532  // should be less then `unsigned short`

static void websocket_write(server_pt srv,
                            int fd,
                            void* data,
                            size_t len,
                            char text,
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

static int ws_write(ws_s* ws, void* data, size_t size, unsigned char text) {
  if ((void*)Server.get_protocol(ws->srv, ws->fd) == ws) {
    websocket_write(ws->srv, ws->fd, data, size, text, 1, 1);
    return 0;
  }
  return -1;
}
