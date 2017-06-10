/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "fio_dict.h"
/**
`fio_dict_s` Is based on a 4-bit trie structure, allowing for fast no-collisions
key-value matching while avoiding any hashing.

It's memory intensive... very memory intensive... but it has 0 collision risk
and offers fairly high performance.

Just to offer some insight, a single key-value pair for the key "hello" will
require ~1,360 bytes. Add the key "bye!" ad you'll add ~1,088 bytes more... but
the key "hello1" will cost only 272 bytes... brrr.
*/

/* *****************************************************************************
Implementation
***************************************************************************** */

/** Returns the `fio_dict_s *` object associated with the key, NULL if none.
 */
fio_dict_s *fio_dict_get(fio_dict_s *dict, void *key, size_t key_len) {
  dict = fio_dict_prefix(dict, key, key_len);
  if (dict && dict->used)
    return dict;
  return NULL;
}

/** Returns the old `fio_dict_s *` object associated with the key, if any.*/
fio_dict_s *fio_dict_set(fio_dict_s *dict, void *key, size_t key_len,
                         fio_dict_s *node) {
  uint8_t *pos = key;
  uint8_t tr;
  fio_dict_s *old;
  if (!key_len || !dict)
    return NULL;
  if (key_len > 1) {
    pos += key_len - 1;
    if (node == NULL) {
      dict = fio_dict_prefix(dict, key, key_len - 1);
      if (!dict)
        return NULL;
    } else
      dict = fio_dict_ensure_prefix(dict, key, key_len - 1);
  }
  tr = *pos & 0xf;
  if (!dict->trie[tr]) {
    if (node == NULL)
      return NULL;
    dict->trie[tr] = malloc(sizeof(fio_dict_s));
    *dict->trie[tr] = (fio_dict_s){.trie_val = tr, .parent = dict};
  }
  dict = dict->trie[tr];
  tr = ((*pos) >> 4) & 0xf;

  if ((old = dict->trie[tr]))
    goto replace_node;
  /* no need to remove what doesn't exist..*/
  if (node == NULL)
    return NULL;
  /* no old, but we have a new node we need to initialize and add. */
  *node = (fio_dict_s){.parent = dict, .trie_val = tr, .used = 1};
  dict->trie[tr] = node;
  return NULL;

replace_node:
  if (node == NULL)
    goto remove_node;
  /* We have an old node to be replaced with a new one. */
  *node = *old;
  node->used = 1;
  dict->trie[tr] = node;
  for (size_t i = 0; i < 16; i++) {
    if (node->trie[i])
      node->trie[i]->parent = node;
  }
  if (old->used)
    return old;
  return NULL;

remove_node:
  return fio_dict_remove(old);
}

/** Returns the old `fio_dict_s *` object associated with the key, if any.*/
fio_dict_s *fio_dict_remove(fio_dict_s *node) {
  fio_dict_s *tmp;
  fio_dict_s *dict = node->parent;
  /* We need to remove the existing node and clear the branch if empty. */
  if (fio_dict_isempty(node)) {
    dict->trie[node->trie_val] = NULL;
    while (fio_dict_isempty(dict)) {
      if (dict->used || !dict->parent)
        break;
      tmp = dict;
      dict = dict->parent;
      dict->trie[tmp->trie_val] = NULL;
      free(tmp);
    }
    if (node->used)
      return node;
    return NULL;
  }
  /* We need to place an alternative node. */
  if (!node->used)
    return NULL;
  tmp = malloc(sizeof(fio_dict_s));
  *tmp = *node;
  tmp->used = 0;
  for (size_t i = 0; i < 16; i++) {
    if (tmp->trie[i])
      tmp->trie[i]->parent = tmp;
  }
  return node;
}

/** Returns a `fio_dict_s *` dictionary (or NULL) of all `prefix` children. */
fio_dict_s *fio_dict_step(fio_dict_s *dict, uint8_t prefix) {
  if (!dict)
    return NULL;
  dict = dict->trie[prefix & 0xf];
  if (!dict)
    return NULL;
  return dict->trie[(prefix >> 4) & 0xf];
}

/** Returns a `fio_dict_s *` dictionary (or NULL) of all `prefix` children. */
fio_dict_s *fio_dict_prefix(fio_dict_s *dict, void *prefix_, size_t len) {
  uint8_t *prefix = prefix_;
  while (dict && len) {
    dict = fio_dict_step(dict, *prefix);
    prefix++;
    len--;
  }
  if (len)
    return NULL;
  return dict;
}

/**
 * Creates a `fio_dict_s *` dictionary (if missing) for the `prefix`...
 *
 * After calling this function a node MUST be added to this dictionary, or
 * memory leaks will occure.
 */
