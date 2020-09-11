# How to Contribute

Thank you for inquiring `facil.io`'s contribution guide. It's people like you and me, that are willing to share our efforts, who help make the world of open source development so inspiring and wonderful.

## TL;DR;

* Play nice.

* Use `clang-format`.

* PRs to the `fio_stl.h` (the facil.io C STL - Simple Type Library) should go to the [facil.io C STL repository](https://github.com/facil-io/cstl).

* PRs to the `fio.h` or `fio.c` files (the facil.io IO Core) should go to the [facil.io IO Core repository](https://github.com/facil-io/io-core).

* All other PRs should go to **the correct version branch** in the [facil.io main framework repository](https://github.com/boazsegev/facil.io).

* Always add a comment in the CHANGELOG to say what you did and credit yourself.

* All copyrights for whatever you contribute will be owned by myself (Boaz Segev) and between us we'll consider them public domain (I don't want to deal with legal discussions).

## Guidelines 

### General Guidelines

"Facil" comes from the Spanish word "easy", and this is embedded in `facil.io`'s DNA.

`facil.io` contributions should (ideally) be:

* **Easy to use**:

    clear and concise API, with macros that emulate "named arguments" when appropriate.

* **Easy to maintain**:

    * *Modular*: even at the price of performance and even (although less desired) at the price of keeping things DRY.

        Developers should be able to simply remove the module from their implementation if they're not using it.

        To clarify, a module should have as small a responsibility as possible without requiring non-core modules. This makes the module easier to maintain and minimizes code fragility and code entanglement.

    * *Succinctly Commented*: Too much commenting is noise (we can read code), but too little and a future maintainer might not understand why the code was written in the first place.

    * *Documented*: document functions using Doxygen style comments, but without Doxygen keywords.

* **Easy to port**:

    When possible, code should be portable. This is both true in regards to CPU architecture and in regards to OS and environment.

    The project currently has the following limitation that might be addressed in the future:

    * The code requires `kqueue` or `epoll` services from the OS, which means Linux / BSD / macOS. The code also supports `poll` as a fallback for the rest of the POSIX systems... but this might not work with Windows.

    * The code assumes a Unix environment (file naming etc').

    * Some of the code (namely some HTTP parts) uses unaligned memory access (requiring newer CPUs and possibly introducing undefined behavior).

* **Easy to compile**:

    The code uses GNU `make` and although we have some CMake support, neither CMake nor `configure` should be *required* at any point.

* **Easy to manage**:

    See the License section below. Contributions must relinquish ownership of contributed code, so licensing and copyright can be managed without the need to reach out to every contributer.


### Community Guideline - Play Nice

As a child, I wasn't any good with people (I'm not sure I'm any better now that I'm older)... which is how come I became good with computers and why we have `facil.io` and other open source projects ;-)

However, I promise to do my best to be a respectful communicator and I ask that you do your best as well.

No matter if discussing a PR (where we might find ourselves entering a heated discussion) or answering an issue (where sometime we find ourselves wondering why people think we work for them)... we should all remember that a little compassion and respect goes a long way.

### Style Guide and Guidelines

A few pointers about code styling (pun intended).

* Use `clang-format` with the `LLVM` style. It's not always the best, but it will offer uniformity.

    There some minor changes to the `LLVM` style, so have a look at our `.clang-format` file.

* Initialize all variables during declaration.

* Use `snake_case` with `readable_names` and avoid CamelCase or VeryLongAndOverlyVerboseNames.

* Use the `fio___` prefix for internal helper functions (note the 3 underscores).

* Prefer verbose readable code. Optimize later (but do optimize).

* Common practice abbreviations, context-specific abbreviations (when in context) and auto-complete optimizations are preferred **only when readability isn't significantly affected**.

* Function names **should** be as succinct as possible.

* Use `goto` to move less-likely code branches to the end of a function's body (specifically, error branches should go to a `goto` label).

    It makes the main body of the function more readable (IMHO) and could help with branch prediction (similar to how `unlikely` might help, but using a different approach).

* Use `goto` before returning from a function when a spinlock / mutex unlock is required (specifically, repetition of the unlock code should be avoided).

## License

The project requires that all the code is licensed under the MIT license (though that may change).

Please refrain from using or offering code that requires a change to the licensing scheme or that might prevent future updates to the licensing scheme (I'm considering ISC).

I discovered GitHub doesn't offer a default CLA (Copyright and Licensing Agreement), so I adopted the one used by [BearSSL](https://www.bearssl.org/contrib.html), meaning:

* the resulting code uses the MIT license, listing me (and only me) as the author. You can take credit by stating that the code was written by yourself, but should attribute copyright and authorship to me (Boaz Segev). This is similar to a "work for hire" approach.

* I will list meaningful contributions in the CHANGELOG and special contributions will be listed in the README and/or here.

This allows me to circumvent any future licensing concerns and prevent contributors from revoking the license attached to their code.

## A quick run-down

`facil.io` is comprised of the following module "families":

* The Simple Template Library Core (`facil.io` STL):

    The module in comprised of a single file (**amalgamation**) header library `fio-stl.h` that's automatically generated using the [facil.io/cstl repository](https://github.com/facil-io/cstl).

    Contributions to this module should be made to the corresponding code slice(s) in the [facil.io/cstl repository](https://github.com/facil-io/cstl).

    Note: the `fio-stl.h` file can be included more then once and offers some core types and features, such as binary String support, Arrays, Hash Maps, atomic operations, etc' (see documentation).

    For example, this module contains the code for the Dynamic Types (`FIOBJ`), the code for the built-in JSON support features, the code for the CLI parser and the code for the custom memory allocator.

* The IO Core Library:

    This module comprises `facil.io`'s IO core and requires the STL Core module.

    Contributions to this module should be made to the corresponding code slice(s) in the [facil.io/io-core repository](https://github.com/facil-io/io-core).

    The module in comprised of two (**amalgamation**) files: `fio.h` and `fio.c` and uses the `fio-stl.h` file.

* `FIOBJ` Extensions:

    The core FIOBJ type system is part of the [facil.io C STL](https://github.com/facil-io/cstl). However, this type system is extendable and indeed some network features require the additional type of `FIOBJ_T_IO` (`fiobj_io.h` and `fiobj_io.c`).

    These extensions live in the [facil.io/facil framework repository](https://github.com/facil-io/facil), in the `lib/facil/fiobj` folder.

    This module adds features used by the HTTP / WebSockets module, such as the mustache template engine, or the extension that routes large HTTP payloads to temporary files.

    If this module was removed, the HTTP / WebSockets module would need to be adjusted.

* HTTP / WebSockets:

    The `http` folder in the [facil.io/facil framework repository](https://github.com/facil-io/facil) refers to the inter-connected HTTP/WebSocket extension / module.

    Although this module family seems very entangled, I did my best to make it easy to maintain and extend with a minimum of entanglement.

    HTTP request and response modules support virtual function tables for future HTTP/2 extensions. The actual request/response implementations might vary between protocol implementation, but their interface should be version agnostic.

* Redis:

    The redis engine is in it's own folder in the [facil.io/facil framework repository](https://github.com/facil-io/facil), both because it's clearly an "add-on" (even though it's a pub/sub add-on) and because it's as optional as it gets.

    This is also a good example for my preference for modular design. The RESP parser is a single file library. It can be easily ported to different projects and is totally separate from the network layer.

### Where to start / Roadmap

Before you start working on a feature, please consider opening a PR to edit this CONTRIBUTING file and letting the community know that you took this feature upon yourself.

Add the feature you want to work on to the following list (or assign an existing feature to yourself). This will also allow us to discuss, in the PR's thread, any questions you might have or any expectations that might effect the API or the feature.

Once you have all the information you need to implementing the feature, the discussion can move to the actual feature's PR.

These are the features that have been requested so far. Even if any of them are assigned, feel free to offer your help:

|      Feature      |      assigned      |      remarks                                        |
|-------------------|--------------------|-----------------------------------------------------|
|   Documentation   |     🙏 Help 🙏     |                                                     |
|-------------------|--------------------|-----------------------------------------------------|
|   Security        |                    |  Some more security features would be nice.         |
|-------------------|--------------------|-----------------------------------------------------|

## Notable Contributions


### pre-0.8.x

* @area55git ([Area55](https://github.com/area55git)) contributed the logo under a [Creative Commons Attribution 4.0 International License.](https://creativecommons.org/licenses/by/4.0/).

* @cdkrot took the time to test some of the demo code using valgrind, detecting a shutdown issue with in core `defer` library and offering a quick fix.

* @madsheep and @nilclass took the time to expose a very quite issue (#16) that involved a long processing `on_open` websocket callback and very short network roundtrips, exposing a weakness in the HTTP/1.x logic.

* @64 took the time to test the pre-released 0.6.0 version and submit [PR #25](https://github.com/boazsegev/facil.io/pull/25), fixing a silent error and some warnings.

* Florian Weber (@Florianjw) took time to challenge the RiskyHash draft and [exposed a byte ordering error (last 7 byte reading order)](https://www.reddit.com/r/crypto/comments/9kk5gl/break_my_ciphercollectionpost/eekxw2f/?context=3).

* Chris Anderson (@injinj) did amazing work exploring a 128 bit variation and attacking RiskyHash using a variation on a Meet-In-The-Middle attack, written by Hening Makholm (@hmakholm) on his ([SMHasher fork](https://github.com/hmakholm/smhasher)). The RiskyHash dfraft was updated to address this attack.

