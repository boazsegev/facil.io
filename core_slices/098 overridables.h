/* *****************************************************************************
Functions that can be overridden
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/**
 * See `FIO_FUNCTIONS`.
 */
typedef struct {
  /* ***************************************************************************
  Concurrency overridable functions:

  These functions are used for worker process and thread creation.
  *************************************************************************** */
  /**
   * Behaves like the system's `fork`.... but calls the correct state callbacks.
   *
   * NOTE: When overriding, remember to call the proper state callbacks and call
   * fio_on_fork in the child process before calling any state callback or other
   * functions.
   */
  int (*fio_fork)(void);

  /**
   * Accepts a pointer to a function and a single argument that should be
   * executed within a new thread.
   *
   * The function should allocate memory for the thread object and return a
   * pointer to the allocated memory that identifies the thread.
   *
   * On error NULL should be returned.
   */
  int (*fio_thread_start)(void *p_thr, void *(*thread_func)(void *), void *arg);

  /**
   * Accepts a pointer returned from `fio_thread_new` (should also free any
   * allocated memory) and joins the associated thread.
   *
   * Return value is ignored.
   */
  int (*fio_thread_join)(void *p_thr);

  /** the size of a thread type */
  size_t size_of_thread_t;
  /* ***************************************************************************
  TLS Support
  *************************************************************************** */
  /** The function calls to be used by protocols that use this TLS as clients */
  fio_tls_functions_s TLS_CLIENT;
  /** The function calls to be used by protocols that use this TLS as servers */
  fio_tls_functions_s TLS_SERVER;
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
  fio_tls_s *(*fio_tls_new)(fio_protocol_s *default_protocol,
                            const char *server_name,
                            const char *public_cert_file,
                            const char *private_key_file,
                            const char *pk_password);

  /**
   * Increase the reference count for the TLS object.
   *
   * Decrease with `fio_tls_destroy`.
   */
  fio_tls_s *(*fio_tls_dup)(fio_tls_s *tls);

  /**
   * Destroys the SSL/TLS context / settings object and frees any related
   * resources / memory.
   */
  void (*fio_tls_free)(fio_tls_s *tls);

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
  void (*fio_tls_cert_add)(fio_tls_s *,
                           const char *server_name,
                           const char *public_cert_file,
                           const char *private_key_file,
                           const char *pk_password);

  /**
   * Adds an ALPN protocol callback to the SSL/TLS context.
   *
   * The first protocol added will act as the default protocol to be selected.
   *
   * Except for the `tls` argument, all arguments can be NULL.
   */
  void (*fio_tls_alpn_add)(fio_tls_s *tls,
                           const char *protocol_name,
                           fio_protocol_s *protocol);

  /**
   * Adds a certificate to the "trust" list, which automatically adds a peer
   * verification requirement.
   *
   * Note, when the fio_tls_s object is used for server connections, this will
   * limit connections to clients that connect using a trusted certificate.
   *
   *      fio_tls_trust(tls, "google-ca.pem" );
   */
  void (*fio_tls_trust)(fio_tls_s *, const char *public_cert_file);

  /* ***************************************************************************
  TLS Support - non-user functions (called by the facil.io library)
  *************************************************************************** */

  /**
   * Returns the number of registered ALPN protocol names.
   *
   * This could be used when deciding if protocol selection should be delegated
   * to the ALPN mechanism, or whether a protocol should be immediately
   * assigned.
   *
   * If no ALPN protocols are registered, zero (0) is returned.
   */
  uintptr_t (*fio_tls_alpn_count)(fio_tls_s *tls);
} fio_overridable_functions_s;

/**
 * This struct contains function pointers for functions that might need to be
 * replaced for different environments - possibly dynamically.
 *
 * Each function pointer can be replaced, allowing facil.io to use a different
 * thread implementation for worker threads or adopt a specific TLS library
 * (possibly replacing TLS implementations during runtime).
 */
extern fio_overridable_functions_s FIO_FUNCTIONS;
