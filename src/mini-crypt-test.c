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
