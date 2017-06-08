#ifndef H_LIST_HASH_TESTS_H
#define H_LIST_HASH_TESTS_H

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
  uint64_t name_len = strlen(name);
  struct ht_dict_test_s *entry = malloc(sizeof(*entry) + data_len + 1);
  if (!entry)
    perror("WTF? can't allocate dictionary entry"), exit(errno);
  memcpy(entry->data, data, data_len + 1);
  entry->len = data_len;
  entry = (void *)fio_ht_add(dict, &entry->node, fio_ht_hash(name, name_len));
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

static void test_fio_types(void) {
  const size_t TEST_CYCLES = 2000000UL;
  fio_list_s list = FIO_LIST_INIT(list);
  fio_ht_s dict = FIO_HASH_TABLE_INIT(dict);
  fprintf(stderr, "Dictionary test %p\n", (void *)&dict);
  struct ht_dict_test_s *item;
  char tmp[20] = {0};
  for (size_t i = 0; i < 8; i++) {
    memcpy(tmp, "item ", 5);
    tmp[5] = '1' + i;
    tmp[6] = 0;
    test_dict_store(&dict, tmp, tmp);
  }
  if (dict.bins)
    fprintf(stderr,
            "ERROR: Hash table has bins when it should act as a list\n");
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

#endif
