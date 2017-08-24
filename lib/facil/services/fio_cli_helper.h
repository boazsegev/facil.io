/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_CLI_HELPER_H
/**

This is a customized version for command line interface (CLI) arguments. All
arguments are converted all command line arguments into a key-value paired Hash.

The CLI helper automatically provides `-?`, `-h` and `-help` support that
prints a short explanation for every option and exits.

The CLI will parse arguments in the form `-arg value` as well as `-arg=value`
and `-argvalue`

NOTICE:

This interface is NOT thread-safe, since normally the command line arguments are
parsed before new threads are spawned.

It's important to set all the requirement before requesting any results,
otherwise, parsing errors might occure - i.e., allowed parameters would be
parsed as errors, since they hadn't been declared yet.

EXAMPLE:

    // initialize the CLI helper.
    fio_cli_start(argc, argv, "App description or NULL");

    // setup possible command line arguments.
    fio_cli_accept_num("port p", "the port to listen to, defaults to 3000.");
    fio_cli_accept_bool("log v", "enable logging");

    // read command line arguments and copy results.
    uint8_t logging = fio_cli_get_int("v");
    const char *port = fio_cli_get_str("port");
    if (!port)
      port = "3000";
    fio_cli_end();
    // .. use parsed information.


*/
#define H_FIO_CLI_HELPER_H

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the CLI helper and adds the `info` string to the help section */
void fio_cli_start(int argc, const char **argv, const char *info);

/** Clears the memory and resources used by the CLI helper */
void fio_cli_end(void);

/**
 * Sets a CLI acceptable argument of type Number (both `int` and `float`).
 *
 * The `aliases` string sets aliases for the same argument. i.e. "string s".
 * Notice that collisions will prefer new information quitely.
 *
 * The first alias will be the name available for `fio_cli_get_*` functions.
 *
 * The `desc` string will be printed if `-?`, `-h` of `-help` are used.
 *
 * The function will crash the application on failure, printing an error
 * message.
 */
void fio_cli_accept_num(const char *aliases, const char *desc);

/**
 * Sets a CLI acceptable argument of type String.
 *
 * The `aliases` string sets aliases for the same argument. i.e. "string s".
 *
 * The first alias will be the name used
 *
 * The `desc` string will be printed if `-?`, `-h` of `-help` are used.
 *
 * The function will crash the application on failure, printing an error
 * message.
 */
void fio_cli_accept_str(const char *aliases, const char *desc);

/**
 * Sets a CLI acceptable argument of type Bool (true if exists).
 *
 * The `aliases` string sets aliases for the same argument. i.e. "string s".
 *
 * The first alias will be the name available for `fio_cli_get_*` functions.
 *
 * The `desc` string will be printed if `-?`, `-h` of `-help` are used.
 *
 * The function will crash the application on failure, printing an error
 * message.
 */
void fio_cli_accept_bool(const char *aliases, const char *desc);

/**
 * Returns a C String containing the value of the received argument, or NULL if
 * none.
 *
 * Boolean that were set to TRUE have the string "1".
 */
const char *fio_cli_get_str(const char *opt);

/**
 * Returns an Integer containing the parsed value of the argument.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
int fio_cli_get_int(const char *opt);

/**
 * Returns a Float containing the parsed value of the argument.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
double fio_cli_get_float(const char *opt);

/**
 * Overrides the existing value of the argument with the requested C String.
 *
 * The String isn't copied, it's only referenced.
 *
 * Boolean that were set to TRUE have the string "1".
 */
void fio_cli_set_str(const char *opt, const char *value);

/**
 * Overrides the existing value of the argument with the requested Integer.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
void fio_cli_set_int(const char *opt, int value);

/**
 * Overrides the existing value of the argument with the requested Float.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
void fio_cli_set_float(const char *opt, double value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
