---
title: facil.io - JSON API
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

## Overview

Parsing, editing and outputting JSON in C can be easily accomplished using [facil.io's dynamic types](fiobj.md) (`FIOBJ`).

There are [faster alternatives as well as slower alternatives out there](json_performance.html) (i.e., the Qajson4c library is probably the most balanced alternative).

However, `facil.io` offers the added benefit of complete parsing from JSON to object. This is in contrast to some parsers that offer a mid-way structure (often a linked list of JSON nodes) or lazy (delayed) parsing for types such as `true`, `false` and Numbers.

`facil.io` also offers the added benefit of complete formatting from a framework wide object type (`FIOBJ`) to JSON. This is in contrast to some solutions that require a linked list of node structures.

### Example

The `facil.io` parser will parse any C string until it either consumes the whole string or completes parsing of a JSON object.

For example, the following program will minify (or prettify) JSON data:

```c
// #include "fio.h" // when using fio.c, use fio.h instead of:
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
```

## Constants

`JSON_MAX_DEPTH` is the maximum depth for nesting. The default value is 512 (should be set during compile time).

Note that it's important to limit the maximum depth for JSON structures since objects might be recursively traversed by some functions (such as the `fiobj_free` or `fiobj2json` functions). 

## Types

### Parsing result types

The JSON parser returns a `FIOBJ` dynamic object of any type (depending on the JSON data).

No assumptions should be made as to the returned data type, but it could easily be tested for, using the `FIOBJ_TYPE(o)` and the `FIOBJ_TYPE_IS(o, type)` macros.

### Formating result type

The JSON formatter returns a `FIOBJ` dynamic String, always.

## Functions

### `fiobj_json_parse`

```c
FIOBJ fiobj_json_parse(fio_str_info_s str, size_t *consumed);
```

Parses a buffer for JSON data.

If `consumed` is not NULL, the `size_t` variable will contain the number of bytes consumed before the parser stopped (due to either error or end of a valid JSON data segment).

Returns a FIOBJ object matching the JSON valid buffer `str`.

If the parsing failed (no complete valid JSON data) `FIOBJ_INVALID` is returned.
 
### `fiobj_json_parse2`

```c
#define fiobj_json_parse2(data_, len_, consumed)                               \
  fiobj_json_parse((fio_str_info_s){.buf = data_, .len = len_}, consumed)
```

Helper macro, calls `fiobj_json_parse` with string information.

### `fiobj2json`

```c
FIOBJ fiobj2json(FIOBJ destination, FIOBJ object, uint8_t beautify);
```

Stringify an object into a JSON string.

If `destination` is a `FIOBJ_T_STRING`, the JSON data will be appended to the end of the string. Otherwise, a new FIOBJ String will be initialized and the data will be written to the new String object (remember to `fiobj_free`).

Note that only the following basic FIOBJ types are supported: Primitives (True / False / NULL), Numbers (Number / Float), Strings, Hashes and Arrays.
 
Some objects (such as the POSIX specific IO type) are unsupported and may be formatted incorrectly (an attempt to convert these types to strings will be made before formatting them).
 
## Important Notes

The parser assumes the whole JSON data is present in the data's buffer. A streaming parser is available in the [`fio-stl.h` library](https://github.com/boazsegev/facil.io/blob/master/lib/facil/fio-stl.h), but isn't implemented for FIOBJ types.
