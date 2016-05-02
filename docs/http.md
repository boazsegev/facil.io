# The HTTP `lib-server` extension

The HTTP library extension is vast yet simple and supports up to HTTP/1.1 (HTTP/2 is on the roadmap).

The HTTP extension is comprised of three main parts:

* [The HTTP Protocol (`struct HttpProtocol`)], it's callback(s) and it's global settings.

* [The HTTP Request (`struct HttpRequest`)] object and it's API.

* [The HTTP Response (`struct HttpResponse`)] object and it's API.

The following information is a quick guide. For updated and full information, read the code and it's comments.

At the end of this file you will find a short example for a simple "Hello World" web service.

## The HTTP Protocol

To implement the HTTP protocol in a way that is optimized for your application, such as offering a static file service or setting the maximum size for uploaded data, some settings are available.

These settings are set after receiving a `struct HttpProtocol` pointer using the `HttpProtocol.create` function or during the `on_init` Server callback (when using the `start_http_server` macro).

The HttpProtocol object is defined as follows:

```c
struct HttpProtocol {
  struct Protocol parent; // for internal use.
  int maximum_body_size; // maximum message body size in MegaBytes.
  void (* on_request)(struct HttpRequest* request); // the `on_request` callback.
  char* public_folder; // (optional) the root folder for sending static files.
  object_pool request_pool; // for internal use
};
```

### The `on_request` callback

Once an HTTP request is received (and parsed), the `on_request` callback will be called.

The `on_request` callback accepts a single argument, a `struct HttpRequest *` pointer which contains all the information about the request and the connection (including the `server_pt` and connection UUID). The details about the HttpRequest data are provided later on.

Once the `on_request` returns, the HttpRequest object is recycled and it's data is no longer valid. Should you need the data for future use (such as when using Websockets and wishing to keep a copy of any cookies), copy the data.

Please note: It is possible that a connection will be disconnected while `on_request` is being processed. Since HTTP connections and requests are designed to handled as stateless, no data should be stored within the connection's data store (avoid using `Server.set_udata` for the connection).

### Using the `start_http_server` macro

The easiest way to use the HTTP extension is by using the `start_http_server` macro which allows easy setup for the `on_request` callback and the `public_folder` string (the root folder for a static file HTTP server). The `start_http_server` macro also allows setting any Server related settings and it provides sensible defaults.

If further customization is required, either use the `on_init` Server callback or initiate the Server using `Server.listen` after creating an HttpProtocol and setting any requested customized data. i.e.

```c
// The `on_request` HTTP callback
void on_request(struct HttpRequest* request) {
  // ...
}
// The `on_init` Server callback
void on_init(server_pt srv) {
  struct HttpProtocol * http = (struct HttpProtocol* )(Server.settings(srv)->protocol);
  http->maximum_body_size = 100; // 100Mb (size is in Mb)
  // ...
}
// The application simply starts the server and quits when signaled (^C).
int main(int argc, char const* argv[]) {
  char * static_folder = NULL; // use a string for a static file service.
  start_http_server(on_request, static_folder,
                    .on_init = on_init,
                    .threads = THREAD_COUNT,
                    .processes = WORKER_COUNT );
  return 0;
}
```
When using the macro, the `public_folder` string can start with the `'~'` character to designate the user's home folder, i.e. `"~/www"`.

The folder's real path will be extended by the macro. This **isn't** available when using the protocol directly.

### Using HttpProtocol Directly

More customization can be achieved in different ways, although usually there is little need for it.

The options and techniques are too many for this short documentation, some include inheritance of the HttpProtocol, wrapping the HttpProtocol within a container Protocol, etc'.

For the purpose of this documentation, it is enough to show that we can set the `maximum_body_size` before initiating the server. i.e.:

