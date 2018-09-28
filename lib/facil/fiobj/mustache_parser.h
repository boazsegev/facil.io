/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_MUSTACHE_LOADR_H
/**
 * A mustache parser using a callback systems that allows this implementation to
 * be framework agnostic (i.e., can be used with any JSON library).
 *
 * When including the mustache parser within an iumplementation file,
 * `INCLUDE_MUSTACHE_IMPLEMENTATION` must be defined as 1. This allows the
 * header's types to be exposed within a containing header.
 *
 * The API has three functions:
 *
 * 1. `mustache_load` loads a template file, converting it to instruction data.
 * 2. `mustache_build` calls any callbacks according to the loaded instructions.
 * 3. `mustache_free` frees the instruction and data memory (the template).
 *
 * The template is loaded and converted to an instruction array using
 * `mustache_load`. This loads any nested templates / partials as well.
 *
 * The resulting instruction array (`mustache_s *`) is composed of three memory
 * segments: header segment, instruction array segment and data segment.
 *
 * The instruction array (`mustache_s *`) can be used to build actual output
 * data using the `mustache_build` function.
 *
 * The `mustache_build` function accepts two opaque pointers for user data
 * (`udata1` and `udata2`) that can be used by the callbacks for data input and
 * data output.
 *
 * The `mustache_build` function is thread safe and many threads can build
 * content based on the same template.
 *
 * While the build function is performed, the following callback might be
 * called:
 *
 * * `mustache_on_arg` - called to output an argument's value .
 * * `mustache_on_text` - called to output raw text.
 * * `mustache_on_section_test` - called when a section is tested for validity.
 * * `mustache_on_section_start` - called when entering a named section.
 * * `mustache_on_formatting_error` - called when a formatting error occurred.
 *
 * Once the template is no longer needed, it's easy to free the template using
 * the `mustache_free` function (which, at the moment, simply calls `free`).
 *
 * For details about mustache templating scheme, see: https://mustache.github.io
 *
 */
#define H_MUSTACHE_LOADR_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(MUSTACHE_NESTING_LIMIT) || !MUSTACHE_NESTING_LIMIT
#undef MUSTACHE_NESTING_LIMIT
#define MUSTACHE_NESTING_LIMIT 64
#endif

#if !defined(__GNUC__) && !defined(__clang__) && !defined(FIO_GNUC_BYPASS)
#define __attribute__(...)
#define __has_include(...) 0
#define __has_builtin(...) 0
#define FIO_GNUC_BYPASS 1
#elif !defined(__clang__) && __GNUC__ < 5
#define __has_builtin(...) 0
#define FIO_GNUC_BYPASS 1
#endif

#ifndef MUSTACHE_FUNC
#define MUSTACHE_FUNC static __attribute__((unused))
#endif

/* *****************************************************************************
Mustache API Functions and Arguments
***************************************************************************** */

/** an opaque type for mustache template data (when caching). */
typedef struct mustache_s mustache_s;

/** Error reporting (in case of errors). */
typedef enum mustache_error_en {
  MUSTACHE_OK,
  MUSTACHE_ERR_TOO_DEEP,
  MUSTACHE_ERR_CLOSURE_MISMATCH,
  MUSTACHE_ERR_FILE_NOT_FOUND,
  MUSTACHE_ERR_FILE_TOO_BIG,
  MUSTACHE_ERR_FILE_NAME_TOO_LONG,
  MUSTACHE_ERR_EMPTY_TEMPLATE,
  MUSTACHE_ERR_UNKNOWN,
  MUSTACHE_ERR_USER_ERROR,
} mustache_error_en;

/** Arguments for the `mustache_load` function. */
typedef struct {
  /** The root template's file name. */
  char const *filename;
  /** The file name's length. */
  size_t filename_len;
  /** Parsing error reporting (can be NULL). */
  mustache_error_en *err;
} mustache_load_args_s;

/**
 * Allows this header to be included within another header while limiting
 * exposure.
 *
 * before including the header within an implementation faile, define
 * INCLUDE_MUSTACHE_IMPLEMENTATION as 1.
 */
#if INCLUDE_MUSTACHE_IMPLEMENTATION

MUSTACHE_FUNC mustache_s *mustache_load(mustache_load_args_s args);

#define mustache_load(...) mustache_load((mustache_load_args_s){__VA_ARGS__})

/** free the mustache template */
inline MUSTACHE_FUNC void mustache_free(mustache_s *mustache) {
  free(mustache);
}

/** Arguments for the `mustache_build` function. */
typedef struct {
  /** The parsed template (an instruction collection). */
  mustache_s *mustache;
  /** Opaque user data (recommended for input review) - children will inherit
   * the parent's udata value. Updated values will propegate to child sections
   * but won't effect parent sections.
   */
  void *udata1;
  /** Opaque user data (recommended for output handling)- children will inherit
   * the parent's udata value. Updated values will propegate to child sections
   * but won't effect parent sections.
   */
  void *udata2;
  /** Formatting error reporting (can be NULL). */
  mustache_error_en *err;
} mustache_build_args_s;
MUSTACHE_FUNC int mustache_build(mustache_build_args_s args);

