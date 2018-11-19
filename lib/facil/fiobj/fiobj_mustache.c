#define INCLUDE_MUSTACHE_IMPLEMENTATION 1
#include <mustache_parser.h>

#include <fiobj_ary.h>
#include <fiobj_hash.h>
#include <fiobj_mustache.h>
#include <fiobj_str.h>

/**
 * Loads a mustache template, converting it into an opaque instruction array.
 *
 * Returns a pointer to the instruction array.
 *
 * The `folder` argument should contain the template's root folder which would
 * also be used to search for any required partial templates.
 *
 * The `filename` argument should contain the template's file name.
 */
mustache_s *fiobj_mustache_load(fio_str_info_s filename) {
  return mustache_load(.filename = filename.data, .filename_len = filename.len);
}

/** Free the mustache template */
void fiobj_mustache_free(mustache_s *mustache) { mustache_free(mustache); }

/**
 * Renders a template into an existing FIOBJ String (`dest`'s end), using the
 * information in the `data` object.
 *
 * Returns FIOBJ_INVALID if an error occured and a FIOBJ String on success.
 */
FIOBJ fiobj_mustache_build2(FIOBJ dest, mustache_s *mustache, FIOBJ data) {
  mustache_build(mustache, .udata1 = (void *)dest, .udata2 = (void *)data);
  return dest;
}

/**
 * Creates a FIOBJ String containing the rendered template using the information
 * in the `data` object.
 *
 * Returns FIOBJ_INVALID if an error occured and a FIOBJ String on success.
 */
FIOBJ fiobj_mustache_build(mustache_s *mustache, FIOBJ data) {
  if (!mustache)
    return FIOBJ_INVALID;
  return fiobj_mustache_build2(fiobj_str_buf(mustache->u.read_only.data_length),
                               mustache, data);
}

/* *****************************************************************************
Mustache Callbacks
***************************************************************************** */

