#include "mini-crypt.h"

#include <stdio.h>
#include <string.h>

/*******************************************************************************
SHA-1 testing
*/
void test_sha1(void) {
  struct {
    char* str;
    char hash[21];
  } sets[] = {
      {"The quick brown fox jumps over the lazy dog",
       {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e, 0xe1,
        0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12,
        0}},  // a set with a string
      {"",
       {
           0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf,
           0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09,
       }},         // an empty set
      {NULL, {0}}  // Stop
  };
  int i = 0;
  sha1_s sha1;
  fprintf(stderr, "+ MiniCrypt");
  while (sets[i].str) {
    MiniCrypt.sha1_init(&sha1);
    MiniCrypt.sha1_write(&sha1, sets[i].str, strlen(sets[i].str));
    if (strcmp(MiniCrypt.sha1_result(&sha1), sets[i].hash)) {
      fprintf(stderr,
              ":\n--- MiniCrypt SHA-1 Test FAILED!\nstring: %s\nexpected: ",
              sets[i].str);
      char* p = sets[i].hash;
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\ngot: ");
      p = MiniCrypt.sha1_result(&sha1);
      while (*p)
        fprintf(stderr, "%02x", *(p++) & 0xFF);
      fprintf(stderr, "\n");
      return;
    }
    i++;
  }
  fprintf(stderr, " SHA-1 passed.\n");
}

/*******************************************************************************
SHA-2 TODO: testing is just a stub for noew (also, SHA-2 isn't implemented)
*/

static char* sha2_variant_names[] = {
    "SHA_512", "SHA_512_256", "SHA_512_224", "SHA_384", "SHA_256", "SHA_224",
};

void test_sha2(void) {
  sha2_s s;
  char* expect = NULL;
  char* got = NULL;
  char* str = "";
  fprintf(stderr, "+ MiniCrypt");
  // start tests
  MiniCrypt.sha2_init(&s, SHA_224);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9\x47\x61\x02\xbb\x28\x82\x34\xc4"
      "\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a\xc5\xb3\xe4\x2f";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_256);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27"
      "\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_384);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\x38\xb0\x60\xa7\x51\xac\x96\x38\x4c\xd9\x32\x7e"
      "\xb1\xb1\xe3\x6a\x21\xfd\xb7\x11\x14\xbe\x07\x43\x4c\x0c"
      "\xc7\xbf\x63\xf6\xe1\xda\x27\x4e\xde\xbf\xe7\x6f\x65\xfb"
      "\xd5\x1a\xd2\xf1\x48\x98\xb9\x5b";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_512);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\xcf\x83\xe1\x35\x7e\xef\xb8\xbd\xf1\x54\x28\x50\xd6\x6d"
      "\x80\x07\xd6\x20\xe4\x05\x0b\x57\x15\xdc\x83\xf4\xa9\x21"
      "\xd3\x6c\xe9\xce\x47\xd0\xd1\x3c\x5d\x85\xf2\xb0\xff\x83"
      "\x18\xd2\x87\x7e\xec\x2f\x63\xb9\x31\xbd\x47\x41\x7a\x81"
      "\xa5\x38\x32\x7a\xf9\x27\xda\x3e";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_512_224);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\x6e\xd0\xdd\x02\x80\x6f\xa8\x9e\x25\xde\x06\x0c\x19\xd3"
      "\xac\x86\xca\xbb\x87\xd6\xa0\xdd\xd0\x5c\x33\x3b\x84\xf4";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_512_256);
  MiniCrypt.sha2_write(&s, str, 0);
  expect =
      "\xc6\x72\xb8\xd1\xef\x56\xed\x28\xab\x87\xc3\x62\x2c\x51\x14\x06"
      "\x9b\xdd\x3a\xd7\xb8\xf9\x73\x74\x98\xd0\xc0\x1e\xce\xf0\x96\x7a";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_224);
  str = "The quick brown fox jumps over the lazy dog";
  MiniCrypt.sha2_write(&s, str, strlen(str));
  expect =
      "\x73\x0e\x10\x9b\xd7\xa8\xa3\x2b\x1c\xb9\xd9\xa0\x9a\xa2"
      "\x32\x5d\x24\x30\x58\x7d\xdb\xc0\xc3\x8b\xad\x91\x15\x25";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  MiniCrypt.sha2_init(&s, SHA_512);
  str = "god is a rotten tomato";
  MiniCrypt.sha2_write(&s, str, strlen(str));
  expect =
      "\x61\x97\x4d\x41\x9f\x77\x45\x21\x09\x4e\x95\xa3\xcb\x4d\xe4\x79"
      "\x26\x32\x2f\x2b\xe2\x62\x64\x5a\xb4\x5d\x3f\x73\x69\xef\x46\x20"
      "\xb2\xd3\xce\xda\xa9\xc2\x2c\xac\xe3\xf9\x02\xb2\x20\x5d\x2e\xfd"
      "\x40\xca\xa0\xc1\x67\xe0\xdc\xdf\x60\x04\x3e\x4e\x76\x87\x82\x74";
  got = MiniCrypt.sha2_result(&s);
  if (strcmp(expect, got))
    goto error;

  fprintf(stderr, " SHA-2 passed.\n");
  return;
  goto error;
error:
  fprintf(stderr,
          ":\n--- MiniCrypt SHA-2 Test FAILED!\ntype: "
          "%s\nstring %s\nexpected: ",
          sha2_variant_names[s.type], str);
  while (*expect)
    fprintf(stderr, "%02x", *(expect++) & 0xFF);
  fprintf(stderr, "\ngot: ");
  while (*got)
    fprintf(stderr, "%02x", *(got++) & 0xFF);
  fprintf(stderr, "\n");
}

