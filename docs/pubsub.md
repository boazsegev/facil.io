---
title: facil.io - Native Pub/Sub
---
# {{ page.title }}

The facil.io framework offers a native Pub/Sub implementation which can be found in the "services" folder.

This Pub/Sub implementation covers the whole process cluster and it can be easily scaled by using Redis (which isn't required except for horizontal scaling).

## Connection based vs. non-connection based Pub/Sub

Pub/Sub can be implemented both as a connection based subscription, where a connection's `on_close` callback unsubscribes from any existing subscriptions.

In these cases, messages can be either forwarded directly to the client or handled by a server-side callback.

An example for this approach can be seen in the [chatroom example](/index.md#an-easy-chatroom-example) using the `websocket_subscribe` function. The same function could have been used with a server-side callback.

A non-connection based Pub/Sub allows a local event to fire whenever a message arrives.

This event allows the server to react to certain channels, allowing remotely published events (by clients or through Redis) to invoke server routines.

This approach uses the `pubsub_subscribe` function directly rather than a connection related function.

## Details and Limitations:

* facil.io doesn't use a Hash table for the Pub/Sub channels, it uses a [4 bit trie](https://en.wikipedia.org/wiki/Trie).

    The cost is higher memory consumption per channel and a limitation of 1024 bytes per channel name (shorter names are better).

    The bonus is high lookup times, zero chance of channel conflicts and an optimized preference for shared prefix channels (i.e. "user:1", "user:2"...).

    Another added bonus is pattern publishing (is addition to pattern subscriptions) which isn't available when using Redis (since Redis doesn't support this feature).

* The Redis client engine does *not* support multiple databases. This is both becasue [database scoping is ignored by Redis during pub/sub](https://redis.io/topics/pubsub#database-amp-scoping) and because [Redis Cluster doesn't support multiple databases](https://redis.io/topics/cluster-spec). This indicated that multiple database support just isn't worth the extra effort.

* The Redis client engine will use two Redis connections **per process** (one for subscriptions and the other for publishing and commands). Both connections will be automatically re-established if timeouts or errors occur.

    A future implementation might the process cluster connections to a single pair (instead of a pair per process), however, this requires rewriting the engine so it will forward channel subscription management data to the main process.
