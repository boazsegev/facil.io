/*
Copyright: Boaz segev, 2017
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
Inline variation
***************************************************************************** */

static inline fio_dict_s *fio_dict_step_inline(fio_dict_s *dict,
                                               uint8_t prefix) {
  if (!dict)
    return NULL;
  dict = dict->trie[prefix & 0xf];
  if (!dict)
    return NULL;
  return dict->trie[(prefix >> 4) & 0xf];
}

static inline fio_dict_s *fio_dict_prefix_inline(fio_dict_s *dict,
                                                 void *prefix_, size_t len) {
  uint8_t *prefix = prefix_;
  while (dict && len) {
    dict = fio_dict_step_inline(dict, *prefix);
    prefix++;
    len--;
  }
  if (len)
    return NULL;
  return dict;
}

/* *****************************************************************************
Implementation
***************************************************************************** */

/** Returns the `fio_dict_s *` object associated with the key, NULL if none.
 */
fio_dict_s *fio_dict_get(fio_dict_s *dict, void *key, size_t key_len) {
  dict = fio_dict_prefix_inline(dict, key, key_len);
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
  if (!node) {
    dict = fio_dict_get(dict, key, key_len);
    fio_dict_remove(dict);
    return dict;
  }
  if (key_len > 1) {
    pos += key_len - 1;
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

  /* no old, but we have a new node we need to initialize and add. */
  *node = (fio_dict_s){.parent = dict, .trie_val = tr, .used = 1};
  dict->trie[tr] = node;
  return NULL;

replace_node:
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
  return fio_dict_step_inline(dict, prefix);
}

/** Returns a `fio_dict_s *` dictionary (or NULL) of all `prefix` children. */
fio_dict_s *fio_dict_prefix(fio_dict_s *dict, void *prefix_, size_t len) {
  return fio_dict_prefix_inline(dict, prefix_, len);
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
void fio_dict_each_match_glob(fio_dict_s *dict, void *pattern, size_t len,
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
        fio_dict_each_match_glob(dict, pos, len, action, arg);
        tr = dict->trie_val + 1;
        dict = dict->parent;
      }
      fio_dict_each_match_glob(dict, pos, len, action, arg);
      return;
    }

    if (*pos == '?') {
      pos++;
      len--;
      for (size_t i = 0; i < 256; i++) {
        if (dict->trie[i & 0xf])
          fio_dict_each_match_glob(dict->trie[i & 0xf]->trie[(i >> 4) & 0xf],
                                   pos, len, action, arg);
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
          if (len < 4) {
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
          fio_dict_each_match_glob(dict->trie[i & 0xf]->trie[(i >> 4) & 0xf],
                                   pos, len, action, arg);
      }
      return;
    }

    if (*pos == '\\' && len > 1) {
      pos++;
      len--;
    }
    dict = fio_dict_step_inline(dict, *pos);
    pos++;
    len--;
  }

  if (dict && dict->used)
    action(dict, arg);
}

/** A binary glob matching helper. Returns 1 on match, otherwise returns 0. */
int fio_glob_match(uint8_t *data, size_t data_len, uint8_t *pattern,
                   size_t pat_len) {
  /* adapted and rewritten, with thankfulness, from the code at:
   * https://github.com/opnfv/kvmfornfv/blob/master/kernel/lib/glob.c
   *
   * Original version's copyright:
   * Copyright 2015 Open Platform for NFV Project, Inc. and its contributors
   * Under the MIT license.
   */

  /*
   * Backtrack to previous * on mismatch and retry starting one
   * character later in the string.  Because * matches all characters
   * (no exception for /), it can be easily proved that there's
   * never a need to backtrack multiple levels.
   */
  uint8_t *back_pat = NULL, *back_str = data;
  size_t back_pat_len = 0, back_str_len = data_len;

  /*
   * Loop over each token (character or class) in pat, matching
   * it against the remaining unmatched tail of str.  Return false
   * on mismatch, or true after matching the trailing nul bytes.
   */
  while (data_len) {
    uint8_t c = *data++;
    uint8_t d = *pattern++;
    data_len--;
    pat_len--;

    switch (d) {
    case '?': /* Wildcard: anything goes */
      break;

    case '*':       /* Any-length wildcard */
      if (!pat_len) /* Optimize trailing * case */
        return 1;
      back_pat = pattern;
      back_pat_len = pat_len;
      back_str = --data; /* Allow zero-length match */
      back_str_len = ++data_len;
      break;

    case '[': { /* Character class */
      uint8_t match = 0, inverted = (*pattern == '^');
      uint8_t *cls = pattern + inverted;
      uint8_t a = *cls++;

      /*
       * Iterate over each span in the character class.
       * A span is either a single character a, or a
       * range a-b.  The first span may begin with ']'.
       */
      do {
        uint8_t b = a;

        if (cls[0] == '-' && cls[1] != ']') {
          b = cls[1];

          cls += 2;
          if (a > b) {
            uint8_t tmp = a;
            a = b;
            b = tmp;
          }
        }
        match |= (a <= c && c <= b);
      } while ((a = *cls++) != ']');

      if (match == inverted)
        goto backtrack;
      pat_len -= cls - pattern;
      pattern = cls;

    } break;
    case '\\':
      d = *pattern++;
      pat_len--;
    /*FALLTHROUGH*/
    default: /* Literal character */
      if (c == d)
        break;
    backtrack:
      if (!back_pat)
        return 0; /* No point continuing */
      /* Try again from last *, one character later in str. */
      pattern = back_pat;
      data = ++back_str;
      data_len = --back_str_len;
      pat_len = back_pat_len;
    }
  }
  return !data_len && !pat_len;
}
