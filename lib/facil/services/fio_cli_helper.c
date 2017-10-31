/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio_cli_helper.h"
#include "fiobj.h"

#include <string.h>
/* *****************************************************************************
State (static data)
***************************************************************************** */

/* static variables are automatically initialized to 0, which is what we need.*/
static int ARGC;
static const char **ARGV;
static fiobj_s *arg_aliases; /* a hash for translating aliases */
static fiobj_s *arg_type;    /* a with information about each argument */
static fiobj_s *parsed;      /* a with information about each argument */
static fiobj_s *help_str;    /* The CLI help string */
static fiobj_s *info_str;    /* The CLI information string */
static int is_parsed;

const char DEFAULT_CLI_INFO[] =
    "This application accepts any of the following possible arguments:";
/* *****************************************************************************
Error / Help handling - printing the information and exiting.
***************************************************************************** */

static void fio_cli_handle_error(void) {
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
    parsed = NULL;
  }
  /* avoid overwriting existing data */
  if (arg_aliases)
    return;
  arg_aliases = fiobj_hash_new();
  arg_type = fiobj_hash_new();
  help_str = fiobj_str_buf(1024);
  if (!info_str) /* might exist through `fio_cli_start` */
    info_str = fiobj_str_static(DEFAULT_CLI_INFO, sizeof(DEFAULT_CLI_INFO) - 1);
}

/* *****************************************************************************
Matching arguments to C string
***************************************************************************** */

/* returns the primamry symbol for the argument, of NULL (if none) */
static inline fiobj_s *fio_cli_get_name(const char *str, size_t len) {
  return fiobj_hash_get2(arg_aliases, str, len);
}

/* *****************************************************************************
Setting an argument's type and alias.
***************************************************************************** */
typedef enum { CLI_BOOL, CLI_NUM, CLI_STR } cli_type;
static void fio_cli_set(const char *aliases, const char *desc, cli_type type) {
  fio_cli_init();
  const char *start = aliases;
  size_t len = 0;
  fiobj_s *arg_name = NULL;

  while (1) {
    /* get rid of any white space or commas */
    while (start[0] == ' ' || start[0] == ',')
      start++;
    /* we're done */
    if (!start[0])
      return;
    len = 0;
    /* find the length of the argument name */
    while (start[len] != 0 && start[len] != ' ' && start[len] != ',')
      len++;

    if (!arg_name) {
      /* this is the main identifier */
      arg_name = fiobj_sym_new(start, len);
      /* add to aliases hash */
      fiobj_hash_set(arg_aliases, arg_name, arg_name);
      /* add the help section and set type*/
      switch (type) {
      case CLI_BOOL:
        fiobj_str_write2(help_str, "\t\e[1m-%s\e[0m\t\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_null());
        break;
      case CLI_NUM:
        fiobj_str_write2(help_str, "\t\e[1m-%s\e[0m \e[2###\e[0m\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_true());
        break;
      case CLI_STR:
        fiobj_str_write2(help_str, "\t\e[1m-%s\e[0m \e[2<val>\e[0m\t%s\n",
                         fiobj_obj2cstr(arg_name).data, desc);
        fiobj_hash_set(arg_type, arg_name, fiobj_false());
        break;
      }
    } else {
      /* this is an alias */
      fiobj_s *tmp = fiobj_sym_new(start, len);
      /* add to aliases hash */
      fiobj_hash_set(arg_aliases, tmp, fiobj_dup(arg_name));
      /* add to description + free it*/
      fiobj_str_write2(help_str, "\t\t\e[1m-%s\e[0m\tsame as -%s\n",
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
  if (!ARGC || !ARGV) {
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

  const char *start;
  size_t len;
  fiobj_s *arg_name;

  /* ignore the first element, it's the program's name. */
  for (int i = 1; i < ARGC; i++) {
    /* test for errors or help requests */
    if (ARGV[i][0] != '-' || ARGV[i][1] == 0) {
      start = ARGV[i];
      goto error;
    }
    if ((ARGV[i][1] == '?' && ARGV[i][2] == 0) ||
        (ARGV[i][1] == 'h' &&
         (ARGV[i][2] == 0 || (ARGV[i][2] == 'e' && ARGV[i][3] == 'l' &&
                              ARGV[i][4] == 'p' && ARGV[i][5] == 0)))) {
      fio_cli_handle_error();
    }
    /* we walk the name backwards, so `name` is tested before `n` */
    start = ARGV[i] + 1;
    len = strlen(start);
    while (len && !(arg_name = fio_cli_get_name(start, len))) {
      len--;
    }
    if (!len)
      goto error;
    /* at this point arg_name is a handle to the argument's Symbol */
    fiobj_s *type = fiobj_hash_get(arg_type, arg_name);
    if (type->type == FIOBJ_T_NULL) {
      /* type is BOOL, no further processing required */
      start = "1";
      len = 1;
      goto set_arg;
    }
    if (start[len] == 0) {
      i++;
      if (i == ARGC)
        goto error;
      start = ARGV[i];
    } else if (start[len] == '=') {
      start = start + len + 1;
    } else
      start = start + len;
    len = 0;
    if (type->type == FIOBJ_T_FALSE) /* no restrictions on data  */
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
    fiobj_hash_set(parsed, arg_name, fiobj_str_static(start, len));
    continue;
  error:
    fprintf(stderr, "\n*** Argument Error: %s\n", start);
    fio_cli_handle_error();
  }
}

/* *****************************************************************************
CLI API
***************************************************************************** */

/** Initialize the CLI helper */
void fio_cli_start(int argc, const char **argv, const char *info) {
  ARGV = argv;
  ARGC = argc;
  if (info_str)
    fiobj_free(info_str);
  if (info) {
    info_str = fiobj_str_static(info, 0);
  } else {
    info_str = fiobj_str_static(DEFAULT_CLI_INFO, sizeof(DEFAULT_CLI_INFO) - 1);
  }
}

/** Clears the memory and resources used by the CLI helper */
void fio_cli_end(void) {
#define free_and_reset(o)                                                      \
  do {                                                                         \
    fiobj_free((o));                                                           \
    o = NULL;                                                                  \
  } while (0);

  free_and_reset(arg_aliases);
  free_and_reset(arg_type);
  free_and_reset(help_str);
  free_and_reset(info_str);
  if (parsed)
    free_and_reset(parsed);

#undef free_and_reset

  ARGC = 0;
  ARGV = NULL;
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
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return NULL;
  fiobj_s *result = fiobj_hash_get(parsed, name);
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
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return 0;
  fiobj_s *result = fiobj_hash_get(parsed, name);
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
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
  if (!name)
    return 0;
  fiobj_s *result = fiobj_hash_get(parsed, name);
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
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
  if (!name) {
    fprintf(stderr, "ERROR: facil.io's CLI helper can only override values for "
                    "valid options\n");
    exit(-1);
  }
  fiobj_hash_set(parsed, name, fiobj_str_static(value, strlen(value)));
}

/**
 * Overrides the existing value of the argument with the requested Integer.
 *
 * For boolean values, the value will be 0 for FALSE and 1 for TRUE.
 */
void fio_cli_set_int(const char *opt, int value) {
  fio_cli_parse();
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
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
  fiobj_s *name = fio_cli_get_name(opt, strlen(opt));
  if (!name) {
    fprintf(stderr, "ERROR: facil.io's CLI helper can only override values for "
                    "valid options\n");
    exit(-1);
  }
  fiobj_hash_set(parsed, name, fiobj_float_new(value));
}
