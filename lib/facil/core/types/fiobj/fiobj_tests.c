#ifdef DEBUG
/*
Copyright: Boaz Segev, 2017
License: MIT
*/

#include "fio2resp.h"
#include "fiobj_internal.h"
#include "fiobj_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* *****************************************************************************
Randomness
***************************************************************************** */

static uint64_t fiobj_rand64(void) {
  static uint64_t seed = (uint64_t)((uintptr_t)(&fiobj_rand64));
  static struct {
    uint64_t seed;
    struct timeval time;
  } data;
  data.seed = seed;
  gettimeofday(&data.time, NULL);
  data.seed = fiobj_sym_hash(&data, sizeof(data));
  seed = data.seed;
  return data.seed;
}
/* *****************************************************************************
A JSON testing
***************************************************************************** */

int fiobj_test_json_str(char const *json, size_t len, uint8_t print_result) {
  clock_t start, end;

  fiobj_s *result = NULL;
  start = clock();
  size_t i = fiobj_json2obj(&result, json, len);
  end = clock();
  if (!i || !result) {
    fprintf(stderr, "FAILED to parse JSON?! consumed %lu and result == %p\n", i,
            (void *)result);
    return -1;
  }
  fprintf(stderr, "* Parsed JSON in %lu\n", end - start);
  start = clock();
  fiobj_s *jstr = fiobj_obj2json(result, 1);
  end = clock();
  fprintf(stderr, "* Formatted JSON in %lu\n", end - start);
  fprintf(
      stderr, "Consumed %lu bytes out of %lu with result length %llu:\n%s\n", i,
      len, fiobj_obj2cstr(jstr).length,
      (print_result) ? fiobj_obj2cstr(jstr).data : "\t\tfiles aren't printed.");

  start = clock();
  fiobj_free(result);
  end = clock();
  fprintf(stderr, "* Freed JSON result in %lu\n", end - start);

  start = clock();
  fiobj_free(jstr);
  end = clock();
  fprintf(stderr, "* Freed JSON String in %lu\n", end - start);
  return 0;
}
static void fiobj_test_hash_json(void) {
  fiobj_pt hash = fiobj_hash_new();
  fiobj_pt hash2 = fiobj_hash_new();
  fiobj_pt hash3 = fiobj_hash_new();
  fiobj_pt syms = fiobj_ary_new(); /* freed within Hashes */
  char num_buffer[68] = "0x";
  fiobj_pt tmp, sym;

  sym = fiobj_sym_new("id", 2);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(hash, sym, fiobj_num_new(fiobj_rand64()));

  sym = fiobj_sym_new("number", 6);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(hash, sym, fiobj_num_new(42));

  sym = fiobj_sym_new("float", 5);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(hash, sym, fiobj_float_new(42.42));

  sym = fiobj_sym_new("string", 6);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(
      hash, sym,
      fiobj_strprintf(u8"𝄞\n\ttake \\ my  \\ %s"
                      "\n\ttake \\ my  \\ whole ❤️  %s ❤️  too...\n",
                      "hand", "heart"));
  sym = fiobj_sym_new("hash", 4);
  fiobj_ary_push(syms, sym);
  tmp = fiobj_hash_new();
  fiobj_hash_set(hash, sym, tmp);

  for (int i = 1; i < 6; i++) {
    /* creates a temporary symbol, since we're not retriving the data */
    sym = fiobj_symprintf("%d", i);
    /* make sure the Hash isn't leaking */
    for (size_t j = 0; j < 7; j++) {
      /* set alternating key-value pairs */
      if (i & 1)
        fiobj_hash_set(
            tmp, sym,
            fiobj_str_new(num_buffer, fio_ltoa(num_buffer + 2, i, 16) + 2));
      else
        fiobj_hash_set(tmp, sym, fiobj_num_new(i));
    }
    fiobj_free(sym);
  }

  sym = fiobj_sym_new("symbols", 7);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(hash, sym, syms);

  /* test`fiobj_iseq */
  {
    /* shallow copy in order */
    size_t len = fiobj_ary_count(syms) - 1;
    for (size_t i = 0; i <= len; i++) {
      sym = fiobj_ary_index(syms, i);
      fiobj_hash_set(hash2, sym, fiobj_dup(fiobj_hash_get(hash, sym)));
    }
    /* shallow copy in reverse order */
    for (size_t i = 0; i <= len; i++) {
      sym = fiobj_ary_index(syms, len - i);
      fiobj_hash_set(hash3, sym, fiobj_dup(fiobj_hash_get(hash, sym)));
    }
    fprintf(stderr, "* Testing shallow copy reference count: %s\n",
            (fiobj_reference_count(syms) == 3) ? "passed." : "FAILED!");
    fprintf(stderr,
            "* Testing deep object equality review:\n"
            "  * Eq. Hash (ordered): %s\n"
            "  * Eq. Hash (unordered): %s\n"
            "  * Hash vs. Array: %s\n",
            (fiobj_iseq(hash, hash2)) ? "passed." : "FAILED!",
            (fiobj_iseq(hash, hash3)) ? "passed." : "FAILED!",
            (fiobj_iseq(hash, syms)) ? "FAILED!" : "passed.");
  }
  /* print JSON string and test parser */
  {
    tmp = fiobj_obj2json(hash, 0);
    fprintf(stderr,
            "* Printing JSON (len: %llu real len: %lu capa: %lu ref: %lu):\n  "
            " %s\n",
            fiobj_obj2cstr(tmp).len, strlen(fiobj_obj2cstr(tmp).data),
            fiobj_str_capa(tmp), fiobj_reference_count(tmp),
            fiobj_obj2cstr(tmp).data);
    fiobj_s *parsed = NULL;
    if (fiobj_json2obj(&parsed, fiobj_obj2cstr(tmp).buffer,
                       fiobj_obj2cstr(tmp).len) == 0) {
      fprintf(stderr, "* FAILD to parse the JSON printed.\n");
    } else {
      // if (!fiobj_iseq(parsed, hash)) {
      //   fiobj_free(tmp);
      //   tmp = fiobj_obj2json(parsed);
      //   fprintf(stderr, "* Parsed JSON is NOT EQUAL to original:\n%s\n\n",
      //           fiobj_obj2cstr(tmp).data);
      //   fiobj_free(tmp);
      //   tmp = fiobj_obj2json(fiobj_hash_get2(parsed, "symbols", 7));
      //   fprintf(stderr, "* Just the Symbols array (str eql == %u):\n%s\n\n",
      //           fiobj_iseq(fiobj_hash_get2(parsed, "string", 6),
      //                      fiobj_hash_get2(hash, "string", 6)),
      //           fiobj_obj2cstr(tmp).data);
      //
      // } else {
      //   fprintf(stderr, "* Parsed JSON is equal to original.\n");
      // }
      fiobj_free(parsed);
    }
    fiobj_free(tmp);
  }
#ifdef H_FIO2RESP_FORMAT_H
  /* print RESP string */
  tmp = resp_fioformat(hash);
  fprintf(stderr, "* Printing RESP (len: %llu capa: %lu):\n   %s\n",
          fiobj_obj2cstr(tmp).len, fiobj_str_capa(tmp),
          fiobj_obj2cstr(tmp).data);
  fiobj_free(tmp);
#endif

  sym = fiobj_sym_new("hash", 4);
  tmp = fiobj_sym_new("1", 1);

  // fprintf(stderr,
  //         "* Reference count for "
  //         "couplet in nested Hash: %llu\n",
  //         OBJ2HEAD(obj2hash(hash3)->items.next->obj).ref);

  fiobj_free(hash);
  // fprintf(stderr, "* Testing nested Array delete reference count: %s\n",
  //         (OBJ2HEAD(syms).ref == 2) ? "passed." : "FAILED!");
  // fprintf(stderr, "* Testing nested Hash delete reference count: %s\n",
  //         (OBJ2HEAD(obj2hash(hash3)->items.next->obj).ref == 2) ? "passed."
  //                                                               : "FAILED!");
  // fprintf(stderr,
  //         "* Testing reference count for "
  //         "nested nested object in nessted Hash: %s\n",
  //         (OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref ==
  //         2)
  //             ? "passed."
  //             : "FAILED!");
  // fprintf(stderr,
  //         "* Reference count for "
  //         "nested nested object in nested Hash: %llu\n",
  //         OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref);

  fiobj_free(hash2);
  // fprintf(stderr, "* Testing nested Array delete reference count: %s\n",
  //         (OBJ2HEAD(syms).ref == 1) ? "passed." : "FAILED!");
  // fprintf(stderr, "* Testing nested Hash delete reference count: %s\n",
  //         (OBJ2HEAD(obj2hash(hash3)->items.next->obj).ref == 1) ? "passed."
  //                                                               : "FAILED!");
  // fprintf(stderr,
  //         "* Testing reference count for "
  //         "nested nested object in nessted Hash: %s\n",
  //         (OBJ2HEAD(obj2hash(hash3)->items.next->obj).ref == 1) ? "passed."
  //                                                               : "FAILED!");
  // fprintf(stderr,
  //         "* Reference count for "
  //         "nested nested object in nested Hash: %llu\n",
  //         OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref);

  fiobj_free(hash3);
  fiobj_free(sym);
  fiobj_free(tmp);
}

