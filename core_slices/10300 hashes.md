## Hashing and Friends

The facil.io IO core also includes a number or hashing and encoding primitives that are often used in network related applications.

### SHA-1

SHA-1 example:

```c
fio_sha1_s sha1;
fio_sha1_init(&sha1);
fio_sha1_write(&sha1,
             "The quick brown fox jumps over the lazy dog", 43);
char *hashed_result = fio_sha1_result(&sha1);
```

#### `fio_sha1_init`

```c
fio_sha1_s fio_sha1_init(void);
```

Initializes or resets the `fio_sha1_s` object. This must be performed before hashing data using SHA-1.

The SHA-1 container type (`fio_sha1_s`) is defines as follows:

The `fio_sha1_s` structure's content should be ignored.

#### `fio_sha1`

```c
inline char *fio_sha1(fio_sha1_s *s, const void *data, size_t len)
```

A SHA1 helper function that performs initialization, writing and finalizing.

SHA-1 hashing container - 

The `sha1_s` type will contain all the sha1 data required to perform the
hashing, managing it's encoding. If it's stack allocated, no freeing will be
required.

#### `fio_sha1_write`

```c
void fio_sha1_write(fio_sha1_s *s, const void *data, size_t len);
```

Writes data to the sha1 buffer.

#### `fio_sha1_result`

```c
char *fio_sha1_result(fio_sha1_s *s);
```

Finalizes the SHA1 hash, returning the Hashed data.

`fio_sha1_result` can be called for the same object multiple times, but the finalization will only be performed the first time this function is called.

### SHA-2

SHA-2 example:

```c
fio_sha2_s sha2;
fio_sha2_init(&sha2, SHA_512);
fio_sha2_write(&sha2,
             "The quick brown fox jumps over the lazy dog", 43);
char *hashed_result = fio_sha2_result(&sha2);
```

#### `fio_sha2_init`

```c
fio_sha2_s fio_sha2_init(fio_sha2_variant_e variant);
```

Initializes / resets the SHA-2 object.

SHA-2 is actually a family of functions with different variants. When initializing the SHA-2 container, a variant must be chosen. The following are valid variants:

* `SHA_512`

* `SHA_384`

* `SHA_512_224`

* `SHA_512_256`

* `SHA_256`

* `SHA_224`

The `fio_sha2_s` structure's content should be ignored.

#### `fio_sha2_write`

```c
void fio_sha2_write(fio_sha2_s *s, const void *data, size_t len);
```

Writes data to the SHA-2 buffer.

#### `fio_sha2_result`

```c
char *fio_sha2_result(fio_sha2_s *s);
```

Finalizes the SHA-2 hash, returning the Hashed data.

`sha2_result` can be called for the same object multiple times, but the finalization will only be performed the first time this function is called.

#### `fio_sha2_512`

```c
inline char *fio_sha2_512(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 512 variant.

#### `fio_sha2_256`

```c
inline char *fio_sha2_256(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 256 variant.

#### `fio_sha2_256`

```c
inline char *fio_sha2_384(fio_sha2_s *s, const void *data,
                          size_t len);
```

A SHA-2 helper function that performs initialization, writing and finalizing.

Uses the SHA2 384 variant.

-------------------------------------------------------------------------------
