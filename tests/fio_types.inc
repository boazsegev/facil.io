#ifndef H_LIST_HASH_TESTS_H
#define H_LIST_HASH_TESTS_H

#include "fio_dict.h"
#include "fio_hash_table.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct ht_dict_test_s {
  uint64_t len;
  fio_ht_node_s node;
  char data[];
};

static void test_dict_store(fio_ht_s *dict, char *name, char *data) {
  uint64_t data_len = strlen(data);
  struct ht_dict_test_s *entry = malloc(sizeof(*entry) + data_len + 1);
  if (!entry)
    perror("WTF? can't allocate dictionary entry"), exit(errno);
  memcpy(entry->data, data, data_len + 1);
  entry->len = data_len;
  entry = (void *)fio_ht_add(dict, &entry->node, fio_ht_hash_cstr(name));
  if (entry) {
    entry = fio_ht_object(struct ht_dict_test_s, node, (void *)(entry));
    fprintf(stderr, "collision node: %p -> %p\n", (void *)entry,
            (void *)(&entry->node));
    fprintf(
        stderr,
        "ERROR: Collision detected for %s with data (len: %llu =? %llu):\n%s\n",
        name, data_len, entry->len, entry->data);
    free(entry);
  }
}

static void test_dict_remove(fio_ht_s *dict, char *name) {
  uint64_t name_len = strlen(name);
  struct ht_dict_test_s *entry =
      (void *)fio_ht_pop(dict, fio_ht_hash(name, name_len));
  if (!entry) {
    fprintf(stderr, "ERROR: Can't find entry for removal: %s\n", name);
    return;
  }
  entry = fio_ht_object(struct ht_dict_test_s, node, (void *)(entry));
  free(entry);
}

