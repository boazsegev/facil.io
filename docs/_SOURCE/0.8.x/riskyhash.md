---
title: facil.io - Risky Hash
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

Risky Hash isn't a stable Hash algorithm, it is more of a statement - this algorithm was chosen although it is "risky", not cryptographically safe.

The assumption is that the risk management is handled by other modules. In facil.io, Risky Hash is used for the Hash Map keys and the risk management is handled by the Map type (which could accept malicious hashes).

The facil.io implementation MAY change at any time, to mitigate risk factors, improve performance or any other reason.

## Current Implementation

Risky Hash is a keyed hashing function which was inspired by both xxHash and SipHash.

It's meant to provide a fast alternative to SipHash, under the assumption that some of the security claims made by SipHash are actually a (hash) Map implementation concern.

Risky Hash wasn't properly tested for attack resistance and shouldn't be used to resist [hash flooding attacks](http://emboss.github.io/blog/2012/12/14/breaking-murmur-hash-flooding-dos-reloaded/) (see [here](https://medium.freecodecamp.org/hash-table-attack-8e4371fc5261)). Hash flooding attacks are decidedly a Hash Map concern. A Map implementation should safe regardless of Hash values.

Risky Hash was tested with [`SMHasher`](https://github.com/rurban/smhasher) ([see results](#smhasher-results)).

Sometime around 2019, the testing suit was updated in a way that exposed an issue Risky Hash has with sparsely hashed data. This was mitigated (but not completely resolved) by an update to the algorithm.

A non-streaming [reference implementation in C is attached](#in-code) The code is easy to read and should be considered an integral part of this specification.

## Status

> Risky Hash is still under development and review. This specification should be considered a working draft.
 
> Risky Hash should be limited to testing and safe environments until it's fully analyzed and reviewed.

This is the third draft of the RiskyHash algorithm and it incorporates updates from community driven feedback.

* Florian Weber (@Florianjw) [exposed coding errors (last 8 byte ordering) and took a bit of time to challenge the algorithm](https://www.reddit.com/r/crypto/comments/9kk5gl/break_my_ciphercollectionpost/eekxw2f/?context=3) and make a few suggestions.

    Following this feedback, the error in the code was fixed, the initialization state was updated and the left-over bytes are now read in order with padding (rather than consumed by the 4th state-word).

* Chris Anderson (@injinj) did amazing work exploring a 128 bit variation and attacking RiskyHash using a variation on a Meet-In-The-Middle attack, written by Hening Makholm (@hmakholm), that discovers hash collisions with a small Hamming distance ([SMHasher fork](https://github.com/hmakholm/smhasher)).

    Following this input, RiskyHash updated the way the message length is incorporated into the final hash and updated the consumption stage to replace the initial XOR with ADD.

After [Risky Hash Version 2](riskyhash_v2) many changes were made. The `LROT` value was reduced (avoiding it becoming an `RROT`). Each consumption vector was allotted it's own prime multiplied. Seed initialization was simplified. Prime numbers were added / updated. The mixing round was simplified.

After [Risky Hash Version 1](riskyhash_v1), the consumption approach was simplified to make RiskyHash easier to implement.

The [previous version can be accessed here](riskyhash_v1).

## Purpose

Risky Hash is designed for fast Hash Map key calculation for both big and small keys. It attempts to act as a 64 bit keyed PRF.

The Risky Hash is designed as a temporary hash (values shouldn't be stored, as the algorithm might be updated without notice) and it is used internally for facil.io's `FIOBJ` Hash Maps.

## Algorithm

A portable (naive) C implementation can be found at the `fio.h` header [and later on in this document](#in-code).

Risky Hash uses 4 reading vectors (state-words), each containing 64 bits.

Risky Hash allows for a 64 bit "seed". that is processed as if it were a 256bit block of data pre-pended to the data being hashed (as if the seed is "copied" to that block 4 times).

Risky Hash uses the following prime numbers:

```txt
FIO_RISKY3_PRIME0 0xCAEF89D1E9A5EB21ULL
FIO_RISKY3_PRIME1 0xAB137439982B86C9ULL
FIO_RISKY3_PRIME2 0xD9FDC73ABE9EDECDULL
FIO_RISKY3_PRIME3 0x3532D520F9511B13ULL
FIO_RISKY3_PRIME4 0x038720DDEB5A8415ULL
```

Risky Hash has three or four stages:

* Initialization stage.

* Seed / Secret reading stage (optional).

* Reading stage.

* Mixing stage.

The hashing algorithm uses an internal 256 bit state for a 64 bit output and the input data is mixed twice into the state on different bit positions (left rotation).

This approach **should** minimize the risk of malicious data weakening the hash function.

The following operations are used:

* `~` marks a bit inversion.
* `+` marks a mod 2^64 addition.
* `XOR` marks an XOR operation.
* `MUL(x,y)` a mod 2^64 multiplication.
* `LROT(x,bits)` is a left rotation of a 64 bit word.
* `>>` is a right shift (not rotate, some bits are lost).
* `<<` is a left shift (not rotate, some bits are lost).

### Initialization

In the initialization stage, Risky Hash attempts to achieves a single goal:

* It must start initialize each reading vector so it is unique and promises (as much as possible).

The four consumption vectors are initialized using a few set bits to promise that they react differently when consuming the same input:

```txt
V1 = 0x0000001000000001
V2 = 0x0000010000000010
V3 = 0x0000100000000100
V4 = 0x0001000000001000
```

### Seed / Secret Reading Stage (optional).

In the seed / secret reading stage, Risky Hash attempts to achieves a single goal:

* Update the hash state using a "secret" (key / salt / seed) in a way that will result in the "secret" having a meaningful impact on the final result.

* Make sure the hash internal state can't be reversed/controlled by a maliciously crafted message.

A seed is a 64bit "word" with at least a single bit set (the secret will be discarded when zero).

When the seed is set, each of the consumption vectors will be multiplied by that seed. Later the last three consumption vectors will be XORed with the seed.

In pseudo code:

```c
if(seed){
    V0 = MUL(V0, seed);
    V1 = MUL(V1, seed);
    V2 = MUL(V2, seed);
    V3 = MUL(V3, seed);
    V1 = V1 XOR seed;
    V2 = V2 XOR seed;
    V3 = V3 XOR seed;
}
```

### Consumption

In the consumption stage, Risky Hash attempts to achieves three goals:

* Consume the data with minimal bias (bits have a fairly even chance at being set or unset).

* Allow parallelism.

   This is achieved by using a number of distinct and separated reading "vectors" (independent state-words).

* Repeated data blocks should produce different results according to their position.

   This is attempted by mixing a number of operations (OX, LROT, addition and multiplication), so the vector is mutated every step (regardless of the input data).

* Maliciously crafted data won't be able to weaken the hash function or expose the "secret".

    This is attempted by reading the data twice into different positions in the consumption vector. This minimizes the possibility of finding a malicious value that could break the state vector.

    However, Risky Hash still could probably be attacked by carefully crafted malicious data that would result in a collision.

Risky Hash consumes data in 64 bit chunks/words.

Each vector reads a single 64 bit word within a 256 bit block, allowing the vectors to be parallelized though any message length of 256 bits or longer.

`V0` reads the first 64 bits, `V0` reads bits 65-128, and so forth...

The 64 bits (8 bytes) are read in **Little Endian** network byte order (improving performance on most common CPUs) and treated as a numerical value.

Any trailing data that doesn't fit in a 64 bit word is padded with zeros. The last byte in a padded word (if any) will contain the least significant 8 bits of the length value (so it's never 0). That word is then consumed the next available vector.

Each vector performs the following operations in each of it's consumption rounds (`Vi` is the vector, `word` is the input data for that vector as a 64 bit word):

```txt
Vi = Vi + word
Vi = LROT(Vi, 29)
Vi = Vi + word
Vi = MUL(Vi, Pi)
```

It is normal for some vectors to perform more consumption rounds than others when the data isn't divisible into 256 bit blocks.

### Hash Mixing

In the mixing stage, Risky Hash attempts to achieves three goals:

* Be irreversible.

   This stage is the "last line of defense" against malicious data. For this reason, it should be infeasible to extract meaningful data from the final result.

* Produce a message digest with minimal bias (bits have a fairly even chance at being set or unset).

* Allow all consumption vectors an equal but different effect on the final hash value.

The following intermediate 64 bit result is calculated:

```txt
result = LROT(V1,17) + LROT(V2,13) + LROT(V3,47) + LROT(V4,57)
```

At this point the length of the input data is finalized an can be added to the calculation.

The consumed (unpadded) message length is treated as a 64 bit word. It is shifted left by 36 bits and XORed with itself. Then, the updated length value is added to the intermediate result:

```txt
length = length XOR (length << 36)
result = result + length
```

The vectors are mixed in with the intermediate result in different positions (using `LROT`):

```txt
  result += v[0] XOR v[1];
  result = result XOR (fio_lrot64(result, 13));
  result += v[1] XOR v[2];
  result = result XOR (fio_lrot64(result, 29));
  result += v[2] XOR v[3];
  result += fio_lrot64(result, 33);
  result += v[3] XOR v[0];
  result = result XOR (fio_lrot64(result, 51));
```

Finally, the intermediate result is mixed with itself to improve bit entropy distribution and hinder reversibility.

```txt
  result = result XOR MUL((result >> 29), P4);
```

## Performance

Risky Hash attempts to balance performance with security concerns, since hash functions are often use by insecure hash table implementations.

However, the design should allow for fairly high performance, for example, by using SIMD instructions or a multi-threaded approach (up to 4 threads).

In fact, even the simple reference implementation at the end of this document offers fairly high performance.

## Attacks, Reports and Security

No known attacks exist for this draft of the Risky Hash algorithm and no known collisions have been found...

...However, it's too early to tell.

At this early stage, please feel free to attack the Risky Hash algorithm and report any security concerns in the [GitHub issue tracker](https://github.com/boazsegev/facil.io/issues).

Later, as Risky Hash usage might increase, attacks should be reported discretely if possible, allowing for a fix to be provided before publication.

## In Code

In C code, the above description might translate like so:

```c
/*
Copyright: Boaz Segev, 2019
License: MIT
*/

#if (defined(__LITTLE_ENDIAN__) && __LITTLE_ENDIAN__) ||                       \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 0
#endif
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif
#elif (defined(__LITTLE_ENDIAN__) && !__LITTLE_ENDIAN__) ||                    \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 0
#endif
#elif !defined(__BIG_ENDIAN__) && !defined(__BYTE_ORDER__) &&                  \
    !defined(__LITTLE_ENDIAN__)
#error Could not detect byte order on this system.
#endif

/* read u64 in little endian */
#if __LITTLE_ENDIAN__ && __has_builtin(__builtin_memcpy)
HFUNC uint64_t FIO_RISKY_BUF2U64(const void *c) {
  uint64_t tmp = 0;
  __builtin_memcpy(&tmp, c, sizeof(tmp));
  return tmp;
}
#else
#define FIO_RISKY_BUF2U64(c)                                                   \
  ((uint64_t)((((uint64_t)((const uint8_t *)(c))[7]) << 56) |                  \
              (((uint64_t)((const uint8_t *)(c))[6]) << 48) |                  \
              (((uint64_t)((const uint8_t *)(c))[5]) << 40) |                  \
              (((uint64_t)((const uint8_t *)(c))[4]) << 32) |                  \
              (((uint64_t)((const uint8_t *)(c))[3]) << 24) |                  \
              (((uint64_t)((const uint8_t *)(c))[2]) << 16) |                  \
              (((uint64_t)((const uint8_t *)(c))[1]) << 8) |                   \
              (((uint8_t *)(c))[0])))
#endif

/** 64 bit left rotation, inlined. */
#define fio_lrot64(i, bits)                                                    \
  (((uint64_t)(i) << ((bits)&63UL)) | ((uint64_t)(i) >> ((-(bits)) & 63UL)))

/* Risky Hash primes */
#define FIO_RISKY3_PRIME0 0xCAEF89D1E9A5EB21ULL
#define FIO_RISKY3_PRIME1 0xAB137439982B86C9ULL
#define FIO_RISKY3_PRIME2 0xD9FDC73ABE9EDECDULL
#define FIO_RISKY3_PRIME3 0x3532D520F9511B13ULL
#define FIO_RISKY3_PRIME4 0x038720DDEB5A8415ULL
/* Risky Hash initialization constants */
#define FIO_RISKY3_IV0 0x0000001000000001ULL
#define FIO_RISKY3_IV1 0x0000010000000010ULL
#define FIO_RISKY3_IV2 0x0000100000000100ULL
#define FIO_RISKY3_IV3 0x0001000000001000ULL

#ifdef __cplusplus
/* the register keyword was deprecated for C++ but is semi-meaningful in C */
#define register
#endif

/*  Computes a facil.io Risky Hash. */
uint64_t fio_risky_hash(const void *data_, size_t len, uint64_t seed) {
  uint64_t v0 = FIO_RISKY3_IV0;
  uint64_t v1 = FIO_RISKY3_IV1;
  uint64_t v2 = FIO_RISKY3_IV2;
  uint64_t v3 = FIO_RISKY3_IV3;
  const uint8_t *data = (const uint8_t *)data_;

#define FIO_RISKY3_ROUND64(vi, w)                                              \
  v##vi += w;                                                                  \
  v##vi = fio_lrot64(v##vi, 29);                                               \
  v##vi += w;                                                                  \
  v##vi *= FIO_RISKY3_PRIME##vi;

#define FIO_RISKY3_ROUND256(w0, w1, w2, w3)                                    \
  FIO_RISKY3_ROUND64(0, w0);                                                   \
  FIO_RISKY3_ROUND64(1, w1);                                                   \
  FIO_RISKY3_ROUND64(2, w2);                                                   \
  FIO_RISKY3_ROUND64(3, w3);

  if (seed) {
    /* process the seed as if it was a prepended 8 Byte string. */
    v0 *= seed;
    v1 *= seed;
    v2 *= seed;
    v3 *= seed;
    v1 ^= seed;
    v2 ^= seed;
    v3 ^= seed;
  }

  for (size_t i = len >> 5; i; --i) {
    /* vectorized 32 bytes / 256 bit access */
    FIO_RISKY3_ROUND256(FIO_RISKY_BUF2U64(data), FIO_RISKY_BUF2U64(data + 8),
                        FIO_RISKY_BUF2U64(data + 16),
                        FIO_RISKY_BUF2U64(data + 24));
    data += 32;
  }
  switch (len & 24) {
  case 24:
    FIO_RISKY3_ROUND64(2, FIO_RISKY_BUF2U64(data + 16));
    /* fallthrough */
  case 16:
    FIO_RISKY3_ROUND64(1, FIO_RISKY_BUF2U64(data + 8));
    /* fallthrough */
  case 8:
    FIO_RISKY3_ROUND64(0, FIO_RISKY_BUF2U64(data + 0));
    data += len & 24;
  }

  uint64_t tmp = (len & 0xFF) << 56; /* add offset information to padding */
  /* leftover bytes */
  switch ((len & 7)) {
  case 7:
    tmp |= ((uint64_t)data[6]) << 48; /* fallthrough */
  case 6:
    tmp |= ((uint64_t)data[5]) << 40; /* fallthrough */
  case 5:
    tmp |= ((uint64_t)data[4]) << 32; /* fallthrough */
  case 4:
    tmp |= ((uint64_t)data[3]) << 24; /* fallthrough */
  case 3:
    tmp |= ((uint64_t)data[2]) << 16; /* fallthrough */
  case 2:
    tmp |= ((uint64_t)data[1]) << 8; /* fallthrough */
  case 1:
    tmp |= ((uint64_t)data[0]);
    /* the last (now padded) byte's position */
    switch ((len & 24)) {
    case 24: /* offset 24 in 32 byte segment */
      FIO_RISKY3_ROUND64(3, tmp);
      break;
    case 16: /* offset 16 in 32 byte segment */
      FIO_RISKY3_ROUND64(2, tmp);
      break;
    case 8: /* offset 8 in 32 byte segment */
      FIO_RISKY3_ROUND64(1, tmp);
      break;
    case 0: /* offset 0 in 32 byte segment */
      FIO_RISKY3_ROUND64(0, tmp);
      break;
    }
  }

  /* irreversible avalanche... I think */
  uint64_t r = (len) ^ ((uint64_t)len << 36);
  r += fio_lrot64(v0, 17) + fio_lrot64(v1, 13) + fio_lrot64(v2, 47) +
       fio_lrot64(v3, 57);
  r += v0 ^ v1;
  r ^= fio_lrot64(r, 13);
  r += v1 ^ v2;
  r ^= fio_lrot64(r, 29);
  r += v2 ^ v3;
  r += fio_lrot64(r, 33);
  r += v3 ^ v0;
  r ^= fio_lrot64(r, 51);
  r ^= (r >> 29) * FIO_RISKY3_PRIME4;
  return r;
}
```

## SMHasher results

The following results were produced on a 2.9 GHz Intel Core i9 machine and won't be updated every time.

Note that the Sparse Hash test found a single collision on a 3 bit sparse hash with a 512 bit key:

```txt
Keyset 'Sparse' - 512-bit keys with up to 3 bits set - 22370049 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      1 (36862.59x) !!!!!
```

For some reason this didn't mark the testing as a failure.

**These are the full results**:


```txt
-------------------------------------------------------------------------------
--- Testing RiskyHash64 "Risky Hash 64 bits v.3" GOOD

[[[ Sanity Tests ]]]

Verification value 0x748F477D ....... PASS
Running sanity check 1     .......... PASS
Running AppendedZeroesTest .......... PASS

[[[ Speed Tests ]]]

Bulk speed test - 262144-byte keys
Alignment  7 -  7.373 bytes/cycle - 21095.06 MiB/sec @ 3 ghz
Alignment  6 -  7.277 bytes/cycle - 20818.90 MiB/sec @ 3 ghz
Alignment  5 -  7.373 bytes/cycle - 21094.47 MiB/sec @ 3 ghz
Alignment  4 -  7.372 bytes/cycle - 21091.20 MiB/sec @ 3 ghz
Alignment  3 -  7.370 bytes/cycle - 21087.09 MiB/sec @ 3 ghz
Alignment  2 -  7.370 bytes/cycle - 21085.46 MiB/sec @ 3 ghz
Alignment  1 -  7.312 bytes/cycle - 20919.78 MiB/sec @ 3 ghz
Alignment  0 -  7.148 bytes/cycle - 20450.49 MiB/sec @ 3 ghz
Average      -  7.324 bytes/cycle - 20955.31 MiB/sec @ 3 ghz

Small key speed test -    1-byte keys -    22.43 cycles/hash
Small key speed test -    2-byte keys -    24.20 cycles/hash
Small key speed test -    3-byte keys -    24.00 cycles/hash
Small key speed test -    4-byte keys -    25.53 cycles/hash
Small key speed test -    5-byte keys -    25.89 cycles/hash
Small key speed test -    6-byte keys -    25.84 cycles/hash
Small key speed test -    7-byte keys -    25.89 cycles/hash
Small key speed test -    8-byte keys -    27.00 cycles/hash
Small key speed test -    9-byte keys -    27.00 cycles/hash
Small key speed test -   10-byte keys -    27.32 cycles/hash
Small key speed test -   11-byte keys -    27.80 cycles/hash
Small key speed test -   12-byte keys -    27.92 cycles/hash
Small key speed test -   13-byte keys -    27.96 cycles/hash
Small key speed test -   14-byte keys -    27.54 cycles/hash
Small key speed test -   15-byte keys -    27.74 cycles/hash
Small key speed test -   16-byte keys -    27.30 cycles/hash
Small key speed test -   17-byte keys -    27.50 cycles/hash
Small key speed test -   18-byte keys -    27.00 cycles/hash
Small key speed test -   19-byte keys -    27.50 cycles/hash
Small key speed test -   20-byte keys -    27.47 cycles/hash
Small key speed test -   21-byte keys -    28.28 cycles/hash
Small key speed test -   22-byte keys -    28.22 cycles/hash
Small key speed test -   23-byte keys -    27.22 cycles/hash
Small key speed test -   24-byte keys -    27.44 cycles/hash
Small key speed test -   25-byte keys -    28.73 cycles/hash
Small key speed test -   26-byte keys -    28.89 cycles/hash
Small key speed test -   27-byte keys -    28.97 cycles/hash
Small key speed test -   28-byte keys -    29.06 cycles/hash
Small key speed test -   29-byte keys -    28.00 cycles/hash
Small key speed test -   30-byte keys -    28.00 cycles/hash
Small key speed test -   31-byte keys -    28.82 cycles/hash
Average                                    27.177 cycles/hash

[[[ Avalanche Tests ]]]

Testing   24-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.709333%
Testing   32-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.592667%
Testing   40-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.795333%
Testing   48-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.688000%
Testing   56-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.659333%
Testing   64-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.740667%
Testing   72-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.706667%
Testing   80-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.773333%
Testing   96-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.724000%
Testing  112-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.702000%
Testing  128-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.677333%
Testing  160-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.739333%
Testing  512-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.786000%
Testing 1024-bit keys ->  64-bit hashes, 300000 reps worst bias is 0.825333%

[[[ Keyset 'Sparse' Tests ]]]

Keyset 'Sparse' - 16-bit keys with up to 9 bits set - 50643 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected          0.6, actual      0 (0.00x)
Testing collisions (high 19-26 bits) - Worst is 25 bits: 45/76 (0.59x)
Testing collisions (high 12-bit) - Expected      50643.0, actual  46547 (0.92x)
Testing collisions (high  8-bit) - Expected      50643.0, actual  50387 (0.99x) (-256)
Testing collisions (low  32-bit) - Expected          0.6, actual      0 (0.00x)
Testing collisions (low  19-26 bits) - Worst is 22 bits: 321/611 (0.52x)
Testing collisions (low  12-bit) - Expected      50643.0, actual  46547 (0.92x)
Testing collisions (low   8-bit) - Expected      50643.0, actual  50387 (0.99x) (-256)
Testing distribution - Worst bias is the 13-bit window at bit  5 - 0.800%

Keyset 'Sparse' - 24-bit keys with up to 8 bits set - 1271626 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        376.5, actual    210 (0.56x)
Testing collisions (high 24-36 bits) - Worst is 36 bits: 18/23 (0.76x)
Testing collisions (high 12-bit) - Expected    1271626.0, actual 1267530 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1271626.0, actual 1271370 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        376.5, actual    193 (0.51x)
Testing collisions (low  24-36 bits) - Worst is 34 bits: 56/94 (0.59x)
Testing collisions (low  12-bit) - Expected    1271626.0, actual 1267530 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1271626.0, actual 1271370 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 46 - 0.108%

Keyset 'Sparse' - 32-bit keys with up to 7 bits set - 4514873 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       4746.0, actual   2628 (0.55x)
Testing collisions (high 26-39 bits) - Worst is 38 bits: 78/74 (1.05x)
Testing collisions (high 12-bit) - Expected    4514873.0, actual 4510777 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    4514873.0, actual 4514617 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       4746.0, actual   2293 (0.48x)
Testing collisions (low  26-39 bits) - Worst is 37 bits: 83/148 (0.56x)
Testing collisions (low  12-bit) - Expected    4514873.0, actual 4510777 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    4514873.0, actual 4514617 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 35 - 0.039%

Keyset 'Sparse' - 40-bit keys with up to 6 bits set - 4598479 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       4923.4, actual   2685 (0.55x)
Testing collisions (high 26-39 bits) - Worst is 35 bits: 598/615 (0.97x)
Testing collisions (high 12-bit) - Expected    4598479.0, actual 4594383 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    4598479.0, actual 4598223 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       4923.4, actual   2447 (0.50x)
Testing collisions (low  26-39 bits) - Worst is 39 bits: 20/38 (0.52x)
Testing collisions (low  12-bit) - Expected    4598479.0, actual 4594383 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    4598479.0, actual 4598223 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 25 - 0.045%

Keyset 'Sparse' - 48-bit keys with up to 6 bits set - 14196869 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      46927.3, actual  26057 (0.56x)
Testing collisions (high 28-43 bits) - Worst is 43 bits: 25/22 (1.09x)
Testing collisions (high 12-bit) - Expected   14196869.0, actual 14192773 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   14196869.0, actual 14196613 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      46927.3, actual  23378 (0.50x)
Testing collisions (low  28-43 bits) - Worst is 43 bits: 20/22 (0.87x)
Testing collisions (low  12-bit) - Expected   14196869.0, actual 14192773 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   14196869.0, actual 14196613 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 14 - 0.027%

Keyset 'Sparse' - 56-bit keys with up to 5 bits set - 4216423 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       4139.3, actual   2302 (0.56x)
Testing collisions (high 26-39 bits) - Worst is 36 bits: 260/258 (1.00x)
Testing collisions (high 12-bit) - Expected    4216423.0, actual 4212327 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    4216423.0, actual 4216167 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       4139.3, actual   2105 (0.51x)
Testing collisions (low  26-39 bits) - Worst is 39 bits: 19/32 (0.59x)
Testing collisions (low  12-bit) - Expected    4216423.0, actual 4212327 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    4216423.0, actual 4216167 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 30 - 0.053%

Keyset 'Sparse' - 64-bit keys with up to 5 bits set - 8303633 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16053.7, actual   9036 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 35 bits: 1918/2006 (0.96x)
Testing collisions (high 12-bit) - Expected    8303633.0, actual 8299537 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8303633.0, actual 8303377 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16053.7, actual   7998 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 37 bits: 286/501 (0.57x)
Testing collisions (low  12-bit) - Expected    8303633.0, actual 8299537 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8303633.0, actual 8303377 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 27 - 0.040%

Keyset 'Sparse' - 72-bit keys with up to 5 bits set - 15082603 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      52965.5, actual  29826 (0.56x)
Testing collisions (high 28-43 bits) - Worst is 40 bits: 217/206 (1.05x)
Testing collisions (high 12-bit) - Expected   15082603.0, actual 15078507 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   15082603.0, actual 15082347 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      52965.5, actual  26439 (0.50x)
Testing collisions (low  28-43 bits) - Worst is 42 bits: 30/51 (0.58x)
Testing collisions (low  12-bit) - Expected   15082603.0, actual 15078507 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   15082603.0, actual 15082347 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 32 - 0.022%

Keyset 'Sparse' - 96-bit keys with up to 4 bits set - 3469497 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       2802.7, actual   1561 (0.56x)
Testing collisions (high 26-39 bits) - Worst is 35 bits: 339/350 (0.97x)
Testing collisions (high 12-bit) - Expected    3469497.0, actual 3465401 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    3469497.0, actual 3469241 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       2802.7, actual   1350 (0.48x)
Testing collisions (low  26-39 bits) - Worst is 36 bits: 96/175 (0.55x)
Testing collisions (low  12-bit) - Expected    3469497.0, actual 3465401 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    3469497.0, actual 3469241 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 17 - 0.055%

Keyset 'Sparse' - 160-bit keys with up to 4 bits set - 26977161 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected     169446.5, actual  94833 (0.56x)
Testing collisions (high 29-45 bits) - Worst is 42 bits: 191/165 (1.15x)
Testing collisions (high 12-bit) - Expected   26977161.0, actual 26973065 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   26977161.0, actual 26976905 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected     169446.5, actual  84478 (0.50x)
Testing collisions (low  29-45 bits) - Worst is 34 bits: 21425/42361 (0.51x)
Testing collisions (low  12-bit) - Expected   26977161.0, actual 26973065 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   26977161.0, actual 26976905 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit  3 - 0.009%

Keyset 'Sparse' - 256-bit keys with up to 3 bits set - 2796417 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1820.7, actual   1018 (0.56x)
Testing collisions (high 25-38 bits) - Worst is 38 bits: 36/28 (1.27x)
Testing collisions (high 12-bit) - Expected    2796417.0, actual 2792321 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2796417.0, actual 2796161 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1820.7, actual    895 (0.49x)
Testing collisions (low  25-38 bits) - Worst is 37 bits: 42/56 (0.74x)
Testing collisions (low  12-bit) - Expected    2796417.0, actual 2792321 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2796417.0, actual 2796161 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 19 - 0.130%

Keyset 'Sparse' - 512-bit keys with up to 3 bits set - 22370049 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      1 (36862.59x) !!!!!
Testing collisions (high 32-bit) - Expected     116512.9, actual  64872 (0.56x)
Testing collisions (high 28-44 bits) - Worst is 44 bits: 33/28 (1.16x)
Testing collisions (high 12-bit) - Expected   22370049.0, actual 22365953 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   22370049.0, actual 22369793 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected     116512.9, actual  58528 (0.50x)
Testing collisions (low  28-44 bits) - Worst is 39 bits: 476/910 (0.52x)
Testing collisions (low  12-bit) - Expected   22370049.0, actual 22365953 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   22370049.0, actual 22369793 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit  5 - 0.015%

Keyset 'Sparse' - 1024-bit keys with up to 2 bits set - 524801 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected         64.1, actual     30 (0.47x)
Testing collisions (high 23-33 bits) - Worst is 31 bits: 73/128 (0.57x)
Testing collisions (high 12-bit) - Expected     524801.0, actual 520705 (0.99x) (-4096)
Testing collisions (high  8-bit) - Expected     524801.0, actual 524545 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected         64.1, actual     40 (0.62x)
Testing collisions (low  23-33 bits) - Worst is 33 bits: 25/32 (0.78x)
Testing collisions (low  12-bit) - Expected     524801.0, actual 520705 (0.99x) (-4096)
Testing collisions (low   8-bit) - Expected     524801.0, actual 524545 (1.00x) (-256)
Testing distribution - Worst bias is the 16-bit window at bit 53 - 0.142%

Keyset 'Sparse' - 2048-bit keys with up to 2 bits set - 2098177 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1025.0, actual    563 (0.55x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 119/128 (0.93x)
Testing collisions (high 12-bit) - Expected    2098177.0, actual 2094081 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2098177.0, actual 2097921 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1025.0, actual    521 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 34/64 (0.53x)
Testing collisions (low  12-bit) - Expected    2098177.0, actual 2094081 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2098177.0, actual 2097921 (1.00x) (-256)
Testing distribution - Worst bias is the 18-bit window at bit 47 - 0.062%

*********FAIL*********

[[[ Keyset 'Permutation' Tests ]]]

Combination Lowbits Tests:
Keyset 'Combination' - up to 7 blocks from a set of 8 - 2396744 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1337.5, actual    734 (0.55x)
Testing collisions (high 25-38 bits) - Worst is 35 bits: 181/167 (1.08x)
Testing collisions (high 12-bit) - Expected    2396744.0, actual 2392648 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2396744.0, actual 2396488 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1337.5, actual    656 (0.49x)
Testing collisions (low  25-38 bits) - Worst is 27 bits: 21365/42798 (0.50x)
Testing collisions (low  12-bit) - Expected    2396744.0, actual 2392648 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2396744.0, actual 2396488 (1.00x) (-256)
Testing distribution - Worst bias is the 18-bit window at bit  7 - 0.078%


Combination Highbits Tests
Keyset 'Combination' - up to 7 blocks from a set of 8 - 2396744 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1337.5, actual    733 (0.55x)
Testing collisions (high 25-38 bits) - Worst is 36 bits: 93/83 (1.11x)
Testing collisions (high 12-bit) - Expected    2396744.0, actual 2392648 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2396744.0, actual 2396488 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1337.5, actual    632 (0.47x)
Testing collisions (low  25-38 bits) - Worst is 28 bits: 10731/21399 (0.50x)
Testing collisions (low  12-bit) - Expected    2396744.0, actual 2392648 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2396744.0, actual 2396488 (1.00x) (-256)
Testing distribution - Worst bias is the 18-bit window at bit  5 - 0.064%


Combination Hi-Lo Tests:
Keyset 'Combination' - up to 6 blocks from a set of 15 - 12204240 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      34678.6, actual  19672 (0.57x)
Testing collisions (high 27-42 bits) - Worst is 38 bits: 611/541 (1.13x)
Testing collisions (high 12-bit) - Expected   12204240.0, actual 12200144 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   12204240.0, actual 12203984 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      34678.6, actual  17053 (0.49x)
Testing collisions (low  27-42 bits) - Worst is 40 bits: 89/135 (0.66x)
Testing collisions (low  12-bit) - Expected   12204240.0, actual 12200144 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   12204240.0, actual 12203984 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 21 - 0.029%


Combination 0x8000000 Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9143 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 40 bits: 66/63 (1.03x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8166 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 36 bits: 525/1023 (0.51x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 24 - 0.028%


Combination 0x0000001 Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9348 (0.57x)
Testing collisions (high 27-41 bits) - Worst is 40 bits: 77/63 (1.20x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8355 (0.51x)
Testing collisions (low  27-41 bits) - Worst is 34 bits: 2094/4095 (0.51x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit 62 - 0.027%


Combination 0x800000000000000 Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9125 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 41 bits: 41/31 (1.28x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8225 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 41 bits: 17/31 (0.53x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 13 - 0.044%


Combination 0x000000000000001 Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9117 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 41 bits: 41/31 (1.28x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8005 (0.49x)
Testing collisions (low  27-41 bits) - Worst is 41 bits: 16/31 (0.50x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 11 - 0.028%


Combination 16-bytes [0-1] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9317 (0.57x)
Testing collisions (high 27-41 bits) - Worst is 38 bits: 288/255 (1.13x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8193 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 41 bits: 20/31 (0.63x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 20 - 0.049%


Combination 16-bytes [0-last] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9383 (0.57x)
Testing collisions (high 27-41 bits) - Worst is 40 bits: 71/63 (1.11x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8077 (0.49x)
Testing collisions (low  27-41 bits) - Worst is 35 bits: 1028/2047 (0.50x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 30 - 0.041%


Combination 32-bytes [0-1] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9257 (0.57x)
Testing collisions (high 27-41 bits) - Worst is 35 bits: 2035/2047 (0.99x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8066 (0.49x)
Testing collisions (low  27-41 bits) - Worst is 41 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 62 - 0.041%


Combination 32-bytes [0-last] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9270 (0.57x)
Testing collisions (high 27-41 bits) - Worst is 37 bits: 537/511 (1.05x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8201 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 41 bits: 21/31 (0.66x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit  9 - 0.040%


Combination 64-bytes [0-1] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9234 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 35 bits: 2067/2047 (1.01x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8203 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 39 bits: 67/127 (0.52x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 60 - 0.040%


Combination 64-bytes [0-last] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9246 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 40 bits: 79/63 (1.23x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8287 (0.51x)
Testing collisions (low  27-41 bits) - Worst is 37 bits: 276/511 (0.54x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 56 - 0.022%


Combination 128-bytes [0-1] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9077 (0.55x)
Testing collisions (high 27-41 bits) - Worst is 35 bits: 2053/2047 (1.00x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8202 (0.50x)
Testing collisions (low  27-41 bits) - Worst is 38 bits: 136/255 (0.53x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 54 - 0.025%


Combination 128-bytes [0-last] Tests:
Keyset 'Combination' - up to 22 blocks from a set of 2 - 8388606 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      16384.0, actual   9192 (0.56x)
Testing collisions (high 27-41 bits) - Worst is 39 bits: 139/127 (1.09x)
Testing collisions (high 12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      16384.0, actual   8321 (0.51x)
Testing collisions (low  27-41 bits) - Worst is 32 bits: 8321/16383 (0.51x)
Testing collisions (low  12-bit) - Expected    8388606.0, actual 8384510 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    8388606.0, actual 8388350 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 43 - 0.051%


[[[ Keyset 'Window' Tests ]]]

Keyset 'Window' - 136-bit key,  20-bit window - 136 tests, 1048576 keys per test
Window at   0 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   1 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   2 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   3 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   4 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   5 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   6 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   7 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   8 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at   9 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  10 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  11 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  12 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  13 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  14 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  15 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  16 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  17 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  18 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  19 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  20 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  21 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  22 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  23 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  24 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  25 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  26 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  27 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  28 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  29 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  30 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  31 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  32 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  33 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  34 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  35 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  36 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  37 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  38 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  39 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  40 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  41 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  42 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  43 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  44 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  45 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  46 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  47 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  48 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  49 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  50 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  51 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  52 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  53 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  54 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  55 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  56 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  57 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  58 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  59 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  60 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  61 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  62 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  63 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  64 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  65 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  66 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  67 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  68 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  69 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  70 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  71 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  72 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  73 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  74 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  75 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  76 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  77 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  78 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  79 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  80 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  81 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  82 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  83 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  84 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  85 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  86 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  87 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  88 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  89 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  90 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  91 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  92 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  93 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  94 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  95 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  96 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  97 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  98 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at  99 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 100 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 101 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 102 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 103 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 104 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 105 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 106 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 107 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 108 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 109 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 110 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 111 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 112 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 113 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 114 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 115 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 116 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 117 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 118 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 119 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 120 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 121 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 122 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 123 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 124 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 125 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 126 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 127 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 128 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 129 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 130 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 131 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 132 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 133 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 134 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 135 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Window at 136 - Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)

[[[ Keyset 'Cyclic' Tests ]]]

Keyset 'Cyclic' - 8 cycles of 8 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    112 (0.48x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 33/29 (1.13x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    104 (0.45x)
Testing collisions (low  24-35 bits) - Worst is 30 bits: 478/931 (0.51x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 13 - 0.118%

Keyset 'Cyclic' - 8 cycles of 9 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    133 (0.57x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 30/29 (1.03x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    130 (0.56x)
Testing collisions (low  24-35 bits) - Worst is 34 bits: 41/58 (0.70x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 46 - 0.082%

Keyset 'Cyclic' - 8 cycles of 10 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    131 (0.56x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 28/29 (0.96x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    124 (0.53x)
Testing collisions (low  24-35 bits) - Worst is 35 bits: 22/29 (0.76x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 28 - 0.117%

Keyset 'Cyclic' - 8 cycles of 11 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    146 (0.63x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 36/29 (1.24x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    100 (0.43x)
Testing collisions (low  24-35 bits) - Worst is 29 bits: 948/1862 (0.51x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 17 - 0.129%

Keyset 'Cyclic' - 8 cycles of 12 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    135 (0.58x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 28/29 (0.96x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    105 (0.45x)
Testing collisions (low  24-35 bits) - Worst is 34 bits: 30/58 (0.52x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 25 - 0.101%

Keyset 'Cyclic' - 8 cycles of 16 bytes - 1000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected        232.8, actual    112 (0.48x)
Testing collisions (high 24-35 bits) - Worst is 35 bits: 37/29 (1.27x)
Testing collisions (high 12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected        232.8, actual    123 (0.53x)
Testing collisions (low  24-35 bits) - Worst is 33 bits: 66/116 (0.57x)
Testing collisions (low  12-bit) - Expected    1000000.0, actual 995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    1000000.0, actual 999744 (1.00x) (-256)
Testing distribution - Worst bias is the 17-bit window at bit 40 - 0.108%


[[[ Keyset 'TwoBytes' Tests ]]]

Keyset 'TwoBytes' - up-to-4-byte keys, 652545 total keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected         99.1, actual     52 (0.52x)
Testing collisions (high 23-34 bits) - Worst is 34 bits: 22/24 (0.89x)
Testing collisions (high 12-bit) - Expected     652545.0, actual 648449 (0.99x) (-4096)
Testing collisions (high  8-bit) - Expected     652545.0, actual 652289 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected         99.1, actual     45 (0.45x)
Testing collisions (low  23-34 bits) - Worst is 24 bits: 12455/25380 (0.49x)
Testing collisions (low  12-bit) - Expected     652545.0, actual 648449 (0.99x) (-4096)
Testing collisions (low   8-bit) - Expected     652545.0, actual 652289 (1.00x) (-256)
Testing distribution - Worst bias is the 16-bit window at bit  9 - 0.129%

Keyset 'TwoBytes' - up-to-8-byte keys, 5471025 total keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       6969.1, actual   3870 (0.56x)
Testing collisions (high 26-40 bits) - Worst is 40 bits: 34/27 (1.25x)
Testing collisions (high 12-bit) - Expected    5471025.0, actual 5466929 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    5471025.0, actual 5470769 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       6969.1, actual   3488 (0.50x)
Testing collisions (low  26-40 bits) - Worst is 40 bits: 21/27 (0.77x)
Testing collisions (low  12-bit) - Expected    5471025.0, actual 5466929 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    5471025.0, actual 5470769 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 33 - 0.064%

Keyset 'TwoBytes' - up-to-12-byte keys, 18616785 total keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      80695.5, actual  45164 (0.56x)
Testing collisions (high 28-43 bits) - Worst is 40 bits: 331/315 (1.05x)
Testing collisions (high 12-bit) - Expected   18616785.0, actual 18612689 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   18616785.0, actual 18616529 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      80695.5, actual  40109 (0.50x)
Testing collisions (low  28-43 bits) - Worst is 41 bits: 93/157 (0.59x)
Testing collisions (low  12-bit) - Expected   18616785.0, actual 18612689 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   18616785.0, actual 18616529 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 53 - 0.014%

Keyset 'TwoBytes' - up-to-16-byte keys, 44251425 total keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected     455926.3, actual 253807 (0.56x)
Testing collisions (high 29-46 bits) - Worst is 46 bits: 35/27 (1.26x)
Testing collisions (high 12-bit) - Expected   44251425.0, actual 44247329 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   44251425.0, actual 44251169 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected     455926.3, actual 226834 (0.50x)
Testing collisions (low  29-46 bits) - Worst is 44 bits: 69/111 (0.62x)
Testing collisions (low  12-bit) - Expected   44251425.0, actual 44247329 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   44251425.0, actual 44251169 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 35 - 0.007%

Keyset 'TwoBytes' - up-to-20-byte keys, 86536545 total keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected    1743569.4, actual 969936 (0.56x)
Testing collisions (high 30-48 bits) - Worst is 47 bits: 61/53 (1.15x)
Testing collisions (high 12-bit) - Expected   86536545.0, actual 86532449 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   86536545.0, actual 86536289 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected    1743569.4, actual 865623 (0.50x)
Testing collisions (low  30-48 bits) - Worst is 44 bits: 256/425 (0.60x)
Testing collisions (low  12-bit) - Expected   86536545.0, actual 86532449 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   86536545.0, actual 86536289 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 48 - 0.004%


[[[ 'MomentChi2' Tests ]]]

Running 1st unseeded MomentChi2 for the low 32bits/step 3 ... 38919480.173781 - 410480.841629
Running 2nd   seeded MomentChi2 for the low 32bits/step 3 ... 38919725.449439 - 410501.016028
KeySeedMomentChi2:  0.0732783 PASS

[[[ Keyset 'Text' Tests ]]]

Keyset 'Text' - keys of form "Foo[XXXX]Bar" - 14776336 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      50836.3, actual  28302 (0.56x)
Testing collisions (high 28-43 bits) - Worst is 43 bits: 36/24 (1.45x)
Testing collisions (high 12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      50836.3, actual  25326 (0.50x)
Testing collisions (low  28-43 bits) - Worst is 41 bits: 52/99 (0.52x)
Testing collisions (low  12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 39 - 0.026%

Keyset 'Text' - keys of form "FooBar[XXXX]" - 14776336 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      50836.3, actual  28738 (0.57x)
Testing collisions (high 28-43 bits) - Worst is 39 bits: 417/397 (1.05x)
Testing collisions (high 12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      50836.3, actual  25405 (0.50x)
Testing collisions (low  28-43 bits) - Worst is 34 bits: 6467/12709 (0.51x)
Testing collisions (low  12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 12 - 0.037%

Keyset 'Text' - keys of form "[XXXX]FooBar" - 14776336 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected      50836.3, actual  28345 (0.56x)
Testing collisions (high 28-43 bits) - Worst is 41 bits: 103/99 (1.04x)
Testing collisions (high 12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected      50836.3, actual  25733 (0.51x)
Testing collisions (low  28-43 bits) - Worst is 43 bits: 16/24 (0.64x)
Testing collisions (low  12-bit) - Expected   14776336.0, actual 14772240 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected   14776336.0, actual 14776080 (1.00x) (-256)
Testing distribution - Worst bias is the 20-bit window at bit 61 - 0.030%


[[[ Keyset 'Zeroes' Tests ]]]

Keyset 'Zeroes' - 204800 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected          9.8, actual      5 (0.51x)
Testing collisions (high 21-30 bits) - Worst is 29 bits: 45/78 (0.58x)
Testing collisions (high 12-bit) - Expected     204800.0, actual 200704 (0.98x)
Testing collisions (high  8-bit) - Expected     204800.0, actual 204544 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected          9.8, actual      5 (0.51x)
Testing collisions (low  21-30 bits) - Worst is 30 bits: 22/39 (0.56x)
Testing collisions (low  12-bit) - Expected     204800.0, actual 200704 (0.98x)
Testing collisions (low   8-bit) - Expected     204800.0, actual 204544 (1.00x) (-256)
Testing distribution - Worst bias is the 15-bit window at bit 19 - 0.135%


[[[ Keyset 'Seed' Tests ]]]

Keyset 'Seed' - 5000000 keys
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       5820.8, actual   3192 (0.55x)
Testing collisions (high 26-40 bits) - Worst is 40 bits: 28/22 (1.23x)
Testing collisions (high 12-bit) - Expected    5000000.0, actual 4995904 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    5000000.0, actual 4999744 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       5820.8, actual   2864 (0.49x)
Testing collisions (low  26-40 bits) - Worst is 30 bits: 11582/23283 (0.50x)
Testing collisions (low  12-bit) - Expected    5000000.0, actual 4995904 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    5000000.0, actual 4999744 (1.00x) (-256)
Testing distribution - Worst bias is the 19-bit window at bit  4 - 0.050%


[[[ Diff 'Differential' Tests ]]]

Testing 8303632 up-to-5-bit differentials in 64-bit keys -> 64 bit hashes.
1000 reps, 8303632000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored

Testing 11017632 up-to-4-bit differentials in 128-bit keys -> 64 bit hashes.
1000 reps, 11017632000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored

Testing 2796416 up-to-3-bit differentials in 256-bit keys -> 64 bit hashes.
1000 reps, 2796416000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored


[[[ DiffDist 'Differential Distribution' Tests ]]]

Testing bit 0
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    505 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 22/31 (0.69x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    509 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 144/255 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 1
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    528 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 21/31 (0.66x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    490 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 21/31 (0.66x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 2
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    543 (0.53x)
Testing collisions (high 25-37 bits) - Worst is 33 bits: 288/511 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    500 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 19/31 (0.59x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 3
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    520 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 31 bits: 1070/2047 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    501 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 30 bits: 2052/4095 (0.50x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 4
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    473 (0.46x)
Testing collisions (high 25-37 bits) - Worst is 26 bits: 32398/65535 (0.49x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    571 (0.56x)
Testing collisions (low  25-37 bits) - Worst is 32 bits: 571/1023 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 5
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    482 (0.47x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 34/63 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    500 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 23/31 (0.72x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 6
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    516 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    512 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 31 bits: 1062/2047 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 7
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    535 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    504 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 69/127 (0.54x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 8
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    514 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 30 bits: 2082/4095 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    484 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 131/255 (0.51x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 9
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    487 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 67/127 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    499 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 37/63 (0.58x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 10
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    469 (0.46x)
Testing collisions (high 25-37 bits) - Worst is 27 bits: 16289/32767 (0.50x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    539 (0.53x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 75/127 (0.59x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 11
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    486 (0.47x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 23/31 (0.72x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    533 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 33 bits: 269/511 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 12
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    526 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 21/31 (0.66x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    466 (0.46x)
Testing collisions (low  25-37 bits) - Worst is 28 bits: 8208/16383 (0.50x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 13
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    473 (0.46x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    497 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 33 bits: 253/511 (0.49x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 14
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    505 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 32 bits: 505/1023 (0.49x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    532 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 147/255 (0.57x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 15
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    529 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    577 (0.56x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 20/31 (0.63x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 16
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    519 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 29 bits: 4207/8191 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    508 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 27 bits: 16260/32767 (0.50x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 17
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    528 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 80/127 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    518 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 138/255 (0.54x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 18
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    574 (0.56x)
Testing collisions (high 25-37 bits) - Worst is 33 bits: 296/511 (0.58x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    537 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 21/31 (0.66x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 19
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    487 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    553 (0.54x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 146/255 (0.57x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 20
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    568 (0.55x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 26/31 (0.81x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    521 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 66/127 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 21
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    474 (0.46x)
Testing collisions (high 25-37 bits) - Worst is 27 bits: 16596/32767 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    527 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 136/255 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 22
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    528 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 42/63 (0.66x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    529 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 32 bits: 529/1023 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 23
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    531 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 44/63 (0.69x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    471 (0.46x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 68/127 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 24
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    557 (0.54x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 44/63 (0.69x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    494 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 25
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    498 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 72/127 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    537 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 34/63 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 26
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    528 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 33 bits: 270/511 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    484 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 38/63 (0.59x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 27
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    499 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 34 bits: 133/255 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    527 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 19/31 (0.59x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 28
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    517 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 40/63 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    529 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 29
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    507 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 40/63 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    509 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 34/63 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 30
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    508 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 34 bits: 129/255 (0.50x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    531 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 22/31 (0.69x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 31
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    534 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 74/127 (0.58x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    493 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 26/31 (0.81x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 32
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    524 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 32 bits: 524/1023 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    512 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 31 bits: 1053/2047 (0.51x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 33
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    562 (0.55x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 24/31 (0.75x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    484 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 43/63 (0.67x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 34
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    524 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 40/63 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    534 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 35
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    518 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 34 bits: 143/255 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    502 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 72/127 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 36
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    530 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    520 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 37
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    541 (0.53x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 19/31 (0.59x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    526 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 31 bits: 1054/2047 (0.51x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 38
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    497 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 28 bits: 8260/16383 (0.50x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    485 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 39
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    495 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 35/63 (0.55x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    531 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 32 bits: 531/1023 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 40
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    468 (0.46x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 16/31 (0.50x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    512 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 45/63 (0.70x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 41
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    496 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 29 bits: 4116/8191 (0.50x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    484 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 26 bits: 32118/65535 (0.49x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 42
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    503 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 19/31 (0.59x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    530 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 32 bits: 530/1023 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 43
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    544 (0.53x)
Testing collisions (high 25-37 bits) - Worst is 32 bits: 544/1023 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    511 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 17/31 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 44
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    547 (0.53x)
Testing collisions (high 25-37 bits) - Worst is 32 bits: 547/1023 (0.53x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    494 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 34/63 (0.53x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 45
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    510 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 29 bits: 4189/8191 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    488 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 127/255 (0.50x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 46
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    499 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 19/31 (0.59x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    496 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 30 bits: 2085/4095 (0.51x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 47
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    561 (0.55x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 22/31 (0.69x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    475 (0.46x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 42/63 (0.66x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 48
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    498 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 33/63 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    509 (0.50x)
Testing collisions (low  25-37 bits) - Worst is 33 bits: 278/511 (0.54x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 49
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    496 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 28 bits: 8305/16383 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    487 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 50
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    527 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 70/127 (0.55x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    500 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 38/63 (0.59x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 51
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    503 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 36 bits: 37/63 (0.58x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    504 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 46/63 (0.72x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 52
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    494 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 66/127 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    505 (0.49x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 139/255 (0.54x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 53
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    492 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 35 bits: 80/127 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    537 (0.52x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 54
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    492 (0.48x)
Testing collisions (high 25-37 bits) - Worst is 34 bits: 131/255 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    546 (0.53x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 20/31 (0.63x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 55
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    499 (0.49x)
Testing collisions (high 25-37 bits) - Worst is 29 bits: 4181/8191 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    554 (0.54x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 36/63 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 56
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    532 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 33 bits: 281/511 (0.55x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    486 (0.47x)
Testing collisions (low  25-37 bits) - Worst is 36 bits: 39/63 (0.61x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 57
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    486 (0.47x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 20/31 (0.63x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    495 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 20/31 (0.63x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 58
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    516 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 33 bits: 262/511 (0.51x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    494 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 30 bits: 2115/4095 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 59
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    525 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 24/31 (0.75x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    488 (0.48x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 66/127 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 60
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    531 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 32 bits: 531/1023 (0.52x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    521 (0.51x)
Testing collisions (low  25-37 bits) - Worst is 35 bits: 72/127 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 61
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    512 (0.50x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 21/31 (0.66x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    469 (0.46x)
Testing collisions (low  25-37 bits) - Worst is 34 bits: 133/255 (0.52x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 62
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    526 (0.51x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    555 (0.54x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 22/31 (0.69x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)

Testing bit 63
Testing collisions ( 64-bit)     - Expected          0.0, actual      0 (0.00x)
Testing collisions (high 32-bit) - Expected       1024.0, actual    529 (0.52x)
Testing collisions (high 25-37 bits) - Worst is 37 bits: 26/31 (0.81x)
Testing collisions (high 12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (high  8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)
Testing collisions (low  32-bit) - Expected       1024.0, actual    540 (0.53x)
Testing collisions (low  25-37 bits) - Worst is 37 bits: 18/31 (0.56x)
Testing collisions (low  12-bit) - Expected    2097152.0, actual 2093056 (1.00x) (-4096)
Testing collisions (low   8-bit) - Expected    2097152.0, actual 2096896 (1.00x) (-256)



Input vcode 0x00000001, Output vcode 0x00000001, Result vcode 0x00000001
Verification value is 0x00000001 - Testing took 943.273420 seconds
-------------------------------------------------------------------------------
```
