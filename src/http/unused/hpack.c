/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "hpack.h"
#include "hpack_data.h"
// #include "mempool.h"
#include "spnlock.h"
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

/* *****************************************************************************
The HPACK context unifies the encoding and decoding contexts.

Each context (encoding, decoding) is a dynamic table implemented as a linked
list of headers (name, value pairs), a spin-lock and a byte-size state that's
HPACK complient.

A single dynamic table (decode / encode) cannot reference more then 65,535 bytes
(absolute max table size).
*/
struct hpack_context_s {
  struct {
    uint16_t size;
    struct http2_header_node_s {
      http2_header_s header;
      struct http2_header_node_s *next;
    } list;
    http2_header_s headers[16];
  } encode, decode;
};

typedef struct {
  uint64_t hash;
  ssize_t refereces;
} hash_data_s;

/* *****************************************************************************
A thread localized buffer for HPACK packing and unpacking implementation.
*/
static __thread struct {
  uint16_t len;
  uint8_t data[HPACK_BUFFER_SIZE];
} thread_buff;

/* *****************************************************************************
Helper Declerations
*/

static inline int64_t decode_int(uint8_t *buf, size_t len, uint8_t prefix,
                                 size_t *pos);
static inline int encode_int(uint64_t i, uint8_t prefix);

static sstring_s *unpack_huffman(void *buf, size_t len);
static int pack_huffman(void *buf, size_t len);

static inline int pack_string(uint8_t *buf, size_t len, _Bool compress);
static inline sstring_s *unpack_string(uint8_t *buf, size_t len, size_t *pos);

/* *****************************************************************************
API Implementation
*/

/* *****************************************************************************
Packing Strings
*/

static inline int pack_string(uint8_t *buf, size_t len, _Bool compress) {
  if (compress) {
    thread_buff.data[thread_buff.len] = 128;
    size_t comp_len = 0;
    for (size_t i = 0; i < len; i++) {
      comp_len += huffman_encode_table[buf[i]].bits;
    }
    if (comp_len & 7)
      comp_len += 8;
    comp_len >>= 3;
    if (encode_int(comp_len, 7)) {
      fprintf(stderr, "Error encoding String length\n");
      return -1;
    };
    pack_huffman(buf, len);

  } else {
    thread_buff.data[thread_buff.len] = 0;
    if (encode_int(len, 7)) {
      fprintf(stderr, "Error encoding String length\n");
      return -1;
    };
    if (thread_buff.len + len > HPACK_BUFFER_SIZE)
      return -1;
    memcpy(thread_buff.data + thread_buff.len, buf, len);
    thread_buff.len += len;
  }

  return 0;
}

static inline sstring_s *unpack_string(uint8_t *buf, size_t len, size_t *pos) {
  thread_buff.len = 0;
  _Bool compressed = buf[*pos] & 128;
  int64_t l = decode_int(buf, len, 7, pos);
  if (l < 0)
    return NULL;
  len = l;
  if (compressed) {
    if (unpack_huffman(buf + (*pos), len) == NULL)
      return NULL;
  } else {
    thread_buff.len = len;
    memcpy(thread_buff.data, buf + (*pos), len);
  }
  *pos += len;
  return (sstring_s *)(&thread_buff);
}

#if defined(DEBUG) && DEBUG == 1

