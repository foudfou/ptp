/* Copyright (c) 2025 Foudil Brétel.  All rights reserved. */
#ifndef HEAP_H
#define HEAP_H

/**
 * A binary heap / priority queue implementation.
 *
 * Employs a dynamic array, so users can start with a small capacity that will
 * grow automatically as required.
 *
 * User MUST provide a comparison function:
 *
 *   int name##_cmp(const HEAP_ITEM_TYPE *a, const HEAP_ITEM_TYPE *b);
 *
 * Implemented as a max-heap, so user can just reverse the _cmp function to
 * otain a min-heap.
 */
#include <stdbool.h>
#include <stdlib.h>
#include "utils/growable.h"

#define HEAP_SIZE_INITIAL 64
#define HEAP_SIZE_FACTOR 2

/*
  « Heaps are usually implemented with an array, as follows:

  - Each element in the array represents a node of the heap, and
  - The parent/child relationship is defined implicitly by the elements'
    indices in the array. »

  https://en.wikipedia.org/wiki/Heap_(data_structure)
*/
#define LEFT(i)   (2*(i) + 1)
#define RIGHT(i)  (2*(i) + 2)
#define PARENT(i) (size_t)(((i) - 1)/2)

#define HEAP_PEEK(h) h.buf[0]

#define HEAP_GENERATE(name, type, limit)        \
    GROWABLE_GENERATE(name, type, 16, 2, limit) \
    HEAP_GENERATE_BASE(name, type)              \
    HEAP_GENERATE_PUSH(name, type)              \
    HEAP_GENERATE_POP(name, type)
//  HEAP_GENERATE_REPLACE_TOP(name, type) implement on demand

#define HEAP_GENERATE_BASE(name, type)                                  \
static inline void name##_swap(type *a, type *b) {                      \
    type tmp = *b;                                                      \
    *b = *a;                                                            \
    *a = tmp;                                                           \
}                                                                       \
                                                                        \
static inline void name##_heapify_down(struct name *h, size_t i)        \
{                                                                       \
    while (i < h->len) {                                                \
        size_t largest = i;                                             \
        size_t l = LEFT(i);                                             \
        size_t r = RIGHT(i);                                            \
        if (l < h->len && name##_cmp(h->buf[i], h->buf[l]) < 0)         \
            largest = l;                                                \
        if (r < h->len && name##_cmp(h->buf[largest], h->buf[r]) < 0)   \
            largest = r;                                                \
        if (largest == i)                                               \
            break;                                                      \
        name##_swap(&h->buf[i], &h->buf[largest]);                      \
        i = largest;                                                    \
    }                                                                   \
}

#define HEAP_GENERATE_PUSH(name, type)                                  \
/**
 * Can fail if growing fails.
 */                                                                     \
static inline bool name##_push(struct name *h, type item)               \
{                                                                       \
    if (h->len >= h->cap && !name##_grow(h, 1))                         \
        return false;                                                   \
    h->buf[h->len] = item;                                              \
    h->len++;                                                           \
                                                                        \
    size_t i = h->len - 1;                                              \
    while (i > 0 && name##_cmp(h->buf[PARENT(i)], h->buf[i]) < 0) {     \
        name##_swap(&h->buf[PARENT(i)], &h->buf[i]);                    \
        i = PARENT(i);                                                  \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

#define HEAP_GENERATE_POP(name, type)                                   \
static inline type name##_pop(struct name *h)                           \
{                                                                       \
    if (h->len == 0)                                                    \
        /* FIXME ok for pointers, but not for other types */            \
        return (type)0;                                                 \
                                                                        \
    type ret = h->buf[0];                                               \
    h->len--;                                                           \
    h->buf[0] = h->buf[h->len];                                         \
    h->buf[h->len] = (type)0;                                           \
                                                                        \
    name##_heapify_down(h, 0);                                          \
                                                                        \
    return ret;                                                         \
}

#define HEAP_GENERATE_REPLACE_TOP(name, type)                           \
/**
 * This is more efficient than pop-then-push, as it avoids a heapify_up.
 *
 * Use HEAP_PEEK() to identify the root item.
 */                                                                     \
static inline void name##_replace_top(struct name *h, type item)        \
{                                                                       \
    if (h->len == 0) {                                                  \
        name##_push(h, item);                                           \
        return;                                                         \
    }                                                                   \
                                                                        \
    h->buf[0] = item;                                                   \
    name##_heapify_down(h, 0);                                          \
}


#endif /* HEAP_H */
