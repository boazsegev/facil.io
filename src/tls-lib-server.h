/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef TLS_LIB_SERVER
#define TLS_LIB_SERVER

/** \file
NOT IMPLEMENTED - this is a stub version, that isn't implemented yet! No SSL/TLS
will be added to the server.

This extension implements SSL/TLS connections for lib-server. This is done using
lib-server's read/write hooks and a wrapper protocol - these techniques can be
used to add other transport and/or protocol negotiation layers as well.

This extension requires OpenSSL to be installed and linked. The OpenSSL header
file should be included BEFORE this header file. If OpenSSL isn't available, a
warning will be displayed, but COMPILATION WILL CONTINUE (unless set to fail).

The choice for a default quite failure was chosen so that this extension could
be stored in the project folder even when not in use.
*/

/* for testing */
// #define SSL_VERIFY_PEER 1

#ifdef SSL_VERIFY_PEER
/* We have OpenSSL - let's do this :-) */
#include "lib-server.h"

/**
NOT IMPLEMENTED - this is a stub version, that isn't implemented yet! No SSL/TLS
will be added to the server.

The TLSServer API is available using the global `TLSServer` object.

This API adds SSL/TLS functionality to lib-server by establishing Read/Write
hooks for SSL/TLS enabled connections and managing protocol entry to make sure
the server's default protocol isn't called upon before the SSL/TLS handshake is
complete.

This extension requires OpenSSL to be installed and linked. The OpenSSL header
file should be included BEFORE this header file. If OpenSSL isn't available, a
warning will be displayed, but COMPILATION WILL CONTINUE (unless set to fail).

Use:

    // from within the server's on_init callback (or as the on_init callback):
    TLSServer.init_server(srv);

*/

struct TLSServer_API___ {
  /** Used to initialize the TLS/SSL hooks from within the on_init callback. */
  void (*init_server)(server_pt srv);
  /** Used to update the sever settings with a new TLS/SSL Protocol and hooks.
   * (this will also setup the on_init callback and make sure that the original
   * callback is called as well).
   */
  void (*update_settings)(struct ServerSettings* settings);
} TLSServer;

/* TLS-Lib-Server: Not implememnted */
#warning TLS-Lib-Server: Not implememnted.

/* End OpenSSL available section */
#elif defined(REQUIRE_TLS)
#warning Open SSL header data wasn't found - to use TLS-Lib-Server, include the ssl.h header before the tls-lib-server.h header.
#else
#warning Open SSL header data wasn't found - to use TLS-Lib-Server, include the ssl.h header before the tls-lib-server.h header.
#endif /* SSL_VERIFY_PEER */

#endif /* TLS_LIB_SERVER */
