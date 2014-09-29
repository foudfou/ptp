#ifndef HASH_H
#define HASH_H

#include <stdlib.h>
#include "list.h"

struct hash {
    size_t size;
    struct list_item ** table;
};

/* djb2
 * This algorithm was first reported by Dan Bernstein
 * many years ago in comp.lang.c
 */
static inline unsigned long __hash(unsigned char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c; // hash*33 + c
    return hash;
}

static inline struct hash* hash_create(size_t size)
{
    struct hash* hash = malloc(sizeof(struct hash));
    hash->size = size;
    hash->table = (struct list_item**)calloc(size, sizeof(struct list_item*));
    for (size_t i = 0; i < hash->size; i++) {
        hash->table[i] = (struct list_item*)malloc(sizeof(struct list_item));
        list_init(hash->table[i]);
    }
    return hash;
}

static inline void hash_destroy(struct hash* hash)
{
    for (size_t i = 0; i < hash->size; i++)
        free(hash->table[i]);
    free(hash->table);
    free(hash);
}

static inline void hash_insert(struct hash* hash,
                               struct list_item* item,
                               unsigned char* key)
{
    unsigned long h = __hash(key) % hash->size;
    printf("%lu\n", h);
    list_prepend(hash->table[h], item);
}

#endif /* HASH_H */
