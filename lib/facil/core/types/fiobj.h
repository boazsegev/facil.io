/*
Copyright: Boaz Segev, 2017
License: MIT
*/
#ifndef FIOBJ_H
#define FIOBJ_H

#include "fiobject.h"

#include "fiobj_ary.h"
#include "fiobj_hash.h"
#include "fiobj_io.h"
#include "fiobj_json.h"
#include "fiobj_numbers.h"
#include "fiobj_primitives.h"
#include "fiobj_str.h"
#include "fiobj_sym.h"

// /** A helper macro for sending fiobj_s Strings(!) through the `sock` library.
// */
// #define fiobj_send(uuid, obj) \
//   sock_write2(.uuid = (uuid), .buffer = (obj), \
//               .offset = (((uintptr_t)fiobj_obj2cstr((obj)).data) - \
//                          ((uintptr_t)(obj))), \
//               .length = fiobj_obj2cstr((obj)).length, \
//               .dealloc = (void
//               (*)(void *))fiobj_free)

#endif
