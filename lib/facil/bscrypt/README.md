# BSCrypt (Basic Crypt)

This is a **basic** crypto library in C.

At the moment, we have:

* SHA-1.
* SHA-2.
* SipHash2-4 (unstreamed).
* Unix crypto-level pseudo-random.
* Base64 encode/decode (not crypto but useful).
* Hex encode/decode (not crypto but useful).
* XOR masking (not crypto but useful)... I'll write this again, **not** crypto - don't use XOR for encrypting stuff!

Future plans (optional, feel free to open a PR with any of these features):

* SHA-3.
* AES GCM / CCM modes encryption/decryption.
* RSA / DH / DHEC.
* TLS 1.2 / 1.3 (I'll probably never get this far, but it would be fun to have a library with a simple API for this).


The library is modular, mostly. Meaning you can copy the [`bscrypt-common.h`](./src/bscrypt/bscrypt-common.h) and any specific feature you want (i.e. take just the SHA-1 by copying the `sha1.h` and `sha1.c` files).

## Sample use

The code is heavily documented, but it does assume you know what you're doing (hardly any overflow / NULL pointer protection).

Some things, like Base64/Hex encoding assume you allocated enough space (the functions are unsafe) and overlapping memory will cause undefined behavior (although decoding will mostly work when performed in place).

The purpose of this lower level API was to allow higher level APIs to be written, so use the provided API with care.

Here's a simple SHA-1 computation:

```c
#include "bscrypt.h"
#include <assert.h>

int main(int argv, const char * argv[]) {
  char * result;
  sha1_s sha1 = bscrypt_sha1_init();
  bscrypt_sha1_write(&sha1, "The quick brown fox jumps over the lazy dog ", 43);
  result = bscrypt_sha1_result(&sha1);
  assert(result == (cahr *)sha1.digest.str); /* locally stored on the stack */
  /* another option is to use a single call with the allocated sha1 variable */
  result = bscrypt_sha1(&sha1,
                        "The quick brown fox jumps over the lazy dog ",
                        43);
  /**/
}
```

## Requirements

The C code is mostly portable (at least, I hope it is) and should work on most (or all) systems.

Some features (i.e. the Pseudo-Random generator) will only work properly on Unix systems... although I did write a fallback alternative for the Pseudo-Random generator, it isn't idle.

I didn't check the code on any system except my Mac OS X and Linux, so you might have to rewrite some include directives (I didn't bother checking the build on anything other then Linux and OS X).

The code wasn't really optimized that much, but what optimizations I did make, I oriented towards 64bit architectures. The SHA-1 and SHA-2 seemed fairly fast on my system (give or take on par with OpenSSL) but slightly slower on the Linux machine (where OpenSSL was noticeably faster)... but it might be because my older hardware wasn't running hardware optimizations in use by OpenSSL.

## Randomness

The pseudo random generator follows the Unix/Linux recommendation of opening the `/dev/urandom` pseudo random stream and reading from it...

...This is slower but better... It also only works on Unix machines.

If you need a faster solution (albite an untested one that probably shouldn't be used for cryptography), compile the code with the `USE_ALT_RANDOM` macro defined (or compile it on a non unix machine). This pseudo random generator will use the CPU clock, the current time and whatever's in top 64 bytes of the stack's memory to seed an SHA-2 hash which is used as a random oracle.

It's about twice as fast, but the (mostly) predictable seed data and the fact that CPU time and stack memory are probably similar on program restart make this less then random.

## Why did I write this?

I don't know... At first it started because I just needed SHA-1 and Base64 encoding for the Websocket handshake code I was writing... and I just hate using large precompiled libraries with complicated APIs for small tasks.

Afterwards, I was thinking that if this project will slowly grow to provide a high level API for TLS connections (without support for weak encryptions), I would finally have a library I can use with a simple "copy and paste" of the code...

...I believe a TLS library that takes away from the developer's control is a good thing (most of us shouldn't bother learning cryptography just to open a secure connection using TLS).

Yes, I know that linking existing libraries makes everything easier (especially when it comes to maintenance and security updates)... but it seems all the existing libraries have weird issues and require so much study I could probably write a new library for what I need (call me paranoid).

Besides, writing code relaxes me. It's good practice.
