#define FIO_JSON
#define FIO_STR_NAME fio_str
#define FIO_LOG
#include "fio-stl.h"

#define FIO_CLI
#include "fio-stl.h"

typedef struct {
  fio_json_parser_s p;
  fio_str_s out;
  uint8_t counter;
  uint8_t done;
} my_json_parser_s;

#define JSON_PARSER_CAST(ptr) FIO_PTR_FROM_FIELD(my_json_parser_s, p, ptr)
#define JSON_PARSER2OUTPUT(p) (&JSON_PARSER_CAST(p)->out)

FIO_IFUNC void my_json_write_seperator(fio_json_parser_s *p) {
  my_json_parser_s *j = JSON_PARSER_CAST(p);
  if (j->counter) {
    switch (fio_json_parser_is_in_object(p)) {
    case 0: /* array */
      if (fio_json_parser_is_in_array(p))
        fio_str_write(&j->out, ",", 1);
      break;
    case 1: /* object */
      // note the reverse `if` statement due to operation ordering
      fio_str_write(&j->out, (fio_json_parser_is_key(p) ? "," : ":"), 1);
      break;
    }
  }
  j->counter |= 1;
}

/** a NULL object was detected */
FIO_JSON_CB void fio_json_on_null(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "null", 4);
}
/** a TRUE object was detected */
static inline void fio_json_on_true(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "true", 4);
}
/** a FALSE object was detected */
FIO_JSON_CB void fio_json_on_false(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "false", 4);
}
/** a Number was detected (long long). */
FIO_JSON_CB void fio_json_on_number(fio_json_parser_s *p, long long i) {
  my_json_write_seperator(p);
  fio_str_write_i(JSON_PARSER2OUTPUT(p), i);
}
/** a Float was detected (double). */
FIO_JSON_CB void fio_json_on_float(fio_json_parser_s *p, double f) {
  my_json_write_seperator(p);
  char buffer[256];
  size_t len = fio_ftoa(buffer, f, 10);
  fio_str_write(JSON_PARSER2OUTPUT(p), buffer, len);
}
/** a String was detected (int / float). update `pos` to point at ending */
FIO_JSON_CB void
fio_json_on_string(fio_json_parser_s *p, const void *start, size_t len) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "\"", 1);
  fio_str_write(JSON_PARSER2OUTPUT(p), start, len);
  fio_str_write(JSON_PARSER2OUTPUT(p), "\"", 1);
}
/** a dictionary object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_object(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "{", 1);
  JSON_PARSER_CAST(p)->counter = 0;
  return 0;
}
/** a dictionary object closure detected */
FIO_JSON_CB void fio_json_on_end_object(fio_json_parser_s *p) {
  fio_str_write(JSON_PARSER2OUTPUT(p), "}", 1);
  JSON_PARSER_CAST(p)->counter = 1;
}
/** an array object was detected, should return 0 unless error occurred. */
FIO_JSON_CB int fio_json_on_start_array(fio_json_parser_s *p) {
  my_json_write_seperator(p);
  fio_str_write(JSON_PARSER2OUTPUT(p), "[", 1);
  JSON_PARSER_CAST(p)->counter = 0;
  return 0;
}
/** an array closure was detected */
FIO_JSON_CB void fio_json_on_end_array(fio_json_parser_s *p) {
  fio_str_write(JSON_PARSER2OUTPUT(p), "]", 1);
  JSON_PARSER_CAST(p)->counter = 1;
}
/** the JSON parsing is complete */
FIO_JSON_CB void fio_json_on_json(fio_json_parser_s *p) {
  JSON_PARSER_CAST(p)->done = 1;
  (void)p;
}
/** the JSON parsing encountered an error */
FIO_JSON_CB void fio_json_on_error(fio_json_parser_s *p) {
  fio_str_write(
      JSON_PARSER2OUTPUT(p), "--- ERROR, invalid JSON after this point.\0", 42);
}

void run_my_json_minifier(char *json, size_t len) {
  my_json_parser_s p = {{0}};
  fio_json_parse(&p.p, json, len);
  if (!p.done)
    FIO_LOG_WARNING(
        "JSON parsing was incomplete, minification output is partial");
  fprintf(stderr, "%s\n", fio_str2ptr(&p.out));
  fio_str_destroy(&p.out);
}

int main(int argc, char const *argv[]) {
  // a default string to demo
  const char *json_cstr =
      u8"{\n\t\"id\":1,\n"
      "\t// comments are ignored.\n"
      "\t\"number\":42,\n"
      "\t\"float\":42.42,\n"
      "\t\"string\":\"\\uD834\\uDD1E oh yeah...\",\n"
      "\t\"hash\":{\n"
      "\t\t\"nested\":true\n"
      "\t},\n"
      "\t\"symbols\":[\"id\","
      "\"number\",\"float\",\"string\",\"hash\",\"symbols\"]\n}";

  fio_str_s json = FIO_STR_INIT_STATIC(json_cstr);

  fio_cli_start(
      argc,
      argv,
      0,
      1,
      "This program runs a JSON roundtrip test. Use:\n\n\tNAME [-d] [-f "
      "<filename>]\n\tNAME [-d] [JSON string]",
      FIO_CLI_STRING("--file -f a file to load for JSON roundtrip testing."),
      FIO_CLI_BOOL("--pretty -p -b test Beautify / Prettify roundtrip."),
      FIO_CLI_BOOL("--verbose -v enable debugging mode logging."));
  if (fio_cli_get_bool("-v")) {
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  }
  if (fio_cli_unnamed_count()) {
    fio_str_destroy(&json);
    fio_str_write(&json, fio_cli_unnamed(0), strlen(fio_cli_unnamed(0)));
  }
  if (fio_cli_get("-f")) {
    fio_str_destroy(&json);
    if (!fio_str_readfile(&json, fio_cli_get("-f"), 0, 0).buf) {
      FIO_LOG_FATAL("File missing: %s", fio_cli_get("-f"));
      exit(-1);
    }
  }

  FIO_LOG_DEBUG2("attempting to parse:\n%s\n", fio_str2ptr(&json));

  // Parsing the JSON and cleanup
  run_my_json_example(fio_str2ptr(&json), fio_str_len(&json));
  fio_str_destroy(&json);
  return 0;
}
