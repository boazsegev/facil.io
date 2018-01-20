/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio_cli.h"
#include "fiobj.h"

#include <string.h>
/* *****************************************************************************
State (static data)
***************************************************************************** */

/* static variables are automatically initialized to 0, which is what we need.*/
static int FIO_CLI_ARGC;
static const char **FIO_CLI_ARGV;
static FIOBJ arg_aliases; /* a hash for translating aliases */
static FIOBJ arg_type;    /* a with information about each argument */
static FIOBJ parsed;      /* a with information about each argument */
static FIOBJ help_str;    /* The CLI help string */
static FIOBJ info_str;    /* The CLI information string */
static int is_parsed;
static int ignore_unknown;

const char DEFAULT_CLI_INFO[] =
    "This application accepts any of the following possible arguments:";
/* *****************************************************************************
Error / Help handling - printing the information and exiting.
***************************************************************************** */

/** Tells the CLI helper to ignore invalid command line arguments. */
void fio_cli_ignore_unknown(void) { ignore_unknown = 1; }

static void fio_cli_handle_error(void) {
  if (ignore_unknown)
    return;
  fio_cstr_s info = fiobj_obj2cstr(info_str);
  fio_cstr_s args = fiobj_obj2cstr(help_str);
  fprintf(stdout,
          "\n"
          "%s\n"
          "%s\n"
          "Use any of the following input formats:\n"
          "\t-arg <value>\t-arg=<value>\t-arg<value>\n"
          "\n"
          "Use the -h, -help or -? to get this information again.\n"
          "\n",
          info.data, args.data);
  fio_cli_end();
  exit(0);
}

/* *****************************************************************************
Initializing the CLI data
***************************************************************************** */

static void fio_cli_init(void) {
  /* if init is called after parsing, discard previous result */
  if (parsed) {
    fiobj_free(parsed);
    parsed = FIOBJ_INVALID;
  }
  /* avoid overwriting existing data */
  if (arg_aliases)
    return;
  arg_aliases = fiobj_hash_new();
  arg_type = fiobj_hash_new();
  help_str = fiobj_str_buf(1024);
  if (!info_str) /* might exist through `fio_cli_start` */
    info_str = fiobj_str_new(DEFAULT_CLI_INFO, sizeof(DEFAULT_CLI_INFO) - 1);
}

/* *****************************************************************************
Matching arguments to C string
***************************************************************************** */

/* returns the primamry symbol for the argument, of NULL (if none) */
static inline FIOBJ fio_cli_get_name(const char *str, size_t len) {
  const uint64_t key = fio_siphash(str, len);
  return fiobj_hash_get2(arg_aliases, key);
}

