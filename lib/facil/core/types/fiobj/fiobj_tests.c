/*
Copyright: Boaz segev, 2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifdef DEBUG

#include "bscrypt.h"
#include "fiobj_types.h"
// #include "fio2resp.h"

/* *****************************************************************************
A JSON testing
***************************************************************************** */

void fiobj_test_hash_json(void) {
  fiobj_pt hash = fiobj_hash_new();
  fiobj_pt hash2 = fiobj_hash_new();
  fiobj_pt hash3 = fiobj_hash_new();
  fiobj_pt syms = fiobj_ary_new(); /* freed within Hashes */
  char num_buffer[68] = "0x";
  fiobj_pt tmp, sym;

  sym = fiobj_sym_new("id", 2);
  fiobj_ary_push(syms, sym);
  fiobj_hash_set(hash, sym, fiobj_num_new(bscrypt_rand64()));

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
      fiobj_strprintf("\n\ttake \\ my  \\ %s"
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
      sym = fiobj_ary_entry(syms, i);
      fiobj_hash_set(hash2, sym, fiobj_dup(fiobj_hash_get(hash, sym)));
    }
    /* shallow copy in reverse order */
    for (size_t i = 0; i <= len; i++) {
      sym = fiobj_ary_entry(syms, len - i);
      fiobj_hash_set(hash3, sym, fiobj_dup(fiobj_hash_get(hash, sym)));
    }
    fprintf(stderr, "* Testing shallow copy reference count: %s\n",
            (OBJ2HEAD(syms).ref == 3) ? "passed." : "FAILED!");
    fprintf(stderr,
            "* Testing deep object equality review:\n"
            "  * Eq. Hash (ordered): %s\n"
            "  * Eq. Hash (unordered): %s\n"
            "  * Hash vs. Array: %s\n",
            (fiobj_iseq(hash, hash2)) ? "passed." : "FAILED!",
            (fiobj_iseq(hash, hash3)) ? "passed." : "FAILED!",
            (fiobj_iseq(hash, syms)) ? "FAILED!" : "passed.");
  }
  /* print JSON string */
  tmp = fiobj_str_new_json(hash);
  fprintf(stderr, "* Printing JSON (len: %llu capa: %lu):\n   %s\n",
          fiobj_obj2cstr(tmp).len, fiobj_str_capa(tmp),
          fiobj_obj2cstr(tmp).data);
  fiobj_free(tmp);

  sym = fiobj_sym_new("hash", 4);
  tmp = fiobj_sym_new("1", 1);

  fprintf(stderr,
          "* Reference count for "
          "nested nested object in nested Hash: %llu\n",
          OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref);

  fiobj_free(hash);
  fprintf(stderr, "* Testing nested Array delete reference count: %s\n",
          (OBJ2HEAD(syms).ref == 2) ? "passed." : "FAILED!");
  fprintf(stderr, "* Testing nested Hash delete reference count: %s\n",
          (OBJ2HEAD(fiobj_hash_get(hash2, sym)).ref == 2) ? "passed."
                                                          : "FAILED!");
  fprintf(stderr,
          "* Testing reference count for "
          "nested nested object in nessted Hash: %s\n",
          (OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref == 2)
              ? "passed."
              : "FAILED!");
  fprintf(stderr,
          "* Reference count for "
          "nested nested object in nested Hash: %llu\n",
          OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref);

  fiobj_free(hash2);
  fprintf(stderr, "* Testing nested Array delete reference count: %s\n",
          (OBJ2HEAD(syms).ref == 1) ? "passed." : "FAILED!");
  fprintf(stderr, "* Testing nested Hash delete reference count: %s\n",
          (OBJ2HEAD(fiobj_hash_get(hash3, sym)).ref == 1) ? "passed."
                                                          : "FAILED!");
  fprintf(stderr,
          "* Testing reference count for "
          "nested nested object in nessted Hash: %s\n",
          (OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref == 1)
              ? "passed."
              : "FAILED!");
  fprintf(stderr,
          "* Reference count for "
          "nested nested object in nested Hash: %llu\n",
          OBJ2HEAD(fiobj_hash_get(fiobj_hash_get(hash3, sym), tmp)).ref);

  fiobj_free(hash3);
  fiobj_free(sym);
  fiobj_free(tmp);
}