#define mustache_build(mustache_s_ptr, ...)                                    \
  mustache_build(                                                              \
      (mustache_build_args_s){.mustache = (mustache_s_ptr), __VA_ARGS__})

/* *****************************************************************************
Client Callbacks - MUST be implemented by the including file
***************************************************************************** */

/**
 * A mustache section allows the callbacks to "walk" backwards towards the root
 * in search of argument data.
 *
 * Note that every section is allowed a separate udata value.
 */
typedef struct mustache_section_s {
  /**
   * READ ONLY. The parent section (when nesting), if any.
   *
   * This is important for accessing the parent's `udata` values when searching
   * for an argument's value.
   *
   * The root's parent is NULL.
   */
  struct mustache_section_s *parent;
  /** Opaque user data (recommended for input review) - children will inherit
   * the parent's udata value. Updated values will propegate to child sections
   * but won't effect parent sections.
   */
  void *udata1;
  /** Opaque user data (recommended for output handling)- children will inherit
   * the parent's udata value. Updated values will propegate to child sections
   * but won't effect parent sections.
   */
  void *udata2;
} mustache_section_s;

/**
 * Called when an argument name was detected in the current section.
 *
 * A conforming implementation will search for the named argument both in the
 * existing section and all of it's parents (walking backwards towards the root)
 * until a value is detected.
 *
 * A missing value should be treated the same as an empty string.
 *
 * A conforming implementation will output the named argument's value (either
 * HTML escaped or not, depending on the `escape` flag) as a string.
 */
static int mustache_on_arg(mustache_section_s *section, const char *name,
                           uint32_t name_len, unsigned char escape);

/**
 * Called when simple template text (string) is detected.
 *
 * A conforming implementation will output data as a string (no escaping).
 */
static int mustache_on_text(mustache_section_s *section, const char *data,
                            uint32_t data_len);

/**
 * Called for nested sections, must return the number of objects in the new
 * subsection (depending on the argument's name).
 *
 * Arrays should return the number of objects in the array.
 *
 * `true` values should return 1.
 *
 * `false` values should return 0.
 *
 * A return value of -1 will stop processing with an error.
 *
 * Please note, this will handle both normal and inverted sections.
 */
static int32_t mustache_on_section_test(mustache_section_s *section,
                                        const char *name, uint32_t name_len);

/**
 * Called when entering a nested section.
 *
 * `index` is a zero based index indicating the number of repetitions that
 * occurred so far (same as the array index for arrays).
 *
 * A return value of -1 will stop processing with an error.
 *
 * Note: this is a good time to update the subsection's `udata` with the value
 * of the array index. The `udata` will always contain the value or the parent's
 * `udata`.
 */
static int mustache_on_section_start(mustache_section_s *section,
                                     char const *name, uint32_t name_len,
                                     uint32_t index);

/**
 * Called for cleanup in case of error.
 */
static void mustache_on_formatting_error(void *udata1, void *udata2);

/* *****************************************************************************

IMPLEMENTATION (beware: monolithic functions ahead)

***************************************************************************** */

/* *****************************************************************************
Internal types
***************************************************************************** */

struct mustache_s {
  /* The number of instructions in the engine */
  union {
    void *read_only_pt; /* ensure pointer wide padding */
    struct {
      uint32_t intruction_count;
      uint32_t data_length;
    } read_only;
  } u;
};

typedef struct mustache__instruction_s {
  enum {
    MUSTACHE_WRITE_TEXT,
    MUSTACHE_WRITE_ARG,
    MUSTACHE_WRITE_ARG_UNESCAPED,
    MUSTACHE_SECTION_START,
    MUSTACHE_SECTION_START_INV,
    MUSTACHE_SECTION_END,
    MUSTACHE_SECTION_GOTO,
  } instruction;
  /** the data the instruction acts upon */
  struct {
    /** The offset from the beginning of the data segment. */
    uint32_t start;
    /** The length of the data. */
    uint32_t len;
  } data;
} mustache__instruction_s;

/* *****************************************************************************
Calling the instrustion list (using the template engine)
***************************************************************************** */

/*
 * This function reviews the instructions listed at the end of the mustache_s
 * and performs any callbacks necessary.
 *
 * The `mustache_s` data is looks like this:
 *
 *  - header (the `mustache_s` struct): lists the length of the instruction
 *    array and data segments.
 *  - Instruction array: lists all the instructions extracted from the
 *    template(s) (an array of `mustache__instruction_s`).
 *  - Data segment: text and data related to the instructions.
 *
 * The instructions, much like machine code, might loop or jump. This is why the
 * functions keep a stack of sorts. This allows the code to avoid recursion and
 * minimize any risk of stack overflow caused by recursive templates.
 *
 * Note:
 *
 * For text and argument instructions, the mustache__instruction_s.data.start
 * and mustache__instruction_s.data.len mark the beginning of the text/argument
 * and it's name.
 *
 * However, for MUSTACHE_SECTION_START instructions, data.len marks position for
 * the complementing MUSTACHE_SECTION_END instruction, allowing for easy jumps
 * in cases where a section is skipped or in cases of a recursive template.
 */
