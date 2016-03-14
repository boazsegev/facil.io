/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "tls-lib-server.h"

/* Only available if we have OpenSSL */
#ifdef SSL_VERIFY_PEER

#define PRINT_MESSAGES 1
/* We have OpenSSL - let's do this :-) */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

/*******************************************************************************
Function declerations
*/

/** Used to initialize the TLS/SSL hooks from within the on_init callback. */
static void init_server(server_pt srv);
/** Used to update the sever settings with a new TLS/SSL Protocol and hooks.
 * (this will also setup the on_init callback and make sure that the original
 * callback is called as well).
 */
static void update_settings(struct ServerSettings* settings);

/*******************************************************************************
The API object
*/
struct TLSServer_API___ TLSServer = {
    .init_server = init_server,
    .update_settings = update_settings,
};

/*******************************************************************************
The TLS Protocol and it's components
*/
struct TLSConnectionData {
  /* temp data feilds. */
  int fd;
  /* state flags require only a single bit... */
};
struct TLSProtocol {
  /** The TLS protocol must be the first element, for poiner inheritance. */
  struct Protocol tls;
  /** The clear-text protocol to be wrapped in the TLS layer. */
  struct Protocol* original_protocol;
  /** The original on_init callback, if any. */
  void (*on_init)(server_pt server);
  void (*on_finish)(server_pt server);
  /** The connection data array. */
  struct TLSConnectionData* data;
  /** The original timeout (to be used after the handshake). */
  unsigned char timeout;
};

#define tls_protocol(srv) \
  ((struct TLSProtocol*)(Server.settings(srv)->protocol))
/*******************************************************************************
Protocol and Server callbacks
*/

static ssize_t writing_hook(server_pt srv, int fd, void* data, size_t len) {
  if (PRINT_MESSAGES)
    fprintf(stderr, "Sending in the clear (%ld bytes): %.*s\n", len, (int)len,
            data);
  int sent = write(fd, data, len);
  if (sent < 0 && (errno & (EWOULDBLOCK | EAGAIN | EINTR)))
    sent = 0;
  return sent;
}

static ssize_t reading_hook(server_pt srv, int fd, void* buffer, size_t size) {
  ssize_t read = 0;
  if ((read = recv(fd, buffer, size, 0)) > 0) {
    if (PRINT_MESSAGES)
      fprintf(stderr, "Got clear text (%ld bytes): %.*s\n", read, (int)read,
              buffer);
    return read;
  } else {
    if (read && (errno & (EWOULDBLOCK | EAGAIN)))
      return 0;
  }
  return -1;
}

static void on_open(server_pt srv, int fd) {
  if (PRINT_MESSAGES)
    fprintf(stderr, "A new TLS connection? sorry, not implemented\n");
}
static void on_close(server_pt srv, int fd) {
  if (PRINT_MESSAGES)
    fprintf(stderr, "SSL/TLS handshake failed.\n");
}
static void on_data(server_pt srv, int fd) {
  if (PRINT_MESSAGES)
    fprintf(stderr, "TLS connections aren't implemented, updating protocol.\n");
  struct TLSProtocol* tls = tls_protocol(srv);
  if (!tls)
    return;
  // hook read-write
  Server.rw_hooks(srv, fd, reading_hook, writing_hook);
  // give the connection control to the original protocol
  if (Server.set_protocol(srv, fd, tls->original_protocol)) {
    if (PRINT_MESSAGES)
      fprintf(stderr, "SSL/TLS ERROR: cannot set the connection's protocol\n");
    return;
  };
  // set the correct timeout
  Server.set_timeout(srv, fd, tls->timeout);
  // call callbacks
  if (tls->original_protocol->on_open)
    tls->original_protocol->on_open(srv, fd);
  if (tls->original_protocol->on_data)  // in case there's data to be read.
    tls->original_protocol->on_data(srv, fd);
}
static void on_ready(server_pt srv, int fd) {
  // Should we continue the OpenSSL `accept` here? ... it requires writing as
  // well as reading...
  if (PRINT_MESSAGES)
    fprintf(stderr,
            "SSL/TLS handshake should continue? only if it isn't running in "
            "parallel...\n");
}

static void on_finish(server_pt srv) {
  struct TLSProtocol* tls = tls_protocol(srv);
  if (tls->on_finish)
    tls->on_finish(srv);
  // free the connection data array
  if (tls->data) {
    free(tls->data);
    tls->data = NULL;
  }
  // free the protocol
  free(tls);
}

/*******************************************************************************
Global initialization
*/
static void global_library_init() {
  volatile static char initialized = 0;
  if (initialized)
    return;
  // this isn't a mutex, but it's a good enough solution for the unlikely
  // chance of a race condition.
  for (size_t i = 0; i < 8; i++) {
    if (initialized >> i)
      return;
    initialized |= (1 << i);
  }
  if (PRINT_MESSAGES)
    fprintf(stderr, "Initializing TLS library.\n");
  // call the initialization functions
  SSL_library_init();  // or OPENSSL_init_ssl() in version 1.1.0
  // OpenSSL_add_all_algorithms(); // - do we need this?
}

/*******************************************************************************
Function implementation
*/

/** Used to initialize the TLS/SSL hooks from within the on_init callback. */
static void init_server(server_pt srv) {
  if (PRINT_MESSAGES)
    fprintf(stderr, "Initializing TLS layer.\n");
  struct ServerSettings* settings = Server.settings(srv);
  settings->on_init = NULL;
  update_settings(settings);

  // call the original on_init callback, unless it's the same as this function.
  if (tls_protocol(srv)->on_init && tls_protocol(srv)->on_init != init_server)
    tls_protocol(srv)->on_init(srv);
}
/** Used to update the sever settings with a new TLS/SSL Protocol and hooks.
 * (this will also setup the on_init callback and make sure that the original
 * callback is called as well).
 */
static void update_settings(struct ServerSettings* settings) {
  if (settings->on_init == init_server)  // settings already updated.
    return;
  // initialize the library
  global_library_init();
  // create a new TLSProtocol object and initialize the data
  struct TLSProtocol* tls = malloc(sizeof(*tls));
  if (!tls) {
    perror("Cannot allocate memory");
    exit(1);  // we do not fail quitely.
  }
  tls->data = calloc(Server.capacity(), sizeof(*(tls->data)));
  if (!tls->data) {
    free(tls);
    perror("Cannot allocate memory");
    exit(1);  // we do not fail quitely.
  }
  // initialize the TLSProtocol data.
  tls->tls.on_open = on_open;
  tls->tls.on_data = on_data;
  tls->tls.on_ready = on_ready;
  tls->tls.on_close = on_close;
  tls->on_init = settings->on_init;
  tls->on_finish = settings->on_finish;
  tls->original_protocol = settings->protocol;
  tls->timeout = settings->timeout;
  // update the settings
  settings->protocol = (void*)tls;
  settings->on_finish = on_finish;
  settings->on_init = init_server;
  settings->timeout = 5;
}

/* End OpenSSL available section*/
#endif /* SSL_VERIFY_PEER */
