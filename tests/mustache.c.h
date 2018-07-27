#include "mustache_parser.h"

static int mustache_on_arg(mustache_section_s *section, const char *name,
                           uint32_t name_len, unsigned char escape) {
  fprintf(stderr, "Adding %sargument %.*s to section %p\n",
          escape ? "escaped " : "", (int)name_len, name, section->udata);
  return 0;
}

static int mustache_on_text(mustache_section_s *section, const char *data,
                            uint32_t data_len) {
  fprintf(stderr, "Adding text (to section %p): %.*s\n", section->udata,
          (int)data_len, data);
  return 0;
}

static int32_t mustache_on_section_test(mustache_section_s *section,
                                        const char *name, uint32_t name_len) {
  section->udata = (void *)((uintptr_t)section->udata + 1);
  fprintf(stderr, "Staring section %p (%.*s)\n", section->udata, (int)name_len,
          name);
  return 1;
}

static int mustache_on_section_start(mustache_section_s *section,
                                     char const *name, uint32_t name_len,
                                     uint32_t index) {
  fprintf(stderr, "Section %p (%.*s) index == %u\n", section->udata,
          (int)name_len, name, index);
  return 0;
}

static void mustache_on_formatting_error(void *udata, void *udata2) {
  (void)udata;
  (void)udata2;
}

static inline void save2file(char const *filename, char const *data,
                             size_t length) {
  int fd = open(filename, O_CREAT | O_RDWR, 0);
  if (fd == -1) {
    perror("Couldn't open / create file for template testing");
    exit(-1);
  }
  fchmod(fd, 0777);
  if (pwrite(fd, data, length, 0) != (ssize_t)length) {
    perror("Mustache template write error");
    exit(-1);
  }
  close(fd);
}

static inline void mustache_print_instructions(mustache_s *m) {
  mustache__instruction_s *ary = (mustache__instruction_s *)(m + 1);
  for (uint32_t i = 0; i < m->u.read_only.intruction_count; ++i) {
    char *name = NULL;
    switch (ary[i].instruction) {
    case MUSTACHE_WRITE_TEXT:
      name = "MUSTACHE_WRITE_TEXT";
      break;
    case MUSTACHE_WRITE_ARG:
      name = "MUSTACHE_WRITE_ARG";
      break;
    case MUSTACHE_WRITE_ARG_UNESCAPED:
      name = "MUSTACHE_WRITE_ARG_UNESCAPED";
      break;
    case MUSTACHE_SECTION_START:
      name = "MUSTACHE_SECTION_START";
      break;
    case MUSTACHE_SECTION_START_INV:
      name = "MUSTACHE_SECTION_START_INV";
      break;
    case MUSTACHE_SECTION_END:
      name = "MUSTACHE_SECTION_END";
      break;
    case MUSTACHE_SECTION_GOTO:
      name = "MUSTACHE_SECTION_GOTO";
      break;
    default:
      name = "UNKNOWN!!!";
      break;
    }
    fprintf(stderr, "[%u] %s, start: %u, len %u\n", i, name, ary[i].data.start,
            ary[i].data.len);
  }
}

void mustache_test(void) {
  char const *template =
      "Hi there{{#user}}{{name}}{{/user}}{{> mustache_test_partial }}";
  char const *partial = "{{& raw1}}{{{raw2}}}{{^negative}}"
                        "{{> mustache_test_partial }}{{/negative}}";
  char const *template_name = "mustache_test_template.mustache";
  char const *partial_name = "mustache_test_partial.mustache";
  save2file(template_name, template, strlen(template));
  save2file(partial_name, partial, strlen(partial));
  mustache_error_en err = MUSTACHE_OK;
  mustache_s *m = mustache_load(.filename = template_name, .err = &err);
  unlink(template_name);
  unlink(partial_name);

  uint32_t expected[] = {
      MUSTACHE_SECTION_START,       MUSTACHE_WRITE_TEXT,
      MUSTACHE_SECTION_START,       MUSTACHE_WRITE_ARG,
      MUSTACHE_SECTION_END,         MUSTACHE_SECTION_START,
      MUSTACHE_WRITE_ARG_UNESCAPED, MUSTACHE_WRITE_ARG_UNESCAPED,
      MUSTACHE_SECTION_START_INV,   MUSTACHE_SECTION_GOTO,
      MUSTACHE_SECTION_END,         MUSTACHE_SECTION_END,
      MUSTACHE_SECTION_END,
  };

#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }
  TEST_ASSERT(m, "Mustache template loading failed with error %u\n", err);

  fprintf(stderr, "* template loaded, testing template instruction array.\n");
  mustache_print_instructions(m);
  mustache__instruction_s *ary = (mustache__instruction_s *)(m + 1);

  TEST_ASSERT(m->u.read_only.intruction_count == 13,
              "Mustache template instruction count error %u\n",
              m->u.read_only.intruction_count);

  for (uint16_t i = 0; i < 13; ++i) {
    TEST_ASSERT(ary[i].instruction == expected[i],
                "Mustache instraction[%u] error, type %u != %u\n", i,
                ary[0].instruction, expected[i]);
  }
  mustache_build(m, .udata = NULL);
  /* cleanup */
  mustache_free(m);
}
