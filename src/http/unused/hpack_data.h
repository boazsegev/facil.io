/********** This file wasn't tested against Endieness variations ************/
#ifndef HPACK_TABLE_DATA_H
#define HPACK_TABLE_DATA_H
/* use the GNU SOURCE extensions */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* Use the standard library. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Allow some inline functions to be created, without requiring their use. */
#ifndef __unused
#define __unused __attribute__((unused))
#endif

/** The Huffman Encoding table was copied from http://httpwg.org/specs/rfc7541.html#huffman.code
  *
  * To create and print the Huffman decoding tree (paste the tree below):
  *
  *       #define HPACK_BUILD_HUFFMAN_TREE
  *       #include "hpack_data.h"
  *       int main(int argc, char const *argv[]) {
  *            print_huff();
  *       }
  */
static const __unused struct {
  const uint32_t code;
  const uint8_t bits;
} huffman_encode_table[] = {
    /* 257 elements, 0..256 all sym + EOS */
    {0x1ff8U, 13},     {0x7fffd8U, 23},   {0xfffffe2U, 28},  {0xfffffe3U, 28},
    {0xfffffe4U, 28},  {0xfffffe5U, 28},  {0xfffffe6U, 28},  {0xfffffe7U, 28},
    {0xfffffe8U, 28},  {0xffffeaU, 24},   {0x3ffffffcU, 30}, {0xfffffe9U, 28},
    {0xfffffeaU, 28},  {0x3ffffffdU, 30}, {0xfffffebU, 28},  {0xfffffecU, 28},
    {0xfffffedU, 28},  {0xfffffeeU, 28},  {0xfffffefU, 28},  {0xffffff0U, 28},
    {0xffffff1U, 28},  {0xffffff2U, 28},  {0x3ffffffeU, 30}, {0xffffff3U, 28},
    {0xffffff4U, 28},  {0xffffff5U, 28},  {0xffffff6U, 28},  {0xffffff7U, 28},
    {0xffffff8U, 28},  {0xffffff9U, 28},  {0xffffffaU, 28},  {0xffffffbU, 28},
    {0x14U, 6},        {0x3f8U, 10},      {0x3f9U, 10},      {0xffaU, 12},
    {0x1ff9U, 13},     {0x15U, 6},        {0xf8U, 8},        {0x7faU, 11},
    {0x3faU, 10},      {0x3fbU, 10},      {0xf9U, 8},        {0x7fbU, 11},
    {0xfaU, 8},        {0x16U, 6},        {0x17U, 6},        {0x18U, 6},
    {0x0U, 5},         {0x1U, 5},         {0x2U, 5},         {0x19U, 6},
    {0x1aU, 6},        {0x1bU, 6},        {0x1cU, 6},        {0x1dU, 6},
    {0x1eU, 6},        {0x1fU, 6},        {0x5cU, 7},        {0xfbU, 8},
    {0x7ffcU, 15},     {0x20U, 6},        {0xffbU, 12},      {0x3fcU, 10},
    {0x1ffaU, 13},     {0x21U, 6},        {0x5dU, 7},        {0x5eU, 7},
    {0x5fU, 7},        {0x60U, 7},        {0x61U, 7},        {0x62U, 7},
    {0x63U, 7},        {0x64U, 7},        {0x65U, 7},        {0x66U, 7},
    {0x67U, 7},        {0x68U, 7},        {0x69U, 7},        {0x6aU, 7},
    {0x6bU, 7},        {0x6cU, 7},        {0x6dU, 7},        {0x6eU, 7},
    {0x6fU, 7},        {0x70U, 7},        {0x71U, 7},        {0x72U, 7},
    {0xfcU, 8},        {0x73U, 7},        {0xfdU, 8},        {0x1ffbU, 13},
    {0x7fff0U, 19},    {0x1ffcU, 13},     {0x3ffcU, 14},     {0x22U, 6},
    {0x7ffdU, 15},     {0x3U, 5},         {0x23U, 6},        {0x4U, 5},
    {0x24U, 6},        {0x5U, 5},         {0x25U, 6},        {0x26U, 6},
    {0x27U, 6},        {0x6U, 5},         {0x74U, 7},        {0x75U, 7},
    {0x28U, 6},        {0x29U, 6},        {0x2aU, 6},        {0x7U, 5},
    {0x2bU, 6},        {0x76U, 7},        {0x2cU, 6},        {0x8U, 5},
    {0x9U, 5},         {0x2dU, 6},        {0x77U, 7},        {0x78U, 7},
    {0x79U, 7},        {0x7aU, 7},        {0x7bU, 7},        {0x7ffeU, 15},
    {0x7fcU, 11},      {0x3ffdU, 14},     {0x1ffdU, 13},     {0xffffffcU, 28},
    {0xfffe6U, 20},    {0x3fffd2U, 22},   {0xfffe7U, 20},    {0xfffe8U, 20},
    {0x3fffd3U, 22},   {0x3fffd4U, 22},   {0x3fffd5U, 22},   {0x7fffd9U, 23},
    {0x3fffd6U, 22},   {0x7fffdaU, 23},   {0x7fffdbU, 23},   {0x7fffdcU, 23},
    {0x7fffddU, 23},   {0x7fffdeU, 23},   {0xffffebU, 24},   {0x7fffdfU, 23},
    {0xffffecU, 24},   {0xffffedU, 24},   {0x3fffd7U, 22},   {0x7fffe0U, 23},
    {0xffffeeU, 24},   {0x7fffe1U, 23},   {0x7fffe2U, 23},   {0x7fffe3U, 23},
    {0x7fffe4U, 23},   {0x1fffdcU, 21},   {0x3fffd8U, 22},   {0x7fffe5U, 23},
    {0x3fffd9U, 22},   {0x7fffe6U, 23},   {0x7fffe7U, 23},   {0xffffefU, 24},
    {0x3fffdaU, 22},   {0x1fffddU, 21},   {0xfffe9U, 20},    {0x3fffdbU, 22},
    {0x3fffdcU, 22},   {0x7fffe8U, 23},   {0x7fffe9U, 23},   {0x1fffdeU, 21},
    {0x7fffeaU, 23},   {0x3fffddU, 22},   {0x3fffdeU, 22},   {0xfffff0U, 24},
    {0x1fffdfU, 21},   {0x3fffdfU, 22},   {0x7fffebU, 23},   {0x7fffecU, 23},
    {0x1fffe0U, 21},   {0x1fffe1U, 21},   {0x3fffe0U, 22},   {0x1fffe2U, 21},
    {0x7fffedU, 23},   {0x3fffe1U, 22},   {0x7fffeeU, 23},   {0x7fffefU, 23},
    {0xfffeaU, 20},    {0x3fffe2U, 22},   {0x3fffe3U, 22},   {0x3fffe4U, 22},
    {0x7ffff0U, 23},   {0x3fffe5U, 22},   {0x3fffe6U, 22},   {0x7ffff1U, 23},
    {0x3ffffe0U, 26},  {0x3ffffe1U, 26},  {0xfffebU, 20},    {0x7fff1U, 19},
    {0x3fffe7U, 22},   {0x7ffff2U, 23},   {0x3fffe8U, 22},   {0x1ffffecU, 25},
    {0x3ffffe2U, 26},  {0x3ffffe3U, 26},  {0x3ffffe4U, 26},  {0x7ffffdeU, 27},
    {0x7ffffdfU, 27},  {0x3ffffe5U, 26},  {0xfffff1U, 24},   {0x1ffffedU, 25},
    {0x7fff2U, 19},    {0x1fffe3U, 21},   {0x3ffffe6U, 26},  {0x7ffffe0U, 27},
    {0x7ffffe1U, 27},  {0x3ffffe7U, 26},  {0x7ffffe2U, 27},  {0xfffff2U, 24},
    {0x1fffe4U, 21},   {0x1fffe5U, 21},   {0x3ffffe8U, 26},  {0x3ffffe9U, 26},
    {0xffffffdU, 28},  {0x7ffffe3U, 27},  {0x7ffffe4U, 27},  {0x7ffffe5U, 27},
    {0xfffecU, 20},    {0xfffff3U, 24},   {0xfffedU, 20},    {0x1fffe6U, 21},
    {0x3fffe9U, 22},   {0x1fffe7U, 21},   {0x1fffe8U, 21},   {0x7ffff3U, 23},
    {0x3fffeaU, 22},   {0x3fffebU, 22},   {0x1ffffeeU, 25},  {0x1ffffefU, 25},
    {0xfffff4U, 24},   {0xfffff5U, 24},   {0x3ffffeaU, 26},  {0x7ffff4U, 23},
    {0x3ffffebU, 26},  {0x7ffffe6U, 27},  {0x3ffffecU, 26},  {0x3ffffedU, 26},
    {0x7ffffe7U, 27},  {0x7ffffe8U, 27},  {0x7ffffe9U, 27},  {0x7ffffeaU, 27},
    {0x7ffffebU, 27},  {0xffffffeU, 28},  {0x7ffffecU, 27},  {0x7ffffedU, 27},
    {0x7ffffeeU, 27},  {0x7ffffefU, 27},  {0x7fffff0U, 27},  {0x3ffffeeU, 26},
    {0x3fffffffU, 30},
};

