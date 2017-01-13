# Server Tools Documentation (incomplete)

The code in this project is heavily commented and the header files could (and probably should) be used for the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is well... less helpful and harder to navigate (I'll leave it at that for now).

The documentation in this folder includes:

* The main [`libserver` API documentation](libserver.md).

    This documents the main library API and should be used when writing custom protocols. This API is (mostly) redundant when using the `http` or `websockets` protocol extensions.

* The [`http` extension API documentation]() (Please help me write this).

    The `http` protocol extension allows quick access to the HTTP protocol necessary for writing web applications.

    Although the `libserver` API is still accessible, the `http_request_s` and `http_response_s` objects and API provide abstractions for the raw HTTP protocol and should be preferred.

* The [`websockets` extension API documentation]() (Please help me write this).

    The `websockets` protocol extension allows quick access to the HTTP and Websockets protocols necessary for writing real-time web applications.

    Although the `libserver` API is still accessible, the `http_request_s`, `http_response_s` and `ws_s` objects and API provide abstractions for the raw HTTP and Websocket protocols and should be preferred.

* Core documentation that documents the libraries used internally.

    The core documentation can be safely ignored by anyone using the `libserver`, `http` or `websockets` frameworks.

    The core libraries include (coming soon):

    * [`libreact`](./libreact.md) - The reactor core functionality (EPoll and KQueue abstractions).

    * [`libasync`](./libasync.md) - The thread pool and task management core functionality.

    * [`libsock`](./libsock.md) - A sockets library that resolves common issues such as fd collisions and user land buffer.

    * `mempool` - A localized memory pool. The documentation in the `.h` file is clear enough, no `md` file is provided.
