/* *****************************************************************************
IO related operations (set protocol, read, write, etc')
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */

FIO_IFUNC sstr_s fio_env_name2str(fio_str_info_s name, uint8_t is_const) {
  sstr_s s;
  if (is_const) {
    sstr_init_const(&s, name.buf, name.len);
  } else {
    sstr_init_copy(&s, name.buf, name.len);
  }
  return s;
}

/**
 * Links an object to a connection's lifetime / environment.
 */
void fio_env_set FIO_NOOP(fio_s *io, fio_env_set_args_s args) {
  sstr_s key = fio_env_name2str(args.name, args.const_name);
  env_obj_s val = {
      .udata = args.udata,
      .on_close = args.on_close,
  };
  env_safe_s *e = io ? &io->env : &fio_data.env;
  env_safe_set(e, key, args.type, val);
}

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 */
int fio_env_unset FIO_NOOP(fio_s *io, fio_env_unset_args_s args) {
  sstr_s key = fio_env_name2str(args.name, 1);
  env_safe_s *e = io ? &io->env : &fio_data.env;
  return env_safe_unset(e, key, args.type);
}

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 */
int fio_env_remove FIO_NOOP(fio_s *io, fio_env_unset_args_s args) {
  sstr_s key = fio_env_name2str(args.name, 1);
  env_safe_s *e = io ? &io->env : &fio_data.env;
  return env_safe_remove(e, key, args.type);
}
