---
title: facil.io - CLI helpers.
sidebar: 0.7.x/_sidebar.md
---
# Command Line Helper

This is a simple Command Line Interface helper extension that makes it easy to handle command line arguments.

To use the facil.io CLI API, include the file `fio_cli.h`

## Overview

Initializing the CLI helper is easy with a single call to the function/macro `fio_cli_start`.

Data can be accessed either as a C string or an integer using the `fio_cli_get` and `fio_cli_set` functions.

### example

```c
fio_cli_start(argc, argv, 0, 0, "this example accepts the following:",

              FIO_CLI_PRINT_HREADER("Concurrency:"),
              FIO_CLI_INT("-t -thread number of threads to run."),
              FIO_CLI_INT("-w -workers number of workers to run."),

              FIO_CLI_PRINT_HREADER("Address Binding:"),
              FIO_CLI_STRING("-b, -address the address to bind to."),
              FIO_CLI_INT("-p,-port the port to bind to."),
              FIO_CLI_PRINT("\t\tset port to zero (0) for Unix s."),

              FIO_CLI_PRINT_HREADER("Logging:"),
              FIO_CLI_BOOL("-v -log enable logging."));

if (fio_cli_get_bool("-v")) {
    printf("Logging is enabled\n");
}

fio_cli_end();
```

## Constants

The following constants are defined by the CLI extension.

#### `FIO_CLI_STRING(argument)`

Indicates the CLI argument should be a String (default).

#### `FIO_CLI_BOOL(argument)`

Indicates the CLI argument is a Boolean value.

#### `FIO_CLI_INT(argument)`

Indicates the CLI argument should be an Integer (numerical). 

#### `FIO_CLI_PRINT_HEADER(string)`

Indicates the CLI argument should only be used when printing the help output. 

Text will be printed underlined, in a new line, with an extra line break preceding it.

#### `FIO_CLI_PRINT(string)`

Indicates the CLI argument should only be used when printing the help output. 

Text will be printed as normal text in a new line.

## Functions

### Initialization / Destruction

#### `fio_cli_start`

```c
void fio_cli_start(int argc, char const *argv[], int unnamed_min,
                   int unnamed_max, char const *description,
                   char const **names);
/* Automatically appends a NULL marker at the end of the `names` array. */
#define fio_cli_start(argc, argv, unnamed_min, unnamed_max, description, ...)  \
  fio_cli_start((argc), (argv), (unnamed_min), (unnamed_max), (description),   \
                (char const *[]){__VA_ARGS__, NULL})
```

This function parses the Command Line Interface (CLI), creating a temporary "dictionary" that allows easy access to the CLI using their names or aliases.

Command line arguments may be typed. If an optional type requirement is provided and the provided argument fails to match the required type, execution will end and an error message will be printed along with a short "help".

The following optional type requirements are:

* FIO_CLI_STRING        - (default) string argument.
* FIO_CLI_BOOL          - boolean argument (no value).
* FIO_CLI_PRINT_HEADER  - header text.
* FIO_CLI_PRINT         - normal text.


Argument names MUST start with the '-' character. The first word starting
without the '-' character will begin the description for the CLI argument.

The arguments "-?", "-h", "-help" and "--help" are automatically handled
unless overridden.

Example use:

```c
fio_cli_start(argc, argv, 0, 0, "this example accepts the following:",

              FIO_CLI_PRINT_HREADER("Concurrency:"),
              FIO_CLI_INT("-t -thread number of threads to run."),
              FIO_CLI_INT("-w -workers number of workers to run."),

              FIO_CLI_PRINT_HREADER("Address Binding:"),
              FIO_CLI_STRING("-b, -address the address to bind to."),
              FIO_CLI_INT("-p,-port the port to bind to."),
              FIO_CLI_PRINT("\t\tset port to zero (0) for Unix s."),

              FIO_CLI_PRINT_HREADER("Logging:"),
              FIO_CLI_BOOL("-v -log enable logging."));
 ```

This would allow access to the named arguments:

```c
fio_cli_get("-b") == fio_cli_get("-address");
```

Once all the data was accessed, free the parsed data dictionary using:

```c
fio_cli_end();
```
It should be noted, arguments will be recognized in a number of forms, i.e.:

```bash
app -t=1 -p3000 -a localhost
```

#### `fio_cli_end`

```c
void fio_cli_end(void);
```

Clears the memory used by the CLI dictionary, removing all parsed data.

### Access CLI arguments

#### `fio_cli_get`

```c
char const *fio_cli_get(char const *name);
```

Returns the argument's value as a NUL terminated C String.

#### `fio_cli_get_i`

```c
int fio_cli_get_i(char const *name);
```

Returns the argument's value as an integer.

#### `fio_cli_get_bool`

```c
#define fio_cli_get_bool(name) (fio_cli_get((name)) != NULL)
```

This MACRO returns the argument's value as a boolean.

#### `fio_cli_unnamed_count`

```c
unsigned int fio_cli_unnamed_count(void);
```

Returns the number of unnamed arguments.

Unnamed arguments are only relevant if the `unnamed_min` argument in `fio_cli_start` was greater than 0.

#### `fio_cli_unnamed`

```c
char const *fio_cli_unnamed(unsigned int index);
```

Returns the unnamed arguments using a 0 (zero) based `index`.

Unnamed arguments are only relevant if the `unnamed_min` argument in `fio_cli_start` was greater than 0.

### Set CLI argument data

#### `fio_cli_set`

```c
void fio_cli_set(char const *name, char const *value);
```

Sets the argument's value as a NUL terminated C String (no copy!).

CAREFUL: This does not automatically detect aliases or type violations! it will only effect the specific name given, even if invalid. i.e.:

```c
fio_cli_start(argc, argv, 0, 0,
             "this is example accepts the following options:",
             FIO_CLI_INT("-p -port the port to bind to"));

fio_cli_set("-p", "hello"); // fio_cli_get("-p") != fio_cli_get("-port");
```

Note: this does NOT copy the C strings to memory. Memory should be kept alive until `fio_cli_end` is called.

#### `fio_cli_set_default`

```c
#define fio_cli_set_default(name, value)                                       \
  if (!fio_cli_get((name)))                                                    \
    fio_cli_set(name, value);
```

This MACRO sets the argument's value only if the argument has no existing value.

CAREFUL: This does not automatically detect aliases or type violations! it will only effect the specific name given, even if invalid. See [fio_cli_set](#fio_cli_set).

## Important Notes

The CLI extension is **NOT** thread safe. If you wish to write data to the CLI storage while facil.io is running, you should protect both read and write access to the storage.
