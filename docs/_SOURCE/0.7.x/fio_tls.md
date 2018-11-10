---
title: facil.io - SSL/TLS Functions
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

SSL/TLS provides Transport Layer Security (TLS) for more secure communication.

facil.io attempts to make TLS connections easy by providing a simplified API that abstracts away the underlying TLS library and utilizes the [Read/Write Hooks](fio#lower-level-read-write-close-hooks) to allow TLS connections and unencrypted connections to share the same API (`fio_write`, `fio_read`, etc').

Future support for BearSSL and OpenSSL is planned.

At the moment, TLS connections aren't supported since no libraries are supported.

To use the facil.io TLS API, include the file `fio_tls.h`

### TLS Certificates and Settings

#### `fio_tls_new`

```c
fio_tls_s *fio_tls_new(const char *server_name, const char *private_key_file,
                       const char *public_cert_file);
```

Creates a new SSL/TLS context / settings object with a default certificate (if any).

```c
fio_tls_s * tls = fio_tls_new("www.example.com",
                              "./ssl/private_key.key",
                              "./ssl/public_key.crt");
```
`NULL` values can be used to create an anonymous (unverified) context / settings object.

```c
fio_tls_s * tls = fio_tls_new("www.example.com", NULL, NULL);
```

`fio_tls_s *` is an opaque type used as a handle for the SSL/TLS functions. It shouldn't be directly accessed.

Remember to call `fio_tls_destroy` once the `fio_tls_s` object is no longer in use.

#### `fio_tls_destroy`

```c
void fio_tls_destroy(fio_tls_s *tls);
```

Destroys the SSL/TLS context / settings object and frees any related resources / memory.


#### `fio_tls_cert_add`

```c
void fio_tls_cert_add(fio_tls_s *, const char *server_name,
                      const char *private_key_file,
                      const char *public_cert_file);
```

Adds a certificate a new SSL/TLS context / settings object (SNI support).

```c
fio_tls_cert_add(tls, "www.example.com",
                      "./ssl/private_key.key",
                      "./ssl/public_key.crt" );
```

#### `fio_tls_proto_add`

```c
void fio_tls_proto_add(fio_tls_s *, const char *protocol_name,
                       void (*callback)(intptr_t uuid, void *udata));
```

Adds an ALPN protocol callback to the SSL/TLS context.

The first protocol added will act as the default protocol to be selected when the client does not support or specify ALPN protocol selection.

### TLS Connection Establishment

#### `fio_tls_accept`

```c
void fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata);
```

Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified context / settings object.

The `uuid` should be a socket UUID that is already connected to a peer (i.e., the result of `fio_accept`).

The `udata` is an opaque user data pointer that is passed along to the protocol selected (if any protocols were added using `fio_tls_proto_add`).


#### `fio_tls_connect`

```c
void fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata);
```


Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified context / settings object.

The `uuid` should be a socket UUID that is already connected to a peer (i.e., one received by a `fio_connect` specified callback `on_connect`).

The `udata` is an opaque user data pointer that is passed along to the protocol selected (if any protocols were added using `fio_tls_proto_add`).


