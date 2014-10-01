#ifndef HASH_H
#define HASH_H
/**
 * This header can be imported multiple times to create different hash types.
 * Just make sure to use a different HASH_NAME.
 */

#include "base.h"
#include "cont.h"
#include "list.h"

/**
 * A hash table is just an array of lists.
 *
 * The size is likely to be a reusable constant.
 */
#define HASH_DECL(name, size)                   \
    struct list_item name[size]

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
#define hash_for_all(hash, size, i, it)                     \
    for ((i) = 0; i < (size); (i++))                        \
        if (!list_is_empty(&hash[i]) && ((it) = &hash[i]))  \
            list_for(it, &hash[i])

#endif  /* HASH_H */


#ifndef HASH_NAME
    #error must give this map type a name by defining HASH_NAME
#endif
#ifndef HASH_TYPE
    #error must define HASH_TYPE
#endif
#ifndef HASH_ITEM_MEMBER
    #error must define HASH_ITEM_MEMBER
#endif
#if defined(HASH_KEY_TYPE)
    assert_compilation(!(sizeof(HASH_KEY_TYPE) % 4));
#else
    #error must define HASH_KEY_TYPE
#endif
#ifndef HASH_KEY_MEMBER
    #error must define HASH_KEY_MEMBER
#endif

#define HASH_FUNCTION(name) HASH_GLUE(HASH_GLUE(HASH_GLUE(hash_, HASH_NAME), _), name)
#define HASH_GLUE(x, y) HASH_GLUE2(x, y)
#define HASH_GLUE2(x, y) x ## y

#if defined(HASH_FUNCTION_PROVIDED)
    /**
     * Consumer provided hash function.
     */
    static inline uint32_t HASH_FUNCTION(hash)(HASH_KEY_TYPE key);
#else
    /**
     * Default hash function (last 4 bytes of the key).
     */
    static inline uint32_t HASH_FUNCTION(hash)(HASH_KEY_TYPE key)
    {
        return (uint32_t)key;
    }
#endif

/**
 * Insert an list item into a hash.
 *
 * @warning we don't check if an item with key @key has already been inserted!
 */
static inline void
HASH_FUNCTION(insert)(struct list_item* hash, const size_t size,
                      HASH_KEY_TYPE key, struct list_item* item)
{
    uint32_t index = HASH_FUNCTION(hash)(key) % size;
    list_prepend(&hash[index], item);
}

/**
 * Gets an entry by its key.
 *
 * Returns NULL when not found.
 */
static inline HASH_TYPE*
HASH_FUNCTION(get)(struct list_item* hash, const size_t size, HASH_KEY_TYPE key)
{
    uint32_t index = HASH_FUNCTION(hash)(key) % size;
    struct list_item* slot = &hash[index];
    struct list_item* it = slot;
    HASH_TYPE* found;
    list_for(it, slot) {
        found = cont(it, HASH_TYPE, HASH_ITEM_MEMBER);
        if (found->HASH_KEY_MEMBER == key)
            return found;
    }
    return NULL;
}


#undef HASH_ITEM_MEMBER
#undef HASH_FUNCTION_PROVIDED
#undef HASH_FUNCTION
#undef HASH_KEY_TYPE
#undef HASH_KEY_MEMBER
#undef HASH_TYPE
#undef HASH_NAME
