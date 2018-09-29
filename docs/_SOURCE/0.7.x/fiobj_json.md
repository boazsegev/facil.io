---
title: facil.io - JSON API
sidebar: 0.7.x/_sidebar.md
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
// include the `fiobj` module from `facil.io`
#include "fiobj.h"
#include <string.h>
// this is passed as an argument to `fiobj_obj2json`
// change this to 1 to prettify.
#define PRETTY 1

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
    // accept command line JSON data
    json = argv[1];
  }
  printf("\nattempting to parse:\n%s\n", json);

  // Parsing the JSON
  FIOBJ obj = FIOBJ_INVALID;
  size_t consumed = fiobj_json2obj(&obj, json, strlen(json));
  // test for errors
  if (!consumed || !obj) {
    printf("\nERROR, couldn't parse data.\n");
    exit(-1);
  }

  // Example use - printing some JSON data
  FIOBJ key = fiobj_str_new("number", 6);
  if (FIOBJ_TYPE_IS(obj, FIOBJ_T_HASH) // make sure the JSON object is a Hash
      && fiobj_hash_get(obj, key)) {   // test for the existence of "number"
    printf("JSON print example - the meaning of life is %zu",
           (size_t)fiobj_obj2num(fiobj_hash_get(obj, key)));
  }
  fiobj_free(key);

  // Formatting the JSON back to a String object and printing it up
  FIOBJ str = fiobj_obj2json(obj, PRETTY);
  printf("\nParsed JSON input was: %zu bytes"
         "\nJSON output is %zu bytes:\n\n%s\n\n",
         consumed, (size_t)fiobj_obj2cstr(str).len, fiobj_obj2cstr(str).data);
  // cleanup
  fiobj_free(str);
  fiobj_free(obj);
  return 0;
}
```

## Constants

`JSON_MAX_DEPTH` is the maximum depth for nesting. The default value is 32 (should be set during compile time).

Since bit mapping is used, the maximum available nesting value is 32 (32 nested levels use 32 bits in a `uint32_t` type).

Note that facil.io avoids recursion to protect against DoS attacks that attempt stack exploding techniques. 

## Types

### Parsing result types

The JSON parser returns a `FIOBJ` dynamic object of any type (depending on the JSON data).

No assumptions should be made 

### Formating result type

The JSON formatter returns a `FIOBJ` dynamic String, always.

## Functions

### `fiobj_json2obj`

```c
size_t fiobj_json2obj(FIOBJ *pobj, const void *data, size_t len);
```

Parses JSON, setting `pobj` to point to the new Object.

Returns the number of bytes consumed. On Error, 0 is returned and no data is consumed.
 

### `fiobj_obj2json`

```c
FIOBJ fiobj_obj2json(FIOBJ, uint8_t pretty);
```

Stringify an object into a JSON string. Remember to `fiobj_free`.

Note that only the following basic fiobj types are supported: Primitives (True / False / NULL), Numbers (Number / Float), Strings, Hashes and Arrays.
 
Some objects (such as the POSIX specific IO type) are unsupported and may be formatted incorrectly.

### `fiobj_obj2json2`

```c
FIOBJ fiobj_obj2json2(FIOBJ dest, FIOBJ object, uint8_t pretty);
```

Formats an object into a JSON string, appending the JSON string to an existing String. Remember to `fiobj_free` as usual.

Note that only the foloowing basic fiobj types are supported: Primitives (True / False / NULL), Numbers (Number / Float), Strings, Hashes and Arrays.
 
Some objects (such as the POSIX specific IO type) are unsupported and may be formatted incorrectly.
 
## Important Notes

The parser assumes the whole JSON data is present in the data's buffer. A streaming parser is coded into the [`fiobj_json.c` source file](https://github.com/boazsegev/facil.io/blob/master/lib/facil/core/types/fiobj/fiobj_json.c) but no external API is exposed.

The [`fiobj_json.h` header file](https://github.com/boazsegev/facil.io/blob/master/lib/facil/core/types/fiobj/fiobj_json.h) might include more data.
