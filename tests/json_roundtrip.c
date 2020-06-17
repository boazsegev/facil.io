#define FIO_CLI
#define FIO_LOG
#include "fio-stl.h"
#define FIO_FIOBJ
#include "fio-stl.h"

// Prettyfy JSON? this is passed as an argument to `fiobj2json`
#define PRETTY 0

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
  FIOBJ_STR_TEMP_VAR_STATIC(json, (char *)json_cstr, strlen(json_cstr));
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
  if (fio_cli_get_bool("-d"))
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  if (fio_cli_unnamed_count()) {
    fiobj_str_destroy(json);
    fiobj_str_write(json, fio_cli_unnamed(0), strlen(fio_cli_unnamed(0)));
  }
  if (fio_cli_get("-f")) {
    fiobj_str_destroy(json);
    if (!fiobj_str_readfile(json, fio_cli_get("-f"), 0, 0).buf) {
      FIO_LOG_FATAL("File missing: %s", fio_cli_get("-f"));
      exit(-1);
    }
  }

  FIO_LOG_DEBUG2("attempting to parse:\n%s\n", fiobj_str2cstr(json).buf);

  // Parsing the JSON
  size_t consumed = 0;
  FIOBJ obj1 = fiobj_json_parse2(
      (char *)fiobj_str2cstr(json).buf, fiobj_str2cstr(json).len, &consumed);
  // test for errors
  FIO_ASSERT(obj1, "couldn't parse data.");

  // formatting the JSON
  size_t consumed2 = 0;
  FIOBJ json2 = fiobj2json(FIOBJ_INVALID, obj1, fio_cli_get_bool("-b"));
  FIOBJ obj2 = fiobj_json_parse2(
      (char *)fiobj_str2cstr(json2).buf, fiobj_str2cstr(json2).len, &consumed2);
  FIO_LOG_DEBUG2("JSON reprtinted:\n%s", fiobj_str2cstr(json2).buf);
  FIO_ASSERT(obj2, "JSON roundtrip parsing failed");
  FIO_LOG_DEBUG2("JSON re-parsed:\n%s", fiobj2cstr(obj2).buf);
  FIO_ASSERT(consumed2 == fiobj_str2cstr(json2).len,
             "Should have consumed all the stringified data");
  FIO_ASSERT(fiobj_is_eq(obj1, obj2),
             "roundtrip objects should have been equal:\n%s\n\t----VS----\n%s",
             fiobj2cstr(obj1).buf,
             fiobj2cstr(obj2).buf);

  // Formatting the JSON back to a String object and printing it up
  FIO_LOG_INFO("JSON input was %zu bytes\nJSON output is %zu bytes.\n",
               consumed,
               (size_t)fiobj_str_len(json2));
  // cleanup
  FIOBJ_STR_TEMP_DESTROY(json);
  fiobj_free(obj1);
  fiobj_free(obj2);
  fiobj_free(json2);
  return 0;
}
