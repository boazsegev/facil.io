/**
In this example we will author a simple protocol example.

In our example we will ignore the client's innput completely and return an
HTTP/1.1 response (this is bad behavior, but your browser might not notice).

Since our protocol isn't real (no real world application will behave this way),
I'm calling it the pseudo protocol.
*/

/* include the core library, without any extensions */
#include "facil.h"

/* a simple HTTP/1.1 response */
static char *HTTP_RESPONSE = "HTTP/1.1 200 OK\r\n"
                             "Content-Length: 12\r\n"
                             "Connection: keep-alive\r\n"
                             "Content-Type: text/plain\r\n"
                             "\r\n"
                             "Hello World!";

/* this will be called when the connection has incoming data. */
void pseudo_on_data(intptr_t uuid, protocol_s *pr) {
  static char buff[4096];
  sock_read(uuid, buff, 4096);
  sock_write2(.uuid = uuid, .dealloc = SOCK_DEALLOC_NOOP,
              .buffer = HTTP_RESPONSE, .length = 101);
  /* we aren't using this in this example */
  (void)pr;
}

/* this will be called when the connection is closed. */
void pseudo_on_close(intptr_t uuid, protocol_s *pr) {
  free(pr);
  (void)uuid;
}

/* this will be called when a connection is opened. */
void pseudo_on_open(intptr_t uuid, void *udata) {
  /*
   * we should allocate a protocol object for this connection.
   *
   * since protocol objects get locked and all that, we need a different
   * protocol object per connection.
   */
  protocol_s *p = malloc(sizeof(*p));
  *p = (protocol_s){
      .service = "Cheating at HTTP", /* our protocol identifier */
      .on_data = emulate_on_data,    /* setting the data callback */
      .on_close = emulate_on_close,  /* setting the close callback */
  };
  /* timeouts are important. timeouts are in seconds. */
  facil_set_timeout(uuid, 5);
  /*
   * this is a very IMPORTANT function call,
   * it attaches the protocol to the socket.
   */
  facil_attach(uuid, p);
  /* we aren't using this in this example */
  (void)udata;
}

/* our main function / starting point */
int main(void) {
  /* try to listen on port 3000. */
  if (facil_listen(.port = "3000", .address = NULL, .on_open = pseudo_on_open,
                   .udata = NULL))
    perror("FATAL ERROR: Couldn't open listening socket"), exit(errno);
  /* run facil with 1 working thread - this blocks until we're done. */
  facil_run(.threads = 1);
  ;
  return 0;
}
