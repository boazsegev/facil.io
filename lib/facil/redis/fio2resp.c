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

static int resp_fioformat_task(fiobj_s *obj, void *s_) {
  fiobj_s *str = s_;

  switch (obj->type) {
  case FIOBJ_T_FALSE:
    fiobj_str_write(str, "false\r\n", 7);
    break;
  case FIOBJ_T_TRUE:
    fiobj_str_write(str, "true\r\n", 6);
    break;
  case FIOBJ_T_STRING:
  case FIOBJ_T_SYMBOL: {
    /* use this opportunity to optimize memory allocation to page boundries */
    fio_cstr_s s = fiobj_obj2cstr(str);
    if (fiobj_str_capa(str) <= s.len + 128 + fiobj_obj2cstr(obj).len) {
      fiobj_str_resize(str, (((fiobj_str_capa(str) >> 12) + 1) << 12) - 1);
      fiobj_str_resize(str, s.len);
    }
  }
  /* fallthrough */
  case FIOBJ_T_NUMBER:
  case FIOBJ_T_FLOAT:
    fiobj_str_join(str, obj);
    fiobj_str_write(str, "\r\n", 2);
    break;
  case FIOBJ_T_IO:
  case FIOBJ_T_NULL:
    fiobj_str_write(str, "$-1\r\n", 4);
    return 0;
  case FIOBJ_T_ARRAY:
    fiobj_str_write2(str, "*%lu\r\n", (unsigned long)fiobj_ary_count(obj));
    break;
  case FIOBJ_T_HASH:
    fiobj_str_write2(str, "*%lu\r\n", (unsigned long)fiobj_hash_count(obj));
    break;
  case FIOBJ_T_COUPLET:
    resp_fioformat_task(fiobj_couplet2key(obj), s_);
    resp_fioformat_task(fiobj_couplet2obj(obj), s_);
    return 0;
  }
  return 0;
}
#undef safe_write_eol
#undef safe_write1
#undef safe_write2
#undef safe_write_i

fiobj_pt resp_fioformat(fiobj_pt obj) {
  fiobj_pt str = fiobj_str_buf(4096);
  fiobj_each2(obj, resp_fioformat_task, str);
  return str;
}