/* *****************************************************************************
Test Hash performance
***************************************************************************** */

#include <time.h>
#define HASH_TEST_SIZE ((4194304 >> 3) + (4194304 >> 4))
#define HASH_TEST_REPEAT 1

static void fiobj_hash_test(void) {
  clock_t start, end;
  fiobj_s *syms;
  fiobj_s *strings;
  fiobj_s *hash;
  fprintf(stderr, "\nTesting Hash and Array allocations\n");

  start = clock();
  syms = fiobj_ary_new();
  strings = fiobj_ary_new();
  for (size_t i = 0; i < HASH_TEST_SIZE; i++) {
    fiobj_ary_push(syms, fiobj_symprintf("sym %lu", i));
    fiobj_ary_push(strings, fiobj_strprintf("str %lu", i));
  }
  end = clock();
  fprintf(stderr,
          "* Created 2 arrays with %d symbols and strings "
          "using printf in %lu.%lus\n",
          HASH_TEST_SIZE << 1, (end - start) / CLOCKS_PER_SEC,
          (end - start) - ((end - start) / CLOCKS_PER_SEC));

  /******* Repeat Testing starts here *******/
  for (size_t i = 0; i < HASH_TEST_REPEAT; i++) {

    start = clock();
    hash = fiobj_hash_new();
    for (size_t i = 0; i < HASH_TEST_SIZE; i++) {
      fiobj_hash_set(hash, fiobj_ary_index(syms, i),
                     fiobj_dup(fiobj_ary_index(strings, i)));
    }
    end = clock();
    fprintf(stderr, "* Set %d items in %lu.%lus\n", HASH_TEST_SIZE,
            (end - start) / CLOCKS_PER_SEC,
            (end - start) - ((end - start) / CLOCKS_PER_SEC));
    fprintf(stderr,
            "   - Final hash-map "
            "length/capacity == %lu/%lu\n",
            fiobj_hash_count(hash), fiobj_hash_capa(hash));
    start = clock();
    for (size_t i = 0; i < HASH_TEST_SIZE; i++) {
      fiobj_hash_set(hash, fiobj_ary_index(syms, i),
                     fiobj_dup(fiobj_ary_index(strings, i)));
    }
    end = clock();
    fprintf(stderr,
            "* Resetting %d (test count == %lu) "
            "items in %lu.%lus\n",
            HASH_TEST_SIZE, fiobj_hash_count(hash),
            (end - start) / CLOCKS_PER_SEC,
            (end - start) - ((end - start) / CLOCKS_PER_SEC));

    start = clock();
    for (size_t i = 0; i < HASH_TEST_SIZE; i++) {
      if (fiobj_hash_get(hash, fiobj_ary_index(syms, i)) !=
          fiobj_ary_index(strings, i))
        fprintf(stderr, "ERROR: fiobj_hash_get FAILED for %s != %s\n",
                fiobj_obj2cstr(fiobj_ary_index(strings, i)).data,
                fiobj_obj2cstr(fiobj_hash_get(hash, fiobj_ary_index(syms, i)))
                    .data),
            exit(-1);
    }
    end = clock();
    fprintf(stderr, "* Seek and test %d items in %lu.%lus\n", HASH_TEST_SIZE,
            (end - start) / CLOCKS_PER_SEC,
            (end - start) - ((end - start) / CLOCKS_PER_SEC));

    start = clock();
    fiobj_free(hash);
    end = clock();
    fprintf(stderr, "* Destroy hash with %d items in %lu.%lus\n",
            HASH_TEST_SIZE, (end - start) / CLOCKS_PER_SEC,
            (end - start) - ((end - start) / CLOCKS_PER_SEC));

    /******* Repeat Testing ends here *******/
  }

  /** cleanup **/

  start = clock();
  fiobj_free(syms);
  fiobj_free(strings);
  end = clock();
  fprintf(stderr,
          "Deallocated 2 arrays with %d symbols and strings "
          "in %lu.%lus\n\n",
          HASH_TEST_SIZE << 1, (end - start) / CLOCKS_PER_SEC,
          (end - start) - ((end - start) / CLOCKS_PER_SEC));
}

