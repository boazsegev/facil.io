/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "libbuffer.h"
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

///////////////////
// The pre-allocated memory per packet
#ifndef BUFFER_PACKET_SIZE
#define BUFFER_PACKET_SIZE (1024 * 64)
#endif
#ifndef BUFFER_MAX_PACKET_POOL
#define BUFFER_MAX_PACKET_POOL 100
#endif
///////////////////
// The packets
struct Packet {
  ssize_t length;
  struct Packet* next;
  void* data;
  char mem[BUFFER_PACKET_SIZE];
  struct {
    unsigned can_interrupt : 1;
    unsigned close_after : 1;
    unsigned rsrv : 6;
  } metadata;
};

///////////////////
// The global packet container pool
static struct {
  int ref_count;
  int pool_count;
  struct Packet* pool;
} ContainerPool = {0, 0};

static pthread_mutex_t container_pool_locker = PTHREAD_MUTEX_INITIALIZER;

static void register_buffer(void) {
  pthread_mutex_lock(&container_pool_locker);
  ContainerPool.ref_count++;
  pthread_mutex_unlock(&container_pool_locker);
}

static void unregister_buffer(void) {
  pthread_mutex_lock(&container_pool_locker);
  ContainerPool.ref_count--;
  if (!ContainerPool.ref_count) {
    struct Packet* to_free;
    while ((to_free = ContainerPool.pool)) {
      ContainerPool.pool = to_free->next;
      free(to_free);
    }
  }
  pthread_mutex_unlock(&container_pool_locker);
}

static struct Packet* get_packet(void) {
  struct Packet* packet;
  pthread_mutex_lock(&container_pool_locker);
  packet = ContainerPool.pool;
  if (packet) {
    ContainerPool.pool = packet->next;
    ContainerPool.pool_count--;
  } else {
    packet = malloc(sizeof(struct Packet));
  }
  pthread_mutex_unlock(&container_pool_locker);
  if (!packet)
    return 0;
  packet->data = packet->mem;
  packet->next = 0;
  packet->length = 0;
  *((char*)&packet->metadata) = 0;
  return packet;
}

static void free_packet(struct Packet* packet) {
  if (packet->data != packet->mem && packet->data) {
    if (packet->length)
      free(packet->data);
    else
      fclose(packet->data);
  }
  pthread_mutex_lock(&container_pool_locker);
  if (ContainerPool.pool_count <= BUFFER_MAX_PACKET_POOL) {
    packet->next = ContainerPool.pool;
    ContainerPool.pool = packet;
    ContainerPool.pool_count++;
  } else
    free(packet);
  pthread_mutex_unlock(&container_pool_locker);
}
///////////////////
// The buffer structure
struct Buffer {
  void* id;
  // pointer to the actual data.
  struct Packet* packet;
  // the amount of data sent from the first packet
  size_t sent;
  // a data locker.
  pthread_mutex_t lock;
};

///////////////////
// helpers

static inline int is_buffer(struct Buffer* object) {
  // if (object->id != is_buffer)
  //   printf("ERROR, Buffer received a non buffer object\n");
  return object->id == is_buffer;
}

///////////////////
// The functions

static inline void* new_buffer(size_t offset) {
  struct Buffer* buffer = malloc(sizeof(struct Buffer));
  if (!buffer)
    return 0;
  *buffer = (struct Buffer){//.lock = PTHREAD_MUTEX_INITIALIZER,
                            .id = is_buffer,
                            .sent = offset,
                            .packet = NULL};

  if (pthread_mutex_init(&buffer->lock, NULL)) {
    free(buffer);
    return 0;
  }
  register_buffer();
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
    unregister_buffer();
  }
}

// applys the move logic for either urgent or non urgent packets
static void insert_packets_to_buffer(struct Buffer* buffer,
                                     struct Packet* packet,
                                     char urgent) {
  pthread_mutex_lock(&buffer->lock);
  struct Packet *tail, **pos = &(buffer->packet);
  if (urgent) {
    while (*pos && (!(*pos)->next || !(*pos)->next->metadata.can_interrupt)) {
      pos = &((*pos)->next);
    }
  } else {
    while (*pos) {
      pos = &((*pos)->next);
    }
  }
  tail = (*pos);
  *pos = packet;
  if (tail) {
    pos = &(packet->next);
    while (*pos)
      pos = &((*pos)->next);
    *pos = tail;
  }
  pthread_mutex_unlock(&buffer->lock);
}

// takes data and places it into the end of the buffer
static inline size_t buffer_move_logic(struct Buffer* buffer,
                                       void* data,
                                       size_t length,
                                       char urgent) {
  if (!is_buffer(buffer))
    return 0;
  if (!length || !data) {
    fprintf(
        stderr,
        "Buffer: Canot move data because either length (%lu) or data (%p) are "
        "invalid\n",
        length, data);
    return 0;
  }
  struct Packet* np = get_packet();
  if (!np)
    return 0;
  np->data = data;
  np->length = length;
  np->next = NULL;
  *((char*)&np->metadata) = 0;
  np->metadata.can_interrupt = 1;
  insert_packets_to_buffer(buffer, np, urgent);
  return length;
}
static size_t buffer_move(struct Buffer* buffer, void* data, size_t length) {
  return buffer_move_logic(buffer, data, length, 0);
}
static size_t buffer_move_next(struct Buffer* buffer,
                               void* data,
                               size_t length) {
  return buffer_move_logic(buffer, data, length, 1);
}

