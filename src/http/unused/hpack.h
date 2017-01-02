/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HPACK_H
#define HPACK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/** Sets the limit for both a single header value and a packed header group.
  * Must be less than 2^16 -1
  */
#define HPACK_BUFFER_SIZE 16384

/** Sets the limit for the amount of data an HPACK dynamic table can reference.
  * Should be less then 65,535 (2^16 -1 is the type size limit).
  */
#define HPACK_MAX_TABLE_SIZE 65535

/** A Short String (SString) struct, up to 65,535 bytes long */
typedef struct sstring_s {
  uint16_t len;
  uint8_t data[];
} sstring_s;

/** An HTTP/2 Header */
typedef struct http2_header_s {
  sstring_s *name;
  sstring_s *value;
} http2_header_s;

/** An HTTP/2 Header Collection */
typedef struct http2_header_array_s {
  size_t len;
  http2_header_s headers[];
} http2_header_array_s;

/** The HPACK context. */
typedef struct hpack_context_s *hpack_context_p;

#endif
