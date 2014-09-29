#ifndef HASH_H
#define HASH_H

#include <stdlib.h>
#include "list.h"

/**
 * A hash table is just an array of lists.
 *
 * The size is likely to be a reusable constant.
 */
#define DECLARE_HASHTABLE(name, size)           \
    struct list_item name[size]

static inline void hash_init(struct list_item* hash, const size_t size)
{
    for (size_t i = 0; i < size; i++) {
        list_init(&hash[i]);
    }
}

/** djb2
 * This algorithm was first reported by Dan Bernstein
 * many years ago in comp.lang.c
 */
static inline unsigned long __hash(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c; // hash*33 + c
    return hash;
}

/**
 * Insert an list item into a hash.
 *
 * @warning we don't check if an item with key @key has already been inserted!
 */
static inline void hash_insert(struct list_item* hash, const size_t size,
                               const char* key, struct list_item* item)
{
    unsigned long index = __hash(key) % size;
    list_prepend(&hash[index], item);
}

/**
 * Delete an item (thus from the list and hash).
 */
static inline void hash_delete(struct list_item* item)
{
    list_delete(item);
}

/**
 * Utility function intended for building hash_get functions.
 */
static inline struct list_item* hash_slot(struct list_item* hash,
                                          const size_t size,
                                          const char* key)
{
    unsigned long index = __hash(key) % size;
    return &hash[index];
}

/**
 * Traverse a hash.
 */
#define hash_for_all(hash, size, i, it)                     \
    for ((i) = 0; i < (size); (i++))                        \
        if (!list_is_empty(&hash[i]) && ((it) = &hash[i]))  \
            list_for(it, &hash[i])

#endif /* HASH_H */