void hpack_test_string_packing(void) {
  size_t pos = 0;
  if (unpack_string((uint8_t *)"\x0a\x63\x75\x73\x74\x6f\x6d\x2d\x6b\x65\x79",
                    11, &pos) == NULL) {
    fprintf(stderr, "* HPACK STRING UNPACKING FAILED(!) for example.\n");
  } else {
    if (thread_buff.len != 10)
      fprintf(stderr, "* HPACK STRING UNPACKING ERROR example len %u != 10.\n",
              thread_buff.len);
    if (memcmp(thread_buff.data, "\x63\x75\x73\x74\x6f\x6d\x2d\x6b\x65\x79",
               10))
      fprintf(stderr, "* HPACK STRING UNPACKING ERROR example returned: %.*s\n",
              (int)thread_buff.len, thread_buff.data);
  }

  pos = 0;
  if (unpack_string(
          (uint8_t *)"\x8c\xf1\xe3\xc2\xe5\xf2\x3a\x6b\xa0\xab\x90\xf4\xff", 13,
          &pos) == NULL) {
    fprintf(stderr,
            "* HPACK STRING UNPACKING FAILED(!) for compressed example.\n");
  } else {
    if (thread_buff.len != 15)
      fprintf(
          stderr,
          "* HPACK STRING UNPACKING ERROR compressed example len %u != 15.\n",
          thread_buff.len);
    if (memcmp(thread_buff.data, "www.example.com", 10))
      fprintf(
          stderr,
          "* HPACK STRING UNPACKING ERROR compressed example returned: %.*s\n",
          (int)thread_buff.len, thread_buff.data);
  }

  uint8_t *str1 =
      (uint8_t *)"This is a string to be packed, either compressed or not.";

  thread_buff.len = 0;
  size_t i;
  for (i = 0; i < 128; i++) {
    if (pack_string(str1, 56, i & 1))
      fprintf(stderr, "* HPACK STRING PACKING FAIL AT %lu\n", i);
  }
  uint16_t len = thread_buff.len;
  uint8_t buff[len];
  memcpy(buff, thread_buff.data, len);

  pos = 0;
  i = 0;
  while (pos < len) {
    if (unpack_string(buff, len, &pos) == NULL)
      fprintf(stderr, "* HPACK STRING UNPACKING FAILED(!) AT %lu\n", i);
    if (thread_buff.len != 56)
      fprintf(stderr, "* HPACK STRING UNPACKING ERROR AT %lu - got string "
                      "length %u instead of 56.\n",
              i, thread_buff.len);
    if (memcmp(str1, thread_buff.data, 56))
      fprintf(stderr, "* HPACK STRING UNPACKING ERROR AT %lu. Got %.*s\n", i,
              (int)thread_buff.len, thread_buff.data);
    i++;
  }
  fprintf(stderr,
          "* HPACK String packing test complete. Detected %lu/128 strings\n",
          i);
}
#endif

/* *****************************************************************************
Integer encoding / decoding
*/

static inline int64_t decode_int(uint8_t *buf, size_t len, uint8_t prefix,
                                 size_t *pos) {
  len -= *pos;
  if (len > 8)
    len = 8;
  uint64_t result = 0;
  uint64_t bit = 0;
  uint8_t mask = ((1 << (prefix)) - 1);

  if ((mask & (buf[*pos])) != mask) {
    result |= (mask & (buf[(*pos)++]));
    return (int64_t)result;
  }

  ++(*pos);
  --len;

  while (len && (buf[*pos] & 128)) {
    result |= ((buf[*pos] & 0x7fU) << (bit));
    bit += 7;
    ++(*pos);
    --len;
  }
  if (!len) {
    return -1;
  }
  result |= ((buf[*pos] & 0x7fU) << bit);
  result += mask;

  ++(*pos);
  return (int64_t)result;
}

static inline int encode_int(uint64_t i, uint8_t prefix) {
  uint8_t mask = ((1 << (prefix)) - 1);
  if (i < mask) {
    // zero out prefix bits
    thread_buff.data[thread_buff.len] &= ~mask;
    // fill in i;
    thread_buff.data[thread_buff.len] |= i;
    thread_buff.len += 1;
    if (thread_buff.len >= HPACK_BUFFER_SIZE)
      return -1;
    return 0;
  }

  thread_buff.data[thread_buff.len] |= mask;
  ++thread_buff.len;
  if (thread_buff.len >= HPACK_BUFFER_SIZE)
    return -1;

  i -= mask;

  while (i >> 7) {
    thread_buff.data[thread_buff.len] = (1 << 7) | (i & ((1 << 7) - 1));
    ++thread_buff.len;
    if (thread_buff.len >= HPACK_BUFFER_SIZE)
      return -1;
    i >>= 7;
  }

  thread_buff.data[thread_buff.len] = i & 0x7fU;
  ++thread_buff.len;
  if (thread_buff.len >= HPACK_BUFFER_SIZE)
    return -1;

  return 0;
}

#if defined(DEBUG) && DEBUG == 1