static void test_fio_hash_table(void) {
  const size_t TEST_CYCLES = 2000000UL;
  fio_list_s list = FIO_LIST_INIT(list);
  fio_ht_s dict = FIO_HASH_TABLE_INIT(dict);
  fprintf(stderr, "Dictionary test %p\n", (void *)&dict);

  {
    uint64_t result = fio_ht_hash(
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e", 15);
    fprintf(stderr, "* fio_ht_hash simple SipHash test %s\n",
            (result == 0xa129ca6149be45e5ULL) ? "passed" : "FAILED");
    fprintf(stderr, "* fio_ht_hash and fio_ht_hash_cstr %s\n",
            (fio_ht_hash("testing if they match", 21) ==
             fio_ht_hash_cstr("testing if they match"))
                ? "match"
                : "FAILED to match!");
  }

  struct ht_dict_test_s *item;
  char tmp[20] = {0};
  for (size_t i = 0; i < 8; i++) {
    memcpy(tmp, "item ", 5);
    tmp[5] = '1' + i;
    tmp[6] = 0;
    test_dict_store(&dict, tmp, tmp);
  }
  if (dict.count != 8)
    fprintf(stderr, "ERROR: Hash table missing objects!\n");
  fio_ht_for_each(struct ht_dict_test_s, node, item, dict)
      fprintf(stderr, "* %s\n", item->data);
  for (size_t i = 0; i < 8; i++) {
    memcpy(tmp, "item ", 5);
    tmp[5] = '1' + i;
    tmp[6] = 0;
    test_dict_remove(&dict, tmp);
  }
  if (dict.count)
    fprintf(stderr, "ERROR: Hash table should be empty!\n");

  memcpy(tmp, "item ", 5);
  size_t i_tmp, pos;
  for (size_t i = 0; i < TEST_CYCLES; i++) {
    i_tmp = i;
    pos = 5;
    while (i_tmp) {
      tmp[pos] = '0' + (i_tmp & 0xf);
      pos++;
      i_tmp >>= 4;
    }
    tmp[pos] = 0;
    test_dict_store(&dict, tmp, tmp);
  }
  if (!dict.bins)
    fprintf(stderr, "ERROR: Hash table should have bins allocated by now!\n");
  if (dict.count != TEST_CYCLES)
    fprintf(stderr,
            "ERROR: Hash table should have more elements, %llu != %lu!\n",
            dict.count, TEST_CYCLES);
  struct ht_dict_test_s *itm;

  fio_ht_for_each(struct ht_dict_test_s, node, itm, dict) {
    fio_ht_remove(&itm->node);
    free(itm);
  }
  if (dict.count)
    fprintf(stderr, "ERROR: Hash table should be empty!\n");
  fio_ht_rehash(&dict, 0);
}
struct dict_test {
  fio_dict_s node;
  char *value;
};

static void test_fio_dict_task(fio_dict_s *node, void *arg) {
  char *key = fio_node2obj(struct dict_test, node, node)->value;
  fprintf(stderr, "*Dictionary test: task  for (%p) %s\n", (void *)node, key);
  fio_dict_remove(node);
  node = fio_dict_get(arg, key, 9);
  if (node)
    fprintf(stderr,
            "ERROR (dict): (%p) \"%s\" "
            "should have been removed\n",
            (void *)node, fio_node2obj(struct dict_test, node, node)->value);
}

static void test_fio_dict_small_task(fio_dict_s *node, void *arg) {
  char *key = fio_node2obj(struct dict_test, node, node)->value;
  fprintf(stderr, "*Dictionary test: slice (%p) %s %s\n", (void *)node, key,
          arg ? arg : "");
}

static void test_fio_dict(void) {
  fio_dict_s dict = FIO_DICT_INIT;
  fio_dict_s *tmp;
  struct dict_test items[5] = {
      {.value = "channel 1"}, {.value = "channel 2"}, {.value = "Ahannel A"},
      {.value = "channel 3"}, {.value = "channel 4"},
  };
  for (size_t i = 0; i < 5; i++) {
    tmp = fio_dict_set(&dict, items[i].value, 9, &items[i].node);
    if (tmp)
      fprintf(stderr, "* ERROR (dict): an item %p (%s) shouldn't exist\n",
              (void *)tmp, fio_node2obj(struct dict_test, node, tmp)->value);
  }
  fio_dict_each(fio_dict_prefix(&dict, (void *)"chan", 4),
                test_fio_dict_small_task, NULL);
  fprintf(stderr, "* Next 7 slices test glob matching.\n");
  fio_dict_each_match_glob(&dict, "Ahannel A", 9, test_fio_dict_small_task,
                           "(exact match)");
  fio_dict_each_match_glob(&dict, "A*A", 3, test_fio_dict_small_task, "(A*A)");
  fio_dict_each_match_glob(&dict, "Ahannel ?", 9, test_fio_dict_small_task,
                           "(Ahannel ?)");
  fio_dict_each_match_glob(&dict, "Ahannel[ ]A", 11, test_fio_dict_small_task,
                           "(Ahannel[ ]A)");
  fio_dict_each_match_glob(&dict, "Ahannel [A]", 11, test_fio_dict_small_task,
                           "(Ahannel [A])");
  fio_dict_each_match_glob(&dict, "Ahannel[^a-z]A", 14,
                           test_fio_dict_small_task, "(Ahannel[^a-z]A)");
  fio_dict_each_match_glob(&dict, "Ahannel [A-Z]", 13, test_fio_dict_small_task,
                           "(Ahannel [A-Z])");
  fio_dict_each_match_glob(&dict, "A?a*A", 5, test_fio_dict_small_task,
                           "(A?n*A)");
  fio_dict_each_match_glob(&dict, "A?a* A", 6, test_fio_dict_small_task,
                           "(A?n* A)");
  fio_dict_each_match_glob(&dict, "A?a*[^a-z]A", 11, test_fio_dict_small_task,
                           "(everything)");
  fprintf(stderr, "* Next slices test glob matching for: *[0-9].\n");
  fio_dict_each_match_glob(&dict, "*[1-9]", 6, test_fio_dict_small_task,
                           "(everything)");
  fprintf(stderr, "* Match and clear at the same time.\n");
  fio_dict_each(&dict, test_fio_dict_task, &dict);
  if (!fio_dict_isempty(&dict))
    fprintf(
        stderr,
        "ERROR (dict): Dictionary should be empty, but nodes still exist!\n");
  if (fio_glob_match((uint8_t *)"A String To Match", 17, (uint8_t *)"[A-Z]*",
                     6))
    fprintf(stderr, "* Simple glob: [A-Z]*\n");
  if (fio_glob_match((uint8_t *)"A String To Match", 17, (uint8_t *)"A String*",
                     9))
    fprintf(stderr, "* Simple glob: A String*\n");
  if (fio_glob_match((uint8_t *)"A String To Match", 17,
                     (uint8_t *)"? Str?ng To Matc?", 17))
    fprintf(stderr, "* Simple glob: ? Str?ng To Matc?\n");
  if (fio_glob_match((uint8_t *)"A String To Match", 17,
                     (uint8_t *)"*ch", 3))
    fprintf(stderr, "* Simple glob: *ch\n");
  if (fio_glob_match((uint8_t *)"A String To Match", 17,
                     (uint8_t *)"[A-Z] String*ch", 15))
    fprintf(stderr, "* Simple glob matching might be working.\n");
  else
    fprintf(stderr, "* Simple glob matching is broken.\n");
}

static void test_fio_types(void) {
  test_fio_hash_table();
  test_fio_dict();
}
#endif
