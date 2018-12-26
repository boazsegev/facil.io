/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <fio.h>

/**
 * This implementation of the facil.io SSL/TLS wrapper API is the default
 * implementation that will be used when no SSL/TLS library is available.
 *
 * The implementation includes redundant code that can be USED AS A TEMPLATE for
 * future implementations.
 *
 * THIS TEMPLATE IMPLEMENTATION DOES NOTHING EXCEPT CRASHING THE PROGRAM.
 */
#include "fio_tls.h"

#if 1 /* TODO: place library compiler flags here */

#define REQUIRE_LIBRARY()

/* TODO: delete me! */
#if !FIO_IGNORE_TLS_IF_MISSING
#undef REQUIRE_LIBRARY
#define REQUIRE_LIBRARY()                                                      \
  FIO_LOG_FATAL("No supported SSL/TLS library available.");                    \
  exit(-1);
#endif

/* *****************************************************************************
The SSL/TLS helper data types
***************************************************************************** */
#define FIO_INCLUDE_STR 1
#define FIO_FORCE_MALLOC 1
#include <fio.h>

typedef struct {
  fio_str_s name; /* fio_str_s provides cache locality for small strings */
  void (*callback)(intptr_t uuid, void *udata);
} alpn_s;

static inline int fio_alpn_cmp(const alpn_s *dest, const alpn_s *src) {
  return fio_str_iseq(&dest->name, &src->name);
}
static inline void fio_alpn_copy(alpn_s *dest, alpn_s *src) {
  *dest = (alpn_s){.name = FIO_STR_INIT, .callback = src->callback};
  fio_str_concat(&dest->name, &src->name);
}
static inline void fio_alpn_destroy(alpn_s *obj) { fio_str_free(&obj->name); }

#define FIO_ARY_NAME alpn_ary
#define FIO_ARY_TYPE alpn_s
#define FIO_ARY_COMPARE(k1, k2) fio_alpn_cmp(&(k1), &(k2))
#define FIO_ARY_COPY(dest, obj) fio_alpn_copy(&(dest), &(obj))
#define FIO_ARY_DESTROY(key) fio_alpn_destroy(&(key))
#include <fio.h>

typedef struct {
  fio_str_s private_key;
  fio_str_s public_key;
} cert_s;

static inline int fio_tls_cert_cmp(const cert_s *dest, const cert_s *src) {
  return fio_str_iseq(&dest->private_key, &src->private_key);
}
static inline void fio_tls_cert_copy(cert_s *dest, cert_s *src) {
  *dest = (cert_s){
      .private_key = FIO_STR_INIT,
      .public_key = FIO_STR_INIT,
  };
  fio_str_concat(&dest->private_key, &src->private_key);
  fio_str_concat(&dest->public_key, &src->public_key);
}
static inline void fio_tls_cert_destroy(cert_s *obj) {
  fio_str_free(&obj->private_key);
  fio_str_free(&obj->public_key);
}

#define FIO_ARY_NAME cert_ary
#define FIO_ARY_TYPE cert_s
#define FIO_ARY_COMPARE(k1, k2) (fio_tls_cert_cmp(&(k1), &(k2)))
#define FIO_ARY_COPY(dest, obj) fio_tls_cert_copy(&(dest), &(obj))
#define FIO_ARY_DESTROY(key) fio_tls_cert_destroy(&(key))
#include <fio.h>

/* *****************************************************************************
The SSL/TLS type
***************************************************************************** */

/** An opaque type used for the SSL/TLS functions. */
struct fio_tls_s {
  alpn_ary_s alpn; /* ALPN is the name for the protocol selection extension */
  cert_ary_s sni;  /* SNI is the name for the server name extension */
  /* TODO: implementation data fields go here */
};

/* *****************************************************************************
SSL/TLS Context (re)-building
***************************************************************************** */

/** Called when the library specific data for the context should be destroyed */
static void fio_tls_destroy_context(fio_tls_s *tls) {
  /* TODO: Library specific implementation */
  (void)tls;
}

