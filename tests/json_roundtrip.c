#define FIO_FIOBJ
#include "fio-stl.h"

// Prettyfy JSON? this is passed as an argument to `fiobj2json`
#define PRETTY 0

int main(int argc, char const *argv[]) {
  // a default string to demo
  const char *json = u8"{\n\t\"id\":1,\n"
                     "\t// comments are ignored.\n"
                     "\t\"number\":42,\n"
                     "\t\"float\":42.42,\n"
                     "\t\"string\":\"\\uD834\\uDD1E oh yeah...\",\n"
                     "\t\"hash\":{\n"
                     "\t\t\"nested\":true\n"
                     "\t},\n"
                     "\t\"symbols\":[\"id\","
                     "\"number\",\"float\",\"string\",\"hash\",\"symbols\"]\n}";
  if (argc == 2) {
    json = argv[1];
  }

  printf("\nattempting to parse:\n%s\n", json);

  // Parsing the JSON
  size_t consumed = 0;
  FIOBJ obj = fiobj_json_parse2((char *)json, strlen(json), &consumed);
  // test for errors
  if (!obj) {
    printf("\nERROR, couldn't parse data.\n");
    exit(-1);
  }

  // Example use - printing some JSON data
  FIOBJ key = fiobj_str_new_cstr("number", 6);
  if (FIOBJ_TYPE_IS(obj, FIOBJ_T_HASH) // make sure the JSON object is a Hash
      && fiobj_hash_get2(obj, key)) {  // test for the existence of "number"
    printf("JSON print example - the meaning of life is %zu",
           (size_t)fiobj2i(fiobj_hash_get2(obj, key)));
  }
  fiobj_free(key);

  // Formatting the JSON back to a String object and printing it up
  FIOBJ str = fiobj2json(FIOBJ_INVALID, obj, PRETTY);
  printf("\nJSON input was %zu bytes\nJSON output is %zu bytes:\n\n%s\n\n",
         consumed, (size_t)fiobj_str_len(str), fiobj_str2ptr(str));
  // cleanup
  fiobj_free(str);
  fiobj_free(obj);
  return 0;
}
