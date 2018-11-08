/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FIO_TLS

/**
 * This is an SSL/TLS extension for the facil.io library.
 */
#define H_FIO_TLS

#include <stdint.h>

/** An opaque type used for the SSL/TLS functions. */
typedef struct fio_tls_s fio_tls_s;

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 *
 *      fio_tls_s * tls = fio_tls_new("www.example.com",
 *                                    "private_key.key",
 *                                    "public_key.crt" );
 */
fio_tls_s *fio_tls_new(const char *server_name, const char *private_key_file,
                       const char *public_cert_file);

/**
 * Adds a certificate a new SSL/TLS context / settings object (SNI support).
 *
 *      fio_tls_cert_add(tls, "www.example.com",
 *                            "private_key.key",
 *                            "public_key.crt" );
 */
void fio_tls_cert_add(fio_tls_s *, const char *server_name,
                      const char *private_key_file,
                      const char *public_cert_file);

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 */
void fio_tls_proto_add(fio_tls_s *, const char *protocol_name,
                       void (*callback)(intptr_t uuid, void *udata));

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified
 * conetext / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * the result of `fio_accept`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_proto_add`).
 */
void fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata);

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified
 * conetext / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * one recived by a `fio_connect` specified callback `on_connect`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_proto_add`).
 */
void fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata);

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
void fio_tls_destroy(fio_tls_s *tls);

#endif
