# Change Log

### Ver. 0.5.3 (next)

**Change**: minor changes to the versioning scheme removed some version MACROS... this isn't API related, so I don't consider it a breaking change, but it might effect code that relied too much on internal workings. The only valid version macros are the `FACIL_VERSION_*` macros, in the `facil.h` header.

**Change**: (`http`) the HTTP/1.x parser was re-written and replaced. It should perform the same, while being easier to maintain. Also, the new parser could potentially be used to author an HTTP client.

**Change**: (`websocket`) the Websocket parser was re-written and replaced, decoupling the parser and message wrapper from the IO layer. Performance might slightly improve, but should mostly remain the same. However, the new code is easier to maintain and easier to port to other implementations. Also, the new parser supports a client mode (message masking).

**Fix**: (`websocket`) fix #16, where a client's first message could have been lost due to long `on_open` processing times. This was fixed by fragmenting the `upgrade` event into two events, adding the `facil_attach_locked` feature and attaching the new protocol before sending the response. Credit to @madsheep and @nilclass for exposing the issue and tracking it down to the `on_open` callbacks.

**Fix**: (`sock`) sockets created using the TCP/IP `sock` library now use `TCP_NODELAY` as the new default. This shouldn't be considered a breaking change as much as it should be considered a fix.

**Fix**: (`http1`) HTTP/1.x is now more fragmented, avoiding a `read` loop to allow for mid-stream / mid-processing protocol upgrades. This also fixes #16 at it's root (besides the improved `websocket` callback handling).

**Fix**: (`http1`) HTTP/1.x now correctly initializes the `udata` pointer to NULL fore each new request.

**Fix**: (`facil`) the `facil_run_every` function now correctly calls the `on_finish` callback when a timer initialization fails. This fixes a leak that could have occurred due to inconsistent API expectations. Workarounds written due to this issue should be removed.

**Fix**: (`defer`) a shutdown issue in `defer_perform_in_fork` was detected by @cdkrot and his fix was implemented.

**Fix**: (`evio`) fixed an issue where the evented IO library failed to reset the state indicator after `evio_close` was called, causing some functions to believe that events are still processed. Now the `evio_isactive` will correctly indicate that the evented IO is inactive after `evio_close` was called.

**Fix**: (documentation) credit to @cdkrot for reporting an outdated demo in the README.