/* *****************************************************************************
 * This section is used to create a static, hard-coded Huffman Tree from the
 * `huffman_encode_table` array. It will be ignored unless
 * HPACK_BUILD_HUFFMAN_TREE is defined.
 *
 * To print the tree to `stderr`, use the `print_huff` function.
 */

#ifdef HPACK_BUILD_HUFFMAN_TREE

/* ****************************************************
 * A simple stack of `void *` - used to avoid recursion
 */

#define STACK_INITIAL_CAPACITY 32

typedef struct {
  size_t capacity;
  size_t length;
  void *data[];
} stack_s;

/** creates a new stack. free the stack using `free`. */
static inline __unused stack_s *stack_new(void) {
  stack_s *stack =
      malloc(sizeof(*stack) + (sizeof(void *) * STACK_INITIAL_CAPACITY));
  stack->capacity = STACK_INITIAL_CAPACITY;
  stack->length = 0;
  return stack;
}

/** Returns the newest member of the stack, removing it from the stack. */
static inline __unused void *stack_pop(stack_s *stack) {
  if (stack->length == 0)
    return NULL;
  stack->length -= 1;
  return stack->data[stack->length];
}
/** returns the topmost (newest) object of the stack without removing it. */
static inline __unused void *stack_peek(stack_s *stack) {
  return (stack->length ? stack->data[stack->length - 1] : NULL);
}
/** returns the number of objects in the stack. */
static inline __unused size_t stack_count(stack_s *stack) {
  return stack->length;
}