/** Called when the library specific data for the context should be built */
static void fio_tls_build_context(fio_tls_s *tls) {
  fio_tls_destroy_context(tls);
  /* TODO: Library specific implementation */

  /* Certificates */
  FIO_ARY_FOR(&tls->sni, pos) {
    fio_str_info_s k = fio_str_info(&pos->private_key);
    fio_str_info_s p = fio_str_info(&pos->public_key);
    if (p.len && k.len) {
      /* TODO: attache certificate */
    } else {
      /* TODO: self signed certificate */
    }
  }

  /* ALPN Protocols */
  FIO_ARY_FOR(&tls->alpn, pos) {
    fio_str_info_s name = fio_str_info(&pos->name);
    (void)name;
    // map to pos->callback;
  }
}

/* *****************************************************************************
SSL/TLS RW Hooks
***************************************************************************** */

/* TODO: this is an example implementation - fix for specific library. */

#define TLS_BUFFER_LENGTH (1 << 15)
typedef struct {
  size_t len;
  char buffer[TLS_BUFFER_LENGTH];
} buffer_s;

/**
 * Implement reading from a file descriptor. Should behave like the file
 * system `read` call, including the setup or errno to EAGAIN / EWOULDBLOCK.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_read(intptr_t uuid, void *udata, void *buf,
                            size_t count) {
  ssize_t ret = read(fio_uuid2fd(uuid), buf, count);
  if (ret > 0) {
    FIO_LOG_DEBUG("Read %zd bytes from %p", ret, (void *)uuid);
  }
  return ret;
  (void)udata;
}

/**
 * When implemented, this function will be called to flush any data remaining
 * in the internal buffer.
 *
 * The function should return the number of bytes remaining in the internal
 * buffer (0 is a valid response) or -1 (on error).
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 */
static ssize_t fio_tls_flush(intptr_t uuid, void *udata) {
  buffer_s *buffer = udata;
  if (!buffer->len) {
    FIO_LOG_DEBUG("Flush empty for %p", (void *)uuid);
    return 0;
  }
  ssize_t r = write(fio_uuid2fd(uuid), buffer->buffer, buffer->len);
  if (r < 0)
    return -1;
  if (r == 0) {
    errno = ECONNRESET;
    return -1;
  }
  size_t len = buffer->len - r;
  if (len)
    memmove(buffer->buffer, buffer->buffer + r, len);
  buffer->len = len;
  FIO_LOG_DEBUG("Sent %zd bytes to %p", r, (void *)uuid);
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
static ssize_t fio_tls_write(intptr_t uuid, void *udata, const void *buf,
                             size_t count) {
  buffer_s *buffer = udata;
  size_t can_copy = TLS_BUFFER_LENGTH - buffer->len;
  if (can_copy > count)
    can_copy = count;
  if (!can_copy)
    goto would_block;
  memcpy(buffer->buffer + buffer->len, buf, can_copy);
  buffer->len += can_copy;
  FIO_LOG_DEBUG("Copied %zu bytes to %p", can_copy, (void *)uuid);
  fio_tls_flush(uuid, udata);
  return can_copy;
would_block:
  errno = EWOULDBLOCK;
  return -1;
}

/**
 * The `close` callback should close the underlying socket / file descriptor.
 *
 * If the function returns a non-zero value, it will be called again after an
 * attempt to flush the socket and any pending outgoing buffer.
 *
 * Note: facil.io library functions MUST NEVER be called by any r/w hook, or a
 * deadlock might occur.
 * */
static ssize_t fio_tls_before_close(intptr_t uuid, void *udata) {
  FIO_LOG_DEBUG("The `before_close` callback was called for %p", (void *)uuid);
  return 1;
  (void)udata;
}
/**
 * Called to perform cleanup after the socket was closed.
 * */
static void fio_tls_cleanup(void *udata) { fio_free(udata); }

static fio_rw_hook_s FIO_TLS_HOOKS = {
    .read = fio_tls_read,
    .write = fio_tls_write,
    .before_close = fio_tls_before_close,
    .flush = fio_tls_flush,
    .cleanup = fio_tls_cleanup,
};

static inline void fio_tls_attach2uuid(intptr_t uuid, fio_tls_s *tls,
                                       void *udata, uint8_t is_server) {
  /* TODO: this is only an example implementation - fox for specific library */
  if (is_server) {
    /* Server mode (accept) */
    FIO_LOG_DEBUG("Attaching TLS read/write hook for %p (server mode).",
                  (void *)uuid);
  } else {
    /* Client mode (connect) */
    FIO_LOG_DEBUG("Attaching TLS read/write hook for %p (client mode).",
                  (void *)uuid);
  }
  /* common implementation */
  fio_rw_hook_set(uuid, &FIO_TLS_HOOKS,
                  fio_malloc(sizeof(buffer_s))); /* 32Kb buffer */
  if (alpn_ary_count(&tls->alpn))
    alpn_ary_get(&tls->alpn, 0).callback(uuid, udata);
}

/* *****************************************************************************
SSL/TLS API implementation - this can be pretty much used as is...
***************************************************************************** */

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 */
fio_tls_s *__attribute__((weak))
fio_tls_new(const char *server_name, const char *key, const char *cert) {
  REQUIRE_LIBRARY();
  fio_tls_s *tls = calloc(sizeof(*tls), 1);
  fio_tls_cert_add(tls, server_name, key, cert);
  return tls;
}
#pragma weak fio_tls_new

/**
 * Adds a certificate  a new SSL/TLS context / settings object.
 *
 *      fio_tls_cert_add(tls, FIO_TLS_CERT("www.example.com",
 *                            "private_key.key",
 *                            "public_key.crt" ));
 */
void __attribute__((weak))
fio_tls_cert_add(fio_tls_s *tls, const char *server_name, const char *key,
                 const char *cert) {
  REQUIRE_LIBRARY();
  cert_s c = {
      .private_key = FIO_STR_INIT,
      .public_key = FIO_STR_INIT,
  };
  if (key && cert) {
    if (fio_str_readfile(&c.private_key, key, 0, 0).data == NULL)
      goto file_missing;
    if (fio_str_readfile(&c.public_key, cert, 0, 0).data == NULL)
      goto file_missing;
  } else {
    /* Self-Signed TLS Certificates (NULL) */
    if (!server_name)
      server_name = "facil.io.tls";
    c.private_key = FIO_STR_INIT_STATIC(server_name);
  }
  cert_ary_push(&tls->sni, c);
  fio_tls_cert_destroy(&c);
  fio_tls_build_context(tls);
  return;
file_missing:
  FIO_LOG_FATAL("TLS certificate file missing for either %s or %s or both.",
                key, cert);
  exit(-1);
}
#pragma weak fio_tls_cert_add

/**
 * Adds an ALPN protocol callback to the SSL/TLS context.
 *
 * The first protocol added will act as the default protocol to be selected.
 */
void __attribute__((weak))
fio_tls_proto_add(fio_tls_s *tls, const char *protocol_name,
                  void (*callback)(intptr_t uuid, void *udata)) {
  REQUIRE_LIBRARY();
  alpn_s tmp = {
      .name = FIO_STR_INIT_STATIC(protocol_name),
      .callback = callback,
  };
  alpn_ary_push(&tls->alpn, tmp);
  fio_alpn_destroy(&tmp);
  fio_tls_build_context(tls);
}
#pragma weak fio_tls_proto_add

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Server, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * the result of `fio_accept`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_proto_add`).
 */
void __attribute__((weak))
fio_tls_accept(intptr_t uuid, fio_tls_s *tls, void *udata) {
  REQUIRE_LIBRARY();
  fio_tls_attach2uuid(uuid, tls, udata, 1);
}
#pragma weak fio_tls_accept

/**
 * Establishes an SSL/TLS connection as an SSL/TLS Client, using the specified
 * context / settings object.
 *
 * The `uuid` should be a socket UUID that is already connected to a peer (i.e.,
 * one received by a `fio_connect` specified callback `on_connect`).
 *
 * The `udata` is an opaque user data pointer that is passed along to the
 * protocol selected (if any protocols were added using `fio_tls_proto_add`).
 */
void __attribute__((weak))
fio_tls_connect(intptr_t uuid, fio_tls_s *tls, void *udata) {
  REQUIRE_LIBRARY();
  fio_tls_attach2uuid(uuid, tls, udata, 0);
}
#pragma weak fio_tls_connect

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
void __attribute__((weak)) fio_tls_destroy(fio_tls_s *tls) {
  if (!tls)
    return;
  REQUIRE_LIBRARY();
  fio_tls_destroy_context(tls);
  alpn_ary_free(&tls->alpn);
  cert_ary_free(&tls->sni);
  free(tls);
}
#pragma weak fio_tls_destroy

#endif /* Library compiler flags */
