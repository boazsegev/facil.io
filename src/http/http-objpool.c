/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "http-objpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/////////////////////////////////////////////////////////
// The ObjectPool

struct ObjectContainer {
  void* object;
  struct ObjectContainer* next;
};

struct ObjectPool {
  pthread_mutex_t lock;
  struct ObjectContainer* objects;
  struct ObjectContainer* containers;
  void* (*create)(void);
  void (*destroy)(void* object);
  int object_count;
  int is_waiting;
  int wait_in;
  int wait_out;
};

/////////////////////////////////////////////////////////
// The API

/**
Returns the (approximate) number of objects available the pool.
*/
int pool_count(object_pool pool) {
  return pool->object_count;
}

/**
Grabs an object from the pool, removing it from the pool's
registry.

The object will be removed from the pool until returned using `push`.
*/
static void* pop(object_pool pool) {
  pthread_mutex_lock(&pool->lock);
  void* object = NULL;
  struct ObjectContainer* c = pool->objects;
  if (c) {
    // we have an availavle object.
    object = c->object;
    // step the objects pool forward
    pool->objects = c->next;
    // move the object's container to the container pool
    c->next = pool->containers;
    pool->containers = c;
    // update the object count
    pool->object_count--;
    // unlock
    pthread_mutex_unlock(&pool->lock);
    // return the object
    return object;
  } else {
    if (pool->wait_in) {
      // this is a blocking object pool - update waiting count
      pool->is_waiting++;
      pthread_mutex_unlock(&pool->lock);
      // time to block
      if (read(pool->wait_out, &(c), 1))
        ;
      return pop(pool);
    } else {
      // reset the object count
      pool->object_count = 0;
      // this is a dynamic object pool - create a new object.
      pthread_mutex_unlock(&pool->lock);
      return pool->create();
    }
  }
  return 0;
}
/**
Returns an object (or pushes a new object) to the pool, making it available
for future `pop` calls.
*/
static void push(object_pool pool, void* object) {
  pthread_mutex_lock(&pool->lock);
  struct ObjectContainer* c = pool->containers;
  if (c) {
    pool->containers = c->next;
  } else {
    c = malloc(sizeof(struct ObjectContainer));
  }
  c->next = pool->objects;
  c->object = object;
  pool->objects = c;
  // update the object count
  pool->object_count++;
  // send a signal if someone is waiting
  if (pool->is_waiting) {
    if (write(pool->wait_in, &c, 1))
      ;
    pool->is_waiting--;
  }
  pthread_mutex_unlock(&pool->lock);
}

/**
Initialize a new ObjectPool that grows when there aren't available objects.

void* (*create)(void* arg):: a callback that returns a new object instance.
void* (*destroy)(void* object):: a callback that destroys an object.
void* arg:: a pointer that will be passed to the `create` callback.
size:: the (initial) number of items in the pool.
*/
static void* new_dynamic(void* (*create)(void),
                         void (*destroy)(void* object),
                         int size) {
  if (!create)
    return NULL;
  struct ObjectPool* pool = malloc(sizeof(struct ObjectPool));
  pool->wait_in = pool->wait_out = pool->is_waiting = 0;
  pool->object_count = 0;
  pool->objects = NULL;
  pool->containers = NULL;
  pool->create = create;
  pool->destroy = destroy;
  if (!destroy)
    pool->destroy = free;
  pthread_mutex_init(&pool->lock, NULL);
  // create initial pool
  while (size--)
    push(pool, create());
  // return the pool object
  return pool;
}
/**
Initialize a new ObjectPool that blocks and waits when there aren't any
available objects.

void* (*create)(void* arg):: a callback that returns a new object instance.
void* (*destroy)(void* object):: a callback that destroys an object.
void* arg:: a pointer that will be passed to the `create` callback.
size:: the (initial) number of items in the pool.
*/
static void* new_blocking(void* (*create)(void),
                          void (*destroy)(void* object),
                          int size) {
  if (!create)
    return NULL;
  int io[2];
  if (pipe(io))
    return NULL;
  struct ObjectPool* pool = malloc(sizeof(struct ObjectPool));
  pool->object_count = 0;
  pool->is_waiting = 0;
  pool->wait_in = io[0];
  pool->wait_out = io[1];
  pool->objects = NULL;
  pool->containers = NULL;
  pool->create = create;
  pool->destroy = destroy;
  if (!destroy)
    pool->destroy = free;
  // create initial pool
  while (size--)
    push(pool, create());
  // return the pool object
  return pool;
}
/**
Destroys the pool object and any items in the pool.
*/
static void destroy(object_pool pool) {
  if (!pool)
    return;
  struct ObjectContainer* c;
  while (pool->containers) {
    c = pool->containers;
    pool->containers = c->next;
    free(c);
  }
  while (pool->objects) {
    c = pool->objects;
    pool->objects = c->next;
    pool->destroy(c->object);
    free(c);
  }
  pool->is_waiting = 0;
  if (pool->wait_in)
    close(pool->wait_in);
  if (pool->wait_out)
    close(pool->wait_out);
  pthread_mutex_destroy(&pool->lock);
  free(pool);
  return;
}

struct ObjectPool_API__ ObjectPool = {
    .create_dynamic = new_dynamic,
    .create_blocking = new_blocking,
    .destroy = destroy,
    .push = push,
    .pop = pop,
    .count = pool_count,
};
