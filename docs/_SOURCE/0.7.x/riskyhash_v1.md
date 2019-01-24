---
title: facil.io - Risky Hash
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

Risky Hash is a keyed hashing function which was inspired by both xxHash and SipHash.

It provides a fast alternative to SipHash when hashing safe data and a streaming variation can be easily implemented.

Risky Hash wasn't properly tested for attack resistance and shouldn't be used with any data that might be malicious, since it's unknown if this could result in [hash flooding attacks](http://emboss.github.io/blog/2012/12/14/breaking-murmur-hash-flooding-dos-reloaded/) (see [here](https://medium.freecodecamp.org/hash-table-attack-8e4371fc5261)).

Risky Hash was tested with [`SMHasher`](https://github.com/rurban/smhasher) ([see results](#smhasher-results)) (passed).

A non-streaming [reference implementation in C is attached](#in-code) The code is easy to read and should be considered as the actual specification.

## Status

Risky Hash is still under development and review. This specification should be considered a working draft.

Risky Hash should be limited to testing and safe environments until it's fully analyzed and reviewed.

## Purpose

Risky Hash is designed for fast Hash Map key calculation for both big and small keys. It attempts to act as a 64 bit keyed PRF.

It's possible to compile facil.io with Risk Hash as the default hashing function (the current default is SipHash1-3) by defining the `FIO_USE_RISKY_HASH` during compilation (`-DFIO_USE_RISKY_HASH`).

## Overview

Risky Hash has three stages:

* Initialization stage.

* Reading stage.

* Mixing stage.

The hashing algorithm uses an internal 256 bit state for a 64 bit output and the input data is mixed twice into the state, using different operations (XOR, addition) on different bit positions (left rotation).

This approach **should** minimize the risk of malicious data weakening the hash function.

### Overview: Initialization

In the initialization stage, Risky Hash attempts to achieves three goals:

* Initialize the hash state using a "secret" (key / salt / seed) in a way that will result in the "secret" having a meaningful impact on the final result.

* Initialize the state in a way that can't be reversed/controlled by a maliciously crafted message.

* Initialize the state with minimal bias (bits have a fairly even chance at being set or unset).

### Overview: Consumption (reading)

In the consumption stage, Risky Hash attempts to achieves three goals:

* Maliciously crafted data won't be able to weaken the hash function or expose the "secret".

    This is achieved by reading the data twice and using a different operation each time. This minimizes the possibility of finding a malicious value that could break both operations.

* Repeated data blocks should produce different results according to their position.

   This is achieved by performing left-rotation and prime multiplication in a way that causes different positions to have different side effects.

* Allow parallelism.

   This is achieved by using a number of distinct and separated reading "vectors".

   This also imposes a constraint about the number of registers, or "hidden variables", each vector should use.

   (for example `a += a + b` could require two registers, while `a = (a * 2) + b` requires one)

It should be noted that Risky Hash consumes data in 64 bit chunks/words.

Any trailing data that doesn't fit in a 64 bit word is padded with zeros and consumed by a specific consumption vector (rather than consumed in order). 

### Overview: Mixing

In the mixing stage, Risky Hash attempts to achieves three goals:

* Be irreversible.

   This stage is the "last line of defense" against malicious data or possible collisions. For this reason, it should be infeasible to extract meaningful data from the final result.

* Produce a message digest with minimal bias (bits have a fairly even chance at being set or unset).

* Allow all consumption vectors an equal but different effect on the final hash value.

Until this point, Risky Hash is contained in four 64 bit hash vectors, each hashing a quarter of the input data.

At this stage, the 256 bits of data are reduced to a 64 bit result.

## Specifics

A non-streaming C implementation can be found at the `fio.h` header, in the static function: `fio_risky_hash` and later on in this document.

Risky Hash uses 4 reading vectors, each containing 64 bits.

Risky Hash requires a 64 bit "secret". Zero is a valid secret, but is highly discouraged. A rotating random number, even if exposed, is much more likely to mitigate any risks than no secret at all.

Risky Hash uses the following prime numbers:

```txt
P[0] = 0xFBBA3FA15B22113B
P[1] = 0xAB137439982B86C9
```

The following operations are used:

* `~` marks a bit inversion.
* `+` marks a mod 2^64 addition.
* `XOR` marks an XOR operation.
* `MUL(x,y)` a mod 2^64 multiplication.
* `LROT(x,bits)` is a left rotation of a 64 bit word.
* `>>` is a right shift (not rotate, some bits are lost).

### Initialization

The four consumption vectors are initialized using the seed ("secret") like so:

```txt
V1 = seed XOR P[1],
V2 = (~seed) + P[1],
V3 = LROT(seed, 17) XOR P[1],
V4 = LROT(seed, 33) + P[1],
```

### Consumption

Each vector reads a single 64 bit word within a 256 bit block, allowing the vectors to be parallelized though any message length of 256 bits or longer.

`V1` reads the first 64 bits, `V2` reads bits 65-128, and so forth...

The 64 bits are read in network byte order (Big-Endian) and treated as a numerical value.

Each vector performs the following operations in each of it's consumption rounds (`V` is the vector, `word` is the input data for that vector):

```txt
V = V XOR word
V = LROT(V, 33) + word
V = MUL(P[0], V)
```

If the data fits evenly in 64 bit words, than it will be read with no padding, even if some vectors perform more consumption rounds than others.

If the last 64 bit word is incomplete, it will be padded with zeros (0) and consumed by the last vector (`V4`), regardless of it's position within a 256 bit block.

### Hash Mixing

At this point the length of the data is finalized an can be added to the calculation.

The following intermediate 64 bit result is calculated:

```txt
result = LROT(V1,17) + LROT(V2,13) + LROT(V3,47) + LROT(V4,57)
```

The consumed (unpadded) message length is added to this word:

```txt
result = result + length
```

The vectors are mixed in with the word using prime number multiplication to minimize any bias:

```txt
result = result + MUL(V1, P[1])
result = result XOR LROT(result, 13);
result = result + MUL(V2, P[1])
result = result XOR LROT(result, 29);
result = result + MUL(V3, P[1])
result = result XOR LROT(result, 33);
result = result + MUL(V4, P[1])
result = result XOR LROT(result, 51);
```

Finally, the result is mixed with itself to improve bit entropy distribution and hinder reversibility.

```txt
result = result XOR MUL(P[0], (result >> 29) ) 
```

## Performance

Risky Hash attempts to balance performance with security concerns, since hash functions are often use by insecure hash table implementations.

However, the design should allow for fairly high performance, for example, by using SIMD instructions or a multi-threaded approach (up to 4 threads).

In fact, even the simple reference implementation at the end of this document offers fairly high performance, averaging 17% faster than xxHash for short keys (up to 31 bytes) and 9% slower on long keys (262,144 bytes).

## Attacks, Reports and Security

This (previous) draft of Risky Hash can be attacked using a [meet-in-the-middle / Long-Neighbor attack](https://github.com/hmakholm/smhasher/blob/master/src/LongNeighborTest.md) which was developed by Henning Makholm in January 2019.

At this early stage, please feel free to attack the Risky Hash algorithm and report any security concerns in the [GitHub issue tracker](https://github.com/boazsegev/facil.io/issues).

Later, as Risky Hash usage might increase, attacks should be reported discretely if possible, allowing for a fix to be provided before publication.

## In Code

In C code, the above description might translate like so:

```c
/*
Copyright: Boaz Segev, 2019
License: MIT
*/

/** 64Bit left rotation, inlined. */
#define fio_lrot64(i, bits)                                                    \
  (((uint64_t)(i) << (bits)) | ((uint64_t)(i) >> ((-(bits)) & 63UL)))

/** Converts an unaligned network ordered byte stream to a 64 bit number. */
#define fio_str2u64(c)                                                         \
  ((uint64_t)((((uint64_t)((uint8_t *)(c))[0]) << 56) |                        \
              (((uint64_t)((uint8_t *)(c))[1]) << 48) |                        \
              (((uint64_t)((uint8_t *)(c))[2]) << 40) |                        \
              (((uint64_t)((uint8_t *)(c))[3]) << 32) |                        \
              (((uint64_t)((uint8_t *)(c))[4]) << 24) |                        \
              (((uint64_t)((uint8_t *)(c))[5]) << 16) |                        \
              (((uint64_t)((uint8_t *)(c))[6]) << 8) |                         \
              ((uint64_t)0 + ((uint8_t *)(c))[7])))

uintptr_t risky_hash(const void *data_, size_t len, uint64_t seed) {
  /* The primes used by Risky Hash */
  const uint64_t primes[] = {
      0xFBBA3FA15B22113B, // 1111101110111010001111111010000101011011001000100001000100111011
      0xAB137439982B86C9, // 1010101100010011011101000011100110011000001010111000011011001001
  };
  /* The consumption vectors initialized state */
  uint64_t v[4] = {
      seed ^ primes[1],
      ~seed + primes[1],
      fio_lrot64(seed, 17) ^ primes[1],
      fio_lrot64(seed, 33) + primes[1],
  };

/* Risky Hash consumption round */
#define fio_risky_consume(w, i)                                                \
  v[i] ^= (w);                                                                 \
  v[i] = fio_lrot64(v[i], 33) + (w);                                           \
  v[i] *= primes[0];

/* compilers could, hopefully, optimize this code for SIMD */
#define fio_risky_consume256(w0, w1, w2, w3)                                   \
  fio_risky_consume(w0, 0);                                                    \
  fio_risky_consume(w1, 1);                                                    \
  fio_risky_consume(w2, 2);                                                    \
  fio_risky_consume(w3, 3);

  /* reading position */
  const uint8_t *data = (uint8_t *)data_;

  /* consume 256bit blocks */
  for (size_t i = len >> 5; i; --i) {
    fio_risky_consume256(fio_str2u64(data), fio_str2u64(data + 8),
                         fio_str2u64(data + 16), fio_str2u64(data + 24));
    data += 32;
  }
  /* Consume any remaining 64 bit words. */
  switch (len & 24) {
  case 24:
    fio_risky_consume(fio_str2u64(data + 16), 2);
  case 16: /* overflow */
    fio_risky_consume(fio_str2u64(data + 8), 1);
  case 8: /* overflow */
    fio_risky_consume(fio_str2u64(data), 0);
    data += len & 24;
  }

  uintptr_t tmp = 0;
  /* consume leftover bytes, if any */
  switch ((len & 7)) {
  case 7: /* overflow */
    tmp |= ((uint64_t)data[6]) << 56;
  case 6: /* overflow */
    tmp |= ((uint64_t)data[5]) << 48;
  case 5: /* overflow */
    tmp |= ((uint64_t)data[4]) << 40;
  case 4: /* overflow */
    tmp |= ((uint64_t)data[3]) << 32;
  case 3: /* overflow */
    tmp |= ((uint64_t)data[2]) << 24;
  case 2: /* overflow */
    tmp |= ((uint64_t)data[1]) << 16;
  case 1: /* overflow */
    tmp |= ((uint64_t)data[0]) << 8;
    fio_risky_consume(tmp, 3);
  }

  /* merge and mix */
  uint64_t result = fio_lrot64(v[0], 17) + fio_lrot64(v[1], 13) +
                    fio_lrot64(v[2], 47) + fio_lrot64(v[3], 57);
  result += len;
  result += v[0] * primes[1];
  result ^= fio_lrot64(result, 13);
  result += v[1] * primes[1];
  result ^= fio_lrot64(result, 29);
  result += v[2] * primes[1];
  result ^= fio_lrot64(result, 33);
  result += v[3] * primes[1];
  result ^= fio_lrot64(result, 51);

  /* irreversible avalanche... I think */
  result ^= (result >> 29) * primes[0];
  return result;

#undef fio_risky_consume256
#undef fio_risky_consume
}
```

## SMHasher results

The following results were produced on a 2.9 GHz Intel Core i9 machine and won't be updated every time.

```txt
-------------------------------------------------------------------------------
--- Testing RiskyHash "facil.io hashing (by Bo)"

[[[ Sanity Tests ]]]

Verification value 0x1A4E494A : PASS
Running sanity check 1    ..........PASS
Running AppendedZeroesTest..........PASS

[[[ Speed Tests ]]]

Bulk speed test - 262144-byte keys
Alignment  7 -  5.838 bytes/cycle - 16701.84 MiB/sec @ 3 ghz
Alignment  6 -  5.852 bytes/cycle - 16742.05 MiB/sec @ 3 ghz
Alignment  5 -  5.835 bytes/cycle - 16692.73 MiB/sec @ 3 ghz
Alignment  4 -  5.447 bytes/cycle - 15585.04 MiB/sec @ 3 ghz
Alignment  3 -  5.834 bytes/cycle - 16690.14 MiB/sec @ 3 ghz
Alignment  2 -  5.837 bytes/cycle - 16699.70 MiB/sec @ 3 ghz
Alignment  1 -  5.141 bytes/cycle - 14708.75 MiB/sec @ 3 ghz
Alignment  0 -  5.465 bytes/cycle - 15635.34 MiB/sec @ 3 ghz
Average      -  5.656 bytes/cycle - 16181.95 MiB/sec @ 3 ghz

Small key speed test -    1-byte keys -    22.31 cycles/hash
Small key speed test -    2-byte keys -    23.00 cycles/hash
Small key speed test -    3-byte keys -    24.00 cycles/hash
Small key speed test -    4-byte keys -    25.00 cycles/hash
Small key speed test -    5-byte keys -    25.00 cycles/hash
Small key speed test -    6-byte keys -    25.00 cycles/hash
Small key speed test -    7-byte keys -    25.00 cycles/hash
Small key speed test -    8-byte keys -    31.00 cycles/hash
Small key speed test -    9-byte keys -    31.00 cycles/hash
Small key speed test -   10-byte keys -    31.00 cycles/hash
Small key speed test -   11-byte keys -    31.00 cycles/hash
Small key speed test -   12-byte keys -    31.00 cycles/hash
Small key speed test -   13-byte keys -    31.00 cycles/hash
Small key speed test -   14-byte keys -    31.00 cycles/hash
Small key speed test -   15-byte keys -    31.00 cycles/hash
Small key speed test -   16-byte keys -    31.00 cycles/hash
Small key speed test -   17-byte keys -    31.00 cycles/hash
Small key speed test -   18-byte keys -    31.00 cycles/hash
Small key speed test -   19-byte keys -    31.00 cycles/hash
Small key speed test -   20-byte keys -    31.00 cycles/hash
Small key speed test -   21-byte keys -    31.00 cycles/hash
Small key speed test -   22-byte keys -    31.00 cycles/hash
Small key speed test -   23-byte keys -    31.41 cycles/hash
Small key speed test -   24-byte keys -    31.00 cycles/hash
Small key speed test -   25-byte keys -    31.00 cycles/hash
Small key speed test -   26-byte keys -    31.44 cycles/hash
Small key speed test -   27-byte keys -    31.00 cycles/hash
Small key speed test -   28-byte keys -    31.48 cycles/hash
Small key speed test -   29-byte keys -    31.00 cycles/hash
Small key speed test -   30-byte keys -    31.00 cycles/hash
Small key speed test -   31-byte keys -    31.00 cycles/hash
Average                                    29.505 cycles/hash

[[[ Differential Tests ]]]

Testing 8303632 up-to-5-bit differentials in 64-bit keys -> 64 bit hashes.
1000 reps, 8303632000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored

Testing 11017632 up-to-4-bit differentials in 128-bit keys -> 64 bit hashes.
1000 reps, 11017632000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored

Testing 2796416 up-to-3-bit differentials in 256-bit keys -> 64 bit hashes.
1000 reps, 2796416000 total tests, expecting 0.00 random collisions..........
0 total collisions, of which 0 single collisions were ignored


[[[ Avalanche Tests ]]]

Testing  32-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.575333%
Testing  40-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.660667%
Testing  48-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.632667%
Testing  56-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.696667%
Testing  64-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.734667%
Testing  72-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.766667%
Testing  80-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.762667%
Testing  88-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.806000%
Testing  96-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.756000%
Testing 104-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.723333%
Testing 112-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.693333%
Testing 120-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.654000%
Testing 128-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.799333%
Testing 136-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.824667%
Testing 144-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.715333%
Testing 152-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.664000%

[[[ Keyset 'Cyclic' Tests ]]]

Keyset 'Cyclic' - 8 cycles of 8 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  46 - 0.036%

Keyset 'Cyclic' - 8 cycles of 9 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  29 - 0.048%

Keyset 'Cyclic' - 8 cycles of 10 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  17 - 0.029%

Keyset 'Cyclic' - 8 cycles of 11 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  59 - 0.048%

Keyset 'Cyclic' - 8 cycles of 12 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  14 - 0.030%


[[[ Keyset 'TwoBytes' Tests ]]]

Keyset 'TwoBytes' - up-to-4-byte keys, 652545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  16-bit window at bit   1 - 0.151%

Keyset 'TwoBytes' - up-to-8-byte keys, 5471025 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  48 - 0.059%

Keyset 'TwoBytes' - up-to-12-byte keys, 18616785 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   6 - 0.020%

Keyset 'TwoBytes' - up-to-16-byte keys, 44251425 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  48 - 0.008%

Keyset 'TwoBytes' - up-to-20-byte keys, 86536545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  40 - 0.005%


[[[ Keyset 'Sparse' Tests ]]]

Keyset 'Sparse' - 32-bit keys with up to 6 bits set - 1149017 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  20 - 0.178%

Keyset 'Sparse' - 40-bit keys with up to 6 bits set - 4598479 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  51 - 0.063%

Keyset 'Sparse' - 48-bit keys with up to 5 bits set - 1925357 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  27 - 0.101%

Keyset 'Sparse' - 56-bit keys with up to 5 bits set - 4216423 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  21 - 0.057%

Keyset 'Sparse' - 64-bit keys with up to 5 bits set - 8303633 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  62 - 0.037%

Keyset 'Sparse' - 96-bit keys with up to 4 bits set - 3469497 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  23 - 0.087%

Keyset 'Sparse' - 256-bit keys with up to 3 bits set - 2796417 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  28 - 0.056%

Keyset 'Sparse' - 2048-bit keys with up to 2 bits set - 2098177 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  51 - 0.080%


[[[ Keyset 'Combination Lowbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  26 - 0.017%


[[[ Keyset 'Combination Highbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   5 - 0.017%


[[[ Keyset 'Combination 0x8000000' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit   0 - 0.085%


[[[ Keyset 'Combination 0x0000001' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  51 - 0.056%


[[[ Keyset 'Combination Hi-Lo' Tests ]]]

Keyset 'Combination' - up to 6 blocks from a set of 15 - 12204240 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   1 - 0.021%


[[[ Keyset 'Window' Tests ]]]

Keyset 'Windowed' - 128-bit key,  20-bit window - 128 tests, 1048576 keys per test
Window at   0 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   1 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   2 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   3 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   4 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   5 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   6 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   7 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   8 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at   9 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  10 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  11 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  12 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  13 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  14 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  15 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  16 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  17 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  18 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  19 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  20 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  21 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  22 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  23 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  24 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  25 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  26 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  27 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  28 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  29 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  30 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  31 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  32 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  33 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  34 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  35 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  36 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  37 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  38 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  39 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  40 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  41 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  42 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  43 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  44 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  45 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  46 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  47 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  48 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  49 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  50 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  51 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  52 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  53 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  54 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  55 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  56 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  57 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  58 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  59 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  60 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  61 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  62 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  63 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  64 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  65 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  66 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  67 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  68 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  69 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  70 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  71 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  72 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  73 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  74 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  75 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  76 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  77 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  78 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  79 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  80 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  81 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  82 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  83 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  84 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  85 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  86 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  87 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  88 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  89 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  90 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  91 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  92 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  93 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  94 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  95 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  96 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  97 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  98 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at  99 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 100 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 101 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 102 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 103 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 104 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 105 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 106 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 107 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 108 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 109 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 110 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 111 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 112 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 113 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 114 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 115 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 116 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 117 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 118 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 119 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 120 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 121 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 122 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 123 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 124 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 125 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 126 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 127 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Window at 128 - Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)

[[[ Keyset 'Text' Tests ]]]

Keyset 'Text' - keys of form "Foo[XXXX]Bar" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  40 - 0.022%

Keyset 'Text' - keys of form "FooBar[XXXX]" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   5 - 0.023%

Keyset 'Text' - keys of form "[XXXX]FooBar" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  44 - 0.022%


[[[ Keyset 'Zeroes' Tests ]]]

Keyset 'Zeroes' - 65536 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  13-bit window at bit  63 - 0.477%


[[[ Keyset 'Seed' Tests ]]]

Keyset 'Seed' - 1000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  37 - 0.064%



Input vcode 0x00000001, Output vcode 0x00000001, Result vcode 0x00000001
Verification value is 0x00000001 - Testing took 806.795982 seconds
-------------------------------------------------------------------------------
```