/** Adds a new object to the tsack, reallocating the stack if necessary.
 * Return the new stack pointer. Returns NULL of failure. */
static inline __unused stack_s *stack_push(stack_s *stack, void *ptr) {
  if (stack->length == stack->capacity) {
    stack->capacity = stack->capacity << 1;
    stack = realloc(stack, sizeof(*stack) + (sizeof(void *) * stack->capacity));
    if (!stack)
      return NULL;
  }
  stack->data[stack->length] = ptr;
  stack->length += 1;
  return stack;
}

/* ****************************************************
 * These functions model a Huffman decoding tree based on the array
 * and print the model to `stderr`.
 */

/** used to print the binary reverse testing */
static __unused void __print_bin_num(uint32_t num) {
  fprintf(stderr, "0b");
  for (size_t i = 0; i < 32; i++) {
    if (num & (1 << (31 - i)))
      fprintf(stderr, "1");
    else
      fprintf(stderr, "0");
  }
}

/** the Huffman Tree printing function */
static __unused void print_huff(void) {
  struct huff_node {
    stack_s *children;
    struct huff_node *zero;
    struct huff_node *one;
    uint32_t depth;
    uint16_t value;
  } tree = {0};
  struct huffman_data {
    uint16_t sym;
    uint32_t code;
    uint8_t bits;
  } * hdata;

  struct huffman_data huffman_decode_table[257] = {};

  /* create a table from the array, inverting the bit order for the decoding tree. */
  tree.children = stack_new();
  for (size_t i = 0; i < 257; i++) {
    huffman_decode_table[i] = (struct huffman_data){
        .sym = i, .code = 0, .bits = huffman_encode_table[i].bits};
    /* inverse the bit order when decoding */
    for (size_t j = 0; j < huffman_encode_table[i].bits; j++) {
      huffman_decode_table[i].code = (huffman_decode_table[i].code << 1) |
                                     ((huffman_encode_table[i].code >> j) & 1);
    }
    // fprintf(stderr, "%d = ", i);
    // __print_bin_num(huffman_decode_table[i].code);
    // fprintf(stderr, " !=(?) ");
    // __print_bin_num(huffman_encode_table[i].code);
    // fprintf(stderr, "\n");
    // huffman_decode_table[i].code = huffman_encode_table[i].code; /* uninverted version */
    /* push all the table rows to the root tree node (they are all it's children). */
    tree.children =
        stack_push(tree.children, (void *)(huffman_decode_table + i));
  }

  /* Convert the table to a tree by "growing" the "children" from the "seed". */
  struct huff_node *pos = &tree;
  stack_s *undone = stack_new();
  undone = stack_push(undone, &tree);
    
  while (undone->length) {
    pos = stack_pop(undone);
    if (pos->children->length == 1) {
      /* leaf node */
      hdata = stack_pop(pos->children);
      free(pos->children);
      pos->children = NULL;
      pos->one = pos->zero = NULL;
      pos->value = hdata->sym;
      continue;
    }
    /* divide children between the left and right branches */
    pos->one = calloc(1, sizeof(struct huff_node));
    pos->zero = calloc(1, sizeof(struct huff_node));
    *pos->one =
        (struct huff_node){.depth = pos->depth + 1, .children = stack_new()};
    *pos->zero =
        (struct huff_node){.depth = pos->depth + 1, .children = stack_new()};
    while (pos->children->length) {
      hdata = stack_pop(pos->children);
      if (hdata->code & (1 << pos->depth))
        pos->one->children = stack_push(pos->one->children, hdata);
      else
        pos->zero->children = stack_push(pos->zero->children, hdata);
    }
    /* push branches to the `undone` stack and release any unused resources or branches */
    free(pos->children);
    pos->children = NULL;
    if (pos->one->children->length == 0) {
      free(pos->one->children);
      free(pos->one);
      pos->one = NULL;
    } else {
      undone = stack_push(undone, pos->one);
    }
    if (pos->zero->children->length == 0) {
      free(pos->zero->children);
      free(pos->zero);
      pos->zero = NULL;
    } else {
      undone = stack_push(undone, pos->zero);
    }
  }
  /* semi-test the resulting tree */
  for (size_t i = 0; i < 257; i++) {
    uint32_t code =
        (huffman_encode_table[i].code << (32 - huffman_encode_table[i].bits));
    uint8_t bit = 0;

    pos = &tree;
    while (pos && (pos->one || pos->zero)) {
      if ((code >> (31 - bit)) & 1)
        pos = pos->one;
      else
        pos = pos->zero;
      bit++;
    }
      /* uninverted test version */
//      code = huffman_encode_table[i].code;
//      
//      pos = &tree;
//      while (pos && (pos->one || pos->zero)) {
//          if (code & 1)
//              pos = pos->one;
//          else
//              pos = pos->zero;
//          code = code >> 1;
//      }

      if (pos == NULL || pos->value != i)
      fprintf(stderr, "FAILED %u != %lu (%p)\n", (pos ? pos->value : -1), i,
              pos),
          exit(1);
  }

  /* print the damn tree */
  fprintf(stderr,
          "\nstatic const struct huffman_decode_node {\n    struct "
          "huffman_decode_node *zero;\n    struct huffman_decode_node "
          "*one;\n    uint16_t value;\n} hpack_huffman_decode_tree =\n");

  stack_push(undone, &tree);

  while (undone->length) {
    pos = stack_pop(undone);

    if (pos->children == NULL) {
      if (pos->one == NULL && pos->zero == NULL) {
        fprintf(stderr, "{.value = %uU }", pos->value);
        continue;
      }
      fprintf(stderr, "{\n .value = 1024U ");
      pos->children = (void *)-1;
    }

    if (pos->zero) {
      fprintf(stderr, ", .zero = &(struct huffman_decode_node)");
      undone = stack_push(undone, pos);
      undone = stack_push(undone, pos->zero);
      pos->zero = NULL;
      continue;
    }
    if (pos->one) {
      fprintf(stderr, ", .one = &(struct huffman_decode_node)");
      undone = stack_push(undone, pos);
      undone = stack_push(undone, pos->one);
      pos->one = NULL;
      continue;
    }
    fprintf(stderr, "}\n");
  }
  fprintf(stderr, ";\n");
  free(undone);
}

