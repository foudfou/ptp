/* Copyright (c) 2014 Foudil Brétel.  All rights reserved. */
#ifndef HASH_H
#define HASH_H
/**
 * A simple hash table inspired from the Linux Kernel and cjdns.
 *
 * Consumer MUST provide a hash function and a key comparison function, with the
 * following signatures:
 *
 *   uint32_t name##_hash(const HASH_KEY_TYPE key);
 *   int name##_cmp(const HASH_KEY_TYPE keyA, const HASH_KEY_TYPE keyB);
 *
 * The reason is that we can hardly make a hash function for all types (struct,
 * char*, int). Neither can we compare keys of unknow formats.
 */

#include "cont.h"
#include "list.h"

/**
 * A hash table is just an array of lists.
 *
 * The size is likely to be a reusable constant.
 */
#define HASH_DECL(name, size)                   \
    struct list_item name[size]

#define HASH_GENERATE(name, struct_name, field_item, field_key, key_type, size) \
    HASH_GENERATE_INSERT(name, key_type, size)                          \
    HASH_GENERATE_GET(name, struct_name, field_item, field_key, key_type, size)

/**
 * Initialize a hash.
 */
static inline void hash_init(struct list_item* hash, const size_t size)
{
    for (size_t i = 0; i < size; i++) {
        list_init(&hash[i]);
    }
}

/**
 * Delete an item (thus from the list and hash).
 */
static inline void hash_delete(struct list_item* item)
{
    list_delete(item);
}

/**
 * Traverse a hash.
 */
#define hash_for(hash, size, it)                            \
    for (size_t i = 0; i < (size); (i++))                   \
        if (!list_is_empty(&hash[i]) && ((it) = &hash[i]))  \
            list_for(it, &hash[i])

#define HASH_GENERATE_INSERT(name, key_type, size)  \
/**
 * Insert a list item into a hash.
 *
 * @warning we don't check if an item with key @key has already been inserted!
 */                                                                     \
static inline void                                                      \
name##_insert(struct list_item* hash, const key_type key, struct list_item* item) \
{                                                                       \
    uint32_t index = name##_hash(key) % size;                           \
    list_prepend(&hash[index], item);                                   \
}

#define HASH_GENERATE_GET(name, struct_name, field_item, field_key, key_type, size)  \
/**
 * Gets an entry by its key.
 *
 * Returns NULL when not found.
 */                                                                     \
static inline struct struct_name*                                       \
name##_get(struct list_item* hash, const key_type key)                  \
{                                                                       \
    uint32_t index = name##_hash(key) % size;                           \
    struct list_item* slot = &hash[index];                              \
    struct list_item* it = slot;                                        \
    struct struct_name* found;                                          \
    list_for(it, slot) {                                                \
        found = cont(it, struct struct_name, field_item);               \
        if (!found)                                                     \
            return NULL;                                                \
        if (!name##_cmp(found->field_key, key))                         \
            return found;                                               \
    }                                                                   \
    return NULL;                                                        \
}

#endif  /* HASH_H */