```c
// The `on_request` HTTP callback
void on_request(struct HttpRequest* request) {
  // ...
}
// create an HttpProtocol, use it, destroy it.
int main(int argc, char const* argv[]) {
  // create an HTTP protocol
  struct HttpProtocol* http = HttpProtocol.create();
  // set any settings
  http->maximum_body_size = 100;
  http->public_folder = "/www";
  // using the HttpResponse pool improves performance when using HttpResponse.
  HttpResponse.init_pool();
  // start the server
  Server.listen((struct ServerSettings){                               
            .protocol = (struct Protocol*)(http),
            .timeout = 5,                                    
            .busy_msg = "HTTP/1.1 503 Service Unavailable\r\n\r\nServer Busy.",                                                         
          });                         
  HttpResponse.destroy_pool();                                            
  HttpProtocol.destroy(http);
}
```

## The HTTP Request

The HTTP protocol has a request-response design and the HttpRequest object (`struct HttpRequest`) is at the heart of the HTTP protocol parsing.

This is the main object's structure:

```c
struct HttpRequest {
  server_pt srv; // a pointer to the server that owns the connection
  uint64_t cuuid; // the connection's UUID
  char buffer[HTTP_HEAD_MAX_SIZE]; // for internal use (the raw header buffer)
  char* method; // the HTTP method (i.e. GET)
  char* path; // the request URL excluding any query parameters (the '?')
  char* query; // the portion of the request URL that follows the ?, if any
  char* version; // the HTTP version provided by the clinet (if any)
  char* host; // the "Host" header (required, always exists)
  size_t content_length; //  the body's content's length, in bytes (can be 0).
  char* content_type; // the body's content type header, if any
  char* upgrade; // the Upgrade header, if any
  char* body_str; // the body of the request, if small enough and exists
  FILE* body_file; // a tmpfile with the body of the request, if exists
  struct private; // for internal use.
};
```
Except when the `FILE * body_file` object is set, the HttpRequest can be copied using a simple assignment:

```c
struct HttpRequest my_copy = *request;
```

If the `body_file` isn't `NULL`, the `tmpfile` will be closed once the `on_request` callback had returned.

### Information about the HttpRequest parser

The HttpRequest parser minimizes any data copying by modifying the incoming stream directly, which is designed to provide a faster and lighter parsing process. To allow for concurrency as well as large request sizes, large requests (larger then 8Kb including header data) are stored in temporary files, to preserve system resources.

The HttpRequest parser limits incoming data both to the `maximum_body_size` and to the hard-coded header size (`HTTP_HEAD_MAX_SIZE`) defined in `http-protocol.h` (set at 8Kb).

These limitation are designed to minimize the effects of DoS attacks by disconnecting connections after 8Kb of headers or the maximum body size was reached and recycling these resources for future connections.

### The HttpRequest API

The data should be quite self explanatory, so for brevity's sake only the API will be listed here.

Although there is no common use case for a thread-safe HttpRequest handling, it should be noted that the HttpRequest API and objects are **not** thread-safe, meaning no more then a single thread should handle an HttpRequest using this API. The HttpRequest can always be copied or safeguarded by the specific implementation.

#### `void HttpRequest.first(struct HttpRequest* request)`

Restarts header iteration. The iteration cycle isn't thread-safe.

#### `int HttpRequest.next(struct HttpRequest* request)`

Moves to the next header.

Returns 0 if the end of the list was reached. The iteration cycle isn't thread-safe.

#### `int HttpRequest.find(struct HttpRequest* request, char* const name)`

Finds a specific header matching the requested string. The search is case insensitive.

Returns 0 if the header couldn't be found.

The iteration cycle isn't thread-safe and it is effected by this function.

#### `char* HttpRequest.name(struct HttpRequest* request)`

Returns the name of the current header in the iteration cycle. The iteration cycle isn't thread-safe.

#### `char* HttpRequest.value(struct HttpRequest* request)`

Returns the value of the current header in the iteration cycle. The iteration cycle isn't thread-safe.

#### `struct HttpRequest* HttpRequest.create(void)`

For internal use. Returns an new heap allocated request object.

#### `HttpRequest.clear(struct HttpRequest* request)`

For internal use. Releases the resources used by a request object but keep's it's core memory for future use.

#### `HttpRequest.destroy(struct HttpRequest* request)`

