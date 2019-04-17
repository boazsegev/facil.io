---
title: facil.io - 0.7.x HTTP / WebSockets Server Documentation
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

facil.io includes an HTTP/1.1 and WebSocket server / framework that could be used to author HTTP and WebSocket services, including REST applications, micro-services, etc'.

Note that, currently, only HTTP/1.1 is supported. Support for HTTP/2 is planned for future versions and could be implemented as a custom protocol until such time.

To use the facil.io HTTP and WebSocket API, include the file `http.h`

## Listening to HTTP Connections

#### `http_listen`

```c
intptr_t http_listen(const char *port, const char *binding,
                     struct http_settings_s);
Listens to HTTP connections at the specified `port` and `binding`. 
#define http_listen(port, binding, ...)                                        \
  http_listen((port), (binding), (struct http_settings_s){__VA_ARGS__})
```

The `http_listen` function is shadowed by the `http_listen` MACRO, which allows the function to accept "named arguments", i.e.:

```c
/* Assuming we defined the HTTP request handler: */
static void on_http_request(http_s *h);
// ... 
if (http_listen("3000", NULL,
            .on_request = on_http_request,
            .public_folder = "www" ) == -1) {
    perror("No listening socket available on port 3000");
    exit(-1);
}
```

In addition to the `port` and `address` argument (explained in [`fio_listen`](fio#fio_listen)), the following arguments are supported:

* `on_request`:

    Callback for normal HTTP requests.

        // callback example:
        void on_request(http_s *request);

* `on_request`:

    Callback for Upgrade and EventSource (SSE) requests.

    Server Sent Events (SSE) / EventSource requests set the `requested_protocol` string to `"sse"`. Other protocols (i.e., WebSockets) are represented exactly the same was as the client requested them (be aware or lower case vs. capitalized representations).

        // callback example:
        void on_upgrade(http_s *request, char *requested_protocol, size_t len);

* `on_request`:

    This callback is ignored for HTTP server mode and is only called when a response (not a request) is received. On server connections, this would normally indicate a protocol error.

        // callback example:
        void on_response(http_s *response);

* `on_finish`:

    This (optional) callback will be called when the HTTP service closes. The `setting` pointer will point to the named arguments passed to the `http_listen` function.

        // callback example:
        void on_finish(struct http_settings_s *settings);

* `udata`:

     Opaque user data. facil.io will ignore this field, but you can use it.

        // type:
        void *udata;

* `public_folder`:

    A public folder for file transfers - allows to circumvent any application layer logic and simply serve static files.

    The static file service supports automatic `gz` pre-compressed file alternatives.

        // type:
        const char *public_folder;

* `public_folder_length`:

    The length of the public_folder string.

        // type:
        size_t public_folder_length;

* `tls`:

    A pointer to a `fio_tls_s` object, for [SSL/TLS support](fio_tls) (fio_tls.h).

        // type:
        void *tls;

* `max_header_size`:

    The maximum number of bytes allowed for the request string (method, path, query), header names and fields.

    Defaults to 32Kib (which is about 4 times more than I would recommend).

    This reflects the total overall size. On HTTP/1.1, each header line (name + value pair) is also limited to a hard-coded HTTP_MAX_HEADER_LENGTH bytes.

        // type:
        size_t max_header_size;

* `max_body_size`:

    The maximum size, in bytes, for an HTTP request's body (posting / downloading).

    Defaults to 50Mb (1024 * 1024 * 50).

        // type:
        size_t max_body_size;

* `max_clients`:

    The maximum number of clients that are allowed to connect concurrently. This value's default setting is usually for the best.

    The default value is computed according to the server's capacity, leaving some breathing room for other network and disk operations.

    Note: clients, by the nature of socket programming, are counted according to their internal file descriptor (`fd`) value. Open files and other sockets count towards a server's limit.

        // type:
        intptr_t max_clients;

* `ws_max_msg_size`:

    The maximum WebSocket message size/buffer (in bytes) for WebSocket connections. Defaults to 250KB (250 * 1024).

        // type:
        size_t ws_max_msg_size;

* `timeout`:

    The maximum WebSocket message size/buffer (in bytes) for WebSocket connections. Defaults to 250KB (250 * 1024).

        // type:
        uint8_t timeout;

* `ws_timeout`:

    Timeout for the WebSocket connections, a ping will be sent whenever the timeout is reached.

    Defaults to 40 seconds.

    Connections are only closed when a ping cannot be sent (the network layer fails). Pongs are ignored.

        // type:
        uint8_t ws_timeout;

* `log`:

    Logging flag - set to TRUE to log HTTP requests.

    Defaults to 0 (false).

        // type:
        uint8_t log;

* `is_client`:

    A read only flag set automatically to indicate the protocol's mode.

    This is ignored by the `http_listen` function but can be accessed through the `on_finish` callback and the [`http_settings`](#http_settings) function.

* `reserved*`:

    Reserved for future use.

        // type:
        intptr_t reserved1;
        intptr_t reserved2;
        intptr_t reserved3;


Returns -1 on error and the socket's `uuid` on success.

The `on_finish` callback is always called (even on errors).
 

## Connecting to HTTP as a Client

#### `http_connect`

```c
intptr_t http_connect(const char *address, struct http_settings_s);
#define http_connect(address, ...)                                             \
  http_connect((address), (struct http_settings_s){__VA_ARGS__})
```

Connects to an HTTP server as a client.

This function accepts the same arguments as the [`http_listen`](#http_listen) function (though some of hem will not be relevant for a client connection).

Upon a successful connection, the `on_response` callback is called with an empty `http_s*` handler (status == 0).

Use the HTTP Handler API to set it's content and send the request to the server. The second time the `on_response` function is called, the `http_s` handle will contain the actual response.
 
The `address` argument should contain a full URL style address for the server. i.e.:
 
          "http:/www.example.com:8080/"

If an `address` includes a path or query data, they will be automatically attached (both of them) to the HTTP handle's `path` property. i.e.
 
         "http:/www.example.com:8080/my_path?foo=bar"
         // will result in:
         fiobj_obj2cstr(h->path).data; //=> "/my_path?foo=bar"
 
To open a WebSocket connection, it's possible to use the `ws` protocol signature. However, it would be better to use the [`websocket_connect`](#websocket_connect) function instead.

Returns -1 on error and the socket's uuid on success.

The `on_finish` callback is always called.
 

## The HTTP Data Handle (Request / Response)

HTTP request and response data is manages using the `http_s` structure type, which is defined as follows:

```c
typedef struct {
  struct {
    void *vtbl;
    uintptr_t flag;
    FIOBJ out_headers;
  } private_data;
  struct timespec received_at;
  FIOBJ method;
  FIOBJ status_str;
  FIOBJ version;
  uintptr_t status;
  FIOBJ path;
  FIOBJ query;
  FIOBJ headers;
  FIOBJ cookies;
  FIOBJ params;
  FIOBJ body;
  void *udata;
} http_s;
```

The `http_s` data in NOT thread safe and can only be accessed safely from within the HTTP callbacks (`on_request`, `on_response` and the `http_defer` callback)

### HTTP Handle Data Access

HTTP data can be accessed using the HTTP handle structure fields.

For example:

```c
/* Collects a temporary reference to the Host header. Don't free the reference.*/
FIOBJ host = fiobj_hash_get(h->headers, HTTP_HEADER_HOST);
```

#### `h->received_at`

```c
struct timespec received_at;
```

A time merker indicating when the request was received. 

#### `h->method`

```c
FIOBJ method;
```

A [FIOBJ String](fiobj_str) containing the HTTP method string.

facil.io accepts non-standard methods as well as the standard GET, PUT, POST, etc' HTTP methods. 

#### `h->status`

```c
uintptr_t status;
```

The status used for the response (or if the object is a response).

When sending a request, the status should be set to 0.

#### `h->status_str`

```c
FIOBJ status_str;
```

A [FIOBJ String](fiobj_str) containing the HTTP status string, for response objects (client mode response).

#### `h->version`

```c
FIOBJ version;
```

A [FIOBJ String](fiobj_str) containing the HTTP version string, if any.

#### `h->path`

```c
FIOBJ path;
```

A [FIOBJ String](fiobj_str) containing the request path string, if any.

#### `h->query`

```c
FIOBJ query;
```

A [FIOBJ String](fiobj_str) containing the request query string, if any.

#### `h->headers`

```c
FIOBJ headers;
```

A [FIOBJ Hash Map](fiobj_hash) containing the received header data as either a [FIOBJ String](fiobj_str) or an [FIOBJ Array](fiobj_ary).

When a header is received multiple times (such as cookie headers), an Array of Strings will be used instead of a single String.

#### `h->cookies`

```c
FIOBJ cookies;
```

A placeholder for a hash of cookie data.

The [FIOBJ Hash Map](fiobj_hash) will be initialized when parsing the request using [`http_parse_cookies`](#http_parse_cookies).

#### `h->params`

```c
FIOBJ params;
```

A placeholder for a hash of request data.

The [FIOBJ Hash Map](fiobj_hash) will be initialized when parsing the request using [`http_parse_body`](#http_parse_body) and [`http_parse_query`](#http_parse_query).

#### `h->body`

```c
FIOBJ body;
```

A [FIOBJ Data](fiobj_data) reader for body data (might be a temporary file, a string or NULL).

#### `h->udata`

```c
void *udata;
```

An opaque user data pointer, which can be used **before** calling [`http_defer`](#http_defer) in order to retain persistent application information across events.

#### `h->private_data`

```c
struct {
    void *vtbl;
    uintptr_t flag;
    FIOBJ out_headers;
} private_data;
```

Private data shouldn't be accessed directly. However, if you need to access the data, limit yourself to the `out_headers` field. The `vtbl` and `flag` should **never* be altered as.

The out headers are set using the [`http_set_header`](#http_set_header), [`http_set_header2`](#http_set_header2), and [`http_set_cookie`](#http_set_cookie) functions.

Reading the outgoing headers is possible by directly accessing the [Hash Map](fiobj_hash) data. However, writing data to the Hash should be avoided.

### Connection Information

#### `http_settings`

```c
struct http_settings_s *http_settings(http_s *h);
```

Returns the settings used to setup the connection or NULL on error.

#### `http_peer_addr`

```c
fio_str_info_s http_peer_addr(http_s *h);
```

Returns the direct address of the connected peer (most possibly an intermediary / proxy server).

NOTE: it is my hope that a future variation of this function will return a best guess at the actual client's address


### Setting Headers and Cookies

#### `http_set_header`

```c
int http_set_header(http_s *h, FIOBJ name, FIOBJ value);
```

Sets an outgoing header, taking ownership of the value object, but NOT the name object (so name objects could be reused in future responses).

Returns -1 on error and 0 on success.
 
#### `http_set_header2`

```c
int http_set_header2(http_s *h, fio_str_info_s name, fio_str_info_s value);
```

Sets an outgoing header, copying the data.

Returns -1 on error and 0 on success.


#### `http_set_cookie`

```c
int http_set_cookie(http_s *h, http_cookie_args_s);
#define http_set_cookie(http___handle, ...)                                    \
  http_set_cookie((http___handle), (http_cookie_args_s){__VA_ARGS__})
```

Sets an outgoing **response** cookie.

To set a *request* cookie, simply add the data to a header named `"cookie"`.

The `http_set_cookie` function is shadowed by the `http_set_cookie` MACRO, which allows the function to accept "named arguments", i.e.:

```c
http_set_cookie(request, .name = "my_cookie", .value = "data");
```

In addition to the handle argument (`http_s *`), the following arguments are supported:

* `name`:

     The cookie's name (Symbol).

        // type:
        const char *name;

* `value`:

     The cookie's value. Leave the value blank (NULL) to delete cookie.

        // type:
        const char *value;

* `domain`:

     The cookie's domain (optional).

        // type:
        const char *domain;

* `path`:

     The cookie's path (optional).

        // type:
        const char *path;

* `name_len`:

     The cookie name's length in bytes or a terminating NUL will be assumed.

        // type:
        size_t name_len;

* `value_len`:

     The cookie value's length in bytes or a terminating NULL will be assumed.

        // type:
        size_t value_len;

* `domain_len`:

     The cookie domain's length in bytes or a terminating NULL will be assumed.

        // type:
        size_t domain_len;

* `path_len`:

     The cookie path's length in bytes or a terminating NULL will be assumed.

        // type:
       size_t path_len;

* `max_age`:

     Max Age (how long should the cookie persist), in seconds (0 == session).

        // type:
       int max_age;

* `secure`:

     Limit cookie to secure connections.

        // type:
       unsigned secure : 1;

* `path_len`:

     Limit cookie to HTTP (intended to prevent JavaScript access/hijacking).

        // type:
       unsigned http_only : 1;

Returns -1 on error and 0 on success.

**Note**: Long cookie names and long cookie values will be considered a security violation and an error will be returned. It should be noted that most proxies and servers will refuse long cookie names or values and many impose total header lengths (including cookies) of ~8Kib.


### Sending a Response / Request


#### `http_finish`

```c
void http_finish(http_s *h);
```

Sends the response headers for a header only response.

**Important**: After this function is called, the `http_s` object is no longer valid.
 

#### `http_send_body`

```c
int http_send_body(http_s *h, void *data, uintptr_t length);
```

Sends the response headers and body.

**Note**: The body is *copied* to the HTTP stream and it's memory should be
freed by the calling function.

Returns -1 on error and 0 on success.

**Important**: After this function is called, the `http_s` object is no longer valid.


#### `http_sendfile`

```c
int http_sendfile(http_s *h, int fd, uintptr_t length, uintptr_t offset);
```

Sends the response headers and the specified file (the response's body).

The file is closed automatically.

Returns -1 on error and 0 on success.

**Important**: After this function is called, the `http_s` object is no longer valid.
 
#### `http_sendfile2`

```c
int http_sendfile2(http_s *h, const char *prefix, size_t prefix_len,
                   const char *encoded, size_t encoded_len);
```

Sends the response headers and the specified file (the response's body).

The `local` and `encoded` strings will be joined into a single string that represent the file name. Either or both of these strings can be empty.

The `encoded` string will be URL decoded while the `local` string will used as is.

Returns 0 on success. A success value WILL CONSUME the `http_s` handle (it will become invalid).

Returns -1 on error (The `http_s` handle should still be used).
 
**Important**: After this function is called, the `http_s` object is no longer valid.


#### `http_send_error`

```c
int http_send_error(http_s *h, size_t error_code);
```

Sends an HTTP error response.

Returns -1 on error and 0 on success.

**Important**: After this function is called, the `http_s` object is no longer valid.

<!-- The `uuid` and `settings` arguments are only required if the `http_s` handle is NULL. -->

### Push Promise (future HTTP/2 support)

**Note**: HTTP/2 isn't implemented yet and these functions will simply fail.

#### `http_push_data`

```c
int http_push_data(http_s *h, void *data, uintptr_t length, FIOBJ mime_type);
```

Pushes a data response when supported (HTTP/2 only).

Returns -1 on error and 0 on success.


#### `http_push_file`

```c
int http_push_file(http_s *h, FIOBJ filename, FIOBJ mime_type);
```

Pushes a file response when supported (HTTP/2 only).

If `mime_type` is NULL, an attempt at automatic detection using `filename` will be made.

Returns -1 on error and 0 on success.

### Rescheduling the HTTP event

#### `http_pause`

```c
void http_pause(http_s *h, void (*task)(http_pause_handle_s *http));
```

Pauses the request / response handling and INVALIDATES the current `http_s` handle (no `http` functions can be called).

The `http_resume` function MUST be called (at some point) using the opaque `http` pointer given to the callback `task`.

The opaque `http_pause_handle_s` pointer is only valid for a single call to `http_resume` and can't be used by any regular `http` function (it's a different data type).

Note: the current `http_s` handle will become invalid once this function is called and it's data might be deallocated, invalid or used by a different thread.

#### `http_resume`

```c
void http_resume(http_pause_handle_s *http, void (*task)(http_s *h),
                 void (*fallback)(void *udata));
```

Resumes a request / response handling within a task and INVALIDATES the current `http_s` handle.

The `task` MUST call one of the `http_send_*`, `http_finish`, or `http_pause`functions.

The (optional) `fallback` will receive the opaque `udata` that was stored in the HTTP handle and can be used for cleanup.

Note: `http_resume` can only be called after calling `http_pause` and entering it's task.

Note: the current `http_s` handle will become invalid once this function is called and it's data might be deallocated, invalidated or used by a different thread.
 
#### `http_paused_udata_get`

```c
void *http_paused_udata_get(http_pause_handle_s *http);
```

Returns the `udata` associated with the paused opaque handle 

#### `http_paused_udata_set`

```c
void *http_paused_udata_set(http_pause_handle_s *http, void *udata);
```

Sets the `udata` associated with the paused opaque handle, returning the old value.
 
### Deeper HTTP Data Parsing

The HTTP extension's initial HTTP parser parses the protocol, but not the HTTP data. This allows improved performance when parsing the data isn't necessary.

However, sometimes an application will want to parse a requests (or a response's) content, cookies or query parameters, converting it into a complex object.

This allows an application to convert a String data such as `"user[name]=Joe"` to a nested Hash Map, where the `params` Hash Map's key `user` maps to a nested Hash Map with the key `name` (and the value `"Joe"`, as described by [`http_add2hash`](#http_add2hash))).

#### `http_parse_body`

```c
int http_parse_body(http_s *h);
```

Attempts to decode the request's body using the [`http_add2hash`](#http_add2hash) scheme.

Supported body types include:

* application/x-www-form-urlencoded

* application/json

* multipart/form-data

This should be called before `http_parse_query`, in order to support JSON data.

If the JSON data isn't an object, it will be saved under the key "JSON" in the `params` hash.

If the `multipart/form-data` type contains JSON files, they will NOT be parsed (they will behave like any other file, with `data`, `type` and `filename` keys assigned). This allows non-object JSON data (such as array) to be handled by the app.

#### `http_parse_query`

```c
void http_parse_query(http_s *h);
```

Parses the query part of an HTTP request/response. Uses [`http_add2hash`](#http_add2hash).

This should be called after the `http_parse_body` function, just in case the
body is a JSON object that doesn't have a Hash Map at it's root.

#### `http_parse_cookies`

```c
void http_parse_cookies(http_s *h, uint8_t is_url_encoded);
```

Parses any Cookie / Set-Cookie headers, using the [`http_add2hash`](#http_add2hash) scheme. 

#### `http_add2hash`

```c
int http_add2hash(FIOBJ dest, char *name, size_t name_len, char *value,
                  size_t value_len, uint8_t encoded);
```

Adds a named parameter to the hash, converting a string to an object and resolving nesting references and URL decoding if required. i.e.:

* "name[]" references a nested Array (nested in the Hash).

* "name[key]" references a nested Hash.

* "name[][key]" references a nested Hash within an array. Hash keys will be unique (repeating a key advances the array).

* These rules can be nested (i.e. "name[][key1][][key2]...")

* "name[][]" is an error (there's no way for the parser to analyze dimensions)

Note: names can't begin with `"["` or end with `"]"` as these are reserved characters.
 

#### `http_add2hash2`

```c
int http_add2hash2(FIOBJ dest, char *name, size_t name_len, FIOBJ value,
                   uint8_t encoded);
```

Same as [`http_add2hash`](#http_add2hash), using an existing object.


### Miscellaneous HTTP Helpers

#### `http_status2str`

```c
fio_str_info_s http_status2str(uintptr_t status);
```

Returns a human readable string representing the HTTP status number. 

#### `http_hijack`

```c
intptr_t http_hijack(http_s *h, fio_str_info_s *leftover);
```

Hijacks the socket away from the HTTP protocol and away from facil.io.

It's possible to hijack the socket and than reconnect it to a new protocol object.

It's possible to call `http_finish` immediately after calling `http_hijack` in order to send any outgoing headers before the hijacking is complete.

If any additional HTTP functions are called after the hijacking, the protocol object might attempt to continue reading data from the buffer.

Returns the underlining socket connection's uuid. If `leftover` isn't NULL, it will be populated with any remaining data in the HTTP buffer (the data will be automatically deallocated, so copy the data when in need).

**WARNING**: this isn't a good way to handle HTTP connections, especially as HTTP/2 enters the picture. To implement Server Sent Events consider calling [`http_upgrade2sse`](#http_upgrade2sse) instead.

#### `http_req2str`

```c
FIOBJ http_req2str(http_s *h);
```

Returns a String object representing the unparsed HTTP request (HTTP version
is capped at HTTP/1.1). Mostly usable for proxy usage and debugging.
 

#### `http_write_log`

```c
void http_write_log(http_s *h);
```

Writes a log line to `stderr` about the request / response object.

This function is called automatically if the `.log` setting is enabled.

## WebSockets

### WebSocket Upgrade From HTTP (Server)

#### `http_upgrade2ws`

```c
int http_upgrade2ws(http_s *http, websocket_settings_s);
#define http_upgrade2ws(http, ...)                                             \
  http_upgrade2ws((http), (websocket_settings_s){__VA_ARGS__})
```

Upgrades an HTTP/1.1 connection to a WebSocket connection.

The `http_upgrade2ws` function is shadowed by the `http_upgrade2ws` MACRO, which allows the function to accept "named arguments".

In addition to the `http_s` argument, the following named arguments can be used:

* `on_open`:

    The (optional) `on_open` callback will be called once the WebSocket connection is established and before is is registered with `facil`, so no `on_message` events are raised before `on_open` returns.

        // callback example:
        void on_open(ws_s *ws);
 
* `on_message`:

    The (optional) `on_message` callback will be called whenever a webSocket message is received for this connection.

    The data received points to the WebSocket's message buffer and it will be overwritten once the function exits (it cannot be saved for later, but it can be copied).

        // callback example:
        void on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text);

* `on_ready`:

    The (optional) `on_ready` callback will be after a the underlying socket's buffer changes it's state from full to empty.

    If the socket's buffer is never used, the callback is never called.

        // callback example:
        void on_ready(ws_s *ws);
 
* `on_shutdown`:

    The (optional) on_shutdown callback will be called if a WebSocket connection is still open while the server is shutting down (called before `on_close`).

        // callback example:
        void on_shutdown(ws_s *ws);
 
* `on_close`:

    The `uuid` is the connection's unique ID that can identify the WebSocket. A value of `uuid == -1` indicates the WebSocket connection wasn't established (an error occurred).

    The `udata` is the user data as set during the upgrade or using the `websocket_udata_set` function.

        // callback example:
        void on_close(intptr_t uuid, void *udata);
 

* `udata`:

    Opaque user data.

        // type:
        void *udata;

This function will end the HTTP stage of the connection and attempt to "upgrade" to a WebSockets connection.

The `http_s` handle will be invalid after this call and the `udata` will be set to the new WebSocket `udata`.

A client connection's `on_finish` callback will be called (since the HTTP stage has finished).

Returns 0 on success or -1 on error.

**NOTE**:

The type used by some of the callbacks (`ws_s`) is an opaque WebSocket handle and has no relationship with the named arguments used in this function cal. It is only used to identify a WebSocket connection.

Similar to an `http_s` handle, it is only valid within the scope of the specific connection (the callbacks / tasks) and shouldn't be stored or accessed otherwise.

### WebSocket Connections (Client)

#### `websocket_connect`

```c
int websocket_connect(const char *url, websocket_settings_s settings);
#define websocket_connect(url, ...)                                        \
  websocket_connect((address), (websocket_settings_s){__VA_ARGS__})
```

Connects to a WebSocket service according to the provided address.

This is a somewhat naive connector object, it doesn't perform any authentication or other logical handling. However, it's quire easy to author a complext authentication logic using a combination of `http_connect` and `http_upgrade2ws`.

In addition to the `url` address, this function accepts the same named arguments as [`http_upgrade2ws`](#http_upgrade2ws).

Returns the `uuid` for the future WebSocket on success.

Returns -1 on error;

### WebSocket Connection Management (write / close)

#### `websocket_write`

```c
int websocket_write(ws_s *ws, fio_str_info_s msg, uint8_t is_text);
```

Writes data to the WebSocket. Returns -1 on failure (0 on success).

#### `websocket_close`

```c
void websocket_close(ws_s *ws);
```

Closes a WebSocket connection. */

### WebSocket Pub/Sub

#### `websocket_subscribe`

```c
uintptr_t websocket_subscribe(struct websocket_subscribe_s args);
#define websocket_subscribe(, ...)                                       \
  websocket_subscribe((struct websocket_subscribe_s){.ws = ws_handle, __VA_ARGS__})
```

Subscribes to a pub/sub channel for, allowing for direct message deliverance when the `on_message` callback is missing.

To unsubscribe from the channel, use [`websocket_unsubscribe`](websocket_unsubscribe) (NOT
`fio_unsubscribe`).

The `websocket_subscribe` function is shadowed by the `websocket_subscribe` MACRO, which allows the function to accept "named arguments".

In addition to the `ws_s *` argument, the following named arguments can be used:

* `channel`:

    The channel name used for the subscription. If missing, an empty string is assumed to be the channel name.

        // type:
        fio_str_info_s channel;
 
* `on_message`:

    An optional callback to be called when a pub/sub message is received.

    If missing, Data is directly written to the WebSocket connection.

        // callback example:
        void on_message(ws_s *ws, fio_str_info_s channel,
                     fio_str_info_s msg, void *udata);

* `on_unsubscribe`:

    An optional callback to be called after the subscription was canceled. This should be used for any required cleanup.

        // callback example:
        void on_unsubscribe(void *udata);

* `match`:

    A callback for pattern matching, as described in [`fio_subscribe`](fio#fio_subscribe).

    Note that only the `FIO_MATCH_GLOB` matching function (or NULL for no pattern matching) is safe to use with the Redis extension.

        // callback example:
        int foo_bar_match_fn(fio_str_info_s pattern, fio_str_info_s channel);
        // type:
        fio_match_fn match;

* `udata`:

    Opaque user data.

        // type:
        void *udata;

* `force_binary`:

    When using direct message forwarding (no `on_message` callback), this indicates if messages should be sent to the client as binary blobs, which is the safest approach.

    By default, facil.io will test for UTF-8 data validity and send the data as text if it's a valid UTF-8. Messages above ~32Kb might be assumed to be binary rather than tested.

        // type:
        unsigned force_binary : 1;

* `force_text`:

    When using direct message forwarding (no `on_message` callback), this indicates if messages should be sent to the client as UTF-8 text.

    By default, facil.io will test for UTF-8 data validity and send the data as text if it's a valid UTF-8. Messages above ~32Kb might be assumed to be binary rather than tested.

    `force_binary` has precedence over `force_text`.

        // type:
        unsigned force_text : 1;


Returns a subscription ID on success and 0 on failure.

All subscriptions are automatically canceled and freed once the WebSocket is closed.

#### `websocket_unsubscribe`

```c
void websocket_unsubscribe(ws_s *ws, uintptr_t subscription_id);
```

Unsubscribes from a channel.

Failures are silent.

All subscriptions are automatically revoked once the WebSocket is closed. So
only use this function to unsubscribe while the WebSocket is open.

#### `websocket_optimize4broadcasts`

```c
void websocket_optimize4broadcasts(intptr_t type, int enable);
```

Enables (or disables) any of the following broadcast optimizations:

* `WEBSOCKET_OPTIMIZE_PUBSUB` - Optimize generic Pub/Sub WebSocket broadcasts.

        #define WEBSOCKET_OPTIMIZE_PUBSUB (-32)

* `WEBSOCKET_OPTIMIZE_PUBSUB_TEXT` - Optimize text Pub/Sub WebSocket broadcasts.

        #define WEBSOCKET_OPTIMIZE_PUBSUB_TEXT (-33)

* `WEBSOCKET_OPTIMIZE_PUBSUB_BINARY` - Optimize binary Pub/Sub WebSocket broadcasts.

        #define WEBSOCKET_OPTIMIZE_PUBSUB_BINARY (-34)

This is normally performed automatically by the `websocket_subscribe` function. However, this function is provided for enabling the pub/sub meta-data based optimizations for external connections / subscriptions.

The pub/sub metadata type ID will match the optimnization type requested (i.e., `WEBSOCKET_OPTIMIZE_PUBSUB`) and the optimized data is a FIOBJ String containing a pre-encoded WebSocket packet ready to be sent. i.e.:
 
```c
FIOBJ pre_wrapped = (FIOBJ)fio_message_metadata(msg,
                          WEBSOCKET_OPTIMIZE_PUBSUB);
fiobj_send_free((intptr_t)msg->udata1, fiobj_dup(pre_wrapped));
```

**Note**: to disable an optimization it should be disabled the same amount of times it was enabled - multiple optimization enablements for the same type are merged, but reference counted (disabled when reference is zero).

### WebSocket Data

#### `websocket_udata_get`

```c
void *websocket_udata_get(ws_s *ws);
```

Returns the opaque user data associated with the WebSocket.


#### `websocket_udata_set`

```c
void *websocket_udata_set(ws_s *ws, void *udata);
```

Sets the opaque user data associated with the WebSocket.

Returns the old value, if any.

#### `websocket_uuid`

```c
intptr_t websocket_uuid(ws_s *ws);
```


Returns the underlying socket UUID.

This is only relevant for collecting the protocol object from outside of WebSocket events, as the socket shouldn't be written to.

#### `websocket_is_client`

```c
uint8_t websocket_is_client(ws_s *ws);
```

Returns 1 if the WebSocket connection is in Client mode (connected to a remote server) and 0 if the connection is in Server mode (a connection established using facil.io's HTTP server).

### WebSocket Helpers

#### `websocket_attach`

```c
void websocket_attach(intptr_t uuid, http_settings_s *http_settings,
                      websocket_settings_s *args, void *data, size_t length);
```

Used **internally**: attaches the WebSocket protocol to the uuid.

## EventSource / Server Sent Events (SSE)

### EventSource (SSE) Connection Management

#### `http_upgrade2sse`

```c
int http_upgrade2sse(http_s *h, http_sse_s);
#define http_upgrade2sse(h, ...)                                               \
  http_upgrade2sse((h), (http_sse_s){__VA_ARGS__})
```

Upgrades an HTTP connection to an EventSource (SSE) connection.

The `http_s` handle will be invalid after this call.

On HTTP/1.1 connections, this will preclude future requests using the same connection.

The `http_upgrade2sse` function is shadowed by the `http_upgrade2sse` MACRO, which allows the function to accept "named arguments", much like `http_listen`. i.e.:

```c
on_open_sse(sse_s * sse) {
  http_sse_subscribe(sse, .channel = CHANNEL_NAME); // a simple subscription example
}
on_upgrade(http_s* h) {
  http_upgrade2sse(h, .on_open = on_open_sse);
}
```

In addition to the `http_s` argument, the following arguments are supported:

* `on_open`:

    The (optional) `on_open` callback will be called once the EventSource connection is established.

    The `http_sse_s` pointer passed to the callback contains a copy of the named arguments passed to the `http_upgrade2sse` function.

        // callback example:
        void on_open(http_sse_s *sse);

* `on_ready`:

    The (optional) `on_ready` callback will be called every time the underlying socket's buffer changes it's state to empty.

    If the socket's buffer is never used, the callback might never get called.

    The `http_sse_s` pointer passed to the callback contains a copy of the named arguments passed to the `http_upgrade2sse` function.

        // callback example:
        void on_ready(http_sse_s *sse);

* `on_shutdown`:

    The (optional) `on_shutdown` callback will be called if a connection is still open while the server is shutting down (called before `on_close`).

    The `http_sse_s` pointer passed to the callback contains a copy of the named arguments passed to the `http_upgrade2sse` function.

        // callback example:
        void on_shutdown(http_sse_s *sse);

* `on_close`:

    The (optional) `on_close` callback will be called once a connection is terminated or failed to be established.

    The `udata` passed to the `http_upgrade2sse` function is available through the `http_sse_s` pointer (`sse->udata`).

        // callback example:
        void on_close(http_sse_s *sse);

* `udata`:

    Opaque user data.

        // type:
        void *udata;
 

#### `http_sse_set_timout`

```c
void http_sse_set_timout(http_sse_s *sse, uint8_t timeout);
```

Sets the ping interval for SSE connections.


#### `http_sse_close`

```c
int http_sse_close(http_sse_s *sse);
```

Closes an EventSource (SSE) connection.

### EventSource (SSE) Pub/Sub

#### `http_sse_subscribe`

```c
uintptr_t http_sse_subscribe(http_sse_s *sse,
                             struct http_sse_subscribe_args args);
#define http_sse_subscribe(sse, ...)                                           \
  http_sse_subscribe((sse), (struct http_sse_subscribe_args){__VA_ARGS__})
```

Subscribes to a pub/sub channel for, allowing for direct message deliverance when the `on_message` callback is missing.

To unsubscribe from the channel, use [`http_sse_unsubscribe`](http_sse_unsubscribe) (NOT
`fio_unsubscribe`).

All subscriptions are automatically canceled and freed once the connection is closed.

The `http_sse_subscribe` function is shadowed by the `http_sse_subscribe` MACRO, which allows the function to accept "named arguments".

In addition to the `sse` argument, the following named arguments can be used:

* `channel`:

    The channel name used for the subscription. If missing, an empty string is assumed to be the channel name.

        // type:
        fio_str_info_s channel;
 
* `on_message`:

    An optional callback to be called when a pub/sub message is received.

    If missing, Data is directly written to the HTTP connection.

        // callback example:
        void on_message(http_sse_s *sse, fio_str_info_s channel,
                     fio_str_info_s msg, void *udata);

* `on_unsubscribe`:

    An optional callback for when a subscription is fully canceled (the subscription's `udata` can be freed).

    If missing, Data is directly written to the HTTP connection.

        // callback example:
        void on_unsubscribe(void *udata);

* `match`:

    A callback for pattern matching, as described in [`fio_subscribe`](fio#fio_subscribe).

    Note that only the `FIO_MATCH_GLOB` matching function (or NULL for no pattern matching) is safe to use with the Redis extension.

        // callback example:
        int foo_bar_match_fn(fio_str_info_s pattern, fio_str_info_s channel);
        // type:
        fio_match_fn match;
 
* `udata`:

    Opaque user data.

        // type:
        void *udata;
 

Returns a subscription ID on success and 0 on failure.

#### `http_sse_unsubscribe`

```c
void http_sse_unsubscribe(http_sse_s *sse, uintptr_t subscription);
```

Cancels a subscription and invalidates the subscription object.

### Writing to the EventSource (SSE) Connection

#### `http_sse_write`

```c
int http_sse_write(http_sse_s *sse, struct http_sse_write_args);
#define http_sse_write(sse, ...)                                               \
  http_sse_write((sse), (struct http_sse_write_args){__VA_ARGS__})
```

Writes data to an EventSource (SSE) connection.

The `http_sse_write` function is shadowed by the `http_sse_write` MACRO, which allows the function to accept "named arguments".

In addition to the `sse` argument, the following named arguments can be used:

 
* `id`:

    Sets the `id` event property (optional).

        // type:
        fio_str_info_s id;
 
* `event`:

    Sets the `event` event property (optional).

        // type:
        fio_str_info_s event;
 
* `data`:

    Sets the `data` event property (optional).

        // type:
        fio_str_info_s data;
 
* `retry`:

    Sets the `retry` event property (optional).

        // type:
        fio_str_info_s retry;
 

Event field details can be found on the [Mozilla developer website](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events).

### EventSource (SSE) Helpers

#### `http_sse2uuid`

```c
intptr_t http_sse2uuid(http_sse_s *sse);
```

Get the connection's UUID (for `fio_defer_io_task`, pub/sub, etc').

#### `http_sse_dup`

```c
http_sse_s *http_sse_dup(http_sse_s *sse);
```

Duplicates an SSE handle by reference, remember to http_sse_free.

Returns the same object (increases a reference count, no allocation is made). 

#### `http_sse_free`

```c
void http_sse_free(http_sse_s *sse);
```

Frees an SSE handle by reference (decreases the reference count).

## Miscellaneous 

### Mime-Type helpers

The HTTP extension allows for easy conversion between file extensions and known Mime-Types.

Many known file extensions are registered by the HTTP extension during startup. However, it's also possible to add/register more Mime-Types during the setup stage.

NOTE:

The Mime-Type helpers are designed to allow for concurrent read access. By design, they are **not** thread safe.

It is recommended (and assumed) that all the calls to `http_mimetype_register` are performed during the setup stage (before calling `fio_start`).

#### `http_mimetype_register`

```c
void http_mimetype_register(char *file_ext, size_t file_ext_len,
                            FIOBJ mime_type_str);
```

Registers a Mime-Type to be associated with a file extension, taking ownership of the `mime_type_str` String (use `fiobj_dup` to keep a copy).

File extension names should exclude the dot (`'.'`) marking the beginning of the extension. i.e., use `"jpg"`, `"html"`, etc' (**not** `".jpg"`).

#### `http_mimetype_find`

```c
FIOBJ http_mimetype_find(char *file_ext, size_t file_ext_len);
```

Finds the mime-type associated with the file extension, returning a String on success and FIOBJ_INVALID on failure.

Remember to call `fiobj_free`.

#### `http_mimetype_find2`

```c
FIOBJ http_mimetype_find2(FIOBJ url);
```

Returns the mime-type associated with the URL or the default mime-type for
HTTP.

Remember to call `fiobj_free`.

#### `http_mimetype_clear`

```c
void http_mimetype_clear(void);
```

Clears the Mime-Type registry (it will be empty after this call). 

### Time / Date Helpers

#### `http_gmtime`

```c
struct tm *http_gmtime(time_t timer, struct tm *tmbuf);
```

A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.

### `http_date2rfc7231`

```c
size_t http_date2rfc7231(char *target, struct tm *tmbuf);
```

Writes an RFC 7231 date representation (HTTP date format) to the `target` buffer.

This requires ~32 bytes of space to be available at the target buffer (unless
it's a super funky year, 32 bytes is about 3 more than you need).

Returns the number of bytes actually written.

#### `http_date2str`

Alias for [`http_date2rfc7231`](#http_date2rfc7231).

#### `http_date2rfc2109`

```c
size_t http_date2rfc2109(char *target, struct tm *tmbuf);
```

Writes an RFC 2109 date representation to the `target` buffer. See [`http_date2rfc7231`](#http_date2rfc7231).

#### `http_date2rfc2822`

```c
size_t http_date2rfc2822(char *target, struct tm *tmbuf);
```

Writes an RFC 2822 date representation to the `target` buffer. See [`http_date2rfc7231`](#http_date2rfc7231).

#### `http_time2str`

```c
size_t http_time2str(char *target, const time_t t);
```

Prints Unix time to a HTTP time formatted string.

This variation implements cached results for faster processing, at the price of a less accurate string.

### URL String Decoding

#### `http_decode_url_unsafe`

```c
ssize_t http_decode_url_unsafe(char *dest, const char *url_data);
```

Decodes a URL encoded string, **no** buffer overflow protection. 

#### `http_decode_url`

```c
ssize_t http_decode_url(char *dest, const char *url_data, size_t length);
```

Decodes a URL encoded string (query / form data), **no** buffer overflow protection.

#### `http_decode_path_unsafe`

```c
ssize_t http_decode_path_unsafe(char *dest, const char *url_data);
```

Decodes the "path" part of a request, **no** buffer overflow protection. 

#### `http_decode_path`

```c
ssize_t http_decode_path(char *dest, const char *url_data, size_t length);
```

Decodes the "path" part of an HTTP request, no buffer overflow protection.

### Commonly Used Header Constants

Some headers are so commonly used, that the HTTP extension pre-allocates memory and objects to represent these headers.

Avoid freeing these headers, as the HTTP extension expects them to remain allocated until the application quits.

#### `HTTP_HEADER_ACCEPT`

```c
extern FIOBJ HTTP_HEADER_ACCEPT;
```

Represents the HTTP Header `"Accept"`.

#### `HTTP_HEADER_CACHE_CONTROL`

```c
extern FIOBJ HTTP_HEADER_CACHE_CONTROL;
```

Represents the HTTP Header `"Cache-Control"`.

#### `HTTP_HEADER_CONNECTION`

```c
extern FIOBJ HTTP_HEADER_CONNECTION;
```

Represents the HTTP Header `"Connection"`.

#### `HTTP_HEADER_CONTENT_ENCODING`

```c
extern FIOBJ HTTP_HEADER_CONTENT_ENCODING;
```

Represents the HTTP Header `"Content-Encoding"`.

#### `HTTP_HEADER_CONTENT_LENGTH`

```c
extern FIOBJ HTTP_HEADER_CONTENT_LENGTH;
```

Represents the HTTP Header `"Content-Length"`.

#### `HTTP_HEADER_CONTENT_RANGE`

```c
extern FIOBJ HTTP_HEADER_CONTENT_RANGE;
```

Represents the HTTP Header `"Content-Range"`.

#### `HTTP_HEADER_CONTENT_TYPE`

```c
extern FIOBJ HTTP_HEADER_CONTENT_TYPE;
```

Represents the HTTP Header `"Content-Type"`.

#### `HTTP_HEADER_COOKIE`

```c
extern FIOBJ HTTP_HEADER_COOKIE;
```

Represents the HTTP Header `"Cookie"`.

#### `HTTP_HEADER_DATE`

```c
extern FIOBJ HTTP_HEADER_DATE;
```

Represents the HTTP Header `"Date"`.

#### `HTTP_HEADER_ETAG`

```c
extern FIOBJ HTTP_HEADER_ETAG;
```

Represents the HTTP Header `"Etag"`.

#### `HTTP_HEADER_HOST`

```c
extern FIOBJ HTTP_HEADER_HOST;
```

Represents the HTTP Header `"Host"`.

#### `HTTP_HEADER_LAST_MODIFIED`

```c
extern FIOBJ HTTP_HEADER_LAST_MODIFIED;
```

Represents the HTTP Header `"Last-Modified"`.

#### `HTTP_HEADER_ORIGIN`

```c
extern FIOBJ HTTP_HEADER_ORIGIN;
```

Represents the HTTP Header `"Origin"`.

#### `HTTP_HEADER_SET_COOKIE`

```c
extern FIOBJ HTTP_HEADER_SET_COOKIE;
```

Represents the HTTP Header `"Set-Cookie"`.

#### `HTTP_HEADER_UPGRADE`

```c
extern FIOBJ HTTP_HEADER_UPGRADE;
```

Represents the HTTP Header `"Upgrade"`.

### Compile Time Settings

#### `HTTP_BUSY_UNLESS_HAS_FDS`

```c
#define HTTP_BUSY_UNLESS_HAS_FDS 64
```

When a new connection is accepted, it will be immediately declined with a 503 service unavailable (server busy) response unless the following number of file descriptors is available.

#### `HTTP_DEFAULT_BODY_LIMIT`

```c
#define HTTP_DEFAULT_BODY_LIMIT (1024 * 1024 * 50)
```

The default limit on HTTP message length (in bytes). A different limit can be set during runtime as part of the `http_listen` function call.

#### `HTTP_MAX_HEADER_COUNT`

```c
#define HTTP_MAX_HEADER_COUNT 128
```

The maximum (hard coded) number of headers per HTTP request, after which the request is considered malicious and the connection is abruptly closed.

#### `HTTP_MAX_HEADER_LENGTH`

```c
#define HTTP_MAX_HEADER_LENGTH 8192
```

the default maximum length for a single header line 
