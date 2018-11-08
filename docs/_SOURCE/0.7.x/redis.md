---
title: facil.io - The Redis Extension
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

facil.io includes a Redis Pub/Sub extension that makes it easy to scale pub/sub applications horizontally.

The extension was written to minimize Redis connections and load, allowing more clusters to connect to a Redis server.

This requires that each facil.io cluster consume less resources - only two connections per cluster instead of two connections per process (as might be common on other implementations).

To use the facil.io Redis extension API, include the file `redis_engine.h`

## Connecting facil.io to Redis

By using the [Core Library's External Pub/Sub Services API](fio#external-pub-sub-services), it's easy to connect an application to a Redis Server. i.e.:

```c
fio_pubsub_engine_s *r = redis_engine_create(.address.data = "localhost");
if (!r){
    perror("Couldn't initialize Redis");
    exit(-1);
}
fio_state_callback_add(FIO_CALL_AT_EXIT,
                           (void (*)(void *))redis_engine_destroy, r);
FIO_PUBSUB_DEFAULT = r;
```

### Connection Management

#### `redis_engine_create`

```c
fio_pubsub_engine_s *redis_engine_create(struct redis_engine_create_args);
#define redis_engine_create(...)                                               \
  redis_engine_create((struct redis_engine_create_args){__VA_ARGS__})
```

Creates and attaches a Redis "engine", which connects to an external Redis service and manages Pub/Sub communication with that service.

The `redis_engine_create` function is shadowed by the `redis_engine_create` MACRO, which allows the function to accept "named arguments", as shown in the above example.

Th possible named arguments for the `redis_engine_create` function call are:

* `address`

    Redis server's address, defaults to `"localhost"`.

        fio_str_info_s address;
* `port`
 
    Redis server's port, defaults to `"6379"`.

        fio_str_info_s port;

* `auth`

    Redis server's password, if any. */

        fio_str_info_s auth;

* `ping_interval`

    A `ping` will be sent every `ping_interval` interval or inactivity. */

        uint8_t ping_interval;

The fio_fio_pubsub_engine_s is active only after facil.io starts running.

A `ping` will be sent every `ping_interval` interval or inactivity. The default value (0) will fallback to facil.io's maximum time of inactivity (5 minutes) before polling on the connection's protocol.

**Note**: The Redis engine can only be initialized *before* facil.io starts up, during the setup stage within the root process. Attempting to initialize a Redis engine while the application is running might not work (and requires a hot restart for any child processes).

#### `redis_engine_destroy`

```c
void redis_engine_destroy(fio_pubsub_engine_s *engine);
```

Detaches and destroys a Redis Pub/Sub engine from facil.io.

### Sending Redis Database Commands

#### `redis_engine_send`

```c
intptr_t redis_engine_send(fio_pubsub_engine_s *engine,
                           FIOBJ command,
                           void (*callback)(fio_pubsub_engine_s *e, FIOBJ reply,
                                            void *udata),
                           void *udata);
```

Sends a Redis command through the engine's connection.

The response will be sent back using the optional callback. `udata` is passed along untouched.

The message will be resent on network failures, until a response validates the fact that the command was sent (or the engine is destroyed).

**Note**: Avoid calling Pub/Sub commands using this function, as it could violate the Redis connection's protocol and could prevent the communication from resuming.
 
**Note2**: The Redis extension is designed for resource conservation, not speed. This might not be the best way to use Redis as a database and should be considered available for occasional use rather than heavy use.


### The RESP parser

The Redis extension includes a RESP parser authored as a single file library (`resp_parser.h`), which can be used independently from the rest of facil.io.

The parser was originally coded in order keep the licensing scheme simple and avoid a debate about MIT licensing vs. BSD-3-clause requirements.

#### `resp_parse`

```c
static size_t resp_parse(resp_parser_s *parser,
                         const void *buffer,
                         size_t length);
```

Parses a RESP content stream, invoking any RESP related callbacks as required.

The `resp_parse` will review the data and call any of the following callback, depending on the RESP protocol's state:

* `resp_on_message` - a local static callback, called when the RESP message was completely parsed.

        static int resp_on_message(resp_parser_s *parser);

* `resp_on_number` - a local static callback, called when a Number object is parsed.

        static int resp_on_number(resp_parser_s *parser, int64_t num);
* `resp_on_okay` - a local static callback, called when a OK message is received.

        static int resp_on_okay(resp_parser_s *parser);
* `resp_on_null` - a local static callback, called when NULL is received.

        static int resp_on_null(resp_parser_s *parser);

* `resp_on_start_string` - a local static callback, called when a String should be allocated.

    `str_len` is the expected number of bytes that will fill the final string object, without any NUL byte marker (the string might be binary).

    If this function returns any value besides 0, parsing is stopped.

        static int resp_on_start_string(resp_parser_s *parser, size_t str_len);

* `resp_on_string_chunk` - a local static callback, called as String objects are streamed. 

        static int resp_on_string_chunk(resp_parser_s *parser, void *data, size_t len);

* `resp_on_end_string` - a local static callback, called when a String object had finished streaming. 

        static int resp_on_end_string(resp_parser_s *parser);

* `resp_on_err_msg` - a local static callback, called an error message is received. 

        static int resp_on_err_msg(resp_parser_s *parser, void *data, size_t len);


* a local static callback, called when an Array should be allocated.

    `array_len` is the expected number of objects that will fill the Array object.
    There's no `resp_on_end_array` callback since the RESP protocol, simply count back from `array_len`.

    However, be aware that a client/server might send nested Arrays in some rare cases.

    If this function returns any value besides 0, parsing is stopped.

        static int resp_on_start_array(resp_parser_s *parser, size_t array_len);

* `resp_on_parser_error` - a local static callback, called when a parser / protocol error occurs. 

        static int resp_on_parser_error(resp_parser_s *parser);



Returns the number of bytes to be resent. i.e., for a return value 5, the last 5 bytes in the buffer need to be resent to the parser.

Data consumed can be safely overwritten (assuming it isn't used by the parsing implementation).

**NOTE**:

The `resp_parser_s` type should be considered opaque, without any user related data.

To attach data to the parser, include the parser within a container `struct`, i.e.:

```c
typedef struct {
  /* place parser at top for pointer casting simplicity*/
  resp_parser_s parser;
  /* place user data after the parser */
  void * udata;
} my_parser_s;
```
 
