#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/utils/cont.h"
#include "../src/utils/list.h"
#include "../src/utils/hash.h"

static const unsigned char HASH_SIZE = 0xff;

/*  Typical object that can be added to a hash. */
struct entry {
    const char* key;
    int val;
    struct list_item item;
};

/*  Define your own facility function for retrieving items. */
static inline struct entry* entry_get(struct list_item* hash,
                                      const size_t size,
                                      const char* key)
{
    struct list_item* slot = hash_slot(hash, size, key);
    struct list_item* it = slot;
    struct entry* found;
    list_for(it, slot) {
        found = cont(it, struct entry, item);
        if (found->key == key)
            return found;
    }
    return NULL;
}

int main ()
{
    /* Hash declaration */
    struct list_item hash[HASH_SIZE];
    hash_init(hash, HASH_SIZE);
    assert(&hash[1] == hash[1].prev);
    assert(hash[1].prev == hash[1].next);

    /* Hash insertion */
    struct entry one = {"one", 1, LIST_ITEM_DECL(one.item)}; // static decl
    hash_insert(hash, HASH_SIZE, one.key, &one.item);
    struct entry bogus_one = {"one", 10, LIST_ITEM_DECL(one.item)};
    // same key not checked !!
    hash_insert(hash, HASH_SIZE, bogus_one.key, &bogus_one.item);
    struct entry two = {"two", 2, LIST_ITEM_DECL(two.item)};
    hash_insert(hash, HASH_SIZE, two.key, &two.item);

    /* Hash getting item */
    struct entry * found = entry_get(hash, HASH_SIZE, "two");
    assert(!strncmp(found->key, "two", 3));
    assert(found->val == 2);
    struct entry * notfound = entry_get(hash, HASH_SIZE, "three");
    assert(!notfound);

    /* Hash deletion */
    found = entry_get(hash, HASH_SIZE, "one");
    assert(found->val == 10);
    hash_delete(&found->item);
    found = entry_get(hash, HASH_SIZE, "one");
    assert(found->val == 1);

    /* Hash traversal */
    int i = 0; struct list_item* it = hash;
    int values[2] = {0}; int j = 0;
    hash_for_all(hash, HASH_SIZE, i, it) {
        struct entry* tmp = cont(it, struct entry, item);
        values[j++] = tmp->val;
    }
    int rule1[] = {2, 1}; int rule2[] = {1, 2};
    assert(!memcmp(values, rule1, 2*sizeof(int)) ||
           !memcmp(values, rule2, 2*sizeof(int)));

    return 0;
}
