/*
Copyright: Boaz Segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.

Copyright refers to the parser, not the protocol.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fio2resp.h"
#include "fiobj.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct resp_format_s {
  uint8_t *dest;
  size_t *size;
  size_t limit;
  int err;
};

static int resp_fioformat_task(fiobj_s *obj, void *s_) {
  struct resp_format_s *s = s_;
  char num_buffer[68];

#define safe_write_eol()                                                       \
  if ((*s->size += 2) <= s->limit) {                                           \
    *s->dest++ = '\r';                                                         \
    *s->dest++ = '\n';                                                         \
  }
#define safe_write1(data)                                                      \
  if (++(*s->size) <= s->limit) {                                              \
    *s->dest++ = (data);                                                       \
  }
#define safe_write2(data, len)                                                 \
  do {                                                                         \
    *s->size += (len);                                                         \
    if (*s->size <= s->limit) {                                                \
      memcpy(s->dest, (data), (len));                                          \
      s->dest += (len);                                                        \
    }                                                                          \
  } while (0)

  fio_cstr_s str = {.data = NULL};

  switch (obj->type) {
  case FIOBJ_T_FALSE:
    str = (fio_cstr_s){.data = "false", .len = 5};
    break;
  case FIOBJ_T_TRUE:
    str = (fio_cstr_s){.data = "true", .len = 4};
    break;
  case FIOBJ_T_NUMBER:
  case FIOBJ_T_STRING:
  case FIOBJ_T_SYMBOL:
    str = fiobj_obj2cstr(obj);
    break;
  case FIOBJ_T_FLOAT:
    str = (fio_cstr_s){.data = num_buffer,
                       .len = fio_ltoa(num_buffer, fiobj_obj2num(obj), 10)};
    break;
  case FIOBJ_T_FILE:
  case FIOBJ_T_IO:
  case FIOBJ_T_NULL:
    safe_write2("$-1\r\n", (resp_obj2str(obj)->len));
    return 0;
  case FIOBJ_T_ARRAY:
    safe_write1('*');
    str = (fio_cstr_s){.data = num_buffer,
                       .len = fio_ltoa(num_buffer, fiobj_ary_count(obj), 10)};
    break;
  case FIOBJ_T_HASH:
    safe_write1('*');
    str = (fio_cstr_s){
        .data = num_buffer,
        .len = fio_ltoa(num_buffer, (fiobj_hash_count(obj) << 1), 10)};
    break;
  case FIOBJ_T_COUPLET:
    resp_fioformat_task(fiobj_couplet2key(obj), s_);
    resp_fioformat_task(fiobj_couplet2obj(obj), s_);
    return 0;
  }
  safe_write2(str.buffer, str.len);
  safe_write_eol();
  return 0;
}
#undef safe_write_eol
#undef safe_write1
#undef safe_write2
#undef safe_write_i

int resp_fioformat(uint8_t *dest, size_t *size, fiobj_pt obj) {
  struct resp_format_s arg = {
      .dest = dest, .size = size, .limit = *size,
  };
  *size = 0;
  fiobj_each2(obj, resp_fioformat_task, &arg);
  if (*arg.size < arg.limit) {
    *arg.dest = 0;
    return 0;
  }
  if (*arg.size == arg.limit)
    return 0;
  return -1;
}
