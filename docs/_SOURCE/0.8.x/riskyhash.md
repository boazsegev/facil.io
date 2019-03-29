---
title: facil.io - Risky Hash
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

Risky Hash is a keyed hashing function which was inspired by both xxHash and SipHash.

It's meant to provide a fast alternative to SipHash.

Risky Hash wasn't properly tested for attack resistance and shouldn't be used with any data that might be malicious, since it's unknown if this could result in [hash flooding attacks](http://emboss.github.io/blog/2012/12/14/breaking-murmur-hash-flooding-dos-reloaded/) (see [here](https://medium.freecodecamp.org/hash-table-attack-8e4371fc5261)).

Risky Hash was tested with [`SMHasher`](https://github.com/rurban/smhasher) ([see results](#smhasher-results)) (passed).

A non-streaming [reference implementation in C is attached](#in-code) The code is easy to read and should be considered an integral part of this specification.

## Status

> Risky Hash is still under development and review. This specification should be considered a working draft.
 
> Risky Hash should be limited to testing and safe environments until it's fully analyzed and reviewed.

This is the second draft of the RiskyHash algorithm and it incorporates updates from community driven feedback.

* Florian Weber (@Florianjw) [exposed coding errors (last 8 byte ordering) and took a bit of time to challenge the algorithm](https://www.reddit.com/r/crypto/comments/9kk5gl/break_my_ciphercollectionpost/eekxw2f/?context=3) and make a few suggestions.

    Following this feedback, the error in the code was fixed, the initialization state was updated and the left-over bytes are now read in order with padding (rather than consumed by the 4th state-word).

* Chris Anderson (@injinj) did amazing work exploring a 128 bit variation and attacking RiskyHash using a variation on a Meet-In-The-Middle attack, written by Hening Makholm (@hmakholm), that discovers hash collisions with a small Hamming distance ([SMHasher fork](https://github.com/hmakholm/smhasher)).

    Following this input, RiskyHash updated the way the message length is incorporated into the final hash and updated the consumption stage to replace the initial XOR with ADD.

The [previous version can be accessed here](riskyhash_v1).

## Purpose

Risky Hash is designed for fast Hash Map key calculation for both big and small keys. It attempts to act as a 64 bit keyed PRF.

It's possible to compile facil.io with Risk Hash as the default hashing function (the current default is SipHash1-3) by defining the `FIO_USE_RISKY_HASH` during compilation (`-DFIO_USE_RISKY_HASH`).

## Algorithm

A non-streaming C implementation can be found at the `fio.h` header, in the static function: `fio_risky_hash` [and later on in this document](#in-code).

Risky Hash uses 4 reading vectors (state-words), each containing 64 bits.

Risky Hash requires a 64 bit "secret". Zero is a valid secret.

Risky Hash uses the following prime numbers:

```txt
P[0] = 0xFBBA3FA15B22113B
P[1] = 0xAB137439982B86C9
```

Risky Hash has three stages:

* Initialization stage.

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

In the initialization stage, Risky Hash attempts to achieves three goals:

* Initialize the hash state using a "secret" (key / salt / seed) in a way that will result in the "secret" having a meaningful impact on the final result.

* Initialize the state in a way that can't be reversed/controlled by a maliciously crafted message.

* Initialize the state with minimal bias (bits have a fairly even chance at being set or unset).

The four consumption vectors are initialized using the seed ("secret") like so:

```txt
V1 = seed XOR P[1]
V2 = (~seed) + P[1]
V3 = LROT(seed, 17) XOR ((~P[0]) + P[1])
V4 = LROT(seed, 33) + (~P[1])
```

### Consumption

In the consumption stage, Risky Hash attempts to achieves three goals:

* Allow parallelism.

   This is achieved by using a number of distinct and separated reading "vectors" (independent state-words).

* Repeated data blocks should produce different results according to their position.

   This is achieved by performing left-rotation and prime multiplication during each round, so the vector is mutated every step (regardless of the input data).

* Maliciously crafted data won't be able to weaken the hash function or expose the "secret".

    This is achieved by reading the data twice into different positions in the consumption vector. This minimizes the possibility of finding a malicious value that could break the state vector.

Risky Hash consumes data in 64 bit chunks/words.

Each vector reads a single 64 bit word within a 256 bit block, allowing the vectors to be parallelized though any message length of 256 bits or longer.

`V1` reads the first 64 bits, `V2` reads bits 65-128, and so forth...

The 64 bits are read in network byte order (Big-Endian) and treated as a numerical value.

Any trailing data that doesn't fit in a 64 bit word is padded with zeros and consumed **in order** (by the next available vector). 

Each vector performs the following operations in each of it's consumption rounds (`V` is the vector, `word` is the input data for that vector as a 64 bit word):

```txt
V = V + word
V = LROT(V, 33)
V = V + word
V = MUL(V, P[0])
```

It is normal for some vectors to perform more consumption rounds than others when the data isn't divisible into 256 bit blocks.

### Hash Mixing

In the mixing stage, Risky Hash attempts to achieves three goals:

* Be irreversible.

   This stage is the "last line of defense" against malicious data or possible collisions. For this reason, it should be infeasible to extract meaningful data from the final result.

* Produce a message digest with minimal bias (bits have a fairly even chance at being set or unset).

* Allow all consumption vectors an equal but different effect on the final hash value.

The following intermediate 64 bit result is calculated:

```txt
result = LROT(V1,17) + LROT(V2,13) + LROT(V3,47) + LROT(V4,57)
```

At this point the length of the input data is finalized an can be added to the calculation.

The consumed (unpadded) message length is treated as a 64 bit word. It is shifted left by 33 bits and XORed with itself. Then, the updated length value is added to the intermediate result:

```txt
length = length XOR (length << 33);
result = result + length
```

The vectors are mixed in with the intermediate result using prime number multiplication to minimize any bias:

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

Finally, the intermediate result is mixed with itself to improve bit entropy distribution and hinder reversibility.

```txt
result = result XOR MUL(P[0], (result >> 29) ) 
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

/** 64 bit left rotation, inlined. */
#define fio_lrot64(i, bits)                                                    \
  (((uint64_t)(i) << ((bits)&63UL)) | ((uint64_t)(i) >> ((-(bits)) & 63UL)))

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

#ifdef __cplusplus
/* the register keyword was deprecated for C++ but is semi-meaningful in C */
#define register
#endif

/* Risky Hash primes */
#define RISKY_PRIME_0 0xFBBA3FA15B22113B
#define RISKY_PRIME_1 0xAB137439982B86C9
  
/* Risky Hash consumption round, accepts a state word s and an input word w */
#define fio_risky_consume(v, w)                                                \
  (v) += (w);                                                                  \
  (v) = fio_lrot64((v), 33);                                                   \
  (v) += (w);                                                                  \
  (v) *= RISKY_PRIME_0;

/*  Computes a facil.io Risky Hash. */
uint64_t fio_risky_hash(const void *data_, size_t len,
                                         uint64_t seed) {
  /* reading position */
  const uint8_t *data = (uint8_t *)data_;

  /* The consumption vectors initialized state */
  register uint64_t v0 = seed ^ RISKY_PRIME_1;
  register uint64_t v1 = ~seed + RISKY_PRIME_1;
  register uint64_t v2 =
      fio_lrot64(seed, 17) ^ ((~RISKY_PRIME_1) + RISKY_PRIME_0);
  register uint64_t v3 = fio_lrot64(seed, 33) + (~RISKY_PRIME_1);

  /* consume 256 bit blocks */
  for (size_t i = len >> 5; i; --i) {
    fio_risky_consume(v0, fio_str2u64(data));
    fio_risky_consume(v1, fio_str2u64(data + 8));
    fio_risky_consume(v2, fio_str2u64(data + 16));
    fio_risky_consume(v3, fio_str2u64(data + 24));
    data += 32;
  }

  /* Consume any remaining 64 bit words. */
  switch (len & 24) {
  case 24:
    fio_risky_consume(v2, fio_str2u64(data + 16));
  case 16: /* overflow */
    fio_risky_consume(v1, fio_str2u64(data + 8));
  case 8: /* overflow */
    fio_risky_consume(v0, fio_str2u64(data));
    data += len & 24;
  }

  uint64_t tmp = 0;
  /* consume leftover bytes, if any */
  switch ((len & 7)) {
  case 7: /* overflow */
    tmp |= ((uint64_t)data[6]) << 8;
  case 6: /* overflow */
    tmp |= ((uint64_t)data[5]) << 16;
  case 5: /* overflow */
    tmp |= ((uint64_t)data[4]) << 24;
  case 4: /* overflow */
    tmp |= ((uint64_t)data[3]) << 32;
  case 3: /* overflow */
    tmp |= ((uint64_t)data[2]) << 40;
  case 2: /* overflow */
    tmp |= ((uint64_t)data[1]) << 48;
  case 1: /* overflow */
    tmp |= ((uint64_t)data[0]) << 56;
    /* ((len >> 3) & 3) is a 0...3 value indicating consumption vector */
    switch ((len >> 3) & 3) {
    case 3:
      fio_risky_consume(v3, tmp);
      break;
    case 2:
      fio_risky_consume(v2, tmp);
      break;
    case 1:
      fio_risky_consume(v1, tmp);
      break;
    case 0:
      fio_risky_consume(v0, tmp);
      break;
    }
  }

  /* merge and mix */
  uint64_t result = fio_lrot64(v0, 17) + fio_lrot64(v1, 13) +
                    fio_lrot64(v2, 47) + fio_lrot64(v3, 57);

  len ^= (len << 33);
  result += len;

  result += v0 * RISKY_PRIME_1;
  result ^= fio_lrot64(result, 13);
  result += v1 * RISKY_PRIME_1;
  result ^= fio_lrot64(result, 29);
  result += v2 * RISKY_PRIME_1;
  result ^= fio_lrot64(result, 33);
  result += v3 * RISKY_PRIME_1;
  result ^= fio_lrot64(result, 51);

  /* irreversible avalanche... I think */
  result ^= (result >> 29) * RISKY_PRIME_0;
  return result;
}

#undef fio_risky_consume
#undef FIO_RISKY_PRIME_0
#undef FIO_RISKY_PRIME_1
```

## SMHasher results

The following results were produced on a 2.9 GHz Intel Core i9 machine and won't be updated every time.

```txt
-------------------------------------------------------------------------------
--- Testing RiskyHash "facil.io hashing (by Bo)"

[[[ Sanity Tests ]]]

Verification value 0x13AA4AB6 : PASS
Running sanity check 1    ..........PASS
Running AppendedZeroesTest..........PASS

[[[ Speed Tests ]]]

Bulk speed test - 262144-byte keys
Alignment  7 -  5.838 bytes/cycle - 16702.48 MiB/sec @ 3 ghz
Alignment  6 -  5.838 bytes/cycle - 16702.90 MiB/sec @ 3 ghz
Alignment  5 -  5.838 bytes/cycle - 16703.48 MiB/sec @ 3 ghz
Alignment  4 -  5.830 bytes/cycle - 16680.17 MiB/sec @ 3 ghz
Alignment  3 -  5.840 bytes/cycle - 16707.69 MiB/sec @ 3 ghz
Alignment  2 -  5.840 bytes/cycle - 16708.61 MiB/sec @ 3 ghz
Alignment  1 -  5.842 bytes/cycle - 16714.10 MiB/sec @ 3 ghz
Alignment  0 -  5.828 bytes/cycle - 16673.79 MiB/sec @ 3 ghz
Average      -  5.837 bytes/cycle - 16699.15 MiB/sec @ 3 ghz

Small key speed test -    1-byte keys -    25.00 cycles/hash
Small key speed test -    2-byte keys -    27.00 cycles/hash
Small key speed test -    3-byte keys -    28.00 cycles/hash
Small key speed test -    4-byte keys -    28.89 cycles/hash
Small key speed test -    5-byte keys -    28.98 cycles/hash
Small key speed test -    6-byte keys -    28.49 cycles/hash
Small key speed test -    7-byte keys -    28.95 cycles/hash
Small key speed test -    8-byte keys -    32.24 cycles/hash
Small key speed test -    9-byte keys -    32.00 cycles/hash
Small key speed test -   10-byte keys -    32.21 cycles/hash
Small key speed test -   11-byte keys -    32.12 cycles/hash
Small key speed test -   12-byte keys -    32.00 cycles/hash
Small key speed test -   13-byte keys -    32.00 cycles/hash
Small key speed test -   14-byte keys -    32.00 cycles/hash
Small key speed test -   15-byte keys -    32.17 cycles/hash
Small key speed test -   16-byte keys -    32.00 cycles/hash
Small key speed test -   17-byte keys -    32.85 cycles/hash
Small key speed test -   18-byte keys -    32.41 cycles/hash
Small key speed test -   19-byte keys -    32.29 cycles/hash
Small key speed test -   20-byte keys -    32.28 cycles/hash
Small key speed test -   21-byte keys -    32.50 cycles/hash
Small key speed test -   22-byte keys -    33.21 cycles/hash
Small key speed test -   23-byte keys -    32.00 cycles/hash
Small key speed test -   24-byte keys -    32.00 cycles/hash
Small key speed test -   25-byte keys -    32.42 cycles/hash
Small key speed test -   26-byte keys -    32.90 cycles/hash
Small key speed test -   27-byte keys -    32.42 cycles/hash
Small key speed test -   28-byte keys -    33.08 cycles/hash
Small key speed test -   29-byte keys -    32.31 cycles/hash
Small key speed test -   30-byte keys -    32.58 cycles/hash
Small key speed test -   31-byte keys -    32.68 cycles/hash
Average                                    31.355 cycles/hash

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

Testing  32-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.655333%
Testing  40-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.728000%
Testing  48-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.738000%
Testing  56-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.690667%
Testing  64-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.675333%
Testing  72-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.698000%
Testing  80-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.667333%
Testing  88-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.691333%
Testing  96-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.692000%
Testing 104-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.684000%
Testing 112-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.709333%
Testing 120-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.754000%
Testing 128-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.714667%
Testing 136-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.790667%
Testing 144-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.672000%
Testing 152-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.702667%

[[[ Keyset 'Cyclic' Tests ]]]

Keyset 'Cyclic' - 8 cycles of 8 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  12 - 0.044%

Keyset 'Cyclic' - 8 cycles of 9 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   6 - 0.028%

Keyset 'Cyclic' - 8 cycles of 10 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  59 - 0.034%

Keyset 'Cyclic' - 8 cycles of 11 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   1 - 0.044%

Keyset 'Cyclic' - 8 cycles of 12 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   2 - 0.029%


[[[ Keyset 'TwoBytes' Tests ]]]

Keyset 'TwoBytes' - up-to-4-byte keys, 652545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  16-bit window at bit   4 - 0.129%

Keyset 'TwoBytes' - up-to-8-byte keys, 5471025 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  42 - 0.058%

Keyset 'TwoBytes' - up-to-12-byte keys, 18616785 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   5 - 0.014%

Keyset 'TwoBytes' - up-to-16-byte keys, 44251425 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  32 - 0.008%

Keyset 'TwoBytes' - up-to-20-byte keys, 86536545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  32 - 0.003%


[[[ Keyset 'Sparse' Tests ]]]

Keyset 'Sparse' - 32-bit keys with up to 6 bits set - 1149017 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  57 - 0.112%

Keyset 'Sparse' - 40-bit keys with up to 6 bits set - 4598479 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  42 - 0.048%

Keyset 'Sparse' - 48-bit keys with up to 5 bits set - 1925357 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  18 - 0.104%

Keyset 'Sparse' - 56-bit keys with up to 5 bits set - 4216423 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  45 - 0.045%

Keyset 'Sparse' - 64-bit keys with up to 5 bits set - 8303633 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   3 - 0.041%

Keyset 'Sparse' - 96-bit keys with up to 4 bits set - 3469497 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  54 - 0.069%

Keyset 'Sparse' - 256-bit keys with up to 3 bits set - 2796417 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  56 - 0.131%

Keyset 'Sparse' - 2048-bit keys with up to 2 bits set - 2098177 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  34 - 0.067%


[[[ Keyset 'Combination Lowbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  31 - 0.012%


[[[ Keyset 'Combination Highbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  12 - 0.017%


[[[ Keyset 'Combination 0x8000000' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  23 - 0.102%


[[[ Keyset 'Combination 0x0000001' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  54 - 0.092%


[[[ Keyset 'Combination Hi-Lo' Tests ]]]

Keyset 'Combination' - up to 6 blocks from a set of 15 - 12204240 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   2 - 0.040%


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
Testing distribution - Worst bias is the  20-bit window at bit  62 - 0.020%

Keyset 'Text' - keys of form "FooBar[XXXX]" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  15 - 0.024%

Keyset 'Text' - keys of form "[XXXX]FooBar" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  59 - 0.022%


[[[ Keyset 'Zeroes' Tests ]]]

Keyset 'Zeroes' - 65536 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  13-bit window at bit  40 - 0.442%


[[[ Keyset 'Seed' Tests ]]]

Keyset 'Seed' - 1000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  53 - 0.162%



Input vcode 0x00000001, Output vcode 0x00000001, Result vcode 0x00000001
Verification value is 0x00000001 - Testing took 684.013358 seconds
-------------------------------------------------------------------------------
```
