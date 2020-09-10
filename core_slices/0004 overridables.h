/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************
0800 threads.h
***************************************************************************** */
#ifndef FIO_MAX_SOCK_CAPACITY /* Development inclusion - ignore line */
#include "0003 main api.h"    /* Development inclusion - ignore line */
#endif                        /* Development inclusion - ignore line */
/* *****************************************************************************
Concurrency overridable functions

These functions can be overridden so as to adjust for different environments.
***************************************************************************** */
/**
 * OVERRIDE THIS to replace the default `fork` implementation.
 *
Behaves like the system's `fork`.... but calls the correct state callbacks.
 *
 * NOTE: When overriding, remember to call the proper state callbacks and call
 * fio_on_fork in the child process before calling any state callback or other
 * functions.
 */
FIO_WEAK int fio_fork(void);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer to a function and a single argument that should be executed
 * within a new thread.
 *
 * The function should allocate memory for the thread object and return a
 * pointer to the allocated memory that identifies the thread.
 *
 * On error NULL should be returned.
 */
FIO_WEAK void *fio_thread_new(void *(*thread_func)(void *), void *arg);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Frees the memory associated with a thread identifier (allows the thread to
 * run it's course, just the identifier is freed).
 */
FIO_WEAK void fio_thread_free(void *p_thr);

/**
 * OVERRIDE THIS to replace the default pthread implementation.
 *
 * Accepts a pointer returned from `fio_thread_new` (should also free any
 * allocated memory) and joins the associated thread.
 *
 * Return value is ignored.
 */
FIO_WEAK int fio_thread_join(void *p_thr);

/**
 * When overrriding fio_fork, call this callback in the child process before
 * calling any other function.
 */
void fio_on_fork(void);

/* *****************************************************************************
TLS Support (weak functions, to be overriden by library wrapper)
***************************************************************************** */

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 *
 * If no server name is provided and no private key and public certificate are
 * provided, an empty TLS object will be created, (maybe okay for clients).
 *
 * If a server name is provided, but no certificate is attached, an anonymous
 * (self-signed) certificate will be initialized.
 *
 *      fio_tls_s * tls = fio_tls_new("www.example.com",
 *                                    "public_key.pem",
 *                                    "private_key.pem", NULL );
 */
FIO_WEAK fio_tls_s *fio_tls_new(const char *server_name,
                                const char *public_cert_file,
                                const char *private_key_file,
                                const char *pk_password);

/**
 * Increase the reference count for the TLS object.
 *
 * Decrease with `fio_tls_destroy`.
 */
FIO_WEAK void fio_tls_dup(fio_tls_s *tls);

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
FIO_WEAK void fio_tls_free(fio_tls_s *tls);

/**
 * Adds a certificate to the SSL/TLS context / settings object.
 *
 * SNI support is implementation / library specific, but SHOULD be provided.
 *
 * If a server name is provided, but no certificate is attached, an anonymous
 * (self-signed) certificate will be initialized.
 *
 *      fio_tls_cert_add(tls, "www.example.com",
 *                            "public_key.pem",
 *                            "private_key.pem", NULL );
 */
FIO_WEAK void fio_tls_cert_add(fio_tls_s *,
                               const char *server_name,
                               const char *public_cert_file,
                               const char *private_key_file,
                               const char *pk_password);

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 *
 * The `on_selected` callback should accept the `uuid`, the user data pointer
 * passed to either `fio_tls_accept` or `fio_tls_connect` (here:
 * `udata_connetcion`) and the user data pointer passed to the
 * `fio_tls_alpn_add` function (`udata_tls`).
 *
 * The `on_cleanup` callback will be called when the TLS object is destroyed (or
 * `fio_tls_alpn_add` is called again with the same protocol name). The
 * `udata_tls` argument will be passed along, as is, to the callback (if set).
 *
 * Except for the `tls` and `protocol_name` arguments, all arguments can be
 * NULL.
 */
FIO_WEAK void fio_tls_alpn_add(fio_tls_s *tls,
                               const char *protocol_name,
                               void (*on_selected)(intptr_t uuid,
                                                   void *udata_connection,
                                                   void *udata_tls),
                               void *udata_tls,
                               void (*on_cleanup)(void *udata_tls));

/**
 * Adds a certificate to the "trust" list, which automatically adds a peer
 * verification requirement.
 *
 * Note, when the fio_tls_s object is used for server connections, this will
 * limit connections to clients that connect using a trusted certificate.
 *
 *      fio_tls_trust(tls, "google-ca.pem" );
 */
FIO_WEAK void fio_tls_trust(fio_tls_s *, const char *public_cert_file);

/* *****************************************************************************
TLS Support - non-user functions (called by the facil.io library)
***************************************************************************** */

/**
 * Returns the number of registered ALPN protocol names.
 *
 * This could be used when deciding if protocol selection should be delegated to
 * the ALPN mechanism, or whether a protocol should be immediately assigned.
 *
 * If no ALPN protocols are registered, zero (0) is returned.
 */
FIO_WEAK uintptr_t fio_tls_alpn_count(fio_tls_s *tls);

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * the result of `fio_accept`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
FIO_WEAK void fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata);

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * one received by a `fio_connect` specified callback `on_connect`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_alpn_add`).
 */
FIO_WEAK void fio_tls_connect(intptr_t uuid,
                              fio_tls_s *tls,
                              const char *server_sni,
                              void *udata);

/* *****************************************************************************
Mark weakness
***************************************************************************** */

#pragma weak fio_fork
#pragma weak fio_thread_new
#pragma weak fio_thread_free
#pragma weak fio_thread_join

#pragma weak fio_tls_new
#pragma weak fio_tls_dup
#pragma weak fio_tls_free
#pragma weak fio_tls_cert_add
#pragma weak fio_tls_alpn_add
#pragma weak fio_tls_trust
#pragma weak fio_tls_alpn_count
#pragma weak fio_tls_accept
#pragma weak fio_tls_connect