**Fix**: (linking) added the missing `-lm` linker flag for `gcc`/Linux (I was using `clang`, which automatically links to the math library, so I didn't notice this).

**Portability**: added `extern "C"` directive for untested C++ support.

**Feature**: 🎉 added a CLI helper service, allowing easy parsing and publishing of possible command line arguments.

**Feature**: 🎉🎉 added a dynamic type library to `facil.io`'s core, making some common web related tasks easier to manage.

**Feature**: 🎉🎉🎉 added native JSON support. JSON strings can be converted to `fiobj_s *` objects and `fiobj_s *` objects can be rendered as JSON! I'm hoping to get it [benchmarked publicly](https://github.com/miloyip/nativejson-benchmark/pull/92).



### Ver. 0.5.2

**Change**: non-breaking changes to the folder structure are also reflected in the updated `makefile` and `.clang_complete`.

**Fix**: (`defer`) fixed `SIGTERM` handling (signal was mistakingly filtered away).

**Fix**: (`http_response`) fixed `http_response_sendfile2` where path concatenation occurred without a folder separator (`/`) and exclusively safe file paths were being ignored (the function assumed an unsafe path to be used, at least in part).

**Fix**: minor fixes and documentation.

**Fix / Feature**: (`facil`) sibling processes will now detect a sibling's death (caused by a crashed process) and shutdown.

**Feature**: @benjcal suggested the script used to create new applications. The current version is a stand-in draft used for testing.

**Feature**: temporary boiler plate code for a simple "hello world" HTTP application using the new application script (see the README). This is a temporary design to allow us to test the script's functionality and decide on the final boiler plate's design.

### Ver. 0.5.1

**Fix**: (`sock`) Fixed an issue where `sock_flush` would always invoke `sock_touch`, even if no data was actually sent on the wire.

**Fix**: (`sock`) fixed a possible issue with `sock_flush_strong` which might have caused the facil.io to hang.

**Fix**: (`Websocket`) fixed an issue with fragmented pipelined Websocket messages.

**Feature**: (`facil`) easily force an IO event, even if it did not occur, using `facil_force_event`.

### Ver. 0.5.0

**Braking changes**: (`pubsub`) The API was changed / updated, making `pubsub_engine_s` objects easier to author and allowing allocations to be avoided by utilizing two `void * udata` fields... Since this is a breaking change, and following semantic versioning, the minor version is updated. I do wish I could have delayed the version bump, as the roadmap ahead is long, but it is what it is.

**Braking changes**: (`facil`) Since the API is already changing a bit, I thought I'd clean it up a bit and have all the `on_X` flow events (`on_close`, `on_fail`, `on_start`...) share the same function signature where possible.

**Changes**: (`facil`) Minor changes to the `fio_cluster_*` API now use signed message types. All negative `msg_type` values are reserved for internal use.

**Fix**: plugging memory leaks while testing the system under very high stress.

**Fix**: (`pubsub`, `fio_dict`) Fixed glob pattern matching... I hope. It seems to work fine, but I'm not sure it the algorithm matches the Redis implementation which is the de-facto standard for channel pattern matching.

**Security**: (`http`) the HTTP parser now breaks pipelined HTTP requests into fragmented events, preventing an attacker from monopolizing requests through endless pipelining of requests that have a long processing time.

**Fix**: (`http`) `http_listen` will now always *copy* the string for the `public_folder`, allowing dynamic strings to be safely used.

**Fix**: (`http`) default error files weren't located due to missing `/` in name. Fixed by adjusting the requirement for the `/` to be more adaptive.

**Fix**: (`http`) dates were off by 1 day. Now fixed.

**Fix**: (`http1`) a minor issue in the `on_data` callback could have caused the parser to crash in rare cases of fragmented pipelined requests on slower connections. This is now fixed.

**Fix**?: (`http`) When decoding the path or a request, the `+` sign is now left unencoded (correct behavior), trusting in better clients in this great jungle.

**Fix**: (`facil`) `facil_defer` would leak memory if a connection was disconnected while a task was scheduled.

**Fix**: (`facil`) `facil_connect` now correctly calls the `on_fail` callback even on immediate failures (i.e. when the function call was missing a target address and port).

**Fix**: (`facil`) `facil_connect` can now be called before other socket events (protected form library initialization conflicts).

**Fix**: (`facil`) `facil_listen` will now always *copy* the string for the `port`, allowing dynamic strings to be safely used when `FACIL_PRINT_STATE` is set.

**Fix**: (`facil`) `facil_last_tick` would crash if called before the library was initialized during socket operations (`facil_listen`, `facil_attach`, etc')...  now `facil_last_tick` falls back to `time()` if nothing happened yet.

**Fix**: (`facil`) `.on_idle` now correctly checks for non networks events as well before the callback is called.

**Fix**: (`defer`) A large enough (or fast enough) thread pool in a forked process would complete the existing tasks before the active flag was set, causing the facil.io reactor to be stranded in an unscheduled mode, as if expecting to exit. This is now fixed by setting a temporary flag pointer for the forked children, preventing a premature task cleanup.

**Changes**: Major folder structure updates make development and support for CMake submodules easier. These changes should also make it easier to push PRs for by offering the `dev` folder for any localized testing prior to submitting the PR.

**Feature**: (`websockets`) The websocket pub/sub support is here - supporting protocol tasks as well as direct client publishsing (and autu text/binary detection)! There are limits and high memory costs related to channel names, since `pubsub` uses a trie for absolute channel matching (i.e. channel name length should be short, definitely less than 1024Bytes).

**Feature**: (`redis`) The websocket pub/sub support features a shiny new Redis engine to synchronize pub/sub across machines! ... I tested it as much as I could, but I know my tests are as buggy as my code, so please test before using.

**Feature**: (`facil_listen`, `http_listen`) supports an optional `on_finish_rw` callback to clean-up the `rw_udata` object.

**Feature**: (`pubsub`) channels now use the available `fio_dict_s` (trie) data store. The potential price of the larger data-structure is elevated by it's absolute protection against hash collisions. Also, I hope that since channels are more often searched than created, this should improve performance when searching for channels by both pattern and perfect match. I hope this combination of hash tables (for client lookup) and tries (for channel traversal) will provide the best balance between string matching, pattern matching, iterations and subscription management.

**Feature**: (`http`) `http_listen` now supports an `on_finish` callback.

**Feature**: (`http1`) HTTP/1.1 will, in some cases, search for available error files (i.e. "400.html") in the `public_folder` root, allowing for custom error messages.

**Feature**: CMake inclusion. Credit to @OwenDelahoy (PR#8).

> To use facil.io in a CMake build you may add it as a submodule to the project's repository.
>
>       git submodule add https://github.com/boazsegev/facil.io.git

> Then add the following line the project's `CMakeLists.txt`
>
>       add_subdirectory(facil.io)

**Optimize**: (`fio_hash_table`) optimize `fio_ht_s` bin memory allocations.

### Ver. 0.4.4

**Fix**: (`pubsub`) Fixed collisions between equal channel names on different engines, so that channels are only considered equal if they share the same name as well as the same engine (rather than only the same name)... actually, if they share the same channel name SipHash value XOR'd with the engine's memory location.

**Fix**: (`facil`) Fixed compiling error on older `gcc` v.4.8.4, discovered on Ubuntu trusty/64.

**Fix**: Fix enhanced CPU cycles introduced in the v.0.4.3 update. Now CPU cycles are lower and thread throttling handles empty queues more effectively.

**Performance**: (`pubsub`) now uses a hash-table storage for the channels, clients and patterns, making the duplicate review in `pubsub_subscribe` much faster, as well as improving large channel collection performance (nothing I can do about pattern pub/sub, though, as they still need to be matched using iterations, channel by channel and for every channel match).

**Feature**: (`http`) The `http_response_sendfile2` function will now test for a `gzip` encoded alternative when the client indicated support for the encoding. To provide a `gzip` alternative file, simply `gzip` the original file and place the `.gz` file in the original file's location.

**Folder Structure**: Updated the folder structure to reflect source code relation to the library. `core` being required files, `http` relating to `http` etc'.

### Ver. 0.4.3

**Fix**: Some killer error handling should now signal all the process group to exit.

**Fix**: (`sock`, `websocket`) `sock_buffer_send` wouldn't automatically schedule a socket buffer flush. This caused some websocket messages to stay in the unsent buffer until a new event would push them along. Now flushing is scheduled and messages are send immediately, regardless of size.

**Fix**: (`facil`) `facil_attach` now correctly calls the `on_close` callback in case of error.

**Fix**: (`facil`) `facil_protocol_try_lock` would return false errors, preventing external access to the internal protocol data... this is now fixed.

**Feature**: (`facil`) Experimental cluster mode messaging, allowing messages to be sent to all the cluster workers. A classic use-case would be a localized pub/sub websocket service that doesn't require a backend database for syncing a single machine... Oh wait, we've added that one too...

**Feature**: (`facil`) Experimental cluster wide pub/sub API with expendable engine support (i.e., I plan to add Redis as a possible engine for websocket pub/sub).

**Update**: (`http`) Updated the `http_listen` to accept the new `sock_rw_hook_set` and `rw_udata` options.

**Update**: (`sock`) Rewrote some of the error handling code. Will it change anything? only if there were issues I didn't know about. It mostly effects errno value availability, I think.

### Ver. 0.4.2

**Fix**: (`sock`) Fixed an issue with the `sendfile` implementation on macOS and BSD, where medium to large files wouldn't be sent correctly.

**Fix**: (`sock`) Fixed the `sock_rw_hook_set` implementation (would lock the wrong `fd`).

**Design**: (`facil`) Separated the Read/Write hooks from the protocol's `on_open` callback by adding a `set_rw_hook` callback, allowing the same protocol to be used either with or without Read/Write hooks (i.e., both HTTP and HTTPS can share the same `on_open` function).

**Fix**: (`evio`, `facil`) Closes the `evio` once facil.io finished running, presumably allowing facil.io to be reinitialized and run again.

**Fix**: (`defer`) return an error if `defer_perform_in_fork` is called from within a running defer-forked process.

**Fix**: (`sock`, `facil`, bscrypt) Add missing `static` keywords.

**Compatibility**: (bscrypt) Add an alternative `HAS_UNIX_FEATURES` test that fits older \*nix compilers.

---

### Ver. 0.4.1

**Fix**: (HTTP/1.1) fixed the default response `date` (should have been "now", but was initialized to 0 instead).

**Fix**: fixed thread throttling for better energy conservation.

**Fix**: fixed stream response logging.

**Compatibility**: (HTTP/1.1) Automatic `should_close` now checks for HTTP/1.0 clients to determine connection persistence.

**Compatibility**: (HTTP/1.1) Added spaces after header names, since some parsers don't seem to read the RFC.

**Fix/Compatibility**: compiling under Linux had been improved.

---

### Ver. 0.4.0

Updated core and design. New API. Minor possible fixes for HTTP pipelining and parsing.

# Historic Change log

The following is a historic change log, from before the `facil_` API.

---

Note: This change log is incomplete. I started it quite late, as interest in the libraries started to warrant better documentation of any changes made.

Although the libraries in this repo are designed to work together, they are also designed to work separately. Hence, the change logs for each library are managed separately. Here are the different libraries and changes:

* [Lib-React (`libreact`)](#lib-react) - The Reactor library.

* [Lib-Sock (`libsock`)](#lib-sock) - The socket helper library (development incomplete).

* [Lib-Async (`libasync`)](#lib-async) - The thread pool and tasking library.

* [Lib-Server (`libserver`)](#lib-server) - The server writing library.

* [MiniCrypt (`minicrypt`)](#minicrypt) - Common simple crypto library (development incomplete).

* [HTTP Protocol (`http`)](#http_protocol) - including request and response helpers, etc'.

* [Websocket extension (`websockets`)](#websocket_extension) - Websocket Protocol for the basic HTTP implementation provided.

## General notes and _future_ plans

Changes I plan to make in future versions:

* Implement a `Server.connect` for client connections and a Websocket client implementation.

* Implement Websocket writing using `libsock` packets instead of `malloc`.

* Remove / fix server task container pooling (`FDTask` and `GroupTask` pools).

## A note about version numbers

I attempt to follow semantic versioning, except that the libraries are still under pre-release development, so version numbers get updated only when a significant change occurs or API breaks.

Libraries with versions less then 0.1.0 have missing features (i.e. `mini-crypt` is missing almost everything except what little published functions it offers).

Minor bug fixes, implementation optimizations etc' might not prompt a change in version numbers (not even the really minor ones).

API breaking changes always cause version bumps (could be a tiny version bump for tiny API changes).

Git commits aren't automatically tested yet and they might introduce new issues or break existing code (I use Git also for backup purposes)...

... In other words, since these libraries are still in early development, test before adopting any updates.

## Lib-React

### V. 0.3.0

* Rewrite from core. The code is (I think) better organized.

*  Different API.

* The reactor is now stateless instead of an object. All state data (except the reactor's ID, which remains static throughout during it's existence), is managed by the OS implementation (`kqueue`/`epoll`).

* Callbacks are statically linked instead of dynamically assigned.

* Better integration with `libsock`.

* (optional) Handles `libsock`'s UUID instead of direct file descriptors, preventing file descriptor collisions.

### V. 0.2.2

* Fixed support for `libsock`, where the `sock_flush` wasn't automatically called due to inline function optimizations used by the compiler (and my errant code).

### V. 0.2.1

Baseline (changes not logged before this point in time).

## Lib-Sock

### V. 0.2.3 (next version number)

* Apple's `getrlimit` is broken, causing server capacity limits to be less than they could / should be.

### V. 0.2.2

* Fixed an issue introduced in `libsock` 0.2.1, where `sock_close` wouldn't close the socket even when all the data was sent.

### V. 0.2.1

* Larger user level buffer - increased from ~4Mb to ~16Mb.

* The system call to `write` will be deferred (asynchronous) when using `libasync`. This can be changed by updating the `SOCK_DELAY_WRITE` value in the `libsock.c` file.

    This will not prevent `sock_write` from emulating a blocking state while the user level buffer is full.

### V. 0.2.0

* Almost the same API. Notice the following: no initialization required; rw_hooks callbacks aren't protected by a lock (use your own thread protection code).

   There was an unknown issue with version 0.1.0 that caused large data sending to hang... tracking it proved harder then re-writing the whole logic, which was both easier and allowed for simplifying some of the code for better maintenance.

* `sock_checkout_packet` will now hang until a packet becomes available. Don't check out more then a single packet at a time and don't hold on to checked out packets, or you might find your threads waiting.

### V. 0.1.0

* Huge rewrite. Different API.

* Uses connection UUIDs instead of direct file descriptors, preventing file descriptor collisions. Note that the UUIDs aren't random and cannot be used to identify the connections across machines or processes.

* No global lock, spin-lock oriented design.

* Better (optional) integration with `libreact`.

### V. 0.0.6

* `libsock` experienced minor API changes, specifically to the `init_socklib` function (which now accepts 0 arguments).

* The `rw_hooks` now support a `flush` callback for hooks that keep an internal buffer. Implementing the `flush` callback will allow these callbacks to prevent a pre-mature closure of the socket stream and ensure that all the data will be sent.

### V. 0.0.5

* Added the client implementation (`sock_connect`).

* Rewrote the whole library to allow for a fixed user-land buffer limit. This means that instead of having buffer packets automatically allocated when more memory is required, the `sock_write(2)` function will hang and flush any pending socket buffers until packets become available.

* File sending is now offset based, so `fseek` data is ignored. This means that it would be possible to cache open `fd` files and send the same file descriptor to multiple clients.

### V. 0.0.4

* Fixed issues with non-system `sendfile` and with underused packet pool memory.

* Added the `.metadata.keep_open` flag, to allow file caching... however, keep in mind that the file offset for read/write is the file's `lseek` position and sending the same file to different sockets will create race conditions related to the file `lseek` position.

* Fix for epoll's on_ready not being sent (sock flush must raise the EAGAIN error, or the on_ready event will not get called). Kqueue is better since the `on_ready` refers to the buffer being clear instead of available (less events to copy the same amount of data, as each data write is optimal when enough data is available to be written).

* optional implementation of sendfile for Apple, BSD and Linux (BSD **not** tested).

* Misc. optimizations. i.e. Buffer packet size now increased to 64Kb, to fit Linux buffer allocation.

* File sending now supports file descriptors.

* TLC support replaced with a simplified read/write hook.

* Changed `struct SockWriteOpt` to a typedef `sock_write_info_s`.

### V. 0.0.3

* Changed `struct Packet` to a typedef `sock_packet_s`.

* fixed and issue where using `sock_write(2)` for big data chunks would cause errors when copying the data to the user buffer.

    it should be noted, for performance reasons, that it is better to send big data using external pointers (especially if the data is cashed) using the `sock_send_packet` function - for cached data, do not set the `packet->external` flag, so the data isn't freed after it was sent.

### V. 0.0.2

* fixed situations in which the `send_packet` might not close a file (if the packet buffer references a `FILE`) before returning an error.

* The use of `sock_free_packet` is now required for any unused Packet object pointers checked out using `sock_checkout_packet`.

    This requirement allows the pool management to minimize memory fragmentation for long running processes.

* `libsock` memory requirements are now higher, as the user land buffer's Packet memory pool is pre-allocated to minimize memory fragmentation.

* Corrected documentation mistakes, such as the one stating that the `sock_send_packet` function will not handle the Packet object's memory on error (it does handle the memory, **always**).

### V. 0.0.1

Baseline (changes not logged before this point in time).

## Lib-Async

### V. 0.4.0

* I rewrote (almost) everything.

* `libasync` now behaves as a global state machine. No more `async_p` objects.

* Uses (by default) `nanosleep` instead of pipes (you can revert back to pipes by setting a simple flag). This, so far, seems to provide better performance (at the expense of a slightly increased CPU load).

### V. 0.3.0

* Fixed task pool initialization to zero-out data that might cause segmentation faults.

* `libasync`'s task pool optimizations and limits were added to minimize memory fragmentation issues for long running processes.

Baseline (changes not logged before this point in time).

## Lib-Server

### V. 0.4.2 (next version number)

* Limited the number of threads (1023) and processes (127) that can be invoked without changing the library's code.

* Minor performance oriented changes.

* Fixed an issue where Websocket upgrade would allow code execution in parallel with `on_open` (protocol locking was fixed while switching the protocol).

* Added `server_each_unsafe` to iterate over all client connections to perform a task. The `unsafe` part in the name is very important - for example, memory could be deallocated during execution.

### V. 0.4.1

* Minor performance oriented changes.

* Shutdown process should now allow single threaded asynchronous (evented) task scheduling.

* Updating a socket's timeout settings automatically "touches" the socket (resets the timeout count).

### V. 0.4.0

* Rewrite from core. The code is more concise with less duplications.

* Different API.

* The server is now a global state machine instead of an object.

* Better integration with `libsock`.

* Handles `libsock`'s UUID instead of direct file descriptors, preventing file descriptor collisions and preventing long running tasks from writing to the wrong client (i.e., if file descriptor 6 got disconnected and someone else connected and receive file descriptor 6 to identify the socket).

* Better concurrency protection and protocol cleanup `on_close`. Now, deferred tasks (`server_task` / `server_each`), the `on_data` callback and even the `on_close` callback all run within a connection's "lock" (busy flag), limiting concurrency for a single connection to the `on_ready` and `ping` callbacks. No it is safe to free the protocol's memory during an `on_close` callback, as it is (almost) guarantied that no running tasks are using that memory (this assumes that `ping` and `on_ready` don't use any data placed protocol's memory).

### V. 0.3.5

* Moved the global server lock (the one protecting global server data integrity) from a mutex to a spin-lock. Considering API design changes that might allow avoiding a lock.

* File sending is now offset based, so `lseek` data is ignored. This means that it should be possible to cache open `fd` files and send the same file descriptor to multiple clients.

### V. 0.3.4

* Updated `sendfile` to only accept file descriptors (not `FILE *`). This is an optimization requirement.

### V. 0.3.3

* fixed situations in which the `sendfile` might not close the file before returning an error.

* There was a chance that the `on_data` callback might return after the connection was disconnected and a new connection was established for the same `fd`. This could have caused the `busy` flag to be cleared even if the new connection was actually busy. This potential issue had been fixed by checking the connection against the UUID counter before clearing the `busy` flag.

* reminder: The `Server.rw_hooks` feature is deprecated. Use `libsock`'s TLC (Transport Layer Callbacks) features instead.

### V. 0.3.2

Baseline (changes not logged before this point in time).

## MiniCrypt (development incomplete)

### V. 0.1.1

* added a "dirty" (and somewhat faster then libc) `gmtime` implementation that ignores localization.

Baseline (changes not logged before this point in time).

## HTTP Protocol

* Sep. 13, 2016: `ETag` support for the static file server, responding with 304 on valid `If-None-Match`.

* Sep. 13, 2016: Updated `HEAD` request handling for static files.

* Fixed pipelining... I think.

* Jun 26, 2016: Fixed logging for static file range requests.

* Jun 26, 2016: Moved URL decoding logic to the `HttpRequest` object.

* Jun 20, 2016: Added basic logging support.

* Jun 20, 2016: Added automatic `Content-Length` header constraints when setting status code to 1xx, 204 or 304.

* Jun 20, 2016: Nicer messages on startup.

* Jun 20, 2016: Updated for new `lib-server` and `libsock`.

* Jun 16, 2016: HttpResponse date handling now utilizes a faster (and dirtier) solution then the standard libc `gmtime_r` and `strftime` solutions.

* Jun 12, 2016: HTTP protocol and HttpResponse `sendfile` and `HttpResponse.sendfile` fixes and optimizations. Now file sending uses file descriptors instead of `FILE *`, avoiding the memory allocations related to `FILE *` data.

* Jun 12, 2016: HttpResponse copy optimizes the first header buffer packet to copy as much of the body as possible into the buffer packet, right after the headers.

* Jun 12, 2016: Optimized mime type search for static file service.

* Jun 9, 2016: Rewrote the HttpResponse implementation to leverage `libsock`'s direct user-land buffer packet injection, minimizing user land data-copying.

* Jun 9, 2016: rewrote the HTTP `sendfile` handling for public folder settings.

* Jun 9, 2016: Fixed an issue related to the new pooling scheme, where old data would persist in some pooled request objects.

* Jun 8, 2016: The HttpRequest object is now being pooled within the request library (not the HTTP protocol implementation) using Atomics (less mutex locking) and minimizing memory fragmentation by pre-initializing the buffer on first request (preventing memory allocated after the first request from getting "stuck behind" any of the pool members).

Jun 7, 2016: Baseline (changes not logged before this point in time).

## Websocket extension

* Resolved issue #6, Credit to @Filly for exposing the issue.

* Memory pool removed. Might be reinstated after patching it up, but when more tests were added, the memory pool failed them.

* Jan 12, 2017: Memory Performance.

     The Websocket connection Protocol now utilizes both a C level memory pool and a local thread storage for temporary data. This helps mitigate possible memory fragmentation issues related to long running processes and long-lived objects.

     In addition, the socket `read` buffer was moved from the protocol object to a local thread storage (assumes pthreads and not green threads). This minimizes the memory footprint for each connection (at the expense of memory locality) and should allow Iodine to support more concurrent connections using less system resources.

     Last, but not least, the default message buffer per connection starts at 4Kb instead of 16Kb (grows as needed, up to `Iodine::Rack.max_msg_size`), assuming smaller messages are the norm.

### Date 20160607

Baseline (changes not logged before this point in time).