/* *****************************************************************************
Basic tests fiobj types
***************************************************************************** */

/* used to print objects */
static int fiobj_test_array_task(fiobj_s *obj, void *arg) {
  static __thread uintptr_t count = 0;
  static __thread const char *stop = ".";
  static __thread fio_ls_s nested = {NULL, NULL, NULL};
  if (!nested.next)
    nested = (fio_ls_s)FIO_LS_INIT(nested);

  if (obj && obj->type == FIOBJ_T_ARRAY) {
    fio_ls_push(&nested, (fiobj_s *)stop);
    if (!count) {
      fio_ls_push(&nested, (fiobj_s *)count);
      fprintf(stderr, "\n* Array data: [");
    } else {
      fio_ls_push(&nested, (fiobj_s *)(--count));
      fprintf(stderr, "[ ");
    }
    stop = " ]";
    count = fiobj_ary_count(obj);
    return 0;
  } else if (obj && obj->type == FIOBJ_T_HASH) {
    fio_ls_push(&nested, (fiobj_s *)stop);
    fio_ls_push(&nested, (fiobj_s *)count);
    if (!count)
      fprintf(stderr, "\n* Hash data: {");
    else {
      fprintf(stderr, "{ ");
    }
    stop = " }";
    count = fiobj_hash_count(obj);
    return 0;
  }
  if (obj && obj->type == FIOBJ_T_COUPLET) {
    fprintf(stderr, "%s:", fiobj_obj2cstr(fiobj_couplet2key(obj)).data);
    fiobj_s *tmp = fiobj_couplet2obj(obj);
    if (tmp->type == FIOBJ_T_HASH) {
      obj = fiobj_couplet2key(obj);
      count += fiobj_hash_count(tmp);
    } else if (tmp->type == FIOBJ_T_ARRAY) {
      obj = fiobj_couplet2key(obj);
      count += fiobj_ary_count(tmp);
    }
  }
  if (--count) {
    fprintf(stderr, "%s, ", fiobj_obj2cstr(obj).data);
  } else {
    count = (uintptr_t)fio_ls_pop(&nested);
    if (count) {
      fprintf(stderr, "%s %s, ", fiobj_obj2cstr(obj).data, stop);
      stop = (char *)fio_ls_pop(&nested);
    } else {
      fprintf(stderr, "%s %s ", fiobj_obj2cstr(obj).data, stop);
      stop = (char *)fio_ls_pop(&nested);
      if (stop[0] != '.')
        fprintf(stderr, "%s", stop);
      else
        fprintf(stderr, "\n (should be 127..0)\n");
    }
  }

  return 0;
  (void)arg;
}

/* *****************************************************************************
The Testing code
***************************************************************************** */

static char num_buffer[148];

/* test were written for OSX (fprintf types) with clang (%s for NULL is okay)
 */