// takes data, copies it and pushes it into the buffer
static size_t buffer_copy_logic(struct Buffer* buffer,
                                void* data,
                                size_t length,
                                char urgent) {
  if (!length || !data) {
    fprintf(
        stderr,
        "Buffer: Canot copy data because either length (%lu) or data (%p) are "
        "invalid\n",
        length, data);
    return 0;
  }
  size_t to_copy = length;
  struct Packet* np = get_packet();
  if (!np) {
    fprintf(stderr, "Couldn't allocate memory for the buffer (on copy)\n");
    return 0;
  }
  // set marker for packet interrupt
  np->metadata.can_interrupt = 1;
  struct Packet* tmp = np;
  while (to_copy) {
    if (to_copy > BUFFER_PACKET_SIZE) {
      memcpy(tmp->mem, data, BUFFER_PACKET_SIZE);
      tmp->data = tmp->mem;
      data += BUFFER_PACKET_SIZE;
      to_copy -= BUFFER_PACKET_SIZE;
      tmp->length = BUFFER_PACKET_SIZE;
      tmp->next = get_packet();
      if (!(tmp->next)) {
        fprintf(stderr, "Couldn't allocate memory for the buffer (on copy)\n");
        // free them all and return 0;
        tmp = np;
        while (tmp) {
          np = tmp;
          tmp = np->next;
          free_packet(np);
        }
        return 0;
      }
      tmp = tmp->next;
    } else {
      memcpy(tmp->mem, data, to_copy);
      tmp->data = tmp->mem;
      tmp->length = to_copy;
      to_copy = 0;
    }
  }
  insert_packets_to_buffer(buffer, np, urgent);
  return length;
}

// takes data, copies it and pushes it into the buffer
static size_t buffer_copy(struct Buffer* buffer, void* data, size_t length) {
  return buffer_copy_logic(buffer, data, length, 0);
}

// takes data, copies it, and places it at the front of the buffer
static size_t buffer_copy_next(struct Buffer* buffer,
                               void* data,
                               size_t length) {
  return buffer_copy_logic(buffer, data, length, 1);
}

// Flushes the buffer (writes as much as it can)...
// This is where a lot of the action takes place :-)
static ssize_t buffer_flush(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return -1;
  ssize_t sent = 0;
  struct Packet* packet;
  pthread_mutex_lock(&buffer->lock);
start_flush:
  // no packets to send
  if (!buffer->packet) {
    pthread_mutex_unlock(&buffer->lock);
    return 0;
  }
  // packet is a file
  if (!buffer->packet->length) {
    // make sure file sending isn't interrupted.
    buffer->packet->metadata.can_interrupt = 0;
    // grab a packet from the pool
    packet = get_packet();
    // read the data
    packet->length =
        fread(packet->data, 1, BUFFER_PACKET_SIZE, buffer->packet->data);
    // read less? done sending file
    if (packet->length < BUFFER_PACKET_SIZE) {
      if (packet->length <= 0) {  // no more data...
        // return the packet we got from the pool.
        free_packet(packet);
        // move the buffer one step forward.
        packet = buffer->packet;
        buffer->packet = buffer->packet->next;
        free_packet(packet);
        packet = NULL;
      } else {  // this will be the last the file will offer.
        // set the next packet.
        packet->next = buffer->packet->next;
        // free the file packet.
        free_packet(buffer->packet);
        // set the data packet as the buffer's packet.
        buffer->packet = packet;
      }
    } else {
      // set the next packet.
      packet->next = buffer->packet;
      // set the data packet as the buffer's packet, the file packet is next.
      buffer->packet = packet;
    }
    // make sure the sent property is reset.
    buffer->sent = 0;
    // restart the flush
    goto start_flush;
  }
  // the packet, at this point, is always a data packet. send the data.
  sent = write(fd, buffer->packet->data + buffer->sent,
               buffer->packet->length - buffer->sent);
  if (sent < 0 && !(errno & (EWOULDBLOCK | EAGAIN | EINTR))) {
    pthread_mutex_unlock(&buffer->lock);
    return -1;
  } else if (sent > 0) {
    buffer->sent += sent;
  }
  if (buffer->sent >= buffer->packet->length) {
    // review the close connection flag means: "Close the connection"
    if (buffer->packet->metadata.close_after) {
      close(fd);
      // buffer clearing should be performed by the Buffer's owner.
    }
    packet = buffer->packet;
    buffer->sent = 0;
    buffer->packet = buffer->packet->next;
    free_packet(packet);
  }
  pthread_mutex_unlock(&(buffer->lock));
  return sent;
}

static int buffer_sendfile(struct Buffer* buffer, FILE* file) {
  if (!is_buffer(buffer))
    return -1;
  struct Packet* np = get_packet();
  if (!np)
    return -1;
  np->data = file;
  np->metadata.can_interrupt = 1;
  insert_packets_to_buffer(buffer, np, 0);
  return 0;
}

static void buffer_close_w_d(struct Buffer* buffer, int fd) {
  if (!is_buffer(buffer))
    return;
  if (!buffer->packet) {
    close(fd);
    return;
  }
  pthread_mutex_lock(&buffer->lock);
  struct Packet* packet = buffer->packet;
  if (!packet)
    goto finish;
  while (packet->next)
    packet = packet->next;
  packet->metadata.close_after = 1;
finish:
  pthread_mutex_unlock(&buffer->lock);
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
    .write_next = (size_t (*)(void*, void*, size_t))buffer_copy_next,
    .write_move_next = (size_t (*)(void*, void*, size_t))buffer_move_next,
    .flush = (ssize_t (*)(void*, int))buffer_flush,
    .close_when_done = (void (*)(void*, int))buffer_close_w_d,
    .pending = (size_t (*)(void*))buffer_pending,
    .empty = (char (*)(void*))buffer_empty,
};
