/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fiobj_types.h"

/* *****************************************************************************
IO VTable
***************************************************************************** */

static int64_t fio_io2i(fiobj_s *io) { return (int64_t)obj2io(io)->fd; }

static int fiobj_io_is_eq(fiobj_s *self, fiobj_s *other) {
  if (!other || other->type != self->type ||
      obj2io(self)->fd != obj2io(other)->fd)
    return 0;
  return 1;
}

static struct fiobj_vtable_s FIOBJ_VTABLE_IO = {
    .free = fiobj_simple_dealloc,
    .to_i = fio_io2i,
    .to_f = fiobj_noop_f,
    .to_str = fiobj_noop_str,
    .is_eq = fiobj_io_is_eq,
    .count = fiobj_noop_count,
    .each1 = fiobj_noop_each1,
};

/* *****************************************************************************
IO API
***************************************************************************** */

/** Wrapps a file descriptor in an IO object. Use `fiobj_free` to close. */
fiobj_s *fio_io_wrap(intptr_t fd) {
  fiobj_head_s *head;
  head = malloc(sizeof(*head) + sizeof(fio_io_s));
  if (!head)
    perror("ERROR: fiobj IO couldn't allocate memory"), exit(errno);
  *head = (fiobj_head_s){
      .ref = 1, .vtable = &FIOBJ_VTABLE_IO,
  };
  *obj2io(HEAD2OBJ(head)) = (fio_io_s){.type = FIOBJ_T_IO, .fd = fd};
  return HEAD2OBJ(head);
}

/**
 * Return an IO's fd.
 *
 * A type error results in -1.
 */
intptr_t fiobj_io_fd(fiobj_s *obj) {
  if (obj->type != FIOBJ_T_IO)
    return -1;
  return ((fio_io_s *)obj)->fd;
}