/** HTML ecape table, created using the following Ruby Script:
a = []
256.times {|i| a[i] = "&\#x#{ i < 16 ? "0#{i.to_s(16)}" : i.to_s(16)};"}
('a'.ord..'z'.ord).each {|i| a[i] = i.chr }
('A'.ord..'Z'.ord).each {|i| a[i] = i.chr }
('0'.ord..'9'.ord).each {|i| a[i] = i.chr }
a['<'.ord] = "&lt;"
a['>'.ord] = "&gt;"
a['&'.ord] = "&amp;"
a['"'.ord] = "&quot;"

b = a.map {|s| s.length }
puts "static char *html_escape_strs[] = {", a.to_s.slice(1..-2) ,"};",
     "static uint8_t html_escape_len[] = {", b.to_s.slice(1..-2),"};"
*/
static const char *html_escape_strs[] = {
    "&#x00;", "&#x01;", "&#x02;", "&#x03;", "&#x04;", "&#x05;", "&#x06;",
    "&#x07;", "&#x08;", "&#x09;", "&#x0a;", "&#x0b;", "&#x0c;", "&#x0d;",
    "&#x0e;", "&#x0f;", "&#x10;", "&#x11;", "&#x12;", "&#x13;", "&#x14;",
    "&#x15;", "&#x16;", "&#x17;", "&#x18;", "&#x19;", "&#x1a;", "&#x1b;",
    "&#x1c;", "&#x1d;", "&#x1e;", "&#x1f;", "&#x20;", "&#x21;", "&quot;",
    "&#x23;", "&#x24;", "&#x25;", "&amp;",  "&#x27;", "&#x28;", "&#x29;",
    "&#x2a;", "&#x2b;", "&#x2c;", "&#x2d;", "&#x2e;", "&#x2f;", "0",
    "1",      "2",      "3",      "4",      "5",      "6",      "7",
    "8",      "9",      "&#x3a;", "&#x3b;", "&lt;",   "&#x3d;", "&gt;",
    "&#x3f;", "&#x40;", "A",      "B",      "C",      "D",      "E",
    "F",      "G",      "H",      "I",      "J",      "K",      "L",
    "M",      "N",      "O",      "P",      "Q",      "R",      "S",
    "T",      "U",      "V",      "W",      "X",      "Y",      "Z",
    "&#x5b;", "&#x5c;", "&#x5d;", "&#x5e;", "&#x5f;", "&#x60;", "a",
    "b",      "c",      "d",      "e",      "f",      "g",      "h",
    "i",      "j",      "k",      "l",      "m",      "n",      "o",
    "p",      "q",      "r",      "s",      "t",      "u",      "v",
    "w",      "x",      "y",      "z",      "&#x7b;", "&#x7c;", "&#x7d;",
    "&#x7e;", "&#x7f;", "&#x80;", "&#x81;", "&#x82;", "&#x83;", "&#x84;",
    "&#x85;", "&#x86;", "&#x87;", "&#x88;", "&#x89;", "&#x8a;", "&#x8b;",
    "&#x8c;", "&#x8d;", "&#x8e;", "&#x8f;", "&#x90;", "&#x91;", "&#x92;",
    "&#x93;", "&#x94;", "&#x95;", "&#x96;", "&#x97;", "&#x98;", "&#x99;",
    "&#x9a;", "&#x9b;", "&#x9c;", "&#x9d;", "&#x9e;", "&#x9f;", "&#xa0;",
    "&#xa1;", "&#xa2;", "&#xa3;", "&#xa4;", "&#xa5;", "&#xa6;", "&#xa7;",
    "&#xa8;", "&#xa9;", "&#xaa;", "&#xab;", "&#xac;", "&#xad;", "&#xae;",
    "&#xaf;", "&#xb0;", "&#xb1;", "&#xb2;", "&#xb3;", "&#xb4;", "&#xb5;",
    "&#xb6;", "&#xb7;", "&#xb8;", "&#xb9;", "&#xba;", "&#xbb;", "&#xbc;",
    "&#xbd;", "&#xbe;", "&#xbf;", "&#xc0;", "&#xc1;", "&#xc2;", "&#xc3;",
    "&#xc4;", "&#xc5;", "&#xc6;", "&#xc7;", "&#xc8;", "&#xc9;", "&#xca;",
    "&#xcb;", "&#xcc;", "&#xcd;", "&#xce;", "&#xcf;", "&#xd0;", "&#xd1;",
    "&#xd2;", "&#xd3;", "&#xd4;", "&#xd5;", "&#xd6;", "&#xd7;", "&#xd8;",
    "&#xd9;", "&#xda;", "&#xdb;", "&#xdc;", "&#xdd;", "&#xde;", "&#xdf;",
    "&#xe0;", "&#xe1;", "&#xe2;", "&#xe3;", "&#xe4;", "&#xe5;", "&#xe6;",
    "&#xe7;", "&#xe8;", "&#xe9;", "&#xea;", "&#xeb;", "&#xec;", "&#xed;",
    "&#xee;", "&#xef;", "&#xf0;", "&#xf1;", "&#xf2;", "&#xf3;", "&#xf4;",
    "&#xf5;", "&#xf6;", "&#xf7;", "&#xf8;", "&#xf9;", "&#xfa;", "&#xfb;",
    "&#xfc;", "&#xfd;", "&#xfe;", "&#xff;"};
static uint8_t html_escape_len[] = {
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 6, 4, 6, 4, 6, 6, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 6, 6, 6, 6,
    6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};