For internal use. Releases the resources used by a request object and frees it's memory.

#### `int HttpRequest.is_request(struct HttpRequest* request)`

For internal use. Validates that the object is indeed a request object.

## The HTTP Response

The `struct HttpResponse` type contains data required for handling the response.

A response may handle up to 8Kb of header data.

This is the main object's structure:

```c
struct HttpResponse {
  size_t content_length; // The body's response length (if any).
  time_t date; // The HTTP date for the response (in seconds since epoche). Defaults to now.
  time_t last_modified; // The HTTP caching date for the response. Defaults to now.
  char header_buffer[]; // for internal use (The actual header data).
  int status; // The response's status.
  struct {} metadata; // for internal use.
};
```

Example use (excluding error checks):

```c
void on_request(struct HttpRequest request) {
  struct HttpResponse* response = HttpResponse.create(req); // (initialize)
  HttpResponse.write_header2(response, "X-Data", "my data");
  HttpResponse.set_cookie(response, (struct HttpCookie){
    .name = "my_cookie",
    .value = "data"
  });
  HttpResponse.write_body(response, "Hello World!\r\n", 14);
  HttpResponse.destroy(response); // release/pool resources
}

int main()
{
  char * public_folder = NULL
  start_http_server(on_request, public_folder, .threads = 16);
}
```

### The HttpResponse API

The HttpResponse data should be quite self explanatory, so for brevity's sake only the API will be listed here.

That said, it should be noted:

To set a response's content length, use `response->content_length` or, if sending the body using a single `write`, it's possible to leave out the content-length header (see `HttpResponse.write_body` for more details).

The same goes for setting the response's date or caching time-stamp (Last-Modified).

The response object and it's API are NOT thread-safe (it is assumed that no two threads handle the same response at the same time).


#### `void HttpResponse.init_pool(void)`

Creates the response object pool (unless it already exists). This function ISN'T thread-safe.

#### `void HttpResponse.destroy_pool(void)`

Destroys the global response object pool. This function ISN'T thread-safe.

#### `struct HttpResponse * HttpResponse.create(struct HttpRequest * request)`

Creates a new response object or recycles a response object from the response pool.

returns NULL on failure, or a pointer to a valid response object.

#### `void HttpResponse.destroy(struct HttpResponse * response)`

Destroys the response object or places it in the response pool for recycling.

#### `int HttpResponse.pool_limit`

The pool limit property (defaults to 64) gets/sets the limit of the pool storage, making sure that excess memory used is cleared rather then recycled.

#### `void HttpResponse.reset(struct HttpResponse*, struct HttpRequest*)`


