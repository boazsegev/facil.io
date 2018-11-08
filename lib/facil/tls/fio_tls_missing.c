/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <fio.h>

/**
 *
 * This implementation of the facil.io SSL/TLS wrapper API is the default
 * implementation that will be used when no SSL/TLS library is available.
 *
 * The implementation includes redundant code that can be used as a template for
 * future implementations.
 *
 * THIS IMPLEMENTATION DOES NOTHING EXCEPT CRASHING THE PROGRAM.
 *
 *
 */
#include "fio_tls.h"

#if !FIO_IGNORE_TLS_IF_MISSING
#define REQUIRE_LIBRARY()                                                      \
  FIO_LOG_FATAL("No supported SSL/TLS library available.");                    \
  exit(-1);
#else
#define REQUIRE_LIBRARY()
#endif

/* *****************************************************************************
The SSL/TLS data type
***************************************************************************** */
/** An opaque type used for the SSL/TLS functions. */
struct fio_tls_s {
  void (*callback)(intptr_t uuid, void *udata);
};

/* *****************************************************************************
SSL/TLS RW Hooks
***************************************************************************** */

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
    FIO_LOG_DEBUG("Read %zd bytes from %p\n", ret, (void *)uuid);
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
    FIO_LOG_DEBUG("Flush empty for %p\n", (void *)uuid);
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
  FIO_LOG_DEBUG("Sent %zd bytes to %p\n", r, (void *)uuid);
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
  FIO_LOG_DEBUG("Copied %zu bytes to %p\n", can_copy, (void *)uuid);
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
  FIO_LOG_DEBUG("The `before_close` callback was called for %p\n",
                (void *)uuid);
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

/* *****************************************************************************
SSL/TLS API implementation
***************************************************************************** */

/**
 * Creates a new SSL/TLS context / settings object with a default certificate
 * (if any).
 */
fio_tls_s *__attribute__((weak))
fio_tls_new(const char *server_name, const char *key, const char *cert) {
  REQUIRE_LIBRARY();
  fio_tls_s *tls = calloc(sizeof(*tls), 1);
  return tls;
  (void)server_name;
  (void)key;
  (void)cert;
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
  (void)tls;
  (void)server_name;
  (void)key;
  (void)cert;
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
  if (!tls->callback)
    tls->callback = callback;
  (void)tls;
  (void)protocol_name;
  (void)callback;
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
  (void)tls;
  (void)udata;
  fio_rw_hook_set(uuid, &FIO_TLS_HOOKS,
                  fio_malloc(sizeof(buffer_s))); /* 32Kb buffer */
  if (tls->callback)
    tls->callback(uuid, udata);
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
  (void)tls;
  (void)udata;
  fio_rw_hook_set(uuid, &FIO_TLS_HOOKS,
                  fio_malloc(sizeof(buffer_s))); /* 32Kb buffer */
  if (tls->callback)
    tls->callback(uuid, udata);
}
#pragma weak fio_tls_connect

/**
 * Destroys the SSL/TLS context / settings object and frees any related
 * resources / memory.
 */
void __attribute__((weak)) fio_tls_destroy(fio_tls_s *tls) {
  REQUIRE_LIBRARY();
  free(tls);
}
#pragma weak fio_tls_destroy
