# Redis Connectivity and Pub/Sub Extension

Here I'll introduce the following:

* The `redis_engine` Pub/Sub Redis client (a short getting started).

* Te RESP parser / formatter (which can be used separately from facil.io).

## Getting started with the `redis_engine`

The facil.io framework contains an optimistic asynchronous Redis client that synchronizes the built in Pub/Sub system with an external Pub/Sub Redis server, allowing facil.io's Pub/Sub to be extended beyond a single machine or process cluster.

The `redis_engine` can be used to send commands to a Redis server, leveraging the existing Redis "subscribe" client in order to send additional asynchronous commands.

When sending commands, it's important that the client communicate with the Redis server using the RESP protocol.

This entails the creation and destruction of RESP objects to be used as a translation layer between the C objects and the network protocol.

The reply will be given asynchronously using a callback that will receive a RESP object that can be easily converted into C data.

The following (non-running and extra verbose) example demonstrates just about the whole of the concepts involved in the communication between facil.io and Redis:

```c
#include "redis_engine.h"

// ... from within a function, maybe within main:
void start_redis(void) {
  PUBSUB_DEFAULT_ENGINE = redis_engine_create(.address = "localhost",
                                              .port = "6379",
                                              .password = "MakeThisSecure");
}
// We'll probably want to use a callback to handle our command's reply
void on_redis_echo(pubsub_engine_s *e, resp_object_s *reply, void *udata);

// within a facil.io event handler, while facil.io is running:
void send_echo(void) {
  // We'll initialize an empty RESP array for our command.
  resp_object_s * cmd = resp_arr2obj(2, NULL);
  // We'll populate the Array with the command String and it's arguments
  resp_obj2arr(cmd)->array[0] = resp_str2obj("ECHO", 4);
  resp_obj2arr(cmd)->array[1] = resp_str2obj("Hello Redis!", 12);
  // We'll send the command, passing a callback that can handle it.
  // We can also pass a `void *` pointer with any data we want.
  redis_engine_send(PUBSUB_DEFAULT_ENGINE, resp_object_s *data,
                             on_redis_echo,(intptr_t)42);
  // unless we keep the data for later use, we need to free the objects
  // this will free the nested objects as well
  resp_free_object(cmd);
}

void on_redis_echo(pubsub_engine_s *r, resp_object_s *reply, void *udata) {
  // The `udata` was simply passed along.                   
  if((intptr_t)data == 42)
    printf("42 is the meaning of life :-)\n");
  else
    printf("Who are you and what did you do with our fish?\n");
  // Handle the reply
  switch (reply->type) {
    case RESP_ERR:
      fprintf(stderr, "ERROR: (Redis) error: %s\n", resp_obj2str(reply)->string);
      break;
    case RESP_STRING:
      fprintf(stderr, "Redis: %s\n", resp_obj2str(reply)->string);
      break;
    case RESP_ARRAY:
      for (size_t i = 0; i < resp_obj2arr(reply)->len; i++) {
        if (resp_obj2arr(reply)->array[i]->type == RESP_ERR ||
            resp_obj2arr(reply)->array[i]->type == RESP_STRING)
          fprintf(stderr, "Redis: %s\n",
                  resp_obj2str(resp_obj2arr(reply)->array[i])->string);
      }
      fprintf(stderr, "Redis: %s\n", resp_obj2str(reply));
      break;
    default:
      fprintf(stderr, "ERROR: (Redis) Unexpected reply type.\n");
      break;
  }
}
```

## The RESP parser / formatter

facil.io favors a decoupled modular design (sometimes at the expense of performance).

This is why, at the heart of the Redis Pub/Sub engine is a RESP parser and formatter that is a completely stand-alone package and can be used outside of facil.io (MIT licensed).

### A RESP parser example

The RESP parser is a state machine that copies the data it is fed, which means that network buffers can be recycled without effecting the parser.

The parser also supports pipelining requests, which means it might stop parsing midway and inform us that a message was received.

The RESP parser is fully documented in the `resp.h` source code comments.

Here's a short example that reads data from an `fd` and forwards any messages to a callback.

```c
// // Remember to create the parser first
// resp_parser_pt p = resp_parser_new();
// ... process
// resp_parser_destroy(p);

void read_resp(resp_parser_pt parser, int fd, void (*callback)(resp_object_s *)) {
  resp_object_s * msg;
  uint8_t buffer[2048];
  ssize_t limit;
  size_t i;

  while(1) {
    limit = read(fd, buffer, 2048);
    if(limit <= 0)
      return;

    while(limit) {
      i = limit;
      msg = resp_parser_feed(parser, buffer, &i);
      if(i == 0) {
        // an error occurred, no data was consumed by the parser.
        exit(-1);
      }
      limit -= i;
      if(msg){
        if(callback)
          callback(msg);
        resp_free_object(msg);
      }
    }
  }
}

```