#endif

/* *****************************************************************************
 * Paste the result of `print_huff()` here:
 */


static const struct huffman_decode_node {
    struct huffman_decode_node *zero;
    struct huffman_decode_node *one;
    uint16_t value;
} hpack_huffman_decode_tree =
{
    .value = 1024U , .zero = &(struct huffman_decode_node){
        .value = 1024U , .zero = &(struct huffman_decode_node){
            .value = 1024U , .zero = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 48U }, .one = &(struct huffman_decode_node){.value = 49U }}
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 50U }, .one = &(struct huffman_decode_node){.value = 97U }}
            }
            , .one = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 99U }, .one = &(struct huffman_decode_node){.value = 101U }}
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 105U }, .one = &(struct huffman_decode_node){.value = 111U }}
            }
        }
        , .one = &(struct huffman_decode_node){
            .value = 1024U , .zero = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 115U }, .one = &(struct huffman_decode_node){.value = 116U }}
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 32U }, .one = &(struct huffman_decode_node){.value = 37U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 45U }, .one = &(struct huffman_decode_node){.value = 46U }}
                }
            }
            , .one = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 47U }, .one = &(struct huffman_decode_node){.value = 51U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 52U }, .one = &(struct huffman_decode_node){.value = 53U }}
                }
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 54U }, .one = &(struct huffman_decode_node){.value = 55U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 56U }, .one = &(struct huffman_decode_node){.value = 57U }}
                }
            }
        }
    }
    , .one = &(struct huffman_decode_node){
        .value = 1024U , .zero = &(struct huffman_decode_node){
            .value = 1024U , .zero = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 61U }, .one = &(struct huffman_decode_node){.value = 65U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 95U }, .one = &(struct huffman_decode_node){.value = 98U }}
                }
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 100U }, .one = &(struct huffman_decode_node){.value = 102U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 103U }, .one = &(struct huffman_decode_node){.value = 104U }}
                }
            }
            , .one = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 108U }, .one = &(struct huffman_decode_node){.value = 109U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 110U }, .one = &(struct huffman_decode_node){.value = 112U }}
                }
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 114U }, .one = &(struct huffman_decode_node){.value = 117U }}
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 58U }, .one = &(struct huffman_decode_node){.value = 66U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 67U }, .one = &(struct huffman_decode_node){.value = 68U }}
                    }
                }
            }
        }
        , .one = &(struct huffman_decode_node){
            .value = 1024U , .zero = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 69U }, .one = &(struct huffman_decode_node){.value = 70U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 71U }, .one = &(struct huffman_decode_node){.value = 72U }}
                    }
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 73U }, .one = &(struct huffman_decode_node){.value = 74U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 75U }, .one = &(struct huffman_decode_node){.value = 76U }}
                    }
                }
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 77U }, .one = &(struct huffman_decode_node){.value = 78U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 79U }, .one = &(struct huffman_decode_node){.value = 80U }}
                    }
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 81U }, .one = &(struct huffman_decode_node){.value = 82U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 83U }, .one = &(struct huffman_decode_node){.value = 84U }}
                    }
                }
            }
            , .one = &(struct huffman_decode_node){
                .value = 1024U , .zero = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 85U }, .one = &(struct huffman_decode_node){.value = 86U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 87U }, .one = &(struct huffman_decode_node){.value = 89U }}
                    }
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 106U }, .one = &(struct huffman_decode_node){.value = 107U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 113U }, .one = &(struct huffman_decode_node){.value = 118U }}
                    }
                }
                , .one = &(struct huffman_decode_node){
                    .value = 1024U , .zero = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 119U }, .one = &(struct huffman_decode_node){.value = 120U }}
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 121U }, .one = &(struct huffman_decode_node){.value = 122U }}
                    }
                    , .one = &(struct huffman_decode_node){
                        .value = 1024U , .zero = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 38U }, .one = &(struct huffman_decode_node){.value = 42U }}
                            , .one = &(struct huffman_decode_node){
                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 44U }, .one = &(struct huffman_decode_node){.value = 59U }}
                        }
                        , .one = &(struct huffman_decode_node){
                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 88U }, .one = &(struct huffman_decode_node){.value = 90U }}
                            , .one = &(struct huffman_decode_node){
                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 33U }, .one = &(struct huffman_decode_node){.value = 34U }}
                                    , .one = &(struct huffman_decode_node){
                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 40U }, .one = &(struct huffman_decode_node){.value = 41U }}
                                }
                                , .one = &(struct huffman_decode_node){
                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 63U }, .one = &(struct huffman_decode_node){
                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 39U }, .one = &(struct huffman_decode_node){.value = 43U }}
                                    }
                                    , .one = &(struct huffman_decode_node){
                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 124U }, .one = &(struct huffman_decode_node){
                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 35U }, .one = &(struct huffman_decode_node){.value = 62U }}
                                        }
                                        , .one = &(struct huffman_decode_node){
                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 0U }, .one = &(struct huffman_decode_node){.value = 36U }}
                                                , .one = &(struct huffman_decode_node){
                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 64U }, .one = &(struct huffman_decode_node){.value = 91U }}
                                            }
                                            , .one = &(struct huffman_decode_node){
                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 93U }, .one = &(struct huffman_decode_node){.value = 126U }}
                                                , .one = &(struct huffman_decode_node){
                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 94U }, .one = &(struct huffman_decode_node){.value = 125U }}
                                                    , .one = &(struct huffman_decode_node){
                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 60U }, .one = &(struct huffman_decode_node){.value = 96U }}
                                                        , .one = &(struct huffman_decode_node){
                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 123U }, .one = &(struct huffman_decode_node){
                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 92U }, .one = &(struct huffman_decode_node){.value = 195U }}
                                                                        , .one = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 208U }, .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 128U }, .one = &(struct huffman_decode_node){.value = 130U }}
                                                                        }
                                                                    }
                                                                    , .one = &(struct huffman_decode_node){
                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 131U }, .one = &(struct huffman_decode_node){.value = 162U }}
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 184U }, .one = &(struct huffman_decode_node){.value = 194U }}
                                                                        }
                                                                        , .one = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 224U }, .one = &(struct huffman_decode_node){.value = 226U }}
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 153U }, .one = &(struct huffman_decode_node){.value = 161U }}
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 167U }, .one = &(struct huffman_decode_node){.value = 172U }}
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                                , .one = &(struct huffman_decode_node){
                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 176U }, .one = &(struct huffman_decode_node){.value = 177U }}
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 179U }, .one = &(struct huffman_decode_node){.value = 209U }}
                                                                            }
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 216U }, .one = &(struct huffman_decode_node){.value = 217U }}
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 227U }, .one = &(struct huffman_decode_node){.value = 229U }}
                                                                            }
                                                                        }
                                                                        , .one = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 230U }, .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 129U }, .one = &(struct huffman_decode_node){.value = 132U }}
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 133U }, .one = &(struct huffman_decode_node){.value = 134U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 136U }, .one = &(struct huffman_decode_node){.value = 146U }}
                                                                                }
                                                                            }
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 154U }, .one = &(struct huffman_decode_node){.value = 156U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 160U }, .one = &(struct huffman_decode_node){.value = 163U }}
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 164U }, .one = &(struct huffman_decode_node){.value = 169U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 170U }, .one = &(struct huffman_decode_node){.value = 173U }}
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                    , .one = &(struct huffman_decode_node){
                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 178U }, .one = &(struct huffman_decode_node){.value = 181U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 185U }, .one = &(struct huffman_decode_node){.value = 186U }}
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 187U }, .one = &(struct huffman_decode_node){.value = 189U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 190U }, .one = &(struct huffman_decode_node){.value = 196U }}
                                                                                }
                                                                            }
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 198U }, .one = &(struct huffman_decode_node){.value = 228U }}
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 232U }, .one = &(struct huffman_decode_node){.value = 233U }}
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 1U }, .one = &(struct huffman_decode_node){.value = 135U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 137U }, .one = &(struct huffman_decode_node){.value = 138U }}
                                                                                    }
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 139U }, .one = &(struct huffman_decode_node){.value = 140U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 141U }, .one = &(struct huffman_decode_node){.value = 143U }}
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                        , .one = &(struct huffman_decode_node){
                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 147U }, .one = &(struct huffman_decode_node){.value = 149U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 150U }, .one = &(struct huffman_decode_node){.value = 151U }}
                                                                                    }
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 152U }, .one = &(struct huffman_decode_node){.value = 155U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 157U }, .one = &(struct huffman_decode_node){.value = 158U }}
                                                                                    }
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 165U }, .one = &(struct huffman_decode_node){.value = 166U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 168U }, .one = &(struct huffman_decode_node){.value = 174U }}
                                                                                    }
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 175U }, .one = &(struct huffman_decode_node){.value = 180U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 182U }, .one = &(struct huffman_decode_node){.value = 183U }}
                                                                                    }
                                                                                }
                                                                            }
                                                                            , .one = &(struct huffman_decode_node){
                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 188U }, .one = &(struct huffman_decode_node){.value = 191U }}
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 197U }, .one = &(struct huffman_decode_node){.value = 231U }}
                                                                                    }
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 239U }, .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 9U }, .one = &(struct huffman_decode_node){.value = 142U }}
                                                                                        }
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 144U }, .one = &(struct huffman_decode_node){.value = 145U }}
                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 148U }, .one = &(struct huffman_decode_node){.value = 159U }}
                                                                                        }
                                                                                    }
                                                                                }
                                                                                , .one = &(struct huffman_decode_node){
                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 171U }, .one = &(struct huffman_decode_node){.value = 206U }}
                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 215U }, .one = &(struct huffman_decode_node){.value = 225U }}
                                                                                        }
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 236U }, .one = &(struct huffman_decode_node){.value = 237U }}
                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 199U }, .one = &(struct huffman_decode_node){.value = 207U }}
                                                                                                , .one = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){.value = 234U }, .one = &(struct huffman_decode_node){.value = 235U }}
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                    , .one = &(struct huffman_decode_node){
                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 192U }, .one = &(struct huffman_decode_node){.value = 193U }}
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 200U }, .one = &(struct huffman_decode_node){.value = 201U }}
                                                                                                }
                                                                                                , .one = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 202U }, .one = &(struct huffman_decode_node){.value = 205U }}
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 210U }, .one = &(struct huffman_decode_node){.value = 213U }}
                                                                                                }
                                                                                            }
                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 218U }, .one = &(struct huffman_decode_node){.value = 219U }}
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 238U }, .one = &(struct huffman_decode_node){.value = 240U }}
                                                                                                }
                                                                                                , .one = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 242U }, .one = &(struct huffman_decode_node){.value = 243U }}
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 255U }, .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 203U }, .one = &(struct huffman_decode_node){.value = 204U }}
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                        , .one = &(struct huffman_decode_node){
                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 211U }, .one = &(struct huffman_decode_node){.value = 212U }}
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 214U }, .one = &(struct huffman_decode_node){.value = 221U }}
                                                                                                    }
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 222U }, .one = &(struct huffman_decode_node){.value = 223U }}
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 241U }, .one = &(struct huffman_decode_node){.value = 244U }}
                                                                                                    }
                                                                                                }
                                                                                                , .one = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 245U }, .one = &(struct huffman_decode_node){.value = 246U }}
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 247U }, .one = &(struct huffman_decode_node){.value = 248U }}
                                                                                                    }
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 250U }, .one = &(struct huffman_decode_node){.value = 251U }}
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 252U }, .one = &(struct huffman_decode_node){.value = 253U }}
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){.value = 254U }, .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 2U }, .one = &(struct huffman_decode_node){.value = 3U }}
                                                                                                        }
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 4U }, .one = &(struct huffman_decode_node){.value = 5U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 6U }, .one = &(struct huffman_decode_node){.value = 7U }}
                                                                                                        }
                                                                                                    }
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 8U }, .one = &(struct huffman_decode_node){.value = 11U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 12U }, .one = &(struct huffman_decode_node){.value = 14U }}
                                                                                                        }
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 15U }, .one = &(struct huffman_decode_node){.value = 16U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 17U }, .one = &(struct huffman_decode_node){.value = 18U }}
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                                , .one = &(struct huffman_decode_node){
                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 19U }, .one = &(struct huffman_decode_node){.value = 20U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 21U }, .one = &(struct huffman_decode_node){.value = 23U }}
                                                                                                        }
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 24U }, .one = &(struct huffman_decode_node){.value = 25U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 26U }, .one = &(struct huffman_decode_node){.value = 27U }}
                                                                                                        }
                                                                                                    }
                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 28U }, .one = &(struct huffman_decode_node){.value = 29U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 30U }, .one = &(struct huffman_decode_node){.value = 31U }}
                                                                                                        }
                                                                                                        , .one = &(struct huffman_decode_node){
                                                                                                            .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 127U }, .one = &(struct huffman_decode_node){.value = 220U }}
                                                                                                            , .one = &(struct huffman_decode_node){
                                                                                                                .value = 1024U , .zero = &(struct huffman_decode_node){.value = 249U }, .one = &(struct huffman_decode_node){
                                                                                                                    .value = 1024U , .zero = &(struct huffman_decode_node){
                                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 10U }, .one = &(struct huffman_decode_node){.value = 13U }}
                                                                                                                    , .one = &(struct huffman_decode_node){
                                                                                                                        .value = 1024U , .zero = &(struct huffman_decode_node){.value = 22U }, .one = &(struct huffman_decode_node){.value = 256U }}
                                                                                                                }
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif
