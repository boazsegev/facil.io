/* *****************************************************************************
Functions that can be overridden
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************




                            TLS Support Sample Code

        These functions can be used as the basis for a TLS implementation




***************************************************************************** */
#if FIO_SAMPLE_TLS || defined(DEBUG)
#define REQUIRE_TLS_LIBRARY()
#else
#undef REQUIRE_TLS_LIBRARY
#define REQUIRE_TLS_LIBRARY()                                                  \
  FIO_LOG_FATAL("No supported SSL/TLS library available.");                    \
  exit(-1);
#endif

/* *****************************************************************************
The SSL/TLS helper data types (could work as is)
***************************************************************************** */

typedef struct {
  sstr_s private_key;
  sstr_s public_key;
  sstr_s password;
} cert_s;

static inline int fio___tls_cert_cmp(const cert_s *dest, const cert_s *src) {
  return sstr_is_eq(&dest->private_key, &src->private_key);
}
static inline void fio___tls_cert_copy(cert_s *dest, cert_s *src) {
  sstr_init_copy2(&dest->private_key, &src->private_key);
  sstr_init_copy2(&dest->public_key, &src->public_key);
  sstr_init_copy2(&dest->password, &src->password);
}
static inline void fio___tls_cert_destroy(cert_s *obj) {
  sstr_destroy(&obj->private_key);
  sstr_destroy(&obj->public_key);
  sstr_destroy(&obj->password);
}

#define FIO_ARRAY_NAME                 fio___tls_cert_ary
#define FIO_ARRAY_TYPE                 cert_s
#define FIO_ARRAY_TYPE_CMP(k1, k2)     (fio___tls_cert_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_cert_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key)    fio___tls_cert_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID         ((cert_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE  1
#define FIO_MALLOC_TMP_USE_SYSTEM      1
#include <fio-stl.h>

typedef struct {
  sstr_s pem;
} trust_s;

