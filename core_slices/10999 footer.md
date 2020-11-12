## Version and Compilation Related Macros

The following macros effect facil.io's compilation and can be used to validate the API version exposed by the library.

### Version Macros

Currently the version macros are only available for facil.io's STL core library.

### Compilation Macros

The facil.io core library has some hard coded values that can be adjusted by defining the following macros during compile time.

#### `FIO_MAX_SOCK_CAPACITY`

This macro define the maximum hard coded number of connections per worker process.

To be more accurate, this number represents the highest `fd` value allowed by library functions.

If the soft coded OS limit is higher than this number, than this limit will be enforced instead.

#### `FIO_ENGINE_POLL`, `FIO_ENGINE_EPOLL`, `FIO_ENGINE_KQUEUE`

If set, facil.io will prefer the specified polling system call (`poll`, `epoll` or `kqueue`) rather then attempting to auto-detect the correct system call.

To set any of these flag while using the facil.io `makefile`, set the `FIO_FORCE_POLL` / `FIO_FORCE_EPOLL` / `FIO_FORCE_KQUEUE` environment variable to true. i.e.:

```bash
FIO_FORCE_POLL=1 make
```

It should be noted that for most use-cases, `epoll` and `kqueue` will perform better.

#### `FIO_CPU_CORES_LIMIT`

The facil.io startup procedure allows for auto-CPU core detection.

Sometimes it would make sense to limit this auto-detection to a lower number, such as on systems with more than 32 cores.

This is only relevant to automated values, when running facil.io with zero threads and processes, which invokes a large matrix of workers and threads (see [facil_start](#facil_start)).

This does NOT effect manually set (non-zero) worker/thread values.

#### `FIO_POLL_MAX_EVENTS`

This macro sets the maximum number of IO events facil.io will pre-schedule at the beginning of each cycle, when using `epoll` or `kqueue` (not when using `poll`).

Since this requires stack pre-allocated memory, this number shouldn't be set too high. Reasonable values range from 8 to 160.

The default value is currently 64.