MUSTACHE_FUNC int(mustache_build)(mustache_build_args_s args) {
  /* extract the instruction array and data segment from the mustache_s */
  mustache__instruction_s *pos =
      (mustache__instruction_s *)(sizeof(*args.mustache) +
                                  (uintptr_t)args.mustache);
  mustache__instruction_s *const start = pos;
  mustache__instruction_s *const end =
      pos + args.mustache->u.read_only.intruction_count;
  char *const data = (char *const)end;

  /* prepare a pre-allocated stack space to flatten recursion needs */
  struct {
    mustache_section_s sec; /* client visible section data */
    uint32_t start;         /* section start instruction position */
    uint32_t end;           /* instruction to jump to after completion */
    uint32_t index;         /* zero based index forr section loops */
    uint32_t count; /* the number of times the section should be performed */
  } section_stack[MUSTACHE_NESTING_LIMIT];

  /* first section (section 0) data */
  section_stack[0].sec = (mustache_section_s){
      .udata1 = args.udata1,
      .udata2 = args.udata2,
  };
  section_stack[0].end = 0;
  uint32_t nesting_pos = 0;

  /* run through the instruction list and persorm each instruction */
  while (pos < end) {
    switch (pos->instruction) {

    case MUSTACHE_WRITE_TEXT:
      if (mustache_on_text(&section_stack[nesting_pos].sec,
                           data + pos->data.start, pos->data.len) == -1) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_USER_ERROR;
        }
        goto error;
      }
      break;

    case MUSTACHE_WRITE_ARG:
      if (mustache_on_arg(&section_stack[nesting_pos].sec,
                          data + pos->data.start, pos->data.len, 1) == -1) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_USER_ERROR;
        }
        goto error;
      }
      break;

    case MUSTACHE_WRITE_ARG_UNESCAPED:
      if (mustache_on_arg(&section_stack[nesting_pos].sec,
                          data + pos->data.start, pos->data.len, 0) == -1) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_USER_ERROR;
        }
        goto error;
      }
      break;
    case MUSTACHE_SECTION_START_INV: /* overfloaw*/
    case MUSTACHE_SECTION_START: {
      /* starting a new section, increased nesting & review */
      if (nesting_pos + 1 == MUSTACHE_NESTING_LIMIT) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_TOO_DEEP;
        }
        goto error;
      }
      section_stack[nesting_pos + 1].sec = section_stack[nesting_pos].sec;
      ++nesting_pos;

      /* find the end of the section */
      mustache__instruction_s *section_end = start + pos->data.len;

      /* test for template (partial) section (nameless) */
      if (pos->data.start == 0) {
        section_stack[nesting_pos].end = section_end - start;
        section_stack[nesting_pos].start = pos - start;
        section_stack[nesting_pos].index = 1;
        section_stack[nesting_pos].count = 0;
        break;
      }

      /* test for user abort signal and cycle value */
      int32_t val = mustache_on_section_test(&section_stack[nesting_pos].sec,
                                             data + pos->data.start,
                                             strlen(data + pos->data.start));
      if (val == -1) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_USER_ERROR;
        }
        goto error;
      }
      if (pos->instruction == MUSTACHE_SECTION_START_INV) {
        if (val == 0) {
          /* perform once for inverted sections */
          val = 1;
        } else {
          /* or don't perform */
          val = 0;
        }
      }

      if (val == 0) {
        --nesting_pos;
        pos = section_end;
      } else {
        /* save start/end positions and index counter */
        section_stack[nesting_pos].end = section_end - start;
        section_stack[nesting_pos].start = pos - start;
        section_stack[nesting_pos].index = val;
        section_stack[nesting_pos].count = 0;
        if (mustache_on_section_start(&section_stack[nesting_pos].sec,
                                      data + pos->data.start,
                                      strlen(data + pos->data.start),
                                      section_stack[nesting_pos].count) == -1) {
          if (args.err) {
            *args.err = MUSTACHE_ERR_USER_ERROR;
          }
          goto error;
        }
      }
      break;
    }

    case MUSTACHE_SECTION_END:
      ++section_stack[nesting_pos].count;
      if (section_stack[nesting_pos].index > section_stack[nesting_pos].count) {
        pos = start + section_stack[nesting_pos].start;
        if (nesting_pos) { /* revert to old udata values */
          section_stack[nesting_pos].sec = section_stack[nesting_pos - 1].sec;
        }
        if (mustache_on_section_start(&section_stack[nesting_pos].sec,
                                      data + pos->data.start,
                                      strlen(data + pos->data.start),
                                      section_stack[nesting_pos].count)) {
          if (args.err) {
            *args.err = MUSTACHE_ERR_USER_ERROR;
          }
          goto error;
        }
      } else {
        pos = start + section_stack[nesting_pos].end; /* in case of recursion */
        --nesting_pos;
      }
      break;

    case MUSTACHE_SECTION_GOTO: {
      /* used to handle recursive sections and re-occuring partials. */
      if (nesting_pos + 1 == MUSTACHE_NESTING_LIMIT) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_TOO_DEEP;
        }
        goto error;
      }
      section_stack[nesting_pos + 1].sec = section_stack[nesting_pos].sec;
      ++nesting_pos;
      if (start[pos->data.len].data.start == 0) {
        section_stack[nesting_pos].end = pos - start;
        section_stack[nesting_pos].index = 1;
        section_stack[nesting_pos].count = 0;
        section_stack[nesting_pos].start = pos->data.len;
        pos = start + pos->data.len;
        break;
      }
      int32_t val = mustache_on_section_test(
          &section_stack[nesting_pos].sec,
          data + start[pos->data.len].data.start,
          strlen(data + start[pos->data.len].data.start));
      if (val == -1) {
        if (args.err) {
          *args.err = MUSTACHE_ERR_USER_ERROR;
        }
        goto error;
      }
      if (val == 0) {
        --nesting_pos;
      } else {
        /* save start/end positions and index counter */
        section_stack[nesting_pos].end = pos - start;
        section_stack[nesting_pos].index = val;
        section_stack[nesting_pos].count = 0;
        section_stack[nesting_pos].start = pos->data.len;
        pos = start + pos->data.len;
        if (mustache_on_section_start(&section_stack[nesting_pos].sec,
                                      data + pos->data.start,
                                      strlen(data + pos->data.start),
                                      section_stack[nesting_pos].count) == -1) {
          if (args.err) {
            *args.err = MUSTACHE_ERR_USER_ERROR;
          }
          goto error;
        }
      }
    } break;
    default:
      /* not a valid engine */
      fprintf(stderr, "ERROR: invalid mustache instruction set detected (wrong "
                      "`mustache_s`?)\n");
      if (args.err) {
        *args.err = MUSTACHE_ERR_UNKNOWN;
      }
      goto error;
    }
    ++pos;
  }

  return 0;
