---
title: facil.io - SSL/TLS Functions
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

SSL/TLS provides Transport Layer Security (TLS) for more secure communication.

facil.io attempts to make TLS connections easy by providing a simplified API that abstracts away the underlying TLS library.

Support for OpenSSL >= `1.1.0` is included starting with facil.io version 0.7.0.beta6.

Future support for BearSSL is planned and an example/template is provided under `fio_tls_missing.c`, making it easy to use your own SSL/TLS library.

The implementation leverages the [Read/Write Hooks](fio#lower-level-read-write-close-hooks) to allow TLS connections and unencrypted connections to share the same code-base and API (`fio_write`, `fio_read`, etc').

To use the facil.io TLS API, include the file `fio_tls.h`

### TLS Certificates and Settings

#### `fio_tls_new`

```c
fio_tls_s *fio_tls_new(const char *server_name,
                       const char *public_certificate_file,
                       const char *private_key_file,
                       const char *private_key_password);
```

Creates a new SSL/TLS context / settings object with a default certificate (if any).

```c
fio_tls_s * tls = fio_tls_new("www.example.com",
                              "./ssl/public_key.pem",
                              "./ssl/private_key.pem",
                              NULL);
```

If a server name is provided, than `NULL` values can be used to create an anonymous (unverified) context / settings object.

```c
fio_tls_s * tls = fio_tls_new("www.example.com", NULL, NULL, NULL);
```

If all values are `NULL`, a TLS object will be created without a certificate. This could be used for clients together with `fio_tls_trust`

```c
fio_tls_s * tls = fio_tls_new(NULL, NULL, NULL, NULL);
fio_tls_trust(tls, "google-ca.pem" );
```

`fio_tls_s *` is an opaque type used as a handle for the SSL/TLS functions. It shouldn't be directly accessed.

Remember to call `fio_tls_destroy` once the `fio_tls_s` object is no longer in use.

#### `fio_tls_dup`

```c
void fio_tls_dup(fio_tls_s *tls);
```

Increase the reference count for the TLS object.

Decrease / free with `fio_tls_destroy`.

#### `fio_tls_destroy`

```c
void fio_tls_destroy(fio_tls_s *tls);
```

Destroys the SSL/TLS context / settings object and frees any related resources / memory.


#### `fio_tls_cert_add`

```c
void fio_tls_cert_add(fio_tls_s *, const char *server_name,
                      const char *public_cert_file,
                      const char *private_key_file,
                      const char *private_key_password);
```

Adds a certificate a new SSL/TLS context / settings object (SNI support).

The `private_key_password` can be NULL if the private key PEM file isn't password protected. 

```c
fio_tls_cert_add(tls, "www.example.com",
                      "./ssl/public_key.pem",
                      "./ssl/private_key.pem",
                      NULL);
```

#### `fio_tls_trust`

```c
void fio_tls_trust(fio_tls_s *, const char *public_cert_file);
```

Adds a certificate to the "trust" list, which automatically adds a peer verification requirement.

Note, when the fio_tls_s object is used for server connections, this will limit connections to clients that connect using a trusted certificate.

```c
fio_tls_trust(tls, "google-ca.pem" );
```

#### `fio_tls_alpn_add`

```c
void fio_tls_alpn_add(fio_tls_s *tls,
                       const char *protocol_name,
                       void (*on_selected)(intptr_t uuid,
                                        void *udata_connection,
                                        void *udata_tls),
                       void *udata_tls,
                       void (*on_cleanup)(void *udata_tls));
```


Adds an ALPN protocol callback to the SSL/TLS context.

The first protocol added will act as the default protocol to be selected.

The `on_selected` callback should accept the connection's `uuid`, the user data pointer passed to either `fio_tls_accept` or `fio_tls_connect` (here: `udata_connetcion`) and the user data pointer passed to the `fio_tls_alpn_add` function (`udata_tls`).

The `on_cleanup` callback will be called when the TLS object is destroyed (or `fio_tls_alpn_add` is called again with the same protocol name). The `udata_tls` argument will be passed along, as is, to the callback (if set).

Except for the `tls` and `protocol_name` arguments, all arguments can be NULL.

#### `fio_tls_alpn_count`

```c
uintptr_t fio_tls_alpn_count(fio_tls_s *tls);
```

Returns the number of registered ALPN protocol names.

This could be used when deciding if protocol selection should be delegated to the ALPN mechanism, or whether a protocol should be immediately assigned.

If no ALPN protocols are registered, zero (0) is returned.

### TLS Connection Establishment

#### `fio_tls_accept`

```c
void fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata);
```

Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified context / settings object.

The `uuid` should be a socket UUID that is already connected to a peer (i.e., the result of `fio_accept`).

The `udata` is an opaque user data pointer that is passed along to the protocol selected (if any protocols were added using `fio_tls_alpn_add`).


#### `fio_tls_connect`

```c
void fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata);
```


Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified context / settings object.

The `uuid` should be a socket UUID that is already connected to a peer (i.e., one received by a `fio_connect` specified callback `on_connect`).

The `udata` is an opaque user data pointer that is passed along to the protocol selected (if any protocols were added using `fio_tls_alpn_add`).


### TLS Compile-Time Options

#### `FIO_TLS_PRINT_SECRET`

```c
#ifndef FIO_TLS_PRINT_SECRET
/* if true, the master key secret should be printed using FIO_LOG_DEBUG */
#define FIO_TLS_PRINT_SECRET 0
#endif
```

By setting `FIO_TLS_PRINT_SECRET` to a true value (1), facil.io will compile in a way that prints out the master key / secret to the debugging log, for use with WireShark or similar network debugging tools.
