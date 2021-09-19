/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
Connection Object Links / Environment
***************************************************************************** */

/** Named arguments for the `fio_env_set` function. */
typedef struct {
  /** A numerical type filter. Defaults to 0. Negative values are reserved. */
  intptr_t type;
  /** The name for the link. The name and type uniquely identify the object. */
  fio_str_info_s name;
  /** The object being linked to the connection. */
  void *udata;
  /** A callback that will be called once the connection is closed. */
  void (*on_close)(void *data);
  /** Set to true (1) if the name string's life lives as long as the `env` . */
  uint8_t const_name;
} fio_env_set_args_s;

/** Named arguments for the `fio_env_unset` function. */
typedef struct {
  intptr_t type;
  fio_str_info_s name;
} fio_env_unset_args_s;

/**
 * Links an object to a connection's lifetime / environment.
 *
 * The `on_close` callback will be called once the connection has died.
 *
 * If the `io` is NULL, the value will be set for the global environment.
 */
void fio_env_set(fio_s *io, fio_env_set_args_s);

/**
 * Links an object to a connection's lifetime, calling the `on_close` callback
 * once the connection has died.
 *
 * If the `io` is NULL, the value will be set for the global environment, in
 * which case the `on_close` callback will only be called once the process
 * exits.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_set(io, ...) fio_env_set(io, (fio_env_set_args_s){__VA_ARGS__})

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found.
 */
int fio_env_unset(fio_s *io, fio_env_unset_args_s);

/**
 * Un-links an object from the connection's lifetime, so it's `on_close`
 * callback will NOT be called.
 *
 * Returns 0 on success and -1 if the object couldn't be found.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_unset(io, ...)                                                 \
  fio_env_unset(io, (fio_env_unset_args_s){__VA_ARGS__})

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 */
int fio_env_remove(fio_s *io, fio_env_unset_args_s);

/**
 * Removes an object from the connection's lifetime / environment, calling it's
 * `on_close` callback as if the connection was closed.
 *
 * This is a helper MACRO that allows the function to be called using named
 * arguments.
 */
#define fio_env_remove(io, ...)                                                \
  fio_env_remove(io, (fio_env_unset_args_s){__VA_ARGS__})