void hpack_test_int_primitive(void) {
  int64_t result;
  size_t pos = 0;
  if ((result = decode_int((uint8_t *)"\x0c", 1, 4, &pos)) != 12)
    fprintf(stderr, "* HPACK INTEGER DECODER ERROR ex. 0c 12 != %" PRId64 "\n",
            result);

  pos = 0;
  if ((result = decode_int((uint8_t *)"\x1f\x9a\x0a", 3, 5, &pos)) != 1337)
    fprintf(stderr,
            "* HPACK INTEGER DECODER ERROR ex. \\x1f\\x9a\\x0a 1337 != %" PRId64
            "\n",
            result);

  thread_buff.len = 0;
  for (int64_t i = 1; i < (1 << 21); i <<= 1) {
    if (encode_int((((i + 5) * 7) / 3), ((i / 7) & 7)))
      fprintf(stderr, "* HPACK INTEGER ENCODE ERROR 1 ( %" PRId64
                      ") (prefix == %" PRId64 ")\n",
              i, ((i / 7) & 7));
  }
  pos = 0;
  for (int64_t i = 1; i < (1 << 21); i <<= 1) {
    result = decode_int(thread_buff.data, thread_buff.len, ((i / 7) & 7), &pos);
    if (result < 0)
      fprintf(stderr, "* HPACK INTEGER DECODE ERROR 1 ( %" PRId64
                      ") (prefix == %" PRId64 ")\n",
              i, ((i / 7) & 7));
    else if (result != (((i + 5) * 7) / 3))
      fprintf(stderr, "* HPACK INTEGER DECODE ERROR 2 expected %" PRId64
                      " got %" PRId64 " (prefix == %" PRId64 ")\n",
              (((i + 5) * 7) / 3), result, ((i / 7) & 7));
  }
  fprintf(stderr, "* HPACK Integer Primitive test complete.\n");
}
#endif
/* *****************************************************************************
Huffman encoding / decoding
*/
static sstring_s *unpack_huffman(void *buf, size_t len) {
  thread_buff.len = 0;
  const struct huffman_decode_node *node;
  node = &hpack_huffman_decode_tree;
  for (size_t i = 0; i < len; i++) {
#define ___review_bit(bit)                                                     \
  do {                                                                         \
    if (((uint8_t *)buf)[i] & (1 << (7 - bit)))                                \
      node = node->one;                                                        \
    else                                                                       \
      node = node->zero;                                                       \
  } while (0);

#define ___review_value()                                                      \
  do {                                                                         \
    if (node->value == 256U) {                                                 \
      goto done;                                                               \
    }                                                                          \
    if (node->value < 256U) {                                                  \
      thread_buff.data[thread_buff.len] = (uint8_t)node->value;                \
      thread_buff.len += 1;                                                    \
      if (thread_buff.len >= HPACK_BUFFER_SIZE)                                \
        return NULL;                                                           \
      node = &hpack_huffman_decode_tree;                                       \
    }                                                                          \
  } while (0);

#define ___process_bit(bit)                                                    \
  ___review_bit(bit);                                                          \
  ___review_value();

    ___process_bit(0);
    ___process_bit(1);
    ___process_bit(2);
    ___process_bit(3);
    ___process_bit(4);
    ___process_bit(5);
    ___process_bit(6);
    ___process_bit(7);

#undef ___review_bit
#undef ___review_value
#undef ___process_bit
  }
done:
  return (sstring_s *)&thread_buff;
}

static int pack_huffman(void *buf, size_t len) {
  thread_buff.data[thread_buff.len] = 0;
  uint8_t *pos = buf;
  uint8_t *end = pos + len;
  uint8_t bits, rem = 8;
  uint32_t code;
  do {
    bits = huffman_encode_table[*pos].bits;
    code = huffman_encode_table[*pos].code;
    if (rem == 0) {
      rem = 8;
      thread_buff.len += 1;
      if (thread_buff.len >= HPACK_BUFFER_SIZE)
        return -1;
    }
    if ((rem & 7) && rem <= bits) {
      thread_buff.data[thread_buff.len] <<= rem;
      thread_buff.data[thread_buff.len] |= code >> (bits - rem);
      bits -= rem;
      rem = 8;
      thread_buff.len += 1;
      if (thread_buff.len >= HPACK_BUFFER_SIZE)
        return -1;
    }
    while (bits >= 8) {
      thread_buff.data[thread_buff.len] = (code >> (bits - 8)) & 0xFF;
      bits -= 8;
      thread_buff.len += 1;
      if (thread_buff.len >= HPACK_BUFFER_SIZE)
        return -1;
    }
    if (bits) {
      thread_buff.data[thread_buff.len] <<= bits;
      thread_buff.data[thread_buff.len] |= (code & ((1 << bits) - 1));
      rem -= bits;
    }
    ++pos;
  } while (pos < end);

  if (rem & 7) {
    // pad last bits as 1.
    thread_buff.data[thread_buff.len] <<= rem;
    thread_buff.data[thread_buff.len] |= ((1 << rem) - 1);
    thread_buff.len += 1;
  }
  return 0;
}

