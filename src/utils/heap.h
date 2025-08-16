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
#include <stdio.h>
#include <stdlib.h>

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

#define HEAP_PEEK(h) h.items[0]

#define HEAP_GENERATE(name, type)     \
    HEAP_GENERATE_BASE(name, type)    \
    HEAP_GENERATE_PUSH(name, type)    \
    HEAP_GENERATE_POP(name, type)
//  HEAP_GENERATE_REPLACE_TOP(name, type) implement on demand

#define HEAP_GENERATE_BASE(name, type)                                  \
    struct name {                                                       \
        size_t  len;                                                    \
        size_t  cap;                                                    \
        type   *items;                                                  \
    };                                                                  \
                                                                        \
/**
 * Allocates the heap array.
 *
 * CAUTION: Consumers MUST free items after use.
 */                                                                     \
static inline                                                           \
bool name##_init(struct name *h, size_t capa) {                         \
    h->items = calloc(capa, sizeof(type));                              \
    if (h->items == NULL)                                               \
        return false;                                                   \
    h->cap = capa;                                                      \
    h->len = 0;                                                         \
    return true;                                                        \
}                                                                       \
                                                                        \
static inline                                                           \
bool name##_grow(struct name *h) {                                      \
    /* FIXME overflow */                                                \
    size_t cap_new = h->cap * 2;                                        \
    type *items_new = realloc(h->items, sizeof(*h->items) * cap_new);   \
    if (items_new == NULL)                                              \
        return false;                                                   \
    h->cap = cap_new;                                                   \
    h->items = items_new;                                               \
    return true;                                                        \
}                                                                       \
                                                                        \
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
        if (l < h->len && name##_cmp(h->items[i], h->items[l]) < 0)     \
            largest = l;                                                \
        if (r < h->len && name##_cmp(h->items[largest], h->items[r]) < 0) \
            largest = r;                                                \
        if (largest == i)                                               \
            break;                                                      \
        name##_swap(&h->items[i], &h->items[largest]);                  \
        i = largest;                                                    \
    }                                                                   \
}

#define HEAP_GENERATE_PUSH(name, type)                                  \
/**
 * Can fail if growing fails.
 */                                                                     \
static inline bool name##_push(struct name *h, type item)               \
{                                                                       \
    if (h->len >= h->cap && !name##_grow(h))                            \
        return false;                                                   \
    h->items[h->len] = item;                                            \
    h->len++;                                                           \
                                                                        \
    size_t i = h->len - 1;                                              \
    while (i > 0 && name##_cmp(h->items[PARENT(i)], h->items[i]) < 0) { \
        name##_swap(&h->items[PARENT(i)], &h->items[i]);                \
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
    type ret = h->items[0];                                             \
    h->len--;                                                           \
    h->items[0] = h->items[h->len];                                     \
    h->items[h->len] = (type)0;                                         \
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
    h->items[0] = item;                                                 \
    name##_heapify_down(h, 0);                                          \
}


#endif /* HEAP_H */
