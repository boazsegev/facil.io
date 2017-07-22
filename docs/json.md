# Using facil.io's JSON support

Parsing, editing and outputting JSON in C can be easily accomplished using [facil.io's dynamic types](fiobj.md) (`fiobj_s`).

There are faster alternatives out there (i.e., the C++ RapidJSON is much faster).

However, `facil.io` offers the added benefit of complete parsing from JSON to object. This is in contrast to some parsers that offer a mid-way structure (often a linked list of JSON nodes) or delay parsing for types such as `true`, `false` and Numbers.

`facil.io` also offers the added benefit of complete formatting from object to JSON. This is in contrast to some solutions that require a linked list of node structures.

## Parsing JSON

The `facil.io` parser will parse any C string until it either consumes the whole string or completes parsing of a JSON object.

For example, the following program will minify (or prettify) JSON data:

```c
// include the `fiobj` module from `facil.io`
#include "fiobj.h"
#include <string.h>
// this is passed as an argument to `fiobj_obj2json`
// change this to 1 to prettify.
#define PRETTY 0

int main(int argc, char const *argv[]) {
  // a default string to demo
  const char * json = u8"{\n\t\"id\":1,\n"
                     "\t// comments are ignored.\n"
                     "\t\"number\":42,\n"
                     "\t\"float\":42.42,\n"
                     "\t\"string\":\"𝄞 oh yeah...\",\n"
                     "\t\"hash\":{\n"
                     "\t\t\"nested\":true\n"
                     "\t},\n"
                     "\t\"symbols\":[\"id\","
                     "\"number\",\"float\",\"string\",\"hash\",\"symbols\"]\n}";
  if (argc == 2) {
    // accept command line JSON data
    json = argv[1];
  }
  printf("\nattempting to parse:\n%s\n", json);
  // actual code for parsing the JSON
  fiobj_s * obj;
  size_t consumed = fiobj_json2obj(&obj, json, strlen(json));
  // test for errors
  if (!obj) {
    printf("\nERROR, couldn't parse data.\n");
    exit(-1);
  }
  // format the JSON back to a String object and print it up
  fiobj_s * str = fiobj_obj2json(obj, PRETTY);
  printf("\nOriginal JSON length was: %lu bytes,"
         "output is %lu bytes:\n\n%s\n\n",
         consumed, (size_t)fiobj_obj2cstr(str).len, fiobj_obj2cstr(str).data);
  // cleanup
  fiobj_free(str);
  fiobj_free(obj);
  return 0;
}
```