/* *****************************************************************************
Basic tests fiobj types
***************************************************************************** */
static char num_buffer[148];

/* test were written for OSX (fprintf types) with clang (%s for NULL is okay)
 */
void fiobj_test(void) {
  /* test hash+array for memory leaks and performance*/
  fiobj_hash_test();
  /* test JSON (I know... it assumes everything else works...) */
  for (int i = 0; i < 1; ++i) {
    fiobj_test_hash_json();
  }
  /* start simple tests */

  fiobj_s *obj;
  size_t i;
  fprintf(stderr, "\n===\nStarting fiobj basic testing:\n");

  obj = fiobj_null();
  if (obj->type != FIOBJ_T_NULL)
    fprintf(stderr, "* FAILED null object test.\n");
  fiobj_free(obj);

  obj = fiobj_false();
  if (obj->type != FIOBJ_T_FALSE)
    fprintf(stderr, "* FAILED false object test.\n");
  fiobj_free(obj);

  obj = fiobj_true();
  if (obj->type != FIOBJ_T_TRUE)
    fprintf(stderr, "* FAILED true object test.\n");
  fiobj_free(obj);

  obj = fiobj_num_new(255);
  if (obj->type != FIOBJ_T_NUMBER || fiobj_obj2num(obj) != 255)
    fprintf(stderr, "* FAILED 255 object test i == %llu with type %p.\n",
            fiobj_obj2num(obj), (void *)obj->type);
  if (strcmp(fiobj_obj2cstr(obj).data, "255"))
    fprintf(stderr, "* FAILED base 10 fiobj_obj2cstr test with %s.\n",
            fiobj_obj2cstr(obj).data);
  i = fio_ltoa(num_buffer, fiobj_obj2num(obj), 16);
  if (strcmp(num_buffer, "00FF"))
    fprintf(stderr, "* FAILED base 16 fiobj_obj2cstr test with (%lu): %s.\n", i,
            num_buffer);
  i = fio_ltoa(num_buffer, fiobj_obj2num(obj), 2);
  if (strcmp(num_buffer, "011111111"))
    fprintf(stderr, "* FAILED base 2 fiobj_obj2cstr test with (%lu): %s.\n", i,
            num_buffer);
  fiobj_free(obj);

  obj = fiobj_float_new(77.777);
  if (obj->type != FIOBJ_T_FLOAT || fiobj_obj2num(obj) != 77 ||
      fiobj_obj2float(obj) != 77.777)
    fprintf(stderr, "* FAILED 77.777 object test.\n");
  if (strcmp(fiobj_obj2cstr(obj).data, "77.777"))
    fprintf(stderr, "* FAILED float2str test with %s.\n",
            fiobj_obj2cstr(obj).data);
  fiobj_free(obj);

  obj = fiobj_str_new("0x7F", 4);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 127)
    fprintf(stderr, "* FAILED 0x7F object test.\n");
  fiobj_free(obj);

  obj = fiobj_str_new("0b01111111", 10);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 127)
    fprintf(stderr, "* FAILED 0b01111111 object test.\n");
  fiobj_free(obj);

  obj = fiobj_str_new("232.79", 6);
  if (obj->type != FIOBJ_T_STRING || fiobj_obj2num(obj) != 232)
    fprintf(stderr, "* FAILED 232 object test. %llu\n", fiobj_obj2num(obj));
  if (fiobj_obj2float(obj) != 232.79)
    fprintf(stderr, "* FAILED fiobj_obj2float test with %f.\n",
            fiobj_obj2float(obj));
  fiobj_free(obj);

  /* test array */
  obj = fiobj_ary_new();
  if (obj->type == FIOBJ_T_ARRAY) {
    fprintf(stderr, "* testing Array. \n");
    for (size_t i = 0; i < 128; i++) {
      fiobj_ary_unshift(obj, fiobj_num_new(i));
      if (fiobj_ary_count(obj) != i + 1)
        fprintf(stderr, "* FAILED Array count. %lu/%lu != %lu\n",
                fiobj_ary_count(obj), fiobj_ary_capa(obj), i + 1);
    }
    fiobj_s *tmp = fiobj_obj2json(obj, 0);
    fprintf(stderr, "Array test printout:\n%s\n",
            tmp ? fiobj_obj2cstr(tmp).data : "ERROR");
    fiobj_free(tmp);
    fiobj_free(obj);
  } else {
    fprintf(stderr, "* FAILED to initialize Array test!\n");
    fiobj_free(obj);
  }

