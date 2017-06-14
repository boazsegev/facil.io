#ifndef H_RESP_PARSER_H
/**
This is a neive implementation of the RESP protocol for Redis.
*/
#define H_RESP_PARSER_H

#include <stdint.h>
#include <stdlib.h>

enum resp_type_enum {
  RESP_ERR = 0,
  RESP_NULL,
  RESP_OK,
  RESP_ARRAY,
  RESP_STRING,
  RESP_NUMBER,
};

typedef struct { enum resp_type_enum type; } resp_object_s;

typedef struct {
  enum resp_type_enum type;
  size_t len;
  size_t pos; /** allows simple iteration. */
  resp_object_s *array[];
} resp_array_s;

typedef struct {
  enum resp_type_enum type;
  size_t len;
  uint8_t string[];
} resp_string_s;

typedef struct {
  enum resp_type_enum type;
  int64_t number;
} resp_number_s;

#define resp_obj2arr(obj)                                                      \
  ((resp_array_s *)(obj->type == RESP_ARRAY ? (obj) : NULL))
#define resp_obj2str(obj)                                                      \
  ((resp_string_s *)(obj->type == RESP_STRING || obj->type == RESP_ERR         \
                         ? (obj)                                               \
                         : NULL))
#define resp_obj2num(obj)                                                      \
  ((resp_number_s *)(obj->type == RESP_NUMBER ? (obj) : NULL))

typedef struct resp_parser_s *resp_parser_pt;

/** create the parser */
resp_parser_pt resp_parser_new(void);

/** free the parser and it's resources. */
void resp_parser_destroy(resp_parser_pt);

/** frees an object returned from the parser. */
void resp_free_object(resp_object_s *obj);

/**
 * Feed the parser with data.
 *
 * Returns any fully parsed object / reply (often an array, but not always) or
 * NULL (needs more data / error).
 *
 * If a RESP object was parsed, it is returned and `len` is updated to reflect
 * the number of bytes actually read.
 *
 * If more data is needed, NULL is returned and `len` is left unchanged.
 *
 * An error is reported by by returning NULL and setting `len` to 0 at the same
 * time.
 *
 * Partial consumption is possible when multiple replys were available in the
 * buffer. Otherwise the parser will consume the whole of the buffer.
 *
 */
resp_object_s *resp_parser_feed(resp_parser_pt, uint8_t *buffer, size_t *len);

/**
 * Formats a RESP object back into a string.
 *
 * Returns 0 on success and -1 on failur.
 *
 * Accepts a memory buffer `dest` to which the data will be written and a poiner
 * to the size of the buffer.
 *
 * Once the function returns, `size` will be updated to include the number of
 * bytes required for the string. If the function returned a failuer, this value
 * can be used to allocate enough memory to contain the string.
 *
 * The string is Binary safe and it ISN'T always NUL terminated.
 */
int resp_format(uint8_t *dest, size_t *size, resp_object_s *obj);

#if DEBUG == 1
void resp_test(void);
#endif

#endif