static inline int fio___tls_trust_cmp(const trust_s *dest, const trust_s *src) {
  return sstr_is_eq(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_copy(trust_s *dest, trust_s *src) {
  sstr_init_copy2(&dest->pem, &src->pem);
}
static inline void fio___tls_trust_destroy(trust_s *obj) {
  sstr_destroy(&obj->pem);
}

#define FIO_ARRAY_NAME                 fio___tls_trust_ary
#define FIO_ARRAY_TYPE                 trust_s
#define FIO_ARRAY_TYPE_CMP(k1, k2)     (fio___tls_trust_cmp(&(k1), &(k2)))
#define FIO_ARRAY_TYPE_COPY(dest, obj) fio___tls_trust_copy(&(dest), &(obj))
#define FIO_ARRAY_TYPE_DESTROY(key)    fio___tls_trust_destroy(&(key))
#define FIO_ARRAY_TYPE_INVALID         ((trust_s){{0}})
#define FIO_ARRAY_TYPE_INVALID_SIMPLE  1
#define FIO_MALLOC_TMP_USE_SYSTEM      1
#include <fio-stl.h>

typedef struct {
  sstr_s name; /* sstr_s provides cache locality for small strings */
  fio_protocol_s *protocol;
} alpn_s;

static inline int fio_alpn_cmp(const alpn_s *dest, const alpn_s *src) {
  return sstr_is_eq(&dest->name, &src->name);
}
static inline void fio_alpn_copy(alpn_s *dest, alpn_s *src) {
  sstr_init_copy2(&dest->name, &src->name);
  dest->protocol = src->protocol;
}
static inline void fio_alpn_destroy(alpn_s *obj) { sstr_destroy(&obj->name); }

#define FIO_OMAP_NAME                fio___tls_alpn_list
#define FIO_MAP_TYPE                 alpn_s
#define FIO_MAP_TYPE_INVALID         ((alpn_s){0})
#define FIO_MAP_TYPE_INVALID_SIMPLE  1
#define FIO_MAP_TYPE_CMP(k1, k2)     fio_alpn_cmp(&(k1), &(k2))
#define FIO_MAP_TYPE_COPY(dest, obj) fio_alpn_copy(&(dest), &(obj))
#define FIO_MAP_TYPE_DESTROY(key)    fio_alpn_destroy(&(key))
#define FIO_MALLOC_TMP_USE_SYSTEM    1
#include <fio-stl.h>

/* *****************************************************************************
The SSL/TLS Context type NOTE: implementations need to edit this
***************************************************************************** */

/** example internal buffer length */
#define TLS_BUFFER_LENGTH (1 << 15)

/** The fio_tls_s type is used for both global and instance data. */
struct fio_tls_s {
  uint32_t ref; /* Reference counter */
  enum {
    FIO_TLS_S_CONTEXT,
    FIO_TLS_S_INSTANCE,
  } type;
};

/** A `fio_tls_s` sub-type used for managing global context data. */
typedef struct {
  fio_tls_s klass;
  fio___tls_alpn_list_s alpn;  /* ALPN - TLS protocol selection extension */
  fio___tls_cert_ary_s sni;    /* SNI - Server Name ID (certificates) */
  fio___tls_trust_ary_s trust; /* Trusted certificate registry (peers) */
  /***** NOTE: implementation instance data fields go here ***********/
} fio_tls_cx_s;

/** A `fio_tls_s` sub-type used for managing connection specific data. */
typedef struct {
  fio_tls_s klass;
  fio_tls_cx_s *cx;
  fio_s *io; /* not to be used except within callbacks (i.e., setting ALPN) */
  /***** NOTE: implementation instance data fields go here ***********/
  size_t len;
  uint8_t alpn_ok;
  char buf[TLS_BUFFER_LENGTH];
} fio_tls_io_s;

/* constructor & reference count manager */
FIO_IFUNC fio_tls_s *fio_tls_dup(fio_tls_s *tls, fio_s *io, uint8_t is_server) {
  fio_tls_io_s *t;
  fio_tls_cx_s *cx;
  if (!tls)
    goto is_new;
  switch (tls->type) {
  case FIO_TLS_S_CONTEXT: {
    t = fio_malloc(sizeof(*t));
    FIO_ASSERT_ALLOC(t);
    *t = (fio_tls_io_s){
        .klass = {.ref = 1, .type = FIO_TLS_S_INSTANCE},
        .cx = (fio_tls_cx_s *)tls,
        .io = io,
    };
    fio_atomic_add(&tls->ref, 1);
    return (fio_tls_s *)t;
  }
  case FIO_TLS_S_INSTANCE:
    fio_atomic_add(&tls->ref, 1);
    if (is_server) {
      /* NOTE: implementations should add server specific logic (handshake) */
    } else {
      /* NOTE: implementations should add client specific logic (handshake) */
    }
    return tls;
  }
is_new:
  cx = malloc(sizeof(*cx));
  FIO_ASSERT_ALLOC(cx);
  *cx = (fio_tls_cx_s){
      .klass = {.ref = 1, .type = FIO_TLS_S_CONTEXT},
      .alpn = FIO_MAP_INIT,
      .sni = FIO_ARRAY_INIT,
      .trust = FIO_ARRAY_INIT,
  };
  return (fio_tls_s *)cx;
}

static fio_tls_s *fio_tls_dup_server(fio_tls_s *tls, fio_s *io) {
  return fio_tls_dup(tls, io, 1);
}

static fio_tls_s *fio_tls_dup_client(fio_tls_s *tls, fio_s *io) {
  return fio_tls_dup(tls, io, 0);
}
static fio_tls_s *fio_tls_dup_master(fio_tls_s *tls) {
  if (!tls)
    return fio_tls_dup(tls, NULL, 0);
  fio_atomic_add(&tls->ref, 1);
  return tls;
}

/* destructor & reference count manager */
static void fio_tls_free(fio_tls_s *tls) {
  if (fio_atomic_sub_fetch(&tls->ref, 1))
    return;
  fio_tls_io_s *t = (fio_tls_io_s *)tls;
  fio_tls_cx_s *cx = (fio_tls_cx_s *)tls;
  switch (tls->type) {
  case FIO_TLS_S_CONTEXT:
    /* NOTE: destruct TLS context data */
    fio___tls_alpn_list_destroy(&cx->alpn);
    fio___tls_cert_ary_destroy(&cx->sni);
    fio___tls_trust_ary_destroy(&cx->trust);
    free(cx);
    break;
  case FIO_TLS_S_INSTANCE:
    fio_tls_free((fio_tls_s *)t->cx);
    /* NOTE: destruct TLS connection data */
    fio_free(t);
    break;
  }
}

/* *****************************************************************************
ALPN Helpers - NOTE: implementations need to edit this
***************************************************************************** */

/** Adds an ALPN data object to the ALPN "list" (set) */
FIO_IFUNC void fio___tls_alpn_add(fio_tls_cx_s *tls,
                                  const char *protocol_name,
                                  fio_protocol_s *protocol) {
  alpn_s tmp = {
      .name = FIO_STR_INIT,
      .protocol = protocol,
  };
  if (protocol_name)
    sstr_init_const(&tmp.name, protocol_name, strlen(protocol_name));
  if (sstr_len(&tmp.name) > 255) {
    FIO_LOG_ERROR("ALPN protocol names are limited to 255 bytes.");
    return;
  }
  fio___tls_alpn_list_set(&tls->alpn, sstr_hash(&tmp.name, 0), tmp, NULL);
  fio_alpn_destroy(&tmp);
}

/** Returns a pointer to the default (first) ALPN object in the TLS (if any). */
FIO_IFUNC alpn_s *fio___tls_alpn_default(fio_tls_cx_s *tls) {
  FIO_MAP_EACH(fio___tls_alpn_list, &tls->alpn, pos) { return &pos->obj; }
  return NULL;
}

/** Returns a pointer to the ALPN data (callback, etc') IF exists in the TLS. */
FIO_IFUNC void fio___tls_alpn_select(fio_tls_io_s *tls,
                                     char *name,
                                     size_t len) {
  if (!tls || !tls->cx || !fio___tls_alpn_list_count(&tls->cx->alpn))
    return;
  alpn_s tmp;
  sstr_init_const(&tmp.name, name, len);
  alpn_s *pos =
      fio___tls_alpn_list_get_ptr(&tls->cx->alpn, sstr_hash(&tmp.name, 0), tmp);
  if (!pos)
    pos = fio___tls_alpn_default(tls->cx);
  fio_protocol_set(tls->io, pos->protocol);
}

/* *****************************************************************************
SSL/TLS Context (re)-building - NOTE: implementations need to edit this
***************************************************************************** */

/** Called when the library specific data for the context should be destroyed */
static void fio___tls_destroy_context(fio_tls_cx_s *tls) {
  /* Perform library specific implementation */
  FIO_LOG_DDEBUG("destroyed TLS context %p", (void *)tls);
  (void)tls;
}

/** Called when the library specific data for the context should be built */
static void fio___tls_build_context(fio_tls_cx_s *tls) {
  fio___tls_destroy_context(tls);
  /* Perform library specific implementation */

  /* Certificates */
  FIO_ARRAY_EACH(fio___tls_cert_ary, &tls->sni, pos) {
    fio_str_info_s k = sstr_info(&pos->private_key);
    fio_str_info_s p = sstr_info(&pos->public_key);
    fio_str_info_s pw = sstr_info(&pos->password);
    if (p.len && k.len) {
      /* TODO: attache certificate */
      (void)pw;
    } else {
      /* TODO: self signed certificate */
    }
  }

  /* ALPN Protocols */
  FIO_MAP_EACH(fio___tls_alpn_list, &tls->alpn, pos) {
    fio_str_info_s name = sstr_info(&pos->obj.name);
    (void)name;
    // map to pos->callback;
  }

  /* Peer Verification / Trust */
  if (fio___tls_trust_ary_count(&tls->trust)) {
    /* TODO: enable peer verification */

    /* TODO: Add each certificate in the PEM to the trust "store" */
    FIO_ARRAY_EACH(fio___tls_trust_ary, &tls->trust, pos) {
      fio_str_info_s pem = sstr_info(&pos->pem);
      (void)pem;
    }
  }

  FIO_LOG_DDEBUG("(re)built TLS context %p", (void *)tls);
}

/* *****************************************************************************
TLS Support
***************************************************************************** */

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
static void fio_tls_cert_add(fio_tls_s *tls,
                             const char *server_name,
                             const char *cert,
                             const char *key,
                             const char *pk_password) {
  REQUIRE_TLS_LIBRARY();
  cert_s c = {
      .private_key = FIO_STR_INIT,
      .public_key = FIO_STR_INIT,
      .password = FIO_STR_INIT,
  };
  fio_tls_cx_s *cx = (fio_tls_cx_s *)tls;
  if (tls->type == FIO_TLS_S_INSTANCE)
    cx = ((fio_tls_io_s *)tls)->cx;
  if (pk_password)
    sstr_init_const(&c.password, pk_password, strlen(pk_password));
  if (key && cert) {
    if (sstr_readfile(&c.private_key, key, 0, 0).buf == NULL)
      goto file_missing;
    if (sstr_readfile(&c.public_key, cert, 0, 0).buf == NULL)
      goto file_missing;
    fio___tls_cert_ary_push(&cx->sni, c);
  } else if (server_name) {
    /* Self-Signed TLS Certificates */
    sstr_init_const(&c.private_key, server_name, strlen(server_name));
    fio___tls_cert_ary_push(&cx->sni, c);
  }
  fio___tls_cert_destroy(&c);
  fio___tls_build_context(cx);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for either %s or %s or both.",
                key,
                cert);
  exit(-1);
}

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 * Except for the `tls` argument, all arguments can be NULL.
 */
static void fio_tls_alpn_add(fio_tls_s *tls,
                             const char *protocol_name,
                             fio_protocol_s *protocol) {
  REQUIRE_TLS_LIBRARY();
  fio_tls_cx_s *cx = (fio_tls_cx_s *)tls;
  if (tls->type == FIO_TLS_S_INSTANCE)
    cx = ((fio_tls_io_s *)tls)->cx;
  fio___tls_alpn_add(cx, protocol_name, protocol);
  fio___tls_build_context(cx);
}

/**
 * Adds a certificate to the "trust" list, which automatically adds a peer
 * verification requirement.
 *
 * Note, when the fio_tls_s object is used for server connections, this will
 * limit connections to clients that connect using a trusted certificate.
 *
 *      fio_tls_trust(tls, "google-ca.pem" );
 */
static void fio_tls_trust(fio_tls_s *tls, const char *public_cert_file) {
  REQUIRE_TLS_LIBRARY();
  trust_s c = {
      .pem = FIO_STR_INIT,
  };
  fio_tls_cx_s *cx = (fio_tls_cx_s *)tls;
  if (tls->type == FIO_TLS_S_INSTANCE)
    cx = ((fio_tls_io_s *)tls)->cx;
  if (!public_cert_file)
    return;
  if (sstr_readfile(&c.pem, public_cert_file, 0, 0).buf == NULL)
    goto file_missing;
  fio___tls_trust_ary_push(&cx->trust, c);
  fio___tls_trust_destroy(&c);
  fio___tls_build_context(cx);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for %s ", public_cert_file);
  exit(-1);
}

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
static fio_tls_s *fio_tls_new(fio_protocol_s *default_protocol,
                              const char *server_name,
                              const char *public_cert_file,
                              const char *private_key_file,
                              const char *pk_password) {
  fio_tls_s *ret = fio_tls_dup(NULL, NULL, 0);
  if (default_protocol) {
    fio_tls_alpn_add(ret, NULL, default_protocol);
  }
  if (server_name)
    fio_tls_cert_add(ret,
                     server_name,
                     public_cert_file,
                     private_key_file,
                     pk_password);
  return ret;
}

/* *****************************************************************************
TLS Support - non-user functions (called by the facil.io library)
*****************************************************************************
*/

/**
 * Returns the number of registered ALPN protocol names.
 *
 * This could be used when deciding if protocol selection should be delegated
 * to the ALPN mechanism, or whether a protocol should be immediately
 * assigned.
 *
 * If no ALPN protocols are registered, zero (0) is returned.
 */
static uintptr_t fio_tls_alpn_count(fio_tls_s *tls) {
  if (!tls)
    return 0;
  REQUIRE_TLS_LIBRARY();
  fio_tls_cx_s *cx = (fio_tls_cx_s *)tls;
  if (tls->type == FIO_TLS_S_INSTANCE)
    cx = ((fio_tls_io_s *)tls)->cx;
  return fio___tls_alpn_list_count(&cx->alpn);
}

/* *****************************************************************************
TLS Protocol Functions - NOTE: implementations need to edit this
***************************************************************************** */

/**
 * Implement reading from a file descriptor. Should behave like the file
 * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.
 */
static ssize_t fio_tls_read(int fd, void *buf, size_t count, fio_tls_s *tls_) {
  FIO_ASSERT(tls_->type == FIO_TLS_S_INSTANCE,
             "A TLS read / Write called before TLS dup");
  fio_tls_io_s *tls = (fio_tls_io_s *)tls_;
  ssize_t ret = fio_sock_read(fd, buf, count);
  if (ret > 0) {
    FIO_LOG_DDEBUG("Read %zd bytes from fd %d", ret, fd);
  }
  return ret;
  (void)tls;
}

/**
 * When implemented, this function will be called to flush any data remaining
 * in the internal buffer.
 */
static int fio_tls_flush(int fd, fio_tls_s *tls_) {
  FIO_ASSERT(tls_->type == FIO_TLS_S_INSTANCE,
             "A TLS read / Write called before TLS dup");
  fio_tls_io_s *tls = (fio_tls_io_s *)tls_;
  if (!tls->len) {
    FIO_LOG_DEBUG("Flush empty for %d", fd);
    return 0;
  }
  ssize_t r = fio_sock_write(fd, tls->buf, tls->len);
  if (r < 0)
    return -1;
  if (r == 0) {
    errno = ECONNRESET;
    return -1;
  }
  size_t len = tls->len - r;
  if (len)
    memmove(tls->buf, tls->buf + r, len);
  tls->len = len;
  FIO_LOG_DEBUG("Sent %zd bytes to fd %d", r, fd);
  return r;
}

/**
 * Implement writing to a file descriptor. Should behave like the file system
 * `write` call.
 *
 * If an internal buffer is implemented and it is full, errno should be set to
 * EWOULDBLOCK and the function should return -1.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_write(int fd,
                             const void *buf,
                             size_t len,
                             fio_tls_s *tls_) {
  FIO_ASSERT(tls_->type == FIO_TLS_S_INSTANCE,
             "A TLS read / Write called before TLS dup");
  fio_tls_io_s *tls = (fio_tls_io_s *)tls_;
  size_t can_copy = TLS_BUFFER_LENGTH - tls->len;
  if (can_copy > len)
    can_copy = len;
  if (!can_copy)
    goto would_block;
  FIO_MEMCPY(tls->buf + tls->len, buf, can_copy);
  tls->len += can_copy;
  FIO_LOG_DEBUG("Copied %zu bytes to fd %d TLS", can_copy, fd);
  fio_tls_flush(fd, tls_);
  return can_copy;
would_block:
  fio_tls_flush(fd, tls_);
  errno = EWOULDBLOCK;
  return -1;
}