/* test cyclic protection */
#if FIOBJ_NESTING_PROTECTION == 1
  {
    fprintf(stderr, "* testing cyclic protection. \n");
    fiobj_s *a1 = fiobj_ary_new();
    fiobj_s *a2 = fiobj_ary_new();
    for (size_t i = 0; i < 129; i++) {
      obj = fiobj_num_new(1024 + i);
      fiobj_ary_push(a1, fiobj_num_new(i));
      fiobj_ary_unshift(a2, fiobj_num_new(i));
      fiobj_ary_push(a1, fiobj_dup(obj));
      fiobj_ary_unshift(a2, obj);
    }
    fiobj_ary_push(a1, a2); /* the intentionally offending code */
    fiobj_ary_push(a2, a1);
    fprintf(stderr,
            "* Printing cyclic array references with "
            "a1[%lu]  == a2 and a2[%lu] == a1\n",
            fiobj_ary_count(a1), fiobj_ary_count(a2));
    {
      fiobj_s *tmp = fiobj_obj2json(a1, 0);
      fprintf(stderr, "%s\n", tmp ? fiobj_obj2cstr(tmp).data : "ERROR");
      fiobj_free(tmp);
    }

    obj = fiobj_dup(fiobj_ary_index(a2, -3));
    if (!obj || obj->type != FIOBJ_T_NUMBER)
      fprintf(stderr, "* FAILED unexpected object %p with type %p\n",
              (void *)obj, (void *)(obj ? obj->type : 0));
    if (OBJ2HEAD(obj)->ref < 2)
      fprintf(stderr, "* FAILED object reference counting test (%lu)\n",
              OBJ2HEAD(obj)->ref);
    if (OBJ2HEAD(a2)->ref)
      fprintf(stderr, "* testing nested object reference count... (%lu)\n",
              OBJ2HEAD(obj)->ref);
    fiobj_free(a1); /* frees both... */
    // fiobj_free(a2);
    if (OBJ2HEAD(obj)->ref != 1)
      fprintf(stderr,
              "* FAILED to free cyclic nested "
              "array members (%lu)\n ",
              OBJ2HEAD(obj)->ref);
    fiobj_free(obj);
  }
#endif

  /* test deep nesting */
  {
    fprintf(stderr, "* testing deep array nesting. \n");
    fiobj_s *top = fiobj_ary_new();
    fiobj_s *pos = top;
    for (size_t i = 0; i < 128; i++) {
      for (size_t j = 0; j < 128; j++) {
        fiobj_ary_push(pos, fiobj_num_new(j));
      }
      fiobj_ary_push(pos, fiobj_ary_new());
      pos = fiobj_ary_index(pos, -1);
      if (!pos || pos->type != FIOBJ_T_ARRAY) {
        fprintf(stderr, "* FAILED Couldn't retrive position -1 (%p)\n",
                (void *)(pos ? pos->type : 0));
        break;
      }
    }
    fiobj_free(top); /* frees both... */
  }
  fprintf(stderr, "* finished fiobj testing.\n");
}
#endif
