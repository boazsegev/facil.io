# A Simple HashMap

The hash map library in `fio_hashmap.h`

The simple Hash Map offers a simple key-value map with a very simple API.

The simple Hash Map type is included in a single file library, `fio_hashmap.h` that can be used independently as well.

The Hash Map defaults to `uint64_t` keys, meaning that matching is performed using a simple integer comparison. However, this could be changed by defining macros before including the library file.

Much like the Array in [the introduction to the simple core types](types.md), Hash Map containers can be placed on the stack as well as allocated dynamically.

## Collision protection

The Hash Map is collision resistant as long as it's keys are truly unique.

If there's a chance that the default `uint64_t` key type will not be be able to uniquely identify a key, the following macros should **all** be defined, allowing the default key system to be replaced:

* `FIO_HASH_KEY_TYPE`
  
  This macro sets the type used for keys.

* `FIO_HASH_KEY_INVALID`    
    
    Empty slots in the Hash Map are initialized so all their bytes are zero.

    This macro should signify a static key that has all it's byte set to zero, making it an invalid key (it cannot be used, and objects placed in that slot will be lost).

* `FIO_HASH_KEY2UINT(key)`

    This macro should convert the key to a unique unsigned number.

    This, in effect, should return the hash value for the key and cannot be zero.

    The value is used to determine the location of the key in the map (prior to any collisions) and a good hash will minimize collisions.

* `FIO_HASH_COMPARE_KEYS(k1, k2)`

    This macro should compare two keys, excluding their hash values (which were compared using the `FIO_HASH_KEY2UINT` macro).

* `FIO_HASH_KEY_ISINVALID(key)`

    Should evaluate as true if the key is an invalid key (all it's bytes set to zero).

* `FIO_HASH_KEY_COPY(key)`

    Keys might contain temporary data (such as strings). To allow the Hash Map to test the key even after the temporary data is out of scope, ac opy needs to be created.

    This macro should return a FIO_HASH_KEY_TYPE object that contains only persistent data. This could achieved by allocating some of the data using `malloc`.

* `FIO_HASH_KEY_DESTROY(key)`

    When the Hash Map is re-hashed, old keys belonging to removed objects are cleared away and need to be destroyed.

    This macro allows dynamically allocated memory to be freed (this is the complement of `FIO_HASH_KEY_COPY`).

    **Note**: This macro must not end a statement (shouldn't use the `;` marker) or code blocks (`{}`). For multiple actions consider using inline functions.

If the `FIO_HASH_KEY_COPY` macro allocated memory dynamically or if there's a need to iterate over the values in the Hash Map before freeing the Hash Map (perhaps to free the object's memory), the `FIO_HASH_FOR_FREE` macro can be used to iterate over the Hash Map, free all the keys and free the Hash Map resources (it calls `fio_hash_free`).

### Example

The following macros can be used to define a String key that protects against collisions and requires the whole string to match.

In this example I'm forcing collisions by setting the `hash` and string length to be equal for all keys, but in a real situation make sure the `hash` values are as unique as can be.


```c
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* the hash key type for string keys */
typedef struct {
  size_t hash;
  size_t len;
  char *str;
} fio_hash_key_s;

/* strdup is usually available... but just in case it isn't */
static inline char *my_strdup(char *str, size_t len) {
  char *ret = malloc(len + 1);
  ret[len] = 0;
  memcpy(ret, str, len);
  return ret;
}

/* define the macro to set the key type */
#define FIO_HASH_KEY_TYPE fio_hash_key_s

/* the macro that returns the key's hash value */
#define FIO_HASH_KEY2UINT(key) ((key).hash)

/* Compare the keys using length testing and `memcmp` (no hash comparison) */
#define FIO_HASH_COMPARE_KEYS(k1, k2)                                          \
  ((k1).len == (k2).len && !memcmp((k1).str, (k2).str, (k2).len))

/* an "all bytes are zero" invalid key */
#define FIO_HASH_KEY_INVALID ((fio_hash_key_s){.hash = 0})

/* tests if a key is the invalid key */
#define FIO_HASH_KEY_ISINVALID(key) ((key).str == NULL)

/* creates a persistent copy of a key, so changing strings keeps the data intact
 */
#define FIO_HASH_KEY_COPY(key)                                                 \
  ((fio_hash_key_s){.hash = (key).hash,                                        \
                    .len = (key).len,                                          \
                    .str = my_strdup((key).str, (key).len)})

/* frees the allocated string, remove the `fprintf` in production */
#define FIO_HASH_KEY_DESTROY(key)                                              \
  (fprintf(stderr, "freeing %s\n", (key).str), free((key).str))

#include "fio_hashmap.h"

int main(void) {
  fio_hash_s hash;
  fio_hash_key_s key1 = {.hash = 1, .len = 5, .str = "hello"};
  fio_hash_key_s key1_copy = {.hash = 1, .len = 5, .str = "hello"};
  fio_hash_key_s key2 = {.hash = 1, .len = 5, .str = "Hello"};
  fio_hash_key_s key3 = {.hash = 1, .len = 5, .str = "Hell0"};
  fio_hash_new(&hash);
  fio_hash_insert(&hash, key1, key1.str);
  key1.str = "oops";
  if (fio_hash_find(&hash, key1))
    fprintf(stderr,
            "ERROR: string comparison should have faild, instead got: %s\n",
            (char *)fio_hash_find(&hash, key1));
  else if (fio_hash_find(&hash, key1_copy))
    fprintf(stderr, "Hash string comparison passed for %s\n",
            (char *)fio_hash_find(&hash, key1_copy));

  fio_hash_insert(&hash, key2, key2.str);
  fio_hash_insert(&hash, key3, key3.str);
  fio_hash_insert(&hash, key3, NULL); /* delete the key3 object */
  fio_hash_rehash(&hash); /* forces the unused key to be destroyed */
  fprintf(stderr, "Did we free %s?\n", key3.str);
  FIO_HASH_FOR_EMPTY(&hash, i) { (void)i->obj; }
  if (fio_hash_find(&hash, key1_copy))
    fprintf(stderr,
            "ERROR: string comparison should have faild, instead got: %s\n",
            (char *)fio_hash_find(&hash, key1));
  fprintf(stderr, "reinserting stuff\n");
  fio_hash_insert(&hash, key2, key2.str);
  fio_hash_insert(&hash, key3, key3.str);
  FIO_HASH_FOR_FREE(&hash, i) { (void)i->obj; }
}
```

