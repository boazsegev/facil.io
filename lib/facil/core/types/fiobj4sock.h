#ifndef H_FIOBJ4SOCK_H
#define H_FIOBJ4SOCK_H
/**
 * Defines a helper for using fiobj with the sock library.
 */

#include "fiobj.h"
#include "sock.h"

static void my_dealloc(void *o) { fiobj_free(o); }

/** send a fiobj_s * object through a socket. */
static inline __attribute__((unused)) int fiobj_send(intptr_t uuid,
                                                     fiobj_s *o) {
  fio_cstr_s s = fiobj_obj2cstr(o);
  // fprintf(stderr, "%s\n", s.data);
  return sock_write2(.uuid = uuid, .buffer = (o),
                     .offset = (((intptr_t)s.data) - ((intptr_t)(o))),
                     .length = s.length,
                     .dealloc = my_dealloc); // (void (*)(void *))fiobj_free
}

#endif
