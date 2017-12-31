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
  else if (obj->type == FIOBJ_T_ARRAY) {
    fiobj_str_join(str, fiobj_num_tmp(fiobj_ary_count(obj)));
    fiobj_str_write(str, "\r\n", 2);
  } else if (obj->type == FIOBJ_T_HASH) {
    fiobj_str_join(str, fiobj_num_tmp(fiobj_hash_count(obj)));
    fiobj_str_write(str, "\r\n", 2);
  } else if (obj->type == FIOBJ_T_COUPLET) {
    fiobj_str_write(str, "*2\r\n", 4);
    fiobj_str_join(str, fiobj_couplet2key(obj));
    fiobj_str_write(str, "\r\n", 2);
    resp_fioformat_task(fiobj_couplet2obj(obj), s_);
  } else {
    fiobj_str_join(str, obj);
    fiobj_str_write(str, "\r\n", 2);
  }
  return 0;
}

fiobj_pt resp_fioformat(fiobj_pt obj) {
  fiobj_pt str = fiobj_str_buf(4096);
  fiobj_each2(obj, resp_fioformat_task, str);
  return str;
}