static inline FIOBJ fiobj_mustache_find_obj(mustache_section_s *section,
                                            const char *name,
                                            uint32_t name_len) {
  FIOBJ key = fiobj_str_tmp();
  fiobj_str_write(key, name, name_len);
  FIOBJ o = FIOBJ_INVALID;
  do {
    if (!FIOBJ_TYPE_IS((FIOBJ)section->udata2, FIOBJ_T_HASH))
      continue;
    o = fiobj_hash_get((FIOBJ)section->udata2, key);
    section = section->parent;
  } while (o == FIOBJ_INVALID && section);
  return o;
}
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
                           uint32_t name_len, unsigned char escape) {
  FIOBJ o = fiobj_mustache_find_obj(section, name, name_len);
  if (!o)
    return 0;
  if (!escape || !FIOBJ_TYPE_IS(o, FIOBJ_T_STRING)) {
    fiobj_str_join((FIOBJ)section->udata1, o);
    return 0;
  }
  /* TODO: html escape */
  fio_str_info_s str = fiobj_obj2cstr(o);
  if (!str.len)
    return 0;
  fio_str_info_s i = fiobj_obj2cstr(o);
  i.capa = fiobj_str_capa_assert((FIOBJ)section->udata1, i.len + str.len + 64);
  do {
    if (i.len + 6 >= i.capa)
      i.capa = fiobj_str_capa_assert((FIOBJ)section->udata1, i.capa + 64);
    i.len = fiobj_str_write((FIOBJ)section->udata1,
                            html_escape_strs[(uint8_t)str.data[0]],
                            html_escape_len[(uint8_t)str.data[0]]);
    --str.len;
    ++str.data;
  } while (str.len);
  (void)section;
  (void)name;
  (void)name_len;
  (void)escape;
  return 0;
}

/**
 * Called when simple template text (string) is detected.
 *
 * A conforming implementation will output data as a string (no escaping).
 */
static int mustache_on_text(mustache_section_s *section, const char *data,
                            uint32_t data_len) {
  FIOBJ dest = (FIOBJ)section->udata1;
  fiobj_str_write(dest, data, data_len);
  return 0;
}

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
                                        const char *name, uint32_t name_len) {
  FIOBJ o = fiobj_mustache_find_obj(section, name, name_len);
  if (!o)
    return 0;
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY))
    return fiobj_ary_count(o);
  return 1;
}

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
                                     uint32_t index) {
  FIOBJ o = fiobj_mustache_find_obj(section, name, name_len);
  if (!o)
    return -1;
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY))
    section->udata2 = (void *)fiobj_ary_index(o, index);
  else
    section->udata2 = (void *)o;
  return 0;
}

/**
 * Called for cleanup in case of error.
 */
static void mustache_on_formatting_error(void *udata1, void *udata2) {
  (void)udata1;
  (void)udata2;
}

/* *****************************************************************************
Testing
***************************************************************************** */

#if DEBUG
static inline void mustache_save2file(char const *filename, char const *data,
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

void fiobj_mustache_test(void) {
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }

  char const *template = "{{=<< >>=}}* Users:\r\n<<#users>><<id>>. <<& name>> "
                         "(<<name>>)\r\n<</users>>";
  char const *template_name = "mustache_test_template.mustache";
  mustache_save2file(template_name, template, strlen(template));
  // mustache_error_en err = MUSTACHE_OK;
  mustache_s *m =
      fiobj_mustache_load((fio_str_info_s){.data = (char *)template_name});
  unlink(template_name);
  TEST_ASSERT(m, "fiobj_mustache_load failed.\n");
  FIOBJ data = fiobj_hash_new();
  FIOBJ key = fiobj_str_new("users", 5);
  FIOBJ ary = fiobj_ary_new2(4);
  fiobj_hash_set(data, key, ary);
  fiobj_free(key);
  for (int i = 0; i < 4; ++i) {
    FIOBJ id = fiobj_str_buf(4);
    fiobj_str_write_i(id, i);
    FIOBJ name = fiobj_str_buf(4);
    fiobj_str_write(name, "User ", 5);
    fiobj_str_write_i(name, i);
    FIOBJ usr = fiobj_hash_new2(2);
    key = fiobj_str_new("id", 2);
    fiobj_hash_set(usr, key, id);
    fiobj_free(key);
    key = fiobj_str_new("name", 4);
    fiobj_hash_set(usr, key, name);
    fiobj_free(key);
    fiobj_ary_push(ary, usr);
  }
  key = fiobj_mustache_build(m, data);
  fiobj_free(data);
  TEST_ASSERT(key, "fiobj_mustache_build failed!\n");
  fprintf(stderr, "%s\n", fiobj_obj2cstr(key).data);
  fiobj_free(key);
}

#endif
