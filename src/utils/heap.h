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

#define LEFT(i)   (2*(i) + 1)
#define RIGHT(i)  (2*(i) + 2)
#define PARENT(i) (size_t)(((i) - 1)/2)


#define HEAP_GENERATE(name, type)     \
    HEAP_GENERATE_BASE(name, type)    \
    HEAP_GENERATE_INSERT(name, type)  \
    HEAP_GENERATE_GET(name, type)

#define HEAP_GENERATE_BASE(name, type)                                  \
    struct name {                                                       \
        size_t  len;                                                    \
        size_t  cap;                                                    \
        type   *items;                                                  \
    };                                                                  \
                                                                        \
static inline                                                           \
bool name##_init(struct name *h, size_t capa) {                         \
    h->items = calloc(capa, sizeof(type));                              \
    if (h->items == NULL)                                               \
        return false;                                                   \
    h->cap = capa;                                                      \
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
static inline void name##swap(type *a, type *b) {                       \
    type tmp = *b;                                                      \
    *b = *a;                                                            \
    *a = tmp;                                                           \
}


#define HEAP_GENERATE_INSERT(name, type)        \
/**
 * Can fail if growing fails.
 */                                                                     \
static inline bool name##_insert(struct name *h, type item)             \
{                                                                       \
    if (h->len >= h->cap && !name##_grow(h))                            \
        return false;                                                   \
    h->items[h->len] = item;                                            \
    h->len++;                                                           \
                                                                        \
    size_t i = h->len - 1;                                              \
    while (i > 0 && name##_cmp(h->items[PARENT(i)], h->items[i]) < 0) { \
        name##swap(&h->items[PARENT(i)], &h->items[i]);                 \
        i = PARENT(i);                                                  \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

#define HEAP_GENERATE_GET(name, type)                                   \
static inline type name##_get(struct name *h)                           \
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
    size_t i = 0;                                                       \
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
        name##swap(&h->items[i], &h->items[largest]);                   \
        i = largest;                                                    \
    }                                                                   \
                                                                        \
    return ret;                                                         \
}


#endif /* HEAP_H */