/*******************************************************************************
Base64
*/
void test_base64(void) {
  struct {
    char* str;
    char* base64;
  } sets[] = {
      // {"Man is distinguished, not only by his reason, but by this singular "
      //  "passion from other animals, which is a lust of the mind, that by a "
      //  "perseverance of delight in the continued and indefatigable generation
      //  "
      //  "of knowledge, exceeds the short vehemence of any carnal pleasure.",
      //  "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB"
      //  "0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIG"
      //  "x1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpb"
      //  "iB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xl"
      //  "ZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3V"
      //  "yZS4="},
      {"any carnal pleasure.", "YW55IGNhcm5hbCBwbGVhc3VyZS4="},
      {"any carnal pleasure", "YW55IGNhcm5hbCBwbGVhc3VyZQ=="},
      {"any carnal pleasur", "YW55IGNhcm5hbCBwbGVhc3Vy"},
      {NULL, NULL}  // Stop
  };
  int i = 0;
  char buffer[1024];
  fprintf(stderr, "+ MiniCrypt");
  while (sets[i].str) {
    MiniCrypt.base64_encode(buffer, sets[i].str, strlen(sets[i].str));
    if (strcmp(buffer, sets[i].base64)) {
      fprintf(stderr,
              ":\n--- MiniCrypt Base64 Test FAILED!\nstring: %s\nlength: %lu\n "
              "expected: %s\ngot: %s\n\n",
              sets[i].str, strlen(sets[i].str), sets[i].base64, buffer);
      break;
    }
    i++;
  }
  if (!sets[i].str)
    fprintf(stderr, " Base64 encode passed.\n");

  i = 0;
  fprintf(stderr, "+ MiniCrypt");
  while (sets[i].str) {
    MiniCrypt.base64_decode(buffer, sets[i].base64, strlen(sets[i].base64));
    if (strcmp(buffer, sets[i].str)) {
      fprintf(stderr,
              ":\n--- MiniCrypt Base64 Test FAILED!\nbase64: %s\nexpected: "
              "%s\ngot: %s\n\n",
              sets[i].base64, sets[i].str, buffer);
      return;
    }
    i++;
  }
  fprintf(stderr, " Base64 decode passed.\n");
}

/*******************************************************************************
Benchmark Vs. OpenSSL (requires OpenSSL) OpenSSL can get significantly faster
(~x2 to ~x3 for SHA-1, ~x2 SHA-2)
*/
// #include "openssl/sha.h"
// #include <time.h>
//
// void benchmark_vs_openssl() {
//   fprintf(stderr, "===================================\n");
//   fprintf(stderr, "MiniCrypt SHA-1 struct size: %lu\n", sizeof(sha1_s));
//   fprintf(stderr, "MiniCrypt SHA-2 struct size: %lu\n", sizeof(sha2_s));
//   fprintf(stderr, "OpenSSL SHA-1 struct size: %lu\n", sizeof(SHA_CTX));
//   fprintf(stderr, "OpenSSL SHA-2/256 struct size: %lu\n",
//   sizeof(SHA256_CTX));
//   fprintf(stderr, "OpenSSL SHA-2/512 struct size: %lu\n",
//   sizeof(SHA512_CTX));
//   fprintf(stderr, "===================================\n");
//
//   sha1_s sha1;
//   sha2_s s;
//   SHA512_CTX s2;
//   SHA256_CTX s3;
//   unsigned char hash[SHA512_DIGEST_LENGTH + 1];
//   hash[SHA512_DIGEST_LENGTH] = 0;
//   clock_t start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     MiniCrypt.sha2_init(&s, SHA_512);
//     MiniCrypt.sha2_write(&s, "The quick brown fox jumps over the lazy dog",
//     43);
//     MiniCrypt.sha2_result(&s);
//   }
//   fprintf(stderr, "MiniCrypt 100K SHA-2/512: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
//
//   start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     SHA512_Init(&s2);
//     SHA512_Update(&s2, "The quick brown fox jumps over the lazy dog", 43);
//     SHA512_Final(hash, &s2);
//   }
//   fprintf(stderr, "OpenSSL 100K SHA-2/512: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
//
//   start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     MiniCrypt.sha2_init(&s, SHA_256);
//     MiniCrypt.sha2_write(&s, "The quick brown fox jumps over the lazy dog",
//     43);
//     MiniCrypt.sha2_result(&s);
//   }
//   fprintf(stderr, "MiniCrypt 100K SHA-2/256: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
//
//   hash[SHA256_DIGEST_LENGTH] = 0;
//   start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     SHA256_Init(&s3);
//     SHA256_Update(&s3, "The quick brown fox jumps over the lazy dog", 43);
//     SHA256_Final(hash, &s3);
//   }
//   fprintf(stderr, "OpenSSL 100K SHA-2/256: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
//
//   start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     MiniCrypt.sha1_init(&sha1);
//     MiniCrypt.sha1_write(&sha1, "The quick brown fox jumps over the lazy
//     dog",
//                          43);
//     MiniCrypt.sha1_result(&sha1);
//   }
//   fprintf(stderr, "MiniCrypt 100K SHA-1: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
//
//   hash[SHA_DIGEST_LENGTH] = 0;
//   SHA_CTX o_sh1;
//   start = clock();
//   for (size_t i = 0; i < 100000; i++) {
//     SHA1_Init(&o_sh1);
//     SHA1_Update(&o_sh1, "The quick brown fox jumps over the lazy dog", 43);
//     SHA1_Final(hash, &o_sh1);
//   }
//   fprintf(stderr, "OpenSSL 100K SHA-1: %lf\n",
//           (double)(clock() - start) / CLOCKS_PER_SEC);
// }
/*******************************************************************************
run all tests
*/

void test_minicrypt(void) {
  test_sha1();
  test_sha2();
  test_base64();
}