error:
  mustache_on_formatting_error(args.udata1, args.udata2);
  return -1;
}

/* *****************************************************************************
Building the instrustion list (parsing the template)
***************************************************************************** */

/* The parsing implementation, converts a template to an instruction array */
MUSTACHE_FUNC mustache_s *(mustache_load)(mustache_load_args_s args) {
  /* Make sure the args string length is set and prepare the path name */
  char *path = NULL;
  uint32_t path_capa = 0;
  uint32_t path_len = 0;
  if (args.filename && !args.filename_len) {
    args.filename_len = strlen(args.filename);
  }

  /* copy the path data (and resolve) into writable memory  */
  if (args.filename[0] == '~' && args.filename[1] == '/' && getenv("HOME")) {
    const char *home = getenv("HOME");
    path_len = strlen(home);
    path_capa =
        path_len + 1 + args.filename_len + 1 + 9 + 1; /* + file extension */
    path = malloc(path_capa);
    if (!path) {
      perror("FATAL ERROR: couldn't allocate memory for path resolution");
      exit(errno);
    }
    memcpy(path, home, path_len);
    if (path[path_len - 1] != '/') {
      path[path_len++] = '/';
    }
    memcpy(path + path_len, args.filename + 2, args.filename_len);
    args.filename_len += path_len;
    args.filename = path;
  }
  /* divide faile name from the root path to the file */

  /*
   * We need a dynamic array to hold the list of instructions...
   * We might as well use the same memory structure as the final product, saving
   * us an allocation and a copy at the end.
   *
   * Allocation starts with 32 instructions.
   */
  struct {
    mustache_s head;               /* instruction array capacity and length */
    mustache__instruction_s ary[]; /* the instruction array */
  } *instructions =
      malloc(sizeof(*instructions) + (32 * sizeof(mustache__instruction_s)));
  if (!instructions) {
    perror(
        "FATAL ERROR: couldn't allocate memory for mustache template parsing");
    exit(errno);
  }
  /* initialize dynamic array */
  instructions->head.u.read_only.intruction_count = 0;
  instructions->head.u.read_only.data_length = 32;
  uint32_t data_len = 0;
  uint8_t *data = NULL;

/* We define a dynamic array handling macro, using 32 instruction chunks  */
#define PUSH_INSTRUCTION(...)                                                  \
  do {                                                                         \
    if (instructions->head.u.read_only.intruction_count ==                     \
        instructions->head.u.read_only.data_length) {                          \
      instructions->head.u.read_only.data_length += 32;                        \
      instructions = realloc(instructions,                                     \
                             sizeof(*instructions) +                           \
                                 (instructions->head.u.read_only.data_length * \
                                  sizeof(mustache__instruction_s)));           \
      if (!instructions) {                                                     \
        perror("FATAL ERROR: couldn't reallocate memory for mustache "         \
               "template path");                                               \
        exit(errno);                                                           \
      }                                                                        \
    }                                                                          \
    instructions->ary[instructions->head.u.read_only.intruction_count++] =     \
        (mustache__instruction_s){__VA_ARGS__};                                \
  } while (0);

  /* a limited local template stack to manage template data "jumps" */
  /* Note: templates can be recursive. */
  int32_t stack_pos = 0;
  struct {
    uint8_t *delimiter_start; /* currunt instruction start delimiter */
    uint8_t *delimiter_end;   /* currunt instruction end delimiter */
    uint32_t data_start;      /* template starting position (with header) */
    uint32_t data_pos;      /* data reading position (how much was consumed) */
    uint32_t data_end;      /* data ending position (for this template) */
    uint16_t del_start_len; /* delimiter length (start) */
    uint16_t del_end_len;   /* delimiter length (end) */
  } template_stack[MUSTACHE_NESTING_LIMIT];
  template_stack[0].data_start = 0;
  template_stack[0].data_pos = 0;
  template_stack[0].data_end = 0;
  template_stack[0].delimiter_start = (uint8_t *)"{{";
  template_stack[0].delimiter_end = (uint8_t *)"}}";
  template_stack[0].del_start_len = 2;
  template_stack[0].del_end_len = 2;

  /* a section data stack, allowing us to safely mark section closures */
  int32_t section_depth = 0;
  struct {
    /* section name, for closure validation */
    struct {
      uint32_t start;
      uint32_t len;
    } name;
    /* position for the section start instruction */
    uint32_t instruction_pos;
  } section_stack[MUSTACHE_NESTING_LIMIT];

#define SECTION2FILENAME() (data + template_stack[stack_pos].data_start + 10)
#define SECTION2FLEN()                                                         \
  ((((uint8_t *)data + template_stack[stack_pos].data_start + 4)[0] << 1) |    \
   (((uint8_t *)data + template_stack[stack_pos].data_start + 4)[1]))

  /* append a filename to the path, managing the C string memory and length */
#define PATH2FULL(folder, folder_len, filename, filename_len)                  \
  do {                                                                         \
    if (path_capa < (filename_len) + (folder_len) + 9 + 1) {                   \
      path_capa = (filename_len) + (folder_len) + 9 + 1;                       \
      path = realloc(path, path_capa);                                         \
      if (!path) {                                                             \
        perror("FATAL ERROR: couldn't allocate memory for path resolution");   \
        exit(errno);                                                           \
      }                                                                        \
    }                                                                          \
    if (path != (char *)(folder) && (folder_len) && (filename)[0] != '/') {    \
      memcpy(path, (folder), (folder_len));                                    \
      path_len = (folder_len);                                                 \
    } else {                                                                   \
      path_len = 0;                                                            \
    }                                                                          \
    if (path != (char *)(filename))                                            \
      memcpy(path + path_len, (filename), (filename_len));                     \
    path_len += (filename_len);                                                \
    path[path_len] = 0;                                                        \
  } while (0);

  /* append a filename to the path, managing the C string memory and length */
#define PATH_WITH_EXT()                                                        \
  do {                                                                         \
    memcpy(path + path_len, ".mustache", 9);                                   \
    path[path_len + 9] = 0; /* keep path_len the same */                       \
  } while (0);

/* We define a dynamic template loading macro to manage memory details */
#define LOAD_TEMPLATE(root, root_len, filename, filname_len)                   \
  do {                                                                         \
    /* find root filename's path start */                                      \
    int32_t root_len_tmp = (root_len);                                         \
    while (root_len_tmp && (((char *)(root))[root_len_tmp - 1] != '/' ||       \
                            (root_len_tmp > 1 &&                               \
                             ((char *)(root))[root_len_tmp - 2] == '\\'))) {   \
      --root_len_tmp;                                                          \
    }                                                                          \
    if ((filname_len) + root_len_tmp >= ((uint32_t)1 << 16)) {                 \
      *args.err = MUSTACHE_ERR_FILE_NAME_TOO_LONG;                             \
      goto error;                                                              \
    }                                                                          \
    PATH2FULL((root), root_len_tmp, (filename), (filname_len));                \
    struct stat f_data;                                                        \
    {                                                                          \
      /* test file name with and without the .mustache extension */            \
      int stat_result = stat(path, &f_data);                                   \
      if (stat_result == -1) {                                                 \
        PATH_WITH_EXT();                                                       \
        stat_result = stat(path, &f_data);                                     \
      }                                                                        \
      if (stat_result == -1) {                                                 \
        if (args.err) {                                                        \
          *args.err = MUSTACHE_ERR_FILE_NOT_FOUND;                             \
        }                                                                      \
        goto error;                                                            \
      }                                                                        \
    }                                                                          \
    if (f_data.st_size >= ((uint32_t)1 << 24)) {                               \
      *args.err = MUSTACHE_ERR_FILE_TOO_BIG;                                   \
      goto error;                                                              \
    }                                                                          \
    /* the data segment's new length after loading the the template */         \
    /* The data segments includes a template header: */                        \
    /*  | 4 bytes template start instruction position | */                     \
    /*  | 2 bytes template name | 4 bytes next template position | */          \
    /*  | template name (filename) | ...[template data]... */                  \
    /* this allows template data to be reused when repeating a template */     \
    const uint32_t new_len =                                                   \
        data_len + 4 + 2 + 4 + path_len + f_data.st_size + 1;                  \
    /* reallocate memory */                                                    \
    data = realloc(data, new_len);                                             \
    if (!data) {                                                               \
      perror("FATAL ERROR: couldn't reallocate memory for mustache "           \
             "data segment");                                                  \
      exit(errno);                                                             \
    }                                                                          \
    /* save instruction position length into template header */                \
    data[data_len + 0] =                                                       \
        (instructions->head.u.read_only.intruction_count >> 3) & 0xFF;         \
    data[data_len + 1] =                                                       \
        (instructions->head.u.read_only.intruction_count >> 2) & 0xFF;         \
    data[data_len + 2] =                                                       \
        (instructions->head.u.read_only.intruction_count >> 1) & 0xFF;         \
    data[data_len + 3] =                                                       \
        (instructions->head.u.read_only.intruction_count) & 0xFF;              \
    /* Add section start marker (to support recursion or repeated partials) */ \
    PUSH_INSTRUCTION(.instruction = MUSTACHE_SECTION_START);                   \
    /* save filename length */                                                 \
    data[data_len + 4 + 0] = (path_len >> 1) & 0xFF;                           \
    data[data_len + 4 + 1] = path_len & 0xFF;                                  \
    /* save data length ("next" pointer) */                                    \
    data[data_len + 4 + 2 + 0] = ((uint32_t)new_len >> 3) & 0xFF;              \
    data[data_len + 4 + 2 + 1] = ((uint32_t)new_len >> 2) & 0xFF;              \
    data[data_len + 4 + 2 + 2] = ((uint32_t)new_len >> 1) & 0xFF;              \
    data[data_len + 4 + 2 + 3] = ((uint32_t)new_len) & 0xFF;                   \
    /* copy filename */                                                        \
    memcpy(data + data_len + 4 + 2 + 4, path, path_len);                       \
    /* open file and dump it into the data segment after the new header */     \
    int fd = open(path, O_RDONLY);                                             \
    if (fd == -1) {                                                            \
      if (args.err) {                                                          \
        *args.err = MUSTACHE_ERR_FILE_NOT_FOUND;                               \
      }                                                                        \
      goto error;                                                              \
    }                                                                          \
    if (pread(fd, (data + data_len + 4 + 3 + 3 + path_len), f_data.st_size,    \
              0) != (ssize_t)f_data.st_size) {                                 \
      if (args.err) {                                                          \
        *args.err = MUSTACHE_ERR_FILE_NOT_FOUND;                               \
      }                                                                        \
      close(fd);                                                               \
      goto error;                                                              \
    }                                                                          \
    if (stack_pos + 1 == MUSTACHE_NESTING_LIMIT) {                             \
      if (args.err) {                                                          \
        *args.err = MUSTACHE_ERR_TOO_DEEP;                                     \
      }                                                                        \
      close(fd);                                                               \
      goto error;                                                              \
    }                                                                          \
    close(fd);                                                                 \
    /* increase the data stack pointer and setup new stack frame */            \
    ++stack_pos;                                                               \
    template_stack[stack_pos].data_start = data_len;                           \
    template_stack[stack_pos].data_pos = data_len + 4 + 3 + 3 + path_len;      \
    template_stack[stack_pos].data_end = new_len - 1;                          \
    template_stack[stack_pos].delimiter_start = (uint8_t *)"{{";               \
    template_stack[stack_pos].delimiter_end = (uint8_t *)"}}";                 \
    template_stack[stack_pos].del_start_len = 2;                               \
    template_stack[stack_pos].del_end_len = 2;                                 \
    /* update new data segment length and add NUL marker */                    \
    data_len = new_len;                                                        \
    data[new_len - 1] = 0;                                                     \
  } while (0);

#define IGNORE_WHITESPACE(str, step)                                           \
  while (isspace(*(str))) {                                                    \
    (str) += (step);                                                           \
  }

  /* Our first template to load is the root template */
  LOAD_TEMPLATE(path, 0, args.filename, args.filename_len);

  /*** As long as the stack has templated to parse - parse the template ***/
  while (stack_pos) {
    /* test reading position against template ending and parse */
    while (template_stack[stack_pos].data_pos <
           template_stack[stack_pos].data_end) {
      /* start parsing at current position */
      uint8_t *const start = data + template_stack[stack_pos].data_pos;
      /* find the next instruction (beg == beginning) */
      uint8_t *beg = (uint8_t *)strstr(
          (char *)start, (char *)template_stack[stack_pos].delimiter_start);
      if (!beg || beg >= data + template_stack[stack_pos].data_end) {
        /* no instructions left, only text */
        PUSH_INSTRUCTION(.instruction = MUSTACHE_WRITE_TEXT,
                         .data = {
                             .start = template_stack[stack_pos].data_pos,
                             .len = template_stack[stack_pos].data_end -
                                    template_stack[stack_pos].data_pos,
                         });
        template_stack[stack_pos].data_pos = template_stack[stack_pos].data_end;
        continue;
      }
      if (beg + template_stack[stack_pos].del_start_len >=
          data + template_stack[stack_pos].data_end) {
        /* overshot... ending the template with a delimiter...*/
        if (args.err) {
          *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
        }
        goto error;
      }
      beg[0] = 0; /* mark the end of any text segment or string, just in case */
      /* find the ending of the instruction */
      uint8_t *end = (uint8_t *)strstr(
          (char *)beg + template_stack[stack_pos].del_start_len,
          (char *)template_stack[stack_pos].delimiter_end);
      if (!end || end >= data + template_stack[stack_pos].data_end) {
        /* delimiter not closed */
        if (args.err) {
          *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
        }
        goto error;
      }
      /* text before instruction? add text instruction */
      if (beg != data + template_stack[stack_pos].data_pos) {
        PUSH_INSTRUCTION(.instruction = MUSTACHE_WRITE_TEXT,
                         .data = {
                             .start = template_stack[stack_pos].data_pos,
                             .len = beg -
                                    (data + template_stack[stack_pos].data_pos),
                         });
      }
      /* update reading position in the stack */
      template_stack[stack_pos].data_pos =
          (end - data) + template_stack[stack_pos].del_end_len;

      /* move the beginning marker the the instruction's content */
      beg += template_stack[stack_pos].del_start_len;

      /* review template instruction (the {{tag}}) */
      uint8_t escape_str = 1;
      switch (beg[0]) {
      case '!':
        /* comment, do nothing */
        break;

      case '=':
        /* define new seperators */
        ++beg;
        --end;
        if (end[0] != '=') {
          if (args.err) {
            *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
          }
          goto error;
        }
        {
          uint8_t *div = beg;
          while (div < end && !isspace(*(char *)div)) {
            ++div;
          }
          if (div == end) {
            if (args.err) {
              *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
            }
            goto error;
          }
          template_stack[stack_pos].delimiter_start = beg;
          template_stack[stack_pos].del_start_len = div - beg;
          div[0] = 0;
          ++div;
          IGNORE_WHITESPACE(div, 1);
          template_stack[stack_pos].delimiter_end = div;
          template_stack[stack_pos].del_end_len = end - div;
          end[0] = 0;
        }
        break;

      case '^': /*overflow*/
        escape_str = 0;
      case '#':
        /* start section (or inverted section) */
        ++beg;
        --end;
        IGNORE_WHITESPACE(beg, 1);
        IGNORE_WHITESPACE(end, -1);
        end[1] = 0;
        if (section_depth >= MUSTACHE_NESTING_LIMIT) {
          if (args.err) {
            *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
          }
          goto error;
        }
        section_stack[section_depth].instruction_pos =
            instructions->head.u.read_only.intruction_count;
        section_stack[section_depth].name.start = beg - data;
        section_stack[section_depth].name.len = (end - beg) + 1;
        ++section_depth;
        PUSH_INSTRUCTION(.instruction =
                             (escape_str ? MUSTACHE_SECTION_START
                                         : MUSTACHE_SECTION_START_INV),
                         .data = {
                             .start = beg - data,
                             .len = (end - beg) + 1,
                         });
        break;

      case '>':
        /* partial template - search data for loaded template or load new */
        ++beg;
        --end;
        IGNORE_WHITESPACE(beg, 1);
        IGNORE_WHITESPACE(end, -1);
        ++end;
        end[0] = 0;
        {
          uint8_t *loaded = data;
          uint8_t *const data_end = data + data_len;
          while (loaded < data_end) {
            uint32_t const fn_len =
                ((loaded[4] & 0xFF) << 1) | (loaded[5] & 0xFF);
            if (fn_len != end - beg || memcmp(beg, loaded + 10, end - beg)) {
              uint32_t const next_offset =
                  ((loaded[6] & 0xFF) << 3) | ((loaded[7] & 0xFF) << 2) |
                  ((loaded[8] & 0xFF) << 1) | (loaded[9] & 0xFF);
              loaded = data + next_offset;
              continue;
            }
            uint32_t const section_start =
                ((loaded[0] & 0xFF) << 3) | ((loaded[1] & 0xFF) << 2) |
                ((loaded[2] & 0xFF) << 1) | (loaded[3] & 0xFF);
            PUSH_INSTRUCTION(.instruction = MUSTACHE_SECTION_GOTO,
                             .data = {
                                 .len = section_start,
                             });
            break;
          }
          if (loaded >= data_end) {
            LOAD_TEMPLATE(SECTION2FILENAME(), SECTION2FLEN(), beg, (end - beg));
          }
        }
        break;

      case '/':
        /* section end */
        ++beg;
        --end;
        IGNORE_WHITESPACE(beg, 1);
        IGNORE_WHITESPACE(end, -1);
        end[1] = 0;
        --section_depth;
        if (!(section_depth + 1) ||
            (end - beg) + 1 != section_stack[section_depth].name.len ||
            memcmp(beg, data + section_stack[section_depth].name.start,
                   section_stack[section_depth].name.len)) {
          if (args.err) {
            *args.err = MUSTACHE_ERR_CLOSURE_MISMATCH;
          }
          goto error;
        }
        /* update the section_start instruction with the ending's location */
        instructions->ary[section_stack[section_depth].instruction_pos]
            .data.len = instructions->head.u.read_only.intruction_count;
        /* push sction end instruction */
        PUSH_INSTRUCTION(.instruction = MUSTACHE_SECTION_END,
                         .data = {
                             .len =
                                 section_stack[section_depth].instruction_pos,
                         });
        break;

      case '{':
        /* step the read position forward if the ending was '}}}' */
        if ((data + template_stack[stack_pos].data_pos)[0] == '}') {
          ++template_stack[stack_pos].data_pos;
        }
        /*overflow*/
      case '&': /*overflow*/
        /* unescaped variable data */
        escape_str = 0;
        /* overflow to default */
      case ':': /*overflow*/
      case '<': /*overflow*/
        ++beg;  /*overflow*/
      default:
        --end;
        IGNORE_WHITESPACE(beg, 1);
        IGNORE_WHITESPACE(end, -1);
        end[1] = 0;
        PUSH_INSTRUCTION(.instruction =
                             (escape_str ? MUSTACHE_WRITE_ARG
                                         : MUSTACHE_WRITE_ARG_UNESCAPED),
                         .data = {
                             .start = beg - data,
                             .len = (end - beg) + 1,
                         });
        break;
      }
    }
    /* templates are treated as sections, allowing for recursion using "goto" */
    /* update the template's section_start instruction with the end position */
    uint32_t const section_start =
        ((data[template_stack[stack_pos].data_start + 0] & 0xFF) << 3) |
        ((data[template_stack[stack_pos].data_start + 1] & 0xFF) << 2) |
        ((data[template_stack[stack_pos].data_start + 2] & 0xFF) << 1) |
        (data[template_stack[stack_pos].data_start + 3] & 0xFF);
    instructions->ary[section_start].data.len =
        instructions->head.u.read_only.intruction_count;
    /* add section end instructiomn for the template section */
    PUSH_INSTRUCTION(.instruction = MUSTACHE_SECTION_END,
                     .data.len = section_start);
    /* pop the stack frame */
    --stack_pos;
  }
  /*** done parsing ***/

  /* is the template empty?*/
  if (!instructions->head.u.read_only.intruction_count) {
    if (args.err) {
      *args.err = MUSTACHE_ERR_EMPTY_TEMPLATE;
    }
    goto error;
  }

  /* We're done making up the instruction list, time to finalize the product */
  /* Make room for the String data at the end of the instruction array */
  instructions = realloc(instructions,
                         sizeof(*instructions) +
                             (instructions->head.u.read_only.intruction_count *
                              sizeof(mustache__instruction_s)) +
                             data_len);
  if (!instructions) {
    perror("FATAL ERROR: couldn't reallocate memory for mustache "
           "template finalization");
    exit(errno);
  }
  /* Copy the data segment to the end of the instruction array */
  instructions->head.u.read_only.data_length = data_len;
  memcpy((void *)((uintptr_t)(instructions + 1) +
                  (instructions->head.u.read_only.intruction_count *
                   sizeof(mustache__instruction_s))),
         data, data_len);
  /* Cleanup, set error code and return. */
  free(data);
  free(path);
  if (args.err) {
    *args.err = MUSTACHE_OK;
  }
  return &instructions->head;
error:
  free(instructions);
  free(data);
  free(path);
  return NULL;

#undef PATH2FULL
#undef PATH_WITH_EXT
#undef SECTION2FILENAME
#undef SECTION2FLEN
#undef LOAD_TEMPLATE
#undef PUSH_INSTRUCTION
#undef IGNORE_WHITESPACE
}

#endif /* INCLUDE_MUSTACHE_IMPLEMENTATION */

#undef MUSTACHE_FUNC
#endif /* H_MUSTACHE_LOADR_H */
