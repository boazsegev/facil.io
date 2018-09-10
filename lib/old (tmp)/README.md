## System Requirements

The `facil.io` library requires Linux / BSD / macOS for proper operation. This is specifically due to the polling system and some file access functionality (which assumes POSIX).

However, I did make my best attempt to support CYGWIN by implementing a `poll` fallback for the polling functionality, I just don't have any way to test the library on CYGWIN.

## Library Requirements

The `facil.io` core library is just the following two files: `fio.h` and `fio.c`.

The rest of the folders and files are bundled extensions to the core library, meant to make some common tasks easier.

Specifically:

- The Command Line Instruction helpers (CLI) makes it easy to handle command line arguments (see `cli/fio_cli.h` and `cli/fio_cli.c`).

- The HTTP / WebSocket extension (`http.h`) makes HTTP and WebSockets a breeze to author. It also uses the `fiobj` library for added ease of use.

