# Server Tools Documentation (incomplete)

The code in this project is heavily commented and the header files could (and probably should) be used for the actual documentation.

However, experience shows that a quick reference guide is immensely helpful and that Doxygen documentation is well... less helpful and harder to navigate (I'll leave it at that for now).

The documentation in this folder includes:

* The main [`lib-server` API documentation](lib-server.md) (coming soon).

    This documents the main framework's API and should be used when writing custom protocols. This API is (mostly) redundant when using the `http` or `websockets` protocol extensions to this framework.

* The [`http` extension API documentation](http.md) (coming soon).

    The `http` protocol extension allows quick access to the HTTP protocol necessary for writing web applications.

    Although the `lib-server` API is still accessible, the `struct HttpRequest` and `struct HttpResponse` objects and API provide abstractions for the raw HTTP protocol and should be preferred.

* The [`websockets` extension API documentation](websockets.md) (coming soon).

    The `websockets` protocol extension allows quick access to the HTTP and Websockets protocols necessary for writing real-time web applications.

    Although the `lib-server` API is still accessible, the `struct HttpRequest` and `struct HttpResponse` objects and API provide abstractions for the raw HTTP protocol and should be preferred.

* Core documentation that documents the libraries used internally.

    The core documentation can be safely ignored by anyone using the `lib-server`, `http` or `websockets` frameworks.

    The core libraries include (coming soon):

    * [`libreact`](./libreact.md) - The reactor core functionality (EPoll and KQueue abstractions).

    * [`libasync`](./libasync.md) - The thread pool and task management core functionality.

    * [`libbuffer`](./libbuffer.md) - User-land buffer for network asynchronous data writing.

    * [`mini-crypt`](./mini-crypt.md) - Cryptography and Base64 encoding helpers (used during the `websockets` handshake).
