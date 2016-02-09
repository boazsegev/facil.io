/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "libbuffer.h"
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
///////////////////
// The buffer class ID
static const void* BufferClassID;

///////////////////
// The packets
struct Packet {
  size_t length;
  struct Packet* next;
  void* data;
};

static void free_packet(struct Packet* packet) {
  if (packet->data) {
    if (packet->length)
      free(packet->data);
    else
      fclose(packet->data);
  }
  free(packet);
}
///////////////////
// The buffer structor
struct Buffer {  // 88 bytes pet buffer
  void* id;
  // a data locker.
  pthread_mutex_t lock;
  // pointer to the actual data.
  struct Packet* packet;
  // the amount of data sent from the first packet
  size_t sent;
};

///////////////////
// helpers

static inline int is_buffer(struct Buffer* object) {
  // if (object->id != &BufferClassID)
  //   printf("ERROR, Buffer received a non buffer object\n");
  return object->id == &BufferClassID;
}

///////////////////
// The functions

static inline void* new_buffer(size_t offset) {
  struct Buffer* buffer = malloc(sizeof(struct Buffer));
  if (!buffer)
    return 0;
  *buffer = (struct Buffer){//.lock = PTHREAD_MUTEX_INITIALIZER,
                            .id = &BufferClassID,
                            .sent = offset,
                            .packet = NULL};

  if (pthread_mutex_init(&buffer->lock, NULL)) {
    free(buffer);
    return 0;
  }
  return buffer;
}

// clears all the buffer data
static inline void clear_buffer(struct Buffer* buffer) {
  if (is_buffer(buffer)) {
    pthread_mutex_lock(&buffer->lock);
    struct Packet* to_free = NULL;
    while ((to_free = buffer->packet)) {
      buffer->packet = buffer->packet->next;
      free_packet(to_free);
    }
    pthread_mutex_unlock(&buffer->lock);
  }
}

// destroys the buffer
static inline void destroy_buffer(struct Buffer* buffer) {
  if (is_buffer(buffer)) {
    clear_buffer(buffer);
    pthread_mutex_destroy(&buffer->lock);
    free(buffer);
  }
}

// takes data and places it into the end of the buffer
static size_t buffer_move(struct Buffer* buffer, void* data, size_t length) {
  if (!is_buffer(buffer))
    return 0;
  struct Packet* np = malloc(sizeof(struct Packet));
  if (!np)
    return 0;
  np->data = data;
  np->length = length;
  np->next = NULL;
  pthread_mutex_lock(&buffer->lock);
  struct Packet** pos = &buffer->packet;
  while (*pos) {
    pos = &(*pos)->next;
  }
  *pos = np;
  pthread_mutex_unlock(&buffer->lock);
  return length;
}

// takes data, copies it and pushes it into the buffer
static size_t buffer_copy(struct Buffer* buffer, void* data, size_t length) {
  void* cpy = NULL;
  if (data && length) {
    cpy = malloc(length);
    if (!cpy)
      return 0;
    memcpy(cpy, data, length);
  }
  if (!buffer_move(buffer, cpy, length)) {
    if (cpy)
      free(cpy);
    return 0;
  }
  return length;
}

// urgent buffer logic
static size_t buffer_next_logic(struct Buffer* buffer,
                                void* data,
                                size_t length,
                                char copy) {
  if (!is_buffer(buffer))
    return 0;
  struct Packet* np = malloc(sizeof(struct Packet));
  if (!np)
    return 0;

  if (copy) {
    np->data = malloc(length);
    if (!np->data) {
      free(np);
      return 0;
    }
    memcpy(np->data, data, length);
  } else {
    np->data = data;
  }
  np->length = length;

  pthread_mutex_lock(&buffer->lock);
  struct Packet** pos = &buffer->packet;
  // if the next packet's length is 0, it is a file packet.
  // file packets insert packets before themselves... so we must wait.
  if (buffer->packet && buffer->packet->next && !buffer->packet->next->length)
    pos = &buffer->packet->next->next;
  // never interrupt a packet in the middle.
  else if (buffer->sent && buffer->packet)
    pos = &buffer->packet->next;
  if (*pos)
    np->next = (*pos)->next;
  else
    np->next = 0;
  (*pos) = np;
  pthread_mutex_unlock(&buffer->lock);
  return length;
}

// takes data, copies it, and places it at the front of the buffer
static size_t buffer_next(struct Buffer* buffer, void* data, size_t length) {
  return buffer_next_logic(buffer, data, length, 1);
}

// takes data, and places it at the front of the buffer
static size_t buffer_move_next(struct Buffer* buffer,
                               void* data,
                               size_t length) {
  return buffer_next_logic(buffer, data, length, 0);
}