### The RESP object types

The types supported by the RESP parser / formatter are:

* `RESP_NULL` : a simple flag object that signifies a NULL / nil object.

* `RESP_OK` : a simple flag object that signifies an OK message.

* `RESP_NUMBER` : an int64_t number.

* `RESP_STRING` : a binary safe String object that contains a string buffer as well as information about it's length.

* `RESP_ERR` : a RESP String sub-type. This is a string object but it's type indicates that the string references an error message.

* `RESP_ARRAY` : an array of RESP objects. Although the library supports nested arrays, they are unsupported by the Redis server and shouldn't be used (except, perhaps, internally).

* `RESP_PUBSUB` : a RESP array sub-type. This is an array object but it's type indicates that it matches the semantics used by Pub/Sub messages.

RESP types can be safely OR'd, meaning that `(RESP_ERR | RESP_STRING)` is TRUE (error is a type of string), while `(RESP_ERR | RESP_NUMBER)` is FALSE (error isn't a number).

#### Handling RESP objects

Each object contains specific properties unique to it's type. The only property shared by all RESP objects is the `type` property.

It's easy to convert between types using the `resp_obj2*` macros.

i.e.

```c
resp_object_s *obj;
if (obj->type | RESP_STRING)
  printf("A RESP string %lu long: %s\n", resp_obj2str(obj)->len,
         resp_obj2str(obj)->string);
else if (obj->type | RESP_ARRAY)
  printf(
      "A RESP array containing %lu items, starting at memory address: %p\n",
      resp_obj2arr(obj)->len, (void *)resp_obj2arr(obj)->array);
```

Since RESP objects might be nested within other REST objects, the function `resp_obj_each` is provided for (hopefully) protected iteration (it protects agains closed loops where objects contain themselves). i.e.:,

```c

int print_object_task(resp_parser_pt p, resp_object_s *obj, void *arg) {
  if (obj->type | RESP_STRING)
    printf("A RESP string %lu long: %s\n", resp_obj2str(obj)->len,
           resp_obj2str(obj)->string);
  else if (obj->type | RESP_ARRAY)
    printf(
        "A RESP array containing %lu items, starting at memory address: %p\n",
        resp_obj2arr(obj)->len, (void * )resp_obj2arr(obj)->array);
  else if (obj->type | RESP_NUMBER)
    printf( "A RESP number: %" PRIi64 ":\n", resp_obj2num(obj)->number);
  else
    printf( "A RESP ... something else.\n");
  return 0;
  // to stop the loop, we can return -1.
}

void print_object(resp_parser_pt p, resp_object_s *obj) {
  resp_obj_each(p, obj, print_object_task, NULL);
}

```

#### Creating and destroying RESP objects

Creating and destroying RESP objects is easy using the `resp_*2obj` functions.

RESP objects are reference counted, so "duplicating" objects (using `resp_dup_object`) does **NOT** create a new copy. Instead, it increases their reference count (along with any array's "children's" reference count).

Similar to the `resp_dup_object` function, the `resp_free_object` function acts on a RESP tree (arrays and all their children, including nested arrays if any).

Destroying a RESP object simply decreases it's reference count (as well as all it's "children's" reference count) and only frees the memory if the object is no longer referenced.

*For this reason, it's best if RESP objects are considered immutable.*

i.e.:

```c
resp_object_s * objects[2];
objects[0] = resp_str2obj("ECHO", 4);
objects[1] = resp_str2obj("Hello Redis!", 12);
resp_object_s * cmd = resp_arr2obj(2, objects);
// this will free the nested objects as well
resp_free_object(cmd);
```

#### Formatting RESP

The RESP objects are transmitted over the network as a stream of bytes.

It's easy convert RESP objects into a stream of bytes using the `resp_format` function. i.e.:

```c
// resp_object_s * obj;
size_t len = 0;
// test for size.
resp_format(parser, NULL, &len, obj);
if (!len) /* something went wrong?!... */
  return -1;
// allocate enough memory
void * buffer = malloc(len);
// write formatted RESP stream to buffer
resp_format(parser, buffer, &len, obj);
```

When formatting the RESP stream, the `parser` object can be NULL. It used to test for RESP protocol extensions and might not always be required.

the `size_t` pointer tells the parser how much space is available in the buffer. Once the function returns, the value contain the actual length (or required length) of the RESP stream. This value might be larger than the available space, indicating more space should be allocated.