#if defined(DEBUG) && DEBUG == 1
void hpack_test_huffman(void) {

  if (unpack_huffman("\x9d\x29\xad\x17\x18\x63\xc7\x8f\x0b\x97\xc8\xe9\xae\x82"
                     "\xae\x43\xd3",
                     17) == NULL)
    fprintf(stderr, "* HPACK HUFFMAN TEST FAILED unpacking error (1).\n");
  else if (memcmp(thread_buff.data, "https://www.example.com", 23) ||
           thread_buff.len != 23)
    fprintf(stderr, "* HPACK HUFFMAN TEST FAILED result error (1).\n");

  if (unpack_huffman("\xf1\xe3\xc2\xe5\xf2\x3a\x6b\xa0\xab\x90\xf4\xff", 12) ==
      NULL)
    fprintf(stderr, "* HPACK HUFFMAN TEST FAILED unpacking error (2).\n");
  else if (memcmp(thread_buff.data, "www.example.com", 15) ||
           thread_buff.len != 15)
    fprintf(stderr, "* HPACK HUFFMAN TEST FAILED result error (2).\n");

  thread_buff.len = 0;
  if (pack_huffman("I want to go home... but I have to write tests... woohoo!",
                   57)) {
    fprintf(stderr, "* HPACK HUFFMAN TEST FAILED packing error (3).\n");
  } else {
    uint8_t str2[thread_buff.len];
    memcpy(str2, thread_buff.data, thread_buff.len);
    if (unpack_huffman(str2, thread_buff.len) == NULL)
      fprintf(stderr, "* HPACK HUFFMAN TEST FAILED unpacking error (3).\n");
    else if (memcmp(thread_buff.data,
                    "I want to go home... but I have to write tests... woohoo!",
                    57) ||
             thread_buff.len != 57)
      fprintf(stderr, "* HPACK HUFFMAN TEST FAILED result error (3).\n*    Got "
                      "(%u): %.*s\n",
              thread_buff.len, (int)thread_buff.len, thread_buff.data);
  }
  fprintf(stderr, "* HPACK Huffman test finished.\n");
}
#endif

/*
static const char *static_data[] = {
    NULL,  "GET",           "POST", "/",   "/index.html", "http", "https",
    "200", "204",           "206",  "304", "400",         "404",  "500",
    NULL,  "gzip, deflate",
};

static const char *static_headers[] = {
    ":authority",
    ":method",
    ":method",
    ":path",
    ":path",
    ":scheme",
    ":scheme",
    ":status",
    ":status",
    ":status",
    ":status",
    ":status",
    ":status",
    ":status",
    "accept-charset",
    "accept-encoding",
    "accept-language",
    "accept-ranges",
    "accept",
    "access-control-allow-origin",
    "age",
    "allow",
    "authorization",
    "cache-control",
    "content-disposition",
    "content-encoding",
    "content-language",
    "content-length",
    "content-location",
    "content-range",
    "content-type",
    "cookie",
    "date",
    "etag",
    "expect",
    "expires",
    "from",
    "host",
    "if-match",
    "if-modified-since",
    "if-none-match",
    "if-range",
    "if-unmodified-since",
    "last-modified",
    "link",
    "location",
    "max-forwards",
    "proxy-authenticate",
    "proxy-authorization",
    "range",
    "referer",
    "refresh",
    "retry-after",
    "server",
    "set-cookie",
    "strict-transport-security",
    "transfer-encoding",
    "user-agent",
    "vary",
    "via",
    "www-authenticate",
};
*/