static ssize_t buffer_flush(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return -1;
  struct Packet* to_free;
  ssize_t sent = 0;
  pthread_mutex_lock(&buffer->lock);
start_flush:
  // no packets to send
  if (!buffer->packet) {
    pthread_mutex_unlock(&buffer->lock);
    return 0;
  }
  // a NULL packet (data is NULL) means: "Close the connection"
  if (!buffer->packet->data) {
    close(fd);
    goto clear_buffer;
  }
  // a Packet with data but no length is a FILE * to be sent
  if (!buffer->packet->length) {
    // allocate buffer of 65,536 Bytes
    struct Packet* np = malloc(sizeof(struct Packet));
    if (!np)
      goto skip_file;
    np->data = malloc(65536);
    if (!np->data) {
      free(np);
      goto skip_file;
    }
    size_t read_len = fread(np->data, 1, 65536, (FILE*)buffer->packet->data);
    if (read_len <= 0) {
      free(np->data);
      free(np);
      goto skip_file;
    }
    np->length = read_len;
    // insert the new packet (np) before the file packet and switch references
    np->next = buffer->packet;
    buffer->packet = np;
    if (feof((FILE*)buffer->packet->next->data)) {
      // we have reached the end of the file...
      // np will now hold the file packet
      np = buffer->packet->next;
      // we remove the file packet from the chain
      buffer->packet->next = np->next;
      // we free the file packet
      free_packet(np);
    }
    goto start_flush;
  skip_file:
    np = buffer->packet;
    buffer->packet = buffer->packet->next;
    free_packet(np);
    goto start_flush;
  }
  sent = write(fd, buffer->packet->data + buffer->sent,
               buffer->packet->length - buffer->sent);
  if (sent < 0 && !(errno & (EWOULDBLOCK | EAGAIN))) {
    close(fd);
    goto clear_buffer;
  } else if (sent > 0) {
    buffer->sent += sent;
  }
  if (buffer->sent >= buffer->packet->length) {
    to_free = buffer->packet;
    buffer->sent = 0;
    buffer->packet = buffer->packet->next;
    free_packet(to_free);
    // a NULL packet (data is NULL) means: "Close the connection"
    if (buffer->packet && !buffer->packet->data) {
      close(fd);
      goto clear_buffer;
    }
  }
  pthread_mutex_unlock(&buffer->lock);
  return sent;
clear_buffer:
  while (buffer->packet) {
    to_free = buffer->packet;
    buffer->packet = buffer->packet->next;
    free_packet(to_free);
  }
  pthread_mutex_unlock(&buffer->lock);
  return sent;
}

static int buffer_sendfile(struct Buffer* buffer, FILE* file) {
  if (!is_buffer(buffer))
    return -1;
  return buffer_move(buffer, file, 0);
}

static void buffer_close_w_d(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return;
  if (!buffer->packet)
    close(fd);
  else
    buffer_move(buffer, NULL, fd);
}
/** returns the sizes of all the pending data packets, excluding files (yet to
 * be implemented). */
size_t buffer_pending(struct Buffer* buffer) {
  if (!is_buffer(buffer))
    return 0;
  size_t len = 0;
  struct Packet* p;
  pthread_mutex_lock(&buffer->lock);
  p = buffer->packet;
  while (p) {
    if (p->data && p->length)
      len += p->length;
    else if (p->data)
      len += 1;  // if it's a file - can we check it's size? expensive?
    else
      break;  // no need to move beyond a close connection packet.
    p = p->next;
  }
  len -= buffer->sent;
  pthread_mutex_unlock(&buffer->lock);
  return len;
}

/** returns true (1) if the buffer is empty, otherwise returns false (0). */
char buffer_empty(struct Buffer* buffer) {
  if (!is_buffer(buffer))
    return 1;
  return buffer->packet == NULL;
}

///////////////////
// The interface

const struct BufferClass Buffer = {
    .new = new_buffer,
    .destroy = (void (*)(void*))destroy_buffer,
    .clear = (void (*)(void*))clear_buffer,
    .sendfile = (int (*)(void*, FILE*))buffer_sendfile,
    .write = (size_t (*)(void*, void*, size_t))buffer_copy,
    .write_move = (size_t (*)(void*, void*, size_t))buffer_move,
    .write_next = (size_t (*)(void*, void*, size_t))buffer_next,
    .write_move_next = (size_t (*)(void*, void*, size_t))buffer_move_next,
    .flush = (ssize_t (*)(void*, int))buffer_flush,
    .close_when_done = (void (*)(void*, int))buffer_close_w_d,
    .pending = (size_t (*)(void*))buffer_pending,
    .empty = (char (*)(void*))buffer_empty,
};
