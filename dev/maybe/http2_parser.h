/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP2_PARSER_H
/**
HTTP/2 Parser / Formatter (under development)
=========================

This parser / formatter is intended to be an implementation independent module
that can be used by other projects as well.

This module takes the very limited responsibility of wrapping and unwrapping
HTTP/2 frames while maintaining the protocol's integrity.

The module does NOT handle HPACK unpacking (performed by the seperate hpack
module), stream_id (request / response) management or anything else.

The module only handles HTTP/2 frames and basic protocol integrity tasks.

Data is written directly to a buffer provied by the parser. When a frame in
unwarpped, an appropriate callback is called.

An example for creating a parser object:

    struct http2_callbacks_s http2_callbacks =
                         {.on_ping = my_on_ping_callback }; // fill in...

    http2_parser_pt = http2_parser_create(.callbacks = &http2_callbacks,
                                          .udata = (void*)((intptr_t)fd));

An example for parsing data from a socket:

    int read2parser(http2_parser_pt p, int fd) {
      while(1) {
        void * buffer = http2_parser_buffer(p);
        size_t len = http2_parser_capacity(p);
        ssize_t incoming = read(fd, buffer, len);
        if( incoming <= 0 ) // fix this
           return -1;
        http2_parser_review(p, incoming);
      }
    }

Review the `struct http2_callbacks_s` for a list of possible callbacks.
*/
#define H_HTTP2_PARSER_H
#include <stdio.h>
#include <stdlib.h>

/** possible HTTP/2 error values. */
enum http2_err_enum {
  H2ERR_OK = 0,
  H2ERR_NO_ERROR = 0,
  H2ERR_PROTOCOL_ERROR = 0x1,
  H2ERR_INTERNAL_ERROR = 0x2,
  H2ERR_FLOW_CONTROL_ERROR = 0x3,
  H2ERR_SETTINGS_TIMEOUT = 0x4,
  H2ERR_STREAM_CLOSED = 0x5,
  H2ERR_FRAME_SIZE_ERROR = 0x6,
  H2ERR_REFUSED_STREAM = 0x7,
  H2ERR_REFUSED_STREAM_PRE_APP = 0x8,
  H2ERR_COMPRESSION_ERROR = 0x9,
  H2ERR_CONNECT_ERROR = 0xa,
  H2ERR_ENHANCE_YOUR_CALM = 0xb,
  H2ERR_INADEQUATE_SECURITY = 0xc,
  H2ERR_HTTP_1_1_REQUIRED = 0xd
};

/** possible HTTP/2 SETTINGS frame values. */
enum http2_setting_enum {
  SETTINGS_HEADER_TABLE_SIZE = 0x1,
  SETTINGS_ENABLE_PUSH = 0x2,
  SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
  SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
  SETTINGS_MAX_FRAME_SIZE = 0x5,
  SETTINGS_MAX_HEADER_LIST_SIZE = 0x6,
};

/** an opaque parser pointer type for the HTTP 2 parser. */
typedef struct http2_parser_s *http2_parser_pt;

/** Callbacks used for handling HTTP/2 data. */
struct http2_callbacks_s {
  /* a callback for data arriving (the message body or part of it). */
  void (*on_body)(http2_parser_pt p, void *udata, uint32_t stream_id,
                  void *body, size_t length);

  /* a callback indicating that the specified `stream_id` can be processed. */
  void (*on_finalized)(http2_parser_pt p, void *udata, uint32_t stream_id);

  /* a callback indicating a request to update a stream's priority. */
  void (*on_priority)(http2_parser_pt p, void *udata, uint32_t stream_id,
                      uint32_t dependance_stream_id,
                      uint8_t exclusive_dependancy, uint8_t weight);

  /* a callback a request to reset an existing stream (disregard). */
  void (*on_reset_stream)(http2_parser_pt p, void *udata, uint32_t stream_id,
                          uint32_t error_code);

  /* a callback indicating a settings update request or acknowledgment. */
  void (*on_settings)(http2_parser_pt p, void *udata, uint16_t identifier,
                      uint32_t value, uint8_t is_ack);

  /* a callback indicating an HTTP/2 ping or pong event. */
  void (*on_ping)(http2_parser_pt p, void *udata, uint64_t payload,
                  uint8_t is_ack);
};

/** Arguments for the `http2_parser_create` function */
struct http2_parser_create_args_s {
  /* a link to a callback structure. missing callbacks default to no_op. */
  struct http2_callbacks_s *callbacks;
  /* the maximum frame size will be pre-allocated as the HTTP/2 buffer. */
  uint32_t max_frame_size;
  /* a user opaque pointer, passed along to any callbacks. */
  void *udata;
};

/** creates an HTTP/2 parser */
http2_parser_pt http2_parser_create(struct http2_parser_create_args_s args);

/** a macro for named arguments semantics. */
#define http2_parser_create(...)                                               \
  http2_parser_create((struct http2_parser_create_args_s){__VA_ARGS__})

/** destroy an HTTP/2 parser (uses reference counting). */
void http2_parser_destroy(http2_parser_pt);

/**
 * Gets the current position in the HTTP/2 parser's buffer.
 *
 * This changes with every read according to the amount of data the parser
 * requires to complete it's next step.
 */
void *http2_parser_buffer(http2_parser_pt);

/**
 * Gets the current capacity for the HTTP/2 parser's buffer.
 *
 * This changes with every read according to the amount of data the parser
 * requires to complete it's next step.
 */
size_t http2_parser_capacity(http2_parser_pt);

/**
 * Signals the parser to process incominng data `length` long.
 *
 * This updates the `http2_parser_buffer` and `http2_parser_capacity` values.
 *
 * Returns 0 on success. If an error occured (i.e., too much data was supplied
 * or a protocol error), returns -1.
 */
enum http2_err_enum http2_parser_review(http2_parser_pt, size_t length);

#endif
