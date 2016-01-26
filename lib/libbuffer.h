/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_BUFFER_H
#define LIB_BUFFER_H

#include <stdlib.h>
#include <sys/types.h>

/************************************************/ /**
This library introduces a packet based Buffer object for network data output.
The buffer is pretty much a wrapper for a binary tree with mutexes.

To Create a Buffer use:

`void * Buffer.new(0)` - the offset sets a pre-sent amount for the first packet.

To destroy a Buffer use:

`void Buffer.destroy(void * buffer)`

To add data to a Buffer use any of:

* `write` will copy the data to a new buffer packet.

`size_t Buffer.write(void * buffer, void * data, size_t length)`

* `write_move` will take ownership of the data, wrap it in a buffer packet and
free the mempry once the packet was sent.

     A NULL packet sent using write_move will close the connection once it had
been reached (all previous data was sent) - same as `close_when_done`.

`size_t Buffer.write_move(void * buffer, void * data, size_t length)`

* `write_next` will COPY the data, and place it as the first packet in the
queue. `write_next` will protect the current packet from being interrupted in
the middle and the data will be sent as soon as possible without cutting any
packets in half.

`size_t Buffer.write_next(void * buffer, void * data, size_t length)`

To send data from a Buffer to a socket / pipe (file descriptor) use:

`size_t Buffer.flush(void * buffer, int fd)`
*/

extern const struct BufferClass {
  void* (*new)(size_t offset);
  void (*destroy)(void* buffer);
  void (*clear)(void* buffer);
  ssize_t (*flush)(void* buffer, int fd);
  size_t (*write)(void* buffer, void* data, size_t length);
  size_t (*write_move)(void* buffer, void* data, size_t length);
  size_t (*write_next)(void* buffer, void* data, size_t length);
  size_t (*write_move_next)(void* buffer, void* data, size_t length);
  void (*close_when_done)(void* buffer, int fd);
  size_t (*pending)(void* buffer);
} Buffer;
#endif