/* *****************************************************************************
Setting an argument's type and alias.
***************************************************************************** */
typedef enum { CLI_BOOL, CLI_NUM, CLI_STR } cli_type;
static void fio_cli_set(const char *aliases, const char *desc, cli_type type) {
  fio_cli_init();
  const char *start = aliases;
  FIOBJ arg_name = FIOBJ_INVALID;

  while (1) {
    /* get rid of any white space or commas */
    while (start[0] == ' ' || start[0] == ',')
      start++;
    /* we're done */
    if (!start[0])
      return;
    size_t len = 0;
    /* find the length of the argument name */
    while (start[len] != 0 && start[len] != ' ' && start[len] != ',')
      len++;

    if (!arg_name) {
      /* this is the main identifier */
      arg_name = fiobj_str_new(start, len);
      /* add to aliases hash */
      fiobj_hash_set(arg_aliases, arg_name, arg_name);
      /* add the help section and set type*/
      switch (type) {
      case CLI_BOOL:
        fiobj_str_write2(help_str, "\t\x1B[1m-%s\x1B[0m\t\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_null());
        break;
      case CLI_NUM:
        fiobj_str_write2(help_str, "\t\x1B[1m-%s\x1B[0m\x1B[2 ###\x1B[0m\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_true());
        break;
      case CLI_STR:
        fiobj_str_write2(help_str,
                         "\t\x1B[1m-%s\x1B[0m\x1B[2 <val>\x1B[0m\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_false());
        break;
      }
    } else {
      /* this is an alias */
      FIOBJ tmp = fiobj_str_new(start, len);
      /* add to aliases hash */
      fiobj_hash_set(arg_aliases, tmp, fiobj_dup(arg_name));
      /* add to description + free it*/
      fiobj_str_write2(help_str, "\t\t\x1B[1m-%s\x1B[0m\tsame as -%s\n",
                       fiobj_obj2cstr(tmp).data, fiobj_obj2cstr(arg_name).data);
      fiobj_free(tmp);
    }
    start += len;
  }
}

/* *****************************************************************************
parsing the arguments
***************************************************************************** */

static void fio_cli_parse(void) {
  if (!FIO_CLI_ARGC || !FIO_CLI_ARGV) {
    fprintf(
        stderr,
        "ERROR: (fio_cli) fio_cli_get_* "
        "can only be called after `fio_cli_start` and before `fio_cli_end`\n");
    exit(-1);
  }
  if (!arg_aliases) {
    fprintf(stderr, "WARNING: (fio_cli) fio_cli_get_* "
                    "should only be called after `fio_cli_accept_*`\n");
    return;
  }
  if (parsed)
    return;
  parsed = fiobj_hash_new();
  // {
  //   FIOBJ json = fiobj_obj2json(arg_aliases, 1);
  //   fprintf(stderr, "%s\n", fiobj_obj2cstr(json).data);
  //   fiobj_free(json);
  //   json = fiobj_obj2json(arg_type, 1);
  //   fprintf(stderr, "%s\n", fiobj_obj2cstr(json).data);
  //   fiobj_free(json);
  // }

  const char *start;
  FIOBJ arg_name;

  /* ignore the first element, it's the program's name. */
  for (int i = 1; i < FIO_CLI_ARGC; i++) {
    /* test for errors or help requests */
    if (FIO_CLI_ARGV[i][0] != '-' || FIO_CLI_ARGV[i][1] == 0) {
      if (ignore_unknown)
        continue;
      start = FIO_CLI_ARGV[i];
      goto error;
    }
    if ((FIO_CLI_ARGV[i][1] == '?' && FIO_CLI_ARGV[i][2] == 0) ||
        (FIO_CLI_ARGV[i][1] == 'h' &&
         (FIO_CLI_ARGV[i][2] == 0 ||
          (FIO_CLI_ARGV[i][2] == 'e' && FIO_CLI_ARGV[i][3] == 'l' &&
           FIO_CLI_ARGV[i][4] == 'p' && FIO_CLI_ARGV[i][5] == 0)))) {
      fio_cli_handle_error();
      continue;
    }
    /* we walk the name backwards, so `name` is tested before `n` */
    start = FIO_CLI_ARGV[i] + 1;
    size_t len = strlen(start);
    while (len && !(arg_name = fio_cli_get_name(start, len))) {
      --len;
    }
    if (!len)
      goto error;
    /* at this point arg_name is a handle to the argument's Symbol */
    FIOBJ type = fiobj_hash_get(arg_type, arg_name);
    if (FIOBJ_TYPE_IS(type, FIOBJ_T_NULL)) {
      /* type is BOOL, no further processing required */
      start = "1";
      len = 1;
      goto set_arg;
    }
    if (start[len] == 0) {
      i++;
      if (i == FIO_CLI_ARGC)
        goto error;
      start = FIO_CLI_ARGV[i];
    } else if (start[len] == '=') {
      start = start + len + 1;
    } else
      start = start + len;
    len = 0;
    if (FIOBJ_TYPE_IS(type, FIOBJ_T_FALSE)) /* no restrictions on data  */
      goto set_arg;
    /* test that the argument is numerical */
    if (start[len] == '-') /* negative number? */
      len++;
    while (start[len] >= '0' && start[len] <= '9')
      len++;
    if (start[len] == '.') { /* float number? */
      while (start[len] >= '0' && start[len] <= '9')
        len++;
    }
    if (start[len]) /* if there's data left, this aint a number. */
      goto error;
  set_arg:
    fiobj_hash_set(parsed, arg_name, fiobj_str_new(start, strlen(start)));
    continue;
  error:
    fprintf(stderr, "\n\t*** Argument Error: %s ***\n", start);
    fio_cli_handle_error();
  }
}

/* *****************************************************************************
CLI API
***************************************************************************** */

/** Initialize the CLI helper */
void fio_cli_start(int argc, const char **argv, const char *info) {
  FIO_CLI_ARGV = argv;
  FIO_CLI_ARGC = argc;
  if (info_str)
    fiobj_free(info_str);
  if (info) {
    info_str = fiobj_str_new(info, strlen(info));
  } else {
    info_str = fiobj_str_new(DEFAULT_CLI_INFO, sizeof(DEFAULT_CLI_INFO) - 1);
  }
}

/** Clears the memory and resources used by the CLI helper */
void fio_cli_end(void) {
#define free_and_reset(o)                                                      \
  do {                                                                         \
    fiobj_free((o));                                                           \
    o = FIOBJ_INVALID;                                                         \
  } while (0);

  free_and_reset(arg_aliases);
  free_and_reset(arg_type);
  free_and_reset(help_str);
  free_and_reset(info_str);
  if (parsed)
    free_and_reset(parsed);

#undef free_and_reset

  FIO_CLI_ARGC = 0;
  FIO_CLI_ARGV = NULL;
  is_parsed = 0;
}

/**
 * Sets a CLI acceptable argument of type Number (both `int` and `float`).
 *
 * The `aliases` string sets aliases for the same argument. i.e. "string
 * s".
 *
 * The first alias will be the name available for `fio_cli_get_*`
 * functions.
 *
 * The `desc` string will be printed if `-?`, `-h` of `-help` are used.
 *
 * The function will crash the application on failure, printing an error
 * message.
 */
void fio_cli_accept_num(const char *aliases, const char *desc) {
  fio_cli_set(aliases, desc, CLI_NUM);
}

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
void fio_cli_accept_str(const char *aliases, const char *desc) {
  fio_cli_set(aliases, desc, CLI_STR);
}

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
void fio_cli_accept_bool(const char *aliases, const char *desc) {
  fio_cli_set(aliases, desc, CLI_BOOL);
}

/**
 * Returns a C String containing the value of the received argument, or NULL
 * if none.
 */
const char *fio_cli_get_str(const char *opt) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return NULL;
  FIOBJ result = fiobj_hash_get(parsed, name);
  if (!result)
    return NULL;
  return fiobj_obj2cstr(result).data;
}

/**
 * Returns an Integer containing the parsed value of the argument.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
int fio_cli_get_int(const char *opt) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return 0;
  FIOBJ result = fiobj_hash_get(parsed, name);
  if (!result)
    return 0;
  return (int)fiobj_obj2num(result);
}

/**
 * Returns a Float containing the parsed value of the argument.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
double fio_cli_get_float(const char *opt) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return 0;
  FIOBJ result = fiobj_hash_get(parsed, name);
  if (!result)
    return 0;
  return fiobj_obj2float(result);
}

/**
 * Overrides the existing value of the argument with the requested C String.
 *
 * Boolean that were set to TRUE have the string "1".
 */
void fio_cli_set_str(const char *opt, const char *value) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name) {
    fprintf(stderr, "ERROR: facil.io's CLI helper can only override values for "
                    "valid options\n");
    exit(-1);
  }
  fiobj_hash_set(parsed, name, fiobj_str_new(value, strlen(value)));
}

/**
 * Overrides the existing value of the argument with the requested Integer.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
void fio_cli_set_int(const char *opt, int value) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name) {
    fprintf(stderr, "ERROR: facil.io's CLI helper can only override values for "
                    "valid options\n");
    exit(-1);
  }
  fiobj_hash_set(parsed, name, fiobj_num_new(value));
}

/**
 * Overrides the existing value of the argument with the requested Float.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
void fio_cli_set_float(const char *opt, double value) {
  fio_cli_parse();
  FIOBJ name = fio_cli_get_name(opt, strlen(opt));
  if (!name) {
    fprintf(stderr, "ERROR: facil.io's CLI helper can only override values for "
                    "valid options\n");
    exit(-1);
  }
  fiobj_hash_set(parsed, name, fiobj_float_new(value));
}
