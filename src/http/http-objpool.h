/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_OBJECT_POOL_H
#define HTTP_OBJECT_POOL_H

#define LIB_OBJECT_POOL_VERSION "0.2.0"

typedef struct ObjectPool* object_pool;

/**
ObjectPool is a helper library for creating dynamic object pools for objects
that don't naturally form a list. It's used for pooling HttpRequest and
HttpResponse objects.

Use using the global ObjectPool object.

i.e.

    void * make_object(void) {malloc(100);}
    void free_object(void * obj) {free(obj);}

    int main() {
      object_pool p = ObjectPool.create_dynamic(make_object, free_object, 0);
      void * obj = ObjectPool.pop(p); // get an object
      ObjectPool.push(obj); // store it back in the pool (careful)
      obj = NULL;
      ObjectPool.destroy(p); // destroy the pool
    }
*/
extern struct ObjectPool_API__ {
  /**
Initialize a new ObjectPool that grows when there aren't available objects.

void* (*create)(void* arg):: a callback that returns a new object instance.
void* (*destroy)(void* object):: a callback that destroys an object.
void* arg:: a pointer that will be passed to the `create` callback.
size:: the (initial) number of items in the pool.
  */
  void* (*create_dynamic)(void* (*create)(void),
                          void (*destroy)(void* object),
                          int size);
  /**
Initialize a new ObjectPool that blocks and waits when there aren't any
available objects.

void* (*create)(void* arg):: a callback that returns a new object instance.
void* (*destroy)(void* object):: a callback that destroys an object.
void* arg:: a pointer that will be passed to the `create` callback.
size:: the (initial) number of items in the pool.
  */
  void* (*create_blocking)(void* (*create)(void),
                           void (*destroy)(void* object),
                           int size);
  /**
Destroys the pool object and any items in the pool.
  */
  void (*destroy)(object_pool pool);
  /**
Grabs an object from the pool, removing it from the pool's
registry.

The object will be removed from the pool until returned using `ObjectPool.push`.
  */
  void* (*pop)(object_pool pool);
  /**
Returns an object (or pushes a new object) to the pool, making it available for
future `ObjectPool.pop` calls.
  */
  void (*push)(object_pool, void* object);
  /**
  Returns the (approximate) number of objects available the pool.
  */
  int (*count)(object_pool pool);
} ObjectPool;

#endif /* end of include guard: HTTP_OBJECT_POOL_H */
