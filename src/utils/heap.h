#ifndef HEAP_H
#define HEAP_H

/**
 * A binary heap / priority queue implementation using a dynamic array.
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

#define HEAP_INIT(var_name, heap_name, item_type, capa)     \
    struct heap_name var_name = {0};                        \
    var_name.items = calloc(capa, sizeof(item_type));       \
    var_name.cap = capa

#define HEAP_GENERATE_BASE(name, type)                                  \
    struct name {                                                       \
        size_t  len;                                                    \
        size_t  cap;                                                    \
        type   *items;                                                  \
    };                                                                  \
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
bool name##_insert(struct name *h, type item)                           \
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
type name##_get(struct name *h)                                         \
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
    while ((i < h->len) &&                                              \
           ((LEFT(i) < h->len && (name##_cmp(h->items[i], h->items[LEFT(i)]) < 0)) || \
            (RIGHT(i) < h->len && name##_cmp(h->items[i], h->items[RIGHT(i)]) < 0))) { \
        size_t child =                                                  \
            (name##_cmp(h->items[LEFT(i)], h->items[RIGHT(i)]) < 0) ? \
            RIGHT(i) : LEFT(i);                                         \
        name##swap(&h->items[i], &h->items[child]);                     \
        i = child;                                                      \
    }                                                                   \
                                                                        \
    return ret;                                                         \
}


#endif /* HEAP_H */
