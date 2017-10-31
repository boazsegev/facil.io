/*
Copyright: Boaz Segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.

Copyright refers to the parser, not the protocol.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fiobj.h"
#include "resp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int resp_fioformat_task(fiobj_s *obj, void *s_) {
  fiobj_s *str = s_;

  if (obj->type == FIOBJ_T_FALSE)
    fiobj_str_write(str, "false\r\n", 7);
  else if (obj->type == FIOBJ_T_TRUE)
    fiobj_str_write(str, "true\r\n", 6);
  else if (obj->type == FIOBJ_T_NULL)
    fiobj_str_write(str, "$-1\r\n", 4);
  else if (obj->type == FIOBJ_T_ARRAY)
    fiobj_str_write2(str, "*%lu\r\n", (unsigned long)fiobj_ary_count(obj));
  else if (obj->type == FIOBJ_T_HASH)
    fiobj_str_write2(str, "*%lu\r\n", (unsigned long)fiobj_hash_count(obj));
  else if (obj->type == FIOBJ_T_COUPLET) {
    fiobj_str_write(str, "*2\r\n", 4);
    resp_fioformat_task(fiobj_couplet2key(obj), s_);
    resp_fioformat_task(fiobj_couplet2obj(obj), s_);
  } else {
    if (obj->type == FIOBJ_T_SYMBOL || FIOBJ_IS_STRING(obj)) {
      /* use this opportunity to optimize memory allocation to page boundries */
      fio_cstr_s s = fiobj_obj2cstr(str);
      if (fiobj_str_capa(str) <= s.len + 128 + fiobj_obj2cstr(obj).len) {
        fiobj_str_resize(str, (((fiobj_str_capa(str) >> 12) + 1) << 12) - 1);
        fiobj_str_resize(str, s.len);
      }
    }
    fiobj_str_join(str, obj);
    fiobj_str_write(str, "\r\n", 2);
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
