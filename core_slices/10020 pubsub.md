## Pub/Sub Services

facil.io supports a [Publish–Subscribe Pattern](https://en.wikipedia.org/wiki/Publish–subscribe_pattern) API which can be used for Inter Process Communication (IPC), messaging, horizontal scaling and similar use-cases.

### Subscription Control


#### `fio_subscribe`

```c
subscription_s *fio_subscribe(subscribe_args_s args);
/** MACRO enabling named arguments. */
#define fio_subscribe(...) fio_subscribe((subscribe_args_s){__VA_ARGS__})
```

This function subscribes to either a numerical "filter" or a named channel (but not both).

The `fio_subscribe` function is shadowed by the `fio_subscribe` MACRO, which allows the function to accept "named arguments", as shown in the following example:

```c
static void my_message_handler(fio_msg_s *msg); 
//...
subscription_s * s = fio_subscribe(.channel = {.data="name", .len = 4},
                                   .on_message = my_message_handler);
```


The function accepts the following named arguments:

* `uuid`

    If `uuid` is set, the subscription will be attached (and owned) by a connection.

    The function will return `NULL` since the subscription ownership will be transferred to the connection's environment instead of returned to the caller.

    The `on_message` callback will be called within a connection's task lock (`FIO_PR_LOCK_TASK`).

    If `uuid` is `<= 0`, the subscription will be considered as a global subscription, owned by the calling function, as if `uuid` wasn't set. A pointer to the subscription will be returned to the calling function.

        // type:
        intptr_t uuid;

* `filter`:

    If `filter` is set, all messages that match the filter's numerical value will be forwarded to the subscription's callback.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by filter.

    **Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks (both subjects are covered later on).

        // type:
        int32_t filter;

* `channel`:

    If `filter == 0` (or unset), than the subscription will be made using `channel` binary name matching. Note that and empty string (`NULL` pointer and `0` length) is a valid channel name.

    Subscriptions can either require to be matched by filter or matched by channel. This will match the subscription by channel (only messages with no `filter` and the same `channel` value will be forwarded to the subscription).

        // type:
        fio_str_info_s channel;


* `is_pattern`:

    If the optional `is_pattern` is set (true), pattern matching will be used as an alternative to exact channel name matching.

    The pattern matching function used for all pattern matching can be set using the global `FIO_PUBSUB_PATTERN_MATCH` function pointer. By default that pointer points to the `fio_glob_match` function, that provides binary glob matching (not UTF-8 matching), which matches the **Redis** pattern matching logic.

    This is significantly slower, as no Hash Map can be used to locate a match - each message published to a channel will be tested for a match against each pattern.

* `on_message`:

    The callback will be called for each message forwarded to the subscription.

        // callback example:
        void on_message(fio_msg_s *msg);

* `on_unsubscribe`:

    The callback will be called once the subscription is canceled, allowing it's resources to be freed.

        // callback example:
        void (*on_unsubscribe)(void *udata1, void *udata2);

* `udata1` and `udata2`:

    These are the opaque user data pointers passed to `on_message` and `on_unsubscribe`.

        // type:
        void *udata1;
        void *udata2;


The function returns a pointer to the opaque subscription type `subscription_s`.

On error, `NULL` will be returned and the `on_unsubscribe` callback will be called.

The `on_message` should accept a pointer to the `fio_msg_s` type:

```c
typedef struct fio_msg_s {
 int32_t filter;
 fio_str_info_s channel;
 intptr_t uuid;
 fio_protocol_s * pr;
 fio_str_info_s msg;
 void *udata1;
 void *udata2;
 uint8_t is_json;
} fio_msg_s;
```

* `filter` is the numerical filter (if any) used in the `fio_subscribe` and `fio_publish` functions. Negative values are reserved and 0 == channel name matching.

* `channel` is an **immutable** binary string containing the channel name given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details. Editing the string will cause undefined behavior.

* `uuid` is the connection's identifier (if any) to which the subscription belongs.

  A connection `uuid` 0 marks an un-bound (non-connection related) subscription.

* `pr` is the connection's protocol (if any).

  If the subscription is bound to a connection, the protocol will be locked using a task lock and will be available using this pointer.

* `msg` is an immutable binary string containing the message data given to `fio_publish`. See the [`fio_str_info_s` return value](`#the-fio_str_info_s-return-value`) for details. Editing the string will cause undefined behavior.

* `is_json` is a binary flag (1 or 0) that marks the message as JSON. This is the flag passed as passed to the `fio_publish` function.

* `udata1` and `udata2` are the opaque user data pointers passed to `fio_subscribe` during the subscription.

**Note (1)**: if a subscription object is no longer required, i.e., if `fio_unsubscribe` will only be called once a connection was closed or once facil.io is shutting down, consider using [`fio_uuid_link`](#fio_uuid_link) or [`fio_state_callback_add`](#fio_state_callback_add) to control the subscription's lifetime.


**Note (2)**: passing protocol object pointers to the `udata` is not safe, since protocol objects might be destroyed or invalidated due to either network events (socket closure) or internal changes (i.e., `fio_attach` being called). The preferred way is to add the `uuid` to the `udata` field and call [`fio_protocol_try_lock`](#fio_protocol_try_lock) to access the protocol object.

#### `fio_subscription_channel`

```c
fio_str_info_s fio_subscription_channel(subscription_s *subscription);
```

This helper returns a temporary String with the subscription's channel (or a binary
string representing the filter).

To keep the string beyond the lifetime of the subscription, copy the string.

#### `fio_message_defer`

```c
void fio_message_defer(fio_msg_s *msg);
```

Defers the subscription's callback handler, so the subscription will be called again for the same message.

A classic use case allows facil.io to handle other events while waiting on a lock / mutex to become available in a multi-threaded environment.

#### `fio_unsubscribe`

```c
void fio_unsubscribe(subscription_s *subscription);
```

Cancels an existing subscription.

This function will be automatically deferred if a subscription task is running on a different thread, which might delay the effects of the function.

The subscription task won't be called again once the function completes it's task.

#### `fio_unsubscribe_uuid`

```c
void fio_unsubscribe_uuid(subscribe_args_s args);
/** MACRO enabling named arguments. */
#define fio_unsubscribe_uuid(...)                                              \
  fio_unsubscribe_uuid((subscribe_args_s){__VA_ARGS__})
```

Cancels an existing subscriptions that was bound to a connection's `uuid`. See `fio_subscribe` and `fio_unsubscribe` for more details.

Accepts the same arguments as `fio_subscribe`, except the `udata` and callback details are ignored (no need to provide `udata` or callback details).

The `fio_unsubscribe_uuid` function is shadowed by the `fio_unsubscribe_uuid` macro, which allows the function to accept the following "named arguments" (see `fio_subscribe` example):

* `uuid`

    Use this function only if a `uuid` value was provided to the `fio_subscribe` function. The same value should be provided here, to make sure the correct subscription is found and removed.

* `filter`:

    If a `filter` value was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

* `channel`:

    If a `channel` name was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

* `match`:

    If an optional `match` callback was provided to the `fio_subscribe` function, it should be provided here as well, to make sure the correct subscription is found and removed.

### Publishing messages

#### `fio_publish`

```c
void fio_publish(fio_publish_args_s args);
```

This function publishes a message to either a numerical "filter" or a named channel (but not both).

The message can be a `NULL` or an empty message.

The `fio_publish` function is shadowed by the `fio_publish` MACRO, which allows the function to accept "named arguments", as shown in the following example:

```c
fio_publish(.channel = {.data="name", .len = 4},
            .message = {.data = "foo", .len = 3});
```

The function accepts the following named arguments:


* `filter`:

    If `filter` is set, all messages that match the filter's numerical value will be forwarded to the subscription's callback.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by filter.

        // type:
        int32_t filter;

* `channel`:

    If `filter == 0` (or unset), than the subscription will be made using `channel` binary name matching. Note that and empty string (NULL pointer and 0 length) is a valid channel name.

    Subscriptions can either require a match by filter or match by channel. This will match the subscription by channel (only messages with no `filter` will be received).

        // type:
        fio_str_info_s channel;


* `message`:

    The message data to be sent.

        // type:
        fio_str_info_s message;

* `is_json`:

    A flag indicating if the message is JSON data or binary/text.

        // type:
        uint8_t is_json;


* `engine`:

    The pub/sub engine that should be used to forward this message (see later). Defaults to `FIO_PUBSUB_DEFAULT` (or `FIO_PUBSUB_CLUSTER`).

    Pub/Sub engines dictate the behavior of the pub/sub instructions. The possible internal pub/sub engines are:

    * `FIO_PUBSUB_CLUSTER` - used to publish the message to all clients in the network cluster. Unless `fio_pubsub_clusterfy` is called (currently unimplemented), acts the same as `FIO_PUBSUB_LOCAL`).

    * `FIO_PUBSUB_LOCAL` - used to publish the message to all the worker processes and the root / master process.

    * `FIO_PUBSUB_PROCESS` - used to publish the message only within the current process.

    * `FIO_PUBSUB_SIBLINGS` - used to publish the message except within the current process.

    * `FIO_PUBSUB_ROOT` - used to publish the message exclusively to the root / master process.
    
    The default pub/sub can be changed globally by assigning a new default engine to the `FIO_PUBSUB_DEFAULT` global variable.

        // type:
        fio_pubsub_engine_s const *engine;
        // default engine:
        extern fio_pubsub_engine_s *FIO_PUBSUB_DEFAULT;

### Pub/Sub Message MiddleWare Meta-Data

It's possible to attach meta-data to facil.io pub/sub messages before they are published.

This is only available for named channels (filter == 0).

This allows, for example, messages to be encoded as network packets for
outgoing protocols (i.e., encoding for WebSocket transmissions), improving
performance in large network based broadcasting.

**Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

#### `FIO_PUBSUB_METADATA_LIMIT` macro

```c
#define FIO_PUBSUB_METADATA_LIMIT 4
```

This compilation macro sets the number of different metadata callbacks that can be attached.

Larger numbers will effect performance.

The default value should be enough for the following metadata objects:

- WebSocket server headers.

- WebSocket client (header + masked message copy).

- EventSource (SSE) encoded named channel and message.


#### `fio_message_metadata`

```c
void *fio_message_metadata(fio_msg_s *msg, int id);;
```

Finds the message's meta-data by the meta-data's type ID. Returns the data or NULL.

#### `fio_message_metadata_add`

```c
// The function:
int fio_message_metadata_add(fio_msg_metadata_fn builder,
                             void (*cleanup)(void *))
// The matadata builder type:
typedef void *(*fio_msg_metadata_fn)(fio_str_info_s ch,
                                     fio_str_info_s msg,
                                     uint8_t is_json);
// Example matadata builder
void * foo_metadata(fio_str_info_s ch,
                    fio_str_info_s msg,
                    uint8_t is_json);

// Example cleanup - called to free the data
void foo_free(void * metadata);

```

It's possible to attach metadata to facil.io named messages (`filter == 0`) before they are published.

This allows, for example, messages to be encoded as network packets for outgoing protocols (i.e., encoding for WebSocket transmissions), improving performance in large network based broadcasting.

Up to `FIO_PUBSUB_METADATA_LIMIT` metadata callbacks can be attached.

The callback should return a `void *` pointer.

To remove a callback, call `fio_message_metadata_remove` with the returned value (the metadata ID).

The cluster messaging system allows some messages to be flagged as JSON and this flag is available to the metadata callback.

Returns a positive number on success (the metadata ID) or zero (0) on failure.

#### `fio_message_metadata_remove`

```c
void fio_message_metadata_remove(int id);
```

Removed the metadata callback.

Removal might be delayed if live metatdata exists.

Removal only occurs if `fio_message_metadata_remove` was called the same number of times `fio_message_metadata_add` was called.

### External Pub/Sub Services

facil.io can be linked with external Pub/Sub services using "engines" (`fio_pubsub_engine_s`).

Pub/Sub engines dictate the behavior of the pub/sub instructions. 

This allows for an application to connect to a service such as Redis or NATs for horizontal pub/sub scaling.

A [Redis engine](redis) is bundled as part of the facio.io extensions but isn't part of the core library.

The default engine can be set using the `FIO_PUBSUB_DEFAULT` global variable. It's initial default is `FIO_PUBSUB_CLUSTER` (see [`fio_publish`](#fio_publish)).

**Note**: filter based messages are considered internal. They aren't shared with external pub/sub services (such as Redis) and they are ignored by meta-data callbacks.

Engines MUST provide the listed function pointers and should be attached using the `fio_pubsub_attach` function.

Engines should disconnect / detach, before being destroyed, by using the `fio_pubsub_detach` function.

When an engine received a message to publish, it should call the `fio_publish` function with the engine to which the message is forwarded. i.e.:

```c
fio_publish(
    .engine = FIO_PROCESS_ENGINE,
    .channel = {0, 4, "name"},
    .message = {0, 4, "data"} );
```


#### `fio_pubsub_attach`

```c
void fio_pubsub_attach(fio_pubsub_engine_s *engine);
```

Attaches an engine, so it's callbacks can be called by facil.io.

The `subscribe` callback will be called for every existing channel.

The engine type defines the following callback:

* `subscribe` - Should subscribe channel. Failures are ignored.

        void subscribe(const fio_pubsub_engine_s *eng,
                       fio_str_info_s channel,
                       fio_match_fn match);

* `unsubscribe` - Should unsubscribe channel. Failures are ignored.

        void unsubscribe(const fio_pubsub_engine_s *eng,
                         fio_str_info_s channel,
                         fio_match_fn match);

* `publish` - Should publish a message through the engine. Failures are ignored.

        int publish(const fio_pubsub_engine_s *eng,
                    fio_str_info_s channel,
                    fio_str_info_s msg, uint8_t is_json);

**Note**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


**IMPORTANT**: The `subscribe` and `unsubscribe` callbacks are called from within an internal lock. They MUST NEVER call pub/sub functions except by exiting the lock using `fio_defer`.

#### `fio_pubsub_detach`

```c
void fio_pubsub_detach(fio_pubsub_engine_s *engine);
```

Detaches an engine, so it could be safely destroyed.

#### `fio_pubsub_reattach`

```c
void fio_pubsub_reattach(fio_pubsub_engine_s *eng);
```

Engines can ask facil.io to call the `subscribe` callback for all active channels.

This allows engines that lost their connection to their Pub/Sub service to resubscribe all the currently active channels with the new connection.

CAUTION: This is an evented task... try not to free the engine's memory while re-subscriptions are under way...

**Note**: the root (master) process will call `subscribe` for any channel **in any process**, while all the other processes will call `subscribe` only for their own channels. This allows engines to use the root (master) process as an exclusive subscription process.


**IMPORTANT**: The `subscribe` and `unsubscribe` callbacks are called from within an internal lock. They MUST NEVER call pub/sub functions except by exiting the lock using `fio_defer`.

#### `fio_pubsub_is_attached`

```c
int fio_pubsub_is_attached(fio_pubsub_engine_s *engine);
```

Returns true (1) if the engine is attached to the system.

-------------------------------------------------------------------------------
