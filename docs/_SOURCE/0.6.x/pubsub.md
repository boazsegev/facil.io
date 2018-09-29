---
title: facil.io - Native Pub/Sub
sidebar: 0.6.x/_sidebar.md
---
# {{{title}}}

The facil.io framework offers a native Pub/Sub implementation which can be found in the "services" folder.

This Pub/Sub implementation covers the whole process cluster and it can be easily scaled by using Redis (which isn't required except for horizontal scaling).

## Connection based vs. non-connection based Pub/Sub

The Websockets extension allows for connection based subscription in adition to the regular Pub/Sub subscriptions.

When using connection based subscription, the connection's `on_close` callback will automatically unsubscribes from any existing subscriptions.

This allows messages to be be either forwarded directly to the client or handled by a server-side callback.

An example for this approach can be seen in the [chatroom example](index.md#an-easy-chatroom-example) using the `websocket_subscribe` function. The same function could have been used with a server-side callback.

A non-connection based Pub/Sub allows a local event to fire whenever a message arrives.

This event allows the server to react to certain channels, allowing remotely published events (by clients or through Redis) to invoke server routines.

This approach uses the `pubsub_subscribe` function directly rather than a connection related function.

## Connecting Redis as a Pub/Sub Service

A built-in Redis engine is bundled with facil.io (it can be removed safely by deleting the folder).

It's simple to add Redis for multi-machine clustering by creating a Redis engine and setting it as the default engine.

i.e.:

```c
int main(void) {
  PUBSUB_DEFAULT_ENGINE =
      redis_engine_create(/** Redis server's address */
                              .address = "localhost", .ping_interval = 2, );
  /* code */
  facil_run(/* settings */);
  redis_engine_destroy(PUBSUB_DEFAULT_ENGINE);
  return 0;
}
```

## Connecting a custom Pub/Sub Service

It's possible. The documentation is available within the header file `pubsub.h` (using `pubsub_engine_s`).

Notice, that the `on_subscribe` will be called for all the channels in the process cluster, so it's possible to connect to the external pub/sub service from a single process (minimizing network traffic) and to publish to the whole cluster.

Also, be aware that messages arrive **at least** once. When using pattern matching, messages might arrive far more than once (i.e., if the service sends two copies, one for the channel and one for the pattern, the cluster might receive four copies, due to internal pattern matching).

Also, see known issues for lost messages when connection is disrupted (this might be pub/sub service specific).

## Details and Limitations:

* facil.io v.0.6.x uses a Hash table for the Pub/Sub channels (replacing the [4 bit trie](https://en.wikipedia.org/wiki/Trie) approach from facil.io v.0.5.x).

    This means that hash collision (using SipHash) might cause excessive `memcmp` calls.

    Pattern publishing (which isn't supported by Redis and shouldn't be confused with pattern subscriptions) was deprecated due to the change in data structure.

* The Redis client engine does *not* support multiple databases. This is both becasue [database scoping is ignored by Redis during pub/sub](https://redis.io/topics/pubsub#database-amp-scoping) and because [Redis Cluster doesn't support multiple databases](https://redis.io/topics/cluster-spec). This indicated that multiple database support just isn't worth the extra effort.

   It's possible to manually send commands to the Redis client and overcome this issue, but disconnections will require resetting the database.

* The Redis client engine will use a single Redis connection **per process** (for publishing) + a single connection **per cluster** (for subscriptions).

    Note that **outgoing** messages are guarantied to be published **at least** once (automatic network error recovery)... however, **incoming** messages might be lost due to network connectivity issues (nothing I can do about this, it's on the Redis server side).
