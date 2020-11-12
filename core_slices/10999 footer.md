## Version and Compilation Related Macros

The following macros effect facil.io's compilation and can be used to validate the API version exposed by the library.

### Version Macros

The version macros relate the version for both facil.io's core library and it's bundled extensions.

#### `FIO_VERSION_MAJOR`

The major version macro is currently zero (0), since the facil.io library's API should still be considered unstable.

In the future, API breaking changes will cause this number to change.

#### `FIO_VERSION_MINOR`

The minor version normally represents new feature or substantial changes that don't effect existing API.

However, as long as facil.io's major version is zero (0), API breaking changes will cause the minor version (rather than the major version) to change.

#### `FIO_VERSION_PATCH`

The patch version is usually indicative to bug fixes.

However, as long as facil.io's major version is zero (0), new feature or substantial changes will cause the patch version to change.

#### `FIO_VERSION_BETA`

A number representing a pre-release beta version.

This indicates the API might change without notice and effects the `FIO_VERSION_STRING`.

#### `FIO_VERSION_STRING`

This macro translates to facil.io's literal string. It can be used, for example, like this:

```c
printf("Running facil.io version" FIO_VERSION_STRING "\r\n");
```

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

#### `FIO_DEFER_THROTTLE_PROGRESSIVE`

The progressive throttling model makes concurrency and parallelism more likely.

Otherwise threads are assumed to be intended for "fallback" in case of slow user code, where a single thread should be active most of the time and other threads are activated only when that single thread is slow to perform. 

By default, `FIO_DEFER_THROTTLE_PROGRESSIVE` is true (1).

#### `FIO_POLL_MAX_EVENTS`

This macro sets the maximum number of IO events facil.io will pre-schedule at the beginning of each cycle, when using `epoll` or `kqueue` (not when using `poll`).

Since this requires stack pre-allocated memory, this number shouldn't be set too high. Reasonable values range from 8 to 160.

The default value is currently 64.

#### `FIO_USE_URGENT_QUEUE`

This macro can be used to disable the priority queue given to outbound IO.

#### `FIO_PUBSUB_SUPPORT`

If true (1), compiles the facil.io pub/sub API. By default, this is true.
