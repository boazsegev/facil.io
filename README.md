# facil.io - Core IO C Library

At the core of the [facil.io framework](https://facil.io) are it's [Simple Template Library (C STL)](https://github.com/facil-io/cstl) and this IO Core libraries.

The IO Core library follows an evented reactor pattern that allows easy management of sockets (`fd`), events and timers using `epoll`, `kqueue` or `poll`.

Some security measures and a user-land based buffer make it easy to send data as well as prevent issues related to `fd` recycling (by using a local `uuid` per connection rather than the `fd`).

The IO Core library also offers [pub/sub services](https://en.wikipedia.org/wiki/Publish–subscribe_pattern) both for local code and for registered connections.

## Documentation

Documentation is available in the [(auto-generated) `fio.md` file](fio.md) as well as [online at facil.io](https://facil.io).

### Running Tests

Testing the STL locally is easy using:

```bash
make test/core
```

The GNU `make` command will compile and run any file in the `tests` folder if it is explicitly listed. i.e.,

```bash
make test/cpp         # Test template compilation in a C++ file (no run)... may fail on some compilers
```

It is possible to use the same `makefile` to compile source code and static library code. See the makefile for details.

### Contribution Notice

If you're submitting a PR, make sure to update the corresponding code slice (file) in the `core_slices` folder.

Also, contributions are subject to the terms and conditions set in [the facil.io contribution guide](https://github.com/boazsegev/facil.io/CONTRIBUTING.md). 
