---
title: facil.io - FIOBJ Mustache API
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

[Mustache logic-less template](http://mustache.github.io) rendering is supported using the `FIOBJ` types and the included mustache parser (which, much like the JSON parser, can be used independently).

Because both the JSON and the Mustache API are based on the FIOBJ type system, it's easy to render JSON data using a Mustache template.

### Mustache Template Initialization API

Template data can be loaded from a file or from memory.

By loading the data from a file, or by providing a file name when loading the template from memory, partial templates are automatically resolved, loaded and parsed.

#### `fiobj_mustache_load`

```c
mustache_s *fiobj_mustache_load(fio_str_info_s filename);
```

Loads a mustache template from a file, converting it into an opaque instruction array.

This is the same as calling:

```c
fiobj_mustache_new(.filename = filename.data, .filename_len = filename.len);
```

Returns an opaque `mustache_s` pointer to the instruction array or NULL (on error).

The `filename` argument should contain the template's file name.

Remember to call `fiobj_mustache_free` when done with the template.

#### `fiobj_mustache_new`

```c
mustache_s *fiobj_mustache_new(mustache_load_args_s args);
#define fiobj_mustache_new(...)                                                \
  fiobj_mustache_new((mustache_load_args_s){__VA_ARGS__})
```

Loads a mustache template, either from memory or a file, converting it into an opaque instruction array.

Returns an opaque `mustache_s` pointer to the instruction array or NULL (on error).

The `mustache_s *` object can be used to render the same template multiple times concurrently.

The `fiobj_mustache_new` function is shadowed by the `fiobj_mustache_new` MACRO, which allows the function to accept any of the following "named arguments":

* **`filename`**  the root template's file name.
    
    ```c
    // argument type:
    char const *filename
    ```

* **`filename_len`**  the file name's length.
    
    ```c
    // argument type:
    size_t filename_len
    ```

* **`data`**  if both `data` and `data_len` are set, `data` will be used as the file's contents (instead of reading the file).
    
    ```c
    // argument type:
    char const *data
    ```

* **`data_len`**  if set, `data` will be used as the file's contents.
    
    ```c
    // argument type:
    size_t data_len
    ```

* **`err`**  a container for any template load errors.
    
    ```c
    // argument type:
    mustache_error_en *err
    ```


Remember to call `fiobj_mustache_free` when done with the template.

**Note**:

By setting the `filename` argument even when the `data` argument exists, it will allow path resolution for partial templates. Otherwise, there is no way to know where to find the partial templates.

The `mustache_error_en` type is an `enum` which, if set, will contain any of the following:

```c
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
```

#### `fiobj_mustache_free`

```c
void fiobj_mustache_free(mustache_s *mustache);
```

Frees the mustache template pointer immediately (careful when using the template concurrently).

### Mustache Template Rendering API


#### `fiobj_mustache_build`

```c
FIOBJ fiobj_mustache_build(mustache_s *mustache, FIOBJ data);
```

Creates a FIOBJ String containing the rendered template using the information in the FIOBJ `data` object (which should be a `FIOBJ_T_HASH` or data access might fail).

Returns FIOBJ_INVALID if an error occurred and a FIOBJ String on success.

Remember to call `fiobj_free` to free the String (or call `fiobj_send_free`).

**Note**: The `mustache_s *` object can be used to render the same template multiple times concurrently.

#### `fiobj_mustache_build2`

```c
FIOBJ fiobj_mustache_build2(FIOBJ dest, mustache_s *mustache, FIOBJ data);
```

Renders a template into an existing FIOBJ String (`dest`'s end), using the information in the `data` object.

Returns FIOBJ_INVALID if an error occurred and a FIOBJ String on success.