Clears the HttpResponse object, linking it with an HttpRequest object (which will be used to set the server's pointer and connection).

#### `char* HttpResponse.status_str(struct HttpResponse*)`

Gets a response status, as a string.

#### `int HttpResponse.write_header( ... )`

The function's prototype looks like so:

```c
int HttpResponse.write_header(struct HttpResponse*,
                              const char* header,
                              const char* value,
                              size_t value_len);
```

Writes a header to the response. This function writes only the requested number of bytes from the header value and can be used even when the header value doesn't contain a NULL terminating byte.

On error, i.e., if the header buffer is full or the headers were already sent (new headers cannot be sent), the function will return -1.

On success, the function returns 0.

#### `int HttpResponse.write_header2( ... )`

The function's prototype looks like so:

```c
int HttpResponse.write_header2(struct HttpResponse*,
                              const char* header,
                              const char* value);
```

Writes a header to the response.

This is equivalent to writing:

```c
HttpResponse.write_header(response, header, value, strlen(value));
```

If the header buffer is full or the headers were already sent (new
headers cannot be sent), the function will return -1.

On success, the function returns 0.


#### `int HttpResponse.set_cookie(struct HttpResponse*, struct HttpCookie)`

Set / Delete a cookie using this helper function.

To set a cookie, use (in this example, a session cookie):
```c
HttpResponse.set_cookie(response, (struct HttpCookie){
        .name = "my_cookie",
        .value = "data" });
```

To delete a cookie, use:

```c
HttpResponse.set_cookie(response, (struct HttpCookie){
        .name = "my_cookie",
        .value = NULL });
```

This function writes a cookie header to the response. Only the requested number of bytes from the cookie value and name are written (if none are provided, a terminating NULL byte is assumed).

Both the name and the value of the cookie are checked for validity (legal characters), but other properties aren't reviewed (domain/path) - please make sure to use only valid data, as HTTP imposes restrictions on these things.

If the header buffer is full or the headers were already sent (new headers cannot be sent), the function will return -1.

On success, the function returns 0.

The `struct HttpCookie` structure offers the following (optional) settings (only the `name` is a required setting):

```c
struct HttpCookie {
  char* name; // The cookie's name (key).
  char* value; // The cookie's value (leave blank to delete cookie).
  char* domain; // The cookie's domain (optional).
  char* path; // The cookie's path (optional).

  size_t name_len; // The cookie name's size in bytes or a terminating NULL will be assumed.
  size_t value_len; // The cookie value's size in bytes or a terminating NULL will be assumed.
  size_t domain_len; // The cookie domain's size in bytes or a terminating NULL will be assumed.
  size_t path_len; // The cookie path's size in bytes or a terminating NULL will be assumed.

  int max_age; // Max Age (how long should the cookie persist), in seconds (0 == session).
  unsigned secure : 1; // Limit cookie to secure connections.
  unsigned http_only : 1; // Limit cookie to HTTP (intended to prevent javascript access/hijacking).
};
```

#### `int HttpResponse.printf(struct HttpResponse*, const char* format, ...)`

Prints a string directly to the header's buffer, appending the header separator (the new line marker '\r\n' should NOT be printed to the headers buffer).

If the header buffer is full or the headers were already sent (new headers cannot be sent), the function will return -1.

On success, the function returns 0.

#### `int HttpResponse.send(struct HttpResponse*)`

Sends the headers (if they weren't previously sent).

If the connection was already closed, the function will return -1. On success,
the function returns 0.

#### `int HttpResponse.write_body(struct HttpResponse*, const char* body, size_t length)`

Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success, the function returns 0.

#### `int HttpResponse.write_body_move(struct HttpResponse*, const char* body, size_t length)`

Sends the headers (if they weren't previously sent) and writes the data to the underlying socket.

The server's outgoing buffer will take ownership of the body and free it's memory using `free` once the data was sent.

If the connection was already closed, the function will return -1. On success, the function returns 0.

#### `int HttpResponse.sendfile(struct HttpResponse*, FILE* pf, size_t length)`

Sends the headers (if they weren't previously sent) and writes the data from the file stream to the underlying socket.

The length property is used for setting protocol data and it is assumed to be correct. The while of the stream will be sent regardless of the `length` properties value.

The server's outgoing buffer will take ownership of the file stream and close the file using `close` once the data was sent.

If the connection was already closed, the function will return -1. On success, the function returns 0.

On error, the file remains open and it's ownership remains owned by the calling function.

#### `int HttpResponse.sendfile2(struct HttpResponse*, char* file_path)`

Sends the complete file referenced by the `file_path` string.

This function requires that the headers weren't previously sent and that the file exists.

On failure, the function will return -1. On success, the function returns 0.

#### `void HttpResponse.close(struct HttpResponse*)`

Closes the connection.

## A Quick Example

Here's a simple "Hello World" using the Http extensions:

```c
// include location may vary according to your makefile and project hierarchy.
#include "http.h"
// Concurrency Settings for our demo code.
#define THREAD_COUNT 1
#define WORKER_COUNT 1
// The `on_request` HTTP callback
void on_request(struct HttpRequest* request) {
  struct HttpResponse* response = HttpResponse.create(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}
// The application simply starts the server and quits when signaled (^C).
int main(int argc, char const* argv[]) {
  char * static_folder = NULL; // use a string for a static file service.
  start_http_server(on_request, static_folder,
                    .threads = THREAD_COUNT,
                    .processes = WORKER_COUNT );
  return 0;
}
```
