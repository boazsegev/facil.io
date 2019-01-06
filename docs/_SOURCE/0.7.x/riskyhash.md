---
title: facil.io - Risky Hash
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

Risky Hash is a keyed hashing function which uses a simplified variation on xxHash's core diffusion approach.

It provides a fast alternative to SipHash when hashing safe data and a streaming variation can be easily implemented.

Risky Hash wasn't tested for attack resistance and shouldn't be used with any data that might be malicious, since it's unknown if this could result in [hash flooding attacks](http://emboss.github.io/blog/2012/12/14/breaking-murmur-hash-flooding-dos-reloaded/) (see [here](https://medium.freecodecamp.org/hash-table-attack-8e4371fc5261)).

Risky Hash was tested with `SMHasher` ([see results](#SMHasher_results)) (passed).

## Purpose

Risky Hash is designed for fast Hash Map key calculation for both big and small keys. It attempts to act as a 64bit PRF.

My hope is for Risky Hash to be secure enough to allow external (unsafe) data to be used. However, this requires time, exposure and analysis by actual cryptographers.

Currently facio.io's default hashing function is (the slower) SipHash1-3 hash function, which was reviewed by the cryptographic community and found to be resistant to multi-collision / hash flooding attacks.

It's possible to compile facil.io with Risk Hash as the default hashing function by defining the `FIO_USE_RISKY_HASH` during compilation (`-DFIO_USE_RISKY_HASH`). This should be limited to testing and safe environments.

## Overview

Risky Hash has four stages:

* Initialization stage.

* Reading stage.

* Mixing stage.

* Avalanche stage.


### Overview: Initialization

In the initialization stage, Risky Hash attempts to achieves three goals:

* Initialize the hash state using a "secret" (key / salt / seed) in a way that will prevent the secret from being exposed by the resulting hash.

* Initialize the hash state with minimal bias (bits have a fairly even chance at being set or unset).

* Initialize the state in a way that can't be reversed/controlled by a maliciously crafted message.

Since the hash data initialization effects every future calculation, this completes the requirement for the "secret" to effect the hash result in a meaningful manner.

This also attempts to keep the "secret" as secret as possible and should help in protecting Risky Hash against first order preimage attacks.

### Overview: Consumption (reading)

In the consumption stage, Risky Hash attempts to achieves three goals:

* Be fast.

    Memory access and disk access are expensive operations. For this reason, they are performed only once.

* Allow parallelism.

   This is achieved by using a number of distinct and separated reading "vectors".

* Repeated data should produce different results.

   This is achieved by mixing rotation and multiplication operations.

It should be noted that Risky Hash consumes data in 64bit chunks/blocks.

Any data that doesn't fit in a 64bit block is padded with zeros and consumed by a specific consumption vector (rather than consumed in order). 

### Overview: Mixing

In the consumption stage, Risky Hash attempts to achieves three goals:

* Be irreversible.

   This stage is the "last line of defense" against malicious data. For this reason, it should be infeasible to extract meaningful data from the final result.

* Produce a message digest with minimal bias (bits have a fairly even chance at being set or unset).

* Allow all consumption vectors and equal but different effect on the final hash value.


## Specifics

A non-streaming implementation can be found at the `fio.h` header, in the static function: `fio_risky_hash`.

Risky Hash uses 4 reading vectors, each containing 64bits and requires a 64bit "secret".

Risky Hash uses the following prime numbers, that pay homage to the xxHash algorithm that inspired it:

* P0 = `0xC2B2AE3D27D4EB4F`

* P1 = `0x9E3779B185EBCA87`

* P2 = `0x165667B19E3779F9`

* P3 = `0x85EBCA77C2B2AE63`

* P4 = `0x27D4EB2F165667C5`

The following operations are used:

* `~` marks a bit inversion.
* `+` marks a mod 2^64 addition..
* `XOR` marks an XOR operation.
* `MUL(x,y)` a mod 2^64 multiplication.
* `LROT(x,bits)` is a left rotation.
* `<<` is a left shift (not rotate).
* `>>` is a right shift (not rotate).

### Initialization

The four consumption vectors are initialized using the seed ("secret") like so:

```txt
V1 = seed + P0 + P1
V2 = (~seed) + P0
V3 = (seed << 9) XOR P3
V4 = (seed >> 17) XOR P2
```

By loosing bits in the shift operations, it should be impossible for a maliciously crafted message to retrieve them is a way that well cancel out the initialization stage in any way.

### Consumption

Each vector reads a single 64bit word within a 256bit block, allowing the vectors to be parallelized though any message length of 256bits or longer.

The 64bits are read in network byte order (Big-Endian) and treated as a numerical value.

Each vector performs the following operations in each of it's consumption rounds:

```txt
V = V + MUL(P0,input_word)
V = LROT(V,33)
V = MUL(P1,V)
```

If the data fits evenly in 64bit words, than it will be read with no padding, even if some vectors perform more consumption rounds than others.

If the last 64 bit word is incomplete, it will be padded with zeros (0) and consumed by the last vector (`V4`), regardless of it's position within a 256bit block.

### Hash Mixing

At this point the length of the data is finalized an can be added to the calculation.

The following intermediate 64bit word is calculated:

```txt
word = LROT(V1,46) + LROT(V2,52) + LROT(V3,57) + LROT(V4,63)
```

The consumed (unpadded) message length is multiplied by P4 and added to this word:

```txt
word = word + MUL(P4,length)
```

The vectors are mixed in with the word using prime number multiplication to minimize any bias:

```txt
word = word XOR V0
word = MUL(word,P3) + P2
word = word XOR V1
word = MUL(word,P3) + P2
word = word XOR V2
word = MUL(word,P3) + P2
word = word XOR V3
word = MUL(word,P3) + P2
```

### Hash Avalanche

The 64bit word resulting form the mixing stage is mixed with an altered version of itself in a way that makes it hard to reconstruct the original word and improves bit dispersion:

The following operations are performed:

```txt
word = word XOR (word >> 33)
word = MUL(word,P1)
word = word XOR (word >> 29)
word = MUL(word,P2)
```

## Attacks, Reports and Security

No known attacks exist at this time. However, the xxHash, after which Risky Hash was modeled, did (and maybe still does) suffer from [a known weakness](https://github.com/Cyan4973/xxHash/issues/54) I have tried to address.

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
  /* inspired by xxHash: Yann Collet, Maciej Adamczyk... */
  const uint64_t primes[] = {
      /* xxHash Primes */
      14029467366897019727ULL, 11400714785074694791ULL, 1609587929392839161ULL,
      9650029242287828579ULL,  2870177450012600261ULL,
  };
  /*
   * 4 x 64 bit vectors for 256bit block consumption.
   * When implementing a streaming variation, more fields might be required.
   */
  struct risky_state_s {
    uint64_t v[4];
  } s = {{
       (seed + primes[0] + primes[1]),
       ((~seed) + primes[0]),
       ((seed << 9) ^ primes[3]),
       ((seed >> 17) ^ primes[2]),
   }};

/* A single data-consuming round, wrd is the data in big-endian 64 bit */
/* the design follows the xxHash basic round scheme and is easy to vectorize */
#define fio_risky_round_single(wrd, i)                                          \
  s.v[(i)] += (wrd)*primes[0];                                                  \
  s.v[(i)] = fio_lrot64(s.v[(i)], 33);                                         \
  s.v[(i)] *= primes[1];

/* an unrolled (vectorizable) 256bit round */
#define fio_risky_round_256(w0, w1, w2, w3)                                    \
  fio_risky_round_single(w0, 0);                                               \
  fio_risky_round_single(w1, 1);                                               \
  fio_risky_round_single(w2, 2);                                               \
  fio_risky_round_single(w3, 3);

  uint8_t *data = (uint8_t *)data_;

  /* loop over 256 bit "blocks" */
  const size_t len_256 = len & (((size_t)-1) << 5);
  for (size_t i = 0; i < len_256; i += 32) {
    /* perform round for block */
    fio_risky_round_256(fio_str2u64(data), fio_str2u64(data + 8),
                        fio_str2u64(data + 16), fio_str2u64(data + 24));
    data += 32;
  }

  /* process last 64bit words in each vector */
  switch (len & 24UL) {
  case 24:
    fio_risky_round_single(fio_str2u64(data), 0);
    fio_risky_round_single(fio_str2u64(data + 8), 1);
    fio_risky_round_single(fio_str2u64(data + 16), 2);
    data += 24;
    break;
  case 16:
    fio_risky_round_single(fio_str2u64(data), 0);
    fio_risky_round_single(fio_str2u64(data + 8), 1);
    data += 16;
    break;
  case 8:
    fio_risky_round_single(fio_str2u64(data), 0);
    data += 8;
    break;
  }

  /* always process the last 64bits, if any, in the 4th vector */
  uint64_t last_bytes = 0;
  switch (len & 7) {
  case 7:
    last_bytes |= ((uint64_t)data[6] & 0xFF) << 56;
  case 6: /* overflow */
    last_bytes |= ((uint64_t)data[5] & 0xFF) << 48;
  case 5: /* overflow */
    last_bytes |= ((uint64_t)data[4] & 0xFF) << 40;
  case 4: /* overflow */
    last_bytes |= ((uint64_t)data[3] & 0xFF) << 32;
  case 3: /* overflow */
    last_bytes |= ((uint64_t)data[2] & 0xFF) << 24;
  case 2: /* overflow */
    last_bytes |= ((uint64_t)data[1] & 0xFF) << 16;
  case 1: /* overflow */
    last_bytes |= ((uint64_t)data[0] & 0xFF) << 8;
    fio_risky_round_single(last_bytes, 3);
  }

  /* mix stage */
  uint64_t result = (fio_lrot64(s.v[3], 63) + fio_lrot64(s.v[2], 57) +
                     fio_lrot64(s.v[1], 52) + fio_lrot64(s.v[0], 46));
  result += len * primes[4];
  result = ((result ^ s.v[0]) * primes[3]) + primes[2];
  result = ((result ^ s.v[1]) * primes[3]) + primes[2];
  result = ((result ^ s.v[2]) * primes[3]) + primes[2];
  result = ((result ^ s.v[3]) * primes[3]) + primes[2];
  /* avalanche */
  result ^= (result >> 33);
  result *= primes[1];
  result ^= (result >> 29);
  result *= primes[2];
  return result;

#undef fio_risky_round_single
#undef fio_risky_round_256
}

```

## SMHasher results

The following results were produced on a 2.9 GHz Intel Core i9 machine and won't be updated every time.

```txt
-------------------------------------------------------------------------------
--- Testing RiskyHash "facil.io hashing (by Bo)"

[[[ Sanity Tests ]]]

Verification value 0x97CB44DF : PASS
Running sanity check 1    ..........PASS
Running AppendedZeroesTest..........PASS

[[[ Speed Tests ]]]

Bulk speed test - 262144-byte keys
Alignment  7 -  5.297 bytes/cycle - 15155.38 MiB/sec @ 3 ghz
Alignment  6 -  5.899 bytes/cycle - 16876.74 MiB/sec @ 3 ghz
Alignment  5 -  5.828 bytes/cycle - 16675.31 MiB/sec @ 3 ghz
Alignment  4 -  5.926 bytes/cycle - 16953.47 MiB/sec @ 3 ghz
Alignment  3 -  5.928 bytes/cycle - 16960.74 MiB/sec @ 3 ghz
Alignment  2 -  5.979 bytes/cycle - 17107.00 MiB/sec @ 3 ghz
Alignment  1 -  5.940 bytes/cycle - 16995.46 MiB/sec @ 3 ghz
Alignment  0 -  5.946 bytes/cycle - 17010.75 MiB/sec @ 3 ghz
Average      -  5.843 bytes/cycle - 16716.86 MiB/sec @ 3 ghz

Small key speed test -    1-byte keys -    36.44 cycles/hash
Small key speed test -    2-byte keys -    36.78 cycles/hash
Small key speed test -    3-byte keys -    34.85 cycles/hash
Small key speed test -    4-byte keys -    35.90 cycles/hash
Small key speed test -    5-byte keys -    36.87 cycles/hash
Small key speed test -    6-byte keys -    36.00 cycles/hash
Small key speed test -    7-byte keys -    35.99 cycles/hash
Small key speed test -    8-byte keys -    40.89 cycles/hash
Small key speed test -    9-byte keys -    40.70 cycles/hash
Small key speed test -   10-byte keys -    40.97 cycles/hash
Small key speed test -   11-byte keys -    41.36 cycles/hash
Small key speed test -   12-byte keys -    41.47 cycles/hash
Small key speed test -   13-byte keys -    41.44 cycles/hash
Small key speed test -   14-byte keys -    41.65 cycles/hash
Small key speed test -   15-byte keys -    40.60 cycles/hash
Small key speed test -   16-byte keys -    40.96 cycles/hash
Small key speed test -   17-byte keys -    41.64 cycles/hash
Small key speed test -   18-byte keys -    40.73 cycles/hash
Small key speed test -   19-byte keys -    40.94 cycles/hash
Small key speed test -   20-byte keys -    40.93 cycles/hash
Small key speed test -   21-byte keys -    41.13 cycles/hash
Small key speed test -   22-byte keys -    41.09 cycles/hash
Small key speed test -   23-byte keys -    41.64 cycles/hash
Small key speed test -   24-byte keys -    42.04 cycles/hash
Small key speed test -   25-byte keys -    42.81 cycles/hash
Small key speed test -   26-byte keys -    49.84 cycles/hash
Small key speed test -   27-byte keys -    45.56 cycles/hash
Small key speed test -   28-byte keys -    41.65 cycles/hash
Small key speed test -   29-byte keys -    41.91 cycles/hash
Small key speed test -   30-byte keys -    40.98 cycles/hash
Small key speed test -   31-byte keys -    41.92 cycles/hash
Average                                    40.570 cycles/hash

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

Testing  32-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.596667%
Testing  40-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.752000%
Testing  48-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.594667%
Testing  56-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.598667%
Testing  64-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.680667%
Testing  72-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.754000%
Testing  80-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.702667%
Testing  88-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.656000%
Testing  96-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.676000%
Testing 104-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.641333%
Testing 112-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.751333%
Testing 120-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.672667%
Testing 128-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.658000%
Testing 136-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.713333%
Testing 144-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.690000%
Testing 152-bit keys ->  64-bit hashes,   300000 reps.......... worst bias is 0.748667%

[[[ Keyset 'Cyclic' Tests ]]]

Keyset 'Cyclic' - 8 cycles of 8 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  34 - 0.031%

Keyset 'Cyclic' - 8 cycles of 9 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  30 - 0.029%

Keyset 'Cyclic' - 8 cycles of 10 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  38 - 0.025%

Keyset 'Cyclic' - 8 cycles of 11 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  51 - 0.038%

Keyset 'Cyclic' - 8 cycles of 12 bytes - 10000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  43 - 0.022%


[[[ Keyset 'TwoBytes' Tests ]]]

Keyset 'TwoBytes' - up-to-4-byte keys, 652545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  16-bit window at bit  44 - 0.140%

Keyset 'TwoBytes' - up-to-8-byte keys, 5471025 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  41 - 0.071%

Keyset 'TwoBytes' - up-to-12-byte keys, 18616785 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  42 - 0.026%

Keyset 'TwoBytes' - up-to-16-byte keys, 44251425 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  59 - 0.007%

Keyset 'TwoBytes' - up-to-20-byte keys, 86536545 total keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  60 - 0.003%


[[[ Keyset 'Sparse' Tests ]]]

Keyset 'Sparse' - 32-bit keys with up to 6 bits set - 1149017 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  42 - 0.092%

Keyset 'Sparse' - 40-bit keys with up to 6 bits set - 4598479 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  56 - 0.038%

Keyset 'Sparse' - 48-bit keys with up to 5 bits set - 1925357 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  48 - 0.070%

Keyset 'Sparse' - 56-bit keys with up to 5 bits set - 4216423 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  57 - 0.030%

Keyset 'Sparse' - 64-bit keys with up to 5 bits set - 8303633 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  60 - 0.034%

Keyset 'Sparse' - 96-bit keys with up to 4 bits set - 3469497 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  42 - 0.062%

Keyset 'Sparse' - 256-bit keys with up to 3 bits set - 2796417 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  19-bit window at bit  38 - 0.072%

Keyset 'Sparse' - 2048-bit keys with up to 2 bits set - 2098177 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  14 - 0.082%


[[[ Keyset 'Combination Lowbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit   2 - 0.013%


[[[ Keyset 'Combination Highbits' Tests ]]]

Keyset 'Combination' - up to 8 blocks from a set of 8 - 19173960 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  26 - 0.019%


[[[ Keyset 'Combination 0x8000000' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  21 - 0.069%


[[[ Keyset 'Combination 0x0000001' Tests ]]]

Keyset 'Combination' - up to 20 blocks from a set of 2 - 2097150 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  18-bit window at bit  37 - 0.066%


[[[ Keyset 'Combination Hi-Lo' Tests ]]]

Keyset 'Combination' - up to 6 blocks from a set of 15 - 12204240 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  17 - 0.022%


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
Testing distribution - Worst bias is the  20-bit window at bit  18 - 0.016%

Keyset 'Text' - keys of form "FooBar[XXXX]" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  54 - 0.020%

Keyset 'Text' - keys of form "[XXXX]FooBar" - 14776336 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  20-bit window at bit  12 - 0.021%


[[[ Keyset 'Zeroes' Tests ]]]

Keyset 'Zeroes' - 65536 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  13-bit window at bit  10 - 0.577%


[[[ Keyset 'Seed' Tests ]]]

Keyset 'Seed' - 1000000 keys
Testing collisions   - Expected     0.00, actual     0.00 ( 0.00x)
Testing distribution - Worst bias is the  17-bit window at bit  45 - 0.107%



Input vcode 0x00000001, Output vcode 0x00000001, Result vcode 0x00000001
Verification value is 0x00000001 - Testing took 790.781358 seconds
-------------------------------------------------------------------------------
```