fio_dict_s *fio_dict_ensure_prefix(fio_dict_s *dict, void *prefix, size_t len) {
  uint8_t *pos = prefix;
  uint8_t tr;

  while (dict && len) {
    tr = *pos & 0xf;
    if (!dict->trie[tr]) {
      dict->trie[tr] = malloc(sizeof(fio_dict_s));
      *dict->trie[tr] = (fio_dict_s){.trie_val = tr, .parent = dict};
    }
    dict = dict->trie[tr];
    tr = (*pos >> 4) & 0xf;
    if (!dict->trie[tr]) {
      dict->trie[tr] = malloc(sizeof(fio_dict_s));
      *dict->trie[tr] = (fio_dict_s){.trie_val = tr, .parent = dict};
    }
    dict = dict->trie[tr];
    pos++;
    len--;
  }
  return dict;
}

/** Traverses a dictionary, performing an action for each item. */
void fio_dict_each(fio_dict_s *dict,
                   void (*action)(fio_dict_s *node, void *arg), void *arg) {
  if (!dict)
    return;
  fio_dict_s *child, *head = dict->parent, *to_call = NULL;
  uint8_t tr;
  tr = 0;

/* We use this to make sure that the function doesn't break if `to_call` is
 * removed from the trie while we are iterating. */
#define test_callback()                                                        \
  do {                                                                         \
    if (to_call)                                                               \
      action(to_call, arg);                                                    \
    to_call = dict;                                                            \
  } while (0);

  while (dict != head) {
  top:
    while (tr < 16) {
      /* walk child */
      if ((child = dict->trie[tr])) {
        dict = child;
        tr = 0;
        goto top;
      }
      tr++;
    }
    if (dict->used)
      test_callback();
    tr = dict->trie_val + 1;
    dict = dict->parent;
  }
  test_callback();
#undef test_callback
}

/**

Traverses a dictionary, performing an action for each item.

based on the same matching behavior used by Redis... hopefully...
https://github.com/antirez/redis/blob/d680eb6dbdf2d2030cb96edfb089be1e2a775ac1/src/util.c#L47
*/
void fio_dict_each_match(fio_dict_s *dict, void *pattern, size_t len,
                         void (*action)(fio_dict_s *node, void *arg),
                         void *arg) {

  if (!dict || !pattern || !action)
    return;
  fio_dict_s *child, *head = dict->parent;
  uint8_t *pos = pattern;

  while (len && dict) {

    if (*pos == '*') {
      /* eat up all '*' and match whatever */
      while (*pos == '*' && len) {
        len--;
        pos++;
      }
      if (!len) {
        fio_dict_each(dict, action, arg);
        return;
      }
      /* test each "tail"... (brrr). */
      head = dict->parent;
      uint8_t tr = 0;
      while (dict != head) {
      glob_top:
        while (tr < 16) {
          /* walk child */
          if ((child = dict->trie[tr])) {
            dict = child;
            tr = 0;
            goto glob_top;
          }
          tr++;
        }
        fio_dict_each_match(dict, pos, len, action, arg);
        tr = dict->trie_val + 1;
        dict = dict->parent;
      }
      fio_dict_each_match(dict, pos, len, action, arg);
      return;
    }

    if (*pos == '?') {
      pos++;
      len--;
      for (size_t i = 0; i < 256; i++) {
        if (dict->trie[i & 0xf])
          fio_dict_each_match(dict->trie[i & 0xf]->trie[(i >> 4) & 0xf], pos,
                              len, action, arg);
      }
      return;
    }

    if (*pos == '[') {
      int state = 0;
      uint8_t map[256] = {0};
      pos++;
      len--;
      if (*pos == '^') {
        state = 1;
        pos++;
        len--;
      }
      if (!len)
        return; /* pattern error */
      while (*pos != ']' && len) {
        if (*pos == '\\') {
          pos++;
          len--;
        }
        if (pos[1] == '-') {
          if (len <= 4) {
            return; /* pattern error */
          }
          uint8_t end, start;
          start = *pos;
          pos += 2;
          len -= 2;
          if (*pos == '\\') {
            pos++;
            len--;
          }
          end = *pos;
          if (start > end) {
            uint8_t tmp;
            tmp = start;
            start = end;
            end = tmp;
          }
          while (start < end)
            map[start++] = 1;
          map[end] = 1; /* prevent endless loop where `end == 255`*/
        } else
          map[*pos] = 1;
        pos++;
        len--;
      }
      if (!len) {
        return;
      }
      pos++;
      len--;
      for (size_t i = 0; i < 256; i++) {
        /* code */
        if (map[i] == state)
          continue;
        if (dict->trie[i & 0xf])
          fio_dict_each_match(dict->trie[i & 0xf]->trie[(i >> 4) & 0xf], pos,
                              len, action, arg);
      }
      return;
    }

    if (*pos == '\\' && len > 1) {
      pos++;
      len--;
    }
    dict = fio_dict_step(dict, *pos);
    pos++;
    len--;
  }

  if (dict && dict->used)
    action(dict, arg);
}