void fiobj_test(void) {
  /* test JSON (I know... it assumes everything else works...) */
  fiobj_test_hash_json();

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
    fprintf(stderr, "* FAILED 255 object test i == %llu with type %d.\n",
            fiobj_obj2num(obj), obj->type);
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
        fprintf(stderr, "* FAILED Array count. %lu/%llu != %lu\n",
                fiobj_ary_count(obj), obj2ary(obj)->capa, i + 1);
    }
    fiobj_each2(obj, fiobj_test_array_task, NULL);
    fiobj_free(obj);
  } else {
    fprintf(stderr, "* FAILED to initialize Array test!\n");
    fiobj_free(obj);
  }

  /* test hash */
  obj = fiobj_hash_new();
  if (obj->type == FIOBJ_T_HASH) {
    fprintf(stderr, "* testing Hash. \n");
    fiobj_s *syms = fiobj_ary_new();
    fiobj_s *a = fiobj_ary_new();
    fiobj_s *tmp;
    fiobj_ary_push(a, fiobj_str_new("String in a nested Array", 24));
    tmp = fiobj_sym_new("array", 5);
    fiobj_hash_set(obj, tmp, a);
    fiobj_free(tmp);

    for (size_t i = 0; i < 128; i++) {
      tmp = fiobj_num_new(i);
      fio_cstr_s s = fiobj_obj2cstr(tmp);
      fiobj_ary_set(syms, fiobj_sym_new(s.buffer, s.len), i);
      fiobj_hash_set(obj, fiobj_ary_entry(syms, i), tmp);
      if (fiobj_hash_count(obj) != i + 2)
        fprintf(stderr, "* FAILED Hash count. %lu != %lu\n",
                fiobj_hash_count(obj), i + 2);
    }

    if (OBJ2HEAD(fiobj_ary_entry(syms, 2)).ref < 2)
      fprintf(stderr, "* FAILED Hash Symbol duplication.\n");

    for (size_t i = 0; i < 128; i++) {
      if ((size_t)fiobj_obj2num(
              fiobj_hash_get(obj, fiobj_ary_entry(syms, i))) != i)
        fprintf(stderr,
                "* FAILED to retrive data from hash for Symbol %s (%p): %lld "
                "(%p) type %d\n",
                fiobj_obj2cstr(fiobj_ary_entry(syms, i)).data,
                (void *)((fio_sym_s *)fiobj_ary_entry(syms, i))->hash,
                fiobj_obj2num(fiobj_hash_get(obj, fiobj_ary_entry(syms, i))),
                (void *)fiobj_hash_get(obj, fiobj_ary_entry(syms, i)),
                fiobj_hash_get(obj, fiobj_ary_entry(syms, i))
                    ? fiobj_hash_get(obj, fiobj_ary_entry(syms, i))->type
                    : 0);
    }

    fiobj_each2(obj, fiobj_test_array_task, NULL);

    // char tmp_buffer[4096];
    // size_t tmp_size = 4096;
    // if (resp_fioformat((uint8_t *)tmp_buffer, &tmp_size, obj))
    //   fprintf(stderr, "* notice, RESP formatting required more space
    //   (%lu).\n",
    //           tmp_size);
    // fprintf(stderr, "* RESP formatting:\n%.*s", (int)tmp_size, tmp_buffer);

    fiobj_free(obj);
    if (OBJ2HEAD(fiobj_ary_entry(syms, 2)).ref != 1)
      fprintf(stderr, "* FAILED Hash Symbol deallocation.\n");
    fiobj_free(syms);
  } else {
    fprintf(stderr, "* FAILED to initialize Hash test!\n");
    if (obj)
      fiobj_free(obj);
  }
  obj = NULL;

  /* test cyclic protection */
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
            "a1, pos %llu  == a2 and a2, pos %llu == a1\n",
            obj2ary(a1)->end, obj2ary(a2)->end);
    fiobj_each2(a1, fiobj_test_array_task, NULL);

    obj = fiobj_dup(fiobj_ary_entry(a2, -3));
    if (!obj || obj->type != FIOBJ_T_NUMBER)
      fprintf(stderr, "* FAILED unexpected object %p with type %d\n",
              (void *)obj, obj ? obj->type : 0);
    if (OBJ2HEAD(obj).ref < 2)
      fprintf(stderr, "* FAILED object reference counting test (%llu)\n",
              OBJ2HEAD(obj).ref);
    fiobj_free(a1); /* frees both... */
    // fiobj_free(a2);
    if (OBJ2HEAD(obj).ref != 1)
      fprintf(stderr,
              "* FAILED to free cyclic nested "
              "array members (%llu)\n ",
              OBJ2HEAD(obj).ref);
    fiobj_free(obj);
  }

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
      pos = fiobj_ary_entry(pos, -1);
      if (!pos || pos->type != FIOBJ_T_ARRAY) {
        fprintf(stderr, "* FAILED Couldn't retrive position -1 (%d)\n",
                pos ? pos->type : 0);
        break;
      }
    }
    fiobj_free(top); /* frees both... */
  }
}
#endif
