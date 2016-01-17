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

///////////////////
// The buffer structor
struct Buffer {
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
  *buffer = (struct Buffer){.lock = PTHREAD_MUTEX_INITIALIZER,
                            .id = &BufferClassID,
                            .sent = offset,
                            .packet = NULL};

  if (pthread_mutex_init(&buffer->lock, NULL)) {
    free(buffer);
    return 0;
  }
  return buffer;
}
static inline void destroy_buffer(struct Buffer* buffer) {
  if (is_buffer(buffer)) {
    pthread_mutex_lock(&buffer->lock);
    void* to_free = NULL;
    while ((to_free = buffer->packet)) {
      buffer->packet = buffer->packet->next;
      free(to_free);
    }
    pthread_mutex_unlock(&buffer->lock);
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
  void* cpy = malloc(length);
  if (!cpy)
    return 0;
  memcpy(cpy, data, length);
  if (!buffer_move(buffer, cpy, length)) {
    free(cpy);
    return 0;
  }
  return length;
}

// takes data, copies it, and places it at the front of the buffer
static size_t buffer_next(struct Buffer* buffer, void* data, size_t length) {
  if (!is_buffer(buffer))
    return 0;
  struct Packet* np = malloc(sizeof(struct Packet) + length);
  if (!np)
    return 0;
  np->data = np + sizeof(struct Packet);
  memcpy(np->data, data, length);
  np->length = length;

  pthread_mutex_lock(&buffer->lock);
  struct Packet** pos = &buffer->packet;
  if (buffer->sent && buffer->packet)
    pos = &buffer->packet->next;
  np->next = (*pos)->next;
  (*pos) = np;
  pthread_mutex_unlock(&buffer->lock);
  return length;
}

static ssize_t buffer_flush(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return -1;
  pthread_mutex_lock(&buffer->lock);
  if (!buffer->packet) {
    pthread_mutex_unlock(&buffer->lock);
    return 0;
  }
  if (!buffer->packet->data) {
    pthread_mutex_unlock(&buffer->lock);
    close(fd);
    return 0;
  }
  ssize_t sent = write(fd, buffer->packet->data + buffer->sent,
                       buffer->packet->length - buffer->sent);
  if (sent < 0 && !(errno & (EWOULDBLOCK | EAGAIN))) {
    close(fd);
  }
  if (sent > 0) {
    buffer->sent += sent;
  }
  if (buffer->sent >= buffer->packet->length) {
    struct Packet* to_free = buffer->packet;
    buffer->sent = 0;
    buffer->packet = buffer->packet->next;
    free(to_free->data);
    free(to_free);
    if (buffer->packet && !buffer->packet->data) {
      close(fd);
      to_free = buffer->packet;
      buffer->packet = buffer->packet->next;
      free(to_free);
    }
  }
  pthread_mutex_unlock(&buffer->lock);
  return sent;
}

static void buffer_close_w_d(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return;
  buffer_move(buffer, NULL, fd);
}

size_t buffer_pending(struct Buffer* buffer) {
  if (!is_buffer(buffer))
    return 0;
  size_t len = 0;
  struct Packet* p;
  pthread_mutex_lock(&buffer->lock);
  p = buffer->packet;
  while (p) {
    len += p->length;
    p = p->next;
  }
  len -= buffer->sent;
  pthread_mutex_unlock(&buffer->lock);
  return len;
}

///////////////////
// The interface

const struct BufferClass Buffer = {
    .new = new_buffer,
    .destroy = (void (*)(void*))destroy_buffer,
    .write = (size_t (*)(void*, void*, size_t))buffer_copy,
    .write_move = (size_t (*)(void*, void*, size_t))buffer_move,
    .write_next = (size_t (*)(void*, void*, size_t))buffer_next,
    .flush = (ssize_t (*)(void*, int))buffer_flush,
    .close_when_done = (void (*)(void*, int))buffer_close_w_d,
    .pending = (size_t (*)(void*))buffer_pending,
};
