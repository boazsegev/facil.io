/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_CLI_HELPER_H
#define H_FIO_CLI_HELPER_H

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* *****************************************************************************
CLI API
***************************************************************************** */

/** Indicates the previous CLI argument should be a String (default). */
#define FIO_CLI_TYPE_STRING ((char *)0x1)
/** Indicates the previous CLI argument is a Boolean value. */
#define FIO_CLI_TYPE_BOOL ((char *)0x2)
/** Indicates the previous CLI argument should be an Integer (numerical). */
#define FIO_CLI_TYPE_INT ((char *)0x3)

/**
 * This function parses the Command Line Interface (CLI), creating a temporary
 * "dictionary" that allows easy access to the CLI using their names or aliases.
 *
 * Command line arguments may be typed. If an optional type requirement is
 * provided and the provided arument fails to match the required type, execution
 * will end and an error message will be printed along with a short "help".
 *
 * The following optional type requirements are:
 *
 * * FIO_CLI_TYPE_STRING - (default) string argument.
 * * FIO_CLI_TYPE_BOOL   - boolean argument (no value).
 * * FIO_CLI_TYPE_INT    - integer argument ('-', '+', '0'-'9' chars accepted).
 *
 * Argument names MUST start with the '-' character. The first word starting
 * without the '-' character will begin the description for the CLI argument.
 *
 * The arguments "-?", "-h", "-help" and "--help" are automatically handled
 * unless overridden.
 *
 * Example use:
 *
 *    fio_cli_start(argc, argv, 0, "this example accepts the following:",
 *                  "-t -thread number of threads to run.", FIO_CLI_TYPE_INT,
 *                  "-w -workers number of workers to run.", FIO_CLI_TYPE_INT,
 *                  "-b, -address the address to bind to.",
 *                  "-p,-port the port to bind to.", FIO_CLI_TYPE_INT,
 *                  "-v -log enable logging.", FIO_CLI_TYPE_BOOL);
 *
 *
 * This would allow access to the named arguments:
 *
 *      fio_cli_get("-b") == fio_cli_get("-address");
 *
 *
 * Once all the data was accessed, free the parsed data dictionary using:
 *
 *      fio_cli_end();
 *
 * It should be noted, arguments will be recognized in a number of forms, i.e.:
 *
 *      app -t=1 -p3000 -a localhost
 *
 * This function is NOT thread safe.
 */
#define fio_cli_start(argc, argv, allow_unknown, description, ...)             \
  fio_cli_start((argc), (argv), (allow_unknown), (description),                \
                (char const *[]){__VA_ARGS__, NULL})
#define FIO_CLI_IGNORE
/**
 * Never use the function directly, always use the MACRO, because the macro
 * attaches a NULL marker at the end of the `names` argument collection.
 */
void fio_cli_start FIO_CLI_IGNORE(int argc, char const *argv[],
                                  int allow_unknown, char const *description,
                                  char const **names);
/**
 * Clears the memory used by the CLI dictionary, removing all parsed data.
 *
 * This function is NOT thread safe.
 */
void fio_cli_end(void);

/** Returns the argument's value as a NUL terminated C String. */
char const *fio_cli_get(char const *name);

/** Returns the argument's value as an integer. */
int fio_cli_get_i(char const *name);

/** This MACRO returns the argument's value as a boolean. */
#define fio_cli_get_bool(name) (fio_cli_get((name)) != NULL)

/** Returns the number of unrecognized argument. */
unsigned int fio_cli_unknown_count(void);

/** Returns the unrecognized argument using a 0 based `index`. */
char const *fio_cli_unknown(unsigned int index);

/**
 * Sets the argument's value as a NUL terminated C String (no copy!).
 *
 * CAREFUL: This does not automatically detect aliases or type violations! it
 * will only effect the specific name given, even if invalid. i.e.:
 *
 *     fio_cli_start(argc, argv,
 *                  "this is example accepts the following options:",
 *                  "-p -port the port to bind to", FIO_CLI_TYPE_INT;
 *
 *     fio_cli_set("-p", "hello"); // fio_cli_get("-p") != fio_cli_get("-port");
 *
 * Note: this does NOT copy the C strings to memory. Memory should be kept alive
 * until `fio_cli_end` is called.
 */
void fio_cli_set(char const *name, char const *value);

/**
 * This MACRO is the same as:
 *
 *     if(!fio_cli_get(name)) {
 *       fio_cli_set(name, value)
 *     }
 */
#define fio_cli_set_default(name, value)                                       \
  if (!fio_cli_get((name)))                                                    \
    fio_cli_set(name, value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
