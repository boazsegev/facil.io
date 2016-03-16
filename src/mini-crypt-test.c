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

void test_sha2(void) {
  // SHA224("")
  // 0x d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f
  // SHA256("")
  // 0x e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  // SHA384("")
  // 0x
  // 38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b
  // SHA512("")
  // 0x
  // cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e
  // SHA512/224("")
  // 0x 6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4
  // SHA512/256("")
  // 0x c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a
  // SHA224("The quick brown fox jumps over the lazy dog")
  // 0x 730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525
  // SHA224("The quick brown fox jumps over the lazy dog.")
  // 0x 619cba8e8e05826e9b8c519c0a5c68f4fb653e8a3d8aa04bb2c8cd4c
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
run all tests
*/

void test_minicrypt(void) {
  test_sha1();
  test_base64();
}
